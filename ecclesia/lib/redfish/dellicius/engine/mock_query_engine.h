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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_MOCK_QUERY_ENGINE_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_MOCK_QUERY_ENGINE_H_

#include "gmock/gmock.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/passkey.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.h"

namespace ecclesia {

class MockQueryEngine : public QueryEngineIntf {
 public:
  virtual ~MockQueryEngine() = default;

  MOCK_METHOD(absl::StatusOr<SubscriptionQueryResult>, ExecuteSubscriptionQuery,
              (absl::Span<const absl::string_view>, const QueryVariableSet &,
               StreamingOptions),
              (override));

  MOCK_METHOD(QueryIdToResult, ExecuteRedpathQuery,
              (absl::Span<const absl::string_view>,
               const RedpathQueryOptions &),
              (override));

  MOCK_METHOD(absl::StatusOr<RedfishInterface *>, GetRedfishInterface,
              (RedfishInterfacePasskey), (override));

  MOCK_METHOD(absl::Status, ExecuteOnRedfishInterface,
              (RedfishInterfacePasskey, const RedfishInterfaceOptions &),
              (override));

  MOCK_METHOD(absl::string_view, GetAgentIdentifier, (), (const, override));

  MOCK_METHOD(void, CancelQueryExecution, (absl::Notification *), (override));
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_MOCK_QUERY_ENGINE_H_
