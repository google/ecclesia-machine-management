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

#include "ecclesia/lib/redfish/event/server/redfish_id.hpp"

#include <cstdint>
#include "gtest/gtest.h"
#include "absl/synchronization/mutex.h"

namespace ecclesia {

namespace {

TEST(RedfishIdTest, UniqueIdRandomStart) {
    RedfishId<> event_id;
    EXPECT_NE(event_id.NextId(), event_id.NextId());
}

TEST(RedfishIdTest, UniqueIdUserStart) {
  RedfishId<> event_id(1);
  EXPECT_EQ(2, event_id.NextId());
  EXPECT_EQ(3, event_id.NextId());
  EXPECT_NE(event_id.NextId(), event_id.NextId());
}

TEST(RedfishIdTest, MonotonicallyIncreasingIds) {
  RedfishId<> event_id;
  int64_t last_id = event_id.NextId();
  for (int i = 0; i < 100; ++i) {
    int64_t next_id = event_id.NextId();
    EXPECT_GT(next_id, last_id);
    last_id = next_id;
  }
}

TEST(RedfishIdTest, MonotonicallyIncreasingEventIdsWithMutex) {
    RedfishId<absl::Mutex> event_id;
    int64_t last_id = event_id.NextId();
    for (int i = 0; i < 100; ++i) {
      int64_t next_id = event_id.NextId();
      EXPECT_GT(next_id, last_id);
      last_id = next_id;
    }
}

}  // namespace

}  // namespace ecclesia
