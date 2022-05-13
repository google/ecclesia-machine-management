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

#include "google/protobuf/duration.pb.h"
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

google::protobuf::Duration MakeProtoDuration(int64_t s, int32_t ns) {
  google::protobuf::Duration proto;
  proto.set_seconds(s);
  proto.set_nanos(ns);
  return proto;
}

// Returns the min absl::Duration that can be represented as
// google::protobuf::Duration.
inline absl::Duration MakeAbslDurationMin() {
  return absl::Seconds(-315576000000) + absl::Nanoseconds(-999999999);
}

// Returns the max absl::Duration that can be represented as
// google::protobuf::Duration.
inline absl::Duration MakeAbslDurationMax() {
  return absl::Seconds(315576000000) + absl::Nanoseconds(999999999);
}

// Returns the min Duration proto representing approximately -10,000 years,
// as defined by google::protobuf::Duration.
inline google::protobuf::Duration MakeProtoDurationMin() {
  google::protobuf::Duration proto;
  proto.set_seconds(-315576000000);
  proto.set_nanos(-999999999);
  return proto;
}

// Returns the max Duration proto representing approximately 10,000 years,
// as defined by google::protobuf::Duration.
inline google::protobuf::Duration MakeProtoDurationMax() {
  google::protobuf::Duration proto;
  proto.set_seconds(315576000000);
  proto.set_nanos(999999999);
  return proto;
}

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

TEST(AbslTimeProtoDuration, EncodeDurationFailure) {
  const absl::Duration kTestCases[] = {
      MakeAbslDurationMin() - absl::Nanoseconds(1),
      MakeAbslDurationMax() + absl::Nanoseconds(1), -absl::InfiniteDuration(),
      absl::InfiniteDuration()};
  for (const auto &d : kTestCases) {
    const auto sor = AbslDurationToProtoDuration(d);
    EXPECT_FALSE(sor.ok()) << "d=" << d;

    google::protobuf::Duration proto;
    const auto status = AbslDurationToProtoDuration(d, &proto);
    EXPECT_FALSE(status.ok()) << "d=" << d;
  }
}

TEST(AbslTimeProtoDuration, DecodeDurationFailure) {
  const google::protobuf::Duration kTestCases[] = {
      MakeProtoDuration(1, -1),
      MakeProtoDuration(-1, 1),
      MakeProtoDuration(0, 999999999 + 1),
      MakeProtoDuration(0, -999999999 - 1),
      MakeProtoDuration(-315576000000 - 1, 0),
      MakeProtoDuration(315576000000 + 1, 0),
      MakeProtoDuration(std::numeric_limits<int64_t>::min(), 0),
      MakeProtoDuration(std::numeric_limits<int64_t>::max(), 0),
      MakeProtoDuration(0, std::numeric_limits<int32_t>::min()),
      MakeProtoDuration(0, std::numeric_limits<int32_t>::max()),
      MakeProtoDuration(std::numeric_limits<int64_t>::min(),
                        std::numeric_limits<int32_t>::min()),
      MakeProtoDuration(std::numeric_limits<int64_t>::max(),
                        std::numeric_limits<int32_t>::max()),
  };
  for (const auto &d : kTestCases) {
    const auto sor = AbslDurationFromProtoDuration(d);
    EXPECT_FALSE(sor.ok()) << "d=" << d.DebugString();
  }
}

TEST(AbslTimeProtoDuration, DurationProtoMax) {
  const auto proto_max = MakeProtoDurationMax();
  const absl::Duration duration_max = MakeAbslDurationMax();
  EXPECT_THAT(AbslDurationFromProtoDuration(proto_max),
              IsOkAndHolds(duration_max));
  EXPECT_THAT(AbslDurationToProtoDuration(duration_max),
              IsOkAndHolds(EqualsProto(proto_max)));
}

TEST(AbslTimeProtoDuration, DurationMaxIsMax) {
  const absl::Duration duration_max = MakeAbslDurationMax();
  EXPECT_THAT(AbslDurationToProtoDuration(duration_max + absl::Nanoseconds(1)),
              IsStatusInvalidArgument());
}

TEST(AbslTimeProtoDuration, DurationProtoMin) {
  const auto proto_min = MakeProtoDurationMin();
  const absl::Duration duration_min = MakeAbslDurationMin();
  EXPECT_THAT(AbslDurationFromProtoDuration(proto_min),
              IsOkAndHolds(duration_min));
  EXPECT_THAT(AbslDurationToProtoDuration(duration_min),
              IsOkAndHolds(EqualsProto(proto_min)));
}

TEST(AbslTimeProtoDuration, DurationMinIsMin) {
  const absl::Duration duration_min = MakeAbslDurationMin();
  EXPECT_THAT(AbslDurationToProtoDuration(duration_min - absl::Nanoseconds(1)),
              IsStatusInvalidArgument());
}

}  // namespace
}  // namespace ecclesia
