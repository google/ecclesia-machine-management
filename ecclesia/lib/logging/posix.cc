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

#include "ecclesia/lib/logging/posix.h"

#include <cerrno>
#include <cstdlib>
#include <string>
#include <system_error>
#include <utility>

#include "absl/strings/str_format.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/interfaces.h"

namespace ecclesia {

std::string PosixErrorMessage(int errno_value) {
  auto ec = std::make_error_code(static_cast<std::errc>(errno_value));
  return absl::StrFormat(" POSIX error %d [%s]", errno_value, ec.message());
}

PosixLogMessageStream::PosixLogMessageStream(int captured_errno,
                                             LogMessageStream lms)
    : WrappedLogMessageStream(*this, std::move(lms)),
      captured_errno_(captured_errno) {}

PosixLogMessageStream::~PosixLogMessageStream() {
  lms_ << PosixErrorMessage(captured_errno_);
  lms_.Flush();
}

PosixLogMessageStreamAndAbort::PosixLogMessageStreamAndAbort(
    int captured_errno, LogMessageStream lms)
    : WrappedLogMessageStream(*this, std::move(lms)),
      captured_errno_(captured_errno) {}

PosixLogMessageStreamAndAbort::~PosixLogMessageStreamAndAbort() {
  lms_ << PosixErrorMessage(captured_errno_);
  lms_.Flush();
  std::abort();
}

PosixLogMessageStreamAndAbort PosixFatalLog(SourceLocation loc) {
  int captured_errno = errno;
  return PosixLogMessageStreamAndAbort(captured_errno,
                                       GetGlobalLogger().MakeStream(0, loc));
};

PosixLogMessageStream PosixErrorLog(SourceLocation loc) {
  int captured_errno = errno;
  return PosixLogMessageStream(captured_errno,
                               GetGlobalLogger().MakeStream(1, loc));
}

PosixLogMessageStream PosixWarningLog(SourceLocation loc) {
  int captured_errno = errno;
  return PosixLogMessageStream(captured_errno,
                               GetGlobalLogger().MakeStream(2, loc));
}

PosixLogMessageStream PosixInfoLog(SourceLocation loc) {
  int captured_errno = errno;
  return PosixLogMessageStream(captured_errno,
                               GetGlobalLogger().MakeStream(3, loc));
}

PosixLogMessageStream PosixDebugLog(SourceLocation loc) {
  int captured_errno = errno;
  return PosixLogMessageStream(captured_errno,
                               GetGlobalLogger().MakeStream(4, loc));
}

}  // namespace ecclesia
