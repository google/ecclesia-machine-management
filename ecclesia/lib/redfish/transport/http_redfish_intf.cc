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

#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/function_ref.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/http/codes.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/json_ptr.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/timing/query_timeout_manager.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/utils.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

//
// Struct tracks the origin of the HttpIntfVariantImpl.
// For instance for the object
// GetCached("/redfish/v1/Systems/system/Storage/1")["Drives"][0] the extended
// path will be:
//   {
//     .uri="/redfish/v1/Systems/system/Storage/1",
//     .properties={"Drives", 0}
//   }
struct RedfishExtendedPath {
  std::string uri;
  std::vector<std::variant<std::string, int>> properties;
  std::string GetFullPath() const {
    std::string raw_path = uri;
    for (const auto &item : properties) {
      std::visit(
          [&](auto &&typed_item) { raw_path += absl::StrCat("/", typed_item); },
          item);
    }
    return raw_path;
  }
};

// Helper function to convert a key-value span to a JSON object that can be
// used as a request body.

nlohmann::json ProcessValueVariant(const RedfishInterface::ListValue &val);
nlohmann::json ProcessValueVariant(const RedfishInterface::ObjectValue &val);
nlohmann::json ProcessValueVariant(int val) { return nlohmann::json(val); }
nlohmann::json ProcessValueVariant(const std::string &val) {
  return nlohmann::json(val);
}
nlohmann::json ProcessValueVariant(const char *val) {
  return nlohmann::json(val);
}
nlohmann::json ProcessValueVariant(bool val) { return nlohmann::json(val); }
nlohmann::json ProcessValueVariant(double val) { return nlohmann::json(val); }
nlohmann::json ProcessValueVariant(const RedfishInterface::ListValue &val) {
  nlohmann::json array(nlohmann::json::value_t::array);
  for (const auto &item : val.items) {
    std::visit([&](auto i) { array.push_back(ProcessValueVariant(i)); }, item);
  }
  return array;
}
nlohmann::json ProcessValueVariant(const RedfishInterface::ObjectValue &val) {
  nlohmann::json obj(nlohmann::json::value_t::object);
  for (const auto &i : val.items) {
    std::visit([&](auto v) { obj[i.first] = ProcessValueVariant(v); },
               i.second);
  }
  return obj;
}

nlohmann::json KvSpanToJson(
    absl::Span<const std::pair<std::string, RedfishInterface::ValueVariant>>
        kv_span) {
  nlohmann::json json(nlohmann::json::value_t::object);
  for (const auto &kv_pair : kv_span) {
    std::visit(
        [&](auto val) { json[kv_pair.first] = ProcessValueVariant(val); },
        kv_pair.second);
  }
  return json;
}

class HttpIntfVariantImpl : public RedfishVariant::ImplIntf {
 public:
  HttpIntfVariantImpl(RedfishInterface *intf, RedfishExtendedPath path,
                      ecclesia::RedfishTransport::Result result,
                      CacheState cache_state)
      : intf_(intf),
        path_(std::move(path)),
        result_(std::move(result)),
        cache_state_(cache_state) {}
  std::unique_ptr<RedfishObject> AsObject() const override;
  std::unique_ptr<RedfishIterable> AsIterable(RedfishVariant::IterableMode mode,
                                              GetParams params) const override;
  std::optional<RedfishTransport::bytes> AsRaw() const override;

  CacheState IsFresh() const override { return cache_state_; }

  bool GetValue(std::string *val) const override {
    if (!std::holds_alternative<nlohmann::json>(result_.body)) {
      return false;
    }
    const auto &json = std::get<nlohmann::json>(result_.body);
    if (!json.is_string()) return false;
    *val = json.get<std::string>();
    return true;
  }
  bool GetValue(int32_t *val) const override {
    if (!std::holds_alternative<nlohmann::json>(result_.body)) {
      return false;
    }
    const auto &json = std::get<nlohmann::json>(result_.body);
    if (json.is_number_integer()) {
      *val = json.get<int32_t>();
      return true;
    }
    if (json.is_number()) {
      double trans_tmp = std::round(json.get<double>());
      if (trans_tmp >
              static_cast<double>(std::numeric_limits<int32_t>::max()) ||
          trans_tmp <
              static_cast<double>(std::numeric_limits<int32_t>::min())) {
        return false;
      }
      *val = trans_tmp;
      return true;
    }
    return false;
  }
  bool GetValue(int64_t *val) const override {
    if (!std::holds_alternative<nlohmann::json>(result_.body)) {
      return false;
    }
    const auto &json = std::get<nlohmann::json>(result_.body);
    if (json.is_number_integer()) {
      *val = json.get<int64_t>();
      return true;
    }
    if (json.is_number()) {
      double trans_tmp = std::round(json.get<double>());
      if (trans_tmp >
              static_cast<double>(std::numeric_limits<int64_t>::max()) ||
          trans_tmp <
              static_cast<double>(std::numeric_limits<int64_t>::min())) {
        return false;
      }
      *val = trans_tmp;
      return true;
    }
    return false;
  }
  bool GetValue(double *val) const override {
    if (!std::holds_alternative<nlohmann::json>(result_.body)) {
      return false;
    }
    const auto &json = std::get<nlohmann::json>(result_.body);
    if (!json.is_number()) return false;
    *val = json.get<double>();
    return true;
  }
  bool GetValue(bool *val) const override {
    if (!std::holds_alternative<nlohmann::json>(result_.body)) {
      return false;
    }
    const auto &json = std::get<nlohmann::json>(result_.body);
    if (!json.is_boolean()) return false;
    *val = json.get<bool>();
    return true;
  }
  bool GetValue(absl::Time *val) const override {
    std::string dt_string;
    if (!GetValue(&dt_string)) {
      return false;
    }
    return absl::ParseTime("%Y-%m-%dT%H:%M:%S%Z", dt_string, val, nullptr);
  }
  std::string DebugString() const override {
    if (std::holds_alternative<nlohmann::json>(result_.body)) {
      return std::get<nlohmann::json>(result_.body).dump(1);
    }
    return RedfishTransportBytesToString(
        std::get<RedfishTransport::bytes>(result_.body));
  }

  void PrintDebugString() const override {
    LOG(INFO) << "Variant:\n" << DebugString();
  }

  std::optional<absl::Duration> GetTimeout() const override {
    if (timeout_mgr_.has_value()) {
      return (*timeout_mgr_)->GetRemainingTimeout();
    }
    return std::nullopt;
  }

  void SetTimeoutManager(ecclesia::QueryTimeoutManager *timeout_mgr) override {
    timeout_mgr_ = timeout_mgr;
  }

 private:
  RedfishInterface *intf_;
  RedfishExtendedPath path_;
  ecclesia::RedfishTransport::Result result_;
  CacheState cache_state_;
  std::optional<ecclesia::QueryTimeoutManager *> timeout_mgr_;
};

// Helper function for automatically fetching an @odata.id reference using
// GET. The goal of this function is to help "flatten" a Redfish service's
// entire Redfish tree to make it appear like a single JSON document.
// For example:
//   input json: { "@odata.id": "/redfish/v1/Chassis/1" }
//   result: returns GET on "/redfish/v1/Chassis/1"
// If the object is not a reference, returns the current json object as-is.
// reuse_code will be propagated to the returned RedfishVariant if no GET is
// performed.
RedfishVariant ResolveReference(
    int reuse_code, nlohmann::json json,
    const absl::flat_hash_map<std::string, std::string> &headers,
    RedfishInterface *intf, RedfishExtendedPath path, CacheState cache_state,
    GetParams params = {}) {
  auto get_uri = [intf](const RedfishExtendedPath &path, GetParams params) {
    return params.freshness == GetParams::Freshness::kRequired
               ? intf->UncachedGetUri(path.GetFullPath(), std::move(params))
               : intf->CachedGetUri(path.GetFullPath(), std::move(params));
  };
  if (absl::StatusOr<std::string> reference = GetObjectUri(json);
      reference.ok()) {
    path = RedfishExtendedPath{*reference, {}};
    if (json.size() == 1) {
      return get_uri(path, std::move(params));
    }
  }
  // Try to expand if expand is requested even if json doesn't have URI
  if (params.expand.has_value()) {
    if (RedfishVariant variant = get_uri(path, std::move(params));
        variant.status().ok()) {
      return variant;
    }
  }

  // Return the object as-is.
  return RedfishVariant(
      std::make_unique<HttpIntfVariantImpl>(intf, std::move(path),
                                            ecclesia::RedfishTransport::Result{
                                                .code = reuse_code,
                                                .body = std::move(json),
                                                .headers = headers,
                                            },
                                            cache_state),
      ecclesia::HttpResponseCodeFromInt(reuse_code), headers);
}

class HttpIntfObjectImpl : public RedfishObject {
 public:
  explicit HttpIntfObjectImpl(
      RedfishInterface *intf, RedfishExtendedPath path,
      ecclesia::RedfishTransport::Result result, CacheState cache_state,
      std::optional<ecclesia::QueryTimeoutManager *> timeout_mgr = std::nullopt)
      : intf_(intf),
        path_(std::move(path)),
        result_(std::move(result)),
        cache_state_(cache_state),
        timeout_mgr_(timeout_mgr) {}
  HttpIntfObjectImpl(const HttpIntfObjectImpl &) = delete;
  HttpIntfObjectImpl &operator=(const HttpIntfObjectImpl &) = delete;

  RedfishVariant operator[](const std::string &node_name) const override {
    return Get(node_name, GetParams{.timeout_manager = timeout_mgr_});
  }

  RedfishVariant Get(const std::string &node_name,
                     GetParams params) const override {
    if (!std::holds_alternative<nlohmann::json>(result_.body)) {
      return RedfishVariant(
          absl::InternalError("Result body is not holding JSON"));
    }
    const auto &json = std::get<nlohmann::json>(result_.body);
    // Update path with a new node name
    RedfishExtendedPath new_path = path_;
    new_path.properties.push_back(node_name);

    auto itr = json.find(node_name);
    if (itr == json.end()) {
      return RedfishVariant(std::make_unique<HttpIntfVariantImpl>(
                                intf_, std::move(new_path),
                                ecclesia::RedfishTransport::Result{
                                    .code = result_.code,
                                    .body = nlohmann::json::value_t::discarded,
                                    .headers = result_.headers,
                                },
                                cache_state_),
                            ecclesia::HttpResponseCodeFromInt(result_.code),
                            result_.headers);
    }
    // Reset expands if requested but not available
    if (params.expand.has_value() &&
        !params.expand.value()
             .ValidateRedfishSupport(intf_->SupportedFeatures())
             .ok()) {
      params.expand.reset();
    }
    // Reset top if requested but not available
    if (params.top.has_value() && !RedfishQueryParamTop::ValidateRedfishSupport(
                                       intf_->SupportedFeatures())
                                       .ok()) {
      params.top.reset();
    }
    // Reset filter if requested but not available
    if (params.filter.has_value() &&
        (!intf_->SupportedFeatures().has_value() ||
         !intf_->SupportedFeatures()->filter_enabled)) {
      params.filter.reset();
    }
    return ResolveReference(result_.code, itr.value(), result_.headers, intf_,
                            std::move(new_path), cache_state_,
                            std::move(params));
  }

  std::optional<std::string> GetUriString() const override {
    if (!std::holds_alternative<nlohmann::json>(result_.body)) {
      return std::nullopt;
    }
    const auto &json = std::get<nlohmann::json>(result_.body);
    auto itr = json.find(PropertyOdataId::Name);
    if (itr == json.end()) return std::nullopt;
    return std::string(itr.value());
  }

  nlohmann::json GetContentAsJson() const override {
    if (!std::holds_alternative<nlohmann::json>(result_.body)) {
      return nlohmann::json::value_t::discarded;
    }
    return std::get<nlohmann::json>(result_.body);
  }

  std::string DebugString() const override {
    if (std::holds_alternative<nlohmann::json>(result_.body)) {
      return std::get<nlohmann::json>(result_.body).dump(1);
    }
    return RedfishTransportBytesToString(
        std::get<RedfishTransport::bytes>(result_.body));
  }

  void PrintDebugString() const override {
    LOG(INFO) << "Object:\n" << DebugString();
  }

  absl::StatusOr<absl::Nonnull<std::unique_ptr<RedfishObject>>>
  EnsureFreshPayload(GetParams params) override {
    if (cache_state_ == CacheState::kIsFresh) {
      return std::make_unique<HttpIntfObjectImpl>(intf_, path_, result_,
                                                  cache_state_);
    }
    if (auto uri = GetUriString(); uri.has_value()) {
      RedfishVariant get_response = intf_->UncachedGetUri(*uri, params);
      if (get_response.status().ok()) {
        std::unique_ptr<RedfishObject> response = get_response.AsObject();
        if (response == nullptr) {
          return absl::InternalError(
              absl::StrFormat("Failed to get non-null payload from Redfish "
                              "response for URI: %s with status: %s",
                              uri.value(), get_response.status().message()));
        }
        return response;
      }
      return get_response.status();
    }
    return absl::NotFoundError("No URI to query");
  }

  void ForEachProperty(absl::FunctionRef<RedfishIterReturnValue(
                           absl::string_view, RedfishVariant value)>
                           itr_func) override {
    if (!std::holds_alternative<nlohmann::json>(result_.body)) {
      return;
    }
    const auto &json = std::get<nlohmann::json>(result_.body);
    for (const auto &items : json.items()) {
      RedfishExtendedPath path = path_;
      path.properties.push_back(items.key());
      if (itr_func(items.key(),
                   RedfishVariant(
                       std::make_unique<HttpIntfVariantImpl>(
                           intf_, std::move(path),
                           ecclesia::RedfishTransport::Result{
                               .code = result_.code,
                               .body = nlohmann::json(items.value()),
                               .headers = result_.headers,
                           },
                           cache_state_),
                       ecclesia::HttpResponseCodeFromInt(result_.code),
                       result_.headers)) == RedfishIterReturnValue::kStop) {
        break;
      }
    }
  }

 private:
  RedfishInterface *intf_;
  RedfishExtendedPath path_;
  ecclesia::RedfishTransport::Result result_;
  CacheState cache_state_;
  std::optional<ecclesia::QueryTimeoutManager *> timeout_mgr_;
};

// HttpIntfArrayIterableImpl implements the RedfishIterable interface with a
// RedfishTransport::Result containing a JSON array. The JSON array must be
// verified before constructing this class.
class HttpIntfArrayIterableImpl : public RedfishIterable {
 public:
  explicit HttpIntfArrayIterableImpl(
      RedfishInterface *intf, RedfishExtendedPath path,
      ecclesia::RedfishTransport::Result result, CacheState cache_state,
      RedfishVariant::IterableMode mode, GetParams params,
      std::optional<ecclesia::QueryTimeoutManager *> timeout_mgr = std::nullopt)
      : intf_(intf),
        path_(std::move(path)),
        result_(std::move(result)),
        cache_state_(cache_state),
        mode_(mode),
        params_(std::move(params)),
        timeout_mgr_(timeout_mgr) {}

  HttpIntfArrayIterableImpl(const HttpIntfArrayIterableImpl &) = delete;
  HttpIntfObjectImpl &operator=(const HttpIntfArrayIterableImpl &) = delete;

  size_t Size() override {
    return std::get<nlohmann::json>(result_.body).size();
  }

  bool Empty() override {
    return std::get<nlohmann::json>(result_.body).empty();
  }

  RedfishVariant operator[](int index) const override {
    const auto &json = std::get<nlohmann::json>(result_.body);
    if (index < 0 || index >= json.size()) {
      return RedfishVariant(absl::OutOfRangeError(
          absl::StrFormat("Index %d out of range for json array", index)));
    }
    auto retval = json[index];
    RedfishExtendedPath new_path = path_;
    new_path.properties.push_back(index);
    if (mode_ == RedfishVariant::IterableMode::kDisableAutoResolve) {
      // Return json object without resolving reference property.
      return RedfishVariant(std::make_unique<HttpIntfVariantImpl>(
                                intf_, std::move(new_path),
                                ecclesia::RedfishTransport::Result{
                                    .code = result_.code,
                                    .body = retval,
                                    .headers = result_.headers,
                                },
                                cache_state_),
                            ecclesia::HttpResponseCodeFromInt(result_.code),
                            result_.headers);
    }
    return ResolveReference(result_.code, json[index], result_.headers, intf_,
                            std::move(new_path), cache_state_,
                            GetParams{.freshness = params_.freshness,
                                      .timeout_manager = timeout_mgr_,
                                      .uri_prefix = params_.uri_prefix});
  }

 private:
  RedfishInterface *intf_;
  RedfishExtendedPath path_;
  ecclesia::RedfishTransport::Result result_;
  CacheState cache_state_;
  RedfishVariant::IterableMode mode_;
  GetParams params_;
  std::optional<ecclesia::QueryTimeoutManager *> timeout_mgr_;
};

// HttpIntfCollectionIterableImpl implements the RedfishIterable interface
// with a JSON object representing a Redfish Collection. The Collection object
// must be verified before constructing this class. Redfish Collection objects
// must have "Members" field which must be an array.
class HttpIntfCollectionIterableImpl : public RedfishIterable {
 public:
  explicit HttpIntfCollectionIterableImpl(
      RedfishInterface *intf, RedfishExtendedPath path,
      ecclesia::RedfishTransport::Result result, CacheState cache_state,
      RedfishVariant::IterableMode mode, GetParams params,
      std::optional<ecclesia::QueryTimeoutManager *> timeout_mgr = std::nullopt)
      : intf_(intf),
        path_(std::move(path)),
        result_(std::move(result)),
        cache_state_(cache_state),
        mode_(mode),
        params_(std::move(params)),
        timeout_mgr_(timeout_mgr) {}
  HttpIntfCollectionIterableImpl(const HttpIntfCollectionIterableImpl &) =
      delete;
  HttpIntfObjectImpl &operator=(const HttpIntfCollectionIterableImpl &) =
      delete;

  size_t Size() override {
    const auto &json = std::get<nlohmann::json>(result_.body);
    // Return size based on the number of elements in Members array.
    auto itr = json.find(PropertyMembers::Name);
    if (itr == json.end() || !itr.value().is_array()) return 0;
    return itr.value().size();
  }

  bool Empty() override {
    const auto &json = std::get<nlohmann::json>(result_.body);
    // Determine emptiness by checking if Members array is empty.
    auto itr = json.find(PropertyMembers::Name);
    if (itr == json.end() || !itr.value().is_array()) return true;
    return itr.value().empty();
  }

  RedfishVariant operator[](int index) const override {
    const auto &json = std::get<nlohmann::json>(result_.body);
    // Check the bounds based on the array in the Members property and access
    // the Members array directly.
    auto itr = json.find(PropertyMembers::Name);
    if (itr == json.end() || !itr.value().is_array() ||
        itr.value().size() <= index) {
      return RedfishVariant(absl::NotFoundError(
          absl::StrFormat("Index %d not found for json collection", index)));
    }
    RedfishExtendedPath new_path = path_;
    auto retval = itr.value()[index];
    new_path.properties.push_back(index);
    if (mode_ == RedfishVariant::IterableMode::kDisableAutoResolve) {
      // Return json object without resolving reference property.
      return RedfishVariant(std::make_unique<HttpIntfVariantImpl>(
                                intf_, std::move(new_path),
                                ecclesia::RedfishTransport::Result{
                                    .code = result_.code,
                                    .body = retval,
                                    .headers = result_.headers,
                                },
                                cache_state_),
                            ecclesia::HttpResponseCodeFromInt(result_.code),
                            result_.headers);
    }
    return ResolveReference(result_.code, itr.value()[index], result_.headers,
                            intf_, std::move(new_path), cache_state_,
                            GetParams{.freshness = params_.freshness,
                                      .timeout_manager = timeout_mgr_,
                                      .uri_prefix = params_.uri_prefix});
  }

 private:
  RedfishInterface *intf_;
  RedfishExtendedPath path_;
  ecclesia::RedfishTransport::Result result_;
  CacheState cache_state_;
  RedfishVariant::IterableMode mode_;
  GetParams params_;
  std::optional<ecclesia::QueryTimeoutManager *> timeout_mgr_;
  absl::Time query_start_time_;
};

std::unique_ptr<RedfishObject> HttpIntfVariantImpl::AsObject() const {
  if (!std::holds_alternative<nlohmann::json>(result_.body)) {
    return nullptr;
  }
  const auto &json = std::get<nlohmann::json>(result_.body);
  if (!json.is_object()) return nullptr;
  return std::make_unique<HttpIntfObjectImpl>(intf_, path_, result_,
                                              cache_state_);
}

std::unique_ptr<RedfishIterable> HttpIntfVariantImpl::AsIterable(
    RedfishVariant::IterableMode mode, GetParams params) const {
  if (!std::holds_alternative<nlohmann::json>(result_.body)) {
    return nullptr;
  }
  const auto &json = std::get<nlohmann::json>(result_.body);
  bool is_collection_iterable = json.is_object() &&
                                json.contains(PropertyMembers::Name) &&
                                json[PropertyMembers::Name].is_array();
  if (json.is_array()) {
    return std::make_unique<HttpIntfArrayIterableImpl>(
        intf_, path_, result_, cache_state_, mode, std::move(params),
        timeout_mgr_);
  }
  // Check if the object is a Redfish collection.
  if (is_collection_iterable) {
    return std::make_unique<HttpIntfCollectionIterableImpl>(
        intf_, path_, result_, cache_state_, mode, std::move(params),
        timeout_mgr_);
  }
  return nullptr;
}

std::optional<RedfishTransport::bytes> HttpIntfVariantImpl::AsRaw() const {
  if (!std::holds_alternative<RedfishTransport::bytes>(result_.body)) {
    return std::nullopt;
  }
  return std::get<RedfishTransport::bytes>(result_.body);
}

class HttpRedfishInterface : public RedfishInterface {
 public:
  HttpRedfishInterface(
      std::unique_ptr<ecclesia::RedfishTransport> transport,
      std::unique_ptr<ecclesia::RedfishCachedGetterInterface> cache,
      RedfishInterface::TrustedEndpoint trusted)
      : transport_(std::move(transport)),
        trusted_(trusted),
        cache_(std::move(cache)),
        cache_factory_(nullptr) {}

  HttpRedfishInterface(std::unique_ptr<ecclesia::RedfishTransport> transport,
                       RedfishTransportCacheFactory cache_factory,
                       RedfishInterface::TrustedEndpoint trusted)
      : transport_(std::move(transport)),
        trusted_(trusted),
        cache_(cache_factory(transport_.get())),
        cache_factory_(std::move(cache_factory)) {}

  bool IsTrusted() const override {
    absl::ReaderMutexLock mu(&transport_mutex_);
    return trusted_ == kTrusted;
  }

  void UpdateTransport(std::unique_ptr<RedfishTransport> new_transport,
                       TrustedEndpoint trusted) {
    absl::WriterMutexLock mu(&transport_mutex_);
    if (cache_factory_ == nullptr) {
      LOG(FATAL) << "Tried to update the endpoint without CacheFactory set";
    }
    trusted_ = trusted;
    transport_ = std::move(new_transport);
    cache_ = cache_factory_(transport_.get());
  }

  RedfishVariant GetRoot(GetParams params,
                         ServiceRootUri service_root) override {
    RedfishVariant root = [&]() {
      return CachedGetUri(ServiceRootToUri(service_root), std::move(params));
    }();
    // parse supported features if not parsed yet
    PopulateSupportedFeatures(root);
    return root;
  }

  RedfishVariant GetRoot(ecclesia::GetParams params,
                         absl::string_view service_root) override {
    RedfishVariant root = [&]() {
      return CachedGetUri(service_root, std::move(params));
    }();
    // parse supported features if not parsed yet
    PopulateSupportedFeatures(root);
    return root;
  }

  // GetUri fetches the given URI and resolves any JSON pointers. Note that
  // this method must not resolve any references (i.e. JSON object containing
  // only "@odata.id") to avoid infinite recursion in case a bad Redfish
  // service has a loop in its OData references.
  RedfishVariant CachedGetUri(absl::string_view uri,
                              GetParams params) override {
    absl::ReaderMutexLock mu(&transport_mutex_);
    if (params.timeout_manager.has_value() &&
        *params.timeout_manager != nullptr) {
      if ((*params.timeout_manager)->ProbeTimeout().code() ==
          absl::StatusCode::kDeadlineExceeded) {
        return RedfishVariant(absl::DeadlineExceededError(
            absl::StrCat("Deadline exceeded while getting uri:", uri)));
      }
    }
    return GetUriHelper(
        uri, params,
        cache_->CachedGet(GetUriWithQueryParameters(uri, params),
                          params.timeout_manager));
  }

  RedfishVariant UncachedGetUri(absl::string_view uri,
                                GetParams params) override {
    absl::ReaderMutexLock mu(&transport_mutex_);
    if (params.timeout_manager.has_value() &&
        *params.timeout_manager != nullptr) {
      if ((*params.timeout_manager)->ProbeTimeout().code() ==
          absl::StatusCode::kDeadlineExceeded) {
        return RedfishVariant(absl::DeadlineExceededError(
            absl::StrCat("Deadline exceeded while getting uri:", uri)));
      }
    }
    return GetUriHelper(
        uri, params,
        cache_->UncachedGet(GetUriWithQueryParameters(uri, params),
                            params.relevance, params.timeout_manager));
  }

  RedfishVariant PostUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) override {
    return PostUri(uri, KvSpanToJson(kv_span).dump());
  }

  RedfishVariant CachedPostUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span,
      absl::Duration duration) override {
    absl::ReaderMutexLock mu(&transport_mutex_);
    auto post_result =
        cache_->CachedPost(uri, KvSpanToJson(kv_span).dump(), duration);
    if (!post_result.result.ok())
      return RedfishVariant(post_result.result.status());
    int code = post_result.result->code;
    absl::flat_hash_map<std::string, std::string> headers =
        post_result.result->headers;
    return RedfishVariant(std::make_unique<HttpIntfVariantImpl>(
                              this, RedfishExtendedPath{std::string(uri)},
                              *std::move(post_result.result),
                              post_result.is_fresh ? CacheState::kIsFresh
                                                   : CacheState::kIsCached),
                          ecclesia::HttpResponseCodeFromInt(code), headers);
  }

  RedfishVariant PostUri(absl::string_view uri,
                         absl::string_view data) override {
    absl::ReaderMutexLock mu(&transport_mutex_);
    absl::StatusOr<ecclesia::RedfishTransport::Result> result =
        transport_->Post(uri, data);
    if (!result.ok()) return RedfishVariant(result.status());
    int code = result->code;
    absl::flat_hash_map<std::string, std::string> headers = result->headers;
    return RedfishVariant(std::make_unique<HttpIntfVariantImpl>(
                              this, RedfishExtendedPath{std::string(uri)},
                              std::move(*result), CacheState::kIsFresh),
                          ecclesia::HttpResponseCodeFromInt(code), headers);
  }

  RedfishVariant DeleteUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) override {
    return DeleteUri(uri, KvSpanToJson(kv_span).dump());
  }

  RedfishVariant DeleteUri(absl::string_view uri,
                           absl::string_view data) override {
    absl::ReaderMutexLock mu(&transport_mutex_);
    absl::StatusOr<ecclesia::RedfishTransport::Result> result =
        transport_->Delete(uri, data);
    if (!result.ok()) return RedfishVariant(result.status());
    int code = result->code;
    absl::flat_hash_map<std::string, std::string> headers = result->headers;
    return RedfishVariant(std::make_unique<HttpIntfVariantImpl>(
                              this, RedfishExtendedPath{std::string(uri)},
                              std::move(*result), CacheState::kIsFresh),
                          ecclesia::HttpResponseCodeFromInt(code), headers);
  }

  RedfishVariant PatchUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) override {
    return PatchUri(uri, KvSpanToJson(kv_span).dump());
  }

  RedfishVariant PatchUri(absl::string_view uri,
                          absl::string_view data) override {
    absl::ReaderMutexLock mu(&transport_mutex_);
    absl::StatusOr<ecclesia::RedfishTransport::Result> result =
        transport_->Patch(uri, data);
    if (!result.ok()) return RedfishVariant(result.status());
    int code = result->code;
    absl::flat_hash_map<std::string, std::string> headers = result->headers;
    return RedfishVariant(std::make_unique<HttpIntfVariantImpl>(
                              this, RedfishExtendedPath{std::string(uri)},
                              std::move(*result), CacheState::kIsFresh),
                          ecclesia::HttpResponseCodeFromInt(code), headers);
  }

  std::optional<RedfishSupportedFeatures> SupportedFeatures() const override {
    absl::MutexLock lock(&supported_features_mutex_);
    return supported_features_;
  }

  void RemoveExpandSupport() {
    absl::MutexLock lock(&supported_features_mutex_);
    remove_expand_support_ = true;
  }

  absl::StatusOr<std::unique_ptr<RedfishEventStream>> Subscribe(
      absl::string_view data,
      std::function<void(const RedfishVariant &event)> &&on_event,
      std::function<void(const absl::Status &status)> &&on_stop) override {
    auto new_callback = [on_event = std::move(on_event),
                         this](const RedfishTransport::Result &result) {
      int code = result.code;
      absl::flat_hash_map<std::string, std::string> headers = result.headers;
      RedfishVariant variant(
          std::make_unique<HttpIntfVariantImpl>(this, RedfishExtendedPath{},
                                                result, CacheState::kIsFresh),
          ecclesia::HttpResponseCodeFromInt(code), headers);
      on_event(variant);
    };
    absl::ReaderMutexLock mu(&transport_mutex_);
    return transport_->Subscribe(data, std::move(new_callback),
                                 std::move(on_stop));
  }

 private:
  // Helper function to resolve JSON pointers after doing a GET.
  RedfishVariant GetUriHelper(
      absl::string_view uri, const GetParams &params,
      ecclesia::RedfishCachedGetterInterface::OperationResult get_res) {
    if (!get_res.result.ok()) return RedfishVariant(get_res.result.status());

    // Handle JSON pointers if needed. Pointers follow a '#' character at the
    // end of a path.
    std::vector<absl::string_view> json_ptrs =
        absl::StrSplit(uri, absl::MaxSplits('#', 1));
    if (json_ptrs.size() < 2) {
      // No pointers, return the payload as-is.
      int code = get_res.result->code;
      absl::flat_hash_map<std::string, std::string> headers =
          get_res.result->headers;
      return RedfishVariant(
          std::make_unique<HttpIntfVariantImpl>(
              this, RedfishExtendedPath{.uri = std::string(uri)},
              *std::move(get_res.result),
              get_res.is_fresh ? CacheState::kIsFresh : CacheState::kIsCached),
          ecclesia::HttpResponseCodeFromInt(code), headers);
    }
    if (!std::holds_alternative<nlohmann::json>(get_res.result->body)) {
      return RedfishVariant(
          absl::InternalError("Result body is not holding JSON"));
    }
    nlohmann::json resolved_ptr = ecclesia::HandleJsonPtr(
        std::get<nlohmann::json>(get_res.result->body), json_ptrs[1]);
    get_res.result->body = std::move(resolved_ptr);
    int code = get_res.result->code;
    absl::flat_hash_map<std::string, std::string> headers =
        get_res.result->headers;
    return RedfishVariant(
        std::make_unique<HttpIntfVariantImpl>(
            this, RedfishExtendedPath{.uri = std::string(uri)},
            *std::move(get_res.result),
            get_res.is_fresh ? CacheState::kIsFresh : CacheState::kIsCached),
        ecclesia::HttpResponseCodeFromInt(code), headers);
  }

  void PopulateSupportedFeatures(const RedfishVariant &root) {
    absl::MutexLock lock(&supported_features_mutex_);
    if (supported_features_.has_value() || !root.status().ok()) {
      return;
    }
    auto root_object = root.AsObject();
    if (root_object == nullptr) {
      return;
    }
    auto expand_features_json =
        (*root_object)[kProtocolFeaturesSupported][kExpandQuery].AsObject();
    RedfishSupportedFeatures features;
    if (expand_features_json != nullptr) {
      if (auto val = expand_features_json->GetNodeValue<ExpandQueryExpandAll>();
          val.has_value()) {
        features.expand.expand_all = *val;
      }
      if (auto val = expand_features_json->GetNodeValue<ExpandQueryLevels>();
          val.has_value()) {
        features.expand.levels = *val;
      }
      if (auto val = expand_features_json->GetNodeValue<ExpandQuerykLinks>();
          val.has_value()) {
        features.expand.links = *val;
      }
      if (auto val =
              expand_features_json->GetNodeValue<ExpandQuerykMaxLevels>();
          val.has_value()) {
        features.expand.max_levels = *val;
      }
      if (auto val = expand_features_json->GetNodeValue<ExpandQuerykNoLinks>();
          val.has_value()) {
        features.expand.no_links = *val;
      }
    }
    auto features_json = (*root_object)[kProtocolFeaturesSupported].AsObject();
    if (features_json != nullptr) {
      if (auto val = features_json->GetNodeValue<TopSkipQuery>();
          val.has_value()) {
        features.top_skip.enable = *val;
      }
    }
    // Process $filter features if applicable
    bool filter_enabled = false;
    if ((*root_object)[kProtocolFeaturesSupported][kFilterQuery].GetValue(
            &filter_enabled);
        filter_enabled) {
      features.filter_enabled = filter_enabled;
    }

    supported_features_ = features;
    if (remove_expand_support_) {
      supported_features_->expand = {};
    }
  }

  mutable absl::Mutex transport_mutex_;
  std::unique_ptr<ecclesia::RedfishTransport> transport_
      ABSL_GUARDED_BY(transport_mutex_);
  RedfishInterface::TrustedEndpoint trusted_ ABSL_GUARDED_BY(transport_mutex_);
  std::unique_ptr<ecclesia::RedfishCachedGetterInterface> cache_
      ABSL_GUARDED_BY(transport_mutex_);
  RedfishTransportCacheFactory cache_factory_ ABSL_GUARDED_BY(transport_mutex_);

  mutable absl::Mutex supported_features_mutex_;

  std::optional<RedfishSupportedFeatures> supported_features_
      ABSL_GUARDED_BY(supported_features_mutex_);
  bool remove_expand_support_ ABSL_GUARDED_BY(supported_features_mutex_) =
      false;
};

}  // namespace

std::unique_ptr<RedfishInterface> NewHttpInterface(
    std::unique_ptr<ecclesia::RedfishTransport> transport,
    std::unique_ptr<ecclesia::RedfishCachedGetterInterface> cache,
    RedfishInterface::TrustedEndpoint trusted) {
  return std::make_unique<HttpRedfishInterface>(std::move(transport),
                                                std::move(cache), trusted);
}

std::unique_ptr<RedfishInterface> NewHttpInterface(
    std::unique_ptr<ecclesia::RedfishTransport> transport,
    RedfishTransportCacheFactory cache_factory,
    RedfishInterface::TrustedEndpoint trusted) {
  return std::make_unique<HttpRedfishInterface>(
      std::move(transport), std::move(cache_factory), trusted);
}

std::unique_ptr<RedfishInterface> NewHttpInterfaceWithoutExpand(
    std::unique_ptr<ecclesia::RedfishTransport> transport,
    RedfishTransportCacheFactory cache_factory,
    RedfishInterface::TrustedEndpoint trusted) {
  auto http_intf = std::make_unique<HttpRedfishInterface>(
      std::move(transport), std::move(cache_factory), trusted);
  http_intf->RemoveExpandSupport();
  return http_intf;
}

}  // namespace ecclesia
