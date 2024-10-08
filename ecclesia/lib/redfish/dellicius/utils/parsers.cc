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

#include "ecclesia/lib/redfish/dellicius/utils/parsers.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_rules.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/engine/query_planner.h"
#include "google/protobuf/text_format.h"

namespace ecclesia {

namespace {

using ExpandConfiguration = RedPathPrefixWithQueryParams::ExpandConfiguration;

absl::StatusOr<GetParams> GetQueryParams(
    const RedPathPrefixWithQueryParams &redpath_prefix_with_query_params) {
  GetParams params;
  if (redpath_prefix_with_query_params.has_top_configuration()) {
    if (redpath_prefix_with_query_params.top_configuration().num_members() <
        0) {
      params.top = RedfishQueryParamTop(0);
    } else {
      params.top = RedfishQueryParamTop(
          redpath_prefix_with_query_params.top_configuration().num_members());
    }
  }
  if (redpath_prefix_with_query_params.has_expand_configuration()) {
    RedfishQueryParamExpand::ExpandType expand_type;
    ExpandConfiguration::ExpandType expand_type_in_rule =
        redpath_prefix_with_query_params.expand_configuration().type();
    if (expand_type_in_rule == ExpandConfiguration::BOTH) {
      expand_type = RedfishQueryParamExpand::ExpandType::kBoth;
    } else if (expand_type_in_rule == ExpandConfiguration::NO_LINKS) {
      expand_type = RedfishQueryParamExpand::ExpandType::kNotLinks;
    } else if (expand_type_in_rule == ExpandConfiguration::ONLY_LINKS) {
      expand_type = RedfishQueryParamExpand::ExpandType::kLinks;
    } else {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Invalid expand type: %s",
          ExpandConfiguration::ExpandType_Name(expand_type_in_rule)));
    }
    params.expand = RedfishQueryParamExpand(
        {expand_type,
         redpath_prefix_with_query_params.expand_configuration().level()});
  }
  // Populate the filter parameter with an empty object to indicate that
  // filter is enabled.
  if (redpath_prefix_with_query_params.filter_enabled()) {
    params.filter = RedfishQueryParamFilter("");
  }
  params.uri_prefix = redpath_prefix_with_query_params.uri_prefix();
  return params;
}

}  // namespace

absl::StatusOr<absl::flat_hash_map<std::string, RedPathRedfishQueryParams>>
ParseQueryRulesFromEmbeddedFiles(
    const std::vector<EmbeddedFile> &embedded_query_rules) {
  absl::flat_hash_map<std::string, RedPathRedfishQueryParams>
      parsed_query_rules;
  // Extract query rules from embedded query files.
  for (const EmbeddedFile &embedded_rule : embedded_query_rules) {
    QueryRules query_rules;
    // Parse query rules into embedded file object.

    if (!google::protobuf::TextFormat::ParseFromString(std::string(embedded_rule.data),
            &query_rules)) {
      return absl::InternalError(
          absl::StrFormat("Invalid query rule:\n%s", embedded_rule.data));
    }
    // Extract RedPath prefix to query params map for each query id.
    for (auto &[query_id, prefix_set_with_query_params] :
         *query_rules.mutable_query_id_to_params_rule()) {
      parsed_query_rules[query_id] =
          ParseQueryRuleParams(std::move(prefix_set_with_query_params));
    }
  }
  return std::move(parsed_query_rules);
}

RedPathRules CreateRedPathRules(
    QueryRules::RedPathPrefixSetWithQueryParams rule) {
  RedPathRules redpath_rules;
  for (auto &redpath_prefix_with_query_params :
       *rule.mutable_redpath_prefix_with_params()) {
    if (redpath_prefix_with_query_params.subscribe()) {
      redpath_rules.redpaths_to_subscribe.insert(
          redpath_prefix_with_query_params.redpath());
    }
    absl::StatusOr<GetParams> params =
        GetQueryParams(redpath_prefix_with_query_params);
    if (!params.ok()) {
      LOG(ERROR) << params.status();
      break;
    }
    redpath_rules.redpath_to_query_params[std::move(
        *redpath_prefix_with_query_params.mutable_redpath())] = *params;
  }
  return redpath_rules;
}

RedPathRedfishQueryParams ParseQueryRuleParams(
    QueryRules::RedPathPrefixSetWithQueryParams rule) {
  RedPathRedfishQueryParams redpath_prefix_to_params;
  // Iterate over each pair of RedPath prefix and Redfish query parameter
  // configuration and build the prefix to param mapping in memory.
  for (auto &redpath_prefix_with_query_params :
       *rule.mutable_redpath_prefix_with_params()) {
    absl::StatusOr<GetParams> params =
        GetQueryParams(redpath_prefix_with_query_params);
    if (!params.ok()) {
      LOG(ERROR) << params.status();
      break;
    }
    redpath_prefix_to_params[std::move(
        *redpath_prefix_with_query_params.mutable_redpath())] = *params;
  }
  return redpath_prefix_to_params;
}

}  // namespace ecclesia
