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

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/function_ref.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "ecclesia/lib/network/testing.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/engine/normalizer.h"
#include "ecclesia/lib/redfish/redpath/engine/query_planner.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/testing/grpc_dynamic_mockup_server.h"
#include "ecclesia/lib/redfish/testing/json_mockup.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/grpc.h"
#include "ecclesia/lib/redfish/transport/grpc_tls_options.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/metrical_transport.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/status/test_macros.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/lib/time/clock_fake.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

namespace {

using RedPathRedfishQueryParams =
    absl::flat_hash_map<std::string /* RedPath */, GetParams>;

using ::tensorflow::serving::net_http::HTTPStatusCode;
using ::tensorflow::serving::net_http::ServerRequestInterface;
using ::tensorflow::serving::net_http::SetContentType;
using ::tensorflow::serving::net_http::SetContentTypeTEXT;
using ::testing::AnyNumber;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::TestParamInfo;
using ::testing::TestWithParam;
using ::testing::UnorderedElementsAre;
using ::testing::Values;
using ::testing::WithParamInterface;
using QueryExecutionResult = QueryPlannerIntf::QueryExecutionResult;
using QueryPlannerOptions = QueryPlanner::ImplOptions;

constexpr absl::string_view kSensorRedPath = "/Chassis[*]/Sensors[*]";
constexpr absl::string_view kSensorsRedPath = "/Chassis[*]/Sensors";
constexpr absl::string_view kAssemblyRedPath = "/Chassis[*]/Assembly";
constexpr absl::string_view kAssembliesRedPath =
    "/Chassis[*]/Assembly/Assemblies";
constexpr absl::string_view kInvalidRedPath = "/Chassis[*]/Unknown";

class TestRedpathNormalizerImpl final
    : public RedpathNormalizer::ImplInterface {
 public:
  TestRedpathNormalizerImpl() = default;

 protected:
  absl::Status Normalize(const RedfishObject &redfish_object,
                         const DelliciusQuery::Subquery &subquery,
                         ecclesia::QueryResultData &data_set,
                         const RedpathNormalizerOptions &options) override {
    QueryValue test_value = ParseTextProtoOrDie(R"pb(
      string_value: "normalized_value"
    )pb");
    (*data_set.mutable_fields())["normalized_key"] = test_value;
    return absl::OkStatus();
  }
};

inline std::unique_ptr<RedpathNormalizer> BuildTestRedpathNormalizer() {
  auto normalizer = std::make_unique<RedpathNormalizer>();
  normalizer->AddRedpathNormalizer(
      std::make_unique<TestRedpathNormalizerImpl>());
  return normalizer;
}

class QueryPlannerTestRunner : public ::testing::Test {
 protected:
  QueryPlannerTestRunner() = default;
  void SetTestParams(absl::string_view mockup) {
    server_ = std::make_unique<FakeRedfishServer>(mockup);
    server_->EnableAllParamsGetHandler();
    intf_ = server_->RedfishClientInterface();
  }

  absl::StatusOr<QueryExecutionResult> PlanAndExecuteQuery(
      const DelliciusQuery &query,
      QueryPlanner::ExecutionMode execution_mode =
          QueryPlanner::ExecutionMode::kFailOnFirstError) {
    CHECK(server_ != nullptr && intf_ != nullptr) << "Test parameters not set!";
    std::unique_ptr<RedpathNormalizer> normalizer =
        BuildDefaultRedpathNormalizer();

    ECCLESIA_ASSIGN_OR_RETURN(
        std::unique_ptr<QueryPlannerIntf> qp,
        BuildQueryPlanner({.query = &query,
                           .normalizer = normalizer.get(),
                           .redfish_interface = intf_.get(),
                           .redpath_rules = {},
                           .execution_mode = execution_mode}));
    ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
    return qp->Run({.variables = args1});
  }

  std::unique_ptr<FakeRedfishServer> server_;
  std::unique_ptr<RedfishInterface> intf_;
};

class QueryPlannerGrpcTestRunner : public ::testing::Test {
 protected:
  QueryPlannerGrpcTestRunner() = default;
  void SetTestParams(absl::string_view mockup) {
    int port = ecclesia::FindUnusedPortOrDie();
    server_ = std::make_unique<ecclesia::GrpcDynamicMockupServer>(
        mockup, "localhost", port);
    StaticBufferBasedTlsOptions options;
    options.SetToInsecure();
    absl::StatusOr<std::unique_ptr<RedfishTransport>> transport =
        CreateGrpcRedfishTransport(absl::StrCat("localhost:", port),
                                   {.clock = &clock_},
                                   options.GetChannelCredentials());
    CHECK_OK(transport.status());
    auto cache_factory = [this](RedfishTransport *transport) {
      return std::make_unique<ecclesia::TimeBasedCache>(transport, &clock_,
                                                        cache_duration_);
    };
    intf_ = NewHttpInterface(std::move(*transport), cache_factory,
                             RedfishInterface::kTrusted);
  }

  ecclesia::FakeClock clock_;
  absl::Notification notification_;
  std::unique_ptr<ecclesia::GrpcDynamicMockupServer> server_;
  std::unique_ptr<RedfishInterface> intf_;
  absl::Duration cache_duration_ = absl::Seconds(1);
};

class MockRedfishObject : public RedfishObject {
 public:
  MOCK_METHOD(RedfishVariant, Get, (absl::string_view, GetParams),
              (const, override));
  MOCK_METHOD(std::optional<std::string>, GetUriString, (), (const, override));
  MOCK_METHOD(absl::StatusOr<std::unique_ptr<RedfishObject>>,
              EnsureFreshPayload, (GetParams params), (override));
  MOCK_METHOD(nlohmann::json, GetContentAsJson, (), (const, override));
  MOCK_METHOD(std::string, DebugString, (), (const, override));
  MOCK_METHOD(void, PrintDebugString, (), (const, override));
  MOCK_METHOD(
      void, ForEachProperty,
      (absl::FunctionRef<RedfishIterReturnValue(absl::string_view key,
                                                RedfishVariant value)>),
      (override));
  // Gmock does not support operator[] overloading, so we proxy with at().
  MOCK_METHOD(RedfishVariant, at, (absl::string_view), (const));
  RedfishVariant operator[](absl::string_view node_name) const override {
    return at(node_name);
  }
};

class MockRedfishIterable : public RedfishIterable {
 public:
  MOCK_METHOD(size_t, Size, (), (override));
  MOCK_METHOD(bool, Empty, (), (override));
  // Gmock does not support operator[] overloading, so we proxy with at().
  MOCK_METHOD(RedfishVariant, at, (int), (const));
  RedfishVariant operator[](int index) const override {
    return at(index);
  }
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
          subquery_id: "AllSensorsNoRpm"
          redpath: "/Chassis[*]/Sensors[ReadingUnits!=RPM]"
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
    stats { payload_size: 707 num_cache_misses: 70 }
    data {
      fields {
        key: "AllSensorsNoRpm"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "indus_eat_temp" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "indus_latm_temp" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "CPU0" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "CPU1" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "CPU0" }
                }
              }
            }
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

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({.query = &query,
                                             .normalizer = normalizer.get(),
                                             .redfish_interface = intf_.get(),
                                             .redpath_rules = {}}));

  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1});

  EXPECT_FALSE(result.query_result.has_status());
  EXPECT_THAT(expected_query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(result.query_result)));
}

TEST_F(QueryPlannerTestRunner,
       QueryPlannerExecutesQueryWithAdditionalNormalizers) {
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
          subquery_id: "AllSensorsNoRpm"
          redpath: "/Chassis[*]/Sensors[ReadingUnits!=RPM]"
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
    stats { payload_size: 745 num_cache_misses: 70 }
    data {
      fields {
        key: "normalized_key"
        value { string_value: "normalized_value" }
      }
      fields {
        key: "AllSensorsNoRpm"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "indus_eat_temp" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "indus_latm_temp" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "CPU0" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "CPU1" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "CPU0" }
                }
              }
            }
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
  std::unique_ptr<RedpathNormalizer> test_normalizer =
      BuildTestRedpathNormalizer();

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner({.query = &query,
                             .normalizer = normalizer.get(),
                             .additional_normalizers = {test_normalizer.get()},
                             .redfish_interface = intf_.get(),
                             .redpath_rules = {}}));

  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1});

  EXPECT_FALSE(result.query_result.has_status());
  EXPECT_THAT(expected_query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(result.query_result)));
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

  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({.query = &query,
                                             .normalizer = normalizer.get(),
                                             .redfish_interface = intf.get(),
                                             .redpath_rules = {}}));
  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  EXPECT_FALSE(qp->Run({.variables = args1}).query_result.has_status());
  EXPECT_FALSE(qp->Run({.variables = args1}).query_result.has_status());

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

  RedPathRules redpath_rules = {
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

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner({.query = &query,
                             .normalizer = normalizer.get(),
                             .redfish_interface = intf_.get(),
                             .redpath_rules = std::move(redpath_rules)}));
  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  EXPECT_FALSE(qp->Run({.variables = args1}).query_result.has_status());
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

  RedPathRules redpath_rules = {
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

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner({.query = &query,
                             .normalizer = normalizer.get(),
                             .redfish_interface = intf_.get(),
                             .redpath_rules = std::move(redpath_rules)}));
  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  EXPECT_FALSE(qp->Run({.variables = args1}).query_result.has_status());
  // Since the order in which the predicates are passed to the $filter string
  // construction method is non-deterministic, the Redfish request can be in two
  // possible forms.
  EXPECT_TRUE(filter_requested1 || filter_requested2);
}

TEST_F(QueryPlannerTestRunner,
       QueryPlannerAppliesFilterWithFuzzyComparisonFromRules) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "SensorTest"
        service_root: "/redfish/v1"
        subquery {
          subquery_id: "SensorsGreater"
          redpath: "/Chassis[*]/Sensors[Id~>=40]"
          properties { property: "Id" type: STRING }
        }
        subquery {
          subquery_id: "SensorsLesser"
          redpath: "/Chassis[*]/Sensors[Id<~5]"
          properties { property: "Id" type: STRING }
        }
      )pb");

  RedPathRules redpath_rules = {
      .redpath_to_query_params = {
          {"/Chassis[*]/Sensors", {.filter = RedfishQueryParamFilter("")}}}};

  SetTestParams("indus_hmb_shim/mockup.shar");
  bool filter_requested1 = false;
  bool filter_requested2 = false;
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis/"
      "Sensors?$filter=Id%20ge%2040%20or%20Id%20lt%205",
      [&](ServerRequestInterface *req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        filter_requested1 = true;
        req->WriteResponseString(R"json({"@odata.id": "uri"})json");
        req->Reply();
      });
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis/"
      "Sensors?$filter=Id%20lt%205%20or%20Id%20ge%2040",
      [&](ServerRequestInterface *req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        filter_requested2 = true;
        req->WriteResponseString(R"json({"@odata.id": "uri"})json");
        req->Reply();
      });

  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner({.query = &query,
                             .normalizer = normalizer.get(),
                             .redfish_interface = intf_.get(),
                             .redpath_rules = std::move(redpath_rules)}));
  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  EXPECT_FALSE(qp->Run({.variables = args1}).query_result.has_status());
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
    stats { payload_size: 160 num_cache_misses: 47 }
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

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({.query = &query,
                                             .normalizer = normalizer.get(),
                                             .redfish_interface = intf_.get(),
                                             .redpath_rules = {}}));
  ASSERT_THAT(qp, NotNull());
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
  QueryExecutionResult result = qp->Run({.variables = args1});

  EXPECT_FALSE(result.query_result.has_status());
  EXPECT_THAT(result.query_result,
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

  result = qp->Run({.variables = args1});
  EXPECT_FALSE(result.query_result.has_status());
  EXPECT_THAT(result.query_result,
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
    stats { payload_size: 157 num_cache_misses: 18 }
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

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({.query = &query,
                                             .normalizer = normalizer.get(),
                                             .redfish_interface = intf_.get(),
                                             .redpath_rules = {}}));
  ASSERT_THAT(qp, NotNull());
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

  QueryExecutionResult result = qp->Run({.variables = multi_value_args});
  EXPECT_FALSE(result.query_result.has_status());
  EXPECT_THAT(result.query_result,
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
    stats { payload_size: 435 num_cache_misses: 18 }
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

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({.query = &query,
                                             .normalizer = normalizer.get(),
                                             .redfish_interface = intf_.get(),
                                             .redpath_rules = {}}));
  ASSERT_THAT(qp, NotNull());
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
  QueryExecutionResult result = qp->Run({.variables = multi_value_args});
  EXPECT_FALSE(result.query_result.has_status());
  EXPECT_THAT(result.query_result,
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
    stats { payload_size: 386 num_cache_misses: 32 }
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

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({.query = &query,
                                             .normalizer = normalizer.get(),
                                             .redfish_interface = intf_.get(),
                                             .redpath_rules = {}}));

  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result =
      qp->Run({.variables = args1, .enable_url_annotation = true});

  EXPECT_FALSE(result.query_result.has_status());
  EXPECT_THAT(result.query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(expected_query_result)));
}

TEST_F(QueryPlannerTestRunner,
       QueryPlannerExecutesWithInjectedRedfishInterface) {
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
    stats { payload_size: 153 num_cache_misses: 32 }
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
                        }
                      }
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
            }
          }
        }
      }
    }
  )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");

  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(qp, BuildQueryPlanner({.query = &query,
                                                 .normalizer = normalizer.get(),
                                                 .redpath_rules = {}}));

  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result =
      qp->Run({.variables = args1, .redfish_interface = intf_.get()});

  EXPECT_FALSE(result.query_result.has_status());
  EXPECT_THAT(result.query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(expected_query_result)));
}

TEST_F(QueryPlannerTestRunner, QueryPlannerRunWithoutRedfishInterface) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "ChassisSubTreeTest"
        subquery {
          subquery_id: "Chassis"
          redpath: "/Chassis[*]"
          properties { property: "Id" type: STRING }
        }
      )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");

  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(qp, BuildQueryPlanner({.query = &query,
                                                 .normalizer = normalizer.get(),
                                                 .redpath_rules = {}}));

  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1});

  EXPECT_TRUE(result.query_result.has_status());
  EXPECT_THAT(result.query_result.status().error_code(),
              ecclesia::ErrorCode::ERROR_INTERNAL);
  EXPECT_THAT(
      result.query_result.status().errors(),
      UnorderedElementsAre("Redfish interface is not set before query "
                           "execution"));
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
    stats { payload_size: 58 num_cache_misses: 18 }
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

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp,
      BuildQueryPlanner(
          {.query = &subscription_query,
           .normalizer = normalizer.get(),
           .redfish_interface = intf_.get(),
           .redpath_rules = {
               .redpath_to_query_params =
                   {{std::string(kAssemblyRedPath),
                     {.expand = RedfishQueryParamExpand(
                          {.type =
                               RedfishQueryParamExpand::ExpandType::kNotLinks,
                           .levels = 1})}}},
               .redpaths_to_subscribe = {std::string(kSensorRedPath),
                                         std::string(kAssemblyRedPath)}}}));
  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result =
      qp->Run({.variables = args1, .query_type = QueryType::kSubscription});

  EXPECT_FALSE(result.query_result.has_status());
  EXPECT_THAT(result.query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(expect_query_result)));

  // Verify Subscription context is valid.
  const std::unique_ptr<QueryPlannerIntf::SubscriptionContext> &context =
      result.subscription_context;
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
TEST_F(QueryPlannerTestRunner,
       SubscriptionToNonNavigationalPropertyDoesNotReturnError) {
  SetTestParams("indus_hmb_shim/mockup.shar");

  DelliciusQuery subscription_query = GetSubscriptionQuery();
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner(
              {.query = &subscription_query,
               .normalizer = normalizer.get(),
               .redfish_interface = intf_.get(),
               .redpath_rules = {.redpaths_to_subscribe = {
                                     std::string(kAssembliesRedPath)}}}));
  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result =
      qp->Run({.variables = args1, .query_type = QueryType::kSubscription});
  EXPECT_FALSE(result.query_result.has_status());
}

// Subscribe to unknown property fails.
TEST_F(QueryPlannerTestRunner,
       SubscriptionToUnknownPropertyDoesNotReturnError) {
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

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner(
              {.query = &subscription_query,
               .normalizer = normalizer.get(),
               .redfish_interface = intf_.get(),
               .redpath_rules = {
                   .redpaths_to_subscribe = {std::string(kInvalidRedPath)}}}));
  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result =
      qp->Run({.variables = args1, .query_type = QueryType::kSubscription});
  EXPECT_FALSE(qp->Run({.variables = args1}).query_result.has_status());
}

// Subscribe to unknown property fails.
TEST_F(QueryPlannerTestRunner, TemplatedQueryWithNoVarsSucceeds) {
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

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(qp, BuildQueryPlanner({
                                  .query = &subscription_query,
                                  .normalizer = normalizer.get(),
                                  .redfish_interface = intf_.get(),
                              }));
  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1});
  EXPECT_FALSE(result.query_result.has_status());
}

// Successful Resume
TEST_F(QueryPlannerTestRunner, ResumesQueryAfterEvent) {
  SetTestParams("indus_hmb_shim/mockup.shar");

  // Setup: Build query planner and Execute query to create subscription.
  DelliciusQuery subscription_query = GetSubscriptionQuery();
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner(
              {.query = &subscription_query,
               .normalizer = normalizer.get(),
               .redfish_interface = intf_.get(),
               .redpath_rules = {
                   .redpaths_to_subscribe = {std::string(kSensorRedPath),
                                             std::string(kAssemblyRedPath)}}}));

  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result =
      qp->Run({.variables = args1, .query_type = QueryType::kSubscription});
  const std::unique_ptr<QueryPlannerIntf::SubscriptionContext> &context =
      result.subscription_context;
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
    absl::StatusOr<QueryResult> resume_query_result = qp->Resume({
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
    absl::StatusOr<QueryResult> resume_query_result = qp->Resume({
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

TEST_F(QueryPlannerTestRunner,
       ResumesQueryAfterEventWithAdditionalNormalizers) {
  SetTestParams("indus_hmb_shim/mockup.shar");

  // Setup: Build query planner and Execute query to create subscription.
  DelliciusQuery subscription_query = GetSubscriptionQuery();
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();
  std::unique_ptr<RedpathNormalizer> test_normalizer =
      BuildTestRedpathNormalizer();

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner(
              {.query = &subscription_query,
               .normalizer = normalizer.get(),
               .additional_normalizers = {test_normalizer.get()},
               .redfish_interface = intf_.get(),
               .redpath_rules = {
                   .redpaths_to_subscribe = {std::string(kSensorRedPath),
                                             std::string(kAssemblyRedPath)}}}));

  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result =
      qp->Run({.variables = args1, .query_type = QueryType::kSubscription});
  const std::unique_ptr<QueryPlannerIntf::SubscriptionContext> &context =
      result.subscription_context;
  ASSERT_THAT(context, NotNull());

  // Case1: Sensor RedPath query resumes on receiving an event on specific
  // Sensor resource in the collection `/Chassis[*]/Sensors[*]`.

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
  absl::StatusOr<QueryResult> resume_query_result = qp->Resume({
      .trie_node = find_sensor_trie_node->second,
      .redfish_variant = sensor_variant,
      .variables = args1,
  });
  EXPECT_THAT(resume_query_result, IsOk());

  QueryResult expect_query_result = ParseTextProtoOrDie(R"pb(
    query_id: "SubscriptionTest"
    data {
      fields {
        key: "normalized_key"
        value { string_value: "normalized_value" }
      }
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

TEST_F(QueryPlannerTestRunner,
       ResumePopulatesErrorsCorrectlyWhenResolvingCollectionMembers) {
  SetTestParams("indus_hmb_shim/mockup.shar");
  // Setup: Build query planner and Execute query to create subscription.
  DelliciusQuery subscription_collection_query = ParseTextProtoOrDie(
      R"pb(
        query_id: "SubscriptionToCollectionTest"
        subquery {
          subquery_id: "Sensors"
          redpath: "/Chassis[*]/Sensors"
          properties { property: "Id" type: STRING }
        }
        subquery {
          subquery_id: "SensorMembers"
          root_subquery_ids: "Sensors"
          redpath: "/Members[*]"
          properties { property: "Name" type: STRING }
          properties { property: "Reading" type: DOUBLE }
        }
      )pb");
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner(
              {.query = &subscription_collection_query,
               .normalizer = normalizer.get(),
               .redfish_interface = intf_.get(),
               .redpath_rules = {
                   .redpaths_to_subscribe = {std::string(kSensorsRedPath)}}}));
  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({
      .variables = args1,
      .query_type = QueryType::kSubscription,
  });
  const std::unique_ptr<QueryPlannerIntf::SubscriptionContext> &context =
      result.subscription_context;
  ASSERT_THAT(context, NotNull());

  // Mock one sensor in the collection to return an error.
  server_->AddHttpGetHandlerWithStatus(
      "/redfish/v1/Chassis/chassis/Sensors/indus_fan3_rpm", "",
      tensorflow::serving::net_http::HTTPStatusCode::SERVICE_UNAV);

  auto find_sensor_trie_node =
      context->redpath_to_trie_node.find(kSensorsRedPath);
  ASSERT_TRUE(find_sensor_trie_node != context->redpath_to_trie_node.end());
  ASSERT_THAT(find_sensor_trie_node->second, NotNull());
  // We don't want to use a JsonMock for the collection here. We need the QP
  // to try to resolve collection members, and fail at that point.
  ecclesia::RedfishVariant sensors_variant =
      intf_->UncachedGetUri("/redfish/v1/Chassis/chassis/Sensors", {});
  absl::StatusOr<QueryResult> resume_query_result = qp->Resume({
      .trie_node = find_sensor_trie_node->second,
      .redfish_variant = sensors_variant,
      .variables = args1,
  });
  EXPECT_THAT(resume_query_result, IsOk());

  // Verify query result.
  EXPECT_THAT(resume_query_result->status().error_code(),
              Eq(ecclesia::ErrorCode::ERROR_UNAVAILABLE));
}

TEST_F(QueryPlannerTestRunner, CannotNormalizeInvalidEvent) {
  SetTestParams("indus_hmb_shim/mockup.shar");

  // Setup: Build query planner and Execute query to create subscription.
  DelliciusQuery subscription_query = GetSubscriptionQuery();
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner(
              {.query = &subscription_query,
               .normalizer = normalizer.get(),
               .redfish_interface = intf_.get(),
               .redpath_rules = {
                   .redpaths_to_subscribe = {std::string(kSensorRedPath),
                                             std::string(kAssemblyRedPath)}}}));
  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({
      .variables = args1,
      .query_type = QueryType::kSubscription,
  });
  const std::unique_ptr<QueryPlannerIntf::SubscriptionContext> &context =
      result.subscription_context;
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
  absl::StatusOr<QueryResult> resume_query_result = qp->Resume({
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

TEST_F(QueryPlannerTestRunner, ResumesQueryWithInjectedRedfishInterface) {
  SetTestParams("indus_hmb_shim/mockup.shar");

  // Setup: Build query planner and Execute query to create subscription.
  DelliciusQuery subscription_query = GetSubscriptionQuery();
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  std::unique_ptr<QueryPlannerIntf> qp;
  ;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner(
              {.query = &subscription_query,
               .normalizer = normalizer.get(),
               .redpath_rules = {
                   .redpaths_to_subscribe = {std::string(kSensorRedPath),
                                             std::string(kAssemblyRedPath)}}}));

  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1,
                                         .query_type = QueryType::kSubscription,
                                         .redfish_interface = intf_.get()});
  const std::unique_ptr<QueryPlannerIntf::SubscriptionContext> &context =
      result.subscription_context;
  ASSERT_THAT(context, NotNull());

  // Assembly RedPath query resumes on receiving an event on a
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
  absl::StatusOr<QueryResult> resume_query_result =
      qp->Resume({.trie_node = find_assembly_trie_node->second,
                  .redfish_variant = assembly_variant,
                  .variables = args1,
                  .redfish_interface = intf_.get()});
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

TEST_F(QueryPlannerTestRunner, ResumesQueryWithoutRedfishInterfaceError) {
  SetTestParams("indus_hmb_shim/mockup.shar");

  // Setup: Build query planner and Execute query to create subscription.
  DelliciusQuery subscription_query = GetSubscriptionQuery();
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner(
              {.query = &subscription_query,
               .normalizer = normalizer.get(),
               .redpath_rules = {
                   .redpaths_to_subscribe = {std::string(kSensorRedPath),
                                             std::string(kAssemblyRedPath)}}}));

  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1,
                                         .query_type = QueryType::kSubscription,
                                         .redfish_interface = intf_.get()});
  const std::unique_ptr<QueryPlannerIntf::SubscriptionContext> &context =
      result.subscription_context;
  ASSERT_THAT(context, NotNull());

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
  absl::StatusOr<QueryResult> resume_query_result = qp->Resume({
      .trie_node = find_sensor_trie_node->second,
      .redfish_variant = sensor_variant,
      .variables = args1,
  });
  EXPECT_THAT(resume_query_result, IsOk());
  EXPECT_TRUE(resume_query_result->has_status());
  EXPECT_THAT(resume_query_result->status().error_code(),
              ecclesia::ErrorCode::ERROR_INTERNAL);
  EXPECT_THAT(resume_query_result->status().errors(),
              UnorderedElementsAre("Redfish interface is not set when "
                                   "resuming with no remaining expressions "
                                   "to query"));
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
    stats { payload_size: 59 num_cache_misses: 2 }
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

  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp,
      BuildQueryPlanner({.query = &query,
                         .normalizer = normalizer.get(),
                         .redfish_interface = intf_.get(),
                         .redpath_rules = {.redpath_to_query_params =
                                               RedPathRedfishQueryParams{}}}));
  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult query_result = qp->Run({.variables = args1});

  EXPECT_FALSE(query_result.query_result.has_status());
  EXPECT_THAT(expected_query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(query_result.query_result)));
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

  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner(
              {.query = &query,
               .normalizer = normalizer.get(),
               .redfish_interface = intf_.get(),
               .redpath_rules = {.redpath_to_query_params = std::move(
                                     redpath_redfish_query_params)}}));
  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  EXPECT_FALSE(qp->Run({.variables = args1}).query_result.has_status());
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
  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({.query = &query,
                                             .normalizer = normalizer.get(),
                                             .redfish_interface = intf_.get(),
                                             .redpath_rules = {}}));
  ASSERT_THAT(qp, NotNull());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1});

  EXPECT_THAT(result.query_result.status().error_code(),
              Eq(ecclesia::ErrorCode::ERROR_INTERNAL));
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
          "Location": {
            "Oem": {
              "Google": {
                "EmbeddedLocationContext": "embedded/logical"
              }
            },
            "PartLocation": {
              "ServiceLabel": "chassis"
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
          "Oem": {
            "Google": {
              "Location": {
                "Oem": {
                  "Google": {
                    "Devpath": "/phys",
                    "EmbeddedLocationContext": "embedded/logical"
                  }
                }
              }
            }
          }
        })json");
                               req->Reply();
                             });

  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();
  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({.query = &query,
                                             .normalizer = normalizer.get(),
                                             .redfish_interface = intf_.get(),
                                             .redpath_rules = {}}));
  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  auto result = qp->Run({.variables = args1});
  EXPECT_FALSE(result.query_result.has_status());
  QueryResult expect_query_result = ParseTextProtoOrDie(R"pb(
    query_id: "EmbeddedResource"
    stats { payload_size: 198 num_cache_misses: 3 }
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
                  key: "_id_"
                  value {
                    identifier {
                      embedded_location_context: "embedded/logical"
                      redfish_location { service_label: "chassis" }
                    }
                  }
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
                  key: "_id_"
                  value {
                    identifier {
                      embedded_location_context: "embedded/logical"
                      local_devpath: "/phys"
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

  // Verify query result.
  EXPECT_THAT(expect_query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(result.query_result)));
}

TEST_F(QueryPlannerTestRunner, QueryPlannerPropertyCollection) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "Names"
        subquery {
          subquery_id: "chassis"
          redpath: "/Chassis[*]"
          properties {
            property: "Name"
            type: STRING
            collect_as: "test_resources"
            collect_as: "chassis_names"
          }
        }
        subquery {
          root_subquery_ids: "chassis"
          subquery_id: "sensors"
          redpath: "/Sensors[*]"
          properties {
            property: "Reading"
            type: DOUBLE
            collect_as: "test_resources"
          }
        }
      )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis", [&](ServerRequestInterface* req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Chassis/chassis",
          "Id": "chassis",
          "Name": "Indus Chassis",
          "Type": "Server",
          "Sensors": {
            "@odata.id": "/redfish/v1/Chassis/chassis/Sensors"
          },
          "Location": {
            "PartLocation": {
              "LocationType": "Slot",
              "ServiceLabel": "IO1"
            },
            "PartLocationContext": "PE4"
          }
        })json");
        req->Reply();
      });

  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis/Sensors/indus_cpu0_pwmon",
      [&](ServerRequestInterface* req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/indus_cpu0_pwmon",
          "@odata.type": "#Sensor.v1_8_0.Sensor",
          "Name": "CPU0",
          "ReadingType": "Power",
          "ReadingUnits": "W",
          "Reading": 30,
          "RelatedItem": [{"@odata.id": "/redfish/v1/Systems/system/Processors/cpu0"}]
        })json");
        req->Reply();
      });

  server_->AddHttpGetHandler("/redfish/v1/Systems/system/Processors/cpu0",
                             [&](ServerRequestInterface* req) {
                               SetContentType(req, "application/json");
                               req->OverwriteResponseHeader("OData-Version",
                                                            "4.0");
                               req->WriteResponseString(R"json({
            "@odata.id": "/redfish/v1/Systems/system/Processors/cpu0",
            "Name": "cpu0",
            "Location": {
                "PartLocation": {
                    "ServiceLabel": "CPU0",
                    "LocationType": "Slot"
                },
                "PartLocationContext": "ROOT"
            }
        })json");
                               req->Reply();
                             });

  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(query);
  ASSERT_THAT(result, IsOk());
  EXPECT_FALSE(result->query_result.has_status());

  CollectedProperties expected_collected_properties = ParseTextProtoOrDie(R"pb(
    properties {
      identifier {
        redfish_location { service_label: "IO1" part_location_context: "PE4" }
      }
      value { string_value: "Indus Chassis" }
    }
    properties {
      value { double_value: 16115 }
      sensor_identifier {
        name: "fan0"
        reading_type: "Rotational"
        reading_units: "RPM"
      }
    }
    properties {
      value { double_value: 16115 }
      sensor_identifier {
        name: "fan1"
        reading_type: "Rotational"
        reading_units: "RPM"
      }
    }
    properties {
      value { double_value: 16115 }
      sensor_identifier {
        name: "fan2"
        reading_type: "Rotational"
        reading_units: "RPM"
      }
    }
    properties {
      value { double_value: 16115 }
      sensor_identifier {
        name: "fan3"
        reading_type: "Rotational"
        reading_units: "RPM"
      }
    }
    properties {
      value { double_value: 16115 }
      sensor_identifier {
        name: "fan4"
        reading_type: "Rotational"
        reading_units: "RPM"
      }
    }
    properties {
      value { double_value: 16115 }
      sensor_identifier {
        name: "fan5"
        reading_type: "Rotational"
        reading_units: "RPM"
      }
    }
    properties {
      value { double_value: 16115 }
      sensor_identifier {
        name: "fan6"
        reading_type: "Rotational"
        reading_units: "RPM"
      }
    }
    properties {
      value { double_value: 28 }
      sensor_identifier {
        name: "indus_eat_temp"
        reading_type: "Temperature"
        reading_units: "Cel"
      }
    }
    properties {
      value { double_value: 35 }
      sensor_identifier {
        name: "indus_latm_temp"
        reading_type: "Temperature"
        reading_units: "Cel"
      }
    }
    properties {
      value { double_value: 30 }
      identifier {
        redfish_location { part_location_context: "ROOT" service_label: "CPU0" }
      }
      sensor_identifier {
        name: "CPU0"
        reading_type: "Power"
        reading_units: "W"
      }
    }
    properties {
      value { double_value: 30 }
      sensor_identifier {
        name: "CPU1"
        reading_type: "Power"
        reading_units: "W"
      }
    }
    properties {
      value { double_value: 60 }
      sensor_identifier {
        name: "CPU0"
        reading_type: "Temperature"
        reading_units: "Cel"
      }
    }
    properties {
      value { double_value: 60 }
      sensor_identifier {
        name: "CPU1"
        reading_type: "Temperature"
        reading_units: "Cel"
      }
    }
  )pb");
  CollectedProperties expected_chassis_names = ParseTextProtoOrDie(R"pb(
    properties {
      identifier {
        redfish_location { service_label: "IO1" part_location_context: "PE4" }
      }
      value { string_value: "Indus Chassis" }
    }
  )pb");
  EXPECT_THAT(
      result->query_result.collected_properties(),
      UnorderedElementsAre(
          Pair("test_resources", IgnoringRepeatedFieldOrdering(EqualsProto(
                                     expected_collected_properties))),
          Pair("chassis_names", IgnoringRepeatedFieldOrdering(
                                    EqualsProto(expected_chassis_names)))));
}

TEST_F(QueryPlannerTestRunner, QueryPlannerPropertyCollectionOfPCIeDevices) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "CollectFWStates"
        subquery {
          subquery_id: "FWState_LaneStatus"
          redpath: "/Systems[*]/PCIeDevices[*]"
          properties: { property: "Id" name: "Id" type: STRING }
          properties: {
            property: "PCIeInterface.Oem.Google.LaneStatus"
            name: "LaneStatus"
            type: STRING
            collect_as: "FWState_LaneStatus_PCIeDevices"
          }
          properties: {
            property: "Links.Chassis.@odata\\.id"
            name: "ChassisLink"
            type: STRING
          }
        }
      )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis", [&](ServerRequestInterface* req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Chassis/chassis",
          "Id": "Habanero16_1",
          "Name": "Habanero16_1",
          "Type": "Server",
          "Location": {
            "PartLocation": {
              "LocationType": "Slot",
              "ServiceLabel": "IO1"
            },
            "PartLocationContext": "PE4"
          }
        })json");
        req->Reply();
      });
  server_->AddHttpGetHandler(
      "/redfish/v1/Systems/system", [&](ServerRequestInterface* req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Systems/system",
          "Id": "system",
          "Name": "system",
          "PCIeDevices": {
            "@odata.id": "/redfish/v1/Systems/system/PCIeDevices"
          }
        })json");
        req->Reply();
      });
  server_->AddHttpGetHandler("/redfish/v1/Systems/system/PCIeDevices",
                             [&](ServerRequestInterface* req) {
                               SetContentType(req, "application/json");
                               req->OverwriteResponseHeader("OData-Version",
                                                            "4.0");
                               req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Systems/system/PCIeDevices",
          "Members": [
            {
              "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Habanero16_1_Link_0"
            }
          ]
        })json");
                               req->Reply();
                             });
  server_->AddHttpGetHandler(
      "/redfish/v1/Systems/system/PCIeDevices/Habanero16_1_Link_0",
      [&](ServerRequestInterface* req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Habanero16_1_Link_0",
          "@odata.type": "#PCIeDevice.v1_4_0.PCIeDevice",
          "Id": "Habanero16_1_Link_0",
          "Links": {
            "Chassis": {
              "@odata.id": "/redfish/v1/Chassis/chassis"
            }
          },

          "Name": "PCIe Device",
          "PCIeInterface": {
            "Oem": {
              "Google": {
                "LaneStatus": "JSON_STRING of Data"
              }
            }
          }
        })json");
        req->Reply();
      });

  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(query);
  ASSERT_THAT(result, IsOk());
  EXPECT_FALSE(result->query_result.has_status());

  CollectedProperties expected_collected_properties = ParseTextProtoOrDie(R"pb(
    properties {
      identifier {
        redfish_location { service_label: "IO1" part_location_context: "PE4" }
      }
      value { string_value: "JSON_STRING of Data" }
    }
  )pb");
  EXPECT_THAT(
      result->query_result.collected_properties(),
      UnorderedElementsAre(Pair("FWState_LaneStatus_PCIeDevices",
                                IgnoringRepeatedFieldOrdering(EqualsProto(
                                    expected_collected_properties)))));
}

TEST_F(QueryPlannerTestRunner,
       QueryPlannerPropertyCollectionOfPCIeDevicesMissingLink) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "CollectFWStates"
        subquery {
          subquery_id: "FWState_LaneStatus"
          redpath: "/Systems[*]/PCIeDevices[*]"
          properties: { property: "Id" name: "Id" type: STRING }
          properties: {
            property: "PCIeInterface.Oem.Google.LaneStatus"
            name: "LaneStatus"
            type: STRING
            collect_as: "FWState_LaneStatus_PCIeDevices"
          }
          properties: {
            property: "Links.Chassis.@odata\\.id"
            name: "ChassisLink"
            type: STRING
          }
        }
      )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");
  server_->AddHttpGetHandler(
      "/redfish/v1/Systems/system", [&](ServerRequestInterface* req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Systems/system",
          "Id": "system",
          "Name": "system",
          "PCIeDevices": {
            "@odata.id": "/redfish/v1/Systems/system/PCIeDevices"
          }
        })json");
        req->Reply();
      });
  server_->AddHttpGetHandler("/redfish/v1/Systems/system/PCIeDevices",
                             [&](ServerRequestInterface* req) {
                               SetContentType(req, "application/json");
                               req->OverwriteResponseHeader("OData-Version",
                                                            "4.0");
                               req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Systems/system/PCIeDevices",
          "Members": [
            {
              "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Habanero16_1_Link_0"
            }
          ]
        })json");
                               req->Reply();
                             });
  server_->AddHttpGetHandler(
      "/redfish/v1/Systems/system/PCIeDevices/Habanero16_1_Link_0",
      [&](ServerRequestInterface* req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Habanero16_1_Link_0",
          "@odata.type": "#PCIeDevice.v1_4_0.PCIeDevice",
          "Id": "Habanero16_1_Link_0",
          "Name": "PCIe Device",
          "PCIeInterface": {
            "Oem": {
              "Google": {
                "LaneStatus": "JSON_STRING of Data"
              }
            }
          }
        })json");
        req->Reply();
      });

  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(query);
  ASSERT_THAT(result, IsOk());
  EXPECT_FALSE(result->query_result.has_status());

  CollectedProperties expected_collected_properties = ParseTextProtoOrDie(R"pb(
    properties { value { string_value: "JSON_STRING of Data" } }
  )pb");
  EXPECT_THAT(
      result->query_result.collected_properties(),
      UnorderedElementsAre(Pair("FWState_LaneStatus_PCIeDevices",
                                IgnoringRepeatedFieldOrdering(EqualsProto(
                                    expected_collected_properties)))));
}

TEST_F(QueryPlannerTestRunner,
       QueryPlannerPropertyCollectionOfPCIeDevicesMissingChassisLink) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "CollectFWStates"
        subquery {
          subquery_id: "FWState_LaneStatus"
          redpath: "/Systems[*]/PCIeDevices[*]"
          properties: { property: "Id" name: "Id" type: STRING }
          properties: {
            property: "PCIeInterface.Oem.Google.LaneStatus"
            name: "LaneStatus"
            type: STRING
            collect_as: "FWState_LaneStatus_PCIeDevices"
          }
          properties: {
            property: "Links.Chassis.@odata\\.id"
            name: "ChassisLink"
            type: STRING
          }
        }
      )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");
  server_->AddHttpGetHandler(
      "/redfish/v1/Systems/system", [&](ServerRequestInterface* req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Systems/system",
          "Id": "system",
          "Name": "system",
          "PCIeDevices": {
            "@odata.id": "/redfish/v1/Systems/system/PCIeDevices"
          }
        })json");
        req->Reply();
      });
  server_->AddHttpGetHandler("/redfish/v1/Systems/system/PCIeDevices",
                             [&](ServerRequestInterface* req) {
                               SetContentType(req, "application/json");
                               req->OverwriteResponseHeader("OData-Version",
                                                            "4.0");
                               req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Systems/system/PCIeDevices",
          "Members": [
            {
              "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Habanero16_1_Link_0"
            }
          ]
        })json");
                               req->Reply();
                             });
  server_->AddHttpGetHandler(
      "/redfish/v1/Systems/system/PCIeDevices/Habanero16_1_Link_0",
      [&](ServerRequestInterface* req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Habanero16_1_Link_0",
          "@odata.type": "#PCIeDevice.v1_4_0.PCIeDevice",
          "Id": "Habanero16_1_Link_0",
          "Links": {
            "SomethingElse": {
              "@odata.id": "/redfish/v1/Chassis/chassis"
            }
          },

          "Name": "PCIe Device",
          "PCIeInterface": {
            "Oem": {
              "Google": {
                "LaneStatus": "JSON_STRING of Data"
              }
            }
          }
        })json");
        req->Reply();
      });

  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(query);
  ASSERT_THAT(result, IsOk());
  EXPECT_FALSE(result->query_result.has_status());

  CollectedProperties expected_collected_properties = ParseTextProtoOrDie(R"pb(
    properties { value { string_value: "JSON_STRING of Data" } }
  )pb");
  EXPECT_THAT(
      result->query_result.collected_properties(),
      UnorderedElementsAre(Pair("FWState_LaneStatus_PCIeDevices",
                                IgnoringRepeatedFieldOrdering(EqualsProto(
                                    expected_collected_properties)))));
}

TEST_F(QueryPlannerTestRunner,
       QueryPlannerPropertyCollectionOfPCIeDevicesMalformedChassisLinks) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "CollectFWStates"
        subquery {
          subquery_id: "FWState_LaneStatus"
          redpath: "/Systems[*]/PCIeDevices[*]"
          properties: { property: "Id" name: "Id" type: STRING }
          properties: {
            property: "PCIeInterface.Oem.Google.LaneStatus"
            name: "LaneStatus"
            type: STRING
            collect_as: "FWState_LaneStatus_PCIeDevices"
          }
          properties: {
            property: "Links.Chassis.@odata\\.id"
            name: "ChassisLink"
            type: STRING
          }
        }
      )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");
  server_->AddHttpGetHandler(
      "/redfish/v1/Systems/system", [&](ServerRequestInterface* req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Systems/system",
          "Id": "system",
          "Name": "system",
          "PCIeDevices": {
            "@odata.id": "/redfish/v1/Systems/system/PCIeDevices"
          }
        })json");
        req->Reply();
      });
  server_->AddHttpGetHandler("/redfish/v1/Systems/system/PCIeDevices",
                             [&](ServerRequestInterface* req) {
                               SetContentType(req, "application/json");
                               req->OverwriteResponseHeader("OData-Version",
                                                            "4.0");
                               req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Systems/system/PCIeDevices",
          "Members": [
            {
              "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Habanero16_1_Link_0"
            }
          ]
        })json");
                               req->Reply();
                             });
  server_->AddHttpGetHandler(
      "/redfish/v1/Systems/system/PCIeDevices/Habanero16_1_Link_0",
      [&](ServerRequestInterface* req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Habanero16_1_Link_0",
          "@odata.type": "#PCIeDevice.v1_4_0.PCIeDevice",
          "Id": "Habanero16_1_Link_0",
          "Links": {
            "Chassis": {
              "SomethingElse": "/redfish/v1/Chassis/chassis"
            }
          },

          "Name": "PCIe Device",
          "PCIeInterface": {
            "Oem": {
              "Google": {
                "LaneStatus": "JSON_STRING of Data"
              }
            }
          }
        })json");
        req->Reply();
      });

  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(query);
  ASSERT_THAT(result, IsOk());
  EXPECT_FALSE(result->query_result.has_status());

  CollectedProperties expected_collected_properties = ParseTextProtoOrDie(R"pb(
    properties { value { string_value: "JSON_STRING of Data" } }
  )pb");
  EXPECT_THAT(
      result->query_result.collected_properties(),
      UnorderedElementsAre(Pair("FWState_LaneStatus_PCIeDevices",
                                IgnoringRepeatedFieldOrdering(EqualsProto(
                                    expected_collected_properties)))));
}

TEST_F(QueryPlannerTestRunner,
       QueryPlannerPropertyCollectionOfPCIeDevicesMissingChassisLocation) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "CollectFWStates"
        subquery {
          subquery_id: "FWState_LaneStatus"
          redpath: "/Systems[*]/PCIeDevices[*]"
          properties: { property: "Id" name: "Id" type: STRING }
          properties: {
            property: "PCIeInterface.Oem.Google.LaneStatus"
            name: "LaneStatus"
            type: STRING
            collect_as: "FWState_LaneStatus_PCIeDevices"
          }
          properties: {
            property: "Links.Chassis.@odata\\.id"
            name: "ChassisLink"
            type: STRING
          }
        }
      )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis", [&](ServerRequestInterface* req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Chassis/chassis",
          "Id": "Habanero16_1",
          "Name": "Habanero16_1",
          "Type": "Server",
        })json");
        req->Reply();
      });
  server_->AddHttpGetHandler(
      "/redfish/v1/Systems/system", [&](ServerRequestInterface* req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Systems/system",
          "Id": "system",
          "Name": "system",
          "PCIeDevices": {
            "@odata.id": "/redfish/v1/Systems/system/PCIeDevices"
          }
        })json");
        req->Reply();
      });
  server_->AddHttpGetHandler("/redfish/v1/Systems/system/PCIeDevices",
                             [&](ServerRequestInterface* req) {
                               SetContentType(req, "application/json");
                               req->OverwriteResponseHeader("OData-Version",
                                                            "4.0");
                               req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Systems/system/PCIeDevices",
          "Members": [
            {
              "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Habanero16_1_Link_0"
            }
          ]
        })json");
                               req->Reply();
                             });
  server_->AddHttpGetHandler(
      "/redfish/v1/Systems/system/PCIeDevices/Habanero16_1_Link_0",
      [&](ServerRequestInterface* req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Habanero16_1_Link_0",
          "@odata.type": "#PCIeDevice.v1_4_0.PCIeDevice",
          "Id": "Habanero16_1_Link_0",
          "Links": {
            "Chassis": {
              "@odata.id": "/redfish/v1/Chassis/chassis"
            }
          },

          "Name": "PCIe Device",
          "PCIeInterface": {
            "Oem": {
              "Google": {
                "LaneStatus": "JSON_STRING of Data"
              }
            }
          }
        })json");
        req->Reply();
      });

  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(query);
  ASSERT_THAT(result, IsOk());
  EXPECT_FALSE(result->query_result.has_status());

  CollectedProperties expected_collected_properties = ParseTextProtoOrDie(R"pb(
    properties { value { string_value: "JSON_STRING of Data" } }
  )pb");
  EXPECT_THAT(
      result->query_result.collected_properties(),
      UnorderedElementsAre(Pair("FWState_LaneStatus_PCIeDevices",
                                IgnoringRepeatedFieldOrdering(EqualsProto(
                                    expected_collected_properties)))));
}

TEST_F(QueryPlannerTestRunner,
       QueryPlannerPropertyCollectionWithMissingProperties) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "Names"
        subquery {
          subquery_id: "chassis"
          redpath: "/Chassis[*]"
          properties {
            property: "RandomProperty"
            type: STRING
            collect_as: "test_resources"
          }
          normalized_properties {
            property: "RandomProperty"
            collect_as: "chassis_names"
          }
        }
      )pb");
  SetTestParams("indus_hmb_shim/mockup.shar");
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis", [&](ServerRequestInterface* req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Chassis/chassis",
          "Id": "chassis",
          "Name": "Indus Chassis",
          "Type": "Server",
          "Sensors": {
            "@odata.id": "/redfish/v1/Chassis/chassis/Sensors"
          },
          "Location": {
            "PartLocation": {
              "LocationType": "Slot",
              "ServiceLabel": "IO1"
            },
            "PartLocationContext": "PE4"
          }
        })json");
        req->Reply();
      });
  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(query);
  ASSERT_THAT(result, IsOk());
  EXPECT_FALSE(result->query_result.has_status());
  EXPECT_THAT(result->query_result.collected_properties(), IsEmpty());
}

TEST_F(QueryPlannerTestRunner,
       QueryPlannerPropertyCollectionWithNormalizedProperties) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "Names"
        subquery {
          subquery_id: "chassis"
          redpath: "/Chassis[*]"
          properties {
            property: "Name"
            type: STRING
            collect_as: "test_resources"
          }
          properties { property: "Id" type: STRING }
          normalized_properties { property: "Id" collect_as: "chassis_ids" }
        }
      )pb");
  SetTestParams("indus_hmb_shim/mockup.shar");
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis", [&](ServerRequestInterface* req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/Chassis/chassis",
          "Id": "chassis",
          "Name": "Indus Chassis",
          "Type": "Server",
          "Sensors": {
            "@odata.id": "/redfish/v1/Chassis/chassis/Sensors"
          },
          "Location": {
            "PartLocation": {
              "LocationType": "Slot",
              "ServiceLabel": "IO1"
            },
            "PartLocationContext": "PE4"
          }
        })json");
        req->Reply();
      });
  CollectedProperties expected_chassis_names = ParseTextProtoOrDie(R"pb(
    properties {
      identifier {
        redfish_location { service_label: "IO1" part_location_context: "PE4" }
      }
      value { string_value: "Indus Chassis" }
    }
  )pb");
  CollectedProperties expected_chassis_ids = ParseTextProtoOrDie(R"pb(
    properties {
      identifier {
        redfish_location { service_label: "IO1" part_location_context: "PE4" }
      }
      value { string_value: "chassis" }
    }
  )pb");

  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(query);
  ASSERT_THAT(result, IsOk());
  EXPECT_FALSE(result->query_result.has_status());
  EXPECT_THAT(
      result->query_result.collected_properties(),
      UnorderedElementsAre(
          Pair("test_resources", IgnoringRepeatedFieldOrdering(
                                     EqualsProto(expected_chassis_names))),
          Pair("chassis_ids", IgnoringRepeatedFieldOrdering(
                                  EqualsProto(expected_chassis_ids)))));
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

  auto transport = std::make_unique<MetricalRedfishTransport>(
      server_->RedfishClientTransport(), Clock::RealClock());
  auto cache = std::make_unique<NullCache>(transport.get());
  auto intf = NewHttpInterface(std::move(transport), std::move(cache),
                               RedfishInterface::kTrusted);
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::Time test_time = absl::UnixEpoch() + absl::Seconds(50);
  FakeClock clock(test_time);

  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({.query = &query,
                                             .normalizer = normalizer.get(),
                                             .redfish_interface = intf.get(),
                                             .redpath_rules = {},
                                             .clock = &clock}));

  ASSERT_THAT(qp, NotNull());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1});

  EXPECT_FALSE(result.query_result.has_status());
  ASSERT_TRUE(result.query_result.has_stats());
  ASSERT_TRUE(result.query_result.stats().has_redfish_metrics());
  EXPECT_THAT(result.query_result.stats().num_requests(), 40);
}

TEST_F(QueryPlannerTestRunner,
       QueryPlannerExecutesCacheMetricsNullCacheCorrectly) {
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

  SetTestParams("indus_hmb_shim/mockup.shar");
  auto transport = std::make_unique<MetricalRedfishTransport>(
      server_->RedfishClientTransport(), Clock::RealClock());
  auto cache = std::make_unique<NullCache>(transport.get());
  auto intf = NewHttpInterface(std::move(transport), std::move(cache),
                               RedfishInterface::kTrusted);
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({.query = &query,
                                             .normalizer = normalizer.get(),
                                             .redfish_interface = intf.get(),
                                             .redpath_rules = {}}));

  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result =
      qp->Run({.variables = args1, .redfish_interface = intf.get()});

  EXPECT_FALSE(result.query_result.has_status());
  ASSERT_TRUE(result.query_result.has_stats());
  ASSERT_TRUE(result.query_result.stats().has_redfish_metrics());
  EXPECT_THAT(result.query_result.stats().num_cache_hits(), 0);
  EXPECT_THAT(result.query_result.stats().num_cache_misses(), 56);
}

TEST_F(QueryPlannerTestRunner,
       QueryPlannerExecutesCacheMetricsTimeBasedCacheCorrectly) {
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

  SetTestParams("indus_hmb_shim/mockup.shar");

  auto transport = std::make_unique<MetricalRedfishTransport>(
      server_->RedfishClientTransport(), Clock::RealClock());

  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  std::unique_ptr<RedfishCachedGetterInterface> cache =
      TimeBasedCache::Create(transport.get(), absl::InfiniteDuration());
  auto intf = NewHttpInterface(std::move(transport), std::move(cache),
                               RedfishInterface::kTrusted);

  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({.query = &query,
                                             .normalizer = normalizer.get(),
                                             .redfish_interface = intf.get(),
                                             .redpath_rules = {}}));

  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1});

  EXPECT_FALSE(result.query_result.has_status());
  ASSERT_TRUE(result.query_result.has_stats());
  ASSERT_TRUE(result.query_result.stats().has_redfish_metrics());

  QueryExecutionResult result_repeat = qp->Run({.variables = args1});

  EXPECT_FALSE(result_repeat.query_result.has_status());
  ASSERT_TRUE(result_repeat.query_result.has_stats());
  EXPECT_THAT(result_repeat.query_result.stats().num_cache_hits(), 56);
  EXPECT_THAT(result_repeat.query_result.stats().num_cache_misses(), 0);
}

struct GetChildUriTestCase {
  std::string test_name;
  std::string redpath_query;
  std::string parent_resource_uri;
  std::string child_resource_uri;
  std::string parent_json_response;
  std::string expected_error_message;
  HTTPStatusCode child_response_status_code;
};

class GetChildUriTest : public QueryPlannerTestRunner,
    public WithParamInterface<GetChildUriTestCase> {
 public:
  static std::string GetTestName(
      const TestParamInfo<GetChildUriTestCase> &info) {
    return info.param.test_name;
  }
};

TEST_P(GetChildUriTest, GetChildUriReturnsCorrectErrorMessage) {
  const GetChildUriTestCase &test_case = GetParam();

  DelliciusQuery query = ParseTextProtoOrDie(
    R"pb(
      query_id: "ChassisTest"
      subquery {
        subquery_id: "Chassis"
        properties { property: "Id" type: STRING }
        properties { property: "Name" type: STRING }
      }
    )pb");
  query.mutable_subquery(0)->set_redpath(test_case.redpath_query);
  SetTestParams("indus_hmb_shim/mockup.shar");
  server_->AddHttpGetHandler(test_case.parent_resource_uri,
    [&](ServerRequestInterface *req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(test_case.parent_json_response);
        req->Reply();
      });
  server_->AddHttpGetHandler(test_case.child_resource_uri,
    [&](ServerRequestInterface *req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({})json");
        req->ReplyWithStatus(test_case.child_response_status_code);
      });
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();
  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({
                            .query = &query,
                            .normalizer = normalizer.get(),
                            .redfish_interface = intf_.get(),
                            .redpath_rules = {}
                          }));
  ASSERT_THAT(qp, NotNull());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1});

  ASSERT_THAT(result.query_result.status().errors().size(), Eq(1));
  EXPECT_THAT(
      result.query_result.status().errors().at(0),
      HasSubstr(test_case.expected_error_message));
}

INSTANTIATE_TEST_SUITE_P(TestGetChildUriErrorMessages, GetChildUriTest,
  Values(
    GetChildUriTestCase{
      .test_name = "PredicateShowsParentUri",
      .redpath_query = "/Chassis[*]",
      .parent_resource_uri = "/redfish/v1/Chassis",
      .child_resource_uri = "/redfish/v1/Chassis/IronFist_1",
      .parent_json_response = R"json({
        "@odata.id": "/redfish/v1/Chassis",
        "Members": [
          {
            "@odata.id": "/redfish/v1/Chassis/IronFist_1"
          },
          {
            "@odata.id": "/redfish/v1/Chassis/IronFist_2"
          }
        ]
      })json",
      .expected_error_message = "Querying predicate [*] from Redpath: "
        "/Chassis[*] resulted in error: At resource URI: "
        "/redfish/v1/Chassis/IronFist_1: Service Unavailable",
      .child_response_status_code = HTTPStatusCode::SERVICE_UNAV
    },
    GetChildUriTestCase{
      .test_name = "NodenameShowsParentUri",
      .redpath_query = "/Chassis/Node1",
      .parent_resource_uri = "/redfish/v1/Chassis",
      .child_resource_uri = "/redfish/v1/Chassis/Node1",
      .parent_json_response = R"json({
        "@odata.id": "/redfish/v1/Chassis",
        "Node1": {
          "@odata.id": "/redfish/v1/Chassis/Node1"
        }
      })json",
      .expected_error_message = "Querying node name Node1 from Redpath: "
        "/Chassis/Node1 resulted in error: At resource URI: "
        "/redfish/v1/Chassis/Node1: Service Unavailable",
      .child_response_status_code = HTTPStatusCode::SERVICE_UNAV
    },
    GetChildUriTestCase{
      .test_name = "PredicateParentUriNotAvailable",
      .redpath_query = "/Chassis[*]",
      .parent_resource_uri = "/redfish/v1/Chassis",
      .child_resource_uri = "/redfish/v1/Chassis/IronFist_1",
      .parent_json_response = R"json({
        "Members": [
          {
            "@odata.id": "/redfish/v1/Chassis/IronFist_1"
          }
        ]
      })json",
      .expected_error_message = "Querying predicate [*] from Redpath: "
        "/Chassis[*] resulted in error: At resource URI: "
        "(Parent URI Not Available from RedfishObject: No @odata.id): "
        "Service Unavailable",
      .child_response_status_code = HTTPStatusCode::SERVICE_UNAV
    },
    GetChildUriTestCase{
      .test_name = "NodenameParentUriNotAvailable",
      .redpath_query = "/Chassis/Node1",
      .parent_resource_uri = "/redfish/v1/Chassis",
      .child_resource_uri = "/redfish/v1/Chassis/Node1",
      .parent_json_response = R"json({
        "Node1": {
          "@odata.id": "/redfish/v1/Chassis/Node1"
        }
      })json",
      .expected_error_message = "Querying node name Node1 from Redpath: "
        "/Chassis/Node1 resulted in error: At resource URI: "
        "(Parent URI Not Available from RedfishObject: No @odata.id): "
        "Service Unavailable",
      .child_response_status_code = HTTPStatusCode::SERVICE_UNAV
    },
    GetChildUriTestCase{
      .test_name = "MalformedPredicateExpression",
      .redpath_query = "/Chassis[(!]",
      .parent_resource_uri = "/redfish/v1/Chassis",
      .child_resource_uri = "/redfish/v1/Chassis/IronFist_1",
      .parent_json_response = R"json({
        "@odata.id": "/redfish/v1/Chassis",
        "Members": [
          {
            "@odata.id": "/redfish/v1/Chassis/IronFist_1"
          }
        ]
      })json",
      .expected_error_message = "Querying predicate [(!] from Redpath: "
        "/Chassis[*] resulted in error: At resource URI: "
        "/redfish/v1/Chassis/IronFist_1: Invalid predicate with mismatched "
        "parenthesis: (!",
      .child_response_status_code = HTTPStatusCode::OK
    }
  ), GetChildUriTest::GetTestName
);

TEST(UriTestErrorMessages, CorrectUris) {
  auto mock_redfish_object = std::make_unique<MockRedfishObject>();
  EXPECT_CALL(*mock_redfish_object, GetContentAsJson()).Times(2)
      .WillRepeatedly(Return(nlohmann::json::parse(R"json({
        "@odata.id": "/redfish/v1/",
        "Foo": {
          "@odata.id": "/redfish/v1/foo"
        },
        "Members": [
          {
            "@odata.id": "/redfish/v1/bar"
          }
        ]
      })json")));
  auto mock_redfish_iterable = std::make_unique<MockRedfishIterable>();
  EXPECT_CALL(*mock_redfish_iterable, Size()).WillOnce(Return(1));

  RedfishResponse redfish_response;
  redfish_response.redfish_object = std::move(mock_redfish_object);
  redfish_response.redfish_iterable = std::move(mock_redfish_iterable);
  QueryResult result;
  QueryVariables query_variables;

  QueryExecutionContext execution_context(
      &result, {}, &query_variables, RedPathPrefixTracker(), nullptr,
      std::move(redfish_response));

  absl::StatusOr<std::string> node_uri =
      GetChildUriFromNode(execution_context, "Foo");
  absl::StatusOr<std::string> iterable_uri =
      GetChildUriFromIterable(execution_context, 0);

  ASSERT_TRUE(node_uri.ok());
  EXPECT_EQ(*node_uri, "/redfish/v1/foo");
  ASSERT_TRUE(iterable_uri.ok());
  EXPECT_EQ(*iterable_uri, "/redfish/v1/bar");
}

TEST(UriTestErrorMessages, NullRedfishObject) {
  QueryResult result;
  QueryVariables query_variables;
  QueryExecutionContext execution_context(
      &result, {}, &query_variables, RedPathPrefixTracker(), nullptr);

  absl::StatusOr<std::string> node_uri =
      GetChildUriFromNode(execution_context, "foo");
  absl::StatusOr<std::string> iterable_uri =
      GetChildUriFromIterable(execution_context, 0xf00);

  ASSERT_THAT(node_uri, IsStatusInternal());
  EXPECT_THAT(node_uri.status().message(), "(RedfishObject is null)");
  ASSERT_THAT(iterable_uri, IsStatusInternal());
  EXPECT_THAT(iterable_uri.status().message(), "(RedfishObject is null)");
}

TEST(UriTestErrorMessages, ComplexTypeResource) {
  RedfishResponse redfish_response;
  redfish_response.redfish_iterable = std::make_unique<MockRedfishIterable>();
  QueryResult result;
  QueryVariables query_variables;

  QueryExecutionContext execution_context(
      &result, {}, &query_variables, RedPathPrefixTracker(), nullptr,
      std::move(redfish_response));
  absl::StatusOr<std::string> iterable_uri =
      GetChildUriFromIterable(execution_context, 0xf00);

  ASSERT_THAT(iterable_uri, IsStatusInternal());
  EXPECT_THAT(iterable_uri.status().message(),
              "(Queried resource is a ComplexType; no URI)");
}

struct GetChildUriFromNodeTestCase {
  std::string test_name;
  nlohmann::json json_response;
  internal_status::IsStatusPolyMatcher status_poly_matcher;
  std::string expected_error_message;
};

class ChildUriFromNodeTest : public TestWithParam<GetChildUriFromNodeTestCase> {
 public:
  static std::string GetTestName(
      const TestParamInfo<GetChildUriFromNodeTestCase>& test_info) {
    return test_info.param.test_name;
  }
};

TEST_P(ChildUriFromNodeTest, GetChildUriFromNode) {
  const GetChildUriFromNodeTestCase& param = GetParam();

  auto mock_redfish_object = std::make_unique<MockRedfishObject>();
  EXPECT_CALL(*mock_redfish_object, GetContentAsJson())
      .WillOnce(Return(param.json_response));

  RedfishResponse redfish_response;
  redfish_response.redfish_object = std::move(mock_redfish_object);
  QueryResult result;
  QueryVariables query_variables;

  QueryExecutionContext execution_context(
      &result, {}, &query_variables, RedPathPrefixTracker(), nullptr,
      std::move(redfish_response));
  absl::StatusOr<std::string> node_uri =
      GetChildUriFromNode(execution_context, "foo");

  ASSERT_THAT(node_uri, param.status_poly_matcher);
  EXPECT_THAT(node_uri.status().message(), param.expected_error_message);
}

INSTANTIATE_TEST_SUITE_P(TestGetChildUriErrorMessages, ChildUriFromNodeTest,
  Values(
    GetChildUriFromNodeTestCase{
      .test_name = "DiscardedResponse",
      .json_response = nlohmann::json::value_t::discarded,
      .status_poly_matcher = IsStatusInternal(),
      .expected_error_message = "(Cannot get content as Json from Parent "
        "RedfishObject)"
    },
    GetChildUriFromNodeTestCase{
      .test_name = "MissingNavigationalProperty",
      .json_response = nlohmann::json::parse(R"json({
      })json"),
      .status_poly_matcher = IsStatusInternal(),
      .expected_error_message = "(Parent URI Not Available from RedfishObject: "
        "No @odata.id)"
    },
    GetChildUriFromNodeTestCase{
      .test_name = "MissingNodeName",
      .json_response = nlohmann::json::parse(R"json({
        "@odata.id": "/redfish/v1/"
      })json"),
      .status_poly_matcher = IsStatusInternal(),
      .expected_error_message = "(RedfishObject does not contain: foo at URI: "
        "/redfish/v1/)"
    },
    GetChildUriFromNodeTestCase{
      .test_name = "MissingNavigationPropertyInNode",
      .json_response = nlohmann::json::parse(R"json({
        "@odata.id": "/redfish/v1/",
        "foo": {}
      })json"),
      .status_poly_matcher = IsStatusInternal(),
      .expected_error_message = "(@odata.id missing from: foo at URI: "
        "/redfish/v1/)"
    },
    GetChildUriFromNodeTestCase{
      .test_name = "NoErrors",
      .json_response = nlohmann::json::parse(R"json({
        "@odata.id": "/redfish/v1/",
        "foo": {
          "@odata.id": "/redfish/v1/foo"
        }
      })json"),
      .status_poly_matcher = IsOk(),
      .expected_error_message = ""
    }
  ), ChildUriFromNodeTest::GetTestName
);

struct GetChildUriFromIterableTestCase {
  std::string test_name;
  nlohmann::json json_response;
  int index;
  internal_status::IsStatusPolyMatcher status_poly_matcher;
  std::string expected_error_message;
};

class ChildUriFromIterableTest
    : public TestWithParam<GetChildUriFromIterableTestCase> {
 public:
  static std::string GetTestName(
      const TestParamInfo<GetChildUriFromIterableTestCase>& test_info) {
    return test_info.param.test_name;
  }
};

TEST_P(ChildUriFromIterableTest, GetChildUriFromIterable) {
  const GetChildUriFromIterableTestCase& param = GetParam();

  auto mock_redfish_object = std::make_unique<MockRedfishObject>();
  EXPECT_CALL(*mock_redfish_object, GetContentAsJson())
      .WillOnce(Return(param.json_response));
  auto mock_redfish_iterable = std::make_unique<MockRedfishIterable>();
  EXPECT_CALL(*mock_redfish_iterable, Size()).Times(AnyNumber())
      .WillOnce(Return(1));

  RedfishResponse redfish_response;
  redfish_response.redfish_object = std::move(mock_redfish_object);
  redfish_response.redfish_iterable = std::move(mock_redfish_iterable);
  QueryResult result;
  QueryVariables query_variables;

  QueryExecutionContext execution_context(
      &result, {}, &query_variables, RedPathPrefixTracker(), nullptr,
      std::move(redfish_response));
  absl::StatusOr<std::string> iterable_uri =
      GetChildUriFromIterable(execution_context, param.index);

  ASSERT_THAT(iterable_uri, param.status_poly_matcher);
  EXPECT_THAT(iterable_uri.status().message(), param.expected_error_message);
}

INSTANTIATE_TEST_SUITE_P(TestGetChildUriErrorMessages, ChildUriFromIterableTest,
  Values(
    GetChildUriFromIterableTestCase{
      .test_name = "DiscardedResponse",
      .json_response = nlohmann::json::value_t::discarded,
      .index = 0,
      .status_poly_matcher = IsStatusInternal(),
      .expected_error_message = "(Cannot get content as Json from Parent "
        "RedfishObject)"
    },
    GetChildUriFromIterableTestCase{
      .test_name = "MissingNavigationalProperty",
      .json_response = nlohmann::json::parse(R"json({
      })json"),
      .index = 0,
      .status_poly_matcher = IsStatusInternal(),
      .expected_error_message = "(Parent URI Not Available from RedfishObject: "
        "No @odata.id)"
    },
    GetChildUriFromIterableTestCase{
      .test_name = "MissingMembersProperty",
      .json_response = nlohmann::json::parse(R"json({
        "@odata.id": "/redfish/v1/"
      })json"),
      .index = 0,
      .status_poly_matcher = IsStatusInternal(),
      .expected_error_message = "(RedfishObject is not an iterable: "
        "/redfish/v1/ does not have Members[])"
    },
    GetChildUriFromIterableTestCase{
      .test_name = "IndexOutOfRangeUnderflow",
      .json_response = nlohmann::json::parse(R"json({
        "@odata.id": "/redfish/v1/",
        "Members": [
          {
            "@odata.id": "/redfish/v1/foo"
          }
        ]
      })json"),
      .index = -1,
      .status_poly_matcher = IsStatusInternal(),
      .expected_error_message = "(Index -1 out of bounds for RedfishIterable "
        "at URI: /redfish/v1/)"
    },
    GetChildUriFromIterableTestCase{
      .test_name = "IndexOutOfRangeOverflow",
      .json_response = nlohmann::json::parse(R"json({
        "@odata.id": "/redfish/v1/",
        "Members": [
          {
            "@odata.id": "/redfish/v1/foo"
          }
        ]
      })json"),
      .index = 1,
      .status_poly_matcher = IsStatusInternal(),
      .expected_error_message = "(Index 1 out of bounds for RedfishIterable "
        "at URI: /redfish/v1/)"
    },
    GetChildUriFromIterableTestCase{
      .test_name = "MembersElementMissingNavigationalProperty",
      .json_response = nlohmann::json::parse(R"json({
        "@odata.id": "/redfish/v1/",
        "Members": [
          {}
        ]
      })json"),
      .index = 0,
      .status_poly_matcher = IsStatusInternal(),
      .expected_error_message = "(@odata.id Not Available in Members[] at "
        "index 0 at URI: /redfish/v1/)"
    },
    GetChildUriFromIterableTestCase{
      .test_name = "NoErrors",
      .json_response = nlohmann::json::parse(R"json({
        "@odata.id": "/redfish/v1/",
        "Members": [
          {
            "@odata.id": "/redfish/v1/foo"
          }
        ]
      })json"),
      .index = 0,
      .status_poly_matcher = IsOk(),
      .expected_error_message = ""
    }
  ), ChildUriFromIterableTest::GetTestName
);

TEST_F(QueryPlannerTestRunner, CheckQueryPlannerPopulatesStatus) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "ChassisTest"
        subquery {
          subquery_id: "Chassis"
          redpath: "/Chassis[*]"
          properties { property: "Id" type: STRING }
          properties { property: "Name" type: STRING }
        }
      )pb");
  SetTestParams("indus_hmb_shim/mockup.shar");
  server_->AddHttpGetHandlerWithStatus("/redfish/v1/Chassis", "",
                                       HTTPStatusCode::SERVICE_UNAV);
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();
  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({.query = &query,
                                             .normalizer = normalizer.get(),
                                             .redfish_interface = intf_.get(),
                                             .redpath_rules = {}}));
  ASSERT_THAT(qp, NotNull());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1});

  EXPECT_THAT(result.query_result.status().error_code(),
              Eq(ecclesia::ErrorCode::ERROR_UNAVAILABLE));
  EXPECT_THAT(
      result.query_result.status().errors().at(0),
      HasSubstr(
          "Querying node name Chassis from Redpath: /Chassis resulted in "
          "error: At resource URI: /redfish/v1/Chassis: Service Unavailable"));
}

TEST_F(QueryPlannerTestRunner, TestNestedNodeNameInQueryProperty) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(query_id: "ManagerCollector"
           subquery {
             subquery_id: "GetManagersIdAndResetType"
             redpath: "/Managers[*]"
             properties { property: "@odata\\.id" type: STRING }
             properties {
               property: "Actions.#Manager\\.Reset.ResetType@Redfish\\.AllowableValues[0]"
               type: STRING
             }
           })pb");

  QueryResult expected_query_result = ParseTextProtoOrDie(R"pb(
    query_id: "ManagerCollector"
    stats { payload_size: 236 num_cache_misses: 4 }
    data {
      fields {
        key: "GetManagersIdAndResetType"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "@odata.id"
                  value { string_value: "/redfish/v1/Managers/ec" }
                }
                fields {
                  key: "Actions.#Manager.Reset.ResetType@Redfish.AllowableValues[0]"
                  value { string_value: "PowerCycle" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "@odata.id"
                  value { string_value: "/redfish/v1/Managers/ecclesia_agent" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  SetTestParams("indus_hmb_cn/mockup.shar");
  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(query);
  ASSERT_THAT(result, IsOk());
  EXPECT_FALSE(result->query_result.has_status());
  EXPECT_THAT(result->query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(expected_query_result)));
}

TEST_F(QueryPlannerTestRunner,
       CheckQueryPlannerInitFailsWithInvalidSubqueryLinks) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(query_id: "ServiceRoot"
           subquery {
             subquery_id: "ChassisLinked1"
             root_subquery_ids: "ChassisLinked2"
             redpath: "/Chassis[*]"
             properties {
               name: "serial_number"
               property: "SerialNumber"
               type: STRING
             }
             properties {
               name: "part_number"
               property: "PartNumber"
               type: STRING
             }
           }
           subquery {
             subquery_id: "ChassisLinked2"
             root_subquery_ids: "ChassisLinked1"
             redpath: "/Chassis[*]"
             properties {
               name: "serial_number"
               property: "SerialNumber"
               type: STRING
             }
             properties {
               name: "part_number"
               property: "PartNumber"
               type: STRING
             }
           })pb");

  SetTestParams("indus_hmb_cn/mockup.shar");
  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(query);
  EXPECT_THAT(result, IsStatusInvalidArgument());
  EXPECT_EQ(result.status().message(), "No root subqueries found in the query");
}

TEST_F(QueryPlannerTestRunner,
       CheckQueryPlannerInitFailsWithMalforedRedPathsInSubqueries) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(query_id: "TestMalformedQuery"
           # Malformed NodeName Expressions
           subquery {
             subquery_id: "MalformedNodeName"
             redpath: "/Chass~is/Sensors[1]"
             properties { property: "@odata.id" type: STRING }
           }
           # Malformed Predicate Expressions
           subquery {
             subquery_id: "MalformedPredicate"
             redpath: "/Chassis[/Sensors[*]"
             properties { property: "@odata.id" type: STRING }
           })pb");

  SetTestParams("indus_hmb_cn/mockup.shar");
  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(query);
  ASSERT_THAT(result.status(), IsStatusInvalidArgument());
}

TEST(QueryPlannerTest, CheckQueryPlannerStopsQueryingOnTransportError) {
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

  // Set up context node for dellicius query.
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  // Instantiate a passthrough normalizer.
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  // Create metrical transport and issue queries.
  std::unique_ptr<RedfishTransport> base_transport =
      std::make_unique<NullTransport>();
  auto transport = std::make_unique<MetricalRedfishTransport>(
      std::move(base_transport), Clock::RealClock());
  auto cache = std::make_unique<NullCache>(transport.get());
  auto intf = NewHttpInterface(std::move(transport), std::move(cache),
                               RedfishInterface::kTrusted);

  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({.query = &query,
                                             .normalizer = normalizer.get(),
                                             .redfish_interface = intf.get(),
                                             .redpath_rules = {}}));
  ASSERT_THAT(qp, NotNull());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  auto result = qp->Run({.variables = args1});
  EXPECT_THAT(result.query_result.status().error_code(),
              Eq(ecclesia::ErrorCode::ERROR_SERVICE_ROOT_UNREACHABLE));
  EXPECT_THAT(
      result.query_result.status().errors().at(0),
      HasSubstr(
          "Attempting to reach service root /redfish/v1 resulted in error: "
          "RedfishVariant object has status: NullTransport"));

  // Redfish Metrics should indicate 1 failed GET request to service root.
  const RedfishMetrics &metrics = result.query_result.stats().redfish_metrics();
  EXPECT_EQ(metrics.uri_to_metrics_map_size(), 1);
  EXPECT_TRUE(metrics.uri_to_metrics_map().contains("/redfish/v1"));
  EXPECT_EQ(metrics.uri_to_metrics_map()
                .at("/redfish/v1")
                .request_type_to_metadata_failures_size(),
            1);
  EXPECT_TRUE(metrics.uri_to_metrics_map()
                  .at("/redfish/v1")
                  .request_type_to_metadata_failures()
                  .contains("GET"));
  EXPECT_EQ(metrics.uri_to_metrics_map()
                .at("/redfish/v1")
                .request_type_to_metadata_size(),
            0);
}

TEST_F(QueryPlannerTestRunner, CheckSubqueryErrorsPopulatedCollectionResource) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(query_id: "ManagerCollector"
           subquery {
             subquery_id: "GetManagersIdAndResetType"
             redpath: "/Managers[*]"
             properties { property: "@odata\\.id" type: STRING }
             properties {
               property: "Actions.#Manager\\.Reset.ResetType@Redfish\\.AllowableValues[0]"
               type: STRING
             }
           })pb");
  SetTestParams("indus_hmb_cn/mockup.shar");

  auto result_json = nlohmann::json::parse(R"json({
  "error": {
    "code": "Base.1.0.GeneralError",
    "message": "A general error has occurred.  See Resolution for information on how to resolve the error, or @Message.ExtendedInfo if Resolution is not provided."
  }
})json");

  bool called = false;
  server_->AddHttpGetHandler(
      "/redfish/v1/Managers/ecclesia_agent", [&](ServerRequestInterface *req) {
        called = true;
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(result_json.dump());
        req->ReplyWithStatus(HTTPStatusCode::UNAUTHORIZED);
      });
  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(query);

  // Ensure that after encountering a NOT_FOUND error, the query status is
  // OK and no error summaries are populated.
  ASSERT_THAT(result, IsOk());
  EXPECT_TRUE(called);
  EXPECT_EQ(result->query_result.status().error_code(),
            ErrorCode::ERROR_UNAUTHENTICATED);
  EXPECT_THAT(
      result->query_result.status().errors().at(0),
      HasSubstr(
          "Querying predicate [*] from Redpath: /Managers[*] resulted in "
          "error: At resource URI: /redfish/v1/Managers/ecclesia_agent: "
          "Unauthorized"));
}

TEST_F(QueryPlannerTestRunner, CheckUnresolvedNodeIsNotAnError) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(query_id: "ManagerCollector"
           subquery {
             subquery_id: "GetManagersIdAndResetType"
             redpath: "/Managers[*]"
             properties { property: "@odata\\.id" type: STRING }
             properties {
               property: "Actions.#Manager\\.Reset.ResetType@Redfish\\.AllowableValues[0]"
               type: STRING
             }
           })pb");
  SetTestParams("indus_hmb_cn/mockup.shar");

  auto result_json = nlohmann::json::parse(R"json({
  "error": {
    "code": "Base.1.0.GeneralError",
    "message": "A general error has occurred.  See Resolution for information on how to resolve the error, or @Message.ExtendedInfo if Resolution is not provided."
  }
})json");

  bool called = false;
  server_->AddHttpGetHandler(
      "/redfish/v1/Managers", [&](ServerRequestInterface *req) {
        called = true;
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(result_json.dump());
        req->ReplyWithStatus(HTTPStatusCode::NOT_FOUND);
      });
  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(query);

  // Ensure that after encountering a NOT_FOUND error, the query status is
  // OK and no error summaries are populated.
  ASSERT_THAT(result, IsOk());
  EXPECT_FALSE(result->query_result.has_status());
  EXPECT_TRUE(called);
}

TEST_F(QueryPlannerTestRunner, TestServiceRootQuery) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(query_id: "ServiceRoot"
           subquery {
             subquery_id: "RedfishVersion"
             redpath: "/"
             properties { name: "Uri" property: "@odata\\.id" type: STRING }
             properties: {
               name: "RedfishSoftwareVersion"
               property: "RedfishVersion"
               type: STRING
             }
           }
           subquery {
             subquery_id: "ChassisLinked"
             root_subquery_ids: "RedfishVersion"
             redpath: "/Chassis[*]"
             properties {
               name: "serial_number"
               property: "SerialNumber"
               type: STRING
             }
             properties {
               name: "part_number"
               property: "PartNumber"
               type: STRING
             }
           })pb");

  QueryResult expected_query_result = ParseTextProtoOrDie(R"pb(
    query_id: "ServiceRoot"
    stats { payload_size: 196 num_cache_misses: 3 }
    data {
      fields {
        key: "RedfishVersion"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "ChassisLinked"
                  value {
                    list_value {
                      values {
                        subquery_value {
                          fields {
                            key: "part_number"
                            value { string_value: "1043652-02" }
                          }
                          fields {
                            key: "serial_number"
                            value { string_value: "MBBQTW194106556" }
                          }
                        }
                      }
                    }
                  }
                }
                fields {
                  key: "RedfishSoftwareVersion"
                  value { string_value: "1.6.1" }
                }
                fields {
                  key: "Uri"
                  value { string_value: "/redfish/v1" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  SetTestParams("indus_hmb_cn/mockup.shar");
  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(query);
  ASSERT_THAT(result, IsOk());
  EXPECT_FALSE(result->query_result.has_status());
  EXPECT_THAT(result->query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(expected_query_result)));
}

TEST(QueryPlannerTest, CheckQueryPlannerSendsOneRequestForEachUri) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(query_id: "SensorCollector"
           subquery {
             subquery_id: "Sensors"
             redpath: "/Chassis[*]/Sensors[*]"
             properties { property: "Name" type: STRING }
             properties { property: "ReadingType" type: STRING }
             properties { property: "ReadingUnits" type: STRING }
             properties { property: "Reading" type: INT64 }
           })pb");

  // Set up context node for dellicius query.
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  // Instantiate a passthrough normalizer.
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  // Create metrical transport and issue queries.
  std::unique_ptr<RedfishTransport> base_transport =
      server.RedfishClientTransport();
  auto transport = std::make_unique<MetricalRedfishTransport>(
      std::move(base_transport), Clock::RealClock());

  auto cache = std::make_unique<NullCache>(transport.get());
  auto intf = NewHttpInterface(std::move(transport), std::move(cache),
                               RedfishInterface::kTrusted);

  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({.query = &query,
                                             .normalizer = normalizer.get(),
                                             .redfish_interface = intf.get(),
                                             .redpath_rules = {}}));
  ASSERT_THAT(qp, NotNull());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1});
  // Metrics should be auto-populated in the query result.
  ASSERT_TRUE(result.query_result.stats().has_redfish_metrics());
  // For each type of redfish request for each URI, validate that the
  // QueryPlanner sends only 1 request.
  for (const auto &uri_x_metric : *result.query_result.mutable_stats()
                                       ->mutable_redfish_metrics()
                                       ->mutable_uri_to_metrics_map()) {
    for (const auto &metadata :
         uri_x_metric.second.request_type_to_metadata()) {
      EXPECT_EQ(metadata.second.request_count(), 1);
    }
  }
}

TEST_F(QueryPlannerGrpcTestRunner, CheckQueryPlannerRespectsTimeoutOnGetRoot) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "ChassisTest"
        subquery {
          subquery_id: "Chassis"
          redpath: "/Chassis[*]"
          properties { property: "Id" type: STRING }
          properties { property: "Name" type: STRING }
        }
      )pb");
  SetTestParams("indus_hmb_shim/mockup.shar");
  std::string expected_str = R"json({
    "@odata.context": "/redfish/v1/$metadata#ServiceRoot.ServiceRoot",
    "@odata.id": "/redfish/v1",
   })json";
  // Make root request wait past the timeout.
  server_->AddHttpGetHandler(
      "/redfish/v1",
      [&](grpc::ServerContext* context, const ::redfish::v1::Request* request,
          redfish::v1::Response* response) {
        response->set_json_str(expected_str);
        response->set_code(200);
        notification_.WaitForNotification();
        return grpc::Status::OK;
      });
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();
  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner({.query = &query,
                             .normalizer = normalizer.get(),
                             .redfish_interface = intf_.get(),
                             .redpath_rules = {},
                             .clock = &clock_,
                             .query_timeout = absl::Seconds(1)}));
  ASSERT_THAT(qp, NotNull());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1});
  EXPECT_THAT(result.query_result.status().error_code(),
              Eq(ecclesia::ErrorCode::ERROR_QUERY_TIMEOUT));
  EXPECT_THAT(result.query_result.status().errors().at(0),
              HasSubstr("Timed out while querying service root"));
  notification_.Notify();
}

TEST_F(QueryPlannerGrpcTestRunner,
       CheckQueryPlannerTimesOutWhenOneRequestHangs) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "ChassisTest"
        subquery {
          subquery_id: "Chassis"
          redpath: "/Chassis[*]"
          properties { property: "Id" type: STRING }
          properties { property: "Name" type: STRING }
        }
        subquery {
          subquery_id: "Sensors"
          root_subquery_ids: "Chassis"
          redpath: "/Sensors[ReadingUnits=RPM]"
          properties { property: "Name" type: STRING }
        }
      )pb");
  SetTestParams("indus_hmb_shim/mockup.shar");
  // Make just one of the sensor requests hang so we timeout.
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis/Sensors/indus_fan7_rpm",
      [&](grpc::ServerContext *context, const ::redfish::v1::Request *request,
          redfish::v1::Response *response) {
        response->set_code(200);
        notification_.WaitForNotification();
        return grpc::Status::OK;
      });
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();
  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner({.query = &query,
                             .normalizer = normalizer.get(),
                             .redfish_interface = intf_.get(),
                             .redpath_rules = {},
                             .clock = &clock_,
                             .query_timeout = absl::Seconds(1)}));
  ASSERT_THAT(qp, NotNull());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1});
  EXPECT_THAT(result.query_result.status().error_code(),
              Eq(ecclesia::ErrorCode::ERROR_QUERY_TIMEOUT));
  notification_.Notify();
}

TEST_F(QueryPlannerGrpcTestRunner,
       CheckQueryPlannerExecutesWithinTimeoutCorrecly) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "ChassisTest"
        subquery {
          subquery_id: "Chassis"
          redpath: "/Chassis[*]"
          properties { property: "Id" type: STRING }
          properties { property: "Name" type: STRING }
          freshness: REQUIRED
        }
        subquery {
          subquery_id: "Sensors"
          root_subquery_ids: "Chassis"
          redpath: "/Sensors[ReadingUnits=RPM]"
          properties { property: "Name" type: STRING }
          freshness: REQUIRED
        }
      )pb");
  SetTestParams("indus_hmb_shim/mockup.shar");
  // For 3 of the sensor requests, advance the clock by 1 second.
  for (absl::string_view uri :
       {"/redfish/v1/Chassis/chassis/Sensors/indus_fan2_rpm",
        "/redfish/v1/Chassis/chassis/Sensors/indus_fan3_rpm",
        "/redfish/v1/Chassis/chassis/Sensors/indus_fan4_rpm"}) {
    server_->AddHttpGetHandler(uri, [&](grpc::ServerContext *context,
                                        const ::redfish::v1::Request *request,
                                        redfish::v1::Response *response) {
      clock_.AdvanceTime(absl::Seconds(1));
      response->set_json_str(R"json({
        "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/fan_x",
        "@odata.type": "#Fan.v1_5_0.Fan",
        "Id": "Fan",
        "Name": "Fan",
        "ReadingUnits": "RPM"
      })json");
      response->set_code(200);
      return grpc::Status::OK;
    });
  }
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();
  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner({.query = &query,
                             .normalizer = normalizer.get(),
                             .redfish_interface = intf_.get(),
                             .redpath_rules = {},
                             .clock = &clock_,
                             .query_timeout = absl::Seconds(10)}));
  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1});
  EXPECT_FALSE(result.query_result.has_status());

  // Execute the query again with a QP with only 1 sec timeout, it should fail.
  std::unique_ptr<QueryPlannerIntf> qp2;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp2, BuildQueryPlanner({.query = &query,
                              .normalizer = normalizer.get(),
                              .redfish_interface = intf_.get(),
                              .redpath_rules = {},
                              .clock = &clock_,
                              .query_timeout = absl::Seconds(2)}));
  EXPECT_THAT(qp2, NotNull());
  QueryExecutionResult timeout_result = qp2->Run({.variables = args1});
  EXPECT_THAT(timeout_result.query_result.status().error_code(),
              Eq(ecclesia::ErrorCode::ERROR_QUERY_TIMEOUT));
}

TEST_F(QueryPlannerGrpcTestRunner, CheckEachQueryPlannerRunRespectsTimeout) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "ChassisTest"
        subquery {
          subquery_id: "Chassis"
          redpath: "/Chassis[*]"
          properties { property: "Id" type: STRING }
          properties { property: "Name" type: STRING }
          freshness: REQUIRED
        }
        subquery {
          subquery_id: "Sensors"
          root_subquery_ids: "Chassis"
          redpath: "/Sensors[ReadingUnits=RPM]"
          properties { property: "Name" type: STRING }
          freshness: REQUIRED
        }
      )pb");
  SetTestParams("indus_hmb_shim/mockup.shar");
  // For 3 of the sensor requests, advance the clock by 1 second.
  for (absl::string_view uri :
       {"/redfish/v1/Chassis/chassis/Sensors/indus_fan2_rpm",
        "/redfish/v1/Chassis/chassis/Sensors/indus_fan3_rpm",
        "/redfish/v1/Chassis/chassis/Sensors/indus_fan4_rpm"}) {
    server_->AddHttpGetHandler(uri, [&](grpc::ServerContext *context,
                                        const ::redfish::v1::Request *request,
                                        redfish::v1::Response *response) {
      clock_.AdvanceTime(absl::Seconds(1));
      response->set_json_str(R"json({
        "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/fan_x",
        "@odata.type": "#Fan.v1_5_0.Fan",
        "Id": "Fan",
        "Name": "Fan",
        "ReadingUnits": "RPM"
      })json");
      response->set_code(200);
      return grpc::Status::OK;
    });
  }
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();
  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner({.query = &query,
                             .normalizer = normalizer.get(),
                             .redfish_interface = intf_.get(),
                             .redpath_rules = {},
                             .clock = &clock_,
                             .query_timeout = absl::Seconds(12)}));
  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  // Issue multiple Run calls with the same QP and ensure they all pass.
  for (int i = 0; i < 3; ++i) {
    QueryExecutionResult result = qp->Run({.variables = args1});
    EXPECT_FALSE(result.query_result.has_status());
  }
}

TEST_F(QueryPlannerTestRunner, ServiceRootQueryWithParamsSuccessful) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(query_id: "ServiceRoot"
           subquery {
             subquery_id: "RedfishVersion"
             redpath: "/"
             properties { name: "Uri" property: "@odata\\.id" type: STRING }
             properties: {
               name: "RedfishSoftwareVersion"
               property: "RedfishVersion"
               type: STRING
             }
           })pb");

  SetTestParams("indus_hmb_cn/mockup.shar");
  auto transport = std::make_unique<MetricalRedfishTransport>(
      server_->RedfishClientTransport(), Clock::RealClock());
  auto cache = std::make_unique<NullCache>(transport.get());
  auto intf = NewHttpInterface(std::move(transport), std::move(cache),
                               RedfishInterface::kTrusted);
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  absl::Time test_time = absl::UnixEpoch() + absl::Seconds(50);
  FakeClock clock(test_time);

  RedPathRules redpath_rules = {
      .redpath_to_query_params = {
          {"/",
           {.expand = RedfishQueryParamExpand(
                {.type = RedfishQueryParamExpand::ExpandType::kNotLinks,
                 .levels = 1})}}}};

  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner({.query = &query,
                             .normalizer = normalizer.get(),
                             .redfish_interface = intf.get(),
                             .redpath_rules = std::move(redpath_rules),
                             .clock = &clock}));

  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  auto result = qp->Run({.variables = args1});
  ASSERT_TRUE(result.query_result.stats().has_redfish_metrics());
  EXPECT_EQ(
      result.query_result.stats().redfish_metrics().uri_to_metrics_map_size(),
      1);
  EXPECT_TRUE(result.query_result.stats()
                  .redfish_metrics()
                  .uri_to_metrics_map()
                  .contains("/redfish/v1?$expand=.($levels=1)"));
}

TEST_F(QueryPlannerTestRunner,
       QueryPlannerExecutesPredicatesOnSingleRedfishObject) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "PredicateOnRedfishObject"
        subquery {
          subquery_id: "EnvironmentMetrics"
          redpath: "/Systems[*]/Processors[*]/EnvironmentMetrics[Id=EnvironmentMetrics]"
          properties { property: "PowerWatts.DataSourceUri" type: STRING }
          properties { property: "PowerWatts.Reading" type: DOUBLE }
        }
      )pb");

  QueryResult expected_query_result = ParseTextProtoOrDie(R"pb(
    query_id: "PredicateOnRedfishObject"
    stats { payload_size: 302 num_cache_misses: 8 }
    data {
      fields {
        key: "EnvironmentMetrics"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "PowerWatts.DataSourceUri"
                  value {
                    string_value: "/redfish/v1/Chassis/chassis/Sensors/indus_cpu0_pwmon"
                  }
                }
                fields {
                  key: "PowerWatts.Reading"
                  value { double_value: 30 }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "PowerWatts.DataSourceUri"
                  value {
                    string_value: "/redfish/v1/Chassis/chassis/Sensors/indus_cpu1_pwmon"
                  }
                }
                fields {
                  key: "PowerWatts.Reading"
                  value { double_value: 30 }
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

  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({.query = &query,
                                             .normalizer = normalizer.get(),
                                             .redfish_interface = intf_.get(),
                                             .redpath_rules = {}}));

  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1});
  ASSERT_FALSE(result.query_result.has_status());
  EXPECT_THAT(expected_query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(result.query_result)));
}

TEST_F(QueryPlannerTestRunner, QueryPlannerQueriesRawDataStringSuccessfully) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "TestRawData"
        subquery {
          subquery_id: "ServiceRoot"
          redpath: "/"
          properties: { property: "Id" type: STRING }
        }
        # Returns the CPER log binary
        subquery {
          subquery_id: "CperBinary"
          root_subquery_ids: "ServiceRoot"
          uri_reference_redpath: "AdditionalDataURI"
          freshness: REQUIRED
          fetch_raw_data { type: STRING }
        }
      )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");
  server_->AddHttpGetHandler("/redfish/v1", [&](ServerRequestInterface *req) {
    SetContentType(req, "application/json");
    req->OverwriteResponseHeader("OData-Version", "4.0");
    req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/",
          "Id": "1",
          "AdditionalDataURI": "/redfish/v1/additional"
        })json");
    req->Reply();
  });

  server_->AddHttpGetHandler(
      "/redfish/v1/additional", [](ServerRequestInterface *req) {
        SetContentTypeTEXT(req);
        req->WriteResponseString("adfasdfasfasdfasdfasdfxzcfaskdfaksdfn");
        req->Reply();
      });

  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({.query = &query,
                                             .normalizer = normalizer.get(),
                                             .redfish_interface = intf_.get(),
                                             .redpath_rules = {}}));
  ASSERT_THAT(qp, NotNull());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1});

  QueryResult expected_query_result = ParseTextProtoOrDie(R"pb(
    query_id: "TestRawData"
    stats { payload_size: 123 num_cache_misses: 2 }
    data {
      fields {
        key: "ServiceRoot"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "CperBinary"
                  value {
                    raw_data {
                      raw_string_value: "YWRmYXNkZmFzZmFzZGZhc2RmYXNkZnh6Y2Zhc2tkZmFrc2Rmbg=="
                    }
                  }
                }
                fields {
                  key: "Id"
                  value { string_value: "1" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  ASSERT_FALSE(result.query_result.has_status());
  EXPECT_THAT(result.query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(expected_query_result)));
}

TEST_F(QueryPlannerTestRunner, QueryPlannerQueriesRawDataBytesSuccessfully) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "TestRawData"
        subquery {
          subquery_id: "ServiceRoot"
          redpath: "/"
          properties: { property: "Id" type: STRING }
        }
        # Returns the CPER log binary
        subquery {
          subquery_id: "CperBinary"
          root_subquery_ids: "ServiceRoot"
          uri_reference_redpath: "AdditionalDataURI"
          freshness: REQUIRED
          fetch_raw_data { type: BYTES }
        }
      )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");
  server_->AddHttpGetHandler("/redfish/v1", [&](ServerRequestInterface *req) {
    SetContentType(req, "application/json");
    req->OverwriteResponseHeader("OData-Version", "4.0");
    req->WriteResponseString(R"json({
          "@odata.id": "/redfish/v1/",
          "Id": "1",
          "AdditionalDataURI": "/redfish/v1/additional"
        })json");
    req->Reply();
  });

  server_->AddHttpGetHandler(
      "/redfish/v1/additional", [](ServerRequestInterface *req) {
        SetContentTypeTEXT(req);
        req->WriteResponseString("adfasdfasfasdfasdfasdfxzcfaskdfaksdfn");
        req->Reply();
      });

  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({.query = &query,
                                             .normalizer = normalizer.get(),
                                             .redfish_interface = intf_.get(),
                                             .redpath_rules = {}}));
  ASSERT_THAT(qp, NotNull());

  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1});

  QueryResult expected_query_result = ParseTextProtoOrDie(R"pb(
    query_id: "TestRawData"
    stats { payload_size: 108 num_cache_misses: 2 }
    data {
      fields {
        key: "ServiceRoot"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "CperBinary"
                  value {
                    raw_data {
                      raw_bytes_value: "adfasdfasfasdfasdfasdfxzcfaskdfaksdfn"
                    }
                  }
                }
                fields {
                  key: "Id"
                  value { string_value: "1" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  ASSERT_FALSE(result.query_result.has_status());
  EXPECT_THAT(result.query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(expected_query_result)));
}

TEST_F(QueryPlannerTestRunner, CollectionResourceCanBeQueried) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "ChassisSubTreeTest"
        subquery {
          subquery_id: "Chassis"
          redpath: "/Chassis/Members[*]"
          properties { property: "Id" type: STRING }
        }
      )pb");

  QueryResult expected_query_result = ParseTextProtoOrDie(R"pb(
    query_id: "ChassisSubTreeTest"
    stats { payload_size: 60 num_cache_misses: 4 }
    data: {
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
  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(qp,
                          BuildQueryPlanner({.query = &query,
                                             .normalizer = normalizer.get(),
                                             .redfish_interface = intf_.get(),
                                             .redpath_rules = {}}));
  ASSERT_THAT(qp, NotNull());
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  QueryExecutionResult result = qp->Run({.variables = args1});

  EXPECT_FALSE(result.query_result.has_status());
  EXPECT_THAT(expected_query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(result.query_result)));
}

TEST_F(QueryPlannerTestRunner,
       QueryPlannerContinuesQueryOnErrorCollectionResource) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(query_id: "SensorCollector"
           subquery {
             subquery_id: "Sensors"
             redpath: "/Chassis[*]/Sensors[*]"
             properties { property: "Name" type: STRING }
             properties { property: "ReadingType" type: STRING }
             properties { property: "ReadingUnits" type: STRING }
             properties { property: "Reading" type: INT64 }
           })pb");
  SetTestParams("indus_hmb_shim/mockup.shar");
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis/Sensors/indus_fan7_rpm",
      [](ServerRequestInterface *req) {
        req->ReplyWithStatus(HTTPStatusCode::UNAUTHORIZED);
      });

  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(
      query, QueryPlanner::ExecutionMode::kContinueOnSubqueryErrors);

  // Ensure that after encountering a NOT_FOUND error, the query status is OK.
  ASSERT_THAT(result, IsOk());
  QueryResult expected_query_result = ParseTextProtoOrDie(R"pb(
    query_id: "SensorCollector"
    status {
      errors: "Querying predicate [*] from Redpath: /Chassis[*]/Sensors[*] resulted in error: At resource URI: /redfish/v1/Chassis/chassis/Sensors/indus_fan7_rpm: Unauthorized"
      error_code: ERROR_UNAUTHENTICATED
    }
    stats { payload_size: 1357 num_cache_misses: 18 }
    data {
      fields {
        key: "Sensors"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "fan0" }
                }
                fields {
                  key: "Reading"
                  value { int_value: 16115 }
                }
                fields {
                  key: "ReadingType"
                  value { string_value: "Rotational" }
                }
                fields {
                  key: "ReadingUnits"
                  value { string_value: "RPM" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "fan1" }
                }
                fields {
                  key: "Reading"
                  value { int_value: 16115 }
                }
                fields {
                  key: "ReadingType"
                  value { string_value: "Rotational" }
                }
                fields {
                  key: "ReadingUnits"
                  value { string_value: "RPM" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "fan2" }
                }
                fields {
                  key: "Reading"
                  value { int_value: 16115 }
                }
                fields {
                  key: "ReadingType"
                  value { string_value: "Rotational" }
                }
                fields {
                  key: "ReadingUnits"
                  value { string_value: "RPM" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "fan3" }
                }
                fields {
                  key: "Reading"
                  value { int_value: 16115 }
                }
                fields {
                  key: "ReadingType"
                  value { string_value: "Rotational" }
                }
                fields {
                  key: "ReadingUnits"
                  value { string_value: "RPM" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "fan4" }
                }
                fields {
                  key: "Reading"
                  value { int_value: 16115 }
                }
                fields {
                  key: "ReadingType"
                  value { string_value: "Rotational" }
                }
                fields {
                  key: "ReadingUnits"
                  value { string_value: "RPM" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "fan5" }
                }
                fields {
                  key: "Reading"
                  value { int_value: 16115 }
                }
                fields {
                  key: "ReadingType"
                  value { string_value: "Rotational" }
                }
                fields {
                  key: "ReadingUnits"
                  value { string_value: "RPM" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "fan6" }
                }
                fields {
                  key: "Reading"
                  value { int_value: 16115 }
                }
                fields {
                  key: "ReadingType"
                  value { string_value: "Rotational" }
                }
                fields {
                  key: "ReadingUnits"
                  value { string_value: "RPM" }
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
                fields {
                  key: "ReadingType"
                  value { string_value: "Temperature" }
                }
                fields {
                  key: "ReadingUnits"
                  value { string_value: "Cel" }
                }
              }
            }
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
                  key: "ReadingType"
                  value { string_value: "Temperature" }
                }
                fields {
                  key: "ReadingUnits"
                  value { string_value: "Cel" }
                }
              }
            }
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
                fields {
                  key: "ReadingType"
                  value { string_value: "Power" }
                }
                fields {
                  key: "ReadingUnits"
                  value { string_value: "W" }
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
                  key: "Reading"
                  value { int_value: 30 }
                }
                fields {
                  key: "ReadingType"
                  value { string_value: "Power" }
                }
                fields {
                  key: "ReadingUnits"
                  value { string_value: "W" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "Name"
                  value { string_value: "CPU0" }
                }
                fields {
                  key: "Reading"
                  value { int_value: 60 }
                }
                fields {
                  key: "ReadingType"
                  value { string_value: "Temperature" }
                }
                fields {
                  key: "ReadingUnits"
                  value { string_value: "Cel" }
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
                  key: "Reading"
                  value { int_value: 60 }
                }
                fields {
                  key: "ReadingType"
                  value { string_value: "Temperature" }
                }
                fields {
                  key: "ReadingUnits"
                  value { string_value: "Cel" }
                }
              }
            }
          }
        }
      }
    }
  )pb");
  EXPECT_THAT(result->query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(expected_query_result)));
}

TEST_F(QueryPlannerTestRunner,
       QueryPlannerStopsQueryOnErrorCollectionResource) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(query_id: "SensorCollector"
           subquery {
             subquery_id: "Sensors"
             redpath: "/Chassis[*]/Sensors[*]"
             properties { property: "Name" type: STRING }
             properties { property: "ReadingType" type: STRING }
             properties { property: "ReadingUnits" type: STRING }
             properties { property: "Reading" type: INT64 }
           })pb");
  SetTestParams("indus_hmb_shim/mockup.shar");
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis/Sensors/indus_fan7_rpm",
      [](ServerRequestInterface *req) {
        req->ReplyWithStatus(HTTPStatusCode::UNAUTHORIZED);
      });

  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(
      query, QueryPlanner::ExecutionMode::kFailOnFirstError);

  ASSERT_THAT(result, IsOk());
  QueryResult expected_query_result = ParseTextProtoOrDie(R"pb(
    query_id: "SensorCollector"
    status {
      errors: "Querying predicate [*] from Redpath: /Chassis[*]/Sensors[*] resulted in error: At resource URI: /redfish/v1/Chassis/chassis/Sensors/indus_fan7_rpm: Unauthorized"
      error_code: ERROR_UNAUTHENTICATED
    }
    stats { payload_size: 187 num_cache_misses: 12 }
  )pb");
  EXPECT_THAT(result->query_result,
              ecclesia::EqualsProto(expected_query_result));
}

TEST_F(QueryPlannerTestRunner, QueryPlannerStopsQueryOnErrorSingletonResource) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(query_id: "SensorCollector"
           subquery {
             subquery_id: "Sensors"
             redpath: "/Chassis[*]/Sensors"
             properties { property: "Name" type: STRING }
             properties { property: "ReadingType" type: STRING }
             properties { property: "ReadingUnits" type: STRING }
             properties { property: "Reading" type: INT64 }
           }
           subquery {
             subquery_id: "Assembly"
             redpath: "/Chassis[*]/Assembly"
             properties { property: "Name" type: STRING }
             properties { property: "ReadingType" type: STRING }
             properties { property: "ReadingUnits" type: STRING }
             properties { property: "Reading" type: INT64 }
           })pb");
  SetTestParams("indus_hmb_shim/mockup.shar");
  std::atomic<bool> called = false;
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis/Sensors",
      [&called](ServerRequestInterface *req) {
        if (called) {
          req->ReplyWithStatus(HTTPStatusCode::OK);
          return;
        }
        called = true;
        req->ReplyWithStatus(HTTPStatusCode::UNAUTHORIZED);
      });

  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis/Assembly",
      [&called](ServerRequestInterface *req) {
        if (called) {
          req->ReplyWithStatus(HTTPStatusCode::OK);
          return;
        }
        called = true;
        req->ReplyWithStatus(HTTPStatusCode::UNAUTHORIZED);
      });

  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(
      query, QueryPlanner::ExecutionMode::kFailOnFirstError);

  ASSERT_THAT(result, IsOk());
  EXPECT_EQ(result->query_result.data().ByteSizeLong(), 0);
}

TEST_F(QueryPlannerTestRunner,
       QueryPlannerContinuesQueryOnErrorSingletonResource) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(query_id: "SensorCollector"
           subquery {
             subquery_id: "Sensors"
             redpath: "/Chassis[*]/Sensors"
             properties { property: "Name" type: STRING }
           }
           subquery {
             subquery_id: "Assembly"
             redpath: "/Chassis[*]/Assembly"
             properties { property: "Name" type: STRING }
           })pb");
  SetTestParams("indus_hmb_shim/mockup.shar");
  std::atomic<bool> called = false;
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis/Sensors",
      [&called](ServerRequestInterface *req) {
        if (!called) {
          called = true;
          req->ReplyWithStatus(HTTPStatusCode::UNAUTHORIZED);
          return;
        }
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json({
            "@odata.id": "/redfish/v1/Chassis/chassis/Sensors",
            "@odata.type": "#SensorCollection.SensorCollection",
            "Name": "Chassis sensors",
            "Members@odata.count": 2,
            "Members": [
                {
                    "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/indus_fan0_rpm"
                },
                {
                    "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/indus_fan1_rpm"
                }
            ]
          })json");
        req->ReplyWithStatus(HTTPStatusCode::OK);
      });

  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/chassis/Assembly",
      [&called](ServerRequestInterface *req) {
        if (!called) {
          called = true;
          req->ReplyWithStatus(HTTPStatusCode::UNAUTHORIZED);
          return;
        }
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(R"json(
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
              }
            ]
          })json");
        req->ReplyWithStatus(HTTPStatusCode::OK);
      });

  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(
      query, QueryPlanner::ExecutionMode::kContinueOnSubqueryErrors);

  ASSERT_THAT(result, IsOk());
  EXPECT_NE(result->query_result.data().ByteSizeLong(), 0);
}

TEST_F(QueryPlannerTestRunner,
       QueryPlannerExecutesTemplatedQueryWithFilterRulesCorrectly) {
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
      )pb");

  SetTestParams("indus_hmb_shim/mockup.shar");
  auto transport = std::make_unique<MetricalRedfishTransport>(
      server_->RedfishClientTransport(), Clock::RealClock());
  auto cache = std::make_unique<NullCache>(transport.get());
  auto intf = NewHttpInterface(std::move(transport), std::move(cache),
                               RedfishInterface::kTrusted);

  std::unique_ptr<RedpathNormalizer> normalizer =
      BuildDefaultRedpathNormalizer();

  RedPathRules redpath_rules = {
      .redpath_to_query_params = {
          {"/Chassis[*]/Sensors", {.filter = RedfishQueryParamFilter("")}}}};

  std::unique_ptr<QueryPlannerIntf> qp;
  ECCLESIA_ASSIGN_OR_FAIL(
      qp, BuildQueryPlanner({.query = &query,
                             .normalizer = normalizer.get(),
                             .redfish_interface = intf.get(),
                             .redpath_rules = std::move(redpath_rules)}));
  ASSERT_THAT(qp, NotNull());
  QueryVariables args1 = ecclesia::QueryVariables();
  QueryVariables::VariableValue val1;
  QueryVariables::VariableValue val2;
  val1.set_name("Threshold");
  *val1.add_values() = "36";
  val2.set_name("Name");
  *val2.add_values() = "indus_latm_temp";

  *args1.add_variable_values() = val1;
  *args1.add_variable_values() = val2;
  QueryExecutionResult result = qp->Run({.variables = args1});

  EXPECT_FALSE(result.query_result.has_status());
  ASSERT_TRUE(result.query_result.stats().has_redfish_metrics());
  EXPECT_TRUE(result.query_result.stats()
                  .redfish_metrics()
                  .uri_to_metrics_map()
                  .contains("/redfish/v1/Chassis/chassis/"
                            "Sensors?$filter=Name%20eq%20%27indus_latm_temp%27%"
                            "20and%20Reading%20lt%2036"));
}

TEST_F(QueryPlannerTestRunner, TestEmptyRootSubQuerySuccess) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(query_id: "ServiceRoot"
           subquery { subquery_id: "RedfishVersion" redpath: "/" }
           subquery {
             subquery_id: "ChassisLinked"
             root_subquery_ids: "RedfishVersion"
             redpath: "/Chassis[*]"
             properties {
               name: "serial_number"
               property: "SerialNumber"
               type: STRING
             }
             properties {
               name: "part_number"
               property: "PartNumber"
               type: STRING
             }
           })pb");

  QueryResult expected_query_result = ParseTextProtoOrDie(R"pb(
    query_id: "ServiceRoot"
    stats { payload_size: 133 num_cache_misses: 3 }
    data {
      fields {
        key: "RedfishVersion"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "ChassisLinked"
                  value {
                    list_value {
                      values {
                        subquery_value {
                          fields {
                            key: "part_number"
                            value { string_value: "1043652-02" }
                          }
                          fields {
                            key: "serial_number"
                            value { string_value: "MBBQTW194106556" }
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

  SetTestParams("indus_hmb_cn/mockup.shar");
  absl::StatusOr<QueryExecutionResult> result = PlanAndExecuteQuery(query);
  ASSERT_THAT(result, IsOk());
  EXPECT_FALSE(result->query_result.has_status());
  EXPECT_THAT(result->query_result,
              ecclesia::IgnoringRepeatedFieldOrdering(
                  ecclesia::EqualsProto(expected_query_result)));
}

TEST(RedpathQueryTimestampTest, TestTimestamp) {
  FakeClock clock(absl::FromUnixSeconds(100));
  QueryPlannerIntf::QueryExecutionResult result;
  result.query_result = ParseTextProtoOrDie(R"pb(
    query_id: "ServiceRoot"
    stats { payload_size: 133 num_cache_misses: 3 }
  )pb");
  {
    RedpathQueryTimestamp timestamp(&result, &clock);
    clock.AdvanceTime(absl::Seconds(10));
  }
  EXPECT_THAT(result.query_result.stats().start_time(),
              EqualsProto(R"pb(seconds: 100)pb"));
  EXPECT_THAT(result.query_result.stats().end_time(),
              EqualsProto(R"pb(seconds: 110)pb"));
}

}  // namespace

}  // namespace ecclesia
