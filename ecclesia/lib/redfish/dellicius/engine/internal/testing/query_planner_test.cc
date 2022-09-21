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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/normalizer.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/query_planner.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/time/clock_fake.h"

namespace ecclesia {

namespace {

constexpr std::string_view kQueryTestRootDir
    = "lib/redfish/dellicius/engine/internal/testing";

TEST(QueryPlannerTest, BasicDelliciusInterpreter) {
  std::string assembly_in_path = ecclesia::GetTestDataDependencyPath(
          ecclesia::JoinFilePaths(kQueryTestRootDir,
                                  "query_in/assembly_in.textproto"));
  std::string sensor_in_path = ecclesia::GetTestDataDependencyPath(
          ecclesia::JoinFilePaths(kQueryTestRootDir,
                                  "query_in/sensor_in.textproto"));
  std::string assembly_out_path = ecclesia::GetTestDataDependencyPath(
          ecclesia::JoinFilePaths(kQueryTestRootDir,
                                  "query_out/assembly_out.textproto"));
  std::string sensor_out_path = ecclesia::GetTestDataDependencyPath(
          ecclesia::JoinFilePaths(kQueryTestRootDir,
                                  "query_out/sensor_out.textproto"));
  FakeClock clock(absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer.
  std::function<
      void(const RedfishVariant &var,
           const DelliciusQuery::Subquery &subquery,
           DelliciusQueryResult &output)> nn = DefaultNormalizer();
  // Set up context node for dellicius query.
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  auto service_root = intf->GetRoot();

  // Query Assembly
  DelliciusQueryResult result_assembly;
  DelliciusQuery query_assembly = ParseTextFileAsProtoOrDie<
      DelliciusQuery>(assembly_in_path);
  auto qp = std::make_unique<QueryPlanner>(query_assembly, nn);
  qp->Run(service_root, clock, result_assembly);
  DelliciusQueryResult intent_output_assembly = ParseTextFileAsProtoOrDie<
      DelliciusQueryResult>(assembly_out_path);
  EXPECT_THAT(intent_output_assembly, IgnoringRepeatedFieldOrdering(
      EqualsProto(result_assembly)));

  // Query Sensor
  DelliciusQueryResult result_sensor;
  DelliciusQuery query_sensors = ParseTextFileAsProtoOrDie<
      DelliciusQuery>(sensor_in_path);
  auto qps = std::make_unique<QueryPlanner>(query_sensors, nn);
  qps->Run(service_root, clock, result_sensor);
  DelliciusQueryResult intent_output_sensor = ParseTextFileAsProtoOrDie<
      DelliciusQueryResult>(sensor_out_path);
  EXPECT_THAT(intent_output_sensor, IgnoringRepeatedFieldOrdering(
      EqualsProto(result_sensor)));
}

}  // namespace

}  // namespace ecclesia
