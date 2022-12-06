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

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/dellicius/engine/config.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
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
class QueryEngine final {
 public:
  // Interface for private implementation of Query Engine using PImpl Idiom
  class QueryEngineIntf {
   public:
    virtual ~QueryEngineIntf() = default;
    virtual std::vector<DelliciusQueryResult> ExecuteQuery(
        absl::Span<const absl::string_view> query_ids) = 0;
    virtual std::vector<DelliciusQueryResult> ExecuteQuery(
        absl::Span<const absl::string_view> query_ids,
        QueryTracker &tracker) = 0;
  };
  QueryEngine(const QueryEngineConfiguration &config, const Clock *clock,
              std::unique_ptr<RedfishInterface> intf);

  QueryEngine(const QueryEngine &) = delete;
  QueryEngine &operator=(const QueryEngine &) = delete;
  QueryEngine(QueryEngine &&other) = default;
  QueryEngine &operator=(QueryEngine &&other) = default;

  std::vector<DelliciusQueryResult> ExecuteQuery(
      absl::Span<const absl::string_view> query_ids) {
    return engine_impl_->ExecuteQuery(query_ids);
  }
  std::vector<DelliciusQueryResult> ExecuteQuery(
      absl::Span<const absl::string_view> query_ids, QueryTracker &tracker) {
    return engine_impl_->ExecuteQuery(query_ids, tracker);
  }

 private:
  std::unique_ptr<QueryEngineIntf> engine_impl_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_QUERY_ENGINE_H_
