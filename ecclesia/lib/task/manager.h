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

// This library defines a generic interface a class for scheduling and running
// background tasks. The interface for tasks themselves is defined in task.h.
//
// This header does not provide an implementation, or specify any particular
// threading or scheduling library. The intent is to provide a generic API for
// scheduling tasks while allowing the implementation to be written and selected
// based upon whatever scheduling primitives are available in the build and
// runtime environments.

#ifndef ECCLESIA_LIB_TASK_MANAGER_H_
#define ECCLESIA_LIB_TASK_MANAGER_H_

#include <memory>
#include <utility>

#include "ecclesia/lib/task/task.h"

namespace ecclesia {

class BackgroundTaskManager {
 public:
  // An RAII object that removes the contained task upon destruction. Note that
  // destroying this object can block, because unregistering a task can block.
  template <typename TaskType>
  class Remover {
   public:
    // Default constructor. This will be associated a null task and so
    // destroying it will do nothing.
    Remover() : manager_(nullptr), task_(nullptr) {}
    // Normal constructor. This will remove the given task from the provided
    // background manager when the Remover object is destroyed.
    Remover(BackgroundTaskManager *manager, TaskType *task)
        : manager_(manager), task_(task) {}

    // You can't copy a remover, as is standard for RAII objects. But you can
    // move it. However, we need to manually implement the move in order to
    // ensure that the stored task is nulled out in the moved-from object.
    Remover(const Remover &other) = delete;
    Remover &operator=(const Remover &other) = delete;
    Remover(Remover &&other) : manager_(other.manager_), task_(other.task_) {
      other.task_ = nullptr;
    }
    Remover &operator=(Remover &&other) {
      manager_ = other.manager_;
      task_ = other.task_;
      other.task_ = nullptr;
      return *this;
    }

    ~Remover() {
      if (task_) manager_->RemoveTaskImpl(task_);
    }

    // The underlying task that this remover will remove.
    TaskType *task() const { return task_; }

    // Carry out the removal of the task this remover is managing. After calling
    // this the remover will contain a null task.
    void Invoke() {
      if (task_) manager_->RemoveTaskImpl(task_);
      task_ = nullptr;
    }

   private:
    BackgroundTaskManager *manager_;
    TaskType *task_;
  };

  BackgroundTaskManager() {}
  virtual ~BackgroundTaskManager() = default;

  // Register a new task to run. The background task manager will continue
  // calling the task periodically until it is unregistered.
  //
  // This will return a Remover that will unregister the task when it is
  // destroyed. The caller is responsible for saving the remover until it is
  // prepared to shut down and destroy the task.
  template <typename TaskType>
  Remover<TaskType> AddTask(std::unique_ptr<TaskType> task) {
    TaskType *task_ptr = task.get();
    AddTaskImpl(std::move(task));
    return Remover(this, task_ptr);
  }

 private:
  virtual void AddTaskImpl(std::unique_ptr<BackgroundTask> task) = 0;

  // Unregister an existing task. This can block, as the manager will not
  // consider the task removed if it is being executed and so this may need to
  // wait until an outstanding run has terminated.
  virtual void RemoveTaskImpl(BackgroundTask *task) = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_TASK_MANAGER_H_
