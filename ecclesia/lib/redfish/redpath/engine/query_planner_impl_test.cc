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
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/engine/normalizer.h"
#include "ecclesia/lib/redfish/redpath/engine/query_planner.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/testing/json_mockup.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/metrical_transport.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"
#include "ecclesia/lib/time/clock.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

namespace {

using RedPathRedfishQueryParams =
    absl::flat_hash_map<std::string /* RedPath */, GetParams>;

using ::tensorflow::serving::net_http::ServerRequestInterface;
using ::tensorflow::serving::net_http::SetContentType;
using ::testing::NotNull;
using ::testing::UnorderedElementsAre;
using QueryExecutionResult = QueryPlannerIntf::QueryExecutionResult;

constexpr absl::string_view kSensorRedPath = "/Chassis[*]/Sensors[*]";
constexpr absl::string_view kAssemblyRedPath = "/Chassis[*]/Assembly";
constexpr absl::string_view kAssembliesRedPath =
    "/Chassis[*]/Assembly/Assemblies";
constexpr absl::string_view kInvalidRedPath = "/Chassis[*]/Unknown";

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
          subquery_id: "Sensors"
          root_subquery_ids: "Chassis"
          redpath: "/Sensors[ReadingUnits=RPM]"
          properties { property: "Name" type: STRING }
        }
        subquery {
          subquery_id: "Assembly"
          root_subquery_ids: "Sensors"
          redpath: "/RelatedItem[0]"
          properties { property: "MemberId" type: STRING }
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
                            key: "Assembly"
                            value {
                              list_value {
                                values {
                                  subquery_value {
                                    fields {
                                      key: "MemberId"
                                      value { string_value: "1" }
                                    }
                                  }
                                }
                              }
                            }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "fan0" }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Assembly"
                            value {
                              list_value {
                                values {
                                  subquery_value {
                                    fields {
                                      key: "MemberId"
                                      value { string_value: "2" }
                                    }
                                  }
                                }
                              }
                            }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "fan1" }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Assembly"
                            value {
                              list_value {
                                values {
                                  subquery_value {
                                    fields {
                                      key: "MemberId"
                                      value { string_value: "4" }
                                    }
                                  }
                                }
                              }
                            }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "fan2" }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Assembly"
                            value {
                              list_value {
                                values {
                                  subquery_value {
                                    fields {
                                      key: "MemberId"
                                      value { string_value: "5" }
                                    }
                                  }
                                }
                              }
                            }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "fan3" }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Assembly"
                            value {
                              list_value {
                                values {
                                  subquery_value {
                                    fields {
                                      key: "MemberId"
                                      value { string_value: "6" }
                                    }
                                  }
                                }
                              }
                            }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "fan4" }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Assembly"
                            value {
                              list_value {
                                values {
                                  subquery_value {
                                    fields {
                                      key: "MemberId"
                                      value { string_value: "7" }
                                    }
                                  }
                                }
                              }
                            }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "fan5" }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Assembly"
                            value {
                              list_value {
                                values {
                                  subquery_value {
                                    fields {
                                      key: "MemberId"
                                      value { string_value: "8" }
                                    }
                                  }
                                }
                              }
                            }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "fan6" }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Assembly"
                            value {
                              list_value {
                                values {
                                  subquery_value {
                                    fields {
                                      key: "MemberId"
                                      value { string_value: "9" }
                                    }
                                  }
                                }
                              }
                            }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "fan7" }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp =
      BuildQueryPlanner({.query = query,
                         .redpath_rules = {},
                         .normalizer = normalizer.get(),
                         .redfish_interface = intf_.get()});

  EXPECT_THAT(qp, IsOk());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  absl::StatusOr<QueryExecutionResult> result = (*qp)->Run({args1});

  EXPECT_THAT(result, IsOk());
  EXPECT_THAT(expected_query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(result->query_result)));
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
  std::unique_ptr<RedfishCachedGetterInterface> cache =
      TimeBasedCache::Create(base_transport.get(), absl::InfiniteDuration());
  auto intf = NewHttpInterface(std::move(base_transport), std::move(cache),
                               RedfishInterface::kTrusted);
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp =
      BuildQueryPlanner({.query = query,
                         .redpath_rules = {},
                         .normalizer = normalizer.get(),
                         .redfish_interface = intf.get()});
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

  QueryPlannerOptions::RedPathRules redpath_rules = {
      .redpath_to_query_params = {
          {"/Chassis",
           {.expand = RedfishQueryParamExpand(
                {.type = RedfishQueryParamExpand::ExpandType::kNotLinks,
                 .levels = 1})}}}};

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

  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp =
      BuildQueryPlanner({.query = query,
                         .redpath_rules = std::move(redpath_rules),
                         .normalizer = normalizer.get(),
                         .redfish_interface = intf_.get()});
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

  QueryPlannerOptions::RedPathRules redpath_rules = {
      .redpath_to_query_params = {
          {"/Chassis[*]/Sensors", {.filter = RedfishQueryParamFilter("")}}}};

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

  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp =
      BuildQueryPlanner({.query = query,
                         .redpath_rules = std::move(redpath_rules),
                         .normalizer = normalizer.get(),
                         .redfish_interface = intf_.get()});
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
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");

  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp =
      BuildQueryPlanner({.query = query,
                         .redpath_rules = {},
                         .normalizer = normalizer.get(),
                         .redfish_interface = intf_.get()});
  EXPECT_THAT(qp, IsOk());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  ecclesia::QueryVariables::VariableValue val1;
  ecclesia::QueryVariables::VariableValue val2;
  ecclesia::QueryVariables::VariableValue val3;
  val1.set_name("Threshold");
  *val1.add_values() = "60";
  val2.set_name("Name");
  *val2.add_values() = "indus_latm_temp";
  val3.set_name("Type");
  *val3.add_values() = "Rotational";

  *args1.add_variable_values() = val1;
  *args1.add_variable_values() = val2;
  *args1.add_variable_values() = val3;
  absl::StatusOr<QueryExecutionResult> result = (*qp)->Run({args1});

  EXPECT_THAT(result, IsOk());
  EXPECT_THAT(result->query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(expect_query_result)));

  ecclesia::QueryVariables multi_value_args = ecclesia::QueryVariables();
  ecclesia::QueryVariables::VariableValue multi_value1;
  ecclesia::QueryVariables::VariableValue multi_value2;
  multi_value1.set_name("Threshold");
  *multi_value1.add_values() = "60";
  *multi_value1.add_values() = "60";
  val2.set_name("Name");
  *val2.add_values() = "indus_latm_temp";
  val3.set_name("Type");
  *val3.add_values() = "Rotational";

  result = (*qp)->Run({args1});
  EXPECT_THAT(result, IsOk());
  EXPECT_THAT(result->query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(expect_query_result)));
}

TEST_F(QueryPlannerTestRunner,
       QueryPlannerExecutesTemplatedQueryWithMultiValueVarsCorrectly) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "ChassisSubTreeTest"
        subquery {
          subquery_id: "Chassis"
          redpath: "/Chassis[Id=$ChassisId]"
          properties { property: "Id" type: STRING }
        }
        subquery {
          subquery_id: "Sensors"
          root_subquery_ids: "Chassis"
          redpath: "/Sensors[Reading<$Threshold and Name=$SensorName]"
          properties { property: "Name" type: STRING }
          properties { property: "Reading" type: INT64 }
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
                            value { string_value: "CPU0" }
                          }
                          fields {
                            key: "Reading"
                            value { int_value: 30 }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Name"
                            value { string_value: "indus_eat_temp" }
                          }
                          fields {
                            key: "Reading"
                            value { int_value: 28 }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp =
      BuildQueryPlanner({.query = query,
                         .redpath_rules = {},
                         .normalizer = normalizer.get(),
                         .redfish_interface = intf_.get()});
  EXPECT_THAT(qp, IsOk());

  ecclesia::QueryVariables multi_value_args = ecclesia::QueryVariables();
  ecclesia::QueryVariables::VariableValue chassis_id_val;
  ecclesia::QueryVariables::VariableValue sensor_name_val;
  ecclesia::QueryVariables::VariableValue threshold_val;
  chassis_id_val.set_name("ChassisId");
  *chassis_id_val.add_values() = "chassis";
  *chassis_id_val.add_values() = "chassis_id_2";
  sensor_name_val.set_name("SensorName");
  *sensor_name_val.add_values() = "indus_latm_temp";
  *sensor_name_val.add_values() = "CPU0";
  *sensor_name_val.add_values() = "indus_eat_temp";
  threshold_val.set_name("Threshold");
  *threshold_val.add_values() = "34";

  *multi_value_args.add_variable_values() = chassis_id_val;
  *multi_value_args.add_variable_values() = sensor_name_val;
  *multi_value_args.add_variable_values() = threshold_val;

  absl::StatusOr<QueryExecutionResult> result = (*qp)->Run({multi_value_args});
  EXPECT_THAT(result, IsOk());
  EXPECT_THAT(result->query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(expect_query_result)));
}
TEST_F(QueryPlannerTestRunner,
       QueryPlannerExecutesTemplatedQueryWithParenAndMultiValueVarsCorrectly) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "ChassisSubTreeTest"
        subquery {
          subquery_id: "Chassis"
          redpath: "/Chassis[Id=$ChassisId]"
          properties { property: "Id" type: STRING }
        }
        subquery {
          subquery_id: "Sensors"
          root_subquery_ids: "Chassis"
          redpath: "/Sensors[(Name=$SensorName or Reading=60) or (!Reading and Id=$SensorId)]"
          properties { property: "Name" type: STRING }
          properties { property: "Id" type: STRING }
          properties { property: "Reading" type: INT64 }
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
                            key: "Id"
                            value { string_value: "indus_fan7_rpm" }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "fan7" }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Id"
                            value { string_value: "indus_eat_temp" }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "indus_eat_temp" }
                          }
                          fields {
                            key: "Reading"
                            value { int_value: 28 }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Id"
                            value { string_value: "indus_latm_temp" }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "indus_latm_temp" }
                          }
                          fields {
                            key: "Reading"
                            value { int_value: 35 }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Id"
                            value { string_value: "indus_cpu0_pwmon" }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "CPU0" }
                          }
                          fields {
                            key: "Reading"
                            value { int_value: 30 }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Id"
                            value { string_value: "i_cpu0_t" }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "CPU0" }
                          }
                          fields {
                            key: "Reading"
                            value { int_value: 60 }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Id"
                            value { string_value: "i_cpu1_t" }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "CPU1" }
                          }
                          fields {
                            key: "Reading"
                            value { int_value: 60 }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    })pb");

  SetTestParams("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp =
      BuildQueryPlanner({.query = query,
                         .redpath_rules = {},
                         .normalizer = normalizer.get(),
                         .redfish_interface = intf_.get()});
  EXPECT_THAT(qp, IsOk());

  ecclesia::QueryVariables multi_value_args = ecclesia::QueryVariables();
  ecclesia::QueryVariables::VariableValue chassis_id_val;
  ecclesia::QueryVariables::VariableValue sensor_name_val;
  ecclesia::QueryVariables::VariableValue sensor_id_val;
  chassis_id_val.set_name("ChassisId");
  *chassis_id_val.add_values() = "chassis";
  *chassis_id_val.add_values() = "chassis_id_2";
  sensor_name_val.set_name("SensorName");
  *sensor_name_val.add_values() = "indus_latm_temp";
  *sensor_name_val.add_values() = "CPU0";
  *sensor_name_val.add_values() = "indus_eat_temp";
  sensor_id_val.set_name("SensorId");
  *sensor_id_val.add_values() = "i_cpu0_t";
  *sensor_id_val.add_values() = "indus_fan7_rpm";

  *multi_value_args.add_variable_values() = chassis_id_val;
  *multi_value_args.add_variable_values() = sensor_name_val;
  *multi_value_args.add_variable_values() = sensor_id_val;
  // Sensors predicate expands to:
  //  ((Name=indus_latm_temp or Name=CPU0 or Name=indus_eat_temp) or Reading=60)
  //  or (!Reading and (Id=i_cpu0_t or Id=indus_fan7_rpm))
  // which translates to ANY sensor with:
  //  Name = indus_latm_temp, indus_eat_temp or CPU,
  //  Reading = 60
  //  No Reading value and Id = i_cpu0_t or indus_fan7_rpm
  absl::StatusOr<QueryExecutionResult> result = (*qp)->Run({multi_value_args});
  EXPECT_THAT(result, IsOk());
  EXPECT_THAT(result->query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(expect_query_result)));
}

TEST_F(QueryPlannerTestRunner, QueryPlannerExecutesWithUrlAnnotations) {
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
                          fields {
                            key: "_uri_"
                            value {
                              string_value: "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/0"
                            }
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
                            key: "_uri_"
                            value {
                              string_value: "/redfish/v1/Chassis/chassis/Sensors/indus_cpu1_pwmon"
                            }
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
                            key: "_uri_"
                            value {
                              string_value: "/redfish/v1/Chassis/chassis/Sensors/i_cpu1_t"
                            }
                          }
                        }
                      }
                    }
                  }
                }
                fields {
                  key: "_uri_"
                  value { string_value: "/redfish/v1/Chassis/chassis" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");

  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp =
      BuildQueryPlanner({.query = query,
                         .redpath_rules = {},
                         .normalizer = normalizer.get(),
                         .redfish_interface = intf_.get()});

  EXPECT_THAT(qp, IsOk());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  absl::StatusOr<QueryExecutionResult> result =
      (*qp)->Run({.variables = args1, .enable_url_annotation = true});

  EXPECT_THAT(result, IsOk());
  EXPECT_THAT(expected_query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(result->query_result)));
}

DelliciusQuery GetSubscriptionQuery() {
  return ParseTextProtoOrDie(
      R"pb(
        query_id: "SubscriptionTest"
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
          redpath: "/Sensors[Reading>16110]"
          properties { property: "Name" type: STRING }
          properties { property: "Reading" type: DOUBLE }
        }
      )pb");
}

TEST_F(QueryPlannerTestRunner, ReturnsCorrectSubscriptionContext) {
  QueryResult expect_query_result = ParseTextProtoOrDie(R"pb(
    query_id: "SubscriptionTest"
    data {
      fields {
        key: "Chassis"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "Id"
                  value { string_value: "chassis" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");

  DelliciusQuery subscription_query = GetSubscriptionQuery();
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp = BuildQueryPlanner(
      {.query = subscription_query,
       .redpath_rules =
           {.redpath_to_query_params =
                {{std::string(kAssemblyRedPath),
                  {.expand = RedfishQueryParamExpand(
                       {.type = RedfishQueryParamExpand::ExpandType::kNotLinks,
                        .levels = 1})}}},
            .redpaths_to_subscribe = {std::string(kSensorRedPath),
                                      std::string(kAssemblyRedPath)}},
       .normalizer = normalizer.get(),
       .redfish_interface = intf_.get()});
  EXPECT_THAT(qp, IsOk());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  absl::StatusOr<QueryExecutionResult> result = (*qp)->Run({args1});

  EXPECT_THAT(result, IsOk());
  EXPECT_THAT(expect_query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(result->query_result)));

  // Verify Subscription context is valid.
  const std::unique_ptr<QueryPlannerIntf::SubscriptionContext> &context =
      result->subscription_context;
  EXPECT_THAT(context, NotNull());

  // Check if context contains trie node for `Sensors` subquery.
  auto find_sensor_trie_node =
      context->redpath_to_trie_node.find(kSensorRedPath);
  EXPECT_TRUE(find_sensor_trie_node != context->redpath_to_trie_node.end());
  EXPECT_THAT(find_sensor_trie_node->second, NotNull());

  // Check if context contains trie node for `Assembly` subquery.
  auto find_assembly_trie_node =
      context->redpath_to_trie_node.find(kAssemblyRedPath);
  EXPECT_TRUE(find_assembly_trie_node != context->redpath_to_trie_node.end());
  EXPECT_THAT(find_assembly_trie_node->second, NotNull());

  bool has_sensor_config = false;
  bool has_assembly_config = false;
  for (const auto &config : context->subscription_configs) {
    // Check if context contains config for `Sensors` subquery.
    if (config.redpath == kSensorRedPath) {
      has_sensor_config = true;
      EXPECT_THAT(config.query_id, "SubscriptionTest");
      EXPECT_THAT(config.redpath, kSensorRedPath);
      EXPECT_THAT(config.uris,
                  UnorderedElementsAre(
                      "/redfish/v1/Chassis/chassis/Sensors/indus_fan0_rpm",
                      "/redfish/v1/Chassis/chassis/Sensors/indus_fan1_rpm",
                      "/redfish/v1/Chassis/chassis/Sensors/indus_fan2_rpm",
                      "/redfish/v1/Chassis/chassis/Sensors/indus_fan3_rpm",
                      "/redfish/v1/Chassis/chassis/Sensors/indus_fan4_rpm",
                      "/redfish/v1/Chassis/chassis/Sensors/indus_fan5_rpm",
                      "/redfish/v1/Chassis/chassis/Sensors/indus_fan6_rpm",
                      "/redfish/v1/Chassis/chassis/Sensors/indus_fan7_rpm",
                      "/redfish/v1/Chassis/chassis/Sensors/indus_eat_temp",
                      "/redfish/v1/Chassis/chassis/Sensors/indus_latm_temp",
                      "/redfish/v1/Chassis/chassis/Sensors/indus_cpu0_pwmon",
                      "/redfish/v1/Chassis/chassis/Sensors/indus_cpu1_pwmon",
                      "/redfish/v1/Chassis/chassis/Sensors/i_cpu0_t",
                      "/redfish/v1/Chassis/chassis/Sensors/i_cpu1_t"));
      EXPECT_THAT(config.predicate, "Reading>16110");
    }

    if (config.redpath == kAssemblyRedPath) {
      has_assembly_config = true;
      EXPECT_THAT(config.query_id, "SubscriptionTest");
      EXPECT_THAT(config.redpath, kAssemblyRedPath);
      EXPECT_THAT(
          config.uris,
          UnorderedElementsAre(
              "/redfish/v1/Chassis/chassis/Assembly?$expand=.($levels=1)"));
      EXPECT_THAT(config.predicate, "");
    }
  }
  EXPECT_TRUE(has_sensor_config);
  EXPECT_TRUE(has_assembly_config);
}

// Subscribe to non navigational property fails.
TEST_F(QueryPlannerTestRunner, SubscriptionToNonNavigationalPropertyFails) {
  SetTestParams("indus_hmb_shim/mockup.shar");

  DelliciusQuery subscription_query = GetSubscriptionQuery();
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp = BuildQueryPlanner(
      {.query = subscription_query,
       .redpath_rules = {.redpaths_to_subscribe = {std::string(
                             kAssembliesRedPath)}},
       .normalizer = normalizer.get(),
       .redfish_interface = intf_.get()});
  EXPECT_THAT(qp, IsOk());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  absl::StatusOr<QueryExecutionResult> result = (*qp)->Run({args1});
  EXPECT_THAT(result, IsStatusInternal());
}

// Subscribe to unknown property fails.
TEST_F(QueryPlannerTestRunner, SubscriptionToUnknownPropertyFails) {
  SetTestParams("indus_hmb_shim/mockup.shar");

  DelliciusQuery subscription_query = ParseTextProtoOrDie(
      R"pb(
        query_id: "SubscriptionTest"
        subquery {
          subquery_id: "Chassis"
          redpath: "/Chassis[*]/Unknown"
          properties { property: "Id" type: STRING }
        }
      )pb");

  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp = BuildQueryPlanner(
      {.query = subscription_query,
       .redpath_rules = {.redpaths_to_subscribe = {std::string(
                             kInvalidRedPath)}},
       .normalizer = normalizer.get(),
       .redfish_interface = intf_.get()});
  EXPECT_THAT(qp, IsOk());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  absl::StatusOr<QueryExecutionResult> result = (*qp)->Run({args1});
  EXPECT_THAT(result, IsStatusInternal());
}

// Subscribe to unknown property fails.
TEST_F(QueryPlannerTestRunner, TemplatedQueryWithNoVarsFails) {
  SetTestParams("indus_hmb_shim/mockup.shar");

  DelliciusQuery subscription_query = ParseTextProtoOrDie(
      R"pb(
        query_id: "SubscriptionTest"
        subquery {
          subquery_id: "Chassis"
          redpath: "/Chassis[Id=$ChassisId]"
          properties { property: "Id" type: STRING }
        }
      )pb");

  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp =
      BuildQueryPlanner({.query = subscription_query,
                         .normalizer = normalizer.get(),
                         .redfish_interface = intf_.get()});
  ASSERT_THAT(qp, IsOk());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  absl::StatusOr<QueryExecutionResult> result = (*qp)->Run({args1});
  EXPECT_THAT(result, IsStatusInvalidArgument());
}

// Successful Resume
TEST_F(QueryPlannerTestRunner, ResumesQueryAfterEvent) {
  SetTestParams("indus_hmb_shim/mockup.shar");

  // Setup: Build query planner and Execute query to create subscription.
  DelliciusQuery subscription_query = GetSubscriptionQuery();
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp = BuildQueryPlanner(
      {.query = subscription_query,
       .redpath_rules = {.redpaths_to_subscribe = {std::string(kSensorRedPath),
                                                   std::string(
                                                       kAssemblyRedPath)}},
       .normalizer = normalizer.get(),
       .redfish_interface = intf_.get()});
  ASSERT_THAT(qp, IsOk());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  absl::StatusOr<QueryExecutionResult> result = (*qp)->Run({args1});
  const std::unique_ptr<QueryPlannerIntf::SubscriptionContext> &context =
      result->subscription_context;
  ASSERT_THAT(context, NotNull());

  // Case1: Sensor RedPath query resumes on receiving an event on specific
  // Sensor resource in the collection `/Chassis[*]/Sensors[*]`.
  {
    // Setup: Make sure we have a trie node to resume Sensor RedPath query.
    auto find_sensor_trie_node =
        context->redpath_to_trie_node.find(kSensorRedPath);
    ASSERT_TRUE(find_sensor_trie_node != context->redpath_to_trie_node.end());
    ASSERT_THAT(find_sensor_trie_node->second, NotNull());

    // Mock sensor event.
    std::unique_ptr<ecclesia::RedfishInterface> sensor_json =
        ecclesia::NewJsonMockupInterface(R"json(
      {
          "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/indus_fan3_rpm",
          "@odata.type": "#Sensor.v1_2_0.Sensor",
          "Id": "indus_fan3_rpm",
          "Name": "fan3",
          "Reading": 16115.0,
          "ReadingUnits": "RPM",
          "ReadingType": "Rotational",
          "RelatedItem": [
              {
                  "@odata.id":
"/redfish/v1/Chassis/chassis/Assembly#/Assemblies/5"
              }
          ],
          "Status": {
              "Health": "OK",
              "State": "Enabled"
          }
      }
    )json");
    ecclesia::RedfishVariant sensor_variant = sensor_json->GetRoot();
    absl::StatusOr<QueryResult> resume_query_result = (*qp)->Resume({
        .trie_node = find_sensor_trie_node->second,
        .redfish_variant = sensor_variant,
        .variables = args1,
    });
    EXPECT_THAT(resume_query_result, IsOk());

    QueryResult expect_query_result = ParseTextProtoOrDie(R"pb(
      query_id: "SubscriptionTest"
      data {
        fields {
          key: "Sensors"
          value {
            list_value {
              values {
                subquery_value {
                  fields {
                    key: "Name"
                    value { string_value: "fan3" }
                  }
                  fields {
                    key: "Reading"
                    value { double_value: 16115 }
                  }
                }
              }
            }
          }
        }
      }
    )pb");

    // Verify query result.
    EXPECT_THAT(expect_query_result,
                ecclesia::IgnoringRepeatedFieldOrdering(
                    ecclesia::EqualsProto(*resume_query_result)));
  }

  {
    // Case2: Assembly RedPath query resumes on receiving an event on a
    // collection type resource. `/Chassis[*]/Assembly`.
    //
    // Setup: Make sure we have a trie node to resume Assembly RedPath query.
    auto find_assembly_trie_node =
        context->redpath_to_trie_node.find(kAssemblyRedPath);
    ASSERT_TRUE(find_assembly_trie_node != context->redpath_to_trie_node.end());
    ASSERT_THAT(find_assembly_trie_node->second, NotNull());

    // Mock sensor event.
    std::unique_ptr<ecclesia::RedfishInterface> assembly_json =
        ecclesia::NewJsonMockupInterface(R"json(
        {
          "@odata.id": "/redfish/v1/Chassis/chassis/Assembly",
          "@odata.type": "#Assembly.v1_2_0.Assembly",
          "Id": "Assembly",
          "Name": "indus",
          "Assemblies": [
            {
              "@odata.id": "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/0",
              "MemberId": "0",
              "Name": "indus"
            },
            {
              "@odata.id": "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/1",
              "MemberId": "1",
              "Name": "fan_40mm"
            },
            {
              "@odata.id": "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/2",
              "MemberId": "2",
              "Name": "fan_40mm"
            },
            {
              "@odata.id": "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/3",
              "MemberId": "3",
              "Name": "fan_assembly"
            }
          ]
        }
    )json");
    ecclesia::RedfishVariant assembly_variant = assembly_json->GetRoot();
    absl::StatusOr<QueryResult> resume_query_result = (*qp)->Resume({
        .trie_node = find_assembly_trie_node->second,
        .redfish_variant = assembly_variant,
        .variables = args1,
    });
    EXPECT_THAT(resume_query_result, IsOk());

    QueryResult expect_query_result = ParseTextProtoOrDie(R"pb(
      query_id: "SubscriptionTest"
      data {
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
      }
    )pb");

    // Verify query result.
    EXPECT_THAT(expect_query_result,
                ecclesia::IgnoringRepeatedFieldOrdering(
                    ecclesia::EqualsProto(*resume_query_result)));
  }
}

TEST_F(QueryPlannerTestRunner, CannotNormalizeInvalidEvent) {
  SetTestParams("indus_hmb_shim/mockup.shar");

  // Setup: Build query planner and Execute query to create subscription.
  DelliciusQuery subscription_query = GetSubscriptionQuery();
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp = BuildQueryPlanner(
      {.query = subscription_query,
       .redpath_rules = {.redpaths_to_subscribe = {std::string(kSensorRedPath),
                                                   std::string(
                                                       kAssemblyRedPath)}},
       .normalizer = normalizer.get(),
       .redfish_interface = intf_.get()});
  ASSERT_THAT(qp, IsOk());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  absl::StatusOr<QueryExecutionResult> result = (*qp)->Run({args1});
  const std::unique_ptr<QueryPlannerIntf::SubscriptionContext> &context =
      result->subscription_context;
  ASSERT_THAT(context, NotNull());

  // Setup: Make sure we have a trie node to resume Sensor RedPath query.
  auto find_sensor_trie_node =
      context->redpath_to_trie_node.find(kSensorRedPath);
  ASSERT_TRUE(find_sensor_trie_node != context->redpath_to_trie_node.end());
  ASSERT_THAT(find_sensor_trie_node->second, NotNull());

  // Mock invalid sensor event.
  // The event is invalid as it is not a Redfish resource that can be
  // normalized.
  std::unique_ptr<ecclesia::RedfishInterface> sensor_json =
      ecclesia::NewJsonMockupInterface(R"json(
    [
      {
        "@odata.id": "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/5"
      }
    ]
  )json");
  ecclesia::RedfishVariant sensor_variant = sensor_json->GetRoot();
  absl::StatusOr<QueryResult> resume_query_result = (*qp)->Resume({
      .trie_node = find_sensor_trie_node->second,
      .redfish_variant = sensor_variant,
      .variables = args1,
  });
  EXPECT_THAT(resume_query_result, IsOk());

  QueryResult expect_query_result = ParseTextProtoOrDie(R"pb(
    query_id: "SubscriptionTest"
  )pb");

  // Verify query result.
  EXPECT_THAT(expect_query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(*resume_query_result)));
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
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp =
      BuildQueryPlanner({.query = query,
                         .redpath_rules = {.redpath_to_query_params =
                                               RedPathRedfishQueryParams{}},
                         .normalizer = normalizer.get(),
                         .redfish_interface = intf_.get()});
  EXPECT_THAT(qp, IsOk());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  absl::StatusOr<QueryExecutionResult> query_result = (*qp)->Run({args1});

  EXPECT_THAT(query_result, IsOk());
  EXPECT_THAT(expected_query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(query_result->query_result)));
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

  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp =
      BuildQueryPlanner({.query = query,
                         .redpath_rules = {.redpath_to_query_params = std::move(
                                               redpath_redfish_query_params)},
                         .normalizer = normalizer.get(),
                         .redfish_interface = intf_.get()});
  EXPECT_THAT(qp, IsOk());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  EXPECT_THAT((*qp)->Run({args1}), IsOk());
  // Since the order in which the predicates are passed to the $filter string
  // construction method is non-deterministic, the Redfish request can be in two
  // possible forms.
  EXPECT_TRUE(filter_requested1);
}

TEST_F(QueryPlannerTestRunner, QueryPlannerHaltsOnWrongPropertyType) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "ChassisTestWrongPropertyType"
        subquery {
          subquery_id: "Chassis"
          redpath: "/Chassis[*]"
          properties { property: "Id" type: STRING }
          properties { property: "Name" type: INT64 }
        }
      )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();
  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp =
      BuildQueryPlanner({.query = query,
                         .redpath_rules = {},
                         .normalizer = normalizer.get(),
                         .redfish_interface = intf_.get()});
  EXPECT_THAT(qp, IsOk());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  absl::StatusOr<QueryExecutionResult> result = (*qp)->Run({args1});

  EXPECT_THAT(result.status().code(),
              testing::Eq(absl::StatusCode::kInvalidArgument));
}

// Test Query Planner's ability to generate sub-fru stable IDs.
TEST_F(QueryPlannerTestRunner, QueryPlannerGeneratesStableId) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "EmbeddedResource"
        service_root: "/redfish/v1"
        subquery {
          subquery_id: "EmbeddedServiceLabel"
          uri: "/redfish/v1/embedded/logical/resource1"
          properties { property: "Id" type: STRING }
        }
        subquery {
          subquery_id: "EmbeddedDevpath"
          uri: "/redfish/v1/embedded/logical/resource2"
          properties { property: "Id" type: STRING }
        }
      )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");
  server_->AddHttpGetHandler("/redfish/v1/embedded/logical/resource1",
                             [&](ServerRequestInterface *req) {
                               SetContentType(req, "application/json");
                               req->OverwriteResponseHeader("OData-Version",
                                                            "4.0");
                               req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/embedded/logical/resource1",
          "Id": "resource1",
          "Name": "resource1",
          "Oem": {
            "Google": {
              "LocationContext": {
                "ServiceLabel": "chassis",
                "EmbeddedLocationContext": ["embedded", "logical"]
              }
            }
          }
        })json");
                               req->Reply();
                             });

  server_->AddHttpGetHandler("/redfish/v1/embedded/logical/resource2",
                             [&](ServerRequestInterface *req) {
                               SetContentType(req, "application/json");
                               req->OverwriteResponseHeader("OData-Version",
                                                            "4.0");
                               req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/embedded/logical/resource2",
          "Id": "resource2",
          "Name": "resource2",
          "Oem": {
            "Google": {
              "LocationContext": {
                "Devpath": "/phys",
                "EmbeddedLocationContext": ["embedded", "logical"]
              }
            }
          }
        })json");
                               req->Reply();
                             });

  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();
  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp =
      BuildQueryPlanner({.query = query,
                         .redpath_rules = {},
                         .normalizer = normalizer.get(),
                         .redfish_interface = intf_.get()});
  EXPECT_THAT(qp, IsOk());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  auto result = (*qp)->Run({args1});
  EXPECT_THAT(result, IsOk());
  QueryResult expect_query_result = ParseTextProtoOrDie(R"pb(
    query_id: "EmbeddedResource"
    data {
      fields {
        key: "EmbeddedServiceLabel"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "Id"
                  value { string_value: "resource1" }
                }
                fields {
                  key: "__EmbeddedLocationContext__"
                  value {
                    identifier {
                      embedded_location_context: "/embedded/logical"
                    }
                  }
                }
                fields {
                  key: "__StableName__"
                  value { identifier { stable_name: "resource1" } }
                }
              }
            }
          }
        }
      }
      fields {
        key: "EmbeddedDevpath"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "Id"
                  value { string_value: "resource2" }
                }
                fields {
                  key: "__EmbeddedLocationContext__"
                  value {
                    identifier {
                      embedded_location_context: "/embedded/logical"
                    }
                  }
                }
                fields {
                  key: "__LocalDevpath__"
                  value { identifier { local_devpath: "/phys" } }
                }
                fields {
                  key: "__StableName__"
                  value { identifier { stable_name: "resource2" } }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  // Verify query result.
  EXPECT_THAT(expect_query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto((*result).query_result)));
}

TEST_F(QueryPlannerTestRunner, QueryPlannerExecutesRedfishMetricsCorrectly) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "ChassisSubTreeTest"
        subquery {
          subquery_id: "Chassis"
          redpath: "/Chassis[*]"
          properties { property: "Id" type: STRING }
        }
        subquery {
          subquery_id: "Sensors"
          root_subquery_ids: "Chassis"
          redpath: "/Sensors[ReadingUnits=RPM]"
          properties { property: "Name" type: STRING }
        }
        subquery {
          subquery_id: "Assembly"
          root_subquery_ids: "Sensors"
          redpath: "/RelatedItem[0]"
          properties { property: "MemberId" type: STRING }
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

  /* Expected query result - here for just reference;
  QueryResult expected_query_result = ParseTextProtoOrDie(R"pb(
    query_id: "ChassisSubTreeTest"
    stats {
      redfish_metrics {
        uri_to_metrics_map {
          key: "/redfish/v1"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 6.556167
                min_response_time_ms: 6.556167
                request_count: 1
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 7.140871
                min_response_time_ms: 7.140871
                request_count: 1
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 6.924295
                min_response_time_ms: 6.924295
                request_count: 1
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/1"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 47.991727
                min_response_time_ms: 47.991727
                request_count: 1
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/2"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 27.222683
                min_response_time_ms: 27.222683
                request_count: 1
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/4"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 35.618979
                min_response_time_ms: 35.618979
                request_count: 1
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/5"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 20.266004
                min_response_time_ms: 20.266004
                request_count: 1
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/6"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 22.556055
                min_response_time_ms: 22.556055
                request_count: 1
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/7"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 23.725464
                min_response_time_ms: 23.725464
                request_count: 1
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/8"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 24.299672
                min_response_time_ms: 24.299672
                request_count: 1
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/9"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 28.325148
                min_response_time_ms: 28.325148
                request_count: 1
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Sensors"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 22.062998
                min_response_time_ms: 22.062998
                request_count: 1
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Sensors/i_cpu0_t"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 17.257873
                min_response_time_ms: 14.314382
                request_count: 2
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Sensors/i_cpu1_t"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 14.200718
                min_response_time_ms: 9.346057
                request_count: 2
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Sensors/indus_cpu0_pwmon"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 20.064532
                min_response_time_ms: 17.189137
                request_count: 2
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Sensors/indus_cpu1_pwmon"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 22.454166
                min_response_time_ms: 13.069837
                request_count: 2
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Sensors/indus_eat_temp"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 17.508497
                min_response_time_ms: 14.023694
                request_count: 2
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Sensors/indus_fan0_rpm"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 51.705523
                min_response_time_ms: 14.958351
                request_count: 2
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Sensors/indus_fan1_rpm"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 22.619286
                min_response_time_ms: 20.15042
                request_count: 2
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Sensors/indus_fan2_rpm"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 13.301133
                min_response_time_ms: 9.266185
                request_count: 2
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Sensors/indus_fan3_rpm"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 17.896466
                min_response_time_ms: 10.218635
                request_count: 2
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Sensors/indus_fan4_rpm"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 22.867223
                min_response_time_ms: 14.362638
                request_count: 2
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Sensors/indus_fan5_rpm"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 22.123926
                min_response_time_ms: 15.940624
                request_count: 2
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Sensors/indus_fan6_rpm"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 20.975509
                min_response_time_ms: 9.882122
                request_count: 2
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Sensors/indus_fan7_rpm"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 20.086548
                min_response_time_ms: 17.435537
                request_count: 2
              }
            }
          }
        }
        uri_to_metrics_map {
          key: "/redfish/v1/Chassis/chassis/Sensors/indus_latm_temp"
          value {
            request_type_to_metadata {
              key: "GET"
              value {
                max_response_time_ms: 13.242509
                min_response_time_ms: 9.777162
                request_count: 2
              }
            }
          }
        }
      }
    }
    data {
      fields {
        key: "Chassis"
        value {
          list_value {
            values {
              subquery_value {
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
                            key: "Assembly"
                            value {
                              list_value {
                                values {
                                  subquery_value {
                                    fields {
                                      key: "MemberId"
                                      value { string_value: "1" }
                                    }
                                  }
                                }
                              }
                            }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "fan0" }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Assembly"
                            value {
                              list_value {
                                values {
                                  subquery_value {
                                    fields {
                                      key: "MemberId"
                                      value { string_value: "2" }
                                    }
                                  }
                                }
                              }
                            }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "fan1" }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Assembly"
                            value {
                              list_value {
                                values {
                                  subquery_value {
                                    fields {
                                      key: "MemberId"
                                      value { string_value: "4" }
                                    }
                                  }
                                }
                              }
                            }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "fan2" }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Assembly"
                            value {
                              list_value {
                                values {
                                  subquery_value {
                                    fields {
                                      key: "MemberId"
                                      value { string_value: "5" }
                                    }
                                  }
                                }
                              }
                            }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "fan3" }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Assembly"
                            value {
                              list_value {
                                values {
                                  subquery_value {
                                    fields {
                                      key: "MemberId"
                                      value { string_value: "6" }
                                    }
                                  }
                                }
                              }
                            }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "fan4" }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Assembly"
                            value {
                              list_value {
                                values {
                                  subquery_value {
                                    fields {
                                      key: "MemberId"
                                      value { string_value: "7" }
                                    }
                                  }
                                }
                              }
                            }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "fan5" }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Assembly"
                            value {
                              list_value {
                                values {
                                  subquery_value {
                                    fields {
                                      key: "MemberId"
                                      value { string_value: "8" }
                                    }
                                  }
                                }
                              }
                            }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "fan6" }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "Assembly"
                            value {
                              list_value {
                                values {
                                  subquery_value {
                                    fields {
                                      key: "MemberId"
                                      value { string_value: "9" }
                                    }
                                  }
                                }
                              }
                            }
                          }
                          fields {
                            key: "Name"
                            value { string_value: "fan7" }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  )pb");
  */
  SetTestParams("indus_hmb_shim/mockup.shar");
  MetricalRedfishTransport *metrical_transport_ptr = nullptr;

  auto transport = std::make_unique<MetricalRedfishTransport>(
      server_->RedfishClientTransport(), Clock::RealClock());
  metrical_transport_ptr = transport.get();
  auto cache = std::make_unique<NullCache>(transport.get());
  auto intf = NewHttpInterface(std::move(transport), std::move(cache),
                               RedfishInterface::kTrusted);
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> qp =
      BuildQueryPlanner({.query = query,
                         .redpath_rules = {},
                         .normalizer = normalizer.get(),
                         .redfish_interface = intf.get(),
                         .metrical_transport = metrical_transport_ptr});

  EXPECT_THAT(qp, IsOk());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  absl::StatusOr<QueryExecutionResult> result = (*qp)->Run({args1});

  EXPECT_THAT(result, IsOk());
  ASSERT_TRUE(result->query_result.has_stats());
  ASSERT_TRUE(result->query_result.stats().has_redfish_metrics());
}

}  // namespace

}  // namespace ecclesia
