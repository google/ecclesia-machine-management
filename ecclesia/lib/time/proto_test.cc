/*
 * Copyright 2022 Google LLC
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

#include "ecclesia/lib/time/proto.h"

#include <cstdint>
#include <limits>
#include <string>

#include "google/protobuf/timestamp.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/time/time.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::testing::Eq;

TEST(AbslTimeProtoTime, ConvertAbslToAndFromProtoIsIdentical) {
  absl::Time absl_time =
      absl::UnixEpoch() + absl::Seconds(632198) + absl::Nanoseconds(4328432);
  auto proto_time = AbslTimeToProtoTime(absl_time);
  ASSERT_TRUE(proto_time.ok()) << proto_time.status().message();
  EXPECT_THAT(*proto_time,
              EqualsProto(R"pb(seconds: 632198 nanos: 4328432)pb"));
  absl::Time reconverted_absl_time = AbslTimeFromProtoTime(*proto_time);
  EXPECT_THAT(reconverted_absl_time, Eq(absl_time));
}

TEST(AbslTimeProtoTime, NanosecondsWithin999999999Ok) {
  absl::Time absl_time = absl::UnixEpoch() + absl::Nanoseconds(999999999);
  auto proto_time = AbslTimeToProtoTime(absl_time);
  ASSERT_TRUE(proto_time.ok()) << proto_time.status().message();
  EXPECT_THAT(*proto_time, EqualsProto(R"pb(seconds: 0 nanos: 999999999)pb"));
  absl::Time reconverted_absl_time = AbslTimeFromProtoTime(*proto_time);
  EXPECT_THAT(reconverted_absl_time, Eq(absl_time));
}

TEST(AbslTimeProtoTime, NanosecondsAbove999999999OverflowOk) {
  absl::Time absl_time =
      absl::UnixEpoch() + absl::Nanoseconds(999999999) + absl::Nanoseconds(1);
  auto proto_time = AbslTimeToProtoTime(absl_time);
  ASSERT_TRUE(proto_time.ok()) << proto_time.status().message();
  EXPECT_THAT(*proto_time, EqualsProto(R"pb(seconds: 1 nanos: 0)pb"));
  absl::Time reconverted_absl_time = AbslTimeFromProtoTime(*proto_time);
  EXPECT_THAT(reconverted_absl_time, Eq(absl_time));
}

TEST(AbslTimeProtoTime, MaxValueOk) {
  absl::Time absl_time = absl::UnixEpoch() +
                         absl::Seconds(std::numeric_limits<int64_t>::max()) +
                         absl::Nanoseconds(999999999);
  auto proto_time = AbslTimeToProtoTime(absl_time);
  ASSERT_TRUE(proto_time.ok()) << proto_time.status().message();
  EXPECT_THAT(*proto_time, EqualsProto(absl::Substitute(
                               R"pb(seconds: $0 nanos: 999999999)pb",
                               std::numeric_limits<int64_t>::max())));
  absl::Time reconverted_absl_time = AbslTimeFromProtoTime(*proto_time);
  EXPECT_THAT(reconverted_absl_time, Eq(absl_time));
}

TEST(AbslTimeProtoTime, OverMaxValueFailure) {
  // Should be the same as InfiniteFuture.
  absl::Time absl_time = absl::UnixEpoch() +
                         absl::Seconds(std::numeric_limits<int64_t>::max()) +
                         absl::Nanoseconds(999999999) + absl::Nanoseconds(1);
  auto proto_time = AbslTimeToProtoTime(absl_time);
  EXPECT_THAT(proto_time, IsStatusInternal());
}

TEST(AbslTimeProtoTime, UnixEpochOk) {
  absl::Time absl_time = absl::UnixEpoch();
  auto proto_time = AbslTimeToProtoTime(absl_time);
  ASSERT_TRUE(proto_time.ok()) << proto_time.status().message();
  EXPECT_THAT(*proto_time, EqualsProto(R"pb(seconds: 0 nanos: 0)pb"));
  absl::Time reconverted_absl_time = AbslTimeFromProtoTime(*proto_time);
  EXPECT_THAT(reconverted_absl_time, Eq(absl_time));
}

TEST(AbslTimeProtoTime, PreUnixEpochFailure) {
  absl::Time absl_time = absl::UnixEpoch() - absl::Nanoseconds(1);
  auto proto_time = AbslTimeToProtoTime(absl_time);
  EXPECT_THAT(proto_time, IsStatusInternal());
}

TEST(AbslTimeProtoTime, InfinitePastFailure) {
  absl::Time absl_time = absl::InfinitePast();
  auto proto_time = AbslTimeToProtoTime(absl_time);
  EXPECT_THAT(proto_time, IsStatusInternal());
}

TEST(AbslTimeProtoTime, InfiniteFutureFailure) {
  absl::Time absl_time = absl::InfinitePast();
  auto proto_time = AbslTimeToProtoTime(absl_time);
  EXPECT_THAT(proto_time, IsStatusInternal());
}

}  // namespace
}  // namespace ecclesia
