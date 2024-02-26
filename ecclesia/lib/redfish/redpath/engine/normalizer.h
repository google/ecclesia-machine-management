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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_NORMALIZER_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_NORMALIZER_H_

#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"

namespace ecclesia {
// Provides an interface for normalizing a redfish response into QueryResultData
// for the property specification in a Dellicius Subquery.
class Normalizer {
 public:
  virtual ~Normalizer() = default;

  virtual absl::StatusOr<ecclesia::QueryResultData> Normalize(
      const ecclesia::RedfishObject &redfish_object,
      const DelliciusQuery::Subquery &subquery) const = 0;
};

// Populates the QueryResultData output using property requirements in the
// subquery.
class NormalizerImpl : public Normalizer {
 public:
  explicit NormalizerImpl() = default;
  absl::StatusOr<ecclesia::QueryResultData> Normalize(
      const ecclesia::RedfishObject &redfish_object,
      const DelliciusQuery::Subquery &subquery) const override;
};
}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_NORMALIZER_H_
