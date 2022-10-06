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

// Defines the interfaces used for supporting logging. This header is normally
// only needed if you want to pass additional information to logging systems.

#ifndef ECCLESIA_LIB_LOGGING_INTERFACES_H_
#define ECCLESIA_LIB_LOGGING_INTERFACES_H_

#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"

namespace ecclesia {

// Provides a utility object that can capture the current source location in
// calling code. Intended only for use with the logging library, not in general.
//
// This relies on the __builtin_* versions of LINE and FILE which are not
// standard by supported on any toolchains we care about.
//
// In general the proper way to capture this object is by adding a default
// argument to your function with the value of SourceLocation::current(). This
// will capture the location of the call size automatically, for later
// inspection.
//
// Example:
//
//   void TracedCall(int p, SourceLocation loc = SourceLocation::current()) {
//     std::cout << "loc.file_name() << ":" << loc.line() << " passed " << p;
//     ... do normal stuff ...
//   }
//
//   void CodeUsingCall() {
//     TracedCall(1);
//     TracedCall(2);
//   }
//
// The TracedCall code will then print out the location in CodeUsingCall that
// are calling it.
class SourceLocation {
 private:
  // Private tag object. Used to block users from being able to call "current"
  // with explicit arguments by making it impossible to construct the first one.
  struct PrivateTag {
   private:
    explicit PrivateTag() = default;
    friend class SourceLocation;
  };

 public:
  // Construct an object referencing the current source location. Must be
  // constructed with the default arguments.
  static constexpr SourceLocation current(
      PrivateTag = PrivateTag(), int line = __builtin_LINE(),
      const char *file_name = __builtin_FILE()) {
    return SourceLocation(line, file_name);
  }

  // The line number of the capturing source location.
  constexpr int line() const { return line_; }

  // The file name of the captured source location.
  constexpr const char *file_name() const { return file_name_; }

 private:
  // The real constructor. Cannot be called directly.
  constexpr SourceLocation(int line, const char *file_name)
      : line_(line), file_name_(file_name) {}

  int line_;
  const char *file_name_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_LOGGING_INTERFACES_H_
