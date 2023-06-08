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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_CONFIG_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_CONFIG_H_
#include <vector>

#include "ecclesia/lib/file/cc_embed_interface.h"

namespace ecclesia {

struct QueryEngineConfiguration {
  struct Flags {
    // Functionally extends the query engine for building the Node Topology
    // referenced for populating devpaths in a query response.
    bool enable_devpath_extension = false;
    // Configures Query Engine to store and reference Redfish URIs of all the
    // nodes of the redfish tree queried. The lifetime of cache is tied to the
    // lifetime of the query engine instance.
    bool enable_cached_uri_dispatch = false;
    // Instructs Query Engine to use MetricalRedfishTransport which will collect
    // response time metrics.
    bool enable_transport_metrics = false;
  };
  Flags flags;
  // available and not passed to QueryEngine through engine configuration.
  std::vector<EmbeddedFile> query_files;
  std::vector<EmbeddedFile> query_rules;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_CONFIG_H_
