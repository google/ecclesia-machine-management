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

// Provide an object which can be used to generate unique sequence numbers.
//
// This is implemented using an atomic counter. It is most useful for cases
// where want to generate numbers when you're not already in a critical section.
// If you're already in code guarded by a mutex it'll be much cheaper to just
// implement a counter yourself.

#ifndef ECCLESIA_LIB_ATOMIC_SEQUENCE_H_
#define ECCLESIA_LIB_ATOMIC_SEQUENCE_H_

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace ecclesia {

class SequenceNumberGenerator {
 public:
  // This type can be constexpr constructed and has a trivial destructor so that
  // it can safely by used as a static lifetime object.
  constexpr SequenceNumberGenerator() : value_(0) {}

  SequenceNumberGenerator(const SequenceNumberGenerator &) = delete;
  SequenceNumberGenerator &operator=(const SequenceNumberGenerator &) = delete;

  // Pick as large of a type as possible that we can reasonably expect to be
  // implemented in a lock-free way. Also use an unsigned value so that in the
  // incredibly unlikely scenario that you can generate enough values to
  // overflow there will be no undefined behavior.
  using ValueType = uintptr_t;

  // Generate a new value. Returns a new number one higher than the previous
  // number returned. This function is threadsafe.
  ValueType GenerateValue() {
    return value_.fetch_add(1, std::memory_order_relaxed);
  }

 private:
  std::atomic<ValueType> value_;
  static_assert(decltype(value_)::is_always_lock_free);
};
static_assert(std::is_trivially_destructible_v<SequenceNumberGenerator>);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_ATOMIC_SEQUENCE_H_
