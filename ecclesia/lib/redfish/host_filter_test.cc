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
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/sysmodel.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::testing::UnorderedElementsAre;

class RedfishSingleHostFilterTest : public ::testing::Test {
 protected:
  RedfishSingleHostFilterTest()
      : single_host_server_(std::make_unique<FakeRedfishServer>(
            "topology_v2_testing/mockup.shar")),
        fake_intf_(single_host_server_->RedfishClientInterface()) {
    host_filter_ =
        std::make_unique<RedfishObjectHostFilter>("host-compute-node");
  }

  std::unique_ptr<FakeRedfishServer> single_host_server_;
  std::unique_ptr<RedfishInterface> fake_intf_;
  std::unique_ptr<RedfishObjectHostFilter> host_filter_;
};

class RedfishMultiHostFilterTest : public ::testing::Test {
 protected:
  RedfishMultiHostFilterTest()
      : multi_host_server_(std::make_unique<FakeRedfishServer>(
            "topology_v2_multi_host_testing/mockup.shar")),
        fake_intf_(multi_host_server_->RedfishClientInterface()),
        sysmodel_(fake_intf_.get()) {
    absl::flat_hash_map<std::string, std::string> system_to_host_domain_map = {
        {"system1", "host-compute-node-1"}, {"system2", "host-compute-node-2"}};
    host_filter_ = std::make_unique<RedfishObjectHostFilter>(
        system_to_host_domain_map, sysmodel_);
  }

  std::unique_ptr<FakeRedfishServer> multi_host_server_;
  std::unique_ptr<RedfishInterface> fake_intf_;
  Sysmodel sysmodel_;
  std::unique_ptr<RedfishObjectHostFilter> host_filter_;
};

TEST_F(RedfishSingleHostFilterTest, GenerateHostSetTest) {
  EXPECT_THAT(host_filter_->GetMultiHostOsDomainSet(),
              UnorderedElementsAre("host-compute-node"));
}

TEST_F(RedfishSingleHostFilterTest, ObjectHostTest) {
  const std::string fake_system_collection_str = R"json({
  "@odata.type": "#ComputerSystemCollection.ComputerSystemCollection",
  "Members": [
    {
      "@odata.id": "/redfish/v1/Systems/system"
    }
  ],
  "Members@odata.count": 1,
  "Name": "Computer System Collection"
})json";
  single_host_server_->AddHttpGetHandlerWithData("/redfish/v1/Systems/system",
                                                 fake_system_collection_str);
  RedfishVariant var = fake_intf_->UncachedGetUri("/redfish/v1/Systems/system");
  std::unique_ptr<RedfishObject> obj = var.AsObject();
  ASSERT_TRUE(obj != nullptr);
  absl::StatusOr<absl::string_view> os_domain =
      host_filter_->GetHostDomainForObj(*obj);
  EXPECT_THAT(os_domain, IsOkAndHolds("host-compute-node"));
}

TEST_F(RedfishSingleHostFilterTest, UriHostTest) {
  absl::StatusOr<absl::string_view> os_domain =
      host_filter_->GetHostDomainFromUri("/redfish/v1/Systems/system");
  EXPECT_THAT(os_domain, IsOkAndHolds("host-compute-node"));
}

TEST_F(RedfishMultiHostFilterTest, GenerateMapTest) {
  auto system_host_map = host_filter_->GetSystemHostDomainNameMap();
  EXPECT_THAT(
      system_host_map,
      UnorderedElementsAre(testing::Pair("system1", "host-compute-node-1"),
                           testing::Pair("system2", "host-compute-node-2")));
}

TEST_F(RedfishMultiHostFilterTest, GenerateHostSetTest) {
  EXPECT_THAT(
      host_filter_->GetMultiHostOsDomainSet(),
      UnorderedElementsAre("host-compute-node-1", "host-compute-node-2"));
}

TEST_F(RedfishMultiHostFilterTest, ObjectHostTest) {
  {
    RedfishVariant var =
        fake_intf_->UncachedGetUri("/redfish/v1/Systems/system_multi1");
    std::unique_ptr<RedfishObject> obj = var.AsObject();
    ASSERT_TRUE(obj != nullptr);
    absl::StatusOr<absl::string_view> os_domain =
        host_filter_->GetHostDomainForObj(*obj);
    EXPECT_THAT(os_domain, IsOkAndHolds("host-compute-node-1"));
  }
  {
    RedfishVariant var = fake_intf_->UncachedGetUri(
        "/redfish/v1/Systems/system_multi2/Processors");
    std::unique_ptr<RedfishObject> obj = var.AsObject();
    ASSERT_TRUE(obj != nullptr);
    absl::StatusOr<absl::string_view> os_domain =
        host_filter_->GetHostDomainForObj(*obj);
    ASSERT_TRUE(os_domain.ok());
    EXPECT_THAT(os_domain, IsOkAndHolds("host-compute-node-2"));
  }
  {
    RedfishVariant var = fake_intf_->UncachedGetUri(
        "/redfish/v1/Systems/system_multi2/Memory/0");
    std::unique_ptr<RedfishObject> obj = var.AsObject();
    ASSERT_TRUE(obj != nullptr);
    absl::StatusOr<absl::string_view> os_domain =
        host_filter_->GetHostDomainForObj(*obj);
    EXPECT_THAT(os_domain, IsOkAndHolds("host-compute-node-2"));
  }
  {
    RedfishVariant var =
        fake_intf_->UncachedGetUri("/redfish/v1/Cables/cable1");
    std::unique_ptr<RedfishObject> obj = var.AsObject();
    ASSERT_TRUE(obj != nullptr);
    absl::StatusOr<absl::string_view> os_domain =
        host_filter_->GetHostDomainForObj(*obj);
    EXPECT_THAT(os_domain, IsOkAndHolds("host-compute-node-1"));
  }
  {
    RedfishVariant var =
        fake_intf_->UncachedGetUri("/redfish/v1/Chassis/multi2");
    std::unique_ptr<RedfishObject> obj = var.AsObject();
    ASSERT_TRUE(obj != nullptr);
    absl::StatusOr<absl::string_view> os_domain =
        host_filter_->GetHostDomainForObj(*obj);
    EXPECT_THAT(os_domain, IsOkAndHolds("host-compute-node-1"));
  }
}

TEST_F(RedfishMultiHostFilterTest, ObjectHostAdditionalTest) {
  const std::string fake_fault_log = R"json({
    "@odata.id": "/redfish/v1/Managers/bmc/LogServices/FaultLog/Entries/1",
    "@odata.type": "#LogEntry.v1_9_0.LogEntry",
    "AdditionalDataURI": "/redfish/v1/Systems/system_multi1"
})json";
  multi_host_server_->AddHttpGetHandlerWithData(
      "/redfish/v1/Managers/bmc/LogServices/FaultLog/Entries/1",
      fake_fault_log);
  RedfishVariant var = fake_intf_->UncachedGetUri(
      "/redfish/v1/Managers/bmc/LogServices/FaultLog/Entries/1");
  std::unique_ptr<RedfishObject> obj = var.AsObject();
  ASSERT_TRUE(obj != nullptr);
  absl::StatusOr<absl::string_view> os_domain =
      host_filter_->GetHostDomainForObj(*obj);
  ASSERT_TRUE(os_domain.ok());
  EXPECT_EQ(*os_domain, "host-compute-node-1");
}

TEST_F(RedfishMultiHostFilterTest, ObjectNoHostTest) {
  RedfishVariant var = fake_intf_->UncachedGetUri("/redfish/v1/Chassis/multi1");
  std::unique_ptr<RedfishObject> obj = var.AsObject();
  ASSERT_TRUE(obj != nullptr);
  absl::StatusOr<absl::string_view> os_domain =
      host_filter_->GetHostDomainForObj(*obj);
  EXPECT_THAT(os_domain,
              absl::NotFoundError("Can't find a host domain for object: "
                                  "/redfish/v1/Chassis/multi1"));
}

TEST_F(RedfishMultiHostFilterTest, ObjectCableNoHostTest) {
  const std::string fake_cable_str = R"json({
  "@odata.id": "/redfish/v1/Cables/dummy",
  "@odata.type": "#Cable.v1_0_0.Cable",
  "Model": "dummy",
  "Name": "dummy",
  "Id": "dummy",
  "Links": {
    "DownstreamChassis": [
      {
        "@odata.id": "/redfish/v1/Chassis/multi1"
      }
    ]
  }
})json";
  multi_host_server_->AddHttpGetHandlerWithData("/redfish/v1/Cables/dummy",
                                                fake_cable_str);
  RedfishVariant var = fake_intf_->UncachedGetUri("/redfish/v1/Cables/dummy");
  std::unique_ptr<RedfishObject> obj = var.AsObject();
  ASSERT_TRUE(obj != nullptr);
  absl::StatusOr<absl::string_view> os_domain =
      host_filter_->GetHostDomainForObj(*obj);
  EXPECT_THAT(os_domain,
              absl::NotFoundError("Can't find a host domain for object: "
                                  "/redfish/v1/Chassis/multi1"));
}

TEST_F(RedfishMultiHostFilterTest, ObjectCableMultiSystemTest) {
  const std::string fake_cable_str = R"json({
  "@odata.id": "/redfish/v1/Cables/dummy",
  "@odata.type": "#Cable.v1_0_0.Cable",
  "Model": "dummy",
  "Name": "dummy",
  "Id": "dummy",
  "Links": {
    "DownstreamChassis": [
      {
        "@odata.id": "/redfish/v1/Chassis/multi1"
      },
      {
        "@odata.id": "/redfish/v1/Chassis/multi2"
      }
    ]
  }
})json";
  multi_host_server_->AddHttpGetHandlerWithData("/redfish/v1/Cables/dummy",
                                                fake_cable_str);
  RedfishVariant var = fake_intf_->UncachedGetUri("/redfish/v1/Cables/dummy");
  std::unique_ptr<RedfishObject> obj = var.AsObject();
  ASSERT_TRUE(obj != nullptr);
  absl::StatusOr<absl::string_view> os_domain =
      host_filter_->GetHostDomainForObj(*obj);
  EXPECT_THAT(os_domain,
              absl::NotFoundError("Can't find a host domain for object: "
                                  "/redfish/v1/Cables/dummy"));
}

TEST_F(RedfishMultiHostFilterTest, ObjectHostMissingConfigTest) {
  const std::string fake_system_collection_str = R"json({
  "@odata.id": "/redfish/v1/Systems",
  "@odata.type": "#ComputerSystemCollection.ComputerSystemCollection",
  "Members": [
      {
          "@odata.id": "/redfish/v1/Systems/system_multi1"
      },
      {
          "@odata.id": "/redfish/v1/Systems/system_multi2"
      },
      {
          "@odata.id": "/redfish/v1/Systems/system_multi3"
      }
  ],
  "Members@odata.count": 3,
  "Name": "Computer System Collection"
})json";
  const std::string fake_system_str = R"json({
    "@odata.id": "/redfish/v1/Systems/system_multi3",
    "Name": "system3"
  })json";
  multi_host_server_->AddHttpGetHandlerWithData("/redfish/v1/Systems",
                                                fake_system_collection_str);
  multi_host_server_->AddHttpGetHandlerWithData(
      "/redfish/v1/Systems/system_multi3", fake_system_str);
  RedfishVariant var =
      fake_intf_->UncachedGetUri("/redfish/v1/Systems/system_multi3");
  std::unique_ptr<RedfishObject> obj = var.AsObject();
  ASSERT_TRUE(obj != nullptr);
  absl::StatusOr<absl::string_view> os_domain =
      host_filter_->GetHostDomainForObj(*obj);
  EXPECT_THAT(os_domain, IsStatusNotFound());
}

TEST_F(RedfishMultiHostFilterTest, UriHostSystemFail) {
  {
    absl::StatusOr<absl::string_view> os_domain =
        host_filter_->GetHostDomainFromUri("/redfish");
    EXPECT_THAT(os_domain, IsStatusNotFound());
  }
  {
    absl::StatusOr<absl::string_view> os_domain =
        host_filter_->GetHostDomainFromUri("/redfish/v1/");
    EXPECT_THAT(os_domain, IsStatusNotFound());
  }
  {
    absl::StatusOr<absl::string_view> os_domain =
        host_filter_->GetHostDomainFromUri("/redfish/v1/Systems");
    EXPECT_THAT(os_domain, IsStatusNotFound());
  }
  {
    absl::StatusOr<absl::string_view> os_domain =
        host_filter_->GetHostDomainFromUri("/NO_EXIST/v1/Systems/system");
    EXPECT_THAT(os_domain, IsStatusNotFound());
  }
  {
    absl::StatusOr<absl::string_view> os_domain =
        host_filter_->GetHostDomainFromUri("/redfish/NO_EXIST/Systems/system");
    EXPECT_THAT(os_domain, IsStatusNotFound());
  }
  {
    absl::StatusOr<absl::string_view> os_domain =
        host_filter_->GetHostDomainFromUri("/redfish/v1/NO_EXIST/system");
    EXPECT_THAT(os_domain, IsStatusNotFound());
  }
}

TEST_F(RedfishMultiHostFilterTest, UriHostChassisFail) {
  {
    absl::StatusOr<absl::string_view> os_domain =
        host_filter_->GetHostDomainFromUri("/redfish/v1/Chassis");
    EXPECT_THAT(os_domain, IsStatusNotFound());
  }
  {
    absl::StatusOr<absl::string_view> os_domain =
        host_filter_->GetHostDomainFromUri("/NO_EXIST/v1/Chassis/chassis");
    EXPECT_THAT(os_domain, IsStatusNotFound());
  }
  {
    absl::StatusOr<absl::string_view> os_domain =
        host_filter_->GetHostDomainFromUri("/redfish/NO_EXIST/Chassis/chassis");
    EXPECT_THAT(os_domain, IsStatusNotFound());
  }
  {
    absl::StatusOr<absl::string_view> os_domain =
        host_filter_->GetHostDomainFromUri("/redfish/v1/NO_EXIST/chassis");
    EXPECT_THAT(os_domain, IsStatusNotFound());
  }
}

TEST_F(RedfishMultiHostFilterTest, SystemNoUriTest) {
  const std::string fake_system_str = R"json({
    "Name": "system2"
  })json";
  multi_host_server_->AddHttpGetHandlerWithData("/redfish/v1/Systems/system2",
                                                fake_system_str);
  RedfishVariant var =
      fake_intf_->UncachedGetUri("/redfish/v1/Systems/system2");
  std::unique_ptr<RedfishObject> obj = var.AsObject();
  ASSERT_TRUE(obj != nullptr);
  absl::StatusOr<absl::string_view> os_domain =
      host_filter_->GetHostDomainForObj(*obj);
  EXPECT_THAT(os_domain, IsStatusNotFound());
}

TEST_F(RedfishMultiHostFilterTest, ChassisNoUriTest) {
  const std::string fake_chassis_str = R"json({
    "Name": "multi1"
  })json";
  multi_host_server_->AddHttpGetHandlerWithData("/redfish/v1/Chassis/multi1",
                                                fake_chassis_str);
  RedfishVariant var =
      fake_intf_->UncachedGetUri("/redfish/v1/Chassis/multi1");
  std::unique_ptr<RedfishObject> obj = var.AsObject();
  ASSERT_TRUE(obj != nullptr);
  absl::StatusOr<absl::string_view> os_domain =
      host_filter_->GetHostDomainForObj(*obj);
  EXPECT_THAT(os_domain, IsStatusNotFound());
}

TEST_F(RedfishMultiHostFilterTest, NoUriGetHostTest) {
  const std::string log_service_str = R"json({
  "@odata.type": "#LogServiceCollection.LogServiceCollection",
  "Description": "Collection of LogServices for this Computer System",
  "Members": [
      {
          "@odata.id": "/redfish/v1/Systems/system_multi1/LogServices/HostLogger"
      }
  ],
  "Members@odata.count": 1,
  "Name": "System Log Services Collection"
})json";
  multi_host_server_->AddHttpGetHandlerWithData(
      "/redfish/v1/Systems/system_multi1/LogServices", log_service_str);
  RedfishVariant var = fake_intf_->UncachedGetUri(
      "/redfish/v1/Systems/system_multi1/LogServices");
  std::unique_ptr<RedfishObject> obj = var.AsObject();
  ASSERT_TRUE(obj != nullptr);
  absl::StatusOr<absl::string_view> os_domain =
      host_filter_->GetHostDomainForObj(*obj);
  EXPECT_THAT(os_domain, IsStatusNotFound());
}

}  // namespace
}  // namespace ecclesia
