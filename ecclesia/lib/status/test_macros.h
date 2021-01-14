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

#ifndef ECCLESIA_LIB_STATUS_TEST_MACROS_H_
#define ECCLESIA_LIB_STATUS_TEST_MACROS_H_

#include "ecclesia/lib/status/macros.h"

namespace ecclesia {

// Inner implementation of ECCLESIA_ASSIGN_OR_FAIL. Adds in a parameter which is
// used to supply the name for the temporary variable used to store the
// statusor.
#define ECCLESIA_ASSIGN_OR_FAIL_IMPL(lhs, rhs, local)  \
  auto local = rhs;                                    \
  EXPECT_TRUE(local.ok()) << local.status().message(); \
  lhs = std::move(local.value());

// Assign-or-fail implementation, similar to ECCLESIA_ASSIGN_OR_RETURN except it
// adds googletest expectation failures on error. Refer to
// ECCLESIA_ASSIGN_OR_RETURN's implementation for more information and for the
// gritty details.
#define ECCLESIA_ASSIGN_OR_FAIL(lhs, rhs) \
  ECCLESIA_ASSIGN_OR_FAIL_IMPL(           \
      lhs, rhs, ECCLESIA_STATUS_MACROS_CONCAT(_local_var_, __LINE__))

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_STATUS_TEST_MACROS_H_
