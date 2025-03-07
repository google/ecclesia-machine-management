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

#include <stdbool.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/passkey.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/dellicius/engine/transport_arbiter_query_engine.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_router/query_router_spec.pb.h"
#include "ecclesia/lib/redfish/redpath/engine/normalizer.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/stubarbiter/arbiter.h"

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

    std::string ToString() const {
      return absl::StrCat(
          "server_tag: ", server_tag, ", server_type: ",
          SelectionSpec::SelectionClass::ServerType_Name(server_type),
          ", server_class: ",
          SelectionSpec::SelectionClass::ServerClass_Name(server_class));
    }
  };

  using ResultCallback = std::function<void(
      const ServerInfo & /* server_info */, QueryResult /* result */)>;

  virtual ~QueryRouterIntf() = default;

  struct RedpathQueryOptions {
    absl::Span<const absl::string_view> query_ids;
    QueryEngineIntf::QueryVariableSet query_arguments;
    StubArbiterInfo::PriorityLabel priority_label =
        StubArbiterInfo::PriorityLabel::kUnknown;
    const ResultCallback &callback;
  };

  // Overloaded function for executing non-templated queries
  // the new function.
  ABSL_DEPRECATED("Use ExecuteQuery with RedpathQueryOptions instead.")
  void ExecuteQuery(absl::Span<const absl::string_view> query_ids,
                    const ResultCallback &callback) const {
    RedpathQueryOptions options{
        .query_ids = query_ids,
        .callback = callback,
    };
    ExecuteQuery(options);
  }

  ABSL_DEPRECATED("Use ExecuteQuery with RedpathQueryOptions instead.")
  void ExecuteQuery(absl::Span<const absl::string_view> query_ids,
                    const QueryEngineIntf::QueryVariableSet &query_arguments,
                    const ResultCallback &callback) const {
    RedpathQueryOptions options{
        .query_ids = query_ids,
        .query_arguments = query_arguments,
        .callback = callback,
    };
    ExecuteQuery(options);
  }

  // Pure-virtual functions that must be overridden by concrete implementations.
  virtual void ExecuteQuery(const RedpathQueryOptions &options) const = 0;

  virtual absl::StatusOr<RedfishInterface *> GetRedfishInterface(
      const ServerInfo &server_info,
      RedfishInterfacePasskey unused_passkey) const = 0;

  virtual absl::Status ExecuteOnRedfishInterface(
      const ServerInfo &server_info, RedfishInterfacePasskey unused_passkey,
      const QueryEngineIntf::RedfishInterfaceOptions &options) const = 0;

  // Cancels query execution.
  // This function is overloaded:
  // `virtual void CancelQueryExecution(absl::Notification* notification)`: This
  // virtual overload accepts an `absl::Notification` pointer. The provided
  // notification will be signaled when the cancellation state is set to true
  // within QueryEngine.
  // `CancelQueryExecution()`: A parameterless overload that performs the
  // cancellation.
  virtual void CancelQueryExecution(absl::Notification *notification) = 0;
  void CancelQueryExecution() { return CancelQueryExecution(nullptr); }
};

// Concrete implementation of Query Router Interface that routes the queries
// to the appropriate query engine.
class QueryRouter : public QueryRouterIntf {
 public:
  using RedfishTransportFactory =
      std::function<absl::StatusOr<std::unique_ptr<RedfishTransport>>(
          StubArbiterInfo::PriorityLabel &)>;

  // Defines the specification of a server that will be supplied by the users of
  // this class. This defines all the servers that the QueryRouter can
  // communicate with.
  struct ServerSpec {
    ServerInfo server_info;

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

    std::unique_ptr<RedfishTransport> transport = nullptr;

    std::optional<RedfishTransportFactory> transport_factory = std::nullopt;
    std::optional<StubArbiterInfo::Type> transport_arbiter_type;

    ServerSpec() = default;
    ServerSpec(const ServerSpec &) = delete;
    ServerSpec &operator=(const ServerSpec &) = delete;
    ServerSpec(ServerSpec &&) = default;
    ServerSpec &operator=(ServerSpec &&) = default;
  };

  // Factory method to create a QueryRouter instance. The servers involved in
  // the instance will be an intersection of the Servers specified in the
  // QueryRouterSpec and the list of ServerSpecs specified.
  static absl::StatusOr<std::unique_ptr<QueryRouterIntf>> Create(
      const QueryRouterSpec &router_spec, std::vector<ServerSpec> server_specs,
      QueryEngineFactory query_engine_factory = QueryEngine::Create,
      RedpathNormalizer::RedpathNormalizersFactory redpath_normalizers_factory =
          DefaultRedpathNormalizerMap,
      TransportArbiterQueryEngineFactory
          transport_arbiter_query_engine_factory =
              QueryEngineWithTransportArbiter::
                  CreateTransportArbiterQueryEngine);

  ~QueryRouter() override = default;

  // Execute the list of queries as per the QueryRouterSpec. `callback` will be
  // called for each QueryResult and is thread safe when the queries are
  // executed in parallel (PATTERN_SERIAL_AGENT, PATTERN_PARALLEL_ALL).
  // `callback` is guaranteed to be executed non-concurrently and clients don't
  // need to independently guard against concurrent access to any of their data
  // structures that are accessed by the callback.
  void ExecuteQuery(const RedpathQueryOptions &options) const override;

  // Returns the RedfishInterface for the given ServerInfo.
  // QueryEngineRawInterfacePasskey is just an empty strongly-typed object that
  // one needs to provide in order to get access to the underlying
  // RedfishInterface.
  absl::StatusOr<RedfishInterface *> GetRedfishInterface(
      const ServerInfo &server_info,
      RedfishInterfacePasskey unused_passkey) const override;

  absl::Status ExecuteOnRedfishInterface(
      const ServerInfo &server_info, RedfishInterfacePasskey unused_passkey,
      const QueryEngineIntf::RedfishInterfaceOptions &options) const override;

  // This function will gracefully terminate all active QueryEngine threads
  // associated with the target query.
  void CancelQueryExecution(absl::Notification *notification) override;

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
    explicit QueryBatch(const QueryRoutingInfo *routing_info)
        : routing_info(*ABSL_DIE_IF_NULL(routing_info)) {}

    std::vector<absl::string_view> queries;
    const QueryRoutingInfo &routing_info;
  };

  using RoutingTable = std::vector<QueryRoutingInfo>;

  using ExecuteFunction = std::function<void(const RedpathQueryOptions &)>;

  QueryRouter(RoutingTable routing_table, QueryPattern query_pattern,
              int max_concurrent_threads);

  // All queries will be executed in series across all agents.
  void ExecuteQuerySerialAll(const RedpathQueryOptions &options) const;

  // All queries will be execute in series for an agent, but queries across
  // agents are executed in parallel
  void ExecuteQuerySerialAgent(const RedpathQueryOptions &options) const;

  // All queries will be executed in parallel across all agents.
  void ExecuteQueryParallelAll(const RedpathQueryOptions &options) const;

  // Each query set in the query_batches list is executed in its own individual
  // thread.
  void ExecuteQueryBatches(absl::Span<const QueryBatch> query_batches,
                           const RedpathQueryOptions &options) const;

  // Returns the latest query cancellation state.
  bool IsQueryExecutionCancelled() const {
    absl::MutexLock lock(&query_cancellation_state_mutex_);
    return query_cancellation_state_;
  }

  int GetExecuteRefCount() const {
    absl::MutexLock lock(&execute_ref_count_mutex_);
    return execute_ref_count_;
  }

  RoutingTable routing_table_;
  ExecuteFunction execute_function_;
  int max_concurrent_threads_;

  // mutable keyword allows locking and unlocking operations to be performed
  // from within const methods
  mutable absl::Mutex query_cancellation_state_mutex_;
  bool query_cancellation_state_
      ABSL_GUARDED_BY(query_cancellation_state_mutex_) = false;

  mutable absl::Mutex execute_ref_count_mutex_;
  mutable int execute_ref_count_ ABSL_GUARDED_BY(execute_ref_count_mutex_) = 0;

  // Condition variable to wait/signal for query cancellation completion.
  mutable absl::CondVar cancel_completion_cond_;
};

// Factory for creating different variants of query router.
// Ideally used in tests to inject different variants of query router
using QueryRouterFactory = absl::AnyInvocable<
    absl::StatusOr<std::unique_ptr<ecclesia::QueryRouterIntf>>(
        const ecclesia::QueryRouterSpec &,
        std::vector<ecclesia::QueryRouter::ServerSpec>,
        ecclesia::QueryEngineFactory,
        ecclesia::RedpathNormalizer::RedpathNormalizersFactory,
        ecclesia::TransportArbiterQueryEngineFactory)>;

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ROUTER_QUERY_ROUTER_H_
