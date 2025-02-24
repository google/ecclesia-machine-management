/*
 * Copyright 2025 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_TRANSPORT_ARBITER_QUERY_ENGINE_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_TRANSPORT_ARBITER_QUERY_ENGINE_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/passkey.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_engine_features.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_spec.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.h"
#include "ecclesia/lib/redfish/redpath/engine/normalizer.h"
#include "ecclesia/lib/redfish/redpath/engine/query_planner.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/stubarbiter/arbiter.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

class QueryEngineWithTransportArbiter : public QueryEngine {
 public:
  using TransportArbiter = StubArbiter<RedfishInterface>;

  // Parameters necessary to configure the query engine.
  struct Params {
    // Generates cache used by query engine, default set to Null cache (no
    // cache).
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

    // Factory for creating RedfishTransports. Note: this function only takes
    // PriorityLabel as input, so the configuration must be defined within
    // the factory.
    std::function<absl::StatusOr<std::unique_ptr<RedfishTransport>>(
        StubArbiterInfo::PriorityLabel &)>
        transport_factory;

    // Type of StubArbiter to use: failover or manual.
    std::optional<StubArbiterInfo::Type> transport_arbiter_type;
    std::optional<absl::Duration> transport_arbiter_refresh;
  };

  static absl::StatusOr<std::unique_ptr<QueryEngineIntf>>
  CreateTransportArbiterQueryEngine(
      QuerySpec query_spec, Params params,
      std::unique_ptr<IdAssigner> id_assigner = nullptr,
      RedpathNormalizer::QueryIdToNormalizerMap id_to_normalizer =
          DefaultRedpathNormalizerMap());

  absl::StatusOr<SubscriptionQueryResult> ExecuteSubscriptionQuery(
      absl::Span<const absl::string_view> query_ids,
      const QueryVariableSet &query_arguments,
      StreamingOptions streaming_options) override {
    return absl::UnimplementedError(
        "Subscription query is not supported with TransportArbiter.");
  }

  absl::StatusOr<RedfishInterface *> GetRedfishInterface(
      RedfishInterfacePasskey unused_passkey) override {
    return absl::InternalError(
        "QueryEngine contains TransportArbiter, which is not supported for "
        "RedfishInterface.");
  }

  absl::Status ExecuteOnRedfishInterface(
      RedfishInterfacePasskey unused_passkey,
      const RedfishInterfaceOptions &options) override;

  QueryIdToResult ExecuteRedpathQuery(
      absl::Span<const absl::string_view> query_ids,
      const RedpathQueryOptions &options) override;

 private:
  // Constructor for QueryEngine with transport arbiter. Note we explicitly set
  // the transport to nullptr here, as it is not needed, and not set the
  // metrical_transport_.
  QueryEngineWithTransportArbiter(
      std::string entity_tag,
      absl::flat_hash_map<std::string, std::unique_ptr<QueryPlannerIntf>>
          id_to_query_plans,
      const Clock *clock, std::unique_ptr<RedpathNormalizer> normalizer,
      std::unique_ptr<TransportArbiter> transport_arbiter,
      QueryEngineFeatures features,
      RedpathNormalizer::QueryIdToNormalizerMap id_to_normalizers =
          DefaultRedpathNormalizerMap())
      : QueryEngine(
            /*entity_tag=*/std::move(entity_tag),
            /*id_to_query_plans=*/std::move(id_to_query_plans), /*clock=*/clock,
            /*legacy_normalizer=*/nullptr, /*normalizer=*/std::move(normalizer),
            /*redfish_interface=*/nullptr, /*features=*/std::move(features),
            /*metrical_transport=*/nullptr,
            /*id_to_normalizers=*/std::move(id_to_normalizers)),
        transport_arbiter_(std::move(transport_arbiter)) {}

  std::unique_ptr<TransportArbiter> transport_arbiter_ = nullptr;
};

using TransportArbiterQueryEngineFactory =
    absl::AnyInvocable<absl::StatusOr<std::unique_ptr<QueryEngineIntf>>(
        QuerySpec query_spec,
        QueryEngineWithTransportArbiter::Params engine_params,
        std::unique_ptr<IdAssigner> id_assigner,
        RedpathNormalizer::QueryIdToNormalizerMap id_to_normalizers)>;

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_TRANSPORT_ARBITER_QUERY_ENGINE_H_
