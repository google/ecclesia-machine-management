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

#include "ecclesia/lib/ipmi/ipmi_manager.h"

#include <cstdlib>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/functional/bind_front.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "ecclesia/lib/ipmi/ipmi_handle.h"
#include "ecclesia/lib/ipmi/ipmi_interface_params.pb.h"
#include "ecclesia/lib/ipmi/ipmitool_mock.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/lib/time/clock_fake.h"

extern "C" {
#include "include/ipmitool/ipmi_intf.h"
}

namespace ecclesia {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArg;

// Constants to use for IPMI timeouts. The first "shouldn't trigger" timeout is
// for events that should complete normally, and is a more normal longer time.
// The second "should trigger" is for events that we expect to time out and is
// very short to keep the runtime of the tests more reasonable.
constexpr absl::Duration kTimeoutThatShouldntTrigger = absl::Seconds(10);
constexpr absl::Duration kTimeoutThatShouldTrigger = absl::Milliseconds(1);

// Lanplus close needs to free the hostname, this helper method emulates a
// portion of that logic so that our test can run without leaking this. This has
// to be injected in some cases where close would be called as the manager does
// create some lanplus interfaces without ever exposing them to the user code,
// such as when an lanplus interface is created but cannot connect.
void CloseLanplus(struct ipmi_intf *intf) {
  if (intf->ssn_params.hostname != nullptr) {
    free(intf->ssn_params.hostname);
    intf->ssn_params.hostname = nullptr;
  }
}

class MockLowLevelInterface {
 public:
  explicit MockLowLevelInterface() {}
  MOCK_METHOD(int, open, (struct ipmi_intf *));
  MOCK_METHOD(void, close, (struct ipmi_intf *));
  MOCK_METHOD(int, keepalive, ());
};

int MockOpen(struct ipmi_intf *intf) {
  return reinterpret_cast<MockLowLevelInterface *>(intf->context)->open(intf);
}

int MockKeepAlive(struct ipmi_intf *intf) {
  return reinterpret_cast<MockLowLevelInterface *>(intf->context)->keepalive();
}

void MockClose(struct ipmi_intf *intf) {
  reinterpret_cast<MockLowLevelInterface *>(intf->context)->close(intf);
}

class IpmiManagerTest : public ::testing::Test {
 public:
  IpmiManagerTest() {
    mock_low_level_interface_.context = &mock_interface_;
    mock_low_level_interface_.keepalive = MockKeepAlive;
    mock_low_level_interface_.open = MockOpen;
    mock_low_level_interface_.close = MockClose;

    // Set the keepalive thread to run on a very short interval with very fast
    // timeouts. You wouldn't want to do this on a real system since this would
    // burn a lot of CPU and do way more keepalives than necessary but in these
    // short test runs we just want maximum responsiveness.
    options_.lanplus_keep_aliver_interval = absl::Milliseconds(1);
    options_.keepalive_arbiter_timeout = absl::Milliseconds(1);

    // Because the keepalive might get called a bunch of times due to the fast
    // cycle and the fact that we can't control exactly how long the tests will
    // run for, just expect keepalive to be called some number of times.
    EXPECT_CALL(mock_interface_, keepalive()).WillRepeatedly(Return(0));

    // Use the normal default factories.
    options_.open_factory =
        absl::bind_front(&IpmiManagerTest::OpenPrototype, this);
    options_.lanplus_factory =
        absl::bind_front(&IpmiManagerTest::LanplusPrototype, this);

    // Everything in these tests are set to run "fast" and so it should be okay
    // to use the real clock (no long sleeping or waiting).
    options_.clock = Clock::RealClock();

    manager_ = std::make_unique<IpmiManagerImpl>(options_);
  }

  ipmi_intf *OpenPrototype() { return &mock_low_level_interface_; }

  std::unique_ptr<ipmi_intf> LanplusPrototype() {
    auto intf = std::make_unique<ipmi_intf>();
    intf->context = &mock_interface_;
    intf->keepalive = MockKeepAlive;
    intf->open = MockOpen;
    intf->close = MockClose;
    return intf;
  }

  IpmiManager::Options options_;

  struct ipmi_intf mock_low_level_interface_;

  testing::StrictMock<MockLowLevelInterface> mock_interface_;

  std::unique_ptr<IpmiManager> manager_;
};

TEST_F(IpmiManagerTest, ParamFactoriesMatchExpectedOutput) {
  IpmiInterfaceParams expected_open_params;
  expected_open_params.mutable_open();

  EXPECT_THAT(IpmiManager::OpenParams(), EqualsProto(expected_open_params));

  IpmiInterfaceParams expected_network_params;
  expected_network_params.mutable_network()->set_hostname("host");
  expected_network_params.mutable_network()->set_port(1701);
  expected_network_params.mutable_network()->set_username("user");
  expected_network_params.mutable_network()->set_password("pass");

  EXPECT_THAT(IpmiManager::NetworkParams("host", 1701, "user", "pass"),
              EqualsProto(expected_network_params));
}

TEST_F(IpmiManagerTest, AcquireOpen) {
  absl::StatusOr<std::unique_ptr<IpmiHandle>> maybe_handle =
      manager_->Acquire(IpmiManager::OpenParams(), kTimeoutThatShouldntTrigger);
  ASSERT_THAT(maybe_handle, IsOk());
  EXPECT_EQ(maybe_handle.value()->raw_intf(), &mock_low_level_interface_);
}

TEST_F(IpmiManagerTest, AcquireValidLanplus) {
  auto params = IpmiManager::NetworkParams("localhost", 623, "root", "pwd");

  EXPECT_CALL(mock_interface_, open).WillOnce(Return(0));

  EXPECT_IPMITOOL_CALL(ipmi_cleanup, _).Times(1);
  EXPECT_CALL(mock_interface_, close)
      .WillOnce(WithArg<0>(Invoke(CloseLanplus)));

  absl::StatusOr<std::unique_ptr<IpmiHandle>> maybe_handle =
      manager_->Acquire(params, kTimeoutThatShouldntTrigger);
  ASSERT_THAT(maybe_handle, IsOk());
  auto handle = std::move(maybe_handle.value());

  EXPECT_STREQ(handle->raw_intf()->ssn_params.hostname, "localhost");
  EXPECT_EQ(handle->raw_intf()->ssn_params.port, 623u);

  // Cannot close a connection when you have a handle
  auto result = manager_->Close(params, kTimeoutThatShouldTrigger);
  EXPECT_THAT(result, IsStatusDeadlineExceeded());

  // Must call close on the connection to stop the keepaliver
  handle.reset();

  EXPECT_THAT(manager_->Close(params, kTimeoutThatShouldntTrigger), IsOk());
}

TEST_F(IpmiManagerTest, AcquireInvalidLanplus) {
  auto params = IpmiManager::NetworkParams("localhost", 623, "root", "pwd");

  EXPECT_CALL(mock_interface_, open).WillOnce(Return(-1));

  // Expect manager to close the lanplus intf during acquire
  EXPECT_IPMITOOL_CALL(ipmi_cleanup, _).Times(1);

  EXPECT_CALL(mock_interface_, close)
      .WillOnce(WithArg<0>(Invoke(CloseLanplus)));

  // Expected to get an error when requesting a unopenable lanplus intf
  auto result = manager_->Acquire(params, absl::InfiniteDuration());
  EXPECT_THAT(result, IsStatusInternal());
}

TEST_F(IpmiManagerTest, CloseOpen) {
  EXPECT_TRUE(
      manager_->Acquire(IpmiManager::OpenParams(), kTimeoutThatShouldntTrigger)
          .ok());

  // Expect manager to close open intf
  EXPECT_IPMITOOL_CALL(ipmi_cleanup, _).Times(1);

  EXPECT_CALL(mock_interface_, close).Times(1);

  EXPECT_THAT(
      manager_->Close(IpmiManager::OpenParams(), kTimeoutThatShouldntTrigger),
      IsOk());
}

TEST_F(IpmiManagerTest, MakeCloser) {
  std::shared_ptr<IpmiManager::InterfaceCloser> closer =
      manager_->MakeCloser(IpmiManager::OpenParams());

  EXPECT_IPMITOOL_CALL(ipmi_cleanup, _).Times(1);
  EXPECT_CALL(mock_interface_, close).Times(1);

  closer.reset();
}

TEST_F(IpmiManagerTest, LanPlusAcquireTwiceAndClose) {
  auto params = IpmiManager::NetworkParams("localhost", 623, "root", "pwd");

  EXPECT_CALL(mock_interface_, open(_)).WillOnce(Return(0));

  // Acquire creates the lanplus interface
  absl::StatusOr<std::unique_ptr<IpmiHandle>> maybe_handle =
      manager_->Acquire(params, kTimeoutThatShouldntTrigger);
  ASSERT_THAT(maybe_handle, IsOk());
  auto handle = std::move(maybe_handle.value());

  ipmi_intf *raw_intf1 = handle->raw_intf();

  // Discard the handle, and see if the same connection persists
  handle.reset();

  // Reacquire the lanplus interface
  absl::StatusOr<std::unique_ptr<IpmiHandle>> maybe_handle2 =
      manager_->Acquire(params, kTimeoutThatShouldntTrigger);
  ASSERT_THAT(maybe_handle2, IsOk());
  auto handle2 = std::move(maybe_handle2.value());

  ipmi_intf *raw_intf2 = handle2->raw_intf();

  // Verify that the same interface was returned
  EXPECT_EQ(raw_intf1, raw_intf2);

  // Now explicitly close the lanplus interface
  handle2.reset();

  EXPECT_IPMITOOL_CALL(ipmi_cleanup, _).Times(1);

  EXPECT_CALL(mock_interface_, close)
      .WillOnce(WithArg<0>(Invoke(CloseLanplus)));

  EXPECT_THAT(manager_->Close(params, kTimeoutThatShouldntTrigger), IsOk());
}

TEST_F(IpmiManagerTest, MultipleAcquires) {
  absl::StatusOr<std::unique_ptr<IpmiHandle>> maybe_handle =
      manager_->Acquire(IpmiManager::OpenParams(), kTimeoutThatShouldntTrigger);
  ASSERT_THAT(maybe_handle, IsOk());
  auto handle = std::move(maybe_handle.value());

  // Attempt to acquire the same resource while still holding the handle
  auto result =
      manager_->Acquire(IpmiManager::OpenParams(), kTimeoutThatShouldTrigger);
  EXPECT_EQ(result.status().code(), absl::StatusCode::kDeadlineExceeded);

  // Let go of the first handle
  handle.reset();

  // Get the handle now that it isn't being held
  EXPECT_THAT(
      manager_->Acquire(IpmiManager::OpenParams(), kTimeoutThatShouldntTrigger),
      IsOk());
}

// Helper clock implemntation that will expect sleep to be called at a fixed
// interval and which will trigger a notification after three sleeps.
class ClockWithKeepaliveCount : public FakeClock {
 public:
  ClockWithKeepaliveCount() : keepalive_count_(0) {}

  // Returns the notification that will be signalled when this is triggered.
  absl::Notification &Notification() { return keepaliver_called_three_times_; }

 private:
  void SleepCallback(absl::Duration d) override {
    EXPECT_EQ(d, absl::Seconds(5));
    if (keepalive_count_ < 3) {
      keepalive_count_++;
    } else if (!keepaliver_called_three_times_.HasBeenNotified()) {
      keepaliver_called_three_times_.Notify();
    }
  }

  absl::Notification keepaliver_called_three_times_;
  int keepalive_count_;
};

class IpmiManagerKeepAliveTest : public ::testing::Test {
 public:
  IpmiManagerKeepAliveTest() {
    mock_low_level_interface_.context = &mock_interface_;
    mock_low_level_interface_.keepalive = MockKeepAlive;
    mock_low_level_interface_.open = MockOpen;
    mock_low_level_interface_.close = MockClose;

    // Default factories
    options_.open_factory =
        absl::bind_front(&IpmiManagerKeepAliveTest::OpenPrototype, this);
    options_.lanplus_factory =
        absl::bind_front(&IpmiManagerKeepAliveTest::LanplusPrototype, this);
    options_.clock = &clock_;

    manager_ = std::make_unique<IpmiManagerImpl>(options_);
  }

  ipmi_intf *OpenPrototype() { return &mock_low_level_interface_; }

  std::unique_ptr<ipmi_intf> LanplusPrototype() {
    auto intf = std::make_unique<ipmi_intf>();
    intf->context = &mock_interface_;
    intf->keepalive = MockKeepAlive;
    intf->open = MockOpen;
    intf->close = MockClose;
    return intf;
  }

  ClockWithKeepaliveCount clock_;

  IpmiManager::Options options_;
  struct ipmi_intf mock_low_level_interface_;
  testing::StrictMock<MockLowLevelInterface> mock_interface_;

  std::unique_ptr<IpmiManager> manager_;
};

TEST_F(IpmiManagerKeepAliveTest, KeepAliverIsRunning) {
  // This test ensures the keep aliver thread is running and pinging
  // at the correct interval
  EXPECT_TRUE(
      clock_.Notification().WaitForNotificationWithTimeout(absl::Seconds(30)));
}

}  // namespace
}  // namespace ecclesia
