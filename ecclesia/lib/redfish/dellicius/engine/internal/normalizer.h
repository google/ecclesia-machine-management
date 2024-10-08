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

#include <stdbool.h>

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
                         SubqueryDataSet &data_set,
                         const NormalizerOptions &normalizer_options) override;

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
                         SubqueryDataSet &data_set,
                         const NormalizerOptions &normalizer_options) override;

  absl::StatusOr<const NodeTopology *> GetNodeTopology() override {
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
  NormalizerImplAddMachineBarepath(
      std::unique_ptr<LocalIdMapT> local_id_map,
      std::unique_ptr<IdAssigner> id_assigner,
      bool use_local_devpath_for_machine_devpath = false)
      : local_id_map_(std::move(local_id_map)),
        id_assigner_(std::move(id_assigner)),
        use_local_devpath_for_machine_devpath_(
            use_local_devpath_for_machine_devpath) {}

 protected:
  absl::Status Normalize(const RedfishObject &redfish_object,
                         const DelliciusQuery::Subquery &subquery,
                         SubqueryDataSet &data_set,
                         const NormalizerOptions &normalizer_options) override {
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

    // Assign the embedded location context for sub-fru resources. As this will
    // be used for unique id purposes.
    if (data_set.has_redfish_location() &&
        !data_set.redfish_location().embedded_location_context().empty()) {
      data_set.mutable_decorators()->set_embedded_location_context(
          data_set.redfish_location().embedded_location_context());
    }

    // Try to find and set a machine devpath, prioritizing local devpath to
    // machine devpath mappings from UHMM when desired.
    absl::StatusOr<std::string> machine_devpath;
    if (use_local_devpath_for_machine_devpath_) {
      if (!data_set.has_devpath()) return absl::OkStatus();
      machine_devpath = id_assigner_->IdForLocalDevpathInDataSet(data_set);
    } else {
      machine_devpath =
          id_assigner_->IdForRedfishLocationInDataSet(data_set, is_root);
      // Attempt to fallback to local devpath to derive machine devpath if we
      // cannot find a machine devpath using redfish location properties.
      if (!machine_devpath.ok() && data_set.has_devpath()) {
        machine_devpath = id_assigner_->IdForLocalDevpathInDataSet(data_set);
      }
    }
    if (machine_devpath.ok()) {
      data_set.mutable_decorators()->set_machine_devpath(
          machine_devpath.value());
    }
    return absl::OkStatus();
  }

 private:
  std::unique_ptr<LocalIdMapT> local_id_map_;
  std::unique_ptr<IdAssigner> id_assigner_;
  bool use_local_devpath_for_machine_devpath_ = false;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_NORMALIZER_H_
