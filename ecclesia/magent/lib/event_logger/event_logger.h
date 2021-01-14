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

#ifndef ECCLESIA_MAGENT_LIB_EVENT_LOGGER_EVENT_LOGGER_H_
#define ECCLESIA_MAGENT_LIB_EVENT_LOGGER_EVENT_LOGGER_H_

#include <memory>
#include <thread>  // NOLINT(build/c++11)
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/magent/lib/event_reader/event_reader.h"

namespace ecclesia {

// An interface for visiting the system event records
class SystemEventVisitor {
 public:
  // Enumeration to allow specifying the desired order of visiting the event
  // records
  enum class VisitDirection {
    FROM_START,
    FROM_END,
  };

  explicit SystemEventVisitor(VisitDirection direction)
      : direction_(direction) {}
  virtual ~SystemEventVisitor() {}

  VisitDirection GetDirection() const { return direction_; }

  // Visit an event record. Return value indicates whether to continue visiting
  // the rest of unvisited records.
  virtual bool Visit(const SystemEventRecord &record) = 0;

 private:
  const VisitDirection direction_;
};

// Define a class to log system events into. The SystemEventLogger periodically
// polls for system events from the readers provided to the constructor.
class SystemEventLogger {
 public:
  // Take in all the system event readers to poll for events from
  SystemEventLogger(std::vector<std::unique_ptr<SystemEventReader>> readers,
                    Clock *clock);

  ~SystemEventLogger() {
    // Signal the logger loop to exit
    exit_loop_.Notify();
    logger_loop_.join();
  }

  // Call the visitor object on every logged system event to allow for decoding
  // of event records, generating error counts etc.
  void Visit(SystemEventVisitor *visitor);

 private:
  void Loop();

  static constexpr absl::Duration kPollingInterval = absl::Seconds(10);
  std::vector<std::unique_ptr<SystemEventReader>> readers_;
  absl::Mutex records_lock_;
  std::vector<SystemEventRecord> records_ ABSL_GUARDED_BY(records_lock_);
  Clock *clock_;
  absl::Notification exit_loop_;
  std::thread logger_loop_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_EVENT_LOGGER_EVENT_LOGGER_H_
