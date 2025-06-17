/*
 * Copyright 2025 Google LLC
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

#ifndef ECCLESIA_LIB_STUBARBITER_ARBITER_H_
#define ECCLESIA_LIB_STUBARBITER_ARBITER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/log/die_if_null.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

struct StubArbiterInfo {
  using MetricsExporter = std::function<void(
      absl::string_view, const absl::Status &, absl::Duration)>;
  enum class Type : std::uint8_t { kManual = 0, kFailover, kUnknown };

  enum class PriorityLabel : std::uint8_t {
    kPrimary = 0,
    kSecondary,
    kUnknown  // kUnknown must remain last for Failover loop.
  };

  struct EndpointMetrics {
    absl::Status status;
    absl::Time start_time;
    absl::Time end_time;
  };

  struct Metrics {
    absl::flat_hash_map<PriorityLabel, EndpointMetrics> endpoint_metrics;
    absl::Status overall_status;
    absl::Time arbiter_start_time;
    absl::Time arbiter_end_time;
  };

  struct MetricsWrapper {
    MetricsWrapper(Metrics *metrics_ptr, const Clock *clock_ptr)
        : metrics(*metrics_ptr), clock(*ABSL_DIE_IF_NULL(clock_ptr)) {
      metrics.arbiter_start_time = clock.Now();
    }
    ~MetricsWrapper() { metrics.arbiter_end_time = clock.Now(); }

   private:
    Metrics &metrics;
    const Clock &clock;
  };

  struct Config {
    Type type = Type::kFailover;
    std::optional<std::vector<absl::StatusCode>> custom_failover_code;
    absl::Duration refresh = absl::Seconds(5);
    std::optional<MetricsExporter> metrics_exporter = std::nullopt;
  };
};

template <typename T>
class StubArbiter {
 public:
  static absl::StatusOr<std::unique_ptr<StubArbiter>> Create(
      const StubArbiterInfo::Config &config,
      absl::AnyInvocable<
          absl::StatusOr<std::unique_ptr<T>>(StubArbiterInfo::PriorityLabel)>
          stub_factory,
      const Clock *clock = Clock::RealClock()) {
    switch (config.type) {
      case StubArbiterInfo::Type::kManual:
      case StubArbiterInfo::Type::kFailover:
        break;
      default:
        return absl::InvalidArgumentError("Unknown StubArbiterType");
    }

    absl::flat_hash_set<absl::StatusCode> failover_codes;
    if (config.custom_failover_code.has_value()) {
      failover_codes.insert(config.custom_failover_code.value().begin(),
                            config.custom_failover_code.value().end());
    } else {
      failover_codes = absl::flat_hash_set<absl::StatusCode>{
          absl::StatusCode::kDeadlineExceeded, absl::StatusCode::kUnavailable,
          absl::StatusCode::kResourceExhausted};
    }
    return absl::WrapUnique(
        new StubArbiter(config.type, std::move(stub_factory), failover_codes,
                        config.refresh, clock, config.metrics_exporter));
  }

  // The Execute function runs the callback synchronously.
  StubArbiterInfo::Metrics Execute(
      absl::AnyInvocable<absl::Status(T *, StubArbiterInfo::PriorityLabel)>
          func,
      StubArbiterInfo::PriorityLabel initial_stub =
          StubArbiterInfo::PriorityLabel::kPrimary) {
    return policy_(std::move(func), initial_stub);
  };

 private:
  StubArbiter(
      StubArbiterInfo::Type type,
      absl::AnyInvocable<
          absl::StatusOr<std::unique_ptr<T>>(StubArbiterInfo::PriorityLabel)>
          stub_factory,
      absl::flat_hash_set<absl::StatusCode> failover_codes,
      absl::Duration refresh, const Clock *clock,
      const std::optional<StubArbiterInfo::MetricsExporter> &metrics_exporter)
      : failover_codes_(std::move(failover_codes)),
        stub_factory_(std::move(stub_factory)),
        clock_(*ABSL_DIE_IF_NULL(clock)),
        refresh_(refresh),
        metrics_exporter_(metrics_exporter) {
    switch (type) {
      case StubArbiterInfo::Type::kManual:
        policy_ = absl::bind_front(&StubArbiter<T>::Manual, this);
        break;
      case StubArbiterInfo::Type::kFailover:
        policy_ = absl::bind_front(&StubArbiter<T>::Failover, this);
        break;
      default:
        break;
    }

    active_stub_label_ = StubArbiterInfo::PriorityLabel::kUnknown;
    active_stub_ = nullptr;
    if (auto stub = stub_factory_(StubArbiterInfo::PriorityLabel::kPrimary);
        stub.ok()) {
      active_stub_ = std::move(*stub);
      active_stub_label_ = StubArbiterInfo::PriorityLabel::kPrimary;
      freshness_time_ = clock_.Now();
    }
  }

  StubArbiterInfo::Metrics Manual(
      absl::AnyInvocable<absl::Status(T *, StubArbiterInfo::PriorityLabel)>
          func,
      StubArbiterInfo::PriorityLabel label) {
    StubArbiterInfo::Metrics metrics;
    StubArbiterInfo::MetricsWrapper metrics_wrapper(&metrics, &clock_);

    absl::MutexLock lock(&stub_mutex_);
    if (active_stub_label_ != label) {
      // Before attempting to create a new stub, we
      // need to clean-up the active stub.
      active_stub_ = nullptr;
      active_stub_label_ = StubArbiterInfo::PriorityLabel::kUnknown;

      absl::StatusOr<std::unique_ptr<T>> stub = stub_factory_(label);
      if (stub.ok()) {
        active_stub_ = std::move(*stub);
        active_stub_label_ = label;
        freshness_time_ = clock_.Now();
      } else {
        metrics.overall_status = stub.status();
        return metrics;
      }
    }

    StubArbiterInfo::EndpointMetrics &endpoint_metrics =
        metrics.endpoint_metrics[label];
    endpoint_metrics.start_time = clock_.Now();
    endpoint_metrics.status = func(active_stub_.get(), label);
    endpoint_metrics.end_time = clock_.Now();

    metrics.overall_status = endpoint_metrics.status;

    return metrics;
  }

  StubArbiterInfo::Metrics Failover(
      absl::AnyInvocable<absl::Status(T *, StubArbiterInfo::PriorityLabel)>
          func,
      StubArbiterInfo::PriorityLabel initial_stub) {
    std::string stub_path;
    StubArbiterInfo::Metrics metrics;
    StubArbiterInfo::MetricsWrapper metrics_wrapper(&metrics, &clock_);

    auto execute_func = [&](T *stub, StubArbiterInfo::PriorityLabel label,
                            absl::string_view stub_path) -> absl::Status {
      StubArbiterInfo::EndpointMetrics &endpoint_metrics =
          metrics.endpoint_metrics[label];
      endpoint_metrics.start_time = clock_.Now();
      absl::Status status = func(stub, label);
      endpoint_metrics.end_time = clock_.Now();
      endpoint_metrics.status = status;

      if (metrics_exporter_.has_value()) {
        (*metrics_exporter_)(
            stub_path, status,
            endpoint_metrics.end_time - metrics.arbiter_start_time);
      }

      return status;
    };

    StubArbiterInfo::PriorityLabel attempted_stub_label =
        StubArbiterInfo::PriorityLabel::kUnknown;
    absl::MutexLock lock(&stub_mutex_);
    // Check if active stub is empty and is the same as the initial stub.
    // If so, then check if the active stub is the primary or the stub is fresh
    // enough. If so, then execute the function on the active stub.

    if (active_stub_ != nullptr &&
        (clock_.Now() < (freshness_time_ + refresh_) ||
         active_stub_label_ == StubArbiterInfo::PriorityLabel::kPrimary)) {
      stub_path = active_stub_label_ == StubArbiterInfo::PriorityLabel::kPrimary
                      ? "primary"
                      : "sticky";

      metrics.overall_status =
          execute_func(active_stub_.get(), active_stub_label_, stub_path);

      if (metrics.overall_status.ok() ||
          !failover_codes_.contains(metrics.overall_status.code())) {
        return metrics;
      }

      stub_path = "";
      if (active_stub_label_ == StubArbiterInfo::PriorityLabel::kSecondary) {
        stub_path = "sticky-fallback";
      }
      // Record the attempted stub label before we clean up the active stub.
      attempted_stub_label = active_stub_label_;
    }

    // Before attempting to create a new stub, we
    // need to clean-up the active stub.
    active_stub_ = nullptr;
    active_stub_label_ = StubArbiterInfo::PriorityLabel::kUnknown;
    for (StubArbiterInfo::PriorityLabel label = initial_stub;
         label != StubArbiterInfo::PriorityLabel::kUnknown;
         label = static_cast<StubArbiterInfo::PriorityLabel>(
             static_cast<int>(label) + 1)) {
      // Want to skip the attempted stub label.
      if (label == attempted_stub_label) {
        continue;
      }

      absl::StatusOr<std::unique_ptr<T>> stub = stub_factory_(label);

      if (!stub.ok()) {
        metrics.endpoint_metrics[label].status = stub.status();
        metrics.overall_status = stub.status();
        if (failover_codes_.contains(stub.status().code())) {
          continue;
        }
        break;
      }

      // If the stub path is empty, then sticky behavior was skipped..
      if (stub_path.empty()) {
        stub_path = label == StubArbiterInfo::PriorityLabel::kPrimary
                        ? "primary"
                        : "fallback";
      }
      metrics.overall_status = execute_func(stub->get(), label, stub_path);
      // If the overall status is ok or the status code is not in the failover
      // codes, then we can break out of the loop.
      if (metrics.overall_status.ok() ||
          !failover_codes_.contains(metrics.overall_status.code())) {
        active_stub_ = std::move(*stub);
        active_stub_label_ = label;
        freshness_time_ = clock_.Now();
        break;
      }
      stub_path = "";
    }
    return metrics;
  }

  absl::AnyInvocable<StubArbiterInfo::Metrics(
      absl::AnyInvocable<absl::Status(T *, StubArbiterInfo::PriorityLabel)>,
      StubArbiterInfo::PriorityLabel)>
      policy_;
  absl::flat_hash_set<absl::StatusCode> failover_codes_;
  absl::AnyInvocable<absl::StatusOr<std::unique_ptr<T>>(
      StubArbiterInfo::PriorityLabel)>
      stub_factory_;
  const Clock &clock_;
  absl::Mutex stub_mutex_;
  std::unique_ptr<T> active_stub_ ABSL_GUARDED_BY(stub_mutex_);
  StubArbiterInfo::PriorityLabel active_stub_label_
      ABSL_GUARDED_BY(stub_mutex_);
  absl::Time freshness_time_ ABSL_GUARDED_BY(stub_mutex_);
  absl::Duration refresh_;
  std::optional<StubArbiterInfo::MetricsExporter> metrics_exporter_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_STUBARBITER_ARBITER_H_
