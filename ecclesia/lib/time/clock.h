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

// This library defines a simplified clock interface that wraps the abseil
// functions for looking at the current time. This is intended to enable more
// testability in code that needs to track the current time by allowing you to
// supply fake/mock clocks that give the test control over when time advances.

#ifndef ECCLESIA_LIB_TIME_CLOCK_H_
#define ECCLESIA_LIB_TIME_CLOCK_H_

#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace ecclesia {

class Clock {
 public:
  // Return a pointer to a global thread-safe singleton clock implemented using
  // the actual abseil time functions.
  static Clock *RealClock();

  virtual ~Clock() = default;

  // Returns current time.
  virtual absl::Time Now() const = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_TIME_CLOCK_H_
