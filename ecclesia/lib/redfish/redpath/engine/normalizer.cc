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
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/dellicius/utils/path_util.h"
#include "ecclesia/lib/redfish/devpath.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/topology.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/time/proto.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

namespace {

constexpr absl::string_view kEmbeddedLocationContext =
    "__EmbeddedLocationContext__";
constexpr absl::string_view kServiceLabel = "__ServiceLabel__";
constexpr absl::string_view kPartLocationContext = "__PartLocationContext__";
constexpr absl::string_view kLocalDevpath = "__LocalDevpath__";
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
  // Implicit collection of Name property to be used for stable IDs.
  add_property(kServiceLabel,
               {"Location.PartLocation.ServiceLabel",
                "PhysicalLocation.PartLocation.ServiceLabel",
                "Oem.Google.Location.PartLocation.ServiceLabel",
                "Oem.Google.PhysicalLocation.PartLocation.ServiceLabel"});
  add_property(
      kPartLocationContext,
      {"Location.PartLocationContext", "PhysicalLocation.PartLocationContext",
       "Oem.Google.Location.PartLocationContext",
       "Oem.Google.PhysicalLocation.PartLocationContext"});
  add_property(
      kEmbeddedLocationContext,
      {"Location.Oem.Google.EmbeddedLocationContext",
       "PhysicalLocation.Oem.Google.EmbeddedLocationContext",
       "Oem.Google.Location.Oem.Google.EmbeddedLocationContext",
       "Oem.Google.PhysicalLocation.Oem.Google.EmbeddedLocationContext"});

  add_property(
      kLocalDevpath,
      {"Location.Oem.Google.Devpath", "PhysicalLocation.Oem.Google.Devpath",
       "Oem.Google.Location.Oem.Google.Devpath",
       "Oem.Google.PhysicalLocation.Oem.Google.Devpath"});

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
        if (absl::ParseTime(kRFC3339DateTime,
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
    if (json_obj.empty()) {
      return absl::NotFoundError(
          absl::StrCat("Encountered empty collection property value during ",
                       "normalization. Property: ", property.property(),
                       " JSON: ", json_obj.dump()));
    }
    ECCLESIA_RETURN_IF_ERROR(GetCollectionPropertyFromRedfishObject(
        property, json_obj, query_value));
  } else if (json_obj.is_null()) {
    if (!property.nullable()) {
      std::string error_message = absl::StrCat(
          "Encountered null property value during normalization. Property: ",
          property.property(), " JSON: ", json_obj.dump());
      LOG(INFO) << error_message;
      return absl::NotFoundError(error_message);
    }
    query_value.set_null_value(QueryValue::NULL_VALUE);
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
        if (absl::ParseTime(kRFC3339DateTime, json_obj.get<std::string>(),
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
    return absl::InvalidArgumentError(
        absl::StrCat("Property `", property.property(),
                     "` cannot be parsed from the redfish object: ",
                     redfish_content.dump()));
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
    absl::StatusOr<QueryValue> property_out =
        GetPropertyFromRedfishObject(json_content, property);
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
    if (!(property.name().empty())) {
      prop_name = property.name();
    } else {
      prop_name = property.property();
      absl::StrReplaceAll({{"\\.", "."}}, &prop_name);
    }

    (*data_set_local.mutable_fields())[prop_name] = *property_out;
  }

  // Identifier to house stable id properties for the query result normalized.
  Identifier identifier;

  // We add additional properties to populate stable id based on Redfish
  // Location.
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
    if (name == kServiceLabel) {
      identifier.mutable_redfish_location()->set_service_label(
          property_out->string_value());
    } else if (name == kPartLocationContext) {
      identifier.mutable_redfish_location()->set_part_location_context(
          property_out->string_value());
    } else if (name == kLocalDevpath) {
      identifier.set_local_devpath(property_out->string_value());
    } else if (name == kEmbeddedLocationContext) {
      identifier.set_embedded_location_context(property_out->string_value());
    }
  }

  // Add identifier to data set if it is not empty.
  if (identifier.ByteSizeLong() > 0) {
    *(*data_set_local.mutable_fields())[kIdentifierTag].mutable_identifier() =
        std::move(identifier);
  }

  // Add URI annotation to data set.
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
  QueryResultDataReader reader(&data_set_local);
  absl::StatusOr<QueryValueReader> query_value_reader =
      reader.Get(kIdentifierTag);
  // Prioritize devpath populated by default normalizer.
  if (query_value_reader.ok() &&
      query_value_reader->identifier().has_local_devpath()) {
    return absl::OkStatus();
  }

  // Derive devpath from Node Topology (URI to local devpath map).
  std::optional<std::string> devpath =
      GetDevpathForObjectAndNodeTopology(redfish_object, topology_);

  if (devpath.has_value()) {
    (*data_set_local.mutable_fields())[kIdentifierTag]
        .mutable_identifier()
        ->set_local_devpath(*devpath);
  }
  return absl::OkStatus();
}

absl::Status RedpathNormalizerImplAddMachineBarepath::Normalize(
    const RedfishObject &redfish_object,
    const DelliciusQuery::Subquery &subquery,
    ecclesia::QueryResultData &data_set_local,
    const RedpathNormalizerOptions &options) {
  QueryResultDataReader reader(&data_set_local);
  absl::StatusOr<QueryValueReader> query_value_reader =
      reader.Get(kIdentifierTag);
  std::string service_label;
  std::string part_location_context;
  if (query_value_reader.ok()) {
    service_label =
        query_value_reader->identifier().redfish_location().service_label();
    part_location_context = query_value_reader->identifier()
                                .redfish_location()
                                .part_location_context();
  }

  // Root devpath is assigned to the root Chassis, to do this we need to track
  // if the resource is Chassis type and has no redfish location.
  std::string resource_type;
  redfish_object.Get(ecclesia::PropertyOdataType::Name)
      .GetValue(&resource_type);
  bool is_root = absl::StrContains(resource_type, "#Chassis.") &&
                 service_label.empty() && part_location_context.empty();

  // Root Chassis may not have the identifier tag populated but by definition it
  // is a root. Need to populate it with empty redfish location.
  if (!query_value_reader.ok() && is_root) {
    *(*data_set_local.mutable_fields())[kIdentifierTag]
         .mutable_identifier() = {};
  }

  // Try to find and set a machine devpath, prioritizing local devpath.
  absl::StatusOr<std::string> machine_devpath;
  if (query_value_reader.ok() &&
      query_value_reader->identifier().has_local_devpath()) {
    machine_devpath =
        id_assigner_->IdForLocalDevpathInQueryResult(data_set_local);
  }
  if (use_redfish_location_as_fallback_ &&
      (!machine_devpath.ok() || machine_devpath->empty())) {
    machine_devpath = id_assigner_->IdForRedfishLocationInQueryResult(
        data_set_local, is_root);
  }
  if (machine_devpath.ok() && !machine_devpath->empty()) {
    (*data_set_local.mutable_fields())[kIdentifierTag]
        .mutable_identifier()
        ->set_machine_devpath(*machine_devpath);
  }
  return absl::OkStatus();
}

std::unique_ptr<RedpathNormalizer> BuildLocalDevpathRedpathNormalizer(
    RedfishInterface *redfish_interface,
    RedpathNormalizer::RedfishStableIdType stable_id_type,
    absl::string_view redfish_topology_config_name) {
  switch (stable_id_type) {
    case RedpathNormalizer::RedfishStableIdType::kRedfishLocation:
      return BuildDefaultRedpathNormalizer();
    case RedpathNormalizer::RedfishStableIdType::kRedfishLocationDerived:
      if (!redfish_topology_config_name.empty()) {
        return BuildDefaultRedpathNormalizerWithLocalDevpath(
            CreateTopologyFromRedfish(redfish_interface,
                                      redfish_topology_config_name));
      }
      return BuildDefaultRedpathNormalizerWithLocalDevpath(
          CreateTopologyFromRedfish(redfish_interface));
  }

  return nullptr;
}

std::unique_ptr<RedpathNormalizer> GetMachineDevpathRedpathNormalizer(
    RedpathNormalizer::RedfishStableIdType stable_id_type,
    absl::string_view redfish_topology_config_name,
    std::unique_ptr<IdAssigner> id_assigner,
    RedfishInterface *redfish_interface) {
  switch (stable_id_type) {
    case RedpathNormalizer::RedfishStableIdType::kRedfishLocation:
      return BuildRedpathNormalizerWithMachineDevpath(std::move(id_assigner));
    case RedpathNormalizer::RedfishStableIdType::kRedfishLocationDerived:
      if (redfish_topology_config_name.empty()) {
        return BuildRedpathNormalizerWithMachineDevpath(
            std::move(id_assigner),
            CreateTopologyFromRedfish(redfish_interface));
      }
      return BuildRedpathNormalizerWithMachineDevpath(
          std::move(id_assigner),
          CreateTopologyFromRedfish(redfish_interface,
                                    redfish_topology_config_name));
  }

  return nullptr;
}

}  // namespace ecclesia
