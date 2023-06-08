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

// Decorates RedfishTransport to gather transport metrics.
class MetricalRedfishTransport : public RedfishTransport {
 public:
  explicit MetricalRedfishTransport(std::unique_ptr<RedfishTransport> base,
                                    const Clock *clock,
                                    RedfishMetrics *transport_metrics)
      : base_transport_(std::move(base)),
        clock_(clock),
        transport_metrics_(transport_metrics) {}

  absl::string_view GetRootUri() override;
  absl::StatusOr<Result> Get(absl::string_view path) override;
  absl::StatusOr<Result> Post(absl::string_view path,
                              absl::string_view data) override;
  absl::StatusOr<Result> Patch(absl::string_view path,
                               absl::string_view data) override;
  absl::StatusOr<Result> Delete(absl::string_view path,
                                absl::string_view data) override;
  // Overwrite the current metrics with a new metrics proto. This is used for
  // collecting metrics over certain intervals.
  void ResetTrackingMetricsProto(RedfishMetrics *transport_metrics) {
    transport_metrics_ = transport_metrics;
  }

 private:
  std::unique_ptr<RedfishTransport> base_transport_;
  const Clock *clock_;
  RedfishMetrics *transport_metrics_ = nullptr;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_METRICAL_TRANSPORT_H_
