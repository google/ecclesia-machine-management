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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_FACTORY_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_FACTORY_H_

#include <memory>

#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/normalizer.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/query_planner.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/interface.h"

namespace ecclesia {

// Builds normalizer that transparently returns queried redfish property without
// normalization for client variables or devpaths.
inline std::unique_ptr<Normalizer> BuildDefaultNormalizer() {
  return std::make_unique<DefaultNormalizer>();
}
// Builds normalizer that transparently returns queried redfish property but
// extends the QueryPlanner to construct devpath for normalized subquery output.
inline std::unique_ptr<Normalizer> BuildDefaultNormalizerWithDevpath(
    RedfishInterface *interface) {
  return std::make_unique<NormalizerDevpathDecorator>(BuildDefaultNormalizer(),
                                                      interface);
}
// Builds the default query planner.
inline std::unique_ptr<QueryPlannerInterface> BuildQueryPlanner(
    const DelliciusQuery &query, Normalizer *normalizer) {
  return std::make_unique<QueryPlanner>(query, normalizer);
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_FACTORY_H_
