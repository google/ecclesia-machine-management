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

#ifndef ECCLESIA_LIB_REDFISH_TRANSPORT_MOCKED_INTERFACE_H_
#define ECCLESIA_LIB_REDFISH_TRANSPORT_MOCKED_INTERFACE_H_

#include "gmock/gmock.h"
#include "ecclesia/lib/redfish/transport/interface.h"

namespace ecclesia {

class RedfishTransportMock : public RedfishTransport {
 public:
  MOCK_METHOD(absl::string_view, GetRootUri, (), (override));
  MOCK_METHOD(absl::StatusOr<Result>, Get, (absl::string_view path),
              (override));
  MOCK_METHOD(absl::StatusOr<Result>, Post,
              (absl::string_view path, absl::string_view data), (override));
  MOCK_METHOD(absl::StatusOr<Result>, Patch,
              (absl::string_view path, absl::string_view data), (override));
  MOCK_METHOD(absl::StatusOr<Result>, Delete,
              (absl::string_view path, absl::string_view data), (override));
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_MOCKED_INTERFACE_H_
