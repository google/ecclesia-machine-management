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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_NORMALIZER_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_NORMALIZER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/node_topology.h"

namespace ecclesia {

// Populates the Subquery output using property requirements in the subquery.
class NormalizerImplDefault final : public Normalizer::ImplInterface {
 public:
  NormalizerImplDefault();

 protected:
  // with fallback to default CSDL bundle.
  absl::Status Normalize(const RedfishObject &redfish_object,
                         const DelliciusQuery::Subquery &subquery,
                         SubqueryDataSet &data_set) const;

 private:
  std::vector<DelliciusQuery::Subquery::RedfishProperty> additional_properties_;
};

// Adds devpath to subquery output.
class NormalizerImplAddDevpath final : public Normalizer::ImplInterface {
 public:
  NormalizerImplAddDevpath(NodeTopology node_topology)
      : topology_(std::move(node_topology)) {}

 protected:
  absl::Status Normalize(const RedfishObject &redfish_object,
                         const DelliciusQuery::Subquery &subquery,
                         SubqueryDataSet &data_set) const override;

  absl::StatusOr<const NodeTopology *> GetNodeTopology() const override {
    return &topology_;
  }

 private:
  NodeTopology topology_;
};

// Adds machine level barepath to subquery output.
template <typename LocalIdMapT>
class NormalizerImplAddMachineBarepath final
    : public Normalizer::ImplInterface {
 public:
  NormalizerImplAddMachineBarepath(std::unique_ptr<LocalIdMapT> local_id_map,
                                   std::unique_ptr<IdAssigner> id_assigner)
      : local_id_map_(std::move(local_id_map)),
        id_assigner_(std::move(id_assigner)) {}

 protected:
  absl::Status Normalize(const RedfishObject &redfish_object,
                         const DelliciusQuery::Subquery &subquery,
                         SubqueryDataSet &data_set) const override {
    // Root devpath is assigned to the root Chassis, to do this we need to track
    // if the resource is Chassis type and has no redfish location.
    bool is_root = false;
    std::string resource_type;
    redfish_object["@odata.type"].GetValue(&resource_type);
    if (absl::StrContains(resource_type, "#Chassis.") &&
        (!data_set.has_redfish_location() ||
         (data_set.redfish_location().service_label().empty() &&
          data_set.redfish_location().part_location_context().empty()))) {
      is_root = true;
    }

    absl::StatusOr<std::string> machine_devpath =
        id_assigner_->IdForRedfishLocationInDataSet(data_set, is_root);
    if (machine_devpath.ok()) {
      data_set.mutable_decorators()->set_machine_devpath(
          machine_devpath.value());
      return absl::OkStatus();
    }

    // We reach here if we cannot derive machine devpath using Redfish Stable id
    // - PartLocationContext + ServiceLabel. We will now try to map a local
    // devpath to machine devpath
    if (!data_set.has_devpath()) return absl::OkStatus();
    machine_devpath = id_assigner_->IdForLocalDevpathInDataSet(data_set);
    if (machine_devpath.ok()) {
      data_set.mutable_decorators()->set_machine_devpath(
          machine_devpath.value());
      return absl::OkStatus();
    }
    return absl::OkStatus();
  }

 private:
  std::unique_ptr<IdAssigner> id_assigner_;
  std::unique_ptr<LocalIdMapT> local_id_map_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_NORMALIZER_H_
