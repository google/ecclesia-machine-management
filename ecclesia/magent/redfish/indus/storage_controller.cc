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

#include "ecclesia/magent/redfish/indus/storage_controller.h"

#include <stdint.h>

#include <bitset>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
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

void AddStaticFields(Json::Value *json,
                     absl::string_view uri) {
  (*json)[kOdataId] = std::string(uri);
  (*json)[kOdataType] = "#StorageController.v1_1_0.StorageController";
  (*json)[kOdataContext] =
      "/redfish/v1/$metadata#StorageController.StorageController";
  (*json)[kId] = "0";
  (*json)[kName] = "NVMe Controller";
}

// Add link to PCIe function.
void AddLinks(Json::Value *json,
              const SystemModel::NvmePluginInfo &nvme_plugin) {
  if (!json) return;

  auto *links = GetJsonObject(json, kLinks);
  auto *pcie_functions = GetJsonArray(links, kPCIeFunctions);
  Json::Value pcie_func_link;
  const PciDbdfLocation &pci_location = nvme_plugin.location.pci_location;
  pcie_func_link[kOdataId] =
      absl::StrFormat("%s/%s/%04x:%02x:%02x/%s/%x", kComputerSystemUri,
                      kPCIeDevices, pci_location.domain().value(),
                      pci_location.bus().value(), pci_location.device().value(),
                      kPCIeFunctions, pci_location.function().value());
  pcie_functions->append(pcie_func_link);
}

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
void AddNvmeSmartCriticalWarnings(Json::Value *json_critical_warnings,
                                  uint8_t critical_warning_attribute) {
  if (!json_critical_warnings) return;

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

void AddNvmeControllerProperties(Json::Value *json,
                                 uint8_t critical_warning_attribute) {
  if (!json) return;

  auto *nvme_ctlr_properties = GetJsonObject(json, kNvmeControllerProperties);
  auto *json_critical_warnings =
      GetJsonObject(nvme_ctlr_properties, kNvmeSmartCriticalWarnings);
  AddNvmeSmartCriticalWarnings(json_critical_warnings,
                               critical_warning_attribute);
}

void AddSmartAttributes(Json::Value *json,
                        ecclesia::SmartLogPageInterface *smart_log) {
  if (!json || !smart_log) return;
  // Add the rest of the SMART attributes in OEM fields.
  auto *oem = GetJsonObject(json, kOem);
  auto *google = GetJsonObject(oem, kGoogle);
  auto *smart_attributes = GetJsonObject(google, kSmartAttributes);
  (*smart_attributes)[kCriticalWarning] = smart_log->critical_warning();
  (*smart_attributes)[kAvailableSparePercent] = smart_log->available_spare();
  (*smart_attributes)[kAvailableSparePercentThreshold] =
      smart_log->available_spare_threshold();
  (*smart_attributes)[kCriticalTemperatureTimeMinute] =
      smart_log->critical_comp_time_minutes();
  (*smart_attributes)[kCompositeTemperatureKelvins] =
      smart_log->composite_temperature_kelvins();
  (*smart_attributes)[kPercentageUsed] = smart_log->percent_used();
}

}  // namespace

void StorageController::Get(
    tensorflow::serving::net_http::ServerRequestInterface *req,
    const ParamsType &params) {
  std::string storage_index = std::get<std::string>(params[0]);
  auto nvme_plugin = system_model_->GetNvmeByPhysLocation(storage_index);
  if (!nvme_plugin.has_value()) {
    req->ReplyWithStatus(
        tensorflow::serving::net_http::HTTPStatusCode::NOT_FOUND);
    return;
  }
  Json::Value json;
  AddStaticFields(&json, req->uri_path());
  AddLinks(&json, *nvme_plugin);

  auto smart_log = nvme_plugin->access_intf->SmartLog();
  if (smart_log.ok()) {
    AddNvmeControllerProperties(&json, (*smart_log)->critical_warning());
    AddSmartAttributes(&json, (*smart_log).get());
  }

  // Add supported controller protocol
  auto *controller_protocols =
      GetJsonArray(&json, kSupportedControllerProtocols);
  controller_protocols->append("PCIe");
  // Add supported device protocol
  auto *device_protocols = GetJsonArray(&json, kSupportedDeviceProtocols);
  device_protocols->append("NVMe");

  JSONResponseOK(json, req);
}

}  // namespace ecclesia
