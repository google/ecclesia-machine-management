/*
 * Copyright 2024 Google LLC
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

#include "ecclesia/lib/redfish/redpath/engine/query_planner_impl.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/engine/factory.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/engine/query_planner.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

namespace {

using ::tensorflow::serving::net_http::ServerRequestInterface;
using ::tensorflow::serving::net_http::SetContentType;

class QueryPlannerTestRunner : public ::testing::Test {
 protected:
  QueryPlannerTestRunner() = default;
  void SetTestParams(absl::string_view mockup) {
    server_ = std::make_unique<FakeRedfishServer>(mockup);
    server_->EnableAllParamsGetHandler();
    intf_ = server_->RedfishClientInterface();
  }

  std::unique_ptr<FakeRedfishServer> server_;
  std::unique_ptr<RedfishInterface> intf_;
};

TEST_F(QueryPlannerTestRunner, QueryPlannerExecutesQueryCorrectly) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "ChassisSubTreeTest"
        subquery {
          subquery_id: "Chassis"
          redpath: "/Chassis[*]"
          properties { property: "Id" type: STRING }
        }
        subquery {
          subquery_id: "Assembly"
          root_subquery_ids: "Chassis"
          redpath: "/Assembly/Assemblies[Name=indus]"
          properties { property: "Name" type: STRING }
        }
        subquery {
          subquery_id: "Sensors"
          root_subquery_ids: "Chassis"
          redpath: "/Sensors[Name=CPU1]"
          properties { property: "Name" type: STRING }
        }
        subquery {
          subquery_id: "UnknownPropertySubquery"
          root_subquery_ids: "Chassis"
          redpath: "/Sensors[*]"
          properties { property: "UnknownProperty" type: STRING }
        }
        subquery {
          subquery_id: "UnknownNodeNameSubquery"
          root_subquery_ids: "Sensors"
          redpath: "/UnknownNodeName"
          properties { property: "Name" type: STRING }
        }
      )pb");

  QueryResult expected_query_result = ParseTextProtoOrDie(R"pb(
    query_id: "ChassisSubTreeTest"
    data {
      fields {
        key: "Chassis"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "Assembly"
                  value {
                    list_value {
                      values {
                        subquery_value {
                          fields {
                            key: "Name"
                            value { string_value: "indus" }
                          }
                        }
                      }
                    }
                  }
                }
                fields {
                  key: "Id"
                  value { string_value: "chassis" }
                }
                fields {
                  key: "Sensors"
                  value {
                    list_value {
                      values {
                        subquery_value {
                          fields {
                            key: "Name"
                            value { string_value: "CPU1" }
                          }
                          fields {
                            key: "UnknownNodeNameSubquery"
                            value {}
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Name"
                            value { string_value: "CPU1" }
                          }
                          fields {
                            key: "UnknownNodeNameSubquery"
                            value {}
                          }
                        }
                      }
                    }
                  }
                }
                fields {
                  key: "UnknownPropertySubquery"
                  value {}
                }
              }
            }
          }
        }
      }
    }
  )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");
  std::unique_ptr<Normalizer> normalizer = BuildDefaultNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp = BuildQueryPlanner(
      query, RedPathRedfishQueryParams{}, normalizer.get(), intf_.get());
  EXPECT_THAT(qp, IsOk());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  absl::StatusOr<QueryResult> query_result = (*qp)->Run({args1});

  EXPECT_THAT(query_result, IsOk());
  EXPECT_THAT(expected_query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(query_result.value())));
}

TEST_F(QueryPlannerTestRunner, QueryPlannerAppliesFreshnessFromQuery) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "ChassisSubTreeTest"
        subquery {
          subquery_id: "Chassis"
          redpath: "/Chassis[*]"
          freshness: REQUIRED
          properties { property: "Id" type: STRING }
        }
        subquery {
          subquery_id: "Assembly"
          root_subquery_ids: "Chassis"
          redpath: "/Assembly/Assemblies[Name=indus]"
          properties { property: "Name" type: STRING }
        }
      )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");
  size_t chassis_query_count = 0;
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis", [&](ServerRequestInterface *req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Chassis/chassis",
          "Id": "1",
          "Assembly": {
            "@odata.id": "/redfish/v1/Chassis/chassis/Assembly"
          }
        })json");
        chassis_query_count++;
        req->Reply();
      });

  size_t assembly_query_count = 0;
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis/Assembly", [&](ServerRequestInterface *req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        assembly_query_count++;
        req->WriteResponseString(R"json({"@odata.id": "uri"})json");
        req->Reply();
      });

  std::unique_ptr<RedfishTransport> base_transport =
      server_->RedfishClientTransport();
  std::unique_ptr<Normalizer> normalizer = BuildDefaultNormalizer();
  std::unique_ptr<RedfishCachedGetterInterface> cache =
      TimeBasedCache::Create(base_transport.get(), absl::InfiniteDuration());
  auto intf = NewHttpInterface(std::move(base_transport), std::move(cache),
                               RedfishInterface::kTrusted);
  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp = BuildQueryPlanner(
      query, RedPathRedfishQueryParams{}, normalizer.get(), intf.get());
  EXPECT_THAT(qp, IsOk());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  EXPECT_THAT((*qp)->Run({args1}), IsOk());
  EXPECT_THAT((*qp)->Run({args1}), IsOk());

  // We expect Chassis to be queried twice due to freshness requirement.
  EXPECT_EQ(chassis_query_count, 2);

  // We expect Assemblies to be queried just once when cache is cold.
  EXPECT_EQ(assembly_query_count, 1);
}

TEST_F(QueryPlannerTestRunner, QueryPlannerAppliesExpandFromRules) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "ChassisSubTreeTest"
        service_root: "/redfish/v1"
        subquery {
          subquery_id: "Chassis"
          redpath: "/Chassis[*]"
          properties { property: "Id" type: STRING }
        }
        subquery {
          subquery_id: "Assembly"
          root_subquery_ids: "Chassis"
          redpath: "/Assembly/Assemblies[Name=indus]"
          freshness: REQUIRED
          properties { property: "Name" type: STRING }
        }
      )pb");

  RedPathRedfishQueryParams redpath_redfish_query_params = {
      {"/Chassis",
       {.expand = RedfishQueryParamExpand(
            {.type = RedfishQueryParamExpand::ExpandType::kNotLinks,
             .levels = 1})}}};

  SetTestParams("indus_hmb_shim/mockup.shar");
  bool expand_requested = false;
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis?$expand=.($levels=1)",
      [&](ServerRequestInterface *req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        expand_requested = true;
        req->WriteResponseString(R"json({"@odata.id": "uri"})json");
        req->Reply();
      });
  std::unique_ptr<Normalizer> normalizer = BuildDefaultNormalizer();
  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp =
      BuildQueryPlanner(query, std::move(redpath_redfish_query_params),
                        normalizer.get(), intf_.get());
  EXPECT_THAT(qp, IsOk());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  EXPECT_THAT((*qp)->Run({args1}), IsOk());
  EXPECT_TRUE(expand_requested);
}

TEST_F(QueryPlannerTestRunner, QueryPlannerAppliesFilterFromRules) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "SensorTest"
        service_root: "/redfish/v1"
        subquery {
          subquery_id: "SensorsGreater"
          redpath: "/Chassis[*]/Sensors[Reading>40]"
          properties { property: "Id" type: STRING }
        }
        subquery {
          subquery_id: "SensorsLesser"
          redpath: "/Chassis[*]/Sensors[Reading<5]"
          properties { property: "Id" type: STRING }
        }
      )pb");

  RedPathRedfishQueryParams redpath_redfish_query_params = {
      {"/Chassis[*]/Sensors", {.filter = RedfishQueryParamFilter("")}}};

  SetTestParams("indus_hmb_shim/mockup.shar");
  bool filter_requested1 = false;
  bool filter_requested2 = false;
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis/"
      "Sensors?$filter=Reading%20gt%2040%20or%20Reading%20lt%205",
      [&](ServerRequestInterface *req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        filter_requested1 = true;
        req->WriteResponseString(R"json({"@odata.id": "uri"})json");
        req->Reply();
      });
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis/"
      "Sensors?$filter=Reading%20lt%205%20or%20Reading%20gt%2040",
      [&](ServerRequestInterface *req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        filter_requested2 = true;
        req->WriteResponseString(R"json({"@odata.id": "uri"})json");
        req->Reply();
      });
  std::unique_ptr<Normalizer> normalizer = BuildDefaultNormalizer();
  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp =
      BuildQueryPlanner(query, std::move(redpath_redfish_query_params),
                        normalizer.get(), intf_.get());
  EXPECT_THAT(qp, IsOk());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  EXPECT_THAT((*qp)->Run({args1}), IsOk());
  // Since the order in which the predicates are passed to the $filter string
  // construction method is non-deterministic, the Redfish request can be in two
  // possible forms.
  EXPECT_TRUE(filter_requested1 || filter_requested2);
}

TEST_F(QueryPlannerTestRunner, QueryPlannerExecutesTemplatedQueryCorrectly) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "ChassisSubTreeTest"
        subquery {
          subquery_id: "Chassis"
          redpath: "/Chassis[*]"
          properties { property: "Id" type: STRING }
        }
        subquery {
          subquery_id: "Assembly"
          root_subquery_ids: "Chassis"
          redpath: "/Assembly/Assemblies[Name=indus]"
          properties { property: "Name" type: STRING }
        }
        subquery {
          subquery_id: "Sensors"
          root_subquery_ids: "Chassis"
          redpath: "/Sensors[Name=$Name and Reading<$Threshold]"
          properties { property: "Name" type: STRING }
          properties { property: "Reading" type: INT64 }
        }
        subquery {
          subquery_id: "UnknownPropertySubquery"
          root_subquery_ids: "Chassis"
          redpath: "/Sensors[*]"
          properties { property: "UnknownProperty" type: STRING }
        }
        subquery {
          subquery_id: "UnknownNodeNameSubquery"
          root_subquery_ids: "Sensors"
          redpath: "/UnknownNodeName"
          properties { property: "Name" type: STRING }
        }
      )pb");

  QueryResult expect_query_result = ParseTextProtoOrDie(R"pb(
    query_id: "ChassisSubTreeTest"
    data {
      fields {
        key: "Chassis"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "Assembly"
                  value {
                    list_value {
                      values {
                        subquery_value {
                          fields {
                            key: "Name"
                            value { string_value: "indus" }
                          }
                        }
                      }
                    }
                  }
                }
                fields {
                  key: "Id"
                  value { string_value: "chassis" }
                }
                fields {
                  key: "Sensors"
                  value {
                    list_value {
                      values {
                        subquery_value {
                          fields {
                            key: "Name"
                            value { string_value: "indus_latm_temp" }
                          }
                          fields {
                            key: "Reading"
                            value { int_value: 35 }
                          }
                          fields {
                            key: "UnknownNodeNameSubquery"
                            value {}
                          }
                        }
                      }
                    }
                  }
                }
                fields {
                  key: "UnknownPropertySubquery"
                  value {}
                }
              }
            }
          }
        }
      }
    }
  )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");
  std::unique_ptr<Normalizer> normalizer = BuildDefaultNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp = BuildQueryPlanner(
      query, RedPathRedfishQueryParams{}, normalizer.get(), intf_.get());
  EXPECT_THAT(qp, IsOk());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  ecclesia::QueryVariables::VariableValue val1;
  ecclesia::QueryVariables::VariableValue val2;
  ecclesia::QueryVariables::VariableValue val3;
  val1.set_name("Threshold");
  val1.set_value("60");
  val2.set_name("Name");
  val2.set_value("indus_latm_temp");
  val3.set_name("Type");
  val3.set_value("Rotational");

  *args1.add_values() = val1;
  *args1.add_values() = val2;
  *args1.add_values() = val3;
  absl::StatusOr<QueryResult> query_result = (*qp)->Run({args1});

  EXPECT_THAT(query_result, IsOk());
  EXPECT_THAT(expect_query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(query_result.value())));
}

TEST_F(QueryPlannerTestRunner, QueryPlannerExecutesUriCorrectly) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "SensorsSubTreeTest"
        subquery {
          subquery_id: "Sensors"
          uri: "/redfish/v1/Chassis/chassis/Sensors/indus_cpu1_pwmon"
          properties { property: "Name" type: STRING }
        }
      )pb");

  QueryResult expected_query_result = ParseTextProtoOrDie(R"pb(
    query_id: "SensorsSubTreeTest"
    data {
      fields {
        key: "Sensors"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "CPU1" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");
  std::unique_ptr<Normalizer> normalizer = BuildDefaultNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp = BuildQueryPlanner(
      query, RedPathRedfishQueryParams{}, normalizer.get(), intf_.get());
  EXPECT_THAT(qp, IsOk());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  absl::StatusOr<QueryResult> query_result = (*qp)->Run({args1});

  EXPECT_THAT(query_result, IsOk());
  EXPECT_THAT(expected_query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(query_result.value())));
}

TEST_F(QueryPlannerTestRunner, QueryPlannerAppliesFilterForUriFromRules) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "SensorTest"
        service_root: "/redfish/v1"
        subquery {
          subquery_id: "SensorsEqual"
          uri: "/redfish/v1/Chassis/chassis/Sensors/indus_cpu1_pwmon"
          properties { property: "Name" type: STRING }
        }
      )pb");

  std::string filter_string1 = "ReadingType%20eq%20Power";
  auto filter = RedfishQueryParamFilter(filter_string1);
  RedPathRedfishQueryParams redpath_redfish_query_params = {
      {"/redfish/v1/Chassis/chassis/Sensors/indus_cpu1_pwmon",
       {.filter = filter}}};

  SetTestParams("indus_hmb_shim/mockup.shar");
  bool filter_requested1 = false;
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis/Sensors/"
      "indus_cpu1_pwmon?$filter=ReadingType%20eq%20Power",
      [&](ServerRequestInterface *req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        filter_requested1 = true;
        req->WriteResponseString(R"json({"@odata.id": "uri"})json");
        req->Reply();
      });
  std::unique_ptr<Normalizer> normalizer = BuildDefaultNormalizer();
  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp =
      BuildQueryPlanner(query, std::move(redpath_redfish_query_params),
                        normalizer.get(), intf_.get());
  EXPECT_THAT(qp, IsOk());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  EXPECT_THAT((*qp)->Run({args1}), IsOk());
  // Since the order in which the predicates are passed to the $filter string
  // construction method is non-deterministic, the Redfish request can be in two
  // possible forms.
  EXPECT_TRUE(filter_requested1);
}

}  // namespace

}  // namespace ecclesia
