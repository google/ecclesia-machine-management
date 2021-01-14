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

#include "ecclesia/lib/logging/globals.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "ecclesia/lib/logging/interfaces.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {
namespace {

// Function that holds the underlying global logger stream factory.
LoggerStreamFactory &GetGlobalLoggerStreamFactory() {
  static auto &lsf = *(new LoggerStreamFactory(
      absl::make_unique<DefaultLogger>(Clock::RealClock())));
  return lsf;
}

}  // namespace

DefaultLogger::DefaultLogger(Clock *clock) : clock_(clock) {}

void DefaultLogger::Write(WriteParameters params) {
  if (params.log_level < 2) {
    absl::MutexLock ml(&mutex_);
    std::cerr << MakeMetadataPrefix(params.log_level, clock_->Now(),
                                    params.source_location)
              << params.text << std::endl;
    if (params.log_level == 0) std::abort();
  }
}

LogMessageStreamAndAbort::LogMessageStreamAndAbort(LogMessageStream lms)
    : WrappedLogMessageStream(*this, std::move(lms)) {}

LogMessageStreamAndAbort::~LogMessageStreamAndAbort() {
  lms_.Flush();
  std::abort();
}

LoggerStreamFactory::LoggerStreamFactory(
    std::unique_ptr<LoggerInterface> logger)
    : logger_(std::move(logger)) {}

void LoggerStreamFactory::SetLogger(std::unique_ptr<LoggerInterface> logger) {
  logger_ = std::move(logger);
}

const LoggerStreamFactory &GetGlobalLogger() {
  return GetGlobalLoggerStreamFactory();
}

void SetGlobalLogger(std::unique_ptr<LoggerInterface> logger) {
  GetGlobalLoggerStreamFactory().SetLogger(std::move(logger));
}

}  // namespace ecclesia
