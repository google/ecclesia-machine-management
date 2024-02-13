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
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/apifs/apifs.h"
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
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_router/query_router_spec.pb.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/transport/http.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "ecclesia/lib/status/test_macros.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/lib/time/clock_fake.h"
#include "tensorflow_serving/util/net_http/public/response_code_enum.h"

namespace ecclesia {

namespace {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedPointwise;

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

void VerifyTrackedPathWithParamsMatchExpected(
    const std::vector<RedPathToQueryParams> &actual_configs) {
  const std::vector<RedPathToQueryParams> expected_configs = {
      RedPathToQueryParams{"/Chassis", "$expand=.($levels=1)"},
      RedPathToQueryParams{"/Chassis[*]", ""},
      RedPathToQueryParams{"/Systems[*]/Processors", "$expand=~($levels=1)"},
      RedPathToQueryParams{"/Systems", "$expand=*($levels=1)"},
      RedPathToQueryParams{"/Systems[*]", ""},
      RedPathToQueryParams{"/Systems[*]/Memory", "$expand=*($levels=1)"},
      RedPathToQueryParams{"/Systems[*]/Processors[*]", ""}};
  EXPECT_THAT(actual_configs,
              UnorderedPointwise(RedPathExpandConfigsEq(), expected_configs));
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
                        bool check_timestamps = false,
                        bool check_metrics = false) {
  if (!check_timestamps) {
    RemoveTimestamps(actual_entries);
    RemoveTimestamps(expected_entries);
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
      FakeQueryEngine::Create(std::move(query_spec), kIndusMockup,
                              {.devpath = FakeQueryEngine::Devpath::kDisable,
                               .metrics = FakeQueryEngine::Metrics::kEnable,
                               .cache = FakeQueryEngine::Cache::kInfinite}));

  {
    // Query assemblies 3 times in a row. Cache is cold only for the 1st query.
    QueryIdToResult first_response = query_engine->ExecuteRedpathQuery(
        {"AssemblyCollectorWithPropertyNameNormalization"});
    QueryIdToResult second_response = query_engine->ExecuteRedpathQuery(
        {"AssemblyCollectorWithPropertyNameNormalization"});
    QueryIdToResult third_response = query_engine->ExecuteRedpathQuery(
        {"AssemblyCollectorWithPropertyNameNormalization"});

    int systems_fetched_counter = 0;
    int processor_collection_fetched_counter = 0;
    for (const QueryIdToResult &response_entries :
         {first_response, second_response, third_response}) {
      // Copy stats from the expected result for data verification first.
      *intent_output_assembly.mutable_results()
           ->at("AssemblyCollectorWithPropertyNameNormalization")
           .mutable_stats() =
          response_entries.results()
              .at("AssemblyCollectorWithPropertyNameNormalization")
              .stats();
      VerifyQueryResults(response_entries, intent_output_assembly);

      // Validate the stats are correct.
      for (const auto &uri_x_metric :
           response_entries.results()
               .at("AssemblyCollectorWithPropertyNameNormalization")
               .stats()
               .redfish_metrics()
               .uri_to_metrics_map()) {
        // Expected systems only fetched only once from the redfish server.
        if (uri_x_metric.first == "/redfish/v1/Systems") {
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
      FakeQueryEngine::Create(std::move(query_spec), kIndusHmbCnMockup,
                              {.devpath = FakeQueryEngine::Devpath::kDisable,
                               .metrics = FakeQueryEngine::Metrics::kEnable,
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
    // Query again. This time all resources upto Thermal should be served
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
  val1.set_value("Temperature");
  val2.set_name("Units");
  val2.set_value("Cel");
  val3.set_name("Threshold");
  val3.set_value("40");
  QueryEngineIntf::QueryVariableSet test_args;
  QueryVariables args1 = QueryVariables();
  *args1.add_values() = val1;
  *args1.add_values() = val2;
  *args1.add_values() = val3;
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
  val1.set_value("Cel");
  // Type and units will remain unset
  QueryEngineIntf::QueryVariableSet test_args;
  QueryVariables args1 = QueryVariables();
  *args1.add_values() = val1;
  // Pass in an empty value to make sure it doesn't mess up the variable
  // substitution.
  *args1.add_values() = val2;
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
  val1.set_value("Cel");
  // Type and units will remain unset
  QueryEngineIntf::QueryVariableSet test_args;
  QueryVariables args1 = QueryVariables();
  *args1.add_values() = val1;
  *args1.add_values() = val2;
  test_args["SensorCollectorTemplate"] = args1;

  // Set the variables for filter criteria #2
  QueryVariables::VariableValue val3;
  QueryVariables::VariableValue val4;
  QueryVariables::VariableValue val5;
  val3.set_name("Type");
  val3.set_value("Temperature");
  val4.set_name("Units");
  val4.set_value("Cel");
  val5.set_name("Threshold");
  val5.set_value("40");
  QueryEngineIntf::QueryVariableSet test_args_filtered;
  QueryVariables args2 = QueryVariables();
  *args2.add_values() = val3;
  *args2.add_values() = val4;
  *args2.add_values() = val5;
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
  server.AddHttpGetHandlerWithStatus(
      temp_sensor_uri, "",
      tensorflow::serving::net_http::HTTPStatusCode::SERVICE_UNAV);
  server.AddHttpGetHandlerWithStatus(
      rotational_sensor_uri, "",
      tensorflow::serving::net_http::HTTPStatusCode::SERVICE_UNAV);
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
              "Oem": {
                "Google": {
                  "LocationContext": {
                    "ServiceLabel": "board1",
                    "EmbeddedLocationContext": ["sub-fru", "logical"]
                  }
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

// Custom matcher to match QueryInfo struct.
MATCHER_P(QueryInfoEq, query_info, "") {
  return ExplainMatchResult(EqualsProto(query_info.query), arg.query,
                            result_listener) &&
         ExplainMatchResult(EqualsProto(query_info.rule), arg.rule,
                            result_listener);
}

TEST(QuerySpecTest, ConvertFromQueryContextPass) {
  DelliciusQuery query_a = ParseTextProtoOrDie(
      R"pb(query_id: "query_a"
           property_sets { properties { property: "property_1" type: STRING } }
      )pb");

  DelliciusQuery query_b = ParseTextProtoOrDie(
      R"pb(query_id: "query_b"
           property_sets { properties { property: "property_1" type: STRING } }
      )pb");

  QueryRules query_rules = ParseTextProtoOrDie(
      R"pb(query_id_to_params_rule {
             key: "query_a"
             value {
               redpath_prefix_with_params {
                 expand_configuration { level: 1 type: BOTH }
               }
             }
           }
      )pb");

  std::string query_a_contents = query_a.ShortDebugString();
  std::string query_b_contents = query_b.ShortDebugString();
  std::string rule_contents = query_rules.ShortDebugString();

  std::vector<EmbeddedFile> queries = {
      EmbeddedFile{.name = "query_a.textproto", .data = query_a_contents},
      EmbeddedFile{.name = "query_b.textproto", .data = query_b_contents},
  };

  std::vector<EmbeddedFile> rules = {
      EmbeddedFile{.name = "query_rules.textproto", .data = rule_contents},
  };

  FakeClock clock;

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec spec,
      QuerySpec::FromQueryContext(
          {.query_files = queries, .query_rules = rules, .clock = &clock}));
  EXPECT_THAT(
      spec.query_id_to_info,
      UnorderedElementsAre(
          Pair("query_a",
               QueryInfoEq(QuerySpec::QueryInfo{
                   .query = std::move(query_a),
                   .rule = std::move(
                       query_rules.mutable_query_id_to_params_rule()->at(
                           "query_a"))})),
          Pair("query_b", QueryInfoEq(QuerySpec::QueryInfo{
                              .query = std::move(query_b)}))));
  EXPECT_EQ(spec.clock, &clock);
}

TEST(QuerySpecTest, ConvertFromQueryContextDuplicateQueries) {
  DelliciusQuery query_a = ParseTextProtoOrDie(
      R"pb(query_id: "query_a"
           property_sets { properties { property: "property_1" type: STRING } }
      )pb");

  std::string query_a_contents = query_a.ShortDebugString();

  std::vector<EmbeddedFile> queries = {
      EmbeddedFile{.name = "query_a.textproto", .data = query_a_contents},
      EmbeddedFile{.name = "query_b.textproto", .data = query_a_contents},
  };

  EXPECT_THAT(QuerySpec::FromQueryContext({.query_files = queries}),
              IsStatusInternal());
}

TEST(QuerySpecTest, ConvertFromQueryContextUnableToReadQuery) {
  std::vector<EmbeddedFile> queries = {
      EmbeddedFile{.name = "query_a.textproto", .data = "UNKNOWN"}};

  EXPECT_THAT(QuerySpec::FromQueryContext({.query_files = queries}),
              IsStatusInternal());
}

TEST(QuerySpecTest, ConvertFromQueryContextUnableToReadQueryRule) {
  DelliciusQuery query_a = ParseTextProtoOrDie(
      R"pb(query_id: "query_a"
           property_sets { properties { property: "property_1" type: STRING } }
      )pb");

  std::string query_a_contents = query_a.ShortDebugString();

  std::vector<EmbeddedFile> queries = {
      EmbeddedFile{.name = "query_a.textproto", .data = query_a_contents},
  };

  std::vector<EmbeddedFile> rules = {
      EmbeddedFile{.name = "query_rules.textproto", .data = "UNKNOWN"},
  };

  EXPECT_THAT(QuerySpec::FromQueryContext(
                  {.query_files = queries, .query_rules = rules}),
              IsStatusInternal());
}

class QuerySpecConvertTest : public testing::Test {
 protected:
  QuerySpecConvertTest()
      : fs_(GetTestTempdirPath()), apifs_(GetTestTempdirPath("test")) {
    fs_.CreateDir("/tmp/test");
  }

  TestFilesystem fs_;
  ApifsDirectory apifs_;
};

TEST_F(QuerySpecConvertTest, ConvertFromQueryFilesPass) {
  DelliciusQuery query_a = ParseTextProtoOrDie(
      R"pb(query_id: "query_a"
           property_sets { properties { property: "property_1" type: STRING } }
      )pb");

  DelliciusQuery query_b = ParseTextProtoOrDie(
      R"pb(query_id: "query_b"
           property_sets { properties { property: "property_1" type: STRING } }
      )pb");

  QueryRules query_rules = ParseTextProtoOrDie(
      R"pb(query_id_to_params_rule {
             key: "query_a"
             value {
               redpath_prefix_with_params {
                 expand_configuration { level: 1 type: BOTH }
               }
             }
           }
      )pb");

  std::string query_a_contents = query_a.ShortDebugString();
  std::string query_b_contents = query_b.ShortDebugString();
  std::string rule_contents = query_rules.ShortDebugString();

  fs_.CreateFile("/tmp/test/query_a.textproto", query_a_contents);
  fs_.CreateFile("/tmp/test/query_b.textproto", query_b_contents);
  fs_.CreateFile("/tmp/test/query_rule.textproto", rule_contents);

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec spec, QuerySpec::FromQueryFiles(
                          {fs_.GetTruePath("/tmp/test/query_a.textproto"),
                           fs_.GetTruePath("/tmp/test/query_b.textproto")},
                          {fs_.GetTruePath("/tmp/test/query_rule.textproto")}));

  EXPECT_THAT(
      spec.query_id_to_info,
      UnorderedElementsAre(
          Pair("query_a",
               QueryInfoEq(QuerySpec::QueryInfo{
                   .query = std::move(query_a),
                   .rule = std::move(
                       query_rules.mutable_query_id_to_params_rule()->at(
                           "query_a"))})),
          Pair("query_b", QueryInfoEq(QuerySpec::QueryInfo{
                              .query = std::move(query_b)}))));
  EXPECT_NE(spec.clock, nullptr);
}

TEST_F(QuerySpecConvertTest, ConvertFromQueryFilesDuplicateQueries) {
  DelliciusQuery query_a = ParseTextProtoOrDie(
      R"pb(query_id: "query_a"
           property_sets { properties { property: "property_1" type: STRING } }
      )pb");

  QueryRules query_rules = ParseTextProtoOrDie(
      R"pb(query_id_to_params_rule {
             key: "query_a"
             value {
               redpath_prefix_with_params {
                 expand_configuration { level: 1 type: BOTH }
               }
             }
           }
      )pb");

  std::string query_a_contents = query_a.ShortDebugString();
  std::string rule_contents = query_rules.ShortDebugString();

  fs_.CreateFile("/tmp/test/query_a.textproto", query_a_contents);
  fs_.CreateFile("/tmp/test/query_b.textproto", query_a_contents);
  fs_.CreateFile("/tmp/test/query_rule.textproto", rule_contents);

  EXPECT_THAT(QuerySpec::FromQueryFiles(
                  {fs_.GetTruePath("/tmp/test/query_a.textproto"),
                   fs_.GetTruePath("/tmp/test/query_b.textproto")},
                  {fs_.GetTruePath("/tmp/test/query_rule.textproto")}),
              IsStatusInternal());
}

TEST_F(QuerySpecConvertTest, ConvertFromQueryFilesUnableToReadQuery) {
  fs_.CreateFile("/tmp/test/query_a.textproto", "UNKNOWN");

  EXPECT_THAT(QuerySpec::FromQueryFiles(
                  {fs_.GetTruePath("/tmp/test/query_a.textproto")}, {}),
              IsStatusInternal());
}

TEST_F(QuerySpecConvertTest, ConvertFromQueryFilesUnableToReadQueryRule) {
  DelliciusQuery query_a = ParseTextProtoOrDie(
      R"pb(query_id: "query_a"
           property_sets { properties { property: "property_1" type: STRING } }
      )pb");

  std::string query_a_contents = query_a.ShortDebugString();

  fs_.CreateFile("/tmp/test/query_a.textproto", query_a_contents);
  fs_.CreateFile("/tmp/test/query_rule.textproto", "UNKNOWN");

  EXPECT_THAT(QuerySpec::FromQueryFiles(
                  {fs_.GetTruePath("/tmp/test/query_a.textproto")},
                  {fs_.GetTruePath("/tmp/test/query_rule.textproto")}),
              IsStatusInternal());
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
  val1.set_value("95");
  QueryEngineIntf::QueryVariableSet test_args;
  QueryVariables args1 = QueryVariables();
  *args1.add_values() = val1;
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

}  // namespace
}  // namespace ecclesia
