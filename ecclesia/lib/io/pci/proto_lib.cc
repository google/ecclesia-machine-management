/*
 * Copyright 2020 Google LLC
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

#include "ecclesia/lib/io/pci/proto_lib.h"

#include "absl/types/optional.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/pci.pb.h"
#include "ecclesia/lib/io/pci/signature.h"

namespace ecclesia {

PciLocationProtobuf PciLocationToProto(const PciLocation &location) {
  PciLocationProtobuf protobuf;
  protobuf.set_domain(location.domain().value());
  protobuf.set_bus(location.bus().value());
  protobuf.set_device(location.device().value());
  protobuf.set_function(location.function().value());
  return protobuf;
}

absl::optional<PciLocation> PciLocationFromProto(
    const PciLocationProtobuf &location) {
  return PciLocation::TryMake(location.domain(), location.bus(),
                              location.device(), location.function());
}

PciBaseSignatureProtobuf PciBaseSignatureToProto(
    const PciBaseSignature &signature) {
  PciBaseSignatureProtobuf protobuf;
  protobuf.set_vendor_id(signature.vendor_id().value());
  protobuf.set_device_id(signature.device_id().value());
  return protobuf;
}

absl::optional<PciBaseSignature> PciBaseSignatureFromProto(
    const PciBaseSignatureProtobuf &signature) {
  return PciBaseSignature::TryMake(signature.vendor_id(),
                                   signature.device_id());
}

PciSubsystemSignatureProtobuf PciSubsystemSignatureToProto(
    const PciSubsystemSignature &signature) {
  PciSubsystemSignatureProtobuf protobuf;
  protobuf.set_vendor_id(signature.vendor_id().value());
  protobuf.set_id(signature.id().value());
  return protobuf;
}

absl::optional<PciSubsystemSignature> PciSubsystemSignatureFromProto(
    const PciSubsystemSignatureProtobuf &signature) {
  return PciSubsystemSignature::TryMake(signature.vendor_id(), signature.id());
}

}  // namespace ecclesia
