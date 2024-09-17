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

#include "ecclesia/lib/redfish/redpath/definitions/query_result/verification.h"

#include <string>
#include <vector>

#include "google/protobuf/timestamp.pb.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/converter.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result_verification.pb.h"
#include "ecclesia/lib/time/proto.h"

namespace ecclesia {
namespace {

bool operator==(const Identifier& a, const Identifier& b) {
  return a.local_devpath() == b.local_devpath() &&
         a.machine_devpath() == b.machine_devpath() &&
         a.embedded_location_context() == b.embedded_location_context() &&
         a.redfish_location().service_label() ==
             b.redfish_location().service_label() &&
         a.redfish_location().part_location_context() ==
             b.redfish_location().part_location_context();
}

bool operator!=(const Identifier& a, const Identifier& b) { return !(a == b); }

template <typename T>
std::string InternalErrorMessage(absl::string_view message, const T& value_a,
                                 const T& value_b,
                                 const VerificationOptions& options) {
  return absl::StrFormat("%s, %s: '%v', %s: '%v'", message, options.label_a,
                         value_a, options.label_b, value_b);
}

template <>
std::string InternalErrorMessage(absl::string_view message,
                                 const Identifier& value_a,
                                 const Identifier& value_b,
                                 const VerificationOptions& options) {
  return absl::StrFormat("%s, %s: '%s', %s: '%s'", message, options.label_a,
                         IdentifierValueToJson(value_a).dump(), options.label_b,
                         IdentifierValueToJson(value_b).dump());
}

template <typename T>
absl::Status Compare(const T& value_a, const T& value_b,
                     Comparison::Operation operation,
                     std::vector<std::string>& errors,
                     const VerificationOptions& options) {
  auto internal_error = [&](absl::string_view message) {
    std::string error_message =
        InternalErrorMessage(message, value_a, value_b, options);
    errors.push_back(error_message);
    return absl::InternalError(error_message);
  };
  switch (operation) {
    case Comparison::OPERATION_EQUAL:
      if (value_a != value_b) {
        return internal_error("Failed equality check");
      }
      break;
    case Comparison::OPERATION_NOT_EQUAL:
      if (value_a == value_b) {
        return internal_error("Failed inequality check");
      }
      break;
    default:
      return absl::InternalError(absl::StrFormat(
          "Unsupported operation %s", Comparison::Operation_Name(operation)));
      break;
  }
  return absl::OkStatus();
}

absl::Status CompareRawData(const QueryValue::RawData& value_a,
                            const QueryValue::RawData& value_b,
                            Comparison::Operation operation,
                            std::vector<std::string>& errors,
                            const VerificationOptions& options) {
  if (value_a.value_case() != value_b.value_case()) {
    return absl::FailedPreconditionError(
        "Raw data values have different types and cannot be compared");
  }
  switch (value_a.value_case()) {
    case QueryValue_RawData::kRawStringValue:
      return Compare(value_a.raw_string_value(), value_b.raw_string_value(),
                     operation, errors, options);
    case QueryValue_RawData::kRawBytesValue:
      return Compare(value_a.raw_bytes_value(), value_b.raw_bytes_value(),
                     operation, errors, options);
    default:
      break;
  }
  return absl::FailedPreconditionError("Unsupported raw data type");
}

}  // namespace

absl::Status CompareQueryValues(const QueryValue& value_a,
                                const QueryValue& value_b,
                                Comparison::Operation operation,
                                std::vector<std::string>& errors,
                                const VerificationOptions& options) {
  if (value_a.kind_case() != value_b.kind_case()) {
    return absl::FailedPreconditionError(
        "Query values have different types and cannot be compared.");
  }
  switch (value_b.kind_case()) {
    case QueryValue::kSubqueryValue:
    case QueryValue::kListValue:
      return absl::FailedPreconditionError(
          "Only scalar values can be compared");
    case QueryValue::kIntValue:
      return Compare(value_a.int_value(), value_b.int_value(), operation,
                     errors, options);
    case QueryValue::kDoubleValue:
      return Compare(value_a.double_value(), value_b.double_value(), operation,
                     errors, options);
    case QueryValue::kStringValue:
      return Compare(value_a.string_value(), value_b.string_value(), operation,
                     errors, options);
    case QueryValue::kBoolValue:
      return Compare(value_a.bool_value(), value_b.bool_value(), operation,
                     errors, options);
    case QueryValue::kTimestampValue:
      return Compare(AbslTimeFromProtoTime(value_a.timestamp_value()),
                     AbslTimeFromProtoTime(value_b.timestamp_value()),
                     operation, errors, options);
    case QueryValue::kIdentifier:
      return Compare(value_a.identifier(), value_b.identifier(), operation,
                     errors, options);
    case QueryValue::kRawData:
      return CompareRawData(value_a.raw_data(), value_b.raw_data(), operation,
                            errors, options);
    default:
      break;
  }
  return absl::FailedPreconditionError("Unsupported query value type");
}

}  // namespace ecclesia
