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

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "google/rpc/code.pb.h"
#include "google/rpc/status.pb.h"
#include "absl/base/attributes.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/function_ref.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_errors.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/join.h"
#include "ecclesia/lib/redfish/dellicius/utils/path_util.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_predicates/predicates.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/lib/time/proto.h"
#include "single_include/nlohmann/json.hpp"
#include "re2/re2.h"

namespace ecclesia {

namespace {
// Pattern for location step: NodeName[Predicate]
constexpr LazyRE2 kLocationStepRegex = {
    "^([a-zA-Z#@][0-9a-zA-Z.]+|)(?:\\[(.*?)\\]|)$"};
constexpr absl::string_view kPredicateSelectAll = "*";

// All RedPath expressions execute relative to service root identified by '/'.
constexpr absl::string_view kServiceRootNode = "/";
constexpr absl::string_view kDefaultRedfishServiceRoot = "/redfish/v1";

using ecclesia::RedfishInterface;
using RedPathStep = std::pair<std::string, std::string>;

// Encapsulates information relevant per redfish object that is queried during
// the overall query execution process; used to tag redfish jsons when logging.
struct TraceInfo {
  std::string redpath_prefix;
  std::string query_id;
  std::vector<std::string> subquery_ids;
};

// Class that encapsulates redpath and predicates.
// If the predicates are templated, then this class contains predicates that
// are resolved with run time values.
// Example:
// redpath: "/Chassis[*]/Sensors[ReadingType=$Type and Reading>$Threshold]"
// On user providing the folowing variables at run time:
// Type = Temperature, Threshold = 50
// this class instance will contain:
// nodes_: {"Chassis", "Sensors"}
// predicates_: {"*", "ReadingType=Temperature and Reading>50"}
class RedPathSteps final {
 public:
  RedPathSteps(const RedPathSteps &) = default;
  RedPathSteps(RedPathSteps &&) = default;
  RedPathSteps &operator=(const RedPathSteps &) = default;
  RedPathSteps &operator=(RedPathSteps &&) = default;
  RedPathSteps(const std::vector<std::string> *nodes,
               std::shared_ptr<std::vector<std::string>> predicates)
     : nodes_(nodes), predicates_(std::move(predicates)) {
    if (nodes_ != nullptr) {
      nodes_iterator_ = nodes_->begin();
    }
    if (predicates_ != nullptr) {
      predicates_iterator_ = predicates_->begin();
    }
  }

  bool IsEndOfRedPath() const {
    if (nodes_ == nullptr) return true;
    return ((nodes_iterator_ != nodes_->end()) &&
           (std::next(nodes_iterator_) == nodes_->end()));
  }

  bool IsPredicateEmpty() const {
    if (predicates_ == nullptr || predicates_->empty()) return true;
    return predicates_iterator_->empty();
  }

  bool IsNodeEmpty() const {
    if (nodes_ == nullptr || nodes_->empty()) return true;
    return nodes_iterator_->empty();
  }

  std::string GetPredicate() const {
    if (IsPredicateEmpty()) return "";
    return *predicates_iterator_;
  }

  std::string GetNode() const {
    if (IsNodeEmpty()) return "";
    return *nodes_iterator_;
  }
  // Prefix increment operator
  void increment() {
    if (nodes_ == nullptr) return;
    nodes_iterator_++;
    if (predicates_ == nullptr) return;
    predicates_iterator_++;
  }

 private:
  // The member variable nodes_ points to the nodes in SubqueryHandle.
  // SubqueryHandle objects must exist beyond the life of RedPathStep objects.
  const std::vector<std::string> *nodes_;
  std::shared_ptr<std::vector<std::string>> predicates_;
  std::vector<std::string>::const_iterator nodes_iterator_;
  std::vector<std::string>::const_iterator predicates_iterator_;
};

// Gets RedfishObject from given RedfishVariant.
// Issues fresh query if the object is served from cache and freshness is
// required.
std::unique_ptr<RedfishObject> GetRedfishObjectWithFreshness(
    const RedfishVariant &variant, const GetParams &params,
    const std::optional<TraceInfo> &trace_info) {
  std::unique_ptr<RedfishObject> redfish_object = variant.AsObject();
  if (!redfish_object) return nullptr;
  if (trace_info.has_value()) {
    LOG(INFO) << "Redfish Object Trace:"
              << "\nredpath prefix: " << trace_info->redpath_prefix
              << "\nquery_id: " << trace_info->query_id << "\nsubquery_ids: "
              << absl::StrJoin(trace_info->subquery_ids, ", ") << '\n'
              << redfish_object->GetContentAsJson().dump(1);
  }
  if (params.freshness != GetParams::Freshness::kRequired) {
    return redfish_object;
  }
  absl::StatusOr<std::unique_ptr<RedfishObject>> refetch_obj =
      redfish_object->EnsureFreshPayload();
  if (!refetch_obj.ok()) return nullptr;
  return std::move(refetch_obj.value());
}

// Fetch a Redfish resource using the redfish_interface
ecclesia::RedfishVariant FetchUri(RedfishInterface *redfish_interface,
                                  absl::string_view uri,
                                  ecclesia::GetParams params = {}) {
  if (redfish_interface == nullptr) {
    return RedfishVariant(absl::InternalError("Null redfish interface"));
  }
  return params.freshness == ecclesia::GetParams::Freshness::kRequired
             ? redfish_interface->UncachedGetUri(uri, std::move(params))
             : redfish_interface->CachedGetUri(uri, std::move(params));
}

// Set the Timestamp object from the given clock
void SetTime(const Clock &clock, google::protobuf::Timestamp &field) {
  auto time = clock.Now();
  if (auto timestamp = AbslTimeToProtoTime(time); timestamp.ok()) {
    field = *std::move(timestamp);
  }
}

// Creates RedPathStep objects from the given RedPath string.
absl::StatusOr<std::vector<RedPathStep>> RedPathToSteps(
    absl::string_view redpath) {
  std::vector<RedPathStep> steps;

  // When queried node is service root itself.
  if (redpath == kServiceRootNode) {
    steps.push_back({});
    return steps;
  }

  for (absl::string_view step_expression :
       absl::StrSplit(redpath, '/', absl::SkipEmpty())) {
    std::string node_name;
    std::string predicate;
    if (!RE2::FullMatch(step_expression, *kLocationStepRegex, &node_name,
                        &predicate) ||
        node_name.empty()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Cannot parse Step expression %s in RedPath %s",
                          step_expression, redpath));
    }
    steps.push_back({node_name, predicate});
  }
  return steps;
}

// Returns true if child RedPath is in expand path of parent RedPath.
bool IsInExpandPath(absl::string_view child_redpath,
                    absl::string_view parent_redpath,
                    size_t parent_expand_levels) {
  size_t expand_levels = 0;
  // Get the diff expression between the 2 RedPaths
  // Example diff for paths /Chassis[*] and /Chassis[*]/Sensors[*] would be
  // /Sensors[*]
  absl::string_view diff = child_redpath.substr(parent_redpath.length());
  std::vector<absl::string_view> step_expressions =
      absl::StrSplit(diff, '/', absl::SkipEmpty());
  // Now count the possible expand levels in the diff expresion.
  for (absl::string_view step_expression : step_expressions) {
    std::string node_name;
    std::string predicate;
    if (RE2::FullMatch(step_expression, *kLocationStepRegex, &node_name,
                       &predicate)) {
      if (!node_name.empty()) {
        ++expand_levels;
      }
      if (!predicate.empty()) {
        ++expand_levels;
      }
    }
  }
  return expand_levels <= parent_expand_levels;
}

GetParams GetQueryParamsForRedPath(
    const RedPathRedfishQueryParams &redpath_to_query_params,
    absl::string_view redpath_prefix) {
  // Set default GetParams value as follows:
  //   Default freshness is Optional
  //   Default Redfish query parameter setting is no expand or filter!
  auto params = GetParams{.freshness = GetParams::Freshness::kOptional,
                          .expand = std::nullopt,
                          .filter = std::nullopt};

  // Get RedPath specific configuration for top, expand, and freshness
  if (auto iter = redpath_to_query_params.find(redpath_prefix);
      iter != redpath_to_query_params.end()) {
    params = iter->second;
  }
  return params;
}

// Returns combined GetParams{} for RedPath expressions in the query.
// There are 2 places where we get parameters associated with RedPath prefix:
// 1) Expand configuration from embedded query_rule file
// 2) Freshness configuration in the Query itself
// 3) Filter configuration toggled in query_rule and derived from predicate.
// In this function, we merge Freshness requirement with expand configuration
// for the redpath prefix.
RedPathRedfishQueryParams CombineQueryParams(
    const DelliciusQuery &query,
    RedPathRedfishQueryParams redpath_to_query_params) {
  absl::flat_hash_map<std::string, DelliciusQuery::Subquery> subquery_map;
  for (const auto &subquery : query.subquery()) {
    subquery_map[subquery.subquery_id()] = subquery;
  }

  // Join all subqueries to form absolute RedPaths
  absl::flat_hash_set<std::vector<std::string>> all_joined_subqueries;
  absl::Status status = JoinSubqueries(query, all_joined_subqueries);
  if (!status.ok()) {
    LOG(ERROR) << "Cannot join subqueries to extract freshness configuration."
               << status.message();
    return {};
  }

  // For each RedPath prefix in joined subquery, get freshness requirement from
  // corresponding subquery.
  for (const auto &subquery_id_list : all_joined_subqueries) {
    std::string redpath_prefix;
    for (const auto &subquery_id : subquery_id_list) {
      auto iter = subquery_map.find(subquery_id);
      if (iter == subquery_map.end()) {
        LOG(ERROR) << "Subquery not found for id: " << subquery_id;
        return {};
      }
      std::string redpath_str = iter->second.redpath();

      // Convert all predicates in RedPath to [*].
      // This is done because engine fetches all members in a collection before
      // applying a predicate expression to filter, which internally is [*]
      // operation.
      RE2::GlobalReplace(&redpath_str, "\\[(.*?)\\]", "[*]");
      if (!absl::StartsWith(redpath_str, "/")) {
        absl::StrAppend(&redpath_prefix, "/", redpath_str);
      } else {
        absl::StrAppend(&redpath_prefix, redpath_str);
      }

      if (iter->second.freshness() != DelliciusQuery::Subquery::REQUIRED) {
        continue;
      }

      auto find_query_params = redpath_to_query_params.find(redpath_prefix);
      if (find_query_params != redpath_to_query_params.end()) {
        find_query_params->second.freshness = GetParams::Freshness::kRequired;
      } else {
        redpath_to_query_params.insert(
            {redpath_prefix,
             GetParams{.freshness = GetParams::Freshness::kRequired}});
      }
    }
  }

  // Now we adjust freshness configuration such that if a RedPath expression
  // has a freshness requirement but is in the expand path of parent RedPath
  // the freshness requirement bubbles up to the parent RedPath
  // Example: /Chassis[*]/Sensors, $expand=*($levels=1) will assume the
  // freshness setting for path /Chassis[*]/Sensors[*].
  absl::string_view last_redpath_with_expand;
  GetParams *last_params = nullptr;
  absl::btree_map<std::string, GetParams> redpaths_to_query_params_ordered{
      redpath_to_query_params.begin(), redpath_to_query_params.end()};
  for (auto &[redpath, params] : redpaths_to_query_params_ordered) {
    if (params.freshness == GetParams::Freshness::kRequired &&
        // Check if last RedPath is prefix of current RedPath.
        absl::StartsWith(redpath, last_redpath_with_expand) &&
        // Check whether last RedPath uses query parameters.
        last_params != nullptr &&
        // Check if the RedPath prefix has an expand configuration.
        last_params->expand.has_value() &&
        // Check if current RedPath is in expand path of last RedPath.
        IsInExpandPath(redpath, last_redpath_with_expand,
                       last_params->expand->levels()) &&
        // Make sure the last redpath expression is not already fetched fresh.
        last_params->freshness == GetParams::Freshness::kOptional) {
      last_params->freshness = GetParams::Freshness::kRequired;
    }

    if (params.expand.has_value() && params.expand->levels() > 0) {
      last_redpath_with_expand = redpath;
      last_params = &params;
    }
  }
  return {redpaths_to_query_params_ordered.begin(),
          redpaths_to_query_params_ordered.end()};
}

// If there is an unpopulated variable the predicate cannot be completely
// invalidated in case there are other conditions in the predicate with
// populated variables. To facilitate this we need to break down the predicate
// and remove only the condition with the unpopulated variable. The input
// predicate has already been substituted with the variable values provided. Any
// remaining variables (string prefixed with $) are to be removed.
//
// Example:
//
// [ReadingType=$Type and ReadingUnits=$Units and Reading>$Threshold]
//        Variable Map: Type = Temperature, Threshold = 50
//
//  After processing this predicate will resolve to:
//
// [ReadingType=Temperature and Reading>50]
//
std::string InvalidateUnpopulatedVariables(absl::string_view predicate) {
  std::vector<absl::string_view> expressions =
      SplitExprByDelimiterWithEscape(predicate, " ", '\\');
  // For a single expression, just return the select all token.
  if (expressions.size() == 1) {
    return std::string(kPredicateSelectAll);
  }
  std::vector<absl::string_view> new_expressions;
  bool skip_next = false;
  int index = 0;
  for (absl::string_view expr : expressions) {
    if (skip_next) {
      skip_next = false;
      index++;
      continue;
    }
    if (absl::StrContains(expr, '$')) {
      if (index == expressions.size() - 1) {
        // Since this is the last expression we need to remove the logical
        // operator seen before. If nothing has been added to the list of new
        // expressions, return select all. This means that no variables have
        // been populated.
        if (new_expressions.empty()) return std::string(kPredicateSelectAll);
        new_expressions.pop_back();
      } else {
        // the next element is the logical operator, skip it to remove from
        // predicate.
        skip_next = true;
      }
    } else {
      new_expressions.push_back(expr);
    }
    index++;
  }
  return absl::StrJoin(new_expressions, " ");
}

// Provides a subquery level abstraction to traverse RedPath step expressions
// and apply predicate expression rules to refine a given node-set or fetch a
// Redfish object directly using uri_reference_redpath.
class SubqueryHandle final {
 public:
  SubqueryHandle(const DelliciusQuery::Subquery &subquery,
                 std::vector<std::pair<std::string, std::string>> redpath_steps,
                 Normalizer *normalizer)
      : subquery_(subquery),
        normalizer_(normalizer) {
    for (auto &pair : redpath_steps) {
      nodes_.push_back(std::move(pair.first));
      predicates_.push_back(std::move(pair.second));
    }
  }

  // Parses given 'redfish_object' for properties requested in the subquery
  // and prepares dataset to be appended in 'result'.
  // When subqueries are linked, the normalized dataset is added to the given
  // 'parent_subquery_dataset' instead of the 'result'
  absl::StatusOr<SubqueryDataSet *> Normalize(
      const RedfishObject &redfish_object, DelliciusQueryResult &result,
      SubqueryDataSet *parent_subquery_dataset,
      const std::function<bool(const DelliciusQueryResult &result)> &callback =
          nullptr);

  void AddChildSubqueryHandle(SubqueryHandle *child_subquery_handle) {
    child_subquery_handles_.push_back(child_subquery_handle);
  }

  const std::vector<SubqueryHandle *> &GetChildSubqueryHandles() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return child_subquery_handles_;
  }

  // Returns true if the encapsulated subquery does not have a root_subquery_id
  // set.
  bool IsRootSubquery() const { return subquery_.root_subquery_ids().empty(); }

  bool IsChildSubquery() const { return !IsRootSubquery(); }

  bool HasChildSubqueries() const { return !child_subquery_handles_.empty(); }

  void SetParentSubqueryDataSet(SubqueryDataSet *parent_subquery_data_set) {
    parent_subquery_data_set_ = parent_subquery_data_set;
  }

  // Substitute variables in the predicates and return a fully resolved redpath
  // steps.
  RedPathSteps SubstituteVariables(const QueryVariables &variables) {
    std::vector<std::pair<std::string, std::string>> replacements;
    replacements.reserve(variables.values_size());
    // Build the list of replacements that will be passed into StrReplaceAll.
    for (const auto &value : variables.values()) {
      if (value.name().empty()) continue;
      std::string result;
      std::string variable_name = absl::StrCat("$", value.name());
      replacements.push_back(std::make_pair(variable_name, value.value()));
    }

    // The `new_predicates` variable is a concrete version of `predicates_`
    // with fully resolved templated arguments. It is created once per query.
    // The new_predicates is a shared_ptr for the following reasons:
    // a. A unique_ptr allows only move constructor. The usage pattern
    // of the `new_predicates` object throughout this code requires
    // copy construction of the predicates object.
    // b. Retaining the ownership of `new_predicates` object in SubqueryHandle
    // and using a raw pointer in RedPathSteps was explored but rejected due to
    // the complexity of handling the lifetime of these objects. Both mutex
    // based and TLS based solutions were explored.
    // In the end shared_ptr was preferred by the team as the most elegant and
    // maintainable approach.

    auto new_predicates = std::make_shared<std::vector<std::string>>();
    new_predicates->reserve(predicates_.size());

    for (const auto &predicate : predicates_) {
      std::string new_predicate = predicate;
      new_predicate = absl::StrReplaceAll(new_predicate, replacements);
      // If after the variable replacement there is still an unfilled variable,
      // remove the predicate step. This will be equivalent to a match-all/*.
      if (absl::StrContains(new_predicate, '$')) {
        LOG(WARNING) << "Unmatched variable within predicate: "
                     << predicate << "[" << new_predicate << "]"
                     << ". Removing predicate step.";
        new_predicate =
            InvalidateUnpopulatedVariables(new_predicate);
      }
      new_predicates->push_back(std::move(new_predicate));
    }
    // SubqueryHandle instance MUST outlast the RedPathSteps instance
    // that uses the address of nodes_. The new_predicates shared pointer will
    // be owned only by RedPathSteps instance after this function returns.
    RedPathSteps new_redpath_steps(&nodes_, new_predicates);
    return new_redpath_steps;
  }

  RedPathSteps GetRedPathSteps() {
    auto new_predicates = std::make_shared<std::vector<std::string>>();
    new_predicates->reserve(predicates_.size());
    for (const auto &predicate : predicates_) {
      new_predicates->push_back(predicate);
    }

    RedPathSteps new_redpath_steps(&nodes_, new_predicates);
    return new_redpath_steps;
  }

  bool HasRedPath() const { return subquery_.has_redpath(); }
  std::string RedPathToString() const {
    if (HasRedPath()) {
      return subquery_.redpath();
    }
    return "";
  }

  bool HasFetchRawData() const { return subquery_.has_fetch_raw_data(); }
  const DelliciusQuery::Subquery::RawData & GetFetchRawData() const {
    return subquery_.fetch_raw_data();
  }

  bool HasUri() const { return subquery_.has_uri(); }
  std::string UriToString() const {
    if (HasUri()) { return subquery_.uri(); }
    return "";
  }
  absl::string_view GetUri() const {
    if (HasUri()) { return subquery_.uri(); }
    return "";
  }

  bool HasUriReferenceRedpath() const {
    return subquery_.has_uri_reference_redpath();
  }
  std::string UriReferenceRedpathToString() const {
    if (HasUriReferenceRedpath()) {
      return subquery_.uri_reference_redpath();
    }
    return "";
  }
  absl::string_view UriReferenceRedpath() const {
    return subquery_.uri_reference_redpath();
  }

  std::string GetSubqueryId() const { return subquery_.subquery_id(); }

  // Terminates redpath step traversal in a subquery.
  void TerminateSubquery() { terminate_subquery_ = true; }

  bool IsSubqueryTerminated() const { return terminate_subquery_; }

 private:
  DelliciusQuery::Subquery subquery_;
  Normalizer *normalizer_;
  // Two separate collections of RedPath Step expressions
  // - (NodeName + Predicate) in the RedPath of a Subquery.
  // Eg. /Chassis[*]/Sensors[1]
  // nodes -      {Chassis, Sensors}
  // predicates - {*, 1}
  std::vector<std::string> nodes_;
  std::vector<std::string> predicates_;
  std::vector<SubqueryHandle *> child_subquery_handles_;
  // Dataset of parent subquery to link the current subquery output with.
  SubqueryDataSet *parent_subquery_data_set_ = nullptr;
  // Terminate the subquery if the response size limit is reached.
  bool terminate_subquery_ = false;
};

struct RedPathContext {
  // Pointer to the SubqueryHandle object the RedPath associates with.
  SubqueryHandle *subquery_handle;
  // Dataset of the root RedPath to which the current RedPath dataset is
  // linked.
  SubqueryDataSet *root_redpath_dataset = nullptr;
  // An object containing RedPath steps with substituted variables in the
  // predicates.
  RedPathSteps redpath_steps;
  // Client callback to send the subquery results
  std::function<bool(const DelliciusQueryResult &result)> callback = nullptr;
};

// A ContextNode describes the RedfishObject relative to which one or more
// RedPath expressions or uri_reference_redpath is executed along with metadata
// necessary for the query operation and tracking.
struct ContextNode {
  // Redfish object relative to which RedPath expression executes.
  std::unique_ptr<RedfishObject> redfish_object;
  // RedPath contexts to execute relative to the Redfish Object.
  std::vector<RedPathContext> redpath_ctx_multiple;
  // Last RedPath executed to get the Redfish object.
  std::string last_executed_redpath;
};

// QueryPlanner encapsulates the logic to interpret subqueries, deduplicate
// RedPath path expressions, dispatch an optimum number of redfish resource
// requests, and return normalized response data per given property
// specification.
class QueryPlanner final : public QueryPlannerInterface {
 public:
  QueryPlanner(const DelliciusQuery &query,
               std::vector<std::unique_ptr<SubqueryHandle>> subquery_handles,
               RedPathRedfishQueryParams redpath_to_query_params,
               RedfishInterface *redfish_interface = nullptr)
      : plan_id_(query.query_id()),
        subquery_handles_(std::move(subquery_handles)),
        redpath_to_query_params_(
            CombineQueryParams(query, std::move(redpath_to_query_params))),
        redfish_interface_(redfish_interface),
        service_root_(query.has_service_root()
                          ? query.service_root()
                          : std::string(kDefaultRedfishServiceRoot)) {}

  DelliciusQueryResult Run(
      const Clock &clock, QueryTracker *tracker,
      const QueryVariables &variables, const RedfishMetrics *metrics = nullptr,
      ExecutionFlags execution_flags = {
          .execution_mode = ExecutionFlags::ExecutionMode::kFailOnFirstError,
          .log_redfish_traces = false}) override;

  DelliciusQueryResult Run(
      const RedfishVariant &variant, const Clock &clock, QueryTracker *tracker,
      const QueryVariables &variables, const RedfishMetrics *metrics = nullptr,
      ExecutionFlags execution_flags = {
          .execution_mode = ExecutionFlags::ExecutionMode::kFailOnFirstError,
          .log_redfish_traces = false}) override;

  void Run(const RedfishVariant &variant, const Clock &clock,
           QueryTracker *tracker, const QueryVariables &variables,
           absl::FunctionRef<bool(const DelliciusQueryResult &result)> callback,
           const RedfishMetrics *metrics = nullptr) override;

  void ProcessSubqueries(
      const RedfishVariant &variant, const QueryVariables &variables,
      std::function<bool(const DelliciusQueryResult &result)> callback,
      DelliciusQueryResult &result, QueryTracker *tracker,
      ExecutionFlags execution_flags = {});

  RedfishVariant FetchUriReference(const ContextNode &context_node,
                                   absl::string_view node_name,
                                   DelliciusQueryResult &result,
                                   QueryTracker *tracker) const;

  RedfishVariant FetchUri(absl::string_view uri,
                          QueryTracker *tracker = nullptr) const;

 private:
  const std::string plan_id_;
  // Collection of all SubqueryHandle instances including both root and child
  // handles.
  std::vector<std::unique_ptr<SubqueryHandle>> subquery_handles_;
  const RedPathRedfishQueryParams redpath_to_query_params_;
  RedfishInterface *redfish_interface_;
  const std::string service_root_;
};

using SubqueryHandleCollection = std::vector<std::unique_ptr<SubqueryHandle>>;

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

  absl::StatusOr<SubqueryHandleCollection> GetSubqueryHandles();

  // Builds SubqueryHandle objects for subqueries linked together in a chain
  // through 'root_subquery_ids' property.
  // Args:
  //   subquery_id: Identifier of the subquery for which SubqueryHandle is
  //   built.
  //   subquery_id_chain: Stores visited ids to help identify loop in
  //   chain.
  //   child_subquery_handle: last built subquery handle to link as
  //   child node.
  absl::Status BuildSubqueryHandleChain(
      const std::string &subquery_id,
      absl::flat_hash_set<std::string> &subquery_id_chain,
      SubqueryHandle *child_subquery_handle);

  const DelliciusQuery &query_;
  absl::flat_hash_map<std::string, std::unique_ptr<SubqueryHandle>>
      id_to_subquery_handle_;
  absl::flat_hash_map<std::string, DelliciusQuery::Subquery> id_to_subquery_;
  Normalizer *normalizer_;
};

// Executes the next predicate expression in each RedPath and returns those
// RedPath contexts whose filter criteria is met by the given RedfishObject.
std::vector<RedPathContext> ExecutePredicateFromEachSubquery(
    const std::vector<RedPathContext> &redpath_ctx_multiple,
    const RedfishObject &redfish_object, int node_index, size_t node_set_size) {
  std::vector<RedPathContext> filtered_redpath_context;
  for (const auto &redpath_ctx : redpath_ctx_multiple) {
    if (redpath_ctx.subquery_handle == nullptr) {
      continue;
    }

    absl::StatusOr<bool> predicate_test = ApplyPredicateRule(
        redfish_object.GetContentAsJson(),
        {.predicate = redpath_ctx.redpath_steps.GetPredicate(),
         .node_index = node_index,
         .node_set_size = node_set_size});
    if (!predicate_test.ok() || !predicate_test.value()) {
      continue;
    }
    filtered_redpath_context.push_back(redpath_ctx);
  }
  return filtered_redpath_context;
}

// Populates Query Result for the requested properties for fully resolved
// RedPath expressions or returns RedPath contexts that have unresolved
// RedPath steps. When full resolved RedPath contexts have child RedPaths
// contexts linked, first the result is populated and then child RedPath
// contexts are retrieved and returned.
std::vector<RedPathContext> PopulateResultOrContinueQuery(
    const RedfishObject &redfish_object,
    const std::vector<RedPathContext> &redpath_ctx_multiple,
    DelliciusQueryResult &result) {
  std::vector<RedPathContext> redpath_ctx_unresolved;
  if (redpath_ctx_multiple.empty()) return redpath_ctx_unresolved;
  for (const auto &redpath_ctx : redpath_ctx_multiple) {
    const auto &subquery_handle = redpath_ctx.subquery_handle;
    if (subquery_handle->IsSubqueryTerminated()) {
      continue;
    }

    bool is_end_of_redpath =
        subquery_handle->HasUriReferenceRedpath() || subquery_handle->HasUri();
    if (subquery_handle->HasRedPath()) {
      is_end_of_redpath =
          redpath_ctx.redpath_steps.IsEndOfRedPath();
    }

    // If there aren't any child subqueries and all step expressions in the
    // current RedPath context have been processed, we can populate the query
    // result for requested properties.
    if (is_end_of_redpath && !subquery_handle->HasChildSubqueries()) {
      subquery_handle
          ->Normalize(redfish_object, result, redpath_ctx.root_redpath_dataset,
                      redpath_ctx.callback)
          .IgnoreError();
      continue;
    }
    // If we have reached the end of RedPath expression but there are child
    // subqueries linked.
    if (is_end_of_redpath) {
      absl::StatusOr<SubqueryDataSet *> last_normalized_dataset;
      if (last_normalized_dataset = subquery_handle->Normalize(
              redfish_object, result, redpath_ctx.root_redpath_dataset,
              redpath_ctx.callback);
          !last_normalized_dataset.ok()) {
        continue;
      }
      // We will insert all the RedPath contexts in the list tracked for the
      // new context node.
      for (const auto &child_subquery_handle :
           subquery_handle->GetChildSubqueryHandles()) {
        if (child_subquery_handle == nullptr) continue;
        RedPathSteps redpath_steps = child_subquery_handle->GetRedPathSteps();
        redpath_ctx_unresolved.push_back(
            {child_subquery_handle, *last_normalized_dataset,
             std::move(redpath_steps), redpath_ctx.callback});
      }
      continue;
    }
    redpath_ctx_unresolved.push_back(redpath_ctx);
    if (subquery_handle->HasRedPath()) {
      redpath_ctx_unresolved.back().redpath_steps.increment();
    }
  }
  return redpath_ctx_unresolved;
}

// Returns a collection of RedPathContext objects that do not have a predicate
// expression in their step expression.
std::vector<RedPathContext> FilterRedPathWithNoPredicate(
    const std::vector<RedPathContext> &redpath_ctx_multiple) {
  std::vector<RedPathContext> redpath_ctx_no_predicate;
  for (const auto &redpath_ctx : redpath_ctx_multiple) {
    // Predicates are not expected in following scenarios
    // 1. When subquery has a URI Reference RedPath.
    // 2. When subquery has an absolute URI.
    // 3. When subquery's RedPath expression is not accompanied by a
    //    predicate expression "[<predicate>]"
    if (redpath_ctx.subquery_handle->HasUriReferenceRedpath() ||
        redpath_ctx.subquery_handle->HasUri() ||
        (redpath_ctx.subquery_handle->HasRedPath() &&
         redpath_ctx.redpath_steps.IsPredicateEmpty())) {
      redpath_ctx_no_predicate.push_back(redpath_ctx);
    }
  }
  return redpath_ctx_no_predicate;
}

using NodeNameToRedPathContexts =
    absl::flat_hash_map<std::string, std::vector<RedPathContext>>;

// Deduplicates the next NodeName expression in the RedPath/UriReference
// of each subquery and returns NodeName to Subquery Iterators map. This is to
// ensure Redfish Request is sent out once but the dataset obtained can be
// processed per Subquery using the mapped RedPathContext objects.
NodeNameToRedPathContexts DeduplicateNodeNamesAcrossSubqueries(
    std::vector<RedPathContext> &&redpath_context_multiple) {
  NodeNameToRedPathContexts node_to_redpath_contexts;
  for (auto &&redpath_context : redpath_context_multiple) {
    // Pair resource name and those RedPaths/UriReference that have this
    // resource as next NodeName.
    std::string node_name;
    if (redpath_context.subquery_handle->HasUri()) {
      node_name = redpath_context.subquery_handle->UriToString();
    } else if (redpath_context.subquery_handle->HasUriReferenceRedpath()) {
      node_name =
          redpath_context.subquery_handle->UriReferenceRedpathToString();
    } else if (redpath_context.subquery_handle->HasRedPath()) {
      node_name = redpath_context.redpath_steps.GetNode();
    }
    node_to_redpath_contexts[node_name].push_back(redpath_context);
  }
  return node_to_redpath_contexts;
}

// Execute the next predicate expressions relative to given context_node from
// each mapped RedPath expression.
// Returns Context Node with an updated RedPath list whose predicate expressions
// filter criteria is met by the mapped context node.
ContextNode ExecutePredicateExpression(const int node_index,
                                       const size_t node_count,
                                       ContextNode context_node,
                                       DelliciusQueryResult &result) {
  // At this step only those RedPath contexts will be returned whose filter
  // criteria is met by the RedfishObject.
  std::vector<RedPathContext> redpath_ctx_filtered =
      ExecutePredicateFromEachSubquery(context_node.redpath_ctx_multiple,
                                       *context_node.redfish_object, node_index,
                                       node_count);
  if (redpath_ctx_filtered.empty()) return context_node;
  redpath_ctx_filtered = PopulateResultOrContinueQuery(
      *context_node.redfish_object, redpath_ctx_filtered, result);
  // Prepare the RedfishObject to serve as ContextNode for remaining
  // unresolved RedPath expressions.
  context_node.redpath_ctx_multiple = std::move(redpath_ctx_filtered);
  return context_node;
}

// Populates the result with the subquery error status that occurs and returns
// the status code.
::google::rpc::Code PopulateSubqueryErrorStatus(
    const absl::Status &node_variant_status,
    const std::vector<RedPathContext> &redpath_ctx_multiple,
    DelliciusQueryResult &result, const std::string &node_name,
    const std::string &last_executed_redpath) {
  ::google::rpc::Code error_code = ::google::rpc::Code::INTERNAL;
  absl::StatusCode code = node_variant_status.code();
  std::string error_message = absl::StrCat(
      "Cannot resolve NodeName ", node_name,
      " to valid Redfish object at path ", last_executed_redpath,
      ". Redfish Request failed with error: ", node_variant_status.ToString());
  if (code == absl::StatusCode::kNotFound) {
    return ::google::rpc::Code::NOT_FOUND;
  }
  // Unless the error is NOT_FOUND, we want to propagate it up to QueryResult.
  if (code == absl::StatusCode::kDeadlineExceeded) {
    error_code = ::google::rpc::Code::DEADLINE_EXCEEDED;
  }
  if (code == absl::StatusCode::kUnauthenticated) {
    error_code = ::google::rpc::Code::UNAUTHENTICATED;
  }
  result.mutable_status()->set_code(error_code);
  result.mutable_status()->set_message(error_message);
  // If the Get fails for the node name, mark the failure status in the
  // relevant subqueries.
  for (const auto &redpath_ctx : redpath_ctx_multiple) {
    ::google::rpc::Status *subquery_status =
        (*result.mutable_subquery_output_by_id())[redpath_ctx.subquery_handle
                                                      ->GetSubqueryId()]
            .mutable_status();
    subquery_status->set_code(error_code);
    subquery_status->set_message(error_message);

    // Add the current subquery error to the DelliciusQueryResult error summary.
    SubqueryErrorSummary error_summary;
    error_summary.set_node_name(node_name);
    error_summary.set_last_executed_redpath(last_executed_redpath);
    *error_summary.mutable_status() = *subquery_status;
    error_summary.set_error_message(std::string(node_variant_status.message()));
    QueryErrors *errors = result.mutable_query_errors();
    (*errors->mutable_subquery_id_to_error_summary())
        [redpath_ctx.subquery_handle->GetSubqueryId()] = error_summary;
    errors->mutable_overall_error_summary()->append(absl::StrCat(
        ::google::rpc::Code_Name(error_code), " error occurred for subquery ",
        redpath_ctx.subquery_handle->GetSubqueryId(),
        " when processing node: ", node_name, "\n"));
  }
  return error_code;
}

void PopulateRawData(std::optional<RedfishTransport::bytes> raw_data,
                    const std::vector<RedPathContext> &redpath_ctx_multiple,
                    DelliciusQueryResult &result) {
  if (!raw_data.has_value()) return;
  for (const auto &redpath_ctx : redpath_ctx_multiple) {
    const auto &subquery_handle = redpath_ctx.subquery_handle;
    if (subquery_handle->IsSubqueryTerminated() ||
        !subquery_handle->HasFetchRawData()) {
      continue;
    }

    // Insert raw data in the parent Subquery dataset if provided.
    // Otherwise, add the raw data to the query result.
    const auto subquery_id = subquery_handle->GetSubqueryId();
    SubqueryOutput *subquery_output = nullptr;
    if (redpath_ctx.root_redpath_dataset == nullptr) {
      subquery_output =
        &(*result.mutable_subquery_output_by_id())[subquery_id];
    } else {
      subquery_output =
          &(*redpath_ctx.root_redpath_dataset->
            mutable_child_subquery_output_by_id())[subquery_id];
    }

    SubqueryDataSet::RawData raw_data_out;
    const auto &fetch_raw_data = subquery_handle->GetFetchRawData();
    std::string raw_str(raw_data.value().begin(), raw_data.value().end());
    if (fetch_raw_data.type() == DelliciusQuery::Subquery::RawData::BYTES) {
      raw_data_out.set_bytes_value(raw_str);
    } else if (fetch_raw_data.type() ==
               DelliciusQuery::Subquery::RawData::STRING) {
      // Encode bytes to a printable format.
      std::string base64_str = absl::Base64Escape(raw_str);
      raw_data_out.set_string_value(base64_str);
    }

    auto *dataset = subquery_output->mutable_data_sets()->Add();
    if (dataset != nullptr) {
      *dataset->mutable_raw_data() = std::move(raw_data_out);
    }
  }
}

// Returns true if any RedPathContexts have a UriReferenceRedpath.
bool HasUriReferenceRedpath(
    const std::vector<RedPathContext> &redpath_ctx_multiple) {
  for (auto &&redpath_context : redpath_ctx_multiple) {
    if (redpath_context.subquery_handle->HasUriReferenceRedpath()) return true;
  }
  return false;
}

// Returns true if any RedPathContexts have a Uri.
bool HasUri(const std::vector<RedPathContext> &redpath_ctx_multiple) {
  for (auto &&redpath_context : redpath_ctx_multiple) {
    if (redpath_context.subquery_handle->HasUri()) return true;
  }
  return false;
}

// Recursively executes RedPath Step expressions across subqueries.
// Dispatches Redfish resource request for each unique NodeName in RedPath
// Step expressions across subqueries followed by invoking predicate handlers
// from each subquery to further refine the data that forms the context node
// of next step expression in each qualified subquery.
void ExecuteRedPathStepFromEachSubquery(
    QueryPlanner *qp, const RedPathRedfishQueryParams &redpath_to_query_params,
    ContextNode &context_node, DelliciusQueryResult &result,
    QueryTracker *tracker, ExecutionFlags execution_flags) {
  // Return if the Context Node does not contain a valid RedfishObject.
  if (!context_node.redfish_object ||
      context_node.redpath_ctx_multiple.empty()) {
    return;
  }

  // First, we will pull NodeName from each RedPath expression across
  // subqueries and then deduplicate them to get a map between NodeName and
  // RedPaths that have the NodeName in common. Recall that in a RedPath
  // "/Chassis[*]/Sensors[*]", RedPath steps are "Chassis[*]" and "Sensors[*]"
  // and NodeName expressions are "Chassis" and "Sensors".
  NodeNameToRedPathContexts node_name_to_redpath_contexts =
      DeduplicateNodeNamesAcrossSubqueries(
          std::move(context_node.redpath_ctx_multiple));
  // Return if there is no redpath expression left to process across
  // subqueries.
  if (node_name_to_redpath_contexts.empty()) {
    return;
  }

  // Set of Nodes that are obtained by executing RedPath expressions relative
  // to Redfish Object encapsulated in given 'context_node'.
  std::vector<ContextNode> context_nodes;

  // We will query each NodeName in node_name_to_redpath_contexts map created
  // above and apply predicate expressions from each RedPath to filter the
  // nodes that produces next set of context nodes.
  for (auto &[node_name, redpath_ctx_multiple] :
           node_name_to_redpath_contexts) {
    if (redpath_ctx_multiple.empty()) continue;

    // redpath expression handling begins
    std::string redpath_to_execute =
        absl::StrCat(context_node.last_executed_redpath, "/", node_name);

    // Get QueryRule configured for the RedPath expression we are about to
    // execute.
    auto get_params_for_redpath =
        GetQueryParamsForRedPath(redpath_to_query_params, redpath_to_execute);

    RedfishVariant node_set_as_variant(absl::OkStatus());

    if (HasUri(redpath_ctx_multiple)) {
      // Fetch the Redfish object associated with the uri in the NodeName
      // expression.
      node_set_as_variant = qp->FetchUri(node_name, tracker);
    } else if (HasUriReferenceRedpath(redpath_ctx_multiple)) {
      // Resolve the nested node in the NodeName expression and fetch the
      // resolved Uri.
      node_set_as_variant =
          qp->FetchUriReference(context_node, node_name, result, tracker);
    } else {
      // Before retrieving the next node check if the $filter query param is
      // enabled. This is done by checking if the filter parameter object has
      // been instantiated. If so, find the predicate(s) and populate the filter
      // param object. At this point template substitution will have occurred so
      // the predicate will be in its final state.
      if (get_params_for_redpath.filter.has_value()) {
        // Go through every predicate in the context and if there are multiple
        // predicates, generate a combined filter string using the "or"
        // operator. This operator is supported in the $filter specification.
        //
        // An alternative to this solution would be for every context node
        // (query scoped) to have its predicate carried with it so we would
        // not have combine the predicates using the redpath_to_query_params
        // map (plan scoped). This would require some refactoring, which would
        // be wasted effort due to the existing effort to refactor Query
        // Planner.
        // Engine.
        absl::flat_hash_set<std::string> predicates;
        for (const auto &redpath_ctx : redpath_ctx_multiple) {
          predicates.insert(redpath_ctx.redpath_steps.GetPredicate());
        }
        std::vector<std::string> predicates_vec(predicates.begin(),
                                                predicates.end());
        get_params_for_redpath.filter->BuildFromRedpathPredicateList(
            predicates_vec);
      }
      // Dispatch Redfish Request for the Redfish Resource associated with the
      // NodeName expression.
      node_set_as_variant =
          context_node.redfish_object->Get(node_name, get_params_for_redpath);
    }

    // Add the last executed RedPath to the record.
    if (tracker != nullptr) {
      tracker->redpaths_queried.insert(
          {redpath_to_execute, get_params_for_redpath});
    }

    // If NodeName does not resolve to a valid Redfish Resource, populate
    // error status and if it the error is just due to data not being present,
    // we can skip it. Halt the execution if intended to fail on first error.
    if (!node_set_as_variant.status().ok()) {
      if (PopulateSubqueryErrorStatus(node_set_as_variant.status(),
                                      redpath_ctx_multiple, result, node_name,
                                      context_node.last_executed_redpath) ==
          ::google::rpc::Code::NOT_FOUND) {
        // It is not considered an error to not find requested nodes in the
        // query. So here we just log and skip the iteration if that is the
        // case. For other errors, we halt execution unless explicitly running
        // with execution_mode = kContinueOnSubqueryErrors.
        DLOG(INFO) << "Cannot resolve NodeName " << node_name
                   << " to valid Redfish object at path "
                   << context_node.last_executed_redpath;
        continue;
      }
      if (execution_flags.execution_mode ==
          ExecutionFlags::ExecutionMode::kFailOnFirstError) {
        LOG(ERROR) << "Halting Query Execution early due to error: "
                   << node_set_as_variant.status().message();
        LOG(ERROR) << "Querying node name: \"" << node_name
                   << "\" relative to Redfish object:\n"
                   << context_node.redfish_object->DebugString();
        return;
      }
    }

    // If log_redfish_traces is enabled, create trace info to stamp logged
    // redfish object with.
    std::optional<TraceInfo> trace_info = std::nullopt;
    if (execution_flags.log_redfish_traces) {
      trace_info = {.redpath_prefix = redpath_to_execute,
                    .query_id = result.query_id()};
      std::vector<std::string> redpath_ctx_multiple_ids;
      for (const RedPathContext &ctx : redpath_ctx_multiple) {
        trace_info->subquery_ids.push_back(
            ctx.subquery_handle->GetSubqueryId());
      }
    }

    // Handle case where RedPath contexts have no predicate expression to
    // execute in their next step expression.
    std::vector<RedPathContext> redpath_ctx_no_predicate =
        FilterRedPathWithNoPredicate(redpath_ctx_multiple);
    if (!redpath_ctx_no_predicate.empty()) {
      std::unique_ptr<RedfishObject> node_as_object =
          GetRedfishObjectWithFreshness(node_set_as_variant,
                                        get_params_for_redpath, trace_info);
      if (!node_as_object) {
        // Return raw data if client asked for it.
        std::optional<RedfishTransport::bytes> node_as_raw =
          node_set_as_variant.AsRaw();
        PopulateRawData(node_as_raw, redpath_ctx_no_predicate, result);
        continue;
      }

      std::vector<RedPathContext> redpath_ctx_filtered =
          PopulateResultOrContinueQuery(*node_as_object,
                                        redpath_ctx_no_predicate, result);
      ContextNode new_context_node{
          .redfish_object = std::move(node_as_object),
          .redpath_ctx_multiple = std::move(redpath_ctx_filtered),
          .last_executed_redpath = redpath_to_execute};
      context_nodes.push_back(std::move(new_context_node));
    }

    if (redpath_ctx_no_predicate.size() == redpath_ctx_multiple.size()) {
      continue;
    }

    // Initialize count to 1 since we know there is at least one Redfish node.
    // This node count could be more than 1 if we are dealing with Redfish
    // collection.
    size_t node_count = 1;

    // First try to access the Redfish node as collection
    GetParams redpath_params = GetQueryParamsForRedPath(
        redpath_to_query_params,
        absl::StrCat(redpath_to_execute, "[", kPredicateSelectAll, "]"));

    std::unique_ptr<RedfishIterable> node_as_iterable =
        node_set_as_variant.AsIterable(
            RedfishVariant::IterableMode::kAllowExpand,
            redpath_params.freshness);

    if (node_as_iterable == nullptr) {
      // We now know that the Redfish node is not a collection/array.
      // We will access the redfish node as a singleton RedfishObject.
      std::unique_ptr<RedfishObject> node_as_object =
          GetRedfishObjectWithFreshness(node_set_as_variant, redpath_params,
                                        trace_info);
      if (!node_as_object) continue;
      ContextNode new_context_node = ExecutePredicateExpression(
          0, node_count,
          {.redfish_object = std::move(node_as_object),
           .redpath_ctx_multiple = redpath_ctx_multiple,
           .last_executed_redpath = redpath_to_execute},
          result);
      context_nodes.push_back(std::move(new_context_node));
      continue;
    }

    // Redfish node is a collection. So from tracker's perspective we are
    // going to execute '[*]' predicate expression as we iterate over each
    // member in collection to test predicate filters.
    absl::StrAppend(&redpath_to_execute, "[", kPredicateSelectAll, "]");
    if (tracker != nullptr) {
      tracker->redpaths_queried.insert({redpath_to_execute, redpath_params});
    }
    node_count = node_as_iterable->Size();

    for (int node_index = 0; node_index < node_count; ++node_index) {
      // If we are dealing with RedfishCollection, get collection member as
      // RedfishObject.
      RedfishVariant indexed_node = (*node_as_iterable)[node_index];
      if (!indexed_node.status().ok()) {
        PopulateSubqueryErrorStatus(indexed_node.status(), redpath_ctx_multiple,
                                    result, node_name, redpath_to_execute);
        DLOG(INFO)
            << "Cannot resolve NodeName " << node_name
            << " to valid Redfish object when executing collection redpath "
            << redpath_to_execute;
        continue;
      }
      if (trace_info.has_value()) {
        trace_info->redpath_prefix = redpath_to_execute;
      }
      std::unique_ptr<RedfishObject> indexed_node_as_object =
          GetRedfishObjectWithFreshness(indexed_node, redpath_params,
                                        trace_info);
      if (!indexed_node_as_object) continue;
      ContextNode new_context_node = ExecutePredicateExpression(
          node_index, node_count,
          {.redfish_object = std::move(indexed_node_as_object),
           .redpath_ctx_multiple = redpath_ctx_multiple,
           .last_executed_redpath = redpath_to_execute},
          result);
      context_nodes.push_back(std::move(new_context_node));
    }
  }

  // Now, for each new Context node obtained after applying Predicate
  // expression from all RedPath expressions, execute next RedPath Step
  // expression from every RedPath context mapped to the context node.
  for (auto &new_context_node : context_nodes) {
    ExecuteRedPathStepFromEachSubquery(qp, redpath_to_query_params,
                                       new_context_node, result, tracker,
                                       execution_flags);
  }
}

absl::StatusOr<SubqueryDataSet *> SubqueryHandle::Normalize(
    const RedfishObject &redfish_object, DelliciusQueryResult &result,
    SubqueryDataSet *parent_subquery_dataset,
    const std::function<bool(const DelliciusQueryResult &result)> &callback) {
  auto id = subquery_.subquery_id();
  ECCLESIA_ASSIGN_OR_RETURN(SubqueryDataSet subquery_dataset,
                            normalizer_->Normalize(redfish_object, subquery_));
  // Insert normalized data in the parent Subquery dataset if provided.
  // Otherwise, add the dataset in the query result.
  SubqueryOutput *subquery_output = nullptr;
  if (parent_subquery_dataset == nullptr) {
    subquery_output =
        &(*result.mutable_subquery_output_by_id())[id];
  } else {
    subquery_output = &(
        *parent_subquery_dataset
             ->mutable_child_subquery_output_by_id())[id];
  }
  // Check if size limit would be honored on appending the normalized data to
  // the result
  if (subquery_.has_max_size_in_bytes() && callback != nullptr) {
    size_t result_bytes =
        result.ByteSizeLong() + subquery_dataset.ByteSizeLong();
    if (result_bytes > subquery_.max_size_in_bytes()) {
      bool should_continue = callback(result);
      subquery_output->Clear();
      if (!should_continue) {
        TerminateSubquery();
        return nullptr;
      }
    }
  }

  auto *dataset = subquery_output->mutable_data_sets()->Add();
  if (dataset != nullptr) {
    *dataset = std::move(subquery_dataset);
  }
  return dataset;
}

void QueryPlanner::ProcessSubqueries(
    const RedfishVariant &variant, const QueryVariables &query_variables,
    const std::function<bool(const DelliciusQueryResult &result)> callback,
    DelliciusQueryResult &result, QueryTracker *tracker,
    ExecutionFlags execution_flags) {
  std::unique_ptr<RedfishObject> redfish_object = variant.AsObject();
  if (!redfish_object) {
    result.mutable_status()->set_code(::google::rpc::Code::FAILED_PRECONDITION);
    result.mutable_status()->set_message(absl::StrCat(
        "Cannot query service root for query with id: ", result.query_id(),
        ", Error: ",
        variant.status().ToString(absl::StatusToStringMode::kWithEverything)));
    return;
  }

  // We will create ContextNode for the RedfishObject relative to which all
  // RedPath expressions will execute.
  ContextNode context_node{.redfish_object = std::move(redfish_object)};

  // Now we create RedPathContext for each RedPath across subqueries and map
  // them to the ContextNode.
  for (auto &subquery_handle : subquery_handles_) {
    // Only consider subqueries that have RedPath expressions to execute
    // relative to service root. This step filters out any child subqueries
    // which execute relative to other subqueries.
    if (!subquery_handle || subquery_handle->IsChildSubquery()) continue;

    // Substitute any variables with their values provided by the query
    // engine.
    RedPathSteps redpath_steps =
        subquery_handle->SubstituteVariables(query_variables);

    // A context node can usually have multiple RedPath expressions mapped.
    // Let's instantiate the RedPathContext list with the one RedPath in the
    // subquery.
    auto node = redpath_steps.GetNode();
    std::vector<RedPathContext> redpath_ctx_multiple = {
        {subquery_handle.get(), nullptr, std::move(redpath_steps), callback}};

    if (subquery_handle->HasRedPath() && node.empty()) {
      // A special case where properties need to be queried from service root
      // itself.
      redpath_ctx_multiple = PopulateResultOrContinueQuery(
          *context_node.redfish_object, redpath_ctx_multiple, result);
    }
    // Update ContextNode with RedPath contexts created for the subquery.
    context_node.redpath_ctx_multiple.insert(
        context_node.redpath_ctx_multiple.end(), redpath_ctx_multiple.begin(),
        redpath_ctx_multiple.end());
  }

  // Recursively execute each RedPath step across subqueries.
  ExecuteRedPathStepFromEachSubquery(this, redpath_to_query_params_,
                                     context_node, result, tracker,
                                     execution_flags);
}

// Runs the Redpath Query; handles construction of the service root
// RedfishVariant.
DelliciusQueryResult QueryPlanner::Run(const Clock &clock,
                                       QueryTracker *tracker,
                                       const QueryVariables &query_variables,
                                       const RedfishMetrics *metrics,
                                       ExecutionFlags execution_flags) {
  DelliciusQueryResult result;
  result.set_query_id(plan_id_);
  ProcessSubqueries(redfish_interface_->GetRoot(GetParams{}, service_root_),
                    query_variables, nullptr, result, tracker, execution_flags);
  if (metrics != nullptr) {
    *result.mutable_redfish_metrics() = *metrics;
  }
  return result;
}

DelliciusQueryResult QueryPlanner::Run(const RedfishVariant &variant,
                                       const Clock &clock,
                                       QueryTracker *tracker,
                                       const QueryVariables &query_variables,
                                       const RedfishMetrics *metrics,
                                       ExecutionFlags execution_flags) {
  DelliciusQueryResult result;
  result.set_query_id(plan_id_);
  ProcessSubqueries(variant, query_variables, nullptr, result, tracker,
                    execution_flags);
  if (metrics != nullptr) {
    *result.mutable_redfish_metrics() = *metrics;
  }
  return result;
}

void QueryPlanner::Run(
    const RedfishVariant &variant, const Clock &clock, QueryTracker *tracker,
    const QueryVariables &query_variables,
    absl::FunctionRef<bool(const DelliciusQueryResult &result)> callback,
    const RedfishMetrics *metrics) {
  DelliciusQueryResult result;
  result.set_query_id(plan_id_);
  SetTime(clock, *result.mutable_start_timestamp());
  ProcessSubqueries(variant, query_variables, callback, result, tracker);
  SetTime(clock, *result.mutable_end_timestamp());
  if (metrics != nullptr) {
    *result.mutable_redfish_metrics() = *metrics;
  }
  callback(result);
}

// Fetch the Redfish object at the given URI_Reference_Redpath.
// The uri_reference_redpath is a non-navigational property and cannot be
// obtained by uri links. It is a nested node that's resolved using the
// redfish tree in the context_node.
RedfishVariant QueryPlanner::FetchUriReference(const ContextNode &context_node,
                                               absl::string_view node_name,
                                               DelliciusQueryResult &result,
                                               QueryTracker *tracker) const {
  const RedfishObject &redfish_object = *(context_node.redfish_object);
  // normalize the redpath format to nested node format
  std::string normalized_node_name =
      absl::StrReplaceAll(node_name, {{"/", "."}});
  // resolve the nest node represented in the normalized_node_name
  absl::StatusOr<nlohmann::json> json_obj = ResolveRedPathNodeToJson(
      redfish_object.GetContentAsJson(), normalized_node_name);
  if (!json_obj.ok()) {
    return RedfishVariant(absl::NotFoundError(
        absl::StrCat("Failed to resolve NodeName: ", normalized_node_name)));
  }

  std::string uri = json_obj->get<std::string>();
  return FetchUri(uri, tracker);
}

// Fetch the redfish object at the given URI.
RedfishVariant QueryPlanner::FetchUri(absl::string_view uri,
                                      QueryTracker *tracker) const {
  // Get QueryRule configured for the RedPath expression we are about to
  // execute.
  GetParams get_params_for_redpath =
      GetQueryParamsForRedPath(redpath_to_query_params_, uri);

  // Dispatch Redfish Request to fetch the Redfish Resource associated
  // with the Uri.
  RedfishVariant node_set_as_variant =
      ecclesia::FetchUri(redfish_interface_, uri, get_params_for_redpath);
  // Add the last executed uri to the record.
  if (tracker != nullptr) {
    tracker->redpaths_queried.insert(
        {std::string(uri), get_params_for_redpath});
  }

  // Propagate a more useful message to the client.
  if (!node_set_as_variant.status().ok() &&
      absl::IsNotFound(node_set_as_variant.status())) {
    return RedfishVariant(absl::NotFoundError(
        absl::StrCat("Failed to fetch "
                     "uri: ",
                     uri, " Error: ", node_set_as_variant.status().message())));
  }
  return node_set_as_variant;
}

absl::Status SubqueryHandleFactory::BuildSubqueryHandleChain(
    const std::string &subquery_id,
    absl::flat_hash_set<std::string> &subquery_id_chain,
    SubqueryHandle *child_subquery_handle) {
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

  std::unique_ptr<SubqueryHandle> new_subquery_handle;
  if (subquery.has_redpath()) {
    // Create a new SubqueryHandle.
    absl::StatusOr<std::vector<RedPathStep>> steps =
        RedPathToSteps(subquery.redpath());
    if (!steps.ok()) {
      LOG(ERROR) << "Cannot create SubqueryHandle for " << subquery_id;
      return steps.status();
    }
    new_subquery_handle = std::make_unique<SubqueryHandle>(
        subquery, (std::move(steps)).value(), normalizer_);
  } else {
    new_subquery_handle = std::make_unique<SubqueryHandle>(
        subquery, std::vector<RedPathStep>(), normalizer_);
  }

  // Raw pointer used to link SubqueryHandle with parent subquery if any.
  SubqueryHandle *new_subquery_handle_ptr = new_subquery_handle.get();
  // Link the given child SubqueryHandle.
  if (child_subquery_handle != nullptr) {
    new_subquery_handle->AddChildSubqueryHandle(child_subquery_handle);
  }
  id_to_subquery_handle_[subquery_id] = std::move(new_subquery_handle);
  // Return if Subquery does not have any root Subquery Ids linked i.e. the
  // subquery itself is a root.
  if (subquery.root_subquery_ids().empty()) {
    return absl::OkStatus();
  }
  // Recursively build root subquery handles.
  for (const auto &root_subquery_id : subquery.root_subquery_ids()) {
    absl::flat_hash_set<std::string> subquery_id_chain_per_root =
        subquery_id_chain;
    auto status = BuildSubqueryHandleChain(
        root_subquery_id, subquery_id_chain_per_root, new_subquery_handle_ptr);
    if (!status.ok()) {
      return status;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<SubqueryHandleCollection>
SubqueryHandleFactory::GetSubqueryHandles() {
  for (const auto &subquery : query_.subquery()) {
    absl::flat_hash_set<std::string> subquery_id_chain;
    absl::Status status = BuildSubqueryHandleChain(subquery.subquery_id(),
                                                   subquery_id_chain, nullptr);
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

}  // namespace

// Builds the default query planner.
absl::StatusOr<std::unique_ptr<QueryPlannerInterface>> BuildDefaultQueryPlanner(
    const DelliciusQuery &query,
    RedPathRedfishQueryParams redpath_to_query_params, Normalizer *normalizer,
    ecclesia::RedfishInterface *redfish_interface) {
  absl::StatusOr<SubqueryHandleCollection> subquery_handle_collection =
      SubqueryHandleFactory::CreateSubqueryHandles(query, normalizer);
  if (!subquery_handle_collection.ok()) {
    return subquery_handle_collection.status();
  }
  return std::make_unique<QueryPlanner>(
      query, *std::move(subquery_handle_collection),
      std::move(redpath_to_query_params), redfish_interface);
}

}  // namespace ecclesia
