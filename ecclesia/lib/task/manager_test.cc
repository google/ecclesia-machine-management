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

#include "ecclesia/lib/task/manager.h"

#include <memory>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/time/time.h"
#include "ecclesia/lib/task/task.h"

namespace ecclesia {
namespace {

using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::UnorderedElementsAre;

// A stub task implementation for testing the manager.
class TestTask : public BackgroundTask {
 public:
  TestTask() = default;

  absl::Duration RunOnce() override { return absl::InfiniteDuration(); }
};

// A minimal manager implementation that can add and remove tasks but doesn't
// actually run anything.
class TestManager : public BackgroundTaskManager {
 public:
  TestManager() = default;

  absl::flat_hash_set<BackgroundTask *> RegisteredTasks() const {
    absl::flat_hash_set<BackgroundTask *> registered;
    registered.reserve(tasks_.size());
    for (const auto &task : tasks_) {
      registered.insert(task.get());
    }
    return registered;
  }

 private:
  absl::flat_hash_set<std::unique_ptr<BackgroundTask>> tasks_;

  void AddTaskImpl(std::unique_ptr<BackgroundTask> task) override {
    tasks_.insert(std::move(task));
  }

  void RemoveTaskImpl(BackgroundTask *task) override { tasks_.erase(task); }
};

TEST(BackgroundTaskManagerTest, RegisterAndUnregister) {
  TestManager manager;

  // No tasks registered at the start.
  EXPECT_THAT(manager.RegisteredTasks(), IsEmpty());

  {
    // Create an register a task. Normally we wouldn't need to save off a raw
    // pointer to the created task but for testing we need something to compare
    // with what the manager returns.
    auto task = std::make_unique<TestTask>();
    TestTask *task_ptr = task.get();
    auto remover = manager.AddTask(std::move(task));
    // The task should now be registered.
    EXPECT_EQ(remover.task(), task_ptr);
    EXPECT_THAT(manager.RegisteredTasks(), UnorderedElementsAre(task_ptr));
  }

  // The task should now be removed.
  EXPECT_THAT(manager.RegisteredTasks(), IsEmpty());
}

TEST(BackgroundTaskManagerTest, RemoverCanBeMoved) {
  TestManager manager;

  // No tasks registered at the start.
  EXPECT_THAT(manager.RegisteredTasks(), IsEmpty());

  {
    // Create an register a task.
    BackgroundTaskManager::Remover<TestTask> remover =
        manager.AddTask(std::make_unique<TestTask>());
    // Move the remover.
    BackgroundTaskManager::Remover<TestTask> remover_move(std::move(remover));
    EXPECT_THAT(remover.task(), IsNull());
    // Move-assign the remover.
    BackgroundTaskManager::Remover<TestTask> remover_massign =
        std::move(remover_move);
    EXPECT_THAT(remover_move.task(), IsNull());
    // The task should still be registered.
    EXPECT_THAT(manager.RegisteredTasks(),
                UnorderedElementsAre(remover_massign.task()));
  }

  // The task should now be removed.
  EXPECT_THAT(manager.RegisteredTasks(), IsEmpty());
}

TEST(BackgroundTaskManagerTest, RemoverManualInvoke) {
  TestManager manager;

  // No tasks registered at the start.
  EXPECT_THAT(manager.RegisteredTasks(), IsEmpty());

  // Create an register a task.
  auto remover = manager.AddTask(std::make_unique<TestTask>());
  // The task is registered.
  EXPECT_THAT(manager.RegisteredTasks(), UnorderedElementsAre(remover.task()));

  // Manually invoke the remover.
  remover.Invoke();
  EXPECT_THAT(remover.task(), IsNull());
  // No tasks are registered.
  EXPECT_THAT(manager.RegisteredTasks(), IsEmpty());
}

}  // namespace
}  // namespace ecclesia
