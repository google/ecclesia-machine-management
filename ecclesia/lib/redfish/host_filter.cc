/*
 * Copyright 2023 Google LLC
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

#include "ecclesia/lib/redfish/host_filter.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/sysmodel.h"

namespace ecclesia {

RedfishObjectHostFilter::RedfishObjectHostFilter(
    absl::flat_hash_map<std::string, std::string> system_to_host_domain_map,
    Sysmodel &sysmodel)
    : system_name_host_domain_map_(std::move(system_to_host_domain_map)) {
  // Go through all computer system and create a map that maps system uri to
  // host domain.
  auto computer_system_func =
      [this](
          std::unique_ptr<RedfishObject> system_obj) -> RedfishIterReturnValue {
    std::optional<std::string> property_name =
        system_obj->GetNodeValue<PropertyName>();
    if (property_name.has_value()) {
      std::optional<std::string> uri_string = system_obj->GetUriString();
      if (auto os_domain_iter =
              system_name_host_domain_map_.find(property_name.value());
          uri_string.has_value() &&
          os_domain_iter != system_name_host_domain_map_.end()) {
        if (auto [it, inserted] = uri_host_domain_map_.insert(
                {uri_string.value(), os_domain_iter->second});
            !inserted) {
          DLOG(WARNING) << "Failed to insert [uri, host domain] set: "
                        << uri_string.value() << ", " << os_domain_iter->second;
        }
      }
    }
    return RedfishIterReturnValue::kContinue;
  };
  sysmodel.QueryAllResources<ResourceComputerSystem>(computer_system_func);
  for (const auto &[system_name, host_domain] : system_name_host_domain_map_) {
    if (auto [it, inserted] = host_domain_set_.insert(host_domain); !inserted) {
      DLOG(WARNING) << "Failed to insert host domain set: " << host_domain;
    }
  }

  // Go through all chassis to see if there are any chassis that is associated
  // with a host.
  auto chassis_func = [this](std::unique_ptr<RedfishObject> chassis_obj)
      -> RedfishIterReturnValue {
    std::optional<std::string> chassis_uri_string = chassis_obj->GetUriString();
    if (!chassis_uri_string.has_value()) {
      return RedfishIterReturnValue::kContinue;
    }
    if (std::unique_ptr<RedfishIterable> compsys_itr =
            (*chassis_obj)[kRfPropertyLinks][kRfPropertyComputerSystems]
                .AsIterable();
        compsys_itr != nullptr) {
      if (compsys_itr->Size() == 1) {
        std::unique_ptr<RedfishObject> system_obj =
            (*compsys_itr)[0].AsObject();
        std::optional<std::string> system_uri_string =
            system_obj->GetUriString();
        if (system_uri_string.has_value() &&
            uri_host_domain_map_.contains(*system_uri_string)) {
          if (auto [it, inserted] = uri_host_domain_map_.insert(
                  {chassis_uri_string.value(),
                   uri_host_domain_map_.at(system_uri_string.value())});
              !inserted) {
            DLOG(WARNING) << "Failed to insert [uri, host domain] set: "
                          << chassis_uri_string.value() << ", "
                          << uri_host_domain_map_.at(system_uri_string.value());
          }
        }
      } else {
        LOG(INFO)
            << "Chassis " << chassis_uri_string.value()
            << " is shared by multiple computer system, skipping map adding."
               "creation.";
      }
    }
    return RedfishIterReturnValue::kContinue;
  };
  sysmodel.QueryAllResources<ResourceChassis>(chassis_func);
}

absl::StatusOr<absl::string_view> RedfishObjectHostFilter::GetHostDomainForObj(
    const RedfishObject &obj) {
  if (host_domain_set_.size() == 1) {
    return *host_domain_set_.begin();
  }
  // If the object is under /redfish/v1/Systems/{id}, this section will return
  // the correct host domain.
  std::optional<std::string> obj_uri = obj.GetUriString();
  if (!obj_uri.has_value()) {
    return absl::NotFoundError("No uri return for object");
  }
  if (absl::StatusOr<absl::string_view> host_domain =
          GetHostDomainFromUri(*obj_uri);
      host_domain.ok()) {
    return *host_domain;
  }
  // For objects with RelatedItem like sensors, events and LogEntry, this
  // section will return the correct host_domain by RelatedItem.
  if (std::optional<std::string> additional_uri =
          obj.GetNodeValue<PropertyAdditionalDataURI>();
      additional_uri.has_value()) {
    if (absl::StatusOr<absl::string_view> host_domain =
            GetHostDomainFromUri(*additional_uri);
        host_domain.ok()) {
      return *host_domain;
    }
  }
  // If the object is a downstream chassis, this section will return the correct
  // host_domain by the downstream chassis link.
  if (std::unique_ptr<RedfishIterable> downstream_chassis_itr =
          (obj)[kRfPropertyLinks][kRfPropertyDownstreamChassis].AsIterable();
      downstream_chassis_itr != nullptr) {
    if (downstream_chassis_itr->Size() == 1) {
      std::unique_ptr<RedfishObject> downstream_chassis_obj =
          (*downstream_chassis_itr)[0].AsObject();
      if (downstream_chassis_obj != nullptr) {
        return GetHostDomainForObj(*downstream_chassis_obj);
      }
    }
  }
  return absl::NotFoundError(
      absl::StrCat("Can't find a host domain for object: ", *obj_uri));
}

absl::StatusOr<absl::string_view> RedfishObjectHostFilter::GetHostDomainFromUri(
    absl::string_view uri) {
  if (host_domain_set_.size() == 1) {
    return *host_domain_set_.begin();
  }
  // If this is a multi-host system, anything under /redfish/v1/Systems/{id}...
  // should assotiate to a host domain.
  std::vector<absl::string_view> path_components = absl::StrSplit(uri, '/');
  if (path_components.size() > 4 && path_components[1] == "redfish" &&
      path_components[2] == "v1" && path_components[3] == "Systems") {
    absl::string_view system_name = path_components[4];
    std::string system_uri = absl::StrCat("/redfish/v1/Systems/", system_name);
    if (auto iter = uri_host_domain_map_.find(system_uri);
        iter != uri_host_domain_map_.end()) {
      return iter->second;
    }
  }
  // If this is a multi-host chassis, things under /redfish/v1/Chassis/{id}...
  // might assotiate to a host domain.
  if (path_components.size() > 4 && path_components[1] == "redfish" &&
      path_components[2] == "v1" && path_components[3] == "Chassis") {
    absl::string_view chassis_name = path_components[4];
    std::string system_uri = absl::StrCat("/redfish/v1/Chassis/", chassis_name);
    if (auto iter = uri_host_domain_map_.find(system_uri);
        iter != uri_host_domain_map_.end()) {
      return iter->second;
    }
  }
  return absl::NotFoundError(
      absl::StrCat("No host domain found for uri: ", uri));
}
}  // namespace ecclesia
