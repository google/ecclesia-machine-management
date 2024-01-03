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

#include "ecclesia/lib/redfish/event/server/subscription.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

// EventSourceId definitions

nlohmann::json EventSourceId::ToJSON() const {
  nlohmann::json json;
  json["key"] = key;
  switch (type) {
    case Type::kDbusObjects:
      json["type"] = "kDbusObjects";
      break;
    case Type::kSocketIO:
      json["type"] = "kSocketIO";
      break;
    case Type::kFileIO:
      json["type"] = "kFileIO";
      break;
  }
  return json;
}

std::string EventSourceId::ToString() const { return this->ToJSON().dump(); }

// EventId definitions.

EventId::EventId(const SubscriptionId& subscription_id_in,
                 const EventSourceId& source_id_in, absl::Time timestamp_in)
    : source_id(source_id_in),
      timestamp(timestamp_in),
      subscription_id(subscription_id_in),
      uuid(absl::HashOf(timestamp_in, source_id_in, subscription_id_in)) {}

nlohmann::json EventId::ToJSON() const {
  nlohmann::json json;
  json["source_id"] = source_id.ToJSON();
  json["timestamp"] =
      absl::FormatTime(absl::RFC3339_full, timestamp, absl::UTCTimeZone());
  json["uuid"] = uuid;
  return json;
}

std::string EventId::ToString() const { return this->ToJSON().dump(); }

// SubscriptionContext definitions.

SubscriptionContext::SubscriptionContext(
    const SubscriptionId& subscription_id_in,
    absl::flat_hash_map<std::string, Trigger> id_to_triggers_in,
    std::function<void(const nlohmann::json&)>&& on_event_callback_in)
    : subscription_id(subscription_id_in),
      id_to_triggers(std::move(id_to_triggers_in)),
      on_event_callback(std::move(on_event_callback_in)) {}

// Trigger definitions.

absl::StatusOr<Trigger> Trigger::Create(const nlohmann::json& trigger_json) {
  // Check if json is valid.
  if (trigger_json.is_discarded()) {
    return absl::InvalidArgumentError("Trigger json is discarded");
  }

  // Parse the JSON data
  auto find_id = trigger_json.find("Id");
  if (find_id == trigger_json.end()) {
    return absl::InvalidArgumentError("Trigger Id not populated");
  }
  // Extract trigger information from the JSON data
  const std::string id = find_id->get<std::string>();

  auto find_origin_resources = trigger_json.find("OriginResources");
  if (find_origin_resources == trigger_json.end()) {
    return absl::InvalidArgumentError("Origin resources not populated");
  }

  std::vector<std::string> origin_resources;
  for (const auto& origin_resource : *find_origin_resources) {
    origin_resources.push_back(origin_resource["@odata.id"].get<std::string>());
  }

  std::string predicate;
  bool event_mask = false;

  auto find_predicate = trigger_json.find("Predicate");
  if (find_predicate != trigger_json.end()) {
    predicate = *find_predicate;
  } else {
    // If predicate is not provided, this trigger will be used to poll data.
    event_mask = true;
  }

  // Create a Trigger object with the extracted information
  Trigger trigger(std::move(origin_resources), predicate, event_mask);

  return trigger;
}

Trigger::Trigger(std::vector<std::string> origin_resources_in,
                 absl::string_view predicate_in, bool mask_in)
    : origin_resources(std::move(origin_resources_in)),
      predicate(predicate_in),
      mask(mask_in) {}

nlohmann::json Trigger::ToJSON() const {
  nlohmann::json json;

  // Create json array and store all origin_resources
  nlohmann::json& origin_resources_json = json["origin_resources"];
  origin_resources_json = nlohmann::json::array();
  origin_resources_json.insert_iterator(origin_resources_json.end(),
                                        origin_resources.begin(),
                                        origin_resources.end());

  nlohmann::json& event_source_to_uri_json = json["event_source_to_uri"];
  event_source_to_uri_json = nlohmann::json::array();
  for (const auto& [event_source, uris] : event_source_to_uri) {
    nlohmann::json uris_as_json_array = nlohmann::json::array();
    for (const auto& uri : uris) {
      uris_as_json_array.push_back(uri);
    }
    nlohmann::json event_source_to_uri_single;
    event_source_to_uri_single["event_source"] = event_source.ToJSON();
    event_source_to_uri_single["uris"] = uris_as_json_array;
    event_source_to_uri_json.push_back(event_source_to_uri_single);
  }

  json["predicate"] = predicate;
  json["mask"] = mask;
  return json;
}

std::string Trigger::ToString() const { return this->ToJSON().dump(); }

}  // namespace ecclesia
