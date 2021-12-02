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

#include "ecclesia/lib/redfish/transport/proto_variant.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::google::protobuf::ListValue;
using ::google::protobuf::Struct;
using ::google::protobuf::Value;
using ::libredfish::RedfishIterable;
using ::libredfish::RedfishObject;
using ::libredfish::RedfishVariant;
using ::testing::IsNull;
using ::testing::NotNull;

Value GetUrl(absl::string_view url_str) {
  Value url;
  url.set_string_value(std::string(url_str));
  return url;
}

Value GetRegistryMember(absl::string_view name) {
  Value member;
  Struct member_struct;
  member_struct.mutable_fields()->insert(
      {"@odata.id", GetUrl(absl::StrCat("/redfish/v1/Registries/", name))});
  member.set_allocated_struct_value(new Struct(std::move(member_struct)));
  return member;
}

Struct GetRegistries() {
  Struct registries;
  registries.mutable_fields()->insert(
      {"@odata.id", GetUrl("/redfish/v1/Registries")});
  Value members;
  ListValue member_list;
  member_list.mutable_values()->Add(GetRegistryMember("Base"));
  member_list.mutable_values()->Add(GetRegistryMember("TaskEvent"));
  member_list.mutable_values()->Add(GetRegistryMember("ResourceEvent"));
  member_list.mutable_values()->Add(GetRegistryMember("OpenBMC"));
  members.set_allocated_list_value(new ListValue(std::move(member_list)));
  registries.mutable_fields()->insert({"Members", members});
  return registries;
}

Struct GetAccountService() {
  Struct account_service;
  Value url;
  url.set_string_value("/redfish/v1/AccountService");
  account_service.mutable_fields()->insert({"@odata.id", url});
  return account_service;
}

Struct GetServiceRoot() {
  Struct service_root;
  Value url;
  url.set_string_value("/redfish/v1");
  service_root.mutable_fields()->insert({"@odata.id", url});
  Value account_service;
  account_service.set_allocated_struct_value(new Struct(GetAccountService()));
  service_root.mutable_fields()->insert({"AccountService", account_service});
  return service_root;
}

TEST(ProtoObjectTest, GetNodeByNameOk) {
  std::unique_ptr<RedfishObject> service_root =
      absl::make_unique<ProtoObject>(GetServiceRoot());
  ASSERT_TRUE(service_root->GetUri().has_value());
  EXPECT_EQ(service_root->GetUri(), "/redfish/v1");
  std::unique_ptr<RedfishObject> account_service =
      (*service_root)["AccountService"].AsObject();
  ASSERT_THAT(account_service, NotNull());
  ASSERT_TRUE(account_service->GetUri().has_value());
  EXPECT_EQ(account_service->GetUri(), "/redfish/v1/AccountService");
}

TEST(ProtoObjectTest, GetNodeByNameNull) {
  std::unique_ptr<RedfishObject> service_root =
      absl::make_unique<ProtoObject>(GetServiceRoot());
  RedfishVariant service_variant = (*service_root)["DummyService"];
  EXPECT_THAT(service_variant.status(), IsStatusNotFound());
  std::unique_ptr<RedfishObject> service_object =
      (*service_root)["DummyService"].AsObject();
  EXPECT_THAT(service_object, IsNull());
}

TEST(ProtoObjectTest, DebugStringOk) {
  std::unique_ptr<RedfishObject> service_root =
      absl::make_unique<ProtoObject>(GetServiceRoot());
  EXPECT_EQ(service_root->DebugString(), GetServiceRoot().DebugString());
}

TEST(ProtoIterableTest, SizeOk) {
  std::unique_ptr<RedfishObject> registries =
      absl::make_unique<ProtoObject>(GetRegistries());
  std::unique_ptr<RedfishIterable> members =
      (*registries)["Members"].AsIterable();
  ASSERT_THAT(members, NotNull());
  EXPECT_EQ(members->Size(),
            GetRegistries().fields().at("Members").list_value().values_size());
  EXPECT_FALSE(members->Empty());
}

TEST(ProtoIterableTest, GetNodeByIndexOk) {
  std::unique_ptr<RedfishObject> registries =
      absl::make_unique<ProtoObject>(GetRegistries());
  std::unique_ptr<RedfishIterable> members =
      (*registries)["Members"].AsIterable();
  ASSERT_THAT(members, NotNull());
  EXPECT_EQ((*members)[0].AsObject()->GetUri(), "/redfish/v1/Registries/Base");
}

TEST(ProtoIterableTest, GetNodeByIndexNull) {
  std::unique_ptr<RedfishObject> registries =
      absl::make_unique<ProtoObject>(GetRegistries());
  std::unique_ptr<RedfishIterable> members =
      (*registries)["Members"].AsIterable();
  ASSERT_THAT(members, NotNull());
  RedfishVariant member_variant = (*members)[4];
  EXPECT_THAT(member_variant.status(), IsStatusOutOfRange());
  EXPECT_THAT((*members)[4].AsObject(), IsNull());
  EXPECT_THAT((*members)[-1].AsObject(), IsNull());
}

template <typename T>
void GetAndVerifyValue(absl::string_view message, const Value& value,
                       const T& expected) {
  SCOPED_TRACE(message);
  RedfishVariant variant(absl::make_unique<ProtoVariantImpl>(value));
  T val;
  ASSERT_TRUE(variant.GetValue(&val));
  EXPECT_EQ(val, expected);
}

template <typename T>
void GetAndVerifyFailure(absl::string_view message, const Value& value,
                         const T&) {
  SCOPED_TRACE(message);
  RedfishVariant variant(absl::make_unique<ProtoVariantImpl>(value));
  T val;
  EXPECT_FALSE(variant.GetValue(&val));
}

TEST(ProtoVariantTest, GetBooleanValue) {
  Value value;
  value.set_bool_value(true);
  GetAndVerifyValue("Test boolean value", value, true);
  value.set_string_value("");
  GetAndVerifyFailure("Test boolean value", value, true);
}

TEST(ProtoVariantTest, GetNumericValue) {
  Value value;
  value.set_number_value(1);
  GetAndVerifyValue("Test double value", value, 1.0);
  GetAndVerifyValue("Test int32_t value", value, int32_t{1});
  GetAndVerifyValue("Test int64_t value", value, int64_t{1});
  value.set_string_value("");
  GetAndVerifyFailure("Test double value", value, 1.0);
  GetAndVerifyFailure("Test int32_t value", value, int32_t{1});
  GetAndVerifyFailure("Test int64_t value", value, int64_t{1});
}

TEST(ProtoVariantTest, GetStringValue) {
  Value value;
  value.set_string_value("123");
  GetAndVerifyValue("Test string value", value, std::string("123"));
  value.set_number_value(1);
  GetAndVerifyFailure("Test string value", value, std::string("123"));
}

TEST(ProtoVariantTest, GetTimeValue) {
  Value value;
  value.set_string_value("2020-12-21T12:34:56+00:00");
  absl::TimeZone utc;
  ASSERT_TRUE(absl::LoadTimeZone("UTC", &utc));

  absl::Time datetime_gold = absl::FromDateTime(2020, 12, 21, 12, 34, 56, utc);
  GetAndVerifyValue("Test string value", value, datetime_gold);
  value.set_number_value(1);
  GetAndVerifyFailure("Test string value", value, datetime_gold);
}

TEST(ProtoVariantTest, GetNullValueOk) {
  RedfishVariant variant(absl::make_unique<ProtoVariantImpl>());
  bool value;
  EXPECT_FALSE(variant.GetValue(&value));
}

TEST(ProtoVariantTest, DebugStringOk) {
  RedfishVariant variant(absl::make_unique<ProtoVariantImpl>(
      GetServiceRoot().fields().at("AccountService")));
  EXPECT_EQ(variant.DebugString(),
            GetServiceRoot().fields().at("AccountService").DebugString());
}

TEST(ProtoVariantTest, AsObjectNotStruct) {
  Value root;
  root.set_bool_value(true);
  RedfishVariant variant(absl::make_unique<ProtoVariantImpl>(root));
  EXPECT_THAT(variant.AsObject(), IsNull());
}

TEST(ProtoVariantTest, AsIterableNotListValue) {
  Value root;
  root.set_bool_value(true);
  RedfishVariant variant(absl::make_unique<ProtoVariantImpl>(root));
  EXPECT_THAT(variant.AsIterable(), IsNull());
}

}  // namespace
}  // namespace ecclesia
