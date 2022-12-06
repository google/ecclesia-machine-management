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
//    auto qp = std::make_unique<QueryPlanner>(
//        query, query_params, &normalizer);
//    qp->Run(service_root, Clock::RealClock(), &tracker);
class QueryPlanner final : public QueryPlannerInterface {
 public:
  // Provides a subquery level abstraction to traverse RedPath step expressions
  // and apply predicate expression rules to refine a given node-set.
  class SubqueryHandle final {
   public:
    SubqueryHandle(const DelliciusQuery::Subquery &subquery,
                   Normalizer *normalizer);
    // Parses given Redfish Resource for attributes requested in the subquery
    // and prepares dataset to be appended in SubqueryOutput.
    void PrepareSubqueryOutput(const RedfishVariant &node,
                               DelliciusQueryResult &result);
    // Moves the RedPath iterator to next RedPath Step expression.
    // Returns false if there is no RedPath Step expression to process else true
    bool LoadNextRedPathStep();
    // Returns true if 'node' matches the predicate rule. If 'node' is part of a
    // Redfish Collection or array, 'node_index' must be the 'index' of 'node'
    // within the Collection or array. 'node_set_size' must be the size of the
    // Collection or array.
    bool ApplyPredicateRule(const RedfishVariant &node, size_t node_index,
                            size_t node_set_size);
    // Returns NodeName from current RedPath Step expression.
    std::optional<std::string> GetNodeNameFromRedPathStep() const;

   private:
    DelliciusQuery::Subquery subquery_;
    Normalizer *normalizer_;
    // Collection of RedPath Step expressions - (NodeName + Predicate) in the
    // RedPath of a Subquery.
    // Eg. /Chassis[*]/Sensors[1] - {(Chassis, *), (Sensors, 1)}
    std::vector<std::pair<std::string, std::string>> steps_in_redpath_;
    std::vector<std::pair<std::string, std::string>>::iterator iter_;
    // Indicates the validity of RedPath within subquery.
    bool is_redpath_valid_;
  };

  // Encapsulates key elements of a query operation.
  struct QueryExecutionContext {
    // Context node.
    std::unique_ptr<RedfishObject> redfish_object;
    // SubqueryHandle instances with the same context node.
    std::vector<SubqueryHandle> subquery_handles;
    // Last redpath expression executed by QueryPlanner.
    std::string last_executed_redpath;
  };
  QueryPlanner(const DelliciusQuery &query,
               RedPathRedfishQueryParams query_params, Normalizer *normalizer);
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
      QueryExecutionContext &execution_context, DelliciusQueryResult &result,
      QueryTracker *tracker);
  std::vector<SubqueryHandle> subquery_handles_;
  std::string plan_id_;
  RedPathRedfishQueryParams query_params_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_QUERY_PLANNER_H_
