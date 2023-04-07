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

#include "ecclesia/lib/redfish/dellicius/utils/id_mapper_devpath.h"

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_mapper.h"

namespace ecclesia {
namespace {

class DevpathIdentifierMapper : public IdentifierMapper<std::string> {
 public:
  DevpathIdentifierMapper(absl::flat_hash_map<std::string, std::string> map)
      : map_(std::move(map)) {}

  virtual absl::StatusOr<std::string> IdentifierForSubqueryDataSet(
      SubqueryDataSet data_set) override {
    if (!data_set.has_devpath() || data_set.devpath().empty()) {
      return absl::NotFoundError("");
    }
    auto itr = map_.find(data_set.devpath());
    if (itr == map_.end()) {
      return absl::NotFoundError("");
    }
    return itr->second;
  }

 private:
  absl::flat_hash_map<std::string, std::string> map_;
};

}  // namespace

std::unique_ptr<IdentifierMapper<std::string>> NewDevpathIdentifierMapper(
    absl::flat_hash_map<std::string, std::string> map) {
  return std::make_unique<DevpathIdentifierMapper>(std::move(map));
}

}  // namespace ecclesia