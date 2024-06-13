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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ROUTER_UTIL_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ROUTER_UTIL_H_


#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_spec.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_router/query_router_spec.pb.h"

namespace ecclesia {

// The process_fn callback is invoked to handle the Query Selection Spec found
// in the Query Router spec, based on the specified server_tag and server_type.
//
// Returns an error if the selection spec is empty or the selection criteria in
// the Query Router spec is not valid; if the callback function returns an
// error.
absl::Status ProcessQueryRouterSpec(
    const QueryRouterSpec& router_spec, absl::string_view server_tag,
    SelectionSpec::SelectionClass::ServerType server_type,
    absl::AnyInvocable<absl::Status(absl::string_view,
                                    const SelectionSpec::QuerySelectionSpec&)>
        process_fn);

// Returns the QuerySpec from the Query Router Spec for the given `server_tag`
// and `server_type`.
//
// Returns an error if selection criteria in the Query Router spec is not valid;
// if the Query Files and Rule Files cannot be read and if the Query File
// doesn't match the Query Id in the Query Router spec.
absl::StatusOr<QuerySpec> GetQuerySpec(
    const QueryRouterSpec& router_spec, absl::string_view server_tag,
    SelectionSpec::SelectionClass::ServerType server_type);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ROUTER_UTIL_H_
