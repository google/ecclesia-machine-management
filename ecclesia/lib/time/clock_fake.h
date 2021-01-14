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

// This library defines a fake clock. It is not a full mock object; instead it
// is a simulation of a clock that does not move forward except when explicitly
// instructed to.

#ifndef ECCLESIA_LIB_TIME_CLOCK_FAKE_H_
#define ECCLESIA_LIB_TIME_CLOCK_FAKE_H_

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

class FakeClock : public Clock {
 public:
  // Construct a fake clock. It can be initialized to a specific time, or if
  // not specified it will default to (the real) Now.
  FakeClock() : FakeClock(absl::Now()) {}
  explicit FakeClock(absl::Time now) : time_(now) {}

  absl::Time Now() const override { return time_; }

  // Move time forward by duration. This cannot be used to move time back.
  void AdvanceTime(absl::Duration duration) { time_ += duration; }

 private:
  absl::Time time_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_TIME_CLOCK_FAKE_H_
