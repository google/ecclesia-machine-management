/*
 * Copyright 2020 Google LLC
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

#include "ecclesia/magent/redfish/indus/memory_metrics.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "ecclesia/lib/mcedecoder/cpu_topology.h"
#include "ecclesia/magent/lib/event_logger/indus/system_event_visitors.h"
#include "ecclesia/magent/lib/event_logger/intel_cpu_topology.h"
#include "ecclesia/magent/lib/event_logger/system_event_visitors.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "json/value.h"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

namespace {

// Get the lastest memory error counts. The function caches the last known
// event time stamp and the error counts. On every call it accumulates the error
// counts since the last time.
const absl::flat_hash_map<int, DimmErrorCount> &GetMemoryErrors(
    SystemModel *system_model) {
  static absl::Time last_event_timestamp = absl::UnixEpoch();
  static auto &result_error_counts =
      *(new absl::flat_hash_map<int, DimmErrorCount>());

  std::unique_ptr<DimmErrorCountingVisitor> visitor =
      CreateIndusDimmErrorCountingVisitor(
          last_event_timestamp, absl::make_unique<IntelCpuTopology>());

  system_model->VisitSystemEvents(visitor.get());
  // update the last event time stamp
  if (visitor->GetLatestRecordTimeStamp()) {
    last_event_timestamp = visitor->GetLatestRecordTimeStamp().value();
  }

  // The visitor scans for records that were not processed since the last time.
  // So accumulate the error counts into the result
  auto error_counts = visitor->GetDimmErrorCounts();

  for (auto &[dimm_num, error_count] : error_counts) {
    if (result_error_counts.contains(dimm_num)) {
      result_error_counts[dimm_num] += error_count;
    } else {
      result_error_counts[dimm_num] = error_count;
    }
  }

  return result_error_counts;
}

}  // namespace

void MemoryMetrics::Get(
    tensorflow::serving::net_http::ServerRequestInterface *req,
    const ParamsType &params) {
  // Expect to be passed in the dimm index
  if (!ValidateResourceIndex(params[0], system_model_->NumDimms())) {
    req->ReplyWithStatus(
        tensorflow::serving::net_http::HTTPStatusCode::NOT_FOUND);
    return;
  }

  int dimm_num = std::get<int>(params[0]);
  const absl::flat_hash_map<int, DimmErrorCount> &mem_errors =
      GetMemoryErrors(system_model_);

  // Fill in the json response
  Json::Value json;
  AddStaticFields(&json);
  json[kOdataId] = std::string(req->uri_path());

  // Error counts are added as an Oem field
  auto *oem = GetJsonObject(&json, kOem);
  auto *google = GetJsonObject(oem, kGoogle);
  auto *memory_error_counts = GetJsonObject(google, kMemoryErrorCounts);

  if (mem_errors.contains(dimm_num)) {
    (*memory_error_counts)[kCorrectable] = mem_errors.at(dimm_num).correctable;
    (*memory_error_counts)[kUncorrectable] =
        mem_errors.at(dimm_num).uncorrectable;
  } else {
    (*memory_error_counts)[kCorrectable] = 0;
    (*memory_error_counts)[kUncorrectable] = 0;
  }

  JSONResponseOK(json, req);
}

}  // namespace ecclesia
