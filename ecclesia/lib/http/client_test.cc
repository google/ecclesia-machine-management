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

#include "ecclesia/lib/http/client.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

using testing::Eq;

TEST(BodyJson, JsonTestFail) {
  HttpClient::HttpResponse rsp{.code = 0, .body = "fasdfasgd", .headers = {}};
  auto json = rsp.GetBodyJson();
  EXPECT_TRUE(json.is_discarded());
}

TEST(BodyJson, SimpleJsonTest) {
  constexpr char kV1[] = R"json(
    {
    "@odata.id": "/redfish/v1",
    "@odata.type": "#ServiceRoot.v1_5_0.ServiceRoot",
    "AccountService": {
      "@odata.id": "/redfish/v1/AccountService"
    },
    "CertificateService": {
      "@odata.id": "/redfish/v1/CertificateService"
    },
    "Chassis": {
      "@odata.id": "/redfish/v1/Chassis"
    },
    "EventService": {
      "@odata.id": "/redfish/v1/EventService"
    },
    "Id": "RootService",
    "JsonSchemas": {
      "@odata.id": "/redfish/v1/JsonSchemas"
    },
    "Links": {
      "Sessions": {
        "@odata.id": "/redfish/v1/SessionService/Sessions"
      }
    },
    "Managers": {
      "@odata.id": "/redfish/v1/Managers"
    },
    "Name": "Root Service",
    "RedfishVersion": "1.9.0",
    "Registries": {
      "@odata.id": "/redfish/v1/Registries"
    },
    "SessionService": {
      "@odata.id": "/redfish/v1/SessionService"
    },
    "Systems": {
      "@odata.id": "/redfish/v1/Systems"
    },
    "Tasks": {
      "@odata.id": "/redfish/v1/TaskService"
    },
    "UUID": "9f93b55e-f481-4f19-bca1-9a9fc5f13fc8",
    "UpdateService": {
      "@odata.id": "/redfish/v1/UpdateService"
    }}
  )json";

  HttpClient::HttpResponse rsp{.code = 0, .body = kV1, .headers = {}};
  auto json = rsp.GetBodyJson();

  ASSERT_FALSE(json.is_discarded());
  EXPECT_THAT(json, Eq(nlohmann::json::parse(kV1, nullptr,
                                             /*allow_exceptions=*/false)));
}

TEST(BodyJson, NestedJsonTest) {
  constexpr char kSomeRequest[] =
      R"json(
      {"inputs":[
        {
          "context":{
            "locale_language":"en"
          },
          "intent":"action.devices.EXECUTE",
          "payload":{
            "commands":[
              {
                "devices":[
                  {
                    "customData":{
                      "capabilities":{
                        "has_chain":false,
                        "has_color":true,
                        "has_ir":false,
                        "has_multizone":false,
                        "has_variable_color_temp":true,
                        "max_kelvin":9000,
                        "min_kelvin":2500
                      },
                      "serial_number":"d073d521c5ec"
                    },
                    "id":"027ba93d-a8e1-4127-8f66-1c97831153a4"
                  }
                ],
                "execution":[
                  {
                    "command":"action.devices.commands.OnOff",
                    "params":{
                      "on":false
                    }
                  }
                ]
              }
            ]
          }
        }
      ],
      "requestId":"12691536647410961204",
      "ContentLength": 1324
    })json";

  // Test that Json::Value returned from GetBodyJson() outlives HttpReponse.
  nlohmann::json json = nlohmann::json::value_t::discarded;
  HttpClient::HttpResponse rsp{.code = 0, .body = kSomeRequest, .headers = {}};
  json = rsp.GetBodyJson();
  ASSERT_FALSE(json.is_discarded());
  EXPECT_THAT(json, Eq(nlohmann::json::parse(kSomeRequest, nullptr,
                                             /*allow_exceptions=*/false)));
}

}  // namespace
}  // namespace ecclesia
