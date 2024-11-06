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

#include "ecclesia/lib/redfish/redpath/definitions/query_result/path_util.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

TEST(GetQueryValueFromResult, EmptyPath) {
  EXPECT_THAT(GetQueryValueFromResult({}, ""), IsStatusInvalidArgument());
}

TEST(GetQueryValueFromResult, MisMatchQueryId) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query1"
  )pb");

  std::string path = "query2";

  EXPECT_THAT(GetQueryValueFromResult(result, path), IsStatusInvalidArgument());
}

TEST(GetQueryValueFromResult, NotSubqueryPath) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query1"
  )pb");

  std::string path = "query1";

  EXPECT_THAT(GetQueryValueFromResult(result, path), IsStatusInvalidArgument());
}

TEST(GetQueryValueFromResult, NoData1) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query1"
  )pb");

  std::string path = "query1.subquery_id";

  EXPECT_THAT(GetQueryValueFromResult(result, path), IsStatusInvalidArgument());
}

TEST(GetQueryValueFromResult, NoData2) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query1"
    data: {}
  )pb");

  std::string path = "query1.subquery_id";

  EXPECT_THAT(GetQueryValueFromResult(result, path), IsStatusInvalidArgument());
}

TEST(GetQueryValueFromResult, WrongTopSubqueryId) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query1"
    data: {
      fields {
        key: "subquery_id1"
        value {
          subquery_value {
            fields {
              key: "_id_"
              value { identifier { local_devpath: "/phys" } }
            }
            fields {
              key: "name1"
              value { string_value: "value1" }
            }
          }
        }
      }
    }
  )pb");

  std::string path = "query1.subquery_id2";

  EXPECT_THAT(GetQueryValueFromResult(result, path), IsStatusInvalidArgument());
}

TEST(GetQueryValueFromResult, WrongPropertyName) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query1"
    data: {
      fields {
        key: "subquery_id1"
        value {
          subquery_value {
            fields {
              key: "_id_"
              value { identifier { local_devpath: "/phys" } }
            }
            fields {
              key: "name1"
              value { string_value: "value1" }
            }
          }
        }
      }
    }
  )pb");

  std::string path = "query1.subquery_id1.name2";

  EXPECT_THAT(GetQueryValueFromResult(result, path), IsStatusInvalidArgument());
}

TEST(GetQueryValueFromResult, GetPropertyName) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query1"
    data: {
      fields {
        key: "subquery_id1"
        value {
          subquery_value {
            fields {
              key: "_id_"
              value { identifier { local_devpath: "/phys" } }
            }
            fields {
              key: "name1"
              value { string_value: "value1" }
            }
          }
        }
      }
    }
  )pb");

  EXPECT_THAT(
      GetQueryValueFromResult(result, "query1.subquery_id1.name1"),
      IsOkAndHolds(ecclesia::EqualsProto(R"pb(string_value: "value1")pb")));

  EXPECT_THAT(GetQueryValueFromResult(result, "query1.subquery_id1._id_"),
              IsOkAndHolds(ecclesia::EqualsProto(R"pb(identifier: {
                                                        local_devpath: "/phys"
                                                      })pb")));
}

TEST(GetQueryValueFromResult, IdentifierNotFormatted) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query1"
    data: {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "name"
                  value { string_value: "value1" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "name"
                  value { string_value: "value2" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  std::string path = "query1.subquery_id.name.name";

  EXPECT_THAT(GetQueryValueFromResult(result, path), IsStatusInvalidArgument());
}

TEST(GetQueryValueFromResult, IdentifierNotSpecified) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query1"
    data: {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "name"
                  value { string_value: "value1" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "name"
                  value { string_value: "value2" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  std::string path = "query1.subquery_id.name==.name";

  EXPECT_THAT(GetQueryValueFromResult(result, path), IsStatusInvalidArgument());
}

TEST(GetQueryValueFromResult, IdentifierNotMatching) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query1"
    data: {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "name"
                  value { string_value: "value1" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "name"
                  value { string_value: "value2" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  std::string path = "query1.subquery_id.name=no_value.name";

  EXPECT_THAT(GetQueryValueFromResult(result, path), IsStatusNotFound());
}

TEST(GetQueryValueFromResult, IdentifierMatch) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query1"
    data: {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "name"
                  value { string_value: "value1" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "name"
                  value { string_value: "value2" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  std::string path = "query1.subquery_id.name=\"value1\".name";

  EXPECT_THAT(
      GetQueryValueFromResult(result, path),
      IsOkAndHolds(ecclesia::EqualsProto(R"pb(string_value: "value1")pb")));
}

TEST(GetQueryValueFromResult, IdentifierMatchOnIdentifier) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query1"
    data: {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "_id_"
                  value { identifier { local_devpath: "/phys" } }
                }
                fields {
                  key: "name"
                  value { string_value: "value1" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "_id_"
                  value { identifier { local_devpath: "/phys/something" } }
                }
                fields {
                  key: "name"
                  value { string_value: "value2" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  std::string path =
      "query1.subquery_id._id_={\"_local_devpath_\":\"/phys\"}.name";

  EXPECT_THAT(
      GetQueryValueFromResult(result, path),
      IsOkAndHolds(ecclesia::EqualsProto(R"pb(string_value: "value1")pb")));
}

TEST(GetQueryValueFromResult, IdentifierMatchManyValues) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query1"
    data: {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "other_name"
                  value { string_value: "value1" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "name"
                  value { string_value: "value1" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "other_name"
                  value { string_value: "value3" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  std::string path = "query1.subquery_id.name=\"value1\".name";

  EXPECT_THAT(
      GetQueryValueFromResult(result, path),
      IsOkAndHolds(ecclesia::EqualsProto(R"pb(string_value: "value1")pb")));
}

TEST(GetQueryValueFromResult, MalformedIndex) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query1"
    data: {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "name"
                  value { string_value: "value1" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "name"
                  value { string_value: "value2" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  std::string path = "query1.subquery_id.0.name";

  EXPECT_THAT(GetQueryValueFromResult(result, path), IsStatusInvalidArgument());
}

TEST(GetQueryValueFromResult, IndexNotInt) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query1"
    data: {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "name"
                  value { string_value: "value1" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "name"
                  value { string_value: "value2" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  std::string path = "query1.subquery_id.[example].name";

  EXPECT_THAT(GetQueryValueFromResult(result, path), IsStatusInvalidArgument());
}

TEST(GetQueryValueFromResult, IndexDoesNotExist) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query1"
    data: {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "name"
                  value { string_value: "value1" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "name"
                  value { string_value: "value2" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  EXPECT_THAT(GetQueryValueFromResult(result, "query1.subquery_id.[3].name"),
              IsStatusNotFound());

  EXPECT_THAT(GetQueryValueFromResult(result, "query1.subquery_id.[-1].name"),
              IsStatusNotFound());
}

TEST(GetQueryValueFromResult, ListIndexSuccess) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query1"
    data: {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "name"
                  value { string_value: "value1" }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "name"
                  value { string_value: "value2" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  EXPECT_THAT(
      GetQueryValueFromResult(result, "query1.subquery_id.[0].name"),
      IsOkAndHolds(ecclesia::EqualsProto(R"pb(string_value: "value1")pb")));
}

TEST(GetQueryValueFromResult, NotAllowedType) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query1"
    data: {
      fields {
        key: "subquery_id"
        value { string_value: "value1" }
      }
    }
  )pb");

  EXPECT_THAT(GetQueryValueFromResult(result, "query1.subquery_id.[0].name"),
              IsStatusInvalidArgument());
}

}  // namespace
}  // namespace ecclesia
