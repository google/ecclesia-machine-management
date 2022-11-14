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
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

// QueryPlanner encapsulates the logic to interpret subqueries, deduplicate
// RedPath path expressions, dispatch an optimum number of redfish resource
// requests, and return normalized response data per given property
// specification.
// Usage:
//    auto qp = std::make_unique<QueryPlanner>(query, &normalizer);
//    qp->Run(service_root, Clock::RealClock(), result);
class QueryPlanner final : public QueryPlannerInterface {
 public:
  // Provides a subquery level abstraction to traverse RedPath step expressions
  // and apply predicate expression rules to refine a given node-set.
  class SubqueryHandle final {
   public:
    // Maps Redfish Resource to SubqueryHandle instances that use the mapped
    // Redfish Resource as context node to query next RedPath Step in their
    // respective path expressions.
    using ContextNodeToSubqueryHandles = absl::flat_hash_map<
        std::string, /* URI for the Redfish Resource serving as context node */
        std::pair<std::unique_ptr<RedfishObject>, /* Redfish Resource */
                  std::vector<SubqueryHandle>>>;
    // Descriptor used in Predicate level filter operation encapsulating
    // contextual information on node and node-set.
    struct FilterContext {
      RedfishVariant node;
      size_t node_index;
      size_t node_set_size;
    };
    // Encapsulates components of a RedPath Step expression - NodeName and
    // Predicate
    struct RedPathStep {
      std::string node_name;
      std::string predicate;
    };
    SubqueryHandle(const DelliciusQuery::Subquery &subquery,
                   Normalizer *normalizer);
    // Processes Redfish Resource filtered by Predicate Expression.
    // Normalizes the data if iterator has reached end of RedPath, else maps the
    // the Subquery to the Redfish resource in the given URI to Subquery map to
    // continue the query operations for next RedPath Step expression using the
    // filtered Redfish Resource as the context node.
    void NormalizeOrContinueQuery(
        const RedfishVariant &node,
        ContextNodeToSubqueryHandles &context_node_to_sq,
        DelliciusQueryResult &result);
    // Filters given node-set using predicate expressions.
    bool ApplyPredicateRule(const FilterContext &filter_context);
    // Returns NodeName in next Step expression in RedPath.
    // Returns Error if SubqueryHandle is not initialized.
    std::optional<std::string> GetNodeNameFromRedPathStep() const;

   private:
    DelliciusQuery::Subquery subquery_;
    Normalizer *normalizer_;
    // Collection of RedPath Step expressions in a subquery.
    std::vector<RedPathStep> steps_in_redpath_;
    std::vector<RedPathStep>::iterator iter_;
    // Indicates the validity of RedPath within subquery.
    bool is_redpath_valid_;
  };
  QueryPlanner(const DelliciusQuery &query, Normalizer *normalizer);
  void Run(const RedfishVariant &variant, const Clock &clock,
           DelliciusQueryResult &result) override;

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
      const RedfishObject *redfish_object,
      std::vector<SubqueryHandle> &subquery_handles,
      DelliciusQueryResult &result);
  std::vector<SubqueryHandle> subquery_handles_;
  std::string plan_id_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_QUERY_PLANNER_H_
