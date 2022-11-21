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

#include "ecclesia/lib/redfish/dellicius/engine/internal/query_planner.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/factory.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/normalizer.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/metrical_transport.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/time/clock_fake.h"

namespace ecclesia {

namespace {

constexpr absl::string_view kQuerySamplesLocation =
    "lib/redfish/dellicius/query/samples";

class QueryPlannerTestRunner : public ::testing::Test {
 protected:
  QueryPlannerTestRunner() = default;
  void SetTestParams(absl::string_view mockup, absl::Time duration) {
    server_ = std::make_unique<FakeRedfishServer>(mockup);
    intf_ = server_->RedfishClientInterface();
    clock_ = std::make_unique<FakeClock>(duration);
  }

  void TestQuery(const std::string &query_in_path,
                 const std::string &query_out_path, Normalizer *normalizer) {
    CHECK(server_ != nullptr && intf_ != nullptr && clock_ != nullptr)
        << "Test parameters not set!";
    DelliciusQueryResult query_result;
    DelliciusQuery query =
        ParseTextFileAsProtoOrDie<DelliciusQuery>(query_in_path);
    QueryPlanner qp(query, normalizer);
    qp.Run(intf_->GetRoot(), *clock_, query_result);
    DelliciusQueryResult intent_output =
        ParseTextFileAsProtoOrDie<DelliciusQueryResult>(query_out_path);
    EXPECT_THAT(intent_output,
                IgnoringRepeatedFieldOrdering(EqualsProto(query_result)));
  }

  std::unique_ptr<FakeRedfishServer> server_;
  std::unique_ptr<FakeClock> clock_;
  std::unique_ptr<RedfishInterface> intf_;
};

TEST_F(QueryPlannerTestRunner, CheckPredicatesFilterNodesAsExpected) {
  std::string sensor_in_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_in/sensor_in_predicates.textproto"));
  std::string sensor_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/sensor_out_predicates.textproto"));
  SetTestParams("indus_hmb_shim/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer.
  auto default_normalizer = BuildDefaultNormalizer();
  TestQuery(sensor_in_path, sensor_out_path, default_normalizer.get());
}

TEST_F(QueryPlannerTestRunner, CheckPredicatesFilterAncestorNodesAsExpected) {
  std::string processor_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/processors_in.textproto"));
  std::string processor_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/processors_out.textproto"));
  SetTestParams("indus_hmb_cn/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer.
  auto default_normalizer = BuildDefaultNormalizer();
  TestQuery(processor_in_path, processor_out_path, default_normalizer.get());
}

TEST_F(QueryPlannerTestRunner, BasicDelliciusInterpreter) {
  std::string assembly_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/assembly_in.textproto"));
  std::string sensor_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/sensor_in.textproto"));
  std::string assembly_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/assembly_out.textproto"));
  std::string sensor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/sensor_out.textproto"));
  SetTestParams("indus_hmb_shim/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer.
  auto default_normalizer = BuildDefaultNormalizer();
  // Query Assembly
  TestQuery(assembly_in_path, assembly_out_path, default_normalizer.get());
  // Query Sensor
  TestQuery(sensor_in_path, sensor_out_path, default_normalizer.get());
}

TEST_F(QueryPlannerTestRunner, DefaultNormalizerWithDevpaths) {
  std::string sensor_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/sensor_in.textproto"));
  std::string sensor_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/devpath_sensor_out.textproto"));
  SetTestParams("indus_hmb_shim/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer with devpath extension.
  auto normalizer_with_devpath = BuildDefaultNormalizerWithDevpath(intf_.get());
  // Query Sensor
  TestQuery(sensor_in_path, sensor_out_path, normalizer_with_devpath.get());
}

TEST(QueryPlannerTest, CheckQueryPlannerSendsOneRequestForEachUri) {
  std::string sensor_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/sensor_in.textproto"));
  std::string sensor_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/devpath_sensor_out.textproto"));
  FakeClock clock(absl::FromUnixSeconds(10));
  // Set up context node for dellicius query.
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  // Instantiate a passthrough normalizer.
  auto default_normalizer = BuildDefaultNormalizer();
  RedfishMetrics metrics;
  {
    std::unique_ptr<RedfishTransport> base_transport =
        server.RedfishClientTransport();
    auto transport = std::make_unique<MetricalRedfishTransport>(
        std::move(base_transport), Clock::RealClock(), metrics);

    auto cache = std::make_unique<NullCache>(transport.get());
    auto intf = NewHttpInterface(std::move(transport), std::move(cache),
                                 RedfishInterface::kTrusted);
    auto service_root = intf->GetRoot();

    // Query Sensor
    DelliciusQueryResult result_sensor;
    DelliciusQuery query_sensor =
        ParseTextFileAsProtoOrDie<DelliciusQuery>(sensor_in_path);
    QueryPlanner qps(query_sensor, default_normalizer.get());
    qps.Run(service_root, clock, result_sensor);
  }
  // For each type of redfish request for each URI, validate that the
  // QueryPlanner sends only 1 request.
  for (const auto &uri_x_metric : *metrics.mutable_uri_to_metrics_map()) {
    for (const auto &metadata :
         uri_x_metric.second.request_type_to_metadata()) {
      EXPECT_EQ(metadata.second.request_count(), 1);
    }
  }
}

TEST(QueryPlannerTest, CheckQueryPlannerStopsQueryingOnTransportError) {
  std::string sensor_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/sensor_in.textproto"));
  std::string sensor_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/devpath_sensor_out.textproto"));
  FakeClock clock(absl::FromUnixSeconds(10));
  // Set up context node for dellicius query.
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  // Instantiate a passthrough normalizer.
  auto default_normalizer = BuildDefaultNormalizer();
  RedfishMetrics metrics;
  {
    std::unique_ptr<RedfishTransport> base_transport =
        std::make_unique<NullTransport>();
    auto transport = std::make_unique<MetricalRedfishTransport>(
        std::move(base_transport), Clock::RealClock(), metrics);

    auto cache = std::make_unique<NullCache>(transport.get());
    auto intf = NewHttpInterface(std::move(transport), std::move(cache),
                                 RedfishInterface::kTrusted);
    auto service_root = intf->GetRoot();

    // Query Sensor
    DelliciusQueryResult result_sensor;
    DelliciusQuery query_sensor =
        ParseTextFileAsProtoOrDie<DelliciusQuery>(sensor_in_path);
    QueryPlanner qps(query_sensor, default_normalizer.get());
    qps.Run(service_root, clock, result_sensor);
  }
  // Validate that no attempt was made by query planner to query redfish service
  // Redfish Metrics should indicate 1 failed GET request to service root which
  // is sent before running the query planner.
  EXPECT_EQ(metrics.uri_to_metrics_map().size(), 1);
  EXPECT_TRUE(metrics.uri_to_metrics_map().contains("/redfish/v1"));
  EXPECT_EQ(metrics.uri_to_metrics_map()
                .at("/redfish/v1")
                .request_type_to_metadata_failures_size(),
            1);
  EXPECT_TRUE(metrics.uri_to_metrics_map()
                  .at("/redfish/v1")
                  .request_type_to_metadata_failures()
                  .contains("GET"));
  EXPECT_EQ(metrics.uri_to_metrics_map()
                .at("/redfish/v1")
                .request_type_to_metadata_size(),
            0);
}

}  // namespace

}  // namespace ecclesia
