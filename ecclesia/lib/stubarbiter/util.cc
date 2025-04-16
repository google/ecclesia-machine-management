/*
 * Copyright 2025 Google LLC
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

#include "ecclesia/lib/stubarbiter/util.h"

#include <string>

#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/stubarbiter/arbiter.h"

namespace ecclesia {

std::string PriorityLabelToString(
    StubArbiterInfo::PriorityLabel priority_label) {
  switch (priority_label) {
    case StubArbiterInfo::PriorityLabel::kPrimary:
      return "Primary";
    case StubArbiterInfo::PriorityLabel::kSecondary:
      return "Secondary";
    default:
      return "Unknown";
  }
}

StubArbiterInfo::PriorityLabel TransportPriorityToPriorityLabel(
    TransportPriority transport_priority) {
  switch (transport_priority) {
    case TRANSPORT_PRIORITY_PRIMARY:
      return StubArbiterInfo::PriorityLabel::kPrimary;
    case TRANSPORT_PRIORITY_SECONDARY:
      return StubArbiterInfo::PriorityLabel::kSecondary;
    default:
      return StubArbiterInfo::PriorityLabel::kUnknown;
  }
}

}  // namespace ecclesia
