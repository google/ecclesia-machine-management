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

#include "ecclesia/lib/redfish/dellicius/tools/simplitune.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/factory.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/time/clock_fake.h"

namespace ecclesia {

namespace {

constexpr absl::string_view kQuerySamplesLocation =
    "lib/redfish/dellicius/query/samples";
constexpr absl::string_view kIntentGeneratedConfigsLocation =
    "lib/redfish/dellicius/tools";
constexpr absl::string_view kIntentGeneratedConfigsFile =
    "sample_generated_configs.textproto";

class SimpliTuneTestRunner : public ::testing::Test {
 protected:
  SimpliTuneTestRunner() = default;
  void SetTestParams(absl::string_view mockup, absl::Time duration) {
    server_ = std::make_unique<FakeRedfishServer>(mockup);
    intf_ = server_->RedfishClientInterface();
    clock_ = std::make_unique<FakeClock>(duration);
  }

  void TestQuery(const std::string &query_in_path, Normalizer *normalizer) {
    CHECK(server_ != nullptr && intf_ != nullptr && clock_ != nullptr)
        << "Test parameters not set!";
    DelliciusQuery query =
        ParseTextFileAsProtoOrDie<DelliciusQuery>(query_in_path);
    auto qp = BuildQueryPlanner(query, RedPathRedfishQueryParams{}, normalizer);
    ASSERT_TRUE(qp.ok());
    QueryTracker tracker;
    absl::StatusOr<DelliciusQueryResult> query_result =
        qp->Run(intf_->GetRoot(), *clock_, &tracker);
    EXPECT_TRUE(query_result.ok());
    auto configs = GenerateExpandConfigurations(tracker);
    RedPathPrefixSetWithQueryParamsCollection generated_configs;
    for (const auto &config : configs) {
      QueryRules::RedPathPrefixSetWithQueryParams prefix_set_with_query_params =
          GetQueryRuleProtoFromConfig(config);
      generated_configs.mutable_redpath_prefix_set_with_params()->Add(
          std::move(prefix_set_with_query_params));
    }
    std::string intent_output_path = GetTestDataDependencyPath(JoinFilePaths(
        kIntentGeneratedConfigsLocation, kIntentGeneratedConfigsFile));
    auto intent_output =
        ParseTextFileAsProtoOrDie<RedPathPrefixSetWithQueryParamsCollection>(
            intent_output_path);
    EXPECT_THAT(intent_output,
                IgnoringRepeatedFieldOrdering(EqualsProto(generated_configs)));
  }

  std::unique_ptr<FakeRedfishServer> server_;
  std::unique_ptr<FakeClock> clock_;
  std::unique_ptr<RedfishInterface> intf_;
};

TEST_F(SimpliTuneTestRunner, CheckTuningEngineGeneratesExpectedConfigurations) {
  std::string assembly_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/assembly_in.textproto"));
  SetTestParams("indus_hmb_shim/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer.
  auto default_normalizer = BuildDefaultNormalizer();
  TestQuery(assembly_in_path, default_normalizer.get());
}

}  // namespace

}  // namespace ecclesia
