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

#include "ecclesia/lib/protobuf/field_mask.h"

#include <vector>

#include "google/protobuf/field_mask.pb.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/util/field_mask_util.h"
#include "absl/strings/string_view.h"

namespace ecclesia {
namespace ecclesia_field_mask_util {
namespace {

// Determine the type that the included protobuf library uses for string view
// parameters by seeing what the type the FromString function takes.
template <typename T>
struct ArgExtractor;

template <typename T>
struct ArgExtractor<void(T, google::protobuf::FieldMask*)> {
  using type = T;
};
using StringViewType = typename ArgExtractor<
    decltype(google::protobuf::util::FieldMaskUtil::FromString)>::type;

}  // namespace

void FromString(absl::string_view str, google::protobuf::FieldMask* out) {
  google::protobuf::util::FieldMaskUtil::FromString(
      StringViewType(str.data(), str.size()), out);
}
void Intersect(const google::protobuf::FieldMask& mask1,
               const google::protobuf::FieldMask& mask2,
               google::protobuf::FieldMask* out) {
  google::protobuf::util::FieldMaskUtil::Intersect(mask1, mask2, out);
}
bool TrimMessage(const google::protobuf::FieldMask& mask,
                 google::protobuf::Message* message) {
  return google::protobuf::util::FieldMaskUtil::TrimMessage(mask, message);
}
bool GetFieldDescriptors(
    const google::protobuf::Descriptor* descriptor, absl::string_view path,
    std::vector<const google::protobuf::FieldDescriptor*>* field_descriptors) {
  return google::protobuf::util::FieldMaskUtil::GetFieldDescriptors(
      descriptor, StringViewType(path.data(), path.size()), field_descriptors);
}

}  // namespace ecclesia_field_mask_util
}  // namespace ecclesia
