/*
 * Copyright 2021 Google LLC
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

#include "ecclesia/lib/usage/map.h"

#include <fcntl.h>

#include <algorithm>
#include <string>
#include <utility>

// IWYU pragma: no_include "base/integral_types.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "ecclesia/lib/usage/serialization.pb.h"
#include "riegeli/bytes/fd_reader.h"
#include "riegeli/bytes/fd_writer.h"
#include "riegeli/records/record_reader.h"
#include "riegeli/records/record_writer.h"

namespace ecclesia {
namespace {

// Functions to convert between absl time and proto time.
absl::Time AbslTimeFromProtoTime(google::protobuf::Timestamp timestamp) {
  // Protobuf time is just a combo of seconds and nanoseconds so we can
  // construct time by just taking the unix epoch and splicing in those two
  // units.
  return absl::UnixEpoch() + absl::Seconds(timestamp.seconds()) +
         absl::Nanoseconds(timestamp.nanos());
}
google::protobuf::Timestamp AbslTimeToProtoTime(absl::Time timestamp) {
  google::protobuf::Timestamp proto_timestamp;
  // Converting time directly into seconds and nanoseconds it a bit tricky if we
  // want to avoid overflow on the nanoseconds. It's a little easier if we
  // instead convert to duration and use division and modulus operators. We can
  // think of the time as just being a duration since the unix epoch.
  //
  // Note that this does not handle infinite past (or even anything pre-epoch)
  // or infinite future. Neither of those times are used in the persistent map.
  absl::Duration duration = timestamp - absl::UnixEpoch();
  proto_timestamp.set_seconds(duration / absl::Seconds(1));
  duration %= absl::Seconds(1);
  proto_timestamp.set_nanos(duration / absl::Nanoseconds(1));
  return proto_timestamp;
}

}  // namespace

PersistentUsageMap::PersistentUsageMap(Options options)
    : persistent_file_(std::move(options.persistent_file)) {
  MergeFromPersistentStore().IgnoreError();
}

void PersistentUsageMap::RecordUse(std::string operation, std::string user,
                                   absl::Time timestamp) {
  OperationUser op_user = {std::move(operation), std::move(user)};
  absl::MutexLock ml(&mutex_);
  InsertOrUpdateMapEntry(std::move(op_user), timestamp);
}

absl::Status PersistentUsageMap::WriteToPersistentStore() {
  // Construct the protobuf to write from the usage map.
  PersistentUsageMapProto proto_map;
  {
    absl::MutexLock ml(&mutex_);
    for (const auto &[key, value] : in_memory_map_) {
      PersistentUsageMapProto::Entry *entry = proto_map.add_entries();
      entry->set_operation(key.operation);
      entry->set_user(key.user);
      *entry->mutable_timestamp() = AbslTimeToProtoTime(value);
    }
  }

  // Open up the file for writing.
  // if the write operation fails.
  riegeli::RecordWriter<riegeli::FdStreamWriter<>> writer(
      riegeli::FdStreamWriter<>(persistent_file_,
                                O_WRONLY | O_CREAT | O_TRUNC));

  // Write the protobuf out to the file.
  // if the output proto is too large.
  if (!writer.WriteRecord(proto_map)) {
    return absl::InternalError("unable to write out the usage map");
  }
  if (!writer.Close()) {
    return absl::InternalError("closing the usage map failed");
  }
  return absl::OkStatus();
}

void PersistentUsageMap::InsertOrUpdateMapEntry(OperationUser op_user,
                                                absl::Time timestamp) {
  auto map_iter = in_memory_map_.find(op_user);
  if (map_iter == in_memory_map_.end()) {
    // No entry was found, so insert a new one.
    in_memory_map_.emplace(std::move(op_user), timestamp);
  } else {
    // Update the timestamp in the map to whichever is newer.
    map_iter->second = std::max(timestamp, map_iter->second);
  }
}

absl::Status PersistentUsageMap::MergeFromPersistentStore() {
  // Try to read data in from a local file.
  riegeli::RecordReader<riegeli::FdStreamReader<>> reader(
      riegeli::FdStreamReader<>(persistent_file_, O_RDONLY));
  PersistentUsageMapProto proto_map;
  if (!reader.ReadRecord(proto_map)) {
    return absl::NotFoundError("unable to read any existing record");
  }

  // Now that we've read the file, insert any entries found in it into the map.
  absl::MutexLock ml(&mutex_);
  for (PersistentUsageMapProto::Entry &entry : *proto_map.mutable_entries()) {
    OperationUser op_user = {std::move(*entry.mutable_operation()),
                             std::move(*entry.mutable_user())};
    InsertOrUpdateMapEntry(std::move(op_user),
                           AbslTimeFromProtoTime(entry.timestamp()));
  }
  return absl::OkStatus();
}

}  // namespace ecclesia
