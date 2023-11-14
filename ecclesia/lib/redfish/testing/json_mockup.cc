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

#include "ecclesia/lib/redfish/testing/json_mockup.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/functional/function_ref.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/interface.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

class JsonMockupVariantImpl : public RedfishVariant::ImplIntf {
 public:
  explicit JsonMockupVariantImpl(nlohmann::json json_view)
      : json_view_(std::move(json_view)) {}
  std::unique_ptr<RedfishObject> AsObject() const override;
  std::unique_ptr<RedfishIterable> AsIterable(
      RedfishVariant::IterableMode mode,
      GetParams::Freshness freshness) const override;
  std::optional<RedfishTransport::bytes> AsRaw() const override {
    return std::nullopt;
  }
  std::string DebugString() const override;

  bool GetValue(std::string *val) const override {
    if (!json_view_.is_string()) return false;
    *val = json_view_.get<std::string>();
    return true;
  }
  bool GetValue(int32_t *val) const override {
    if (!json_view_.is_number_integer()) return false;
    *val = json_view_.get<int32_t>();
    return true;
  }
  bool GetValue(int64_t *val) const override {
    if (!json_view_.is_number_integer()) return false;
    *val = json_view_.get<int64_t>();
    return true;
  }
  bool GetValue(double *val) const override {
    if (!json_view_.is_number()) return false;
    *val = json_view_.get<double>();
    return true;
  }
  bool GetValue(bool *val) const override {
    if (!json_view_.is_boolean()) return false;
    *val = json_view_.get<bool>();
    return true;
  }
  bool GetValue(absl::Time *val) const override {
    if (!json_view_.is_string()) return false;
    std::string dt_string = json_view_.get<std::string>();
    std::string err;
    absl::ParseTime("%Y-%m-%dT%H:%M:%S%Z", dt_string, val, &err);
    return err.empty();
  }

 private:
  nlohmann::json json_view_;
};

class JsonMockupIterable : public RedfishIterable {
 public:
  explicit JsonMockupIterable(nlohmann::json json_view)
      : json_view_(std::move(json_view)) {}
  size_t Size() override {
    // Case 1: JSON is array
    if (json_view_.is_array()) return json_view_.size();
    // Case 2: JSON is possibly a Redfish Collection
    if (json_view_.is_object()) {
      JsonMockupObject obj(json_view_);
      auto size = obj.GetNodeValue<int>("Members@odata.count");
      if (size.has_value()) return size.value();
    }
    return 0;
  }
  bool Empty() override { return Size() == 0; }
  RedfishVariant operator[](int index) const override {
    // Case 1: JSON is array
    if (json_view_.is_array() && index >= 0 && json_view_.size() > index) {
      return RedfishVariant(
          std::make_unique<JsonMockupVariantImpl>(json_view_.at(index)));
    }
    // Case 2: JSON is possibly a Redfish Collection
    if (json_view_.is_object()) {
      auto arr = json_view_.find("Members");
      if (arr != json_view_.end() && arr.value().is_array() && index >= 0 &&
          arr.value().size() > index) {
        return RedfishVariant(
            std::make_unique<JsonMockupVariantImpl>(arr.value().at(index)));
      }
    }
    // Default case: return nothing
    return RedfishVariant(absl::NotFoundError(
        absl::StrCat("Could not find index: ", index,
                     " in JSON: ", json_view_.dump(/*indent=*/1))));
  }

 private:
  nlohmann::json json_view_;
};

std::unique_ptr<RedfishObject> JsonMockupVariantImpl::AsObject() const {
  if (json_view_.is_object()) {
    return std::make_unique<JsonMockupObject>(json_view_);
  }
  return nullptr;
}

std::unique_ptr<RedfishIterable> JsonMockupVariantImpl::AsIterable(
    RedfishVariant::IterableMode mode, GetParams::Freshness freshness) const {
  // Verify that we are actually iterable, either as an array or a Collection
  if (json_view_.is_array()) {
    return std::make_unique<JsonMockupIterable>(json_view_);
  } else if (json_view_.is_object()) {
    auto members = json_view_.find("Members");
    if (members == json_view_.end() || !members->is_array()) return nullptr;
    auto size = json_view_.find("Members@odata.count");
    if (size == json_view_.end() || !size->is_number_integer()) return nullptr;
    return std::make_unique<JsonMockupIterable>(json_view_);
  }
  return nullptr;
}

std::string JsonMockupVariantImpl::DebugString() const {
  return json_view_.dump(/*indent=*/1);
}

class JsonMockupMockup : public RedfishInterface {
 public:
  explicit JsonMockupMockup(absl::string_view raw_json) {
    json_model_ = nlohmann::json::parse(raw_json, nullptr, false);
    if (json_model_.is_discarded()) {
      LOG(FATAL) << "Could not load JSON.";
    }
  }
  bool IsTrusted() const override { return true; }
  void UpdateTransport(std::unique_ptr<RedfishTransport> new_transport,
                       TrustedEndpoint trusted) override {
    // There's no reason why this cannot be a no-op, but for now just terminate
    // in case someone is expecting some particular behaviour.
    LOG(FATAL) << "Tried to update the endpoint of a JsonMockup";
  }
  RedfishVariant GetRoot(GetParams params,
                         ServiceRootUri service_root) override {
    return RedfishVariant(std::make_unique<JsonMockupVariantImpl>(json_model_));
  }
  RedfishVariant GetRoot(GetParams params,
                         absl::string_view service_root) override {
    return RedfishVariant(std::make_unique<JsonMockupVariantImpl>(json_model_));
  }

  RedfishVariant UncachedGetUri(absl::string_view uri,
                                GetParams params) override {
    // We will implement GetUri as walking the URI from the root JSON node.

    auto current_json = json_model_;
    auto path_list = absl::StrSplit(uri, '/');
    for (const auto &p : path_list) {
      if (p.empty()) continue;
      if (current_json.is_object()) {
        // Treat it as a pure JSON object:
        auto next_node = current_json.find(p);
        if (next_node != current_json.end()) {
          current_json = next_node.value();
          continue;
        }
        // Treat it as a collection:
        auto members = current_json.find("Members");
        if (members != current_json.end() && members.value().is_array()) {
          current_json = members.value();
          // Intentionally fall through and allow the current_json.is_array()
          // case handle the indexing.
        }
      }
      if (current_json.is_array()) {
        size_t index;
        if (absl::SimpleAtoi(p, &index) && index >= 0 &&
            index < current_json.size()) {
          current_json = current_json.at(index);
          continue;
        }
      }
      // We failed to make progress on the path
      return RedfishVariant(
          absl::NotFoundError(absl::StrCat("Could not resolve URI", uri)));
    }
    return RedfishVariant(
        std::make_unique<JsonMockupVariantImpl>(current_json));
  }
  RedfishVariant CachedGetUri(absl::string_view uri,
                              GetParams params) override {
    // There is no caching in this mockup.
    return UncachedGetUri(uri, params);
  }
  RedfishVariant PostUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) override {
    return RedfishVariant(
        absl::UnimplementedError("Updates to json_mockup are not supported."));
  }
  RedfishVariant DeleteUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) override {
    return RedfishVariant(
        absl::UnimplementedError("Deletes to json_mockup are not supported."));
  }
  RedfishVariant CachedPostUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span,
      absl::Duration duration) override {
    return RedfishVariant(absl::UnimplementedError(
        "Cached POST to json_mockup is not supported."));
  }
  RedfishVariant PostUri(absl::string_view uri,
                         absl::string_view data) override {
    return RedfishVariant(
        absl::UnimplementedError("Updates to json_mockup are not supported."));
  }
  RedfishVariant DeleteUri(absl::string_view uri,
                           absl::string_view data) override {
    return RedfishVariant(
        absl::UnimplementedError("Deletes to json_mockup are not supported."));
  }
  RedfishVariant PatchUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) override {
    return RedfishVariant(
        absl::UnimplementedError("Updates to json_mockup are not supported."));
  }
  RedfishVariant PatchUri(absl::string_view uri,
                          absl::string_view data) override {
    return RedfishVariant(
        absl::UnimplementedError("Updates to json_mockup are not supported."));
  }

 private:
  nlohmann::json json_model_;
};

}  // namespace

RedfishVariant JsonMockupObject::Get(const std::string &node_name,
                                     GetParams params) const {
  auto node = json_view_.find(node_name);
  if (node == json_view_.end())
    return RedfishVariant(absl::NotFoundError(absl::StrCat(
        "no node '", node_name, "' in JSON: ", json_view_.dump(/*indent=*/1))));
  return RedfishVariant(std::make_unique<JsonMockupVariantImpl>(node.value()));
}

void JsonMockupObject::ForEachProperty(
    absl::FunctionRef<RedfishIterReturnValue(absl::string_view,
                                             RedfishVariant value)>
        itr_func) {
  for (const auto &items : json_view_.items()) {
    if (itr_func(items.key(),
                 RedfishVariant(std::make_unique<JsonMockupVariantImpl>(
                     items.value()))) == RedfishIterReturnValue::kStop) {
      break;
    }
  }
}

std::unique_ptr<RedfishInterface> NewJsonMockupInterface(
    absl::string_view raw_json) {
  return std::make_unique<JsonMockupMockup>(raw_json);
}

}  // namespace ecclesia
