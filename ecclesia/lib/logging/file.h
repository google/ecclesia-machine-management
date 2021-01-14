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

// This library provides an implementation of the logging interface that will
// write log data out to files on the filesystem. It will create separate files
// for every logging level that will include all logs from that level as well
// as all the more-severe levels.
//
// For processes that wish to use this logging they should use SetGlobalLogger
// to install a copy of this logger in their main.

#ifndef ECCLESIA_LIB_LOGGING_FILE_H_
#define ECCLESIA_LIB_LOGGING_FILE_H_

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "ecclesia/lib/logging/interfaces.h"
#include "ecclesia/lib/time/clock.h"

ABSL_DECLARE_FLAG(std::vector<std::string>, ecclesia_file_logging_dirs);

namespace ecclesia {

class FileLogger : public LoggerInterface {
 public:
  // Create a logger instance that will log to all files with the given base
  // filename. This is conventionally the basename of the path in argv[0].
  //
  // This will fail if the logging files cannot be created.
  static absl::StatusOr<std::unique_ptr<FileLogger>> Create(
      absl::string_view base_filename, Clock *clock);

  // Installs a signal handler that will log stacktraces to standard error and
  // the log files. This will install signal handlers and set global variables
  // and so should not be called more than once during the run of a process (and
  // is certainly thread-hostile).
  //
  // You normally don't want to call this until after this logger has already
  void InstallFailureHandler();

 private:
  // Unchecked constructor. This is only called by the Create() factory after
  // all of the parameters have been successfully constructed.
  struct LogFile {
    std::string path;
    std::ofstream stream;
  };
  FileLogger(Clock *clock, std::vector<LogFile> logfiles);

  // Open a new level-specific logging file in the specified directory. This
  // will also create/update a shorthand symlink to the new file, now the "most
  // recent" file created.
  static LogFile OpenLogFile(absl::string_view logging_dir,
                             absl::string_view base_filename, int log_level,
                             absl::Time creation_time);

  void Write(WriteParameters params) override;

  // Used to synchronize all logging operations.
  absl::Mutex mutex_;
  // Used to capture timestamps for the log lines.
  Clock *clock_;

  // The log files that will be written to. The file for logging level N
  // corresponds to logfiles_[N].
  std::vector<LogFile> logfiles_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_LOGGING_FILE_H_
