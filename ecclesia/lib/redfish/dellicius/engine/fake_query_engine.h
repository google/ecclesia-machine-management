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
#include <utility>
#include <vector>

#include "absl/functional/function_ref.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/passkey.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {

class FakeQueryEngine : public QueryEngineIntf {
 public:
  enum class Devpath : uint8_t { kEnable = 0, kDisable };
  enum class Metrics : uint8_t { kEnable = 0, kDisable };
  enum class Cache : uint8_t { kDisable = 0, kInfinite };

  struct Params {
    Devpath devpath = Devpath::kEnable;
    Metrics metrics = Metrics::kDisable;
    Cache cache = Cache::kInfinite;
  };

  static absl::StatusOr<std::unique_ptr<QueryEngineIntf>> Create(
      const QueryContext &query_context, absl::string_view mockup_name,
      Params params) {
    auto query_engine = absl::WrapUnique(new FakeQueryEngine(mockup_name));
    ECCLESIA_RETURN_IF_ERROR(
        query_engine->InitializeQueryEngine(query_context, params));
    return std::move(query_engine);
  }

  QueryIdToResult ExecuteRedpathQuery(
      absl::Span<const absl::string_view> query_ids,
      ServiceRootType service_root_uri,
      const QueryVariableSet &query_arguments) override {
    return query_engine_->ExecuteRedpathQuery(query_ids, service_root_uri,
                                              query_arguments);
  }

  void ExecuteQuery(
      absl::Span<const absl::string_view> query_ids,
      absl::FunctionRef<bool(const DelliciusQueryResult &result)> callback,
      ServiceRootType service_root_uri,
      const QueryVariableSet &query_arguments) override {
    query_engine_->ExecuteQuery(query_ids, callback, service_root_uri,
                                query_arguments);
  }

  std::vector<DelliciusQueryResult> ExecuteQuery(
      absl::Span<const absl::string_view> query_ids,
      ServiceRootType service_root_uri,
      const QueryVariableSet &query_arguments) override {
    return query_engine_->ExecuteQuery(query_ids, service_root_uri,
                                       query_arguments);
  }

  absl::StatusOr<RedfishInterface *> GetRedfishInterface(
      RedfishInterfacePasskey unused_passkey) override {
    return query_engine_->GetRedfishInterface(unused_passkey);
  }

 private:
  explicit FakeQueryEngine(absl::string_view mockup_name)
      : redfish_server_(mockup_name) {
    redfish_server_.EnableExpandGetHandler();
  }

  absl::Status InitializeQueryEngine(const QueryContext &query_context,
                                     Params params) {
    // Devpaths will not be generated if the stable_id_type is kRedfishLocation
    // without an IdAssigner passed to the Query Engine object.
    const QueryEngineParams::RedfishStableIdType stable_id_type =
        (params.devpath == Devpath::kEnable)
            ? QueryEngineParams::RedfishStableIdType::kRedfishLocationDerived
            : QueryEngineParams::RedfishStableIdType::kRedfishLocation;

    auto *cache_factory = (params.cache == Cache::kDisable)
                              ? NullCache::Create
                              : ([](RedfishTransport *transport) {
                                  return TimeBasedCache::Create(
                                      transport, absl::InfiniteDuration());
                                });

    ECCLESIA_ASSIGN_OR_RETURN(
        auto query_engine,
        QueryEngine::Create(
            query_context,
            {.transport = redfish_server_.RedfishClientTransport(),
             .cache_factory = cache_factory,
             .stable_id_type = stable_id_type,
             .feature_flags = {.enable_redfish_metrics =
                                   (params.metrics == Metrics::kEnable)}}));

    query_engine_ = std::move(query_engine);
    return absl::OkStatus();
  }

  FakeRedfishServer redfish_server_;
  std::unique_ptr<QueryEngineIntf> query_engine_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_FAKE_QUERY_ENGINE_H_