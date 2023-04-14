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
#include <iomanip>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/parse.h"
#include "absl/log/initialize.h"
#include "absl/container/fixed_array.h"
#include "absl/flags/flag.h"
#include "absl/flags/usage.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/file/dir.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/engine/config.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_rules.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/tools/query_flag.pb.h"
#include "ecclesia/lib/redfish/dellicius/tools/redfish_backend.h"
#include "ecclesia/lib/redfish/dellicius/tools/transport_cache.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/metrical_transport.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "ecclesia/lib/time/clock.h"

ABSL_RETIRED_FLAG(bool, devpath_enabled, false,
                  "Boolean to enable devpath extension.");
ABSL_RETIRED_FLAG(std::string, hostname, "localhost",
                  "Hostname of the Redfish server.");
ABSL_RETIRED_FLAG(int, port, 8000, "Port number of the server.");
ABSL_RETIRED_FLAG(
    std::string, transport, "http",
    "redfish transport can be either http, loas_grpc, or insecure_grpc");
ABSL_RETIRED_FLAG(
    std::string, input_dir, "",
    "Absolute path to directory containing textproto files for Queries.");
ABSL_RETIRED_FLAG(std::string, query_rule_location, "",
                  "Absolute path to textproto coontaining rules for Queries.");
ABSL_RETIRED_FLAG(std::vector<std::string>, query_ids, {},
                  "List of Identifiers for the Dellicius Queries to execute.");

ABSL_FLAG(
    std::string, flag_file, "",
    "Absolute path to the flag file containing the configuration of query_cli");
ABSL_FLAG(bool, metrics_enabled, false, "Boolean to enable redfish metrics.");
ABSL_FLAG(std::string, transport_metric_output_dir, "",
          "Absolute path to output directory to store transport metrics.");
ABSL_FLAG(size_t, iteration_count, 1,
          "Number of times the queries should run.");
ABSL_FLAG(ecclesia::CachePolicy, cache_policy, {},
          "Cache policy to use in the format <policy>:<policy_data>. \nAllowed "
          "arguments are \"no_cache\" or \"time_based:<duration_in_seconds>\"");

namespace ecclesia {

namespace {

constexpr absl::string_view kUsage =
    "A debug tool to Query a given Redfish Service using Dellicius "
    "Queries.\n\nThe tool requires the following at minimum:\n  "
    "1. An absolute path to the directory containing textproto files for all "
    "Dellicius Queries that DelliciusQueryEngine should be configured for.\n  "
    "2. A list of string identifiers corresponding to the DelliciusQuery files "
    "in the directory that the user expects to execute using the tool.\n"
    "Example:\n"
    "  blaze run ecclesia/lib/redfish/dellicius/debug:query_cli"
    " -- --input_dir=/usr/local/google/home/foo/repos/query_in "
    "--query_ids=SensorCollector,AssemblyCollector --devpath_enabled=true"
    " --hostname='localhost' --port=8000\n";

// Identifier for the output metrics file.
constexpr absl::string_view kMetricOutId = "query_cli_redfish_metrics";

struct DelliciusQueryMetadata {
  struct Query {
    // Captures raw string value of query_id attribute in DelliciusQuery proto.
    std::string query_id;
    // Stores parsed DelliciusQuery proto formatted in human readable debug
    // string
    std::string query_data;
  };
  struct Rules {
    std::string rule_id;
    // Stores parsed QueryRules proto formatted in human readable debug string
    std::string rule_data;
  };
  Rules query_rules;
  std::vector<Query> queries;
};

std::unique_ptr<RedfishCachedGetterInterface> GetRedfishCache(
    RedfishTransport *redfish_transport) {
  CachePolicy redfish_cache_policy = absl::GetFlag(FLAGS_cache_policy);
  if (redfish_cache_policy.type ==
          CachePolicy::CachePolicytType::kTimeBasedCache &&
      redfish_cache_policy.cache_duration_seconds.has_value()) {
    return std::make_unique<TimeBasedCache>(
        redfish_transport, ecclesia::Clock::RealClock(),
        absl::Seconds(redfish_cache_policy.cache_duration_seconds.value()));
  }
  return std::make_unique<NullCache>(redfish_transport);
}

absl::StatusOr<DelliciusQueryMetadata> GetQueriesFromLocation(
    absl::string_view query_dir, absl::string_view rule_location = "") {
  DelliciusQueryMetadata metadata;
  absl::Status status =
      WithEachFileInDirectory(query_dir, [&](absl::string_view dir_entry) {
        auto query_file_path = JoinFilePaths(query_dir, dir_entry);
        DelliciusQuery query_in =
            ParseTextFileAsProtoOrDie<DelliciusQuery>(query_file_path);
        metadata.queries.push_back({.query_id = query_in.query_id(),
                                    .query_data = query_in.DebugString()});
      });
  if (!status.ok()) return status;
  if (metadata.queries.empty()) {
    return absl::NotFoundError(absl::StrFormat(
        "Cannot process Dellicius Queries from given location %s", query_dir));
  }

  // Populate QueryRules if present.
  if (rule_location.empty()) {
    return metadata;
  }

  QueryRules query_rules =
      ParseTextFileAsProtoOrDie<QueryRules>(std::string{rule_location});
  metadata.query_rules.rule_data = query_rules.DebugString();
  return metadata;
}

int QueryMain(int argc, char **argv) {
  absl::SetProgramUsageMessage(kUsage);

  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();

  std::string flag_file_path = absl::GetFlag(FLAGS_flag_file);
  if (flag_file_path.empty()) {
    LOG(ERROR) << "Flag file is empty. Please pass the path to the flag file";
    return -1;
  }
  QueryFlag query_flags = ParseTextFileAsProtoOrDie<QueryFlag>(flag_file_path);

  // Parse Dellicius Queries from the given directory.
  absl::string_view query_rule_location = query_flags.query_rule_location();
  absl::StatusOr<DelliciusQueryMetadata> queries_metadata =
      GetQueriesFromLocation(query_flags.input_dir(), query_rule_location);
  if (!queries_metadata.ok()) {
    LOG(ERROR) << queries_metadata.status();
    return -1;
  }
  // Parse DelliciusQuery identifiers for the queries to dispatch.
  std::vector<std::string> query_ids_provided(query_flags.query_id().begin(),
                                              query_flags.query_id().end());
  if (query_ids_provided.empty()) {
    LOG(ERROR) << "Query Identifier list empty. Try again with non-empty set of"
               << "DelliciusQuery Identifiers.";
    return -1;
  }
  // Construct EmbeddedFile objects to be used with DelliciusQueryEngine.
  std::vector<EmbeddedFile> embedded_files;
  std::for_each(
      queries_metadata->queries.begin(), queries_metadata->queries.end(),
      [&](DelliciusQueryMetadata::Query &query_id_and_data) {
        embedded_files.push_back({.name = query_id_and_data.query_id,
                                  .data = query_id_and_data.query_data});
      });
  EmbeddedFile query_rule_embedded_file;
  if (!query_rule_location.empty() &&
      !queries_metadata->query_rules.rule_data.empty()) {
    query_rule_embedded_file.name = queries_metadata->query_rules.rule_id;
    query_rule_embedded_file.data = queries_metadata->query_rules.rule_data;
  }
  // Build QueryEngineConfiguration from command line arguments.
  QueryEngineConfiguration config{
      .flags{.enable_devpath_extension = query_flags.devpath_enabled(),
             .enable_cached_uri_dispatch = false},
      .query_files{embedded_files},
      .query_rules{query_rule_embedded_file}};
  // Configure HTTP transport.
  std::unique_ptr<RedfishInterface> intf;
  RedfishTransportConfig transport_config = {
      .hostname = query_flags.hostname(),
      .port = query_flags.port(),
      .type = query_flags.transport(),
      .cert_chain = query_flags.cert_chain(),
      .private_key = query_flags.private_key(),
      .root_cert = query_flags.root_cert(),
  };

  absl::StatusOr<std::unique_ptr<RedfishTransport>> transport =
      CreateRedfishTransport(transport_config);

  if (!transport.ok()) {
    LOG(ERROR) << "CreateRedfishTransport failed with: "
               << transport.status().ToString();
    return -1;
  }

  RedfishMetrics transport_metrics;
  if (absl::GetFlag(FLAGS_metrics_enabled)) {
    *transport = std::make_unique<MetricalRedfishTransport>(
        std::move(*transport), Clock::RealClock(), transport_metrics);
  }

  std::unique_ptr<RedfishCachedGetterInterface> cache =
      GetRedfishCache(transport->get());

  intf = NewHttpInterface(std::move(*transport), std::move(cache),
                          RedfishInterface::kTrusted);
  if (auto root = intf->GetRoot(); root.AsObject() == nullptr) {
    LOG(ERROR) << "Error connecting to redfish service. "
               << "Check host configuration";
    return -1;
  }

  // Build QueryEngine
  QueryEngine query_engine(config, Clock::RealClock(), std::move(intf));

  // Dispatch Dellicius Queries
  for (size_t i = 0; i < absl::GetFlag(FLAGS_iteration_count); ++i) {
    std::vector<DelliciusQueryResult> response_entries =
        query_engine.ExecuteQuery(absl::FixedArray<absl::string_view>(
            query_ids_provided.begin(), query_ids_provided.end()));
    std::cout << "Output from Dellicius Query Engine: " << std::endl;
    std::for_each(
        response_entries.begin(), response_entries.end(),
        [](auto &entry) { std::cout << entry.DebugString() << std::endl; });
  }

  if (!absl::GetFlag(FLAGS_metrics_enabled)) {
    return 0;
  }

  std::string output_directory(
      absl::GetFlag(FLAGS_transport_metric_output_dir));
  std::string metric_out_file_location =
      JoinFilePaths(output_directory, kMetricOutId);
  std::ofstream metric_file_stream(metric_out_file_location,
                                   std::ofstream::out | std::ofstream::trunc);
  if (metric_file_stream.bad() || !metric_file_stream.is_open()) {
    LOG(ERROR) << "Error opening output file for storing transport metrics ";
    return -1;
  }
  std::cout << "Writing transport metrics in file " << metric_out_file_location
            << std::endl;
  metric_file_stream << std::setw(2) << transport_metrics.DebugString()
                     << std::endl;
  return 0;
}

}  // namespace

}  // namespace ecclesia

int main(int argc, char **argv) { return ecclesia::QueryMain(argc, argv); }
