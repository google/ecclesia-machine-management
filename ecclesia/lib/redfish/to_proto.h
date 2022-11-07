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

#ifndef ECCLESIA_LIB_REDFISH_TO_PROTO_H_
#define ECCLESIA_LIB_REDFISH_TO_PROTO_H_

#include "absl/status/status.h"
#include "ecclesia/lib/redfish/interface.h"
#include "google/protobuf/message.h"

namespace ecclesia {

// “Convert” a Redfish object “obj” into a proto message by filling out “msg”.
// The proto definition should match the JSON schema of the Redfish object.
// Proto fields that do not have the equivalent JSON property will be left
// unchanged. Supports proto values types are integers, floats, strings, enums,
// and sub-messages. Supports optional and repeated fields.
//
// Returns ok if the conversion is successful. Otherwise returns an appropriate
// status with a message. If the return value is not ok, “msg” may be
// half-filled, and should not be used.
//
// This works by looking through each proto field and try to find a matching
// property in “obj” by name. For a enum field it is assumed that the
// corresponding Redfish property is a string. For sub-messages it works
// recursively.
//
// Functionally this is similar to google::protobuf::util::JsonFormat::Parse(), but this
// works on RedfishObjects, which is a higher level abstraction of the
// underlying format (mostly but not necessarily JSON).
absl::Status RedfishObjToProto(const RedfishObject &obj, google::protobuf::Message *msg);
}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TO_PROTO_H_
