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

#include "ecclesia/lib/redfish/dellicius/engine/file_backed_query_engine.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/status/macros.h"
#include "google/protobuf/text_format.h"

namespace ecclesia {
namespace {

absl::StatusOr<QueryResult> ReadFromFile(const std::string& filename) {
  ApifsFile query_result_file(filename);
  ECCLESIA_ASSIGN_OR_RETURN(std::string result_data, query_result_file.Read());

  QueryResult query_result;
  if (!google::protobuf::TextFormat::ParseFromString(result_data, &query_result)) {
    return absl::InternalError(
        absl::StrCat("Unable to read query result from file: ", filename));
  }
  return std::move(query_result);
}

}  // namespace

absl::StatusOr<std::unique_ptr<QueryEngineIntf>> FileBackedQueryEngine::Create(
    absl::string_view path, Cache cache) {
  ApifsDirectory dir((std::string(path)));
  if (!dir.Exists()) {
    return absl::NotFoundError(
        absl::StrCat("Directory '", path, "' does not exist."));
  }

  ECCLESIA_ASSIGN_OR_RETURN(std::vector<std::string> filenames,
                            dir.ListEntryPaths());

  QueryIdToFilenameMap query_id_to_filename_map;

  for (const std::string& filename : filenames) {
    absl::StatusOr<QueryResult> query_result = ReadFromFile(filename);
    if (!query_result.ok()) {
      continue;
    }
    auto [it, inserted] =
        query_id_to_filename_map.insert({query_result->query_id(), filename});
    if (!inserted) {
      return absl::InternalError(
          absl::StrCat("Duplicate file for query '", query_result->query_id()));
    }
  }

  if (query_id_to_filename_map.empty()) {
    return absl::InternalError("No files found in directory.");
  }

  return absl::WrapUnique(
      new FileBackedQueryEngine(std::move(query_id_to_filename_map), cache));
}

QueryIdToResult FileBackedQueryEngine::ExecuteRedpathQuery(
    absl::Span<const absl::string_view> query_ids,
    const RedpathQueryOptions& options) {
  QueryIdToResult result;
  for (absl::string_view query_id : query_ids) {
    QueryResult& result_entry = (*result.mutable_results())[query_id];
    absl::MutexLock lock(&query_result_mutex_);
    if (cache_ == Cache::kInfinite) {
      auto it = query_result_cache_.find(query_id);
      if (it != query_result_cache_.end()) {
        result_entry = it->second;
        continue;
      }
    }
    auto it = query_id_to_filename_map_.find(query_id);
    if (it == query_id_to_filename_map_.end()) {
      result_entry.mutable_status()->mutable_errors()->Add(
          absl::StrCat("No file for query id '", query_id, "' found"));
      continue;
    }
    absl::StatusOr<QueryResult> query_result = ReadFromFile(it->second);
    if (!query_result.ok()) {
      result_entry.mutable_status()->mutable_errors()->Add(
          std::string(query_result.status().message()));
      continue;
    }

    if (cache_ == Cache::kInfinite) {
      query_result_cache_[query_id] = *query_result;
    }
    result_entry = std::move(*query_result);
  }

  return result;
}

}  // namespace ecclesia
