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

#include "ecclesia/lib/logging/file.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/logging/posix.h"
#include "ecclesia/lib/time/clock.h"

ABSL_FLAG(
    std::vector<std::string>, ecclesia_file_logging_dirs,
    std::vector<std::string>({"/data/logs", "/tmp"}),
    "Directories that the file logging should attempt to write logs to. The "
    "first directory in the list that exists is the one which will be used.");

ABSL_FLAG(int, ecclesia_file_log_level, -1,
          "The level of logging to create log files for. If none is provided "
          "or the value provided is outside of 0 to 4 inclusive range, the "
          "default of kDefaultLogLevel is used instead.");

namespace ecclesia {
namespace {

constexpr int kDefaultLogLevel = 3;
constexpr int kMaxLogLevel = 4;

// Provides a function that can be called by the absl failure signal handler to
// log the given test to all of the known logfiles. In order to make this
// handler async-signal-safe the logfiles are stored in a global variable as a
// array of file descriptors. This array should be populated once when the
// handler is installed and never touched again.
int *logging_file_descriptors = nullptr;   // Array of log file fds.
size_t logging_file_descriptors_size = 0;  // Size of the above array.
void SignalHandlerWriter(const char *failure_data) {
  if (!failure_data) return;  // No data, nothing to write.
  // Go through each file descriptor and write out the failure data. If any of
  // the write calls fails we just give up and move on.
  size_t failure_data_size = std::strlen(failure_data);
  for (size_t i = 0; i < logging_file_descriptors_size; ++i) {
    int fd = logging_file_descriptors[i];
    if (fd < 0) break;  // If for some reason the fd is bad, skip it.
    // Track the remaining data to be written out.
    const char *remaining_data = failure_data;
    size_t remaining_data_size = failure_data_size;
    // Write data until we're done or write fails.
    while (remaining_data_size > 0) {
      ssize_t ret = write(fd, remaining_data, remaining_data_size);
      // Treat both negative and zero return values as errors. While a zero
      // value might mean we could continue with additional writes, we don't
      // want to potentially get stuck in a loop making no forward progrsss in
      // the signal handler and so we give up immediately.
      if (ret <= 0) break;
      // If we get here we wrote out at least some of the bytes.
      remaining_data += ret;
      remaining_data_size -= ret;
    }
  }
}

// Find the directory to use for logging. This will be found by checking for a
// writeable logging directory.
absl::StatusOr<std::string> FindLoggingDirectory() {
  for (const std::string &dir :
       absl::GetFlag(FLAGS_ecclesia_file_logging_dirs)) {
    if (access(dir.c_str(), W_OK) == 0) {
      return dir;
    }
  }
  return absl::NotFoundError("no writable log directory could be found");
}

}  // namespace

absl::StatusOr<std::unique_ptr<FileLogger>> FileLogger::Create(
    absl::string_view base_filename, Clock *clock) {
  // Try to find a logging directory to use.
  auto maybe_dir = FindLoggingDirectory();
  if (!maybe_dir.ok()) return maybe_dir.status();

  // We found a logging directory, so try to create files.
  int log_level = absl::GetFlag(FLAGS_ecclesia_file_log_level);
  if (log_level < 0 || log_level > kMaxLogLevel) {
    log_level = kDefaultLogLevel;
  }
  absl::Time creation_time = clock->Now();
  std::vector<LogFile> logfiles;
  logfiles.reserve(log_level + 1);
  for (int level = 0; level <= log_level; ++level) {
    logfiles.push_back(
        OpenLogFile(*maybe_dir, base_filename, level, creation_time));
  }

  // Check that all of the files were able to be opened. We only consider this a
  // success if all of the files could open.
  for (const auto &logfile : logfiles) {
    if (!logfile.stream.good()) {
      return absl::InternalError("unable to open all log files");
    }
  }

  // If we get here then everything needed to create the logger works.
  return absl::WrapUnique(new FileLogger(clock, std::move(logfiles)));
}

void FileLogger::InstallFailureHandler() {
  // It seems a bit weird to call the logging system while setting up the
  // logging system but this should actually be safe. Even if for some reason
  // this is called before "this" has been installed as the global logger you'll
  // still have the existing handler to catch this.
  Check(logging_file_descriptors == nullptr,
        "a failure handler has not already been installed");
  // Allocate the file descriptor array. Populate it with -1 so that if we can't
  // open the log files then the handler can tell the fd is invalid.
  logging_file_descriptors = new int[logfiles_.size()];
  logging_file_descriptors_size = logfiles_.size();
  std::fill_n(logging_file_descriptors, logging_file_descriptors_size, -1);
  // Create file descriptors for each log file, in append mode. If the creation
  // fails there's not much we can do other than logging an error.
  for (size_t i = 0; i < logfiles_.size(); ++i) {
    int fd = open(logfiles_[i].path.c_str(), O_WRONLY | O_APPEND);
    if (fd == -1) {
      ErrorLog() << "unable to open " << logfiles_[i].path
                 << " for writing stacktraces on program exit";
    } else {
      logging_file_descriptors[i] = fd;
    }
  }
  // Install the global handler.
  absl::InstallFailureSignalHandler({.writerfn = &SignalHandlerWriter});
}

FileLogger::FileLogger(Clock *clock, std::vector<LogFile> logfiles)
    : clock_(clock), logfiles_(std::move(logfiles)) {}

FileLogger::LogFile FileLogger::OpenLogFile(absl::string_view logging_dir,
                                            absl::string_view base_filename,
                                            int log_level,
                                            absl::Time creation_time) {
  // Determine the filenames to use.
  std::string timestamp_str =
      absl::FormatTime("%Y%m%d-%H%M%E6S", creation_time, absl::UTCTimeZone());
  std::string log_filename =
      absl::StrFormat("%s.log.L%d.%s", base_filename, log_level, timestamp_str);

  // Create the symlink that this will point to.
  std::string symlink_path = JoinFilePaths(
      logging_dir, absl::StrFormat("%s.log.L%d", base_filename, log_level));
  // Try to unlink. This can "fail" normally if the path just doens't exist but
  // for any other failure log an error.
  if (unlink(symlink_path.c_str()) == -1 && errno != ENOENT) {
    PosixErrorLog() << "unable to remove " << symlink_path << " symlink";
  }
  // Try to symlink. If it fails log an error, but continue.
  if (symlink(log_filename.c_str(), symlink_path.c_str()) == -1) {
    PLOG(ERROR) << "unable to create " << symlink_path << " symlink";
  }

  // Construct the actual log file stream.
  std::string log_path = JoinFilePaths(logging_dir, log_filename);
  std::ofstream log_stream(log_path);
  return {.path = std::move(log_path), .stream = std::move(log_stream)};
}

void FileLogger::Write(WriteParameters params) {
  // Before even taking the log mutex make sure the log level is low enough
  // that it will go to at least one file. This is technically redundant but
  // it does minimize the work done for completely discarded logs.
  if (params.log_level < logfiles_.size()) {
    absl::MutexLock ml(&mutex_);
    std::string metadata = MakeMetadataPrefix(params.log_level, clock_->Now(),
                                              params.source_location);
    // Write the log out to every log file of a matching or lower severity.
    // Note that highest severity == 0, so lower means counting UP, not down.
    for (size_t i = params.log_level; i < logfiles_.size(); ++i) {
      logfiles_[i].stream << metadata << params.text << std::endl;
    }
    // If the log is a fatal log then we also write the log to standard error
    // and terminate the process.
    if (params.log_level == 0) {
      std::cerr << metadata << params.text << std::endl;
      std::abort();
    }
  }
}

}  // namespace ecclesia
