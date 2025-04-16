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

#include "gtest/gtest.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/stubarbiter/arbiter.h"

namespace ecclesia {
namespace {

TEST(PriorityLabelToString, ReturnsCorrectString) {
  EXPECT_EQ(PriorityLabelToString(StubArbiterInfo::PriorityLabel::kPrimary),
            "Primary");
  EXPECT_EQ(PriorityLabelToString(StubArbiterInfo::PriorityLabel::kSecondary),
            "Secondary");
}

TEST(PriorityLabelToString, ReturnsUnknownForInvalidPriorityLabel) {
  EXPECT_EQ(PriorityLabelToString(StubArbiterInfo::PriorityLabel::kUnknown),
            "Unknown");
}

TEST(TransportPriorityToPriorityLabel, ReturnsCorrectPriorityLabel) {
  EXPECT_EQ(TransportPriorityToPriorityLabel(
                TransportPriority::TRANSPORT_PRIORITY_PRIMARY),
            StubArbiterInfo::PriorityLabel::kPrimary);
  EXPECT_EQ(TransportPriorityToPriorityLabel(
                TransportPriority::TRANSPORT_PRIORITY_SECONDARY),
            StubArbiterInfo::PriorityLabel::kSecondary);
}

TEST(TransportPriorityToPriorityLabel, ReturnsUnknownForInvalidPriorityLabel) {
  EXPECT_EQ(TransportPriorityToPriorityLabel(
                TransportPriority::TRANSPORT_PRIORITY_UNKNOWN),
            StubArbiterInfo::PriorityLabel::kUnknown);
}

}  // namespace
}  // namespace ecclesia
