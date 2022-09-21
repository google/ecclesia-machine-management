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

#include "ecclesia/lib/redfish/dellicius/engine/internal/query_planner.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/time/proto.h"

namespace ecclesia {

namespace {

constexpr absl::string_view kPredicateSelectAll = "*";

// TODO (b/241784544): Add support for other Predicate Expr used in Redpath.
bool ApplySelectAllFilter(const RedfishVariant &/*variant*/) { return true; }

// TODO (b/241784544): Expand the validity check using regex for all predicates.
// Only Checks if predicate expression is enclosed in square brackets.
absl::StatusOr<std::pair<std::string, std::string>> GetNodeAndPredicate(
    absl::string_view step) {
  size_t predicate_start = step.find_first_of('[');
  size_t predicate_end = step.find_first_of(']');
  if ((predicate_start == std::string::npos)
      || (predicate_end == std::string::npos)) {
    return absl::InvalidArgumentError("Invalid location step expression");
  }
  absl::string_view predicate_expr = step.substr(
      predicate_start + 1, (predicate_end - predicate_start - 1));
  absl::string_view node_name = step.substr(0, predicate_start);
  return std::make_pair(std::string(node_name), std::string(predicate_expr));
}

}  // namespace

SubqueryHandle::SubqueryHandle(const DelliciusQuery::Subquery &subquery)
    : subquery_(subquery) {
  // Step expressions in Subquery's redpath are split into pairs of node name
  // and predicate expression handlers.
  for (absl::string_view step_expression : absl::StrSplit(
      subquery.redpath(), '/', absl::SkipEmpty())) {
    // TODO (b/241784544): Add logic to validate Redpath and support
    // malformed path error code
    absl::StatusOr<std::pair<std::string, std::string>> node_x_predicate
        = GetNodeAndPredicate(step_expression);
    if (!node_x_predicate.ok()) {
      ecclesia::ErrorLog() << node_x_predicate.status();
      return;
    }
    auto [node_name, predicate] = node_x_predicate.value();
    if (predicate == kPredicateSelectAll) {
      steps_in_redpath_.emplace_back(node_name, ApplySelectAllFilter);
    } else {
      ecclesia::ErrorLog() << "Unknown predicate " << predicate;
      return;
    }
  }

  // An iterator is configured to traverse the Step expressions in subquery's
  // redpath as redfish requests are dispatched.
  iter_ = steps_in_redpath_.begin();
  continue_ = true;
}

std::optional<std::string> SubqueryHandle::NextNodeInRedpath() const {
  if (continue_) { return iter_->first; }
  return std::nullopt;
}

void SubqueryHandle::FilterNodeSet(const RedfishVariant &redfish_variant,
                                   NormalizerCallback normalizer,
                                   DelliciusQueryResult &response) {
  // Apply the predicate rule on the given RedfishObject.
  continue_ = iter_->second(redfish_variant);
  // If it is the last step expression in the redpath, stop the traversal and
  // normalize.
  if (!steps_in_redpath_.empty()
      && iter_->first == steps_in_redpath_.back().first) {
    // Normalize the data if final Step expression successfully applies on the
    // redfish object.
    if (continue_) { normalizer(redfish_variant, subquery_, response); }
    continue_ = false;
  } else {
    ++iter_;
  }
}

void QueryPlanner::QualifyEachSubquery(
      const RedfishVariant &var,
      std::vector<SubqueryHandle> handles,
      DelliciusQueryResult &result) {
  for (auto &subquery_handle : handles) {
    subquery_handle.FilterNodeSet(var, normalizer_cb_, result);
  }
  RunRecursive(var, handles, result);
}

void QueryPlanner::Dispatch(
    const RedfishVariant &var,
    NodeToSubqueryHandles &resource_x_sq,
    DelliciusQueryResult &result) {
  // Dispatch Redfish resource requests for each unique resource identified.
  for (auto &[resource_name, handles] : resource_x_sq) {
    auto variant = var[resource_name];
    if (variant.AsObject() == nullptr) { continue; }
    std::unique_ptr<RedfishIterable> collection = variant.AsIterable();
    // If resource is a collection, qualify for each resource in the collection.
    if (collection != nullptr) {
      for (const auto member : *collection) {
        QualifyEachSubquery(member, handles, result);
      }
    } else {  // Qualify when resource is a singleton.
      QualifyEachSubquery(variant, handles, result);
    }
  }
}

QueryPlanner::NodeToSubqueryHandles QueryPlanner::DeduplicateExpression(
    const RedfishVariant &var,
    std::vector<SubqueryHandle> &subquery_handles,
    DelliciusQueryResult &result) {
  NodeToSubqueryHandles node_to_subquery;
  for (auto &subquery_handle : subquery_handles) {
    // Pair resource name and those subqueries that have this resource as next
    // element in their respective redpaths
    auto node_name = subquery_handle.NextNodeInRedpath();
    if (node_name != std::nullopt) {
      node_to_subquery[*node_name].push_back(subquery_handle);
    }
  }
  return node_to_subquery;
}

void QueryPlanner::RunRecursive(
    const RedfishVariant &variant,
    std::vector<SubqueryHandle> &subquery_handles,
    DelliciusQueryResult &result) {
  NodeToSubqueryHandles node_to_subquery = DeduplicateExpression(
      variant, subquery_handles, result);
  if (node_to_subquery.empty()) return;
  Dispatch(variant, node_to_subquery, result);
}

void QueryPlanner::Run(const RedfishVariant &variant,
                       const Clock &clock,
                       DelliciusQueryResult &result) {
  auto timestamp = AbslTimeToProtoTime(clock.Now());
  if (timestamp.ok()) {
    result.mutable_start_timestamp()->CopyFrom(*std::move(timestamp));
  }
  RunRecursive(variant, subquery_handles_, result);
  timestamp = AbslTimeToProtoTime(clock.Now());
  if (timestamp.ok()) {
    result.mutable_end_timestamp()->CopyFrom(*std::move(timestamp));
  }
}

QueryPlanner::QueryPlanner(const DelliciusQuery &query,
                           NormalizerCallback normalizer)
    : normalizer_cb_(normalizer), plan_id_(query.query_id()) {
  // Create subquery handles aka subquery plans.
  for (const auto &subquery : query.subquery()) {
    auto subquery_handle = SubqueryHandle(subquery);
    if (subquery_handle.NextNodeInRedpath() == std::nullopt) { continue; }
    subquery_handles_.push_back(std::move(subquery_handle));
  }
}

}  // namespace ecclesia
