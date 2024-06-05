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

#include "ecclesia/lib/redfish/redpath/engine/redpath_trie.h"

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/join.h"
#include "ecclesia/lib/status/macros.h"
#include "re2/re2.h"

namespace ecclesia {

namespace {

// Pattern for location step: NodeName[Predicate]
constexpr LazyRE2 kLocationStepRegex = {
    "^([a-zA-Z#@][0-9a-zA-Z.]+|)(?:\\[(.*?)\\]|)$"};

// Inserts RedPath expression into RedPath trie.
RedPathTrieNode *InsertRedPathExpressions(
    RedPathTrieNode *current_node,
    const std::vector<RedPathExpression> &redpath_expressions) {
  for (const auto &expression : redpath_expressions) {
    if (auto it = current_node->expression_to_trie_node.find(expression);
        it == current_node->expression_to_trie_node.end()) {
      // End of prefix. Insert the expression here.
      RedPathTrieNode *prev_node = current_node;
      auto new_node = std::make_unique<RedPathTrieNode>();
      current_node = new_node.get();
      prev_node->expression_to_trie_node[expression] = std::move(new_node);
    } else {
      // Still in prefix. Look into the next node.
      current_node = it->second.get();
    }
  }
  return current_node;
}

// Splits RedPath into individual expressions.
absl::StatusOr<std::vector<RedPathExpression>> SplitRedPath(
    const std::string &redpath, RedPathExpression::Type type) {
  std::vector<RedPathExpression> redpath_expressions;
  for (const auto &redpath_step :
       absl::StrSplit(redpath, '/', absl::SkipEmpty())) {
    std::string node_name;
    std::string predicate;
    if (!RE2::FullMatch(redpath_step, *kLocationStepRegex, &node_name,
                        &predicate)) {
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid redpath step: ", redpath_step));
    }
    redpath_expressions.push_back({type, node_name});

    if (!predicate.empty()) {
      // working with templated queries.
      redpath_expressions.push_back(
          {RedPathExpression::Type::kPredicate, predicate});
    }
  }
  return redpath_expressions;
}

}  // namespace

// Prints RedPath trie node to string.
// TEST-ONLY: Useful for debugging QueryPlan.
std::string RedPathTrieNode::ToString(absl::string_view prefix) const {
  std::string result = "RedPathPrefix: " + std::string(prefix);
  if (!subquery_id.empty()) {
    result += ", Subquery: " + subquery_id;
  }
  result += "\n";

  absl::flat_hash_map<RedPathExpression, std::string> expression_to_log;
  for (const auto &[expression, trie_node] : expression_to_trie_node) {
    std::string new_prefix(prefix);
    if (expression.type == RedPathExpression::Type::kPredicate) {
      new_prefix += "[" + expression.expression + "]";
    } else {
      new_prefix += "/" + expression.expression;
    }
    result += trie_node->ToString(new_prefix);
  }
  return result;
}

absl::Status RedPathTrieBuilder::ProcessSubquerySequence(
    RedPathTrieNode *root_node,
    const std::vector<std::string> &subquery_sequence) {
  RedPathTrieNode *current_node = root_node;
  for (const auto &subquery_id : subquery_sequence) {
    const DelliciusQuery::Subquery *subquery =
        subquery_id_to_subquery_[subquery_id];
    std::vector<RedPathExpression> redpath_expressions;
    if (subquery->has_redpath()) {
      const std::string &redpath = subquery->redpath();
      ECCLESIA_ASSIGN_OR_RETURN(
          redpath_expressions,
          SplitRedPath(redpath, RedPathExpression::Type::kNodeName));
    } else if (subquery->has_uri_reference_redpath()) {
      std::string normalized_node_name =
          absl::StrReplaceAll(subquery->uri_reference_redpath(), {{"/", "."}});
      redpath_expressions.push_back(
          {RedPathExpression::Type::kNodeNameJsonPointer,
           normalized_node_name});
    } else if (subquery->has_uri()) {
      redpath_expressions.push_back(
          {RedPathExpression::Type::kNodeNameUriPointer, subquery->uri()});
    }
    // Insert RedPath Expression
    current_node = InsertRedPathExpressions(current_node, redpath_expressions);

    if (!current_node->subquery_id.empty() &&
        current_node->subquery_id != subquery_id) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Same RedPath found in multiple subqueries. Check subqueries ",
          current_node->subquery_id, " and ", subquery_id));
    }
    current_node->subquery_id = subquery_id;
  }
  return absl::OkStatus();
}

absl::StatusOr<const absl::flat_hash_set<std::vector<std::string>> *>
RedPathTrieBuilder::GetSubquerySequences() {
  if (subquery_sequences_.empty()) {
    return absl::InternalError("Subquery sequence is not set!");
  }
  return &subquery_sequences_;
}

absl::StatusOr<std::unique_ptr<RedPathTrieNode>>
RedPathTrieBuilder::CreateRedPathTrie() {
  // Join subqueries by resolving root associations in all subqueries.
  absl::flat_hash_set<std::vector<std::string>> subquery_sequences;
  ECCLESIA_RETURN_IF_ERROR(JoinSubqueries(*query_, subquery_sequences));
  subquery_sequences_ = std::move(subquery_sequences);

  // Build RedPath prefix tree.
  auto root_node = std::make_unique<RedPathTrieNode>();
  RedPathTrieNode *root_node_raw = root_node.get();

  // Insert RedPath expressions in RedPath trie from each Subquery in each
  // subquery sequence in the sequence collection provided.
  for (const auto &subquery_sequence : subquery_sequences_) {
    ECCLESIA_RETURN_IF_ERROR(
        ProcessSubquerySequence(root_node_raw, subquery_sequence));
  }
  return root_node;
}

}  // namespace ecclesia
