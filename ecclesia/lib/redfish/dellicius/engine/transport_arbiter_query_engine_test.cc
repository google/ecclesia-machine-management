/*
 * Copyright 2025 Google LLC
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

#include "ecclesia/lib/redfish/dellicius/engine/transport_arbiter_query_engine.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/passkey.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/dellicius/engine/test_queries_embedded.h"
#include "ecclesia/lib/redfish/dellicius/engine/test_query_rules_embedded.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner_devpath.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_engine_features.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_spec.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/redpath_subscription.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/transport/http.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/status/test_macros.h"
#include "ecclesia/lib/stubarbiter/arbiter.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/lib/time/clock_fake.h"
#include "tensorflow_serving/util/net_http/public/response_code_enum.h"

namespace ecclesia {

namespace {

constexpr absl::string_view kQuerySamplesLocation =
    "lib/redfish/dellicius/query/samples";
constexpr absl::string_view kIndusMockup = "indus_hmb_shim/mockup.shar";
constexpr absl::Time clock_time = absl::FromUnixSeconds(10);

using PriorityLabel = StubArbiterInfo::PriorityLabel;

void RemoveTimestamps(QueryIdToResult &entries) {
  for (auto &[query_id, entry] : *entries.mutable_results()) {
    entry.mutable_stats()->clear_start_time();
    entry.mutable_stats()->clear_end_time();
    for (auto &[uri, metadata] : *entry.mutable_stats()
                                      ->mutable_redfish_metrics()
                                      ->mutable_uri_to_metrics_map()) {
      metadata.mutable_request_type_to_metadata()->clear();
    }
  }
}

void RemoveStats(QueryIdToResult &entries) {
  for (auto &[query_id, entry] : *entries.mutable_results()) {
    entry.clear_stats();
  }
}

void VerifyQueryResults(QueryIdToResult actual_entries,
                        QueryIdToResult expected_entries) {
  RemoveTimestamps(actual_entries);
  RemoveTimestamps(expected_entries);

  RemoveStats(actual_entries);
  RemoveStats(expected_entries);

  EXPECT_THAT(actual_entries, EqualsProto(expected_entries));
}

absl::StatusOr<std::unique_ptr<QueryEngineIntf>>
GetQueryEngineWithTransportArbiter(
    FakeRedfishServer &server,
    absl::Span<const EmbeddedFile> query_files = kDelliciusQueries,
    absl::Span<const EmbeddedFile> query_rules = kQueryRules,
    const Clock *clock = Clock::RealClock(),
    QueryEngineWithTransportArbiter::Params query_engine_params =
        {
            .features = StandardQueryEngineFeatures(),
        },
    std::optional<std::function<
        absl::StatusOr<std::unique_ptr<RedfishTransport>>(PriorityLabel)>>
        transport_factory = std::nullopt) {
  // Set up Query Engine with ID assigner. This is needed for the normalizer to
  // assign the stable id fields.
  absl::flat_hash_map<std::string, std::string> devpath_map = {};
  std::unique_ptr<IdAssigner> id_assigner =
      NewMapBasedDevpathAssigner(devpath_map);

  if (transport_factory.has_value()) {
    query_engine_params.transport_factory = std::move(*transport_factory);
  } else {
    query_engine_params.transport_factory = [&](PriorityLabel label)
        -> absl::StatusOr<std::unique_ptr<RedfishTransport>> {
      FakeRedfishServer::Config config = server.GetConfig();
      auto http_client = std::make_unique<CurlHttpClient>(
          LibCurlProxy::CreateInstance(), HttpCredential{});
      std::string network_endpoint =
          absl::StrFormat("%s:%d", config.hostname, config.port);
      return HttpRedfishTransport::MakeNetwork(std::move(http_client),
                                               network_endpoint);
    };
  }

  ECCLESIA_ASSIGN_OR_RETURN(
      QuerySpec query_spec,
      QuerySpec::FromQueryContext({.query_files = query_files,
                                   .query_rules = query_rules,
                                   .clock = clock}));

  return QueryEngineWithTransportArbiter::CreateTransportArbiterQueryEngine(
      query_spec, std::move(query_engine_params), std::move(id_assigner));
}

TEST(QueryEngineTest, CreateQueryEngineWithTransportArbiter) {
  FakeRedfishServer server(kIndusMockup);
  EXPECT_THAT(GetQueryEngineWithTransportArbiter(server), IsOk());
}

TEST(QueryEngineTest, FailToCreateArbiter) {
  FakeRedfishServer server(kIndusMockup);

  ECCLESIA_ASSIGN_OR_FAIL(QuerySpec query_spec,
                          QuerySpec::FromQueryContext({
                              .query_files = kDelliciusQueries,
                          }));
  EXPECT_THAT(
      QueryEngineWithTransportArbiter::CreateTransportArbiterQueryEngine(
          query_spec,
          {.transport_arbiter_type = StubArbiterInfo::Type::kUnknown}),
      IsStatusInvalidArgument());
}

TEST(QueryEngineTest, FailToCreateQueryEngineWithNoStreamingFeature) {
  FakeRedfishServer server(kIndusMockup);

  ECCLESIA_ASSIGN_OR_FAIL(QuerySpec query_spec,
                          QuerySpec::FromQueryContext({
                              .query_files = kDelliciusQueries,
                          }));

  QueryEngineFeatures features;
  features.set_enable_streaming(false);
  EXPECT_THAT(
      QueryEngineWithTransportArbiter::CreateTransportArbiterQueryEngine(
          query_spec,
          {
              .features = std::move(features),
          }),
      IsStatusInvalidArgument());
}

TEST(QueryEngineTest, ExecuteQueryWithTransportArbiter) {
  QueryIdToResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(
          GetTestDataDependencyPath(JoinFilePaths(
              kQuerySamplesLocation, "query_out/sensor_out.textproto")));

  FakeRedfishServer server(kIndusMockup);
  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<QueryEngineIntf> query_engine,
                          GetQueryEngineWithTransportArbiter(server));
  QueryIdToResult response_entries =
      query_engine->ExecuteRedpathQuery({"SensorCollector"});

  auto results = response_entries.results().at("SensorCollector").data();
  auto sensors = results.fields().at("Sensors");
  EXPECT_EQ(sensors.list_value().values_size(), 14);

  *intent_output_sensor.mutable_results()
       ->at("SensorCollector")
       .mutable_status()
       ->add_transport_code() = ParseTextProtoOrDie(R"pb(
    transport_priority: TRANSPORT_PRIORITY_PRIMARY
    error_code: ERROR_NONE
  )pb");
  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_output_sensor)});
}

TEST(QueryEngineTest, ExecuteQueryWithManualTypeArbiter) {
  QueryIdToResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(
          GetTestDataDependencyPath(JoinFilePaths(
              kQuerySamplesLocation, "query_out/sensor_out.textproto")));

  FakeRedfishServer server(kIndusMockup);
  FakeClock clock{clock_time};
  ECCLESIA_ASSIGN_OR_FAIL(
      std::unique_ptr<QueryEngineIntf> query_engine,
      GetQueryEngineWithTransportArbiter(
          server, kDelliciusQueries, kQueryRules, &clock,
          {.features = StandardQueryEngineFeatures(),
           .transport_arbiter_type = StubArbiterInfo::Type::kManual}));
  QueryIdToResult primary_response_entries =
      query_engine->ExecuteRedpathQuery({"SensorCollector"});

  QueryValue sensors = primary_response_entries.results()
                           .at("SensorCollector")
                           .data()
                           .fields()
                           .at("Sensors");
  EXPECT_EQ(sensors.list_value().values_size(), 14);

  *intent_output_sensor.mutable_results()
       ->at("SensorCollector")
       .mutable_status()
       ->add_transport_code() = ParseTextProtoOrDie(R"pb(
    transport_priority: TRANSPORT_PRIORITY_PRIMARY
    error_code: ERROR_NONE
  )pb");
  VerifyQueryResults(std::move(primary_response_entries),
                     {intent_output_sensor});

  QueryIdToResult secondary_response_entries =
      query_engine->ExecuteRedpathQuery(
          {"SensorCollector"}, {.priority_label = PriorityLabel::kSecondary});

  sensors = secondary_response_entries.results()
                .at("SensorCollector")
                .data()
                .fields()
                .at("Sensors");
  EXPECT_EQ(sensors.list_value().values_size(), 14);

  intent_output_sensor.mutable_results()
      ->at("SensorCollector")
      .mutable_status()
      ->clear_transport_code();

  *intent_output_sensor.mutable_results()
       ->at("SensorCollector")
       .mutable_status()
       ->add_transport_code() = ParseTextProtoOrDie(R"pb(
    transport_priority: TRANSPORT_PRIORITY_SECONDARY
    error_code: ERROR_NONE
  )pb");

  VerifyQueryResults(std::move(secondary_response_entries),
                     {std::move(intent_output_sensor)});
}

TEST(QueryEngineTest, FailToCreatePrimaryransport) {
  QueryIdToResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(
          GetTestDataDependencyPath(JoinFilePaths(
              kQuerySamplesLocation, "query_out/sensor_out.textproto")));

  FakeClock clock{clock_time};
  FakeRedfishServer server(kIndusMockup);
  ECCLESIA_ASSIGN_OR_FAIL(
      std::unique_ptr<QueryEngineIntf> query_engine,
      GetQueryEngineWithTransportArbiter(
          server, kDelliciusQueries, kQueryRules, &clock,
          {.features = StandardQueryEngineFeatures()},
          [&](PriorityLabel label)
              -> absl::StatusOr<std::unique_ptr<RedfishTransport>> {
            if (label == PriorityLabel::kPrimary) {
              return absl::UnavailableError(
                  "Failed to create primary transport");
            }
            FakeRedfishServer::Config config = server.GetConfig();
            auto http_client = std::make_unique<CurlHttpClient>(
                LibCurlProxy::CreateInstance(), HttpCredential{});
            std::string network_endpoint =
                absl::StrFormat("%s:%d", config.hostname, config.port);
            return HttpRedfishTransport::MakeNetwork(std::move(http_client),
                                                     network_endpoint);
          }));

  QueryIdToResult response_entries =
      query_engine->ExecuteRedpathQuery({"SensorCollector"});

  QueryValue sensors = response_entries.results()
                           .at("SensorCollector")
                           .data()
                           .fields()
                           .at("Sensors");
  EXPECT_EQ(sensors.list_value().values_size(), 14);

  *intent_output_sensor.mutable_results()
       ->at("SensorCollector")
       .mutable_status()
       ->add_transport_code() = ParseTextProtoOrDie(R"pb(
    transport_priority: TRANSPORT_PRIORITY_SECONDARY
    error_code: ERROR_NONE
  )pb");
  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_output_sensor)});
}

TEST(QueryEngineTest, ExecuteQueryWithTransportArbiterAndFailOnPrimary) {
  QueryIdToResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(
          GetTestDataDependencyPath(JoinFilePaths(
              kQuerySamplesLocation, "query_out/sensor_out.textproto")));

  FakeClock clock{clock_time};
  FakeRedfishServer server(kIndusMockup);
  std::string temp_sensor_uri =
      "/redfish/v1/Chassis/chassis/Sensors/indus_eat_temp";

  ECCLESIA_ASSIGN_OR_FAIL(
      std::unique_ptr<QueryEngineIntf> query_engine,
      GetQueryEngineWithTransportArbiter(
          server, kDelliciusQueries, kQueryRules, &clock,
          {.features = StandardQueryEngineFeatures()},
          [&](PriorityLabel label)
              -> absl::StatusOr<std::unique_ptr<RedfishTransport>> {
            if (label == PriorityLabel::kPrimary) {
              server.AddHttpGetHandlerWithStatus(
                  temp_sensor_uri, "",
                  tensorflow::serving::net_http::HTTPStatusCode::SERVICE_UNAV);
            } else {
              server.AddHttpGetHandlerWithOwnedData(temp_sensor_uri, R"json({
        "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/indus_eat_temp",
        "@odata.type": "#Sensor.v1_2_0.Sensor",
        "Reading": 28.0,
        "ReadingUnits": "Cel",
        "ReadingType": "Temperature",
        "Name": "indus_eat_temp"
      })json");
            }
            FakeRedfishServer::Config config = server.GetConfig();
            auto http_client = std::make_unique<CurlHttpClient>(
                LibCurlProxy::CreateInstance(), HttpCredential{});
            std::string network_endpoint =
                absl::StrFormat("%s:%d", config.hostname, config.port);
            return HttpRedfishTransport::MakeNetwork(std::move(http_client),
                                                     network_endpoint);
          }));

  QueryIdToResult response_entries =
      query_engine->ExecuteRedpathQuery({"SensorCollector"});

  QueryValue sensors = response_entries.results()
                           .at("SensorCollector")
                           .data()
                           .fields()
                           .at("Sensors");
  EXPECT_EQ(sensors.list_value().values_size(), 14);

  *intent_output_sensor.mutable_results()
       ->at("SensorCollector")
       .mutable_status()
       ->add_transport_code() = ParseTextProtoOrDie(R"pb(
    transport_priority: TRANSPORT_PRIORITY_PRIMARY
    error_code: ERROR_UNAVAILABLE
  )pb");

  *intent_output_sensor.mutable_results()
       ->at("SensorCollector")
       .mutable_status()
       ->add_transport_code() = ParseTextProtoOrDie(R"pb(
    transport_priority: TRANSPORT_PRIORITY_SECONDARY
    error_code: ERROR_NONE
  )pb");

  VerifyQueryResults(std::move(response_entries),
                     {std::move(intent_output_sensor)});
}

TEST(QueryEngineTest, ExecuteQueryWithTransportArbiterTransportFreshness) {
  QueryIdToResult intent_output_sensor =
      ParseTextFileAsProtoOrDie<QueryIdToResult>(
          GetTestDataDependencyPath(JoinFilePaths(
              kQuerySamplesLocation, "query_out/sensor_out.textproto")));

  FakeClock clock{clock_time};
  FakeRedfishServer server(kIndusMockup);
  std::string temp_sensor_uri =
      "/redfish/v1/Chassis/chassis/Sensors/indus_eat_temp";
  ECCLESIA_ASSIGN_OR_FAIL(
      std::unique_ptr<QueryEngineIntf> query_engine,
      GetQueryEngineWithTransportArbiter(
          server, kDelliciusQueries, kQueryRules, &clock,
          {.features = StandardQueryEngineFeatures()},
          [&](PriorityLabel label)
              -> absl::StatusOr<std::unique_ptr<RedfishTransport>> {
            if (label == PriorityLabel::kPrimary) {
              // Ensure two subqueries return errors.
              server.AddHttpGetHandlerWithStatus(
                  temp_sensor_uri, "",
                  tensorflow::serving::net_http::HTTPStatusCode::SERVICE_UNAV);
            } else {
              server.AddHttpGetHandlerWithOwnedData(temp_sensor_uri, R"json({
        "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/indus_eat_temp",
        "@odata.type": "#Sensor.v1_2_0.Sensor",
        "Reading": 28.0,
        "ReadingUnits": "Cel",
        "ReadingType": "Temperature",
        "Name": "indus_eat_temp"
      })json");
            }
            FakeRedfishServer::Config config = server.GetConfig();
            auto http_client = std::make_unique<CurlHttpClient>(
                LibCurlProxy::CreateInstance(), HttpCredential{});
            std::string network_endpoint =
                absl::StrFormat("%s:%d", config.hostname, config.port);
            return HttpRedfishTransport::MakeNetwork(std::move(http_client),
                                                     network_endpoint);
          }));

  QueryIdToResult first_response_entries =
      query_engine->ExecuteRedpathQuery({"SensorCollector"});

  QueryValue sensors = first_response_entries.results()
                           .at("SensorCollector")
                           .data()
                           .fields()
                           .at("Sensors");
  EXPECT_EQ(sensors.list_value().values_size(), 14);
  *intent_output_sensor.mutable_results()
       ->at("SensorCollector")
       .mutable_status()
       ->add_transport_code() = ParseTextProtoOrDie(R"pb(
    transport_priority: TRANSPORT_PRIORITY_PRIMARY
    error_code: ERROR_UNAVAILABLE
  )pb");

  *intent_output_sensor.mutable_results()
       ->at("SensorCollector")
       .mutable_status()
       ->add_transport_code() = ParseTextProtoOrDie(R"pb(
    transport_priority: TRANSPORT_PRIORITY_SECONDARY
    error_code: ERROR_NONE
  )pb");
  VerifyQueryResults(std::move(first_response_entries), {intent_output_sensor});

  QueryIdToResult second_response_entries =
      query_engine->ExecuteRedpathQuery({"SensorCollector"});
  intent_output_sensor.mutable_results()
      ->at("SensorCollector")
      .mutable_status()
      ->clear_transport_code();
  *intent_output_sensor.mutable_results()
       ->at("SensorCollector")
       .mutable_status()
       ->add_transport_code() = ParseTextProtoOrDie(R"pb(
    transport_priority: TRANSPORT_PRIORITY_SECONDARY
    error_code: ERROR_NONE
  )pb");

  VerifyQueryResults(std::move(second_response_entries),
                     {intent_output_sensor});

  clock.AdvanceTime(absl::Seconds(10));
  QueryIdToResult refresh_response_entries =
      query_engine->ExecuteRedpathQuery({"SensorCollector"});

  intent_output_sensor.mutable_results()
      ->at("SensorCollector")
      .mutable_status()
      ->clear_transport_code();
  *intent_output_sensor.mutable_results()
       ->at("SensorCollector")
       .mutable_status()
       ->add_transport_code() = ParseTextProtoOrDie(R"pb(
    transport_priority: TRANSPORT_PRIORITY_PRIMARY
    error_code: ERROR_UNAVAILABLE
  )pb");

  *intent_output_sensor.mutable_results()
       ->at("SensorCollector")
       .mutable_status()
       ->add_transport_code() = ParseTextProtoOrDie(R"pb(
    transport_priority: TRANSPORT_PRIORITY_SECONDARY
    error_code: ERROR_NONE
  )pb");

  VerifyQueryResults(std::move(refresh_response_entries),
                     {std::move(intent_output_sensor)});
}

TEST(QueryEngineTest, ExecuteSubscriptionQueryWithTransportArbiter) {
  FakeRedfishServer server(kIndusMockup);
  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<QueryEngineIntf> query_engine,
                          GetQueryEngineWithTransportArbiter(server));

  // Set up callbacks for subscription query.
  // This is what the user of query engine subscription API provides and in this
  // test we expect these callbacks to be invoked along with the desired
  // parameters.
  auto client_on_event_callback =
      [&](const QueryResult &, const RedPathSubscription::EventContext &) {};
  auto client_on_stop_callback = [&](const absl::Status &) {};

  // Execute Subscription Query.
  // 1. Setup streaming options
  QueryEngineIntf::StreamingOptions streaming_options{
      .on_event_callback = client_on_event_callback,
      .on_stop_callback = client_on_stop_callback};

  // 2. Execute subscription query
  EXPECT_THAT(
      query_engine->ExecuteSubscriptionQuery({"Thermal"}, streaming_options),
      IsStatusUnimplemented());
}

TEST(QueryEngineTest, GetRedfishInterfaceWithTransportArbiter) {
  FakeRedfishServer server(kIndusMockup);
  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<QueryEngineIntf> query_engine,
                          GetQueryEngineWithTransportArbiter(server));

  EXPECT_THAT(query_engine->GetRedfishInterface(
                  RedfishInterfacePasskeyFactory::GetPassKey()),
              IsStatusInternal());
}

TEST(QueryEngineTest, ExecuteOnRedfishInterfaceWithTransportArbiter) {
  FakeRedfishServer server(kIndusMockup);
  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<QueryEngineIntf> query_engine,
                          GetQueryEngineWithTransportArbiter(server));

  EXPECT_THAT(query_engine->ExecuteOnRedfishInterface(
                  RedfishInterfacePasskeyFactory::GetPassKey(),
                  {.callback =
                       [](const RedfishInterface &redfish_interface) {
                         return absl::OkStatus();
                       }}),
              IsOk());
}

}  // namespace
}  // namespace ecclesia
