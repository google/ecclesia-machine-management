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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_RESULT_PATH_UTIL_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_RESULT_PATH_UTIL_H_

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"

namespace ecclesia {
// Return the QueryValue for the given path.
//
// The path is a dot-separated string that represents the hierarchical
// structure of the QueryValue. Each segment of the path is either a subquery id
// or an identifier and can end with a property name.
//
// Example subquery id path: "query1.subquery_id1.subquery_id2.property_name"
// Example list path: "query1.subquery_id1.[2].subquery_id3.[4].property_name"
// Example identifier path:
// "query1.subquery_id1.property_name="value".subquery_id2.property_name"
//
// Return type is QueryValue, so the result can be a primitive, list, or
// subquery type.
absl::StatusOr<QueryValue> GetQueryValueFromResult(const QueryResult& result,
                                                   absl::string_view path);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_RESULT_PATH_UTIL_H_
