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

// This defines the interface that represents a background task. These tasks
// represent something that needs to do a fixed amount of work that should be
// re-run on a recurring basis. They should not be used for background work that
// needs to "run forever".

#ifndef ECCLESIA_LIB_TASK_TASK_H_
#define ECCLESIA_LIB_TASK_TASK_H_

#include "absl/time/time.h"

namespace ecclesia {

class BackgroundTask {
 public:
  BackgroundTask() {}
  virtual ~BackgroundTask() = default;

  // Execute the background task a single time. The returned duration will tell
  // the task manager how long it must wait before calling this again.
  //
  // The task manager will not call this while a prior run is still running and
  // so it does not have to be re-entrant. However, the manager does not provide
  // any synchronizations guarantees beyond this.
  virtual absl::Duration RunOnce() = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_TASK_TASK_H_
