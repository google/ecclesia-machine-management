/*
 * Copyright 2023 Google LLC
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

#include "ecclesia/lib/redfish/redpath/definitions/query_result/converter.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "google/rpc/code.pb.h"
#include "google/rpc/status.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "single_include/nlohmann/json.hpp"
#include "re2/re2.h"

namespace ecclesia {

namespace {

constexpr char kLocalDevpathTag[] = "_local_devpath_";
constexpr char kMachineDevpathTag[] = "_machine_devpath_";
constexpr char kEmbeddedLocationContextTag[] = "_embedded_location_context_";

absl::Status Validate(const google::protobuf::Timestamp& t) {
  const auto sec = t.seconds();
  const auto ns = t.nanos();
  if (sec < -62135596800 || sec > 253402300799) {
    return absl::InvalidArgumentError(absl::StrCat("seconds=", sec));
  }
  if (ns < 0 || ns > 999999999) {
    return absl::InvalidArgumentError(absl::StrCat("nanos=", ns));
  }
  return absl::OkStatus();
}

absl::StatusOr<absl::Time> DecodeGoogleApiProto(
    const google::protobuf::Timestamp& proto) {
  absl::Status status = Validate(proto);
  if (!status.ok()) return status;
  return absl::FromUnixSeconds(proto.seconds()) +
         absl::Nanoseconds(proto.nanos());
}

absl::Status EncodeGoogleApiProto(absl::Time t,
                                  google::protobuf::Timestamp* proto) {
  const int64_t s = absl::ToUnixSeconds(t);
  proto->set_seconds(s);
  proto->set_nanos(static_cast<int32_t>((t - absl::FromUnixSeconds(s)) /
                                        absl::Nanoseconds(1)));
  return Validate(*proto);
}

absl::StatusOr<google::protobuf::Timestamp> EncodeGoogleApiProto(absl::Time t) {
  google::protobuf::Timestamp proto;
  absl::Status status = EncodeGoogleApiProto(t, &proto);
  if (!status.ok()) return status;
  return proto;
}

void AddChildSubQuery(QueryResultDataBuilder& builder,
                      absl::string_view subquery_id,
                      const ::ecclesia::SubqueryOutput& subquery_output) {
  QueryValueBuilder subquery_builder = builder[subquery_id];
  for (const auto& data_set : subquery_output.data_sets()) {
    if (data_set.has_raw_data()) {
      subquery_builder = data_set.raw_data();
      continue;
    }
    QueryValueBuilder value_builder = subquery_builder.append();
    Identifier identifier;
    if (data_set.has_devpath() && !data_set.devpath().empty()) {
      identifier.set_local_devpath(data_set.devpath());
    }
    if (data_set.has_decorators()) {
      if (data_set.decorators().has_machine_devpath() &&
          !data_set.decorators().machine_devpath().empty()) {
        identifier.set_machine_devpath(data_set.decorators().machine_devpath());
      }
      if (data_set.decorators().has_embedded_location_context() &&
          !data_set.decorators().embedded_location_context().empty()) {
        identifier.set_embedded_location_context(
            data_set.decorators().embedded_location_context());
      }
    }
    if (identifier.has_local_devpath() || identifier.has_machine_devpath() ||
        identifier.has_embedded_location_context()) {
      value_builder[kIdentifierTag] = std::move(identifier);
    }

    if (data_set.has_uri_annotation()) {
      value_builder[kUriAnnotationTag] = data_set.uri_annotation();
    }

    for (const ecclesia::SubqueryDataSet::Property& property :
         data_set.properties()) {
      if (property.has_string_value()) {
        value_builder[property.name()] = property.string_value();
      } else if (property.has_int64_value()) {
        value_builder[property.name()] = property.int64_value();
      } else if (property.has_double_value()) {
        value_builder[property.name()] = property.double_value();
      } else if (property.has_boolean_value()) {
        value_builder[property.name()] = property.boolean_value();
      } else if (property.has_timestamp_value()) {
        value_builder[property.name()] = property.timestamp_value();
      } else if (property.has_collection_value()) {
        QueryValueBuilder list_value_builder = value_builder[property.name()];
        for (const auto& value : property.collection_value().values()) {
          if (value.has_string_value()) {
            list_value_builder.append(value.string_value());
          } else if (value.has_int64_value()) {
            list_value_builder.append(value.int64_value());
          } else if (value.has_double_value()) {
            list_value_builder.append(value.double_value());
          } else if (value.has_boolean_value()) {
            list_value_builder.append(value.boolean_value());
          } else if (value.has_timestamp_value()) {
            list_value_builder.append(value.timestamp_value());
          }
        }
      }
    }
    for (const auto& [child_subquery_id, child_subquery_output] :
         data_set.child_subquery_output_by_id()) {
      QueryResultData child_subquery_value;
      QueryResultDataBuilder child_subquery_builder(&child_subquery_value);
      AddChildSubQuery(child_subquery_builder, child_subquery_id,
                       child_subquery_output);
      // Adds the child subquery's fields.
      value_builder[child_subquery_id] =
          child_subquery_value.fields().at(child_subquery_id);
    }
  }
}

void AddSubQuery(
    QueryResultDataBuilder& builder,
    const google::protobuf::Map<std::string, ::ecclesia::SubqueryOutput>& subquery_in) {
  for (const auto& [subquery_id, subquery_output] : subquery_in) {
    AddChildSubQuery(builder, subquery_id, subquery_output);
  }
}

absl::StatusOr<google::protobuf::Timestamp> TimestampStrToTimestamp(
    absl::string_view value) {
  static constexpr LazyRE2 kRedfishDatetimeRegex = {
      R"(^(?:[1-9]\d{3}-(?:(?:0[1-9]|1[0-2])-(?:0[1-9]|1\d|2[0-8])|(?:0[13-9]|1[0-2])-(?:29|30)|(?:0[13578]|1[02])-31)|(?:[1-9]\d(?:0[48]|[2468][048]|[13579][26])|(?:[2468][048]|[13579][26])00)-02-29)T(?:[01]\d|2[0-3]):[0-5]\d:[0-5]\d(?:\.\d{1,9})?(?:Z|[+-][01]\d:[0-5]\d)?$)"};

  static constexpr absl::string_view kRedfishDatetimePlusOffset =
      "%Y-%m-%dT%H:%M:%E6S%Ez";
  static constexpr absl::string_view kRedfishDatetimeNoOffset =
      "%Y-%m-%dT%H:%M:%E6S";

  if (RE2::FullMatch(value, *kRedfishDatetimeRegex)) {
    absl::Time result;
    std::string errors;
    if (absl::ParseTime(kRedfishDatetimePlusOffset, value, &result, &errors)) {
      return EncodeGoogleApiProto(result);
    }
    if (absl::ParseTime(kRedfishDatetimeNoOffset, value, &result, &errors)) {
      return EncodeGoogleApiProto(result);
    }
  }
  return absl::InternalError("Unable to convert to timestamp value");
}

// Takes a google rpc code as an int, as from a DelliciusQueryResult Status,
// and converts it to a QueryResultStatus.
ErrorCode RpcCodeToQueryResultErrorCode(int code) {
  google::rpc::Code rpc_code = static_cast<google::rpc::Code>(code);
  switch (rpc_code) {
    case google::rpc::OK:
    case google::rpc::NOT_FOUND:
      return ErrorCode::ERROR_NONE;
    case google::rpc::FAILED_PRECONDITION:
      return ErrorCode::ERROR_SERVICE_ROOT_UNREACHABLE;
    case google::rpc::DEADLINE_EXCEEDED:
      return ErrorCode::ERROR_NETWORK;
    case google::rpc::UNAUTHENTICATED:
      return ErrorCode::ERROR_UNAUTHENTICATED;
    case google::rpc::UNAVAILABLE:
      return ErrorCode::ERROR_UNAVAILABLE;
    default:
      return ErrorCode::ERROR_INTERNAL;
  }
}

}  // namespace

QueryResult ToQueryResult(const ecclesia::DelliciusQueryResult& result_in) {
  QueryResult result;
  result.set_query_id(result_in.query_id());
  if (!result_in.status().message().empty()) {
    result.mutable_status()->set_error_code(
        RpcCodeToQueryResultErrorCode(result_in.status().code()));
    result.mutable_status()->add_errors(result_in.status().message());
  }
  if (result_in.has_start_timestamp()) {
    *result.mutable_stats()->mutable_start_time() = result_in.start_timestamp();
  }
  if (result_in.has_end_timestamp()) {
    *result.mutable_stats()->mutable_end_time() = result_in.end_timestamp();
  }
  if (result_in.has_redfish_metrics()) {
    *result.mutable_stats()->mutable_redfish_metrics() =
        result_in.redfish_metrics();
  }
  QueryResultData data;
  QueryResultDataBuilder builder(&data);
  AddSubQuery(builder, result_in.subquery_output_by_id());
  *result.mutable_data() = std::move(data);
  return result;
}

nlohmann::json QueryResultDataToJson(const QueryResultData& query_result) {
  nlohmann::json json = nlohmann::json::object();
  for (const auto& [key, value] : query_result.fields()) {
    json[key] = ValueToJson(value);
  }
  return json;
}

nlohmann::json ListValueToJson(const ListValue& value) {
  nlohmann::json json = nlohmann::json::array();
  for (const auto& item : value.values()) {
    json.push_back(ValueToJson(item));
  }
  return json;
}

nlohmann::json IdentifierValueToJson(const Identifier& value) {
  nlohmann::json json = nlohmann::json::object();
  if (value.has_local_devpath() && !value.local_devpath().empty()) {
    json[kLocalDevpathTag] = value.local_devpath();
  }
  if (value.has_machine_devpath() && !value.machine_devpath().empty()) {
    json[kMachineDevpathTag] = value.machine_devpath();
  }
  if (value.has_embedded_location_context() &&
      !value.embedded_location_context().empty()) {
    json[kEmbeddedLocationContextTag] = value.embedded_location_context();
  }
  return json;
}

nlohmann::json ValueToJson(const QueryValue& value) {
  nlohmann::json json;
  switch (value.kind_case()) {
    case QueryValue::kIntValue:
      json = value.int_value();
      break;
    case QueryValue::kDoubleValue:
      json = value.double_value();
      break;
    case QueryValue::kStringValue:
      json = value.string_value();
      break;
    case QueryValue::kBoolValue:
      json = value.bool_value();
      break;
    case QueryValue::kTimestampValue:
      if (auto ts = DecodeGoogleApiProto(value.timestamp_value());
          ts.ok()) {
        json = absl::FormatTime(*ts);
      } else {
        json = nullptr;
      }
      break;
    case QueryValue::kSubqueryValue:
      json = QueryResultDataToJson(value.subquery_value());
      break;
    case QueryValue::kListValue:
      json = ListValueToJson(value.list_value());
      break;
    case QueryValue::kIdentifier:
      json = IdentifierValueToJson(value.identifier());
      break;
    case QueryValue::kRawData: {
      const QueryValue::RawData& raw_data = value.raw_data();
      if (raw_data.has_raw_string_value()) {
        json = raw_data.raw_string_value();
      } else {
        // The raw bytes value might have null terminators so we need to
        // explicitly construct the string using the full length, and ensure it
        // is base64 encoded.
        json =
            absl::Base64Escape(std::string(raw_data.raw_bytes_value().begin(),
                                           raw_data.raw_bytes_value().end()));
      }
      break;
    }
    case QueryValue::KIND_NOT_SET:
      json = nlohmann::json::object();
      break;
  }
  return json;
}

QueryResultData JsonToQueryResultData(const nlohmann::json& json) {
  QueryResultData result;
  if (json.is_object()) {
    auto& fields = *result.mutable_fields();
    for (const auto& [key, value] : json.items()) {
      fields[key] = JsonToQueryValue(value);
    }
  }
  return result;
}

QueryValue JsonToQueryValue(const nlohmann::json& json) {
  QueryValue value;
  if (json.is_null()) {
    value.set_string_value("null");
  } else if (json.is_number_integer()) {
    value.set_int_value(json.get<int64_t>());
  } else if (json.is_number_float()) {
    value.set_double_value(json.get<double>());
  } else if (json.is_string()) {
    std::string str_value = json.get<std::string>();
    if (auto timestamp = TimestampStrToTimestamp(str_value); timestamp.ok()) {
      *value.mutable_timestamp_value() = std::move(*timestamp);
    } else {
      value.set_string_value(std::move(str_value));
    }
  } else if (json.is_boolean()) {
    value.set_bool_value(json.get<bool>());
  } else if (json.is_object()) {
    if (auto identifier = JsonToIdentifierValue(json); identifier.has_value()) {
      *value.mutable_identifier() = std::move(*identifier);
    } else {
      *value.mutable_subquery_value() = JsonToQueryResultData(json);
    }
  } else if (json.is_array()) {
    *value.mutable_list_value() = JsonToQueryListValue(json);
  }
  return value;
}

ListValue JsonToQueryListValue(const nlohmann::json& json) {
  ListValue value;
  if (json.is_array()) {
    for (const nlohmann::json& item : json) {
      *value.add_values() = JsonToQueryValue(item);
    }
  }
  return value;
}

std::optional<Identifier> JsonToIdentifierValue(const nlohmann::json& json) {
  if (json.is_object()) {
    Identifier id;
    for (const auto& [key, value] : json.items()) {
      if (key == kLocalDevpathTag) {
        id.set_local_devpath(value.get<std::string>());
      } else if (key == kMachineDevpathTag) {
        id.set_machine_devpath(value.get<std::string>());
      } else if (key == kEmbeddedLocationContextTag) {
        id.set_embedded_location_context(value.get<std::string>());
      }
    }
    if (id.has_local_devpath() || id.has_machine_devpath() ||
        id.has_embedded_location_context()) {
      return id;
    }
  }
  return std::nullopt;
}

}  // namespace ecclesia
