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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_PREDICATES_PREDICATE_OBJECT_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_PREDICATES_PREDICATE_OBJECT_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace ecclesia {

// A simple structure containing the information in a relational expression.
struct RelationalExpression {
  std::string lhs;
  std::string rel_operator;
  std::string rhs;
  // Populated only if the relational expression is checking if a property is
  // present or not.
  std::string property_name;
};

// Contains all of the information contained in a predicate. The ordering of the
// operators and the expressions is important. Each logical operator goes
// between two expressions. Here is a visualization:
//
// logical_operators: {and, or}
// child_predicates: {exp1, exp2, exp3}
// This would turn into "exp1 and exp2 or exp3".
//
// If the PredicateObject doesn't have logical operators that means it is a
// single relational expression, therefore the logical_operators and
// child_predicates lists will be empty.
struct PredicateObject {
  RelationalExpression relational_expression;
  std::vector<std::string> logical_operators;
  std::vector<PredicateObject> child_predicates;
};

// Takes a Redpath format predicate and turns it into a machine readable object.
// Example: "Prop1<=42 or Prop1>84"
//   logical_operators = [" or "]
//   child_predicates = [[lhs: "Prop1", rel_operator: "<=", rhs: "42"],
//                       [lhs: "Prop1", rel_operator: ">", rhs: "84"]
//                      ]
// Property existence check predicates are not supported by this method
// currently. For example: !Property or Property.SubProperty
absl::StatusOr<PredicateObject> CreatePredicateObject(
    absl::string_view predicate);

// Constructs a redpath predicate string from a predicate object. Does not check
// if the object has valid data for construction.
std::string PredicateObjectToString(const PredicateObject &predicate_object);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_PREDICATES_PREDICATE_OBJECT_H_
