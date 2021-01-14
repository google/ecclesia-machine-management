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

#include "ecclesia/lib/time/clock.h"

#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace ecclesia {
namespace {

// The abseil clock functions are already thread-safe and this clock doesn't
// have any state of its own, so it is also thread-safe. If any state is later
// added to this object then care must be taken to preserve thread-safety.
class RealClock : public Clock {
 public:
  absl::Time Now() const override { return absl::Now(); }
};

}  // namespace

Clock *Clock::RealClock() {
  static Clock &real_clock = *(new class RealClock());
  return &real_clock;
}

}  // namespace ecclesia
