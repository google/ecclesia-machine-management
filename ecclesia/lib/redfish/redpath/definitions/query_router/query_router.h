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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ROUTER_QUERY_ROUTER_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ROUTER_QUERY_ROUTER_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_router/query_router_spec.pb.h"
#include "ecclesia/lib/redfish/transport/interface.h"

namespace ecclesia {

// Interface for Query Router. Provides interface methods for executing Redpath
// queries and structures to augment query results from the Router.
class QueryRouterIntf {
 public:
  // The callback structure will include information identifying the data's
  // source.
  struct ServerInfo {
    std::string server_tag;
    SelectionSpec::SelectionClass::ServerType server_type;

    template <typename H>
    friend H AbslHashValue(H h, const ServerInfo& s) {
      return H::combine(std::move(h), s.server_tag, s.server_type);
    }

    bool operator==(const ServerInfo& other) const {
      return server_tag == other.server_tag && server_type == other.server_type;
    }
  };

  using ResultCallback = std::function<void(const ServerInfo& /* server_info */,
                                            QueryResult /* result */)>;

  virtual ~QueryRouterIntf() = default;

  // Overloaded function for executing non-templated queries
  void ExecuteQuery(absl::Span<const absl::string_view> query_ids,
                    const ResultCallback& callback) const {
    ExecuteQuery(query_ids, {}, callback);
  }

  // Pure-virtual functions that must be overridden by concrete implementations.
  virtual void ExecuteQuery(
      absl::Span<const absl::string_view> query_ids,
      const QueryEngineIntf::QueryVariableSet& query_arguments,
      const ResultCallback& callback) const = 0;
};

// Concrete implementation of Query Router Interface that routes the queries
// to the appropriate query engine.
class QueryRouter : public QueryRouterIntf {
 public:
  // Defines the specification of a server that will be supplied by the users of
  // this class. This defines all the servers that the QueryRouter can
  // communicate with.
  struct ServerSpec {
    ServerInfo server_info;
    QueryEngineParams::RedfishStableIdType stable_id_type =
        QueryEngineParams::RedfishStableIdType::kRedfishLocation;
    std::unique_ptr<RedfishTransport> transport = nullptr;
    std::unique_ptr<IdAssigner> id_assigner = nullptr;
    std::optional<std::string> node_local_system_id = std::nullopt;

    ServerSpec() = default;
    ServerSpec(const ServerSpec&) = delete;
    ServerSpec& operator=(const ServerSpec&) = delete;
    ServerSpec(ServerSpec&&) = default;
    ServerSpec& operator=(ServerSpec&&) = default;
  };

  // Factory method to create a QueryRouter instance. The servers involved in
  // the instance will be an intersection of the Servers specified in the
  // QueryRouterSpec and the list of ServerSpecs specified.
  static absl::StatusOr<std::unique_ptr<QueryRouterIntf>> Create(
      const QueryRouterSpec& router_spec, std::vector<ServerSpec> server_specs,
      QueryEngineFactory query_engine_factory = QueryEngine::Create);

  ~QueryRouter() override = default;

  // Execute the list of queries as per the QueryRouterSpec. `callback` will be
  // called for each QueryResult.
  void ExecuteQuery(absl::Span<const absl::string_view> query_ids,
                    const QueryEngineIntf::QueryVariableSet& query_arguments,
                    const ResultCallback& callback) const override;

 private:
  // Defines the core elements of the routing table - server info, query engine
  // and the query ids(applicable to this query engine)
  struct QueryRoutingInfo {
    ServerInfo server_info;
    std::unique_ptr<QueryEngineIntf> query_engine;
    absl::flat_hash_set<std::string> query_ids;
    std::optional<std::string> node_local_system_id = std::nullopt;
  };

  using RoutingTable = std::vector<QueryRoutingInfo>;

  explicit QueryRouter(RoutingTable routing_table)
      : routing_table_(std::move(routing_table)) {}

  RoutingTable routing_table_;
};

// Factory for creating different variants of query router.
// Ideally used in tests to inject different variants of query router
using QueryRouterFactory = absl::AnyInvocable<
    absl::StatusOr<std::unique_ptr<ecclesia::QueryRouterIntf>>(
        const ecclesia::QueryRouterSpec&,
        std::vector<ecclesia::QueryRouter::ServerSpec>,
        ecclesia::QueryEngineFactory)>;

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ROUTER_QUERY_ROUTER_H_
