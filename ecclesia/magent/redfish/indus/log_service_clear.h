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

#ifndef ECCLESIA_MAGENT_REDFISH_INDUS_LOG_SERVICE_CLEAR_H_
#define ECCLESIA_MAGENT_REDFISH_INDUS_LOG_SERVICE_CLEAR_H_

#include <sys/stat.h>
#include <sys/wait.h>

#include <cstdlib>
#include <string>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "json/value.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

ABSL_DECLARE_FLAG(std::string, system_event_clear_script_path);

namespace ecclesia {

class LogServiceClear : public Resource {
 public:
  LogServiceClear()
      : Resource(absl::StrCat(kLogServiceSystemEventsUri, "/Actions/",
                              kLogService, ".", kClearLog)) {}

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &params) override {
    req->ReplyWithStatus(
        tensorflow::serving::net_http::HTTPStatusCode::METHOD_NA);
  }

  void Post(tensorflow::serving::net_http::ServerRequestInterface *req,
            const ParamsType &params) override {
    std::string script = absl::GetFlag(FLAGS_system_event_clear_script_path);
    if (script.empty()) {
      req->ReplyWithStatus(
          tensorflow::serving::net_http::HTTPStatusCode::METHOD_NA);
      return;
    } else if (!FileExists(script.c_str())) {
      Respond("Base.1.0.GeneralError", "Script not found",
              tensorflow::serving::net_http::HTTPStatusCode::NOT_FOUND, req);
      return;
    }

    int status = std::system(script.c_str());
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
      Respond("Base.1.0.Success", "Success",
              tensorflow::serving::net_http::HTTPStatusCode::OK, req);
    } else {
      Respond("Base.1.0.GeneralError", "Failed to execute script",
              tensorflow::serving::net_http::HTTPStatusCode::ERROR, req);
    }
  }

  static bool FileExists(const char *path) {
    struct stat buffer;
    return stat(path, &buffer) == 0;
  }

  static void Respond(
      absl::string_view rf_code, absl::string_view msg,
      tensorflow::serving::net_http::HTTPStatusCode code,
      tensorflow::serving::net_http::ServerRequestInterface *req) {
    Json::Value res;
    res[kResponseError][kResponseCode] = std::string(rf_code);
    res[kResponseError][kResponseMessage] = std::string(msg);
    JSONResponse(res, code, req);
  }
};

}  // namespace ecclesia
#endif  // ECCLESIA_MAGENT_REDFISH_INDUS_LOG_SERVICE_CLEAR_H_
