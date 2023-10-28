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

#include "ecclesia/lib/redfish/transport/metrical_transport.h"

#include <memory>

#include "absl/base/thread_annotations.h"
#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

namespace {

// Describes a redfish request.
struct RedfishRequest {
  // Full redfish resource URI including query params.
  absl::string_view uri;
  // Redfish Method to invoke on the URI.
  absl::string_view type;
};

// Creates metrics around a single redfish request.
// Shares the RedfishMetrics object and the mutex guarding it with the
// MetricalRedfishTransport class.
class RedfishTrace final {
 public:
  RedfishTrace(RedfishRequest request, const Clock *clock,
               RedfishMetrics *redfish_metrics,
               absl::Mutex *metrics_mutex)
      : request_(request), clock_(clock), metrics_mutex_(metrics_mutex),
        redfish_metrics_(redfish_metrics) {
    start_timestamp_ = clock->Now();
  }
  ~RedfishTrace() {
    if (redfish_metrics_ == nullptr) return;
    end_timestamp_ = clock_->Now();
    double response_time_ms =
        absl::ToDoubleMilliseconds(end_timestamp_ - start_timestamp_);
    absl::MutexLock lock(metrics_mutex_);
    RedfishMetrics::RequestMetadata *metadata;
    RedfishMetrics::Metrics *uri_metrics =
        &(*redfish_metrics_->mutable_uri_to_metrics_map())[request_.uri];
    if (!has_request_failed_) {
      metadata =
          &(*uri_metrics->mutable_request_type_to_metadata())[request_.type];
    } else {
      metadata =
          &(*uri_metrics
               ->mutable_request_type_to_metadata_failures())[request_.type];
    }
    if (metadata->request_count() == 0) {
      metadata->set_max_response_time_ms(response_time_ms);
      metadata->set_min_response_time_ms(response_time_ms);
    } else if (response_time_ms > metadata->max_response_time_ms()) {
      metadata->set_max_response_time_ms(response_time_ms);
    } else if (response_time_ms < metadata->min_response_time_ms()) {
      metadata->set_min_response_time_ms(response_time_ms);
    }
    metadata->set_request_count(metadata->request_count() + 1);
  }

  // Prepares the RedfishTrace object for recording Request Metadata for
  // Transport Error
  void RecordError() { has_request_failed_ = true; }

 private:
  const RedfishRequest request_;
  const Clock *clock_;
  absl::Mutex *metrics_mutex_;
  RedfishMetrics *redfish_metrics_ ABSL_GUARDED_BY(metrics_mutex_);
  absl::Time start_timestamp_;
  absl::Time end_timestamp_;
  // Flag used to populate request metadata for transport failures.
  bool has_request_failed_ = false;
};

}  // namespace

absl::string_view MetricalRedfishTransport::GetRootUri() {
  CHECK(base_transport_ != nullptr);
  return base_transport_->GetRootUri();
}

absl::StatusOr<RedfishTransport::Result> MetricalRedfishTransport::Get(
    absl::string_view path) {
  CHECK(base_transport_ != nullptr);
  metrics_mutex_.Lock();
  RedfishMetrics *metrics = transport_metrics_;
  metrics_mutex_.Unlock();
  auto trace = RedfishTrace({path, "GET"}, clock_, metrics, &metrics_mutex_);
  auto result = base_transport_->Get(path);
  if (!result.ok()) {
    trace.RecordError();
  }
  return result;
}
absl::StatusOr<RedfishTransport::Result> MetricalRedfishTransport::Post(
    absl::string_view path, absl::string_view data) {
  CHECK(base_transport_ != nullptr);
  metrics_mutex_.Lock();
  RedfishMetrics *metrics = transport_metrics_;
  metrics_mutex_.Unlock();
  auto trace = RedfishTrace({path, "POST"}, clock_, metrics, &metrics_mutex_);
  auto result = base_transport_->Post(path, data);
  if (!result.ok()) {
    trace.RecordError();
  }
  return result;
}
absl::StatusOr<RedfishTransport::Result> MetricalRedfishTransport::Patch(
    absl::string_view path, absl::string_view data) {
  CHECK(base_transport_ != nullptr);
  metrics_mutex_.Lock();
  RedfishMetrics *metrics = transport_metrics_;
  metrics_mutex_.Unlock();
  auto trace = RedfishTrace({path, "PATCH"}, clock_, metrics, &metrics_mutex_);
  auto result = base_transport_->Patch(path, data);
  if (!result.ok()) {
    trace.RecordError();
  }
  return result;
}
absl::StatusOr<RedfishTransport::Result> MetricalRedfishTransport::Delete(
    absl::string_view path, absl::string_view data) {
  CHECK(base_transport_ != nullptr);
  metrics_mutex_.Lock();
  RedfishMetrics *metrics = transport_metrics_;
  metrics_mutex_.Unlock();
  auto trace = RedfishTrace({path, "DELETE"}, clock_, metrics, &metrics_mutex_);
  auto result = base_transport_->Delete(path, data);
  if (!result.ok()) {
    trace.RecordError();
  }
  return result;
}
}  // namespace ecclesia
