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

#ifndef ECCLESIA_LIB_REDFISH_TRANSPORT_STRUCT_PROTO_CONVERSION_H_
#define ECCLESIA_LIB_REDFISH_TRANSPORT_STRUCT_PROTO_CONVERSION_H_

#include "google/protobuf/struct.pb.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
// Converts a Struct into a JSON object.
nlohmann::json StructToJson(const google::protobuf::Struct& message);

// Assumes the input JSON is an object. If the JSON is not an object, returns
// an empty Struct.
google::protobuf::Struct JsonToStruct(const nlohmann::json& json);

// Converts a Value into a JSON value.
nlohmann::json ValueToJson(const google::protobuf::Value& message);

// Converts every input JSON into the corresponding Struct_Value:
// Struct_Value <-> JSON
////////////////////////////
// null_value       null
// number_value     double
// string_value     string
// bool_value       boolean
// struct_value     object
// list_value       array
////////////////////////////
google::protobuf::Value JsonToValue(const nlohmann::json& json);

// Converts a ListValue into a JSON value.
nlohmann::json ListValueToJson(const google::protobuf::ListValue& message);

// Assumes the input JSON is an array. If the JSON is not an array, returns an
// empty ListValue
google::protobuf::ListValue JsonToListValue(const nlohmann::json& json);
}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_STRUCT_PROTO_CONVERSION_H_
