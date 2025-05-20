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
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::ecclesia::EqualsProto;
using ::ecclesia::IgnoringRepeatedFieldOrdering;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::TestWithParam;
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
  QueryVerificationResult result;
  ASSERT_THAT(
      CompareQueryValues(test_case.query_value_a, test_case.query_value_a,
                         Verification::COMPARE_EQUAL, result),
      IsOk());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST_P(EqualValuesTest, EqualFailure) {
  const QueryValueInputs& test_case = GetParam();
  QueryVerificationResult result;
  ASSERT_THAT(
      CompareQueryValues(test_case.query_value_a, test_case.query_value_b,
                         Verification::COMPARE_EQUAL, result),
      IsStatusInternal());
  ASSERT_THAT(result.errors(), SizeIs(1));
  EXPECT_THAT(result.errors(0).msg(), HasSubstr("Failed equality check"));
}

TEST_P(EqualValuesTest, NotEqualSuccess) {
  const QueryValueInputs& test_case = GetParam();
  QueryVerificationResult result;
  ASSERT_THAT(
      CompareQueryValues(test_case.query_value_a, test_case.query_value_b,
                         Verification::COMPARE_NOT_EQUAL, result),
      IsOk());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST_P(EqualValuesTest, NotEqualFailure) {
  const QueryValueInputs& test_case = GetParam();
  QueryVerificationResult result;
  ASSERT_THAT(
      CompareQueryValues(test_case.query_value_a, test_case.query_value_a,
                         Verification::COMPARE_NOT_EQUAL, result),
      IsStatusInternal());
  ASSERT_THAT(result.errors(), SizeIs(1));
  EXPECT_THAT(result.errors(0).msg(), HasSubstr("Failed inequality check"));
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
  QueryVerificationResult result;
  EXPECT_THAT(
      CompareQueryValues(qv_a, qv_b, Verification::COMPARE_EQUAL, result),
      IsStatusFailedPrecondition());
  ASSERT_THAT(result.errors(), IsEmpty());
}

TEST(CompareQueryValuesTest, RawDataValueNotSet) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(raw_data {})pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(raw_data {})pb");
  QueryVerificationResult result;
  EXPECT_THAT(
      CompareQueryValues(qv_a, qv_b, Verification::COMPARE_EQUAL, result),
      IsStatusFailedPrecondition());
  ASSERT_THAT(result.errors(), IsEmpty());
}

TEST(CompareQueryValuesTest, DifferentTypes) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(bool_value: false)pb");
  QueryVerificationResult result;
  EXPECT_THAT(
      CompareQueryValues(qv_a, qv_b, Verification::COMPARE_EQUAL, result),
      IsStatusFailedPrecondition());
  ASSERT_THAT(result.errors(), IsEmpty());
}

TEST(CompareQueryValuesTest, QueryValueNotSet) {
  QueryValue qv_a;
  QueryValue qv_b;
  QueryVerificationResult result;
  EXPECT_THAT(
      CompareQueryValues(qv_a, qv_b, Verification::COMPARE_EQUAL, result),
      IsStatusFailedPrecondition());
  ASSERT_THAT(result.errors(), IsEmpty());
}

TEST(CompareQueryValuesTest, UnknownOperation) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  QueryVerificationResult result;
  EXPECT_THAT(
      CompareQueryValues(qv_a, qv_b, Verification::COMPARE_UNKNOWN, result),
      IsOk());
  ASSERT_THAT(result.errors(), IsEmpty());
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
  QueryVerificationResult result;
  EXPECT_THAT(
      CompareQueryValues(qv_a, qv_a, Verification::COMPARE_EQUAL, result),
      IsStatusFailedPrecondition());
  ASSERT_THAT(result.errors(), IsEmpty());
  EXPECT_THAT(
      CompareQueryValues(qv_a, qv_b, Verification::COMPARE_EQUAL, result),
      IsStatusFailedPrecondition());
  ASSERT_THAT(result.errors(), IsEmpty());
}

// Struct to hold the subquery values to be compared in various
// parameterized tests.
struct QueryValueInputsWithExpectations {
  QueryValue query_value_a;
  QueryValue query_value_b;
  QueryResultDataVerification data_compare;
  ListValueVerification list_compare;
  internal_status::IsStatusPolyMatcher expected_status;
  QueryVerificationResult expected_result;
};

// In Subquery Values Test, query_value_a and query_value_b are different
// values.
using SubqueryValueTests = TestWithParam<QueryValueInputsWithExpectations>;

TEST_P(SubqueryValueTests, CompareSubqueryValues) {
  const QueryValueInputsWithExpectations& test_case = GetParam();
  QueryVerificationResult result;
  EXPECT_THAT(CompareSubqueryValues(test_case.query_value_a.subquery_value(),
                                    test_case.query_value_b.subquery_value(),
                                    test_case.data_compare, result),
              test_case.expected_status);
  EXPECT_THAT(result, IgnoringRepeatedFieldOrdering(
                          EqualsProto(test_case.expected_result)));
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
            .expected_result = ParseTextProtoOrDie(
                R"pb(errors { msg: "Missing property 'foo' in valueA" })pb"),
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
            .expected_result = ParseTextProtoOrDie(
                R"pb(errors { msg: "Missing property 'foo' in valueB" })pb"),
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
            .expected_result = {},
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
            .expected_result = {},
        },
        QueryValueInputsWithExpectations{
            .query_value_a = ParseTextProtoOrDie(R"pb(subquery_value {})pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(subquery_value {})pb"),
            .expected_status = IsOk(),
            .expected_result = {},
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
            .expected_result = {},
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
            .expected_result = {},
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
            .expected_result = {},
        }));

// In List Values Test, query_value_a and query_value_b are different values.
using ListValueTests = TestWithParam<QueryValueInputsWithExpectations>;
TEST_P(ListValueTests, CompareListValues) {
  const QueryValueInputsWithExpectations& test_case = GetParam();
  QueryVerificationResult result;
  EXPECT_THAT(CompareListValues(test_case.query_value_a.list_value(),
                                test_case.query_value_b.list_value(),
                                test_case.list_compare, result),
              test_case.expected_status);
  EXPECT_THAT(result, IgnoringRepeatedFieldOrdering(
                          EqualsProto(test_case.expected_result)));
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
            .expected_result = ParseTextProtoOrDie(
                R"pb(errors {
                       msg: "Missing value in 'valueB' with identifier index=0"
                     })pb"),
        },
        QueryValueInputsWithExpectations{
            .query_value_a = ParseTextProtoOrDie(R"pb(list_value {
                                                        values { list_value {} }
                                                      })pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(list_value {
                                                        values { list_value {} }
                                                      })pb"),
            .expected_status = IsStatusFailedPrecondition(),
            .expected_result = {},
        },
        QueryValueInputsWithExpectations{
            .query_value_a = ParseTextProtoOrDie(R"pb(list_value {})pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(list_value {})pb"),
            .expected_status = IsOk(),
            .expected_result = {},
        },
        QueryValueInputsWithExpectations{
            .query_value_a =
                ParseTextProtoOrDie(R"pb(list_value { values {} })pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(list_value {})pb"),
            .list_compare = ParseTextProtoOrDie(R"pb(identifiers: "foo")pb"),
            .expected_status = IsStatusInternal(),
            .expected_result = ParseTextProtoOrDie(
                R"pb(errors {
                       msg: "Missing identifier in 'valueA': Identifiers are only supported for subquery values"
                     })pb"),
        },
        QueryValueInputsWithExpectations{
            .query_value_a = ParseTextProtoOrDie(
                R"pb(list_value { values { subquery_value {} } })pb"),
            .query_value_b = ParseTextProtoOrDie(R"pb(list_value {})pb"),
            .list_compare = ParseTextProtoOrDie(R"pb(identifiers: "foo")pb"),
            .expected_status = IsStatusInternal(),
            .expected_result = ParseTextProtoOrDie(
                R"pb(errors {
                       msg: "Missing identifier in 'valueA': property 'foo' is not present"
                     })pb"),
        },
        QueryValueInputsWithExpectations{
            .query_value_a = ParseTextProtoOrDie(R"pb(list_value {})pb"),
            .query_value_b = ParseTextProtoOrDie(
                R"pb(list_value { values { subquery_value {} } })pb"),
            .list_compare = ParseTextProtoOrDie(R"pb(identifiers: "foo")pb"),
            .expected_status = IsStatusInternal(),
            .expected_result = ParseTextProtoOrDie(
                R"pb(errors {
                       msg: "Missing identifier in 'valueB': property 'foo' is not present"
                     })pb"),
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
            .expected_result = ParseTextProtoOrDie(
                R"pb(errors { msg: "Duplicate identifier in valueA: foo=1" })pb"),
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
            .expected_result = ParseTextProtoOrDie(
                R"pb(errors {
                       msg: "Missing value in 'valueA' with identifier foo=1"
                     })pb"),
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
            .expected_result = ParseTextProtoOrDie(
                R"pb(errors {
                       msg: "Missing value in 'valueB' with identifier foo=1"
                     })pb"),
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
            .expected_result = ParseTextProtoOrDie(
                R"pb(errors {
                       msg: "Missing value in 'valueB' with identifier foo=2"
                     }
                     errors {
                       msg: "Missing value in 'valueA' with identifier foo=3"
                     })pb"),
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
            .expected_result = {},
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
            .expected_result = {},
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
            .expected_result = {},
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
            .expected_result = {},
        }));
TEST(CompareListValuesTest, MisAlignedListValues) {
  QueryValue qv_a = ParseTextProtoOrDie(
      R"pb(list_value {
             values {
               subquery_value {
                 fields {
                   key: "foo"
                   value: { int_value: 1 }
                 }
                 fields {
                   key: "_uri_"
                   value: { string_value: "/redfish/v1/index0" }
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
  QueryValue qv_b = ParseTextProtoOrDie(
      R"pb(list_value {
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
                 fields {
                   key: "_uri_"
                   value: { string_value: "/redfish/v1/index1b" }
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
  QueryVerificationResult result;
  EXPECT_THAT(CompareListValues(qv_a.list_value(), qv_b.list_value(),
                                verification, result),
              IsStatusInternal());
  EXPECT_THAT(
      result,
      IgnoringRepeatedFieldOrdering(EqualsProto(
          R"pb(errors {
                 msg: "(path: '[1].foo', uri: '/redfish/v1/index1b') Failed equality check, valueA: '2', valueB: '1'"
                 path: "[1].foo"
                 uri: "/redfish/v1/index1b"
               }
               errors {
                 msg: "(path: '[0].foo', uri: '/redfish/v1/index0') Failed equality check, valueA: '1', valueB: '2'"
                 path: "[0].foo"
                 uri: "/redfish/v1/index0"
               })pb")));
}

TEST(CompareQueryResultsTest, QueryIdsMismatchForAndB) {
  QueryResult qr_a = ParseTextProtoOrDie(R"pb(
    query_id: "query_a"
  )pb");
  QueryResult qr_b = ParseTextProtoOrDie(R"pb(
    query_id: "query_b"
  )pb");
  QueryResultVerification verification;
  QueryVerificationResult result;
  EXPECT_THAT(CompareQueryResults(qr_a, qr_b, verification, result),
              IsStatusFailedPrecondition());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(CompareQueryResultsTest, QueryIdsMismatchForVerification) {
  QueryResult qr = ParseTextProtoOrDie(R"pb(
    query_id: "query_a"
  )pb");
  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_b"
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(CompareQueryResults(qr, qr, verification, result),
              IsStatusInvalidArgument());
  EXPECT_THAT(result, EqualsProto(""));
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
  QueryVerificationResult result;
  EXPECT_THAT(CompareQueryResults(qr, qr, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
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
  QueryVerificationResult result;
  EXPECT_THAT(CompareQueryResults(qr, qr, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
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
  QueryVerificationResult result;
  EXPECT_THAT(CompareQueryResults(qr, qr, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST(CompareQueryResultsTest, SuccessConditionalSimpleQueryResult) {
  QueryResult qr_a = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 0 }
      }
      fields {
        key: "conditional_key"
        value { int_value: 1 }
      }
    }
  )pb");
  QueryResult qr_b = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key"
        value { int_value: 1 }
      }
    }
  )pb");
  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          verify { comparison: COMPARE_EQUAL }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key"
                condition {
                  validation {
                    operation: OPERATION_GREATER_THAN_OR_EQUAL
                    operands { int_value: 1 }
                  }
                }
              }
            }
            conditional_verify { verify { comparison: COMPARE_NOT_EQUAL } }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { verify { comparison: COMPARE_EQUAL } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(CompareQueryResults(qr_a, qr_b, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(CompareQueryResultsTest, SuccessAllOfConditionalSimpleQueryResult) {
  QueryResult qr_a = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 0 }
      }
      fields {
        key: "conditional_key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key1"
        value { string_value: "/phys" }
      }
    }
  )pb");
  QueryResult qr_b = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key1"
        value { string_value: "/phys" }
      }
    }
  )pb");
  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          verify { comparison: COMPARE_EQUAL }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key0"
                condition {
                  validation {
                    operation: OPERATION_GREATER_THAN_OR_EQUAL
                    operands { int_value: 1 }
                  }
                }
              }
              conditions {
                property_name: "conditional_key1"
                condition {
                  validation {
                    operation: OPERATION_STRING_CONTAINS
                    operands { string_value: "/phys" }
                  }
                }
              }
            }
            conditional_verify { verify { comparison: COMPARE_NOT_EQUAL } }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { verify { comparison: COMPARE_EQUAL } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(CompareQueryResults(qr_a, qr_b, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(CompareQueryResultsTest, SuccessAnyOfConditionalSimpleQueryResult) {
  QueryResult qr_a = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 0 }
      }
      fields {
        key: "conditional_key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key1"
        value { string_value: "/phys" }
      }
    }
  )pb");
  QueryResult qr_b = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key1"
        value { string_value: "/phys" }
      }
    }
  )pb");
  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          verify { comparison: COMPARE_EQUAL }
          overrides {
            any_of {
              conditions {
                property_name: "conditional_key0"
                condition {
                  validation {
                    operation: OPERATION_GREATER_THAN_OR_EQUAL
                    operands { int_value: 1 }
                  }
                }
              }
              conditions {
                property_name: "conditional_key1"
                condition {
                  validation {
                    operation: OPERATION_STRING_CONTAINS
                    operands { string_value: "/phys0" }
                  }
                }
              }
            }
            conditional_verify { verify { comparison: COMPARE_NOT_EQUAL } }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { verify { comparison: COMPARE_EQUAL } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(CompareQueryResults(qr_a, qr_b, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(CompareQueryResultsTest, SuccessMultipleOverridesSimpleQueryResult) {
  QueryResult qr_a = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 0 }
      }
      fields {
        key: "conditional_key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key1"
        value { string_value: "/phys" }
      }
    }
  )pb");
  QueryResult qr_b = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key1"
        value { string_value: "/phys" }
      }
    }
  )pb");
  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          verify { comparison: COMPARE_EQUAL }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key0"
                condition {
                  validation {
                    operation: OPERATION_LESS_THAN
                    operands { int_value: 1 }
                  }
                }
              }
            }
            conditional_verify { verify { comparison: COMPARE_NOT_EQUAL } }
          }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key1"
                condition {
                  validation {
                    operation: OPERATION_STRING_CONTAINS
                    operands { string_value: "/phys" }
                  }
                }
              }
            }
            conditional_verify { verify { comparison: COMPARE_NOT_EQUAL } }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { verify { comparison: COMPARE_EQUAL } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(CompareQueryResults(qr_a, qr_b, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(CompareQueryResultsTest, SuccessConditionalSubqueryQueryResult) {
  QueryResult qr_a = ParseTextProtoOrDie(R"pb(
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
      fields {
        key: "conditional_key"
        value { int_value: 1 }
      }
    }
  )pb");
  QueryResult qr_b = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value {
          subquery_value {
            fields {
              key: "key1"
              value { int_value: 1 }
            }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { int_value: 1 }
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
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key"
                condition {
                  validation {
                    operation: OPERATION_GREATER_THAN_OR_EQUAL
                    operands { int_value: 1 }
                  }
                }
              }
            }
            conditional_verify {
              data_compare {
                fields {
                  key: "key1"
                  value { verify { comparison: COMPARE_NOT_EQUAL } }
                }
              }
            }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { verify { comparison: COMPARE_EQUAL } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(CompareQueryResults(qr_a, qr_b, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(CompareQueryResultsTest, SuccessConditionalListQueryResult) {
  QueryResult qr_a = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { list_value { values { int_value: 0 } } }
      }
      fields {
        key: "conditional_key"
        value { int_value: 1 }
      }
    }
  )pb");
  QueryResult qr_b = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { list_value { values { int_value: 1 } } }
      }
      fields {
        key: "conditional_key"
        value { int_value: 1 }
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
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key"
                condition {
                  validation {
                    operation: OPERATION_GREATER_THAN_OR_EQUAL
                    operands { int_value: 1 }
                  }
                }
              }
            }
            conditional_verify {
              list_compare {
                verify { verify { comparison: COMPARE_NOT_EQUAL } }
              }
            }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { verify { comparison: COMPARE_EQUAL } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(CompareQueryResults(qr_a, qr_b, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST(CompareQueryResultsTest, SuccessConditionalSimpleQueryResultDefault) {
  QueryResult qr_a = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 0 }
      }
      fields {
        key: "conditional_key"
        value { int_value: 1 }
      }
    }
  )pb");
  QueryResult qr_b = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key"
        value { int_value: 1 }
      }
    }
  )pb");
  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          verify { comparison: COMPARE_NOT_EQUAL }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key"
                condition {
                  validation {
                    operation: OPERATION_LESS_THAN
                    operands { int_value: 1 }
                  }
                }
              }
            }
            conditional_verify { verify { comparison: COMPARE_EQUAL } }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { verify { comparison: COMPARE_EQUAL } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(CompareQueryResults(qr_a, qr_b, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(CompareQueryResultsTest, SuccessAllOfConditionalSimpleQueryResultDefault) {
  QueryResult qr_a = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 0 }
      }
      fields {
        key: "conditional_key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key1"
        value { string_value: "/phys" }
      }
    }
  )pb");
  QueryResult qr_b = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key1"
        value { string_value: "/phys" }
      }
    }
  )pb");
  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          verify { comparison: COMPARE_NOT_EQUAL }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key0"
                condition {
                  validation {
                    operation: OPERATION_GREATER_THAN_OR_EQUAL
                    operands { int_value: 1 }
                  }
                }
              }
              conditions {
                property_name: "conditional_key1"
                condition {
                  validation {
                    operation: OPERATION_STRING_CONTAINS
                    operands { string_value: "/phys0" }
                  }
                }
              }
            }
            conditional_verify { verify { comparison: COMPARE_EQUAL } }
          }
        }
      }
      fields {
        key: "conditional_key0"
        value { verify { comparison: COMPARE_EQUAL } }
      }
      fields {
        key: "conditional_key1"
        value { verify { comparison: COMPARE_EQUAL } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(CompareQueryResults(qr_a, qr_b, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(CompareQueryResultsTest, SuccessAnyOfConditionalSimpleQueryResultDefault) {
  QueryResult qr_a = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 0 }
      }
      fields {
        key: "conditional_key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key1"
        value { string_value: "/phys" }
      }
    }
  )pb");
  QueryResult qr_b = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key1"
        value { string_value: "/phys" }
      }
    }
  )pb");
  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          verify { comparison: COMPARE_NOT_EQUAL }
          overrides {
            any_of {
              conditions {
                property_name: "conditional_key0"
                condition {
                  validation {
                    operation: OPERATION_GREATER_THAN
                    operands { int_value: 1 }
                  }
                }
              }
              conditions {
                property_name: "conditional_key1"
                condition {
                  validation {
                    operation: OPERATION_STRING_CONTAINS
                    operands { string_value: "something" }
                  }
                }
              }
            }
            conditional_verify { verify { comparison: COMPARE_EQUAL } }
          }
        }
      }
      fields {
        key: "conditional_key0"
        value { verify { comparison: COMPARE_EQUAL } }
      }
      fields {
        key: "conditional_key1"
        value { verify { comparison: COMPARE_EQUAL } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(CompareQueryResults(qr_a, qr_b, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(CompareQueryResultsTest,
     SuccessMultipleOverridesSimpleQueryResultDefault) {
  QueryResult qr_a = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 0 }
      }
      fields {
        key: "conditional_key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key1"
        value { string_value: "/phys" }
      }
    }
  )pb");
  QueryResult qr_b = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key1"
        value { string_value: "/phys" }
      }
    }
  )pb");
  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          verify { comparison: COMPARE_NOT_EQUAL }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key0"
                condition {
                  validation {
                    operation: OPERATION_GREATER_THAN
                    operands { int_value: 1 }
                  }
                }
              }
            }
            conditional_verify { verify { comparison: COMPARE_EQUAL } }
          }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key1"
                condition {
                  validation {
                    operation: OPERATION_STRING_CONTAINS
                    operands { string_value: "/phys0" }
                  }
                }
              }
            }
            conditional_verify { verify { comparison: COMPARE_EQUAL } }
          }
        }
      }
      fields {
        key: "conditional_key0"
        value { verify { comparison: COMPARE_EQUAL } }
      }
      fields {
        key: "conditional_key1"
        value { verify { comparison: COMPARE_EQUAL } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(CompareQueryResults(qr_a, qr_b, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(CompareQueryResultsTest, SuccessConditionalSubqueryQueryResultDefault) {
  QueryResult qr_a = ParseTextProtoOrDie(R"pb(
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
      fields {
        key: "conditional_key"
        value { int_value: 1 }
      }
    }
  )pb");
  QueryResult qr_b = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value {
          subquery_value {
            fields {
              key: "key1"
              value { int_value: 1 }
            }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { int_value: 1 }
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
              value { verify { comparison: COMPARE_NOT_EQUAL } }
            }
          }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key"
                condition {
                  validation {
                    operation: OPERATION_LESS_THAN
                    operands { int_value: 1 }
                  }
                }
              }
            }
            conditional_verify {
              data_compare {
                fields {
                  key: "key1"
                  value { verify { comparison: COMPARE_EQUAL } }
                }
              }
            }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { verify { comparison: COMPARE_EQUAL } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(CompareQueryResults(qr_a, qr_b, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(CompareQueryResultsTest, SuccessConditionalListQueryResultDefault) {
  QueryResult qr_a = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { list_value { values { int_value: 0 } } }
      }
      fields {
        key: "conditional_key"
        value { int_value: 1 }
      }
    }
  )pb");
  QueryResult qr_b = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { list_value { values { int_value: 1 } } }
      }
      fields {
        key: "conditional_key"
        value { int_value: 1 }
      }
    }
  )pb");
  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          list_compare { verify { verify { comparison: COMPARE_NOT_EQUAL } } }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key"
                condition {
                  validation {
                    operation: OPERATION_LESS_THAN
                    operands { int_value: 1 }
                  }
                }
              }
            }
            conditional_verify {
              list_compare { verify { verify { comparison: COMPARE_EQUAL } } }
            }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { verify { comparison: COMPARE_EQUAL } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(CompareQueryResults(qr_a, qr_b, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryValueTest, EmtpyVerification) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  QueryValueVerification verification;
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryValue(qv, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryValueTest, VerificationWithNoValidation) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  QueryValueVerification verification = ParseTextProtoOrDie(R"pb(verify {})pb");
  std::vector<std::string> errors;
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryValue(qv, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryValueTest, VerificationWithNoOperandas) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  QueryValueVerification verification =
      ParseTextProtoOrDie(R"pb(verify { validation {} })pb");
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryValue(qv, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryValueTest, VerificationWithJustOperands) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  QueryValueVerification verification = ParseTextProtoOrDie(
      R"pb(verify { validation { operation: OPERATION_GREATER_THAN } })pb");
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryValue(qv, verification, result), IsStatusInternal());
  ASSERT_THAT(result, EqualsProto(""));
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
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryValue(qv, verification, result),
              IsStatusFailedPrecondition());
  ASSERT_THAT(result, EqualsProto(""));
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

  QueryVerificationResult result;
  for (Verification::Validation::Operation operation : test_case.operations) {
    QueryValueVerification verification;
    Verification::Validation& validation =
        *verification.mutable_verify()->mutable_validation()->Add();
    validation.set_operation(operation);
    *validation.add_operands() = query_value;
    ASSERT_THAT(VerifyQueryValue(query_value, verification, result),
                IsStatusInternal());
    ASSERT_THAT(result, EqualsProto(""));
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
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryValue(test_case.query_value_a, verification, result),
              IsOk());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST_P(GreaterLesserValuesTest, GreaterThanFailure) {
  const QueryValueInputs& test_case = GetParam();
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_operation(Verification::Validation::OPERATION_GREATER_THAN);
  *validation.add_operands() = test_case.query_value_a;
  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(test_case.query_value_b, verification, result),
              IsStatusInternal());
  ASSERT_THAT(result.errors(), SizeIs(1));
  EXPECT_THAT(result.errors(0).msg(),
              HasSubstr("Failed OPERATION_GREATER_THAN check"));
}

TEST_P(GreaterLesserValuesTest, GreaterThanOrEqualSuccess) {
  const QueryValueInputs& test_case = GetParam();
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_operation(
      Verification::Validation::OPERATION_GREATER_THAN_OR_EQUAL);
  *validation.add_operands() = test_case.query_value_b;
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryValue(test_case.query_value_a, verification, result),
              IsOk());
  ASSERT_THAT(result, EqualsProto(""));

  // Test Equal Values
  result.Clear();
  EXPECT_THAT(VerifyQueryValue(test_case.query_value_b, verification, result),
              IsOk());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST_P(GreaterLesserValuesTest, GreaterThanOrEqualFailure) {
  const QueryValueInputs& test_case = GetParam();
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_operation(
      Verification::Validation::OPERATION_GREATER_THAN_OR_EQUAL);
  *validation.add_operands() = test_case.query_value_a;
  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(test_case.query_value_b, verification, result),
              IsStatusInternal());
  ASSERT_THAT(result.errors(), SizeIs(1));
  EXPECT_THAT(result.errors(0).msg(),
              HasSubstr("Failed OPERATION_GREATER_THAN_OR_EQUAL check"));
}

TEST_P(GreaterLesserValuesTest, LessThanSuccess) {
  const QueryValueInputs& test_case = GetParam();
  QueryVerificationResult result;
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_operation(Verification::Validation::OPERATION_LESS_THAN);
  *validation.add_operands() = test_case.query_value_a;
  EXPECT_THAT(VerifyQueryValue(test_case.query_value_b, verification, result),
              IsOk());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST_P(GreaterLesserValuesTest, LessThanFailure) {
  const QueryValueInputs& test_case = GetParam();
  QueryVerificationResult result;
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_operation(Verification::Validation::OPERATION_LESS_THAN);
  *validation.add_operands() = test_case.query_value_b;
  ASSERT_THAT(VerifyQueryValue(test_case.query_value_a, verification, result),
              IsStatusInternal());
  ASSERT_THAT(result.errors(), SizeIs(1));
  EXPECT_THAT(result.errors(0).msg(),
              HasSubstr("Failed OPERATION_LESS_THAN check"));
}

TEST_P(GreaterLesserValuesTest, LessThanOrEqualSuccess) {
  const QueryValueInputs& test_case = GetParam();
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_operation(
      Verification::Validation::OPERATION_LESS_THAN_OR_EQUAL);
  *validation.add_operands() = test_case.query_value_a;

  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryValue(test_case.query_value_b, verification, result),
              IsOk());
  EXPECT_THAT(result, EqualsProto(""));

  // Test Equal Values
  result.Clear();
  EXPECT_THAT(VerifyQueryValue(test_case.query_value_a, verification, result),
              IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST_P(GreaterLesserValuesTest, LessThanOrEqualFailure) {
  const QueryValueInputs& test_case = GetParam();

  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_operation(
      Verification::Validation::OPERATION_LESS_THAN_OR_EQUAL);
  *validation.add_operands() = test_case.query_value_b;

  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(test_case.query_value_a, verification, result),
              IsStatusInternal());
  ASSERT_THAT(result.errors(), SizeIs(1));
  EXPECT_THAT(result.errors(0).msg(),
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
  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(qv, verification, result), IsStatusInternal());

  ASSERT_THAT(result.errors(), SizeIs(1));
  EXPECT_THAT(result.errors(0).msg(),
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
  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(qv, verification, result), IsOk());

  ASSERT_THAT(result, EqualsProto(""));
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
  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(qv, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(""));
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

  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(qv, verification, result), IsStatusInternal());
  ASSERT_THAT(
      result,
      EqualsProto(
          R"pb(errors {
                 msg: "Failed operation OPERATION_STRING_STARTS_WITH, value: 'foobar', operand: 'bar'"
               }
               errors {
                 msg: "Failed operation OPERATION_STRING_ENDS_WITH, value: 'foobar', operand: 'foo'"
               })pb"));
}

TEST(VerifyQueryValueTest, InRangeFail) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 2)pb");
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_range(Verification::Validation::RANGE_IN);
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(int_value: 3)pb");

  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(qv, verification, result), IsStatusInternal());
  ASSERT_THAT(result,
              EqualsProto(
                  R"pb(errors {
                         msg: "Failed equality check, valueA: '2', valueB: '3'"
                       })pb"));
}

TEST(VerifyQueryValueTest, RangeUnknownOption) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 2)pb");
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_range(Verification::Validation::RANGE_UNKNOWN);
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(int_value: 2)pb");

  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(qv, verification, result),
              IsStatusFailedPrecondition());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryValueTest, RangeUnsupported) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 2)pb");
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_range(static_cast<Verification::Validation::Range>(
      Verification::Validation::RANGE_UNKNOWN - 1));
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(int_value: 2)pb");

  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(qv, verification, result),
              IsStatusFailedPrecondition());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryValueTest, RangeMissingOperands) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 2)pb");
  QueryValueVerification verification;
  verification.mutable_verify()->mutable_validation()->Add()->set_range(
      Verification::Validation::RANGE_IN);

  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(qv, verification, result),
              IsStatusFailedPrecondition());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryValueTest, InRangeSuccess) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 2)pb");
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_range(Verification::Validation::RANGE_IN);
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(int_value: 2)pb");

  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(qv, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryValueTest, NotInRangeFail) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 2)pb");
  QueryValueVerification verification;

  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_range(Verification::Validation::RANGE_IN);
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(int_value: 2)pb");
  validation.set_range(Verification::Validation::RANGE_NOT_IN);

  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(qv, verification, result), IsStatusInternal());
  ASSERT_THAT(
      result,
      EqualsProto(
          R"pb(errors {
                 msg: "Failed inequality check, valueA: '2', valueB: '2'"
               })pb"));
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

  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(qv, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryValueTest, IntervalFailSingleOperand) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 0)pb");
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_interval(Verification::Validation::INTERVAL_OPEN);
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(qv, verification, result),
              IsStatusFailedPrecondition());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryValueTest, IntervalFailMisMatchTypes) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(int_value: 0)pb");
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_interval(Verification::Validation::INTERVAL_OPEN);
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(int_value: 1)pb");
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(double_value: 1)pb");
  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(qv, verification, result),
              IsStatusFailedPrecondition());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryValueTest, IntervalFailUnsupportedType) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(string_value: "0")pb");
  QueryValueVerification verification;
  Verification::Validation& validation =
      *verification.mutable_verify()->mutable_validation()->Add();
  validation.set_interval(Verification::Validation::INTERVAL_OPEN);
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(string_value: "1")pb");
  *validation.add_operands() = ParseTextProtoOrDie(R"pb(string_value: "1")pb");
  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(qv, verification, result),
              IsStatusFailedPrecondition());
  ASSERT_THAT(result, EqualsProto(""));
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

  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(qv_b, verification, result),
              IsStatusFailedPrecondition());
  ASSERT_THAT(result, EqualsProto(""));
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
  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(qv_b, verification, result),
              IsStatusFailedPrecondition());
  ASSERT_THAT(result, EqualsProto(R"pb()pb"));
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

  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(qv_b, verification, result), IsStatusInternal());
  ASSERT_THAT(result,
              EqualsProto(
                  R"pb(
                    errors { msg: "Value '1' is not in the interval (1, 10)" }
                  )pb"));
  result.Clear();

  ASSERT_THAT(VerifyQueryValue(qv_c, verification, result), IsStatusInternal());
  ASSERT_THAT(result, EqualsProto(
                          R"pb(
                            errors {
                              msg: "Value '10' is not in the interval (1, 10)"
                            })pb"));
  result.Clear();

  validation.set_interval(Verification::Validation::INTERVAL_CLOSED);
  ASSERT_THAT(VerifyQueryValue(qv_a, verification, result), IsStatusInternal());
  ASSERT_THAT(result, EqualsProto(
                          R"pb(
                            errors {
                              msg: "Value '0' is not in the interval [1, 10]"
                            })pb"));
  result.Clear();

  ASSERT_THAT(VerifyQueryValue(qv_d, verification, result), IsStatusInternal());
  ASSERT_THAT(result, EqualsProto(
                          R"pb(
                            errors {
                              msg: "Value '100' is not in the interval [1, 10]"
                            })pb"));
  result.Clear();

  validation.set_interval(Verification::Validation::INTERVAL_OPEN_CLOSED);
  ASSERT_THAT(VerifyQueryValue(qv_b, verification, result), IsStatusInternal());
  ASSERT_THAT(result, EqualsProto(
                          R"pb(
                            errors {
                              msg: "Value '1' is not in the interval (1, 10]"
                            })pb"));
  result.Clear();

  ASSERT_THAT(VerifyQueryValue(qv_d, verification, result), IsStatusInternal());
  ASSERT_THAT(result, EqualsProto(
                          R"pb(
                            errors {
                              msg: "Value '100' is not in the interval (1, 10]"
                            })pb"));
  result.Clear();

  validation.set_interval(Verification::Validation::INTERVAL_CLOSED_OPEN);
  ASSERT_THAT(VerifyQueryValue(qv_a, verification, result), IsStatusInternal());
  ASSERT_THAT(result, EqualsProto(
                          R"pb(
                            errors {
                              msg: "Value '0' is not in the interval [1, 10)"
                            })pb"));
  result.Clear();

  ASSERT_THAT(VerifyQueryValue(qv_c, verification, result), IsStatusInternal());
  ASSERT_THAT(result, EqualsProto(
                          R"pb(
                            errors {
                              msg: "Value '10' is not in the interval [1, 10)"
                            })pb"));
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
  QueryVerificationResult result;
  ASSERT_THAT(VerifyQueryValue(qv_b, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(R"pb()pb"));
  result.Clear();

  validation.set_interval(Verification::Validation::INTERVAL_CLOSED);
  ASSERT_THAT(VerifyQueryValue(qv_a, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(R"pb()pb"));
  result.Clear();
  ASSERT_THAT(VerifyQueryValue(qv_c, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(R"pb()pb"));
  result.Clear();

  validation.set_interval(Verification::Validation::INTERVAL_OPEN_CLOSED);
  ASSERT_THAT(VerifyQueryValue(qv_b, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(R"pb()pb"));
  result.Clear();
  ASSERT_THAT(VerifyQueryValue(qv_c, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(R"pb()pb"));
  result.Clear();

  validation.set_interval(Verification::Validation::INTERVAL_CLOSED_OPEN);
  ASSERT_THAT(VerifyQueryValue(qv_a, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(R"pb()pb"));
  result.Clear();
  ASSERT_THAT(VerifyQueryValue(qv_b, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(R"pb()pb"));
  result.Clear();
}

TEST(VerifyListValueTest, MissingVerifyField) {
  ListValue lv = ParseTextProtoOrDie(R"pb(
    values { int_value: 1 }
    values { int_value: 2 }
  )pb");
  ListValueVerification verification;
  QueryVerificationResult result;
  EXPECT_THAT(VerifyListValue(lv, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(R"pb()pb"));
}

TEST(VerifyListValueTest, BasicListVerification) {
  ListValue lv = ParseTextProtoOrDie(R"pb(
    values { int_value: 1 }
    values { int_value: 2 }
  )pb");
  ListValueVerification verification = ParseTextProtoOrDie(R"pb(
    verify { verify {} }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(VerifyListValue(lv, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(R"pb()pb"));
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
  QueryVerificationResult result;
  EXPECT_THAT(VerifyListValue(lv, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(R"pb()pb"));
}

TEST(VerifyListValueTest, ListofListVerification) {
  ListValue lv = ParseTextProtoOrDie(R"pb(
    values { list_value { values { int_value: 1 } } }
  )pb");
  ListValueVerification verification = ParseTextProtoOrDie(R"pb(
    verify {}
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(VerifyListValue(lv, verification, result),
              IsStatusFailedPrecondition());
  ASSERT_THAT(result, EqualsProto(R"pb()pb"));
}

struct VerifyQueryValueInputsWithExpectations {
  QueryValue query_value;
  QueryResultDataVerification data_compare;
  internal_status::IsStatusPolyMatcher expected_status;
  QueryVerificationResult expected_result;
};
using VerificationSubqueryValueTest =
    TestWithParam<VerifyQueryValueInputsWithExpectations>;

TEST_P(VerificationSubqueryValueTest, VerifySubqueryValueTest) {
  const VerifyQueryValueInputsWithExpectations& test_case = GetParam();
  QueryVerificationResult result;
  ASSERT_THAT(VerifySubqueryValue(test_case.query_value.subquery_value(),
                                  test_case.data_compare, result),
              test_case.expected_status);
  ASSERT_THAT(result, IgnoringRepeatedFieldOrdering(
                          EqualsProto(test_case.expected_result)));
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
            .expected_status = IsOk(),
            .expected_result = ParseTextProtoOrDie(
                R"pb(errors { msg: "Missing required property 'bar'" })pb"),
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
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryResult(qr, verification, result),
              IsStatusInvalidArgument());
  ASSERT_THAT(result, EqualsProto(R"pb()pb"));
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
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryResult(qr, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(R"pb()pb"));
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
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryResult(qr, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(R"pb()pb"));
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
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryResult(qr, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(R"pb()pb"));
}

struct VerifyQueryResultErrorStrings {
  QueryResult query_result;
  QueryResultVerification verification;
  internal_status::IsStatusPolyMatcher expected_status;
  QueryVerificationResult expected_result;
};
using VerifyQueryResultErrorStringsTest =
    TestWithParam<VerifyQueryResultErrorStrings>;

TEST_P(VerifyQueryResultErrorStringsTest, VerifyQueryResult) {
  const VerifyQueryResultErrorStrings& test_case = GetParam();
  QueryVerificationResult result;
  ASSERT_THAT(
      VerifyQueryResult(test_case.query_result, test_case.verification, result),
      test_case.expected_status);
  ASSERT_THAT(result, IgnoringRepeatedFieldOrdering(
                          EqualsProto(test_case.expected_result)));
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
                fields {
                  key: "_uri_"
                  value: { string_value: "/redfish/v1/example" }
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
            .expected_result = ParseTextProtoOrDie(R"pb(
              errors {
                msg: "(path: 'query_1.key0[0]', uri: '/redfish/v1/example') Failed OPERATION_GREATER_THAN check, value: '0', operand: '0'"
                path: "query_1.key0[0]"
                uri: "/redfish/v1/example"
              }
            )pb"),
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
            .expected_result = ParseTextProtoOrDie(R"pb(
              errors {
                msg: "(path: 'query_1.key0[0].subkey0') Failed operation OPERATION_STRING_STARTS_WITH, value: 'bar', operand: 'foo'"
                path: "query_1.key0[0].subkey0"
              }
            )pb"),
        }));

using CompareQueryResultErrorStringsTest =
    TestWithParam<VerifyQueryResultErrorStrings>;

TEST_P(CompareQueryResultErrorStringsTest, CompareQueryResult) {
  const VerifyQueryResultErrorStrings& test_case = GetParam();
  QueryVerificationResult result;
  ASSERT_THAT(
      CompareQueryResults(test_case.query_result, test_case.query_result,
                          test_case.verification, result),
      test_case.expected_status);
  ASSERT_THAT(result, IgnoringRepeatedFieldOrdering(
                          EqualsProto(test_case.expected_result)));
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
            .expected_result = ParseTextProtoOrDie(R"pb(
              errors {
                msg: "(path: 'query_1.key0[0]') Failed inequality check, valueA: '0', valueB: '0'"
                path: "query_1.key0[0]"
              }
            )pb"),
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
                          fields {
                            key: "_uri_"
                            value: { string_value: "/redfish/v1/example" }
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
            .expected_result = ParseTextProtoOrDie(R"pb(
              errors {
                msg: "(path: 'query_1.key0.subkey0=\"foo\".element', uri: '/redfish/v1/example') Failed inequality check, valueA: 'bar', valueB: 'bar'"
                path: "query_1.key0.subkey0=\"foo\".element"
                uri: "/redfish/v1/example"
              }
            )pb"),
        }));

TEST(VerifyQueryResultTest, MultipleErrors) {
  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          list_compare {
            verify {
              data_compare {
                fields {
                  key: "key1"
                  value { verify { presence: PRESENCE_REQUIRED } }
                }
                fields {
                  key: "key2"
                  value { verify { presence: PRESENCE_REQUIRED } }
                }
              }
            }
          }
        }
      }
    })pb");
  QueryResult qr = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "key3"
                  value { int_value: 1 }
                }
              }
            }
          }
        }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryResult(qr, verification, result), IsStatusInternal());
  ASSERT_THAT(result, IgnoringRepeatedFieldOrdering(EqualsProto(R"pb(
                errors {
                  msg: "Missing required property 'key1'"
                  path: "query_1.key0[0]"
                }
                errors {
                  msg: "Missing required property 'key2'"
                  path: "query_1.key0[0]"
                }
              )pb")));
}

TEST(VerifyQueryResultTest, SuccessConditionalSimpleQueryResult) {
  QueryResult qr = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 0 }
      }
      fields {
        key: "conditional_key"
        value { int_value: 1 }
      }
    }
  )pb");

  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value { verify { presence: PRESENCE_REQUIRED } }
      }
      fields {
        key: "key1"
        value {
          verify { presence: PRESENCE_REQUIRED }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key"
                condition {
                  validation {
                    operation: OPERATION_GREATER_THAN_OR_EQUAL
                    operands { int_value: 1 }
                  }
                }
              }
            }
            conditional_verify { verify { presence: PRESENCE_OPTIONAL } }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { verify { presence: PRESENCE_REQUIRED } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryResult(qr, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryResultTest, SuccessAllOfSimpleQueryResult) {
  QueryResult qr = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 0 }
      }
      fields {
        key: "conditional_key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key1"
        value { string_value: "/phys" }
      }
    }
  )pb");

  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          verify {
            validation {
              operation: OPERATION_GREATER_THAN_OR_EQUAL
              operands { int_value: 1 }
            }
          }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key0"
                condition {
                  validation {
                    operation: OPERATION_GREATER_THAN_OR_EQUAL
                    operands { int_value: 0 }
                  }
                }
              }
              conditions {
                property_name: "conditional_key1"
                condition {
                  validation {
                    operation: OPERATION_STRING_CONTAINS
                    operands { string_value: "/phys" }
                  }
                }
              }
            }
            conditional_verify {
              verify {
                validation {
                  operation: OPERATION_LESS_THAN
                  operands { int_value: 1 }
                }
              }
            }
          }
        }
      }
      fields {
        key: "conditional_key0"
        value { verify { presence: PRESENCE_REQUIRED } }
      }
      fields {
        key: "conditional_key1"
        value { verify { presence: PRESENCE_REQUIRED } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryResult(qr, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryResultTest, SuccessAnyOfSimpleQueryResult) {
  QueryResult qr = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 0 }
      }
      fields {
        key: "conditional_key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key1"
        value { string_value: "/phys" }
      }
    }
  )pb");

  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          verify {
            validation {
              operation: OPERATION_GREATER_THAN_OR_EQUAL
              operands { int_value: 1 }
            }
          }
          overrides {
            any_of {
              conditions {
                property_name: "conditional_key0"
                condition {
                  validation {
                    operation: OPERATION_LESS_THAN
                    operands { int_value: 0 }
                  }
                }
              }
              conditions {
                property_name: "conditional_key1"
                condition {
                  validation {
                    operation: OPERATION_STRING_CONTAINS
                    operands { string_value: "/phys" }
                  }
                }
              }
            }
            conditional_verify {
              verify {
                validation {
                  operation: OPERATION_LESS_THAN
                  operands { int_value: 1 }
                }
              }
            }
          }
        }
      }
      fields {
        key: "conditional_key0"
        value { verify { presence: PRESENCE_REQUIRED } }
      }
      fields {
        key: "conditional_key1"
        value { verify { presence: PRESENCE_REQUIRED } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryResult(qr, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryResultTest, SuccessMultipleOverridesSimpleQueryResult) {
  QueryResult qr = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 0 }
      }
      fields {
        key: "conditional_key"
        value { string_value: "/phys/something" }
      }
    }
  )pb");

  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          verify {
            validation {
              operation: OPERATION_GREATER_THAN
              operands { int_value: 1 }
            }
          }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key"
                condition {
                  validation {
                    operation: OPERATION_STRING_NOT_CONTAINS
                    operands { string_value: "/phys" }
                  }
                }
              }
            }
            conditional_verify {
              verify {
                validation {
                  operation: OPERATION_GREATER_THAN_OR_EQUAL
                  operands { int_value: 1 }
                }
              }
            }
          }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key"
                condition {
                  validation {
                    operation: OPERATION_STRING_STARTS_WITH
                    operands { string_value: "/phys" }
                  }
                }
              }
            }
            conditional_verify {
              verify {
                validation {
                  operation: OPERATION_LESS_THAN
                  operands { int_value: 1 }
                }
              }
            }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { verify { presence: PRESENCE_REQUIRED } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryResult(qr, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryResultTest, SuccessConditionalSubqueryQueryResult) {
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
      fields {
        key: "conditional_key"
        value { int_value: 1 }
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
              value { verify { presence: PRESENCE_REQUIRED } }
            }
            fields {
              key: "key2"
              value { verify { presence: PRESENCE_REQUIRED } }
            }
          }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key"
                condition {
                  validation {
                    operation: OPERATION_GREATER_THAN_OR_EQUAL
                    operands { int_value: 1 }
                  }
                }
              }
            }
            conditional_verify {
              data_compare {
                fields {
                  key: "key1"
                  value { verify { presence: PRESENCE_REQUIRED } }
                }
                fields {
                  key: "key2"
                  value { verify { presence: PRESENCE_OPTIONAL } }
                }
              }
            }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { verify { presence: PRESENCE_REQUIRED } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryResult(qr, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryResultTest, SuccessConditionalListQueryResult) {
  QueryResult qr = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value {
          list_value {
            values { int_value: 0 }
            values { int_value: 1 }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { int_value: 1 }
      }
    }
  )pb");

  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
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
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key"
                condition {
                  validation {
                    operation: OPERATION_GREATER_THAN_OR_EQUAL
                    operands { int_value: 1 }
                  }
                }
              }
            }
            conditional_verify {
              list_compare {
                verify {
                  verify {
                    validation {
                      operation: OPERATION_GREATER_THAN_OR_EQUAL
                      operands { int_value: 0 }
                    }
                  }
                }
              }
            }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { verify { presence: PRESENCE_REQUIRED } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryResult(qr, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryResultTest, SuccessConditionalSimpleQueryResultDefault) {
  QueryResult qr = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 0 }
      }
      fields {
        key: "conditional_key"
        value { int_value: 1 }
      }
    }
  )pb");

  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          verify {
            validation {
              operation: OPERATION_LESS_THAN
              operands { int_value: 1 }
            }
          }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key"
                condition {
                  validation {
                    operation: OPERATION_LESS_THAN
                    operands { int_value: 0 }
                  }
                }
              }
            }
            conditional_verify {
              verify {
                validation {
                  operation: OPERATION_GREATER_THAN_OR_EQUAL
                  operands { int_value: 1 }
                }
              }
            }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { verify { presence: PRESENCE_REQUIRED } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryResult(qr, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryResultTest, SuccessAllOfSimpleQueryResultDefault) {
  QueryResult qr = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 0 }
      }
      fields {
        key: "conditional_key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key1"
        value { string_value: "/phys" }
      }
    }
  )pb");

  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          verify {
            validation {
              operation: OPERATION_LESS_THAN
              operands { int_value: 1 }
            }
          }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key0"
                condition {
                  validation {
                    operation: OPERATION_GREATER_THAN_OR_EQUAL
                    operands { int_value: 0 }
                  }
                }
              }
              conditions {
                property_name: "conditional_key1"
                condition {
                  validation {
                    operation: OPERATION_STRING_CONTAINS
                    operands { string_value: "/phys1" }
                  }
                }
              }
            }
            conditional_verify {
              verify {
                validation {
                  operation: OPERATION_GREATER_THAN_OR_EQUAL
                  operands { int_value: 1 }
                }
              }
            }
          }
        }
      }
      fields {
        key: "conditional_key0"
        value { verify { presence: PRESENCE_REQUIRED } }
      }
      fields {
        key: "conditional_key1"
        value { verify { presence: PRESENCE_REQUIRED } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryResult(qr, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryResultTest, SuccessAnyOfSimpleQueryResultDefault) {
  QueryResult qr = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 0 }
      }
      fields {
        key: "conditional_key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key1"
        value { string_value: "/phys" }
      }
    }
  )pb");

  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          verify {
            validation {
              operation: OPERATION_LESS_THAN
              operands { int_value: 1 }
            }
          }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key0"
                condition {
                  validation {
                    operation: OPERATION_GREATER_THAN_OR_EQUAL
                    operands { int_value: 0 }
                  }
                }
              }
              conditions {
                property_name: "conditional_key1"
                condition {
                  validation {
                    operation: OPERATION_STRING_CONTAINS
                    operands { string_value: "/phys1" }
                  }
                }
              }
            }
            conditional_verify {
              verify {
                validation {
                  operation: OPERATION_GREATER_THAN_OR_EQUAL
                  operands { int_value: 1 }
                }
              }
            }
          }
        }
      }
      fields {
        key: "conditional_key0"
        value { verify { presence: PRESENCE_REQUIRED } }
      }
      fields {
        key: "conditional_key1"
        value { verify { presence: PRESENCE_REQUIRED } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryResult(qr, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryResultTest, SuccessConditionalSubqueryQueryResultDefault) {
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
      fields {
        key: "conditional_key"
        value { int_value: 1 }
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
              value {
                verify {
                  validation {
                    operation: OPERATION_LESS_THAN
                    operands { int_value: 1 }
                  }
                }
              }
            }
          }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key"
                condition {
                  validation {
                    operation: OPERATION_LESS_THAN
                    operands { int_value: 0 }
                  }
                }
              }
            }
            conditional_verify {
              data_compare {
                fields {
                  key: "key1"
                  value {
                    verify {
                      validation {
                        operation: OPERATION_GREATER_THAN_OR_EQUAL
                        operands { int_value: 1 }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { verify { presence: PRESENCE_REQUIRED } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryResult(qr, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryResultTest, SuccessConditionalListQueryResultDefault) {
  QueryResult qr = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value {
          list_value {
            values { int_value: 0 }
            values { int_value: 1 }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { int_value: 1 }
      }
    }
  )pb");

  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          list_compare {
            verify {
              verify {
                validation {
                  operation: OPERATION_LESS_THAN_OR_EQUAL
                  operands { int_value: 1 }
                }
              }
            }
          }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key"
                condition {
                  validation {
                    operation: OPERATION_LESS_THAN
                    operands { int_value: 1 }
                  }
                }
              }
            }
            conditional_verify {
              list_compare {
                verify {
                  verify {
                    validation {
                      operation: OPERATION_GREATER_THAN
                      operands { int_value: 1 }
                    }
                  }
                }
              }
            }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { verify { presence: PRESENCE_REQUIRED } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryResult(qr, verification, result), IsOk());
  ASSERT_THAT(result, EqualsProto(""));
}

TEST(VerifyQueryResultTest, SuccessMultipleOverridesSimpleQueryResultDefault) {
  QueryResult qr = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value { int_value: 1 }
      }
      fields {
        key: "conditional_key"
        value { string_value: "/phys/something" }
      }
    }
  )pb");

  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          verify {
            validation {
              operation: OPERATION_GREATER_THAN_OR_EQUAL
              operands { int_value: 1 }
            }
          }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key"
                condition {
                  validation {
                    operation: OPERATION_STRING_NOT_CONTAINS
                    operands { string_value: "/phys" }
                  }
                }
              }
            }
            conditional_verify {
              verify {
                validation {
                  operation: OPERATION_GREATER_THAN
                  operands { int_value: 1 }
                }
              }
            }
          }
          overrides {
            all_of {
              conditions {
                property_name: "conditional_key"
                condition {
                  validation {
                    operation: OPERATION_STRING_ENDS_WITH
                    operands { string_value: "/phys" }
                  }
                }
              }
            }
            conditional_verify {
              verify {
                validation {
                  operation: OPERATION_LESS_THAN
                  operands { int_value: 1 }
                }
              }
            }
          }
        }
      }
      fields {
        key: "conditional_key"
        value { verify { presence: PRESENCE_REQUIRED } }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryResult(qr, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
}

struct StableIdRequirementTestCase {
  std::string test_name;
  QueryResult query_result;
  std::vector<internal_status::IsStatusPolyMatcher> status_matchers;
};

using VerifyQueryResultStableIdTest =
    TestWithParam<StableIdRequirementTestCase>;

TEST_P(VerifyQueryResultStableIdTest, TestStableIdRequirements) {
  const StableIdRequirementTestCase& test_case = GetParam();

  QueryResultVerification verification = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data_verify {
      fields {
        key: "key0"
        value {
          list_compare {
            verify {
              data_compare {
                fields {
                  key: "_id_"
                  value {
                    verify {
                      presence: PRESENCE_REQUIRED
                      stable_id_requirement: STABLE_ID_PRESENCE_NONE
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
  QueryVerificationResult result;
  for (int i = 0; i < Verification::StableIdRequirement_ARRAYSIZE; ++i) {
    Verification::StableIdRequirement requirement =
        static_cast<Verification::StableIdRequirement>(i);
    verification.mutable_data_verify()
        ->mutable_fields()
        ->at("key0")
        .mutable_list_compare()
        ->mutable_verify()
        ->mutable_data_compare()
        ->mutable_fields()
        ->at("_id_")
        .mutable_verify()
        ->set_stable_id_requirement(requirement);
    result.Clear();
    EXPECT_THAT(VerifyQueryResult(test_case.query_result, verification, result),
                test_case.status_matchers[requirement]);
  }
}

INSTANTIATE_TEST_SUITE_P(
    VerifyQueryResultStableIdTest, VerifyQueryResultStableIdTest,
    Values(
        StableIdRequirementTestCase{
            .test_name = "RequirementNone",
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
                            key: "_id_"
                            value {
                              identifier {
                                redfish_location {
                                  service_label: "DIMM0"
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
            .status_matchers = {IsOk(), IsOk(), IsStatusInternal(),
                                IsStatusInternal()},
        },
        StableIdRequirementTestCase{
            .test_name = "RequirementDevpath",
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
                            key: "_id_"
                            value {
                              identifier {
                                local_devpath: "/phys/DIMM0"
                                machine_devpath: "/phys/DIMM0"
                                redfish_location {
                                  service_label: "DIMM0"
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
            .status_matchers = {IsOk(), IsOk(), IsOk(), IsStatusInternal()},
        },
        StableIdRequirementTestCase{
            .test_name = "RequirementSubfru",
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
                            key: "_id_"
                            value {
                              identifier {
                                local_devpath: "/phys/CPU0"
                                machine_devpath: "/phys/CPU0"
                                embedded_location_context: "die0_core0:thread0"
                                redfish_location {
                                  service_label: "CPU0"
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
            .status_matchers = {IsOk(), IsOk(), IsOk(), IsOk()},
        }
      )
);

TEST(VerifyQueryResultTest, SuccessConditionalStableIdQueryResult) {
  QueryResult qr = ParseTextProtoOrDie(R"pb(
    query_id: "query_1"
    data {
      fields {
        key: "key0"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "_id_"
                  value {
                    identifier {
                      local_devpath: "/phys/DIMM0"
                      machine_devpath: "/phys/DIMM0"
                      redfish_location {
                        service_label: "DIMM0"
                      }
                    }
                  }
                }
                fields {
                  key: "present_in_firmware"
                  value {
                    string_value: "Enabled"
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
                      redfish_location {
                        service_label: "DIMM1"
                      }
                    }
                  }
                }
                fields {
                  key: "present_in_firmware"
                  value {
                    string_value: "Absent"
                  }
                }
              }
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
          list_compare {
            verify {
              data_compare {
                fields {
                  key: "_id_"
                  value {
                    verify {
                      presence: PRESENCE_REQUIRED
                      stable_id_requirement: STABLE_ID_PRESENCE_DEVPATH
                    }
                    overrides {
                      all_of {
                        conditions {
                          property_name: "present_in_firmware"
                          condition {
                            validation {
                              operation: OPERATION_STRING_NOT_CONTAINS
                              operands { string_value: "Enabled" }
                            }
                          }
                        }
                      }
                      conditional_verify {
                        verify {
                          presence: PRESENCE_REQUIRED
                          stable_id_requirement: STABLE_ID_PRESENCE_NONE
                        }
                      }
                    }
                  }
                }
                fields {
                  key: "present_in_firmware"
                  value { verify { presence: PRESENCE_REQUIRED } }
                }
              }
            }
          }
        }
      }
    }
  )pb");
  QueryVerificationResult result;
  EXPECT_THAT(VerifyQueryResult(qr, verification, result), IsOk());
  EXPECT_THAT(result, EqualsProto(""));
};

}  // namespace
}  // namespace ecclesia
