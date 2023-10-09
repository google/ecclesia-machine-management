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

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_rules.pb.h"
#include "google/protobuf/text_format.h"

namespace ecclesia {

using ExpandConfiguration = RedPathPrefixWithQueryParams::ExpandConfiguration;

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
    for (const auto &[query_id, prefix_set_with_query_params] :
         query_rules.query_id_to_params_rule()) {
      RedPathRedfishQueryParams redpath_prefix_to_params;
      // Iterate over each pair of RedPath prefix and Redfish query parameter
      // configuration and build the prefix to param mapping in memory.
      for (const auto &redpath_prefix_with_query_params :
           prefix_set_with_query_params.redpath_prefix_with_params()) {
        RedfishQueryParamExpand::ExpandType expand_type;
        ExpandConfiguration::ExpandType expand_type_in_rule =
            redpath_prefix_with_query_params.expand_configuration().type();
        if (expand_type_in_rule == ExpandConfiguration::BOTH) {
          expand_type = RedfishQueryParamExpand::kBoth;
        } else if (expand_type_in_rule == ExpandConfiguration::NO_LINKS) {
          expand_type = RedfishQueryParamExpand::kNotLinks;
        } else if (expand_type_in_rule == ExpandConfiguration::ONLY_LINKS) {
          expand_type = RedfishQueryParamExpand::kLinks;
        } else {
          break;
        }
        GetParams params{.expand = RedfishQueryParamExpand(
                             {.type = expand_type,
                              .levels = redpath_prefix_with_query_params
                                            .expand_configuration()
                                            .level()})};

        redpath_prefix_to_params[redpath_prefix_with_query_params.redpath()] =
            params;
      }
      parsed_query_rules[query_id] = redpath_prefix_to_params;
    }
  }
  return parsed_query_rules;
}

}  // namespace ecclesia
