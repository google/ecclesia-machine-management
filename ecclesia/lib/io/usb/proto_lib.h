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

#ifndef ECCLESIA_LIB_IO_USB_PROTO_LIB_H_
#define ECCLESIA_LIB_IO_USB_PROTO_LIB_H_

#include <optional>

#include "ecclesia/lib/io/usb/usb.h"
#include "ecclesia/lib/io/usb/usb.pb.h"

namespace ecclesia {

// Functions for converting ecclesia::UsbLocation to and from
// ecclesia::UsbLocationProtobuf
UsbLocationProtobuf UsbLocationToProto(const UsbLocation &location);
std::optional<UsbLocation> UsbLocationFromProto(
    const UsbLocationProtobuf &location);

// Function for converting ecclesia::UsbSignature to
// ecclesia::UsbSignatureProtobuf
UsbSignatureProtobuf UsbSignatureToProto(const UsbSignature &signature);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_USB_PROTO_LIB_H_
