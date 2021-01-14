/*
 * Copyright 2020 Google LLC
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

#include "ecclesia/magent/redfish/indus/storage.h"

#include <bitset>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/magent/lib/nvme/smart_log_page.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/sysmodel/x86/nvme.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "json/value.h"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {
namespace {

// From NVME Spec revision 1.3, below is the bit mask of the "critical warning"
// attribute
// [0] spare_below_threshold
// [1] critical_temperature_warning
// [2] reliability_degraded
// [3] in_read_only_mode
// [4] backup_device_failed
// [5] pmr_unreliable
// There is no corresponding field of critical_temperature_warning in the
// Redfish standard NVMeSMARTCriticalWarnings
void AddRedfishNvmeSmartCriticalWarnings(uint8_t critical_warning_attribute,
                                         Json::Value *json_critical_warnings) {
  std::bitset<8> critical_warning(critical_warning_attribute);
  (*json_critical_warnings)[kSpareCapacityWornOut] =
      static_cast<bool>(critical_warning[0]);
  (*json_critical_warnings)[kOverallSubsystemDegraded] =
      static_cast<bool>(critical_warning[2]);
  (*json_critical_warnings)[kMediaInReadOnly] =
      static_cast<bool>(critical_warning[3]);
  (*json_critical_warnings)[kPowerBackupFailed] =
      static_cast<bool>(critical_warning[4]);
  (*json_critical_warnings)[kPMRUnreliable] =
      static_cast<bool>(critical_warning[5]);
}

}  // namespace

void Storage::Get(tensorflow::serving::net_http::ServerRequestInterface *req,
                  const ParamsType &params) {
  std::string storage_index = std::get<std::string>(params[0]);
  Json::Value json;
  json[kOdataType] = "#Storage.v1_9_0.Storage";
  json[kOdataId] = std::string(req->uri_path());
  json[kOdataContext] = "/redfish/v1/$metadata#Storage.Storage";
  auto nvme_plugin = system_model_->GetNvmeByPhysLocation(storage_index);
  if (!nvme_plugin.has_value()) {
    req->ReplyWithStatus(
        tensorflow::serving::net_http::HTTPStatusCode::NOT_FOUND);
    return;
  }
  // Fill in the json response
  json[kName] = nvme_plugin->access_intf->GetKernelName();
  AddNvmeStorageControllers(&json, req->uri_path(), *nvme_plugin);
  AddDrives(&json, req->uri_path());
  JSONResponseOK(json, req);
}

void Storage::AddNvmeStorageControllers(
    Json::Value *json, absl::string_view url,
    const SystemModel::NvmePluginInfo &nvme_plugin) {
  auto *storage_controllers = GetJsonArray(json, kStorageControllers);
  Json::Value storage_controller;
  storage_controller[kOdataId] = absl::StrCat(url, "#/StorageControllers/0");
  storage_controller[kMemberId] = 0;
  // Add supported controller protocol
  auto *controller_protocols =
      GetJsonArray(&storage_controller, kSupportedControllerProtocols);
  controller_protocols->append("PCIe");
  // Add supported device protocol
  auto *device_protocols =
      GetJsonArray(&storage_controller, kSupportedDeviceProtocols);
  device_protocols->append("NVMe");
  // Add link to PCIe function.
  auto *links = GetJsonObject(&storage_controller, kLinks);
  auto *pcie_functions = GetJsonArray(links, kPCIeFunctions);
  Json::Value pcie_func_link;
  const PciLocation &pci_location = nvme_plugin.location.pci_location;
  pcie_func_link[kOdataId] =
      absl::StrFormat("%s/%s/%04x:%02x:%02x/%s/%x", kComputerSystemUri,
                      kPCIeDevices, pci_location.domain().value(),
                      pci_location.bus().value(), pci_location.device().value(),
                      kPCIeFunctions, pci_location.function().value());
  pcie_functions->append(pcie_func_link);

  // Add NVMe SMART attributes
  auto smart_log = nvme_plugin.access_intf->SmartLog();
  if (smart_log.ok()) {
    auto *nvme_ctlr_properties =
        GetJsonObject(&storage_controller, kNvmeControllerProperties);
    // Decomposite the "critical warning" attributes into various boolean fields
    // in NVMeSMARTCriticalWarnings.
    auto *json_critical_warnings =
        GetJsonObject(nvme_ctlr_properties, kNvmeSmartCriticalWarnings);
    uint8_t critical_warning = (*smart_log)->critical_warning();
    AddRedfishNvmeSmartCriticalWarnings(critical_warning,
                                        json_critical_warnings);

    // Add the rest of the SMART attributes in OEM fields.
    auto *oem = GetJsonObject(nvme_ctlr_properties, kOem);
    auto *google = GetJsonObject(oem, kGoogle);
    auto *smart_attributes = GetJsonObject(google, kSmartAttributes);
    (*smart_attributes)[kCriticalWarning] = critical_warning;
    (*smart_attributes)[kAvailableSparePercent] =
        (*smart_log)->available_spare();
    (*smart_attributes)[kAvailableSparePercentThreshold] =
        (*smart_log)->available_spare_threshold();
    (*smart_attributes)[kCriticalTemperatureTimeMinute] =
        (*smart_log)->critical_comp_time_minutes();
    (*smart_attributes)[kCompositeTemperatureKelvins] =
        (*smart_log)->composite_temperature_kelvins();
    (*smart_attributes)[kPercentageUsed] = (*smart_log)->percent_used();
  }

  storage_controllers->append(storage_controller);
}

}  // namespace ecclesia
