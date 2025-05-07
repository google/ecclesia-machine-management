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

#ifndef ECCLESIA_LIB_REDFISH_HOST_FILTER_H_
#define ECCLESIA_LIB_REDFISH_HOST_FILTER_H_

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/sysmodel.h"

namespace ecclesia {

// A Filter class that handles a host filter feature for a Redfish object. In
// multi-host machines, we need some filter for every Redfish object so that we
// could find the associate host domain.
class RedfishObjectHostFilter {
 public:
  // Constructor for single host Redfish backend.
  RedfishObjectHostFilter(absl::string_view single_host_domain) {
    host_domain_set_.insert(std::string(single_host_domain));
  }

  // Constructor for multi host Redfish backend.
  RedfishObjectHostFilter(
      absl::flat_hash_map<std::string, std::string> system_to_host_domain_map,
      Sysmodel &sysmodel);

  void UpdateUriHostDomainMap(Sysmodel &sysmodel);

  absl::StatusOr<absl::string_view> GetHostDomainForObj(
      const RedfishObject &obj);
  // Returns a map with computer system name as keys and the host domain name as
  // values.
  const absl::flat_hash_map<std::string, std::string> &
  GetSystemHostDomainNameMap() {
    return system_name_host_domain_map_;
  }
  // Returns a set with all host domain.
  const absl::flat_hash_set<std::string> &GetMultiHostOsDomainSet() {
    return host_domain_set_;
  }
  // Given a Redfish URI, returns the associated host domain if available.
  absl::StatusOr<absl::string_view> GetHostDomainFromUri(absl::string_view uri);

 private:
  // A set that collects all host domains.
  absl::flat_hash_set<std::string> host_domain_set_;

  // A map that specifies the relationship between system name and host domain.
  absl::flat_hash_map<std::string, std::string> system_name_host_domain_map_;

  // A map that specifies the relationship between system uri and host domain.
  absl::flat_hash_map<std::string, std::string> uri_host_domain_map_;
};
}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_HOST_FILTER_H_
