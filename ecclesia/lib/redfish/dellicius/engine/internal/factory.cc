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

#include "ecclesia/lib/redfish/dellicius/engine/internal/factory.h"

#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/query_planner.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "re2/re2.h"

namespace ecclesia {

namespace {

using RedPathStep = std::pair<std::string, std::string>;
using SubqueryHandleCollection =
    std::vector<std::unique_ptr<QueryPlanner::SubqueryHandle>>;

// Regex definitions to identify LocationStep expression and extract NodeName,
// Predicate and Filter Expressions.
// Note: Regex patterns here are not meant to detect malformed RedPaths.
//
// Pattern for location step: NodeName[Predicate]
constexpr LazyRE2 kLocationStepRegex = {
    "^([a-zA-Z#@][0-9a-zA-Z.]+)(?:\\[(.*?)\\]|)$"};

// Creates RedPathStep objects from the given RedPath string.
absl::StatusOr<std::vector<RedPathStep>> RedPathToSteps(
    absl::string_view redpath) {
  std::vector<RedPathStep> steps;
  for (absl::string_view step_expression :
       absl::StrSplit(redpath, '/', absl::SkipEmpty())) {
    std::string node_name, predicate;
    if (!RE2::FullMatch(step_expression, *kLocationStepRegex, &node_name,
                        &predicate)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Cannot parse Step expression %s in RedPath %s",
                          step_expression, redpath));
    }
    steps.push_back({node_name, predicate});
  }
  return steps;
}

// Generates SubqueryHandles for all Root Subqueries after resolving links
// within each subquery.
class SubqueryHandleFactory {
 public:
  static absl::StatusOr<SubqueryHandleCollection> CreateSubqueryHandles(
      const DelliciusQuery &query, Normalizer *normalizer) {
    return std::move(SubqueryHandleFactory(query, normalizer))
        .GetSubqueryHandles();
  }

 private:
  SubqueryHandleFactory(const DelliciusQuery &query, Normalizer *normalizer)
      : query_(query), normalizer_(normalizer) {
    for (const auto &subquery : query.subquery()) {
      id_to_subquery_[subquery.subquery_id()] = subquery;
    }
  }

  absl::StatusOr<SubqueryHandleCollection> GetSubqueryHandles() && {
    for (const auto &subquery : query_.subquery()) {
      absl::flat_hash_set<std::string> subquery_id_chain;
      absl::Status status = BuildSubqueryHandleChain(
          subquery.subquery_id(), subquery_id_chain, nullptr);
      if (!status.ok()) {
        return status;
      }
    }
    SubqueryHandleCollection subquery_handle_collection;
    for (auto &&[_, subquery_handle] : id_to_subquery_handle_) {
      subquery_handle_collection.push_back(std::move(subquery_handle));
    }
    if (subquery_handle_collection.empty()) {
      return absl::InternalError("No SubqueryHandle created");
    }
    return subquery_handle_collection;
  }

  // Builds SubqueryHandle objects for subqueries linked together in a chain
  // through 'root_subquery_ids' property.
  // Args:
  //   subquery_id: Identifier of the subquery for which SubqueryHandle is built
  //   subquery_id_chain: Stores visited ids to help identify loop in chain
  //   child_subquery_handle: last built subquery handle to link as child node.
  absl::Status BuildSubqueryHandleChain(
      const std::string &subquery_id,
      absl::flat_hash_set<std::string> &subquery_id_chain,
      QueryPlanner::SubqueryHandle *child_subquery_handle) {
    auto id_to_subquery_iter = id_to_subquery_.find(subquery_id);
    if (id_to_subquery_iter == id_to_subquery_.end()) {
      return absl::InternalError(
          absl::StrFormat("Cannot find a subquery for id: %s", subquery_id));
    }
    DelliciusQuery::Subquery &subquery = id_to_subquery_iter->second;
    // Subquery links create a loop if same subquery id exists in the chain.
    if (!subquery_id_chain.insert(subquery_id).second) {
      return absl::InternalError("Loop detected in subquery links");
    }

    // Find SubqueryHandle for given SubqueryId
    auto id_to_subquery_handle_iter = id_to_subquery_handle_.find(subquery_id);
    // If SubqueryHandle exists for the given identifier and a child subquery
    // handle is provided, link the child SubqueryHandle.
    if (id_to_subquery_handle_iter != id_to_subquery_handle_.end()) {
      if (child_subquery_handle != nullptr) {
        id_to_subquery_handle_iter->second->AddChildSubqueryHandle(
            child_subquery_handle);
      }
      return absl::OkStatus();
    }
    // Create a new SubqueryHandle.
    absl::StatusOr<std::vector<RedPathStep>> steps =
        RedPathToSteps(subquery.redpath());
    if (!steps.ok()) {
      LOG(ERROR) << "Cannot create SubqueryHandle for " << subquery_id;
      return steps.status();
    }
    auto new_subquery_handle = std::make_unique<QueryPlanner::SubqueryHandle>(
        subquery, steps.value(), normalizer_);
    // Raw pointer used to link SubqueryHandle with parent subquery if any.
    QueryPlanner::SubqueryHandle *new_subquery_handle_ptr =
        new_subquery_handle.get();
    // Link the given child SubqueryHandle.
    if (child_subquery_handle != nullptr) {
      new_subquery_handle->AddChildSubqueryHandle(child_subquery_handle);
    }
    id_to_subquery_handle_[subquery_id] = std::move(new_subquery_handle);
    // Return if Subquery does not have any root Subquery Ids linked i.e the
    // subquery itself is a root.
    if (subquery.root_subquery_ids().empty()) {
      return absl::OkStatus();
    }
    // Recursively build root subquery handles.
    for (const auto &root_subquery_id : subquery.root_subquery_ids()) {
      absl::flat_hash_set<std::string> subquery_id_chain_per_root =
          subquery_id_chain;
      auto status =
          BuildSubqueryHandleChain(root_subquery_id, subquery_id_chain_per_root,
                                   new_subquery_handle_ptr);
      if (!status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  const DelliciusQuery &query_;
  absl::flat_hash_map<std::string,
                      std::unique_ptr<QueryPlanner::SubqueryHandle>>
      id_to_subquery_handle_;
  absl::flat_hash_map<std::string, DelliciusQuery::Subquery> id_to_subquery_;
  Normalizer *normalizer_;
};

}  // namespace

// Builds the default query planner.
absl::StatusOr<std::unique_ptr<QueryPlannerInterface>> BuildQueryPlanner(
    const DelliciusQuery &query, RedPathRedfishQueryParams query_params,
    Normalizer *normalizer) {
  absl::StatusOr<SubqueryHandleCollection> subquery_handle_collection =
      SubqueryHandleFactory::CreateSubqueryHandles(query, normalizer);
  if (!subquery_handle_collection.ok()) {
    return subquery_handle_collection.status();
  }
  return std::make_unique<QueryPlanner>(
      query, *std::move(subquery_handle_collection), std::move(query_params));
}

}  // namespace ecclesia
