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

#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result_validator.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {
namespace {

struct SubqueryConfig {
  const DelliciusQuery::Subquery* config;
  std::vector<std::string> paths;
};

absl::Status TraverseQueryConfig(
    SubqueryConfig& subquery_config,
    absl::flat_hash_map<std::string, std::vector<SubqueryConfig>>&
        subquery_id_to_config,
    absl::flat_hash_map<std::string,
                        DelliciusQuery::Subquery::RedfishProperty const*>&
        property_name_to_config_map) {
  auto populate_property_map =
      [&property_name_to_config_map](
          std::string path, SubqueryConfig& subquery_config) -> absl::Status {
    for (const DelliciusQuery::Subquery::RedfishProperty& property :
         subquery_config.config->properties()) {
      std::string property_name = property.property();
      if (!(property.name().empty())) {
        property_name = property.name();
      }

      const auto [_, status] = property_name_to_config_map.insert(
          {absl::StrCat(path, "/", property_name), &property});

      if (!status) {
        return absl::InternalError(absl::StrCat("Duplicate path: ", path));
      }
    }

    subquery_config.paths.push_back(std::move(path));
    return absl::OkStatus();
  };

  if (!subquery_config.paths.empty()) {
    return absl::OkStatus();
  }

  // Traverse each root, such that each root has a complete set of paths.
  absl::string_view subquery_id = subquery_config.config->subquery_id();
  if (subquery_config.config->root_subquery_ids().empty()) {
    ECCLESIA_RETURN_IF_ERROR(
        populate_property_map(absl::StrCat("/", subquery_id), subquery_config));
    return absl::OkStatus();
  }

  for (absl::string_view root_subquery :
       subquery_config.config->root_subquery_ids()) {
    if (root_subquery == subquery_id) {
      return absl::InternalError(
          absl::StrCat("Cycle detected: ", root_subquery));
    }

    auto root_it = subquery_id_to_config.find(root_subquery);
    if (root_it == subquery_id_to_config.end()) {
      return absl::NotFoundError(
          absl::StrCat("No subquery found for ", root_subquery));
    }

    // Check if root has paths generated. If empty then move to root node
    for (SubqueryConfig& root_config : root_it->second) {
      if (root_config.paths.empty()) {
        ECCLESIA_RETURN_IF_ERROR(TraverseQueryConfig(
            root_config, subquery_id_to_config, property_name_to_config_map));

        if (root_config.paths.empty()) {
          return absl::NotFoundError(
              absl::StrCat("No paths were generated for ", root_subquery));
        }
      }
      for (absl::string_view root_path : root_config.paths) {
        ECCLESIA_RETURN_IF_ERROR(populate_property_map(
            absl::StrCat(root_path, "/", subquery_id), subquery_config));
      }
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<absl::flat_hash_map<
    std::string, DelliciusQuery::Subquery::RedfishProperty const*>>
BuildQueryConfigMap(const DelliciusQuery& query) {
  absl::flat_hash_map<std::string, std::vector<SubqueryConfig>>
      subquery_id_to_config;
  subquery_id_to_config.reserve(query.subquery_size());

  // Pre-processes the Redpath query config, and relate the subquery_id to the
  // subquery configuration via a map. This will be helpful during the
  // traversal and populating the final map of path to property vector.
  for (const DelliciusQuery_Subquery& subquery : query.subquery()) {
    subquery_id_to_config[subquery.subquery_id()].push_back(
        {.config = &subquery});
  }

  // Traverse the Redpath query config and build paths via roots of the
  // subquery_id. Then relate each subquery to all possible paths to the
  // property configurations.
  absl::flat_hash_map<std::string,
                      DelliciusQuery::Subquery::RedfishProperty const*>
      property_name_to_config_map;
  for (auto& [_, subquery_list] : subquery_id_to_config) {
    for (SubqueryConfig& subquery : subquery_list) {
      ECCLESIA_RETURN_IF_ERROR(TraverseQueryConfig(
          subquery, subquery_id_to_config, property_name_to_config_map));
    }
  }

  return property_name_to_config_map;
}

absl::Status CompareTypes(
    absl::string_view path, const QueryValue::KindCase& query_result_property,
    const DelliciusQuery::Subquery::RedfishProperty::PrimitiveType&
        property_type) {
  bool is_equal = false;
  switch (query_result_property) {
    case QueryValue::kIntValue:
      is_equal =
          property_type == DelliciusQuery::Subquery::RedfishProperty::INT64;
      break;
    case QueryValue::kDoubleValue:
      is_equal =
          property_type == DelliciusQuery::Subquery::RedfishProperty::DOUBLE;
      break;
    case QueryValue::kStringValue:
      is_equal =
          property_type == DelliciusQuery::Subquery::RedfishProperty::STRING;
      break;
    case QueryValue::kBoolValue:
      is_equal =
          property_type == DelliciusQuery::Subquery::RedfishProperty::BOOLEAN;
      break;
    case QueryValue::kIdentifier:
      is_equal = true;
      break;
    case QueryValue::kNullValue:
    case QueryValue::kListValue:
    case QueryValue::kTimestampValue:
    case QueryValue::kSubqueryValue:
    case QueryValue::kRawData:
    case QueryValue::KIND_NOT_SET:
      break;
  }

  if (is_equal) {
    return absl::OkStatus();
  }
  return absl::InternalError(absl::StrCat("Unexpected value type for ", path));
}

absl::Status CheckProperty(
    absl::string_view path, const QueryValue& value,
    const DelliciusQuery::Subquery::RedfishProperty& property_config) {
  if (value.ByteSizeLong() == 0) {
    return absl::InternalError(absl::StrCat("Empty value for ", path));
  }

  if (property_config.property_element_type() ==
      DelliciusQuery::Subquery::RedfishProperty::COLLECTION_PRIMITIVE) {
    if (value.kind_case() != QueryValue::kListValue) {
      return absl::InternalError(
          absl::StrCat("Unexpected value type for ", path));
    }
    for (const ecclesia::QueryValue& element : value.list_value().values()) {
      if (element.ByteSizeLong() == 0) {
        return absl::InternalError(absl::StrCat("Empty value for ", path));
      }
      ECCLESIA_RETURN_IF_ERROR(
          CompareTypes(path, element.kind_case(), property_config.type()));
    }

    return absl::OkStatus();
  }

  return CompareTypes(path, value.kind_case(), property_config.type());
}
}  // namespace

absl::Status QueryResultValidator::TraverseQueryResult(
    absl::string_view path, const QueryResultData& query_result) {
  for (const auto& [key, value] : query_result.fields()) {
    std::string current_path = absl::StrCat(path, "/", key);

    if (auto property_it = subquery_id_to_property_.find(current_path);
        property_it != subquery_id_to_property_.end()) {
      ECCLESIA_RETURN_IF_ERROR(
          CheckProperty(current_path, value, *property_it->second));
      continue;
    }

    if (value.has_list_value()) {
      for (const ecclesia::QueryValue& child : value.list_value().values()) {
        ECCLESIA_RETURN_IF_ERROR(
            TraverseQueryResult(current_path, child.subquery_value()));
      }
      continue;
    }

    if (value.has_subquery_value()) {
      ECCLESIA_RETURN_IF_ERROR(
          TraverseQueryResult(current_path, value.subquery_value()));
      continue;
    }

    return absl::InternalError(
        absl::StrCat("Unknown ", current_path, " missing from configuration"));
  }

  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<QueryResultValidator>>
QueryResultValidator::Create(const DelliciusQuery* query) {
  if (query == nullptr) {
    return absl::InvalidArgumentError("Query Configuration is Empty");
  }

  if (query->query_id().empty()) {
    return absl::InvalidArgumentError("Query ID is empty");
  }

  ECCLESIA_ASSIGN_OR_RETURN(SubqueryIdToPropertyMap property_name_to_config_map,
                            BuildQueryConfigMap(*query));
  return absl::WrapUnique(
      new QueryResultValidator(*query, std::move(property_name_to_config_map)));
}

absl::Status QueryResultValidator::Validate(const QueryResult& query_result) {
  if (query_result.ByteSizeLong() == 0) {
    return absl::InvalidArgumentError("Query result is empty");
  }

  absl::string_view query_id = query_result.query_id();
  if (query_id != query_.query_id()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Query id mismatch: ", query_id, " vs ", query_.query_id()));
  }

  // Start Traversing the query_result tree
  return TraverseQueryResult("", query_result.data());
}

absl::Status ValidateQueryResult(const DelliciusQuery& query,
                                 const QueryResult& result) {
  ECCLESIA_ASSIGN_OR_RETURN(
      std::unique_ptr<QueryResultValidatorIntf> query_result_validator,
      QueryResultValidator::Create(&query));

  return query_result_validator->Validate(result);
}

}  // namespace ecclesia
