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

#include "ecclesia/lib/logging/file_globals.h"

#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/logging/file.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/interfaces.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

bool TrySetGlobalLoggerToFileLogger(absl::string_view argv0) {
  auto maybe_logger =
      FileLogger::Create(GetBasename(argv0), Clock::RealClock());
  if (maybe_logger.ok()) {
    FileLogger *logger = maybe_logger->get();
    SetGlobalLogger(std::move(*maybe_logger));
    logger->InstallFailureHandler();
    return true;
  }
  ErrorLog() << "unable to enable file-based logging: "
             << maybe_logger.status();
  return false;
}

}  // namespace ecclesia
