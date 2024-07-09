/*
 * Copyright 2023 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_RESULT_CONVERTER_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_RESULT_CONVERTER_H_

#include <optional>

#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

// Converts legacy DelliciusQueryResult to QueryResult.
QueryResult ToQueryResult(const ecclesia::DelliciusQueryResult& result_in);

// Utility functions to convert Query Result Data to Json
nlohmann::json QueryResultDataToJson(const QueryResultData& query_result);
nlohmann::json ValueToJson(const QueryValue& value);
nlohmann::json ListValueToJson(const ListValue& value);
nlohmann::json IdentifierValueToJson(const Identifier& value);

// Utility functions to convert Json to Query Result Data
QueryResultData JsonToQueryResultData(const nlohmann::json& json);
QueryValue JsonToQueryValue(const nlohmann::json& json);
ListValue JsonToQueryListValue(const nlohmann::json& json);
std::optional<Identifier> JsonToIdentifierValue(const nlohmann::json& json);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_RESULT_CONVERTER_H_
