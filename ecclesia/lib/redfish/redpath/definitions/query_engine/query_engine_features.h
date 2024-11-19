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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ENGINE_QUERY_ENGINE_FEATURES_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ENGINE_QUERY_ENGINE_FEATURES_H_

#include <type_traits>

#include "ecclesia/lib/redfish/redpath/definitions/passkey/annotation_passkey.h"
#include "ecclesia/lib/redfish/redpath/definitions/passkey/log_redfish_traces_passkey.h"
#include "ecclesia/lib/redfish/redpath/definitions/passkey/metrics_passkey.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_engine_features.pb.h"

namespace ecclesia {

inline QueryEngineFeatures DefaultQueryEngineFeatures() {
  QueryEngineFeatures features;
  features.set_fail_on_first_error(true);
  return features;
}

inline QueryEngineFeatures StandardQueryEngineFeatures() {
  QueryEngineFeatures features;
  features.set_fail_on_first_error(true);
  features.set_enable_streaming(true);
  return features;
}

template <typename... Passkeys>
inline QueryEngineFeatures EnableQueryEngineFeatures(
    Passkeys... list_passkeys) {
  QueryEngineFeatures features = DefaultQueryEngineFeatures();
  for (const auto p : {list_passkeys...}) {
    using T = std::decay_t<decltype(p)>;
    if constexpr (std::is_same_v<T, RedfishAnnotationsPasskey>) {
      features.set_enable_url_annotation(true);
    }
    if constexpr (std::is_same_v<T, RedfishMetricsPasskey>) {
      features.set_enable_redfish_metrics(true);
    }
    if constexpr (std::is_same_v<T, RedfishLogRedfishTracesPasskey>) {
      features.set_log_redfish_traces(true);
    }
  }

  return features;
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ENGINE_QUERY_ENGINE_FEATURES_H_
