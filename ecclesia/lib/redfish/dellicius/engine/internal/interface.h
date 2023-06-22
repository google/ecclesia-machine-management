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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_INTERFACE_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_INTERFACE_H_

#include <memory>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

// Maps RedPath prefix to the Expand value.
using RedPathRedfishQueryParams =
    absl::flat_hash_map<std::string /* RedPath */, GetParams>;

// A lightweight tracker capturing executed Redpaths in a single Query.
// It is a key construct used in tuning Redfish Query Parameters.
// This also serves as a placeholder for any contextual information required
// around a query operation.
struct QueryTracker {
  RedPathRedfishQueryParams redpaths_queried;
};

// Provides an interface for normalizing a redfish response into SubqueryDataSet
// for the property specification in a Dellicius Subquery.
class Normalizer {
 public:
  class ImplInterface {
   public:
    virtual ~ImplInterface() = default;

    virtual absl::Status Normalize(const RedfishObject &redfish_object,
                                   const DelliciusQuery::Subquery &query,
                                   SubqueryDataSet &data_set) const = 0;
  };

  // Returns normalized dataset, possibly empty. Normalizers can be nested
  // and empty dataset on one level can be extended in outer normalizers.
  absl::StatusOr<SubqueryDataSet> Normalize(
      const RedfishObject &redfish_object,
      const DelliciusQuery::Subquery &query) {
    if (impl_chain_.empty()) return absl::NotFoundError("No normalizers added");
    SubqueryDataSet data_set;
    for (const auto &impl : impl_chain_) {
      ECCLESIA_RETURN_IF_ERROR(
          impl->Normalize(redfish_object, query, data_set));
    }
    // Return an error if data set is empty - no field and no devpath
    if (data_set.properties().empty() && !data_set.has_devpath()) {
      return absl::NotFoundError("Resulting dataset is empty");
    }
    return data_set;
  }

  void AddNormilizer(std::unique_ptr<ImplInterface> impl) {
    impl_chain_.push_back(std::move(impl));
  }

 protected:
  std::vector<std::unique_ptr<ImplInterface>> impl_chain_;
};

// Provides an interface for executing a Dellicius Query plan.
class QueryPlannerInterface {
 public:
  virtual ~QueryPlannerInterface() = default;
  // Executes query plan using RedfishVariant as root.
  // The RedfishVariant can be the service root (redfish/v1) or any redfish
  // resource acting as local root for redfish subtree.
  virtual DelliciusQueryResult Run(const RedfishVariant &variant,
                                   const Clock &clock,
                                   QueryTracker *tracker) = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_INTERFACE_H_
