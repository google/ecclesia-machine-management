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

#ifndef ECCLESIA_LIB_STATUS_POSIX_H_
#define ECCLESIA_LIB_STATUS_POSIX_H_

#include <cerrno>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace ecclesia {

// Helper factory method for taking a POSIX errno and translating it to an
// applicable absl::Status error with the provided message. The resulting
// absl::Status message will be prepended with the error_number.
absl::Status PosixErrorToStatus(std::string_view message,
                                int error_number = errno);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_STATUS_POSIX_H_
