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
#include <utility>

#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/time/proto.h"

namespace ecclesia {

absl::StatusOr<SubqueryDataSet> DefaultNormalizer::Normalize(
    const RedfishVariant &var,
    const DelliciusQuery::Subquery &subquery) const {
  auto data_set_local = SubqueryDataSet();
  for (const auto &property_requirement : subquery.properties()) {
    SubqueryDataSet::SubqueryData response;
    // Check the redfish payload for the property listed in data model.
    RedfishVariant payload = var[property_requirement.property()];
    using RedfishProperty = DelliciusQuery::Subquery::RedfishProperty;
    switch (property_requirement.type()) {
      case RedfishProperty::STRING: {
        std::string stringvalue;
        if (payload.GetValue(&stringvalue)) {
          response.set_string_value(stringvalue);
        }
        break;
      }
      case RedfishProperty::BOOLEAN: {
        bool boolvalue;
        if (payload.GetValue(&boolvalue)) {
          response.set_boolean_value(boolvalue);
        }
        break;
      }
      case RedfishProperty::DOUBLE: {
        double doublevalue;
        if (payload.GetValue(&doublevalue)) {
          response.set_double_value(doublevalue);
        }
        break;
      }
      case RedfishProperty::INT64: {
        int64_t intvalue;
        if (payload.GetValue(&intvalue)) {
          response.set_int64_value(intvalue);
        }
        break;
      }
      case RedfishProperty::DATE_TIME_OFFSET: {
        absl::Time timevalue;
        if (payload.GetValue(&timevalue)) {
          absl::StatusOr<
              google::protobuf::Timestamp> timestamp = AbslTimeToProtoTime(
                  timevalue);
          if (timestamp.ok()) {
            *response.mutable_timestamp_value() = std::move(*timestamp);
          }
        }
        break;
      }
      default: { break; }
    }
    if (response.value_case()) {
      // By default, name of the queried property is set as name if the client
      // application does not provide a name to map the parsed property to.
      if (property_requirement.has_name()) {
        response.set_name(property_requirement.name());
      }
      response.set_name(property_requirement.property());
      *data_set_local.add_data() = std::move(response);
    }
  }
  if (data_set_local.data().empty()) {
    return absl::NotFoundError(
        "Redfish object does not have any of the required properties");
  }
  return data_set_local;
}

absl::StatusOr<SubqueryDataSet> NormalizerDevpathDecorator::Normalize(
    const RedfishVariant &var,
    const DelliciusQuery::Subquery &subquery) const {
  absl::StatusOr<SubqueryDataSet> normalized_data
      = default_normalizer_->Normalize(var, subquery);
  if (!normalized_data.ok()) return normalized_data;
  std::optional<std::string> odata_id = var.AsObject()->GetUriString();
  if (odata_id.has_value()) {
    auto it = topology_.uri_to_associated_node_map.find(odata_id.value());
    if (it != topology_.uri_to_associated_node_map.end()) {
      if (!it->second.empty()) {
        normalized_data.value().set_devpath(it->second[0]->local_devpath);
        return normalized_data;
      }
    }
  }
  return normalized_data;
}

}  // namespace ecclesia
