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

#ifndef ECCLESIA_LIB_REDFISH_PROTO_REDFISH_V1_GRPC_INCLUDE_H_
#define ECCLESIA_LIB_REDFISH_PROTO_REDFISH_V1_GRPC_INCLUDE_H_

#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"

namespace ecclesia {

#ifdef INTERNAL_GRPC_GEN
using GrpcRedfishV1 = ::redfish::v1::grpc_gen::RedfishV1;
#else
using GrpcRedfishV1 = ::redfish::v1::RedfishV1;
#endif
}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_PROTO_REDFISH_V1_GRPC_INCLUDE_H_
