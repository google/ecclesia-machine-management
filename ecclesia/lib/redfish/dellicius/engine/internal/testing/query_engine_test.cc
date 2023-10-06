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

#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/passkey.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/testing/test_queries_embedded.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/testing/test_query_rules_embedded.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine_fake.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/transport/http.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/lib/time/clock_fake.h"

namespace ecclesia {

namespace {

using ::testing::IsEmpty;
using ::testing::Not;
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

void VerifyQueryResults(std::vector<DelliciusQueryResult> actual_entries,
                        std::vector<DelliciusQueryResult> expected_entries,
                        bool check_timestamps = false) {
  auto remove_timestamps = [](std::vector<DelliciusQueryResult> &entries) {
    for (DelliciusQueryResult &entry : entries) {
      entry.clear_start_timestamp();
      entry.clear_end_timestamp();
    }
  };

  if (!check_timestamps) {
    remove_timestamps(actual_entries);
    remove_timestamps(expected_entries);
  }

  EXPECT_THAT(actual_entries,
              UnorderedElementsAreArrayOfProtos(expected_entries));
}

void VerifyQueryResults(std::vector<QueryResult> actual_entries,
                        std::vector<QueryResult> expected_entries,
                        bool check_timestamps = false) {
  auto remove_timestamps = [](std::vector<QueryResult> &entries) {
    for (QueryResult &entry : entries) {
      entry.mutable_stats()->clear_start_time();
      entry.mutable_stats()->clear_end_time();
    }
  };

  if (!check_timestamps) {
    remove_timestamps(actual_entries);
    remove_timestamps(expected_entries);
  }

  EXPECT_THAT(actual_entries,
              UnorderedElementsAreArrayOfProtos(expected_entries));
}

absl::StatusOr<QueryEngine> GetDefaultQueryEngine(
    FakeRedfishServer &server,
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
  return CreateQueryEngine(query_context, {.transport = std::move(transport)});
}

TEST(QueryEngineTest, QueryEngineDevpathConfiguration) {
  std::string sensor_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/devpath_sensor_out.textproto"));

  std::vector<DelliciusQueryResult> response_entries =
      FakeQueryEngineEnvironment(
          {.flags{.enable_devpath_extension = true},
           .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}},
          kIndusMockup, clock_time)
          .GetEngine()
          .ExecuteQuery({"SensorCollector"});

  DelliciusQueryResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(sensor_out_path);

  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_output_sensor)});
}

TEST(QueryEngineTest, QueryEngineDefaultConfiguration) {
  std::string sensor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/sensor_out.textproto"));

  std::vector<DelliciusQueryResult> response_entries =
      FakeQueryEngineEnvironment(
          {.flags{.enable_devpath_extension = false},
           .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}},
          kIndusMockup, clock_time)
          .GetEngine()
          .ExecuteQuery({"SensorCollector"});

  DelliciusQueryResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(sensor_out_path);

  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_output_sensor)});
}

TEST(QueryEngineTest, QueryEngineRedfishIntfAccessor) {
  std::string sensor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/sensor_out.textproto"));
  EXPECT_TRUE(
      FakeQueryEngineEnvironment(
          {.flags{.enable_devpath_extension = false},
           .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}},
          kIndusMockup, clock_time)
          .GetEngine()
          // Uniform initialization "{}" in place of the passkey will cause
          // compilation errors.
          .GetRedfishInterface(RedfishInterfacePasskeyFactory::GetPassKey())
          .ok());
}

TEST(QueryEngineTest, QueryEngineWithExpandConfiguration) {
  std::string sensor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/assembly_out.textproto"));

  QueryTracker query_tracker;
  std::vector<DelliciusQueryResult> response_entries =
      FakeQueryEngineEnvironment(
          {.flags{.enable_devpath_extension = false},
           .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()},
           .query_rules{kQueryRules.begin(), kQueryRules.end()}},
          kIndusMockup, clock_time)
          .GetEngine()
          .ExecuteQuery({"AssemblyCollectorWithPropertyNameNormalization"},
                        query_tracker);

  DelliciusQueryResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(sensor_out_path);

  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_output_sensor)});

  std::vector<RedPathToQueryParams> tracked_configs;
  tracked_configs.reserve(query_tracker.redpaths_queried.size());
  for (auto paths_with_params : query_tracker.redpaths_queried) {
    RedPathToQueryParams redpath_to_query_params;
    redpath_to_query_params.tracked_path = paths_with_params.first;
    if (paths_with_params.second.expand.has_value()) {
      redpath_to_query_params.query_params =
          paths_with_params.second.expand->ToString();
    }
    tracked_configs.push_back(redpath_to_query_params);
  }
  VerifyTrackedPathWithParamsMatchExpected(tracked_configs);
}

TEST(QueryEngineTest, QueryEngineInvalidQueries) {
  // Invalid Query Id

  std::vector<DelliciusQueryResult> response_entries =
      FakeQueryEngineEnvironment(
          {.flags{.enable_devpath_extension = false},
           .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}},
          kIndusMockup, clock_time)
          .GetEngine()
          .ExecuteQuery({"ThereIsNoSuchId"});

  EXPECT_EQ(response_entries.size(), 0);
}

TEST(QueryEngineTest, QueryEngineConcurrentQueries) {
  std::string assembly_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/assembly_out.textproto"));
  std::string sensor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/sensor_out.textproto"));

  std::vector<DelliciusQueryResult> response_entries =
      FakeQueryEngineEnvironment(
          {.flags{.enable_devpath_extension = false},
           .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}},
          kIndusMockup, clock_time)
          .GetEngine()
          .ExecuteQuery({"SensorCollector",
                         "AssemblyCollectorWithPropertyNameNormalization"});

  DelliciusQueryResult intent_sensor_out =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(sensor_out_path);
  DelliciusQueryResult intent_assembly_out =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(assembly_out_path);

  VerifyQueryResults(
      std::move(response_entries),
      {std::move(intent_sensor_out), std::move(intent_assembly_out)});
}

TEST(QueryEngineTest, QueryEngineEmptyItemDevpath) {
  std::string assembly_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/devpath_assembly_out.textproto"));

  std::vector<DelliciusQueryResult> response_entries =
      FakeQueryEngineEnvironment(
          {.flags{.enable_devpath_extension = true},
           .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}},
          kIndusMockup, clock_time)
          .GetEngine()
          .ExecuteQuery({"AssemblyCollectorWithPropertyNameNormalization"});

  DelliciusQueryResult intent_assembly_out =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(assembly_out_path);

  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_assembly_out)});
}
TEST(QueryEngineTest, QueryEngineWithCacheConfiguration) {
  std::string assembly_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/assembly_out.textproto"));

  RedfishMetrics metrics;
  FakeQueryEngineEnvironment fake_engine_env(
      {.flags{.enable_devpath_extension = false,
              .enable_transport_metrics = true},
       .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()},
       .query_rules{kQueryRules.begin(), kQueryRules.end()}},
      kIndusMockup, clock_time,
      FakeQueryEngineEnvironment::CachingMode::kNoExpiration);
  QueryEngine &query_engine = fake_engine_env.GetEngine();

  {
    // Query assemblies
    std::vector<DelliciusQueryResult> response_entries =
        query_engine.ExecuteQueryWithAggregatedMetrics(
            {"AssemblyCollectorWithPropertyNameNormalization"}, &metrics);
    DelliciusQueryResult intent_output_assembly =
        ParseTextFileAsProtoOrDie<DelliciusQueryResult>(assembly_out_path);

    VerifyQueryResults(std::move(response_entries),
                       {std::move(intent_output_assembly)});
  }
  {
    // Query assemblies again. This time we expect QueryEngine uses cache.
    std::vector<DelliciusQueryResult> response_entries =
        query_engine.ExecuteQueryWithAggregatedMetrics(
            {"AssemblyCollectorWithPropertyNameNormalization"}, &metrics);
    DelliciusQueryResult intent_output_assembly =
        ParseTextFileAsProtoOrDie<DelliciusQueryResult>(assembly_out_path);
    VerifyQueryResults(std::move(response_entries),
                       {std::move(intent_output_assembly)});
  }
  {
    // Query assemblies again and again we expect QueryEngine to use cache.
    std::vector<DelliciusQueryResult> response_entries =
        query_engine.ExecuteQueryWithAggregatedMetrics(
            {"AssemblyCollectorWithPropertyNameNormalization"}, &metrics);
    DelliciusQueryResult intent_output_assembly =
        ParseTextFileAsProtoOrDie<DelliciusQueryResult>(assembly_out_path);
    VerifyQueryResults(std::move(response_entries),
                       {std::move(intent_output_assembly)});
  }

  bool traced_systems = false;
  bool traced_processor_collection = false;
  for (const auto &uri_x_metric : *metrics.mutable_uri_to_metrics_map()) {
    if (uri_x_metric.first == "/redfish/v1/Systems") {
      traced_systems = true;
      for (const auto &metadata :
           uri_x_metric.second.request_type_to_metadata()) {
        EXPECT_EQ(metadata.second.request_count(), 1);
      }
    }

    // Note query_rule sample_query_rules.textproto uses level 1 expand at
    // Processors collection. But the query has freshness = true for the
    // members in processor collection and not the collection resource. Yet we
    // see the collection getting fetched fresh each time because the freshness
    // requirement bubbles up if child redpath is in expand path of parent
    // redpath.
    if (uri_x_metric.first ==
        "/redfish/v1/Systems/system/Processors?$expand=~($levels=1)") {
      traced_processor_collection = true;
      for (const auto &metadata :
           uri_x_metric.second.request_type_to_metadata()) {
        EXPECT_EQ(metadata.second.request_count(), 3);
      }
    }
  }
  EXPECT_TRUE(traced_systems);
  EXPECT_TRUE(traced_processor_collection);
}

// Tests that when transport metrics are enabled per DelliciusQueryResult,
// the metrics are independent of other DelliciusQueryResults.
TEST(QueryEngineTest, QueryEngineWithTransportMetricsEnabled) {
  // Create QueryEngine with transport metrics
  FakeQueryEngineEnvironment fake_engine_env(
      {.flags{.enable_devpath_extension = false,
              .enable_transport_metrics = true},
       .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()},
       .query_rules{kQueryRules.begin(), kQueryRules.end()}},
      kIndusHmbCnMockup, clock_time,
      FakeQueryEngineEnvironment::CachingMode::kNoExpiration);
  QueryEngine &query_engine = fake_engine_env.GetEngine();
  // Hold all the metrics collected from each query execution to validate later.
  RedfishMetrics metrics_first, metrics_cached;
  {
    // On first query, thermal subsystem won't be queried explicitly since
    // chassis level 2 expand will return thermal objects. Here we expect to see
    // only 1 expand URI dispatched.
    std::vector<DelliciusQueryResult> response_entries =
        query_engine.ExecuteQuery({"Thermal"});
    ASSERT_THAT(response_entries, Not(IsEmpty()));
    metrics_first = response_entries.back().redfish_metrics();
  }
  {
    // Query again. This time all resources upto Thermal should be served from
    // cache. All Thermal objects will be freshly queried.
    std::vector<DelliciusQueryResult> response_entries =
        query_engine.ExecuteQuery({"Thermal"});
    ASSERT_THAT(response_entries, Not(IsEmpty()));
    metrics_cached = response_entries.back().redfish_metrics();
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
  std::string query_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/service_root_google_out.textproto"));

  std::vector<DelliciusQueryResult> response_entries =
      FakeQueryEngineEnvironment(
          {.flags{.enable_devpath_extension = true},
           .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}},
          kComponentIntegrityMockupPath, clock_time)
          .GetEngine()
          .ExecuteQuery({"GoogleServiceRoot"},
                        QueryEngine::ServiceRootType::kGoogle);

  DelliciusQueryResult intent_query_out =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(query_out_path);
  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_query_out)});
}

TEST(QueryEngineTest, QueryEngineTestTemplatedQuery) {
  std::string query_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/sensor_out_template.textproto"));

  // Build the argument map.
  QueryVariables::VariableValue val1, val2, val3;
  val1.set_name("Type");
  val1.set_value("Temperature");
  val2.set_name("Units");
  val2.set_value("Cel");
  val3.set_name("Threshold");
  val3.set_value("40");
  QueryVariableSet test_args = QueryVariableSet();
  QueryVariables args1 = QueryVariables();
  *args1.add_values() = val1;
  *args1.add_values() = val2;
  *args1.add_values() = val3;
  test_args["SensorCollectorTemplate"] = args1;

  std::vector<DelliciusQueryResult> response_entries =
      FakeQueryEngineEnvironment(
          {.flags{.enable_devpath_extension = true},
           .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}},
          kIndusMockup, clock_time)
          .GetEngine()
          .ExecuteQuery({"SensorCollectorTemplate"},
                        QueryEngine::ServiceRootType::kRedfish, test_args);

  DelliciusQueryResult intent_query_out =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(query_out_path);
  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_query_out)});
}

TEST(QueryEngineTest, QueryEngineTestTemplatedUnfilledVars) {
  std::string query_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/sensor_out_template_full.textproto"));

  QueryVariables::VariableValue val1, val2;
  val1.set_name("Units");
  val1.set_value("Cel");
  // Type and units will remain unset
  QueryVariableSet test_args = QueryVariableSet();
  QueryVariables args1 = QueryVariables();
  *args1.add_values() = val1;
  // Pass in an empty value to make sure it doesn't mess up the variable
  // substitution.
  *args1.add_values() = val2;
  test_args["SensorCollectorTemplate"] = args1;

  std::vector<DelliciusQueryResult> response_entries =
      FakeQueryEngineEnvironment(
          {.flags{.enable_devpath_extension = true},
           .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}},
          kIndusMockup, clock_time)
          .GetEngine()
          .ExecuteQuery({"SensorCollectorTemplate"},
                        QueryEngine::ServiceRootType::kRedfish, test_args);

  // Since some of the variables are unfilled only sensors with a unit of "Cel"
  // will be returned. All other parts of the predicate are ignored.
  DelliciusQueryResult intent_query_out =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(query_out_path);
  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_query_out)});
}

TEST(QueryEngineTest, QueryEngineTestTemplatedNoVars) {
  std::string query_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/sensor_out.textproto"));
  std::vector<DelliciusQueryResult> response_entries =
      FakeQueryEngineEnvironment(
          {.query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}},
          kIndusMockup, clock_time)
          .GetEngine()
          .ExecuteQuery({"SensorCollectorTemplate"},
                        QueryEngine::ServiceRootType::kRedfish);

  // Since all variables are unfilled the entire predicate will be replaced with
  // "select all". Therefore the whole set of sensors should be returned.
  DelliciusQueryResult intent_query_out =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(query_out_path);
  // Changing the query ID in the expected result so a whole new output file
  // doesn't need made.
  *intent_query_out.mutable_query_id() = "SensorCollectorTemplate";
  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_query_out)});
}

TEST(QueryEngineTest, QueryEngineAggregatedTransportMetrics) {
  RedfishMetrics transport_metrics;
  FakeQueryEngineEnvironment fake_engine_env(
      {.flags{.enable_devpath_extension = false,
              .enable_transport_metrics = true},
       .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()},
       .query_rules{kQueryRules.begin(), kQueryRules.end()}},
      kIndusMockup, clock_time,
      FakeQueryEngineEnvironment::CachingMode::kNoExpiration);
  QueryEngine &query_engine = fake_engine_env.GetEngine();
  std::vector<DelliciusQueryResult> response_entries =
      query_engine.ExecuteQueryWithAggregatedMetrics(
          {"AssemblyCollectorWithPropertyNameNormalization"},
          &transport_metrics);
  EXPECT_EQ(transport_metrics.uri_to_metrics_map().size(), 5);
  RedfishMetrics transport_metrics_2;
  std::vector<DelliciusQueryResult> response_entries_2 =
      query_engine.ExecuteQueryWithAggregatedMetrics({"SensorCollector"},
                                                     &transport_metrics_2);
  // Run another query to make sure the metrics are cleared per query
  EXPECT_EQ(transport_metrics_2.uri_to_metrics_map().size(), 17);
  // Double checking first metric set to make sure it was not overwritten.
  EXPECT_EQ(transport_metrics.uri_to_metrics_map().size(), 5);
}

TEST(QueryEngineTest, QueryEngineTransportMetricsInResult) {
  std::string assembly_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/assembly_out_with_metrics.textproto"));
  std::string sensors_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/sensor_out_with_metrics.textproto"));

  FakeQueryEngineEnvironment fake_engine_env(
      {.flags{.enable_devpath_extension = false,
              .enable_transport_metrics = true},
       .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()},
       .query_rules{kQueryRules.begin(), kQueryRules.end()}},
      kIndusMockup, clock_time,
      FakeQueryEngineEnvironment::CachingMode::kNoExpiration);
  QueryEngine &query_engine = fake_engine_env.GetEngine();

  // Validate first query result with metrics.
  std::vector<DelliciusQueryResult> response_entries =
      query_engine.ExecuteQuery(
          {"AssemblyCollectorWithPropertyNameNormalization"});
  DelliciusQueryResult intent_output_assembly =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(assembly_out_path);
  VerifyQueryResults(response_entries, {intent_output_assembly});
  // Validate second query result with metrics.
  std::vector<DelliciusQueryResult> response_entries_2 =
      query_engine.ExecuteQuery({"SensorCollector"});
  DelliciusQueryResult intent_output_sensors =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(sensors_out_path);
  VerifyQueryResults(std::move(response_entries_2),
                     {std::move(intent_output_sensors)});
  // Ensure metrics from first response are not overwritten.
  VerifyQueryResults(response_entries, {intent_output_assembly});
}

TEST(QueryEngineTest, QueryEngineWithDefaultNormalizer) {
  FakeRedfishServer server(kIndusMockup);
  FakeClock clock{clock_time};
  absl::StatusOr<QueryEngine> query_engine =
      GetDefaultQueryEngine(server, kDelliciusQueries, &clock);
  EXPECT_TRUE(query_engine.ok());

  DelliciusQueryResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(
          GetTestDataDependencyPath(JoinFilePaths(
              kQuerySamplesLocation, "query_out/sensor_out.textproto")));
  std::vector<DelliciusQueryResult> response_entries =
      query_engine->ExecuteQuery({"SensorCollector"});
  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_output_sensor)});
}

TEST(QueryEngineTest, TestQueryEngineFactoryForParserError) {
  FakeRedfishServer server(kIndusMockup);
  EXPECT_EQ(GetDefaultQueryEngine(server, {{"Test", "{}"}}).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(QueryEngineTest, TestQueryEngineFactoryForInvalidQuery) {
  FakeRedfishServer server(kIndusMockup);
  EXPECT_EQ(GetDefaultQueryEngine(server, {{"Test", ""}}).status().code(),
            absl::StatusCode::kInternal);
}

TEST(QueryEngineTest, QueryEngineWithTranslation) {
  std::string assembly_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/assembly_out_translated.textproto"));

  FakeQueryEngineEnvironment fake_engine_env(
      {.flags{.enable_devpath_extension = false,
              .enable_transport_metrics = false},
       .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()},
       .query_rules{kQueryRules.begin(), kQueryRules.end()}},
      kIndusMockup, clock_time,
      FakeQueryEngineEnvironment::CachingMode::kNoExpiration);
  QueryEngine &query_engine = fake_engine_env.GetEngine();

  // Validate first query result with metrics.
  std::vector<QueryResult> response_entries = query_engine.ExecuteRedpathQuery(
      {"AssemblyCollectorWithPropertyNameNormalization"});
  QueryResult intent_output_assembly =
      ParseTextFileAsProtoOrDie<QueryResult>(assembly_out_path);
  VerifyQueryResults(response_entries, {intent_output_assembly});
}

TEST(QueryEngineTest, QueryEngineWithTranslationAndLocalDevpath) {
  std::string assembly_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation,
                    "query_out/devpath_sensor_out_translated.textproto"));

  FakeQueryEngineEnvironment fake_engine_env(
      {.flags{.enable_devpath_extension = true,
              .enable_transport_metrics = false},
       .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()},
       .query_rules{kQueryRules.begin(), kQueryRules.end()}},
      kIndusMockup, clock_time,
      FakeQueryEngineEnvironment::CachingMode::kNoExpiration);
  QueryEngine &query_engine = fake_engine_env.GetEngine();

  // Validate first query result with metrics.
  std::vector<QueryResult> response_entries =
      query_engine.ExecuteRedpathQuery({"SensorCollector"});
  QueryResult intent_output_assembly =
      ParseTextFileAsProtoOrDie<QueryResult>(assembly_out_path);
  VerifyQueryResults(response_entries, {intent_output_assembly});
}
}  // namespace

}  // namespace ecclesia
