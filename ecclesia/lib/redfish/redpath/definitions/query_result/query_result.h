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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_RESULT_QUERY_RESULT_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_RESULT_QUERY_RESULT_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/timestamp.pb.h"
#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/redpath_subscription.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"

namespace ecclesia {

// Key for Identifier type QueryValues when part of QueryResultData.
inline static constexpr absl::string_view kIdentifierTag = "_id_";
inline static constexpr absl::string_view kUriAnnotationTag = "_uri_";

// A builder class for ecclesia::QueryValue.
class QueryValueBuilder {
 public:
  // Creates a new QueryValueBuilder that will fill in the contents of
  // `query_value`. We do not take ownership of `query_value`; it must outlive
  // this QueryValueBuilder, and any child QueryValueBuilders created for
  // subfields.
  explicit QueryValueBuilder(QueryValue* unowned_query_value)
      : query_value_(*ABSL_DIE_IF_NULL(unowned_query_value)) {}

  // Sets `value` to a boolean
  QueryValueBuilder& operator=(bool other) {
    query_value_.set_bool_value(other);
    return *this;
  }

  // Sets `value` to an int64
  QueryValueBuilder& operator=(int64_t other) {
    query_value_.set_int_value(other);
    return *this;
  }

  // Sets `value` to a double
  QueryValueBuilder& operator=(double other) {
    query_value_.set_double_value(other);
    return *this;
  }

  // Sets `value` to a timestamp
  QueryValueBuilder& operator=(google::protobuf::Timestamp other) {
    *query_value_.mutable_timestamp_value() = std::move(other);
    return *this;
  }

  // Sets `value` to a string.  This overload is needed for string literals;
  // without it, the compiler would try to coerce the literal into a bool.
  QueryValueBuilder& operator=(const char* other) {
    query_value_.set_string_value(other);
    return *this;
  }

  // Sets `value` to a null value.
  QueryValueBuilder& operator=(ecclesia::QueryValue::NullValue other) {
    query_value_.set_null_value(other);
    return *this;
  }

  // Sets `value` to a string.
  QueryValueBuilder& operator=(const std::string& other) {
    query_value_.set_string_value(other);
    return *this;
  }

  // Sets `value` to a string.
  QueryValueBuilder& operator=(absl::string_view other) {
    query_value_.set_string_value(std::string(other));
    return *this;
  }

  // Sets `value` to a null value.
  QueryValueBuilder& operator=(ecclesia::QueryValue::NullValue other) {
    query_value_.set_null_value(other);
    return *this;
  }

  // Move content from other value.
  QueryValueBuilder& operator=(QueryValue other) {
    query_value_ = std::move(other);
    return *this;
  }

  // Forces `value` to be a list and moves content from another ListValue.
  QueryValueBuilder& operator=(ListValue other) {
    *query_value_.mutable_list_value() = std::move(other);
    return *this;
  }

  // Forces `value` to be a QueryResultData and moves content from another
  // QueryResultData.
  QueryValueBuilder& operator=(QueryResultData other) {
    *query_value_.mutable_subquery_value() = std::move(other);
    return *this;
  }

  // Sets `value` to given raw data value.
  QueryValueBuilder& operator=(const SubqueryDataSet::RawData& raw_data) {
    if (raw_data.has_bytes_value()) {
      *query_value_.mutable_raw_data()->mutable_raw_bytes_value() =
          raw_data.bytes_value();
    } else if (raw_data.has_string_value()) {
      *query_value_.mutable_raw_data()->mutable_raw_string_value() =
          raw_data.string_value();
    }
    return *this;
  }

  QueryValueBuilder& operator=(Identifier other) {
    *query_value_.mutable_identifier() = std::move(other);
    return *this;
  }

  // Forces `value` to be a list, and adds a new element to it.  Returns a new
  // QueryValueBuilder for the new list element.
  QueryValueBuilder append() {
    ListValue* list_value = query_value_.mutable_list_value();
    return QueryValueBuilder(list_value->add_values());
  }

  // Forces `value` to be a list, and adds a new scalar element to it.
  template <typename T>
  void append(T element) {
    append() = element;
  }

  // Forces `value` to be a QueryValueBuilder, and ensures that there is a field
  // with the given `name`, creating it if necessary.  Returns a new
  // QueryValueBuilder for the field's value.
  QueryValueBuilder operator[](absl::string_view name) {
    QueryResultData* result_value = query_value_.mutable_subquery_value();
    return QueryValueBuilder(
        &(*result_value->mutable_fields())[std::string(name)]);
  }

  // Forces `value` to be a QueryValueBuilder, and allows users to modify
  // elements of a QueryValue list item. Note that modifications may alter the
  // list item's type.
  QueryValueBuilder at(int index) {
    CHECK(index >= 0 && index < query_value_.list_value().values_size())
        << "Invalid index: " << index;
    return QueryValueBuilder(
        query_value_.mutable_list_value()->mutable_values(index));
  }

 private:
  QueryValue& query_value_;
};

// A builder class for ecclesia::QueryResultData.
class QueryResultDataBuilder {
 public:
  // Creates a new QueryResultDataBuilder that will fill in the contents of
  // `unowned_query_result`.  We do not take ownership of the QueryResultData;
  // it must outlive this QueryResultDataBuilder, and any QueryValueBuilders
  // that are created for subfields.
  explicit QueryResultDataBuilder(QueryResultData* unowned_query_result)
      : query_result_(*ABSL_DIE_IF_NULL(unowned_query_result)) {}

  // Ensures that the QueryResultData contains a field with the given `name`,
  // creating it if necessary.  Returns a new QueryValueBuilder for the field's
  // value.
  QueryValueBuilder operator[](absl::string_view key) {
    return QueryValueBuilder(
        &(*query_result_.mutable_fields())[std::string(key)]);
  }

 private:
  QueryResultData& query_result_;
};

// A reader class for ecclesia::QueryValue.
class QueryValueReader {
 public:
  // Creates a new QueryValueReader that will allow reading the contents of
  // `query_value`. We do not take ownership of `query_value`; it must outlive
  // this QueryValueReader, and any child QueryValueReaders created for
  // subfields.
  explicit QueryValueReader(const QueryValue* query_value)
      : query_value_(*ABSL_DIE_IF_NULL(query_value)) {}

  // Returns true if there is a subquery result for the given key.
  bool Has(absl::string_view key) const {
    return query_value_.has_subquery_value() &&
           query_value_.subquery_value().fields().contains(key);
  }

  // Returns a QueryValueReader for the underlying subquery result for the given
  // key; or an error if the key is not found.
  absl::StatusOr<QueryValueReader> Get(absl::string_view key) const;

  // Returns the appropriately-typed value for the given `PropertyDefinition`.
  template <typename PropertyDefinitionT>
  absl::StatusOr<typename PropertyDefinitionT::type> GetValue() const {
    if constexpr (std::is_same_v<typename PropertyDefinitionT::type,
                                 std::string>) {
      return GetStringValue(PropertyDefinitionT::Name);
    }
    if constexpr (std::is_same_v<typename PropertyDefinitionT::type, int>) {
      return GetIntValue(PropertyDefinitionT::Name);
    }
    if constexpr (std::is_same_v<typename PropertyDefinitionT::type, double>) {
      return GetDoubleValue(PropertyDefinitionT::Name);
    }
    if constexpr (std::is_same_v<typename PropertyDefinitionT::type, bool>) {
      return GetBoolValue(PropertyDefinitionT::Name);
    }
  }

  // Returns the string value for a given key; returns error if the key is not
  // present.
  absl::StatusOr<std::string> GetStringValue(absl::string_view key) const;

  // Returns the int value for a given key; returns error if the key is not
  // present.
  absl::StatusOr<int64_t> GetIntValue(absl::string_view key) const;

  // Returns the double value for a given key; returns error if the key is not
  // present.
  absl::StatusOr<double> GetDoubleValue(absl::string_view key) const;

  // Returns the boolean value for a given key; returns error if the key is not
  // present.
  absl::StatusOr<bool> GetBoolValue(absl::string_view key) const;

  // Returns the identifier for the underlying subquery value;
  absl::StatusOr<Identifier> GetIdentifier() const;

  // Returns a QueryValueReader for the underlying subquery result.
  QueryValueReader operator[](absl::string_view key) const {
    CHECK(query_value_.has_subquery_value());
    return QueryValueReader(&query_value_.subquery_value().fields().at(key));
  }

  // Returns a QueryValueReader for the value of the List item at the given
  // index.
  QueryValueReader operator[](int index) const {
    CHECK(query_value_.has_list_value());
    if (index < 0 || index >= query_value_.list_value().values_size()) {
      LOG(FATAL) << "Invalid index: " << index;
    }
    return QueryValueReader(&query_value_.list_value().values(index));
  }

  // Returns the size of the list value.
  int size() const {
    CHECK(query_value_.has_list_value());
    return query_value_.list_value().values_size();
  }

  // Returns the string value.
  std::string string_value() const { return query_value_.string_value(); }

  // Returns the int value.
  int64_t int_value() const { return query_value_.int_value(); }

  // Returns the double value.
  double double_value() const { return query_value_.double_value(); }

  // Returns the bool value.
  bool bool_value() const { return query_value_.bool_value(); }

  // Returns the names of the keys in the underlying subquery value.
  std::vector<std::string> field_keys() const;

  // Returns a const reference to the Identifier value.
  const Identifier& identifier() const { return query_value_.identifier(); }

  // Returns the timestamp value.
  google::protobuf::Timestamp timestamp_value() const {
    return query_value_.timestamp_value();
  }

  // Returns the list values. Can be used for iterating through the list
  // elements. This also supports range-based loops.
  google::protobuf::RepeatedPtrField<QueryValue> list_values() const {
    return query_value_.list_value().values();
  }

  // Returns the type of the `query_value`.
  QueryValue::KindCase kind() const { return query_value_.kind_case(); }

 private:
  const QueryValue& query_value_;
};

// A reader class for ecclesia::QueryResultData.
class QueryResultDataReader {
 public:
  // Creates a new QueryResultDataReader that will allow reading the contents of
  // `unowned_query_result`.  We do not take ownership of the QueryResultData;
  // it must outlive this QueryResultDataReader, and any QueryValueReaders that
  // are created for subfields.
  explicit QueryResultDataReader(const QueryResultData* unowned_query_result)
      : query_result_(*ABSL_DIE_IF_NULL(unowned_query_result)) {}

  QueryValueReader operator[](absl::string_view key) const {
    return QueryValueReader(&query_result_.fields().at(key));
  }

  bool Has(absl::string_view key) const {
    return query_result_.fields().contains(key);
  }

  // Returns a QueryValueReader for the given key; or an error if the key is not
  // found.
  absl::StatusOr<QueryValueReader> Get(absl::string_view key) const;

 private:
  const QueryResultData& query_result_;
};

// Encapsulates query result for a subscription query along with subscription
// handle.
struct SubscriptionQueryResult {
  QueryIdToResult result;

  // Subscription handle used to cancel an event subscription.
  std::unique_ptr<RedPathSubscription> subscription;
};

// Utility functions to help process Query Results

// Returns the query result from the Result Map for a specified query id.
// Returns an error if the query is not found in the map. Also returns an error
// if the query result contains an error.
//
// Please note that this function  accepts the query result map as value; if the
// query result is found in the map, it will be moved. Clients should expect the
// map to be in an invalid state after the function.
absl::StatusOr<QueryResult> GetQueryResult(QueryIdToResult result,
                                           absl::string_view query_id);

// Returns true if the query result contains an error.
bool QueryResultHasErrors(const QueryResult& query_result);

// Returns true if at least one of the query results in the map contains an
// error.
bool QueryOutputHasErrors(const QueryIdToResult& query_output);

// Decodes the start and end time in the query and returns the elapsed time.
// Returns error if the time or the duration is invalid or if the result doesn't
// contain time statistics.
absl::StatusOr<absl::Duration> GetQueryDuration(
    const QueryResult& query_result);

// Removes the subquery result for the given identifier from the query result.
// Returns true if the subquery result was found and removed. Returns false if
// the subquery result was not found.
bool RemoveDataForIdentifier(QueryResult& query_result,
                             const Identifier& identifier);

// Returns the subquery result for the given identifier from the query result.
// Returns error if the subquery result was not found.
absl::StatusOr<std::vector<QueryResultData>> GetDataForIdentifier(
    const QueryResult& query_result, const Identifier& identifier);

// Converts query result status to an absl::Status.
absl::Status StatusFromQueryResultStatus(const ecclesia::Status& status);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_RESULT_QUERY_RESULT_H_
