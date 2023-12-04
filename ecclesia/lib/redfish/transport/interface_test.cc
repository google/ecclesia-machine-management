/*
 * Copyright 2023 Google LLC
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

#include "ecclesia/lib/redfish/transport/interface.h"

#include <memory>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

TEST(NullTransport, AllRpcReturnsInternalError) {
  std::unique_ptr<RedfishTransport> transport =
      std::make_unique<NullTransport>();
  EXPECT_THAT(transport->Get(""), ecclesia::IsStatusInternal());
  EXPECT_THAT(transport->Post("", ""), ecclesia::IsStatusInternal());
  EXPECT_THAT(transport->Patch("", ""), ecclesia::IsStatusInternal());
  EXPECT_THAT(transport->Delete("", ""), ecclesia::IsStatusInternal());
  EXPECT_THAT(transport->Patch("", ""), ecclesia::IsStatusInternal());

  absl::AnyInvocable<void(const absl::Status &) const> on_stop =
      [](const absl::Status &) {};

  RedfishTransport::EventCallback on_event =
      [](const RedfishTransport::Result &) {};
  EXPECT_THAT(transport->Subscribe("", std::move(on_event), on_stop),
              ecclesia::IsStatusInternal());
}

}  // namespace
}  // namespace ecclesia
