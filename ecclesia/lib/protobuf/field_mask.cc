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

#include <cctype>
#include <string_view>
#include <vector>

#include "google/protobuf/field_mask.pb.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/util/field_mask_util.h"

namespace ecclesia {
namespace ecclesia_field_mask_util {
namespace {

// Determine the type that the included protobuf library uses for string view
// parameters by seeing what the type the FromString function takes.
template <typename T>
struct ArgExtractor;

template <typename T>
struct ArgExtractor<void(T, google::protobuf::FieldMask *)> {
  using type = T;
};
using StringViewType = typename ArgExtractor<
    decltype(google::protobuf::util::FieldMaskUtil::FromString)>::type;

}  // namespace

void FromString(absl::string_view str, google::protobuf::FieldMask *out) {
  google::protobuf::util::FieldMaskUtil::FromString(
      StringViewType(str.data(), str.size()), out);
}
bool GetFieldDescriptors(
    const google::protobuf::Descriptor *descriptor, absl::string_view path,
    std::vector<const google::protobuf::FieldDescriptor *> *field_descriptors) {
  return google::protobuf::util::FieldMaskUtil::GetFieldDescriptors(
      descriptor, StringViewType(path.data(), path.size()), field_descriptors);
}
void ToCanonicalForm(const google::protobuf::FieldMask &mask,
                     google::protobuf::FieldMask *out) {
  google::protobuf::util::FieldMaskUtil::ToCanonicalForm(mask, out);
}
void Intersect(const google::protobuf::FieldMask &mask1,
               const google::protobuf::FieldMask &mask2,
               google::protobuf::FieldMask *out) {
  google::protobuf::util::FieldMaskUtil::Intersect(mask1, mask2, out);
}
void Subtract(const google::protobuf::Descriptor *descriptor,
              const google::protobuf::FieldMask &mask1,
              const google::protobuf::FieldMask &mask2,
              google::protobuf::FieldMask *out) {
  google::protobuf::util::FieldMaskUtil::Subtract(descriptor, mask1, mask2, out);
}
bool TrimMessage(const google::protobuf::FieldMask &mask,
                 google::protobuf::Message *message) {
  return google::protobuf::util::FieldMaskUtil::TrimMessage(mask, message);
}
absl::StatusOr<std::string> SnakeCaseToCamelCase(std::string_view input,
                                                 bool to_lower_case) {
  // to_lower_case determines if it is UpperCaseCamel or lowerCaseCamel.
  bool after_underscore = !to_lower_case;
  std::string output;
  for (char input_char : input) {
    if (input_char >= 'A' && input_char <= 'Z') {
      return absl::InternalError(
          "The field name must not contain uppercase letters.");
    }
    if (after_underscore) {
      if (input_char >= 'a' && input_char <= 'z') {
        output.push_back(input_char + 'A' - 'a');
        after_underscore = false;
      } else {
        return absl::InternalError(
            "The character after a \"_\" must be a lowercase letter");
      }
    } else if (input_char == '_') {
      after_underscore = true;
    } else {
      output.push_back(input_char);
    }
  }
  if (after_underscore) {
    // Trailing "_".
    return absl::InternalError("Trailing \"_\"");
  }
  return output;
}

absl::StatusOr<std::string> CamelCaseToSnakeCase(std::string_view input,
                                                 bool to_lower_case) {
  std::string output;
  auto *convert = to_lower_case ? [](char c) { return std::tolower(c); }
                                : [](char c) { return std::toupper(c); };

  for (size_t i = 0; i < input.size(); ++i) {
    char c = input[i];
    if (!isalpha(c) && !isdigit(c)) {
      return absl::InternalError("Contains non-alphabet character");
    }
    if (std::isupper(c) && i != 0) {
      output.push_back('_');
    }
    output.push_back(static_cast<char>(convert(c)));
  }
  return output;
}

}  // namespace ecclesia_field_mask_util
}  // namespace ecclesia
