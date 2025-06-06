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

#include "ecclesia/lib/stubarbiter/arbiter.h"

#include <memory>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "ecclesia/lib/status/test_macros.h"
#include "ecclesia/lib/testing/status.h"
#include "ecclesia/lib/thread/thread.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/lib/time/clock_fake.h"

namespace ecclesia {
namespace {

constexpr absl::string_view kPrimary = "primary";
constexpr absl::string_view kSecondary = "secondary";

using ::testing::Field;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class MockStub {
 public:
  explicit MockStub(absl::string_view name) : name_(name) {}

  std::string GetName() { return name_; }

 private:
  std::string name_;
};

using MockStubArbiter = StubArbiter<MockStub>;

std::string GetName(StubArbiterInfo::PriorityLabel label) {
  switch (label) {
    case StubArbiterInfo::PriorityLabel::kPrimary:
      return std::string(kPrimary);
    case StubArbiterInfo::PriorityLabel::kSecondary:
      return std::string(kSecondary);
    default:
      return "";
  }
}

absl::StatusOr<std::unique_ptr<StubArbiter<MockStub>>> CreateMockStubArbiter(
    const StubArbiterInfo::Config &config = {},
    const Clock *clock = Clock::RealClock()) {
  return MockStubArbiter::Create(
      config,
      [](StubArbiterInfo::PriorityLabel label)
          -> absl::StatusOr<std::unique_ptr<MockStub>> {
        return std::make_unique<MockStub>(GetName(label));
      },
      clock);
}

TEST(StubArbiterTest, CreateDefaultArbiter) {
  EXPECT_THAT(CreateMockStubArbiter(), IsOk());
}

TEST(StubArbiterTest, CreateArbiter) {
  EXPECT_THAT(CreateMockStubArbiter({
                  .type = StubArbiterInfo::Type::kManual,
              }),
              IsOk());
}

TEST(StubArbiterTest, ExecuteManual) {
  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<MockStubArbiter> arbiter,
                          CreateMockStubArbiter({
                              .type = StubArbiterInfo::Type::kManual,
                          }));

  std::string name;
  arbiter->Execute(
      [&name](MockStub *stub,
              StubArbiterInfo::PriorityLabel label) -> absl::Status {
        name = stub->GetName();
        return absl::OkStatus();
      });

  EXPECT_EQ(name, kPrimary);
  arbiter->Execute(
      [&name](MockStub *stub,
              StubArbiterInfo::PriorityLabel label) -> absl::Status {
        name = stub->GetName();
        return absl::OkStatus();
      },
      StubArbiterInfo::PriorityLabel::kSecondary);

  EXPECT_EQ(name, kSecondary);
}

TEST(StubArbiterTest, ExecuteFailover) {
  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<MockStubArbiter> arbiter,
                          CreateMockStubArbiter());

  std::string name;
  StubArbiterInfo::Metrics metrics_primary = arbiter->Execute(
      [&name](MockStub *stub,
              StubArbiterInfo::PriorityLabel label) -> absl::Status {
        name = stub->GetName();
        return absl::OkStatus();
      });
  EXPECT_EQ(name, kPrimary);
  EXPECT_THAT(metrics_primary.overall_status, IsOk());
  EXPECT_THAT(metrics_primary.endpoint_metrics,
              UnorderedElementsAre(Pair(
                  StubArbiterInfo::PriorityLabel::kPrimary,
                  Field(&StubArbiterInfo::EndpointMetrics::status, IsOk()))));
  StubArbiterInfo::Metrics metrics_secondary = arbiter->Execute(
      [&name](MockStub *stub,
              StubArbiterInfo::PriorityLabel label) -> absl::Status {
        name = stub->GetName();
        return absl::OkStatus();
      },
      StubArbiterInfo::PriorityLabel::kSecondary);
  EXPECT_EQ(name, kPrimary);
  EXPECT_THAT(metrics_secondary.overall_status, IsOk());
  EXPECT_THAT(metrics_secondary.endpoint_metrics,
              UnorderedElementsAre(Pair(
                  StubArbiterInfo::PriorityLabel::kPrimary,
                  Field(&StubArbiterInfo::EndpointMetrics::status, IsOk()))));

  StubArbiterInfo::Metrics metrics_failover = arbiter->Execute(
      [&name](MockStub *stub,
              StubArbiterInfo::PriorityLabel label) -> absl::Status {
        name = stub->GetName();
        if (name == kPrimary) {
          return absl::DeadlineExceededError("Deadline exceeded");
        }
        return absl::OkStatus();
      });
  EXPECT_EQ(name, kSecondary);
  EXPECT_THAT(metrics_failover.overall_status, IsOk());
  EXPECT_THAT(
      metrics_failover.endpoint_metrics,
      UnorderedElementsAre(
          Pair(StubArbiterInfo::PriorityLabel::kPrimary,
               Field(&StubArbiterInfo::EndpointMetrics::status,
                     IsStatusDeadlineExceeded())),
          Pair(StubArbiterInfo::PriorityLabel::kSecondary,
               Field(&StubArbiterInfo::EndpointMetrics::status, IsOk()))));
}

TEST(StubArbiterTest, FailedExecuteFailover) {
  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<MockStubArbiter> arbiter,
                          CreateMockStubArbiter());

  StubArbiterInfo::Metrics metrics = arbiter->Execute(
      [](MockStub *stub, StubArbiterInfo::PriorityLabel label) -> absl::Status {
        if (stub->GetName() == kPrimary) {
          return absl::DeadlineExceededError("Deadline exceeded");
        }
        return absl::UnavailableError("Service unavailable");
      });
  EXPECT_THAT(metrics.overall_status, IsStatusUnavailable());

  EXPECT_THAT(
      metrics.endpoint_metrics,
      UnorderedElementsAre(Pair(StubArbiterInfo::PriorityLabel::kPrimary,
                                Field(&StubArbiterInfo::EndpointMetrics::status,
                                      IsStatusDeadlineExceeded())),
                           Pair(StubArbiterInfo::PriorityLabel::kSecondary,
                                Field(&StubArbiterInfo::EndpointMetrics::status,
                                      IsStatusUnavailable()))));
}

TEST(StubArbiterTest, FailedExecuteFailoverWithCustomFailoverCode) {
  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<MockStubArbiter> arbiter,
                          CreateMockStubArbiter({
                              .custom_failover_code =
                                  std::vector<absl::StatusCode>{
                                      absl::StatusCode::kUnavailable,
                                      absl::StatusCode::kResourceExhausted},
                          }));

  StubArbiterInfo::Metrics metrics_deadline_exceeded = arbiter->Execute(
      [](MockStub *stub, StubArbiterInfo::PriorityLabel label) -> absl::Status {
        if (stub->GetName() == kPrimary) {
          return absl::DeadlineExceededError("Deadline exceeded");
        }
        return absl::UnavailableError("Service unavailable");
      });
  EXPECT_THAT(metrics_deadline_exceeded.overall_status,
              IsStatusDeadlineExceeded());

  EXPECT_THAT(
      metrics_deadline_exceeded.endpoint_metrics,
      UnorderedElementsAre(Pair(StubArbiterInfo::PriorityLabel::kPrimary,
                                Field(&StubArbiterInfo::EndpointMetrics::status,
                                      IsStatusDeadlineExceeded()))));

  StubArbiterInfo::Metrics metrics_unavailable = arbiter->Execute(
      [](MockStub *stub, StubArbiterInfo::PriorityLabel label) -> absl::Status {
        return absl::UnavailableError("Service secondary unavailable");
      });
  EXPECT_THAT(metrics_unavailable.overall_status, IsStatusUnavailable());

  EXPECT_THAT(
      metrics_unavailable.endpoint_metrics,
      UnorderedElementsAre(Pair(StubArbiterInfo::PriorityLabel::kPrimary,
                                Field(&StubArbiterInfo::EndpointMetrics::status,
                                      IsStatusUnavailable())),
                           Pair(StubArbiterInfo::PriorityLabel::kSecondary,
                                Field(&StubArbiterInfo::EndpointMetrics::status,
                                      IsStatusUnavailable()))));
}

TEST(StubArbiterTest, CheckExecutionTime) {
  FakeClock clock(absl::FromUnixSeconds(1700000000));
  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<MockStubArbiter> arbiter,
                          CreateMockStubArbiter({}, &clock));

  StubArbiterInfo::Metrics metrics = arbiter->Execute(
      [&clock](MockStub *stub,
               StubArbiterInfo::PriorityLabel label) -> absl::Status {
        clock.AdvanceTime(absl::Seconds(1));
        return absl::OkStatus();
      });
  EXPECT_THAT(metrics.overall_status, IsOk());
  EXPECT_EQ(metrics.arbiter_start_time, absl::FromUnixSeconds(1700000000));
  EXPECT_EQ(metrics.arbiter_end_time, absl::FromUnixSeconds(1700000001));

  EXPECT_THAT(metrics.endpoint_metrics,
              UnorderedElementsAre(Pair(
                  StubArbiterInfo::PriorityLabel::kPrimary,
                  AllOf(Field(&StubArbiterInfo::EndpointMetrics::start_time,
                              absl::FromUnixSeconds(1700000000)),
                        Field(&StubArbiterInfo::EndpointMetrics::end_time,
                              absl::FromUnixSeconds(1700000001))))));
}

TEST(StubArbiterTest, CheckExecutionTimeFailover) {
  FakeClock clock(absl::FromUnixSeconds(1700000000));
  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<MockStubArbiter> arbiter,
                          CreateMockStubArbiter({}, &clock));

  StubArbiterInfo::Metrics metrics = arbiter->Execute(
      [&clock](MockStub *stub,
               StubArbiterInfo::PriorityLabel label) -> absl::Status {
        clock.AdvanceTime(absl::Seconds(1));
        if (stub->GetName() == kPrimary) {
          return absl::DeadlineExceededError("Deadline exceeded");
        }
        return absl::OkStatus();
      });
  EXPECT_THAT(metrics.overall_status, IsOk());

  EXPECT_EQ(metrics.arbiter_start_time, absl::FromUnixSeconds(1700000000));
  EXPECT_EQ(metrics.arbiter_end_time, absl::FromUnixSeconds(1700000002));

  EXPECT_THAT(
      metrics.endpoint_metrics,
      UnorderedElementsAre(
          Pair(StubArbiterInfo::PriorityLabel::kPrimary,
               AllOf(Field(&StubArbiterInfo::EndpointMetrics::start_time,
                           absl::FromUnixSeconds(1700000000)),
                     Field(&StubArbiterInfo::EndpointMetrics::end_time,
                           absl::FromUnixSeconds(1700000001)))),
          Pair(StubArbiterInfo::PriorityLabel::kSecondary,
               AllOf(Field(&StubArbiterInfo::EndpointMetrics::start_time,
                           absl::FromUnixSeconds(1700000001)),
                     Field(&StubArbiterInfo::EndpointMetrics::end_time,
                           absl::FromUnixSeconds(1700000002))))));
}

TEST(StubArbiterTest, CheckPrimaryFreshness) {
  FakeClock clock(absl::FromUnixSeconds(1700000000));

  absl::flat_hash_map<std::string, int> counter;
  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<MockStubArbiter> arbiter,
                          MockStubArbiter::Create(
                              {},
                              [&counter](StubArbiterInfo::PriorityLabel label)
                                  -> absl::StatusOr<std::unique_ptr<MockStub>> {
                                std::string name = GetName(label);
                                counter[name]++;
                                return std::make_unique<MockStub>(name);
                              },
                              &clock));

  arbiter->Execute(
      [](MockStub *stub, StubArbiterInfo::PriorityLabel label) -> absl::Status {
        if (stub->GetName() == kPrimary) {
          return absl::OkStatus();
        }
        return absl::DeadlineExceededError("Deadline exceeded");
      });

  clock.AdvanceTime(absl::Seconds(10));
  StubArbiterInfo::Metrics metrics = arbiter->Execute(
      [](MockStub *stub, StubArbiterInfo::PriorityLabel label) -> absl::Status {
        if (stub->GetName() == kPrimary) {
          return absl::OkStatus();
        }
        return absl::DeadlineExceededError("Deadline exceeded");
      });
  ASSERT_THAT(metrics.overall_status, IsOk());
  EXPECT_EQ(metrics.arbiter_start_time, absl::FromUnixSeconds(1700000010));
  EXPECT_EQ(metrics.arbiter_end_time, absl::FromUnixSeconds(1700000010));

  EXPECT_THAT(counter, UnorderedElementsAre(Pair("primary", 1)));
}

TEST(StubArbiterTest, FailoverFailPrimaryStub) {
  FakeClock clock(absl::FromUnixSeconds(1700000000));

  absl::flat_hash_map<std::string, int> counter;
  ECCLESIA_ASSIGN_OR_FAIL(
      std::unique_ptr<MockStubArbiter> arbiter,
      MockStubArbiter::Create(
          {},
          [&counter](StubArbiterInfo::PriorityLabel label)
              -> absl::StatusOr<std::unique_ptr<MockStub>> {
            std::string name = GetName(label);
            counter[name]++;
            if (name == kPrimary) {
              return absl::DeadlineExceededError("Deadline exceeded");
            }
            return std::make_unique<MockStub>(name);
          },
          &clock));

  StubArbiterInfo::Metrics metrics = arbiter->Execute(
      [](MockStub *stub, StubArbiterInfo::PriorityLabel label) -> absl::Status {
        return absl::OkStatus();
      });

  EXPECT_THAT(counter,
              UnorderedElementsAre(Pair("primary", 2), Pair("secondary", 1)));
  EXPECT_THAT(
      metrics.endpoint_metrics,
      UnorderedElementsAre(
          Pair(StubArbiterInfo::PriorityLabel::kPrimary,
               Field(&StubArbiterInfo::EndpointMetrics::status,
                     IsStatusDeadlineExceeded())),
          Pair(StubArbiterInfo::PriorityLabel::kSecondary,
               Field(&StubArbiterInfo::EndpointMetrics::status, IsOk()))));
}

TEST(StubArbiterTest, CheckPrimaryFreshnessFailover) {
  FakeClock clock(absl::FromUnixSeconds(1700000000));

  absl::flat_hash_map<std::string, int> counter;
  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<MockStubArbiter> arbiter,
                          MockStubArbiter::Create(
                              {},
                              [&counter](StubArbiterInfo::PriorityLabel label)
                                  -> absl::StatusOr<std::unique_ptr<MockStub>> {
                                std::string name = GetName(label);
                                counter[name]++;
                                return std::make_unique<MockStub>(name);
                              },
                              &clock));

  arbiter->Execute(
      [](MockStub *stub, StubArbiterInfo::PriorityLabel label) -> absl::Status {
        if (stub->GetName() == kPrimary) {
          return absl::OkStatus();
        }
        return absl::DeadlineExceededError("Deadline exceeded");
      });

  clock.AdvanceTime(absl::Seconds(10));
  StubArbiterInfo::Metrics metrics = arbiter->Execute(
      [](MockStub *stub, StubArbiterInfo::PriorityLabel label) -> absl::Status {
        if (stub->GetName() == kPrimary) {
          return absl::DeadlineExceededError("Deadline exceeded");
        }
        return absl::OkStatus();
      });
  ASSERT_THAT(metrics.overall_status, IsOk());
  EXPECT_EQ(metrics.arbiter_start_time, absl::FromUnixSeconds(1700000010));
  EXPECT_EQ(metrics.arbiter_end_time, absl::FromUnixSeconds(1700000010));

  EXPECT_THAT(counter,
              UnorderedElementsAre(Pair("primary", 1), Pair("secondary", 1)));
}

TEST(StubArbiterTest, CheckSecondaryFreshness) {
  FakeClock clock(absl::FromUnixSeconds(1700000000));

  absl::flat_hash_map<std::string, int> counter;
  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<MockStubArbiter> arbiter,
                          MockStubArbiter::Create(
                              {.refresh = absl::Seconds(10)},
                              [&counter](StubArbiterInfo::PriorityLabel label)
                                  -> absl::StatusOr<std::unique_ptr<MockStub>> {
                                std::string name = GetName(label);
                                counter[name]++;
                                return std::make_unique<MockStub>(name);
                              },
                              &clock));

  StubArbiterInfo::Metrics metrics = arbiter->Execute(
      [&clock](MockStub *stub,
               StubArbiterInfo::PriorityLabel label) -> absl::Status {
        clock.AdvanceTime(absl::Seconds(1));
        if (stub->GetName() == kPrimary) {
          return absl::DeadlineExceededError("Deadline exceeded");
        }
        return absl::OkStatus();
      });
  ASSERT_THAT(metrics.overall_status, IsOk());
  EXPECT_EQ(metrics.arbiter_start_time, absl::FromUnixSeconds(1700000000));
  EXPECT_EQ(metrics.arbiter_end_time, absl::FromUnixSeconds(1700000002));
  EXPECT_THAT(counter,
              UnorderedElementsAre(Pair("primary", 1), Pair("secondary", 1)));

  clock.AdvanceTime(absl::Seconds(1));
  metrics = arbiter->Execute(
      [&clock](MockStub *stub,
               StubArbiterInfo::PriorityLabel label) -> absl::Status {
        clock.AdvanceTime(absl::Seconds(1));
        if (stub->GetName() == kPrimary) {
          return absl::DeadlineExceededError("Deadline exceeded");
        }
        return absl::OkStatus();
      });
  ASSERT_THAT(metrics.overall_status, IsOk());
  EXPECT_EQ(metrics.arbiter_start_time, absl::FromUnixSeconds(1700000003));
  EXPECT_EQ(metrics.arbiter_end_time, absl::FromUnixSeconds(1700000004));

  EXPECT_THAT(counter,
              UnorderedElementsAre(Pair("primary", 1), Pair("secondary", 1)));

  clock.AdvanceTime(absl::Seconds(10));
  metrics = arbiter->Execute(
      [&clock](MockStub *stub,
               StubArbiterInfo::PriorityLabel label) -> absl::Status {
        clock.AdvanceTime(absl::Seconds(1));
        if (stub->GetName() == kPrimary) {
          return absl::DeadlineExceededError("Deadline exceeded");
        }
        return absl::OkStatus();
      });
  ASSERT_THAT(metrics.overall_status, IsOk());
  EXPECT_EQ(metrics.arbiter_start_time, absl::FromUnixSeconds(1700000014));
  EXPECT_EQ(metrics.arbiter_end_time, absl::FromUnixSeconds(1700000016));

  EXPECT_THAT(counter,
              UnorderedElementsAre(Pair("primary", 2), Pair("secondary", 2)));
}

void MultiThreadedStubArbiterQuery(
    MockStubArbiter *arbiter, ThreadFactoryInterface *thread_factory,
    absl::AnyInvocable<absl::Status(StubArbiterInfo::PriorityLabel)> func,
    absl::AnyInvocable<void(StubArbiterInfo::Metrics &)> check_metrics) {
  constexpr int kNumThreads = 10;
  std::vector<std::unique_ptr<ThreadInterface>> threads(kNumThreads);
  absl::Notification notification;

  for (int i = 0; i < kNumThreads; ++i) {
    threads.push_back(thread_factory->New([&]() {
      // Wait for notification.
      notification.WaitForNotification();
      StubArbiterInfo::Metrics metrics = arbiter->Execute(
          [&func](MockStub *stub,
                  StubArbiterInfo::PriorityLabel label) -> absl::Status {
            return func(label);
          });
      check_metrics(metrics);
    }));
  }
  notification.Notify();
  for (std::unique_ptr<ThreadInterface> &thread : threads) {
    if (thread) {
      thread->Join();
    }
  }
};

// Following tests are meant to be run with TSAN.
// blaze test //ecclesia/lib/stubarbiter:arbiter_test
// --runs_per_test_detects_flakes --runs_per_test=1000 --config=tsan
TEST(StubArbiterTest, MockStubArbiterQueryPrimaryMultithreaded) {
  FakeClock clock(absl::FromUnixSeconds(1700000000));

  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<MockStubArbiter> arbiter,
                          CreateMockStubArbiter({}, &clock));

  MultiThreadedStubArbiterQuery(
      arbiter.get(), GetDefaultThreadFactory(),
      [](StubArbiterInfo::PriorityLabel label) -> absl::Status {
        return absl::OkStatus();
      },
      [](StubArbiterInfo::Metrics &metrics) -> void {
        EXPECT_THAT(metrics.overall_status, IsOk());
        EXPECT_THAT(
            metrics.endpoint_metrics,
            UnorderedElementsAre(Pair(
                StubArbiterInfo::PriorityLabel::kPrimary,
                Field(&StubArbiterInfo::EndpointMetrics::status, IsOk()))));
      });
}

TEST(StubArbiterTest, MockStubArbiterQueryFailoverMultithreaded) {
  FakeClock clock(absl::FromUnixSeconds(1700000000));

  ECCLESIA_ASSIGN_OR_FAIL(std::unique_ptr<MockStubArbiter> arbiter,
                          CreateMockStubArbiter({}, &clock));
  ThreadFactoryInterface *thread_factory = GetDefaultThreadFactory();
  clock.AdvanceTime(absl::Seconds(10));
  MultiThreadedStubArbiterQuery(
      arbiter.get(), thread_factory,
      [](StubArbiterInfo::PriorityLabel label) -> absl::Status {
        if (label == StubArbiterInfo::PriorityLabel::kPrimary) {
          return absl::DeadlineExceededError("Deadline exceeded");
        }
        return absl::OkStatus();
      },
      [](StubArbiterInfo::Metrics &metrics) -> void {
        EXPECT_THAT(metrics.overall_status, IsOk());
        if (metrics.endpoint_metrics.contains(
                StubArbiterInfo::PriorityLabel::kPrimary)) {
          EXPECT_THAT(metrics.endpoint_metrics,
                      UnorderedElementsAre(
                          Pair(StubArbiterInfo::PriorityLabel::kPrimary,
                               Field(&StubArbiterInfo::EndpointMetrics::status,
                                     IsStatusDeadlineExceeded())),
                          Pair(StubArbiterInfo::PriorityLabel::kSecondary,
                               Field(&StubArbiterInfo::EndpointMetrics::status,
                                     IsOk()))));
        } else {
          EXPECT_THAT(
              metrics
                  .endpoint_metrics[StubArbiterInfo::PriorityLabel::kSecondary]
                  .status,
              IsOk());
        }
      });

  // Execute initial query on secondary stub.
  clock.AdvanceTime(absl::Seconds(10));
  MultiThreadedStubArbiterQuery(
      arbiter.get(), thread_factory,
      [](StubArbiterInfo::PriorityLabel label) -> absl::Status {
        return absl::OkStatus();
      },
      [](StubArbiterInfo::Metrics &metrics) -> void {
        EXPECT_THAT(metrics.overall_status, IsOk());
        EXPECT_THAT(
            metrics.endpoint_metrics,
            UnorderedElementsAre(Pair(
                StubArbiterInfo::PriorityLabel::kPrimary,
                Field(&StubArbiterInfo::EndpointMetrics::status, IsOk()))));
      });
}

}  // namespace
}  // namespace ecclesia
