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

#ifndef ECCLESIA_LIB_TIME_PROTO_H_
#define ECCLESIA_LIB_TIME_PROTO_H_

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"

namespace ecclesia {

// Functions to convert between absl time and proto time.
absl::Time AbslTimeFromProtoTime(google::protobuf::Timestamp timestamp);
// Proto time is not as flexible as absl time; it does not handle timestamps
// before unix epoch or beyond InfiniteFuture. Return an error status in these
// situations.
absl::StatusOr<google::protobuf::Timestamp> AbslTimeToProtoTime(
    absl::Time timestamp);

// Decodes the given protobuf and returns an absl::Duration, or returns an error
// status if the argument is invalid.
absl::StatusOr<absl::Duration> AbslDurationFromProtoDuration(
    const google::protobuf::Duration &proto);

// Encodes an absl::Duration as a google::protobuf::Duration.  Returns an error
// if the given absl::Duration is beyond the range allowed by the protobuf.
// Otherwise, truncates toward zero with nanosecond precision and returns the
// google::protobuf::Duration.
//
// Note: +/- absl::InfiniteDuration() cannot be encoded because they are not
// representable in the protobuf.
absl::StatusOr<google::protobuf::Duration> AbslDurationToProtoDuration(
    absl::Duration d);

// Same as above but taking a proto pointer.
absl::Status AbslDurationToProtoDuration(absl::Duration d,
                                         google::protobuf::Duration *proto);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_TIME_PROTO_H_
