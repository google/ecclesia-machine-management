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

// Helper functions for logging information from POSIX errors. These will use
// errno and strerror to produce generic error messages that can be included in
// logging when a failure occurs.

#ifndef ECCLESIA_LIB_LOGGING_POSIX_H_
#define ECCLESIA_LIB_LOGGING_POSIX_H_

#include <string>

#include "absl/base/attributes.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/interfaces.h"

namespace ecclesia {

// Given an errno value, produce a generic error message containing both the
// error number as well as a standard string message describing it.
std::string PosixErrorMessage(int errno_value);

// Wrapper around LogMessageStream that will append the PosixErrorMessage output
// just before the contents are flushed.
class PosixLogMessageStream
    : public WrappedLogMessageStream<PosixLogMessageStream> {
 public:
  PosixLogMessageStream(int captured_errno, LogMessageStream lms);

  PosixLogMessageStream(const PosixLogMessageStream &other) = delete;
  PosixLogMessageStream &operator=(const PosixLogMessageStream &other) = delete;

  ~PosixLogMessageStream();

 private:
  int captured_errno_;
};

// Wrapper that also adds in an abort for fatal logs, similar to the standard
// wrapper used by FatalLog.
class PosixLogMessageStreamAndAbort
    : public WrappedLogMessageStream<PosixLogMessageStreamAndAbort> {
 public:
  PosixLogMessageStreamAndAbort(int captured_errno, LogMessageStream lms);

  PosixLogMessageStreamAndAbort(const PosixLogMessageStreamAndAbort &other) =
      delete;
  PosixLogMessageStreamAndAbort &operator=(
      const PosixLogMessageStreamAndAbort &other) = delete;

  ABSL_ATTRIBUTE_NORETURN ~PosixLogMessageStreamAndAbort();

 private:
  int captured_errno_;
};

// Logging functions that capture errno append PosixErrorMessage to the logging.
// Each Posix*Log corresponds to the standard *Log function.
PosixLogMessageStreamAndAbort PosixFatalLog(
    SourceLocation loc = SourceLocation::current());
PosixLogMessageStream PosixErrorLog(
    SourceLocation loc = SourceLocation::current());
PosixLogMessageStream PosixWarningLog(
    SourceLocation loc = SourceLocation::current());
PosixLogMessageStream PosixInfoLog(
    SourceLocation loc = SourceLocation::current());
PosixLogMessageStream PosixDebugLog(
    SourceLocation loc = SourceLocation::current());

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_LOGGING_POSIX_H_
