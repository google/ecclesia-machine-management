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

#include "ecclesia/lib/redfish/interface.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/redfish/dellicius/utils/path_util.h"

namespace ecclesia {
namespace {

// Replaces periods in predicates with slashes. Only the property side (left
// hand side) should be affected. Value side (right hand side), may contain
// periods, eg decimals and timestamps.
std::string ReplacePeriodsInPredicates(const std::string &predicate) {
  std::vector<std::string> result = {};
  absl::flat_hash_set<std::string> rel_operators = {"lt", "gt", "le",
                                                    "ge", "eq", "ne"};
  absl::flat_hash_set<std::string> log_operators = {"or", "and"};
  bool looking = true;
  std::vector<std::string> tokens = absl::StrSplit(predicate, ' ');
  for (const std::string &token : tokens) {
    if (rel_operators.contains(token)) {
      // Token is a relational operator, therefore the next token will be a
      // value which we should not change.
      looking = false;
      result.push_back(token);
    } else if (log_operators.contains(token)) {
      // Token is a logical operator, therefore the next token will be a
      // property which we should change.
      looking = true;
      result.push_back(token);
    } else {
      // Token is either rhs or lhs of a predicate expression. If looking is
      // true, it will be the lhs, which needs periods swapped
      if (looking) {
        std::vector<std::string> nodes = SplitNodeNameForNestedNodes(token);
        result.push_back(absl::StrJoin(nodes, "/"));
      } else {
        result.push_back(token);
      }
    }
  }
  return absl::StrJoin(result, " ");
}

// The only difference between Redpath predicate format and $filter format are
// the relational operators. Redpath supports spaces or no spaces surrounding a
// relational operator so both substitutions need to be done. Substitutions with
// surrounding spaces will be done first as the $filter format requires spaces.
std::string GetFilterString(absl::string_view predicate) {
  std::vector<std::pair<std::string, std::string>> relational_operators_spaces =
      {{" < ", " lt "},  {" > ", " gt "}, {" <= ", " le "},
       {" >= ", " ge "}, {" = ", " eq "}, {" != ", " ne "}};
  std::vector<std::pair<std::string, std::string>> relational_operators = {
      {"<", " lt "},  {">", " gt "}, {"<=", " le "},
      {">=", " ge "}, {"=", " eq "}, {"!=", " ne "}};
  // If the supplied predicate uses surrounding spaces the conversion should be
  // complete.
  std::string filter_string_intermediate =
      absl::StrReplaceAll(predicate, relational_operators_spaces);
  // If the operators have no spaces the final replacement will cover it.
  std::string filter_string_with_spaces =
      absl::StrReplaceAll(filter_string_intermediate, relational_operators);

  // Another difference between Redpath predicates and $filter format is
  // breadcrumbs are denoted with a slash rather than a period/dot.
  std::string filter_string_no_dots =
      ReplacePeriodsInPredicates(filter_string_with_spaces);
  return absl::StrReplaceAll(filter_string_no_dots, {{" ", "%20"}});
}

}  // namespace

void RedfishQueryParamFilter::BuildFromRedpathPredicate(
    absl::string_view predicate) {
  filter_string_ = GetFilterString(predicate);
}

void RedfishQueryParamFilter::BuildFromRedpathPredicateList(
    const std::vector<std::string> &predicates) {
  std::vector<std::string> filter_strings;
  filter_strings.reserve(predicates.size());
  for (absl::string_view predicate : predicates) {
    filter_strings.push_back(GetFilterString(predicate));
  }
  filter_string_ = absl::StrJoin(filter_strings, "%20or%20");
}

RedfishQueryParamTop::RedfishQueryParamTop(
    size_t numMembers)
    : numMembers_(numMembers) {}

std::string RedfishQueryParamTop::ToString() const {
  return absl::StrCat("$top=", numMembers_);
}

absl::Status RedfishQueryParamTop::ValidateRedfishSupport(
    const absl::optional<RedfishSupportedFeatures> &features) {
  if (!features.has_value()) {
    return absl::InternalError("Top query parameter is not supported.");
  }
  if (!features->top_skip.enable) {
    return absl::InternalError("'$top' and '$skip' are not supported");
  }
  return absl::OkStatus();
}

RedfishQueryParamExpand::RedfishQueryParamExpand(
    RedfishQueryParamExpand::Params params)
    : type_(params.type), levels_(params.levels) {}

std::string RedfishQueryParamExpand::ToString() const {
  std::string expand_type;
  switch (type_) {
    case ExpandType::kBoth:
      expand_type = "*";
      break;
    case ExpandType::kNotLinks:
      expand_type = ".";
      break;
    case ExpandType::kLinks:
      expand_type = "~";
      break;
  }
  return absl::StrCat("$expand=", expand_type, "($levels=", levels_, ")");
}

absl::Status RedfishQueryParamExpand::ValidateRedfishSupport(
    const absl::optional<RedfishSupportedFeatures> &features) const {
  if (!features.has_value()) {
    return absl::InternalError("Expands are not supported.");
  }
  std::string expand_type;
  switch (type_) {
    case ExpandType::kBoth:
      if (!features->expand.expand_all) {
        return absl::InternalError("'expand_all' is not supported");
      }
      break;
    case ExpandType::kNotLinks:
      if (!features->expand.no_links) {
        return absl::InternalError("'no_links' is not supported");
      }
      break;
    case ExpandType::kLinks:
      if (!features->expand.links) {
        return absl::InternalError("'links' is not supported");
      }
      break;
  }
  if (levels_ > features->expand.max_levels) {
    return absl::InternalError(
        "number of levels exceed max levels set in redfish features");
  }
  return absl::OkStatus();
}
std::unique_ptr<RedfishObject> RedfishVariant::AsFreshObject() const {
  if (!ptr_) return nullptr;
  std::unique_ptr<RedfishObject> obj = ptr_->AsObject();
  if (!obj) return nullptr;
  return obj->EnsureFreshPayload().value_or(nullptr);
}

}  // namespace ecclesia
