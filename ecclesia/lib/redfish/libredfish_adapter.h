/*
 * Copyright 2021 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_LIBREDFISH_ADAPTER_H_
#define ECCLESIA_LIB_REDFISH_LIBREDFISH_ADAPTER_H_

#include <memory>

#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/redfish/raw.h"

extern "C" {
#include "redfishService.h"
}

namespace libredfish {

redfishAsyncOptions ConvertOptions(const RedfishRawInterfaceOptions& options);

serviceHttpHandler NewLibredfishAdapter(
    std::unique_ptr<ecclesia::HttpClient> client,
    const RedfishRawInterfaceOptions& default_options);

}  // namespace libredfish

#endif  // ECCLESIA_LIB_REDFISH_LIBREDFISH_ADAPTER_H_
