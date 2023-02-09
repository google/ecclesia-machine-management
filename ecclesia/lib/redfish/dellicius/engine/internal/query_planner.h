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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_QUERY_PLANNER_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_QUERY_PLANNER_H_

#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

// QueryPlanner encapsulates the logic to interpret subqueries, deduplicate
// RedPath path expressions, dispatch an optimum number of redfish resource
// requests, and return normalized response data per given property
// specification.
// Usage:
//    auto qp = std::make_unique<QueryPlanner>(
//        query, subquery_handles, query_params);
//    qp->Run(service_root, Clock::RealClock(), &tracker);
class QueryPlanner final : public QueryPlannerInterface {
 public:
  // Provides a subquery level abstraction to traverse RedPath step expressions
  // and apply predicate expression rules to refine a given node-set.
  class SubqueryHandle final {
   public:
    using RedPathIterator =
        std::vector<std::pair<std::string, std::string>>::const_iterator;
    SubqueryHandle(
        const DelliciusQuery::Subquery &subquery,
        std::vector<std::pair<std::string, std::string>> redpath_steps,
        Normalizer *normalizer)
        : subquery_(subquery),
          normalizer_(normalizer),
          redpath_steps_(std::move(redpath_steps)) {}

    // Parses given 'redfish_object' for properties requested in the subquery
    // and prepares dataset to be appended in 'result'.
    // When subqueries are linked, the normalized dataset is added to the given
    // 'parent_subquery_dataset' instead of the 'result'
    absl::StatusOr<SubqueryDataSet *> Normalize(
        const RedfishObject &redfish_object, DelliciusQueryResult &result,
        SubqueryDataSet *parent_subquery_dataset);

    void AddChildSubqueryHandle(SubqueryHandle *child_subquery_handle) {
      child_subquery_handles_.push_back(child_subquery_handle);
    }

    std::vector<SubqueryHandle *> GetChildSubqueryHandles() const {
      return child_subquery_handles_;
    }

    // Returns true if encapsulated subquery does not have a root subquery.
    bool IsRootSubquery() const {
      return subquery_.root_subquery_ids().empty();
    }

    bool HasChildSubqueries() const { return !child_subquery_handles_.empty(); }

    void SetParentSubqueryDataSet(SubqueryDataSet *parent_subquery_data_set) {
      parent_subquery_data_set_ = parent_subquery_data_set;
    }

    RedPathIterator GetRedPathIterator() { return redpath_steps_.begin(); }

    bool IsEndOfRedPath(const RedPathIterator &iter) {
      return (iter != redpath_steps_.end()) &&
             (next(iter) == redpath_steps_.end());
    }

    std::string RedPathToString() const { return subquery_.redpath(); }

   private:
    DelliciusQuery::Subquery subquery_;
    Normalizer *normalizer_;
    // Collection of RedPath Step expressions - (NodeName + Predicate) in the
    // RedPath of a Subquery.
    // Eg. /Chassis[*]/Sensors[1] - {(Chassis, *), (Sensors, 1)}
    std::vector<std::pair<std::string, std::string>> redpath_steps_;
    // Index into RedPath step expressions
    size_t redpath_step_index_ = 0;
    std::vector<SubqueryHandle *> child_subquery_handles_;
    // Dataset of parent subquery to link the current subquery output with.
    SubqueryDataSet *parent_subquery_data_set_ = nullptr;
  };

  struct RedPathContext {
    // Pointer to the SubqueryHandle object the redpath iterator associates with
    SubqueryHandle *subquery_handle;
    // Dataset of the root RedPath to which the current RedPath dataset is
    // linked.
    SubqueryDataSet *root_redpath_dataset = nullptr;
    // Iterator configured to iterate over RedPath steps - NodeName and
    // Predicate pair
    SubqueryHandle::RedPathIterator redpath_steps_iterator;
  };

  struct ContextNode {
    // Redfish object serving as context node for RedPath expression.
    std::unique_ptr<RedfishObject> redfish_object;
    // RedPaths to execute relative to the Redfish Object as context node.
    std::vector<RedPathContext> redpath_ctx_multiple;
    // Last RedPath executed to get the Redfish object.
    std::string last_executed_redpath;
  };

  QueryPlanner(const DelliciusQuery &query,
               std::vector<std::unique_ptr<SubqueryHandle>> subquery_handles,
               RedPathRedfishQueryParams query_params)
      : plan_id_(query.query_id()),
        subquery_handles_(std::move(subquery_handles)),
        query_params_(std::move(query_params)) {}

  DelliciusQueryResult Run(const RedfishVariant &variant, const Clock &clock,
                           QueryTracker *tracker) override;

 private:
  // NodeToSubqueryHandles associates Redfish resource pointed by NodeName to
  // all subquery handles at a certain RedPath depth.
  // Example:
  //    SQ1 RedPath: /Chassis[*]/Processors[*]
  //    SQ2 RedPath: /Chassis[*]/Systems[*]
  //    NodeToSubqueryHandles at depth 1 : {"Chassis": {SQ1 Handle, SQ2 Handle}}
  using NodeToSubqueryHandles =
      absl::flat_hash_map<std::string, std::vector<SubqueryHandle>>;
  // Recursively executes RedPath Step expressions across subqueries.
  // Dispatches Redfish resource request for each unique NodeName in RedPath
  // Step expressions across subqueries followed by invoking predicate handlers
  // from each subquery to further refine the data that forms the context node
  // of next step expression in each qualified subquery.
  void ExecuteRedPathStepFromEachSubquery(
      ContextNode &context_node, DelliciusQueryResult &result,
      QueryTracker *tracker);
  const std::string plan_id_;
  // Collection of all SubqueryHandle instances including both root and child
  // handles.
  std::vector<std::unique_ptr<SubqueryHandle>> subquery_handles_;
  const RedPathRedfishQueryParams query_params_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_QUERY_PLANNER_H_
