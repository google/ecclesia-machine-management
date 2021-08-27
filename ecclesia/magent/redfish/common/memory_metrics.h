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

#ifndef ECCLESIA_MAGENT_REDFISH_COMMON_MEMORY_METRICS_H_
#define ECCLESIA_MAGENT_REDFISH_COMMON_MEMORY_METRICS_H_

#include <functional>
#include <memory>
#include <utility>

#include "ecclesia/lib/mcedecoder/cpu_topology.h"
#include "ecclesia/magent/lib/event_logger/system_event_visitors.h"
#include "ecclesia/magent/redfish/core/index_resource.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

using DimmErrorCountingVisitorFactory =
    std::function<std::unique_ptr<DimmErrorCountingVisitor>(
        absl::Time, std::unique_ptr<CpuTopologyInterface>)>;

class MemoryMetrics : public IndexResource<int> {
 public:
  explicit MemoryMetrics(SystemModel *system_model,
                         DimmErrorCountingVisitorFactory visitor_factory)
      : IndexResource(kMemoryMetricsUriPattern),
        system_model_(system_model),
        visitor_factory_(std::move(visitor_factory)) {}

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &params) override;

  void AddStaticFields(nlohmann::json *json) {
    (*json)[kOdataType] = "#MemoryMetrics.v1_2_0.MemoryMetrics";
    (*json)[kOdataContext] =
        "/redfish/v1/$metadata#MemoryMetrics.MemoryMetrics";
    (*json)[kName] = "Memory Metrics";
    (*json)[kId] = "Metrics";
  }

  SystemModel *const system_model_;
  DimmErrorCountingVisitorFactory visitor_factory_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_COMMON_MEMORY_METRICS_H_
