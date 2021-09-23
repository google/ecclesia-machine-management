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

#ifndef ECCLESIA_LIB_THREAD_THREAD_MOCK_H_
#define ECCLESIA_LIB_THREAD_THREAD_MOCK_H_

#include <functional>
#include <memory>

#include "gmock/gmock.h"
#include "ecclesia/lib/thread/thread.h"

namespace ecclesia {

class MockThread : public ThreadInterface {
 public:
  explicit MockThread(std::function<void()> runner) : runner_(runner) {}

  MOCK_METHOD(void, Join, (), (override));
  MOCK_METHOD(void, Detach, (), (override));
  MOCK_METHOD(bool, IsSelf, (), (override));

  void Run() { runner_(); }

 private:
  std::function<void()> runner_;
};

class MockThreadFactory : public ThreadFactoryInterface {
 public:
  MOCK_METHOD(std::unique_ptr<ThreadInterface>, New, (std::function<void()>),
              (override));
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_THREAD_THREAD_MOCK_H_
