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

#include "ecclesia/lib/cache/rcu_store.h"

#include "gtest/gtest.h"
#include "ecclesia/lib/cache/rcu_snapshot.h"

namespace ecclesia {
namespace {

TEST(RcuStore, StoreAndRead) {
  RcuStore<int> store(5);
  auto snapshot = store.Read();
  EXPECT_EQ(*snapshot, 5);
}

TEST(RcuStore, MultipleViewsStayAlive) {
  RcuStore<int> store(17);
  auto snapshot1 = store.Read();
  store.Update(43);
  auto snapshot2 = store.Read();
  store.Update(31415);
  auto snapshot3 = store.Read();
  EXPECT_EQ(*snapshot1, 17);
  EXPECT_EQ(*snapshot2, 43);
  EXPECT_EQ(*snapshot3, 31415);
}

}  // namespace
}  // namespace ecclesia
