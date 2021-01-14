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

// Provides support for a lock-type object that's backed by file locking via
// flock

#ifndef ECCLESIA_LIB_FILE_LOCKFILE_H_
#define ECCLESIA_LIB_FILE_LOCKFILE_H_

#include <memory>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"

namespace ecclesia {

class LockFile;

// This class provides a set of operations that can be performed on the locked
// file. This class delegates the operation to the actual LockFile object.
class LockedLockFile {
 public:
  LockedLockFile(LockFile *lock_file);

  LockedLockFile(const LockedLockFile &) = delete;
  LockedLockFile &operator=(const LockedLockFile &) = delete;
  LockedLockFile(LockedLockFile &&other);
  LockedLockFile &operator=(LockedLockFile &&other);
  ~LockedLockFile();

  // Functions to read, store and clear contents of the lock file.  Note that if
  // Write or Clear fails then the file contents are left in an unknown state.
  absl::StatusOr<std::string> Read();
  absl::Status Write(absl::string_view value);
  absl::Status Clear();

 private:
  LockFile *lock_file_;
};

// This class provides a LockFile object backed by a file and provides access
// to the locked file via LockedLockFile object.
class LockFile {
 public:
  // Factory method to creating new instances of lock file.
  static absl::StatusOr<std::unique_ptr<LockFile>> Create(std::string path);

  LockFile(LockFile &) = delete;
  LockFile &operator=(LockFile &) = delete;
  ~LockFile();
  // Attempt to acquire an exclusive lock on the lockfile without blocking.
  // Returns a locked file object on success. Operations on the file are
  // performed using that object. Once the returned object is destructed, the
  // file is unlocked automatically.
  absl::StatusOr<LockedLockFile> TryLock();

  // Get the modification time of the lockfile.
  absl::StatusOr<absl::Time> GetModTime();

 private:
  friend class LockedLockFile;

  explicit LockFile(std::string path);

  // Release the held lock. The lock is released automatically when the
  // LockedLockFile instance is destructed.
  void Unlock();

  // Opens the lockfile. The file is closed automatically when the instance is
  // destructed.
  absl::Status Open();
  void Close();

  // Private members that performs operations on the file. These are called
  // using the friend class object which is returned with the file locked.
  absl::StatusOr<std::string> Read();
  absl::Status Write(absl::string_view value);
  absl::Status Clear();

 private:
  int fd_;
  std::string path_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_FILE_LOCKFILE_H_
