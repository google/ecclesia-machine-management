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
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/redfish/interface.h"
#include "jansson.h"

namespace libredfish {
namespace {

struct JsonDeleter {
  inline void operator()(json_t *ptr) const { json_decref(ptr); }
};
using JsonUniquePtr = std::unique_ptr<json_t, JsonDeleter>;
struct FreeDeleter {
  inline void operator()(void *ptr) const { free(ptr); }
};
using MallocChar = std::unique_ptr<char, FreeDeleter>;

class JsonMockupVariantImpl : public RedfishVariant::ImplIntf {
 public:
  explicit JsonMockupVariantImpl(json_t *json_view) : json_view_(json_view) {}
  std::unique_ptr<RedfishObject> AsObject() override;
  std::unique_ptr<RedfishIterable> AsIterable() override;
  std::string DebugString() override;

  bool GetValue(std::string *val) override {
    if (json_view_ == nullptr || !json_is_string(json_view_)) return false;
    *val = std::string(json_string_value(json_view_));
    return true;
  }
  bool GetValue(int32_t *val) override {
    if (json_view_ == nullptr || !json_is_integer(json_view_)) return false;
    *val = (int)json_integer_value(json_view_);
    return true;
  }
  bool GetValue(int64_t *val) override {
    if (json_view_ == nullptr || !json_is_integer(json_view_)) return false;
    *val = json_integer_value(json_view_);
    return true;
  }
  bool GetValue(double *val) override {
    if (json_view_ == nullptr || !json_is_real(json_view_)) return false;
    *val = json_real_value(json_view_);
    return true;
  }
  bool GetValue(bool *val) override {
    if (json_view_ == nullptr || !json_is_boolean(json_view_)) return false;
    *val = json_boolean_value(json_view_);
    return true;
  }

 private:
  json_t *json_view_;
};

class JsonMockupObject : public RedfishObject {
 public:
  explicit JsonMockupObject(json_t *json_view)
      : json_view_(ecclesia::DieIfNull(json_view)) {}
  RedfishVariant GetNode(const std::string &node_name) const override {
    return RedfishVariant(absl::make_unique<JsonMockupVariantImpl>(
        json_object_get(json_view_, node_name.c_str())));
  }
  absl::optional<std::string> GetUri() override {
    return GetNodeValue<std::string>("@odata.id");
  }

  json_t *json_view_;
};

class JsonMockupIterable : public RedfishIterable {
 public:
  explicit JsonMockupIterable(json_t *json_view)
      : json_view_(ecclesia::DieIfNull(json_view)) {}
  size_t Size() override {
    // Case 1: JSON is array
    if (json_is_array(json_view_)) return json_array_size(json_view_);
    // Case 2: JSON is possibly a Redfish Collection
    if (json_is_object(json_view_)) {
      JsonMockupObject obj(json_view_);
      auto size = obj.GetNodeValue<int>("Members@odata.count");
      if (size.has_value()) return size.value();
    }
    return 0;
  }
  bool Empty() override { return Size() == 0; }
  RedfishVariant GetIndex(int index) override {
    // Case 1: JSON is array
    if (json_is_array(json_view_)) {
      return RedfishVariant(absl::make_unique<JsonMockupVariantImpl>(
          json_array_get(json_view_, index)));
    }
    // Case 2: JSON is possibly a Redfish Collection
    if (json_is_object(json_view_)) {
      JsonMockupObject obj(json_view_);
      auto members = obj.GetNode("Members");
      auto members_view = members.AsIterable();
      if (members_view) {
        return members_view->GetIndex(index);
      }
    }
    // Default case: return nothing
    return RedfishVariant(nullptr);
  }

 private:
  json_t *json_view_;
};

std::unique_ptr<RedfishObject> JsonMockupVariantImpl::AsObject() {
  if (!json_view_) {
    return nullptr;
  }
  return absl::make_unique<JsonMockupObject>(json_view_);
}

std::unique_ptr<RedfishIterable> JsonMockupVariantImpl::AsIterable() {
  if (!json_view_) {
    return nullptr;
  }
  // Verify that we are actually iterable, either as an array or a Collection
  if (json_is_array(json_view_)) {
    return absl::make_unique<JsonMockupIterable>(json_view_);
  } else if (json_is_object(json_view_)) {
    json_t *members = json_object_get(json_view_, "Members");
    if (!members || !json_is_array(members)) return nullptr;
    json_t *size = json_object_get(json_view_, "Members@odata.count");
    if (!size || !json_is_integer(size)) return nullptr;
    return absl::make_unique<JsonMockupIterable>(json_view_);
  }
  return nullptr;
}

std::string JsonMockupVariantImpl::DebugString() {
  if (!json_view_) return "(null payload)";
  size_t flags = JSON_INDENT(2);
  MallocChar output(json_dumps(json_view_, flags));
  if (!output) return "(null output)";
  std::string ret(output.get());
  return ret;
}

class JsonMockupMockup : public RedfishInterface {
 public:
  explicit JsonMockupMockup(absl::string_view raw_json) {
    json_error_t err;
    json_model_ = JsonUniquePtr(json_loads(raw_json.data(), 0, &err));
    if (!json_model_) {
      ecclesia::FatalLog() << "Could not load JSON: " << err.text;
    }
  }
  bool IsTrusted() const override { return true; }
  void UpdateEndpoint(absl::string_view endpoint,
                      TrustedEndpoint trusted) override {
    // There's no reason why this cannot be a no-op, but for now just terminate
    // in case someone is expecting some particular behaviour.
    ecclesia::FatalLog() << "Tried to update the endpoint of a JsonMockup";
  }
  RedfishVariant GetRoot() override {
    return RedfishVariant(
        absl::make_unique<JsonMockupVariantImpl>(json_model_.get()));
  }
  RedfishVariant GetUri(absl::string_view uri) override {
    // We will implement GetUri as walking the URI from the root JSON node.
    RedfishVariant current_node(
        absl::make_unique<JsonMockupVariantImpl>(json_model_.get()));
    auto path_list = absl::StrSplit(uri, '/');
    for (const auto &p : path_list) {
      if (p.empty()) continue;
      // Try treating it as an object:
      if (auto obj = current_node.AsObject()) {
        auto next_node = obj->GetNode(std::string(p));
        if (next_node.AsObject() || next_node.AsIterable()) {
          current_node = std::move(next_node);
          continue;
        }
      }
      // Try treating it as an iterable:
      if (auto iter = current_node.AsIterable()) {
        size_t index;
        if (absl::SimpleAtoi(p, &index)) {
          auto next_node = iter->GetIndex(index);
          if (next_node.AsObject() || next_node.AsIterable()) {
            current_node = std::move(next_node);
            continue;
          }
        }
      }
      // We failed to make progress on the path
      return RedfishVariant();
    }
    return current_node;
  }
  RedfishVariant PostUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) override {
    return RedfishVariant();
  }
  RedfishVariant PostUri(
      absl::string_view uri,
      absl::string_view data) override {
    return RedfishVariant();
  }

 private:
  JsonUniquePtr json_model_;
};

}  // namespace

std::unique_ptr<RedfishInterface> NewJsonMockupInterface(
    absl::string_view raw_json) {
  return absl::make_unique<JsonMockupMockup>(raw_json);
}

}  // namespace libredfish
