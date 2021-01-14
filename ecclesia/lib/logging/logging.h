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

// Functions for logging errors, warnings and information, as well as for doing
// log-and-abort operations.
//
// This operation follows a familiar pattern for C++: to write out logs you use
// the << operator to stream objects to the logs you want. This allows you to
// write to logs with code like:
//
//   ErrorLog() << "something bad happened: " << status_object;
//
// Any object that you can stream to a std::ostream should be able to stream to
// the logging. In addition to logging the text of your log message the logging
// system can also capture timestamps and source code locations.
//
// This logging system rates severity via log levels, starting at 0 (the most
// severe) and increasing from there. The general idea is:
//   * 0 = unrecoverable error, abort program after logging
//   * 1 = serious error occurred, generally unexpected
//   * 2 = warning, something undesirable but not necessarily unexpected
//   * 3 = info, general information
//   * 4 = debug, verbose and noisy information
// These are generally called "Fatal", "Error", "Warning", "Info" and "Debug".
//
// Note that unlike some logging systems this one does not discard or compile
// out code just because a particular log level is not being recorded. Keep this
// in mind when writing expensive logging code: just because the logs aren't
// being recorded, doesn't mean the code isn't executing.
//
// On the flip side, just because we aren't optimizing away the logging code
// that doesn't make it a good idea to have side effects from your logging. We
// do not promise that we will never optimize away any logging in the future.
//
// While the object returned by the Log functions for streaming obvious has an
// actual type, users should treat it as being unspecified by the API. The
// object can be streamed to by anything that supports operator<< to a
// std::ostream but it is not itself a std::ostream. The objects should also not
// be saved or passed around, as this will result in the logs having incorrect
// source location information.
//
// If you wish to build additional log wrapper functions to add more features or
// options they should be constributed to this library rather than trying to
// build on top of the implementation details of these functions.

#ifndef ECCLESIA_LIB_LOGGING_LOGGING_H_
#define ECCLESIA_LIB_LOGGING_LOGGING_H_

#include <memory>
#include <utility>

#include "absl/strings/string_view.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/interfaces.h"

namespace ecclesia {

// Logging functions for the 5 standard log levels.
inline LogMessageStreamAndAbort FatalLog(
    SourceLocation loc = SourceLocation::current()) {
  return LogMessageStreamAndAbort(GetGlobalLogger().MakeStream(0, loc));
}
inline LogMessageStream ErrorLog(
    SourceLocation loc = SourceLocation::current()) {
  return GetGlobalLogger().MakeStream(1, loc);
}
inline LogMessageStream WarningLog(
    SourceLocation loc = SourceLocation::current()) {
  return GetGlobalLogger().MakeStream(2, loc);
}
inline LogMessageStream InfoLog(
    SourceLocation loc = SourceLocation::current()) {
  return GetGlobalLogger().MakeStream(3, loc);
}
inline LogMessageStream DebugLog(
    SourceLocation loc = SourceLocation::current()) {
  return GetGlobalLogger().MakeStream(4, loc);
}

// An function that will check a condition and log a fatal error if it fails.
// Similar to assert, but integrated into the logging system.
//
// The function takes two parameters: the condition you want to check and a
// description of the condition to be printed in the output. Additional output
// can be streamed into the result and will be appended to the failure message.
//
// Note that the description should be the condition you're checking to be true
// not an error message to be reported if it is false. For example if you were
// checking if a number is even a reasonable description use would be:
//
//   Check(x % 2 == 0, "x is an even number");
//
// The description is NOT an error message. If you want to add an error message
// then use << to add additional text to the result.
inline LogMessageStream Check(bool condition, absl::string_view description,
                              SourceLocation loc = SourceLocation::current()) {
  if (condition) {
    return GetGlobalLogger().MakeNullStream();
  } else {
    auto fatal_logger = GetGlobalLogger().MakeStream(0, loc);
    fatal_logger << "Check failed (" << description << ") ";
    return fatal_logger;
  }
}

// A macro version of Check that will just use the condition expression as the
// description. For cases where the description would just be a trivial
// description of the expression (e.g. if checking x > 0 would just be "x is
// greater than 0") this can be a little simpler.
//
// This is implemented as a macro because that's the only way to capture the
// text of the expression. However, it is named as a function for uniformity
// with the rest of the logging API.
#define CheckCondition(expr) Check(expr, #expr)

// A function that will check if a given pointer is null and report a fatal log
// if it is. Otherwise it will return the pointer.
//
// This should not be used as a standalone statement, prefer to use Check in
// those cases. This should be reserved for situations where you require the
// "returns the pointer" behavior, e.g. in a variable declaration or class
// member initializer.
template <typename T>
T &&DieIfNull(T &&ptr, SourceLocation loc = SourceLocation::current()) {
  if (ptr == nullptr) {
    FatalLog(loc) << "pointer is null";
  }
  return std::forward<T>(ptr);
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_LOGGING_LOGGING_H_
