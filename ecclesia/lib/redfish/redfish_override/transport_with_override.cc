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
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1_grpc_include.h"
#include "ecclesia/lib/redfish/redfish_override/rf_override.pb.h"
#include "ecclesia/lib/redfish/transport/grpc.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/struct_proto_conversion.h"
#include "grpc/grpc_security_constants.h"
#include "grpcpp/client_context.h"
#include "grpcpp/create_channel.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "re2/re2.h"

namespace ecclesia {
namespace {
using OverrideValue = OverrideField::OverrideValue;
using IndividualObjectIdentifier = ObjectIdentifier::IndividualObjectIdentifier;

constexpr absl::string_view kTargetKey = "target";
constexpr absl::string_view kResourceKey = "redfish-resource";

// This is the Grpc credentials specific for getting override policy through
// gRPC Redfish BMC.
// The metadata actually does nothing but using gRPC RPCs it is required. We're
// adding some reasonable dummy datas in this credential.
class GrpcCredentialsForOverride : public grpc::MetadataCredentialsPlugin {
 public:
  explicit GrpcCredentialsForOverride(absl::string_view target_fqdn)
      : target_fqdn_(target_fqdn) {}
  // Sends out the target server and the Redfish resource as part of
  // gRPC credentials.
  grpc::Status GetMetadata(
      grpc::string_ref /*service_url*/, grpc::string_ref /*method_name*/,
      const grpc::AuthContext & /*channel_auth_context*/,
      std::multimap<grpc::string, grpc::string> *metadata) override {
    metadata->insert(std::make_pair(kTargetKey, target_fqdn_));
    metadata->insert(std::make_pair(kResourceKey, "/redfish/v1"));
    return grpc::Status::OK;
  }

 private:
  std::string target_fqdn_;
};

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
  }
  if (override_value.has_override_by_reading()) {
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
  }
  if (object_identifier.has_array_field()) {
    return absl::InvalidArgumentError(
        "Please specify a field before adding inside an array type "
        "json");
  }
  if (object_identifier.has_array_idx()) {
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
  }
  if (object_identifier.has_array_field()) {
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
  if (!std::holds_alternative<nlohmann::json>(result.body)) {
    return absl::InternalError("Result body is not storing JSON");
  }
  auto &json = std::get<nlohmann::json>(result.body);
  switch (field.Action_case()) {
    case OverrideField::kActionReplace: {
      auto result_check =
          FindObjectAndAct(json, field.action_replace().object_identifier(), 0,
                           field.action_replace().override_value(), transport,
                           OverrideField::ActionCase::kActionReplace);
      if (!result_check.ok()) {
        return result_check;
      }
      break;
    }
    case OverrideField::kActionAdd: {
      auto result_check =
          FindObjectAndAct(json, field.action_add().object_identifier(), 0,
                           field.action_add().override_value(), transport,
                           OverrideField::ActionCase::kActionAdd);
      if (!result_check.ok()) {
        return result_check;
      }
      break;
    }
    case OverrideField::kActionClear: {
      auto result_check =
          FindObjectAndAct(json, field.action_clear().object_identifier(), 0,
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

OverridePolicy GetOverridePolicy(absl::string_view policy_file_path) {
  OverridePolicy policy;
  absl::Status read_policy = GetBinaryProto(policy_file_path, &policy);
  if (!read_policy.ok()) {
    LOG(WARNING) << "Read policy file failed: " << read_policy.message();
    return OverridePolicy::default_instance();
  }
  return policy;
}

OverridePolicy GetOverridePolicy(
    absl::string_view hostname, std::optional<int> port,
    const std::shared_ptr<grpc::ChannelCredentials> &creds) {
  OverridePolicy policy;
  std::string service_address(
      port.has_value() ? absl::StrCat(hostname, ":", *port) : hostname);
  auto client = GrpcRedfishV1::NewStub(
      grpc::CreateChannel(service_address, creds));
  if (client == nullptr) {
    LOG(WARNING) << "Override Stub creation failed";
    return OverridePolicy::default_instance();
  }
  grpc::ClientContext context;
  redfish::v1::GetOverridePolicyRequest request;
  redfish::v1::GetOverridePolicyResponse response;
  GrpcTransportParams params;

  context.set_deadline(ToChronoTime(params.clock->Now() + params.timeout));
  context.set_credentials(grpc::experimental::MetadataCredentialsFromPlugin(
      std::unique_ptr<grpc::MetadataCredentialsPlugin>(
          std::make_unique<GrpcCredentialsForOverride>(service_address)),
      GRPC_SECURITY_NONE));

  auto status = client->GetOverridePolicy(&context, request, &response);
  if (!status.ok()) {
    LOG(WARNING) << "GetOverridePolicy failed: " << status.error_message();
    return OverridePolicy::default_instance();
  }
  bool result = google::protobuf::TextFormat::ParseFromString(response.policy(), &policy);
  if (!result) {
    LOG(WARNING) << "Byte is unable to translate to proto "
                 << response.policy();
    return OverridePolicy::default_instance();
  }
  return policy;
}


absl::StatusOr<RedfishTransport::Result> RedfishTransportWithOverride::Get(
    absl::string_view path) {
  auto get_result = redfish_transport_->Get(path);
  if (!get_result.ok()) {
    return get_result;
  }
  std::string checked_path = std::string(path);
  auto extend_pos = path.find_first_of('?');
  if (extend_pos != std::string::npos) {
    checked_path = path.substr(0, extend_pos);
  }
  extend_pos = path.find_first_of('#');
  if (extend_pos != std::string::npos) {
    checked_path = path.substr(0, extend_pos);
  }
  auto iter = override_policy_.override_content_map_uri().find(checked_path);
  if (iter != override_policy_.override_content_map_uri().end()) {
    for (const auto &field : iter->second.override_field()) {
      if (field.has_apply_condition() && field.apply_condition().is_expand() &&
          !absl::StrContains(path, "$expand=")) {
        continue;
      }
      auto update_status =
          ResultUpdateHelper(field, *get_result, redfish_transport_.get());
      if (!update_status.ok()) {
        LOG(WARNING) << absl::StrFormat(
            "Failed to perform override to uri: %s, failure: %s.", path,
            update_status.message());
      }
    }
  }
  for (const auto &[uri_regex, override_content] :
       override_policy_.override_content_map_regex()) {
    if (!RE2::FullMatch(checked_path, uri_regex)) {
      continue;
    }
    for (const auto &field : override_content.override_field()) {
      if (field.has_apply_condition() && field.apply_condition().is_expand() &&
          !absl::StrContains(path, "$expand=")) {
        continue;
      }
      auto update_status =
          ResultUpdateHelper(field, *get_result, redfish_transport_.get());
      if (!update_status.ok()) {
        LOG(WARNING) << absl::StrFormat(
            "Failed to perform override to uri: %s, failure: %s.", path,
            update_status.message());
      }
    }
  }
  return *get_result;
}

}  // namespace ecclesia
