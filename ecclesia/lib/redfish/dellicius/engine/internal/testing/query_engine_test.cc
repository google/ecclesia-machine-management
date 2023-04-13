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

#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"

#include <memory>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/engine/config.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/testing/test_queries_embedded.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/testing/test_query_rules_embedded.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/metrical_transport.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/lib/time/clock_fake.h"

namespace ecclesia {

namespace {

using ::testing::UnorderedPointwise;

constexpr absl::string_view kQuerySamplesLocation =
    "lib/redfish/dellicius/query/samples";

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

class QueryEngineTest : public ::testing::Test {
 protected:
  QueryEngineTest()
      : server_("indus_hmb_shim/mockup.shar"),
        clock_(absl::FromUnixSeconds(10)),
        intf_(server_.RedfishClientInterface()) {}

  void SetUp() override {
    FakeRedfishServer::Config config = server_.GetConfig();
    HttpCredential creds;
    auto curl_http_client =
        std::make_unique<CurlHttpClient>(LibCurlProxy::CreateInstance(), creds);
    std::string network_endpoint =
        absl::StrFormat("%s:%d", config.hostname, config.port);
    transport_ = HttpRedfishTransport::MakeNetwork(std::move(curl_http_client),
                                                   network_endpoint);

    cache_factory_ = [](RedfishTransport *transport) {
      return std::make_unique<ecclesia::NullCache>(transport);
    };
  }

  FakeRedfishServer server_;
  FakeClock clock_;
  std::unique_ptr<RedfishInterface> intf_;
  std::unique_ptr<RedfishTransport> transport_;
  RedfishTransportCacheFactory cache_factory_;
};

TEST_F(QueryEngineTest, QueryEngineDevpathConfiguration) {
  std::string sensor_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/devpath_sensor_out.textproto"));

  QueryEngineConfiguration config{
      .flags{.enable_devpath_extension = true},
      .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}};
  QueryEngine query_engine(config, std::move(transport_), cache_factory_,
                           &clock_);
  std::vector<DelliciusQueryResult> response_entries =
      query_engine.ExecuteQuery({"SensorCollector"});

  DelliciusQueryResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(sensor_out_path);
  EXPECT_EQ(response_entries.size(), 1);
  EXPECT_THAT(intent_output_sensor,
              IgnoringRepeatedFieldOrdering(EqualsProto(response_entries[0])));
}

TEST_F(QueryEngineTest, QueryEngineDefaultConfiguration) {
  std::string sensor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/sensor_out.textproto"));

  QueryEngineConfiguration config{
      .flags{.enable_devpath_extension = false},
      .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}};
  QueryEngine query_engine(config, std::move(transport_), cache_factory_,
                           &clock_);
  std::vector<DelliciusQueryResult> response_entries =
      query_engine.ExecuteQuery({"SensorCollector"});

  DelliciusQueryResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(sensor_out_path);
  EXPECT_EQ(response_entries.size(), 1);
  EXPECT_THAT(intent_output_sensor,
              IgnoringRepeatedFieldOrdering(EqualsProto(response_entries[0])));
}

TEST_F(QueryEngineTest, QueryEngineWithExpandConfiguration) {
  std::string sensor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/assembly_out.textproto"));

  QueryEngineConfiguration config{
      .flags{.enable_devpath_extension = false},
      .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()},
      .query_rules{kQueryRules.begin(), kQueryRules.end()}};

  QueryEngine query_engine(config, std::move(transport_), cache_factory_,
                           &clock_);
  QueryTracker query_tracker;
  std::vector<DelliciusQueryResult> response_entries =
      query_engine.ExecuteQuery(
          {"AssemblyCollectorWithPropertyNameNormalization"}, query_tracker);
  DelliciusQueryResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(sensor_out_path);
  EXPECT_EQ(response_entries.size(), 1);
  EXPECT_THAT(intent_output_sensor,
              IgnoringRepeatedFieldOrdering(EqualsProto(response_entries[0])));

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

TEST_F(QueryEngineTest, QueryEngineInvalidQueries) {
  QueryEngineConfiguration config{
      .flags{.enable_devpath_extension = false},
      .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}};

  // Invalid Query Id
  QueryEngine query_engine(config, std::move(transport_), cache_factory_,
                           &clock_);
  std::vector<DelliciusQueryResult> response_entries =
      query_engine.ExecuteQuery({"ThereIsNoSuchId"});
  EXPECT_EQ(response_entries.size(), 0);
}

TEST_F(QueryEngineTest, QueryEngineConcurrentQueries) {
  std::string assembly_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/assembly_out.textproto"));
  std::string sensor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/sensor_out.textproto"));

  QueryEngineConfiguration config{
      .flags{.enable_devpath_extension = false},
      .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}};
  QueryEngine query_engine(config, std::move(transport_), cache_factory_,
                           &clock_);
  std::vector<DelliciusQueryResult> response_entries =
      query_engine.ExecuteQuery(
          {"SensorCollector",
           "AssemblyCollectorWithPropertyNameNormalization"});
  DelliciusQueryResult intent_sensor_out =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(sensor_out_path);
  DelliciusQueryResult intent_assembly_out =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(assembly_out_path);
  EXPECT_EQ(response_entries.size(), 2);
  EXPECT_THAT(intent_sensor_out,
              IgnoringRepeatedFieldOrdering(EqualsProto(response_entries[0])));
  EXPECT_THAT(intent_assembly_out,
              IgnoringRepeatedFieldOrdering(EqualsProto(response_entries[1])));
}

TEST_F(QueryEngineTest, QueryEngineEmptyItemDevpath) {
  std::string assembly_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/devpath_assembly_out.textproto"));

  QueryEngineConfiguration config{
      .flags{.enable_devpath_extension = true},
      .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}};
  QueryEngine query_engine(config, std::move(transport_), cache_factory_,
                           &clock_);
  std::vector<DelliciusQueryResult> response_entries =
      query_engine.ExecuteQuery(
          {"AssemblyCollectorWithPropertyNameNormalization"});
  DelliciusQueryResult intent_assembly_out =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(assembly_out_path);
  EXPECT_EQ(response_entries.size(), 1);
  EXPECT_THAT(intent_assembly_out,
              IgnoringRepeatedFieldOrdering(EqualsProto(response_entries[0])));
}

TEST_F(QueryEngineTest, QueryEngineWithCacheConfiguration) {
  auto config = server_.GetConfig();
  HttpCredential creds;
  auto curl_http_client = std::make_unique<ecclesia::CurlHttpClient>(
      ecclesia::LibCurlProxy::CreateInstance(), creds);
  auto transport = ecclesia::HttpRedfishTransport::MakeNetwork(
      std::move(curl_http_client),
      absl::StrFormat("%s:%d", config.hostname, config.port));
  auto cache_factory = [this](RedfishTransport *transport) {
    return std::make_unique<ecclesia::TimeBasedCache>(transport, &clock_,
                                                      absl::InfiniteDuration());
  };

  std::string assembly_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/assembly_out.textproto"));

  QueryEngineConfiguration query_engine_configuration{
      .flags{.enable_devpath_extension = false},
      .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()},
      .query_rules{kQueryRules.begin(), kQueryRules.end()}};

  RedfishMetrics metrics;
  auto metrical_transport = std::make_unique<MetricalRedfishTransport>(
      std::move(transport), Clock::RealClock(), metrics);

  QueryEngine query_engine(query_engine_configuration,
                           std::move(metrical_transport), cache_factory,
                           &clock_);
  {
    // Query assemblies
    std::vector<DelliciusQueryResult> response_entries =
        query_engine.ExecuteQuery(
            {"AssemblyCollectorWithPropertyNameNormalization"});
    DelliciusQueryResult intent_output_assembly =
        ParseTextFileAsProtoOrDie<DelliciusQueryResult>(assembly_out_path);
    EXPECT_EQ(response_entries.size(), 1);
    EXPECT_THAT(intent_output_assembly, IgnoringRepeatedFieldOrdering(
                                            EqualsProto(response_entries[0])));
  }
  {
    // Query assemblies again. This time we expect QueryEngine uses cache
    std::vector<DelliciusQueryResult> response_entries =
        query_engine.ExecuteQuery(
            {"AssemblyCollectorWithPropertyNameNormalization"});
    DelliciusQueryResult intent_output_sensor =
        ParseTextFileAsProtoOrDie<DelliciusQueryResult>(assembly_out_path);
    EXPECT_EQ(response_entries.size(), 1);
    EXPECT_THAT(intent_output_sensor, IgnoringRepeatedFieldOrdering(
                                          EqualsProto(response_entries[0])));
  }
  {
    // Query assemblies again.
    std::vector<DelliciusQueryResult> response_entries =
        query_engine.ExecuteQuery(
            {"AssemblyCollectorWithPropertyNameNormalization"});
    DelliciusQueryResult intent_output_sensor =
        ParseTextFileAsProtoOrDie<DelliciusQueryResult>(assembly_out_path);
    EXPECT_EQ(response_entries.size(), 1);
    EXPECT_THAT(intent_output_sensor, IgnoringRepeatedFieldOrdering(
                                          EqualsProto(response_entries[0])));
  }
  // We requested Processors 3 times when we executed Assembly query 3 times.
  // Note we have specified in the query that processors should always be fresh.
  // So we should expect processors queried 3 times while Systems is queried
  // once just like all other cached resources.
  bool traced_processors = false;
  bool traced_systems = false;
  for (const auto &uri_x_metric : *metrics.mutable_uri_to_metrics_map()) {
    if (uri_x_metric.first == "/redfish/v1/Systems/system/Processors/0") {
      traced_processors = true;
      for (const auto &metadata :
           uri_x_metric.second.request_type_to_metadata()) {
        EXPECT_EQ(metadata.second.request_count(), 3);
      }
    }
    if (uri_x_metric.first == "/redfish/v1/Systems") {
      traced_systems = true;
      for (const auto &metadata :
           uri_x_metric.second.request_type_to_metadata()) {
        EXPECT_EQ(metadata.second.request_count(), 1);
      }
    }
  }
  EXPECT_TRUE(traced_processors);
  EXPECT_TRUE(traced_systems);
}

}  // namespace

}  // namespace ecclesia
