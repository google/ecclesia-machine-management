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

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/http/codes.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/json_ptr.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/time/clock.h"
#include "single_include/nlohmann/json.hpp"

// TODO (b/223426511) change to true when expand support is implemented on BMC
// and then removed afer rollout is done
ABSL_FLAG(bool, ecclesia_enable_expanded_iterations, false,
          "Enable RedFish expands for mmanager iterations.");

namespace ecclesia {
namespace {

// Represents whether the data in a RedfishVariant originated from a backend
// or from a cached copy.
enum CacheState {
  // The data originated from a Redfish backend.
  kIsFresh = 0,
  // The data originated from a cached copy.
  kIsCached = 1
};

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
  std::vector<std::variant<std::string, size_t>> properties;
};

// Helper function to convert a key-value span to a JSON object that can be
// used as a request body.

nlohmann::json ProcessValueVariant(RedfishInterface::ListValue val);
nlohmann::json ProcessValueVariant(RedfishInterface::ObjectValue val);
nlohmann::json ProcessValueVariant(int val) { return nlohmann::json(val); }
nlohmann::json ProcessValueVariant(std::string val) {
  return nlohmann::json(val);
}
nlohmann::json ProcessValueVariant(const char *val) {
  return nlohmann::json(val);
}
nlohmann::json ProcessValueVariant(bool val) { return nlohmann::json(val); }
nlohmann::json ProcessValueVariant(double val) { return nlohmann::json(val); }
nlohmann::json ProcessValueVariant(RedfishInterface::ListValue val) {
  nlohmann::json array(nlohmann::json::value_t::array);
  for (const auto &item : val.items) {
    std::visit([&](auto i) { array.push_back(ProcessValueVariant(i)); }, item);
  }
  return array;
}
nlohmann::json ProcessValueVariant(RedfishInterface::ObjectValue val) {
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
  std::unique_ptr<RedfishIterable> AsIterable(
      RedfishVariant::IterableMode mode) const override;

  bool GetValue(std::string *val) const override {
    if (!result_.body.is_string()) return false;
    *val = result_.body.get<std::string>();
    return true;
  }
  bool GetValue(int32_t *val) const override {
    if (result_.body.is_number_integer()) {
      *val = result_.body.get<int32_t>();
      return true;
    } else if (result_.body.is_number()) {
      double trans_tmp = std::round(result_.body.get<double>());
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
    if (result_.body.is_number_integer()) {
      *val = result_.body.get<int64_t>();
      return true;
    } else if (result_.body.is_number()) {
      double trans_tmp = std::round(result_.body.get<double>());
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
    if (!result_.body.is_number()) return false;
    *val = result_.body.get<double>();
    return true;
  }
  bool GetValue(bool *val) const override {
    if (!result_.body.is_boolean()) return false;
    *val = result_.body.get<bool>();
    return true;
  }
  bool GetValue(absl::Time *val) const override {
    std::string dt_string;
    if (!GetValue(&dt_string)) {
      return false;
    }
    return absl::ParseTime("%Y-%m-%dT%H:%M:%S%Z", dt_string, val, nullptr);
  }
  std::string DebugString() const override { return result_.body.dump(); }

 private:
  RedfishInterface *intf_;
  RedfishExtendedPath path_;
  ecclesia::RedfishTransport::Result result_;
  CacheState cache_state_;
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
RedfishVariant ResolveReference(int reuse_code, nlohmann::json json,
                                RedfishInterface *intf,
                                RedfishExtendedPath path,
                                CacheState cache_state,
                                absl::string_view source,
                                GetParams params = {}) {
  std::string reference;
  if (json.is_object()) {
    if (auto odata = json.find(PropertyOdataId::Name);
        // Object can be re-read by URI. Store it.
        odata != json.end() && odata.value().is_string()) {
      reference = odata.value().get<std::string>();
      path = RedfishExtendedPath{reference, {}};
    }
  }
  if (json.size() == 1 && !reference.empty()) {
    // It is a reference, call GET on it
    return intf->CachedGetUri(reference, params);
  }
  // Return the object as-is.
  return RedfishVariant(
      std::make_unique<HttpIntfVariantImpl>(intf, std::move(path),
                                            ecclesia::RedfishTransport::Result{
                                                .code = reuse_code,
                                                .body = std::move(json),
                                            },
                                            cache_state),
      ecclesia::HttpResponseCodeFromInt(reuse_code));
}

class HttpIntfObjectImpl : public RedfishObject {
 public:
  explicit HttpIntfObjectImpl(RedfishInterface *intf, RedfishExtendedPath path,
                              ecclesia::RedfishTransport::Result result,
                              CacheState cache_state)
      : intf_(intf),
        path_(std::move(path)),
        result_(std::move(result)),
        cache_state_(cache_state) {}
  HttpIntfObjectImpl(const HttpIntfObjectImpl &) = delete;
  HttpIntfObjectImpl &operator=(const HttpIntfObjectImpl &) = delete;

  RedfishVariant operator[](const std::string &node_name) const override {
    RedfishExtendedPath new_path = path_;
    new_path.properties.push_back(node_name);
    auto itr = result_.body.find(node_name);
    if (itr == result_.body.end()) {
      return RedfishVariant(std::make_unique<HttpIntfVariantImpl>(
                                intf_, std::move(new_path),
                                ecclesia::RedfishTransport::Result{
                                    .code = result_.code,
                                    .body = nlohmann::json::value_t::discarded,
                                },
                                cache_state_),
                            ecclesia::HttpResponseCodeFromInt(result_.code));
    }
    return ResolveReference(result_.code, itr.value(), intf_,
                            std::move(new_path), cache_state_,
                            "HttpIntfObjectImpl");
  }
  std::optional<std::string> GetUriString() const override {
    auto itr = result_.body.find(PropertyOdataId::Name);
    if (itr == result_.body.end()) return std::nullopt;
    return std::string(itr.value());
  }
  std::string DebugString() override { return result_.body.dump(); }

  std::unique_ptr<RedfishObject> EnsureFreshPayload(GetParams params) {
    if (cache_state_ == kIsFresh) {
      return std::make_unique<HttpIntfObjectImpl>(intf_, path_, result_,
                                                  cache_state_);
    }
    if (auto uri = GetUriString(); uri.has_value()) {
      return intf_->UncachedGetUri(*uri).AsObject();
    }
    return nullptr;
  }

  void ForEachProperty(absl::FunctionRef<RedfishIterReturnValue(
                           absl::string_view, RedfishVariant value)>
                           itr_func) {
    for (const auto &items : result_.body.items()) {
      RedfishExtendedPath path = path_;
      path.properties.push_back(items.key());
      if (itr_func(items.key(),
                   RedfishVariant(
                       std::make_unique<HttpIntfVariantImpl>(
                           intf_, std::move(path),
                           ecclesia::RedfishTransport::Result{
                               .code = result_.code,
                               .body = items.value(),
                           },
                           cache_state_),
                       ecclesia::HttpResponseCodeFromInt(result_.code))) ==
          RedfishIterReturnValue::kStop) {
        break;
      }
    }
  }

 private:
  RedfishInterface *intf_;
  RedfishExtendedPath path_;
  ecclesia::RedfishTransport::Result result_;
  CacheState cache_state_;
};

// HttpIntfArrayIterableImpl implements the RedfishIterable interface with a
// RedfishTransport::Result containing a JSON array. The JSON array must be
// verified before constructing this class.
class HttpIntfArrayIterableImpl : public RedfishIterable {
 public:
  explicit HttpIntfArrayIterableImpl(RedfishInterface *intf,
                                     RedfishExtendedPath path,
                                     ecclesia::RedfishTransport::Result result,
                                     CacheState cache_state)
      : intf_(intf),
        path_(std::move(path)),
        result_(std::move(result)),
        cache_state_(cache_state) {}

  HttpIntfArrayIterableImpl(const HttpIntfArrayIterableImpl &) = delete;
  HttpIntfObjectImpl &operator=(const HttpIntfArrayIterableImpl &) = delete;

  size_t Size() override { return result_.body.size(); }

  bool Empty() override { return result_.body.empty(); }

  RedfishVariant operator[](int index) const override {
    if (index < 0 || index >= result_.body.size()) {
      return RedfishVariant(absl::OutOfRangeError(
          absl::StrFormat("Index %d out of range for json array", index)));
    }
    auto retval = result_.body[index];
    RedfishExtendedPath new_path = path_;
    new_path.properties.push_back(index);
    return ResolveReference(result_.code, result_.body[index], intf_,
                            std::move(new_path), cache_state_,
                            "HttpIntfArrayIterableImpl");
  }

 private:
  RedfishInterface *intf_;
  RedfishExtendedPath path_;
  ecclesia::RedfishTransport::Result result_;
  CacheState cache_state_;
};

// HttpIntfCollectionIterableImpl implements the RedfishIterable interface with
// a JSON object representing a Redfish Collection. The Collection object must
// be verified before constructing this class. Redfish Collection objects must
// have "Members@odata.count" and "Members" fields.
class HttpIntfCollectionIterableImpl : public RedfishIterable {
 public:
  explicit HttpIntfCollectionIterableImpl(
      RedfishInterface *intf, RedfishExtendedPath path,
      ecclesia::RedfishTransport::Result result, CacheState cache_state)
      : intf_(intf),
        path_(std::move(path)),
        result_(std::move(result)),
        cache_state_(cache_state) {}
  HttpIntfCollectionIterableImpl(const HttpIntfCollectionIterableImpl &) =
      delete;
  HttpIntfObjectImpl &operator=(const HttpIntfCollectionIterableImpl &) =
      delete;

  size_t Size() override {
    // Return size based on Members@odata.count.
    auto itr = result_.body.find(PropertyMembersCount::Name);
    if (itr == result_.body.end() || !itr.value().is_number()) return 0;
    return itr.value().get<size_t>();
  }

  bool Empty() override {
    // Determine emptiness by checking Members@odata.count.
    auto itr = result_.body.find(PropertyMembersCount::Name);
    if (itr == result_.body.end() || !itr.value().is_number()) return true;
    return itr.value().get<size_t>() == 0;
  }

  RedfishVariant operator[](int index) const override {
    // Check the bounds based on the array in the Members property and access
    // the Members array directly.
    auto itr = result_.body.find(PropertyMembers::Name);
    if (itr == result_.body.end() || !itr.value().is_array() ||
        itr.value().size() <= index) {
      return RedfishVariant(absl::NotFoundError(
          absl::StrFormat("Index %d not found for json collection", index)));
    }
    RedfishExtendedPath new_path = path_;
    new_path.properties.push_back(index);
    return ResolveReference(result_.code, itr.value()[index], intf_,
                            std::move(new_path), cache_state_,
                            "HttpIntfCollectionIterableImpl");
  }

 private:
  RedfishInterface *intf_;
  RedfishExtendedPath path_;
  ecclesia::RedfishTransport::Result result_;
  CacheState cache_state_;
};

std::unique_ptr<RedfishObject> HttpIntfVariantImpl::AsObject() const {
  if (!result_.body.is_object()) return nullptr;
  return std::make_unique<HttpIntfObjectImpl>(intf_, path_, result_,
                                              cache_state_);
}

std::unique_ptr<RedfishIterable> HttpIntfVariantImpl::AsIterable(
    RedfishVariant::IterableMode mode) const {
  // We do expand the iterable in two cases:
  // 1. This is an object that contains "Members" field, aka collection_iterable
  // 2. This is a sub object requested using obj["Members"] syntax
  bool is_collection_iterable =
      result_.body.is_object() &&
      result_.body.contains(PropertyMembers::Name) &&
      result_.body.contains(PropertyMembersCount::Name);
  bool is_members_array =
      result_.body.is_array() && path_.properties.size() == 1 &&
      std::holds_alternative<std::string>(path_.properties[0]) &&
      std::get<std::string>(path_.properties[0]) == PropertyMembers::Name;
  if (mode == RedfishVariant::IterableMode::kAllowExpand &&
      absl::GetFlag(FLAGS_ecclesia_enable_expanded_iterations) &&
      (is_members_array || is_collection_iterable)) {
    RedfishVariant expanded_variant = intf_->CachedGetUri(
        path_.uri, GetParams{.query_params = {RedfishQueryParamExpand(
                                 {.type = RedfishQueryParamExpand::kAll})}});
    // Ignore the result if there is a failure or result code changed
    if (expanded_variant.status().ok() &&
        expanded_variant.httpcode() == result_.code) {
      if (is_members_array) {
        expanded_variant = expanded_variant[PropertyMembers::Name];
      }
      return expanded_variant.AsIterable(
          RedfishVariant::IterableMode::kDisableExpand);
    }
  }
  if (result_.body.is_array()) {
    return std::make_unique<HttpIntfArrayIterableImpl>(intf_, path_, result_,
                                                       cache_state_);
  }
  // Check if the object is a Redfish collection.
  if (is_collection_iterable) {
    return std::make_unique<HttpIntfCollectionIterableImpl>(
        intf_, path_, result_, cache_state_);
  }
  return nullptr;
}

class HttpRedfishInterface : public RedfishInterface {
 public:
  HttpRedfishInterface(
      std::unique_ptr<ecclesia::RedfishTransport> transport,
      std::unique_ptr<ecclesia::RedfishCachedGetterInterface> cache,
      RedfishInterface::TrustedEndpoint trusted,
      ServiceRootUri service_root = ServiceRootUri::kRedfish)
      : transport_(std::move(transport)),
        trusted_(trusted),
        cache_(std::move(cache)),
        cache_factory_(nullptr),
        service_root_(service_root) {}

  HttpRedfishInterface(std::unique_ptr<ecclesia::RedfishTransport> transport,
                       RedfishTransportCacheFactory cache_factory,
                       RedfishInterface::TrustedEndpoint trusted,
                       ServiceRootUri service_root = ServiceRootUri::kRedfish)
      : transport_(std::move(transport)),
        trusted_(trusted),
        cache_(cache_factory(transport_.get())),
        cache_factory_(std::move(cache_factory)),
        service_root_(service_root) {}

  bool IsTrusted() const override {
    absl::ReaderMutexLock mu(&transport_mutex_);
    return trusted_ == kTrusted;
  }

  void UpdateTransport(std::unique_ptr<RedfishTransport> new_transport,
                       TrustedEndpoint trusted) {
    absl::WriterMutexLock mu(&transport_mutex_);
    if (cache_factory_ == nullptr) {
      ecclesia::FatalLog()
          << "Tried to update the endpoint without CacheFactory set";
    }
    trusted_ = trusted;
    transport_ = std::move(new_transport);
    cache_ = cache_factory_(transport_.get());
  }

  RedfishVariant GetRoot(GetParams params) override {
    if (service_root_ == ServiceRootUri::kGoogle) {
      return CachedGetUri(kGoogleServiceRoot, std::move(params));
    }

    return CachedGetUri(kServiceRoot, std::move(params));
  }

  // GetUri fetches the given URI and resolves any JSON pointers. Note that
  // this method must not resolve any references (i.e. JSON object containing
  // only "@odata.id") to avoid infinite recursion in case a bad Redfish
  // service has a loop in its OData references.
  RedfishVariant CachedGetUri(absl::string_view uri,
                              GetParams params) override {
    absl::ReaderMutexLock mu(&transport_mutex_);
    std::string full_redfish_path = GetUriWithQueryParameters(uri, params);
    RedfishCachedGetterInterface::GetResult result =
        cache_->CachedGet(full_redfish_path);
    return GetUriHelper(uri, std::move(params), std::move(result));
  }

  RedfishVariant UncachedGetUri(absl::string_view uri,
                                GetParams params) override {
    absl::ReaderMutexLock mu(&transport_mutex_);
    auto get_result = cache_->UncachedGet(uri);
    return GetUriHelper(uri, std::move(params), std::move(get_result));
  }

  RedfishVariant PostUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) override {
    return PostUri(uri, KvSpanToJson(kv_span).dump());
  }

  RedfishVariant PostUri(absl::string_view uri,
                         absl::string_view data) override {
    absl::ReaderMutexLock mu(&transport_mutex_);
    absl::StatusOr<ecclesia::RedfishTransport::Result> result =
        transport_->Post(uri, data);
    if (!result.ok()) return RedfishVariant(result.status());
    int code = result->code;
    return RedfishVariant(std::make_unique<HttpIntfVariantImpl>(
                              this, RedfishExtendedPath{std::string(uri)},
                              std::move(*result), kIsFresh),
                          ecclesia::HttpResponseCodeFromInt(code));
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
    return RedfishVariant(std::make_unique<HttpIntfVariantImpl>(
                              this, RedfishExtendedPath{std::string(uri)},
                              std::move(*result), kIsFresh),
                          ecclesia::HttpResponseCodeFromInt(code));
  }

 private:
  // extends uri with query parameters if needed
  std::string GetUriWithQueryParameters(absl::string_view uri,
                                        const GetParams &params) {
    if (params.query_params.empty()) {
      return std::string(uri);
    }
    std::string query_args = absl::StrJoin(
        params.query_params.begin(), params.query_params.end(), "&",
        [](std::string *output,
           const std::variant<ecclesia::RedfishQueryParamExpand> &query_arg) {
          std::visit([output](auto arg) { output->append(arg.ToString()); },
                     query_arg);
        });
    return absl::StrCat(uri, "?", query_args);
  }

  // Helper function to resolve JSON pointers after doing a GET.
  RedfishVariant GetUriHelper(
      absl::string_view uri, const GetParams &params,
      ecclesia::RedfishCachedGetterInterface::GetResult get_res) {
    if (!get_res.result.ok()) return RedfishVariant(get_res.result.status());

    // Handle JSON pointers if needed. Pointers follow a '#' character at the
    // end of a path.
    std::vector<absl::string_view> json_ptrs =
        absl::StrSplit(uri, absl::MaxSplits('#', 1));
    if (json_ptrs.size() < 2) {
      // No pointers, return the payload as-is.
      int code = get_res.result->code;
      return RedfishVariant(
          std::make_unique<HttpIntfVariantImpl>(
              this, RedfishExtendedPath{.uri = std::string(uri)},
              *std::move(get_res.result),
              get_res.is_fresh ? kIsFresh : kIsCached),
          ecclesia::HttpResponseCodeFromInt(code));
    }
    nlohmann::json resolved_ptr =
        ecclesia::HandleJsonPtr(get_res.result->body, json_ptrs[1]);
    get_res.result->body = std::move(resolved_ptr);
    int code = get_res.result->code;
    return RedfishVariant(
        std::make_unique<HttpIntfVariantImpl>(
            this, RedfishExtendedPath{.uri = std::string(uri)},
            *std::move(get_res.result),
            get_res.is_fresh ? kIsFresh : kIsCached),
        ecclesia::HttpResponseCodeFromInt(code));
  }

  mutable absl::Mutex transport_mutex_;
  std::unique_ptr<ecclesia::RedfishTransport> transport_
      ABSL_GUARDED_BY(transport_mutex_);
  RedfishInterface::TrustedEndpoint trusted_ ABSL_GUARDED_BY(transport_mutex_);
  std::unique_ptr<ecclesia::RedfishCachedGetterInterface> cache_
      ABSL_GUARDED_BY(transport_mutex_);
  RedfishTransportCacheFactory cache_factory_ ABSL_GUARDED_BY(transport_mutex_);
  const ServiceRootUri service_root_;
};

}  // namespace

std::unique_ptr<RedfishInterface> NewHttpInterface(
    std::unique_ptr<ecclesia::RedfishTransport> transport,
    std::unique_ptr<ecclesia::RedfishCachedGetterInterface> cache,
    RedfishInterface::TrustedEndpoint trusted, ServiceRootUri service_root) {
  return std::make_unique<HttpRedfishInterface>(
      std::move(transport), std::move(cache), trusted, service_root);
}

std::unique_ptr<RedfishInterface> NewHttpInterface(
    std::unique_ptr<ecclesia::RedfishTransport> transport,
    RedfishTransportCacheFactory cache_factory,
    RedfishInterface::TrustedEndpoint trusted, ServiceRootUri service_root) {
  return std::make_unique<HttpRedfishInterface>(
      std::move(transport), std::move(cache_factory), trusted, service_root);
}

}  // namespace ecclesia
