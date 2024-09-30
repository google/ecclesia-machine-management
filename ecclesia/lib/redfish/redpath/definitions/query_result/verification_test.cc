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

#include "ecclesia/lib/redfish/redpath/definitions/query_result/verification.h"

#include <string>
#include <vector>

#include "google/protobuf/timestamp.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result_verification.pb.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::testing::ContainsRegex;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::TestWithParam;
using ::testing::UnorderedElementsAreArray;
using ::testing::Values;

// Struct to hold the scalar query values to be compared in various
// parameterized tests.
struct QueryValueInputs {
  QueryValue query_value_a;
  QueryValue query_value_b;
};

// In Equal Values Test, query_value_a and query_value_b are different values.
using EqualValuesTest = TestWithParam<QueryValueInputs>;
// In Greater/Lesser Values Test, query_value_a is greater than query_value_b.
using GreaterLesserValuesTest = TestWithParam<QueryValueInputs>;

TEST_P(EqualValuesTest, EqualSuccess) {
  const QueryValueInputs& test_case = GetParam();
  std::vector<std::string> errors;
  ASSERT_THAT(
      CompareQueryValues(test_case.query_value_a, test_case.query_value_a,
                         Verification::COMPARE_EQUAL, errors),
      IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST_P(EqualValuesTest, EqualFailure) {
  const QueryValueInputs& test_case = GetParam();
  std::vector<std::string> errors;
  ASSERT_THAT(
      CompareQueryValues(test_case.query_value_a, test_case.query_value_b,
                         Verification::COMPARE_EQUAL, errors),
      IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0], HasSubstr("Failed equality check"));
}

TEST_P(EqualValuesTest, NotEqualSuccess) {
  const QueryValueInputs& test_case = GetParam();
  std::vector<std::string> errors;
  ASSERT_THAT(
      CompareQueryValues(test_case.query_value_a, test_case.query_value_b,
                         Verification::COMPARE_NOT_EQUAL, errors),
      IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST_P(EqualValuesTest, NotEqualFailure) {
  const QueryValueInputs& test_case = GetParam();
  std::vector<std::string> errors;
  ASSERT_THAT(
      CompareQueryValues(test_case.query_value_a, test_case.query_value_a,
                         Verification::COMPARE_NOT_EQUAL, errors),
      IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0], HasSubstr("Failed inequality check"));
}

INSTANTIATE_TEST_SUITE_P(
    EqualVales, EqualValuesTest,
    Values(
        QueryValueInputs{
            .query_value_a = ParseTextProtoOrDie(R"pb(int_value: 1)pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(int_value: 2)pb"),
        },
        QueryValueInputs{
            .query_value_a = ParseTextProtoOrDie(R"pb(double_value: 1.23)pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(double_value: 3.14)pb"),
        },
        QueryValueInputs{
            .query_value_a = ParseTextProtoOrDie(R"pb(string_value: "foo")pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(string_value: "bar")pb"),
        },
        QueryValueInputs{
            .query_value_a = ParseTextProtoOrDie(R"pb(bool_value: true)pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(bool_value: false)pb"),
        },
        QueryValueInputs{
            .query_value_a = ParseTextProtoOrDie(R"pb(timestamp_value {
                                                        seconds: 1694462500
                                                        nanos: 0
                                                      })pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(timestamp_value {
                                                        seconds: 1694462400
                                                        nanos: 0
                                                      })pb"),
        },
        QueryValueInputs{
            .query_value_a =
                ParseTextProtoOrDie(R"pb(identifier {
                                           local_devpath: "/phys/IO0"
                                           machine_devpath: "/phys/PE0/IO0"
                                         })pb"),
            .query_value_b =
                ParseTextProtoOrDie(R"pb(identifier {
                                           local_devpath: "/phys/"
                                           machine_devpath: "/phys/PE0"
                                         })pb"),
        },
        QueryValueInputs{
            .query_value_a = ParseTextProtoOrDie(R"pb(raw_data {
                                                        raw_string_value: "foo"
                                                      })pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(raw_data {
                                                        raw_string_value: "bar"
                                                      })pb"),
        },
        QueryValueInputs{
            .query_value_a = ParseTextProtoOrDie(R"pb(raw_data {
                                                        raw_bytes_value: "foo"
                                                      })pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(raw_data {
                                                        raw_bytes_value: "bar"
                                                      })pb"),
        }));

TEST(CompareQueryValuesTest, RawDataDifferentTypes) {
  QueryValue qv_a =
      ParseTextProtoOrDie(R"pb(raw_data { raw_string_value: "foo" })pb");
  QueryValue qv_b =
      ParseTextProtoOrDie(R"pb(raw_data { raw_bytes_value: "foo" })pb");
  std::vector<std::string> errors;
  EXPECT_THAT(
      CompareQueryValues(qv_a, qv_b, Verification::COMPARE_EQUAL, errors),
      IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(CompareQueryValuesTest, RawDataValueNotSet) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(raw_data {})pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(raw_data {})pb");
  std::vector<std::string> errors;
  EXPECT_THAT(
      CompareQueryValues(qv_a, qv_b, Verification::COMPARE_EQUAL, errors),
      IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(CompareQueryValuesTest, DifferentTypes) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(bool_value: false)pb");
  std::vector<std::string> errors;
  EXPECT_THAT(
      CompareQueryValues(qv_a, qv_b, Verification::COMPARE_EQUAL, errors),
      IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(CompareQueryValuesTest, QueryValueNotSet) {
  QueryValue qv_a;
  QueryValue qv_b;
  std::vector<std::string> errors;
  EXPECT_THAT(
      CompareQueryValues(qv_a, qv_b, Verification::COMPARE_EQUAL, errors),
      IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(CompareQueryValuesTest, UnknownOperation) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  std::vector<std::string> errors;
  EXPECT_THAT(
      CompareQueryValues(qv_a, qv_b, Verification::COMPARE_UNKNOWN, errors),
      IsStatusInternal());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(CompareQueryValuesTest, NonScalarValues) {
  QueryValue qv_a =
      ParseTextProtoOrDie(R"pb(list_value { values { int_value: 1 } })pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(subquery_value {
                                               fields {
                                                 key: "foo"
                                                 value: { int_value: 1 }
                                               }
                                             })pb");
  std::vector<std::string> errors;
  EXPECT_THAT(
      CompareQueryValues(qv_a, qv_a, Verification::COMPARE_EQUAL, errors),
      IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
  EXPECT_THAT(
      CompareQueryValues(qv_a, qv_b, Verification::COMPARE_EQUAL, errors),
      IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
}

// Struct to hold the subquery values to be compared in various
// parameterized tests.
struct QueryValueInputsWithExpectations {
  QueryValue query_value_a;
  QueryValue query_value_b;
  QueryResultDataVerification data_compare;
  ListValueVerification list_compare;
  internal_status::IsStatusPolyMatcher expected_status;
  std::vector<std::string> expected_errors;
};

// In Subquery Values Test, query_value_a and query_value_b are different
// values.
using SubqueryValueTests = TestWithParam<QueryValueInputsWithExpectations>;

TEST_P(SubqueryValueTests, CompareSubqueryValues) {
  const QueryValueInputsWithExpectations& test_case = GetParam();
  std::vector<std::string> errors;
  EXPECT_THAT(CompareSubqueryValues(test_case.query_value_a.subquery_value(),
                                    test_case.query_value_b.subquery_value(),
                                    test_case.data_compare, errors),
              test_case.expected_status);
  EXPECT_THAT(errors, UnorderedElementsAreArray(test_case.expected_errors));
}

INSTANTIATE_TEST_SUITE_P(
    SubqueryValueTests, SubqueryValueTests,
    Values(
        QueryValueInputsWithExpectations{
            .query_value_a = ParseTextProtoOrDie(R"pb(subquery_value {})pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(subquery_value {
                                                        fields {
                                                          key: "foo"
                                                          value: {
                                                            int_value: 1
                                                          }
                                                        }
                                                      })pb"),
            .data_compare = ParseTextProtoOrDie(R"pb(
              fields {
                key: "foo"
                value { verify { comparison: COMPARE_EQUAL } }
              }
            )pb"),
            .expected_status = IsOk(),
            .expected_errors = {"Missing property foo in valueA"},
        },
        QueryValueInputsWithExpectations{
            .query_value_a = ParseTextProtoOrDie(R"pb(subquery_value {})pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(subquery_value {})pb"),
            .data_compare = ParseTextProtoOrDie(R"pb(
              fields {
                key: "foo"
                value { verify { comparison: COMPARE_EQUAL } }
              }
            )pb"),
            .expected_status = IsOk(),
            .expected_errors = {"Missing property foo in valueA",
                                "Missing property foo in valueB"},
        },
        QueryValueInputsWithExpectations{
            .query_value_a = ParseTextProtoOrDie(R"pb(subquery_value {
                                                        fields {
                                                          key: "foo"
                                                          value: {
                                                            int_value: 1
                                                          }
                                                        }
                                                      })pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(subquery_value {
                                                        fields {
                                                          key: "foo"
                                                          value: {
                                                            string_value: "1"
                                                          }
                                                        }
                                                      })pb"),
            .data_compare = ParseTextProtoOrDie(R"pb(
              fields {
                key: "foo"
                value { verify { comparison: COMPARE_EQUAL } }
              }
            )pb"),
            .expected_status = IsStatusFailedPrecondition(),
            .expected_errors = {},
        },
        QueryValueInputsWithExpectations{
            .query_value_a = ParseTextProtoOrDie(R"pb(subquery_value {})pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(subquery_value {})pb"),
            .expected_status = IsOk(),
            .expected_errors = {},
        },
        QueryValueInputsWithExpectations{
            .query_value_a = ParseTextProtoOrDie(R"pb(subquery_value {
                                                        fields {
                                                          key: "foo"
                                                          value: {
                                                            int_value: 1
                                                          }
                                                        }
                                                      })pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(subquery_value {
                                                        fields {
                                                          key: "foo"
                                                          value: {
                                                            int_value: 1
                                                          }
                                                        }
                                                      })pb"),
            .data_compare = ParseTextProtoOrDie(
                R"pb(fields {
                       key: "foo"
                       value { verify { comparison: COMPARE_EQUAL } }
                     })pb"),
            .expected_status = IsOk(),
            .expected_errors = {},
        },
        QueryValueInputsWithExpectations{
            .query_value_a =
                ParseTextProtoOrDie(R"pb(subquery_value {
                                           fields {
                                             key: "foo"
                                             value: {
                                               subquery_value {
                                                 fields {
                                                   key: "bar"
                                                   value: { int_value: 1 }
                                                 }
                                               }
                                             }
                                           }
                                         })pb"),
            .query_value_b =
                ParseTextProtoOrDie(R"pb(subquery_value {
                                           fields {
                                             key: "foo"
                                             value: {
                                               subquery_value {
                                                 fields {
                                                   key: "bar"
                                                   value: { int_value: 1 }
                                                 }
                                               }
                                             }
                                           }
                                         })pb"),
            .data_compare = ParseTextProtoOrDie(
                R"pb(fields {
                       key: "foo"
                       value {
                         data_compare {
                           fields {
                             key: "bar"
                             value: { verify { comparison: COMPARE_EQUAL } }
                           }
                         }
                       }
                     })pb"),
            .expected_status = IsOk(),
            .expected_errors = {},
        },
        QueryValueInputsWithExpectations{
            .query_value_a = ParseTextProtoOrDie(
                R"pb(subquery_value {
                       fields {
                         key: "foo"
                         value: { list_value { values { int_value: 1 } } }
                       }
                     })pb"),
            .query_value_b = ParseTextProtoOrDie(
                R"pb(subquery_value {
                       fields {
                         key: "foo"
                         value: { list_value { values { int_value: 1 } } }
                       }
                     })pb"),
            .data_compare = ParseTextProtoOrDie(
                R"pb(fields {
                       key: "foo"
                       value {
                         list_compare {
                           verify { verify { comparison: COMPARE_EQUAL } }
                         }
                       }
                     })pb"),
            .expected_status = IsOk(),
            .expected_errors = {},
        }));

// In List Values Test, query_value_a and query_value_b are different values.
using ListValueTests = TestWithParam<QueryValueInputsWithExpectations>;
TEST_P(ListValueTests, CompareListValues) {
  const QueryValueInputsWithExpectations& test_case = GetParam();
  std::vector<std::string> errors;
  EXPECT_THAT(CompareListValues(test_case.query_value_a.list_value(),
                                test_case.query_value_b.list_value(),
                                test_case.list_compare, errors),
              test_case.expected_status);
  EXPECT_THAT(errors, UnorderedElementsAreArray(test_case.expected_errors));
}

INSTANTIATE_TEST_SUITE_P(
    CompareListValueTests, ListValueTests,
    Values(
        QueryValueInputsWithExpectations{
            .query_value_a =
                ParseTextProtoOrDie(R"pb(list_value {
                                           values {
                                             subquery_value {
                                               fields {
                                                 key: "foo"
                                                 value: { int_value: 1 }
                                               }
                                             }
                                           }
                                         })pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(list_value {})pb"),
            .expected_status = IsStatusInternal(),
            .expected_errors = {"Missing value in valueB with identifier "
                                "index=0"},
        },
        QueryValueInputsWithExpectations{
            .query_value_a = ParseTextProtoOrDie(R"pb(list_value {
                                                        values { list_value {} }
                                                      })pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(list_value {
                                                        values { list_value {} }
                                                      })pb"),
            .expected_status = IsStatusFailedPrecondition(),
            .expected_errors = {},
        },
        QueryValueInputsWithExpectations{
            .query_value_a = ParseTextProtoOrDie(R"pb(list_value {})pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(list_value {})pb"),
            .expected_status = IsOk(),
            .expected_errors = {},
        },
        QueryValueInputsWithExpectations{
            .query_value_a =
                ParseTextProtoOrDie(R"pb(list_value { values {} })pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(list_value {})pb"),
            .list_compare = ParseTextProtoOrDie(R"pb(identifiers: "foo")pb"),
            .expected_status = IsStatusInternal(),
            .expected_errors = {"Missing identifier in valueA: Identifiers are "
                                "only supported for subquery values"},
        },
        QueryValueInputsWithExpectations{
            .query_value_a = ParseTextProtoOrDie(
                R"pb(list_value { values { subquery_value {} } })pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(list_value {})pb"),
            .list_compare = ParseTextProtoOrDie(R"pb(identifiers: "foo")pb"),
            .expected_status = IsStatusInternal(),
            .expected_errors =
                {"Missing identifier in valueA: property foo is not present"},
        },
        QueryValueInputsWithExpectations{
            .query_value_a = ParseTextProtoOrDie(
                R"pb(list_value {
                       values {
                         subquery_value {
                           fields {
                             key: "foo"
                             value: { int_value: 1 }
                           }
                         }
                       }
                       values {
                         subquery_value {
                           fields {
                             key: "foo"
                             value: { int_value: 1 }
                           }
                         }
                       }
                     })pb"),
            .query_value_b =
                ParseTextProtoOrDie(R"pb(list_value {
                                           values {
                                             subquery_value {
                                               fields {
                                                 key: "foo"
                                                 value: { int_value: 1 }
                                               }
                                             }
                                           }
                                         })pb"),
            .list_compare = ParseTextProtoOrDie(R"pb(identifiers: "foo")pb"),
            .expected_status = IsStatusInternal(),
            .expected_errors = {"Duplicate identifier in valueA: foo=1"},
        },
        QueryValueInputsWithExpectations{
            .query_value_a = ParseTextProtoOrDie(R"pb(list_value {})pb"),
            .query_value_b =
                ParseTextProtoOrDie(R"pb(list_value {
                                           values {
                                             subquery_value {
                                               fields {
                                                 key: "foo"
                                                 value: { int_value: 1 }
                                               }
                                             }
                                           }
                                         })pb"),
            .list_compare = ParseTextProtoOrDie(R"pb(identifiers: "foo")pb"),
            .expected_status = IsStatusInternal(),
            .expected_errors =
                {"Missing value in valueA with identifier foo=1"},
        },
        QueryValueInputsWithExpectations{
            .query_value_a = ParseTextProtoOrDie(R"pb(list_value {
                                                        values { int_value: 1 }
                                                      })pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(list_value {
                                                        values { int_value: 1 }
                                                      })pb"),
            .list_compare = ParseTextProtoOrDie(
                R"pb(verify { verify { comparison: COMPARE_EQUAL } })pb"),
            .expected_status = IsOk(),
            .expected_errors = {},
        },
        QueryValueInputsWithExpectations{
            .query_value_a =
                ParseTextProtoOrDie(R"pb(list_value {
                                           values {
                                             subquery_value {
                                               fields {
                                                 key: "foo"
                                                 value: { int_value: 1 }
                                               }
                                             }
                                           }
                                         })pb"),
            .query_value_b =
                ParseTextProtoOrDie(R"pb(list_value {
                                           values {
                                             subquery_value {
                                               fields {
                                                 key: "foo"
                                                 value: { int_value: 1 }
                                               }
                                             }
                                           }
                                         })pb"),
            .expected_status = IsOk(),
            .expected_errors = {},
        },
        QueryValueInputsWithExpectations{
            .query_value_a =
                ParseTextProtoOrDie(R"pb(list_value {
                                           values {
                                             subquery_value {
                                               fields {
                                                 key: "foo"
                                                 value: { int_value: 1 }
                                               }
                                             }
                                           }
                                           values {
                                             subquery_value {
                                               fields {
                                                 key: "foo"
                                                 value: { int_value: 2 }
                                               }
                                             }
                                           }
                                         })pb"),
            .query_value_b =
                ParseTextProtoOrDie(R"pb(list_value {
                                           values {
                                             subquery_value {
                                               fields {
                                                 key: "foo"
                                                 value: { int_value: 2 }
                                               }
                                             }
                                           }
                                           values {
                                             subquery_value {
                                               fields {
                                                 key: "foo"
                                                 value: { int_value: 1 }
                                               }
                                             }
                                           }
                                         })pb"),
            .list_compare = ParseTextProtoOrDie(R"pb(identifiers: "foo")pb"),
            .expected_status = IsOk(),
            .expected_errors = {},
        },
        QueryValueInputsWithExpectations{
            .query_value_a =
                ParseTextProtoOrDie(R"pb(list_value {
                                           values {
                                             subquery_value {
                                               fields {
                                                 key: "_id_"
                                                 value {
                                                   identifier {
                                                     local_devpath: "/phys"
                                                     machine_devpath: "/phys"
                                                   }
                                                 }
                                               }
                                             }
                                           }
                                         })pb"),
            .query_value_b =
                ParseTextProtoOrDie(R"pb(list_value {
                                           values {
                                             subquery_value {
                                               fields {
                                                 key: "_id_"
                                                 value {
                                                   identifier {
                                                     local_devpath: "/phys"
                                                     machine_devpath: "/phys"
                                                   }
                                                 }
                                               }
                                             }
                                           }
                                         })pb"),
            .list_compare = ParseTextProtoOrDie(R"pb(identifiers: "_id_")pb"),
            .expected_status = IsOk(),
            .expected_errors = {},
        }));
TEST(CompareListValuesTest, MisAlignedListValues) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(list_value {
                                               values {
                                                 subquery_value {
                                                   fields {
                                                     key: "foo"
                                                     value: { int_value: 1 }
                                                   }
                                                 }
                                               }
                                               values {
                                                 subquery_value {
                                                   fields {
                                                     key: "foo"
                                                     value: { int_value: 2 }
                                                   }
                                                 }
                                               }
                                             })pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(list_value {
                                               values {
                                                 subquery_value {
                                                   fields {
                                                     key: "foo"
                                                     value: { int_value: 2 }
                                                   }
                                                 }
                                               }
                                               values {
                                                 subquery_value {
                                                   fields {
                                                     key: "foo"
                                                     value: { int_value: 1 }
                                                   }
                                                 }
                                               }
                                             })pb");

  ListValueVerification verification = ParseTextProtoOrDie(R"pb(
    verify {
      data_compare {
        fields {
          key: "foo"
          value: { verify { comparison: COMPARE_EQUAL } }
        }
      }
    }
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareListValues(qv_a.list_value(), qv_b.list_value(),
                                verification, errors),
              IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0],
              ContainsRegex("Failed equality check, valueA: '.', valueB: '.'"));
}

TEST(CompareQueryResultsTest, QueryIdsMismatchForAndB) {
  QueryResult qr_a = ParseTextProtoOrDie(R"pb(
    query_id: "query_a"
  )pb");
  QueryResult qr_b = ParseTextProtoOrDie(R"pb(
    query_id: "query_b"
  )pb");
  QueryResultVerification verification;
  std::vector<std::string> errors;
  EXPECT_THAT(CompareQueryResults(qr_a, qr_b, verification, errors),
              IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(CompareQueryResultsTest, QueryIdsMismatchForVerification) {
  QueryResult qr = ParseTextProtoOrDie(R"pb(
    query_id: "query_a"
  )pb");
  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_b"
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareQueryResults(qr, qr, verification, errors),
              IsStatusInvalidArgument());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(CompareQueryResultsTest, SuccessSimpleQueryResult) {
  QueryResult qr = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 0 }
      }
    }
  )pb");
  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value { verify { comparison: COMPARE_EQUAL } }
      }
    }
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareQueryResults(qr, qr, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(CompareQueryResultsTest, SuccessSubqueryQueryResult) {
  QueryResult qr = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value {
          subquery_value {
            fields {
              key: "key1"
              value { int_value: 0 }
            }
          }
        }
      }
    }
  )pb");
  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          data_compare {
            fields {
              key: "key1"
              value { verify { comparison: COMPARE_EQUAL } }
            }
          }
        }
      }
    }
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareQueryResults(qr, qr, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(CompareQueryResultsTest, SuccessListQueryResult) {
  QueryResult qr = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { list_value { values { int_value: 0 } } }
      }
    }
  )pb");
  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          list_compare { verify { verify { comparison: COMPARE_EQUAL } } }
        }
      }
    }
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareQueryResults(qr, qr, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyQueryValueTest, Unimplemented) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  QueryValueVerification verification;
  std::vector<std::string> errors;
  EXPECT_THAT(VerifyQueryValue(qv_a, verification, errors),
              IsStatusUnimplemented());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyListValueTest, Unimplemented) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(list_value {
                                               values { int_value: 1 }
                                               values { int_value: 2 }
                                             })pb");
  ListValueVerification verification;
  std::vector<std::string> errors;
  EXPECT_THAT(VerifyListValue(qv_a, verification, errors),
              IsStatusUnimplemented());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifySubqueryValueTest, Unimplemented) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(subquery_value {
                                               fields {
                                                 key: "foo"
                                                 value: { int_value: 1 }
                                               }
                                             })pb");
  QueryResultDataVerification verification;
  std::vector<std::string> errors;
  EXPECT_THAT(VerifySubqueryValue(qv_a, verification, errors),
              IsStatusUnimplemented());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyQueryResultTest, Unimplemented) {
  QueryResult qr_a = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 0 }
      }
    }
  )pb");
  QueryResultVerification verification;
  std::vector<std::string> errors;
  EXPECT_THAT(VerifyQueryResult(qr_a, verification, errors),
              IsStatusUnimplemented());
  ASSERT_THAT(errors, IsEmpty());
}

}  // namespace
}  // namespace ecclesia
