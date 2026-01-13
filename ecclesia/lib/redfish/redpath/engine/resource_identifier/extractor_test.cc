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

#include "ecclesia/lib/redfish/redpath/engine/resource_identifier/extractor.h"

#include <memory>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/engine/normalizer.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::ecclesia::EqualsProto;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Optional;
using ::testing::Return;
using ::testing::SetArgReferee;

// Mock Normalizer for testing.
class MockNormalizer : public RedpathNormalizer::ImplInterface {
 public:
  MOCK_METHOD(absl::Status, Normalize,
              (const RedfishObject& redfish_object,
               const DelliciusQuery::Subquery& query, QueryResultData& data_set,
               const RedpathNormalizerOptions& normalizer_options),
              (override));
  MOCK_METHOD(absl::Status, Normalize,
              (const DelliciusQuery& query, ecclesia::QueryResult& query_result,
               const RedpathNormalizerOptions& options),
              (override));
};

TEST(DefaultIdentifierExtractorTest, ExtractIdentifierSuccess) {
  DefaultIdentifierExtractor extractor;
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");

  auto mock_normalizer = std::make_unique<MockNormalizer>();
  QueryResultData result = ParseTextProtoOrDie(R"pb(
    fields {
      key: "_id_"
      value { identifier { machine_devpath: "/phys" } }
    }
  )pb");
  EXPECT_CALL(*mock_normalizer, Normalize(_, _, _, _))
      .WillOnce(DoAll(SetArgReferee<2>(result), Return(absl::OkStatus())));
  RedpathNormalizer normalizer;
  normalizer.AddRedpathNormalizer(std::move(mock_normalizer));

  std::unique_ptr<RedfishInterface> redfish_interface =
      server.RedfishClientInterface();
  IdentifierExtractorIntf::Deps deps{.interface = redfish_interface.get(),
                                     .normalizer = normalizer,
                                     .options = {}};

  RedfishVariant variant =
      redfish_interface->CachedGetUri("/redfish/v1/Chassis/chassis");
  std::unique_ptr<RedfishObject> obj = variant.AsObject();
  absl::StatusOr<IdentifierExtractorIntf::ResourceIdentifier>
      resource_identifier = extractor.Extract(*obj, deps);
  EXPECT_THAT(resource_identifier,
              ecclesia::IsOkAndHolds(Field(
                  &IdentifierExtractorIntf::ResourceIdentifier::identifier,
                  Optional(EqualsProto(R"pb(
                    machine_devpath: "/phys"
                  )pb")))));
}

TEST(DefaultIdentifierExtractorTest, NormalizeFails) {
  DefaultIdentifierExtractor extractor;
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  auto mock_normalizer = std::make_unique<MockNormalizer>();
  EXPECT_CALL(*mock_normalizer, Normalize(_, _, _, _))
      .WillOnce(Return(absl::InternalError("internal error")));
  RedpathNormalizer normalizer;
  normalizer.AddRedpathNormalizer(std::move(mock_normalizer));

  std::unique_ptr<RedfishInterface> redfish_interface =
      server.RedfishClientInterface();
  IdentifierExtractorIntf::Deps deps{.interface = redfish_interface.get(),
                                     .normalizer = normalizer,
                                     .options = {}};

  RedfishVariant variant = redfish_interface->CachedGetUri(
      "/redfish/v1/Chassis/chassis/Sensors/i_cpu0_t");
  std::unique_ptr<RedfishObject> obj = variant.AsObject();

  EXPECT_THAT(extractor.Extract(*obj, deps), IsStatusFailedPrecondition());
}

TEST(DefaultIdentifierExtractorTest, IdentifierNotFoundInDefault) {
  DefaultIdentifierExtractor extractor;
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");

  auto mock_normalizer = std::make_unique<MockNormalizer>();
  QueryResultData related_item_result = ParseTextProtoOrDie(R"pb(
    fields {
      key: "not_identifier"
      value { string_value: "blah blah" }
    }
  )pb");
  EXPECT_CALL(*mock_normalizer, Normalize(_, _, _, _))
      .WillOnce(DoAll(SetArgReferee<2>(related_item_result),
                      Return(absl::OkStatus())));
  RedpathNormalizer normalizer;
  normalizer.AddRedpathNormalizer(std::move(mock_normalizer));

  std::unique_ptr<RedfishInterface> redfish_interface =
      server.RedfishClientInterface();
  IdentifierExtractorIntf::Deps deps{.interface = redfish_interface.get(),
                                     .normalizer = normalizer,
                                     .options = {}};

  RedfishVariant variant = redfish_interface->CachedGetUri(
      "/redfish/v1/Chassis/chassis/Sensors/i_cpu0_t");
  std::unique_ptr<RedfishObject> obj = variant.AsObject();

  EXPECT_THAT(extractor.Extract(*obj, deps), IsStatusNotFound());
}

}  // namespace
}  // namespace ecclesia
