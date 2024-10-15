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
#include "absl/strings/str_format.h"
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
      IsOk());
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
            .query_value_a = ParseTextProtoOrDie(R"pb(subquery_value {
                                                        fields {
                                                          key: "foo"
                                                          value: {
                                                            int_value: 1
                                                          }
                                                        }
                                                      })pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(subquery_value {})pb"),
            .data_compare = ParseTextProtoOrDie(R"pb(
              fields {
                key: "foo"
                value { verify { comparison: COMPARE_EQUAL } }
              }
            )pb"),
            .expected_status = IsOk(),
            .expected_errors = {"Missing property foo in valueB"},
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
            .query_value_a = ParseTextProtoOrDie(R"pb(list_value {})pb"),
            .query_value_b = ParseTextProtoOrDie(
                R"pb(list_value { values { subquery_value {} } })pb"),
            .list_compare = ParseTextProtoOrDie(R"pb(identifiers: "foo")pb"),
            .expected_status = IsStatusInternal(),
            .expected_errors =
                {"Missing identifier in valueB: property foo is not present"},
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
            .list_compare = ParseTextProtoOrDie(R"pb(identifiers: "foo")pb"),
            .expected_status = IsStatusInternal(),
            .expected_errors =
                {"Missing value in valueB with identifier foo=1"},
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
                                               fields {
                                                 key: "foo"
                                                 value: { int_value: 3 }
                                               }
                                             }
                                           }
                                         })pb"),
            .list_compare = ParseTextProtoOrDie(R"pb(identifiers: "foo")pb"),
            .expected_status = IsStatusInternal(),
            .expected_errors =
                {"Missing value in valueA with identifier foo=3",
                 "Missing value in valueB with identifier foo=2"},
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
  ASSERT_THAT(errors, SizeIs(2));
  EXPECT_THAT(
      errors,
      UnorderedElementsAreArray(
          {"(Path: index=1.foo) Failed equality check, valueA: '2', "
           "valueB: '1'",
           "(Path: index=0.foo) Failed equality check, valueA: '1', valueB: "
           "'2'"}));
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

TEST(VerifyQueryValueTest, EmtpyVerification) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  QueryValueVerification verification;
  std::vector<std::string> errors;
  EXPECT_THAT(VerifyQueryValue(qv, verification, errors),
              IsStatusInvalidArgument());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyQueryValueTest, VerificationWithNoValidation) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  QueryValueVerification verification = ParseTextProtoOrDie(R"pb(verify {})pb");
  std::vector<std::string> errors;
  EXPECT_THAT(VerifyQueryValue(qv, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyQueryValueTest, VerificationWithNoOperandas) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  QueryValueVerification verification =
      ParseTextProtoOrDie(R"pb(verify { validation {} })pb");
  std::vector<std::string> errors;
  EXPECT_THAT(VerifyQueryValue(qv, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyQueryValueTest, VerificationWithJustOperands) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  QueryValueVerification verification = ParseTextProtoOrDie(
      R"pb(verify { validation { operation: OPERATION_GREATER_THAN } })pb");
  std::vector<std::string> errors;
  EXPECT_THAT(VerifyQueryValue(qv, verification, errors), IsStatusInternal());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyQueryValueTest, VerificationWithMismatchedOperandaType) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  QueryValueVerification verification =
      ParseTextProtoOrDie(R"pb(verify {
                                 validation {
                                   operands { string_value: "foo" }
                                   operation: OPERATION_GREATER_THAN
                                 }
                               })pb");
  std::vector<std::string> errors;
  EXPECT_THAT(VerifyQueryValue(qv, verification, errors),
              IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
}

struct UnsupportedValidationTestCase {
  QueryValue::KindCase value_type;
  std::vector<Verification::Validation::Operation> operations;
};
using UnsupportedValidationTest = TestWithParam<UnsupportedValidationTestCase>;

TEST_P(UnsupportedValidationTest, UnsupportedOperation) {
  const UnsupportedValidationTestCase& test_case = GetParam();

  QueryValue query_value;
  switch (test_case.value_type) {
    case QueryValue::kIntValue:
      query_value.set_int_value(1);
      break;
    case QueryValue::kDoubleValue:
      query_value.set_double_value(1.0);
      break;
    case QueryValue::kBoolValue:
      query_value.set_bool_value(true);
      break;
    case QueryValue::kStringValue:
      query_value.set_string_value("foo");
      break;
    case QueryValue::kTimestampValue:
      query_value.mutable_timestamp_value()->set_seconds(1);
      break;
    case QueryValue::kIdentifier:
      query_value.mutable_identifier()->set_local_devpath("foo");
      break;
    case QueryValue::kRawData:
      query_value.mutable_raw_data()->set_raw_string_value("foo");
      break;
    default:
      break;
  };

  std::vector<std::string> errors;
  for (Verification::Validation::Operation operation : test_case.operations) {
    QueryValueVerification verification;
    Verification::Validation& validation =
        *verification.mutable_verify()->mutable_validation()->Add();
    validation.set_operation(operation);
    *validation.add_operands() = query_value;
    ASSERT_THAT(VerifyQueryValue(query_value, verification, errors),
                IsStatusInternal());
    ASSERT_THAT(errors, IsEmpty());
  }
}

INSTANTIATE_TEST_SUITE_P(
    UnsupportedValidationTest, UnsupportedValidationTest,
    Values(
        UnsupportedValidationTestCase{
            .value_type = QueryValue::kIntValue,
            .operations =
                {
                    Verification::Validation::OPERATION_UNKNOWN,
                    Verification::Validation::OPERATION_STRING_CONTAINS,
                    Verification::Validation::OPERATION_STRING_NOT_CONTAINS,
                    Verification::Validation::OPERATION_STRING_STARTS_WITH,
                    Verification::Validation::OPERATION_STRING_NOT_STARTS_WITH,
                    Verification::Validation::OPERATION_STRING_ENDS_WITH,
                    Verification::Validation::OPERATION_STRING_NOT_ENDS_WITH,
                    Verification::Validation::OPERATION_STRING_REGEX_MATCH,
                    Verification::Validation::OPERATION_STRING_NOT_REGEX_MATCH,
                }},
        UnsupportedValidationTestCase{
            .value_type = QueryValue::kDoubleValue,
            .operations =
                {
                    Verification::Validation::OPERATION_UNKNOWN,
                    Verification::Validation::OPERATION_STRING_CONTAINS,
                    Verification::Validation::OPERATION_STRING_NOT_CONTAINS,
                    Verification::Validation::OPERATION_STRING_STARTS_WITH,
                    Verification::Validation::OPERATION_STRING_NOT_STARTS_WITH,
                    Verification::Validation::OPERATION_STRING_ENDS_WITH,
                    Verification::Validation::OPERATION_STRING_NOT_ENDS_WITH,
                    Verification::Validation::OPERATION_STRING_REGEX_MATCH,
                    Verification::Validation::OPERATION_STRING_NOT_REGEX_MATCH,
                }},
        UnsupportedValidationTestCase{
            .value_type = QueryValue::kStringValue,
            .operations =
                {
                    Verification::Validation::OPERATION_UNKNOWN,
                    Verification::Validation::OPERATION_GREATER_THAN,
                    Verification::Validation::OPERATION_GREATER_THAN_OR_EQUAL,
                    Verification::Validation::OPERATION_LESS_THAN,
                    Verification::Validation::OPERATION_LESS_THAN_OR_EQUAL,
                }},
        UnsupportedValidationTestCase{
            .value_type = QueryValue::kBoolValue,
            .operations =
                {
                    Verification::Validation::OPERATION_UNKNOWN,
                    Verification::Validation::OPERATION_GREATER_THAN,
                    Verification::Validation::OPERATION_GREATER_THAN_OR_EQUAL,
                    Verification::Validation::OPERATION_LESS_THAN,
                    Verification::Validation::OPERATION_LESS_THAN_OR_EQUAL,
                    Verification::Validation::OPERATION_STRING_CONTAINS,
                    Verification::Validation::OPERATION_STRING_NOT_CONTAINS,
                    Verification::Validation::OPERATION_STRING_STARTS_WITH,
                    Verification::Validation::OPERATION_STRING_NOT_STARTS_WITH,
                    Verification::Validation::OPERATION_STRING_ENDS_WITH,
                    Verification::Validation::OPERATION_STRING_NOT_ENDS_WITH,
                    Verification::Validation::OPERATION_STRING_REGEX_MATCH,
                    Verification::Validation::OPERATION_STRING_NOT_REGEX_MATCH,
                }},
        UnsupportedValidationTestCase{
            .value_type = QueryValue::kTimestampValue,
            .operations =
                {
                    Verification::Validation::OPERATION_UNKNOWN,
                    Verification::Validation::OPERATION_STRING_CONTAINS,
                    Verification::Validation::OPERATION_STRING_NOT_CONTAINS,
                    Verification::Validation::OPERATION_STRING_STARTS_WITH,
                    Verification::Validation::OPERATION_STRING_NOT_STARTS_WITH,
                    Verification::Validation::OPERATION_STRING_ENDS_WITH,
                    Verification::Validation::OPERATION_STRING_NOT_ENDS_WITH,
                    Verification::Validation::OPERATION_STRING_REGEX_MATCH,
                    Verification::Validation::OPERATION_STRING_NOT_REGEX_MATCH,
                }},
        UnsupportedValidationTestCase{
            .value_type = QueryValue::kIdentifier,
            .operations =
                {
                    Verification::Validation::OPERATION_UNKNOWN,
                    Verification::Validation::OPERATION_GREATER_THAN,
                    Verification::Validation::OPERATION_GREATER_THAN_OR_EQUAL,
                    Verification::Validation::OPERATION_LESS_THAN,
                    Verification::Validation::OPERATION_LESS_THAN_OR_EQUAL,
                    Verification::Validation::OPERATION_STRING_CONTAINS,
                    Verification::Validation::OPERATION_STRING_NOT_CONTAINS,
                    Verification::Validation::OPERATION_STRING_STARTS_WITH,
                    Verification::Validation::OPERATION_STRING_NOT_STARTS_WITH,
                    Verification::Validation::OPERATION_STRING_ENDS_WITH,
                    Verification::Validation::OPERATION_STRING_NOT_ENDS_WITH,
                    Verification::Validation::OPERATION_STRING_REGEX_MATCH,
                    Verification::Validation::OPERATION_STRING_NOT_REGEX_MATCH,
                }},
        UnsupportedValidationTestCase{
            .value_type = QueryValue::kRawData,
            .operations = {
                Verification::Validation::OPERATION_UNKNOWN,
                Verification::Validation::OPERATION_GREATER_THAN,
                Verification::Validation::OPERATION_GREATER_THAN_OR_EQUAL,
                Verification::Validation::OPERATION_LESS_THAN,
                Verification::Validation::OPERATION_LESS_THAN_OR_EQUAL,
                Verification::Validation::OPERATION_STRING_CONTAINS,
                Verification::Validation::OPERATION_STRING_NOT_CONTAINS,
                Verification::Validation::OPERATION_STRING_STARTS_WITH,
                Verification::Validation::OPERATION_STRING_NOT_STARTS_WITH,
                Verification::Validation::OPERATION_STRING_ENDS_WITH,
                Verification::Validation::OPERATION_STRING_NOT_ENDS_WITH,
                Verification::Validation::OPERATION_STRING_REGEX_MATCH,
                Verification::Validation::OPERATION_STRING_NOT_REGEX_MATCH,
            }}));

TEST_P(GreaterLesserValuesTest, GreaterThanSuccess) {
  const QueryValueInputs& test_case = GetParam();
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_operation(Verification::Validation::OPERATION_GREATER_THAN);
  *validation.add_operands() = test_case.query_value_b;
  std::vector<std::string> errors;
  EXPECT_THAT(VerifyQueryValue(test_case.query_value_a, verification, errors),
              IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST_P(GreaterLesserValuesTest, GreaterThanFailure) {
  const QueryValueInputs& test_case = GetParam();
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_operation(Verification::Validation::OPERATION_GREATER_THAN);
  *validation.add_operands() = test_case.query_value_a;
  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(test_case.query_value_b, verification, errors),
              IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0], HasSubstr("Failed OPERATION_GREATER_THAN check"));
}

TEST_P(GreaterLesserValuesTest, GreaterThanOrEqualSuccess) {
  const QueryValueInputs& test_case = GetParam();
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_operation(
      Verification::Validation::OPERATION_GREATER_THAN_OR_EQUAL);
  *validation.add_operands() = test_case.query_value_b;
  std::vector<std::string> errors;
  EXPECT_THAT(VerifyQueryValue(test_case.query_value_a, verification, errors),
              IsOk());
  ASSERT_THAT(errors, IsEmpty());

  // Test Equal Values
  errors.clear();
  EXPECT_THAT(VerifyQueryValue(test_case.query_value_b, verification, errors),
              IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST_P(GreaterLesserValuesTest, GreaterThanOrEqualFailure) {
  const QueryValueInputs& test_case = GetParam();
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_operation(
      Verification::Validation::OPERATION_GREATER_THAN_OR_EQUAL);
  *validation.add_operands() = test_case.query_value_a;
  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(test_case.query_value_b, verification, errors),
              IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0],
              HasSubstr("Failed OPERATION_GREATER_THAN_OR_EQUAL check"));
}

TEST_P(GreaterLesserValuesTest, LessThanSuccess) {
  const QueryValueInputs& test_case = GetParam();
  std::vector<std::string> errors;
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_operation(Verification::Validation::OPERATION_LESS_THAN);
  *validation.add_operands() = test_case.query_value_a;
  EXPECT_THAT(VerifyQueryValue(test_case.query_value_b, verification, errors),
              IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST_P(GreaterLesserValuesTest, LessThanFailure) {
  const QueryValueInputs& test_case = GetParam();
  std::vector<std::string> errors;
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_operation(Verification::Validation::OPERATION_LESS_THAN);
  *validation.add_operands() = test_case.query_value_b;
  ASSERT_THAT(VerifyQueryValue(test_case.query_value_a, verification, errors),
              IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0], HasSubstr("Failed OPERATION_LESS_THAN check"));
}

TEST_P(GreaterLesserValuesTest, LessThanOrEqualSuccess) {
  const QueryValueInputs& test_case = GetParam();
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_operation(
      Verification::Validation::OPERATION_LESS_THAN_OR_EQUAL);
  *validation.add_operands() = test_case.query_value_a;

  std::vector<std::string> errors;
  EXPECT_THAT(VerifyQueryValue(test_case.query_value_b, verification, errors),
              IsOk());
  ASSERT_THAT(errors, IsEmpty());

  // Test Equal Values
  errors.clear();
  EXPECT_THAT(VerifyQueryValue(test_case.query_value_a, verification, errors),
              IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST_P(GreaterLesserValuesTest, LessThanOrEqualFailure) {
  const QueryValueInputs& test_case = GetParam();

  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_operation(
      Verification::Validation::OPERATION_LESS_THAN_OR_EQUAL);
  *validation.add_operands() = test_case.query_value_b;

  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(test_case.query_value_a, verification, errors),
              IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0],
              HasSubstr("Failed OPERATION_LESS_THAN_OR_EQUAL check"));
}

INSTANTIATE_TEST_SUITE_P(
    GreaterLesserTests, GreaterLesserValuesTest,
    Values(
        QueryValueInputs{
            .query_value_a = ParseTextProtoOrDie(R"pb(int_value: 2)pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(int_value: 1)pb"),
        },
        QueryValueInputs{
            .query_value_a = ParseTextProtoOrDie(R"pb(double_value: 3.14)pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(double_value: 1.23)pb"),
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
        }));

struct StringOperationTestCase {
  Verification::Validation::Operation operation;
  QueryValue operand;
};
using StringOperationTest = TestWithParam<StringOperationTestCase>;

TEST_P(StringOperationTest, OperationStringFailure) {
  StringOperationTestCase test_case = GetParam();
  QueryValue qv = ParseTextProtoOrDie(R"pb(string_value: "somethingElse")pb");
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_operation(test_case.operation);
  *validation.add_operands() = test_case.operand;
  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(qv, verification, errors), IsStatusInternal());

  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0],
              HasSubstr(absl::StrFormat(
                  "Failed operation %s, value: '%s', operand: '%s'",
                  Verification::Validation::Operation_Name(test_case.operation),
                  qv.string_value(), test_case.operand.string_value())));
}

TEST_P(StringOperationTest, OperationStringSuccess) {
  StringOperationTestCase test_case = GetParam();
  QueryValue qv = ParseTextProtoOrDie(R"pb(string_value: "foobar")pb");
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_operation(test_case.operation);
  *validation.add_operands() = test_case.operand;
  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(qv, verification, errors), IsOk());

  ASSERT_THAT(errors, IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(
    StringOperationTests, StringOperationTest,
    Values(
        StringOperationTestCase{
            .operation = Verification::Validation::OPERATION_STRING_CONTAINS,
            .operand = ParseTextProtoOrDie(R"pb(string_value: "oob")pb"),
        },
        StringOperationTestCase{
            .operation =
                Verification::Validation::OPERATION_STRING_NOT_CONTAINS,
            .operand = ParseTextProtoOrDie(R"pb(string_value: "something")pb"),
        },
        StringOperationTestCase{
            .operation = Verification::Validation::OPERATION_STRING_STARTS_WITH,
            .operand = ParseTextProtoOrDie(R"pb(string_value: "foo")pb"),
        },
        StringOperationTestCase{
            .operation =
                Verification::Validation::OPERATION_STRING_NOT_STARTS_WITH,
            .operand = ParseTextProtoOrDie(R"pb(string_value: "something")pb"),
        },
        StringOperationTestCase{
            .operation = Verification::Validation::OPERATION_STRING_ENDS_WITH,
            .operand = ParseTextProtoOrDie(R"pb(string_value: "bar")pb"),
        },
        StringOperationTestCase{
            .operation =
                Verification::Validation::OPERATION_STRING_NOT_ENDS_WITH,
            .operand = ParseTextProtoOrDie(R"pb(string_value: "Else")pb"),
        },
        StringOperationTestCase{
            .operation = Verification::Validation::OPERATION_STRING_REGEX_MATCH,
            .operand = ParseTextProtoOrDie(R"pb(string_value: "^f.*r$")pb"),
        },
        StringOperationTestCase{
            .operation =
                Verification::Validation::OPERATION_STRING_NOT_REGEX_MATCH,
            .operand = ParseTextProtoOrDie(R"pb(string_value: "^s.*e$")pb"),
        }));

TEST(VerifyQueryValueTest, MultipleValidationsSuccess) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(string_value: "foobar")pb");
  QueryValueVerification verification = ParseTextProtoOrDie(R"pb(
    verify {
      validation {
        operation: OPERATION_STRING_CONTAINS
        operands { string_value: "oob" }
      }
      validation {
        operation: OPERATION_STRING_NOT_CONTAINS
        operands { string_value: "something" }
      }

      validation {
        operation: OPERATION_STRING_STARTS_WITH
        operands { string_value: "foo" }
      }
      validation {
        operation: OPERATION_STRING_ENDS_WITH
        operands { string_value: "bar" }
      }
    }
  )pb");
  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(qv, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyQueryValueTest, MultipleValidationsFailure) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(string_value: "foobar")pb");
  QueryValueVerification verification = ParseTextProtoOrDie(R"pb(
    verify {
      validation {
        operation: OPERATION_STRING_CONTAINS
        operands { string_value: "oob" }
      }
      validation {
        operation: OPERATION_STRING_NOT_CONTAINS
        operands { string_value: "something" }
      }

      validation {
        operation: OPERATION_STRING_STARTS_WITH
        operands { string_value: "bar" }
      }
      validation {
        operation: OPERATION_STRING_ENDS_WITH
        operands { string_value: "foo" }
      }
    }
  )pb");
  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(qv, verification, errors), IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(2));
  EXPECT_THAT(errors[0],
              HasSubstr("Failed operation OPERATION_STRING_STARTS_WITH, value: "
                        "'foobar', operand: 'bar'"));
  EXPECT_THAT(errors[1],
              HasSubstr("Failed operation OPERATION_STRING_ENDS_WITH, value: "
                        "'foobar', operand: 'foo'"));
}

TEST(VerifyQueryValueTest, InRangeFail) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 2)pb");
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_range(Verification::Validation::RANGE_IN);
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(int_value: 3)pb");

  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(qv, verification, errors), IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0],
              HasSubstr("Failed equality check, valueA: '2', valueB: '3'"));
}

TEST(VerifyQueryValueTest, RangeUnknownOption) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 2)pb");
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_range(Verification::Validation::RANGE_UNKNOWN);
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(int_value: 2)pb");

  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(qv, verification, errors),
              IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
}
TEST(VerifyQueryValueTest, RangeUnsupported) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 2)pb");
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_range(static_cast<Verification::Validation::Range>(
      Verification::Validation::RANGE_UNKNOWN - 1));
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(int_value: 2)pb");

  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(qv, verification, errors),
              IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyQueryValueTest, RangeMissingOperands) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 2)pb");
  QueryValueVerification verification;
  verification.mutable_verify()->mutable_validation()->Add()->set_range(
      Verification::Validation::RANGE_IN);

  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(qv, verification, errors),
              IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyQueryValueTest, InRangeSuccess) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 2)pb");
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_range(Verification::Validation::RANGE_IN);
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(int_value: 2)pb");

  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(qv, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
}
TEST(VerifyQueryValueTest, NotInRangeFail) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 2)pb");
  QueryValueVerification verification;

  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_range(Verification::Validation::RANGE_IN);
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(int_value: 2)pb");
  validation.set_range(Verification::Validation::RANGE_NOT_IN);

  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(qv, verification, errors), IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0],
              HasSubstr("Failed inequality check, valueA: '2', valueB: '2'"));
}

TEST(VerifyQueryValueTest, NotInRangeSuccess) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 2)pb");
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_range(Verification::Validation::RANGE_IN);
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(int_value: 3)pb");
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(int_value: 4)pb");
  validation.set_range(Verification::Validation::RANGE_NOT_IN);

  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(qv, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyQueryValueTest, IntervalFailSingleOperand) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 0)pb");
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_interval(Verification::Validation::INTERVAL_OPEN);
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(qv, verification, errors),
              IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyQueryValueTest, IntervalUnknownOption) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(int_value: 0)pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(int_value: 5)pb");
  QueryValue qv_c = ParseTextProtoOrDie(R"pb(int_value: 10)pb");

  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  *validation.add_operands() = qv_a;
  *validation.add_operands() = qv_c;

  validation.set_interval(Verification::Validation::INTERVAL_UNKNOWN);

  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(qv_b, verification, errors),
              IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyQueryValueTest, IntervalUnsupported) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(int_value: 0)pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(int_value: 5)pb");
  QueryValue qv_c = ParseTextProtoOrDie(R"pb(int_value: 10)pb");

  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  *validation.add_operands() = qv_a;
  *validation.add_operands() = qv_c;

  validation.set_interval(static_cast<Verification::Validation::Interval>(
      Verification::Validation::INTERVAL_UNKNOWN - 1));
  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(qv_b, verification, errors),
              IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyQueryValueTest, IntervalFail) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(int_value: 0)pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  QueryValue qv_c = ParseTextProtoOrDie(R"pb(int_value: 10)pb");
  QueryValue qv_d = ParseTextProtoOrDie(R"pb(int_value: 100)pb");

  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  *validation.add_operands() = qv_b;
  *validation.add_operands() = qv_c;

  validation.set_interval(Verification::Validation::INTERVAL_OPEN);

  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(qv_b, verification, errors), IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(
      errors[0],
      HasSubstr(
          "Failed OPERATION_GREATER_THAN check, value: '1', operand: '1'"));
  errors.clear();
  ASSERT_THAT(VerifyQueryValue(qv_c, verification, errors), IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(
      errors[0],
      HasSubstr(
          "Failed OPERATION_LESS_THAN check, value: '10', operand: '10'"));
  errors.clear();

  validation.set_interval(Verification::Validation::INTERVAL_CLOSED);
  ASSERT_THAT(VerifyQueryValue(qv_a, verification, errors), IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0], HasSubstr("Failed OPERATION_GREATER_THAN_OR_EQUAL "
                                   "check, value: '0', operand: '1'"));
  errors.clear();
  ASSERT_THAT(VerifyQueryValue(qv_d, verification, errors), IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0], HasSubstr("Failed OPERATION_LESS_THAN_OR_EQUAL check, "
                                   "value: '100', operand: '10'"));
  errors.clear();

  validation.set_interval(Verification::Validation::INTERVAL_OPEN_CLOSED);
  ASSERT_THAT(VerifyQueryValue(qv_b, verification, errors), IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0], HasSubstr("Failed OPERATION_GREATER_THAN "
                                   "check, value: '1', operand: '1'"));
  errors.clear();
  ASSERT_THAT(VerifyQueryValue(qv_d, verification, errors), IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0], HasSubstr("Failed OPERATION_LESS_THAN_OR_EQUAL check, "
                                   "value: '100', operand: '10'"));
  errors.clear();

  validation.set_interval(Verification::Validation::INTERVAL_CLOSED_OPEN);
  ASSERT_THAT(VerifyQueryValue(qv_a, verification, errors), IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0], HasSubstr("Failed OPERATION_GREATER_THAN_OR_EQUAL "
                                   "check, value: '0', operand: '1'"));
  errors.clear();
  ASSERT_THAT(VerifyQueryValue(qv_c, verification, errors), IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0], HasSubstr("Failed OPERATION_LESS_THAN check, "
                                   "value: '10', operand: '10'"));
  errors.clear();
}

TEST(VerifyQueryValueTest, IntervalSuccess) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(int_value: 0)pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(int_value: 5)pb");
  QueryValue qv_c = ParseTextProtoOrDie(R"pb(int_value: 10)pb");

  QueryValueVerification verification;

  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  *validation.add_operands() = qv_a;
  *validation.add_operands() = qv_c;

  validation.set_interval(Verification::Validation::INTERVAL_OPEN);
  std::vector<std::string> errors;
  ASSERT_THAT(VerifyQueryValue(qv_b, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
  errors.clear();

  validation.set_interval(Verification::Validation::INTERVAL_CLOSED);
  ASSERT_THAT(VerifyQueryValue(qv_a, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
  errors.clear();
  ASSERT_THAT(VerifyQueryValue(qv_c, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
  errors.clear();

  validation.set_interval(Verification::Validation::INTERVAL_OPEN_CLOSED);
  ASSERT_THAT(VerifyQueryValue(qv_b, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
  errors.clear();
  ASSERT_THAT(VerifyQueryValue(qv_c, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
  errors.clear();

  validation.set_interval(Verification::Validation::INTERVAL_CLOSED_OPEN);
  ASSERT_THAT(VerifyQueryValue(qv_a, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
  errors.clear();
  ASSERT_THAT(VerifyQueryValue(qv_b, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
  errors.clear();
}

TEST(VerifyListValueTest, MissingVerifyField) {
  ListValue lv = ParseTextProtoOrDie(R"pb(
    values { int_value: 1 }
    values { int_value: 2 }
  )pb");
  ListValueVerification verification;
  std::vector<std::string> errors;
  EXPECT_THAT(VerifyListValue(lv, verification, errors),
              IsStatusInvalidArgument());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyListValueTest, BasicListVerification) {
  ListValue lv = ParseTextProtoOrDie(R"pb(
    values { int_value: 1 }
    values { int_value: 2 }
  )pb");
  ListValueVerification verification = ParseTextProtoOrDie(R"pb(
    verify { verify {} }
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(VerifyListValue(lv, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyListValueTest, ListofSuberyqueryVerification) {
  ListValue lv = ParseTextProtoOrDie(R"pb(
    values {
      subquery_value {
        fields {
          key: "foo"
          value: { int_value: 1 }
        }
      }
    }
  )pb");
  ListValueVerification verification = ParseTextProtoOrDie(R"pb(
    verify {
      data_compare {
        fields {
          key: "foo"
          value: { verify {} }
        }
      }
    }
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(VerifyListValue(lv, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyListValueTest, ListofListVerification) {
  ListValue lv = ParseTextProtoOrDie(R"pb(
    values { list_value { values { int_value: 1 } } }
  )pb");
  ListValueVerification verification = ParseTextProtoOrDie(R"pb(
    verify {}
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(VerifyListValue(lv, verification, errors),
              IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
}

struct VerifyQueryValueInputsWithExpectations {
  QueryValue query_value;
  QueryResultDataVerification data_compare;
  internal_status::IsStatusPolyMatcher expected_status;
  std::vector<std::string> expected_errors;
};
using VerificationSubqueryValueTest =
    TestWithParam<VerifyQueryValueInputsWithExpectations>;

TEST_P(VerificationSubqueryValueTest, VerifySubqueryValueTest) {
  const VerifyQueryValueInputsWithExpectations& test_case = GetParam();
  std::vector<std::string> errors;
  ASSERT_THAT(VerifySubqueryValue(test_case.query_value.subquery_value(),
                                  test_case.data_compare, errors),
              test_case.expected_status);
  ASSERT_THAT(errors, UnorderedElementsAreArray(test_case.expected_errors));
}
INSTANTIATE_TEST_SUITE_P(
    VerificationSubqueryValueTest, VerificationSubqueryValueTest,
    Values(
        VerifyQueryValueInputsWithExpectations{
            .query_value = ParseTextProtoOrDie(R"pb(subquery_value {
                                                      fields {
                                                        key: "foo"
                                                        value: { int_value: 1 }
                                                      }
                                                    })pb"),
            .data_compare = ParseTextProtoOrDie(
                R"pb(fields {
                       key: "foo"
                       value: {
                         verify {
                           presence: PRESENCE_REQUIRED,
                           validation: {
                             operation: OPERATION_GREATER_THAN
                             operands { int_value: 0 }
                           }
                         }
                       }
                     })pb"),
            .expected_status = IsOk(),
        },
        VerifyQueryValueInputsWithExpectations{
            .query_value = ParseTextProtoOrDie(R"pb(subquery_value {
                                                      fields {
                                                        key: "foo"
                                                        value: { int_value: 1 }
                                                      }
                                                    })pb"),
            .data_compare = ParseTextProtoOrDie(
                R"pb(fields {
                       key: "bar"
                       value: {
                         verify {
                           presence: PRESENCE_REQUIRED,
                         }
                       }
                     })pb"),
            .expected_status = IsStatusInternal(),
            .expected_errors = {"Missing required property bar"},
        },
        VerifyQueryValueInputsWithExpectations{
            .query_value = ParseTextProtoOrDie(R"pb(subquery_value {
                                                      fields {
                                                        key: "foo"
                                                        value: { int_value: 1 }
                                                      }
                                                    })pb"),
            .data_compare = ParseTextProtoOrDie(
                R"pb(fields {
                       key: "bar"
                       value: {
                         verify {
                           presence: PRESENCE_OPTIONAL,
                         }
                       }
                     })pb"),
            .expected_status = IsOk(),
        },
        VerifyQueryValueInputsWithExpectations{
            .query_value = ParseTextProtoOrDie(R"pb(subquery_value {
                                                      fields {
                                                        key: "foo"
                                                        value: { int_value: 1 }
                                                      }
                                                    })pb"),
            .data_compare = ParseTextProtoOrDie(
                R"pb(fields {
                       key: "bar"
                       value: {}
                     })pb"),
            .expected_status = IsOk(),
        },
        VerifyQueryValueInputsWithExpectations{
            .query_value =
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
                       value: {
                         data_compare {
                           fields {
                             key: "bar"
                             value: { verify {} }
                           }
                         }
                       }
                     })pb"),
            .expected_status = IsOk(),
        },
        VerifyQueryValueInputsWithExpectations{
            .query_value = ParseTextProtoOrDie(R"pb(subquery_value {
                                                      fields {
                                                        key: "foo"
                                                        value: { list_value {} }
                                                      }
                                                    })pb"),
            .data_compare = ParseTextProtoOrDie(
                R"pb(fields {
                       key: "foo"
                       value: { list_compare { verify {} } }
                     })pb"),
            .expected_status = IsOk(),
        }));

TEST(VerifyQueryResultTest, QueryIdsMismatchForVerification) {
  QueryResult qr = ParseTextProtoOrDie(R"pb(
    query_id: "query_a"
  )pb");
  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_b"
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(VerifyQueryResult(qr, verification, errors),
              IsStatusInvalidArgument());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyQueryResultTest, SuccessSimpleQueryResult) {
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
        value { verify {} }
      }
    }
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(VerifyQueryResult(qr, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyQueryResultTest, SuccessSubqueryQueryResult) {
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
              value { verify {} }
            }
          }
        }
      }
    }
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(VerifyQueryResult(qr, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(VerifyQueryResultTest, SuccessListQueryResult) {
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
        value { list_compare { verify { verify {} } } }
      }
    }
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(VerifyQueryResult(qr, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

struct VerifyQueryResultErrorStrings {
  QueryResult query_result;
  QueryResultVerification verification;
  internal_status::IsStatusPolyMatcher expected_status;
  std::vector<std::string> expected_errors;
};
using VerifyQueryResultErrorStringsTest =
    TestWithParam<VerifyQueryResultErrorStrings>;

TEST_P(VerifyQueryResultErrorStringsTest, VerifyQueryResult) {
  const VerifyQueryResultErrorStrings& test_case = GetParam();
  std::vector<std::string> errors;
  ASSERT_THAT(
      VerifyQueryResult(test_case.query_result, test_case.verification, errors),
      test_case.expected_status);
  ASSERT_THAT(errors, UnorderedElementsAreArray(test_case.expected_errors));
}

INSTANTIATE_TEST_SUITE_P(
    VerifyQueryResultErrorStringsTest, VerifyQueryResultErrorStringsTest,
    Values(
        VerifyQueryResultErrorStrings{
            .query_result = ParseTextProtoOrDie(R"pb(
              query_id: "query_1"
              data {
                fields {
                  key: "key0"
                  value { list_value { values { int_value: 0 } } }
                }
              }
            )pb"),
            .verification = ParseTextProtoOrDie(
                R"pb(query_id: "query_1"
                     data_verify {
                       fields {
                         key: "key0"
                         value {
                           list_compare {
                             verify {
                               verify {
                                 validation {
                                   operation: OPERATION_GREATER_THAN
                                   operands { int_value: 0 }
                                 }
                               }
                             }
                           }
                         }
                       }
                     })pb"),
            .expected_status = IsStatusInternal(),
            .expected_errors =
                {"(Path: query_1.key0.index=0) Failed OPERATION_GREATER_THAN "
                 "check, value: '0', operand: '0'"},
        },
        VerifyQueryResultErrorStrings{
            .query_result = ParseTextProtoOrDie(R"pb(
              query_id: "query_1"
              data {
                fields {
                  key: "key0"
                  value {
                    list_value {
                      values {
                        subquery_value {
                          fields {
                            key: "subkey0"
                            value { string_value: "bar" }
                          }
                        }
                      }
                    }
                  }
                }
              }
            )pb"),
            .verification = ParseTextProtoOrDie(
                R"pb(query_id: "query_1"
                     data_verify {
                       fields {
                         key: "key0"
                         value {
                           list_compare {
                             identifiers: "subkey0"
                             verify {
                               data_compare {
                                 fields {
                                   key: "subkey0"
                                   value {
                                     verify {
                                       validation {
                                         operation: OPERATION_STRING_STARTS_WITH
                                         operands { string_value: "foo" }
                                       }
                                     }
                                   }
                                 }
                               }
                             }
                           }
                         }
                       }
                     })pb"),
            .expected_status = IsStatusInternal(),
            .expected_errors =
                {"(Path: query_1.key0.index=0.subkey0) Failed operation "
                 "OPERATION_STRING_STARTS_WITH, value: 'bar', operand: 'foo'"},
        }));

using CompareQueryResultErrorStringsTest =
    TestWithParam<VerifyQueryResultErrorStrings>;

TEST_P(CompareQueryResultErrorStringsTest, CompareQueryResult) {
  const VerifyQueryResultErrorStrings& test_case = GetParam();
  std::vector<std::string> errors;
  ASSERT_THAT(
      CompareQueryResults(test_case.query_result, test_case.query_result,
                          test_case.verification, errors),
      test_case.expected_status);
  ASSERT_THAT(errors, UnorderedElementsAreArray(test_case.expected_errors));
}

INSTANTIATE_TEST_SUITE_P(
    CompareQueryResultErrorStringsTest, CompareQueryResultErrorStringsTest,
    Values(
        VerifyQueryResultErrorStrings{
            .query_result = ParseTextProtoOrDie(R"pb(
              query_id: "query_1"
              data {
                fields {
                  key: "key0"
                  value { list_value { values { int_value: 0 } } }
                }
              }
            )pb"),
            .verification = ParseTextProtoOrDie(
                R"pb(query_id: "query_1"
                     data_verify {
                       fields {
                         key: "key0"
                         value {
                           list_compare {
                             verify { verify { comparison: COMPARE_NOT_EQUAL } }
                           }
                         }
                       }
                     })pb"),
            .expected_status = IsStatusInternal(),
            .expected_errors = {"(Path: query_1.key0.index=0) Failed "
                                "inequality check, valueA: '0', valueB: '0'"},
        },
        VerifyQueryResultErrorStrings{
            .query_result = ParseTextProtoOrDie(R"pb(
              query_id: "query_1"
              data {
                fields {
                  key: "key0"
                  value {
                    list_value {
                      values {
                        subquery_value {
                          fields {
                            key: "subkey0"
                            value { string_value: "foo" }
                          }
                          fields {
                            key: "element"
                            value { string_value: "bar" }
                          }
                        }
                      }
                    }
                  }
                }
              }
            )pb"),
            .verification = ParseTextProtoOrDie(
                R"pb(query_id: "query_1"
                     data_verify {
                       fields {
                         key: "key0"
                         value {
                           list_compare {
                             identifiers: "subkey0"
                             verify {
                               data_compare {
                                 fields {
                                   key: "element"
                                   value {
                                     verify { comparison: COMPARE_NOT_EQUAL }
                                   }
                                 }
                               }
                             }
                           }
                         }
                       }
                     })pb"),
            .expected_status = IsStatusInternal(),
            .expected_errors =
                {"(Path: query_1.key0.subkey0=\"foo\".element) Failed "
                 "inequality check, valueA: 'bar', valueB: 'bar'"},
        }));

}  // namespace
}  // namespace ecclesia
