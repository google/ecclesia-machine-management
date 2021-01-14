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

#ifndef ECCLESIA_MAGENT_DAEMONS_COMMON_MAIN_H_
#define ECCLESIA_MAGENT_DAEMONS_COMMON_MAIN_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "ecclesia/magent/lib/thread_pool/thread_pool.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver_interface.h"

ABSL_FLAG(int, port, 3995, "Port number for the magent to listen on");
ABSL_FLAG(std::string, assemblies_dir, "/etc/google/magent",
          "Path to a directory containing JSON Assemblies");

namespace ecclesia {

class RequestExecutor : public tensorflow::serving::net_http::EventExecutor {
 public:
  explicit RequestExecutor(int num_threads) : thread_pool_(num_threads) {}
  void Schedule(std::function<void()> fn) override {
    thread_pool_.Schedule(fn);
  }

 private:
  ecclesia::ThreadPool thread_pool_;
};

inline std::unique_ptr<tensorflow::serving::net_http::HTTPServerInterface>
CreateServer(int port) {
  // Size of the thread pool for handling incoming http requests
  constexpr unsigned kNumWorkerThreads = 5;
  auto options =
      absl::make_unique<tensorflow::serving::net_http::ServerOptions>();
  options->AddPort(port);
  options->SetExecutor(absl::make_unique<RequestExecutor>(kNumWorkerThreads));
  auto server = CreateEvHTTPServer(std::move(options));
  return server;
}

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_DAEMONS_COMMON_MAIN_H_
