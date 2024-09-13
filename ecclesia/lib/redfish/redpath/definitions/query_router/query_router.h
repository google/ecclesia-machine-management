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
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/die_if_null.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/passkey.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/interface.h"
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
    SelectionSpec::SelectionClass::ServerClass server_class;

    template <typename H>
    friend H AbslHashValue(H h, const ServerInfo &s) {
      return H::combine(std::move(h), s.server_tag, s.server_type,
                        s.server_class);
    }

    bool operator==(const ServerInfo &other) const {
      return std::tie(server_tag, server_type, server_class) ==
             std::tie(other.server_tag, other.server_type, other.server_class);
    }
  };

  using ResultCallback = std::function<void(
      const ServerInfo &/* server_info */, QueryResult /* result */)>;

  virtual ~QueryRouterIntf() = default;

  // Overloaded function for executing non-templated queries
  void ExecuteQuery(absl::Span<const absl::string_view> query_ids,
                    const ResultCallback &callback) const {
    ExecuteQuery(query_ids, {}, callback);
  }

  // Pure-virtual functions that must be overridden by concrete implementations.
  virtual void ExecuteQuery(
      absl::Span<const absl::string_view> query_ids,
      const QueryEngineIntf::QueryVariableSet &query_arguments,
      const ResultCallback &callback) const = 0;

  virtual absl::StatusOr<RedfishInterface *> GetRedfishInterface(
      const ServerInfo &server_info,
      RedfishInterfacePasskey unused_passkey) const = 0;
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
    std::unique_ptr<RedfishTransport> transport = nullptr;
    std::unique_ptr<IdAssigner> id_assigner = nullptr;
    std::optional<std::string> node_local_system_id = std::nullopt;
    // Gets set during QueryRouter initialization, based on the StableIdConfig
    // declared.
    QueryEngineParams::RedfishStableIdType stable_id_type =
        QueryEngineParams::RedfishStableIdType::kRedfishLocation;
    // When initializing QueryRouter using QueryRouterBuilder, this
    // field is set to prevent QueryRouter constructors from repeating parsing
    // of the stable id config.
    bool parsed_stable_id_type_from_spec = false;

    ServerSpec() = default;
    ServerSpec(const ServerSpec&) = delete;
    ServerSpec &operator=(const ServerSpec &) = delete;
    ServerSpec(ServerSpec&&) = default;
    ServerSpec &operator=(ServerSpec &&) = default;
  };

  // Factory method to create a QueryRouter instance. The servers involved in
  // the instance will be an intersection of the Servers specified in the
  // QueryRouterSpec and the list of ServerSpecs specified.
  static absl::StatusOr<std::unique_ptr<QueryRouterIntf>> Create(
      const QueryRouterSpec &router_spec, std::vector<ServerSpec> server_specs,
      QueryEngineFactory query_engine_factory = QueryEngine::Create);

  ~QueryRouter() override = default;

  // Execute the list of queries as per the QueryRouterSpec. `callback` will be
  // called for each QueryResult and is thread safe when the queries are
  // executed in parallel (PATTERN_SERIAL_AGENT, PATTERN_PARALLEL_ALL).
  // `callback` is guaranteed to be executed non-concurrently and clients don't
  // need to independently guard against concurrent access to any of their data
  // structures that are accessed by the callback.
  void ExecuteQuery(absl::Span<const absl::string_view> query_ids,
                    const QueryEngineIntf::QueryVariableSet &query_arguments,
                    const ResultCallback &callback) const override;

  // Returns the RedfishInterface for the given ServerInfo.
  // QueryEngineRawInterfacePasskey is just an empty strongly-typed object that
  // one needs to provide in order to get access to the underlying
  // RedfishInterface.
  absl::StatusOr<RedfishInterface *> GetRedfishInterface(
      const ServerInfo &server_info,
      RedfishInterfacePasskey unused_passkey) const override;

 private:
  // Defines the core elements of the routing table - server info, query engine
  // and the query ids(applicable to this query engine)
  struct QueryRoutingInfo {
    ServerInfo server_info;
    std::unique_ptr<QueryEngineIntf> query_engine;
    absl::flat_hash_set<std::string> query_ids;
    std::optional<std::string> node_local_system_id = std::nullopt;
  };

  // Defines the elements to be sent as a batch to the query engine for parallel
  // execution.
  struct QueryBatch {
    explicit QueryBatch(const QueryRoutingInfo* routing_info)
        : routing_info(*ABSL_DIE_IF_NULL(routing_info)) {}

    std::vector<absl::string_view> queries;
    const QueryRoutingInfo &routing_info;
  };

  using RoutingTable = std::vector<QueryRoutingInfo>;

  using ExecuteFunction = std::function<void(
      absl::Span<const absl::string_view>,
      const QueryEngineIntf::QueryVariableSet&, const ResultCallback&)>;

  QueryRouter(RoutingTable routing_table, QueryPattern query_pattern,
              int max_concurrent_threads);

  // All queries will be executed in series across all agents.
  void ExecuteQuerySerialAll(
      absl::Span<const absl::string_view> query_ids,
      const QueryEngineIntf::QueryVariableSet &query_arguments,
      const ResultCallback &callback) const;

  // All queries will be execute in series for an agent, but queries across
  // agents are executed in parallel
  void ExecuteQuerySerialAgent(
      absl::Span<const absl::string_view> query_ids,
      const QueryEngineIntf::QueryVariableSet &query_arguments,
      const ResultCallback &callback) const;

  // All queries will be executed in parallel across all agents.
  void ExecuteQueryParallelAll(
      absl::Span<const absl::string_view> query_ids,
      const QueryEngineIntf::QueryVariableSet &query_arguments,
      const ResultCallback &callback) const;

  // Each query set in the query_batches list is executed in its own individual
  // thread.
  void ExecuteQueryBatches(
      absl::Span<const QueryBatch> query_batches,
      const QueryEngineIntf::QueryVariableSet &query_arguments,
      const ResultCallback &callback) const;

  RoutingTable routing_table_;
  ExecuteFunction execute_function_;
  int max_concurrent_threads_;
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
