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

#include "ecclesia/magent/redfish/indus/pcie_function.h"

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "ecclesia/lib/io/pci/config.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/pci.h"
#include "ecclesia/lib/io/pci/signature.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/redfish/indus/pcie_device_collection.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "json/value.h"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

void PCIeFunction::Get(
    tensorflow::serving::net_http::ServerRequestInterface *req,
    const ParamsType &params) {
  if (params.size() != 2 || !absl::holds_alternative<std::string>(params[0]) ||
      !absl::holds_alternative<int>(params[1])) {
    req->ReplyWithStatus(
        tensorflow::serving::net_http::HTTPStatusCode::BAD_REQUEST);
    return;
  }

  // params[0] is expected to be "<domain>:<bus>:<device>". We concatenate the
  // params[1] to form a PCI location string.
  std::string pci_domain_bus_dev = std::get<std::string>(params[0]);
  int pci_func = std::get<int>(params[1]);
  std::string pcie_location_str =
      absl::StrCat(pci_domain_bus_dev, ".", pci_func);
  auto pcie_location = PciLocation::FromString(pcie_location_str);
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
  json[kOdataType] = "#PCIeFunction.v1_2_3.PCIeFunction";
  json[kOdataId] = std::string(req->uri_path());
  json[kOdataContext] = "/redfish/v1/$metadata#PCIeFunction.PCIeFunction";
  json[kId] = std::to_string(pci_func);
  json[kFunctionId] = pci_func;
  // Hardcode the function type to physical, as we don't support virtual PCIe
  // function in Indus.
  json[kFunctionType] = "Physical";
  json[kName] = pcie_location_str;

  absl::StatusOr<PciBaseSignature> base_signature =
      pcie_dev->ConfigSpace()->BaseSignature();
  if (base_signature.ok()) {
    json[kDeviceId] =
        absl::StrFormat("0x%04X", base_signature->device_id().value());
    json[kVendorId] =
        absl::StrFormat("0x%04X", base_signature->vendor_id().value());
  }

  absl::StatusOr<PciSubsystemSignature> subsys_signature =
      pcie_dev->ConfigSpace()->SubsystemSignature();
  if (subsys_signature.ok()) {
    json[kSubsystemId] =
        absl::StrFormat("0x%04X", subsys_signature->id().value());
    json[kSubsystemVendorId] =
        absl::StrFormat("0x%04X", subsys_signature->vendor_id().value());
  }

  absl::StatusOr<uint32_t> class_code = pcie_dev->ConfigSpace()->ClassCode();
  if (class_code.ok()) {
    json[kClassCode] = absl::StrFormat("0x%06X", *class_code);
  }

  // Add domain:bus:device.func info
  auto *oem = GetJsonObject(&json, kOem);
  auto *google = GetJsonObject(oem, kGoogle);
  auto *pci_location = GetJsonObject(google, kPciLocation);
  (*pci_location)[kPciLocationDomain] =
      absl::StrFormat("0x%04X", pcie_location->domain().value());
  (*pci_location)[kPciLocationBus] =
      absl::StrFormat("0x%02X", pcie_location->bus().value());
  (*pci_location)[kPciLocationDevice] =
      absl::StrFormat("0x%02X", pcie_location->device().value());
  (*pci_location)[kPciLocationFunction] =
      absl::StrFormat("0x%X", pcie_location->function().value());

  // Add link to the parent PCIeDevice.
  auto *links = GetJsonObject(&json, kLinks);
  auto *pcie_device = GetJsonObject(links, kPCIeDevice);
  (*pcie_device)[kOdataId] = absl::Substitute("$0/$1/$2", kComputerSystemUri,
                                              kPCIeDevices, pci_domain_bus_dev);

  // Add topology info (links to upstream and downstream PCIeFunctions)
  absl::StatusOr<SystemModel::PciNodeConnections> pcie_connections =
      system_model_->GetPciNodeConnections(*pcie_location);
  if (pcie_connections.ok()) {
    auto *links_oem = GetJsonObject(links, kOem);
    auto *links_oem_google = GetJsonObject(links_oem, kGoogle);
    if (pcie_connections->parent.has_value()) {
      PciDeviceLocation pcie_dev_id(*pcie_connections->parent);
      auto *upstream = GetJsonObject(links_oem_google, kUpstreamPCIeFunction);
      (*upstream)[kOdataId] =
          absl::Substitute("$0/$1/$2/$3/$4", kComputerSystemUri, kPCIeDevices,
                           pcie_dev_id.ToString(), kPCIeFunctions,
                           pcie_connections->parent->function().value());
    }
    auto *downstream = GetJsonArray(links_oem_google, kDownsteamPCIeFunctions);
    for (const auto &child : pcie_connections->children) {
      PciDeviceLocation pcie_dev_id(child);
      Json::Value pcie_func_link;
      pcie_func_link[kOdataId] = absl::Substitute(
          "$0/$1/$2/$3/$4", kComputerSystemUri, kPCIeDevices,
          pcie_dev_id.ToString(), kPCIeFunctions, child.function().value());
      downstream->append(pcie_func_link);
    }
  }

  JSONResponseOK(json, req);
}

}  // namespace ecclesia
