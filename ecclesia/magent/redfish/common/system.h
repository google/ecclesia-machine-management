/*
 * Copyright 2021 Google LLC
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

#ifndef ECCLESIA_MAGENT_REDFISH_COMMON_SYSTEM_H_
#define ECCLESIA_MAGENT_REDFISH_COMMON_SYSTEM_H_

#include <optional>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/magent/redfish/common/pcie_device_collection.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

class ComputerSystem : public Resource {
 public:
  ComputerSystem(SystemModel *system_model, const std::string system_name)
      : Resource(kComputerSystemUri),
        system_model_(system_model),
        system_name_(std::move(system_name)) {}

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &params) override {
    nlohmann::json json;
    json[kOdataType] = "#ComputerSystem.v1_8_0.ComputerSystem";
    json[kOdataId] = std::string(Uri());
    json[kOdataContext] = "/redfish/v1/$metadata#ComputerSystem.ComputerSystem";

    json[kName] = system_name_;
    json[kId] = "system";
    json[kLogServices][kOdataId] = kLogServicesUri;

    auto *memory = GetJsonObject(&json, kMemory);
    (*memory)[kOdataId] = absl::StrCat(Uri(), "/", kMemory);
    auto *processors = GetJsonObject(&json, kProcessors);
    (*processors)[kOdataId] = absl::StrCat(Uri(), "/", kProcessors);
    auto *storages = GetJsonObject(&json, kStorage);
    (*storages)[kOdataId] = absl::StrCat(Uri(), "/", kStorage);

    // Add the PCIe Devices which is an array of links of the PCIeDevice
    // resources.
    nlohmann::json pcie_devices = GetPcieDeviceUrlsAsJsonArray(system_model_);
    json[kPCIeDevices] = pcie_devices;

    auto *oem = GetJsonObject(&json, kOem);
    auto *google = GetJsonObject(oem, kGoogle);
    // Read boot number from the System Model and return if available
    auto maybe_boot_number = system_model_->GetBootNumber();
    if (maybe_boot_number.has_value()) {
      (*google)[kBootNumber] = maybe_boot_number.value();
    }
    auto sys_uptime = system_model_->GetSystemUptimeSeconds();
    if (sys_uptime.ok()) {
      (*google)[kSystemUptime] = sys_uptime.value();
    }

    auto total_memory_size = system_model_->GetSystemTotalMemoryBytes();
    if (total_memory_size.ok()) {
      auto *memory_summary = GetJsonObject(&json, kMemorySummary);
      (*memory_summary)[kTotalSystemMemoryGiB] =
          total_memory_size.value() >> 30;
    }

    JSONResponseOK(json, req);
  }

  SystemModel *const system_model_;
  const std::string system_name_;
};
}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_COMMON_SYSTEM_H_
