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
  normalizer->AddNormilizer(absl::make_unique<NormalizerImplDefault>());
  return normalizer;
}

// Builds normalizer that transparently returns queried redfish property but
// extends the QueryPlanner to construct devpath for normalized subquery output.
inline std::unique_ptr<Normalizer> BuildDefaultNormalizerWithLocalDevpath(
    NodeTopology node_topology) {
  auto normalizer = BuildDefaultNormalizer();
  normalizer->AddNormilizer(
      absl::make_unique<NormalizerImplAddDevpath>(std::move(node_topology)));
  return normalizer;
}

// Extends default normalizer to populate machine devpaths using Redfish stable
// identifier.
template <typename LocalIdMapT>
std::unique_ptr<Normalizer> BuildDefaultNormalizerWithMachineDevpath(
    const std::string &server_tag, std::unique_ptr<LocalIdMapT> local_id_map,
    const IdAssignerFactory<LocalIdMapT> &id_assigner_factory) {
  if (local_id_map == nullptr) return nullptr;
  std::unique_ptr<Normalizer> normalizer = BuildDefaultNormalizer();
  normalizer->AddNormilizer(
      absl::make_unique<NormalizerImplAddMachineBarepath<LocalIdMapT>>(
          std::move(local_id_map),
          id_assigner_factory(*local_id_map.get(), server_tag)));
  return normalizer;
}

// Returns a decorated query engine normalizer that has the following
// normalizers stacked from lower to higher layers of abstraction:
// default, local devpath, machine devpath.
//
// Use this normalizer if you want query engine to construct machine devpath
// using local devpath.
template <typename LocalIdMapT>
std::unique_ptr<Normalizer> BuildDefaultNormalizerWithMachineDevpath(
    const std::string &server_tag, std::unique_ptr<LocalIdMapT> local_id_map,
    const IdAssignerFactory<LocalIdMapT> &id_assigner_factory,
    NodeTopology node_topology) {
  if (local_id_map == nullptr) return nullptr;
  auto normalizer = BuildDefaultNormalizer();
  normalizer->AddNormilizer(
      absl::make_unique<NormalizerImplAddDevpath>(std::move(node_topology)));
  normalizer->AddNormilizer(
      absl::make_unique<NormalizerImplAddMachineBarepath<LocalIdMapT>>(
          std::move(local_id_map),
          id_assigner_factory(*local_id_map.get(), server_tag)));
  return normalizer;
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_FACTORY_H_
