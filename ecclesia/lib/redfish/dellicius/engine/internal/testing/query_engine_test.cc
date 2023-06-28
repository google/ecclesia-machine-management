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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/testing/test_queries_embedded.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/testing/test_query_rules_embedded.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine_fake.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/transport/http.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/time/clock_fake.h"

namespace ecclesia {

namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedPointwise;

constexpr absl::string_view kQuerySamplesLocation =
    "lib/redfish/dellicius/query/samples";
constexpr absl::string_view kIndusMockup = "indus_hmb_shim/mockup.shar";
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
  EXPECT_THAT(response_entries, ElementsAre(IgnoringRepeatedFieldOrdering(
                                    EqualsProto(intent_output_sensor))));
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
  EXPECT_THAT(response_entries, ElementsAre(IgnoringRepeatedFieldOrdering(
                                    EqualsProto(intent_output_sensor))));
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
  EXPECT_THAT(response_entries, ElementsAre(IgnoringRepeatedFieldOrdering(
                                    EqualsProto(intent_output_sensor))));

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
  EXPECT_THAT(
      response_entries,
      ElementsAre(
          IgnoringRepeatedFieldOrdering(EqualsProto(intent_sensor_out)),
          IgnoringRepeatedFieldOrdering(EqualsProto(intent_assembly_out))));
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
  EXPECT_THAT(response_entries, ElementsAre(IgnoringRepeatedFieldOrdering(
                                    EqualsProto(intent_assembly_out))));
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
        query_engine.ExecuteQueryWithMetrics(
            {"AssemblyCollectorWithPropertyNameNormalization"}, &metrics);
    DelliciusQueryResult intent_output_assembly =
        ParseTextFileAsProtoOrDie<DelliciusQueryResult>(assembly_out_path);
    EXPECT_THAT(response_entries, ElementsAre(IgnoringRepeatedFieldOrdering(
                                      EqualsProto(intent_output_assembly))));
  }
  {
    // Query assemblies again. This time we expect QueryEngine uses cache.
    std::vector<DelliciusQueryResult> response_entries =
        query_engine.ExecuteQuery(
            {"AssemblyCollectorWithPropertyNameNormalization"});
    DelliciusQueryResult intent_output_assembly =
        ParseTextFileAsProtoOrDie<DelliciusQueryResult>(assembly_out_path);
    EXPECT_THAT(response_entries, ElementsAre(IgnoringRepeatedFieldOrdering(
                                      EqualsProto(intent_output_assembly))));
  }
  {
    // Query assemblies again and again we expect QueryEngine to use cache.
    std::vector<DelliciusQueryResult> response_entries =
        query_engine.ExecuteQuery(
            {"AssemblyCollectorWithPropertyNameNormalization"});
    DelliciusQueryResult intent_output_assembly =
        ParseTextFileAsProtoOrDie<DelliciusQueryResult>(assembly_out_path);
    EXPECT_THAT(response_entries, ElementsAre(IgnoringRepeatedFieldOrdering(
                                      EqualsProto(intent_output_assembly))));
  }

  bool traced_systems = false;
  bool traced_processor_collection = false;
  bool traced_processors = false;
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
    if (uri_x_metric.first == "/redfish/v1/Systems/system/Processors") {
      traced_processor_collection = true;
      for (const auto &metadata :
           uri_x_metric.second.request_type_to_metadata()) {
        EXPECT_EQ(metadata.second.request_count(), 3);
      }
    }

    if (uri_x_metric.first == "/redfish/v1/Systems/system/Processors/0") {
      traced_processors = true;
      for (const auto &metadata :
           uri_x_metric.second.request_type_to_metadata()) {
        EXPECT_EQ(metadata.second.request_count(), 3);
      }
    }
  }
  EXPECT_TRUE(traced_systems);
  EXPECT_TRUE(traced_processor_collection);
  EXPECT_TRUE(traced_processors);
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
  EXPECT_THAT(response_entries, ElementsAre(IgnoringRepeatedFieldOrdering(
                                    EqualsProto(intent_query_out))));
}

TEST(QueryEngineTest, QueryEngineTransportMetrics) {
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
      query_engine.ExecuteQueryWithMetrics(
          {"AssemblyCollectorWithPropertyNameNormalization"},
          &transport_metrics);
  EXPECT_EQ(transport_metrics.uri_to_metrics_map().size(), 8);
  RedfishMetrics transport_metrics_2;
  std::vector<DelliciusQueryResult> response_entries_2 =
      query_engine.ExecuteQueryWithMetrics({"SensorCollector"},
                                           &transport_metrics_2);
  // Run another query to make sure the metrics are cleared per query
  EXPECT_EQ(transport_metrics_2.uri_to_metrics_map().size(), 15);
  // Double checking first metric set to make sure it was not overwritten.
  EXPECT_EQ(transport_metrics.uri_to_metrics_map().size(), 8);
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
  return CreateQueryEngine({.query_files = query_files,
                            .transport = std::move(transport),
                            .clock = clock});
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
  EXPECT_THAT(response_entries, ElementsAre(IgnoringRepeatedFieldOrdering(
                                    EqualsProto(intent_output_sensor))));
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

}  // namespace

}  // namespace ecclesia
