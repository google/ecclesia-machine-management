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

#ifndef ECCLESIA_LIB_REDFISH_TIMING_QUERY_TIMEOUT_MANAGER_H_
#define ECCLESIA_LIB_REDFISH_TIMING_QUERY_TIMEOUT_MANAGER_H_

#include "absl/base/thread_annotations.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

// Class that manages the timeout for a given query, designed so that any calls
// around server requests can easily be timed by wrapping them with StartTime()
// and EndTime(), and the timeout will be updated accordingly.
class QueryTimeoutManager {
  /* Example usage:
  ...
  QueryTimeoutManager timeout_mgr(clock, timeout);
  // Start query timing session.
  RETURN_IF_ERROR(timeout_mgr.StartTime());
  // Server request 1.
  RETURN_IF_ERROR(timeout_mgr.ProbeTimeout());
  redfish_variant1 =
      redfish_interface_.CachedGetUri(node_name, get_params_for_redpath);
  <handle redfish_variant1 status, return if timeout>
  ...
  RETURN_IF_ERROR(timeout_mgr.ProbeTimeout());
  // Server request 2.
  redfish_variant2 =
      redfish_interface_.CachedGetUri(node_name2, get_params_for_redpath);
    <handle redfish_variant2 status, return if timeout>
  ...
  // End query timing session.
  RETURN_IF_ERROR(timeout_mgr.EndTime());
  */
 public:
  QueryTimeoutManager(const Clock &clock, absl::Duration timeout)
      : clock_(clock), timeout_(timeout) {
    if (timeout_ <= absl::ZeroDuration()) {
      LOG(ERROR) << "QueryTimeoutManager initialized with zero or negative "
                    "timeout. This is probably not intended. The next call to "
                    "ProbeTimeout() will return a DeadlineExceededError.";
    }
  }

  // Ends the timer and updates the timeout. Returns an error if the timeout
  // has been exceeded.
  absl::Status ProbeTimeout() {
    absl::MutexLock lock(&timeout_mutex_);
    return CheckTimeout();
  }

  // Starts the timer for the initialized timeout.
  // If a timing session is already in progress from calling StartTime()
  // previously, this function will block until that session is completed by an
  // EndTime() call.
  absl::Status StartTiming() {
    absl::MutexLock lock(&timeout_mutex_);
    while (in_progress_) {
      if (progress_cv_.WaitWithTimeout(&timeout_mutex_, timeout_)) {
        // timeout expired.
        return absl::DeadlineExceededError("Query Timeout already exceeded.");
      }
    }
    in_progress_ = true;
    start_time_ = clock_.Now();
    timeout_time_ = start_time_ + timeout_;
    return absl::OkStatus();
  }

  // Ends the timing session. Checks timeout a last time and signals end of the
  // timing session so a new session can be started with StartTime().
  absl::Status EndTiming() {
    absl::MutexLock lock(&timeout_mutex_);
    absl::Status ret = CheckTimeout();
    in_progress_ = false;
    progress_cv_.Signal();
    return ret;
  }

  // Returns the remaining timeout.
  absl::Duration GetRemainingTimeout() {
    absl::MutexLock lock(&timeout_mutex_);
    absl::Time now = clock_.Now();
    if (now >= timeout_time_) {
      return absl::ZeroDuration();
    }
    return timeout_time_ - clock_.Now();
  }

 private:
  const Clock &clock_;
  mutable absl::Mutex timeout_mutex_;
  absl::CondVar progress_cv_;
  bool in_progress_ ABSL_GUARDED_BY(timeout_mutex_) = false;
  const absl::Duration timeout_ ABSL_GUARDED_BY(timeout_mutex_);
  absl::Time start_time_ ABSL_GUARDED_BY(timeout_mutex_);
  absl::Time timeout_time_ ABSL_GUARDED_BY(timeout_mutex_);

  absl::Status CheckTimeout() ABSL_EXCLUSIVE_LOCKS_REQUIRED(timeout_mutex_) {
    if (!in_progress_) {
      return absl::FailedPreconditionError(
          "No timing session started with StartTime().");
    }
    // If the time that has passed since the last measured end time has made us
    // exceed the timeout, return an appropriate error.
    absl::Time now = clock_.Now();
    if (now >= timeout_time_) {
      return absl::DeadlineExceededError("Query Timeout exceeded.");
    }
    return absl::OkStatus();
  }
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TIMING_QUERY_TIMEOUT_MANAGER_H_
