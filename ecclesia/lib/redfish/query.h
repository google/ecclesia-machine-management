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

#ifndef ECCLESIA_LIB_REDFISH_QUERY_H_
#define ECCLESIA_LIB_REDFISH_QUERY_H_

#include "ecclesia/lib/redfish/sysmodel.h"

namespace ecclesia {
// GetRedPath invokes result_callback in order to be able to pull out desired
// resources queried by handling the Redpath.
// GitHub link to the RedPath spec : https://github.com/DMTF/libredfish#redpath
void GetRedPath(RedfishInterface* intf, absl::string_view redpath,
                Sysmodel::ResultCallback result_callback);

}  // namespace ecclesia
#endif  // ECCLESIA_LIB_REDFISH_QUERY_H_
