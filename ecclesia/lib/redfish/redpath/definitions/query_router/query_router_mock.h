/*
 * Copyright 2024 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ROUTER_QUERY_ROUTER_MOCK_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ROUTER_QUERY_ROUTER_MOCK_H_

#include <memory>

#include "gmock/gmock.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/passkey.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_router/query_router.h"

namespace ecclesia {

class QueryRouterMock : public QueryRouterIntf {
 public:
  ~QueryRouterMock() override = default;

  static std::unique_ptr<QueryRouterIntf> Create() {
    return std::make_unique<QueryRouterMock>();
  }

  MOCK_METHOD(void, ExecuteQuery,
              (const QueryRouterIntf::RedpathQueryOptions &),
              (const, override));

  MOCK_METHOD(absl::StatusOr<RedfishInterface *>, GetRedfishInterface,
              (const ServerInfo &, RedfishInterfacePasskey), (const, override));

  MOCK_METHOD(absl::Status, ExecuteOnRedfishInterface,
              (const ServerInfo &, RedfishInterfacePasskey,
               const QueryEngineIntf::RedfishInterfaceOptions &),
              (const, override));
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ROUTER_QUERY_ROUTER_MOCK_H_
