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

#include "ecclesia/lib/cache/rcu_snapshot.h"

#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/memory/memory.h"

namespace ecclesia {
namespace {

TEST(RcuSnapshot, CreateAndDestroyObject) {
  RcuSnapshot<int>::WithInvalidator object = RcuSnapshot<int>::Create(13);
  RcuSnapshot<int> &original = object.snapshot;
  auto copy = original;
  EXPECT_EQ(*original, 13);
  EXPECT_EQ(*copy, 13);
}

TEST(RcuSnapshot, InvalidateSnapshot) {
  RcuSnapshot<int>::WithInvalidator object = RcuSnapshot<int>::Create(31);
  RcuSnapshot<int> &original = object.snapshot;
  auto copy = original;
  EXPECT_TRUE(original.IsFresh());
  EXPECT_TRUE(copy.IsFresh());
  object.invalidator.InvalidateSnapshot();
  EXPECT_FALSE(original.IsFresh());
  EXPECT_FALSE(copy.IsFresh());
}

TEST(RcuSnapshot, SimpleNotification) {
  RcuSnapshot<int>::WithInvalidator object = RcuSnapshot<int>::Create(37);
  RcuNotification notification;
  object.snapshot.RegisterNotification(notification);
  EXPECT_FALSE(notification.HasTriggered());
  object.invalidator.InvalidateSnapshot();
  EXPECT_TRUE(notification.HasTriggered());
}

TEST(RcuSnapshot, NotificationTriggersImmediatelyOnInvalidSnapshot) {
  RcuSnapshot<int>::WithInvalidator object = RcuSnapshot<int>::Create(37);
  object.invalidator.InvalidateSnapshot();
  RcuNotification notification;
  object.snapshot.RegisterNotification(notification);
  EXPECT_TRUE(notification.HasTriggered());
}

TEST(RcuSnapshot, NotificationCanBeReset) {
  RcuSnapshot<int>::WithInvalidator object1 = RcuSnapshot<int>::Create(41);
  RcuSnapshot<int>::WithInvalidator object2 = RcuSnapshot<int>::Create(42);
  RcuNotification notification;
  object1.snapshot.RegisterNotification(notification);
  EXPECT_FALSE(notification.HasTriggered());
  object1.invalidator.InvalidateSnapshot();
  EXPECT_TRUE(notification.HasTriggered());
  notification.Reset();
  object2.snapshot.RegisterNotification(notification);
  EXPECT_FALSE(notification.HasTriggered());
  object2.invalidator.InvalidateSnapshot();
  EXPECT_TRUE(notification.HasTriggered());
}

TEST(RcuSnapshot, NotificationCanBeRegisteredWithMultipleSnapshots) {
  RcuSnapshot<int>::WithInvalidator object1 = RcuSnapshot<int>::Create(43);
  RcuSnapshot<int>::WithInvalidator object2 = RcuSnapshot<int>::Create(44);
  RcuNotification notification;
  object1.snapshot.RegisterNotification(notification);
  object2.snapshot.RegisterNotification(notification);
  EXPECT_FALSE(notification.HasTriggered());
  object2.invalidator.InvalidateSnapshot();
  EXPECT_TRUE(notification.HasTriggered());

  // Resetting the notification and then invalidating snapshot1 shouldn't do
  // anything because it should no longer be registered.
  notification.Reset();
  EXPECT_FALSE(notification.HasTriggered());
  object1.invalidator.InvalidateSnapshot();
  EXPECT_FALSE(notification.HasTriggered());
}

TEST(RcuSnapshot, NotificationCanBeUsedWithMultipleTypes) {
  RcuSnapshot<int>::WithInvalidator int_object = RcuSnapshot<int>::Create(59);
  RcuSnapshot<std::string>::WithInvalidator str_object =
      RcuSnapshot<std::string>::Create("abc");
  RcuNotification notification;
  int_object.snapshot.RegisterNotification(notification);
  str_object.snapshot.RegisterNotification(notification);
  EXPECT_FALSE(notification.HasTriggered());
  str_object.invalidator.InvalidateSnapshot();
  EXPECT_TRUE(notification.HasTriggered());
}

TEST(RcuSnapshot, DeleteNotificationBeforeSnapshot) {
  RcuSnapshot<int>::WithInvalidator object = RcuSnapshot<int>::Create(51);
  auto notification_ptr = std::make_unique<RcuNotification>();
  object.snapshot.RegisterNotification(*notification_ptr);
  notification_ptr = nullptr;
}

TEST(RcuSnapshot, DeleteSnapshotBeforeNotification) {
  RcuNotification notification;
  {
    RcuSnapshot<int>::WithInvalidator object = RcuSnapshot<int>::Create(53);
    object.snapshot.RegisterNotification(notification);
    object.invalidator.InvalidateSnapshot();
  }
}

TEST(RcuSnapshot, DeleteSnapshotWithoutInvalidation) {
  RcuNotification notification;
  {
    RcuSnapshot<int>::WithInvalidator object = RcuSnapshot<int>::Create(53);
    object.snapshot.RegisterNotification(notification);
  }
}

TEST(RcuSnapshot, DependsOnSnapshots) {
  RcuSnapshot<int>::WithInvalidator s1 = RcuSnapshot<int>::Create(101);
  RcuSnapshot<std::string>::WithInvalidator s2 =
      RcuSnapshot<std::string>::Create("102");
  RcuSnapshot<int>::WithInvalidator s3 = RcuSnapshot<int>::Create(103);
  RcuSnapshot<int>::WithInvalidator s4 = RcuSnapshot<int>::Create(104);

  std::vector<RcuSnapshot<int>> s3_and_4 = {s3.snapshot, s4.snapshot};

  RcuSnapshot<int> dep = RcuSnapshot<int>::CreateDependent(
      RcuSnapshotDependsOn(s1.snapshot, s2.snapshot, s3_and_4), 410);
  RcuNotification notification;
  dep.RegisterNotification(notification);

  EXPECT_TRUE(dep.IsFresh());
  EXPECT_FALSE(notification.HasTriggered());

  s3.invalidator.InvalidateSnapshot();
  EXPECT_FALSE(dep.IsFresh());
  EXPECT_TRUE(notification.HasTriggered());
}

TEST(RcuSnapshot, DependsOnStaleSnapshot) {
  RcuSnapshot<int>::WithInvalidator s1 = RcuSnapshot<int>::Create(101);
  RcuSnapshot<std::string> s2 = RcuSnapshot<std::string>::CreateStale("102");

  RcuSnapshot<int> dep = RcuSnapshot<int>::CreateDependent(
      RcuSnapshotDependsOn(s1.snapshot, s2), 203);
  RcuNotification notification;
  dep.RegisterNotification(notification);

  EXPECT_FALSE(dep.IsFresh());
  EXPECT_TRUE(notification.HasTriggered());
}

TEST(RcuSnapshot, DependsOnSnapshotsWithMultipleNotifications) {
  RcuSnapshot<int>::WithInvalidator s1 = RcuSnapshot<int>::Create(123);
  RcuSnapshot<std::string>::WithInvalidator s2 =
      RcuSnapshot<std::string>::Create("456");

  RcuSnapshot<int> dep = RcuSnapshot<int>::CreateDependent(
      RcuSnapshotDependsOn(s1.snapshot, s2.snapshot), 579);
  RcuNotification notification;
  dep.RegisterNotification(notification);

  EXPECT_TRUE(dep.IsFresh());
  EXPECT_FALSE(notification.HasTriggered());

  s1.invalidator.InvalidateSnapshot();
  EXPECT_FALSE(dep.IsFresh());
  EXPECT_TRUE(notification.HasTriggered());

  s2.invalidator.InvalidateSnapshot();
  EXPECT_FALSE(dep.IsFresh());
  EXPECT_TRUE(notification.HasTriggered());
}

}  // namespace
}  // namespace ecclesia
