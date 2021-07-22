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

#include "ecclesia/lib/io/usb/proto_lib.h"

#include <cstddef>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/io/usb/usb.h"
#include "ecclesia/lib/io/usb/usb.pb.h"

namespace ecclesia {

UsbLocationProtobuf UsbLocationToProto(const UsbLocation &location) {
  UsbLocationProtobuf proto_location;
  proto_location.set_bus(location.Bus().value());
  std::vector<int> ports;
  for (size_t i = 0; i < location.NumPorts(); i++) {
    ports.push_back(location.Port(i)->value());
  }
  if (ports.empty()) {
    // Root device will be "0"
    proto_location.set_ports("0");
  } else {
    proto_location.set_ports(absl::StrJoin(ports, "."));
  }
  return proto_location;
}

absl::optional<UsbLocation> UsbLocationFromProto(
    const UsbLocationProtobuf &location) {
  auto maybe_bus = UsbBusLocation::TryMake(location.bus());
  absl::optional<UsbPortSequence> maybe_port_sequence;
  // Check if root device aka ports = "0"
  if (location.ports() != "0") {
    std::vector<int> ports;
    for (const auto port_str : absl::StrSplit(location.ports(), '.')) {
      int port_num;
      if (absl::SimpleAtoi(port_str, &port_num)) {
        ports.push_back(port_num);
      } else {
        return absl::nullopt;
      }
    }
    maybe_port_sequence = UsbPortSequence::TryMake(ports);
  } else {
    maybe_port_sequence = UsbPortSequence();
  }
  if (maybe_bus.has_value() && maybe_port_sequence.has_value()) {
    return UsbLocation(maybe_bus.value(), maybe_port_sequence.value());
  }
  return absl::nullopt;
}

UsbSignatureProtobuf UsbSignatureToProto(const UsbSignature &signature) {
  UsbSignatureProtobuf protobuf;
  protobuf.set_product_id(signature.product_id);
  protobuf.set_vendor_id(signature.vendor_id);
  return protobuf;
}

}  // namespace ecclesia
