/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ecclesia/lib/redfish/redpath/engine/normalizer.h"

#include <stdint.h>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/path_util.h"
#include "ecclesia/lib/redfish/devpath.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/time/proto.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

namespace {

constexpr absl::string_view kEmbeddedLocationContext =
    "__EmbeddedLocationContext__";
constexpr absl::string_view kLocalDevpath = "__LocalDevpath__";
constexpr absl::string_view kMachineDevpath = "__MachineDevpath__";
constexpr absl::string_view kStableName = "__StableName__";
constexpr absl::string_view kSubFru = "__SubFru__";

std::vector<DelliciusQuery::Subquery::RedfishProperty>
GetAdditionalProperties() {
  std::vector<DelliciusQuery::Subquery::RedfishProperty> result;
  auto add_property = [&](absl::string_view name,
                          std::vector<std::string> properties,
                          bool is_collection = false) {
    for (std::string &property : properties) {
      DelliciusQuery::Subquery::RedfishProperty new_prop;
      new_prop.set_name(std::string(name));
      new_prop.set_property(std::move(property));
      if (is_collection) {
        new_prop.set_property_element_type(
            DelliciusQuery::Subquery::RedfishProperty::COLLECTION_PRIMITIVE);
      }
      new_prop.set_type(DelliciusQuery::Subquery::RedfishProperty::STRING);
      result.push_back(std::move(new_prop));
    }
  };
  // Implicit collection of Name property to be used for stable IDs.
  add_property(kStableName, {"Name"});
  add_property(kSubFru, {"Oem.Google.LocationContext.ServiceLabel",
                         "Oem.Google.LocationContext.Devpath"});
  add_property(kEmbeddedLocationContext,
               {"Oem.Google.LocationContext.EmbeddedLocationContext"}, true);

  add_property(kLocalDevpath, {"Location.Oem.Google.Devpath",
                               "PhysicalLocation.Oem.Google.Devpath",
                               "Oem.Google.LocationContext.Devpath"});

  return result;
}

absl::Status GetCollectionPropertyFromRedfishObject(
    const DelliciusQuery::Subquery::RedfishProperty &property,
    const nlohmann::json &json_obj, QueryValue &query_value) {
  if (!json_obj.is_array()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Tried to get array property from non array json object: ",
                     json_obj.dump()));
  }
  switch (property.type()) {
    case DelliciusQuery::Subquery::RedfishProperty::STRING: {
      if (!std::all_of(
              json_obj.begin(), json_obj.end(),
              [](const nlohmann::json &el) { return el.is_string(); })) {
        return absl::InvalidArgumentError(
            absl::StrCat("Error querying property ", property.property(),
                         " as string array from object: ", json_obj.dump()));
      }
      for (const std::string &value :
           json_obj.get<std::vector<std::string>>()) {
        query_value.mutable_list_value()->add_values()->set_string_value(value);
      }
      break;
    }
    case DelliciusQuery::Subquery::RedfishProperty::BOOLEAN: {
      if (!std::all_of(
              json_obj.begin(), json_obj.end(),
              [](const nlohmann::json &el) { return el.is_boolean(); })) {
        return absl::InvalidArgumentError(
            absl::StrCat("Error querying property ", property.property(),
                         " as boolean array from object: ", json_obj.dump()));
      }
      for (const bool value : json_obj.get<std::vector<bool>>()) {
        query_value.mutable_list_value()->add_values()->set_bool_value(value);
      }
      break;
    }
    case DelliciusQuery::Subquery::RedfishProperty::DOUBLE: {
      if (!std::all_of(
              json_obj.begin(), json_obj.end(),
              [](const nlohmann::json &el) { return el.is_number(); })) {
        return absl::InvalidArgumentError(
            absl::StrCat("Error querying property ", property.property(),
                         " as number array from object: ", json_obj.dump()));
      }
      for (const double value : json_obj.get<std::vector<double>>()) {
        query_value.mutable_list_value()->add_values()->set_double_value(value);
      }
      break;
    }
    case DelliciusQuery::Subquery::RedfishProperty::INT64: {
      if (!std::all_of(
              json_obj.begin(), json_obj.end(),
              [](const nlohmann::json &el) { return el.is_number(); })) {
        return absl::InvalidArgumentError(
            absl::StrCat("Error querying property ", property.property(),
                         " as number array from object: ", json_obj.dump()));
      }
      for (const int64_t value : json_obj.get<std::vector<int64_t>>()) {
        query_value.mutable_list_value()->add_values()->set_int_value(value);
      }
      break;
    }
    case DelliciusQuery::Subquery::RedfishProperty::DATE_TIME_OFFSET: {
      for (const auto &json_value : json_obj) {
        if (!json_value.is_string()) {
          return absl::InvalidArgumentError(
              absl::StrCat("Error querying property ", property.property(),
                           " as a timestamp string from non string object: ",
                           json_obj.dump()));
        }
        absl::Time timevalue;
        if (absl::ParseTime("%Y-%m-%dT%H:%M:%S%Z",
                            json_value.get<std::string>(), &timevalue,
                            nullptr)) {
          absl::StatusOr<google::protobuf::Timestamp> timestamp =
              AbslTimeToProtoTime(timevalue);
          if (timestamp.ok()) {
            *query_value.mutable_list_value()
                 ->add_values()
                 ->mutable_timestamp_value() = *timestamp;
          }
        }
      }
      break;
    }
    default: {
      break;
    }
  }
  return absl::OkStatus();
}
absl::StatusOr<QueryValue> GetPropertyFromRedfishObject(
    const nlohmann::json &redfish_content,
    const DelliciusQuery::Subquery::RedfishProperty &property) {
  // A property requirement can specify nested nodes like
  // 'Thresholds.UpperCritical.Reading' or a simple property like 'Name'.
  // We will split the property name to ensure we process all node names in
  // the property expression.

  ECCLESIA_ASSIGN_OR_RETURN(
      nlohmann::json json_obj,
      ResolveRedPathNodeToJson(redfish_content, property.property()));

  using RedfishProperty = DelliciusQuery::Subquery::RedfishProperty;

  QueryValue query_value;
  if (property.property_element_type() ==
      RedfishProperty::COLLECTION_PRIMITIVE) {
    ECCLESIA_RETURN_IF_ERROR(GetCollectionPropertyFromRedfishObject(
        property, json_obj, query_value));
  } else if (json_obj.is_null()) {
    LOG(INFO) << "Encoutnered null property value during normalization: "
              << json_obj.dump();
  } else {
    switch (property.type()) {
      case RedfishProperty::STRING: {
        if (!json_obj.is_string()) {
          return absl::InvalidArgumentError(absl::StrCat(
              "Error querying property ", property.property(),
              " as a string from non string object: ", json_obj.dump()));
        }
        query_value.set_string_value(json_obj.get<std::string>());
        break;
      }
      case RedfishProperty::BOOLEAN: {
        if (!json_obj.is_boolean()) {
          return absl::InvalidArgumentError(absl::StrCat(
              "Error querying property ", property.property(),
              " as a boolean from non boolean object: ", json_obj.dump()));
        }
        query_value.set_bool_value(json_obj.get<bool>());
        break;
      }
      case RedfishProperty::DOUBLE: {
        if (!json_obj.is_number()) {
          return absl::InvalidArgumentError(absl::StrCat(
              "Error querying property ", property.property(),
              " as a number from non number object: ", json_obj.dump()));
        }
        query_value.set_double_value(json_obj.get<double>());
        break;
      }
      case RedfishProperty::INT64: {
        if (!json_obj.is_number()) {
          return absl::InvalidArgumentError(absl::StrCat(
              "Error querying property ", property.property(),
              " as an number from non number object: ", json_obj.dump()));
        }
        query_value.set_int_value(json_obj.get<int64_t>());
        break;
      }
      case RedfishProperty::DATE_TIME_OFFSET: {
        absl::Time timevalue;
        if (!json_obj.is_string()) {
          return absl::InvalidArgumentError(
              absl::StrCat("Error querying property ", property.property(),
                           " as a timestamp string from non string object: ",
                           json_obj.dump()));
        }
        if (absl::ParseTime("%Y-%m-%dT%H:%M:%S%Z", json_obj.get<std::string>(),
                            &timevalue, nullptr)) {
          absl::StatusOr<google::protobuf::Timestamp> timestamp =
              AbslTimeToProtoTime(timevalue);
          if (timestamp.ok()) {
            *query_value.mutable_timestamp_value() = std::move(*timestamp);
          }
        }
        break;
      }
      default: {
        break;
      }
    }
  }
  if (query_value.kind_case() == QueryValue::KIND_NOT_SET) {
    return absl::InvalidArgumentError("Invalid property type");
  }
  return query_value;
}

}  // namespace

RedpathNormalizerImplDefault::RedpathNormalizerImplDefault()
    : additional_properties_(GetAdditionalProperties()) {}

absl::Status RedpathNormalizerImplDefault::Normalize(
    const ecclesia::RedfishObject &redfish_object,
    const DelliciusQuery::Subquery &subquery,
    ecclesia::QueryResultData &data_set_local,
    const RedpathNormalizerOptions &options) {
  const nlohmann::json json_content = redfish_object.GetContentAsJson();
  for (const DelliciusQuery::Subquery::RedfishProperty &property :
       subquery.properties()) {
    auto property_out = GetPropertyFromRedfishObject(json_content, property);
    // It is not an error if normalizer fails to normalize a property if
    // required property is not part of Resource attributes.
    if (!property_out.ok()) {
      if (property_out.status().code() == absl::StatusCode::kInvalidArgument) {
        return property_out.status();
      }
      continue;
    }
    // By default, name of the queried property is set as name if the client
    // application does not provide a name to map the parsed property to.
    std::string prop_name;
    if (property.has_name()) {
      prop_name = property.name();
    } else {
      prop_name = property.property();
      absl::StrReplaceAll({{"\\.", "."}}, &prop_name);
    }

    (*data_set_local.mutable_fields())[prop_name] = *property_out;
  }

  // We add additional properties to populate stable id based on Redfish
  // Location. Only add stable name if a LocationContext is present.
  bool is_sub_fru = false;
  std::string sub_fru_name;
  for (const DelliciusQuery::Subquery::RedfishProperty &property :
       additional_properties_) {
    auto property_out = GetPropertyFromRedfishObject(json_content, property);
    if (!property_out.ok()) {
      if (property_out.status().code() == absl::StatusCode::kInvalidArgument) {
        return property_out.status();
      }
      continue;
    }
    std::string name = property.name();
    if (name == kLocalDevpath) {
      QueryValue query_value;
      query_value.mutable_identifier()->set_local_devpath(
          property_out->string_value());
      (*data_set_local.mutable_fields())[name] = query_value;
    } else if (name == kEmbeddedLocationContext) {
      QueryValue query_value;
      std::string embedded_location_context;
      for (const auto &value : property_out->list_value().values()) {
        embedded_location_context.append("/");
        embedded_location_context.append(value.string_value());
      }
      query_value.mutable_identifier()->set_embedded_location_context(
          embedded_location_context);
      (*data_set_local.mutable_fields())[name] = query_value;
    } else if (name == kStableName) {
      sub_fru_name = std::move(*property_out->mutable_string_value());
    }
    // This flag is set when a LocationContext is present with a ServiceLabel
    // field.
    if (name == kSubFru) {
      is_sub_fru = true;
    }
  }

  // Add stable name if a LocationContext is present.
  if (is_sub_fru) {
    QueryValue query_value;
    query_value.mutable_identifier()->set_stable_name(sub_fru_name);
    (*data_set_local.mutable_fields())[kStableName] = query_value;
  }

  if (options.enable_url_annotation) {
    if (auto it = json_content.find(ecclesia::PropertyOdataId::Name);
        it != json_content.end()) {
      QueryValue query_value;
      query_value.set_string_value(it->get<std::string>());
      (*data_set_local.mutable_fields())[kUriAnnotationTag] =
          std::move(query_value);
    }
  }
  return absl::OkStatus();
}

absl::Status RedpathNormalizerImplAddDevpath::Normalize(
    const RedfishObject &redfish_object,
    const DelliciusQuery::Subquery &subquery,
    ecclesia::QueryResultData &data_set_local,
    const RedpathNormalizerOptions &options) {
  // Prioritize devpath populated by default normalizer.
  if (data_set_local.fields().contains(kLocalDevpath)) {
    return absl::OkStatus();
  }

  // Derive devpath from Node Topology (URI to local devpath map).
  std::optional<std::string> devpath =
      GetDevpathForObjectAndNodeTopology(redfish_object, topology_);

  if (devpath.has_value()) {
    QueryValue query_value;
    query_value.mutable_identifier()->set_local_devpath(devpath.value());
    (*data_set_local.mutable_fields())[kLocalDevpath] = query_value;
  }
  return absl::OkStatus();
}

absl::Status RedpathNormalizerImplAddMachineBarepath::Normalize(
    const RedfishObject &redfish_object,
    const DelliciusQuery::Subquery &subquery,
    ecclesia::QueryResultData &data_set_local,
    const RedpathNormalizerOptions &options) {
  //  We will now try to map a local devpath to machine devpath
  if (!(data_set_local.fields().contains(kLocalDevpath))) {
    return absl::OkStatus();
  }
  absl::StatusOr<std::string> machine_devpath =
      id_assigner_->IdForLocalDevpathInDataSet(data_set_local);
  if (machine_devpath.ok()) {
    QueryValue query_value;
    query_value.mutable_identifier()->set_machine_devpath(*machine_devpath);
    (*data_set_local.mutable_fields())[kMachineDevpath] =
        std::move(query_value);
  }
  return absl::OkStatus();
}

}  // namespace ecclesia
