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
#include <utility>
#include <vector>

#include "google/protobuf/timestamp.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/converter.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result_verification.pb.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/time/proto.h"
#include "re2/re2.h"

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

absl::StatusOr<std::string> GenerateIdentifier(
    const ListValueVerification& validation, const QueryValue& value) {
  if (!value.has_subquery_value()) {
    return absl::FailedPreconditionError(
        "Identifiers are only supported for subquery values");
  }

  std::vector<std::string> identifier_values;
  for (absl::string_view identifier : validation.identifiers()) {
    if (auto it = value.subquery_value().fields().find(identifier);
        it != value.subquery_value().fields().end()) {
      std::string property_value;
      if (it->second.has_identifier()) {
        property_value = IdentifierValueToJson(it->second.identifier()).dump();
      } else {
        property_value = ValueToJson(it->second).dump();
      }
      identifier_values.push_back(
          absl::StrCat(identifier, "=", property_value));
    } else {
      return absl::FailedPreconditionError(
          absl::StrCat("property ", identifier, " is not present"));
    }
  }
  return absl::StrJoin(identifier_values, ",");
}

template <typename T>
absl::Status Compare(const T& value_a, const T& value_b,
                     Verification::Compare comparison,
                     std::vector<std::string>& errors,
                     const VerificationOptions& options) {
  auto internal_error = [&](absl::string_view message) {
    std::string error_message =
        InternalErrorMessage(message, value_a, value_b, options);
    errors.push_back(error_message);
    return absl::InternalError(error_message);
  };
  switch (comparison) {
    case Verification::COMPARE_UNKNOWN:
      // This means that the comparison is not specified and the verification
      // should be skipped.
      return absl::OkStatus();
    case Verification::COMPARE_EQUAL:
      if (value_a != value_b) {
        return internal_error("Failed equality check");
      }
      break;
    case Verification::COMPARE_NOT_EQUAL:
      if (value_a == value_b) {
        return internal_error("Failed inequality check");
      }
      break;
    default:
      return absl::InternalError(absl::StrFormat(
          "Unsupported comparison %s", Verification::Compare_Name(comparison)));
      break;
  }
  return absl::OkStatus();
}

absl::Status CompareRawData(const QueryValue::RawData& value_a,
                            const QueryValue::RawData& value_b,
                            Verification::Compare comparison,
                            std::vector<std::string>& errors,
                            const VerificationOptions& options) {
  if (value_a.value_case() != value_b.value_case()) {
    return absl::FailedPreconditionError(
        "Raw data values have different types and cannot be compared");
  }
  switch (value_a.value_case()) {
    case QueryValue_RawData::kRawStringValue:
      return Compare(value_a.raw_string_value(), value_b.raw_string_value(),
                     comparison, errors, options);
    case QueryValue_RawData::kRawBytesValue:
      return Compare(value_a.raw_bytes_value(), value_b.raw_bytes_value(),
                     comparison, errors, options);
    default:
      break;
  }
  return absl::FailedPreconditionError("Unsupported raw data type");
}

template <typename T>
absl::Status Operation(const T& value, const T& operand,
                       const Verification::Validation::Operation& operation,
                       std::vector<std::string>& errors) {
  switch (operation) {
    case Verification::Validation::OPERATION_GREATER_THAN:
      if (value > operand) {
        return absl::OkStatus();
      }
      break;
    case Verification::Validation::OPERATION_GREATER_THAN_OR_EQUAL:
      if (value >= operand) {
        return absl::OkStatus();
      }
      break;
    case Verification::Validation::OPERATION_LESS_THAN:
      if (value < operand) {
        return absl::OkStatus();
      }
      break;
    case Verification::Validation::OPERATION_LESS_THAN_OR_EQUAL:
      if (value <= operand) {
        return absl::OkStatus();
      }
      break;
    default:
      return absl::InternalError(
          absl::StrFormat("Unsupported operation %s",
                          Verification::Validation::Operation_Name(operation)));
  }

  std::string error_message = absl::StrFormat(
      "Failed %s check, value: '%v', operand: '%v'",
      Verification::Validation::Operation_Name(operation), value, operand);
  errors.push_back(error_message);
  return absl::InternalError(error_message);
}

absl::Status OperationString(
    absl::string_view value, absl::string_view operand,
    const Verification::Validation::Operation& operation,
    std::vector<std::string>& errors) {
  switch (operation) {
    case Verification::Validation::OPERATION_STRING_CONTAINS:
      if (absl::StrContains(value, operand)) {
        return absl::OkStatus();
      }
      break;
    case Verification::Validation::OPERATION_STRING_NOT_CONTAINS:
      if (!absl::StrContains(value, operand)) {
        return absl::OkStatus();
      }
      break;
    case Verification::Validation::OPERATION_STRING_STARTS_WITH:
      if (absl::StartsWith(value, operand)) {
        return absl::OkStatus();
      }
      break;
    case Verification::Validation::OPERATION_STRING_NOT_STARTS_WITH:
      if (!absl::StartsWith(value, operand)) {
        return absl::OkStatus();
      }
      break;
    case Verification::Validation::OPERATION_STRING_ENDS_WITH:
      if (absl::EndsWith(value, operand)) {
        return absl::OkStatus();
      }
      break;
    case Verification::Validation::OPERATION_STRING_NOT_ENDS_WITH:
      if (!absl::EndsWith(value, operand)) {
        return absl::OkStatus();
      }
      break;
    case Verification::Validation::OPERATION_STRING_REGEX_MATCH:
      if (RE2::PartialMatch(value, RE2(operand))) {
        return absl::OkStatus();
      }
      break;
    case Verification::Validation::OPERATION_STRING_NOT_REGEX_MATCH:
      if (!RE2::PartialMatch(value, RE2(operand))) {
        return absl::OkStatus();
      }
      break;
    default:
      return absl::InternalError(
          absl::StrFormat("Unsupported operation %s",
                          Verification::Validation::Operation_Name(operation)));
  }
  std::string error_message = absl::StrFormat(
      "Failed operation %s, value: '%s', operand: '%s'",
      Verification::Validation::Operation_Name(operation), value, operand);
  errors.push_back(error_message);
  return absl::InternalError(error_message);
}

absl::Status OperationQueryValue(
    const QueryValue& value, const QueryValue& operand,
    const Verification::Validation::Operation& operation,
    std::vector<std::string>& errors) {
  if (value.kind_case() != operand.kind_case()) {
    return absl::FailedPreconditionError(
        "Value and operand have different types and cannot be compared");
  }
  switch (value.kind_case()) {
    case QueryValue::kSubqueryValue:
    case QueryValue::kListValue:
      return absl::FailedPreconditionError("Value are not scalar values");
    case QueryValue::kIntValue:
      return Operation(value.int_value(), operand.int_value(), operation,
                       errors);
    case QueryValue::kDoubleValue:
      return Operation(value.double_value(), operand.double_value(), operation,
                       errors);
    case QueryValue::kStringValue:
      return OperationString(value.string_value(), operand.string_value(),
                             operation, errors);
    case QueryValue::kBoolValue:
      return absl::InternalError("Operation does not support boolean values");
    case QueryValue::kTimestampValue:
      return Operation(AbslTimeFromProtoTime(value.timestamp_value()),
                       AbslTimeFromProtoTime(operand.timestamp_value()),
                       operation, errors);
    case QueryValue::kIdentifier:
      return absl::InternalError(
          "Operation does not support Identifier values");
    case QueryValue::kRawData:
      return absl::InternalError("Operation does not support Raw Data");
    default:
      return absl::FailedPreconditionError(
          "Unsupported value type for operation");
      break;
  }
  return absl::OkStatus();
}

absl::Status Range(const QueryValue& value,
                   const google::protobuf::RepeatedPtrField<QueryValue>& operands,
                   const Verification::Validation::Range& range,
                   std::vector<std::string>& errors) {
  if (operands.empty()) {
    return absl::FailedPreconditionError(
        "Atleast one operand is required for range check. None provided.");
  }

  bool success = false;
  std::string error_message;
  Verification::Compare comparison;
  switch (range) {
    case Verification::Validation::RANGE_UNKNOWN:
      return absl::FailedPreconditionError("Range is not provided");
    case Verification::Validation::RANGE_IN:
      comparison = Verification::COMPARE_EQUAL;
      error_message = "Value is not in the range of operands provided";
      break;
    case Verification::Validation::RANGE_NOT_IN:
      comparison = Verification::COMPARE_NOT_EQUAL;
      error_message = "Value is in the range of operands provided";
      break;
    default:
      return absl::FailedPreconditionError(absl::StrFormat(
          "Unsupported range %s", Verification::Validation::Range_Name(range)));
  }

  std::vector<std::string> internal_errors;
  for (const QueryValue& operand : operands) {
    if (CompareQueryValues(value, operand, comparison, internal_errors,
                           VerificationOptions())
            .ok()) {
      success = true;
      break;
    }
  }
  if (!success) {
    errors.insert(errors.end(), internal_errors.begin(), internal_errors.end());
    return absl::InternalError(error_message);
  }
  return absl::OkStatus();
}

absl::Status Interval(const QueryValue& value,
                      const google::protobuf::RepeatedPtrField<QueryValue>& operands,
                      const Verification::Validation::Interval& interval,
                      std::vector<std::string>& errors) {
  if (operands.size() != 2) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Two operands are required for interval check. %d provided.",
        operands.size()));
  }
  switch (interval) {
    case Verification::Validation::INTERVAL_UNKNOWN:
      return absl::FailedPreconditionError("Interval is not provided");
    case Verification::Validation::INTERVAL_OPEN:
      if (OperationQueryValue(value, operands[0],
                              Verification::Validation::OPERATION_GREATER_THAN,
                              errors)
              .ok() &&
          OperationQueryValue(value, operands[1],
                              Verification::Validation::OPERATION_LESS_THAN,
                              errors)
              .ok()) {
        return absl::OkStatus();
      }
      break;
    case Verification::Validation::INTERVAL_CLOSED:
      if (OperationQueryValue(
              value, operands[0],
              Verification::Validation::OPERATION_GREATER_THAN_OR_EQUAL, errors)
              .ok() &&
          OperationQueryValue(
              value, operands[1],
              Verification::Validation::OPERATION_LESS_THAN_OR_EQUAL, errors)
              .ok()) {
        return absl::OkStatus();
      }
      break;
    case Verification::Validation::INTERVAL_OPEN_CLOSED:
      if (OperationQueryValue(value, operands[0],
                              Verification::Validation::OPERATION_GREATER_THAN,
                              errors)
              .ok() &&
          OperationQueryValue(
              value, operands[1],
              Verification::Validation::OPERATION_LESS_THAN_OR_EQUAL, errors)
              .ok()) {
        return absl::OkStatus();
      }
      break;
    case Verification::Validation::INTERVAL_CLOSED_OPEN:
      if (OperationQueryValue(
              value, operands[0],
              Verification::Validation::OPERATION_GREATER_THAN_OR_EQUAL, errors)
              .ok() &&
          OperationQueryValue(value, operands[1],
                              Verification::Validation::OPERATION_LESS_THAN,
                              errors)
              .ok()) {
        return absl::OkStatus();
      }
      break;
    default:
      return absl::FailedPreconditionError("Interval is not set");
  }

  return absl::InternalError(
      "Value is not in the interval of operands provided");
}

}  // namespace

absl::Status CompareQueryValues(const QueryValue& value_a,
                                const QueryValue& value_b,
                                Verification::Compare comparison,
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
      return Compare(value_a.int_value(), value_b.int_value(), comparison,
                     errors, options);
    case QueryValue::kDoubleValue:
      return Compare(value_a.double_value(), value_b.double_value(), comparison,
                     errors, options);
    case QueryValue::kStringValue:
      return Compare(value_a.string_value(), value_b.string_value(), comparison,
                     errors, options);
    case QueryValue::kBoolValue:
      return Compare(value_a.bool_value(), value_b.bool_value(), comparison,
                     errors, options);
    case QueryValue::kTimestampValue:
      return Compare(AbslTimeFromProtoTime(value_a.timestamp_value()),
                     AbslTimeFromProtoTime(value_b.timestamp_value()),
                     comparison, errors, options);
    case QueryValue::kIdentifier:
      return Compare(value_a.identifier(), value_b.identifier(), comparison,
                     errors, options);
    case QueryValue::kRawData:
      return CompareRawData(value_a.raw_data(), value_b.raw_data(), comparison,
                            errors, options);
    default:
      break;
  }
  return absl::FailedPreconditionError("Unsupported query value type");
}

absl::Status CompareListValues(const ListValue& value_a,
                               const ListValue& value_b,
                               const ListValueVerification& verification,
                               std::vector<std::string>& errors,
                               const VerificationOptions& options) {
  absl::flat_hash_map<std::string,
                      std::pair<const QueryValue*, const QueryValue*>>
      data_map;
  data_map.reserve(value_a.values_size() + value_b.values_size());

  auto add_to_data_map = [&](const ListValue& list_value,
                             absl::string_view label, bool is_first_item,
                             bool use_index) -> absl::Status {
    int index = 0;
    for (const ecclesia::QueryValue& value : list_value.values()) {
      std::pair<const QueryValue*, const QueryValue*>* data_map_item = nullptr;
      if (use_index) {
        data_map_item = &data_map[absl::StrCat("index=", index++)];
      } else {
        absl::StatusOr<std::string> identifier =
            GenerateIdentifier(verification, value);
        if (!identifier.ok()) {
          std::string error_message =
              absl::StrCat("Missing identifier in ", label, ": ",
                           identifier.status().message());
          errors.push_back(error_message);
          return absl::InternalError(error_message);
        }
        if (auto it = data_map.find(*identifier); it != data_map.end()) {
          if (is_first_item ||
              (!is_first_item && it->second.second != nullptr)) {
            std::string error_message = absl::StrCat("Duplicate identifier in ",
                                                     label, ": ", *identifier);
            errors.push_back(error_message);
            return absl::InternalError(error_message);
          }
          data_map_item = &it->second;
        } else {
          data_map_item = &data_map[*identifier];
        }
      }
      if (is_first_item) {
        data_map_item->first = &value;
      } else {
        data_map_item->second = &value;
      }
    }
    return absl::OkStatus();
  };

  bool use_index = verification.identifiers().empty();
  ECCLESIA_RETURN_IF_ERROR(add_to_data_map(value_a, options.label_a,
                                           /*is_first_item=*/true, use_index));
  ECCLESIA_RETURN_IF_ERROR(add_to_data_map(value_b, options.label_b,
                                           /*is_first_item=*/false, use_index));

  for (const auto& [id, values] : data_map) {
    const auto& [list_item_a, list_item_b] = values;
    std::vector<std::string> error_messages;
    auto check_list_item =
        [&identifier = id, &errors = errors, &error_messages = error_messages](
            const QueryValue* list_item, absl::string_view label) {
          if (list_item == nullptr) {
            std::string error_message = absl::StrCat(
                "Missing value in ", label, " with identifier ", identifier);
            errors.push_back(error_message);
            error_messages.push_back(std::move(error_message));
          }
        };
    check_list_item(list_item_a, options.label_a);
    check_list_item(list_item_b, options.label_b);
    if (!error_messages.empty()) {
      return absl::InternalError(absl::StrJoin(error_messages, "\n"));
    }

    switch (list_item_a->kind_case()) {
      case QueryValue::kSubqueryValue:
        ECCLESIA_RETURN_IF_ERROR(CompareSubqueryValues(
            list_item_a->subquery_value(), list_item_b->subquery_value(),
            verification.verify().data_compare(), errors, options));
        break;
      case QueryValue::kListValue:
        return absl::FailedPreconditionError(
            "Query result contains a list of lists, invalid structure");
      default:
        ECCLESIA_RETURN_IF_ERROR(CompareQueryValues(
            *list_item_a, *list_item_b,
            verification.verify().verify().comparison(), errors, options));
    }
  }

  return absl::OkStatus();
}

absl::Status CompareSubqueryValues(
    const QueryResultData& value_a, const QueryResultData& value_b,
    const QueryResultDataVerification& verification,
    std::vector<std::string>& errors, const VerificationOptions& options) {
  const google::protobuf::Map<std::string, QueryValue>& fields_a = value_a.fields();
  const google::protobuf::Map<std::string, QueryValue>& fields_b = value_b.fields();

  for (const auto& [property, operations] : verification.fields()) {
    auto a_it = fields_a.find(property);
    auto b_it = fields_b.find(property);
    if (a_it == fields_a.end() && b_it == fields_b.end()) {
      // If the property is not present in both, then there is no need to
      // compare or report an error.
      continue;
    }
    if (a_it == fields_a.end()) {
      errors.push_back(
          absl::StrCat("Missing property ", property, " in ", options.label_a));
      continue;
    }
    if (b_it == fields_b.end()) {
      errors.push_back(
          absl::StrCat("Missing property ", property, " in ", options.label_b));
      continue;
    }

    switch (a_it->second.kind_case()) {
      case QueryValue::kSubqueryValue:
        ECCLESIA_RETURN_IF_ERROR(CompareSubqueryValues(
            a_it->second.subquery_value(), b_it->second.subquery_value(),
            operations.data_compare(), errors, options));
        break;
      case QueryValue::kListValue:
        ECCLESIA_RETURN_IF_ERROR(CompareListValues(
            a_it->second.list_value(), b_it->second.list_value(),
            operations.list_compare(), errors, options));
        break;
      default:
        ECCLESIA_RETURN_IF_ERROR(CompareQueryValues(
            a_it->second, b_it->second, operations.verify().comparison(),
            errors, options));
    }
  }

  return absl::OkStatus();
}

absl::Status CompareQueryResults(const QueryResult& query_result_a,
                                 const QueryResult& query_result_b,
                                 const QueryResultVerification& verification,
                                 std::vector<std::string>& errors,
                                 const VerificationOptions& options) {
  if (query_result_a.query_id() != query_result_b.query_id()) {
    return absl::FailedPreconditionError(absl::StrCat(
        "Query results have different query IDs: ", query_result_a.query_id(),
        "(", options.label_a, ") vs ", query_result_b.query_id(), "(",
        options.label_b, ")"));
  }

  if (query_result_a.query_id() != verification.query_id()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Query result has query ID ", query_result_a.query_id(),
                     " which does not match the verification query "
                     "ID ",
                     verification.query_id()));
  }

  return CompareSubqueryValues(query_result_a.data(), query_result_b.data(),
                               verification.data_verify(), errors, options);
}

absl::Status VerifyQueryValue(const QueryValue& value,
                              const QueryValueVerification& verification,
                              std::vector<std::string>& errors,
                              const VerificationOptions& options) {
  if (!verification.has_verify()) {
    return absl::InvalidArgumentError(
        "Query value verification must have a verify field");
  }

  // Presence checking is handled by the caller. This function can only verify
  // properties that are present in the query value.
  if (!verification.verify().has_validation()) {
    return absl::OkStatus();
  }

  if (verification.verify().validation().has_operation()) {
    if (verification.verify().validation().operands().empty()) {
      return absl::InternalError(
          "Query value verification must have at least one operand");
    }

    return OperationQueryValue(
        value, verification.verify().validation().operands(0),
        verification.verify().validation().operation(), errors);
  }

  if (verification.verify().validation().has_range()) {
    return Range(value, verification.verify().validation().operands(),
                 verification.verify().validation().range(), errors);
  }

  if (verification.verify().validation().has_interval()) {
    return Interval(value, verification.verify().validation().operands(),
                    verification.verify().validation().interval(), errors);
  }

  return absl::OkStatus();
}

absl::Status VerifyListValue(const QueryValue& value,
                             const ListValueVerification& verification,
                             std::vector<std::string>& errors,
                             const VerificationOptions& options) {
  return absl::UnimplementedError("Not implemented");
}

absl::Status VerifySubqueryValue(
    const QueryValue& value, const QueryResultDataVerification& verification,
    std::vector<std::string>& errors, const VerificationOptions& options) {
  return absl::UnimplementedError("Not implemented");
}

absl::Status VerifyQueryResult(const QueryResult& query_result,
                               const QueryResultVerification& verification,
                               std::vector<std::string>& errors,
                               const VerificationOptions& options) {
  return absl::UnimplementedError("Not implemented");
}

}  // namespace ecclesia