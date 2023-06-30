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
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/path_util.h"
#include "ecclesia/lib/redfish/devpath.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/time/proto.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace ecclesia {

namespace {

const google::protobuf::FieldDescriptor *GetFieldDescriptor(
    const google::protobuf::Message &message, absl::string_view field_name) {
  std::vector<absl::string_view> field_path =
      absl::StrSplit(field_name, '.', absl::SkipEmpty());
  const google::protobuf::Descriptor *descriptor = message.GetDescriptor();
  const google::protobuf::FieldDescriptor *field = nullptr;
  for (const auto &sub_field : field_path) {
    field = descriptor->FindFieldByName(std::string(sub_field));
    if (field == nullptr) {
      continue;
    }
    descriptor = field->message_type();
  }
  return field;
}

// Adds Redfish property to subquery based on field options in given subquery.
// Returns name of the property added to the subquery.
std::string UpdateSubqueryFromFieldOptions(
    absl::string_view field_name, const SubqueryDataSet &data_set_local,
    DelliciusQuery::Subquery &subquery_local) {
  const google::protobuf::FieldDescriptor *field_descriptor =
      GetFieldDescriptor(data_set_local, field_name);
  const auto &properties =
      field_descriptor->options().GetExtension(query_options).properties();
  std::string property_label =
      field_descriptor->options().GetExtension(query_options).label();
  for (const auto &property : properties) {
    DelliciusQuery::Subquery::RedfishProperty property_requirement;
    property_requirement.set_property(property);
    property_requirement.set_name(property_label);
    property_requirement.set_type(
        DelliciusQuery::Subquery::RedfishProperty::STRING);
    subquery_local.mutable_properties()->Add(std::move(property_requirement));
  }
  return property_label;
}

}  // namespace

absl::Status NormalizerImplDefault::Normalize(
    const RedfishObject &redfish_object,
    const DelliciusQuery::Subquery &subquery,
    SubqueryDataSet &data_set_local) const {
  // Before mapping observed RedfishProperties to queried properties, we update
  // the property requirement in subquery to add additional properties to
  // populate stable id based on Redfish Location.
  DelliciusQuery::Subquery subquery_local = subquery;
  std::string service_label = UpdateSubqueryFromFieldOptions(
      "redfish_location.service_label", data_set_local, subquery_local);
  std::string part_location_context = UpdateSubqueryFromFieldOptions(
      "redfish_location.part_location_context", data_set_local, subquery_local);

  for (const auto &property_requirement : subquery_local.properties()) {
    SubqueryDataSet::Property property_out;
    absl::string_view property_name = property_requirement.property();

    // A property requirement can specify nested nodes like
    // 'Thresholds.UpperCritical.Reading' or a simple property like 'Name'.
    // We will split the property name to ensure we process all node names in
    // the property expression.
    absl::StatusOr<nlohmann::json> json_obj =
        ResolveNodeNameToJsonObj(redfish_object, property_name);
    if (!json_obj.ok()) {
      // It is not an error if normalizer fails to normalize a property if
      // required property is not part of Resource attributes.
      continue;
    }

    using RedfishProperty = DelliciusQuery::Subquery::RedfishProperty;
    switch (property_requirement.type()) {
      case RedfishProperty::STRING: {
        if (json_obj->is_string()) {
          property_out.set_string_value(json_obj->get<std::string>());
        }
        break;
      }
      case RedfishProperty::BOOLEAN: {
        if (json_obj->is_boolean()) {
          property_out.set_boolean_value(json_obj->get<bool>());
        }
        break;
      }
      case RedfishProperty::DOUBLE: {
        if (json_obj->is_number()) {
          property_out.set_double_value(json_obj->get<double>());
        }
        break;
      }
      case RedfishProperty::INT64: {
        if (json_obj->is_number()) {
          property_out.set_int64_value(json_obj->get<int64_t>());
        }
        break;
      }
      case RedfishProperty::DATE_TIME_OFFSET: {
        absl::Time timevalue;
        if (!json_obj->is_string()) {
          break;
        }
        if (absl::ParseTime("%Y-%m-%dT%H:%M:%S%Z", json_obj->get<std::string>(),
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
      continue;
    }

    // Populate RedfishLocation field in SubqueryDataSet.
    if (property_requirement.type() == RedfishProperty::STRING &&
        property_requirement.has_name() &&
        (property_requirement.name() == service_label ||
         property_requirement.name() == part_location_context)) {
      absl::string_view name = property_requirement.name();
      if (name == service_label) {
        *data_set_local.mutable_redfish_location()->mutable_service_label() =
            property_out.string_value();
      } else if (name == part_location_context) {
        *data_set_local.mutable_redfish_location()
             ->mutable_part_location_context() = property_out.string_value();
      }
      continue;
    }

    // By default, name of the queried property is set as name if the client
    // application does not provide a name to map the parsed property to.
    if (property_requirement.has_name()) {
      property_out.set_name(property_requirement.name());
    } else {
      std::string prop_name = property_requirement.property();
      absl::StrReplaceAll({{"\\.", "."}}, &prop_name);
      property_out.set_name(prop_name);
    }
    *data_set_local.add_properties() = std::move(property_out);
  }
  return absl::OkStatus();
}

absl::Status NormalizerImplAddDevpath::Normalize(
    const RedfishObject &redfish_object,
    const DelliciusQuery::Subquery &subquery, SubqueryDataSet &data_set) const {
  std::optional<std::string> devpath =
      GetDevpathForObjectAndNodeTopology(redfish_object, topology_);
  if (devpath.has_value()) {
    data_set.set_devpath(*devpath);
  }
  return absl::OkStatus();
}

absl::Status NormalizerImplAddMachineBarepath::Normalize(
    const RedfishObject &redfish_object,
    const DelliciusQuery::Subquery &subquery, SubqueryDataSet &data_set) const {
  absl::StatusOr<std::string> machine_devpath =
      id_assigner_.IdForRedfishLocationInDataSet(data_set);
  if (machine_devpath.ok()) {
    data_set.mutable_decorators()->set_machine_devpath(machine_devpath.value());
  }
  return absl::OkStatus();
}

}  // namespace ecclesia
