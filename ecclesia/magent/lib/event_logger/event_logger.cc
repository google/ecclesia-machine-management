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

#include "ecclesia/magent/lib/event_logger/event_logger.h"

#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/magent/lib/event_reader/event_reader.h"

namespace ecclesia {

SystemEventLogger::SystemEventLogger(
    std::vector<std::unique_ptr<SystemEventReader>> readers, Clock *clock)
    : readers_(std::move(readers)),
      clock_(clock),
      logger_loop_(&SystemEventLogger::Loop, this) {}

void SystemEventLogger::Visit(SystemEventVisitor *visitor) {
  absl::MutexLock l(&records_lock_);
  if (visitor->GetDirection() ==
      SystemEventVisitor::VisitDirection::FROM_START) {
    for (auto &record : records_) {
      if (!visitor->Visit(record)) break;
    }
  } else {
    for (auto it = records_.rbegin(); it != records_.rend(); ++it) {
      if (!visitor->Visit(*it)) break;
    }
  }
}

void SystemEventLogger::Loop() {
  do {
    for (auto &reader : readers_) {
      while (auto record = reader->ReadEvent()) {
        absl::MutexLock l(&records_lock_);
        // Add timestamp to the record
        record.value().timestamp = clock_->Now();
        records_.push_back(record.value());
      }
    }
  } while (!exit_loop_.WaitForNotificationWithTimeout(kPollingInterval));
}

}  // namespace ecclesia
