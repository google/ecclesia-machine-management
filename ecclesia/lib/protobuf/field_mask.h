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

// This library provides a thin wrapper around the protobuf field mask utility
// library, specifically exporting functions that match the static FieldMaskUtil
// functions we use.
//
// This library is provided because depending on which versions of the protobuf
// libraries you link against, you get slightly different signatures for some of
// these functions. In particular, there is variation between whether or not a
// custom StringPiece class is used, or a more standard string_view. This
// wrapper provides string_view based functions, and abstracts away any possible
// necessary translations between standard and protobuf types.
//
// If the standard github protobuf repo adopts a string_view type for these
// functions then this wrapper can be removed and calls to it can be replaced
// with direct calls to the underlying functions.

#ifndef ECCLESIA_LIB_PROTOBUF_FIELD_MASK_H_
#define ECCLESIA_LIB_PROTOBUF_FIELD_MASK_H_

#include <vector>

#include "google/protobuf/field_mask.pb.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "absl/strings/string_view.h"

namespace ecclesia {
namespace ecclesia_field_mask_util {

// For documentation on these functions see FieldMaskUtil.
void FromString(absl::string_view str, ::google::protobuf::FieldMask* out);
void Intersect(const ::google::protobuf::FieldMask& mask1,
               const ::google::protobuf::FieldMask& mask2,
               ::google::protobuf::FieldMask* out);
bool TrimMessage(const ::google::protobuf::FieldMask& mask,
                 ::google::protobuf::Message* message);
bool GetFieldDescriptors(
    const ::google::protobuf::Descriptor* descriptor, absl::string_view path,
    std::vector<const ::google::protobuf::FieldDescriptor*>* field_descriptors);
template <typename T>
bool IsValidPath(absl::string_view path) {
  return GetFieldDescriptors(T::descriptor(), path, nullptr);
}

}  // namespace ecclesia_field_mask_util
}  // namespace ecclesia

#endif  // ECCLESIA_LIB_PROTOBUF_FIELD_MASK_H_
