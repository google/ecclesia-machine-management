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

#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/time/proto.h"

namespace ecclesia {

void DefaultNormalizer::operator()(const RedfishVariant &var,
                                const DelliciusQuery::Subquery &subquery,
                                DelliciusQueryResult &output) const {
  for (const auto &property_requirement : subquery.properties()) {
    DelliciusQueryResult::SubqueryOutput::SubqueryData response;
    // Check the redfish payload for the property listed in data model.
    RedfishVariant payload = var[property_requirement.property()];
    // TODO (b/241784544): Add logic to infer type from csdl.
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
      auto* subquery_to_output = output.mutable_subquery_output_by_id();
      (*subquery_to_output)[subquery.subquery_id()].mutable_data()->Add(
          std::move(response));
    }
  }
}

}  // namespace ecclesia
