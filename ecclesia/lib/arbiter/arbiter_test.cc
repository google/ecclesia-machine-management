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

#include "ecclesia/lib/arbiter/arbiter.h"

#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "ecclesia/lib/testing/status.h"

using ::testing::StrictMock;

namespace ecclesia {
namespace {

// For testing we need a class which will be wrapped in an arbiter
// with a method to call
class Foo {
 public:
  virtual absl::Status DoSomething() = 0;
  virtual absl::Status DeleteSomething() = 0;
  virtual ~Foo() = default;
};

class FooMock : public Foo {
 public:
  MOCK_METHOD(absl::Status, DoSomething, (), (override));
  MOCK_METHOD(absl::Status, DeleteSomething, (), (override));
};

void FooDeleter(Foo *foo) {
  ASSERT_TRUE(foo->DeleteSomething().ok());
  delete foo;
}

// Convenient shorthand duration.
constexpr absl::Duration k500ms = absl::Milliseconds(500);

class ArbiterTest : public ::testing::Test {
 protected:
  // Expects that `client_lock` has a lock on `mock_client`, and that
  // operator->() works as expected.
  template <typename T>
  void ExpectLocked(const ExclusiveLock<T> &foo_lock,
                    StrictMock<FooMock> *foo_mock) {
    ASSERT_EQ(foo_lock.get(), foo_mock);
    EXPECT_CALL(*foo_mock, DoSomething());
    EXPECT_TRUE(foo_lock->DoSomething().ok());
  }
};

TEST_F(ArbiterTest, Acquire) {
  auto foo = std::make_unique<StrictMock<FooMock>>();
  auto foo_ptr = foo.get();

  Arbiter<Foo> arbiter(std::move(foo));

  ASSERT_FALSE(arbiter.SubjectIsLocked());

  {
    auto maybe_el = arbiter.Acquire(k500ms);
    ASSERT_TRUE(maybe_el.ok());
    ExclusiveLock<Foo> exclusive_lock = std::move(*maybe_el);

    ASSERT_TRUE(arbiter.SubjectIsLocked());

    ExpectLocked(exclusive_lock, foo_ptr);
    EXPECT_THAT(arbiter.Acquire(k500ms), IsStatusDeadlineExceeded());
  }

  ASSERT_FALSE(arbiter.SubjectIsLocked());

  ASSERT_TRUE(arbiter.Acquire(k500ms).ok());
}

TEST_F(ArbiterTest, AcquireOrDie) {
  auto foo = std::make_unique<StrictMock<FooMock>>();
  auto foo_ptr = foo.get();

  Arbiter<Foo> arbiter(std::move(foo));

  ASSERT_FALSE(arbiter.SubjectIsLocked());

  {
    ExclusiveLock<Foo> exclusive_lock = arbiter.AcquireOrDie();

    ASSERT_TRUE(arbiter.SubjectIsLocked());

    ExpectLocked(exclusive_lock, foo_ptr);
    EXPECT_THAT(arbiter.Acquire(k500ms), IsStatusDeadlineExceeded());
  }

  ASSERT_FALSE(arbiter.SubjectIsLocked());
  ExclusiveLock<Foo> exclusive_lock = arbiter.AcquireOrDie();
}

TEST_F(ArbiterTest, NonDefaultDeleter) {
  auto foo = std::unique_ptr<StrictMock<FooMock>, decltype(&FooDeleter)>(
      new StrictMock<FooMock>(), FooDeleter);
  auto foo_ptr = foo.get();

  Arbiter<StrictMock<FooMock>> arbiter(std::move(foo));
  ExclusiveLock<StrictMock<FooMock>> exclusive_lock = arbiter.AcquireOrDie();

  EXPECT_CALL(*foo_ptr, DeleteSomething());
}

TEST_F(ArbiterTest, OutOfOrderDestruction) {
  StrictMock<FooMock> *foo_ptr;

  // This is silly, don't do anything like this in real code.
  ExclusiveLock<Foo> exclusive_lock = [&]() {
    auto foo = std::make_unique<StrictMock<FooMock>>();
    foo_ptr = foo.get();

    return Arbiter<Foo>(std::move(foo)).AcquireOrDie();
  }();

  ExpectLocked(exclusive_lock, foo_ptr);
}

}  // namespace
}  // namespace ecclesia
