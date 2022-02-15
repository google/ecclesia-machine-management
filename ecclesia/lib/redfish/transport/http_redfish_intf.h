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

#ifndef ECCLESIA_LIB_REDFISH_TRANSPORT_HTTP_REDFISH_INTF_H_
#define ECCLESIA_LIB_REDFISH_TRANSPORT_HTTP_REDFISH_INTF_H_

#include <functional>
#include <memory>

#include "absl/base/attributes.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

using RedfishTransportCacheFactory =
    std::function<std::unique_ptr<ecclesia::RedfishCachedGetterInterface>(
        ecclesia::RedfishTransport*)>;

// Constructs a RedfishInterface backed by a Redfish Transport.
std::unique_ptr<RedfishInterface> NewHttpInterface(
    std::unique_ptr<ecclesia::RedfishTransport> transport,
    std::unique_ptr<ecclesia::RedfishCachedGetterInterface> cache,
    RedfishInterface::TrustedEndpoint trusted,
    ServiceRootUri service_root = ServiceRootUri::kRedfish);

// Constructs a RedfishInterface backed by a Redfish Transport with a cache
// factory. Is useful for the use-case when transport can be changed runtime
std::unique_ptr<RedfishInterface> NewHttpInterface(
    std::unique_ptr<ecclesia::RedfishTransport> transport,
    RedfishTransportCacheFactory cache_factory,
    RedfishInterface::TrustedEndpoint trusted,
    ServiceRootUri service_root = ServiceRootUri::kRedfish);
}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_HTTP_REDFISH_INTF_H_
