/*
 * Copyright 2024 Google LLC
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

#include "ecclesia/lib/redfish/node_topology.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/hash/hash_testing.h"
#include "ecclesia/lib/redfish/location.h"
#include "ecclesia/lib/redfish/types.h"

namespace ecclesia {
namespace {

using ::testing::Eq;
using ::testing::Ne;

TEST(NodeTest, EqualityCheck) {
  const Node n1 = {
      .name = "name",
      .model = "model",
      .local_devpath = "devpath",
      .type = kBoard,
      .associated_uris = {"uri1", "uri2"},
      .supplemental_location_info =
          SupplementalLocationInfo{.service_label = "label",
                                   .part_location_context = "context"},
      .replaceable = true};

  Node n2 = n1;
  EXPECT_THAT(n1, Eq(n2));

  n2.name = "name2";
  EXPECT_THAT(n1, Ne(n2));
  n2 = n1;
  n2.model = "model2";
  EXPECT_THAT(n1, Ne(n2));
  n2 = n1;
  n2.local_devpath = "devpath2";
  EXPECT_THAT(n1, Ne(n2));
  n2 = n1;
  n2.type = kConnector;
  EXPECT_THAT(n1, Ne(n2));
  n2 = n1;
  n2.associated_uris = {"uri1", "uri3"};
  EXPECT_THAT(n1, Ne(n2));
  n2 = n1;
  n2.replaceable = false;
  EXPECT_THAT(n1, Ne(n2));
  n2 = n1;
  n2.supplemental_location_info = SupplementalLocationInfo{
      .service_label = "label2", .part_location_context = "context2"};
  EXPECT_THAT(n1, Ne(n2));
}

TEST(NodeTest, HashesCorrectly) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      Node(),
      Node{.name = "name"},
      Node{.model = "model"},
      Node{.local_devpath = "devpath"},
      Node{.type = kBoard},
      Node{.associated_uris = {"uri1", "uri2"}},
      Node{.replaceable = true},
      Node{.name = "name",
           .model = "model",
           .local_devpath = "devpath",
           .type = kConnector,
           .associated_uris = {"uri1", "uri2", "uri3"},
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "label",
                                        .part_location_context = "context"},
           .replaceable = true},
      Node{.name = "name2",
           .model = "model2",
           .local_devpath = "devpath2",
           .type = kDevice,
           .associated_uris = {"uri1"},
           .replaceable = false},
  }));
}

}  // namespace
}  // namespace ecclesia
