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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_FILE_BACKED_QUERY_ENGINE_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_FILE_BACKED_QUERY_ENGINE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/passkey.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"

namespace ecclesia {

// This class serves as a test double for the Query Engine, relying on textproto
// query result files stored within a specified filesystem path. It reads the
// contents of the textproto file corresponding to a given query_id and returns
// the QueryResult when its ExecuteRedpathQuery() method is invoked for that
// query_id.
class FileBackedQueryEngine : public QueryEngineIntf {
 public:
  // To determine if the contents of the file must be cached to prevent
  // subsequence FileIO. If enabled, the cache is updated on the first
  // ExecuteRedpathQuery() for a given query_id and not at the time of object
  // creation.
  enum class Cache : uint8_t { kDisable = 0, kInfinite };

  // Creates an object of FileBackedQueryEngine. Returns an error if the path
  // doesn't exist, if no query result files are present in the directory or if
  // there are duplicate files for a given query ID in the directory.
  static absl::StatusOr<std::unique_ptr<QueryEngineIntf>> Create(
      absl::string_view path, Cache cache = Cache::kDisable);

  QueryIdToResult ExecuteRedpathQuery(
      absl::Span<const absl::string_view> query_ids,
      ServiceRootType service_root_uri,
      const QueryVariableSet &query_arguments) override;

  absl::StatusOr<SubscriptionQueryResult> ExecuteSubscriptionQuery(
      absl::Span<const absl::string_view> query_ids,
      const QueryVariableSet &query_arguments,
      StreamingOptions streaming_options) override {
    return absl::UnimplementedError(
        "ExecuteSubscriptionQuery() method is not supported");
  }

  ABSL_DEPRECATED("Use ExecuteRedpathQuery Instead")
  std::vector<DelliciusQueryResult> ExecuteQuery(
      absl::Span<const absl::string_view> query_ids,
      ServiceRootType service_root_uri,
      const QueryVariableSet &query_arguments) override {
    // ExecuteQuery() method is deprecated and is not supported
    return {};
  }

  absl::StatusOr<RedfishInterface *> GetRedfishInterface(
      RedfishInterfacePasskey unused_passkey) override {
    return absl::UnimplementedError(
        "No redfish interface in FileBakedQueryEngine");
  }

 private:
  using QueryIdToFilenameMap = absl::flat_hash_map<std::string, std::string>;

  FileBackedQueryEngine(QueryIdToFilenameMap query_id_to_filename_map,
                        Cache cache)
      : query_id_to_filename_map_(std::move(query_id_to_filename_map)),
        cache_(cache) {}

  const QueryIdToFilenameMap query_id_to_filename_map_;
  const Cache cache_;
  absl::Mutex query_result_mutex_;
  absl::flat_hash_map<std::string, QueryResult> query_result_cache_
      ABSL_GUARDED_BY(query_result_mutex_);
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_FILE_BACKED_QUERY_ENGINE_H_
