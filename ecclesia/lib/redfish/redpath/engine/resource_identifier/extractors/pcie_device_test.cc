/*
 * Copyright 2026 Google LLC
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

#include "ecclesia/lib/redfish/redpath/engine/resource_identifier/extractors/pcie_device.h"

#include <memory>
#include <optional>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/engine/normalizer.h"
#include "ecclesia/lib/redfish/redpath/engine/resource_identifier/extractor.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ecclesia::EqualsProto;
using ecclesia::ParseTextProtoOrDie;
using ::testing::_;
using ::testing::Optional;
using ::testing::Return;

// Mock Normalizer for testing.
class MockIdentifierExtractor : public IdentifierExtractorIntf {
 public:
  MOCK_METHOD(absl::StatusOr<IdentifierExtractorIntf::ResourceIdentifier>,
              Extract, (const RedfishObject& obj, const Deps& deps),
              (const, override));
};

TEST(PcieDeviceIdentifierExtractorTest, ExtractIdentifierSuccess) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");

  server.AddHttpGetHandlerWithOwnedData(
    "/redfish/v1/Systems/system/PCIeDevices/0000_3f_00",
        R"json({
      "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/$0",
      "@odata.type": "#PCIeDevice.v1_9_0.PCIeDevice",
      "Id": "0000_3f_00",
      "Links": {
        "Chassis": [{
          "@odata.id": "/redfish/v1/Chassis/chassis"
        }]
      }
    })json");

  MockIdentifierExtractor mock_extractor;
  IdentifierExtractorIntf::ResourceIdentifier mock_identifier;
  mock_identifier.identifier = ParseTextProtoOrDie(R"pb(
    machine_devpath: "/phys/PE0/IO0"
  )pb");
  EXPECT_CALL(mock_extractor, Extract(_, _)).WillOnce(Return(mock_identifier));
  PcieDeviceIdentifierExtractor extractor(&mock_extractor);

  RedpathNormalizer normalizer;
  std::unique_ptr<RedfishInterface> redfish_interface =
      server.RedfishClientInterface();
  IdentifierExtractorIntf::Deps deps{.interface = redfish_interface.get(),
                                     .normalizer = normalizer,
                                     .options = {}};

  RedfishVariant variant = redfish_interface->CachedGetUri(
      "/redfish/v1/Systems/system/PCIeDevices/0000_3f_00");
  std::unique_ptr<RedfishObject> obj = variant.AsObject();
  EXPECT_THAT(extractor.Extract(*obj, deps),
              IsOkAndHolds(Field(
                  &IdentifierExtractorIntf::ResourceIdentifier::identifier,
                  Optional(EqualsProto(R"pb(
                    machine_devpath: "/phys/PE0/IO0"
                  )pb")))));
}

TEST(PcieDeviceIdentifierExtractorTest, NotPcieDevice) {
  DefaultIdentifierExtractor default_extractor;
  PcieDeviceIdentifierExtractor extractor(&default_extractor);
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  RedpathNormalizer normalizer;
  std::unique_ptr<RedfishInterface> redfish_interface =
      server.RedfishClientInterface();
  IdentifierExtractorIntf::Deps deps{.interface = redfish_interface.get(),
                                     .normalizer = normalizer,
                                     .options = {}};
  RedfishVariant variant =
      redfish_interface->CachedGetUri("/redfish/v1/Chassis/chassis");
  std::unique_ptr<RedfishObject> obj = variant.AsObject();
  EXPECT_THAT(extractor.Extract(*obj, deps), IsStatusNotFound());
}

TEST(PcieDeviceIdentifierExtractorTest, LinksNotFound) {
  DefaultIdentifierExtractor default_extractor;
  PcieDeviceIdentifierExtractor extractor(&default_extractor);
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  RedpathNormalizer normalizer;
  std::unique_ptr<RedfishInterface> redfish_interface =
      server.RedfishClientInterface();
  IdentifierExtractorIntf::Deps deps{.interface = redfish_interface.get(),
                                     .normalizer = normalizer,
                                     .options = {}};
  server.AddHttpGetHandlerWithOwnedData(
      "/redfish/v1/Systems/system/PCIeDevices/fake",
      R"json({
        "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/fake",
        "@odata.type": "#PCIeDevice.v1_9_0.PCIeDevice",
        "Id": "fake"
      })json");

  RedfishVariant variant = redfish_interface->CachedGetUri(
      "/redfish/v1/Systems/system/PCIeDevices/fake");
  std::unique_ptr<RedfishObject> obj = variant.AsObject();
  EXPECT_THAT(extractor.Extract(*obj, deps), IsStatusNotFound());
}

TEST(PcieDeviceIdentifierExtractorTest, ChassisNotFound) {
  DefaultIdentifierExtractor default_extractor;
  PcieDeviceIdentifierExtractor extractor(&default_extractor);
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  RedpathNormalizer normalizer;
  std::unique_ptr<RedfishInterface> redfish_interface =
      server.RedfishClientInterface();
  IdentifierExtractorIntf::Deps deps{.interface = redfish_interface.get(),
                                     .normalizer = normalizer,
                                     .options = {}};
  server.AddHttpGetHandlerWithOwnedData(
      "/redfish/v1/Systems/system/PCIeDevices/fake",
      R"json({
        "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/fake",
        "@odata.type": "#PCIeDevice.v1_9_0.PCIeDevice",
        "Id": "fake",
        "Links": {}
      })json");

  RedfishVariant variant = redfish_interface->CachedGetUri(
      "/redfish/v1/Systems/system/PCIeDevices/fake");
  std::unique_ptr<RedfishObject> obj = variant.AsObject();
  EXPECT_THAT(extractor.Extract(*obj, deps), IsStatusNotFound());
}

TEST(PcieDeviceIdentifierExtractorTest, OdataIdNotFoundInChassis) {
  DefaultIdentifierExtractor default_extractor;
  PcieDeviceIdentifierExtractor extractor(&default_extractor);
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  RedpathNormalizer normalizer;
  std::unique_ptr<RedfishInterface> redfish_interface =
      server.RedfishClientInterface();
  IdentifierExtractorIntf::Deps deps{.interface = redfish_interface.get(),
                                     .normalizer = normalizer,
                                     .options = {}};
  server.AddHttpGetHandlerWithOwnedData(
      "/redfish/v1/Systems/system/PCIeDevices/fake",
      R"json({
        "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/fake",
        "@odata.type": "#PCIeDevice.v1_9_0.PCIeDevice",
        "Id": "fake",
        "Links": {
          "Chassis": [{}]
        }
      })json");

  RedfishVariant variant = redfish_interface->CachedGetUri(
      "/redfish/v1/Systems/system/PCIeDevices/fake");
  std::unique_ptr<RedfishObject> obj = variant.AsObject();
  EXPECT_THAT(extractor.Extract(*obj, deps), IsStatusNotFound());
}

TEST(PcieDeviceIdentifierExtractorTest, ChassisIsEmptyArray) {
  DefaultIdentifierExtractor default_extractor;
  PcieDeviceIdentifierExtractor extractor(&default_extractor);
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  RedpathNormalizer normalizer;
  std::unique_ptr<RedfishInterface> redfish_interface =
      server.RedfishClientInterface();
  IdentifierExtractorIntf::Deps deps{.interface = redfish_interface.get(),
                                     .normalizer = normalizer,
                                     .options = {}};
  server.AddHttpGetHandlerWithOwnedData(
      "/redfish/v1/Systems/system/PCIeDevices/fake",
      R"json({
        "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/fake",
        "@odata.type": "#PCIeDevice.v1_9_0.PCIeDevice",
        "Id": "fake",
        "Links": {
          "Chassis": []
        }
      })json");

  RedfishVariant variant = redfish_interface->CachedGetUri(
      "/redfish/v1/Systems/system/PCIeDevices/fake");
  std::unique_ptr<RedfishObject> obj = variant.AsObject();
  EXPECT_THAT(extractor.Extract(*obj, deps), IsStatusNotFound());
}

TEST(PcieDeviceIdentifierExtractorTest, ChassisUriReturnsError) {
  DefaultIdentifierExtractor default_extractor;
  PcieDeviceIdentifierExtractor extractor(&default_extractor);
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  RedpathNormalizer normalizer;
  std::unique_ptr<RedfishInterface> redfish_interface =
      server.RedfishClientInterface();
  IdentifierExtractorIntf::Deps deps{.interface = redfish_interface.get(),
                                     .normalizer = normalizer,
                                     .options = {}};
  server.AddHttpGetHandlerWithOwnedData(
      "/redfish/v1/Systems/system/PCIeDevices/fake",
      R"json({
        "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/fake",
        "@odata.type": "#PCIeDevice.v1_9_0.PCIeDevice",
        "Id": "fake",
        "Links": {
          "Chassis": [{
            "@odata.id": "/redfish/v1/Chassis/i_do_not_exist"
          }]
        }
      })json");

  RedfishVariant variant = redfish_interface->CachedGetUri(
      "/redfish/v1/Systems/system/PCIeDevices/fake");
  std::unique_ptr<RedfishObject> obj = variant.AsObject();
  EXPECT_THAT(extractor.Extract(*obj, deps), IsStatusFailedPrecondition());
}
}  // namespace
}  // namespace ecclesia
