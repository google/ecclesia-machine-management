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
#include <string>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"

namespace ecclesia {

// Validation requirements per google::protobuf::Duration.
absl::Status Validate(const google::protobuf::Duration &d) {
  const auto sec = d.seconds();
  const auto ns = d.nanos();
  if (sec < -315576000000 || sec > 315576000000) {
    return absl::InvalidArgumentError(absl::StrCat("seconds=", sec));
  }
  if (ns < -999999999 || ns > 999999999) {
    return absl::InvalidArgumentError(absl::StrCat("nanos=", ns));
  }
  if ((sec < 0 && ns > 0) || (sec > 0 && ns < 0)) {
    return absl::InvalidArgumentError("sign mismatch");
  }
  return absl::OkStatus();
}

absl::Time AbslTimeFromProtoTime(google::protobuf::Timestamp timestamp) {
  // Protobuf time is just a combo of seconds and nanoseconds so we can
  // construct time by just taking the unix epoch and splicing in those two
  // units.
  return absl::UnixEpoch() + absl::Seconds(timestamp.seconds()) +
         absl::Nanoseconds(timestamp.nanos());
}

absl::StatusOr<google::protobuf::Timestamp> AbslTimeToProtoTime(
    absl::Time timestamp) {
  google::protobuf::Timestamp proto_timestamp;
  // Converting time directly into seconds and nanoseconds it a bit tricky if we
  // want to avoid overflow on the nanoseconds. It's a little easier if we
  // instead convert to duration and use division and modulus operators. We can
  // think of the time as just being a duration since the unix epoch.
  //
  // Note that this does not handle infinite past (or even anything pre-epoch)
  // or infinite future.
  if (timestamp < absl::UnixEpoch()) {
    return absl::InternalError(
        "timestamps earlier than UnixEpoch cannot be converted to "
        "protobuf::Timestamp.");
  }
  if (timestamp == absl::InfiniteFuture()) {
    return absl::InternalError(
        "InfiniteFuture timestamp cannot be converted to protobuf::Timestamp.");
  }
  absl::Duration duration = timestamp - absl::UnixEpoch();
  proto_timestamp.set_seconds(duration / absl::Seconds(1));
  duration %= absl::Seconds(1);
  proto_timestamp.set_nanos(duration / absl::Nanoseconds(1));
  return proto_timestamp;
}

absl::StatusOr<google::protobuf::Duration> AbslDurationToProtoDuration(
    absl::Duration d) {
  google::protobuf::Duration proto;
  absl::Status status = AbslDurationToProtoDuration(d, &proto);
  if (!status.ok()) return status;
  return proto;
}

absl::Status AbslDurationToProtoDuration(absl::Duration d,
                                         google::protobuf::Duration *proto) {
  // s and n may both be negative, per the Duration proto spec.
  const int64_t s = absl::IDivDuration(d, absl::Seconds(1), &d);
  const int64_t n = absl::IDivDuration(d, absl::Nanoseconds(1), &d);
  proto->set_seconds(s);
  proto->set_nanos(n);
  return Validate(*proto);
}

absl::StatusOr<absl::Duration> AbslDurationFromProtoDuration(
    const google::protobuf::Duration &proto) {
  absl::Status status = Validate(proto);
  if (!status.ok()) return status;
  return absl::Seconds(proto.seconds()) + absl::Nanoseconds(proto.nanos());
}

}  // namespace ecclesia
