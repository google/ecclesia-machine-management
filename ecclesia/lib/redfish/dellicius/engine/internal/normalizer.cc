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

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "google/protobuf/timestamp.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_split.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/devpath.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/time/proto.h"

namespace ecclesia {

namespace {

RedfishVariant GetNestedObject(RedfishVariant &&var,
                               absl::Span<const std::string> nested_nodes) {
  if (nested_nodes.empty() || !var.AsObject()) {
    return std::move(var);
  }
  RedfishVariant nested_object = var[nested_nodes[0]];
  return GetNestedObject(std::move(nested_object), nested_nodes.subspan(1));
}

}  // namespace

absl::Status NormalizerImplDefault::Normalize(
    const RedfishVariant &var, const DelliciusQuery::Subquery &subquery,
    SubqueryDataSet &data_set_local) const {
  for (const auto &property_requirement : subquery.properties()) {
    SubqueryDataSet::Property property;
    // A property requirement can specify nested nodes like
    // 'Thresholds.UpperCritical.Reading' or a simple property like 'Name'.
    // We will split the property name to ensure we process all node names in
    // the property expression.
    std::vector<std::string> names;
    std::string_view node_name = property_requirement.property();
    if (auto pos = node_name.find("@odata."); pos != std::string::npos) {
      names = absl::StrSplit(node_name.substr(0, pos), '.', absl::SkipEmpty());
      names.push_back(std::string(node_name.substr(pos)));
    } else {
      names = absl::StrSplit(node_name, '.', absl::SkipEmpty());
    }
    // Check the redfish payload for the property listed in data model.
    RedfishVariant payload = var[names[0]];
    if (names.size() > 1) {
      payload = GetNestedObject(
          std::move(payload),
          absl::Span<const std::string>(&names[1], names.size() - 1));
    }
    using RedfishProperty = DelliciusQuery::Subquery::RedfishProperty;
    switch (property_requirement.type()) {
      case RedfishProperty::STRING: {
        std::string stringvalue;
        if (payload.GetValue(&stringvalue)) {
          property.set_string_value(stringvalue);
        }
        break;
      }
      case RedfishProperty::BOOLEAN: {
        bool boolvalue;
        if (payload.GetValue(&boolvalue)) {
          property.set_boolean_value(boolvalue);
        }
        break;
      }
      case RedfishProperty::DOUBLE: {
        double doublevalue;
        if (payload.GetValue(&doublevalue)) {
          property.set_double_value(doublevalue);
        }
        break;
      }
      case RedfishProperty::INT64: {
        int64_t intvalue;
        if (payload.GetValue(&intvalue)) {
          property.set_int64_value(intvalue);
        }
        break;
      }
      case RedfishProperty::DATE_TIME_OFFSET: {
        absl::Time timevalue;
        if (payload.GetValue(&timevalue)) {
          absl::StatusOr<google::protobuf::Timestamp> timestamp =
              AbslTimeToProtoTime(timevalue);
          if (timestamp.ok()) {
            *property.mutable_timestamp_value() = std::move(*timestamp);
          }
        }
        break;
      }
      default: {
        break;
      }
    }
    if (property.value_case()) {
      // By default, name of the queried property is set as name if the client
      // application does not provide a name to map the parsed property to.
      if (property_requirement.has_name()) {
        property.set_name(property_requirement.name());
      } else {
        property.set_name(property_requirement.property());
      }
      *data_set_local.add_properties() = std::move(property);
    }
  }
  return absl::OkStatus();
}

absl::Status NormalizerImplAddDevpath::Normalize(
    const RedfishVariant &var, const DelliciusQuery::Subquery &subquery,
    SubqueryDataSet &data_set) const {
  std::unique_ptr<RedfishObject> redfish_object = var.AsObject();
  if (redfish_object == nullptr) {
    return absl::OkStatus();
  }

  std::optional<std::string> devpath =
      GetDevpathForObjectAndNodeTopology(*redfish_object, topology_);
  if (devpath.has_value()) {
    data_set.set_devpath(*devpath);
  }
  return absl::OkStatus();
}

}  // namespace ecclesia
