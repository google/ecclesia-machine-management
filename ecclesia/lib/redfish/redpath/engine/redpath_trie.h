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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_REDPATH_TRIE_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_REDPATH_TRIE_H_

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/die_if_null.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"

namespace ecclesia {

// Describes a RedPath expression.
struct RedPathExpression {
  // Type of RedPath expression.
  // Example: For RedPath `/Chassis[Id=1]/Sensors[Id=$arg1]`
  //   `/` => kRoot
  //   `Chassis` => kNodeName
  //   `[Id=1]` => kPredicate
  //   `[Reading=$arg1]` => kPredicateTemplate
  enum class Type : uint8_t {
    kNodeName = 0,
    kPredicate,
    kPredicateTemplate,
    kNodeNameJsonPointer,
    kNodeNameUriPointer
  };
  Type type;
  std::string expression;

  RedPathExpression(Type type, absl::string_view expression)
      : type(type), expression(expression) {}

  bool operator==(const RedPathExpression &other) const {
    return type == other.type && expression == other.expression;
  }

  bool operator!=(const RedPathExpression &other) const {
    return !(*this == other);
  }

  template <typename H>
  friend H AbslHashValue(H h, const RedPathExpression &n) {
    return H::combine(std::move(h), n.type, n.expression);
  }

  template <typename Sink>
  friend void AbslStringify(Sink &sink, const RedPathExpression &expr) {
    std::string type_str;
    switch (expr.type) {
      case RedPathExpression::Type::kNodeName:
        type_str = "node_name";
        break;
      case RedPathExpression::Type::kPredicate:
        type_str = "predicate";
        break;
      case RedPathExpression::Type::kPredicateTemplate:
        type_str = "predicate_template";
        break;
      case RedPathExpression::Type::kNodeNameJsonPointer:
        type_str = "node_name_json_pointer";
        break;
      case RedPathExpression::Type::kNodeNameUriPointer:
        type_str = "node_name_uri_pointer";
        break;
    }
    absl::Format(&sink, "{ type: \"%s\" expression: \"%s\"}", type_str,
                 expr.expression);
  }
};

// Trie data structure specifically designed to represent and process RedPath
// expressions. RedPath is a hierarchical path-like query containing expressions
// used for querying data organized in Redfish resource tree.
//
// Trie Structure:
// - Each node in the trie (RedPathTrieNode) represents an expression in RedPath
// - The 'expression_to_trie_node' map within each node establishes the
//   parent-child relationships forming the trie structure.
// - Leaf nodes may contain a 'subquery_id' if a subquery terminates at that
//   node.
//
// Example:
// RedPath1: /Chassis[Id=1]/Sensors[Reading=$arg1]
// RedPath2: /Chassis[Id=2]
//
//                {(RedPathExpression: kRoot, ""), (Root Node)}
//                                    |
//        {(RedPathExpression: kNodeName, "Chassis"), (Chassis Node)}
//                                 /    \
// {(RedPathExpression: kPredicate,    {(RedPathExpression: kPredicate,
// "Id=1"), (Chassis Predicate1 Node)}    "Id=2"), (Chassis Predicate2 Node)}
//                            /
// {(RedPathExpression: kNodeName,
//  "Sensors"), (Sensors Node)}
//                        /
// {(RedPathExpression: kPredicate,
// "Reading=$arg1"), (Sensors Predicate Node)}
//
struct RedPathTrieNode {
  // A node with non-empty `subquery_id` suggests that a subquery ends at this
  // node.
  std::string subquery_id;
  absl::flat_hash_map<RedPathExpression, std::unique_ptr<RedPathTrieNode>>
      expression_to_trie_node;

  // Converts a RedPathTrieNode to string.
  std::string ToString(absl::string_view prefix = "") const;
};

// Builds RedPath trie.
class RedPathTrieBuilder {
 public:
  explicit RedPathTrieBuilder(const DelliciusQuery *query)
      : query_(ABSL_DIE_IF_NULL(query)) {
    for (const auto &subquery : query_->subquery()) {
      subquery_id_to_subquery_[subquery.subquery_id()] = &subquery;
    }
  }

  absl::StatusOr<std::unique_ptr<RedPathTrieNode>> CreateRedPathTrie();

  // Returns the subquery execution sequences that are derived from RedPath
  // query after resolving all `root_subquery_ids` associations between
  // subqueries.
  absl::StatusOr<const absl::flat_hash_set<std::vector<std::string>> *>
  GetSubquerySequences();

 private:
  absl::Status ProcessSubquerySequence(
      RedPathTrieNode *root_node,
      const std::vector<std::string> &subquery_sequence);

  const DelliciusQuery *query_;
  absl::flat_hash_map<std::string, const DelliciusQuery::Subquery *>
      subquery_id_to_subquery_;
  absl::flat_hash_set<std::vector<std::string>> subquery_sequences_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_REDPATH_TRIE_H_
