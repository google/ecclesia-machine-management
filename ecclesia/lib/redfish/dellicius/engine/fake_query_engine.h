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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_FAKE_QUERY_ENGINE_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_FAKE_QUERY_ENGINE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/passkey.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_engine_features.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_spec.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_router/query_router_spec.pb.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {

class FakeQueryEngine : public QueryEngineIntf {
 public:
  using QueryEngineIntf::ExecuteRedpathQuery;
  using QueryEngineIntf::ExecuteSubscriptionQuery;

  enum class DevpathMethod : uint8_t { kDevpath2 = 0, kDevpath3 };
  enum class Metrics : uint8_t { kEnable = 0, kDisable };
  enum class Annotations : uint8_t { kEnable = 0, kDisable };
  enum class Cache : uint8_t { kDisable = 0, kInfinite };
  enum class Streaming : uint8_t { kEnable = 0, kDisable };
  enum class FailOnFirstError : uint8_t { kEnable = 0, kDisable };

  struct Params {
    // Method to use for constructing/discovering devpaths. This defaults to
    // using Devpath2 (crawling the Redfish tree) to discover devpaths.
    DevpathMethod devpath_method = DevpathMethod::kDevpath2;
    Metrics metrics = Metrics::kDisable;
    Annotations annotations = Annotations::kDisable;
    Cache cache = Cache::kInfinite;
    std::optional<std::string> entity_tag;
    std::unique_ptr<IdAssigner> id_assigner;
    Streaming streaming = Streaming::kDisable;
    FailOnFirstError fail_on_first_error = FailOnFirstError::kDisable;
  };

  static absl::StatusOr<std::unique_ptr<FakeQueryEngine>> Create(
      QuerySpec query_spec, absl::string_view mockup_name, Params params) {
    auto query_engine = absl::WrapUnique(new FakeQueryEngine(mockup_name));
    ECCLESIA_RETURN_IF_ERROR(query_engine->InitializeQueryEngine(
        std::move(query_spec), std::move(params)));
    return std::move(query_engine);
  }

  FakeRedfishServer *GetFakeRedfishServer() { return &redfish_server_; }

  QueryIdToResult ExecuteRedpathQuery(
      absl::Span<const absl::string_view> query_ids,
      const RedpathQueryOptions &options) override {
    return query_engine_->ExecuteRedpathQuery(query_ids, options);
  }

  absl::StatusOr<SubscriptionQueryResult> ExecuteSubscriptionQuery(
      absl::Span<const absl::string_view> query_ids,
      const QueryVariableSet &query_arguments,
      StreamingOptions streaming_options) override {
    return query_engine_->ExecuteSubscriptionQuery(query_ids, query_arguments,
                                                   streaming_options);
  }

  absl::StatusOr<RedfishInterface *> GetRedfishInterface(
      RedfishInterfacePasskey unused_passkey) override {
    return query_engine_->GetRedfishInterface(unused_passkey);
  }

  absl::Status ExecuteOnRedfishInterface(
      RedfishInterfacePasskey unused_passkey,
      const RedfishInterfaceOptions &options) override {
    return query_engine_->ExecuteOnRedfishInterface(unused_passkey, options);
  }

  absl::string_view GetAgentIdentifier() const override {
    return query_engine_->GetAgentIdentifier();
  }

  void CancelQueryExecution(absl::Notification *notification) override {
    query_engine_->CancelQueryExecution(notification);
  }

 private:
  explicit FakeQueryEngine(absl::string_view mockup_name)
      : redfish_server_(mockup_name) {
    redfish_server_.EnableAllParamsGetHandler();
  }

  absl::Status InitializeQueryEngine(QuerySpec query_spec, Params params) {
    // Devpaths will not be generated if the stable_id_type is kRedfishLocation
    // without an IdAssigner passed to the Query Engine object.
    const QueryEngineParams::RedfishStableIdType stable_id_type =
        (params.devpath_method == DevpathMethod::kDevpath2)
            ? QueryEngineParams::RedfishStableIdType::kRedfishLocationDerived
            : QueryEngineParams::RedfishStableIdType::kRedfishLocation;

    auto *cache_factory = (params.cache == Cache::kDisable)
                              ? NullCache::Create
                              : ([](RedfishTransport *transport) {
                                  return TimeBasedCache::CreateDeepCache(
                                      transport, absl::InfiniteDuration());
                                });

    QueryEngineFeatures features = DefaultQueryEngineFeatures();
    features.set_enable_url_annotation(params.annotations ==
                                       Annotations::kEnable);
    features.set_enable_streaming(params.streaming == Streaming::kEnable);
    features.set_enable_redfish_metrics(params.metrics == Metrics::kEnable);
    features.set_fail_on_first_error(params.fail_on_first_error ==
                                     FailOnFirstError::kEnable);

    std::unique_ptr<QueryEngineIntf> query_engine;
    ECCLESIA_ASSIGN_OR_RETURN(
        query_engine,
        QueryEngine::Create(
            std::move(query_spec),
            {.transport = redfish_server_.RedfishClientTransport(),
             .cache_factory = cache_factory,
             .entity_tag = params.entity_tag.value_or(""),
             .stable_id_type = stable_id_type,
             .features = std::move(features)},
            std::move(params.id_assigner)));
    query_engine_ = std::move(query_engine);
    return absl::OkStatus();
  }

  FakeRedfishServer redfish_server_;
  std::unique_ptr<QueryEngineIntf> query_engine_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_FAKE_QUERY_ENGINE_H_
