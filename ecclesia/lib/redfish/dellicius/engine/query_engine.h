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

#include <stdbool.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/passkey.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_rules.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_engine_features.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_engine_features.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_spec.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/redpath_subscription.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/engine/normalizer.h"
#include "ecclesia/lib/redfish/redpath/engine/query_planner.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/metrical_transport.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "ecclesia/lib/stubarbiter/arbiter.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

// Parameters necessary to configure the query engine.
struct QueryEngineParams {
  // Stable id types used to configure engine for an appropriate normalizer that
  // decorates the query result with desired stable id type.
  enum class RedfishStableIdType : uint8_t {
    kRedfishLocation,  // Redfish Standard - PartLocationContext + ServiceLabel
    kRedfishLocationDerived  // Derived from Redfish topology.
  };

  // Transport medium over which Redfish queries are sent to the redfish server.
  std::unique_ptr<RedfishTransport> transport;
  // Generates cache used by query engine, default set to Null cache (no cache).
  RedfishTransportCacheFactory cache_factory = NullCache::Create;
  // Optional attribute to uniquely identify redfish server where necessary.
  std::string entity_tag;
  // Type of stable identifier to use in query result
  QueryEngineParams::RedfishStableIdType stable_id_type =
      QueryEngineParams::RedfishStableIdType::kRedfishLocation;
  // Captures toggleable features controlled by the user.
  QueryEngineFeatures features = StandardQueryEngineFeatures();

  // Node topology configuration:-
  // This configuration is used with
  // RedfishStableIdType::kRedfishLocationDerived feature flag to instruct
  // QueryEngine to traverse the tree for specific resources and their
  // subordinates to build physical topology.
  std::string redfish_topology_config_name;

  // instead of QueryEngineParams::RedfishStableIdType.
  static RedpathNormalizer::RedfishStableIdType
  GetRedpathNormalizerStableIdType(
      QueryEngineParams::RedfishStableIdType stable_id_type) {
    switch (stable_id_type) {
      case QueryEngineParams::RedfishStableIdType::kRedfishLocation:
        return RedpathNormalizer::RedfishStableIdType::kRedfishLocation;
      case QueryEngineParams::RedfishStableIdType::kRedfishLocationDerived:
        return RedpathNormalizer::RedfishStableIdType::kRedfishLocationDerived;
    }
  }
};

// QueryEngine is logical composition of Redfish query interpreter, dispatcher
// and normalizer built to execute a statically defined set of Redfish Queries
// based on accompanying optional query rules.
//
// There are few ways to instantiate QueryEngine:
//  (A) Build QueryEngine without devpath decorators (local id based on redfish
//      stable id):
//  ECCLESIA_ASSIGN_OR_RETURN(std::unique_ptr<QueryEngineIntf> query_engine,
//      QueryEngine::Create(std::move(query_spec),
//                          {.transport = std::move(transport)}));
//
//  (B) Build QueryEngine with non default local stable id:
//  ECCLESIA_ASSIGN_OR_RETURN(std::unique_ptr<QueryEngineIntf> query_engine,
//      QueryEngine::Create(std::move(query_spec),
//                          {.transport = std::move(transport),
//                           .stable_id_type =
//            QueryEngineParams::RedfishStableIdType::kRedfishLocationDerived});
//
//  (C) Build QueryEngine with machine level stable id decorator:
//  ECCLESIA_ASSIGN_OR_RETURN(std::unique_ptr<QueryEngineIntf> query_engine,
//      QueryEngine::Create(std::move(query_spec),
//                          {.transport = std::move(transport),
//                           .entity_tag = "node0",
//                           .stable_id_type =
//              QueryEngineParams::RedfishStableIdType::kRedfishLocation},
//              std::move(my_machine_id_assigner));
//

class QueryEngineIntf {
 public:
  // A set of populated variables for 1 to many queries.
  using QueryVariableSet = absl::flat_hash_map<std::string, QueryVariables>;

  enum class ServiceRootType : uint8_t { kRedfish, kGoogle, kCustom };

  // Options for creating a query stream.
  struct StreamingOptions {
    // Callback to stream query result on event.
    using OnEventCallback = std::function<void(
        const QueryResult & /*result*/,
        const RedPathSubscription::EventContext & /*context*/)>;
    // Callback invoked when event stream closes.
    using OnStopCallback = std::function<void(const absl::Status &)>;
    // Facilitates event subscription.
    // It is responsible creating a RedfishEvent stream for given
    // RedPathSubscription configuration object. It registers the given
    // on_event and on_stop callbacks and invokes them asynchronously.
    using SubscriptionBroker =
        std::function<absl::StatusOr<std::unique_ptr<RedPathSubscription>>(
            const std::vector<RedPathSubscription::Configuration>
                &configurations,
            RedfishInterface &, RedPathSubscription::OnEventCallback,
            RedPathSubscription::OnStopCallback)>;

    OnEventCallback on_event_callback;
    OnStopCallback on_stop_callback;
    SubscriptionBroker subscription_broker = RedPathSubscriptionImpl::Create;
  };

  struct RedpathQueryOptions {
    ServiceRootType service_root_uri;
    QueryVariableSet query_arguments;
    StubArbiterInfo::PriorityLabel priority_label =
        StubArbiterInfo::PriorityLabel::kUnknown;
  };

  struct RedfishInterfaceOptions {
    std::function<absl::Status(const RedfishInterface &)> callback;
    StubArbiterInfo::PriorityLabel priority_label =
        StubArbiterInfo::PriorityLabel::kUnknown;
  };

  virtual ~QueryEngineIntf() = default;

  ABSL_DEPRECATED("Use ExecuteRedpathQuery with RedpathQueryOptions instead.")
  QueryIdToResult ExecuteRedpathQuery(
      absl::Span<const absl::string_view> query_ids,
      ServiceRootType service_root_uri = ServiceRootType::kCustom) {
    RedpathQueryOptions options = {.service_root_uri = service_root_uri};
    return ExecuteRedpathQuery(query_ids, options);
  }

  ABSL_DEPRECATED("Use ExecuteRedpathQuery with RedpathQueryOptions instead.")
  QueryIdToResult ExecuteRedpathQuery(
      absl::Span<const absl::string_view> query_ids,
      ServiceRootType service_root_uri,
      const QueryVariableSet &query_arguments) {
    RedpathQueryOptions options = {.service_root_uri = service_root_uri,
                                   .query_arguments = query_arguments};
    return ExecuteRedpathQuery(query_ids, options);
  }

  // Executes a subscription query.
  // Takes a set of Query Identifiers along with `StreamingOptions` that contain
  // callbacks to be invoked on event and when stream closes.
  // This API returns a `SubscriptionQueryResult` which wraps `QueryIdToResult`
  //  and a `RedPathSubscription` object that can be used by the user
  // application to cancel a subscription
  absl::StatusOr<SubscriptionQueryResult> ExecuteSubscriptionQuery(
      absl::Span<const absl::string_view> query_ids,
      StreamingOptions streaming_options) {
    return ExecuteSubscriptionQuery(query_ids, {},
                                    std::move(streaming_options));
  }

  virtual QueryIdToResult ExecuteRedpathQuery(
      absl::Span<const absl::string_view> query_ids,
      const RedpathQueryOptions &options) = 0;

  // Executes a subscription query.
  // Overloads ExecuteSubscriptionQuery to allow specifying `query_arguments`
  // for templated queries in addition to `query_ids` and `streaming_options`.
  virtual absl::StatusOr<SubscriptionQueryResult> ExecuteSubscriptionQuery(
      absl::Span<const absl::string_view> query_ids,
      const QueryVariableSet &query_arguments,
      StreamingOptions streaming_options) = 0;

  // QueryEngineRawInterfacePasskey is just an empty strongly-typed object
  // that one needs to provide in order to invoke the member function.
  // We restrict the visibility of QueryEngineRawInterfacePasskey so that
  // we can understand which users are using raw-interface features which
  // are not yet available in the query engine.
  virtual absl::StatusOr<RedfishInterface *> GetRedfishInterface(
      RedfishInterfacePasskey unused_passkey) = 0;

  virtual absl::Status ExecuteOnRedfishInterface(
      RedfishInterfacePasskey unused_passkey,
      const RedfishInterfaceOptions &options) = 0;

  // Returns the server tag, if available
  virtual absl::string_view GetAgentIdentifier() const { return ""; }

  // Cancels query execution.
  // This function is overloaded:
  // `virtual void CancelQueryExecution(absl::Notification* notification)`: This
  // virtual overload accepts an `absl::Notification` pointer. The provided
  // notification will be signaled when the cancellation state is set to true
  // within QueryEngine.
  // `CancelQueryExecution()`: A parameterless overload that performs the
  // cancellation.
  virtual void CancelQueryExecution(absl::Notification *notification) = 0;
  void CancelQueryExecution() { CancelQueryExecution(nullptr); }
};

class QueryEngine : public QueryEngineIntf {
 public:
  using QueryEngineIntf::ExecuteRedpathQuery;

  // Creates query engine for machine devpath decorator extensions.
  static absl::StatusOr<std::unique_ptr<QueryEngineIntf>> Create(
      QuerySpec query_spec, QueryEngineParams params,
      std::unique_ptr<IdAssigner> id_assigner = nullptr,
      RedpathNormalizer::QueryIdToNormalizerMap id_to_normalizer =
          DefaultRedpathNormalizerMap());

  QueryEngine(const QueryEngine &) = delete;
  QueryEngine &operator=(const QueryEngine &) = delete;
  QueryEngine(QueryEngine &&other) = default;
  QueryEngine &operator=(QueryEngine &&other) = default;

  QueryIdToResult ExecuteRedpathQuery(
      absl::Span<const absl::string_view> query_ids,
      const RedpathQueryOptions &options) override;

  absl::StatusOr<SubscriptionQueryResult> ExecuteSubscriptionQuery(
      absl::Span<const absl::string_view> query_ids,
      const QueryVariableSet &query_arguments,
      StreamingOptions streaming_options) override;

  absl::StatusOr<RedfishInterface *> GetRedfishInterface(
      RedfishInterfacePasskey unused_passkey) override;

  absl::Status ExecuteOnRedfishInterface(
      RedfishInterfacePasskey unused_passkey,
      const RedfishInterfaceOptions &options) override;

  void CancelQueryExecution(absl::Notification *notification) override;

  absl::string_view GetAgentIdentifier() const override { return entity_tag_; }

 protected:
  QueryEngine(
      std::string entity_tag,
      absl::flat_hash_map<std::string, std::unique_ptr<QueryPlannerIntf>>
          id_to_query_plans,
      const Clock *clock, std::unique_ptr<Normalizer> legacy_normalizer,
      std::unique_ptr<RedpathNormalizer> normalizer,
      std::unique_ptr<RedfishInterface> redfish_interface,
      QueryEngineFeatures features,
      MetricalRedfishTransport *metrical_transport = nullptr,
      RedpathNormalizer::QueryIdToNormalizerMap id_to_normalizers =
          DefaultRedpathNormalizerMap())
      : id_to_redpath_query_plans_(std::move(id_to_query_plans)),
        clock_(clock),
        features_(std::move(features)),
        entity_tag_(std::move(entity_tag)),
        legacy_normalizer_(std::move(legacy_normalizer)),
        normalizer_(std::move(normalizer)),
        redfish_interface_(std::move(redfish_interface)),
        metrical_transport_(metrical_transport),
        id_to_normalizers_(std::move(id_to_normalizers)) {}

  // Maps query id to query planner.
  absl::flat_hash_map<std::string, std::unique_ptr<QueryPlannerIntf>>
      id_to_redpath_query_plans_;
  const Clock *clock_;
  // Collection of flags dictating query engine execution.
  QueryEngineFeatures features_;

  void HandleRedfishEvent(
      const RedfishVariant &variant,
      const RedPathSubscription::EventContext &event_context,
      absl::FunctionRef<
          void(const QueryResult &result,
               const RedPathSubscription::EventContext &event_context)>
          on_event_callback);

  int GetExecuteRefCount() {
    absl::MutexLock lock(&execute_ref_count_mutex_);
    return execute_ref_count_;
  }

  bool IsQueryExecutionCancelled() {
    absl::MutexLock lock(&query_cancellation_state_mutex_);
    return query_cancellation_state_;
  }

  std::string entity_tag_;

  // Maps query id to subscription context.
  // Subscription context is used to create subscriptions and resume query
  // operations on event.
  absl::flat_hash_map<std::string,
                      std::unique_ptr<QueryPlannerIntf::SubscriptionContext>>
      id_to_subscription_context_;
  std::unique_ptr<Normalizer> legacy_normalizer_;
  std::unique_ptr<RedpathNormalizer> normalizer_;
  std::unique_ptr<RedfishInterface> redfish_interface_;
  // Used during query metrics collection.
  MetricalRedfishTransport *metrical_transport_ = nullptr;
  // Maps query id to additional normalizers, these normalizers are used to
  // decorate the query result with additional information right after the
  // regular normalizer above, which are optional based on the query id.
  RedpathNormalizer::QueryIdToNormalizerMap id_to_normalizers_;

  // Tracks the query cancellation.
  // This flag is set to true when query cancellation is initiated and is reset
  // when query cancellation is completed. Any subsequent query cancellation
  // request will be terminated if this flag is set to true.
  absl::Mutex query_cancellation_state_mutex_;
  bool query_cancellation_state_
      ABSL_GUARDED_BY(query_cancellation_state_mutex_) = false;

  // Tracks the number of active query executions.
  // Cancellation request is only processed when there are active query
  // executions.
  absl::Mutex execute_ref_count_mutex_;
  int execute_ref_count_ ABSL_GUARDED_BY(execute_ref_count_mutex_) = 0;

  // Condition variable to wait/signal for query cancellation completion.
  absl::CondVar cancel_completion_cond_;
};

// Factory for creating different variants of query engine.
//
//  Ideally used in tests to inject different types of Query Engine variants
//  like `MockQueryEngine`, `FakeQueryEngine`, `FileBackedQueryEngine`.
using QueryEngineFactory =
    absl::AnyInvocable<absl::StatusOr<std::unique_ptr<QueryEngineIntf>>(
        QuerySpec query_spec, QueryEngineParams engine_params,
        std::unique_ptr<IdAssigner> id_assigner,
        RedpathNormalizer::QueryIdToNormalizerMap id_to_normalizers)>;

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_QUERY_ENGINE_H_
