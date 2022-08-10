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

#include "ecclesia/lib/redfish/query.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "re2/re2.h"

namespace ecclesia {
namespace {

constexpr absl::string_view kRedPathSelectAllElements = "*";
constexpr absl::string_view kRedPathSelectLastElements = "last()";

// Helper function used to determine whether the current RedfishVariant is
// iterable. If RedfishVariant is iterable, the condition is applied to the
// members, if not, the condition is applied to the current RedfishVariant.
template <typename F>
void ApplyFilter(RedfishVariant&& variant, F filter_condition,
                 std::vector<RedfishVariant>& temp) {
  auto iter = variant.AsIterable();
  if (iter != nullptr) {
    for (auto member : *iter) {
      if (filter_condition(member)) {
        temp.push_back(std::move(member));
      }
    }
  } else if (filter_condition(variant)) {
    temp.push_back(std::move(variant));
  }
}
// Helper function used to ensure that both sides of the inequality sign
// compare as numbers.
template <typename F>
void InequalityFilterPropertyForNumbers(RedfishVariant&& var,
                                        F filter_condition,
                                        absl::string_view property,
                                        std::vector<RedfishVariant>& temp) {
  const auto condition = [property,
                          &filter_condition](const RedfishVariant& variant) {
    if (!variant.AsObject()->GetContentAsJson().contains(property)) {
      return false;
    }
    double number;
    if (absl::SimpleAtod(variant.AsObject()
                             ->GetContentAsJson()[std::string(property)]
                             .dump(),
                         &number)) {
      return filter_condition(number);
    }
    return false;
  };
  ApplyFilter(std::move(var), condition, temp);
}
// Helper function decide which inequality or equal condition will be used.
void FilterOnPropertyWithValue(absl::string_view inequality_string,
                               absl::string_view subpath_content,
                               std::vector<RedfishVariant>& temp,
                               RedfishVariant&& var) {
  std::vector<absl::string_view> split_subpath =
      absl::StrSplit(subpath_content, inequality_string);
  double value;
  if (absl::SimpleAtod(split_subpath[1], &value)) {
    const auto condition = [&inequality_string, value](double number) {
      if (inequality_string == ">=") return number >= value;
      if (inequality_string == ">") return number > value;
      if (inequality_string == "<=") return number <= value;
      if (inequality_string == "<") return number < value;
      if (inequality_string == "!=") return number != value;
      if (inequality_string == "=") return number == value;
      return false;
    };
    InequalityFilterPropertyForNumbers(std::move(var), condition,
                                       split_subpath[0], temp);
  } else {
    const auto condition = [split_subpath,
                            inequality_string](const RedfishVariant& variant) {
      if (variant.AsObject()->GetContentAsJson().contains(split_subpath[0])) {
        if (inequality_string == "!=") {
          return variant.AsObject()
                     ->GetContentAsJson()[std::string(split_subpath[0])] !=
                 split_subpath[1];
        }
        return variant.AsObject()
                   ->GetContentAsJson()[std::string(split_subpath[0])] ==
               split_subpath[1];
      }
      return false;
    };
    ApplyFilter(std::move(var), condition, temp);
  }
}

// Helper function used to select the json file that matches
// [node.child.grandchild = value] or [node.child.grandchild] condition.
void FilterOnPropertyContainingChild(absl::string_view subpath_content,
                                     RedfishVariant&& var,
                                     std::vector<RedfishVariant>& temp) {
  std::vector<absl::string_view> split_subpath =
      absl::StrSplit(subpath_content, absl::ByAnyChar(".="));
  std::vector<RedfishVariant> subobject;
  const auto condition = [split_subpath](const RedfishVariant& variant) {
    nlohmann::json json_res = variant.AsObject()->GetContentAsJson();
    for (const auto& elem : split_subpath) {
      if (!json_res.contains(elem) && json_res != elem &&
          json_res.dump() != elem) {
        return false;
      }
      if (elem != split_subpath.back()) {
        json_res = json_res[std::string(elem)];
      }
    }
    return true;
  };
  ApplyFilter(std::move(var), condition, temp);
}
}  // namespace

void GetRedPath(RedfishInterface* intf, absl::string_view redpath,
                Sysmodel::ResultCallback result_callback) {
  if (redpath.empty()) return;
  std::vector<absl::string_view> split_redpath = absl::StrSplit(redpath, '/');
  auto root = intf->GetRoot();
  // To match partial RedPath stored in split_redpath.
  static constexpr LazyRE2 kRedPathRegex = {
      "([0-9a-zA-Z]+)(\\[([a-zA-Z0-9=@#.\\(\\)!></_$\\s]+|\\*)\\])*"};
  static constexpr LazyRE2 kComparisonRedPathRegex = {
      "([a-zA-Z]+)([<>!=]+)([a-zA-Z0-9.#_\\s]+)*"};
  std::vector<RedfishVariant> passed_the_condition;
  passed_the_condition.push_back(std::move(root));
  std::string path, subpath, subpath_content;
  std::string propertyname, symbol, value;
  size_t num;
  for (const auto& elem : split_redpath) {
    if (elem.empty()) continue;
    // path: uri part which is the string before the square bracket.
    // subpath: square bracket and filter. [index][node=name][*][nodename]
    // subpath_content: filter part which is the string inside square bracket
    // and have multiple format.
    if (!RE2::FullMatch(elem, *kRedPathRegex, &path, &subpath,
                        &subpath_content))
      return;
    if (passed_the_condition.empty()) break;
    std::vector<RedfishVariant> temp;
    for (auto& var : passed_the_condition) {
      var = var[path];
      if (subpath_content.empty()) {
        temp.push_back(std::move(var));
        continue;
      }
      if (subpath_content == kRedPathSelectAllElements) {
        // [*] : Selects all the elements from an array or object.
        const auto condition = [](const RedfishVariant& variant) {
          return true;
        };
        ApplyFilter(std::move(var), condition, temp);
      } else if (subpath_content == kRedPathSelectLastElements) {
        // [last()] : Selects the last index number JSON entity from an array or
        // object.
        auto iter_members = var.AsIterable();
        if (iter_members != nullptr) {
          var = var[iter_members->Size() - 1];
        } else {
          continue;
        }
        temp.push_back(std::move(var));
      } else if (absl::SimpleAtoi(subpath_content, &num)) {
        // [index] : Selects the index number JSON entity from an array or
        // object.
        var = var[num];
        temp.push_back(std::move(var));
      } else if (RE2::FullMatch(subpath_content, *kComparisonRedPathRegex,
                                &propertyname, &symbol, &value)) {
        // [name<=value] : Selects all the elements from an array or object
        // where the property "name" is less than or equal to "value".
        // [name<value] :Selects all the elements from an array or object
        // where the property "name" is less than "value".
        // [name>=value] : Selects all the elements from an array or object
        // where the property "name" is greater than or equal to "value".
        // [name>value] : Selects all the elements from an array or object
        // where the property "name" is greater than "value".
        // [name!=value] : Selects all the elements from an array or object
        // where the property "name" does not equal "value".
        // [name=value] : Selects all the elements from an array or
        // object where the property "name" is equal to "value".
        FilterOnPropertyWithValue(symbol, subpath_content, temp,
                                  std::move(var));
      } else if (absl::StrContains(subpath_content, ".") &&
                 !absl::StrContains(subpath_content, "@odata.")) {
        // Checking whether subpath_content have '@' will help us to divide out
        // filter's "nodename" have a dot in it like [@odata.id].
        // [node.child] : Selects all the elements from an array or object that
        // contain a property named "node" which contains "child".
        // [node.child=value] :  Selects all the elements from an array or
        // object that contain a property named "node" which contains "child"
        // equal "value".
        FilterOnPropertyContainingChild(subpath_content, std::move(var), temp);
      } else if (!absl::StrContains(subpath_content, '=')) {
        // [nodename] : Selects all the elements from an array or object that
        // contain a property named "nodename".
        const auto condition =
            [subpath_content](const RedfishVariant& variant) {
              return variant.AsObject()->GetContentAsJson().contains(
                  subpath_content);
            };
        ApplyFilter(std::move(var), condition, temp);
      }
    }
    passed_the_condition.swap(temp);
  }
  for (const auto& var : passed_the_condition) {
    auto obj = var.AsObject();
    if (obj == nullptr) continue;
    if (result_callback(std::move(obj)) == RedfishIterReturnValue::kStop) {
      break;
    }
  }
}
}  // namespace ecclesia
