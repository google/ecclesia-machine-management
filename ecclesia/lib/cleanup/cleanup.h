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

// Helper RAII objects for doing automatic cleanup when scopes are exited.

#ifndef ECCLESIA_LIB_UTIL_FD_CLEANUP_H_
#define ECCLESIA_LIB_UTIL_FD_CLEANUP_H_

#include <functional>
#include <type_traits>

namespace ecclesia {

// A RAII class that runs a provided function on destruction.
class LambdaCleanup {
 public:
  explicit LambdaCleanup(std::function<void()> f) : f_(std::move(f)) {}
  LambdaCleanup(const LambdaCleanup &) = delete;
  LambdaCleanup &operator=(const LambdaCleanup &) = delete;
  ~LambdaCleanup() { f_(); }

 private:
  std::function<void()> f_;
};

}  // namespace ecclesia
#endif  // ECCLESIA_LIB_UTIL_FD_CLEANUP_H_
