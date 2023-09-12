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

#include "ecclesia/lib/redfish/health_rollup.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/health_rollup.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/time/proto.h"

constexpr char kCritical[] = "Critical";
constexpr char kWarning[] = "Warning";
constexpr char kResourceEventRegistry[] = "ResourceEvent";
constexpr char kResourceErrorsDetected[] = "ResourceErrorsDetected";
constexpr char kResourceStateChange[] = "ResourceStateChanged";
constexpr char kResourceErrorThresholdExceeded[] =
    "ResourceErrorThresholdExceeded";

namespace ecclesia {
namespace {

// Message Registry and Type, obtained from Status.Conditions, used for
// determining which message type to populate in the oneof in HealthRollup.
struct MessageRegistryAndType {
  const std::string message_registry;
  const std::string message_type;
};

std::optional<MessageRegistryAndType> GetMessageRegistryAndTypeForCondition(
    const RedfishObject &condition_obj) {
  std::optional<std::string> message_id =
      condition_obj.GetNodeValue<PropertyMessageId>();
  if (!message_id.has_value()) return std::nullopt;
  // MessageId should be of format
  // "<Registry>.<major_version>.<minor_version>.<Type>"
  std::vector<std::string> type_parts = absl::StrSplit(*message_id, '.');
  if (type_parts.size() != 4) {
    LOG(ERROR) << "Message registry poorly formatted: " << *message_id;
    return std::nullopt;
  }
  return MessageRegistryAndType{.message_registry = type_parts[0],
                                .message_type = type_parts[3]};
}

absl::StatusOr<std::string> GetSeverityForCondition(
    const RedfishObject &condition_obj) {
  std::optional<std::string> severity =
      condition_obj.GetNodeValue<PropertySeverity>();
  if (severity.has_value()) {
    return *severity;
  }
  return absl::NotFoundError("No Severity property for condition.");
}

absl::StatusOr<google::protobuf::Timestamp> GetProtoTimeForCondition(
    const RedfishObject &condition_obj) {
  std::optional<absl::Time> timestamp_property =
      condition_obj.GetNodeValue<PropertyTimestamp>();
  if (timestamp_property.has_value()) {
    absl::StatusOr<google::protobuf::Timestamp> proto_timestamp =
        AbslTimeToProtoTime(*timestamp_property);
    if (proto_timestamp.ok()) {
      return *proto_timestamp;
    }
    return absl::InternalError("Error parsing health rollup timestamp.");
  }
  return absl::NotFoundError("No Timestamp property for condition.");
}

absl::StatusOr<HealthRollup::ResourceEvent> ExtractResourceEventFromMessageArgs(
    const RedfishVariant &message_args,
    const MessageRegistryAndType &message_registry_and_type) {
  HealthRollup::ResourceEvent resource_event;
  if (message_registry_and_type.message_registry != kResourceEventRegistry) {
    return absl::FailedPreconditionError(
        absl::StrCat("Unknown message registry: ",
                     message_registry_and_type.message_registry));
  }
  // All supported message types have two message args.  If support is added
  // for messages with additional args, the error will have to be on a
  // per-message type basis.
  std::unique_ptr<RedfishIterable> message_args_itr = message_args.AsIterable();
  if (message_args_itr == nullptr) {
    return absl::FailedPreconditionError("MessageArgs is not iterable.");
  }
  if (message_args_itr->Size() < 2) {
    return absl::FailedPreconditionError(
        "Condition contains too few MessageArgs.");
  }
  // If there are extra args, just use the first two, but log a warning. Fewer
  // than two means an event cannot be extracted.
  if (message_args_itr->Size() > 2) {
    LOG_FIRST_N(WARNING, 10)
        << "Condition contains a noncompliant number of message args, size: "
        << message_args_itr->Size();
  }

  // Parse by message type per ResourceEvent definitions for supported types.
  if (message_registry_and_type.message_type == kResourceErrorsDetected) {
    // Two string args.
    std::string resource_event_id, error_type;
    message_args[0].GetValue(&resource_event_id);
    message_args[1].GetValue(&error_type);
    *resource_event.mutable_errors_detected()->mutable_resource_identifier() =
        std::move(resource_event_id);
    *resource_event.mutable_errors_detected()->mutable_error_type() =
        std::move(error_type);
  } else if (message_registry_and_type.message_type == kResourceStateChange) {
    // Two string args.
    std::string resource_event_id, state_change;
    message_args[0].GetValue(&resource_event_id);
    message_args[1].GetValue(&state_change);
    *resource_event.mutable_state_change()->mutable_resource_identifier() =
        std::move(resource_event_id);
    *resource_event.mutable_state_change()->mutable_state_change() =
        std::move(state_change);
  } else if (message_registry_and_type.message_type ==
             kResourceErrorThresholdExceeded) {
    // One string and one value arg.
    std::string resource_event_id;
    message_args[0].GetValue(&resource_event_id);
    *resource_event.mutable_threshold_exceeded()
         ->mutable_resource_identifier() = std::move(resource_event_id);
    double threshold;
    message_args[1].GetValue(&threshold);
    resource_event.mutable_threshold_exceeded()->set_threshold(threshold);
  } else {
    return absl::InternalError(absl::StrCat(
        "Unknown message type: ", message_registry_and_type.message_type));
  }
  return resource_event;
}
}  // namespace

absl::StatusOr<HealthRollup> ExtractHealthRollup(
    const RedfishObject &obj,
    absl::AnyInvocable<std::optional<std::string>(const RedfishObject &)>
        devpath_resolver) {
  std::optional<std::string> resource_uri = obj.GetUriString();
  HealthRollup health_rollup;
  std::unique_ptr<RedfishObject> status_obj = obj[kRfPropertyStatus].AsObject();
  if (!status_obj)
    return absl::InternalError("No Status for determining Health Rollup");
  std::optional<std::string> health_rollup_property =
      status_obj->GetNodeValue<PropertyHealthRollup>();
  if (!health_rollup_property.has_value() ||
      (*health_rollup_property != kCritical &&
       *health_rollup_property != kWarning)) {
    return health_rollup;
  }
  std::unique_ptr<RedfishIterable> conditions =
      (*status_obj)[kRfPropertyConditions].AsIterable();
  if (!conditions || conditions->Empty()) {
    return absl::FailedPreconditionError(
        "HealthRollup present but no Conditions to evaluate.");
  }

  for (const RedfishVariant condition : *conditions) {
    std::unique_ptr<RedfishObject> condition_obj = condition.AsObject();
    if (!condition_obj) {
      return absl::InternalError(
          "Error fetching condition object as RedfishObject");
    }
    std::optional<MessageRegistryAndType> message_registry_and_type =
        GetMessageRegistryAndTypeForCondition(*condition_obj);
    if (!message_registry_and_type.has_value()) {
      return absl::InternalError(
          "Error determining message registry and type.");
    }
    absl::StatusOr<HealthRollup::ResourceEvent> resource_event =
        ExtractResourceEventFromMessageArgs(condition[kRfPropertyMessageArgs],
                                            *message_registry_and_type);
    if (!resource_event.ok()) {
      LOG_FIRST_N(WARNING, 10)
          << "Extracting resource event from args failed on URI "
          << resource_uri.value_or("")
          << " with message: " << resource_event.status().message();
      continue;
    }
    absl::StatusOr<std::string> severity =
        GetSeverityForCondition(*condition_obj);
    if (severity.ok()) {
      resource_event->set_severity(*severity);
    } else {
      LOG(ERROR) << "Health rollup severity error: "
                 << severity.status().message();
    }
    absl::StatusOr<google::protobuf::Timestamp> proto_timestamp =
        GetProtoTimeForCondition(*condition_obj);
    if (proto_timestamp.ok()) {
      *resource_event->mutable_timestamp() = *proto_timestamp;
    } else {
      LOG(ERROR) << "Health rollup timestamp error: "
                 << proto_timestamp.status().message();
    }

    // Fetch origin of condition devpath if OriginOfCondition URI differs from
    // current resource URI, resolving from current URI otherwise.
    if (resource_uri.has_value()) {
      if (std::unique_ptr<RedfishObject> condition_origin_obj =
              (*condition_obj)[kRfPropertyOriginOfCondition].AsObject();
          (condition_origin_obj != nullptr) &&
          condition_origin_obj->GetUriString().value_or("") != *resource_uri) {
        std::optional<std::string> origin_devpath =
            devpath_resolver(*condition_origin_obj);
        if (!origin_devpath.has_value()) continue;
        *resource_event->mutable_origin_devpath() = *std::move(origin_devpath);
      } else {
        std::optional<std::string> current_devpath = devpath_resolver(obj);
        if (!current_devpath.has_value()) continue;
        *resource_event->mutable_origin_devpath() = *std::move(current_devpath);
      }
    }

    *health_rollup.add_resource_events() = *std::move(resource_event);
  }
  return health_rollup;
}

absl::StatusOr<HealthRollup> ExtractHealthRollup(const RedfishObject &obj) {
  return ExtractHealthRollup(
      obj, [](const RedfishObject &unused) { return std::nullopt; });
}

}  // namespace ecclesia