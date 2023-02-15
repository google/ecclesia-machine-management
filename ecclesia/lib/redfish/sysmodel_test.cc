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

#include "ecclesia/lib/redfish/sysmodel.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {
namespace {

using ::tensorflow::serving::net_http::ServerRequestInterface;
using ::testing::ElementsAre;

class SysmodelTest : public testing::Test {
 public:
  SysmodelTest() = default;

  void InitServer(absl::string_view mockup_path) {
    mockup_server_ = std::make_unique<FakeRedfishServer>(mockup_path);
    intf_ = mockup_server_->RedfishClientInterface();
    sysmodel_ = std::make_unique<Sysmodel>(intf_.get());
    mockup_server_->EnableExpandGetHandler();
  }

  static void SendJsonHttpResponse(ServerRequestInterface *req,
                                   absl::string_view json) {
    ::tensorflow::serving::net_http::SetContentType(req, "application/json");
    req->OverwriteResponseHeader("OData-Version", "4.0");
    req->WriteResponseString(json);
    req->Reply();
  }

 protected:
  std::unique_ptr<FakeRedfishServer> mockup_server_;
  std::unique_ptr<RedfishInterface> intf_;
  std::unique_ptr<Sysmodel> sysmodel_;
  std::string root_with_expands_;
};

TEST_F(SysmodelTest, GetResourceSystemExpands) {
  int expanded_request_count = 0;
  InitServer("topology_v2_testing/mockup.shar");
  // Store original json
  auto systems_json =
      intf_->CachedGetUri("/redfish/v1/Systems").AsObject()->DebugString();
  mockup_server_->AddHttpGetHandler("/redfish/v1/Systems?$expand=.($levels=1)",
                                    [&](ServerRequestInterface *req) {
                                      expanded_request_count++;
                                      SendJsonHttpResponse(req, systems_json);
                                    });
  sysmodel_->QueryAllResources<ResourceSystem>(
      [&](std::unique_ptr<RedfishObject>) -> RedfishIterReturnValue {
        return RedfishIterReturnValue::kStop;
      });
  EXPECT_EQ(expanded_request_count, 1);
}

TEST_F(SysmodelTest, GetResourceDriveDoesntReturnDuplicateDrives) {
  InitServer("topology_v2_testing/mockup.shar");
  // Inject a Chassis Drive resource that duplicates an existing ComputerSystem
  // Drive resource.
  mockup_server_->AddHttpGetHandlerWithData(
      "/redfish/v1/Chassis/child2/Drives?$expand=.($levels=1)",
      R"json({
        "@odata.id": "/redfish/v1/Chassis/child2/Drives",
        "@odata.type": "#DriveCollection.DriveCollection",
        "Members": [
          {
            "@odata.id": "/redfish/v1/Systems/system/Storage/1/Drives/0"
          }
        ],
        "Members@odata.count": 1,
        "Name": "Drive Collection"
      })json");

  std::vector<std::string> returned_uris;
  sysmodel_->QueryAllResources<ResourceDrive>(
      [&](std::unique_ptr<RedfishObject> obj) -> RedfishIterReturnValue {
        std::optional<std::string> uri = obj->GetUriString();
        if (uri.has_value()) {
          returned_uris.push_back(*std::move(uri));
        }
        return RedfishIterReturnValue::kContinue;
      });
  EXPECT_THAT(returned_uris,
              ElementsAre("/redfish/v1/Systems/system/Storage/1/Drives/0"));
}

TEST_F(SysmodelTest, GetResourceProcessorExpands) {
  int expand_processor_count = 0;
  InitServer("topology_v2_testing/mockup.shar");
  // Store original json
  auto processor_json =
      intf_->CachedGetUri("/redfish/v1/Systems/system/Processors")
          .AsObject()
          ->DebugString();
  mockup_server_->AddHttpGetHandler(
      "/redfish/v1/Systems/system/Processors?$expand=.($levels=1)",
      [&](ServerRequestInterface *req) {
        expand_processor_count++;
        SendJsonHttpResponse(req, processor_json);
      });
  sysmodel_->QueryAllResources<ResourceProcessor>(
      [&](std::unique_ptr<RedfishObject>) -> RedfishIterReturnValue {
        return RedfishIterReturnValue::kStop;
      });
  EXPECT_EQ(expand_processor_count, 1);
}

TEST_F(SysmodelTest, GetResourcePhysicalLpuExpands) {
  int expand_processor_count = 0;
  InitServer("topology_v2_testing/mockup.shar");
  // Store original json
  auto processor_json =
      intf_->CachedGetUri("/redfish/v1/Systems/system/Processors")
          .AsObject()
          ->DebugString();
  mockup_server_->AddHttpGetHandler(
      "/redfish/v1/Systems/system/Processors?$expand=.($levels=3)",
      [&](ServerRequestInterface *req) {
        expand_processor_count++;
        SendJsonHttpResponse(req, processor_json);
      });
  sysmodel_->QueryAllResources<AbstractionPhysicalLpu>(
      [&](std::unique_ptr<RedfishObject>) -> RedfishIterReturnValue {
        return RedfishIterReturnValue::kStop;
      });
  EXPECT_EQ(expand_processor_count, 1);
}

}  // namespace
}  // namespace ecclesia
