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

#include "ecclesia/lib/redfish/dellicius/engine/internal/query_planner.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "google/rpc/code.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/function_ref.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/engine/factory.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_errors.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner_devpath.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/topology.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/metrical_transport.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/lib/time/clock_fake.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/public/response_code_enum.h"

namespace ecclesia {

namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::Eq;
using ::testing::ByMove;

constexpr absl::string_view kQuerySamplesLocation =
    "lib/redfish/dellicius/query/samples";

constexpr absl::string_view kMockObject = R"json(
  {
    "Object": {
      "NestedObject": {
          "Property": 90
      }
    }
  }
)json";

// Test Redfish Object class with mockable Get().
class MockableGetRedfishObject : public RedfishObject {
 public:
  MockableGetRedfishObject() = default;
  MockableGetRedfishObject(const MockableGetRedfishObject &) = delete;
  RedfishVariant operator[](const std::string &node_name) const override {
    return RedfishVariant(
        absl::UnimplementedError("TestRedfishObject [] unsupported"));
  }
  nlohmann::json GetContentAsJson() const override { return kMockObject; }
  MOCK_METHOD(RedfishVariant, Get,
              (const std::string &node_name, GetParams params),
              (const, override));
  std::string DebugString() const override { return std::string(kMockObject); }
  std::optional<std::string> GetUriString() const override { return ""; }
  absl::StatusOr<std::unique_ptr<RedfishObject>> EnsureFreshPayload(
      GetParams params) override {
    return std::make_unique<MockableGetRedfishObject>();
  }
  void ForEachProperty(
      absl::FunctionRef<ecclesia::RedfishIterReturnValue(
          absl::string_view key, RedfishVariant value)> /*unused*/) override {}
};

// Test Redfish Object class with [] operator delgating to mocked Get() method.
class MockableIndexRedfishIterable : public RedfishIterable {
 public:
  MockableIndexRedfishIterable() = default;
  MockableIndexRedfishIterable(const MockableIndexRedfishIterable &) = delete;
  RedfishVariant operator[](int index) const override { return Get(index); }
  MOCK_METHOD(RedfishVariant, Get, (int index), (const));
  MOCK_METHOD(size_t, Size, (), (override));
  bool Empty() override { return false; }
};

// Test RedfishVariant Impl class with mockable AsObject() and AsIterable();
class MockableObjectRedfishVariantImpl : public RedfishVariant::ImplIntf {
 public:
  explicit MockableObjectRedfishVariantImpl(absl::string_view val)
      : str_value_(val) {}
  MockableObjectRedfishVariantImpl(const MockableObjectRedfishVariantImpl &) =
      delete;

  MOCK_METHOD(std::unique_ptr<RedfishObject>, AsObject, (), (const, override));
  MOCK_METHOD(std::unique_ptr<RedfishIterable>, AsIterable,
              (RedfishVariant::IterableMode mode, GetParams params),
              (const, override));

  std::optional<ecclesia::RedfishTransport::bytes> AsRaw() const override {
    return std::nullopt;
  }
  bool GetValue(std::string *val) const override { return false; }
  bool GetValue(int32_t *val) const override { return false; }
  bool GetValue(int64_t *val) const override { return false; }
  bool GetValue(double *val) const override { return false; }
  bool GetValue(bool *val) const override { return false; }
  bool GetValue(absl::Time *val) const override { return false; }
  std::string DebugString() const override { return str_value_; }
  CacheState IsFresh() const override { return CacheState::kUnknown; }

 private:
  std::string str_value_;
};

// Can't use FieldsAre to accept any struct in MockableGetRedfishObject::Get(),
// so we use custom matcher to return true for any GetParams struct.
MATCHER_P(AnyGetParams, get_param, "") { return true; }

class QueryPlannerTestRunner : public ::testing::Test {
 protected:
  QueryPlannerTestRunner() = default;
  void SetTestParams(absl::string_view mockup, absl::Time duration) {
    server_ = std::make_unique<FakeRedfishServer>(mockup);
    intf_ = server_->RedfishClientInterface();
    clock_ = std::make_unique<FakeClock>(duration);
  }

  void TestQuery(
      const std::string &query_in_path, const std::string &query_out_path,
      Normalizer *normalizer, bool check_timestamp = false,
      ExecutionFlags execution_flags = {
          .execution_mode = ExecutionFlags::ExecutionMode::kFailOnFirstError,
          .log_redfish_traces = false,
          .enable_url_annotation = false}) {
    CHECK(server_ != nullptr && intf_ != nullptr && clock_ != nullptr)
        << "Test parameters not set!";
    DelliciusQuery query =
        ParseTextFileAsProtoOrDie<DelliciusQuery>(query_in_path);
    absl::StatusOr<std::unique_ptr<QueryPlannerInterface>> qp =
        BuildDefaultQueryPlanner(query, RedPathRedfishQueryParams{},
                                 normalizer, nullptr);
    ASSERT_TRUE(qp.ok());
    absl::StatusOr<DelliciusQueryResult> query_result =
        (*qp)->Run(intf_->GetRoot(), *clock_, /*tracker=*/nullptr,
                   /*variables=*/{}, /*metrics=*/nullptr, execution_flags);
    ASSERT_TRUE(query_result.ok());
    DelliciusQueryResult intent_output =
        ParseTextFileAsProtoOrDie<DelliciusQueryResult>(query_out_path);
    if (!check_timestamp) {
      intent_output.clear_start_timestamp();
      intent_output.clear_end_timestamp();
      query_result.value().clear_start_timestamp();
      query_result.value().clear_end_timestamp();
    }
    EXPECT_THAT(*query_result, ecclesia::IgnoringRepeatedFieldOrdering(
                    ecclesia::EqualsProto(intent_output)));
  }

  std::unique_ptr<FakeRedfishServer> server_;
  std::unique_ptr<FakeClock> clock_;
  std::unique_ptr<RedfishInterface> intf_;
};

TEST_F(QueryPlannerTestRunner, CheckPredicatesFilterNodesAsExpected) {
  std::string sensor_in_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_in/sensor_in_predicates.textproto"));
  std::string sensor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation,
                    "query_out/legacy/legacy_sensor_out_predicates.textproto"));
  SetTestParams("indus_hmb_shim/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer.
  auto default_normalizer = BuildDefaultNormalizer();
  TestQuery(sensor_in_path, sensor_out_path, default_normalizer.get());
}

TEST_F(QueryPlannerTestRunner, CheckPredicatesFilterAncestorNodesAsExpected) {
  std::string processor_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/processors_in.textproto"));
  std::string processor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation,
                    "query_out/legacy/legacy_processors_out.textproto"));
  SetTestParams("indus_hmb_cn/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer.
  auto default_normalizer = BuildDefaultNormalizer();
  TestQuery(processor_in_path, processor_out_path, default_normalizer.get());
}

TEST_F(QueryPlannerTestRunner, BasicDelliciusInterpreter) {
  std::string assembly_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/assembly_in.textproto"));
  std::string sensor_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/sensor_in.textproto"));
  std::string assembly_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/legacy/legacy_assembly_out.textproto"));
  std::string sensor_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/legacy/legacy_sensor_out.textproto"));
  SetTestParams("indus_hmb_shim/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer.
  auto default_normalizer = BuildDefaultNormalizer();
  // Query Assembly
  TestQuery(assembly_in_path, assembly_out_path, default_normalizer.get());
  // Query Sensor
  TestQuery(sensor_in_path, sensor_out_path, default_normalizer.get());
}

TEST_F(QueryPlannerTestRunner, DefaultNormalizerWithDevpaths) {
  std::string sensor_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/sensor_in.textproto"));
  std::string sensor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation,
                    "query_out/legacy/legacy_devpath_sensor_out.textproto"));
  SetTestParams("indus_hmb_shim/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer with devpath extension.
  auto normalizer_with_devpath = BuildDefaultNormalizerWithLocalDevpath(
      CreateTopologyFromRedfish(intf_.get()));
  // Query Sensor
  TestQuery(sensor_in_path, sensor_out_path, normalizer_with_devpath.get());
}

TEST_F(QueryPlannerTestRunner, DefaultNormalizerWithUrlAnnotations) {
  std::string sensor_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/sensor_in.textproto"));
  std::string sensor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation,
                    "query_out/legacy/legacy_annotation_sensor_out.textproto"));
  SetTestParams("indus_hmb_shim/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer with devpath extension.
  auto default_normalizer = BuildDefaultNormalizer();
  // Query Sensor
  TestQuery(sensor_in_path, sensor_out_path, default_normalizer.get(),
            /*check_timestamp=*/false,
            {.execution_mode = ExecutionFlags::ExecutionMode::kFailOnFirstError,
             .log_redfish_traces = false,
             .enable_url_annotation = true});
}

TEST_F(QueryPlannerTestRunner,
       CheckChildSubqueryOutputCorrectlyGroupsUnderParent) {
  std::string query_in_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_in/sensor_in_links.textproto"));
  std::string query_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation,
                    "query_out/legacy/legacy_sensor_out_links.textproto"));
  SetTestParams("indus_hmb_shim/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer with devpath extension.
  auto normalizer_with_devpath = BuildDefaultNormalizerWithLocalDevpath(
      CreateTopologyFromRedfish(intf_.get()));
  // Query Sensor
  TestQuery(query_in_path, query_out_path, normalizer_with_devpath.get());
}

TEST_F(QueryPlannerTestRunner, TestNestedNodeNameInQueryProperty) {
  std::string query_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/managers_in.textproto"));
  std::string query_out_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_out/legacy/legacy_managers_out.textproto"));
  SetTestParams("indus_hmb_cn/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer.
  auto default_normalizer = BuildDefaultNormalizer();
  TestQuery(query_in_path, query_out_path, default_normalizer.get());
}

TEST_F(QueryPlannerTestRunner, TestServiceRootQuery) {
  std::string query_in_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_in/service_root_in.textproto"));
  std::string query_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation,
                    "query_out/legacy/legacy_service_root_out.textproto"));
  SetTestParams("indus_hmb_cn/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer.
  auto default_normalizer = BuildDefaultNormalizer();
  TestQuery(query_in_path, query_out_path, default_normalizer.get());
}

TEST(QueryPlannerTest, CheckQueryPlannerInitFailsWithInvalidSubqueryLinks) {
  std::string query_in_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_in/malformed_query_links.textproto"));
  DelliciusQuery query_sensor =
      ParseTextFileAsProtoOrDie<DelliciusQuery>(query_in_path);
  auto default_normalizer = BuildDefaultNormalizer();
  absl::StatusOr<std::unique_ptr<QueryPlannerInterface>> qps =
      BuildDefaultQueryPlanner(query_sensor, RedPathRedfishQueryParams{},
                               default_normalizer.get());
  EXPECT_FALSE(qps.ok());
}

TEST(QueryPlannerTest,
     CheckQueryPlannerInitFailsWithMalforedRedPathsInSubqueries) {
  std::string query_in_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_in/malformed_query.textproto"));
  DelliciusQuery query_input_proto =
      ParseTextFileAsProtoOrDie<DelliciusQuery>(query_in_path);
  auto default_normalizer = BuildDefaultNormalizer();
  absl::StatusOr<std::unique_ptr<QueryPlannerInterface>> qp =
      BuildDefaultQueryPlanner(query_input_proto, RedPathRedfishQueryParams{},
                               default_normalizer.get());
  EXPECT_FALSE(qp.ok());
}

TEST(QueryPlannerTest, CheckQueryPlannerSendsOneRequestForEachUri) {
  std::string sensor_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/sensor_in.textproto"));
  std::string sensor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation,
                    "query_out/legacy/legacy_devpath_sensor_out.textproto"));
  FakeClock clock(absl::FromUnixSeconds(10));
  // Set up context node for dellicius query.
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  // Instantiate a passthrough normalizer.
  auto default_normalizer = BuildDefaultNormalizer();

  // Create metrical transport and issue queries.
  std::unique_ptr<RedfishTransport> base_transport =
      server.RedfishClientTransport();
  auto transport = std::make_unique<MetricalRedfishTransport>(
      std::move(base_transport), Clock::RealClock());
  // This metrics object is owned and populated by the metrical transport. Use
  // it to populate metrics in the QueryPlanner result.
  const RedfishMetrics *metrics = transport->GetConstMetrics();
  ASSERT_NE(metrics, nullptr);
  auto cache = std::make_unique<NullCache>(transport.get());
  auto intf = NewHttpInterface(std::move(transport), std::move(cache),
                               RedfishInterface::kTrusted);
  auto service_root = intf->GetRoot();

  // Query Sensor
  DelliciusQuery query_sensor =
      ParseTextFileAsProtoOrDie<DelliciusQuery>(sensor_in_path);
  auto qps = BuildDefaultQueryPlanner(query_sensor, RedPathRedfishQueryParams{},
                                      default_normalizer.get());
  ASSERT_TRUE(qps.ok());
  DelliciusQueryResult result_sensor =
      (*qps)->Run(service_root, clock, nullptr, {}, metrics);
  // Metrics should be auto-populated in the query result.
  ASSERT_TRUE(result_sensor.has_redfish_metrics());
  // For each type of redfish request for each URI, validate that the
  // QueryPlanner sends only 1 request.
  for (const auto &uri_x_metric :
       *result_sensor.mutable_redfish_metrics()->mutable_uri_to_metrics_map()) {
    for (const auto &metadata :
         uri_x_metric.second.request_type_to_metadata()) {
      EXPECT_EQ(metadata.second.request_count(), 1);
    }
  }
}

TEST(QueryPlannerTest, CheckQueryPlannerStopsQueryingOnTransportError) {
  std::string sensor_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/sensor_in.textproto"));
  std::string sensor_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation,
                    "query_out/legacy/legacy_devpath_sensor_out.textproto"));
  FakeClock clock(absl::FromUnixSeconds(10));
  // Set up context node for dellicius query.
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  // Instantiate a passthrough normalizer.
  auto default_normalizer = BuildDefaultNormalizer();

  // Create metrical transport and issue queries.
  std::unique_ptr<RedfishTransport> base_transport =
      std::make_unique<NullTransport>();
  auto transport = std::make_unique<MetricalRedfishTransport>(
      std::move(base_transport), Clock::RealClock());

  // This metrics object is owned and populated by the metrical transport. Use
  // it to populate metrics in the QueryPlanner result.
  const RedfishMetrics *metrics = transport->GetConstMetrics();
  ASSERT_NE(metrics, nullptr);

  auto cache = std::make_unique<NullCache>(transport.get());
  auto intf = NewHttpInterface(std::move(transport), std::move(cache),
                               RedfishInterface::kTrusted);
  auto service_root = intf->GetRoot();

  // Query Sensor
  DelliciusQuery query_sensor =
      ParseTextFileAsProtoOrDie<DelliciusQuery>(sensor_in_path);
  absl::StatusOr<std::unique_ptr<QueryPlannerInterface>> qps =
      BuildDefaultQueryPlanner(query_sensor, RedPathRedfishQueryParams{},
                               default_normalizer.get());
  ASSERT_TRUE(qps.ok());
  absl::StatusOr<DelliciusQueryResult> result_sensor =
      (*qps)->Run(service_root, clock, nullptr, {}, metrics);
  ASSERT_TRUE(result_sensor.ok());
  EXPECT_THAT((*result_sensor).status().code(),
              Eq(::google::rpc::Code::FAILED_PRECONDITION));

  // Metrics should be auto-populated in the query result.
  ASSERT_TRUE(result_sensor->has_redfish_metrics());
  // Validate that no attempt was made by query planner to query redfish service
  // Redfish Metrics should indicate 1 failed GET request to service root which
  // is sent before running the query planner.
  EXPECT_EQ(metrics->uri_to_metrics_map().size(), 1);
  EXPECT_TRUE(metrics->uri_to_metrics_map().contains("/redfish/v1"));
  EXPECT_EQ(metrics->uri_to_metrics_map()
                .at("/redfish/v1")
                .request_type_to_metadata_failures_size(),
            1);
  EXPECT_TRUE(metrics->uri_to_metrics_map()
                  .at("/redfish/v1")
                  .request_type_to_metadata_failures()
                  .contains("GET"));
  EXPECT_EQ(metrics->uri_to_metrics_map()
                .at("/redfish/v1")
                .request_type_to_metadata_size(),
            0);
}

TEST_F(QueryPlannerTestRunner, CheckSubqueryErrorsPopulated) {
  std::string query_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/sensor_in.textproto"));
  SetTestParams("indus_hmb_shim/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer with devpath extension.
  auto normalizer_with_devpath = BuildDefaultNormalizerWithLocalDevpath(
      CreateTopologyFromRedfish(intf_.get()));
  // Create Query Planner for Sensors query.
  DelliciusQuery query =
      ParseTextFileAsProtoOrDie<DelliciusQuery>(query_in_path);
  absl::StatusOr<std::unique_ptr<QueryPlannerInterface>> qp =
      BuildDefaultQueryPlanner(query, RedPathRedfishQueryParams{},
                               normalizer_with_devpath.get());
  ASSERT_TRUE(qp.ok());
  // Create mock RedfishVariant to return deadline exceeded error, and Redfish
  // Object that will return it when Redfish request is issued.
  std::unique_ptr<MockableGetRedfishObject> mock_rf_obj =
      std::make_unique<MockableGetRedfishObject>();
  // Mock Get() call one for Chassis node in the redpath.
  EXPECT_CALL(*mock_rf_obj, Get("Chassis", AnyGetParams(_)))
      .WillOnce(Return(ByMove(
          RedfishVariant(absl::DeadlineExceededError("deadline exceeded")))));
  // Create context node that will return the mocked Redfish Object.
  std::unique_ptr<MockableObjectRedfishVariantImpl> mock_context_node_variant =
      std::make_unique<MockableObjectRedfishVariantImpl>("test");
  EXPECT_CALL(*mock_context_node_variant, AsObject())
      .WillOnce(Return(ByMove(std::move(mock_rf_obj))));
  RedfishVariant mock_context_node(std::move(mock_context_node_variant));
  // Run the query and ensure the subquery responses has the status populated
  // with the right error.
  absl::StatusOr<DelliciusQueryResult> query_result =
      (*qp)->Run(mock_context_node, *clock_, nullptr, {});
  ASSERT_TRUE(query_result.ok());
  for (const auto &[id, subquery_output] :
       query_result.value().subquery_output_by_id()) {
    EXPECT_THAT(subquery_output.status().code(),
                Eq(::google::rpc::Code::DEADLINE_EXCEEDED));
  }
  // Validate Error Summary in DelliciusQueryResult
  EXPECT_THAT(query_result->query_errors().subquery_id_to_error_summary(),
              testing::SizeIs(1));
  for (const auto &[subquery_id, subquery_error] :
       query_result->query_errors().subquery_id_to_error_summary()) {
    EXPECT_THAT(subquery_error.node_name(), Eq("Chassis"));
    EXPECT_TRUE(
        absl::StrContains(query_result->query_errors().overall_error_summary(),
                          "DEADLINE_EXCEEDED error occurred"));
  }
  // Ensure the top level QueryResult status also reflects the error.
  EXPECT_THAT(query_result->status().code(),
              Eq(::google::rpc::Code::DEADLINE_EXCEEDED));
}

TEST_F(QueryPlannerTestRunner, CheckSubqueryErrorsPopulatedCollectionResource) {
  std::string query_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/sensor_in.textproto"));
  SetTestParams("indus_hmb_shim/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer with devpath extension.
  auto normalizer_with_devpath = BuildDefaultNormalizerWithLocalDevpath(
      CreateTopologyFromRedfish(intf_.get()));
  // Create Query Planner for Sensors query.
  DelliciusQuery query =
      ParseTextFileAsProtoOrDie<DelliciusQuery>(query_in_path);
  absl::StatusOr<std::unique_ptr<QueryPlannerInterface>> qp =
      BuildDefaultQueryPlanner(query, RedPathRedfishQueryParams{},
                               normalizer_with_devpath.get());
  ASSERT_TRUE(qp.ok());

  // Create mock RedfishVariant to return unauthenticated error for the
  // collection request, and Redfish Object that will return it when Redfish
  // request is issued.
  std::unique_ptr<MockableIndexRedfishIterable> mock_rf_iterable =
      std::make_unique<MockableIndexRedfishIterable>();
  // Mock Get() call to return variant with unauthenticated error status.
  EXPECT_CALL(*mock_rf_iterable, Size()).WillOnce(Return(1));
  EXPECT_CALL(*mock_rf_iterable, Get(_))
      .WillOnce(Return(ByMove(
          RedfishVariant(absl::UnauthenticatedError("unauthenticated")))));

  // Create mock RedfishVariant that return a variant with ok status on Get()
  // and returns the mocked RedfishIterable on AsIterable().
  std::unique_ptr<MockableObjectRedfishVariantImpl> mock_ok_rf_variant_impl =
      std::make_unique<MockableObjectRedfishVariantImpl>("test");
  EXPECT_CALL(*mock_ok_rf_variant_impl, AsIterable(_, _))
      .WillOnce(Return(ByMove(std::move(mock_rf_iterable))));
  RedfishVariant ok_rf_variant(std::move(mock_ok_rf_variant_impl));
  std::unique_ptr<MockableGetRedfishObject> mock_ok_rf_obj =
      std::make_unique<MockableGetRedfishObject>();
  // Mock Get() call one for Chassis node in the redpath.
  EXPECT_CALL(*mock_ok_rf_obj, Get("Chassis", AnyGetParams(_)))
      .WillOnce(Return(ByMove(std::move(ok_rf_variant))));

  // Create context node to return the mocked Redfish Object.
  std::unique_ptr<MockableObjectRedfishVariantImpl> mock_context_node_variant =
      std::make_unique<MockableObjectRedfishVariantImpl>("test");
  EXPECT_CALL(*mock_context_node_variant, AsObject())
      .WillOnce(Return(ByMove(std::move(mock_ok_rf_obj))));
  RedfishVariant mock_context_node(std::move(mock_context_node_variant));

  // Run the query and ensure the subquery responses has the status populated
  // with the right error.
  absl::StatusOr<DelliciusQueryResult> query_result =
      (*qp)->Run(mock_context_node, *clock_, nullptr, {});
  ASSERT_TRUE(query_result.ok());
  for (const auto &[id, subquery_output] :
       query_result.value().subquery_output_by_id()) {
    EXPECT_THAT(subquery_output.status().code(),
                Eq(::google::rpc::Code::UNAUTHENTICATED));
  }
  // Ensure the top level QueryResult status also reflects the error.
  EXPECT_THAT(query_result->status().code(),
              Eq(::google::rpc::Code::UNAUTHENTICATED));
}

TEST_F(QueryPlannerTestRunner, CheckSubqueryErrorHaltsExecution) {
  // The assembly query has two root subqueries, This tests that only one of
  // them should execute. After encountering an error on the first Subquery, the
  // execution will halt, and there will be no more calls to the mock
  // RedfishObject. If execution doesn't halt, this test should fail with an
  // error related to calls to the mock RedfishObject not being defined.
  std::string query_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/assembly_in.textproto"));
  SetTestParams("indus_hmb_shim/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer with devpath extension.
  auto normalizer_with_devpath = BuildDefaultNormalizerWithLocalDevpath(
      CreateTopologyFromRedfish(intf_.get()));
  // Create Query Planner.
  DelliciusQuery query =
      ParseTextFileAsProtoOrDie<DelliciusQuery>(query_in_path);
  absl::StatusOr<std::unique_ptr<QueryPlannerInterface>> qp =
      BuildDefaultQueryPlanner(query, RedPathRedfishQueryParams{},
                               normalizer_with_devpath.get());
  ASSERT_TRUE(qp.ok());
  // Create mock RedfishVariant to return deadline exceeded error when Systems
  // is queried.
  std::unique_ptr<MockableGetRedfishObject> mock_rf_obj =
      std::make_unique<MockableGetRedfishObject>();
  // Mock any GET call to return error once. The QueryPlanner builds a map from
  // the node name to redpath context, and iterates over it to issue GETs.
  // Therefore the order is indeterminate and we can't just mock the exact
  // behavior for a GET to a specific node, when we expect execution to abort
  // before another node is requested.
  EXPECT_CALL(*mock_rf_obj, Get(AnyGetParams(_), AnyGetParams(_)))
      .WillOnce(Return(ByMove(
          RedfishVariant(absl::DeadlineExceededError("deadline exceeded")))));
  // Create context node that will return the mocked Redfish Object.
  std::unique_ptr<MockableObjectRedfishVariantImpl> mock_context_node_variant =
      std::make_unique<MockableObjectRedfishVariantImpl>("test");
  EXPECT_CALL(*mock_context_node_variant, AsObject())
      .WillOnce(Return(ByMove(std::move(mock_rf_obj))));
  RedfishVariant mock_context_node(std::move(mock_context_node_variant));
  // Run the query and ensure the status is populated. Runs with
  // execution_mode = kFailOnFirstError by default.
  absl::StatusOr<DelliciusQueryResult> query_result =
      (*qp)->Run(mock_context_node, *clock_, nullptr, {});
  ASSERT_TRUE(query_result.ok());
  EXPECT_THAT(query_result->status().code(),
              Eq(::google::rpc::Code::DEADLINE_EXCEEDED));
}

TEST_F(QueryPlannerTestRunner, CheckQueryHaltsIfErrorWithinOneSubquery) {
  // The sensor query has just one subquery, that normally returns a lot of
  // sensor entities. This test checks that if an error is encountered when
  // executing the redpath step for one context node (one processor entity),
  // the execution halts early and the query result has the error status.
  std::string query_in_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_in/all_processors_in.textproto"));
  SetTestParams("indus_hmb_shim/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer with devpath extension.
  auto normalizer_with_devpath = BuildDefaultNormalizerWithLocalDevpath(
      CreateTopologyFromRedfish(intf_.get()));
  // Create Query Planner.
  DelliciusQuery query =
      ParseTextFileAsProtoOrDie<DelliciusQuery>(query_in_path);
  absl::StatusOr<std::unique_ptr<QueryPlannerInterface>> qp =
      BuildDefaultQueryPlanner(query, RedPathRedfishQueryParams{},
                               normalizer_with_devpath.get());
  // We need to test that the query planner halts execution on the FIRST error.
  // We mock the first two processors to return different errors. In the QP,
  // we set the error status based on the last (which should be the only) error.
  // If QP doesn't halt on the first error, the query will continue to execute
  // and the status will be set to the second error.
  // GATEWAY_TO corresponds to DEADLINE_EXCEEDED.
  server_->AddHttpGetHandlerWithStatus(
      "/redfish/v1/Systems/system/Processors/0", "",
      tensorflow::serving::net_http::HTTPStatusCode::GATEWAY_TO);
  server_->AddHttpGetHandlerWithStatus(
      "/redfish/v1/Systems/system/Processors/1", "",
      tensorflow::serving::net_http::HTTPStatusCode::SERVICE_UNAV);
  ASSERT_TRUE(qp.ok());
  absl::StatusOr<DelliciusQueryResult> query_result = (*qp)->Run(
      intf_->GetRoot(), *clock_, /*tracker=*/nullptr,
      /*variables=*/{}, /*metrics=*/nullptr,
      {.execution_mode = ExecutionFlags::ExecutionMode::kFailOnFirstError,
       .log_redfish_traces = false,
       .enable_url_annotation = false});
  ASSERT_TRUE(query_result.ok());
  EXPECT_THAT(query_result->status().code(),
              Eq(google::rpc::Code::DEADLINE_EXCEEDED));
}

TEST_F(QueryPlannerTestRunner, CheckUnresolvedNodeIsNotAnError) {
  std::string query_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/managers_in.textproto"));
  SetTestParams("indus_hmb_shim/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer with devpath extension.
  auto normalizer_with_devpath = BuildDefaultNormalizerWithLocalDevpath(
      CreateTopologyFromRedfish(intf_.get()));
  // Create Query Planner.
  DelliciusQuery query =
      ParseTextFileAsProtoOrDie<DelliciusQuery>(query_in_path);
  absl::StatusOr<std::unique_ptr<QueryPlannerInterface>> qp =
      BuildDefaultQueryPlanner(query, RedPathRedfishQueryParams{},
                               normalizer_with_devpath.get());
  ASSERT_TRUE(qp.ok());
  // Create mock RedfishVariant to return not found error
  std::unique_ptr<MockableGetRedfishObject> mock_rf_obj =
      std::make_unique<MockableGetRedfishObject>();
  EXPECT_CALL(*mock_rf_obj, Get(AnyGetParams(_), AnyGetParams(_)))
      .WillOnce(Return(ByMove(
          RedfishVariant(absl::NotFoundError("node not found")))));
  // Create context node that will return the mocked Redfish Object.
  std::unique_ptr<MockableObjectRedfishVariantImpl> mock_context_node_variant =
      std::make_unique<MockableObjectRedfishVariantImpl>("test");
  EXPECT_CALL(*mock_context_node_variant, AsObject())
      .WillOnce(Return(ByMove(std::move(mock_rf_obj))));
  RedfishVariant mock_context_node(std::move(mock_context_node_variant));
  absl::StatusOr<DelliciusQueryResult> query_result =
      (*qp)->Run(mock_context_node, *clock_, nullptr, {});
  // Ensure that after encountering a NOT_FOUND error, the query status is OK,
  // and no error summaries are populated.
  ASSERT_TRUE(query_result.ok());
  ASSERT_THAT(query_result->status().code(), Eq(::google::rpc::Code::OK));
  ASSERT_THAT(query_result->query_errors().subquery_id_to_error_summary(),
              testing::SizeIs(0));
}


TEST_F(QueryPlannerTestRunner, CheckSubqueryErrorDoesntHaltExecutionIfDesired) {
  std::string query_in_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation, "query_in/assembly_in.textproto"));
  SetTestParams("indus_hmb_shim/mockup.shar", absl::FromUnixSeconds(10));
  // Instantiate a passthrough normalizer with devpath extension.
  auto normalizer_with_devpath = BuildDefaultNormalizerWithLocalDevpath(
      CreateTopologyFromRedfish(intf_.get()));
  // Create Query Planner.
  DelliciusQuery query =
      ParseTextFileAsProtoOrDie<DelliciusQuery>(query_in_path);
  absl::StatusOr<std::unique_ptr<QueryPlannerInterface>> qp =
      BuildDefaultQueryPlanner(query, RedPathRedfishQueryParams{},
                               normalizer_with_devpath.get());
  ASSERT_TRUE(qp.ok());
  // Create mock RedfishVariant to return deadline exceeded error when Systems
  // is queried.
  std::unique_ptr<MockableGetRedfishObject> mock_rf_obj =
      std::make_unique<MockableGetRedfishObject>();
  // Since query execution should NOT halt, expect both subqueries are
  // executed, even though they return errors.
  EXPECT_CALL(*mock_rf_obj, Get("Systems", AnyGetParams(_)))
      .WillRepeatedly(Return(ByMove(
          RedfishVariant(absl::DeadlineExceededError("deadline exceeded")))));
  EXPECT_CALL(*mock_rf_obj, Get("Chassis", AnyGetParams(_)))
      .WillRepeatedly(Return(ByMove(
          RedfishVariant(absl::DeadlineExceededError("deadline exceeded")))));
  // Create context node that will return the mocked Redfish Object.
  std::unique_ptr<MockableObjectRedfishVariantImpl> mock_context_node_variant =
      std::make_unique<MockableObjectRedfishVariantImpl>("test");
  EXPECT_CALL(*mock_context_node_variant, AsObject())
      .WillOnce(Return(ByMove(std::move(mock_rf_obj))));
  RedfishVariant mock_context_node(std::move(mock_context_node_variant));
  // Run query with execution_mode=kContinueOnSubqueryErrors; ensure the status
  // is populated.
  absl::StatusOr<DelliciusQueryResult> query_result =
      (*qp)->Run(mock_context_node, *clock_, nullptr, {}, nullptr,
                 {ExecutionFlags::ExecutionMode::kContinueOnSubqueryErrors});
  ASSERT_TRUE(query_result.ok());
  EXPECT_THAT(query_result->status().code(),
              Eq(::google::rpc::Code::DEADLINE_EXCEEDED));
}

TEST_F(QueryPlannerTestRunner,
       NormalizerPrioritizesLocalDevpathForMachineDevpath) {
  std::string query_in_path = GetTestDataDependencyPath(JoinFilePaths(
      kQuerySamplesLocation, "query_in/all_processors_in.textproto"));
  std::string query_out_path = GetTestDataDependencyPath(
      JoinFilePaths(kQuerySamplesLocation,
                    "query_out/legacy/legacy_sensor_out_links.textproto"));
  SetTestParams("indus_hmb_shim/mockup.shar", absl::FromUnixSeconds(10));

  absl::flat_hash_set<std::string> expected_machine_devpaths = {
      "/phys/PEO/IO0/CPU0", "/phys/PEO/IO0/CPU1"};
  // Instantiate a normalizer with machine devpath from local devpath.
  absl::flat_hash_map<std::string, std::string> devpath_map = {
      {"/phys/CPU0", "/phys/PEO/IO0/CPU0"},
      {"/phys/CPU1", "/phys/PEO/IO0/CPU1"}};
  std::unique_ptr<IdAssigner> id_assigner =
      NewMapBasedDevpathAssigner(devpath_map);

  // MapBasedDevpathAssigner should only model local devpath to machine devpath
  // mappings. RedfishLocation should not be used when prioritizing local
  // devpath, trying to use it will throw an error as it using it for machine
  // devpath unimplemented.
  std::unique_ptr<Normalizer> normalizer_with_devpath =
      BuildNormalizerWithMachineDevpath(std::move(id_assigner),
                                        CreateTopologyFromRedfish(intf_.get()));
  // Create the QueryPlanner, issue the query, and check that the devpaths
  // were populated correctly.
  DelliciusQuery query =
      ParseTextFileAsProtoOrDie<DelliciusQuery>(query_in_path);
  absl::StatusOr<std::unique_ptr<QueryPlannerInterface>> qp =
      BuildDefaultQueryPlanner(query, RedPathRedfishQueryParams{},
                               normalizer_with_devpath.get(), nullptr);
  ASSERT_TRUE(qp.ok());
  absl::StatusOr<DelliciusQueryResult> query_result =
      (*qp)->Run(intf_->GetRoot(), *clock_, /*tracker=*/nullptr,
                 /*variables=*/{});
  absl::flat_hash_set<std::string> actual_machine_devpaths;
  ASSERT_TRUE(query_result.ok());
  const auto it = query_result->subquery_output_by_id().find("Processors");
  ASSERT_TRUE(it != query_result->subquery_output_by_id().end());
  for (const SubqueryDataSet &data_set : it->second.data_sets()) {
    actual_machine_devpaths.insert(data_set.decorators().machine_devpath());
  }
  ASSERT_THAT(actual_machine_devpaths,
              testing::UnorderedElementsAreArray(expected_machine_devpaths));
}

}  // namespace

}  // namespace ecclesia
