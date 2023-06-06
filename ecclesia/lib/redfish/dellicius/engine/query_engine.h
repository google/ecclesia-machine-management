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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_QUERY_ENGINE_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_QUERY_ENGINE_H_

#include <memory>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/dellicius/engine/config.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

// QueryEngine is logical composition of interpreter, dispatcher and normalizer.
// A client application builds QueryEngine for a finite set of Dellicius Queries
// and optional feature flags encapsulated in a QueryEngineConfiguration object.
// Example Usage:
//   std::unique_ptr<RedfishInterface> intf = ...;
//   QueryEngineConfiguration config{
//       .flags{.enable_devpath_extension = true,
//              .enable_cached_uri_dispatch = false},
//       .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}};
//   QueryEngine query_engine(config, &clock, std::move(intf));
//   std::vector<DelliciusQueryResult> response_entries =
//       query_engine.ExecuteQuery({"SensorCollector"});
class QueryEngine {
 public:
  enum class ServiceRootType { kRedfish, kGoogle };
  // Interface for private implementation of Query Engine using PImpl Idiom
  class QueryEngineIntf {
   public:
    virtual ~QueryEngineIntf() = default;
    virtual std::vector<DelliciusQueryResult> ExecuteQuery(
        ServiceRootType service_root_uri,
        absl::Span<const absl::string_view> query_ids) = 0;
    virtual std::vector<DelliciusQueryResult> ExecuteQuery(
        ServiceRootType service_root_uri,
        absl::Span<const absl::string_view> query_ids,
        QueryTracker &tracker) = 0;
    virtual const NodeTopology &GetTopology() = 0;
  };

  // Default RedfishTransportCacheFactory that creates a NullCache (no caching).
  static std::unique_ptr<RedfishCachedGetterInterface> CreateNullCache(
      RedfishTransport *transport) {
    return std::make_unique<ecclesia::NullCache>(transport);
  }

  QueryEngine(const QueryEngineConfiguration &config,
              std::unique_ptr<RedfishTransport> transport,
              RedfishTransportCacheFactory cache_factory = CreateNullCache,
              const Clock *clock = Clock::RealClock());
  QueryEngine(const QueryEngine &) = delete;
  QueryEngine &operator=(const QueryEngine &) = delete;
  QueryEngine(QueryEngine &&other) = default;
  QueryEngine &operator=(QueryEngine &&other) = default;

  std::vector<DelliciusQueryResult> ExecuteQuery(
      absl::Span<const absl::string_view> query_ids,
      ServiceRootType service_root_uri = ServiceRootType::kRedfish) {
    return engine_impl_->ExecuteQuery(service_root_uri, query_ids);
  }
  std::vector<DelliciusQueryResult> ExecuteQuery(
      absl::Span<const absl::string_view> query_ids, QueryTracker &tracker,
      ServiceRootType service_root_uri = ServiceRootType::kRedfish) {
    return engine_impl_->ExecuteQuery(service_root_uri, query_ids, tracker);
  }
  const NodeTopology &GetTopology() { return engine_impl_->GetTopology(); }

 private:
  std::unique_ptr<QueryEngineIntf> engine_impl_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_QUERY_ENGINE_H_
