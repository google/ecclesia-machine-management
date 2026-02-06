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

#include "ecclesia/lib/redfish/redpath/engine/resource_identifier/extractors/related_item.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
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
class MockExtractor : public IdentifierExtractorIntf {
 public:
  MOCK_METHOD(absl::StatusOr<IdentifierExtractorIntf::ResourceIdentifier>,
              Extract, (const RedfishObject& obj, const Deps& deps),
              (const, override));
};

TEST(RelatedItemIdentifierExtractorTest, ExtractIdentifierSuccess) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");

  server.AddHttpGetHandlerWithOwnedData(
      "/redfish/v1/Chassis/chassis/Sensors/sensor",
      R"json({
    "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/sensor",
    "@odata.type": "#Sensor.v1_2_0.Sensor",
    "Id": "sensor",
    "Name": "CPU0",
    "Reading": 60.0,
    "ReadingUnits": "Cel",
    "ReadingType": "Temperature",
    "RelatedItem": [
        {
            "@odata.id": "/redfish/v1/Chassis/chassis"
        }
    ]
  })json");

  MockExtractor mock_extractor;
  IdentifierExtractorIntf::ResourceIdentifier resource_identifier;
  resource_identifier.identifier =
      ParseTextProtoOrDie(R"pb(machine_devpath: "/phys")pb");
  EXPECT_CALL(mock_extractor, Extract(_, _))
      .WillOnce(Return(resource_identifier));
  RelatedItemIdentifierExtractor extractor(&mock_extractor);

  RedpathNormalizer normalizer;
  std::unique_ptr<RedfishInterface> redfish_interface =
      server.RedfishClientInterface();
  IdentifierExtractorIntf::Deps deps{.interface = redfish_interface.get(),
                                     .normalizer = normalizer,
                                     .options = {}};

  RedfishVariant variant = redfish_interface->CachedGetUri(
      "/redfish/v1/Chassis/chassis/Sensors/"
      "sensor");
  std::unique_ptr<RedfishObject> obj = variant.AsObject();
  EXPECT_THAT(extractor.Extract(*obj, deps),
              ecclesia::IsOkAndHolds(Field(
                  &IdentifierExtractorIntf::ResourceIdentifier::identifier,
                  Optional(EqualsProto(R"pb(
                    machine_devpath: "/phys"
                  )pb")))));
}

TEST(RelatedItemIdentifierExtractorTest,
     ExtractIdentifierSuccessOfFirstRelatedItem) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");

  server.AddHttpGetHandlerWithOwnedData(
      "/redfish/v1/Chassis/chassis/Sensors/sensor",
      R"json({
    "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/sensor",
    "@odata.type": "#Sensor.v1_2_0.Sensor",
    "Id": "sensor",
    "Name": "CPU0",
    "Reading": 60.0,
    "ReadingUnits": "Cel",
    "ReadingType": "Temperature",
    "RelatedItem": [
        {
            "@odata.id": "/redfish/v1/Chassis/chassis"
        },{
            "@odata.id": "/redfish/v1/Systems/system"
        }
    ]
  })json");

  MockExtractor mock_extractor;
  IdentifierExtractorIntf::ResourceIdentifier resource_identifier;
  resource_identifier.identifier =
      ParseTextProtoOrDie(R"pb(machine_devpath: "/phys")pb");
  EXPECT_CALL(mock_extractor, Extract(_, _))
      .WillOnce(Return(resource_identifier));
  RelatedItemIdentifierExtractor extractor(&mock_extractor);

  RedpathNormalizer normalizer;
  std::unique_ptr<RedfishInterface> redfish_interface =
      server.RedfishClientInterface();
  IdentifierExtractorIntf::Deps deps{.interface = redfish_interface.get(),
                                     .normalizer = normalizer,
                                     .options = {}};

  RedfishVariant variant = redfish_interface->CachedGetUri(
      "/redfish/v1/Chassis/chassis/Sensors/"
      "sensor");
  std::unique_ptr<RedfishObject> obj = variant.AsObject();
  EXPECT_THAT(extractor.Extract(*obj, deps),
              ecclesia::IsOkAndHolds(Field(
                  &IdentifierExtractorIntf::ResourceIdentifier::identifier,
                  Optional(EqualsProto(R"pb(
                    machine_devpath: "/phys"
                  )pb")))));
}
TEST(RelatedItemIdentifierExtractorTest,
     ExtractIdentifierSuccessOfSecondRelatedItem) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");

  server.AddHttpGetHandlerWithOwnedData(
      "/redfish/v1/Chassis/chassis/Sensors/sensor",
      R"json({
    "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/sensor",
    "@odata.type": "#Sensor.v1_2_0.Sensor",
    "Id": "sensor",
    "Name": "CPU0",
    "Reading": 60.0,
    "ReadingUnits": "Cel",
    "ReadingType": "Temperature",
    "RelatedItem": [
      {
            "@odata.id": "/redfish/v1/Systems/system"
        },{
            "@odata.id": "/redfish/v1/Chassis/chassis"
        }
    ]
  })json");

  MockExtractor mock_extractor;
  IdentifierExtractorIntf::ResourceIdentifier resource_identifier;
  resource_identifier.identifier =
      ParseTextProtoOrDie(R"pb(machine_devpath: "/phys")pb");
  EXPECT_CALL(mock_extractor, Extract(_, _))
      .WillOnce(Return(absl::InternalError("internal error")))
      .WillOnce(Return(resource_identifier));
  RelatedItemIdentifierExtractor extractor(&mock_extractor);

  RedpathNormalizer normalizer;
  std::unique_ptr<RedfishInterface> redfish_interface =
      server.RedfishClientInterface();
  IdentifierExtractorIntf::Deps deps{.interface = redfish_interface.get(),
                                     .normalizer = normalizer,
                                     .options = {}};

  RedfishVariant variant = redfish_interface->CachedGetUri(
      "/redfish/v1/Chassis/chassis/Sensors/"
      "sensor");
  std::unique_ptr<RedfishObject> obj = variant.AsObject();
  EXPECT_THAT(extractor.Extract(*obj, deps),
              ecclesia::IsOkAndHolds(Field(
                  &IdentifierExtractorIntf::ResourceIdentifier::identifier,
                  Optional(EqualsProto(R"pb(
                    machine_devpath: "/phys"
                  )pb")))));
}

TEST(RelatedItemIdentifierExtractorTest, NullRedfishInterface) {
  MockExtractor mock_extractor;
  RelatedItemIdentifierExtractor extractor(&mock_extractor);
  RedpathNormalizer normalizer;
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");

  std::unique_ptr<RedfishInterface> redfish_interface =
      server.RedfishClientInterface();
  IdentifierExtractorIntf::Deps deps{
      .interface = nullptr, .normalizer = normalizer, .options = {}};

  RedfishVariant variant =
      redfish_interface->CachedGetUri("/redfish/v1/Chassis/chassis");
  std::unique_ptr<RedfishObject> obj = variant.AsObject();
  EXPECT_THAT(extractor.Extract(*obj, deps),
              ecclesia::IsStatusInvalidArgument());
}

TEST(RelatedItemIdentifierExtractorTest, RelatedItemNotFound) {
  MockExtractor mock_extractor;
  RelatedItemIdentifierExtractor extractor(&mock_extractor);
  RedpathNormalizer normalizer;
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");

  std::unique_ptr<RedfishInterface> redfish_interface =
      server.RedfishClientInterface();
  IdentifierExtractorIntf::Deps deps{.interface = redfish_interface.get(),
                                     .normalizer = normalizer,
                                     .options = {}};

  RedfishVariant variant =
      redfish_interface->CachedGetUri("/redfish/v1/Chassis/chassis");
  std::unique_ptr<RedfishObject> obj = variant.AsObject();
  EXPECT_THAT(extractor.Extract(*obj, deps), ecclesia::IsStatusNotFound());
}

TEST(RelatedItemIdentifierExtractorTest, OdataIdNotFound) {
  MockExtractor mock_extractor;
  RelatedItemIdentifierExtractor extractor(&mock_extractor);
  RedpathNormalizer normalizer;
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");

  server.AddHttpGetHandlerWithOwnedData("/redfish/v1/Chassis/test_chassis",
                                        R"json({
 "RelatedItem": [
        {
            "not_odata_id": "/redfish/v1/Chassis/chassis"
        }
    ]
  })json");

  std::unique_ptr<RedfishInterface> redfish_interface =
      server.RedfishClientInterface();
  IdentifierExtractorIntf::Deps deps{.interface = redfish_interface.get(),
                                     .normalizer = normalizer,
                                     .options = {}};

  RedfishVariant variant =
      redfish_interface->CachedGetUri("/redfish/v1/Chassis/test_chassis");
  std::unique_ptr<RedfishObject> obj = variant.AsObject();
  EXPECT_THAT(extractor.Extract(*obj, deps), ecclesia::IsStatusNotFound());
}

TEST(RelatedItemIdentifierExtractorTest, OdataIdNotString) {
  MockExtractor mock_extractor;
  RelatedItemIdentifierExtractor extractor(&mock_extractor);
  RedpathNormalizer normalizer;
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");

  server.AddHttpGetHandlerWithOwnedData("/redfish/v1/Chassis/test_chassis",
                                        R"json({
 "RelatedItem": [
        {
            "@odata.id": 123
        }
    ]
  })json");

  std::unique_ptr<RedfishInterface> redfish_interface =
      server.RedfishClientInterface();
  IdentifierExtractorIntf::Deps deps{.interface = redfish_interface.get(),
                                     .normalizer = normalizer,
                                     .options = {}};

  RedfishVariant variant =
      redfish_interface->CachedGetUri("/redfish/v1/Chassis/test_chassis");
  std::unique_ptr<RedfishObject> obj = variant.AsObject();
  EXPECT_THAT(extractor.Extract(*obj, deps), ecclesia::IsStatusNotFound());
}

TEST(RelatedItemIdentifierExtractorTest, IdentifierNotFoundInRelatedItem) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  server.AddHttpGetHandlerWithOwnedData(
      "/redfish/v1/Chassis/chassis/Sensors/sensor",
      R"json({
    "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/sensor",
    "@odata.type": "#Sensor.v1_2_0.Sensor",
    "Id": "sensor",
    "Name": "CPU0",
    "Reading": 60.0,
    "ReadingUnits": "Cel",
    "ReadingType": "Temperature",
    "RelatedItem": [
        {
            "@odata.id": "/redfish/v1/Chassis/chassis"
        }
    ]
  })json");

  MockExtractor mock_extractor;
  EXPECT_CALL(mock_extractor, Extract(_, _))
      .WillOnce(Return(absl::NotFoundError("Identifier not found")));
  RelatedItemIdentifierExtractor extractor(&mock_extractor);

  RedpathNormalizer normalizer;
  std::unique_ptr<RedfishInterface> redfish_interface =
      server.RedfishClientInterface();
  IdentifierExtractorIntf::Deps deps{.interface = redfish_interface.get(),
                                     .normalizer = normalizer,
                                     .options = {}};

  RedfishVariant variant = redfish_interface->CachedGetUri(
      "/redfish/v1/Chassis/chassis/Sensors/"
      "sensor");
  std::unique_ptr<RedfishObject> obj = variant.AsObject();

  EXPECT_THAT(extractor.Extract(*obj, deps), ecclesia::IsStatusNotFound());
}

}  // namespace
}  // namespace ecclesia
