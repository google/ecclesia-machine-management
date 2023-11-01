/*
 * Copyright 2022 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_TRANSPORT_METRICAL_TRANSPORT_H_
#define ECCLESIA_LIB_REDFISH_TRANSPORT_METRICAL_TRANSPORT_H_

#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

// Thread-safe metrics. The MetricalRedfishTransport uses a thread local
// variable to collect metrics. Use "GetConstMetrics" api to populate a query
// result with the collected metrics. Use "ResetMetrics" api to clear metrics
// before reusing the transport for another query.

// Decorates RedfishTransport to gather transport metrics.
class MetricalRedfishTransport : public RedfishTransport {
 public:
  explicit MetricalRedfishTransport(std::unique_ptr<RedfishTransport> base,
                                    const Clock *clock)
      : base_transport_(std::move(base)),
        clock_(clock) {
    ResetMetrics();
  }

  absl::string_view GetRootUri() override;
  absl::StatusOr<Result> Get(absl::string_view path) override;
  absl::StatusOr<Result> Post(absl::string_view path,
                              absl::string_view data) override;
  absl::StatusOr<Result> Patch(absl::string_view path,
                               absl::string_view data) override;
  absl::StatusOr<Result> Delete(absl::string_view path,
                                absl::string_view data) override;
  // Reset metrics once the metric collection is complete.
  static void ResetMetrics();
  static RedfishMetrics GetMetrics();
  static const RedfishMetrics *GetConstMetrics();

 private:
  std::unique_ptr<RedfishTransport> base_transport_;
  const Clock *clock_;
  inline static thread_local RedfishMetrics redfish_transport_metrics_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_METRICAL_TRANSPORT_H_
