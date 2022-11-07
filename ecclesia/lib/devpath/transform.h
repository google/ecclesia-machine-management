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

// Utility function for performing standard transformations of devpath fields in
// protocol buffers. This is mostly useful for taking protobuf-structured data
// that contains devpath fields and switching them between being machine paths
// or domain-specific ones.

#ifndef ECCLESIA_MMANAGER_LIB_DEVPATH_TRANSFORM_H_
#define ECCLESIA_MMANAGER_LIB_DEVPATH_TRANSFORM_H_

#include <functional>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "google/protobuf/message.h"

namespace ecclesia {
// Generic function for transforming a devpath field. It should expect a devpath
// as a parameter and return either the newly transformed path (on success) or
// nothing (on failure).
using TransformDevpathFunction =
    std::function<std::optional<std::string>(absl::string_view)>;

// Given a generic protocol buffer, use the supplied function to transform all
// of the devpath fields (specified by the field mask).
//
// The paths parameter is expected to follow the standard field mask string
// format defined by FieldMaskUtil::FromString in field_mask_util.h.
//
// Returns true if all of the fields were successfully transformed, or false
// otherwise. If the transformation fails the message is left in an unspecified
// state: it may have been partially transformed, or maybe not.
bool TransformProtobufDevpaths(const TransformDevpathFunction &transform,
                               absl::string_view field_mask,
                               google::protobuf::Message *message);
}  // namespace ecclesia

#endif  // ECCLESIA_MMANAGER_LIB_DEVPATH_TRANSFORM_H_
