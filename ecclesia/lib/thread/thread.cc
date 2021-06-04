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

#include "ecclesia/lib/thread/thread.h"

#include <functional>
#include <thread>  // NOLINT(build/c++11)

namespace ecclesia {

namespace {

class ThreadFactoryImpl : public ThreadFactoryInterface {
 public:
  class StdThread : public ThreadInterface {
   public:
    explicit StdThread(const std::function<void()> &runner) : thread_(runner) {}

    void Join() override { thread_.join(); }
    void Detach() override { thread_.detach(); }

    bool IsSelf() override {
      return thread_.get_id() == std::this_thread::get_id();
    }

   private:
    std::thread thread_;
  };

  std::unique_ptr<ThreadInterface> New(std::function<void()> runner) override {
    return std::make_unique<StdThread>(runner);
  }
};

}  // namespace

ThreadFactoryInterface *GetDefaultThreadFactory() {
  static ThreadFactoryInterface *factory = []() {
    return new ThreadFactoryImpl();
  }();

  return factory;
}

}  // namespace ecclesia
