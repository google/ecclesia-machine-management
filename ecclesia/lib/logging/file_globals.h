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

// Helper library that will try and install file-based global logging. Because
// this is just a wrapper around SetGlobalLogger it has similar preconditions:
// it cannot be called while other threads which may be using the logging system
// are active.

#ifndef ECCLESIA_LIB_LOGGING_FILE_GLOBALS_H_
#define ECCLESIA_LIB_LOGGING_FILE_GLOBALS_H_

#include "absl/strings/string_view.h"

namespace ecclesia {

// Try to set the global logger to the file logger. Returns a bool indicating if
// it was successful. If it fails it will log an Error-level message to the
// existing installed logger.
//
// The parameter will be used to generate the file prefix to use for the logs.
// It is assumed to be argv[0], and the basename of the path will be used as the
// file prefix. If you pass something that is not argv[0] it should at least
// "look like" what argv[0] would.
//
// This setup will also install stacktrace dumper that will write a stacktrace
// out to all of the log files on most failure signals. This means that once
// this function has successfully been installed it cannot be safely removed.
// Thus, the global logger should not be changed again after this returns. If
// you want to install a file logger without installing the signal handler you
// will have to create and install the logger yourself.
bool TrySetGlobalLoggerToFileLogger(absl::string_view argv0);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_LOGGING_FILE_GLOBALS_H_
