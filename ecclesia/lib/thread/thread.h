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

#ifndef ECCLESIA_LIB_THREAD_THREAD_H_
#define ECCLESIA_LIB_THREAD_THREAD_H_

#include <functional>
#include <memory>

namespace ecclesia {

class ThreadInterface {
 public:
  virtual ~ThreadInterface() = default;

  virtual void Join() = 0;
  virtual void Detach() = 0;

  // Return true if the thread is the caller (E.g., pthread_self).
  virtual bool IsSelf() = 0;
};

class ThreadFactoryInterface {
 public:
  virtual ~ThreadFactoryInterface() = default;

  virtual std::unique_ptr<ThreadInterface> New(
      std::function<void()> runner) = 0;
};

ThreadFactoryInterface *GetDefaultThreadFactory();

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_THREAD_THREAD_H_
