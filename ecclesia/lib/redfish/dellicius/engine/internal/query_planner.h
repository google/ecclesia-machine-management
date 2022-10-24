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
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/function_ref.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

// QueryPlanner encapsulates the logic to interpret subqueries, deduplicate
// redpath path expressions, dispatch an optimum number of redfish resource
// requests, and return normalized response data for given data model.
// Usage:
//    auto qp = std::make_unique<QueryPlanner>(query, &normalizer);
//    qp->Run(service_root, Clock::RealClock(), result);
class QueryPlanner final : public QueryPlannerInterface {
 public:
  QueryPlanner(const DelliciusQuery &query, Normalizer *normalizer);
  //   TODO(b/241784544): Does not handle queries targetting a collection.
  //   TODO(b/241784544): Does not handle arrays types.
  void Run(const RedfishVariant &variant, const Clock &clock,
           DelliciusQueryResult &result) override;

 private:
  // Defines return values for a filter operation using predicate expressions.
  enum class PredicateReturnValue {
    kContinue = 0,  // node-set successfully refined using predicate.
    kEndOfRedpath,  // node-set successfully refined and end of redpath reached.
    kEndByPredicate  // node-set cannot be further refined using the predicate.
  };
  // Provides a subquery level abstraction to traverse redpath step expressions
  // and apply predicate expression rules to refine a given node-set.
  class SubqueryHandle final {
   public:
    explicit SubqueryHandle(const DelliciusQuery::Subquery &subquery);
    DelliciusQuery::Subquery GetSubquery() const { return subquery_; }
    std::optional<std::string> NextNodeInRedpath() const;
    // Filters node-set pointed by given redfish variant by invoking the
    // predicate handler associated with current 'Step' expression in redpath.
    PredicateReturnValue FilterNodeSet(const RedfishVariant &redfish_variant);

   private:
    // A redpath step is a pair of NodeTest expression and Predicate.
    // The alias pairs the qualified name of redfish resource and a callback
    // implementing predicate.
    using RedpathStep =
        std::pair<std::string, absl::FunctionRef<bool(const RedfishVariant &)>>;
    DelliciusQuery::Subquery subquery_;
    std::vector<RedpathStep> steps_in_redpath_;
    std::vector<RedpathStep>::iterator iter_;
    // Indicates the validity of redpath within subquery.
    bool is_redpath_valid_;
  };
  // NodeToSubqueryHandles associates Redfish resource pointed by NodeTest to
  // all subquery handles at a certain redpath depth.
  // Example:
  //    SQ1 Redpath: /Chassis[*]/Processors[*]
  //    SQ2 Redpath: /Chassis[*]/Systems[*]
  //    NodeToSubqueryHandles at depth 1 : {"Chassis": {SQ1 Handle, SQ2 Handle}}
  using NodeToSubqueryHandles =
      absl::flat_hash_map<std::string, std::vector<SubqueryHandle>>;
  // Dispatches redfish resource request for each unique redfish resource at a
  // particular depth in redpath and further refines the output node-set using
  // filtering expressions from each subquery.
  void Dispatch(const RedfishVariant &var, NodeToSubqueryHandles &resource_x_sq,
                DelliciusQueryResult &result);
  // Invokes predicate handlers from each subquery to further refine the data
  // that forms the basis of next step expression in each qualified subquery.
  void QualifyEachSubquery(const RedfishVariant &var,
                           std::vector<SubqueryHandle> handles,
                           DelliciusQueryResult &result);
  // Query plan runner responsible for deduplicating and dispatching Step
  // expressions recursively (indirect) till all redpaths are processed.
  void RunRecursive(const RedfishVariant &variant,
                    std::vector<SubqueryHandle> &subquery_handles,
                    DelliciusQueryResult &result);
  // Deduplicates the next NodeTest expression in the redpath of each subquery
  // and returns NodeTest to SubqueryHandle map.
  static NodeToSubqueryHandles DeduplicateExpression(
      std::vector<SubqueryHandle> &subquery_handles,
      DelliciusQueryResult &result);
  // Callback function encapsulating the logic to normalize redfish response
  // data for specific data model.
  Normalizer *normalizer_;
  std::vector<SubqueryHandle> subquery_handles_;
  std::string plan_id_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_QUERY_PLANNER_H_
