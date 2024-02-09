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

#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result_validator.h"

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/status/test_macros.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

TEST(CreateValidatorTest, FailCreateEmptyQueryCofnig) {
  auto validator = QueryResultValidator::Create(nullptr);
  ASSERT_THAT(validator.status(), IsStatusInvalidArgument());
  EXPECT_EQ(validator.status().message(), "Query Configuration is Empty");
}
TEST(QueryResultValidatorTest, FailCreateEmptyQueryId) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    subquery {
      subquery_id: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");

  absl::StatusOr<std::unique_ptr<QueryResultValidatorIntf>> validator =
      QueryResultValidator::Create(&query);
  ASSERT_THAT(validator.status(), IsStatusInvalidArgument());
  EXPECT_EQ(validator.status().message(), "Query ID is empty");
}
TEST(CreateValidatorTest, FailCreateMissingRootId) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");
  absl::StatusOr<std::unique_ptr<QueryResultValidatorIntf>> validator =
      QueryResultValidator::Create(&query);
  ASSERT_THAT(validator.status(), IsStatusNotFound());
  EXPECT_EQ(validator.status().message(), "No subquery found for subquery_id");
}
TEST(CreateValidatorTest, FailCreateDuplicatePaths) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id1"
      properties { property: "property" type: STRING }
    }
  )pb");
  absl::StatusOr<std::unique_ptr<QueryResultValidatorIntf>> validator =
      QueryResultValidator::Create(&query);
  ASSERT_THAT(validator.status(), IsStatusInternal());
  EXPECT_EQ(validator.status().message(),
            "Duplicate path: /subquery_id1/subquery_id2");
}

TEST(CreateValidatorTest, FailCreateCycleDetected) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id2"
      properties { property: "property" type: STRING }
    }
  )pb");
  absl::StatusOr<std::unique_ptr<QueryResultValidatorIntf>> validator =
      QueryResultValidator::Create(&query);
  ASSERT_THAT(validator.status(), IsStatusInternal());
  EXPECT_EQ(validator.status().message(), "Cycle detected: subquery_id2");
}
TEST(CreateValidatorTest, SuccessCreateValidator) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");
  EXPECT_THAT(QueryResultValidator::Create(&query), IsOk());
}
TEST(CreateValidatorTest, SuccessCreateValidator2Levels) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id1"
      properties { property: "property" type: STRING }
    }
  )pb");
  EXPECT_THAT(QueryResultValidator::Create(&query), IsOk());
}

TEST(CreateValidatorTest, SuccessCreateValidator2Branched2Levels) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id3"
      root_subquery_ids: "subquery_id1"
      properties { property: "property" type: STRING }
    }
  )pb");
  EXPECT_THAT(QueryResultValidator::Create(&query), IsOk());
}

TEST(CreateValidatorTest, SuccessCreateValidatorMultiBranch) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id3"
      root_subquery_ids: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id3"
      properties { property: "property" type: STRING }
    }
  )pb");
  EXPECT_THAT(QueryResultValidator::Create(&query), IsOk());
}

TEST(QueryResultValidatorTest, FailValidateEmptyResult) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");

  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<QueryResultValidator> validator,
                          QueryResultValidator::Create(&query));

  absl::Status status = validator->Validate({});
  ASSERT_THAT(status, IsStatusInvalidArgument());
  EXPECT_EQ(status.message(), "Query result is empty");
}

TEST(QueryResultValidatorTest, FailWrongQueryId) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");

  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<QueryResultValidator> validator,
                          QueryResultValidator::Create(&query));

  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "Different_id"
    data {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "Differentproperty"
                  value { string_value: "OK" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  absl::Status status = validator->Validate(result);
  ASSERT_THAT(status, IsStatusInvalidArgument());
  EXPECT_EQ(status.message(), "Query id mismatch: Different_id vs query_id");
}

TEST(QueryResultValidatorTest, FailWrongPropertyValidate) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");

  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<QueryResultValidator> validator,
                          QueryResultValidator::Create(&query));

  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    data {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "Differentproperty"
                  value { string_value: "OK" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  absl::Status status = validator->Validate(result);
  ASSERT_THAT(status, IsStatusInternal());
  EXPECT_EQ(status.message(),
            "Unknown /subquery_id/Differentproperty missing from "
            "configuration");
}

TEST(QueryResultValidatorTest, FailWrongTypeValidate) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");

  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<QueryResultValidator> validator,
                          QueryResultValidator::Create(&query));

  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    data {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "property"
                  value { int_value: 6 }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  absl::Status status = validator->Validate(result);
  ASSERT_THAT(status, IsStatusInternal());
  EXPECT_EQ(status.message(),
            "Unexpected value type for /subquery_id/property");
}

TEST(QueryResultValidatorTest, FailWrongTypeValidatePart2) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id"
      properties { property: "property" type: INT64 }
    }
  )pb");

  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<QueryResultValidator> validator,
                          QueryResultValidator::Create(&query));

  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    data {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "property"
                  value { string_value: "ok" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  absl::Status status = validator->Validate(result);
  ASSERT_THAT(status, IsStatusInternal());
  EXPECT_EQ(status.message(),
            "Unexpected value type for /subquery_id/property");
}

TEST(QueryResultValidatorTest, FailEmptyValueValidate) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");

  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<QueryResultValidator> validator,
                          QueryResultValidator::Create(&query));

  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    data {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "property"
                  value {}
                }
              }
            }
          }
        }
      }
    }
  )pb");

  absl::Status status = validator->Validate(result);
  ASSERT_THAT(status, IsStatusInternal());
  EXPECT_EQ(status.message(), "Empty value for /subquery_id/property");
}

TEST(QueryResultValidatorTest, FailCompareComplexType) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");

  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<QueryResultValidator> validator,
                          QueryResultValidator::Create(&query));

  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    data {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "property"
                  value {
                    subquery_value {
                      fields {
                        key: "sub-property"
                        value { string_value: "abc" }
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

  absl::Status status = validator->Validate(result);
  ASSERT_THAT(status, IsStatusInternal());
  EXPECT_EQ(status.message(),
            "Unexpected value type for /subquery_id/property");
}

TEST(QueryResultValidatorTest, SuccessValidateSubquery) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");

  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<QueryResultValidator> validator,
                          QueryResultValidator::Create(&query));

  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    data {
      fields {
        key: "subquery_id"
        value {
          subquery_value {
            fields {
              key: "property"
              value { string_value: "OK" }
            }
          }
        }
      }
    }
  )pb");

  EXPECT_THAT(validator->Validate(result), IsOk());
}

TEST(QueryResultValidatorTest, SuccessValidate) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");

  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<QueryResultValidator> validator,
                          QueryResultValidator::Create(&query));

  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    data {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "property"
                  value { string_value: "OK" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  EXPECT_THAT(validator->Validate(result), IsOk());
}

TEST(QueryResultValidatorTest, SuccessValidatorMultiBranch) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id3"
      root_subquery_ids: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id3"
      properties { property: "property" type: STRING }
    }
  )pb");
  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<QueryResultValidator> validator,
                          QueryResultValidator::Create(&query));

  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    data {
      fields {
        key: "subquery_id1"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "property"
                  value { string_value: "OK" }
                }
                fields {
                  key: "subquery_id2"
                  value {
                    list_value {
                      values {
                        subquery_value {
                          fields {
                            key: "property"
                            value { string_value: "OK" }
                          }
                        }
                      }
                    }
                  }
                }
                fields {
                  key: "subquery_id3"
                  value {
                    list_value {
                      values {
                        subquery_value {
                          fields {
                            key: "property"
                            value { string_value: "OK" }
                          }
                          fields {
                            key: "subquery_id2"
                            value {
                              list_value {
                                values {
                                  subquery_value {
                                    fields {
                                      key: "property"
                                      value { string_value: "OK" }
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
            }
          }
        }
      }
    }
  )pb");
  EXPECT_THAT(validator->Validate(result), IsOk());
}

TEST(QueryResultValidatorTest, FailWrongCollectionType) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id"
      properties {
        property: "property"
        type: STRING
        property_element_type: COLLECTION_PRIMITIVE
      }
    }
  )pb");

  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<QueryResultValidator> validator,
                          QueryResultValidator::Create(&query));

  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    data {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "property"
                  value { string_value: "Ok" }
                }
              }
            }
          }
        }
      }
    }
  )pb");

  absl::Status status = validator->Validate(result);
  ASSERT_THAT(status, IsStatusInternal());
  EXPECT_EQ(status.message(),
            "Unexpected value type for /subquery_id/property");
}

TEST(QueryResultValidatorTest, FailWrongListValueValidate) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id"
      properties {
        property: "property"
        type: STRING
        property_element_type: COLLECTION_PRIMITIVE
      }
    }
  )pb");

  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<QueryResultValidator> validator,
                          QueryResultValidator::Create(&query));

  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    data {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "property"
                  value {
                    list_value {
                      values { string_value: "Ok" }
                      values { int_value: 1 }
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

  absl::Status status = validator->Validate(result);
  ASSERT_THAT(status, IsStatusInternal());
  EXPECT_EQ(status.message(),
            "Unexpected value type for /subquery_id/property");
}

TEST(QueryResultValidatorTest, FailEmptyListValueValidate) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id"
      properties {
        property: "property"
        type: STRING
        property_element_type: COLLECTION_PRIMITIVE
      }
    }
  )pb");

  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<QueryResultValidator> validator,
                          QueryResultValidator::Create(&query));

  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    data {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "property"
                  value {
                    list_value {
                      values { string_value: "Ok" }
                      values {}
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

  absl::Status status = validator->Validate(result);
  ASSERT_THAT(status, IsStatusInternal());
  EXPECT_EQ(status.message(), "Empty value for /subquery_id/property");
}

TEST(QueryResultValidatorTest, SuccessListValidate) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id"
      properties {
        property: "property"
        type: STRING
        property_element_type: COLLECTION_PRIMITIVE
      }
    }
  )pb");

  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<QueryResultValidator> validator,
                          QueryResultValidator::Create(&query));

  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    data {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "property"
                  value {
                    list_value {
                      values { string_value: "Ok" }
                      values { string_value: "dokey" }
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

  EXPECT_THAT(validator->Validate(result), IsOk());
}

TEST(QueryResultValidatorTest, SuccessStatelessValidator) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");
  QueryResult result = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    data {
      fields {
        key: "subquery_id"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "property"
                  value { string_value: "OK" }
                }
              }
            }
          }
        }
      }
    }
  )pb");
  EXPECT_THAT(ValidateQueryResult(query, result), IsOk());
}

}  // namespace
}  // namespace ecclesia
