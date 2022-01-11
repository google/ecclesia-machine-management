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

#include "ecclesia/lib/redfish/transport/struct_proto_conversion.h"

#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

nlohmann::json StructToJson(const google::protobuf::Struct& message) {
  auto json = nlohmann::json::object();
  for (const auto& [key, value] : message.fields()) {
    json[key] = ValueToJson(value);
  }
  return json;
}

google::protobuf::Struct JsonToStruct(const nlohmann::json& json) {
  google::protobuf::Struct message;
  if (json.is_object()) {
    auto* fields = message.mutable_fields();

    for (const auto& [key, value] : json.items()) {
      (*fields)[key] = JsonToValue(value);
    }
  }
  return message;
}

nlohmann::json ValueToJson(const google::protobuf::Value& message) {
  nlohmann::json json;
  switch (message.kind_case()) {
    case google::protobuf::Value::kNullValue:
      json = nullptr;
      break;
    case google::protobuf::Value::kNumberValue:
      json = message.number_value();
      break;
    case google::protobuf::Value::kStringValue:
      json = message.string_value();
      break;
    case google::protobuf::Value::kBoolValue:
      json = message.bool_value();
      break;
    case google::protobuf::Value::kStructValue:
      json = StructToJson(message.struct_value());
      break;
    case google::protobuf::Value::kListValue:
      json = ListValueToJson(message.list_value());
      break;
    case google::protobuf::Value::KIND_NOT_SET:
      json = nlohmann::json::object();
      break;
  }
  return json;
}

google::protobuf::Value JsonToValue(const nlohmann::json& json) {
  google::protobuf::Value message;
  if (json.is_null()) {
    message.set_null_value(google::protobuf::NullValue::NULL_VALUE);
  } else if (json.is_number()) {
    auto dbl_value = json.get<double>();
    message.set_number_value(dbl_value);
  } else if (json.is_string()) {
    auto str_value = json.get<std::string>();
    message.set_string_value(str_value);
  } else if (json.is_boolean()) {
    auto bool_value = json.get<bool>();
    message.set_bool_value(bool_value);
  } else if (json.is_object()) {
    *message.mutable_struct_value() = JsonToStruct(json);
  } else if (json.is_array()) {
    *message.mutable_list_value() = JsonToListValue(json);
  }
  return message;
}

nlohmann::json ListValueToJson(const google::protobuf::ListValue& message) {
  nlohmann::json json = nlohmann::json::array();
  for (const auto& value : message.values()) {
    json.push_back(ValueToJson(value));
  }
  return json;
}

google::protobuf::ListValue JsonToListValue(const nlohmann::json& json) {
  google::protobuf::ListValue message;
  if (json.is_array()) {
    for (const auto& value : json) {
      *message.add_values() = JsonToValue(value);
    }
  }
  return message;
}

}  // namespace ecclesia
