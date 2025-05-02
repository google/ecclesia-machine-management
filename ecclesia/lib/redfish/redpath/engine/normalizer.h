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

#include <stdbool.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {

struct RedpathNormalizerOptions {
  bool enable_url_annotation = false;
};

// Provides an interface for normalizing a redfish response into QueryResultData
// for the property specification in a Dellicius Subquery.
class RedpathNormalizer {
 public:
  // Stable id types used to configure engine for an appropriate normalizer that
  // decorates the query result with desired stable id type.
  enum class RedfishStableIdType : uint8_t {
    kRedfishLocation,  // Redfish Standard - PartLocationContext + ServiceLabel
    kRedfishLocationDerived  // Derived from Redfish topology.
  };

  using QueryIdToNormalizerMap =
      absl::flat_hash_map<std::string, std::unique_ptr<RedpathNormalizer>>;
  // Factory for creating a map of RedpathNormalizer, keyed by query id.
  using RedpathNormalizersFactory =
      absl::AnyInvocable<QueryIdToNormalizerMap()>;
  // Map of RedpathNormalizersFactory, keyed by platform name.
  using NormalizersFactoryMap = absl::flat_hash_map<
      std::string, ecclesia::RedpathNormalizer::RedpathNormalizersFactory>;

  class ImplInterface {
   public:
    virtual ~ImplInterface() = default;

    virtual absl::Status Normalize(const RedfishObject &redfish_object,
                                   const DelliciusQuery::Subquery &query,
                                   ecclesia::QueryResultData &data_set,
                                   const RedpathNormalizerOptions &options) = 0;
  };

  // Returns normalized dataset, possibly empty. RedpathNormalizers can be
  // nested and empty dataset on one level can be extended in outer normalizers.
  // This normalization would start from an empty dataset.
  absl::StatusOr<ecclesia::QueryResultData> Normalize(
      const RedfishObject &redfish_object,
      const DelliciusQuery::Subquery &query,
      const RedpathNormalizerOptions &options) {
    ecclesia::QueryResultData data_set;
    ECCLESIA_RETURN_IF_ERROR(
        Normalize(redfish_object, query, data_set, options));
    return data_set;
  }

  // Similar to above method but accepts an existing dataset, so we can chain
  // multiple normalizers.
  absl::Status Normalize(const RedfishObject &redfish_object,
                         const DelliciusQuery::Subquery &query,
                         ecclesia::QueryResultData &data_set,
                         const RedpathNormalizerOptions &options) {
    // It's ok to use a simple mutex here. If we ever detect lock contention
    // and we know that the writes are less frequent, we can convert this mutex
    // to Reader-writer lock.
    absl::MutexLock l(&impl_chain_mu_);
    if (impl_chain_.empty()) return absl::NotFoundError("No normalizers added");

    for (const auto &impl : impl_chain_) {
      ECCLESIA_RETURN_IF_ERROR(
          impl->Normalize(redfish_object, query, data_set, options));
    }

    // Return an error if data set is empty - no field and no devpath
    if (data_set.fields_size() <= 0) {
      return absl::NotFoundError("Resulting dataset is empty");
    }
    return absl::OkStatus();
  }

  // Similar to above method but does not require a RedfishObject or Subquery.
  absl::Status Normalize(ecclesia::QueryResultData &data_set,
                         const RedpathNormalizerOptions &options) {
    DummyRedfishObject dummy_redfish_object;
    return Normalize(dummy_redfish_object, DelliciusQuery::Subquery(), data_set,
                     options);
  }

  void AddRedpathNormalizer(std::unique_ptr<ImplInterface> impl) {
    absl::MutexLock l(&impl_chain_mu_);
    impl_chain_.push_back(std::move(impl));
  }

 protected:
  absl::Mutex impl_chain_mu_;  // Protects impl_chain_
  std::vector<std::unique_ptr<ImplInterface>> impl_chain_
      ABSL_GUARDED_BY(impl_chain_mu_);
};

// Populates the Subquery output using property requirements in the subquery.
class RedpathNormalizerImplDefault final
    : public RedpathNormalizer::ImplInterface {
 public:
  RedpathNormalizerImplDefault();

 protected:
  absl::Status Normalize(const RedfishObject &redfish_object,
                         const DelliciusQuery::Subquery &subquery,
                         ecclesia::QueryResultData &data_set,
                         const RedpathNormalizerOptions &options) override;

 private:
  std::vector<DelliciusQuery::Subquery::RedfishProperty> additional_properties_;
};

// Adds devpath to subquery output.
class RedpathNormalizerImplAddDevpath final
    : public RedpathNormalizer::ImplInterface {
 public:
  explicit RedpathNormalizerImplAddDevpath(NodeTopology node_topology)
      : topology_(std::move(node_topology)) {}

 protected:
  absl::Status Normalize(const RedfishObject &redfish_object,
                         const DelliciusQuery::Subquery &subquery,
                         ecclesia::QueryResultData &data_set,
                         const RedpathNormalizerOptions &options) override;

 private:
  NodeTopology topology_;
};

// Adds machine level barepath to subquery output.
class RedpathNormalizerImplAddMachineBarepath final
    : public RedpathNormalizer::ImplInterface {
 public:
  explicit RedpathNormalizerImplAddMachineBarepath(
      std::unique_ptr<IdAssigner> id_assigner,
      bool use_redfish_location_as_fallback)
      : id_assigner_(std::move(id_assigner)),
      use_redfish_location_as_fallback_(use_redfish_location_as_fallback) {}

 protected:
  absl::Status Normalize(const RedfishObject &redfish_object,
                         const DelliciusQuery::Subquery &subquery,
                         ecclesia::QueryResultData &data_set,
                         const RedpathNormalizerOptions &options) override;

 private:
  std::unique_ptr<IdAssigner> id_assigner_;
  bool use_redfish_location_as_fallback_;
};

// Step 1 for Devpath2 and Step 1 for Devpath3:
// Builds normalizer that transparently returns queried redfish property without
// normalization for client variables or devpaths.
// Local Devpath derived here: From "GetAdditionalProperties()" collection.
// Redfish Location derived here: From "GetAdditionalProperties()" collection.
inline std::unique_ptr<RedpathNormalizer> BuildDefaultRedpathNormalizer() {
  auto normalizer = std::make_unique<RedpathNormalizer>();
  normalizer->AddRedpathNormalizer(
      std::make_unique<RedpathNormalizerImplDefault>());
  return normalizer;
}

// Step 2 for Devpath2:
// Local Devpath derived here (Fallback to step 1): From node topology.
inline std::unique_ptr<RedpathNormalizer>
BuildDefaultRedpathNormalizerWithLocalDevpath(NodeTopology node_topology) {
  // Includes step 1 for Devpath2
  auto normalizer = BuildDefaultRedpathNormalizer();
  // Step 2 for Devpath2
  normalizer->AddRedpathNormalizer(
      std::make_unique<RedpathNormalizerImplAddDevpath>(
          std::move(node_topology)));
  return normalizer;
}

// Step 2 for Devpath3:
// Machine Devpath derived here:
// 1: From node_local_barepath map. Requires local devpath present from step 1.
// 2 (Fallback): From redfish location map. Requires service label and part
// location context present from step 1.
inline std::unique_ptr<RedpathNormalizer>
BuildRedpathNormalizerWithMachineDevpath(
    std::unique_ptr<IdAssigner> id_assigner) {
  // Includes step 1 for Devpath3
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();
  // Step 2 for Devpath3
  normalizer->AddRedpathNormalizer(
      std::make_unique<RedpathNormalizerImplAddMachineBarepath>(
          std::move(id_assigner), /*use_redfish_location_as_fallback=*/true));
  return normalizer;
}

// Step 3 for Devpath2:
// Machine Devpath derived here: From node_local_barepath map. Requires local
// devpath present from step 1 or step 2. No fallback.
inline std::unique_ptr<RedpathNormalizer>
BuildRedpathNormalizerWithMachineDevpath(
    std::unique_ptr<IdAssigner> id_assigner, NodeTopology node_topology) {
  // Includes steps 1 and 2 for Devpath2
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizerWithLocalDevpath(std::move(node_topology));
  // Step 3 for Devpath2
  normalizer->AddRedpathNormalizer(
      std::make_unique<RedpathNormalizerImplAddMachineBarepath>(
          std::move(id_assigner), /*use_redfish_location_as_fallback=*/false));
  return normalizer;
}

// Returns an empty set of RedpathNormalizers.
inline RedpathNormalizer::QueryIdToNormalizerMap DefaultRedpathNormalizerMap() {
  return {};
}

// For stable_id_type = kRedfishLocationDerived, returns "Step 2 for Devpath2".
// For stable_id_type = kRedfishLocation, returns "Step 1 for Devpath3".
std::unique_ptr<RedpathNormalizer> BuildLocalDevpathRedpathNormalizer(
    RedfishInterface *redfish_interface,
    RedpathNormalizer::RedfishStableIdType stable_id_type,
    absl::string_view redfish_topology_config_name);

// For stable_id_type = kRedfishLocationDerived, returns "Step 3 for Devpath2".
// For stable_id_type = kRedfishLocation, returns "Step 2 for Devpath3".
std::unique_ptr<RedpathNormalizer> GetMachineDevpathRedpathNormalizer(
    RedpathNormalizer::RedfishStableIdType stable_id_type,
    absl::string_view redfish_topology_config_name,
    std::unique_ptr<IdAssigner> id_assigner,
    RedfishInterface *redfish_interface);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_NORMALIZER_H_
