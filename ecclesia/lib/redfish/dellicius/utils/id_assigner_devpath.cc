/*
 * Copyright 2023 Google LLC
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

#include "ecclesia/lib/redfish/dellicius/utils/id_assigner_devpath.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {
namespace {

class MapBasedDevpathAssigner : public IdAssigner {
 public:
  explicit MapBasedDevpathAssigner(
      absl::flat_hash_map<std::string, std::string> map)
      : map_(std::move(map)) {}

  virtual absl::StatusOr<std::string> IdForLocalDevpathInDataSet(
      const SubqueryDataSet& data_set) override {
    if (!data_set.has_devpath() || data_set.devpath().empty()) {
      return absl::NotFoundError("");
    }
    auto itr = map_.find(data_set.devpath());
    if (itr == map_.end()) {
      return absl::NotFoundError("");
    }
    return itr->second;
  }

  absl::StatusOr<std::string> IdForLocalDevpathInQueryResult(
      const QueryResultData& query_result) override {
    QueryResultDataReader reader(&query_result);
    ECCLESIA_ASSIGN_OR_RETURN(QueryValueReader query_value_reader,
                              reader.Get(kIdentifierTag));
    if (query_value_reader.identifier().local_devpath().empty()) {
      return absl::NotFoundError("Cannot find local devpath in query result.");
    }
    auto itr = map_.find(query_value_reader.identifier().local_devpath());
    if (itr == map_.end()) {
      return absl::NotFoundError("Cannot find devpath in map.");
    }
    return itr->second;
  }

  virtual absl::StatusOr<std::string> IdForRedfishLocationInDataSet(
      const SubqueryDataSet& /*data_set*/, bool /*is_root*/) override {
    return absl::NotFoundError("");
  }

  absl::StatusOr<std::string> IdForRedfishLocationInQueryResult(
      const QueryResultData& query_result /*data_set*/,
      bool is_root /*is_root*/) override {
    return absl::UnimplementedError("");
  }

 private:
  absl::flat_hash_map<std::string, std::string> map_;
};

}  // namespace

std::unique_ptr<IdAssigner> NewMapBasedDevpathAssigner(
    absl::flat_hash_map<std::string, std::string> map) {
  return std::make_unique<MapBasedDevpathAssigner>(std::move(map));
}

}  // namespace ecclesia
