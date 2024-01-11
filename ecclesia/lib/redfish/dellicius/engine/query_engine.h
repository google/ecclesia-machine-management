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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/function_ref.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/config.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/passkey.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

// Encapsulates the context needed to execute RedPath query.
struct QueryContext {
  // Describes the RedPath queries that engine will be configured to execute.
  absl::Span<const EmbeddedFile> query_files;
  // Rules used to configure Redfish query parameter - $expand for
  // specific RedPath prefixes in given queries.
  absl::Span<const EmbeddedFile> query_rules = {};
  const Clock *clock = Clock::RealClock();
};

// Parameters necessary to configure the query engine.
struct QueryEngineParams {
  // Stable id types used to configure engine for an appropriate normalizer that
  // decorates the query result with desired stable
  // id type.
  enum class RedfishStableIdType : uint8_t {
    kRedfishLocation,  // Redfish Standard - PartLocationContext + ServiceLabel
    kRedfishLocationDerived  // Derived from Redfish topology.
  };

  struct FeatureFlags {
    // Creates a query engine using metrical transport. When enabled,
    // DelliciusQueryResult will have RedfishMetrics object populated.
    bool enable_redfish_metrics = false;
    bool fail_on_first_error = true;
    bool log_redfish_traces = false;
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
  FeatureFlags feature_flags;

  // Node topology configuration:-
  // This configuration is used with
  // RedfishStableIdType::kRedfishLocationDerived feature flag to instruct
  // QueryEngine to traverse the tree for specific resources and their
  // subordinates to build physical topology.
  std::string redfish_topology_config_name;
};

// QueryEngine is logical composition of Redfish query interpreter, dispatcher
// and normalizer built to execute a statically defined set of Redfish Queries
// based on accompanying optional query rules.
// A few ways to instantiate QueryEngine using factory APIs:
//  (A) Build QueryEngine with default configuration (local id based on redfish
//      stable id):
//  QueryContext query_context{.query_files = query_files};
//  absl::StatusOr<QueryEngine> query_engine = CreateQueryEngine(query_context,
//      {.transport = std::move(transport)});
//
//  (B) Build QueryEngine with non default local stable id:
//  QueryContext query_context{.query_files = query_files};
//  absl::StatusOr<QueryEngine> query_engine = CreateQueryEngine(query_context,
//      {.transport = std::move(transport),
//       .stable_id_type =
//            QueryEngineParams::RedfishStableIdType::kRedfishLocationDerived});
//
//  (C) Build QueryEngine with machine level stable id decorator:
//  QueryContext query_context{.query_files = query_files};
//  absl::StatusOr<QueryEngine> query_engine =
//    CreateQueryEngine<MyStableIdMapType>(
//        query_context,
//        {.transport = std::move(transport),
//          .entity_tag = "node0",
//          .stable_id_type =
//              QueryEngineParams::RedfishStableIdType::kRedfishLocation},
//        std::move(my_stable_id_map), std::move(my_machine_id_assigner));
//
//  (D) Build QueryEngine using custom normalizer:
//  QueryContext query_context{.query_files = query_files};
//  absl::StatusOr<QueryEngine> query_engine = CreateQueryEngine(
//     query_context, std::move(redfish_interface),
//     std::move(my_custom_normalizer));

class QueryEngineIntf {
 public:
  // A set of populated variables for 1 to many queries.
  using QueryVariableSet = absl::flat_hash_map<std::string, QueryVariables>;

  enum class ServiceRootType : uint8_t { kRedfish, kGoogle, kCustom };

  virtual ~QueryEngineIntf() = default;

  ABSL_DEPRECATED("Use ExecuteRedpathQuery Instead")
  void ExecuteQuery(
      absl::Span<const absl::string_view> query_ids,
      absl::FunctionRef<bool(const DelliciusQueryResult &result)> callback,
      ServiceRootType service_root_uri = ServiceRootType::kRedfish) {
    ExecuteQuery(query_ids, callback, service_root_uri, {});
  }

  ABSL_DEPRECATED("Use ExecuteRedpathQuery Instead")
  virtual void ExecuteQuery(
      absl::Span<const absl::string_view> query_ids,
      absl::FunctionRef<bool(const DelliciusQueryResult &result)> callback,
      ServiceRootType service_root_uri,
      const QueryVariableSet &query_arguments) = 0;

  ABSL_DEPRECATED("Use ExecuteRedpathQuery Instead")
  std::vector<DelliciusQueryResult> ExecuteQuery(
      absl::Span<const absl::string_view> query_ids,
      ServiceRootType service_root_uri = ServiceRootType::kCustom) {
    return ExecuteQuery(query_ids, service_root_uri, {});
  }

  ABSL_DEPRECATED("Use ExecuteRedpathQuery Instead")
  virtual std::vector<DelliciusQueryResult> ExecuteQuery(
      absl::Span<const absl::string_view> query_ids,
      ServiceRootType service_root_uri,
      const QueryVariableSet &query_arguments) = 0;

  QueryIdToResult ExecuteRedpathQuery(
      absl::Span<const absl::string_view> query_ids,
      ServiceRootType service_root_uri = ServiceRootType::kCustom) {
    return ExecuteRedpathQuery(query_ids, service_root_uri, {});
  }

  virtual QueryIdToResult ExecuteRedpathQuery(
      absl::Span<const absl::string_view> query_ids,
      ServiceRootType service_root_uri,
      const QueryVariableSet &query_arguments) = 0;

  // QueryEngineRawInterfacePasskey is just an empty strongly-typed object
  // that one needs to provide in order to invoke the member function.
  // We restrict the visibility of QueryEngineRawInterfacePasskey so that
  // we can understand which users are using raw-interface features which
  // are not yet available in the query engine.
  virtual absl::StatusOr<RedfishInterface *> GetRedfishInterface(
      RedfishInterfacePasskey unused_passkey) = 0;

  // Returns the server tag, if available
  virtual absl::string_view GetAgentIdentifier() const { return ""; }
};

class QueryEngine : public QueryEngineIntf {
 public:
  using QueryEngineIntf::ExecuteQuery;
  using QueryEngineIntf::ExecuteRedpathQuery;

  // Creates query engine for machine devpath decorator extensions.
  static absl::StatusOr<std::unique_ptr<QueryEngineIntf>> Create(
      const QueryContext &query_context, QueryEngineParams params,
      std::unique_ptr<IdAssigner> id_assigner = nullptr);

  ABSL_DEPRECATED("Use QueryEngine factory methods instead.")
  QueryEngine(const QueryEngineConfiguration &config,
              std::unique_ptr<RedfishTransport> transport,
              RedfishTransportCacheFactory cache_factory = NullCache::Create,
              const Clock *clock = Clock::RealClock());

  explicit QueryEngine(std::unique_ptr<QueryEngineIntf> engine_impl)
      : engine_impl_(std::move(engine_impl)) {}

  QueryEngine(const QueryEngine &) = delete;
  QueryEngine &operator=(const QueryEngine &) = delete;
  QueryEngine(QueryEngine &&other) = default;
  QueryEngine &operator=(QueryEngine &&other) = default;

  // The callback will be called when SubqueryOutput exceeds the
  // max_size_limit in the query
  ABSL_DEPRECATED("Use ExecuteRedpathQuery Instead")
  void ExecuteQuery(
      absl::Span<const absl::string_view> query_ids,
      absl::FunctionRef<bool(const DelliciusQueryResult &result)> callback,
      ServiceRootType service_root_uri,
      const QueryVariableSet &query_arguments) override {
    return engine_impl_->ExecuteQuery(query_ids, callback, service_root_uri,
                                      query_arguments);
  }

  ABSL_DEPRECATED("Use ExecuteRedpathQuery Instead")
  std::vector<DelliciusQueryResult> ExecuteQuery(
      absl::Span<const absl::string_view> query_ids,
      ServiceRootType service_root_uri,
      const QueryVariableSet &query_arguments) override {
    return engine_impl_->ExecuteQuery(query_ids, service_root_uri,
                                      query_arguments);
  }

  QueryIdToResult ExecuteRedpathQuery(
      absl::Span<const absl::string_view> query_ids,
      ServiceRootType service_root_uri,
      const QueryVariableSet &query_arguments) override {
    return engine_impl_->ExecuteRedpathQuery(query_ids, service_root_uri,
                                             query_arguments);
  }

  absl::StatusOr<RedfishInterface *> GetRedfishInterface(
      RedfishInterfacePasskey unused_passkey) override {
    return engine_impl_->GetRedfishInterface(unused_passkey);
  }

  absl::string_view GetAgentIdentifier() const override {
    return engine_impl_->GetAgentIdentifier();
  }

 private:
  std::unique_ptr<QueryEngineIntf> engine_impl_;
};

// Build query engine based on given |configuration| to execute queries in
// |query_context|.
absl::StatusOr<QueryEngine> CreateQueryEngine(const QueryContext &query_context,
                                              QueryEngineParams engine_params);

// Creates query engine for machine devpath decorator extensions.
absl::StatusOr<QueryEngine> CreateQueryEngine(
    const QueryContext &query_context, QueryEngineParams engine_params,
    std::unique_ptr<IdAssigner> id_assigner);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_QUERY_ENGINE_H_
