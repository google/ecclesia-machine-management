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

#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"

#include <sys/stat.h>

#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/engine/fake_query_engine.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/passkey.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_rules.pb.h"
#include "ecclesia/lib/redfish/dellicius/engine/test_queries_embedded.h"
#include "ecclesia/lib/redfish/dellicius/engine/test_query_rules_embedded.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner_devpath.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_engine_features.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_spec.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/redpath_subscription.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_router/query_router_spec.pb.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/testing/json_mockup.h"
#include "ecclesia/lib/redfish/transport/http.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "ecclesia/lib/status/test_macros.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"
#include "ecclesia/lib/thread/thread.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/lib/time/clock_fake.h"
#include "tensorflow_serving/util/net_http/public/response_code_enum.h"

namespace ecclesia {

namespace {

using ::tensorflow::serving::net_http::HTTPStatusCode;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::UnorderedElementsAre;

constexpr absl::string_view kQuerySamplesLocation =
    "lib/redfish/dellicius/query/samples";
constexpr absl::string_view kIndusMockup = "indus_hmb_shim/mockup.shar";
constexpr absl::string_view kIndusHmbCnMockup = "indus_hmb_cn/mockup.shar";
constexpr absl::string_view kComponentIntegrityMockupPath =
    "features/component_integrity/mockup.shar";
constexpr absl::Time clock_time = absl::FromUnixSeconds(10);

struct RedPathToQueryParams {
  std::string tracked_path;
  std::string query_params;
};

MATCHER(RedPathExpandConfigsEq, "") {
  const RedPathToQueryParams &lhs = std::get<0>(arg);
  const RedPathToQueryParams &rhs = std::get<1>(arg);
  return std::tie(lhs.tracked_path, lhs.query_params) ==
         std::tie(rhs.tracked_path, rhs.query_params);
}

void RemoveRequestCounts(QueryIdToResult &entries) {
  for (auto &[query_id, entry] : *entries.mutable_results()) {
    entry.mutable_stats()->clear_num_requests();
  }
}
void RemoveMetrics(QueryIdToResult &entries) {
  for (auto &[query_id, entry] : *entries.mutable_results()) {
    entry.mutable_stats()->clear_redfish_metrics();
  }
}
void RemoveTimestamps(QueryIdToResult &entries) {
  for (auto &[query_id, entry] : *entries.mutable_results()) {
    entry.mutable_stats()->clear_start_time();
    entry.mutable_stats()->clear_end_time();
    for (auto &[uri, metadata] : *entry.mutable_stats()
                                      ->mutable_redfish_metrics()
                                      ->mutable_uri_to_metrics_map()) {
      metadata.mutable_request_type_to_metadata()->clear();
    }
  }
}

void VerifyQueryResults(QueryIdToResult actual_entries,
                        QueryIdToResult expected_entries,
                        bool check_num_requests = false,
                        bool check_timestamps = false,
                        bool check_metrics = false) {
  if (!check_timestamps) {
    RemoveTimestamps(actual_entries);
    RemoveTimestamps(expected_entries);
  }
  if (!check_metrics) {
    RemoveMetrics(actual_entries);
    RemoveMetrics(expected_entries);
  }
  if (!check_num_requests) {
    RemoveRequestCounts(actual_entries);
    RemoveRequestCounts(expected_entries);
  }
  EXPECT_THAT(actual_entries, EqualsProto(expected_entries));
}

absl::StatusOr<QueryEngine> GetDefaultQueryEngine(
    FakeRedfishServer &server,
    absl::Span<const EmbeddedFile> query_files = kDelliciusQueries,
    absl::Span<const EmbeddedFile> query_rules = kQueryRules,
    const Clock *clock = Clock::RealClock(),
    const QueryEngineParams &query_engine_params = {}) {
  FakeRedfishServer::Config config = server.GetConfig();
  auto http_client = std::make_unique<CurlHttpClient>(
      LibCurlProxy::CreateInstance(), HttpCredential{});
  std::string network_endpoint =
      absl::StrFormat("%s:%d", config.hostname, config.port);
  std::unique_ptr<RedfishTransport> transport =
      HttpRedfishTransport::MakeNetwork(std::move(http_client),
                                        network_endpoint);

  QueryContext query_context{
      .query_files = query_files, .query_rules = query_rules, .clock = clock};
  return CreateQueryEngine(
      query_context, {.transport = std::move(transport),
                      .entity_tag = query_engine_params.entity_tag,
                      .stable_id_type = query_engine_params.stable_id_type,
                      .redfish_topology_config_name =
                          query_engine_params.redfish_topology_config_name});
}

absl::StatusOr<QueryEngine> GetQueryEngineWithIdAssigner(
    FakeRedfishServer &server, std::unique_ptr<IdAssigner> id_assigner,
    absl::Span<const EmbeddedFile> query_files = kDelliciusQueries,
    const Clock *clock = Clock::RealClock()) {
  FakeRedfishServer::Config config = server.GetConfig();
  auto http_client = std::make_unique<CurlHttpClient>(
      LibCurlProxy::CreateInstance(), HttpCredential{});
  std::string network_endpoint =
      absl::StrFormat("%s:%d", config.hostname, config.port);
  std::unique_ptr<RedfishTransport> transport =
      HttpRedfishTransport::MakeNetwork(std::move(http_client),
                                        network_endpoint);

  QueryContext query_context{.query_files = query_files, .clock = clock};
  return CreateQueryEngine(
      query_context,
      {.transport = std::move(transport),
       .entity_tag = "test_node_id",
       .stable_id_type =
           QueryEngineParams::RedfishStableIdType::kRedfishLocationDerived},
      std::move(id_assigner));
}

TEST(QueryEngineTest, QueryEngineDevpathConfiguration) {
  QueryIdToResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(GetTestDataDependencyPath(
          JoinFilePaths(kQuerySamplesLocation,
                        "query_out/devpath_sensor_out.textproto")));

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext({.query_files = kDelliciusQueries}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(std::move(query_spec), kIndusMockup, {}));
  QueryIdToResult response_entries =
      query_engine->ExecuteRedpathQuery({"SensorCollector"});

  auto results =
      response_entries.results().at("SensorCollector").data();
  auto sensors = results.fields().at("Sensors");
  EXPECT_EQ(sensors.list_value().values_size(), 14);

  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_output_sensor)});
}

TEST(QueryEngineTest, QueryEngineRedfishIntfAccessor) {
  std::string sensor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/sensor_out.textproto"));

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext({.query_files = kDelliciusQueries}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(std::move(query_spec), kIndusMockup, {}));
  EXPECT_TRUE(
      query_engine
          ->GetRedfishInterface(RedfishInterfacePasskeyFactory::GetPassKey())
          .ok());
}

// Test $top queries
// Use the paginated version of SensorCollector query for this test
TEST(QueryEngineTest, QueryEngineTopConfiguration) {
  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext({.query_files = kDelliciusQueries,
                                  .query_rules = kQueryRules}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(std::move(query_spec), kIndusMockup, {}));
  QueryIdToResult response_entries =
      query_engine->ExecuteRedpathQuery({"PaginatedSensorCollector"});

  auto results =
      response_entries.results().at("PaginatedSensorCollector").data();
  auto sensors = results.fields().at("Sensors");
  EXPECT_EQ(sensors.list_value().values_size(), 4);
}

TEST(QueryEngineTest, QueryEngineWithExpandConfiguration) {
}

TEST(QueryEngineTest, QueryEngineInvalidQueries) {
  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext({.query_files = kDelliciusQueries}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(std::move(query_spec), kIndusMockup,
                              {.devpath = FakeQueryEngine::Devpath::kDisable}));

  // Invalid Query Id
  QueryIdToResult response_entries =
      query_engine->ExecuteRedpathQuery({"ThereIsNoSuchId"});

  EXPECT_EQ(response_entries.results().size(), 0);
}

TEST(QueryEngineTest, QueryEngineConcurrentQueries) {
  QueryIdToResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(
          GetTestDataDependencyPath(JoinFilePaths(
              kQuerySamplesLocation, "query_out/sensor_out.textproto")));
  QueryIdToResult intent_output_assembly =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(
          GetTestDataDependencyPath(JoinFilePaths(
              kQuerySamplesLocation, "query_out/assembly_out.textproto")));

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext({.query_files = kDelliciusQueries}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(std::move(query_spec), kIndusMockup,
                              {.devpath = FakeQueryEngine::Devpath::kDisable}));

  QueryIdToResult response_entries = query_engine->ExecuteRedpathQuery(
      {"SensorCollector", "AssemblyCollectorWithPropertyNameNormalization"});

  RemoveTimestamps(response_entries);
  RemoveTimestamps(intent_output_sensor);
  RemoveTimestamps(intent_output_assembly);
  EXPECT_THAT(
      response_entries.results().at("SensorCollector"),
      EqualsProto(intent_output_sensor.results().at("SensorCollector")));
  EXPECT_THAT(response_entries.results().at(
                  "AssemblyCollectorWithPropertyNameNormalization"),
              EqualsProto(intent_output_assembly.results().at(
                  "AssemblyCollectorWithPropertyNameNormalization")));
}

TEST(QueryEngineTest, QueryEngineEmptyItemDevpath) {
  QueryIdToResult intent_output_assembly =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(GetTestDataDependencyPath(
          JoinFilePaths(kQuerySamplesLocation,
                        "query_out/devpath_assembly_out.textproto")));

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext({.query_files = kDelliciusQueries}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(std::move(query_spec), kIndusMockup,
                              {.entity_tag = "test_node_id"}));

  QueryIdToResult response_entries = query_engine->ExecuteRedpathQuery(
      {"AssemblyCollectorWithPropertyNameNormalization"});

  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_output_assembly)});
}

TEST(QueryEngineTest, QueryEngineWithCacheConfiguration) {
  QueryIdToResult intent_output_assembly =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(
          GetTestDataDependencyPath(JoinFilePaths(
              kQuerySamplesLocation, "query_out/assembly_out.textproto")));

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext(
          {.query_files = kDelliciusQueries, .query_rules = kQueryRules}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(
          std::move(query_spec), kIndusMockup,
          {.devpath = FakeQueryEngine::Devpath::kDisable,
           .metrics = FakeQueryEngine::Metrics::kEnable,
           .annotations = FakeQueryEngine::Annotations::kDisable,
           .cache = FakeQueryEngine::Cache::kInfinite}));

  {
    // Query assemblies 3 times in a row. Cache is cold only for the 1st query.
    QueryIdToResult first_response = query_engine->ExecuteRedpathQuery(
        {"AssemblyCollectorWithPropertyNameNormalization"});

    // Expect 4 requests to be made to the redfish server - Chassis, Processors
    // Systems, and Service Root
    EXPECT_EQ(first_response.results()
                  .at("AssemblyCollectorWithPropertyNameNormalization")
                  .stats()
                  .num_requests(),
              4);

    // For the next 2 queries, we expect 1 request to be made to the redfish
    // server - Processors; because the other resources are served from cache.
    // The processors collection resource is fetched fresh each time because
    // the freshness requirement.
    QueryIdToResult second_response = query_engine->ExecuteRedpathQuery(
        {"AssemblyCollectorWithPropertyNameNormalization"});
    EXPECT_EQ(second_response.results()
                  .at("AssemblyCollectorWithPropertyNameNormalization")
                  .stats()
                  .num_requests(),
              1);
    QueryIdToResult third_response = query_engine->ExecuteRedpathQuery(
        {"AssemblyCollectorWithPropertyNameNormalization"});
    EXPECT_EQ(third_response.results()
                  .at("AssemblyCollectorWithPropertyNameNormalization")
                  .stats()
                  .num_requests(),
              1);

    int systems_fetched_counter = 0;
    int processor_collection_fetched_counter = 0;
    for (const QueryIdToResult &response_entries :
         {first_response, second_response, third_response}) {
      // Validate the stats are correct.
      for (const auto &uri_x_metric :
           response_entries.results()
               .at("AssemblyCollectorWithPropertyNameNormalization")
               .stats()
               .redfish_metrics()
               .uri_to_metrics_map()) {
        // Expected systems only fetched only once from the redfish server.
        if (uri_x_metric.first == "/redfish/v1/Systems?$expand=*($levels=1)") {
          systems_fetched_counter++;
          for (const auto &metadata :
               uri_x_metric.second.request_type_to_metadata()) {
            EXPECT_EQ(metadata.second.request_count(), 1);
          }
        }
        // Note query_rule sample_query_rules.textproto uses level 1 expand at
        // Processors collection. But the query has freshness = true for the
        // members in processor collection and not the collection resource. Yet
        // we see the collection being fetched fresh each time because the
        // freshness requirement bubbles up if child redpath is in the expand
        // path of parent redpath.
        if (uri_x_metric.first ==
            "/redfish/v1/Systems/system/Processors?$expand=~($levels=1)") {
          processor_collection_fetched_counter++;
          for (const auto &metadata :
               uri_x_metric.second.request_type_to_metadata()) {
            EXPECT_EQ(metadata.second.request_count(), 1);
          }
        }
      }
    }
    // Both requests are fetched from Redfish server on the 1st query.
    EXPECT_THAT(systems_fetched_counter, Eq(1));
    EXPECT_THAT(processor_collection_fetched_counter, Eq(3));
  }
}

// Tests that when transport metrics are enabled per QueryResult,
// the metrics are independent of other QueryResult.
TEST(QueryEngineTest, QueryEngineWithTransportMetricsEnabled) {
  // Create QueryEngine with transport metrics

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext(
          {.query_files = kDelliciusQueries, .query_rules = kQueryRules}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(
          std::move(query_spec), kIndusHmbCnMockup,
          {.devpath = FakeQueryEngine::Devpath::kDisable,
           .metrics = FakeQueryEngine::Metrics::kEnable,
           .annotations = FakeQueryEngine::Annotations::kDisable,
           .cache = FakeQueryEngine::Cache::kInfinite}));

  // Hold all the metrics collected from each query execution to validate
  // later.
  RedfishMetrics metrics_first;
  RedfishMetrics metrics_cached;
  {
    // On first query, thermal subsystem won't be queried explicitly since
    // chassis level 2 expand will return thermal objects. Here we expect to
    // see only 1 expand URI dispatched.
    QueryIdToResult response_entries =
        query_engine->ExecuteRedpathQuery({"Thermal"});
    ASSERT_THAT(response_entries.results(), Not(IsEmpty()));
    metrics_first =
        response_entries.results().at("Thermal").stats().redfish_metrics();
  }
  {
    // Query again. This time all resources up to Thermal should be served
    // from cache. All Thermal objects will be freshly queried.
    QueryIdToResult response_entries =
        query_engine->ExecuteRedpathQuery({"Thermal"});
    ASSERT_THAT(response_entries.results(), Not(IsEmpty()));
    metrics_cached =
        response_entries.results().at("Thermal").stats().redfish_metrics();
  }

  size_t traced_chassis_expand = 0;
  size_t traced_thermal = 0;

  for (const RedfishMetrics &metrics : {metrics_first, metrics_cached}) {
    for (const auto &uri_x_metric : metrics.uri_to_metrics_map()) {
      if (uri_x_metric.first == "/redfish/v1/Chassis?$expand=.($levels=2)") {
        ++traced_chassis_expand;
        for (const auto &metadata :
             uri_x_metric.second.request_type_to_metadata()) {
          EXPECT_EQ(metadata.second.request_count(), 1);
        }
      }

      if (uri_x_metric.first ==
          "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/0") {
        ++traced_thermal;
        for (const auto &metadata :
             uri_x_metric.second.request_type_to_metadata()) {
          EXPECT_EQ(metadata.second.request_count(), 1);
        }
      }
    }
  }
  EXPECT_EQ(traced_chassis_expand, 1);
  EXPECT_EQ(traced_thermal, 1);
}

TEST(QueryEngineTest, QueryEngineTestGoogleRoot) {
  QueryIdToResult intent_output_service_root =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(GetTestDataDependencyPath(
          JoinFilePaths(kQuerySamplesLocation,
                        "query_out/service_root_google_out.textproto")));

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext({.query_files = kDelliciusQueries}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(std::move(query_spec),
                              kComponentIntegrityMockupPath, {}));

  QueryIdToResult response_entries = query_engine->ExecuteRedpathQuery(
      {"GoogleServiceRoot"}, QueryEngine::ServiceRootType::kGoogle);

  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_output_service_root)});
}

TEST(QueryEngineTest, QueryEngineWithUrlAnnotations) {
  QueryIdToResult intent_output_assembly =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(GetTestDataDependencyPath(
          JoinFilePaths(kQuerySamplesLocation,
                        "query_out/assembly_out_with_annotations.textproto")));

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext({.query_files = kDelliciusQueries}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(
          std::move(query_spec), kIndusHmbCnMockup,
          {.devpath = FakeQueryEngine::Devpath::kDisable,
           .metrics = FakeQueryEngine::Metrics::kDisable,
           .annotations = FakeQueryEngine::Annotations::kEnable,
           .cache = FakeQueryEngine::Cache::kInfinite}));

  QueryIdToResult response_entries = query_engine->ExecuteRedpathQuery(
      {"AssemblyCollectorWithPropertyNameNormalization"});

  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_output_assembly)});
}

TEST(QueryEngineTest, QueryEngineTestCustomServiceRoot) {
  FakeRedfishServer server(kComponentIntegrityMockupPath);
  FakeClock clock{clock_time};
  absl::StatusOr<QueryEngine> query_engine =
      GetDefaultQueryEngine(server, kDelliciusQueries, kQueryRules, &clock);
  EXPECT_TRUE(query_engine.ok());
  // Execute query where custom service root is set to /google/v1.
  QueryIdToResult response =
      query_engine->ExecuteRedpathQuery({"CustomServiceRoot"});
  QueryIdToResult intent_output = ParseTextFileAsProtoOrDie<QueryIdToResult>(
      GetTestDataDependencyPath(JoinFilePaths(
          kQuerySamplesLocation,
          "query_out/service_root_google_out_translated.textproto")));
  // Ignore the query id, since the expected data is from the google service
  // root query.
  EXPECT_THAT(
      response.results().at("CustomServiceRoot").data(),
      EqualsProto(intent_output.results().at("GoogleServiceRoot").data()));
}

TEST(QueryEngineTest, QueryEngineTestTemplatedQuery) {
  QueryIdToResult intent_query_out = ParseTextFileAsProtoOrDie<QueryIdToResult>(
      GetTestDataDependencyPath(JoinFilePaths(
          kQuerySamplesLocation, "query_out/sensor_out_template.textproto")));

  // Build the argument map.
  QueryVariables::VariableValue val1;
  QueryVariables::VariableValue val2;
  QueryVariables::VariableValue val3;
  val1.set_name("Type");
  *val1.add_values() = "Temperature";
  val2.set_name("Units");
  *val2.add_values() = "Cel";
  val3.set_name("Threshold");
  *val3.add_values() = "40";
  QueryEngineIntf::QueryVariableSet test_args;
  QueryVariables args1 = QueryVariables();
  *args1.add_variable_values() = val1;
  *args1.add_variable_values() = val2;
  *args1.add_variable_values() = val3;
  test_args["SensorCollectorTemplate"] = args1;

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext({.query_files = kDelliciusQueries}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(std::move(query_spec), kIndusMockup, {}));

  QueryIdToResult response_entries = query_engine->ExecuteRedpathQuery(
      {"SensorCollectorTemplate"}, QueryEngine::ServiceRootType::kRedfish,
      test_args);

  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_query_out)});
}

TEST(QueryEngineTest, QueryEngineTestTemplatedUnfilledVars) {
  QueryIdToResult intent_query_out =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(GetTestDataDependencyPath(
          JoinFilePaths(kQuerySamplesLocation,
                        "query_out/sensor_out_template_full.textproto")));

  QueryVariables::VariableValue val1;
  QueryVariables::VariableValue val2;
  val1.set_name("Units");
  *val1.add_values() = "Cel";
  // Type and units will remain unset
  QueryEngineIntf::QueryVariableSet test_args;
  QueryVariables args1 = QueryVariables();
  *args1.add_variable_values() = val1;
  // Pass in an empty value to make sure it doesn't mess up the variable
  // substitution.
  *args1.add_variable_values() = val2;
  test_args["SensorCollectorTemplate"] = args1;

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext({.query_files = kDelliciusQueries}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(std::move(query_spec), kIndusMockup, {}));

  QueryIdToResult response_entries = query_engine->ExecuteRedpathQuery(
      {"SensorCollectorTemplate"}, QueryEngine::ServiceRootType::kRedfish,
      test_args);

  // Since some of the variables are unfilled only sensors with a unit of
  // "Cel" will be returned. All other parts of the predicate are ignored.
  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_query_out)});
}

TEST(QueryEngineTest, DifferentVariableValuesWorkWithTemplatedQuery) {
  QueryIdToResult intent_query_out_full =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(GetTestDataDependencyPath(
          JoinFilePaths(kQuerySamplesLocation,
                        "query_out/sensor_out_template_full.textproto")));
  QueryIdToResult intent_query_out_filtered =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(GetTestDataDependencyPath(
          JoinFilePaths(kQuerySamplesLocation,
                        "query_out/sensor_out_template.textproto")));

  // Set the variables for filter criteria #1
  QueryVariables::VariableValue val1;
  QueryVariables::VariableValue val2;
  val1.set_name("Units");
  *val1.add_values() = "Cel";
  // Type and units will remain unset
  QueryEngineIntf::QueryVariableSet test_args;
  QueryVariables args1 = QueryVariables();
  *args1.add_variable_values() = val1;
  *args1.add_variable_values() = val2;
  test_args["SensorCollectorTemplate"] = args1;

  // Set the variables for filter criteria #2
  QueryVariables::VariableValue val3;
  QueryVariables::VariableValue val4;
  QueryVariables::VariableValue val5;
  val3.set_name("Type");
  *val3.add_values() = "Temperature";
  val4.set_name("Units");
  *val4.add_values() = "Cel";
  val5.set_name("Threshold");
  *val5.add_values() = "40";
  QueryEngineIntf::QueryVariableSet test_args_filtered;
  QueryVariables args2 = QueryVariables();
  *args2.add_variable_values() = val3;
  *args2.add_variable_values() = val4;
  *args2.add_variable_values() = val5;
  test_args_filtered["SensorCollectorTemplate"] = args2;

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext({.query_files = kDelliciusQueries}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(std::move(query_spec), kIndusMockup, {}));

  // Execute query with first set of variable values.
  QueryIdToResult response_entries = query_engine->ExecuteRedpathQuery(
      {"SensorCollectorTemplate"}, QueryEngine::ServiceRootType::kRedfish,
      test_args);
  VerifyQueryResults(std::move(response_entries),
                     std::move(intent_query_out_full));

  // Run query again with different variable values for the same templated
  // RedPath query.
  QueryIdToResult response_entries_filtered = query_engine->ExecuteRedpathQuery(
      {"SensorCollectorTemplate"}, QueryEngine::ServiceRootType::kRedfish,
      test_args_filtered);
  VerifyQueryResults(std::move(response_entries_filtered),
                     std::move(intent_query_out_filtered));
}

TEST(QueryEngineTest, QueryEngineTestTemplatedNoVars) {
  // Since all variables are unfilled the entire predicate will be replaced
  // with "select all". Therefore the whole set of sensors should be returned.
  QueryIdToResult intent_query_out = ParseTextFileAsProtoOrDie<QueryIdToResult>(
      GetTestDataDependencyPath(JoinFilePaths(
          kQuerySamplesLocation, "query_out/sensor_out.textproto")));

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext({.query_files = kDelliciusQueries}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(std::move(query_spec), kIndusMockup,
                              {.devpath = FakeQueryEngine::Devpath::kDisable}));

  QueryIdToResult response_entries = query_engine->ExecuteRedpathQuery(
      {"SensorCollectorTemplate"}, QueryEngine::ServiceRootType::kRedfish);

  // Changing the query ID in the expected result so a whole new output file
  // doesn't need made.
  QueryIdToResult new_intent_query_out;
  new_intent_query_out.mutable_results()->insert(
      {"SensorCollectorTemplate",
       intent_query_out.results().at("SensorCollector")});
  *new_intent_query_out.mutable_results()
       ->at("SensorCollectorTemplate")
       .mutable_query_id() = "SensorCollectorTemplate";
  VerifyQueryResults(std::move(response_entries),
                     std::move(new_intent_query_out));
}

TEST(QueryEngineTest, QueryEngineTransportMetricsInResult) {
  QueryIdToResult intent_output_assembly =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(GetTestDataDependencyPath(
          JoinFilePaths(kQuerySamplesLocation,
                        "query_out/assembly_out_with_metrics.textproto")));
  QueryIdToResult intent_output_sensors =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(GetTestDataDependencyPath(
          JoinFilePaths(kQuerySamplesLocation,
                        "query_out/sensor_out_with_metrics.textproto")));

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext(
          {.query_files = kDelliciusQueries, .query_rules = kQueryRules}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(std::move(query_spec), kIndusMockup,
                              {.devpath = FakeQueryEngine::Devpath::kDisable,
                               .metrics = FakeQueryEngine::Metrics::kEnable,
                               .cache = FakeQueryEngine::Cache::kInfinite}));

  // Validate first query result with metrics.
  QueryIdToResult response_entries = query_engine->ExecuteRedpathQuery(
      {"AssemblyCollectorWithPropertyNameNormalization"});
  VerifyQueryResults(response_entries, intent_output_assembly);
  // Validate second query result with metrics.
  QueryIdToResult response_entries_2 =
      query_engine->ExecuteRedpathQuery({"SensorCollector"});
  VerifyQueryResults(std::move(response_entries_2),
                     {std::move(intent_output_sensors)});
  // Ensure metrics from first response are not overwritten.
  VerifyQueryResults(response_entries, intent_output_assembly);
}

TEST(QueryEngineTest, QueryEngineWithDefaultNormalizer) {
  FakeRedfishServer server(kIndusMockup);
  FakeClock clock{clock_time};
  absl::StatusOr<QueryEngine> query_engine =
      GetDefaultQueryEngine(server, kDelliciusQueries, kQueryRules, &clock);
  EXPECT_TRUE(query_engine.ok());

  QueryIdToResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(
          GetTestDataDependencyPath(JoinFilePaths(
              kQuerySamplesLocation, "query_out/sensor_out.textproto")));
  QueryIdToResult response_entries =
      query_engine->ExecuteRedpathQuery({"SensorCollector"});
  VerifyQueryResults(std::move(response_entries),
                     std::move(intent_output_sensor));
}

TEST(QueryEngineTest, QueryEngineWithIdAssigner) {
  FakeRedfishServer server(kIndusMockup);
  FakeClock clock{clock_time};

  absl::flat_hash_map<std::string, std::string> devpath_map = {};
  auto id_assigner = NewMapBasedDevpathAssigner(devpath_map);

  absl::StatusOr<QueryEngine> query_engine = GetQueryEngineWithIdAssigner(
      server, std::move(id_assigner), kDelliciusQueries, &clock);
  EXPECT_TRUE(query_engine.ok());

  QueryIdToResult intent_output_assembly =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(GetTestDataDependencyPath(
          JoinFilePaths(kQuerySamplesLocation,
                        "query_out/devpath_assembly_out.textproto")));
  QueryIdToResult response_entries = query_engine->ExecuteRedpathQuery(
      {"AssemblyCollectorWithPropertyNameNormalization"});
  VerifyQueryResults(std::move(response_entries),
                     std::move(intent_output_assembly));
}

TEST(QueryEngineTest, TestQueryEngineFactoryForParserError) {
  FakeRedfishServer server(kIndusMockup);
  EXPECT_EQ(GetDefaultQueryEngine(server, {{"Test", "{}"}}).status().code(),
            absl::StatusCode::kInternal);
}

TEST(QueryEngineTest, TestQueryEngineFactoryForInvalidQuery) {
  FakeRedfishServer server(kIndusMockup);
  EXPECT_EQ(GetDefaultQueryEngine(server, {{"Test", ""}}).status().code(),
            absl::StatusCode::kInternal);
}

TEST(QueryEngineTest, QueryEngineWithTranslation) {
  QueryIdToResult intent_output_assembly =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(
          GetTestDataDependencyPath(JoinFilePaths(
              kQuerySamplesLocation, "query_out/assembly_out.textproto")));

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext({.query_files = kDelliciusQueries}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(std::move(query_spec), kIndusMockup,
                              {.devpath = FakeQueryEngine::Devpath::kDisable}));

  // Validate first query result with metrics.
  QueryIdToResult response_entries = query_engine->ExecuteRedpathQuery(
      {"AssemblyCollectorWithPropertyNameNormalization"});
  VerifyQueryResults(response_entries, intent_output_assembly);
}

TEST(QueryEngineTest, QueryEngineWithTranslationAndLocalDevpath) {
  QueryIdToResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(GetTestDataDependencyPath(
          JoinFilePaths(kQuerySamplesLocation,
                        "query_out/devpath_sensor_out.textproto")));

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext({.query_files = kDelliciusQueries}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(std::move(query_spec), kIndusMockup, {}));

  // Validate first query result with metrics.
  QueryIdToResult response_entries =
      query_engine->ExecuteRedpathQuery({"SensorCollector"});
  VerifyQueryResults(response_entries, intent_output_sensor);
}

TEST(QueryEngineTest, MalformedQueryRulesFailEngineConstruction) {
  constexpr absl::string_view kQueryRuleStr = R"pb(
    query_id_to_params_rule { key: "Assembly"
                              value {
                                redpath_prefix_with_params {
                                  redpath: "/Systems"
                                  expand_configuration { level: 1 type: BOTH }
                                }
                              })pb";

  const std::vector<EmbeddedFile> query_rules = {
      {.name = "query_rules.pb", .data = kQueryRuleStr}};

  FakeRedfishServer server(kIndusMockup);
  EXPECT_THAT(GetDefaultQueryEngine(server, kDelliciusQueries, query_rules),
              IsStatusInternal());
}

TEST(QueryEngineTest, QueryEngineNoHaltOnFirstFailure) {
  FakeRedfishServer server(kIndusMockup);
  std::string temp_sensor_uri =
      "/redfish/v1/Chassis/chassis/Sensors/indus_eat_temp";
  std::string rotational_sensor_uri =
      "/redfish/v1/Chassis/chassis/Sensors/indus_fan4_rpm";
  // Ensure two subqueries return errors.
  server.AddHttpGetHandlerWithStatus(temp_sensor_uri, "",
                                     HTTPStatusCode::SERVICE_UNAV);
  server.AddHttpGetHandlerWithStatus(rotational_sensor_uri, "",
                                     HTTPStatusCode::SERVICE_UNAV);
  // Set up query engine with redfish metrics and to not halt on first error.
  FakeClock clock{clock_time};
  FakeRedfishServer::Config config = server.GetConfig();
  auto http_client = std::make_unique<CurlHttpClient>(
      LibCurlProxy::CreateInstance(), HttpCredential{});
  std::string network_endpoint =
      absl::StrFormat("%s:%d", config.hostname, config.port);
  std::unique_ptr<RedfishTransport> transport =
      HttpRedfishTransport::MakeNetwork(std::move(http_client),
                                        network_endpoint);

  QueryContext query_context{.query_files = kDelliciusQueries,
                             .query_rules = kQueryRules,
                             .clock = &clock};
  absl::StatusOr<QueryEngine> query_engine =
      CreateQueryEngine(query_context, {.transport = std::move(transport),
                                        .entity_tag = "test_node_id",
                                        .features = ParseTextProtoOrDie(R"pb(
                                          enable_redfish_metrics: true
                                          fail_on_first_error: false
                                        )pb")});
  ASSERT_TRUE(query_engine.ok());
  EXPECT_EQ(query_engine->GetAgentIdentifier(), "test_node_id");
  // Issue the query and assert that both GETs were issued, ensuring that the
  // query execution continued AFTER the first error occurred.
  QueryResult query_result =
      query_engine->ExecuteRedpathQuery({"SensorCollectorWithChassisLinks"})
          .results()
          .at("SensorCollectorWithChassisLinks");
  EXPECT_TRUE(
      query_result.stats().redfish_metrics().uri_to_metrics_map().contains(
          temp_sensor_uri));
  EXPECT_TRUE(
      query_result.stats().redfish_metrics().uri_to_metrics_map().contains(
          rotational_sensor_uri));
}

TEST(QueryEngineTest, QueryEngineUsesGivenTopologyConfig) {
  FakeRedfishServer server(kComponentIntegrityMockupPath);
  FakeClock clock{clock_time};
  {
    QueryEngineParams params{
        .stable_id_type =
            QueryEngineParams::RedfishStableIdType::kRedfishLocationDerived,
        .redfish_topology_config_name = "redfish_test.textpb"};
    absl::StatusOr<QueryEngine> query_engine = GetDefaultQueryEngine(
        server, kDelliciusQueries, kQueryRules, &clock, params);
    EXPECT_TRUE(query_engine.ok());

    std::string query_result_str = R"pb(
      results {
        key: "AssemblyCollectorWithPropertyNameNormalization"
        value {
          query_id: "AssemblyCollectorWithPropertyNameNormalization"
          stats {}
          data {
            fields {
              key: "Chassis"
              value {
                list_value {
                  values {
                    subquery_value {
                      fields {
                        key: "_id_"
                        value { identifier { local_devpath: "/phys" } }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    )pb";
    QueryIdToResult intent_output_sensor =
        ParseTextAsProtoOrDie<QueryIdToResult>(query_result_str);

    QueryIdToResult response_entries = query_engine->ExecuteRedpathQuery(
        {"AssemblyCollectorWithPropertyNameNormalization"});
    VerifyQueryResults(std::move(response_entries),
                       {std::move(intent_output_sensor)});
  }

  {
    QueryEngineParams params{
        .stable_id_type =
            QueryEngineParams::RedfishStableIdType::kRedfishLocationDerived};
    absl::StatusOr<QueryEngine> query_engine = GetDefaultQueryEngine(
        server, kDelliciusQueries, kQueryRules, &clock, params);
    EXPECT_TRUE(query_engine.ok());

    std::string query_result_str = R"pb(
      results {
        key: "AssemblyCollectorWithPropertyNameNormalization"
        value {
          query_id: "AssemblyCollectorWithPropertyNameNormalization"
          stats {}
          data {
            fields {
              key: "Chassis"
              value {
                list_value {
                  values {
                    subquery_value {
                      fields {
                        key: "_id_"
                        value { identifier { local_devpath: "/phys" } }
                      }
                    }
                  }
                  values {
                    subquery_value {
                      fields {
                        key: "_id_"
                        value {
                          identifier { local_devpath: "/phys:device:erot-gpu1" }
                        }
                      }
                    }
                  }
                  values {
                    subquery_value {
                      fields {
                        key: "_id_"
                        value {
                          identifier { local_devpath: "/phys:device:erot-gpu2" }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    )pb";
    QueryIdToResult intent_output_sensor =
        ParseTextAsProtoOrDie<QueryIdToResult>(query_result_str);

    QueryIdToResult response_entries = query_engine->ExecuteRedpathQuery(
        {"AssemblyCollectorWithPropertyNameNormalization"});
    ASSERT_EQ(response_entries.results().size(), 1);
    VerifyQueryResults(std::move(response_entries),
                       {std::move(intent_output_sensor)});
  }
}

TEST(QueryEngineTest, QueryEngineLocationContextSuccess) {
  QueryIdToResult intent_output_embedded_location =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(GetTestDataDependencyPath(
          JoinFilePaths(kQuerySamplesLocation,
                        "query_out/embedded_location_out.textproto")));
  FakeRedfishServer server(kIndusMockup);
  std::string deeply_nested_resource_uri = "/redfish/v1/deeply/nested/resource";
  std::string deeply_nested_resource_data = R"json(
            {
              "@odata.id": "/redfish/v1/deeply/nested/resource",
              "Id": "resource",
              "Location": {
                "Oem": {
                  "Google": {
                    "EmbeddedLocationContext": "sub-fru/logical"
                  }
                },
                "PartLocation": {
                  "ServiceLabel": "board1"
                }
              }
            }
           )json";
  server.AddHttpGetHandlerWithData(deeply_nested_resource_uri,
                                   deeply_nested_resource_data);
  // Set up Query Engine with ID assigner. This is needed for the normalizer to
  // assign the embedded location context.
  absl::flat_hash_map<std::string, std::string> devpath_map = {};
  auto id_assigner = NewMapBasedDevpathAssigner(devpath_map);
  FakeClock clock{clock_time};
  FakeRedfishServer::Config config = server.GetConfig();
  auto http_client = std::make_unique<CurlHttpClient>(
      LibCurlProxy::CreateInstance(), HttpCredential{});
  std::string network_endpoint =
      absl::StrFormat("%s:%d", config.hostname, config.port);
  std::unique_ptr<RedfishTransport> transport =
      HttpRedfishTransport::MakeNetwork(std::move(http_client),
                                        network_endpoint);
  // Set up Query Engine with query files and query rules.
  QueryContext query_context{.query_files = kDelliciusQueries,
                             .query_rules = kQueryRules,
                             .clock = &clock};
  absl::StatusOr<QueryEngine> query_engine = CreateQueryEngine(
      query_context,
      {.transport = std::move(transport),
       .stable_id_type =
           QueryEngineParams::RedfishStableIdType::kRedfishLocation},
      std::move(id_assigner));
  ASSERT_TRUE(query_engine.ok());
  QueryIdToResult response_entries =
      query_engine->ExecuteRedpathQuery({"EmbeddedResource"});
  ASSERT_EQ(response_entries.results().size(), 1);
  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_output_embedded_location)});
}

TEST(QueryEngineTest, QueryEngineSubRootStableIdServiceLabel) {
  QueryIdToResult intent_output_sub_root_location =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(GetTestDataDependencyPath(
          JoinFilePaths(kQuerySamplesLocation,
                        "query_out/sub_root_location_out.textproto")));
  FakeRedfishServer server(kIndusMockup);
  std::string sub_root_resource_uri = "/redfish/v1/root/resource";
  std::string sub_root_resource_data = R"json(
            {
              "@odata.id": "/redfish/v1/root_chassis/resource",
              "Id": "resource",
              "Location": {
                "Oem": {
                  "Google": {
                    "EmbeddedLocationContext": "resource"
                  }
                },
                "PartLocation": {
                  "ServiceLabel": ""
                }
              }
            }
           )json";
  server.AddHttpGetHandlerWithData(sub_root_resource_uri,
                                   sub_root_resource_data);
  // Set up Query Engine with ID assigner. This is needed for the normalizer to
  // assign the stable id fields.
  absl::flat_hash_map<std::string, std::string> devpath_map = {};
  auto id_assigner = NewMapBasedDevpathAssigner(devpath_map);
  FakeClock clock{clock_time};
  FakeRedfishServer::Config config = server.GetConfig();
  auto http_client = std::make_unique<CurlHttpClient>(
      LibCurlProxy::CreateInstance(), HttpCredential{});
  std::string network_endpoint =
      absl::StrFormat("%s:%d", config.hostname, config.port);
  std::unique_ptr<RedfishTransport> transport =
      HttpRedfishTransport::MakeNetwork(std::move(http_client),
                                        network_endpoint);
  // Set up Query Engine with query files and query rules.
  QueryContext query_context{.query_files = kDelliciusQueries,
                             .query_rules = kQueryRules,
                             .clock = &clock};
  absl::StatusOr<QueryEngine> query_engine = CreateQueryEngine(
      query_context,
      {.transport = std::move(transport),
       .stable_id_type =
           QueryEngineParams::RedfishStableIdType::kRedfishLocation},
      std::move(id_assigner));
  ASSERT_TRUE(query_engine.ok());
  QueryIdToResult response_entries =
      query_engine->ExecuteRedpathQuery({"SubRootResource"});
  ASSERT_EQ(response_entries.results().size(), 1);
  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_output_sub_root_location)});
}

TEST(QueryEngineTest, QueryEngineCreateUsingQuerySpec) {
  FakeRedfishServer server(kComponentIntegrityMockupPath);
  FakeClock clock{clock_time};

  FakeRedfishServer::Config config = server.GetConfig();
  auto http_client = std::make_unique<CurlHttpClient>(
      LibCurlProxy::CreateInstance(), HttpCredential{});
  std::string network_endpoint =
      absl::StrFormat("%s:%d", config.hostname, config.port);
  std::unique_ptr<RedfishTransport> transport =
      HttpRedfishTransport::MakeNetwork(std::move(http_client),
                                        network_endpoint);

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext({.query_files = kDelliciusQueries,
                                   .query_rules = kQueryRules,
                                   .clock = &clock}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      QueryEngine::Create(std::move(query_spec),
                          {.transport = std::move(transport)}));

  // Execute query where custom service root is set to /google/v1.
  QueryIdToResult response =
      query_engine->ExecuteRedpathQuery({"CustomServiceRoot"});
  QueryIdToResult intent_output = ParseTextFileAsProtoOrDie<QueryIdToResult>(
      GetTestDataDependencyPath(JoinFilePaths(
          kQuerySamplesLocation,
          "query_out/service_root_google_out_translated.textproto")));

  EXPECT_THAT(
      response.results().at("CustomServiceRoot").data(),
      EqualsProto(intent_output.results().at("GoogleServiceRoot").data()));
}

// Test $filter query, include multiple predicates and variable substitution.
// Filter support is enabled in the query rules.
TEST(QueryEngineTest, QueryEngineFilterConfiguration) {
  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext(
          {.query_files = kDelliciusQueries, .query_rules = kQueryRules}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(std::move(query_spec), kIndusMockup, {}));

  QueryVariables::VariableValue val1;
  val1.set_name("Ceiling");
  *val1.add_values() = "95";
  QueryEngineIntf::QueryVariableSet test_args;
  QueryVariables args1 = QueryVariables();
  *args1.add_variable_values() = val1;
  test_args["FilteredSensorCollector"] = args1;

  QueryIdToResult response_entries = query_engine->ExecuteRedpathQuery(
      {"FilteredSensorCollector"}, QueryEngine::ServiceRootType::kRedfish,
      test_args);

  // Fake Redfish Server currently supports the $filter parameter, but does not
  // actually perform the filtering. Query Planner is still responsible for
  // filtering. At this point we just need to verify the query succeeded.
  // used.
  // based on $filter parameter.
  auto results =
      response_entries.results().at("FilteredSensorCollector").data();
  auto sensors_templated = results.fields().at("SensorsTemplated");
  auto sensors_static = results.fields().at("SensorsStatic");
  EXPECT_EQ(sensors_templated.list_value().values_size(), 6);
  EXPECT_EQ(sensors_static.list_value().values_size(), 7);
}

TEST(QueryEngineTest, CanCreateQueryEngineWithStreamingFeature) {
  FakeRedfishServer server(kComponentIntegrityMockupPath);
  FakeClock clock{clock_time};

  FakeRedfishServer::Config config = server.GetConfig();
  auto http_client = std::make_unique<CurlHttpClient>(
      LibCurlProxy::CreateInstance(), HttpCredential{});
  std::string network_endpoint =
      absl::StrFormat("%s:%d", config.hostname, config.port);
  std::unique_ptr<RedfishTransport> transport =
      HttpRedfishTransport::MakeNetwork(std::move(http_client),
                                        network_endpoint);

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext({.query_files = kDelliciusQueries,
                                   .query_rules = kQueryRules,
                                   .clock = &clock}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      QueryEngine::Create(std::move(query_spec),
                          {.transport = std::move(transport),
                           .features = StreamingQueryEngineFeatures()}));
}

TEST(QueryEngineTest, CanExecuteSubscriptionQuerySuccessfully) {
  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext(
          {.query_files = kDelliciusQueries, .query_rules = kQueryRules}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(
          std::move(query_spec), kIndusMockup,
          {.streaming = FakeQueryEngine::Streaming::kEnable}));

  bool found_thermal_config = false;
  bool found_assembly_collector_config = false;

  // Set up callbacks for subscription query.
  // This is what the user of query engine subscription API provides and in this
  // test we expect these callbacks to be invoked along with the desired
  // parameters.
  auto client_on_event_callback =
      [&](const QueryResult &, const RedPathSubscription::EventContext &) {};
  auto client_on_stop_callback = [&](const absl::Status &) {};

  // Set up a mock subscription broker.
  // In this test we use this mock to inject events and close the stream.
  // We capture callbacks provided by QueryEngine to invoke for injecting
  // events.
  RedPathSubscription::OnEventCallback captured_event_callback;
  RedPathSubscription::OnStopCallback captured_stop_callback;
  auto subscription_broker =
      [&](const std::vector<RedPathSubscription::Configuration> &configurations,
          RedfishInterface &,
          RedPathSubscription::OnEventCallback event_callback,
          RedPathSubscription::OnStopCallback stop_callback)
      -> absl::StatusOr<std::unique_ptr<RedPathSubscription>> {
    for (const auto &configuration : configurations) {
      if (configuration.query_id ==
          "AssemblyCollectorWithPropertyNameNormalization") {
        found_assembly_collector_config = true;
        EXPECT_THAT(
            configuration.uris,
            UnorderedElementsAre(
                "/redfish/v1/Systems/system/Processors?$expand=~($levels=1)"));
        EXPECT_THAT(configuration.predicate, "");
        EXPECT_THAT(configuration.redpath, "/Systems[*]/Processors");
      } else if (configuration.query_id == "Thermal") {
        found_thermal_config = true;
        EXPECT_THAT(
            configuration.uris,
            UnorderedElementsAre("/redfish/v1/Chassis?$expand=.($levels=2)"));
        EXPECT_THAT(configuration.predicate, "");
        EXPECT_THAT(configuration.redpath, "/Chassis");
      }
    }
    captured_event_callback = std::move(event_callback);
    captured_stop_callback = std::move(stop_callback);
    return nullptr;
  };

  // Execute Subscription Query.
  // 1. Setup streaming options
  QueryEngineIntf::StreamingOptions streaming_options{
      .on_event_callback = client_on_event_callback,
      .on_stop_callback = client_on_stop_callback,
      .subscription_broker = subscription_broker};

  // 2. Execute subscription query
  absl::StatusOr<SubscriptionQueryResult> subscription_query_result =
      query_engine->ExecuteSubscriptionQuery(
          {"AssemblyCollectorWithPropertyNameNormalization", "Thermal"},
          streaming_options);
  EXPECT_THAT(subscription_query_result.status(), IsOk());
  EXPECT_TRUE(found_assembly_collector_config);
  EXPECT_TRUE(found_thermal_config);

  // Verify the query result captured in subscription query. This will have
  // results for all subqueries that don't have a subscribe rule configured.
  EXPECT_THAT(subscription_query_result->result.results()
                  .at("AssemblyCollectorWithPropertyNameNormalization")
                  .data(),
              IgnoringRepeatedFieldOrdering(EqualsProto(R"pb(
                fields {
                  key: "Chassis"
                  value {
                    list_value {
                      values {
                        subquery_value {
                          fields {
                            key: "_id_"
                            value { identifier { local_devpath: "/phys" } }
                          }
                          fields {
                            key: "part_number"
                            value { string_value: "1043652-02" }
                          }
                          fields {
                            key: "serial_number"
                            value { string_value: "MBBQTW194106556" }
                          }
                        }
                      }
                    }
                  }
                }
              )pb")));

  EXPECT_THAT(subscription_query_result->result.results().at("Thermal").data(),
              IgnoringRepeatedFieldOrdering(EqualsProto(R"pb()pb")));
}

TEST(QueryEngineTest, CanHandleEventsCorrectly) {
  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext(
          {.query_files = kDelliciusQueries, .query_rules = kQueryRules}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(
          std::move(query_spec), kIndusMockup,
          {.streaming = FakeQueryEngine::Streaming::kEnable}));

  bool found_assembly_query_result = false;
  bool found_thermal_query_result = false;
  bool found_stream_close = false;

  // Set up callbacks for subscription query.
  // This is what the user of query engine subscription API provides and in this
  // test we expect these callbacks to be invoked along with the desired
  // parameters.
  auto client_on_event_callback =
      [&](const QueryResult &single_query_result,
          const RedPathSubscription::EventContext &context) {
        if (single_query_result.query_id() ==
            "AssemblyCollectorWithPropertyNameNormalization") {
          found_assembly_query_result = true;
        } else if (single_query_result.query_id() == "Thermal") {
          found_thermal_query_result = true;
        }
      };
  auto client_on_stop_callback = [&](const absl::Status &status) {
    found_stream_close = true;
    EXPECT_THAT(status, IsOk());
  };

  // Set up a mock subscription broker.
  // In this test we use this mock to inject events and close the stream.
  // We capture callbacks provided by QueryEngine to invoke for injecting
  // events.
  RedPathSubscription::OnEventCallback captured_event_callback;
  RedPathSubscription::OnStopCallback captured_stop_callback;
  auto subscription_broker =
      [&](const std::vector<RedPathSubscription::Configuration> &configurations,
          RedfishInterface &,
          RedPathSubscription::OnEventCallback event_callback,
          RedPathSubscription::OnStopCallback stop_callback)
      -> absl::StatusOr<std::unique_ptr<RedPathSubscription>> {
    captured_event_callback = std::move(event_callback);
    captured_stop_callback = std::move(stop_callback);
    return nullptr;
  };

  // Execute Subscription Query.
  // 1. Setup streaming options
  QueryEngineIntf::StreamingOptions streaming_options{
      .on_event_callback = client_on_event_callback,
      .on_stop_callback = client_on_stop_callback,
      .subscription_broker = subscription_broker};

  // 2. Execute subscription query
  absl::StatusOr<SubscriptionQueryResult> subscription_query_result =
      query_engine->ExecuteSubscriptionQuery(
          {"AssemblyCollectorWithPropertyNameNormalization", "Thermal"},
          streaming_options);
  ASSERT_THAT(subscription_query_result.status(), IsOk());

  // Now send events one by one.
  // Note that we are using a collection type resource in the mock events below
  // but they need not be only collections. These can be any redfish resource.
  // 1. Send event for assembly
  // Mock event.
  std::unique_ptr<ecclesia::RedfishInterface> processor_json =
      NewJsonMockupInterface(R"json(
    {
        "@odata.context":
"/redfish/v1/$metadata#ProcessorCollection.ProcessorCollection",
        "@odata.id": "/redfish/v1/Systems/system/Processors",
        "@odata.type": "#ProcessorCollection.ProcessorCollection",
        "Members": [
            {
                "@odata.id": "/redfish/v1/Systems/system/Processors/0"
            },
            {
                "@odata.id": "/redfish/v1/Systems/system/Processors/1"
            }
        ],
        "Members@odata.count": 2,
        "Name": "Processor Collection"
    }
  )json");
  RedfishVariant processor_variant = processor_json->GetRoot();
  captured_event_callback(
      processor_variant,
      {.query_id = "AssemblyCollectorWithPropertyNameNormalization",
       .redpath = "/Systems[*]/Processors",
       .event_id = "foo",
       .event_timestamp = "bar"});
  EXPECT_TRUE(found_assembly_query_result);

  // 2. Send event for thermal
  std::unique_ptr<ecclesia::RedfishInterface> chassis_json =
      NewJsonMockupInterface(R"json(
    {
      "@odata.context":
      "/redfish/v1/$metadata#ChassisCollection.ChassisCollection",
      "@odata.id": "/redfish/v1/Chassis",
      "@odata.type": "#ChassisCollection.ChassisCollection",
      "Members": [
          {
              "@odata.id": "/redfish/v1/Chassis/chassis"
          }
      ],
      "Members@odata.count": 1,
      "Name": "Indus"
    }
  )json");
  ecclesia::RedfishVariant chassis_variant = chassis_json->GetRoot();
  captured_event_callback(chassis_variant, {.query_id = "Thermal",
                                            .redpath = "/Chassis",
                                            .event_id = "foo",
                                            .event_timestamp = "bar"});
  EXPECT_TRUE(found_thermal_query_result);

  // 3. Close the stream
  captured_stop_callback(absl::OkStatus());
  EXPECT_TRUE(found_stream_close);
}

TEST(QueryEngineTest, CanHandleInvalidEvents) {
  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext(
          {.query_files = kDelliciusQueries, .query_rules = kQueryRules}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(
          std::move(query_spec), kIndusMockup,
          {.streaming = FakeQueryEngine::Streaming::kEnable}));

  bool found_assembly_query_result = false;
  bool found_thermal_query_result = false;

  // Set up callbacks for subscription query.
  // This is what the user of query engine subscription API provides and in this
  // test we expect these callbacks to be invoked along with the desired
  // parameters.
  auto client_on_event_callback =
      [&](const QueryResult &single_query_result,
          const RedPathSubscription::EventContext &context) {
        if (single_query_result.query_id() ==
            "AssemblyCollectorWithPropertyNameNormalization") {
          found_assembly_query_result = true;
        } else if (single_query_result.query_id() == "Thermal") {
          found_thermal_query_result = true;
        }
      };
  auto client_on_stop_callback = [&](const absl::Status &) {};

  // Set up a mock subscription broker.
  // In this test we use this mock to inject events and close the stream.
  // We capture callbacks provided by QueryEngine to invoke for injecting
  // events.
  RedPathSubscription::OnEventCallback captured_event_callback;
  RedPathSubscription::OnStopCallback captured_stop_callback;
  auto subscription_broker =
      [&](const std::vector<RedPathSubscription::Configuration> &configurations,
          RedfishInterface &,
          RedPathSubscription::OnEventCallback event_callback,
          RedPathSubscription::OnStopCallback stop_callback)
      -> absl::StatusOr<std::unique_ptr<RedPathSubscription>> {
    captured_event_callback = std::move(event_callback);
    captured_stop_callback = std::move(stop_callback);
    return nullptr;
  };

  // Execute Subscription Query.
  // 1. Setup streaming options
  QueryEngineIntf::StreamingOptions streaming_options{
      .on_event_callback = client_on_event_callback,
      .on_stop_callback = client_on_stop_callback,
      .subscription_broker = subscription_broker};

  // 2. Execute subscription query
  absl::StatusOr<SubscriptionQueryResult> subscription_query_result =
      query_engine->ExecuteSubscriptionQuery(
          {"AssemblyCollectorWithPropertyNameNormalization", "Thermal"},
          streaming_options);
  ASSERT_THAT(subscription_query_result.status(), IsOk());

  // Now send events one by one.
  // Note that we are using a collection type resource in the mock events below
  // but they need not be only collections. These can be any redfish resource.
  // 1. Send invalid event for assembly - wrong redpath
  // Mock event.
  std::unique_ptr<ecclesia::RedfishInterface> processor_json =
      NewJsonMockupInterface(R"json(
    {
    }
  )json");
  RedfishVariant processor_variant = processor_json->GetRoot();
  captured_event_callback(
      processor_variant,
      {.query_id = "AssemblyCollectorWithPropertyNameNormalization",
       .redpath = "/Systems",
       .event_id = "foo",
       .event_timestamp = "bar"});
  EXPECT_FALSE(found_assembly_query_result);

  // 2. Send invalid event for thermal - wrong query id
  std::unique_ptr<ecclesia::RedfishInterface> chassis_json =
      NewJsonMockupInterface(R"json(
    {
    }
  )json");
  ecclesia::RedfishVariant chassis_variant = chassis_json->GetRoot();
  captured_event_callback(chassis_variant, {.query_id = "InvalidQueryId",
                                            .redpath = "/Chassis",
                                            .event_id = "foo",
                                            .event_timestamp = "bar"});
  EXPECT_FALSE(found_thermal_query_result);
}

TEST(QueryEngineTest, CanHandleInvalidSubscriptionRequest) {
  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext(
          {.query_files = kDelliciusQueries, .query_rules = kQueryRules}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(
          std::move(query_spec), kIndusMockup,
          {.streaming = FakeQueryEngine::Streaming::kEnable}));

  // Set up callbacks for subscription query.
  auto client_on_event_callback =
      [&](const QueryResult &, const RedPathSubscription::EventContext &) {};
  auto client_on_stop_callback = [&](const absl::Status &) {};

  // Execute Subscription Query.
  // 1. Setup streaming options
  QueryEngineIntf::StreamingOptions streaming_options{
      .on_event_callback = client_on_event_callback,
      .on_stop_callback = client_on_stop_callback};

  // 2. Execute query that does not have a subscription rule.
  absl::StatusOr<SubscriptionQueryResult> subscription_query_result =
      query_engine->ExecuteSubscriptionQuery({"ManagerCollector"},
                                             streaming_options);
  EXPECT_THAT(subscription_query_result.status(), IsStatusInternal());
}

TEST(QueryEngineTest, QueryEngineFailsOnServiceUnavailability) {
  FakeRedfishServer server(kIndusMockup);
  std::string rotational_sensor_uri =
      "/redfish/v1/Chassis/chassis/Sensors/indus_fan4_rpm";
  server.AddHttpGetHandlerWithStatus(rotational_sensor_uri, "",
                                     HTTPStatusCode::SERVICE_UNAV);
  FakeClock clock{clock_time};
  FakeRedfishServer::Config config = server.GetConfig();
  auto http_client = std::make_unique<CurlHttpClient>(
      LibCurlProxy::CreateInstance(), HttpCredential{});
  std::string network_endpoint =
      absl::StrFormat("%s:%d", config.hostname, config.port);
  std::unique_ptr<RedfishTransport> transport =
      HttpRedfishTransport::MakeNetwork(std::move(http_client),
                                        network_endpoint);

  QueryContext query_context{.query_files = kDelliciusQueries,
                             .query_rules = kQueryRules,
                             .clock = &clock};
  absl::StatusOr<QueryEngine> query_engine =
      CreateQueryEngine(query_context, {.transport = std::move(transport)});
  ASSERT_TRUE(query_engine.ok());
  QueryIdToResult query_result =
      query_engine->ExecuteRedpathQuery({"SensorCollectorWithChassisLinks"});
  EXPECT_THAT(query_result.results()
                  .at("SensorCollectorWithChassisLinks")
                  .status()
                  .error_code(),
              Eq(ecclesia::ErrorCode::ERROR_UNAVAILABLE));
}

TEST(QueryEngineTest, QueryEngineQueriesAutoExpandResourceOnce) {
  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext(
          {.query_files = kDelliciusQueries, .query_rules = kQueryRules}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(
          std::move(query_spec), kIndusHmbCnMockup,
          {.devpath = FakeQueryEngine::Devpath::kDisable,
           .metrics = FakeQueryEngine::Metrics::kEnable,
           .annotations = FakeQueryEngine::Annotations::kDisable}));

  QueryIdToResult response_entries =
      query_engine->ExecuteRedpathQuery({"AssemblyAutoExpand"});

  int assembly_fetched_counter = 0;
  for (const auto &uri_x_metric : response_entries.results()
                                      .at("AssemblyAutoExpand")
                                      .stats()
                                      .redfish_metrics()
                                      .uri_to_metrics_map()) {
    // We expect assembly resource to be fetched.
    if (uri_x_metric.first == "/redfish/v1/Chassis/chassis/Assembly") {
      assembly_fetched_counter++;
      for (const auto &metadata :
           uri_x_metric.second.request_type_to_metadata()) {
        EXPECT_EQ(metadata.second.request_count(), 1);
      }
      continue;
    }

    // We don't expect individual assemblies to be fetched since they are
    // auto-expanded.
    EXPECT_NE(uri_x_metric.first,
              "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/0");
  }

  // Expect only 1 assembly fetch.
  EXPECT_THAT(assembly_fetched_counter, Eq(1));
}

// The goal of this test is to ensure thread safety of QueryEngine by checking
// that each query result returns 1 uri count for assembly resource.
// In the case of failure, the URI counts will be unevenly distributed.
// Also this test is run with tsan to detect any data race.
TEST(QueryEngineTest, QueryEngineQueriesAutoExpandResourceOnceMultithreaded) {
  constexpr int kNumThreads = 10;
  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext(
          {.query_files = kDelliciusQueries, .query_rules = kQueryRules}));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(
          std::move(query_spec), kIndusHmbCnMockup,
          {.devpath = FakeQueryEngine::Devpath::kDisable,
           .metrics = FakeQueryEngine::Metrics::kEnable,
           .annotations = FakeQueryEngine::Annotations::kDisable,
           .cache = FakeQueryEngine::Cache::kDisable}));

  ThreadFactoryInterface *thread_factory = GetDefaultThreadFactory();
  std::vector<std::unique_ptr<ThreadInterface>> threads(kNumThreads);
  absl::Notification notification;
  for (int i = 0; i < kNumThreads; ++i) {
    threads.push_back(thread_factory->New([&]() {
      // Wait for notification.
      notification.WaitForNotification();
      QueryIdToResult response_entries =
          query_engine->ExecuteRedpathQuery({"AssemblyAutoExpand"});
      int assembly_fetched_counter = 0;
      for (const auto &uri_x_metric : response_entries.results()
                                          .at("AssemblyAutoExpand")
                                          .stats()
                                          .redfish_metrics()
                                          .uri_to_metrics_map()) {
        // We expect assembly resource to be fetched.
        if (uri_x_metric.first == "/redfish/v1/Chassis/chassis/Assembly") {
          assembly_fetched_counter++;
          for (const auto &metadata :
               uri_x_metric.second.request_type_to_metadata()) {
            EXPECT_EQ(metadata.second.request_count(), 1);
          }
          continue;
        }

        // We don't expect individual assemblies to be fetched since they are
        // auto-expanded.
        EXPECT_NE(uri_x_metric.first,
                  "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/0");
      }
      // Expect only 1 assembly fetch.
      EXPECT_THAT(assembly_fetched_counter, Eq(1));
    }));
  }
  notification.Notify();
  for (std::unique_ptr<ThreadInterface> &thread : threads) {
    if (thread) {
      thread->Join();
    }
  }
}

TEST(QueryEngineTest, QueryEngineExecutesQueryRuleWithUriPrefix) {
  constexpr absl::string_view kQueryRuleStr = R"pb(
    query_id_to_params_rule {
      key: "AssemblyCollectorWithPropertyNameNormalization"
      value {
        redpath_prefix_with_params { redpath: "/Systems" uri_prefix: "/tlbmc" }
      }
    })pb";

  auto query_spec = QuerySpec::FromQueryContext(
      {.query_files = kDelliciusQueries,
       .query_rules = {{.name = "query_rules.pb", .data = kQueryRuleStr}}});
  ASSERT_THAT(query_spec, IsOk());
  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(
          std::move(*query_spec), kIndusMockup,
          {.devpath = FakeQueryEngine::Devpath::kDisable,
           .metrics = FakeQueryEngine::Metrics::kEnable,
           .annotations = FakeQueryEngine::Annotations::kDisable,
           .cache = FakeQueryEngine::Cache::kInfinite}));

  int systems_fetched_counter = 0;
  QueryIdToResult response = query_engine->ExecuteRedpathQuery(
      {"AssemblyCollectorWithPropertyNameNormalization"});
  // Validate the stats are correct.
  for (const auto &uri_x_metric :
       response.results()
           .at("AssemblyCollectorWithPropertyNameNormalization")
           .stats()
           .redfish_metrics()
           .uri_to_metrics_map()) {
    // Expected systems only fetched only once from the redfish server.
    if (uri_x_metric.first == "/tlbmc/redfish/v1/Systems") {
      systems_fetched_counter++;
    }
  }
  EXPECT_EQ(systems_fetched_counter, 1);
}

TEST(QueryEngineTest, QueryEngineAppliesQueryRulesToServiceRoot) {
  constexpr absl::string_view kQueryRuleStr = R"pb(
    query_id_to_params_rule {
      key: "AssemblyCollectorWithPropertyNameNormalization"
      value {
        redpath_prefix_with_params { redpath: "/" uri_prefix: "/tlbmc" }
      }
    })pb";

  auto query_spec = QuerySpec::FromQueryContext(
      {.query_files = kDelliciusQueries,
       .query_rules = {{.name = "query_rules.pb", .data = kQueryRuleStr}}});
  ASSERT_THAT(query_spec, IsOk());
  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FakeQueryEngine::Create(
          std::move(*query_spec), kIndusMockup,
          {.devpath = FakeQueryEngine::Devpath::kDisable,
           .metrics = FakeQueryEngine::Metrics::kEnable,
           .annotations = FakeQueryEngine::Annotations::kDisable,
           .cache = FakeQueryEngine::Cache::kInfinite}));

  int service_root_fetch_counter = 0;
  QueryIdToResult response = query_engine->ExecuteRedpathQuery(
      {"AssemblyCollectorWithPropertyNameNormalization"});
  // Validate the stats are correct.
  for (const auto &uri_x_metric :
       response.results()
           .at("AssemblyCollectorWithPropertyNameNormalization")
           .stats()
           .redfish_metrics()
           .uri_to_metrics_map()) {
    // Expected systems only fetched only once from the redfish server.
    if (uri_x_metric.first == "/tlbmc/redfish/v1") {
      service_root_fetch_counter++;
    }
  }
  EXPECT_EQ(service_root_fetch_counter, 1);
}

}  // namespace
}  // namespace ecclesia
