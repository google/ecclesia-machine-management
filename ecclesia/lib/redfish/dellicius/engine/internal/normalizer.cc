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
#include <string_view>
#include <utility>
#include <vector>

#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/path_util.h"
#include "ecclesia/lib/redfish/devpath.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/time/proto.h"

namespace ecclesia {

namespace {

constexpr absl::string_view kServiceLabel = "__ServiceLabel__";
constexpr absl::string_view kPartLocationContext = "__PartLocationContext__";
constexpr absl::string_view kLocalDevpath = "__LocalDevpath__";

std::vector<DelliciusQuery::Subquery::RedfishProperty>
GetAdditionalProperties() {
  std::vector<DelliciusQuery::Subquery::RedfishProperty> result;
  auto add_property = [&](absl::string_view name,
                          std::vector<std::string> properties) {
    for (std::string &property : properties) {
      DelliciusQuery::Subquery::RedfishProperty new_prop;
      new_prop.set_name(std::string(name));
      new_prop.set_property(std::move(property));
      new_prop.set_type(DelliciusQuery::Subquery::RedfishProperty::STRING);
      result.push_back(std::move(new_prop));
    }
  };

  add_property(kServiceLabel, {"Location.PartLocation.ServiceLabel",
                               "PhysicalLocation.PartLocation.ServiceLabel"});
  add_property(kPartLocationContext, {"Location.PartLocationContext",
                                      "PhysicalLocation.PartLocationContext"});
  add_property(kLocalDevpath, {"Location.Oem.Google.Devpath",
                               "PhysicalLocation.Oem.Google.Devpath"});

  return result;
}

absl::StatusOr<SubqueryDataSet::Property> GetPropertyFromRedfishObject(
    const RedfishObject &redfish_object,
    const DelliciusQuery::Subquery::RedfishProperty &property) {
  // A property requirement can specify nested nodes like
  // 'Thresholds.UpperCritical.Reading' or a simple property like 'Name'.
  // We will split the property name to ensure we process all node names in
  // the property expression.
  ECCLESIA_ASSIGN_OR_RETURN(
      nlohmann::json json_obj,
      ResolveNodeNameToJsonObj(redfish_object, property.property()));

  using RedfishProperty = DelliciusQuery::Subquery::RedfishProperty;

  SubqueryDataSet::Property property_out;
  switch (property.type()) {
    case RedfishProperty::STRING: {
      if (json_obj.is_string()) {
        property_out.set_string_value(json_obj.get<std::string>());
      }
      break;
    }
    case RedfishProperty::BOOLEAN: {
      if (json_obj.is_boolean()) {
        property_out.set_boolean_value(json_obj.get<bool>());
      }
      break;
    }
    case RedfishProperty::DOUBLE: {
      if (json_obj.is_number()) {
        property_out.set_double_value(json_obj.get<double>());
      }
      break;
    }
    case RedfishProperty::INT64: {
      if (json_obj.is_number()) {
        property_out.set_int64_value(json_obj.get<int64_t>());
      }
      break;
    }
    case RedfishProperty::DATE_TIME_OFFSET: {
      absl::Time timevalue;
      if (!json_obj.is_string()) {
        break;
      }
      if (absl::ParseTime("%Y-%m-%dT%H:%M:%S%Z", json_obj.get<std::string>(),
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
  if (!property_out.value_case()) {
    return absl::InvalidArgumentError("Invalid property type");
  }
  return property_out;
}

}  // namespace

NormalizerImplDefault::NormalizerImplDefault()
    : additional_properties_(GetAdditionalProperties()) {}

absl::Status NormalizerImplDefault::Normalize(
    const RedfishObject &redfish_object,
    const DelliciusQuery::Subquery &subquery,
    SubqueryDataSet &data_set_local) const {
  for (const DelliciusQuery::Subquery::RedfishProperty &property :
       subquery.properties()) {
    auto property_out = GetPropertyFromRedfishObject(redfish_object, property);
    // It is not an error if normalizer fails to normalize a property if
    // required property is not part of Resource attributes.
    if (!property_out.ok()) {
      continue;
    }
    // By default, name of the queried property is set as name if the client
    // application does not provide a name to map the parsed property to.
    if (property.has_name()) {
      property_out->set_name(property.name());
    } else {
      std::string prop_name = property.property();
      absl::StrReplaceAll({{"\\.", "."}}, &prop_name);
      property_out->set_name(std::move(prop_name));
    }
    *data_set_local.add_properties() = std::move(*property_out);
  }

  // We add additional properties to populate stable id based on Redfish
  // Location.
  for (const DelliciusQuery::Subquery::RedfishProperty &property :
       additional_properties_) {
    auto property_out = GetPropertyFromRedfishObject(redfish_object, property);
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
    }
  }
  return absl::OkStatus();
}

absl::Status NormalizerImplAddDevpath::Normalize(
    const RedfishObject &redfish_object,
    const DelliciusQuery::Subquery &subquery, SubqueryDataSet &data_set) const {
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
