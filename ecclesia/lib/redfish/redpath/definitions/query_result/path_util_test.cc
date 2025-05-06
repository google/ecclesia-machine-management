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
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::testing::TestWithParam;
using ::testing::ValuesIn;

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

  EXPECT_THAT(GetQueryValueFromResult(result, path), IsStatusNotFound());
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

  EXPECT_THAT(GetQueryValueFromResult(result, path), IsStatusNotFound());
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

  EXPECT_THAT(GetQueryValueFromResult(result, path), IsStatusNotFound());
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

  std::string path = "query1.subquery_id[name==].name";

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

  std::string path = "query1.subquery_id[name=no_value].name";

  EXPECT_THAT(GetQueryValueFromResult(result, path), IsStatusNotFound());
}

TEST(GetQueryValueFromResult, RightSquareBracketMissing) {
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

  EXPECT_THAT(GetQueryValueFromResult(result, "query1.subquery_id[1.name"),
              IsStatusNotFound());
}

TEST(GetQueryValueFromResult, ExtraLeftSquareBracket) {
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

  EXPECT_THAT(GetQueryValueFromResult(result, "query1.subquery_id[[1].name"),
              IsStatusInvalidArgument());
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

  std::string path = "query1.subquery_id[name=value1].name";

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

  std::string path = "query1.subquery_id[_id_={_local_devpath_:/phys}].name";

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

  std::string path = "query1.subquery_id[name=value1].name";

  EXPECT_THAT(
      GetQueryValueFromResult(result, path),
      IsOkAndHolds(ecclesia::EqualsProto(R"pb(string_value: "value1")pb")));
}

TEST(GetQueryValueFromResult, IdentifierMatchMultiplePredicates) {
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
                  key: "name1"
                  value { string_value: "value1" }
                }
                fields {
                  key: "name2"
                  value { string_value: "value2" }
                }
                fields {
                  key: "name3"
                  value { string_value: "value3" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  std::string path = "query1.subquery_id[name1=value1;name2=value2].name1";

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

  EXPECT_THAT(GetQueryValueFromResult(result, path), IsStatusNotFound());
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

  std::string path = "query1.subquery_id[example].name";

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

  EXPECT_THAT(GetQueryValueFromResult(result, "query1.subquery_id[3].name"),
              IsStatusNotFound());

  EXPECT_THAT(GetQueryValueFromResult(result, "query1.subquery_id[-1].name"),
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
      GetQueryValueFromResult(result, "query1.subquery_id[0].name"),
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

  EXPECT_THAT(GetQueryValueFromResult(result, "query1.subquery_id[0].name"),
              IsStatusInvalidArgument());
}

struct GetMutableQueryValueFromResultTestCase {
  QueryResult result;
  std::string path;
  internal_status::IsStatusPolyMatcher expected_status_code = IsOk();
  QueryValue expected_value;
};

using GetMutableQueryValueFromResultFailureTest =
    TestWithParam<GetMutableQueryValueFromResultTestCase>;

TEST_P(GetMutableQueryValueFromResultFailureTest, ReturnNullptrRespoonse) {
  GetMutableQueryValueFromResultTestCase test_cases = GetParam();
  EXPECT_THAT(
      GetMutableQueryValueFromResult(test_cases.result, test_cases.path),
      test_cases.expected_status_code);
}

INSTANTIATE_TEST_SUITE_P(
    GetMutableQueryValueFromResultTests,
    GetMutableQueryValueFromResultFailureTest,
    ValuesIn<GetMutableQueryValueFromResultTestCase>({
        {.result = ParseTextProtoOrDie(R"pb(
           query_id: "query1"
         )pb"),
         .path = "",
         .expected_status_code = IsStatusInvalidArgument()},
        {.result = ParseTextProtoOrDie(R"pb(
           query_id: "query1"
         )pb"),
         .path = "query2",
         .expected_status_code = IsStatusInvalidArgument()},
        {.result = ParseTextProtoOrDie(R"pb(
           query_id: "query1"
         )pb"),
         .path = "query1",
         .expected_status_code = IsStatusInvalidArgument()},
        {.result = ParseTextProtoOrDie(R"pb(
           query_id: "query1"
         )pb"),
         .path = "query1.subquery_id",
         .expected_status_code = IsStatusInvalidArgument()},
        {.result = ParseTextProtoOrDie(R"pb(
           query_id: "query1"
           data: {}
         )pb"),
         .path = "query1.subquery_id",
         .expected_status_code = IsStatusInvalidArgument()},
        {.result = ParseTextProtoOrDie(R"pb(
           query_id: "query1"
           data: {
             fields {
               key: "subquery_id"
               value { string_value: "value1" }
             }
           }
         )pb"),
         .path = "query1.subquery_id[0].name",
         .expected_status_code = IsStatusInvalidArgument()},
        {.result = ParseTextProtoOrDie(R"pb(
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
         )pb"),
         .path = "query1.subquery_id2",
         .expected_status_code = IsStatusNotFound()},
        {.result = ParseTextProtoOrDie(R"pb(
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
         )pb"),
         .path = "query1.subquery_id1.name2",
         .expected_status_code = IsStatusNotFound()},
        {.result = ParseTextProtoOrDie(R"pb(
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
         )pb"),
         .path = "query1.subquery_id.name.name",
         .expected_status_code = IsStatusNotFound()},
        {.result = ParseTextProtoOrDie(R"pb(
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
         )pb"),
         .path = "query1.subquery_id[name==].name",
         .expected_status_code = IsStatusInvalidArgument()},
        {.result = ParseTextProtoOrDie(R"pb(
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
         )pb"),
         .path = "query1.subquery_id[name=no_value].name",
         .expected_status_code = IsStatusNotFound()},
        {.result = ParseTextProtoOrDie(R"pb(
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
         )pb"),
         .path = "query1.subquery_id.0.name",
         .expected_status_code = IsStatusNotFound()},
        {.result = ParseTextProtoOrDie(R"pb(
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
         )pb"),
         .path = "query1.subquery_id[example].name",
         .expected_status_code = IsStatusInvalidArgument()},
        {.result = ParseTextProtoOrDie(R"pb(
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
         )pb"),
         .path = "query1.subquery_id[3].name",
         .expected_status_code = IsStatusNotFound()},
        {.result = ParseTextProtoOrDie(R"pb(
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
         )pb"),
         .path = "query1.subquery_id[-1].name",
         .expected_status_code = IsStatusNotFound()},
        {.result = ParseTextProtoOrDie(R"pb(
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
                         value { identifier {} }
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
                         value { identifier {} }
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
         )pb"),
         .path = "query1.subquery_id[_id_={_local_devpath_:/"
                 "phys,_machine_devpath_:}].name",
         .expected_status_code = IsStatusNotFound()},
        {.result = ParseTextProtoOrDie(R"pb(
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
                         value {}
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
                         value {}
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
         )pb"),
         .path = "query1.subquery_id[_id_={_local_devpath_:/"
                 "phys,_machine_devpath_:}].name",
         .expected_status_code = IsStatusNotFound()},
    }));

using GetMutableQueryValueFromResultSuccessTest =
    TestWithParam<GetMutableQueryValueFromResultTestCase>;

TEST_P(GetMutableQueryValueFromResultSuccessTest, SuccessResponses) {
  GetMutableQueryValueFromResultTestCase test_cases = GetParam();

  absl::StatusOr<QueryValue*> value =
      GetMutableQueryValueFromResult(test_cases.result, test_cases.path);
  EXPECT_THAT(value,
              IsOkAndHolds(ecclesia::EqualsProto(test_cases.expected_value)));
}

INSTANTIATE_TEST_SUITE_P(
    GetMutableQueryValueFromResultTests,
    GetMutableQueryValueFromResultSuccessTest,
    ValuesIn<GetMutableQueryValueFromResultTestCase>({
        {
            .result = ParseTextProtoOrDie(R"pb(
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
            )pb"),
            .path = "query1.subquery_id1.name1",
            .expected_value =
                ParseTextProtoOrDie(R"pb(string_value: "value1")pb"),
        },
        {
            .result = ParseTextProtoOrDie(R"pb(
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
            )pb"),
            .path = "query1.subquery_id1._id_",
            .expected_value = ParseTextProtoOrDie(R"pb(identifier: {
                                                         local_devpath: "/phys"
                                                       })pb"),
        },
        {
            .result = ParseTextProtoOrDie(R"pb(
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
            )pb"),
            .path = "query1.subquery_id[name=value1].name",
            .expected_value =
                ParseTextProtoOrDie(R"pb(string_value: "value1")pb"),
        },
        {
            .result = ParseTextProtoOrDie(R"pb(
              query_id: "query1"
              data: {
                fields {
                  key: "subquery_id"
                  value {
                    list_value {
                      values {
                        subquery_value {
                          fields {
                            key: "int_value"
                            value { int_value: 1 }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "int_value"
                            value { int_value: 10 }
                          }
                        }
                      }
                    }
                  }
                }
              }
            )pb"),
            .path = "query1.subquery_id[int_value=1].int_value",
            .expected_value = ParseTextProtoOrDie(R"pb(int_value: 1)pb"),
        },
        {
            .result = ParseTextProtoOrDie(R"pb(
              query_id: "query1"
              data: {
                fields {
                  key: "subquery_id"
                  value {
                    list_value {
                      values {
                        subquery_value {
                          fields {
                            key: "double_value"
                            value { double_value: 1 }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "double_value"
                            value { double_value: 10 }
                          }
                        }
                      }
                    }
                  }
                }
              }
            )pb"),
            .path = "query1.subquery_id[double_value=1].double_value",
            .expected_value = ParseTextProtoOrDie(R"pb(double_value: 1)pb"),
        },
        {
            .result = ParseTextProtoOrDie(R"pb(
              query_id: "query1"
              data: {
                fields {
                  key: "subquery_id"
                  value {
                    list_value {
                      values {
                        subquery_value {
                          fields {
                            key: "bool_value"
                            value { bool_value: true }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "bool_value"
                            value { bool_value: false }
                          }
                        }
                      }
                    }
                  }
                }
              }
            )pb"),
            .path = "query1.subquery_id[bool_value=true].bool_value",
            .expected_value = ParseTextProtoOrDie(R"pb(bool_value: true)pb"),
        },
        {
            .result = ParseTextProtoOrDie(R"pb(
              query_id: "query1"
              data: {
                fields {
                  key: "subquery_id"
                  value {
                    list_value {
                      values {
                        subquery_value {
                          fields {
                            key: "timestamp_value"
                            value {
                              timestamp_value { seconds: 1708980000 nanos: 0 }
                            }
                          }
                        }
                      }
                      values {
                        subquery_value {
                          fields {
                            key: "timestamp_value"
                            value {
                              timestamp_value { seconds: 2708980000 nanos: 0 }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            )pb"),
            .path = "query1.subquery_id[timestamp_value=2024-02-26T20:40:00Z]."
                    "timestamp_value",
            .expected_value = ParseTextProtoOrDie(R"pb(timestamp_value: {
                                                         seconds: 1708980000
                                                         nanos: 0
                                                       })pb"),
        },
        {
            .result = ParseTextProtoOrDie(R"pb(
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
                            value {
                              identifier { local_devpath: "/phys/something" }
                            }
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
            )pb"),
            .path = "query1.subquery_id[_id_={_local_devpath_:/phys}].name",
            .expected_value =
                ParseTextProtoOrDie(R"pb(string_value: "value1")pb"),
        },
        {
            .result = ParseTextProtoOrDie(R"pb(
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
                            value {
                              identifier {
                                local_devpath: "",
                                machine_devpath: "/phys"
                              }
                            }
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
                            value {
                              identifier {
                                local_devpath: "/phys",
                                machine_devpath: "/phys"
                              }
                            }
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
            )pb"),
            .path = "query1.subquery_id[_id_={_local_devpath_:/"
                    "phys,_machine_devpath_:/phys}].name",
            .expected_value =
                ParseTextProtoOrDie(R"pb(string_value: "value2")pb"),
        },
        {
            .result = ParseTextProtoOrDie(R"pb(
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
                            value {
                              identifier {
                                local_devpath: "/phys",
                                machine_devpath: ""
                              }
                            }
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
                            value {
                              identifier {
                                local_devpath: "/phys",
                                machine_devpath: "/phys"
                              }
                            }
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
            )pb"),
            .path = "query1.subquery_id[_id_={_local_devpath_:/"
                    "phys,_machine_devpath_:/phys}].name",
            .expected_value =
                ParseTextProtoOrDie(R"pb(string_value: "value2")pb"),
        },
        {
            .result = ParseTextProtoOrDie(R"pb(
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
                            value {
                              identifier {
                                local_devpath: "/phys",
                                machine_devpath: "/phys"
                              }
                            }
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
                            value {
                              identifier { local_devpath: "/phys/something" }
                            }
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
            )pb"),
            .path = "query1.subquery_id[_id_={_local_devpath_:/"
                    "phys,_machine_devpath_:/phys}].name",
            .expected_value =
                ParseTextProtoOrDie(R"pb(string_value: "value1")pb"),
        },
        {
            .result = ParseTextProtoOrDie(R"pb(
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
            )pb"),
            .path = "query1.subquery_id[name=value1].name",
            .expected_value =
                ParseTextProtoOrDie(R"pb(string_value: "value1")pb"),
        },
        {
            .result = ParseTextProtoOrDie(R"pb(
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
            )pb"),
            .path = "query1.subquery_id[0].name",
            .expected_value =
                ParseTextProtoOrDie(R"pb(string_value: "value1")pb"),
        },
    }));

struct RemoveQueryValueFromResultTestCase {
  QueryResult result;
  std::string path;
  internal_status::IsStatusPolyMatcher expected_status_code = IsOk();
  QueryResult expected_output;
};

using RemoveQueryValueFromResultFailureTest =
    TestWithParam<RemoveQueryValueFromResultTestCase>;

TEST_P(RemoveQueryValueFromResultFailureTest, ReturnNullptrRespoonse) {
  RemoveQueryValueFromResultTestCase test_cases = GetParam();
  EXPECT_THAT(RemoveQueryValueFromResult(test_cases.result, test_cases.path),
              test_cases.expected_status_code);
}

INSTANTIATE_TEST_SUITE_P(
    RemoveQueryValueFromResultTests, RemoveQueryValueFromResultFailureTest,
    ValuesIn<RemoveQueryValueFromResultTestCase>({
        {.result = ParseTextProtoOrDie(R"pb(
           query_id: "query1"
         )pb"),
         .path = "",
         .expected_status_code = IsStatusInvalidArgument()},
        {.result = ParseTextProtoOrDie(R"pb(
           query_id: "query1"
         )pb"),
         .path = "query2",
         .expected_status_code = IsStatusInvalidArgument()},
        {.result = ParseTextProtoOrDie(R"pb(
           query_id: "query1"
         )pb"),
         .path = "query1",
         .expected_status_code = IsStatusInvalidArgument()},
        {.result = ParseTextProtoOrDie(R"pb(
           query_id: "query1"
         )pb"),
         .path = "query1.subquery_id",
         .expected_status_code = IsStatusInvalidArgument()},
        {.result = ParseTextProtoOrDie(R"pb(
           query_id: "query1"
           data: {}
         )pb"),
         .path = "query1.subquery_id",
         .expected_status_code = IsStatusInvalidArgument()},
        {.result = ParseTextProtoOrDie(R"pb(
           query_id: "query1"
           data: {
             fields {
               key: "subquery_id"
               value { string_value: "value1" }
             }
           }
         )pb"),
         .path = "query1.subquery_id[0].name",
         .expected_status_code = IsStatusInvalidArgument()},
        {.result = ParseTextProtoOrDie(R"pb(
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
         )pb"),
         .path = "query1.subquery_id2",
         .expected_status_code = IsStatusNotFound()},
        {.result = ParseTextProtoOrDie(R"pb(
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
         )pb"),
         .path = "query1.subquery_id1.name2",
         .expected_status_code = IsStatusNotFound()},
        {.result = ParseTextProtoOrDie(R"pb(
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
         )pb"),
         .path = "query1.subquery_id.name.name",
         .expected_status_code = IsStatusNotFound()},
        {.result = ParseTextProtoOrDie(R"pb(
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
         )pb"),
         .path = "query1.subquery_id[name==].name",
         .expected_status_code = IsStatusInvalidArgument()},
        {.result = ParseTextProtoOrDie(R"pb(
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
         )pb"),
         .path = "query1.subquery_id[name=no_value].name",
         .expected_status_code = IsStatusNotFound()},
        {.result = ParseTextProtoOrDie(R"pb(
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
         )pb"),
         .path = "query1.subquery_id.0.name",
         .expected_status_code = IsStatusNotFound()},
        {.result = ParseTextProtoOrDie(R"pb(
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
         )pb"),
         .path = "query1.subquery_id[example].name",
         .expected_status_code = IsStatusInvalidArgument()},
        {.result = ParseTextProtoOrDie(R"pb(
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
         )pb"),
         .path = "query1.subquery_id[3].name",
         .expected_status_code = IsStatusNotFound()},
        {.result = ParseTextProtoOrDie(R"pb(
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
         )pb"),
         .path = "query1.subquery_id[-1].name",
         .expected_status_code = IsStatusNotFound()},
    }));

using RemoveQueryValueFromResultSuccessTest =
    TestWithParam<RemoveQueryValueFromResultTestCase>;

TEST_P(RemoveQueryValueFromResultSuccessTest, SuccessResponses) {
  RemoveQueryValueFromResultTestCase test_cases = GetParam();
  QueryResult out_result = std::move(test_cases.result);
  EXPECT_THAT(RemoveQueryValueFromResult(out_result, test_cases.path), IsOk());
  EXPECT_THAT(out_result, EqualsProto(test_cases.expected_output));
}

INSTANTIATE_TEST_SUITE_P(
    RemoveQueryValueFromResultTests, RemoveQueryValueFromResultSuccessTest,
    ValuesIn<RemoveQueryValueFromResultTestCase>({
        {
            .result = ParseTextProtoOrDie(R"pb(
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
            )pb"),
            .path = "query1.subquery_id1.name1",
            .expected_output = ParseTextProtoOrDie(
                R"pb(query_id: "query1"
                     data {
                       fields {
                         key: "subquery_id1"
                         value {
                           subquery_value {
                             fields {
                               key: "_id_"
                               value { identifier { local_devpath: "/phys" } }
                             }
                           }
                         }
                       }
                     })pb"),
        },
        {
            .result = ParseTextProtoOrDie(R"pb(
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
            )pb"),
            .path = "query1.subquery_id1._id_",
            .expected_output = ParseTextProtoOrDie(
                R"pb(query_id: "query1"
                     data {
                       fields {
                         key: "subquery_id1"
                         value {
                           subquery_value {
                             fields {
                               key: "name1"
                               value { string_value: "value1" }
                             }
                           }
                         }
                       }
                     })pb"),
        },
        {
            .result = ParseTextProtoOrDie(R"pb(
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
            )pb"),
            .path = "query1.subquery_id[name=value1].name",
            .expected_output = ParseTextProtoOrDie(
                R"pb(query_id: "query1"
                     data {
                       fields {
                         key: "subquery_id"
                         value {
                           list_value {
                             values { subquery_value {} }
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
                     })pb"),
        },
        {
            .result = ParseTextProtoOrDie(R"pb(
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
                            value {
                              identifier { local_devpath: "/phys/something" }
                            }
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
            )pb"),
            .path = "query1.subquery_id[_id_={_local_devpath_:/phys}].name",
            .expected_output = ParseTextProtoOrDie(
                R"pb(query_id: "query1"
                     data {
                       fields {
                         key: "subquery_id"
                         value {
                           list_value {
                             values {
                               subquery_value {
                                 fields {
                                   key: "_id_"
                                   value {
                                     identifier { local_devpath: "/phys" }
                                   }
                                 }
                               }
                             }
                             values {
                               subquery_value {
                                 fields {
                                   key: "_id_"
                                   value {
                                     identifier {
                                       local_devpath: "/phys/something"
                                     }
                                   }
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
                     })pb"),
        },
        {
            .result = ParseTextProtoOrDie(R"pb(
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
                            value {
                              identifier { local_devpath: "/phys/something" }
                            }
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
            )pb"),
            .path = "query1.subquery_id[_id_={_local_devpath_:/phys}]",
            .expected_output = ParseTextProtoOrDie(
                R"pb(query_id: "query1"
                     data {
                       fields {
                         key: "subquery_id"
                         value {
                           list_value {
                             values {
                               subquery_value {
                                 fields {
                                   key: "_id_"
                                   value {
                                     identifier {
                                       local_devpath: "/phys/something"
                                     }
                                   }
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
                     })pb"),
        },
        {
            .result = ParseTextProtoOrDie(R"pb(
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
            )pb"),
            .path = "query1.subquery_id[name=value1].name",
            .expected_output = ParseTextProtoOrDie(
                R"pb(query_id: "query1"
                     data {
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
                             values { subquery_value {} }
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
                     })pb"),
        },
        {
            .result = ParseTextProtoOrDie(R"pb(
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
            )pb"),
            .path = "query1.subquery_id[0]",
            .expected_output = ParseTextProtoOrDie(
                R"pb(query_id: "query1"
                     data {
                       fields {
                         key: "subquery_id"
                         value {
                           list_value {
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
                     })pb"),
        },
    }));

}  // namespace
}  // namespace ecclesia
