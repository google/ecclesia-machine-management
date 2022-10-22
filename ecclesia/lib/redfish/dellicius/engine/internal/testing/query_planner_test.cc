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
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/protobuf/parse.h"
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

TEST(QueryPlannerTest, BasicDelliciusInterpreter) {
  std::string assembly_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/assembly_in.textproto"));
  std::string sensor_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/sensor_in.textproto"));
  std::string assembly_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/assembly_out.textproto"));
  std::string sensor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_out/sensor_out.textproto"));
  FakeClock clock(absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer.
  DefaultNormalizer default_normalizer;
  // Set up context node for dellicius query.
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  auto service_root = intf->GetRoot();

  // Query Assembly
  DelliciusQueryResult result_assembly;
  DelliciusQuery query_assembly =
      ParseTextFileAsProtoOrDie<DelliciusQuery>(assembly_in_path);
  QueryPlanner qp(query_assembly, &default_normalizer);
  qp.Run(service_root, clock, result_assembly);
  DelliciusQueryResult intent_output_assembly =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(assembly_out_path);
  EXPECT_THAT(intent_output_assembly,
              IgnoringRepeatedFieldOrdering(EqualsProto(result_assembly)));

  // Query Sensor
  DelliciusQueryResult result_sensor;
  DelliciusQuery query_sensors =
      ParseTextFileAsProtoOrDie<DelliciusQuery>(sensor_in_path);
  QueryPlanner qps(query_sensors, &default_normalizer);
  qps.Run(service_root, clock, result_sensor);
  DelliciusQueryResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(sensor_out_path);
  EXPECT_THAT(intent_output_sensor,
              IgnoringRepeatedFieldOrdering(EqualsProto(result_sensor)));
}

TEST(QueryPlannerTest, DefaultNormalizerWithDevpaths) {
  std::string sensor_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/sensor_in.textproto"));
  std::string sensor_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/devpath_sensor_out.textproto"));
  FakeClock clock(absl::FromUnixSeconds(10));
  // Set up context node for dellicius query.
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  auto service_root = intf->GetRoot();

  // Instantiate a passthrough normalizer with devpath extension.
  NormalizerDevpathDecorator normalizer_devpath_decorator(
      std::make_unique<DefaultNormalizer>(), intf.get());
  // Query Sensor
  DelliciusQueryResult result_sensor;
  DelliciusQuery query_sensor =
      ParseTextFileAsProtoOrDie<DelliciusQuery>(sensor_in_path);
  QueryPlanner qps(query_sensor, &normalizer_devpath_decorator);
  qps.Run(service_root, clock, result_sensor);
  DelliciusQueryResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<DelliciusQueryResult>(sensor_out_path);
  EXPECT_THAT(intent_output_sensor,
              IgnoringRepeatedFieldOrdering(EqualsProto(result_sensor)));
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
  DefaultNormalizer default_normalizer;
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
    QueryPlanner qps(query_sensor, &default_normalizer);
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
  DefaultNormalizer default_normalizer;
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
    QueryPlanner qps(query_sensor, &default_normalizer);
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
