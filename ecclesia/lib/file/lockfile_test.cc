/*
 * Copyright 2021 Google LLC
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

#include "ecclesia/lib/file/lockfile.h"

#include <memory>
#include <thread>  // NOLINT(build/c++11)

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/functional/bind_front.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/status/test_macros.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::testing::Not;

// A helper thread class that grabs the lock file, signals that it has done so,
// and then waits to be signaled to release it.
class LockThread {
 public:
  LockThread(LockFile *lock, absl::Notification *lock_held,
             absl::Notification *done)
      : lock_(lock), lock_held_(lock_held), done_(done) {}

  void Start() {
    thread_ = std::thread(absl::bind_front(&LockThread::Run, this));
  }

  void Run() {
    ECCLESIA_ASSIGN_OR_FAIL(LockedLockFile locked_file, lock_->TryLock());
    lock_held_->Notify();
    done_->WaitForNotification();
  }

  void Join() { thread_.join(); }

 private:
  LockFile *lock_;
  absl::Notification *lock_held_;
  absl::Notification *done_;
  std::thread thread_;
};

// Sets up a temporary directory in the test directory to store lock files
// created during each test.
class LockFileTest : public ::testing::Test {
 protected:
  LockFileTest() : test_fs_(GetTestTempdirPath()) {}

  static std::string LockPath(absl::string_view filename) {
    return JoinFilePaths(GetTestTempdirPath(), filename);
  }

  TestFilesystem test_fs_;
};

TEST_F(LockFileTest, TestSharedLockfileInstance) {
  ECCLESIA_ASSIGN_OR_FAIL(auto lock_file,
                          LockFile::Create(LockPath("shared.lock")));
  absl::Notification lock_held;
  absl::Notification end_thread;

  LockThread thread(lock_file.get(), &lock_held, &end_thread);
  thread.Start();
  lock_held.WaitForNotification();

  // The instance is shared so both the thread and test can hold the lock.
  EXPECT_THAT(lock_file->TryLock(), IsOk());

  end_thread.Notify();
  thread.Join();
}

TEST_F(LockFileTest, TestSeparateLockfileInstance) {
  ECCLESIA_ASSIGN_OR_FAIL(
      auto lock_for_thread,
      LockFile::Create(LockPath("separate_instance_test.lock")));
  ECCLESIA_ASSIGN_OR_FAIL(
      auto lock_for_test,
      LockFile::Create(LockPath("separate_instance_test.lock")));
  absl::Notification lock_held;
  absl::Notification end_thread;

  LockThread thread(lock_for_thread.get(), &lock_held, &end_thread);
  thread.Start();
  lock_held.WaitForNotification();

  // Only one instance can hold the lock.
  EXPECT_THAT(lock_for_test->TryLock(), Not(IsOk()));

  end_thread.Notify();
  thread.Join();

  // Once the other instance relinquishes the lock, locking should succeed for
  // the other thread.
  EXPECT_THAT(lock_for_test->TryLock(), IsOk());
}

TEST_F(LockFileTest, TestSeparateLockfiles) {
  ECCLESIA_ASSIGN_OR_FAIL(auto lock_for_thread,
                          LockFile::Create(LockPath("file1.lock")));
  ECCLESIA_ASSIGN_OR_FAIL(auto lock_for_test,
                          LockFile::Create(LockPath("file2.lock")));
  absl::Notification lock_held;
  absl::Notification end_thread;

  LockThread thread(lock_for_thread.get(), &lock_held, &end_thread);
  thread.Start();
  lock_held.WaitForNotification();

  // The locks are different files, so both locks can be held.
  EXPECT_THAT(lock_for_test->TryLock(), IsOk());
  end_thread.Notify();

  EXPECT_THAT(lock_for_test->TryLock(), IsOk());
  thread.Join();
}

//
// Tests of the lockfile serialization.
//

TEST_F(LockFileTest, ReadEmptyFileTest) {
  ECCLESIA_ASSIGN_OR_FAIL(auto lock_file,
                          LockFile::Create(LockPath("emptyfile.lock")));
  ECCLESIA_ASSIGN_OR_FAIL(auto locked_file, lock_file->TryLock());
  EXPECT_THAT(locked_file.Read(), IsOkAndHolds(""));
}

TEST_F(LockFileTest, WrittenFileCanBeReadBackTest) {
  ECCLESIA_ASSIGN_OR_FAIL(auto lock_file,
                          LockFile::Create(LockPath("read_write.lock")));
  ECCLESIA_ASSIGN_OR_FAIL(auto locked_file, lock_file->TryLock());
  EXPECT_THAT(locked_file.Write("device-id: abcd"), IsOk());
  EXPECT_THAT(locked_file.Read(), IsOkAndHolds("device-id: abcd"));
}

TEST_F(LockFileTest, MultipleWritesGetLastWriteTest) {
  ECCLESIA_ASSIGN_OR_FAIL(auto lock_file,
                          LockFile::Create(LockPath("multiple_writes.lock")));
  ECCLESIA_ASSIGN_OR_FAIL(auto locked_file, lock_file->TryLock());
  EXPECT_THAT(locked_file.Write("device-id: abcd"), IsOk());
  EXPECT_THAT(locked_file.Write("device-id: pqrs"), IsOk());
  EXPECT_THAT(locked_file.Read(), IsOkAndHolds("device-id: pqrs"));
}

TEST_F(LockFileTest, WritesUsingTwoDifferentLocksTest) {
  {
    // Write data to the lock file
    ECCLESIA_ASSIGN_OR_FAIL(auto lock_file,
                            LockFile::Create(LockPath("diff_write.lock")));
    ECCLESIA_ASSIGN_OR_FAIL(auto locked_file, lock_file->TryLock());
    EXPECT_THAT(locked_file.Write("device-id: abcd"), IsOk());
  }
  {
    // Ensure that lock file is unlocked. Lock it again and read data back in.
    ECCLESIA_ASSIGN_OR_FAIL(auto lock_file,
                            LockFile::Create(LockPath("diff_write.lock")));
    ECCLESIA_ASSIGN_OR_FAIL(auto locked_file, lock_file->TryLock());
    EXPECT_THAT(locked_file.Read(), IsOkAndHolds("device-id: abcd"));
  }
}

TEST_F(LockFileTest, ClearTest) {
  ECCLESIA_ASSIGN_OR_FAIL(auto lock_file,
                          LockFile::Create(LockPath("write_clear.lock")));
  ECCLESIA_ASSIGN_OR_FAIL(auto locked_file, lock_file->TryLock());
  EXPECT_THAT(locked_file.Write("device-id: abcd"), IsOk());
  EXPECT_THAT(locked_file.Clear(), IsOk());
  EXPECT_THAT(locked_file.Read(), IsOkAndHolds(""));
}

}  // namespace
}  // namespace ecclesia
