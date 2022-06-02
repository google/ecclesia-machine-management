/*
 * Copyright 2021 Google LLC
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

#ifndef ECCLESIA_LIB_IPMI_IPMI_MANAGER_MOCK_H_
#define ECCLESIA_LIB_IPMI_IPMI_MANAGER_MOCK_H_

#include <memory>

#include "gmock/gmock.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "ecclesia/lib/ipmi/ipmi_handle.h"
#include "ecclesia/lib/ipmi/ipmi_interface_params.pb.h"
#include "ecclesia/lib/ipmi/ipmi_manager.h"

namespace ecclesia {

class MockIpmiManager : public IpmiManager {
 public:
  MockIpmiManager() {}

  MOCK_METHOD(absl::StatusOr<std::unique_ptr<IpmiHandle>>, Acquire,
              (const IpmiInterfaceParams &params, absl::Duration timeout),
              (override));

  MOCK_METHOD(absl::Status, Close,
              (const IpmiInterfaceParams &params, absl::Duration timeout),
              (override));

  MOCK_METHOD(void, EnableGlobalIpmiLogging, (), (override));
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IPMI_IPMI_MANAGER_MOCK_H_
