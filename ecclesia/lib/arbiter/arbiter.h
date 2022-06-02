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

#ifndef ECCLESIA_LIB_ARBITER_H_
#define ECCLESIA_LIB_ARBITER_H_

#include <memory>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"

// These classes enable transactional behavior when using a subject resource.
// An Arbiter instance can be used to grant an exclusive lock over a backing
// instance of a subject. Only one valid ExclusiveLock object associated with
// the same arbiter can exist at once.
//
// Example usage:
//
//   std::unique_ptr<Foo> foo_ptr = ...;
//   Arbiter<Foo> foo_arbiter(std::move(foo_ptr));
//   ...
//   // Does three things sequentially.
//   Status DoThreeCommands() {
//     ASSIGN_OR_RETURN(ExclusiveLock<Foo> foo,
//                      foo_arbiter.Acquire(/*timeout=*/absl::Seconds(30)));
//     foo->CommandOne();
//     foo->CommandTwo();
//     foo->CommandThree();
//   }
//
// Ownership of the subject is shared between the arbiter and the exclusive
// locks that arbiter emits, ensuring that if the arbiter goes out of scope
// while an ExclusiveLock exists, that lock continues to be able to provide
// access to the subject previously under arbitration.

namespace ecclesia {

template <typename T>
class ExclusiveLock;

// A class for acquiring an exclusive lock on a subject. This implementation is
// thread-safe.
template <typename T>
class Arbiter {
  friend class ExclusiveLock<T>;

 public:
  // Requires that `subject` not be null.
  explicit Arbiter<T>(std::unique_ptr<T> subject)
      : arbiter_state_(std::make_shared<ArbiterState>(std::move(subject))) {}

  // This constructor supports pointers to types that use custom destructors,
  // but T must be the exact type of the subject, not a parent class.
  template <typename Deleter>
  explicit Arbiter<T>(std::unique_ptr<T, Deleter> subject)
      : arbiter_state_(std::make_shared<ArbiterState>(std::move(subject))) {}

  // Returns whether `subject_` is locked by an ExclusiveLock instance.
  bool SubjectIsLocked() const { return arbiter_state_->SubjectIsLocked(); }

  // Returns an ExclusiveLock<T> granting exclusive access to a subject,
  // relinquished only on destruction of the returned ExclusiveLock. This method
  // will block until an exclusive lock is acquired or the timeout expires,
  // whichever comes first. Will return DEADLINE_EXCEEDED if a timeout occurred
  // while waiting for exclusive lock. A non-positive timeout value will try to
  // lock without blocking.
  absl::StatusOr<ExclusiveLock<T>> Acquire(absl::Duration timeout) {
    return arbiter_state_->Acquire(timeout);
  }

  // Returns an ExclusiveLock<T> granting exclusive access to a subject,
  // relinquished only on destruction of the returned ExclusiveLock. This method
  // will block indefinitely until an exclusive lock is acquired.
  ExclusiveLock<T> AcquireOrDie() {
    // Ok to use .value() since Acquire() only fails due to timeout.
    return Acquire(absl::InfiniteDuration()).value();
  }

 private:
  // Encapsulates the state of the arbiter. Making this a separate class allows
  // shared_ptr/weak_ptr semantics to be hidden from the user.
  class ArbiterState : public std::enable_shared_from_this<ArbiterState> {
   public:
    explicit ArbiterState(std::shared_ptr<T> subject)
        : subject_(std::move(subject)) {
      Check(subject_ != nullptr, "subject is not null")
          << "Arbiter was constructed with a null subject which is strictly "
             "prohibited.";
    }

    absl::StatusOr<ExclusiveLock<T>> Acquire(absl::Duration timeout) {
      absl::MutexLock lock(&mu_);
      absl::Condition condition(this, &ArbiterState::SubjectIsUnlocked);
      if (!mu_.AwaitWithTimeout(condition, timeout)) {
        return absl::DeadlineExceededError("Could not acquire exclusive lock");
      }

      locked_ = true;

      return ExclusiveLock<T>(subject_, std::move(this->shared_from_this()));
    }

    void Release() {
      absl::MutexLock lock(&mu_);
      Check(locked_, "release called on an unlocked subject")
          << "Lock released when Arbiter not locked";
      locked_ = false;
    }

    bool SubjectIsLocked() const {
      absl::MutexLock lock(&mu_);
      return locked_;
    }

   private:
    // absl::Condition requires this method be callable while the mutex is held.
    bool SubjectIsUnlocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      return !locked_;
    }

    // The subject under arbitration. Ownership is shared with at most one
    // extant ExclusiveLock.
    std::shared_ptr<T> subject_;

    // The mutex that coordinates access to `locked_`.
    mutable absl::Mutex mu_;

    // Whether or not there exists an ExclusiveLock emitted by this arbiter
    // state.
    bool locked_ ABSL_GUARDED_BY(mu_) = false;
  };

  // Has ownership of the subject resource. Extant ExclusiveLocks have a
  // weak_ptr to this instance.
  std::shared_ptr<ArbiterState> arbiter_state_;
};

// A type for representing an exclusive lock of a subject of type T. Instances
// of this should be acquired through a call to Arbiter::Acquire().
template <typename T>
class ExclusiveLock {
 public:
  friend class Arbiter<T>;
  using ArbiterState = typename Arbiter<T>::ArbiterState;

  ExclusiveLock(ExclusiveLock &&other)
      : subject_(std::move(other.subject_)),
        releaser_(std::move(other.releaser_)) {}

  // Disallows copy.
  ExclusiveLock(const ExclusiveLock &) = delete;
  ExclusiveLock &operator=(const ExclusiveLock &) = delete;
  ExclusiveLock &operator=(ExclusiveLock &&) = delete;

  // Provides access to the contained subject.
  T *get() const { return subject_.get(); }
  T *operator->() const { return subject_.get(); }
  T &operator*() const { return *subject_; }

 private:
  // On destruction, notifies the parent arbiter that the resource can now be
  // released in a new ExclusiveLock instance.
  class Releaser {
   public:
    explicit Releaser(std::shared_ptr<ArbiterState> arbiter_state)
        : arbiter_state_(arbiter_state) {}

    ~Releaser() {
      std::shared_ptr<ArbiterState> arbiter_state = arbiter_state_.lock();
      if (arbiter_state == nullptr) {
        ErrorLog() << "ExclusiveLock destroyed after its Arbiter";
        return;
      }

      arbiter_state->Release();
    }

    std::weak_ptr<ArbiterState> arbiter_state_;
  };

  explicit ExclusiveLock(std::shared_ptr<T> subject,
                         std::shared_ptr<ArbiterState> arbiter_state)
      : subject_(std::move(subject)),
        releaser_(std::make_unique<Releaser>(std::move(arbiter_state))) {}

  // Ownership is shared with the parent arbiter.
  std::shared_ptr<T> subject_;

  // Actual holder of the lock.
  std::unique_ptr<Releaser> releaser_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_ARBITER_H_
