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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_FACTORY_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_FACTORY_H_

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/normalizer.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/node_topology.h"

namespace ecclesia {

// Builds normalizer that transparently returns queried redfish property without
// normalization for client variables or devpaths.
inline std::unique_ptr<Normalizer> BuildDefaultNormalizer() {
  auto normalizer = std::make_unique<Normalizer>();
  normalizer->AddNormalizer(absl::make_unique<NormalizerImplDefault>());
  return normalizer;
}

// Builds normalizer that transparently returns queried redfish property but
// extends the QueryPlanner to construct devpath for normalized subquery output.
inline std::unique_ptr<Normalizer> BuildDefaultNormalizerWithLocalDevpath(
    NodeTopology node_topology) {
  auto normalizer = BuildDefaultNormalizer();
  normalizer->AddNormalizer(
      absl::make_unique<NormalizerImplAddDevpath>(std::move(node_topology)));
  return normalizer;
}

// Extends default normalizer to populate machine devpaths using Redfish stable
// identifier.
inline std::unique_ptr<Normalizer> BuildNormalizerWithMachineDevpath(
    std::unique_ptr<IdAssigner> id_assigner) {
  std::unique_ptr<Normalizer> normalizer = BuildDefaultNormalizer();
  // Templatized based on int since the map is not being used and will be a part
  // of the id assigner.
  normalizer->AddNormalizer(
      absl::make_unique<NormalizerImplAddMachineBarepath<int>>(
          nullptr, std::move(id_assigner)));
  return normalizer;
}

// Extends default normalizer to populate machine devpaths using local devpath
// as the stable id.
inline std::unique_ptr<Normalizer> BuildNormalizerWithMachineDevpath(
    std::unique_ptr<IdAssigner> id_assigner, NodeTopology node_topology) {
  std::unique_ptr<Normalizer> normalizer =
      BuildDefaultNormalizerWithLocalDevpath(std::move(node_topology));
  // Templatized based on int since the map is not being used and will be a part
  // of the id assigner.
  normalizer->AddNormalizer(
      absl::make_unique<NormalizerImplAddMachineBarepath<int>>(
          nullptr, std::move(id_assigner),
          /*use_local_devpath_for_machine_devpath=*/true));
  return normalizer;
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_FACTORY_H_
