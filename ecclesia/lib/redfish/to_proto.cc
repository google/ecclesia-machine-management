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

#include "ecclesia/lib/redfish/to_proto.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "absl/status/status.h"
#include "ecclesia/lib/redfish/interface.h"

namespace ecclesia {
namespace {

// The signature of the setter and adder member functions in google::protobuf::Reflection.
// Examples of these members are SetInt32, AddFloat, etc.
template <typename ProtoValueType>
using MutateFieldFuncType = void (google::protobuf::Reflection::*)(
    google::protobuf::Message *, const google::protobuf::FieldDescriptor *,
    ProtoValueType) const;

// Copies the value of one Redfish property from RedfishObject “obj” into a
// proto field in a Message “msg”. The Redfish property to copy is determined by
// the name of the proto field, which is defined by argument “field”.
//
// The first two template arguments are the underlying JSON value type and the
// underlying proto value type, respectively. The “AddField” argument is the
// AddXxxx member function in the Reflection class that is used to add a value
// of the denoted type (for example Reflection::AddFloat), and similarly for the
// “SetField” argument. These two arguments are needed because Reflection does
// not provide public AddField<>() and SetField<>() generic member functions.
template <typename JsonType, typename ProtoType,
          MutateFieldFuncType<ProtoType> AddField,
          MutateFieldFuncType<ProtoType> SetField>
void RedfishPropertyToProto(const RedfishObject &obj,
                            const google::protobuf::Reflection &ref,
                            const google::protobuf::FieldDescriptor &field,
                            google::protobuf::Message *msg) {
  if (field.is_repeated()) {
    std::unique_ptr<RedfishIterable> iter = obj[field.name()].AsIterable();
    if (!iter) return;
    for (size_t repeat_idx = 0; repeat_idx < iter->Size(); repeat_idx++) {
      JsonType value;
      if (!(*iter)[repeat_idx].GetValue(&value)) break;
      (ref.*AddField)(msg, &field, std::move(value));
    }
  } else {
    std::optional<JsonType> value = obj.GetNodeValue<JsonType>(field.name());
    if (!value.has_value()) return;
    (ref.*SetField)(msg, &field, *std::move(value));
  }
}

}  // namespace

absl::Status RedfishObjToProto(const RedfishObject &obj, google::protobuf::Message *msg) {
  if (msg == nullptr) {
    return absl::InternalError(
        "Converting Redfish object into null proto message");
  }

  const google::protobuf::Descriptor *desc = msg->GetDescriptor();
  const google::protobuf::Reflection *ref = msg->GetReflection();

  // For each field, try to find a JSON property in “obj” with the same name,
  // and copy the value.
  for (int i = 0; i < desc->field_count(); i++) {
    const google::protobuf::FieldDescriptor *field = desc->field(i);
    switch (field->type()) {
      case google::protobuf::FieldDescriptor::TYPE_FLOAT:
        RedfishPropertyToProto<double, float, &google::protobuf::Reflection::AddFloat,
                               &google::protobuf::Reflection::SetFloat>(obj, *ref,
                                                                *field, msg);
        break;
      case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
        RedfishPropertyToProto<double, double, &google::protobuf::Reflection::AddDouble,
                               &google::protobuf::Reflection::SetDouble>(obj, *ref,
                                                                 *field, msg);
        break;
      case google::protobuf::FieldDescriptor::TYPE_INT64:
        RedfishPropertyToProto<int64_t, int64_t,
                               &google::protobuf::Reflection::AddInt64,
                               &google::protobuf::Reflection::SetInt64>(obj, *ref,
                                                                *field, msg);
        break;
      case google::protobuf::FieldDescriptor::TYPE_UINT64:
        RedfishPropertyToProto<int64_t, uint64_t,
                               &google::protobuf::Reflection::AddUInt64,
                               &google::protobuf::Reflection::SetUInt64>(obj, *ref,
                                                                 *field, msg);
        break;
      case google::protobuf::FieldDescriptor::TYPE_INT32:
        RedfishPropertyToProto<int32_t, int32_t,
                               &google::protobuf::Reflection::AddInt32,
                               &google::protobuf::Reflection::SetInt32>(obj, *ref,
                                                                *field, msg);
        break;
      case google::protobuf::FieldDescriptor::TYPE_UINT32:
        RedfishPropertyToProto<int32_t, uint32_t,
                               &google::protobuf::Reflection::AddUInt32,
                               &google::protobuf::Reflection::SetUInt32>(obj, *ref,
                                                                 *field, msg);
        break;
      case google::protobuf::FieldDescriptor::TYPE_BOOL:
        RedfishPropertyToProto<bool, bool, &google::protobuf::Reflection::AddBool,
                               &google::protobuf::Reflection::SetBool>(obj, *ref,
                                                               *field, msg);
        break;
      case google::protobuf::FieldDescriptor::TYPE_STRING:
        RedfishPropertyToProto<std::string, std::string,
                               &google::protobuf::Reflection::AddString,
                               &google::protobuf::Reflection::SetString>(obj, *ref,
                                                                 *field, msg);
        break;
      case google::protobuf::FieldDescriptor::TYPE_ENUM: {
        // Enum values are string in JSON.
        const google::protobuf::EnumDescriptor *enum_desc = field->enum_type();
        if (field->is_repeated()) {
          std::unique_ptr<RedfishIterable> iter =
              obj[field->name()].AsIterable();
          if (!iter) break;
          for (size_t repeat_idx = 0; repeat_idx < iter->Size(); repeat_idx++) {
            std::string value;
            if (!(*iter)[repeat_idx].GetValue(&value)) break;
            const google::protobuf::EnumValueDescriptor *enum_value =
                enum_desc->FindValueByName(std::move(value));
            ref->AddEnum(msg, field, enum_value);
          }
        } else {
          std::optional<std::string> value =
              obj.GetNodeValue<std::string>(field->name());
          if (!value.has_value()) break;
          const google::protobuf::EnumValueDescriptor *enum_value =
              enum_desc->FindValueByName(*value);
          ref->SetEnum(msg, field, enum_value);
        }
        break;
      }
      case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
        // For sub messages, just recursively copy the value of it.
        if (field->is_repeated()) {
          absl::Status status = absl::OkStatus();
          obj[field->name()].Each().Do(
              [&](std::unique_ptr<RedfishObject> &sub_obj) {
                if (!status.ok()) {
                  // Conversion failed before. Move onto the next obj.
                  return RedfishIterReturnValue::kContinue;
                }
                absl::Status sub_status =
                    RedfishObjToProto(*sub_obj, ref->AddMessage(msg, field));
                if (!sub_status.ok()) {
                  status = sub_status;
                  return RedfishIterReturnValue::kStop;
                }
                return RedfishIterReturnValue::kContinue;
              });
          if (!status.ok()) {
            return status;
          }
        } else {
          std::unique_ptr<RedfishObject> sub_obj =
              obj[field->name()].AsObject();
          if (!sub_obj) break;
          absl::Status status =
              RedfishObjToProto(*sub_obj, ref->MutableMessage(msg, field));
          if (!status.ok()) {
            return status;
          }
        }
        break;
      default:
        return absl::UnimplementedError(
            "Unimplemented conversion from Redfish object to proto");
    }
  }
  return absl::OkStatus();
}

}  // namespace ecclesia
