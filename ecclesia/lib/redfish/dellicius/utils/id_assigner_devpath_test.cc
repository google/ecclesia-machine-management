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

#include "ecclesia/lib/redfish/dellicius/utils/id_assigner_devpath.h"

#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

TEST(IdDevpath, CanMap) {
  absl::flat_hash_map<std::string, std::string> map;
  map["/fru1"] = "/global/fru1";
  map["/fru2"] = "/global/fru2";
  auto assigner = NewMapBasedDevpathAssigner(std::move(map));

  EXPECT_THAT(assigner->IdForLocalDevpathInDataSet(ParseTextProtoOrDie(R"pb(
    devpath: "/fru1"
    properties { name: "Name" string_value: "Fru1" }
    properties { name: "Location.ServiceLabel" string_value: "fru1" }
  )pb")),
              IsOkAndHolds("/global/fru1"));
  EXPECT_THAT(assigner->IdForLocalDevpathInDataSet(ParseTextProtoOrDie(R"pb(
    devpath: "/fru2"
    properties { name: "Name" string_value: "Fru2" }
    properties { name: "Location.ServiceLabel" string_value: "fru2" }
  )pb")),
              IsOkAndHolds("/global/fru2"));
}

TEST(IdDevpath, CannotMapWithoutDevpathInDataSet) {
  absl::flat_hash_map<std::string, std::string> map;
  map["/fru1"] = "/global/fru1";
  auto assigner = NewMapBasedDevpathAssigner(std::move(map));

  EXPECT_THAT(assigner->IdForLocalDevpathInDataSet(ParseTextProtoOrDie(R"pb(
    properties { name: "Name" string_value: "Fru1" }
    properties { name: "Location.ServiceLabel" string_value: "fru1" }
  )pb")),
              IsStatusNotFound());
}

TEST(IdDevpath, CannotMapWithoutMatchingEntry) {
  absl::flat_hash_map<std::string, std::string> map;
  map["/fru1"] = "/global/fru1";
  auto assigner = NewMapBasedDevpathAssigner(std::move(map));

  EXPECT_THAT(assigner->IdForLocalDevpathInDataSet(ParseTextProtoOrDie(R"pb(
    devpath: "/fru2"
    properties { name: "Name" string_value: "Fru2" }
    properties { name: "Location.ServiceLabel" string_value: "fru2" }
  )pb")),
              IsStatusNotFound());
}

TEST(IdDevpath, CanMapDevpathInQueryResult) {
  absl::flat_hash_map<std::string, std::string> map;
  map["/fru1"] = "/global/fru1";
  map["/fru2"] = "/global/fru2";
  auto assigner = NewMapBasedDevpathAssigner(std::move(map));

  EXPECT_THAT(assigner->IdForLocalDevpathInQueryResult(ParseTextProtoOrDie(R"pb(
    fields {
      key: "Name"
      value { string_value: "fru1" }
    }
    fields {
      key: "_id_"
      value { identifier { local_devpath: "/fru1" } }
    }
  )pb")),
              IsOkAndHolds("/global/fru1"));
  EXPECT_THAT(assigner->IdForLocalDevpathInQueryResult(ParseTextProtoOrDie(R"pb(
    fields {
      key: "Name"
      value { string_value: "fru2" }
    }
    fields {
      key: "_id_"
      value { identifier { local_devpath: "/fru2" } }
    }
  )pb")),
              IsOkAndHolds("/global/fru2"));
}

TEST(IdDevpath, CannotMapWithoutDevpathInQueryResult) {
  absl::flat_hash_map<std::string, std::string> map;
  map["/fru1"] = "/global/fru1";
  auto assigner = NewMapBasedDevpathAssigner(std::move(map));

  EXPECT_THAT(assigner->IdForLocalDevpathInQueryResult(ParseTextProtoOrDie(R"pb(
    fields {
      key: "Name"
      value { string_value: "fru1" }
    }
  )pb")),
              IsStatusNotFound());
}

TEST(IdDevpath, CannotMapDevpathWithoutMatchingEntryInMap) {
  absl::flat_hash_map<std::string, std::string> map;
  map["/fru1"] = "/global/fru1";
  auto assigner = NewMapBasedDevpathAssigner(std::move(map));

  EXPECT_THAT(assigner->IdForLocalDevpathInQueryResult(ParseTextProtoOrDie(R"pb(
    fields {
      key: "Name"
      value { string_value: "fru2" }
    }
    fields {
      key: "__id__"
      value { identifier { local_devpath: "/node0/fru2" } }
    }
  )pb")),
              IsStatusNotFound());
}

TEST(IdDevpath, CannotHandleRedfishLocation) {
  absl::flat_hash_map<std::string, std::string> map;
  map["/fru1"] = "/global/fru1";
  auto assigner = NewMapBasedDevpathAssigner(std::move(map));

  EXPECT_THAT(assigner->IdForRedfishLocationInQueryResult(
                  ParseTextProtoOrDie(R"pb(
                    fields {
                      key: "Name"
                      value { string_value: "fru2" }
                    }
                    fields {
                      key: "_id_"
                      value { identifier { local_devpath: "/fru1" } }
                    }
                  )pb"),
                  false),
              IsStatusUnimplemented());
}

}  // namespace
}  // namespace ecclesia
