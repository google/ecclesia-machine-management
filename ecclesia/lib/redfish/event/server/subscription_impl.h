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

#ifndef ECCLESIA_LIB_REDFISH_EVENT_SERVER_SUBSCRIPTION_IMPL_H_
#define ECCLESIA_LIB_REDFISH_EVENT_SERVER_SUBSCRIPTION_IMPL_H_

#include <memory>

#include "ecclesia/lib/redfish/event/server/subscription.h"

namespace ecclesia {

// Returns an implementation of SubscriptionService.
// SubscriptionService is provided a |redfish_handler| to subscribe and query
// redfish resources and |subscription_store| tracks created event subscriptions
// and is queried on each event.
std::unique_ptr<SubscriptionService> CreateSubscriptionService(
    std::unique_ptr<RedfishHandler> redfish_handler,
    std::unique_ptr<SubscriptionStore> subscription_store);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_EVENT_SERVER_SUBSCRIPTION_IMPL_H_
