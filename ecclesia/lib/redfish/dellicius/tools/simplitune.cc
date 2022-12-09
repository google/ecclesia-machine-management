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

#include "ecclesia/lib/redfish/dellicius/tools/simplitune.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_rules.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "re2/re2.h"

namespace ecclesia {

namespace {

// Pattern for location step: NodeName[Predicate]
constexpr LazyRE2 kLocationStepRegex = {
    "^([a-zA-Z#@][0-9a-zA-Z.]+)(?:(\\[.*?\\])|)$"};
// Expand types supported.
// In the interest of reducing the run time, we only generate homegeneous expand
// configs where all expand types would be of one of the three types defined in
// Redfish spec.
// queries.
constexpr std::array<RedfishQueryParamExpand::ExpandType, 2> kExpandTypes = {
    RedfishQueryParamExpand::kBoth, RedfishQueryParamExpand::kNotLinks
    /* , RedfishQueryParamExpand::kLinks */};
// Meta character prefixing expand value in generated Redpath Prefix configs
constexpr absl::string_view kExpandMetaCharacter = "$";

// Descriptor encapsulating contextual information on a tuning iteration.
struct TuningContext {
  // Depth in Tree where next Expand is allowed.
  size_t next_allowed_expand_depth = 0;
  // Next Expand Level
  size_t next_expand_level = 0;
};

// Naive PowerSet utility.
std::vector<std::vector<std::string>> GenerateSubsetsOfRedPathSet(
    std::vector<std::string> prefix_set) {
  double total_subsets = std::pow(2, static_cast<double>(prefix_set.size()));
  std::vector<std::vector<std::string>> subsets(
      static_cast<size_t>(total_subsets));

  for (int i = 0; i < total_subsets; i++) {
    std::vector<std::string> single_prefix_set;
    for (int j = 0; j < prefix_set.size(); j++) {
      if (i & (1 << j)) {
        single_prefix_set.push_back(prefix_set[j]);
      }
    }
    subsets.push_back(single_prefix_set);
  }
  return subsets;
}

// Prefix tree of RedPath Expressions, purpose built for generating unique
// all possible configurations of Redfish Query Parameters for each RedPath
// prefix in the tree.
// Example:
//   RedPath: /Chassis[*]/Sensors[*]
//   Prefixes: /Chassis, /Chassis[*], /Chassis[*]/Sensors,
//             /Chassis[*]/Sensors[*]
//   Configurations:
//     Combination x: {<PREFIX1>, <EXPAND LEVEL>}, {<PREFIX2>, <EXPAND LEVEL>}
//     Combination 1: {/Chassis, 1}, {/Chassis[*]/Sensors, 1}
//     Combination 2: {/Chassis, 3}
//     ...
class RedPathTrie {
 public:
  // Adds RedPath expression to the prefix tree and returns Max Expand value
  // derived from max size of subtree.
  size_t AddExpression(absl::Span<const std::string> expressions,
                       absl::string_view last_prefix, size_t depth) {
    if (expressions.empty()) {
      return 0;
    }
    absl::string_view expr = expressions[0];
    depth_ = depth;
    // Create child node
    RedPathTrie &next_prefix_node = trie_[expr];
    // Populate child node attributes
    next_prefix_node.redpath_prefix_ = redpath_prefix_;
    if (absl::StartsWith(expr, "[")) {
      is_predicate_ = true;
      absl::StrAppend(&next_prefix_node.redpath_prefix_, expr);
    } else {
      absl::StrAppend(&next_prefix_node.redpath_prefix_, "/", expr);
    }
    longest_prefix_ =
        std::max(1 + next_prefix_node.AddExpression(
                         expressions.subspan(1),
                         next_prefix_node.redpath_prefix_, depth_ + 1),
                 longest_prefix_);
    return longest_prefix_;
  }

  void GetRedPathPrefixSetsFromSingleNode(
      TuningContext ctx, RedPathTrie &child_node,
      absl::flat_hash_set<absl::flat_hash_set<std::string>>
          &set_of_prefix_set_across_expressions) const {
    // Get RedPath prefix associated with the child node.
    std::string path_prefix = child_node.redpath_prefix_;
    // Append 'Expand' level escaped by metacharacter.
    absl::StrAppend(&path_prefix, kExpandMetaCharacter, ctx.next_expand_level);
    // Select the path_prefix if current depth is valid expand point relative
    // to last expand point.
    // For example, if last expand was at depth 0 for level 1, the next
    // expand can occur at depth 2.
    // We are not using predicate as a Expand Set point.
    bool is_valid_expand_point = false;
    if (ctx.next_allowed_expand_depth <= depth_ && !is_predicate_) {
      // Set next_allowed_expand_depth to prepare tuning context for next node
      // in subtree.
      ctx.next_allowed_expand_depth = depth_ + ctx.next_expand_level + 1;
      is_valid_expand_point = true;
    }

    // Prefix sets received while backtracking from child nodes.
    absl::flat_hash_set<absl::flat_hash_set<std::string>>
        prefix_sets_from_next_depth =
            child_node.GetRedPathPrefixSetsFromAllNodes(ctx);

    // Aggregate all RedPath Prefixes in a single list to create a power set
    absl::flat_hash_set<std::string> aggregated_prefix_set;
    for (const auto &prefix_set : prefix_sets_from_next_depth) {
      aggregated_prefix_set.insert(prefix_set.begin(), prefix_set.end());
    }

    // Add RedPath prefix from lists obtained from other expression nodes
    // at the current tree depth.
    for (const auto &prefix_set : set_of_prefix_set_across_expressions) {
      aggregated_prefix_set.insert(prefix_set.begin(), prefix_set.end());
    }

    // Generate power set of aggregated prefixes.
    absl::flat_hash_set<absl::flat_hash_set<std::string>>
        redpath_prefix_power_set;
    if (!aggregated_prefix_set.empty()) {
      auto subsets_as_vectors = GenerateSubsetsOfRedPathSet(
          {aggregated_prefix_set.begin(), aggregated_prefix_set.end()});
      for (const auto &redpath_subset : subsets_as_vectors) {
        absl::flat_hash_set<std::string> as_set;
        as_set.insert(redpath_subset.begin(), redpath_subset.end());
        redpath_prefix_power_set.insert(std::move(as_set));
      }
    }

    if (is_valid_expand_point) {
      // Create combinations as follows
      // 1. Just the expression as a single element list
      absl::flat_hash_set<std::string> expr_alone = {path_prefix};

      // 2. Expression combined with subsets in power set.
      absl::flat_hash_set<absl::flat_hash_set<std::string>>
          expr_curr_next_combined;
      for (auto prefix_set_at_next_level : redpath_prefix_power_set) {
        prefix_set_at_next_level.insert(path_prefix);
        expr_curr_next_combined.insert(prefix_set_at_next_level);
      }

      set_of_prefix_set_across_expressions.insert(std::move(expr_alone));
      set_of_prefix_set_across_expressions.insert(
          expr_curr_next_combined.begin(), expr_curr_next_combined.end());
    }
    set_of_prefix_set_across_expressions.insert(
        redpath_prefix_power_set.begin(), redpath_prefix_power_set.end());
  }

  absl::flat_hash_set<absl::flat_hash_set<std::string>>
  GetRedPathPrefixSetsFromAllNodes(TuningContext &current_tuning_ctx) {
    // Aggregate expand subsets across expressions across expand levels.
    absl::flat_hash_set<absl::flat_hash_set<std::string>>
        set_of_subsets_across_expand_levels;
    for (size_t level = 1; level < longest_prefix_; ++level) {
      // Aggregates RedPath subsets with expand across expressions but gets
      // reset for each expand level.
      absl::flat_hash_set<absl::flat_hash_set<std::string>>
          set_of_prefix_set_across_expressions;
      for (auto &expr : trie_) {
        if (expr.second.longest_prefix_ < level) {
          continue;
        }
        current_tuning_ctx.next_expand_level = level;
        GetRedPathPrefixSetsFromSingleNode(
            current_tuning_ctx, expr.second,
            set_of_prefix_set_across_expressions);
      }

      // Now save all RedPath prefix sets generated for all expressions at
      // current depth of the trie for the specific Expand Level defined by
      // the outer for loop.
      set_of_subsets_across_expand_levels.insert(
          set_of_prefix_set_across_expressions.begin(),
          set_of_prefix_set_across_expressions.end());
    }
    return set_of_subsets_across_expand_levels;
  }

 private:
  // Linked subtree.
  absl::flat_hash_map<std::string, RedPathTrie> trie_;
  // RedPath Prefix including this node.
  std::string redpath_prefix_;
  // Depth of the subtree this node is located at.
  size_t depth_ = 0;
  // Longest RedPath prefix from this node.
  size_t longest_prefix_ = 0;
  bool is_predicate_ = false;
};

// Constructs GetParams object for each RedPath in every given RedPath set.
// Given RedPath set of sets has RedPath expressions with expand meta character
// embedded which is decoded to construct GetParams object and mapped with
// RedPath expression.
void PopulateQueryParamsForEachCombination(
    const absl::flat_hash_set<absl::flat_hash_set<std::string>>
        &prefix_set_with_expand_combinations,
    std::vector<RedPathRedfishQueryParams> &expand_configs) {
  // Construct expand configuration for each expand type.
  for (const auto &expand_type : kExpandTypes) {
    for (const auto &prefix_set_with_expand :
         prefix_set_with_expand_combinations) {
      RedPathRedfishQueryParams redpath_and_query_param;
      // For each combination of path expression with expand parameters.
      for (absl::string_view prefix_with_expand : prefix_set_with_expand) {
        size_t level = 0;
        // Indicates validity of expand level escaped by meta character in
        // RedPath expression.
        bool expand_value = false;
        std::string prefix;
        if (auto pos = prefix_with_expand.find_last_of(kExpandMetaCharacter);
            pos != std::string::npos) {
          prefix = prefix_with_expand.substr(0, pos);
          expand_value =
              absl::SimpleAtoi(prefix_with_expand.substr(pos + 1), &level);
        }
        if (expand_value && !prefix.empty()) {
          GetParams params{.expand = RedfishQueryParamExpand(
                               {.type = expand_type, .levels = level})};
          redpath_and_query_param[prefix] = std::move(params);
        }
      }
      if (!redpath_and_query_param.empty()) {
        expand_configs.push_back(redpath_and_query_param);
      }
    }
  }
}

void Tune(const absl::flat_hash_set<std::string> &redpaths,
          std::vector<RedPathRedfishQueryParams> &expand_configs) {
  RedPathTrie trie;
  size_t max_expand = 0;
  for (const auto &path : redpaths) {
    // All redpath expressions where both nodename and predicate are considered
    // individual expressions.
    std::vector<std::string> stripped_expressions;
    // All RedPath Step Expressions
    std::vector<std::string> redpath_expressions =
        absl::StrSplit(path, '/', absl::SkipEmpty());
    for (const auto &redpath_step : redpath_expressions) {
      std::string node_name, predicate;
      if (RE2::FullMatch(redpath_step, *kLocationStepRegex, &node_name,
                         &predicate)) {
        stripped_expressions.push_back(node_name);
        if (!predicate.empty()) {
          stripped_expressions.push_back(predicate);
        }
      }
    }
    max_expand =
        trie.AddExpression(absl::Span<std::string>{&stripped_expressions[0],
                                                   stripped_expressions.size()},
                           "", 0);
  }
  if (max_expand == 0) return;
  TuningContext tuning_ctx;
  absl::flat_hash_set<absl::flat_hash_set<std::string>>
      prefix_with_expand_combinations;
  prefix_with_expand_combinations =
      trie.GetRedPathPrefixSetsFromAllNodes(tuning_ctx);
  PopulateQueryParamsForEachCombination(prefix_with_expand_combinations,
                                        expand_configs);
}

}  // namespace

std::vector<RedPathRedfishQueryParams> GenerateExpandConfigurations(
    const QueryTracker &tracker) {
  std::vector<RedPathRedfishQueryParams> expand_configs;
  absl::flat_hash_set<std::string> redpaths_to_tune;
  for (const auto &[redpath, _] : tracker.redpaths_queried) {
    redpaths_to_tune.insert(redpath);
  }
  Tune(redpaths_to_tune, expand_configs);
  DLOG(INFO) << "Generated configs: ";
  size_t count = 0;
  for (const auto &expand_config : expand_configs) {
    DLOG(INFO) << "Config #" << ++count;
    for (const auto &[prefix, params] : expand_config) {
      if (!params.expand.has_value()) continue;
      DLOG(INFO) << "Path: " << prefix
                 << " QueryParam: " << params.expand->ToString();
    }
  }
  return expand_configs;
}

QueryRules::RedPathPrefixSetWithQueryParams GetQueryRuleProtoFromConfig(
    const RedPathRedfishQueryParams &redpath_to_query_params) {
  // Prepare the QueryRule proto descriptor using the golden configuration
  // identified after tuning for performance.
  QueryRules::RedPathPrefixSetWithQueryParams prefix_set_with_params_proto;
  for (const auto &[redpath, query_param] : redpath_to_query_params) {
    if (!query_param.expand.has_value()) {
      LOG(ERROR) << "Invalid Redfish query parameter in tuned configuration";
      return prefix_set_with_params_proto;
    }
    RedPathPrefixWithQueryParams::ExpandConfiguration expand_config;
    expand_config.set_level(query_param.expand->levels());
    if (query_param.expand->type() == RedfishQueryParamExpand::kLinks) {
      expand_config.set_type(
          RedPathPrefixWithQueryParams::ExpandConfiguration::ONLY_LINKS);
    } else if (query_param.expand->type() ==
               RedfishQueryParamExpand::kNotLinks) {
      expand_config.set_type(
          RedPathPrefixWithQueryParams::ExpandConfiguration::NO_LINKS);
    } else if (query_param.expand->type() == RedfishQueryParamExpand::kBoth) {
      expand_config.set_type(
          RedPathPrefixWithQueryParams::ExpandConfiguration::BOTH);
    }
    RedPathPrefixWithQueryParams prefix_with_query_params;
    prefix_with_query_params.set_redpath(redpath);
    *prefix_with_query_params.mutable_expand_configuration() =
        std::move(expand_config);
    prefix_set_with_params_proto.mutable_redpath_prefix_with_params()->Add(
        std::move(prefix_with_query_params));
  }
  return prefix_set_with_params_proto;
}

}  // namespace ecclesia
