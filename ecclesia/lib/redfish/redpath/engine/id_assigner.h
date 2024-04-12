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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_ID_ASSIGNER_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_ID_ASSIGNER_H_

#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"

namespace ecclesia {
// Generic interface which assigns an identifier to a QueryResultData output.
class RedpathEngineIdAssigner {
 public:
  virtual ~RedpathEngineIdAssigner() = default;

  // Assign an identifier based on local devpath in the data_set.
  // Return absl::NotFoundError if a translation does not exist.
  virtual absl::StatusOr<std::string> IdForLocalDevpathInDataSet(
      const ecclesia::QueryResultData& data_set) = 0;
};

template <typename LocalIdMapT>
class LocalIdMapBasedDevpathAssigner : public RedpathEngineIdAssigner {
 public:
  LocalIdMapBasedDevpathAssigner(LocalIdMapT local_id_map,
                                 absl::string_view entity_tag)
      : map_(std::move(local_id_map)), entity_tag_(entity_tag) {}

  absl::StatusOr<std::string> IdForLocalDevpathInDataSet(
      const ecclesia::QueryResultData& data_set) override {
    constexpr absl::string_view kLocalDevpath = "__LocalDevpath__";

    QueryResultDataReader reader(&data_set);

    absl::StatusOr<QueryValueReader> query_value_reader =
        reader.Get(kLocalDevpath);
    if (!query_value_reader.ok()) {
      return absl::NotFoundError(
          absl::StrFormat("%s property not found\n", kLocalDevpath));
    }

    if (!(query_value_reader.value().identifier().has_local_devpath()) ||
        (query_value_reader.value().identifier().local_devpath()).empty()) {
      return absl::NotFoundError(
          absl::StrFormat("%s property not found\n", kLocalDevpath));
    }
    std::string local_devpath =
        query_value_reader.value().identifier().local_devpath();

    if (local_devpath[0] != '/') {
      return absl::InvalidArgumentError("devpath doesn't start with /");
    }
    // Strip devpath of the tailing suffix/connector name iteratively trying
    // to locate the "head" part in the
    // map_.node_local_barepath_to_machine_barepath
    // This is required to cover the case when local_devpath tree reported by
    // redfish is not the same as machine_devpath subtree.
    std::string devpath = (local_devpath).substr(0, (local_devpath).find(':'));

    while (!devpath.empty()) {
      typename LocalIdMapT::EntityTagBarepath entity_tag_barepath = {
          .entity_tag = entity_tag_, .barepath = std::string(devpath)};
      auto itr = map_.node_local_barepath_to_machine_barepath.find(
          entity_tag_barepath);
      if (itr != map_.node_local_barepath_to_machine_barepath.end()) {
        return StringReplace((local_devpath), devpath, itr->second, false);
      }
      // Unknown devpath. remove the last connector name and try again
      devpath = devpath.substr(0, devpath.rfind('/'));
    }
    return absl::NotFoundError("");
  }

 private:
  const LocalIdMapT map_;
  const std::string entity_tag_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_ID_ASSIGNER_H_
