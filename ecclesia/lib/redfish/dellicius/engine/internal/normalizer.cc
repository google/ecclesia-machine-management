/*
 * Copyright 2022 Google LLC
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

#include "ecclesia/lib/redfish/dellicius/engine/internal/normalizer.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/timestamp.pb.h"
#include "google/rpc/code.pb.h"
#include "google/rpc/status.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/path_util.h"
#include "ecclesia/lib/redfish/devpath.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/time/proto.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

namespace {

constexpr absl::string_view kStableName = "__StableName__";
constexpr absl::string_view kServiceLabel = "__ServiceLabel__";
constexpr absl::string_view kPartLocationContext = "__PartLocationContext__";
constexpr absl::string_view kEmbeddedLocationContext =
    "__EmbeddedLocationContext__";
constexpr absl::string_view kLocalDevpath = "__LocalDevpath__";
constexpr absl::string_view kSubFru = "__SubFru__";
constexpr absl::string_view kRFC3339DateTime = "%Y-%m-%d%ET%H:%M:%E*S%Ez";

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

  // Implicit collection of Name property to be used for stable IDs only if
  // LocationContext.ServiceLabel or LocationContext.Devpath is present.
  add_property(kStableName, {"Name"});
  add_property(kSubFru, {"Oem.Google.LocationContext.ServiceLabel",
                         "Oem.Google.LocationContext.Devpath"});

  add_property(kServiceLabel, {"Location.PartLocation.ServiceLabel",
                               "PhysicalLocation.PartLocation.ServiceLabel",
                               "Oem.Google.LocationContext.ServiceLabel"});
  add_property(
      kPartLocationContext,
      {"Location.PartLocationContext", "PhysicalLocation.PartLocationContext",
       "Oem.Google.LocationContext.PartLocationContext"});

  add_property(kEmbeddedLocationContext,
               {"Oem.Google.LocationContext.EmbeddedLocationContext"}, true);

  add_property(kLocalDevpath, {"Location.Oem.Google.Devpath",
                               "PhysicalLocation.Oem.Google.Devpath",
                               "Oem.Google.LocationContext.Devpath"});

  return result;
}

absl::Status GetCollectionPropertyFromRedfishObject(
    const DelliciusQuery::Subquery::RedfishProperty &property,
    const nlohmann::json &json_obj, SubqueryDataSet::Property &property_out) {
  if (!json_obj.is_array()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Tried to get array property from non array json object: ",
                     json_obj.dump()));
  }
  switch (property.type()) {
    case DelliciusQuery::Subquery::RedfishProperty::STRING: {
      for (const std::string &value :
           json_obj.get<std::vector<std::string>>()) {
        property_out.mutable_collection_value()
            ->add_values()
            ->set_string_value(value);
      }
      break;
    }

    case DelliciusQuery::Subquery::RedfishProperty::BOOLEAN: {
      for (const bool value : json_obj.get<std::vector<bool>>()) {
        property_out.mutable_collection_value()
            ->add_values()
            ->set_boolean_value(value);
      }
      break;
    }
    case DelliciusQuery::Subquery::RedfishProperty::DOUBLE: {
      for (const double value : json_obj.get<std::vector<double>>()) {
        property_out.mutable_collection_value()
            ->add_values()
            ->set_double_value(value);
      }
      break;
    }
    case DelliciusQuery::Subquery::RedfishProperty::INT64: {
      for (const int64_t value : json_obj.get<std::vector<int64_t>>()) {
        property_out.mutable_collection_value()
            ->add_values()
            ->set_int64_value(value);
      }
      break;
    }
    case DelliciusQuery::Subquery::RedfishProperty::DATE_TIME_OFFSET: {
      for (const auto &json_value : json_obj) {
        if (!json_value.is_string()) {
          break;
        }
        absl::Time timevalue;
        if (absl::ParseTime(kRFC3339DateTime,
                            json_value.get<std::string>(), &timevalue,
                            nullptr)) {
          absl::StatusOr<google::protobuf::Timestamp> timestamp =
              AbslTimeToProtoTime(timevalue);
          if (timestamp.ok()) {
            *property_out.mutable_collection_value()
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
absl::StatusOr<SubqueryDataSet::Property> GetPropertyFromRedfishObject(
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

  SubqueryDataSet::Property property_out;
  if (property.property_element_type() ==
      RedfishProperty::COLLECTION_PRIMITIVE) {
    ECCLESIA_RETURN_IF_ERROR(GetCollectionPropertyFromRedfishObject(
        property, json_obj, property_out));
  } else if (json_obj.is_null()) {
    LOG(INFO) << "Encountered null property value during normalization: "
              << json_obj.dump();
  } else {
    // clients to use new property value fields.
    switch (property.type()) {
      case RedfishProperty::STRING: {
        if (!json_obj.is_string()) {
          std::string error_message = absl::StrCat(
              "Error querying property ", property.property(),
              " as a string from non string object: ", json_obj.dump());
          LOG(ERROR) << error_message;
          return absl::InvalidArgumentError(error_message);
        }
        property_out.set_string_value(json_obj.get<std::string>());
        break;
      }
      case RedfishProperty::BOOLEAN: {
        if (!json_obj.is_boolean()) {
          std::string error_message = absl::StrCat(
              "Error querying property ", property.property(),
              " as a boolean from non boolean object: ", json_obj.dump());
          LOG(ERROR) << error_message;
          return absl::InvalidArgumentError(error_message);
        }
        property_out.set_boolean_value(json_obj.get<bool>());
        break;
      }
      case RedfishProperty::DOUBLE: {
        if (!json_obj.is_number()) {
          std::string error_message = absl::StrCat(
              "Error querying property ", property.property(),
              " as a number from non number object: ", json_obj.dump());
          LOG(ERROR) << error_message;
          return absl::InvalidArgumentError(error_message);
        }
        property_out.set_double_value(json_obj.get<double>());
        break;
      }
      case RedfishProperty::INT64: {
        if (!json_obj.is_number()) {
          std::string error_message = absl::StrCat(
              "Error querying property ", property.property(),
              " as a number from non number object: ", json_obj.dump());
          LOG(ERROR) << error_message;
          return absl::InvalidArgumentError(error_message);
        }
        property_out.set_int64_value(json_obj.get<int64_t>());
        break;
      }
      case RedfishProperty::DATE_TIME_OFFSET: {
        absl::Time timevalue;
        if (!json_obj.is_string()) {
          std::string error_message = absl::StrCat(
              "Error querying property ", property.property(),
              " as a timestamp from non string object: ", json_obj.dump());
          LOG(ERROR) << error_message;
          return absl::InvalidArgumentError(error_message);
        }
        if (absl::ParseTime(kRFC3339DateTime, json_obj.get<std::string>(),
                            &timevalue, nullptr)) {
          absl::StatusOr<google::protobuf::Timestamp> timestamp =
              AbslTimeToProtoTime(timevalue);
          if (timestamp.ok()) {
            *property_out.mutable_timestamp_value() = std::move(*timestamp);
          }
        }
        break;
      }
      default: {
        break;
      }
    }
  }
  return property_out;
}

}  // namespace

NormalizerImplDefault::NormalizerImplDefault()
    : additional_properties_(GetAdditionalProperties()) {}

absl::Status NormalizerImplDefault::Normalize(
    const RedfishObject &redfish_object,
    const DelliciusQuery::Subquery &subquery, SubqueryDataSet &data_set_local,
    const NormalizerOptions &normalizer_options) {
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
    if (!(property.name().empty())) {
      property_out->set_name(property.name());
    } else {
      std::string prop_name = property.property();
      absl::StrReplaceAll({{"\\.", "."}}, &prop_name);
      property_out->set_name(std::move(prop_name));
    }
    *data_set_local.add_properties() = std::move(*property_out);
  }

  // We add additional properties to populate stable id based on Redfish
  // Location. Only add stable name if a LocationContext is present.
  bool is_sub_fru = false;
  std::string sub_fru_name;
  for (const DelliciusQuery::Subquery::RedfishProperty &property :
       additional_properties_) {
    auto property_out = GetPropertyFromRedfishObject(json_content, property);
    if (!property_out.ok()) {
      continue;
    }
    absl::string_view name = property.name();
    if (name == kServiceLabel) {
      *data_set_local.mutable_redfish_location()->mutable_service_label() =
          std::move(*property_out->mutable_string_value());
    } else if (name == kPartLocationContext) {
      *data_set_local.mutable_redfish_location()
           ->mutable_part_location_context() =
          std::move(*property_out->mutable_string_value());
    } else if (name == kLocalDevpath) {
      *data_set_local.mutable_devpath() =
          std::move(*property_out->mutable_string_value());
    } else if (name == kEmbeddedLocationContext) {
      for (const auto &value : property_out->collection_value().values()) {
        *data_set_local.mutable_redfish_location()
             ->mutable_embedded_location_context()
             ->Add() = value.string_value();
      }
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
    *data_set_local.mutable_redfish_location()->mutable_stable_name() =
        std::move(sub_fru_name);
  }

  if (normalizer_options.enable_url_annotation) {
    std::string odata_id;
    if (redfish_object[ecclesia::PropertyOdataId::Name].GetValue(&odata_id)) {
      data_set_local.set_uri_annotation(std::move(odata_id));
    }
  }
  return absl::OkStatus();
}

absl::Status NormalizerImplAddDevpath::Normalize(
    const RedfishObject &redfish_object,
    const DelliciusQuery::Subquery &subquery, SubqueryDataSet &data_set,
    const NormalizerOptions &normalizer_options) {
  // Prioritize devpath populated by default normalizer.
  if (data_set.has_devpath()) {
    return absl::OkStatus();
  }

  // Derive devpath from Node Topology (URI to local devpath map).
  std::optional<std::string> devpath =
      GetDevpathForObjectAndNodeTopology(redfish_object, topology_);
  if (devpath.has_value()) {
    data_set.set_devpath(*devpath);
  }
  return absl::OkStatus();
}

}  // namespace ecclesia
