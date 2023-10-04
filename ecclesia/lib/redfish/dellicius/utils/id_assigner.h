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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_ID_ASSIGNER_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_ID_ASSIGNER_H_

#include <functional>
#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"

namespace ecclesia {

// Generic interface which assigns an identifier to a SubqueryDataSet output.
class IdAssigner {
 public:
  virtual ~IdAssigner() = default;

  // Assign an identifier based on local devpath in the data_set.
  // Return absl::NotFoundError if a translation does not exist.
  virtual absl::StatusOr<std::string> IdForLocalDevpathInDataSet(
      SubqueryDataSet data_set) = 0;

  // Assign an identifier based on Redfish location in the data_set.
  // Return absl::NotFoundError if a translation does not exist.
  virtual absl::StatusOr<std::string> IdForRedfishLocationInDataSet(
      SubqueryDataSet data_set, bool is_root = false) = 0;
};

// Generic factory type to generate IdAssigner for given local id map and
// entity_tag
template <typename LocalIdMapT>
using IdAssignerFactory = std::function<std::unique_ptr<IdAssigner>(
    const LocalIdMapT &map, std::string entity_tag)>;

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_ID_ASSIGNER_H_
