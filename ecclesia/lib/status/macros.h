/*
 * Copyright 2020 Google LLC
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

// This library provides several useful macros for simplifying very, very common
// patterns that occur when writing code that make extensive use of absl::Status
// and absl::StatusOr.
//
// In particular, it provides macros:
//
//   * ECCLESIA_RETURN_IF_ERROR(expr):
//       This will evaluate an expression that is expected to produce an
//       absl::Status. If the status is not OK it returns the status.
//
//       An example of usage is:
//
//         ECCLESIA_RETURN_IF_ERROR(file->Write(my_text));
//         ECCLESIA_RETURN_IF_ERROR(file->Write(more_text));
//
//       This will try and perform a pair of writes, returning immediately if
//       either one of them fails. If both succeed then the code will continue
//       on, not returning at all.
//
//   * ECCLESIA_ASSIGN_OR_RETURN(var, expr):
//       This will evaluate an expression that is expected to produce an
//       absl::StatusOr<T> for some type. If the status is not OK it returns the
//       status. If the status is OK it will move the value into var. The
//       expression for var can be either an existing variable name, or the
//       declaration of a new variable.
//
//       Examples usage would look like:
//
//         ECCLESIA_ASSIGN_OR_RETURN(buffer, file->Read());
//         ECCLESIA_ASSIGN_OR_RETURN(DataStructure data,
//         parser->Decode(buffer));
//
//       The first example is a simple assignment: read from a file, returning
//       the not-OK status if it fails and assigning the results to "buffer" if
//       it succeeds. The second example then does something more complex, not
//       just assigning a variable but declaring a new DataStructure variable.
//       However, after that declaration, it works the same way: if the parser
//       decode fails it returns the not-OK status, and otherwise it initializes
//       the "data" variable with the result.
//
//       The variable will be assigned using move assignment, and so the type
//       stored in the StatusOr must be movable (or copyable).
//
// Unlike some other status macro libraries you may or may not have seen, this
// one does not attempt to provide any complex error handling or extensive
// customization options. If you are writing code that needs to do more than
// immediately return on an error then you'll have to write it out manually.
//
// In general it is best to avoid using this code directly in header files if
// possible, since the macros in here will then pollute the namespaces of any
// code that includes the headers.
//
// Don't use any of the ECCLESIA_STATUS_MACROS_* macros directly, they're only
// intended to help implement the actual documented macros.

#ifndef ECCLESIA_LIB_STATUS_MACROS_H_
#define ECCLESIA_LIB_STATUS_MACROS_H_

// Return-if-error implementation. We use a simple do-while idiom to wrap the
// code in a scope that avoids introducing any variables and plays nicely with
// surrounding expressions.
#define ECCLESIA_RETURN_IF_ERROR(expr) \
  do {                                 \
    auto _expr_result = (expr);        \
    if (!_expr_result.ok()) {          \
      return _expr_result;             \
    }                                  \
  } while (0)

// Assign-or-return implementation. Unfortunately, it's not really possible for
// us to avoid leaking a temporary variable into the enclosing scope. We can't
// wrap the whole thing in a scope because then "var" can't be used to declare a
// new variable. So instead we make up a new name and bolt the line number onto
// it. This does then mean that you can't use multiple of these macros on a
// single line but we're not trying to support that kind of thing.
#define ECCLESIA_ASSIGN_OR_RETURN(var, expr)    \
  ECCLESIA_STATUS_MACROS_ASSIGN_OR_RETURN_IMPL( \
      ECCLESIA_STATUS_MACROS_CONCAT(_status_or_expr, __LINE__), var, expr)

// Inner implementation of ECCLESIA_ASSIGN_OR_RETURN. Adds in a parameter which
// is used to supply the name for the temporary variable used to store the
// statusor.
#define ECCLESIA_STATUS_MACROS_ASSIGN_OR_RETURN_IMPL(statusor, var, expr) \
  auto statusor = (expr);                                                 \
  if (!statusor.ok()) {                                                   \
    return statusor.status();                                             \
  }                                                                       \
  var = std::move(statusor).value();

// Helpers for concatenating values, needed to construct "unique" names.
// These helpers are also used in //ecclesia/lib/testing/macros.h.
#define ECCLESIA_STATUS_MACROS_CONCAT_INNER(x, y) x##y
#define ECCLESIA_STATUS_MACROS_CONCAT(x, y) \
  ECCLESIA_STATUS_MACROS_CONCAT_INNER(x, y)

#endif  // ECCLESIA_LIB_STATUS_MACROS_H_
