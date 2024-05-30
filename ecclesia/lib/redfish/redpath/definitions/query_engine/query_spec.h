/*
 * Copyright 2024 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ENGINE_QUERY_SPEC_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ENGINE_QUERY_SPEC_H_

#include <optional>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_rules.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

// Encapsulates the context needed to execute RedPath query.
struct QueryContext {
  // Describes the RedPath queries that engine will be configured to execute.
  absl::Span<const EmbeddedFile> query_files;
  // Rules used to configure Redfish query parameter - $expand for
  // specific RedPath prefixes in given queries.
  absl::Span<const EmbeddedFile> query_rules;
  const Clock *clock = Clock::RealClock();
};

// Encapsulates the queries and rules needed to execute Redpath Query.
// This is a resolved QueryContext containing the actual query and rule read
// from the embedded files.
struct QuerySpec {
  struct QueryInfo {
    DelliciusQuery query;
    QueryRules::RedPathPrefixSetWithQueryParams rule;
    std::optional<absl::Duration> timeout;
  };

  // Map of query id to query info.
  absl::flat_hash_map<std::string, QueryInfo> query_id_to_info;
  const Clock *clock = Clock::RealClock();

  // Utility function to convert query context to resolved QuerySpec.
  static absl::StatusOr<QuerySpec> FromQueryContext(
      const QueryContext &query_context);

  // Utility function to read query and rule files and convert them to a
  // resolved QuerySpec.
  static absl::StatusOr<QuerySpec> FromQueryFiles(
      absl::Span<const std::string> query_files,
      absl::Span<const std::string> query_rules,
      const Clock *clock = Clock::RealClock());
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ENGINE_QUERY_SPEC_H_
