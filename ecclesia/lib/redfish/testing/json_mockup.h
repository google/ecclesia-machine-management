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

#ifndef ECCLESIA_LIB_REDFISH_TESTING_JSON_MOCKUP_H_
#define ECCLESIA_LIB_REDFISH_TESTING_JSON_MOCKUP_H_

#include <memory>

#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

// Constructor method for creating a JsonMockupInterface.
// Returns nullptr in case the interface failed to be constructed.
std::unique_ptr<RedfishInterface> NewJsonMockupInterface(
    absl::string_view raw_json);

class JsonMockupObject : public RedfishObject {
 public:
  explicit JsonMockupObject(nlohmann::json json_view)
      : json_view_(std::move(json_view)) {}
  RedfishVariant operator[](const std::string &node_name) const override;

  std::optional<std::string> GetUriString() const override {
    return GetNodeValue<std::string>("@odata.id");
  }

  nlohmann::json GetContentAsJson() const override { return json_view_; }

  std::string DebugString() override { return json_view_.dump(/*indent=*/1); }

  std::unique_ptr<RedfishObject> EnsureFreshPayload(GetParams params) override {
    return std::unique_ptr<RedfishObject>(this);
  }

  void ForEachProperty(absl::FunctionRef<RedfishIterReturnValue(
                           absl::string_view, RedfishVariant value)>
                           itr_func) override;

  nlohmann::json json_view_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TESTING_JSON_MOCKUP_H_
