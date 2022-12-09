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
#include <fstream>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/factory.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_rules.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/tools/simplitune.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/lib/time/proto.h"

ABSL_FLAG(std::string, hostname, "localhost",
          "Hostname of the Redfish server.");
ABSL_FLAG(int, port, 8000, "Port number of the server.");
ABSL_FLAG(std::string, query_path, "", "Absolute path of the query to tune.");
ABSL_FLAG(std::string, expand_config_path, "",
          "Absolute path to the output directory to place the tuned config.");

namespace ecclesia {

namespace {
// Identifier for the output metrics file.
constexpr absl::string_view kQueryParamsOutId = "tuned_query_params.textproto";
constexpr absl::string_view kUsage =
    "Simplitune: Tool for tuning Redpath Queries for performance optimization. "
    "\n\nThe tool requires the following at minimum:\n  "
    "1. An absolute path to the Dellicius Query that needs to be tuned for "
    "performance.\n "
    "2. An absolute path to the Dellicius Query Output that will be used to "
    "validate if the query works with tuned dispatch config. \n "
    "3. An absolute path to output director for placing the tuned "
    "configuration. \n "
    "Example:\n"
    "blaze run ecclesia/lib/redfish/dellicius/tools:simplitune_main"
    " -- --query_path="
    "/usr/local/google/home/foo/repos/query_in/sensor_in.textproto "
    " --expand_config_path="
    "/usr/local/google/home/foo/repos/config_out "
    " --hostname='localhost' --port=8000\n";

int SimpliTuneMain(int argc, char **argv) {
  absl::SetProgramUsageMessage(kUsage);
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();

  // Configure HTTP transport.
  auto curl_http_client = std::make_unique<CurlHttpClient>(
      LibCurlProxy::CreateInstance(), HttpCredential());
  std::unique_ptr<RedfishTransport> transport =
      HttpRedfishTransport::MakeNetwork(
          std::move(curl_http_client),
          absl::StrCat(absl::GetFlag(FLAGS_hostname), ":",
                       absl::GetFlag(FLAGS_port)));

  std::unique_ptr<NullCache> cache =
      std::make_unique<NullCache>(transport.get());
  std::unique_ptr<RedfishInterface> intf = NewHttpInterface(
      std::move(transport), std::move(cache), RedfishInterface::kTrusted);
  RedfishVariant root = intf->GetRoot();
  if (root.AsObject() == nullptr) {
    LOG(ERROR) << "Error connecting to redfish service. "
               << "Check host configuration";
    return -1;
  }

  // Tracker for tracing Redpaths executed by query planner in a single query
  // operation.
  QueryTracker tracker;
  // Instantiate a passthrough normalizer.
  auto default_normalizer = BuildDefaultNormalizer();
  const Clock *clock = Clock::RealClock();
  DelliciusQuery query = ParseTextFileAsProtoOrDie<DelliciusQuery>(
      absl::GetFlag(FLAGS_query_path));
  absl::StatusOr<std::unique_ptr<QueryPlannerInterface>> qp = BuildQueryPlanner(
      query, RedPathRedfishQueryParams{}, default_normalizer.get());
  if (!qp.ok()) {
    LOG(ERROR) << qp.status();
    return -1;
  }

  DelliciusQueryResult query_result = (*qp)->Run(root, *clock, &tracker);
  LOG(INFO) << query_result.DebugString();
  // Latency with no expand
  double min_latency = absl::ToDoubleMilliseconds(
      AbslTimeFromProtoTime(query_result.end_timestamp()) -
      AbslTimeFromProtoTime(query_result.start_timestamp()));
  size_t config_count = 0;
  LOG(INFO) << "Config #" << config_count << " No Expand Response time (ms) "
            << min_latency;

  std::vector<RedPathRedfishQueryParams> configs =
      GenerateExpandConfigurations(tracker);
  RedPathRedfishQueryParams tuned_query_params;
  size_t total_configs = configs.size();
  LOG(INFO) << "Total configs generated: " << total_configs;
  for (auto &&config : configs) {
    LOG(INFO) << "Config #[" << ++config_count << "/" << total_configs << "]";
    bool is_config_valid = true;
    for (const auto &[redpath, query_param] : config) {
      if (!query_param.expand.has_value()) {
        is_config_valid = false;
        break;
      }
      LOG(INFO) << redpath
                << " Query Parameters: " << query_param.expand->ToString();
    }
    if (!is_config_valid || config.empty()) {
      std::cout << "Invalid config!";
      continue;
    }

    // Build QueryPlanner for the given Redfish query parameter configuration.
    qp = BuildQueryPlanner(query, config, default_normalizer.get());
    if (!qp.ok()) {
      LOG(ERROR) << qp.status();
      return -1;
    }
    query_result = (*qp)->Run(root, *clock, nullptr);
    double response_time_ms = absl::ToDoubleMilliseconds(
        AbslTimeFromProtoTime(query_result.end_timestamp()) -
        AbslTimeFromProtoTime(query_result.start_timestamp()));
    LOG(INFO) << "Response time (ms) " << response_time_ms;
    LOG(INFO) << "\n\n";
    // Mark current configuration as golden if it has lowest latency.
    if (response_time_ms < min_latency) {
      min_latency = response_time_ms;
      tuned_query_params = config;
    }
  }

  QueryRules::RedPathPrefixSetWithQueryParams prefix_set_with_params =
      GetQueryRuleProtoFromConfig(tuned_query_params);

  std::string output_directory(absl::GetFlag(FLAGS_expand_config_path));
  std::ofstream params_file_stream(
      JoinFilePaths(output_directory, kQueryParamsOutId), std::ofstream::out);
  if (params_file_stream.bad() || !params_file_stream.is_open()) {
    LOG(ERROR)
        << "Error opening output file for storing tuned query parameters";
    return -1;
  }
  if (prefix_set_with_params.redpath_prefix_with_params().empty()) {
    std::cout << "RedPath query has the best performance without any Redfish "
                 "Query Parameters."
              << std::endl;
    return 0;
  }
  std::cout << "Tuned Dispatch Rules for the redpath query: " << std::endl;
  for (const auto &rule : prefix_set_with_params.redpath_prefix_with_params()) {
    std::cout
        << rule.redpath()
        << " Expand Level: " << rule.expand_configuration().level()
        << " Expand Type {0: Undefined 1: Not links 2: Only links 3: Both}: "
        << rule.expand_configuration().type() << std::endl;
    params_file_stream << std::setw(2) << rule.DebugString() << std::endl;
  }
  std::cout << " Response Time (ms) " << min_latency << std::endl;
  return 0;
}

}  // namespace

}  // namespace ecclesia

int main(int argc, char **argv) { return ecclesia::SimpliTuneMain(argc, argv); }
