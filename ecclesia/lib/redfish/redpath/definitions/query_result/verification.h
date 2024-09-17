/*
 * Copyright 2024 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_REDFISH_REDPATH_DEFINITIONS_QUERY_RESULT_VERIFICATION_H_
#define ECCLESIA_LIB_REDFISH_REDFISH_REDPATH_DEFINITIONS_QUERY_RESULT_VERIFICATION_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result_verification.pb.h"

namespace ecclesia {

// Options for comparison functions. These are used to provide more useful
// error messages.
struct VerificationOptions {
  VerificationOptions& ExpectedActual() {
    label_a = "Expected";
    label_b = "Actual";
    return *this;
  }

  VerificationOptions& GoldenExperimental() {
    label_a = "Golden";
    label_b = "Experimental";
    return *this;
  }

  // The label to use for the values being compared.
  absl::string_view label_a = "valueA";
  absl::string_view label_b = "valueB";
};

// Compare two scalar query values against the given operation.
absl::Status CompareQueryValues(
    const QueryValue& value_a, const QueryValue& value_b,
    Comparison::Operation operation, std::vector<std::string>& errors,
    const VerificationOptions& options = VerificationOptions());

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDFISH_REDPATH_DEFINITIONS_QUERY_RESULT_VERIFICATION_H_
