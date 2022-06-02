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

#ifndef ECCLESIA_LIB_TASK_TASK_WRAPPER_MANAGER_H_
#define ECCLESIA_LIB_TASK_TASK_WRAPPER_MANAGER_H_

#include <cassert>
#include <memory>
#include <utility>

#include "ecclesia/lib/task/manager.h"
#include "ecclesia/lib/task/task.h"

namespace ecclesia {

// Implement a trivial background task manager. This is used to capture a
// provided task. It doesn't actually run anything, it just saves it and exposes
// it publicly for tests to be able to call. Only a single task can be wrapped.
class SingleTaskWrapperManager : public BackgroundTaskManager {
 public:
  SingleTaskWrapperManager() {}

  // Expose the underlying task for tests to use.
  BackgroundTask *WrappedTask() const { return task_.get(); }

 private:
  // Handle the task registration. This assumes that only one task (the updater)
  // will be registered. If a second task gets registered this will assert.
  void AddTaskImpl(std::unique_ptr<BackgroundTask> task) override {
    // Should not register more than one task;
    assert(!task_);
    task_ = std::move(task);
  }

  // Unregister the task. Will assert if the task doesn't match the stored task.
  // Otherwise it just deletes the task. We don't actually run the tasks in the
  // background so we don't need to wait for anything to finish running.
  void RemoveTaskImpl(BackgroundTask *task) override {
    // Tried to remove a task that was never registered.
    assert(task == task_.get());
    task_ = nullptr;
  }

  std::unique_ptr<BackgroundTask> task_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_TASK_TASK_WRAPPER_MANAGER_H_
