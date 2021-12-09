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

#ifndef ECCLESIA_LIB_REDFISH_TRANSPORT_PROTO_VARIANT_H_
#define ECCLESIA_LIB_REDFISH_TRANSPORT_PROTO_VARIANT_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "google/protobuf/struct.pb.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/redfish/interface.h"

namespace ecclesia {

// The Proto Value based RedfishVariant implementation.
class ProtoVariantImpl : public libredfish::RedfishVariant::ImplIntf {
 public:
  ProtoVariantImpl();
  explicit ProtoVariantImpl(const google::protobuf::Value &value);

  // ProtoVariantImpl is copiable and movable
  ProtoVariantImpl(const ProtoVariantImpl &) = default;
  ProtoVariantImpl &operator=(const ProtoVariantImpl &) = default;
  ProtoVariantImpl(ProtoVariantImpl &&other) = default;
  ProtoVariantImpl &operator=(ProtoVariantImpl &&other) = default;

  std::unique_ptr<libredfish::RedfishObject> AsObject() const override;
  std::unique_ptr<libredfish::RedfishIterable> AsIterable() const override;

  bool GetValue(std::string *val) const override;
  bool GetValue(int32_t *val) const override;
  bool GetValue(int64_t *val) const override;
  bool GetValue(double *val) const override;
  bool GetValue(bool *val) const override;
  bool GetValue(absl::Time *val) const override;
  std::string DebugString() const override;

 private:
  google::protobuf::Value value_;
};

// The Proto Struct based ProtoObject implementation.
class ProtoObject : public libredfish::RedfishObject {
 public:
  explicit ProtoObject(google::protobuf::Struct const &proto_struct);

  // ProtoObject is copiable and movable
  ProtoObject(const ProtoObject &) = default;
  ProtoObject &operator=(const ProtoObject &) = default;
  ProtoObject(ProtoObject &&other) = default;
  ProtoObject &operator=(ProtoObject &&other) = default;

  libredfish::RedfishVariant operator[](
      const std::string &node_name) const override;

  absl::optional<std::string> GetUri() override;

  std::string DebugString() override;

  void ForEachProperty(
      std::function<ForEachReturn(absl::string_view key,
                                  libredfish::RedfishVariant value)>) override;

 private:
  google::protobuf::Struct proto_struct_;
};

// The Proto ListValue based ProtoIterable implementation.
class ProtoIterable : public libredfish::RedfishIterable {
 public:
  explicit ProtoIterable(google::protobuf::ListValue const &list_value);

  // ProtoIterable is copiable and movable
  ProtoIterable(const ProtoIterable &) = default;
  ProtoIterable &operator=(const ProtoIterable &) = default;
  ProtoIterable(ProtoIterable &&other) = default;
  ProtoIterable &operator=(ProtoIterable &&other) = default;

  size_t Size() override;
  bool Empty() override;
  libredfish::RedfishVariant operator[](int index) const override;

 private:
  google::protobuf::ListValue list_value_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_PROTO_VARIANT_H_
