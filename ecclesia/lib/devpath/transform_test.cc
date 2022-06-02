/*
 * Copyright 2020 Google LLC
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

#include "ecclesia/lib/devpath/transform.h"

#include <algorithm>
#include <optional>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/devpath/transform_test.pb.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/testing/proto.h"

namespace ecclesia {
namespace {

using ::ecclesia_transform_test::ComplicatedMessage;
using ::ecclesia_transform_test::NestedMessage;
using ::ecclesia_transform_test::NestedRepeatedOneof;
using ::ecclesia_transform_test::OneofMessage;
using ::ecclesia_transform_test::RepeatedMessage;
using ::ecclesia_transform_test::SimpleMessage;

TEST(TransformDevpathsToDevpathsTest, EmptyTransform) {
  SimpleMessage message = ParseTextProtoOrDie(R"pb(
    devpath: "/phys/A"
    not_devpath: "/phys/B"
    also_devpath: "/phys/C/D"
    not_even_a_string: 4
  )pb");
  SimpleMessage expected = message;

  // This should never be called.
  auto func = [](absl::string_view devpath) -> std::optional<std::string> {
    ADD_FAILURE() << "transform function was incorrectly called";
    return std::nullopt;
  };

  EXPECT_TRUE(TransformProtobufDevpaths(func, "", &message));
  EXPECT_THAT(message, EqualsProto(expected));
}

TEST(TransformDevpathsToDevpathsTest, SuccessfulTransform) {
  SimpleMessage message = ParseTextProtoOrDie(R"pb(
    devpath: "/phys/A"
    not_devpath: "/phys/B"
    also_devpath: "/phys/C/D"
    not_even_a_string: 4
  )pb");
  SimpleMessage expected = ParseTextProtoOrDie(R"pb(
    devpath: "/phys/A/ZZ"
    not_devpath: "/phys/B"
    also_devpath: "/phys/C/D/ZZZ"
    not_even_a_string: 4
  )pb");

  // The transform will append /Z* where the number of Z's is equal to the
  // number of '/' in the original string.
  auto func = [](absl::string_view devpath) -> std::optional<std::string> {
    std::string result = std::string(devpath);
    result.push_back('/');
    result.append(std::count(devpath.begin(), devpath.end(), '/'), 'Z');
    return result;
  };

  EXPECT_TRUE(
      TransformProtobufDevpaths(func, "devpath,also_devpath", &message));
  EXPECT_THAT(message, EqualsProto(expected));
}

TEST(TransformDevpathsToDevpathsTest, IdentityTransformDoesNotSet) {
  // In this case also_devpath is not set. Performing an identity transform
  // should not cause it to be set.
  SimpleMessage message = ParseTextProtoOrDie(R"pb(
    devpath: "/phys/A/ZZ"
    not_devpath: "/phys/B"
    not_even_a_string: 4
  )pb");
  SimpleMessage expected = message;

  // The transform will return the original value. For fields which are not
  // already set, they should remain unset.
  auto func = [](absl::string_view devpath) -> std::optional<std::string> {
    return std::string(devpath);
  };

  EXPECT_TRUE(
      TransformProtobufDevpaths(func, "devpath,also_devpath", &message));
  EXPECT_THAT(message, EqualsProto(expected));
}

TEST(TransformDevpathsToDevpathsTest, SuccessfulNestedTransform) {
  NestedMessage message = ParseTextProtoOrDie(R"pb(
    sub { devpath: "/phys" number: 1 }
  )pb");
  NestedMessage expected = ParseTextProtoOrDie(R"pb(
    sub { devpath: "/phys/plus" number: 1 }
  )pb");

  // The transform will append /plus.
  auto func = [](absl::string_view devpath) -> std::optional<std::string> {
    return std::string(devpath) + "/plus";
  };

  EXPECT_TRUE(TransformProtobufDevpaths(func, "sub.devpath", &message));
  EXPECT_THAT(message, EqualsProto(expected));
}

TEST(TransformDevpathsToDevpathsTest, FailsIfAnyTransformFails) {
  SimpleMessage message = ParseTextProtoOrDie(R"pb(
    devpath: "/phys/A"
    not_devpath: "/phys/B"
    also_devpath: "/phys/C/D"
    not_even_a_string: 4
  )pb");
  SimpleMessage first_result = message;

  // Fail on /phys/C/D, otherwise return the value unmodified.
  auto func = [](absl::string_view devpath) -> std::optional<std::string> {
    if (devpath == "/phys/C/D")
      return std::nullopt;
    else
      return std::string(devpath);
  };

  // First, only try transforming devpath. This should work and not actually
  // modify the message value.
  EXPECT_TRUE(TransformProtobufDevpaths(func, "devpath", &message));
  EXPECT_THAT(message, EqualsProto(first_result));
  // Now also include also_devpath. The function we're using should fail on
  // that and thus fail the entire transform.
  EXPECT_FALSE(
      TransformProtobufDevpaths(func, "devpath,also_devpath", &message));
}

TEST(TransformDevpathsToDevpathsTest, CannotTransformMissingFields) {
  SimpleMessage message = ParseTextProtoOrDie(R"pb(
    devpath: "/phys/A"
    not_devpath: "/phys/B"
    also_devpath: "/phys/C/D"
    not_even_a_string: 4
  )pb");

  // This should never be called.
  auto func = [](absl::string_view devpath) -> std::optional<std::string> {
    ADD_FAILURE() << "transform function was incorrectly called";
    return std::nullopt;
  };

  EXPECT_FALSE(TransformProtobufDevpaths(func, "not_a_field", &message));
}

TEST(TransformDevpathsToDevpathsTest, CannotTransformIntegers) {
  SimpleMessage message = ParseTextProtoOrDie(R"pb(
    devpath: "/phys/A"
    not_devpath: "/phys/B"
    also_devpath: "/phys/C/D"
    not_even_a_string: 4
  )pb");

  // This should never be called.
  auto func = [](absl::string_view devpath) -> std::optional<std::string> {
    ADD_FAILURE() << "transform function was incorrectly called";
    return std::nullopt;
  };

  EXPECT_FALSE(TransformProtobufDevpaths(func, "not_even_a_string", &message));
}

TEST(TransformDevpathsToDevpathsTest, SuccessfulTransformRepeatedFields) {
  RepeatedMessage message = ParseTextProtoOrDie(R"pb(
    multiple_devpaths: "/phys/B"
    multiple_devpaths: "/phys/C"
  )pb");

  RepeatedMessage expected = ParseTextProtoOrDie(R"pb(
    multiple_devpaths: "/phys/B/C"
    multiple_devpaths: "/phys/C/C"
  )pb");

  auto func = [](absl::string_view devpath) -> std::optional<std::string> {
    std::string result(devpath);
    absl::StrAppend(&result, "/C");
    return result;
  };

  EXPECT_TRUE(TransformProtobufDevpaths(func, "multiple_devpaths", &message));
  EXPECT_THAT(message, EqualsProto(expected));
}

TEST(TransformDevpathsToDevpathsTest, ComplicatedMessage) {
  auto func =
      [](absl::string_view non_empty_value) -> std::optional<std::string> {
    if (non_empty_value.empty()) {
      return std::nullopt;
    }

    std::string result(non_empty_value);
    absl::StrAppend(&result, "aaa");
    return result;
  };

  {
    ComplicatedMessage message = ParseTextProtoOrDie(R"pb(
      bb: {
        multiple_devpaths: "b"
        cc: { devpath: "c1" }
        cc: { devpath: "c2" }
      }
      also_devpath: "a"
    )pb");
    ComplicatedMessage expected = ParseTextProtoOrDie(R"pb(
      bb: {
        multiple_devpaths: "b"
        cc: { devpath: "c1aaa" }
        cc: { devpath: "c2aaa" }
      }
      also_devpath: "a"
    )pb");

    EXPECT_TRUE(TransformProtobufDevpaths(func, "bb.cc.devpath", &message));
    EXPECT_THAT(message, EqualsProto(expected));
  }

  {
    ComplicatedMessage message = ParseTextProtoOrDie(R"pb(
      also_devpath: "a"
    )pb");
    ComplicatedMessage expected = message;

    EXPECT_TRUE(TransformProtobufDevpaths(func, "bb.cc.devpath", &message));
    EXPECT_THAT(message, EqualsProto(expected));
  }

  {
    ComplicatedMessage message = ParseTextProtoOrDie(R"pb(
      bb: { multiple_devpaths: "b1" multiple_devpaths: "b2" }
    )pb");
    ComplicatedMessage expected = ParseTextProtoOrDie(R"pb(
      bb: { multiple_devpaths: "b1aaa" multiple_devpaths: "b2aaa" }
    )pb");

    EXPECT_TRUE(
        TransformProtobufDevpaths(func, "bb.multiple_devpaths", &message));
    EXPECT_THAT(message, EqualsProto(expected));
  }
}

// Using the original FieldMaskUtil::GetFieldDescriptors wouldn’t pass this
// test, because it doesn’t dive into repeated sub-messages.
TEST(TransformDevpathsToDevpathsTest, RepeatedNestedMessageWithFullMask) {
  auto func =
      [](absl::string_view non_empty_value) -> std::optional<std::string> {
    if (non_empty_value.empty()) {
      return std::nullopt;
    }

    std::string result(non_empty_value);
    absl::StrAppend(&result, "aaa");
    return result;
  };

  NestedRepeatedOneof message = ParseTextProtoOrDie(R"pb(
    r: { sub_a: { devpath_a: "aaa" } }
    r: { sub_a: { devpath_a: "bbb" } }
  )pb");
  NestedRepeatedOneof expected = ParseTextProtoOrDie(R"pb(
    r: { sub_a: { devpath_a: "aaaaaa" } }
    r: { sub_a: { devpath_a: "bbbaaa" } }
  )pb");
  EXPECT_TRUE(TransformProtobufDevpaths(func, "r.sub_a.devpath_a", &message));
  EXPECT_THAT(message, EqualsProto(expected));
}

TEST(TransformDevpathsToDevpathsTest, OneofMessage) {
  auto func =
      [](absl::string_view non_empty_value) -> std::optional<std::string> {
    if (non_empty_value.empty()) {
      return std::nullopt;
    }

    std::string result(non_empty_value);
    absl::StrAppend(&result, "aaa");
    return result;
  };

  {
    OneofMessage message = ParseTextProtoOrDie(R"pb(devpath: "aaa")pb");
    OneofMessage expected = ParseTextProtoOrDie(R"pb(devpath: "aaaaaa")pb");
    EXPECT_TRUE(TransformProtobufDevpaths(func, "devpath", &message));
    EXPECT_THAT(message, EqualsProto(expected));
  }

  {
    OneofMessage message = ParseTextProtoOrDie(R"pb()pb");
    OneofMessage expected = message;
    // False if the transform function says so.
    EXPECT_FALSE(TransformProtobufDevpaths(func, "devpath", &message));
    EXPECT_THAT(message, EqualsProto(expected));
  }

  {
    OneofMessage message = ParseTextProtoOrDie(R"pb(not_devpath: 1)pb");
    OneofMessage expected = message;
    EXPECT_FALSE(TransformProtobufDevpaths(func, "not_devpath", &message));
    EXPECT_THAT(message, EqualsProto(expected));
  }

  // Various kinds of nested oneof’s.
  {
    OneofMessage message = ParseTextProtoOrDie(R"pb(
      a { devpath: "aaa" }
    )pb");
    OneofMessage expected = ParseTextProtoOrDie(R"pb(
      a { devpath: "aaaaaa" }
    )pb");

    EXPECT_TRUE(TransformProtobufDevpaths(func, "a.devpath", &message));
    EXPECT_THAT(message, EqualsProto(expected));
  }

  {
    OneofMessage message = ParseTextProtoOrDie(R"pb(
      b { devpath: "aaa" devpath: "bbb" }
    )pb");
    OneofMessage expected = ParseTextProtoOrDie(R"pb(
      b { devpath: "aaaaaa" devpath: "bbbaaa" }
    )pb");

    EXPECT_TRUE(TransformProtobufDevpaths(func, "b.devpath", &message));
    EXPECT_THAT(message, EqualsProto(expected));
  }

  {
    OneofMessage message = ParseTextProtoOrDie(R"pb(
      c { devpath: "aaa" }
    )pb");
    OneofMessage expected = ParseTextProtoOrDie(R"pb(
      c { devpath: "aaaaaa" }
    )pb");

    EXPECT_TRUE(TransformProtobufDevpaths(func, "c.devpath", &message));
    EXPECT_THAT(message, EqualsProto(expected));
  }
}

TEST(TransformDevpathsToDevpathsTest, IdentityOnOneof) {
  // Identify transformation on unset oneof’s should work as on normal fields.
  auto identity = [](absl::string_view value) -> std::optional<std::string> {
    return std::string(value);
  };

  {
    OneofMessage message = ParseTextProtoOrDie(R"pb()pb");
    OneofMessage expected = message;
    EXPECT_TRUE(TransformProtobufDevpaths(identity, "devpath", &message));
    EXPECT_THAT(message, EqualsProto(expected));
  }

  {
    OneofMessage message = ParseTextProtoOrDie(R"pb(not_devpath: 1)pb");
    OneofMessage expected = message;
    EXPECT_TRUE(TransformProtobufDevpaths(identity, "devpath", &message));
    EXPECT_THAT(message, EqualsProto(expected));
  }

  {
    OneofMessage message = ParseTextProtoOrDie(R"pb(another_devpath: "aaa")pb");
    OneofMessage expected = message;
    EXPECT_TRUE(TransformProtobufDevpaths(identity, "devpath", &message));
    EXPECT_THAT(message, EqualsProto(expected));
  }
}

}  // namespace
}  // namespace ecclesia
