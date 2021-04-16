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

#include "ecclesia/magent/redfish/common/pcie_device.h"

#include <cstdint>
#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "ecclesia/lib/io/pci/config.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/pci.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "json/value.h"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {
namespace {

// A helper function to convert the PcieLinkSpeed enum to Redfish PCIeType enum
// string.
std::string PcieLinkSpeedToType(PcieLinkSpeed speed) {
  switch (speed) {
    case PcieLinkSpeed::kGen1Speed2500MT:
      return "Gen1";
    case PcieLinkSpeed::kGen2Speed5GT:
      return "Gen2";
    case PcieLinkSpeed::kGen3Speed8GT:
      return "Gen3";
    case PcieLinkSpeed::kGen4Speed16GT:
      return "Gen4";
    case PcieLinkSpeed::kGen5Speed32GT:
      return "Gen5";
    default:
      return "";
  }
}

}  // namespace

void PCIeDevice::Get(tensorflow::serving::net_http::ServerRequestInterface *req,
                     const ParamsType &params) {
  if (params.size() != 1 || !absl::holds_alternative<std::string>(params[0])) {
    req->ReplyWithStatus(
        tensorflow::serving::net_http::HTTPStatusCode::BAD_REQUEST);
    return;
  }
  auto pcie_name = std::get<std::string>(params[0]);
  // pcie_name is expected to be "<domain>:<bus>:<device>". A PCIe device should
  // always has a function of 0. Therefore, we append the ".0" to form a
  // complete PCI location.
  std::string pcie_location_str = absl::StrCat(pcie_name, ".0");
  auto pcie_location = PciDbdfLocation::FromString(pcie_location_str);
  if (!pcie_location.has_value()) {
    req->ReplyWithStatus(
        tensorflow::serving::net_http::HTTPStatusCode::BAD_REQUEST);
    return;
  }
  auto pcie_dev = system_model_->GetPciDevice(*pcie_location);
  if (pcie_dev == nullptr) {
    req->ReplyWithStatus(
        tensorflow::serving::net_http::HTTPStatusCode::NOT_FOUND);
    return;
  }

  Json::Value json;
  json[kOdataType] = "#PCIeDevice.v1_5_0.PCIeDevice";
  json[kOdataId] = std::string(req->uri_path());
  json[kOdataContext] = "/redfish/v1/$metadata#PCIeDevice.PCIeDevice";
  json[kDeviceType] = "SingleFunction";
  json[kId] = pcie_name;
  json[kName] = std::get<std::string>(params[0]);

  auto link_caps = pcie_dev->PcieLinkCurrentCapabilities();
  auto link_max_caps = pcie_dev->PcieLinkMaxCapabilities();
  auto *pcie_interface = GetJsonObject(&json, kPCIeInterface);
  if (link_caps.ok()) {
    (*pcie_interface)[kPCIeType] = PcieLinkSpeedToType(link_caps->speed);
    (*pcie_interface)[kLanesInUse] = PcieLinkWidthToInt(link_caps->width);
  }
  if (link_max_caps.ok()) {
    (*pcie_interface)[kMaxPCIeType] = PcieLinkSpeedToType(link_max_caps->speed);
    (*pcie_interface)[kMaxLanes] = PcieLinkWidthToInt(link_max_caps->width);
  }

  auto *links = GetJsonObject(&json, kLinks);
  auto *pcie_functions = GetJsonArray(links, kPCIeFunctions);
  // Enumerate the PCIe functions and add valid links.
  for (uint8_t pci_func = 0; pci_func < 8; ++pci_func) {
    std::string pcie_location_str = absl::StrCat(pcie_name, ".", pci_func);
    auto pcie_location = PciDbdfLocation::FromString(pcie_location_str);
    if (!pcie_location.has_value()) {
      continue;
    }
    auto pcie_dev = system_model_->GetPciDevice(*pcie_location);
    // The PCI func is enumerated from smaller to larger number. If any one is
    // nullptr, there's no need to try larger pci_func.
    if (pcie_dev == nullptr) {
      break;
    }
    Json::Value pcie_func_link;
    pcie_func_link[kOdataId] =
        absl::Substitute("$0/$1/$2", req->uri_path(), kPCIeFunctions, pci_func);
    pcie_functions->append(pcie_func_link);
  }

  JSONResponseOK(json, req);
}

}  // namespace ecclesia
