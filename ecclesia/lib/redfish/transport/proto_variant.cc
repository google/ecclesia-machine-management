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

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "google/protobuf/struct.pb.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"

namespace ecclesia {
namespace {
using ::google::protobuf::ListValue;
using ::google::protobuf::NULL_VALUE;
using ::google::protobuf::Struct;
using ::google::protobuf::Value;
using ::libredfish::RedfishIterable;
using ::libredfish::RedfishObject;
using ::libredfish::RedfishVariant;
constexpr absl::string_view kODataId = "@odata.id";
}  // namespace

ProtoVariantImpl::ProtoVariantImpl() { value_.set_null_value(NULL_VALUE); }
ProtoVariantImpl::ProtoVariantImpl(const Value &value) : value_(value) {}

std::unique_ptr<RedfishObject> ProtoVariantImpl::AsObject() const {
  if (!value_.has_struct_value()) {
    return nullptr;
  }
  return absl::make_unique<ProtoObject>(value_.struct_value());
}

std::unique_ptr<RedfishIterable> ProtoVariantImpl::AsIterable() const {
  if (!value_.has_list_value()) {
    return nullptr;
  }
  return absl::make_unique<ProtoIterable>(value_.list_value());
}

bool ProtoVariantImpl::GetValue(std::string *val) const {
  if (!value_.has_string_value()) {
    return false;
  }
  *val = value_.string_value();
  return true;
}

bool ProtoVariantImpl::GetValue(int32_t *val) const {
  if (!value_.has_number_value()) {
    return false;
  }
  *val = static_cast<int32_t>(value_.number_value());
  return true;
}

bool ProtoVariantImpl::GetValue(int64_t *val) const {
  if (!value_.has_number_value()) {
    return false;
  }
  *val = static_cast<int64_t>(value_.number_value());
  return true;
}

bool ProtoVariantImpl::GetValue(double *val) const {
  if (!value_.has_number_value()) {
    return false;
  }
  *val = value_.number_value();
  return true;
}

bool ProtoVariantImpl::GetValue(bool *val) const {
  if (!value_.has_bool_value()) {
    return false;
  }
  *val = value_.bool_value();
  return true;
}

bool ProtoVariantImpl::GetValue(absl::Time* val) const {
  std::string dt_string;
  if (!GetValue(&dt_string)) {
    return false;
  }
  return absl::ParseTime("%Y-%m-%dT%H:%M:%S%Z", dt_string, val, nullptr);
}

std::string ProtoVariantImpl::DebugString() const {
  return value_.DebugString();
}

ProtoObject::ProtoObject(Struct const &proto_struct)
    : proto_struct_(proto_struct) {}

RedfishVariant ProtoObject::operator[](const std::string &node_name) const {
  if (!proto_struct_.fields().contains(node_name)) {
    return RedfishVariant(absl::NotFoundError("no such node"));
  }
  return RedfishVariant(absl::make_unique<ProtoVariantImpl>(
      proto_struct_.fields().at(node_name)));
}

absl::optional<std::string> ProtoObject::GetUri() {
  if (!proto_struct_.fields().contains(kODataId) ||
      !proto_struct_.fields().at(kODataId).has_string_value()) {
    return absl::nullopt;
  }
  return proto_struct_.fields().at(kODataId).string_value();
}

std::string ProtoObject::DebugString() { return proto_struct_.DebugString(); }

ProtoIterable::ProtoIterable(ListValue const &list_value)
    : list_value_(list_value) {}

size_t ProtoIterable::Size() { return list_value_.values_size(); }

bool ProtoIterable::Empty() { return list_value_.values().empty(); }

RedfishVariant ProtoIterable::operator[](int index) const {
  if (index < 0 || index >= list_value_.values_size()) {
    return RedfishVariant(absl::OutOfRangeError(absl::StrCat(
        "index should be >= 0 and < ", list_value_.values_size())));
  }
  return RedfishVariant(
      absl::make_unique<ProtoVariantImpl>(list_value_.values(index)));
}

}  // namespace ecclesia
