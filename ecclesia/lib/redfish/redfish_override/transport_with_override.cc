/*
 * Copyright 2022 Google LLC
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

#include "ecclesia/lib/redfish/redfish_override/transport_with_override.h"

#include <fstream>
#include <string>

#include "google/protobuf/text_format.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/redfish/redfish_override/rf_override.pb.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/struct_proto_conversion.h"
#include "single_include/nlohmann/json.hpp"
#include "re2/re2.h"

namespace ecclesia {
namespace {
using OverrideValue = OverrideField::OverrideValue;
using IndividualObjectIdentifier = ObjectIdentifier::IndividualObjectIdentifier;

absl::Status GetBinaryProto(absl::string_view path, google::protobuf::Message *message) {
  std::ifstream ifs((std::string(path)));
  if (!ifs) {
    return absl::InvalidArgumentError(
        absl::StrCat("File doesn't exist: ", path));
  }
  if (auto parse_status = message->ParseFromIstream(&ifs); !parse_status) {
    return absl::InvalidArgumentError(
        absl::StrCat("Parse binary file to proto failed: ", path));
  }
  return absl::OkStatus();
}

bool CheckValue(nlohmann::json &json,
                const OverrideRequirement::Requirement &requirement, int idx) {
  auto object_identifier = requirement.object_identifier();
  if (idx == object_identifier.individual_object_identifier_size()) {
    for (const auto &value : requirement.value()) {
      if (ValueToJson(value) == json) {
        return true;
      }
    }
    return false;
  }
  if (object_identifier.individual_object_identifier()
          .Get(idx)
          .has_field_name()) {
    if (json.find(object_identifier.individual_object_identifier()
                      .Get(idx)
                      .field_name()) != json.end()) {
      return CheckValue(json[object_identifier.individual_object_identifier()
                                 .Get(idx)
                                 .field_name()],
                        requirement, idx + 1);
    }
  } else if (object_identifier.individual_object_identifier()
                 .Get(idx)
                 .has_array_field()) {
    if (!json.is_array()) {
      return false;
    }
    for (auto &iter : json) {
      if (iter.contains(object_identifier.individual_object_identifier()
                            .Get(idx)
                            .array_field()
                            .field_name()) &&
          iter[object_identifier.individual_object_identifier()
                   .Get(idx)
                   .array_field()
                   .field_name()] ==
              ValueToJson(object_identifier.individual_object_identifier()
                              .Get(idx)
                              .array_field()
                              .value())) {
        return CheckValue(iter, requirement, idx + 1);
      }
    }
  } else if (object_identifier.individual_object_identifier()
                 .Get(idx)
                 .has_array_idx()) {
    if (!json.is_array()) {
      return false;
    }
    if (object_identifier.individual_object_identifier().Get(idx).array_idx() >=
        json.size()) {
      return false;
    }
    return CheckValue(json[object_identifier.individual_object_identifier()
                               .Get(idx)
                               .array_idx()],
                      requirement, idx + 1);
  }
  return false;
}

bool CheckRequirement(const OverrideRequirement &override_requirement,
                      RedfishTransport *transport) {
  for (const auto &requirement : override_requirement.requirement()) {
    auto get_result = transport->Get(requirement.uri());
    if (!get_result.ok()) {
      return false;
    }
    if (!CheckValue((*get_result).body, requirement, 0)) {
      return false;
    }
  }
  return true;
}

absl::Status JsonReplace(nlohmann::json &json,
                         const OverrideValue &override_value,
                         RedfishTransport *transport) {
  if (override_value.has_value()) {
    nlohmann::json replace_json = ValueToJson(override_value.value());
    if (json.type() != replace_json.type()) {
      return absl::InvalidArgumentError(
          "Value type is different from original type");
    }
    json = replace_json;
    return absl::OkStatus();
  } else if (override_value.has_override_by_reading()) {
    return absl::UnimplementedError("To be implemented");
  }
  return absl::InvalidArgumentError("Replace Json not found.");
}

absl::Status JsonAdd(nlohmann::json &json,
                     const IndividualObjectIdentifier &object_identifier,
                     const OverrideValue &override_value,
                     RedfishTransport *transport) {
  nlohmann::json add_json;
  if (override_value.has_value()) {
    add_json = ValueToJson(override_value.value());
  } else if (override_value.has_override_by_reading()) {
    return absl::UnimplementedError("To be implemented");
  } else {
    return absl::InvalidArgumentError("Added Json not found.");
  }
  if (object_identifier.has_field_name()) {
    if (json.find(object_identifier.field_name()) != json.end()) {
      if (json[object_identifier.field_name()].is_array()) {
        json[object_identifier.field_name()].push_back(add_json);
        return absl::OkStatus();
      }
      return absl::InvalidArgumentError("Json field already exist.");
    }
    json[object_identifier.field_name()] = add_json;
    return absl::OkStatus();
  } else if (object_identifier.has_array_field()) {
    return absl::InvalidArgumentError(
        "Please specify a field before adding inside an array type "
        "json");
  } else if (object_identifier.has_array_idx()) {
    if (!json.is_array()) {
      return absl::InvalidArgumentError("Json is not an array type");
    }
    if (object_identifier.array_idx() >= json.size()) {
      return absl::InvalidArgumentError("index is out of array range");
    }
    if (!json[object_identifier.array_idx()].is_array()) {
      return absl::InvalidArgumentError(
          "Please specify a field before adding inside an array type "
          "json");
    }
    json[object_identifier.array_idx()].push_back(add_json);
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError("Added Json not found.");
}

absl::Status JsonClear(nlohmann::json &json,
                       const IndividualObjectIdentifier &object_identifier) {
  if (object_identifier.has_field_name()) {
    json.erase(json.find(object_identifier.field_name()));
    return absl::OkStatus();
  } else if (object_identifier.has_array_field()) {
    if (!json.is_array()) {
      return absl::InvalidArgumentError("Json is not an array type");
    }
    for (int i = 0; i < json.size(); i++) {
      if (json[i].contains(object_identifier.array_field().field_name()) &&
          json[i][object_identifier.array_field().field_name()] ==
              ValueToJson(object_identifier.array_field().value())) {
        json.erase(i);
        return absl::OkStatus();
      }
    }
  } else if (object_identifier.has_array_idx()) {
    if (!json.is_array()) {
      return absl::InvalidArgumentError("Json is not an array type");
    }
    if (object_identifier.array_idx() >= json.size()) {
      return absl::InvalidArgumentError("index is out of array range");
    }
    json.erase(object_identifier.array_idx());
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError("Cleared Json not found.");
}

// Recursively find the target sub-JSON object in a JSON object till, then call
// the action functions (JsonReplace, JsonAdd, JsonClear). The idx is the index
// to do recursive find with object_identifier
absl::Status FindObjectAndAct(nlohmann::json &json,
                              const ObjectIdentifier &object_identifier,
                              int idx, const OverrideValue &override_value,
                              ecclesia::RedfishTransport *transport,
                              OverrideField::ActionCase action) {
  if (action == OverrideField::ActionCase::kActionReplace &&
      idx == object_identifier.individual_object_identifier_size()) {
    return JsonReplace(json, override_value, transport);
  }
  if (action == OverrideField::ActionCase::kActionAdd &&
      idx == object_identifier.individual_object_identifier_size() - 1) {
    return JsonAdd(json,
                   object_identifier.individual_object_identifier().Get(idx),
                   override_value, transport);
  }
  if (action == OverrideField::ActionCase::kActionClear &&
      idx == object_identifier.individual_object_identifier_size() - 1) {
    return JsonClear(json,
                     object_identifier.individual_object_identifier().Get(idx));
  }
  if (object_identifier.individual_object_identifier()
          .Get(idx)
          .has_field_name()) {
    if (json.find(object_identifier.individual_object_identifier()
                      .Get(idx)
                      .field_name()) != json.end()) {
      return FindObjectAndAct(
          json[object_identifier.individual_object_identifier()
                   .Get(idx)
                   .field_name()],
          object_identifier, idx + 1, override_value, transport, action);
    }
  } else if (object_identifier.individual_object_identifier()
                 .Get(idx)
                 .has_array_field()) {
    if (!json.is_array()) {
      return absl::InvalidArgumentError("Json is not an array type");
    }
    for (auto &iter : json) {
      if (iter.contains(object_identifier.individual_object_identifier()
                            .Get(idx)
                            .array_field()
                            .field_name()) &&
          iter[object_identifier.individual_object_identifier()
                   .Get(idx)
                   .array_field()
                   .field_name()] ==
              ValueToJson(object_identifier.individual_object_identifier()
                              .Get(idx)
                              .array_field()
                              .value())) {
        return FindObjectAndAct(iter, object_identifier, idx + 1,
                                override_value, transport, action);
      }
    }
  } else if (object_identifier.individual_object_identifier()
                 .Get(idx)
                 .has_array_idx()) {
    if (!json.is_array()) {
      return absl::InvalidArgumentError("Json is not an array type");
    }
    if (object_identifier.individual_object_identifier().Get(idx).array_idx() >=
        json.size()) {
      return absl::InvalidArgumentError("index is out of array range");
    }
    return FindObjectAndAct(
        json[object_identifier.individual_object_identifier()
                 .Get(idx)
                 .array_idx()],
        object_identifier, idx + 1, override_value, transport, action);
  }
  return absl::InvalidArgumentError("Required Json not found.");
}
// Updating the result by the specific OverrideField
absl::Status ResultUpdateHelper(const OverrideField &field,
                                RedfishTransport::Result &result,
                                RedfishTransport *transport) {
  switch (field.Action_case()) {
    case OverrideField::kActionReplace: {
      auto result_check = FindObjectAndAct(
          result.body, field.action_replace().object_identifier(), 0,
          field.action_replace().override_value(), transport,
          OverrideField::ActionCase::kActionReplace);
      if (!result_check.ok()) {
        return result_check;
      }
      break;
    }
    case OverrideField::kActionAdd: {
      auto result_check =
          FindObjectAndAct(result.body, field.action_add().object_identifier(),
                           0, field.action_add().override_value(), transport,
                           OverrideField::ActionCase::kActionAdd);
      if (!result_check.ok()) {
        return result_check;
      }
      break;
    }
    case OverrideField::kActionClear: {
      auto result_check = FindObjectAndAct(
          result.body, field.action_clear().object_identifier(), 0,
          field.action_add().override_value(), transport,
          OverrideField::ActionCase::kActionClear);
      if (!result_check.ok()) {
        return result_check;
      }
      break;
    }
    default:
      return absl::InvalidArgumentError("No action specified");
  }
  return absl::OkStatus();
}
}  // namespace
OverridePolicy LoadOverridePolicy(absl::string_view policy_selector_path,
                                  RedfishTransport *transport) {
  OverridePolicy policy;
  OverridePolicySelector selector;
  absl::Status read_selector = GetBinaryProto(policy_selector_path, &selector);
  if (!read_selector.ok()) {
    WarningLog() << "Read selector file failed: " << read_selector.message();
    return policy;
  }
  std::string policy_file_path;
  for (const auto &policy_selector : selector.policy_selector()) {
    if (CheckRequirement(policy_selector.second, transport)) {
      auto pos = policy_selector_path.find_last_of('/');
      if (pos == std::string::npos) {
        continue;
      }
      policy_file_path = absl::StrCat(policy_selector_path.substr(0, pos), "/",
                                      policy_selector.first);
    }
  }
  if (policy_file_path.empty()) {
    WarningLog() << "No matching policy";
    return policy;
  }
  absl::Status read_policy = GetBinaryProto(policy_file_path, &policy);
  if (!read_policy.ok()) {
    WarningLog() << "Read policy file failed: " << read_policy.message();
    return policy;
  }
  return policy;
}

RedfishTransportWithOverride::RedfishTransportWithOverride(
    std::unique_ptr<RedfishTransport> redfish_transport,
    absl::string_view policy_selector_path)
    : redfish_transport_(std::move(redfish_transport)),
      override_policy_(
          LoadOverridePolicy(policy_selector_path, redfish_transport_.get())) {}

absl::StatusOr<RedfishTransport::Result> RedfishTransportWithOverride::Get(
    absl::string_view path) {
  auto get_result = redfish_transport_->Get(path);
  if (!get_result.ok()) {
    return get_result;
  }
  absl::MutexLock mu(&policy_lock_);
  auto iter = override_policy_.override_content_map_uri().find(path);
  if (iter != override_policy_.override_content_map_uri().end()) {
    for (const auto &field : iter->second.override_field()) {
      auto update_status =
          ResultUpdateHelper(field, *get_result, redfish_transport_.get());
      if (!update_status.ok()) {
        WarningLog() << absl::StrFormat(
            "Failed to perform override to uri: %s, failure: %s.", path,
            update_status.message());
      }
    }
  }
  for (const auto &[uri_regex, override_content] :
       override_policy_.override_content_map_regex()) {
    if (!RE2::FullMatch(path, uri_regex)) {
      continue;
    }
    for (const auto &field : override_content.override_field()) {
      auto update_status =
          ResultUpdateHelper(field, *get_result, redfish_transport_.get());
      if (!update_status.ok()) {
        WarningLog() << absl::StrFormat(
            "Failed to perform override to uri: %s, failure: %s.", path,
            update_status.message());
      }
    }
  }
  return *get_result;
}

}  // namespace ecclesia
