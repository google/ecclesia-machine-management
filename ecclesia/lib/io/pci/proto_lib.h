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

// Conversion functions for going between C++ types and their proto equivalents.
//
// There are two forms of function: XxxToProto and XxxFromProto. The first form
// is guranteed to succeed and so will always return the protobuf type directly
// but the second form can fail if the protobuf contains out-of-range data and
// so it returns an optional value that is null when the conversion fails.

#ifndef ECCLESIA_LIB_IO_PCI_PROTO_LIB_H_
#define ECCLESIA_LIB_IO_PCI_PROTO_LIB_H_

#include "absl/types/optional.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/pci.pb.h"
#include "ecclesia/lib/io/pci/signature.h"

namespace ecclesia {

PciLocationProtobuf PciLocationToProto(const PciLocation &location);
absl::optional<PciLocation> PciLocationFromProto(
    const PciLocationProtobuf &location);

PciBaseSignatureProtobuf PciBaseSignatureToProto(
    const PciBaseSignature &signature);
absl::optional<PciBaseSignature> PciBaseSignatureFromProto(
    const PciBaseSignatureProtobuf &signature);

PciSubsystemSignatureProtobuf PciSubsystemSignatureToProto(
    const PciSubsystemSignature &signature);
absl::optional<PciSubsystemSignature> PciSubsystemSignatureFromProto(
    const PciSubsystemSignatureProtobuf &signature);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_PCI_PROTO_LIB_H_
