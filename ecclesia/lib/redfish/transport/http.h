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

#ifndef ECCLESIA_LIB_REDFISH_TRANSPORT_HTTP_H_
#define ECCLESIA_LIB_REDFISH_TRANSPORT_HTTP_H_

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

class HttpRedfishTransport : public RedfishTransport {
 public:
  // Params:
  //   client: HttpClient instance
  //   endpoint: e.g. "localhost:80", "https://10.0.0.1", "[::1]:8000"
  HttpRedfishTransport(std::unique_ptr<HttpClient> client,
                       std::string endpoint);
  absl::StatusOr<Result> Get(absl::string_view path) override;
  absl::StatusOr<Result> Post(absl::string_view path,
                              absl::string_view data) override;
  absl::StatusOr<Result> Patch(absl::string_view path,
                               absl::string_view data) override;
  absl::StatusOr<Result> Delete(absl::string_view path,
                                absl::string_view data) override;

 private:
  std::unique_ptr<HttpClient> client_;
  std::string endpoint_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_HTTP_H_
