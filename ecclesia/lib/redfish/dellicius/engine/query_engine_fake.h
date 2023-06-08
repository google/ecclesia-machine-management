/*
 * Copyright 2023 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_QUERY_ENGINE_FAKE_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_QUERY_ENGINE_FAKE_H_

#include <memory>
#include <utility>
#include <variant>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/dellicius/engine/config.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/time/clock_fake.h"

namespace ecclesia {
// Class that encapsualtes all the boilerplate needed for creating and using
// an ecclesia::QueryEngine for unit tests. Can be initialized with simple
// caching or no caching.
//
// Example usage for issuing one query, no need to save any intermediate locals:
// std::vector<DelliciusQueryResult> response_entries =
//       FakeQueryEngineEnvironment(
//           {.flags{.enable_devpath_extension = true},
//           .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}},
//           "indus_hmb_shim/mockup.shar")
//        .GetEngine().ExecuteQuery(....)
//
// Example usage with caching and metrics:
//   RedfishMetrics metrics;
//   FakeQueryEngineEnvironment fake_engine_env(
//           {.flags{.enable_devpath_extension = true,
//                   .enable_transport_metrics = true},
//           .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}},
//           "indus_hmb_shim/mockup.shar", kNoExpiration)
//   std::unique_ptr<QueryEngine> query_engine = fake_engine_env.GetEngine();
//   query_engine.ExecuteQueryWithMetrics(
//             {"SensorCollection"},
//             &transport_metrics);
class FakeQueryEngineEnvironment {
 public:
  enum class CachingMode { kNoExpiration, kDisableCaching};

  explicit FakeQueryEngineEnvironment(
      const QueryEngineConfiguration& config, absl::string_view mockup_name,
      absl::Time clock_time,
      const std::variant<CachingMode, RedfishTransportCacheFactory>&
          caching_mode = CachingMode::kDisableCaching)
      : config_(config), server_(mockup_name), clock_(clock_time) {
    CHECK_OK(
        InitializeQueryEngine(server_.RedfishClientTransport(), caching_mode));
  }
  // Disable copy and move constructors.
  FakeQueryEngineEnvironment(const FakeQueryEngineEnvironment&) = delete;
  FakeQueryEngineEnvironment& operator=(const FakeQueryEngineEnvironment&) =
      delete;
  QueryEngine& GetEngine() { return *query_engine_; }

 protected:
  absl::Status InitializeQueryEngine(
      std::unique_ptr<RedfishTransport> transport,
      std::variant<CachingMode, RedfishTransportCacheFactory> caching_mode) {
    // When a CachingMode is specified as the cache policy, create the
    // corresponding cache factory and pass it to query engine construction.
    if (std::holds_alternative<CachingMode>(caching_mode)) {
      CachingMode mode = std::get<CachingMode>(caching_mode);
      if (mode == CachingMode::kNoExpiration) {
        query_engine_ = std::make_unique<QueryEngine>(
            config_, std::move(transport),
            [this](RedfishTransport* transport) {
              return std::make_unique<TimeBasedCache>(transport, &clock_,
                                                      absl::InfiniteDuration());
            },
            &clock_);
      } else if (mode == CachingMode::kDisableCaching) {
        query_engine_ = std::make_unique<QueryEngine>(
            config_, std::move(transport), QueryEngine::CreateNullCache,
            &clock_);
      }
      return absl::OkStatus();
    }
    // Construct the query engine with a user supplied custom cache factory.
    if (std::holds_alternative<RedfishTransportCacheFactory>(caching_mode)) {
      RedfishTransportCacheFactory factory =
          std::get<RedfishTransportCacheFactory>(caching_mode);
      if (factory != nullptr) {
        query_engine_ = std::make_unique<QueryEngine>(
            config_, std::move(transport), factory, &clock_);
      }
      return absl::OkStatus();
    }
    return absl::InternalError(
        "Tried to construct FakeQueryEngineEnvironment without specifying a "
        "caching policy. Use a QueryEngine::CachingMode member or supply a "
        "cache "
        "factory");
  }

 private:
  QueryEngineConfiguration config_;
  FakeRedfishServer server_;
  FakeClock clock_;
  std::unique_ptr<QueryEngine> query_engine_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_QUERY_ENGINE_FAKE_H_
