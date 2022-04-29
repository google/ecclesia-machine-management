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

#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/thread/thread.h"
#include "ecclesia/lib/time/clock_fake.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::UnorderedElementsAre;

using ::tensorflow::serving::net_http::ServerRequestInterface;
using ::tensorflow::serving::net_http::SetContentType;

TEST(HttpRedfishInterfaceMultithreadedTest, NoMultithreadedIssuesOnGet) {
  // Number of threads to test with.
  static constexpr int kThreads = 5;
  // Number of requests made by each thread.
  static constexpr int kRequestsPerThread = 20;

  // Create the RedfishInterface. Do not use caching in order to exercise the
  // full HTTP stack.
  CurlHttpClient client(LibCurlProxy::CreateInstance(), HttpCredential());
  auto server =
      std::make_unique<FakeRedfishServer>("barebones_session_auth/mockup.shar");
  std::vector<std::unique_ptr<ThreadInterface>> threads;
  auto curl_http_client = std::make_unique<ecclesia::CurlHttpClient>(
      ecclesia::LibCurlProxy::CreateInstance(), ecclesia::HttpCredential());
  auto transport = ecclesia::HttpRedfishTransport::MakeNetwork(
      std::move(curl_http_client),
      absl::StrFormat("%s:%d", server->GetConfig().hostname,
                      server->GetConfig().port));
  auto cache = std::make_unique<ecclesia::NullCache>(transport.get());
  auto intf = NewHttpInterface(std::move(transport), std::move(cache),
                               RedfishInterface::kTrusted);

  // Create the work for each thread.
  auto my_getter_func = [&intf]() {
    for (int req = 0; req < kRequestsPerThread; ++req) {
      auto result = intf->GetRoot();
      ASSERT_TRUE(result.status().ok()) << result.status().message();
      EXPECT_THAT(result.httpcode(), Eq(200));
      nlohmann::json expected =
          nlohmann::json::parse(R"json({
  "@odata.context": "/redfish/v1/$metadata#ServiceRoot.ServiceRoot",
  "@odata.id": "/redfish/v1",
  "@odata.type": "#ServiceRoot.v1_5_0.ServiceRoot",
  "Chassis": {
  "@odata.id": "/redfish/v1/Chassis"
  },
  "Id": "RootService",
  "Links": {
  "Sessions": {
    "@odata.id": "/redfish/v1/SessionService/Sessions"
  }
  },
  "Name": "Root Service",
  "RedfishVersion": "1.6.1"
})json",
                                nullptr, /*allow_exceptions=*/false);
      EXPECT_THAT(nlohmann::json::parse(result.DebugString(), nullptr,
                                        /*allow_exceptions=*/false),
                  Eq(expected));
    }
  };

  // Create a bunch of threads and make them do the same work.
  auto thread_factory = GetDefaultThreadFactory();
  for (int i = 0; i < kThreads; ++i) {
    threads.push_back(thread_factory->New(my_getter_func));
  }
  for (auto &t : threads) {
    t->Join();
  }
}

// Test harness to start a FakeRedfishServer and create a RedfishInterface
// for testing.
class HttpRedfishInterfaceTest : public ::testing::Test {
 protected:
  HttpRedfishInterfaceTest() {
    server_ = std::make_unique<ecclesia::FakeRedfishServer>(
        "barebones_session_auth/mockup.shar");
    auto config = server_->GetConfig();
    ecclesia::HttpCredential creds;
    auto curl_http_client = std::make_unique<ecclesia::CurlHttpClient>(
        ecclesia::LibCurlProxy::CreateInstance(), creds);
    auto transport = ecclesia::HttpRedfishTransport::MakeNetwork(
        std::move(curl_http_client),
        absl::StrFormat("%s:%d", config.hostname, config.port));
    auto cache_factory = [this](RedfishTransport *transport) {
      return std::make_unique<ecclesia::TimeBasedCache>(transport, &clock_,
                                                        absl::Minutes(1));
    };
    auto cache = std::make_unique<ecclesia::TimeBasedCache>(
        transport.get(), &clock_, absl::Minutes(1));
    intf_ = NewHttpInterface(std::move(transport), cache_factory,
                             RedfishInterface::kTrusted);
  }

  ecclesia::FakeClock clock_;
  std::unique_ptr<ecclesia::FakeRedfishServer> server_;
  std::unique_ptr<RedfishInterface> intf_;
};

TEST_F(HttpRedfishInterfaceTest, UpdateTransport) {
  // Spin up a second server as a second endpoint to connect to.
  auto server2 = std::make_unique<ecclesia::FakeRedfishServer>(
      "barebones_session_auth/mockup.shar");
  auto config = server2->GetConfig();

  // Set up handlers on the second server to return a different payload.
  constexpr absl::string_view kSecondServerResponse = R"json({
  "@odata.id": "/redfish/v1/Chassis/chassis",
  "Id": "1",
  "Name": "MyTestResource",
  "Description": "My Test Resource"
})json";
  int called_count = 0;
  server2->AddHttpGetHandler("/redfish/v1/Chassis/chassis",
                             [&](ServerRequestInterface *req) {
                               called_count++;
                               SetContentType(req, "application/json");
                               req->WriteResponseString(kSecondServerResponse);
                               req->Reply();
                             });

  // First GET should fetch from the original server.
  EXPECT_TRUE(intf_->IsTrusted());
  EXPECT_THAT(
      nlohmann::json::parse(
          intf_->CachedGetUri("/redfish/v1/Chassis/chassis").DebugString(),
          nullptr,
          /*allow_exceptions=*/false),
      Eq(nlohmann::json::parse(R"json({
    "@odata.context": "/redfish/v1/$metadata#Chassis.Chassis",
    "@odata.id": "/redfish/v1/Chassis/chassis",
    "@odata.type": "#Chassis.v1_10_0.Chassis",
    "Id": "chassis",
    "Name": "chassis",
    "Status": {
        "State": "StandbyOffline"
    }
})json")));

  // Update the endpoint.
  {
    ecclesia::HttpCredential creds;
    auto curl_http_client = std::make_unique<ecclesia::CurlHttpClient>(
        ecclesia::LibCurlProxy::CreateInstance(), creds);
    auto new_transport = ecclesia::HttpRedfishTransport::MakeNetwork(
        std::move(curl_http_client),
        absl::StrFormat("%s:%d", config.hostname, config.port));
    intf_->UpdateTransport(std::move(new_transport),
                           RedfishInterface::kUntrusted);
  }
  // Subsequent GET should fetch from the new server. The cache should be wiped.
  EXPECT_FALSE(intf_->IsTrusted());
  EXPECT_THAT(
      nlohmann::json::parse(
          intf_->CachedGetUri("/redfish/v1/Chassis/chassis").DebugString(),
          nullptr,
          /*allow_exceptions=*/false),
      Eq(nlohmann::json::parse(kSecondServerResponse)));
  EXPECT_THAT(called_count, Eq(1));

  // Cache policy should be followed. Verify the cached copy is returned.
  EXPECT_THAT(
      nlohmann::json::parse(
          intf_->CachedGetUri("/redfish/v1/Chassis/chassis").DebugString(),
          nullptr,
          /*allow_exceptions=*/false),
      Eq(nlohmann::json::parse(kSecondServerResponse)));
  EXPECT_THAT(called_count, Eq(1));

  // Verify after advancing time, a fresh copy is returned.
  clock_.AdvanceTime(absl::Minutes(2));
  EXPECT_THAT(
      nlohmann::json::parse(
          intf_->CachedGetUri("/redfish/v1/Chassis/chassis").DebugString(),
          nullptr,
          /*allow_exceptions=*/false),
      Eq(nlohmann::json::parse(kSecondServerResponse)));
  EXPECT_THAT(called_count, Eq(2));
}

TEST_F(HttpRedfishInterfaceTest, GetRoot) {
  auto root = intf_->GetRoot();
  EXPECT_THAT(nlohmann::json::parse(root.DebugString(), nullptr,
                                    /*allow_exceptions=*/false),
              Eq(nlohmann::json::parse(R"json({
  "@odata.context": "/redfish/v1/$metadata#ServiceRoot.ServiceRoot",
  "@odata.id": "/redfish/v1",
  "@odata.type": "#ServiceRoot.v1_5_0.ServiceRoot",
  "Chassis": {
      "@odata.id": "/redfish/v1/Chassis"
  },
  "Id": "RootService",
  "Links": {
      "Sessions": {
          "@odata.id": "/redfish/v1/SessionService/Sessions"
      }
  },
  "Name": "Root Service",
  "RedfishVersion": "1.6.1"
})json")));
}

TEST_F(HttpRedfishInterfaceTest, CrawlToChassisCollection) {
  auto chassis_collection = intf_->GetRoot()[kRfPropertyChassis];
  EXPECT_THAT(nlohmann::json::parse(chassis_collection.DebugString(), nullptr,
                                    /*allow_exceptions=*/false),
              Eq(nlohmann::json::parse(R"json({
    "@odata.context": "/redfish/v1/$metadata#ChassisCollection.ChassisCollection",
    "@odata.id": "/redfish/v1/Chassis",
    "@odata.type": "#ChassisCollection.ChassisCollection",
    "Members": [
        {
            "@odata.id": "/redfish/v1/Chassis/chassis"
        }
    ],
    "Members@odata.count": 1,
    "Name": "Chassis Collection"
})json")));
}

TEST_F(HttpRedfishInterfaceTest, CrawlToChassis) {
  auto chassis_collection = intf_->GetRoot()[kRfPropertyChassis][0];
  EXPECT_THAT(nlohmann::json::parse(chassis_collection.DebugString(), nullptr,
                                    /*allow_exceptions=*/false),
              Eq(nlohmann::json::parse(R"json({
    "@odata.context": "/redfish/v1/$metadata#Chassis.Chassis",
    "@odata.id": "/redfish/v1/Chassis/chassis",
    "@odata.type": "#Chassis.v1_10_0.Chassis",
    "Id": "chassis",
    "Name": "chassis",
    "Status": {
        "State": "StandbyOffline"
    }
})json")));
}

TEST_F(HttpRedfishInterfaceTest, GetUri) {
  auto chassis = intf_->UncachedGetUri("/redfish/v1/Chassis/chassis");
  EXPECT_THAT(nlohmann::json::parse(chassis.DebugString(), nullptr,
                                    /*allow_exceptions=*/false),
              Eq(nlohmann::json::parse(R"json({
    "@odata.context": "/redfish/v1/$metadata#Chassis.Chassis",
    "@odata.id": "/redfish/v1/Chassis/chassis",
    "@odata.type": "#Chassis.v1_10_0.Chassis",
    "Id": "chassis",
    "Name": "chassis",
    "Status": {
        "State": "StandbyOffline"
    }
})json")));
}

TEST_F(HttpRedfishInterfaceTest, GetUriFragmentString) {
  auto chassis = intf_->UncachedGetUri("/redfish/v1/Chassis/chassis#/Name");
  EXPECT_THAT(chassis.DebugString(), Eq("\"chassis\""));
}

TEST_F(HttpRedfishInterfaceTest, GetUriFragmentObject) {
  auto status = intf_->UncachedGetUri("/redfish/v1/Chassis/chassis#/Status");
  EXPECT_THAT(nlohmann::json::parse(status.DebugString(), nullptr,
                                    /*allow_exceptions=*/false),
              Eq(nlohmann::json::parse(R"json({
    "State": "StandbyOffline"
})json")));
}

TEST_F(HttpRedfishInterfaceTest, EachTest) {
  std::vector<std::string> names;
  intf_->GetRoot()[kRfPropertyChassis].Each().Do(
      [&names](std::unique_ptr<RedfishObject> &obj) {
        auto name = obj->GetNodeValue<PropertyName>();
        if (name.has_value()) names.push_back(*std::move(name));
        return RedfishIterReturnValue::kContinue;
      });
  EXPECT_THAT(names, ElementsAre("chassis"));
}

TEST_F(HttpRedfishInterfaceTest, ForEachPropertyTest) {
  auto chassis = intf_->UncachedGetUri("/redfish/v1/Chassis/chassis");
  std::vector<std::pair<std::string, std::string>> all_properties;
  chassis.AsObject()->ForEachProperty(
      [&all_properties](absl::string_view name, RedfishVariant value) {
        all_properties.push_back(
            std::make_pair(std::string(name), value.DebugString()));
        return RedfishIterReturnValue::kContinue;
      });
  EXPECT_THAT(
      all_properties,
      UnorderedElementsAre(
          std::make_pair("@odata.context",
                         "\"/redfish/v1/$metadata#Chassis.Chassis\""),
          std::make_pair("@odata.id", "\"/redfish/v1/Chassis/chassis\""),
          std::make_pair("@odata.type", "\"#Chassis.v1_10_0.Chassis\""),
          std::make_pair("Id", "\"chassis\""),
          std::make_pair("Name", "\"chassis\""),
          std::make_pair("Status", "{\"State\":\"StandbyOffline\"}")));
}

TEST_F(HttpRedfishInterfaceTest, ForEachPropertyTestStop) {
  auto chassis = intf_->UncachedGetUri("/redfish/v1/Chassis/chassis");
  std::vector<std::pair<std::string, std::string>> all_properties;
  int called = 0;
  chassis.AsObject()->ForEachProperty(
      [&called](absl::string_view name, RedfishVariant value) {
        ++called;
        return RedfishIterReturnValue::kStop;
      });
  EXPECT_THAT(called, Eq(1));
}

TEST_F(HttpRedfishInterfaceTest, CachedGet) {
  int called_count = 0;
  auto result_json = nlohmann::json::parse(R"json({
    "Id": "1",
    "Name": "MyResource",
    "Description": "My Test Resource"
  })json");
  server_->AddHttpGetHandler("/my/uri", [&](ServerRequestInterface *req) {
    called_count++;
    SetContentType(req, "application/json");
    req->WriteResponseString(result_json.dump());
    req->Reply();
  });

  // The first GET will need to hit the backend as the cache is empty.
  {
    auto result = intf_->CachedGetUri("/my/uri", GetParams{});
    EXPECT_THAT(called_count, Eq(1));
    EXPECT_THAT(nlohmann::json::parse(result.DebugString(), nullptr, false),
                Eq(result_json));
  }

  // The next GET should hit the cache. called_count should not increase.
  clock_.AdvanceTime(absl::Seconds(1));
  {
    auto result = intf_->CachedGetUri("/my/uri", GetParams{});
    EXPECT_THAT(called_count, Eq(1));
    EXPECT_THAT(nlohmann::json::parse(result.DebugString(), nullptr, false),
                Eq(result_json));
  }

  // After the age expires, called_count should increase.
  clock_.AdvanceTime(absl::Minutes(1));
  {
    auto result = intf_->CachedGetUri("/my/uri", GetParams{});
    EXPECT_THAT(called_count, Eq(2));
    EXPECT_THAT(nlohmann::json::parse(result.DebugString(), nullptr, false),
                Eq(result_json));
  }
}

TEST_F(HttpRedfishInterfaceTest, GetWithExpand) {
  server_->AddHttpGetHandler("/redfish/v1", [&](ServerRequestInterface *req) {
    SetContentType(req, "application/json");
    // Reply will redirect the chassis
    auto reply = nlohmann::json::parse(
        R"json({
              "@odata.id": "/redfish/v1",
              "Chassis": {
                "@odata.id": "/redfish/v2/Chassis"
              },
              "FakeItemWithoutId": {},
              "ProtocolFeaturesSupported": {
                "ExpandQuery": {
                  "ExpandAll": true,
                  "Levels": true,
                  "Links": true,
                  "MaxLevels": 6,
                  "NoLinks": true
                }
              }
            })json");
    req->WriteResponseString(reply.dump());
    req->Reply();
  });
  int called_chassis_expanded_count = 0;
  server_->AddHttpGetHandler("/redfish/v2/Chassis?$expand=*($levels=1)",
                             [&](ServerRequestInterface *req) {
                               SetContentType(req, "application/json");
                               called_chassis_expanded_count++;
                               req->WriteResponseString(R"json({})json");
                               req->Reply();
                             });
  int called_fake_item_expanded_count = 0;
  server_->AddHttpGetHandler(
      "/redfish/v1/FakeItemWithoutId?$expand=.($levels=1)",
      [&](ServerRequestInterface *req) {
        SetContentType(req, "application/json");
        called_fake_item_expanded_count++;
        req->WriteResponseString(R"json({})json");
        req->Reply();
      });

  auto redfish_object = intf_->GetRoot().AsObject();
  ASSERT_NE(redfish_object, nullptr);
  redfish_object->GetExpanded(
      "Chassis", RedfishQueryParamExpand({RedfishQueryParamExpand::kBoth, 1}));
  EXPECT_EQ(called_chassis_expanded_count, 1);
  redfish_object->GetExpanded(
      "FakeItemWithoutId",
      RedfishQueryParamExpand({RedfishQueryParamExpand::kNotLinks, 1}));
  EXPECT_EQ(called_fake_item_expanded_count, 1);
}

TEST_F(HttpRedfishInterfaceTest, GetWithoutExpand) {
  int called_expanded_count = 0;
  server_->AddHttpGetHandler("/redfish/v1", [&](ServerRequestInterface *req) {
    SetContentType(req, "application/json");
    auto reply = nlohmann::json::parse(
        R"json({
              "@odata.id": "/redfish/v1",
              "Chassis": {
                "@odata.id": "/redfish/v1/Chassis"
              }
            })json");
    req->WriteResponseString(reply.dump());
    req->Reply();
  });
  server_->AddHttpGetHandler("/redfish/v1/Chassis",
                             [&](ServerRequestInterface *req) {
                               SetContentType(req, "application/json");
                               req->WriteResponseString(R"json({})json");
                               req->Reply();
                             });
  server_->AddHttpGetHandler("/redfish/v1/Chassis?$expand=*($levels=1)",
                             [&](ServerRequestInterface *req) {
                               SetContentType(req, "application/json");
                               called_expanded_count++;
                               req->WriteResponseString(R"json({})json");
                               req->Reply();
                             });
  auto redfish_object = intf_->GetRoot().AsObject();
  ASSERT_NE(redfish_object, nullptr);
  redfish_object->GetExpanded(
      "Chassis", RedfishQueryParamExpand({RedfishQueryParamExpand::kBoth, 1}));
  EXPECT_EQ(called_expanded_count, 0);
}

TEST_F(HttpRedfishInterfaceTest, CachedGetWithOperator) {
  int parent_called_count = 0;
  int child_called_count = 0;
  auto json_parent = nlohmann::json::parse(R"json({
    "Id": "1",
    "Name": "MyResource",
    "Description": "My Test Resource",
    "Reference": { "@odata.id": "/my/other/uri" }
  })json");
  auto json_child = nlohmann::json::parse(R"json({
    "Id": "2",
    "Name": "MyOtherResource",
    "Description": "My Other Test Resource"
  })json");
  server_->AddHttpGetHandler("/my/uri", [&](ServerRequestInterface *req) {
    parent_called_count++;
    SetContentType(req, "application/json");
    req->WriteResponseString(json_parent.dump());
    req->Reply();
  });
  server_->AddHttpGetHandler("/my/other/uri", [&](ServerRequestInterface *req) {
    child_called_count++;
    SetContentType(req, "application/json");
    req->WriteResponseString(json_child.dump());
    req->Reply();
  });

  // The first GET will need to hit the backend as the cache is empty.
  auto parent = intf_->CachedGetUri("/my/uri", GetParams{});
  EXPECT_THAT(parent_called_count, Eq(1));
  EXPECT_THAT(nlohmann::json::parse(parent.DebugString(), nullptr, false),
              Eq(json_parent));

  // Get the child, cache is empty and will increment the child's handler once.
  auto child = parent["Reference"];
  EXPECT_THAT(child_called_count, Eq(1));
  EXPECT_THAT(nlohmann::json::parse(child.DebugString(), nullptr, false),
              Eq(json_child));

  // Getting the parent again should retrieve the cached result.
  auto parent2 = intf_->CachedGetUri("/my/uri", GetParams{});
  EXPECT_THAT(parent_called_count, Eq(1));
  EXPECT_THAT(nlohmann::json::parse(parent2.DebugString(), nullptr, false),
              Eq(json_parent));

  // Getting the child again should hit the cache.
  auto child2 = parent2["Reference"];
  EXPECT_THAT(child_called_count, Eq(1));
  EXPECT_THAT(nlohmann::json::parse(child2.DebugString(), nullptr, false),
              Eq(json_child));

  // Getting the child directly should still hit the cache.
  auto direct_child = intf_->CachedGetUri("/my/other/uri", GetParams{});
  EXPECT_THAT(child_called_count, Eq(1));
  EXPECT_THAT(nlohmann::json::parse(direct_child.DebugString(), nullptr, false),
              Eq(json_child));

  // Advance time and ensure this invalidates the cache and refetches the URI.
  clock_.AdvanceTime(absl::Seconds(1) + absl::Minutes(1));
  auto child3 = parent2["Reference"];
  EXPECT_THAT(child_called_count, Eq(2));
  EXPECT_THAT(nlohmann::json::parse(child3.DebugString(), nullptr, false),
              Eq(json_child));
}

TEST_F(HttpRedfishInterfaceTest, EnsureFreshPayloadDoesNotDoubleGet) {
  int called_count = 0;
  auto result_json = nlohmann::json::parse(R"json({
    "@odata.id": "/my/uri",
    "Id": "1",
    "Name": "MyResource",
    "Description": "My Test Resource"
  })json");
  server_->AddHttpGetHandler("/my/uri", [&](ServerRequestInterface *req) {
    called_count++;
    SetContentType(req, "application/json");
    req->WriteResponseString(result_json.dump());
    req->Reply();
  });

  // The first GET will need to hit the backend as the cache is empty.
  {
    auto result = intf_->CachedGetUri("/my/uri", GetParams{});
    EXPECT_THAT(called_count, Eq(1));
    EXPECT_THAT(nlohmann::json::parse(result.DebugString(), nullptr, false),
                Eq(result_json));

    // Converting to object and checking for freshness should not hit backend
    // again. called_count should not increase.
    auto obj = result.AsObject();
    ASSERT_TRUE(obj);
    auto new_obj = obj->EnsureFreshPayload();
    ASSERT_TRUE(new_obj);
    EXPECT_THAT(called_count, Eq(1));
    EXPECT_THAT(new_obj->DebugString(), Eq(obj->DebugString()));
  }

  // The next GET should hit the cache. called_count should not increase.
  clock_.AdvanceTime(absl::Seconds(1));
  {
    auto result = intf_->CachedGetUri("/my/uri", GetParams{});
    EXPECT_THAT(called_count, Eq(1));
    EXPECT_THAT(nlohmann::json::parse(result.DebugString(), nullptr, false),
                Eq(result_json));
    // Converting to object and checking for freshness should cause a new
    // fetch from the backend. called_count should increase.
    auto obj = result.AsObject();
    ASSERT_TRUE(obj);
    auto new_obj = obj->EnsureFreshPayload();
    ASSERT_TRUE(new_obj);
    EXPECT_THAT(called_count, Eq(2));
    EXPECT_THAT(new_obj->DebugString(), Eq(obj->DebugString()));
  }

  // After the age expires, called_count should increase.
  clock_.AdvanceTime(absl::Minutes(1));
  {
    auto result = intf_->CachedGetUri("/my/uri", GetParams{});
    EXPECT_THAT(called_count, Eq(3));
    EXPECT_THAT(nlohmann::json::parse(result.DebugString(), nullptr, false),
                Eq(result_json));

    // Converting to object and checking for freshness should not hit backend
    // again. called_count should not increase.
    auto obj = result.AsObject();
    ASSERT_TRUE(obj);
    auto new_obj = obj->EnsureFreshPayload();
    ASSERT_TRUE(new_obj);
    EXPECT_THAT(called_count, Eq(3));
    EXPECT_THAT(new_obj->DebugString(), Eq(obj->DebugString()));
  }
}
TEST_F(HttpRedfishInterfaceTest, EnsureFreshPayloadDoesNotDoubleGetUncached) {
  int called_count = 0;
  auto result_json = nlohmann::json::parse(R"json({
    "@odata.id": "/my/uri",
    "Id": "1",
    "Name": "MyResource",
    "Description": "My Test Resource"
  })json");
  server_->AddHttpGetHandler("/my/uri", [&](ServerRequestInterface *req) {
    called_count++;
    SetContentType(req, "application/json");
    req->WriteResponseString(result_json.dump());
    req->Reply();
  });

  // The first GET will need to hit the backend as the cache is empty.
  {
    auto result = intf_->CachedGetUri("/my/uri", GetParams{});
    EXPECT_THAT(called_count, Eq(1));
    EXPECT_THAT(nlohmann::json::parse(result.DebugString(), nullptr, false),
                Eq(result_json));
    // Converting to object and checking for freshness should not hit backend
    // again. called_count should not increase.
    auto obj = result.AsObject();
    ASSERT_TRUE(obj);
    auto new_obj = obj->EnsureFreshPayload();
    ASSERT_TRUE(new_obj);
    EXPECT_THAT(called_count, Eq(1));
    EXPECT_THAT(new_obj->DebugString(), Eq(obj->DebugString()));
  }

  // The next GET is explicitly uncached. called_count should increase.
  {
    auto result = intf_->UncachedGetUri("/my/uri", GetParams{});
    EXPECT_THAT(called_count, Eq(2));
    EXPECT_THAT(nlohmann::json::parse(result.DebugString(), nullptr, false),
                Eq(result_json));
    // Converting to object and checking for freshness should not hit backend
    // again. called_count should not increase.
    auto obj = result.AsObject();
    ASSERT_TRUE(obj);
    auto new_obj = obj->EnsureFreshPayload();
    ASSERT_TRUE(new_obj);
    EXPECT_THAT(called_count, Eq(2));
    EXPECT_THAT(new_obj->DebugString(), Eq(obj->DebugString()));
  }
}
TEST_F(HttpRedfishInterfaceTest, EnsureFreshPayloadFailsWithNoOdataId) {
  auto result_json = nlohmann::json::parse(R"json({
    "Id": "1",
    "Name": "MyResource",
    "Description": "My Test Resource With no @odata.id property"
  })json");
  server_->AddHttpGetHandler("/my/uri", [&](ServerRequestInterface *req) {
    SetContentType(req, "application/json");
    req->WriteResponseString(result_json.dump());
    req->Reply();
  });

  // First GET primes the cache.
  auto result1 = intf_->CachedGetUri("/my/uri", GetParams{});
  // Second GET returns the cached copy.
  auto result2 = intf_->CachedGetUri("/my/uri", GetParams{});
  auto obj = result2.AsObject();
  ASSERT_TRUE(obj);
  auto new_obj = obj->EnsureFreshPayload();
  EXPECT_FALSE(new_obj);
}

TEST_F(HttpRedfishInterfaceTest, PostHandler) {
  bool called = false;
  server_->AddHttpPostHandler("/my/uri", [&](ServerRequestInterface *req) {
    int64_t size;
    auto buf = req->ReadRequestBytes(&size);
    ASSERT_THAT(size, Gt(0));
    auto read_request = nlohmann::json::parse(
        absl::string_view(buf.get(), size), /*cb=*/nullptr,
        /*allow_exceptions=*/false);
    ASSERT_FALSE(read_request.is_discarded());
    EXPECT_THAT(read_request, Eq(nlohmann::json::parse(
                                  R"json({
  "int": 1,
  "string": "hello",
  "char": "hi",
  "bool": true,
  "double": 3.14,
  "list": [
    1,
    "string",
    [ "nested", "list" ],
    { "nested": "obj" }
  ],
  "obj": {
    "obj_int": 2,
    "obj_string": "goodbye",
    "obj_char": "bye",
    "obj_bool": false,
    "obj_double": 6.28,
    "obj_list": [
      2,
      "string",
      [ "nested", "list" ],
      { "nested": "obj" }
    ],
    "obj_obj": {
      "nested": 3
    }
  }
})json",
                                  /*cb=*/nullptr, /*exceptions=*/false)));
    called = true;

    SetContentType(req, "application/json");
    req->WriteResponseString("{}");
    req->Reply();
  });

  constexpr char kHi[] = "hi";
  constexpr char kBye[] = "bye";
  auto result = intf_->PostUri(
      "/my/uri",
      {{"int", 1},
       {"string", "hello"},
       {"char", kHi},
       {"bool", true},
       {"double", 3.14},
       {"list",
        RedfishInterface::ListValue{
            .items = {1, "string",
                      RedfishInterface::ListValue{.items = {"nested", "list"}},
                      RedfishInterface::ObjectValue{
                          .items = {{"nested", "obj"}}}}}},
       {"obj",
        RedfishInterface::ObjectValue{
            .items = {{"obj_int", 2},
                      {"obj_string", "goodbye"},
                      {"obj_char", kBye},
                      {"obj_bool", false},
                      {"obj_double", 6.28},
                      {"obj_list",
                       RedfishInterface::ListValue{
                           .items = {2, "string",
                                     RedfishInterface::ListValue{
                                         .items = {"nested", "list"}},
                                     RedfishInterface::ObjectValue{
                                         .items = {{"nested", "obj"}}}}}},
                      {"obj_obj", RedfishInterface::ObjectValue{
                                      .items = {{"nested", 3}}}}}}}});
  EXPECT_TRUE(called);
}

TEST_F(HttpRedfishInterfaceTest, PatchHandler) {
  bool called = false;
  server_->AddHttpPatchHandler("/my/uri", [&](ServerRequestInterface *req) {
    int64_t size;
    auto buf = req->ReadRequestBytes(&size);
    ASSERT_THAT(size, Gt(0));
    auto read_request = nlohmann::json::parse(
        absl::string_view(buf.get(), size), /*cb=*/nullptr,
        /*allow_exceptions=*/false);
    ASSERT_FALSE(read_request.is_discarded());
    EXPECT_THAT(read_request, Eq(nlohmann::json::parse(
                                  R"json({
  "int": 1,
  "string": "hello",
  "char": "hi",
  "bool": true,
  "double": 3.14,
  "list": [
    1,
    "string",
    [ "nested", "list" ],
    { "nested": "obj" }
  ],
  "obj": {
    "obj_int": 2,
    "obj_string": "goodbye",
    "obj_char": "bye",
    "obj_bool": false,
    "obj_double": 6.28,
    "obj_list": [
      2,
      "string",
      [ "nested", "list" ],
      { "nested": "obj" }
    ],
    "obj_obj": {
      "nested": 3
    }
  }
})json",
                                  /*cb=*/nullptr, /*exceptions=*/false)));
    called = true;

    SetContentType(req, "application/json");
    req->WriteResponseString("{}");
    req->Reply();
  });

  constexpr char kHi[] = "hi";
  constexpr char kBye[] = "bye";
  auto result = intf_->PatchUri(
      "/my/uri",
      {{"int", 1},
       {"string", "hello"},
       {"char", kHi},
       {"bool", true},
       {"double", 3.14},
       {"list",
        RedfishInterface::ListValue{
            .items = {1, "string",
                      RedfishInterface::ListValue{.items = {"nested", "list"}},
                      RedfishInterface::ObjectValue{
                          .items = {{"nested", "obj"}}}}}},
       {"obj",
        RedfishInterface::ObjectValue{
            .items = {{"obj_int", 2},
                      {"obj_string", "goodbye"},
                      {"obj_char", kBye},
                      {"obj_bool", false},
                      {"obj_double", 6.28},
                      {"obj_list",
                       RedfishInterface::ListValue{
                           .items = {2, "string",
                                     RedfishInterface::ListValue{
                                         .items = {"nested", "list"}},
                                     RedfishInterface::ObjectValue{
                                         .items = {{"nested", "obj"}}}}}},
                      {"obj_obj", RedfishInterface::ObjectValue{
                                      .items = {{"nested", 3}}}}}}}});
  EXPECT_TRUE(called);
}

}  // namespace
}  // namespace ecclesia
