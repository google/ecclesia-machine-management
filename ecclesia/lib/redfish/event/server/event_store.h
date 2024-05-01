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

#ifndef ECCLESIA_LIB_REDFISH_EVENT_SERVER_EVENT_STORE_H_
#define ECCLESIA_LIB_REDFISH_EVENT_SERVER_EVENT_STORE_H_

#include <cstddef>
#include <memory>

#include "ecclesia/lib/redfish/event/server/subscription.h"

namespace ecclesia {

// Creates EventStore that can contain events up to the count `store_size`.
std::unique_ptr<EventStore> CreateEventStore(size_t store_size);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_EVENT_SERVER_EVENT_STORE_H_
