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

using ::testing::Contains;
using ::testing::ContainsRegex;
using ::testing::ElementsAreArray;
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

TEST(CompareSubqueryValues, MisMatchingQueryType) {
  QueryValue list = ParseTextProtoOrDie(R"pb(list_value {})pb");
  QueryValue subquery = ParseTextProtoOrDie(R"pb(subquery_value {})pb");
  QueryResultDataVerification verification;
  std::vector<std::string> errors;
  EXPECT_THAT(CompareSubqueryValues(list, subquery, verification, errors),
              IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(CompareSubqueryValues, MissingPropertyInSubqueryA) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(subquery_value {})pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(subquery_value {
                                               fields {
                                                 key: "foo"
                                                 value: { int_value: 1 }
                                               }
                                             })pb");
  QueryResultDataVerification verification = ParseTextProtoOrDie(R"pb(
    fields {
      key: "foo"
      value { verify { comparison: COMPARE_EQUAL } }
    }
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareSubqueryValues(qv_a, qv_b, verification, errors), IsOk());
  EXPECT_THAT(errors, ElementsAreArray({"Missing property foo in valueA"}));
}

TEST(CompareSubqueryValues, MissingPropertyInSubqueryBoth) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(subquery_value {})pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(subquery_value {})pb");
  QueryResultDataVerification verification = ParseTextProtoOrDie(R"pb(
    fields {
      key: "foo"
      value { verify { comparison: COMPARE_EQUAL } }
    }
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareSubqueryValues(qv_a, qv_b, verification, errors), IsOk());
  EXPECT_THAT(errors, ElementsAreArray({"Missing property foo in valueA",
                                        "Missing property foo in valueB"}));
}

TEST(CompareSubqueryValues, MisMatchingValue) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(subquery_value {
                                               fields {
                                                 key: "foo"
                                                 value: { int_value: 1 }
                                               }
                                             })pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(subquery_value {
                                               fields {
                                                 key: "foo"
                                                 value: { string_value: "1" }
                                               }
                                             })pb");
  QueryResultDataVerification verification = ParseTextProtoOrDie(R"pb(
    fields {
      key: "foo"
      value { verify { comparison: COMPARE_EQUAL } }
    }
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareSubqueryValues(qv_a, qv_b, verification, errors),
              IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(CompareSubqueryValues, EmptyVerification) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(subquery_value {})pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(subquery_value {})pb");
  QueryResultDataVerification verification;
  std::vector<std::string> errors;
  EXPECT_THAT(CompareSubqueryValues(qv_a, qv_b, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(CompareSubqueryValues, SucessBasicValue) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(subquery_value {
                                               fields {
                                                 key: "foo"
                                                 value: { int_value: 1 }
                                               }
                                             })pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(subquery_value {
                                               fields {
                                                 key: "foo"
                                                 value: { int_value: 1 }
                                               }
                                             })pb");
  QueryResultDataVerification verification = ParseTextProtoOrDie(R"pb(
    fields {
      key: "foo"
      value { verify { comparison: COMPARE_EQUAL } }
    }
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareSubqueryValues(qv_a, qv_b, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(CompareSubqueryValues, SucessSubquery) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(subquery_value {
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
                                             })pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(subquery_value {
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
                                             })pb");
  QueryResultDataVerification verification = ParseTextProtoOrDie(R"pb(
    fields {
      key: "foo"
      value {
        data_compare {
          fields {
            key: "bar"
            value: { verify { comparison: COMPARE_EQUAL } }
          }
        }
      }
    }
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareSubqueryValues(qv_a, qv_b, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(CompareSubqueryValues, SucessList) {
  QueryValue qv_a = ParseTextProtoOrDie(
      R"pb(subquery_value {
             fields {
               key: "foo"
               value: { list_value { values { int_value: 1 } } }
             }
           })pb");
  QueryValue qv_b = ParseTextProtoOrDie(
      R"pb(subquery_value {
             fields {
               key: "foo"
               value: { list_value { values { int_value: 1 } } }
             }
           })pb");
  QueryResultDataVerification verification = ParseTextProtoOrDie(R"pb(
    fields {
      key: "foo"
      value { list_compare { verify { verify { comparison: COMPARE_EQUAL } } } }
    }
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareSubqueryValues(qv_a, qv_b, verification, errors), IsOk());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(CompareListValuesTest, MisMatchingQueryType) {
  QueryValue list = ParseTextProtoOrDie(R"pb(list_value {})pb");
  QueryValue subquery = ParseTextProtoOrDie(R"pb(subquery_value {})pb");
  ListValueVerification verification;
  std::vector<std::string> errors;
  EXPECT_THAT(CompareListValues(list, subquery, verification, errors),
              IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(CompareListValuesTest, MissingIndexAsIndentifier) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(list_value {
                                               values {
                                                 subquery_value {
                                                   fields {
                                                     key: "foo"
                                                     value: { int_value: 1 }
                                                   }
                                                 }
                                               }
                                             })pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(list_value {})pb");

  ListValueVerification verification;
  std::vector<std::string> errors;
  EXPECT_THAT(CompareListValues(qv_a, qv_b, verification, errors),
              IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0],
              HasSubstr("Missing value in valueB with identifier index=0"));
}

TEST(CompareListValuesTest, ListofLists) {
  QueryValue qv_a =
      ParseTextProtoOrDie(R"pb(list_value { values { list_value {} } })pb");
  QueryValue qv_b =
      ParseTextProtoOrDie(R"pb(list_value { values { list_value {} } })pb");

  ListValueVerification verification;
  std::vector<std::string> errors;
  EXPECT_THAT(CompareListValues(qv_a, qv_b, verification, errors),
              IsStatusFailedPrecondition());
  ASSERT_THAT(errors, IsEmpty());
}

TEST(CompareListValuesTest, EmptyListValue) {
  QueryValue qv = ParseTextProtoOrDie(R"pb(list_value {})pb");
  ListValueVerification verification;
  std::vector<std::string> errors;
  EXPECT_THAT(CompareListValues(qv, qv, verification, errors), IsOk());
  EXPECT_THAT(errors, IsEmpty());
}

TEST(CompareListValuesTest, EmptyListValueWithIdentifier) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(list_value { values {} })pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(list_value {})pb");
  ListValueVerification verification = ParseTextProtoOrDie(R"pb(
    identifiers: "foo"
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareListValues(qv_a, qv_b, verification, errors),
              IsStatusInternal());
  EXPECT_THAT(errors,
              ElementsAreArray({"Missing identifier in valueA: Identifiers are "
                                "only supported for subquery values"}));
}

TEST(CompareListValuesTest, MissingPropertyInSubqueryA) {
  QueryValue qv_a =
      ParseTextProtoOrDie(R"pb(list_value { values { subquery_value {} } })pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(list_value {})pb");
  ListValueVerification verification = ParseTextProtoOrDie(R"pb(
    identifiers: "foo"
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareListValues(qv_a, qv_b, verification, errors),
              IsStatusInternal());
  EXPECT_THAT(
      errors,
      ElementsAreArray(
          {"Missing identifier in valueA: property foo is not present"}));
}

TEST(CompareListValuesTest, DuplicateIdentifierInSubqueryA) {
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
                                                     value: { int_value: 1 }
                                                   }
                                                 }
                                               }
                                             })pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(list_value {
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
    identifiers: "foo"
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareListValues(qv_a, qv_b, verification, errors),
              IsStatusInternal());
  EXPECT_THAT(errors,
              ElementsAreArray({"Duplicate identifier in valueA: foo=1"}));
}

TEST(CompareListValuesTest, MissingPropertyInSubqueryB) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(list_value {
                                               values {
                                                 subquery_value {
                                                   fields {
                                                     key: "foo"
                                                     value: { int_value: 1 }
                                                   }
                                                 }
                                               }
                                             })pb");
  QueryValue qv_b =
      ParseTextProtoOrDie(R"pb(list_value { values { subquery_value {} } })pb");
  ListValueVerification verification = ParseTextProtoOrDie(R"pb(
    identifiers: "foo"
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareListValues(qv_a, qv_b, verification, errors),
              IsStatusInternal());
  EXPECT_THAT(
      errors,
      Contains("Missing identifier in valueB: property foo is not present"));
}

TEST(CompareListValuesTest, DuplicateIdentifierInSubqueryB) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(list_value {
                                               values {
                                                 subquery_value {
                                                   fields {
                                                     key: "foo"
                                                     value: { int_value: 1 }
                                                   }
                                                 }
                                               }
                                             })pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(list_value {
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
                                             })pb");

  ListValueVerification verification = ParseTextProtoOrDie(R"pb(
    identifiers: "foo"
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareListValues(qv_a, qv_b, verification, errors),
              IsStatusInternal());
  EXPECT_THAT(errors,
              ElementsAreArray({"Duplicate identifier in valueB: foo=1"}));
}

TEST(CompareListValuesTest, MissingIdentifierInSubqueryA) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(list_value {})pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(list_value {
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
    identifiers: "foo"
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareListValues(qv_a, qv_b, verification, errors),
              IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0],
              HasSubstr("Missing value in valueA with identifier foo=1"));
}

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
  EXPECT_THAT(CompareListValues(qv_a, qv_b, verification, errors),
              IsStatusInternal());
  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_THAT(errors[0],
              ContainsRegex("Failed equality check, valueA: '.', valueB: '.'"));
}

TEST(CompareListValuesTest, SucessCompareQueryValues) {
  QueryValue qv =
      ParseTextProtoOrDie(R"pb(list_value { values { int_value: 1 } })pb");
  ListValueVerification verification = ParseTextProtoOrDie(R"pb(
    verify { verify { comparison: COMPARE_EQUAL } }
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareListValues(qv, qv, verification, errors), IsOk());
  EXPECT_THAT(errors, IsEmpty());
}

TEST(CompareListValuesTest, SucessIndexAsIndentifier) {
  QueryValue qv_a = ParseTextProtoOrDie(R"pb(list_value {
                                               values {
                                                 subquery_value {
                                                   fields {
                                                     key: "foo"
                                                     value: { int_value: 1 }
                                                   }
                                                 }
                                               }
                                             })pb");
  QueryValue qv_b = ParseTextProtoOrDie(R"pb(list_value {
                                               values {
                                                 subquery_value {
                                                   fields {
                                                     key: "foo"
                                                     value: { int_value: 1 }
                                                   }
                                                 }
                                               }
                                             })pb");

  ListValueVerification verification;
  std::vector<std::string> errors;
  EXPECT_THAT(CompareListValues(qv_a, qv_b, verification, errors), IsOk());
  EXPECT_THAT(errors, IsEmpty());
}

TEST(CompareListValuesTest, SucessWithDefinedIndentifier) {
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
    identifiers: "foo"
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareListValues(qv_a, qv_b, verification, errors), IsOk());
  EXPECT_THAT(errors, IsEmpty());
}

TEST(CompareListValuesTest, SucessWithDefinedIndentifierAsDevpath) {
  QueryValue qv_a =
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
                               })pb");
  QueryValue qv_b =
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
                               })pb");

  ListValueVerification verification = ParseTextProtoOrDie(R"pb(
    identifiers: "_id_"
  )pb");
  std::vector<std::string> errors;
  EXPECT_THAT(CompareListValues(qv_a, qv_b, verification, errors), IsOk());
  EXPECT_THAT(errors, IsEmpty());
}

TEST(CompareQueryResultsTest, Unimplemented) {
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
  EXPECT_THAT(CompareQueryResults(qr_a, qr_a, verification, errors),
              IsStatusUnimplemented());
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
