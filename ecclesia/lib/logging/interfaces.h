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

// Defines the interfaces used for implementing logging. This header is normally
// only needed if you want to write your own logging backend.
//
// The primary interface in ths class is the LoggerInterface. This defines an
// object that accept log messages and then route them out to different backends
// depending on the log level associated with the message.
//
// The interface doesn't set any overall constraints on how the logger chooses
// to capture the logs: you can write them to files, write them to consoles,
// discard them as uninteresting, forward them to an external service, or any
// combination of the above.

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

// Generic logger interface for writing logging out to different kinds of sinks.
class LoggerInterface {
 public:
  // Parameters that will be passed to the Write call.
  struct WriteParameters {
    // The log level of the call. This should be used to control what sinks the
    // logs will be sent to. Different LoggerInterface implementations will have
    // different behaviors for this.
    int log_level;
    // The source location where the logging call originated from.
    SourceLocation source_location;
    // The raw text to written out. This does not contain any metadata derived
    // from the above fields, so if the sink needs metadata attached the logger
    // must add it itself. This normally would be done with MakeMetadataPrefix.
    absl::string_view text;
  };

  LoggerInterface() = default;
  virtual ~LoggerInterface() = default;

  // Standard helper function for generating a standard string with metadata
  // information that can be prefixed to log lines. Captures log level,
  // timestamp and source location.
  static std::string MakeMetadataPrefix(
      int log_level, absl::Time timestamp,
      SourceLocation loc) {  // Find the base name of the source file logging.
                             // Logging the full path is
    // a bit too noisy and depends on too many details of the source tree.
    absl::string_view loc_file_name = loc.file_name();
    auto slash_pos = loc_file_name.find_last_of('/');
    if (slash_pos != loc_file_name.npos) {
      loc_file_name.remove_prefix(slash_pos + 1);
    }
    // Use a UTC timestamp with microsecond precision.
    std::string timestamp_str =
        absl::FormatTime("%Y-%m-%d %H:%M:%E6S", timestamp, absl::UTCTimeZone());
    // Combine the log level, timestamp and location into a prefix.
    return absl::StrFormat("L%d %s %s:%d] ", log_level,
                           std::move(timestamp_str), loc_file_name, loc.line());
  }

  // Take a log line and write it out to the appropriate sinks for the given log
  // level. The text does not contain a trailing newline and so the writer
  // should add one if necessary.
  //
  // NOTE: The text not having a trailing newline does not mean that it does not
  // contain any newlines.
  //
  // This function is expected to be thread-safe. In particular:
  //   * log text from concurrent writes should _never_ be interleaved
  //   * log metadata (timestamps) should be sequenced in the same way that
  //     writes are sequenced, i.e. timestamps in logs should be ordered in the
  //     same way as the log lines themselves
  // The MakeMetadataPrefix function provides a standard format for prefixing
  // log metadata to text-based logs. However, it is not required if the
  // underlying sink has its own ways to represent such metadata.
  virtual void Write(WriteParameters params) = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_LOGGING_INTERFACES_H_
