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

#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"

namespace ecclesia {

// Generic interface which assigns an identifier to a SubqueryDataSet output.
template <typename IdT>
class IdAssigner {
 public:
  virtual ~IdAssigner() {}

  // Assign an identifier to the the data_set. Return absl::NotFoundError if a
  // translation does not exist.
  virtual absl::StatusOr<IdT> IdForSubqueryDataSet(
      SubqueryDataSet data_set) = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_ID_ASSIGNER_H_
