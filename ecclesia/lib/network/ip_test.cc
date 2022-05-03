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

#include "ecclesia/lib/network/ip.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace ecclesia {
namespace {

using ::testing::AnyOf;
using ::testing::IsFalse;
using ::testing::IsTrue;

// Unfortunately, we have no way of knowing a priori whether or not the test
// environment actually supports IPv4 or IPv6, so we can't actually check to see
// that the functions return the "correct" result. Instead we just call them to
// make sure that the functions run without crashing or leaking resources.

TEST(IsIpv4Localhost, TryCall) {
  EXPECT_THAT(IsIpv4LocalhostAvailable(), AnyOf(IsTrue(), IsFalse()));
}

TEST(IsIpv6Localhost, TryCall) {
  EXPECT_THAT(IsIpv6LocalhostAvailable(), AnyOf(IsTrue(), IsFalse()));
}

}  // namespace
}  // namespace ecclesia
