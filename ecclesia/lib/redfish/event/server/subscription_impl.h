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
// SubscriptionService is provided a `subscription_backend` to subscribe and
// query redfish resources, a `subscription_store` to track and query created
// event subscriptions and an `event_store` to store Redfish events to dispatch.
std::unique_ptr<SubscriptionService> CreateSubscriptionService(
    std::unique_ptr<SubscriptionBackend> subscription_backend,
    std::unique_ptr<SubscriptionStore> subscription_store,
    std::unique_ptr<EventStore> event_store);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_EVENT_SERVER_SUBSCRIPTION_IMPL_H_
