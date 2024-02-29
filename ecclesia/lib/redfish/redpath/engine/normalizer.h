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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/synchronization/mutex.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {
// Provides an interface for normalizing a redfish response into QueryResultData
// for the property specification in a Dellicius Subquery.
class Normalizer {
 public:
  class ImplInterface {
   public:
    virtual ~ImplInterface() = default;

    virtual absl::Status Normalize(const RedfishObject &redfish_object,
                                   const DelliciusQuery::Subquery &query,
                                   ecclesia::QueryResultData &data_set) = 0;
  };

  // Returns normalized dataset, possibly empty. Normalizers can be nested
  // and empty dataset on one level can be extended in outer normalizers.
  absl::StatusOr<ecclesia::QueryResultData> Normalize(
      const RedfishObject &redfish_object,
      const DelliciusQuery::Subquery &query) {
    // It's ok to use a simple mutex here. If we ever detect lock contention
    // and we know that the writes are less frequent, we can convert this mutex
    // to Reader-writer lock.
    absl::MutexLock l(&impl_chain_mu_);
    if (impl_chain_.empty()) return absl::NotFoundError("No normalizers added");
    ecclesia::QueryResultData data_set;

    for (const auto &impl : impl_chain_) {
      ECCLESIA_RETURN_IF_ERROR(
          impl->Normalize(redfish_object, query, data_set));
    }

    // Return an error if data set is empty - no field and no devpath
    if (data_set.fields_size() <= 0) {
      return absl::NotFoundError("Resulting dataset is empty");
    }
    return data_set;
  }

  void AddNormalizer(std::unique_ptr<ImplInterface> impl) {
    absl::MutexLock l(&impl_chain_mu_);
    impl_chain_.push_back(std::move(impl));
  }

 protected:
  absl::Mutex impl_chain_mu_;  // Protects impl_chain_
  std::vector<std::unique_ptr<ImplInterface>> impl_chain_
      ABSL_GUARDED_BY(impl_chain_mu_);
};

// Populates the Subquery output using property requirements in the subquery.
class NormalizerImplDefault final : public Normalizer::ImplInterface {
 public:
  NormalizerImplDefault();

 protected:
  absl::Status Normalize(const RedfishObject &redfish_object,
                         const DelliciusQuery::Subquery &subquery,
                         ecclesia::QueryResultData &data_set) override;

 private:
  std::vector<DelliciusQuery::Subquery::RedfishProperty> additional_properties_;
};

// Adds devpath to subquery output.
class NormalizerImplAddDevpath final : public Normalizer::ImplInterface {
 public:
  explicit NormalizerImplAddDevpath(NodeTopology node_topology)
      : topology_(std::move(node_topology)) {}

 protected:
  absl::Status Normalize(const RedfishObject &redfish_object,
                         const DelliciusQuery::Subquery &subquery,
                         ecclesia::QueryResultData &data_set) override;

 private:
  NodeTopology topology_;
};

// Builds normalizer that transparently returns queried redfish property without
// normalization for client variables or devpaths.
inline std::unique_ptr<Normalizer> BuildDefaultNormalizer() {
  auto normalizer = std::make_unique<Normalizer>();
  normalizer->AddNormalizer(std::make_unique<NormalizerImplDefault>());
  return normalizer;
}

// Builds normalizer that transparently returns queried redfish property but
// extends the QueryPlanner to construct devpath for normalized subquery output.
inline std::unique_ptr<Normalizer> BuildDefaultNormalizerWithLocalDevpath(
    NodeTopology node_topology) {
  auto normalizer = BuildDefaultNormalizer();
  normalizer->AddNormalizer(
      std::make_unique<NormalizerImplAddDevpath>(std::move(node_topology)));
  return normalizer;
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_NORMALIZER_H_
