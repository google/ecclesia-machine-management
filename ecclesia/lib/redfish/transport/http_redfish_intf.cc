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
#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/http/codes.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/json_ptr.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "single_include/nlohmann/json.hpp"

namespace libredfish {
namespace {

// Helper function to convert a key-value span to a JSON object that can be
// used as a request body.
nlohmann::json KvSpanToJson(
    absl::Span<const std::pair<std::string, RedfishInterface::ValueVariant>>
        kv_span) {
  nlohmann::json json;
  for (const auto &kv_pair : kv_span) {
    std::visit([&](auto val) { json[kv_pair.first] = val; }, kv_pair.second);
  }
  return json;
}

class HttpIntfVariantImpl : public RedfishVariant::ImplIntf {
 public:
  HttpIntfVariantImpl(RedfishInterface *intf,
                      ecclesia::RedfishTransport::Result result)
      : intf_(intf), result_(std::move(result)) {}
  std::unique_ptr<RedfishObject> AsObject() const override;
  std::unique_ptr<RedfishIterable> AsIterable() const override;
  bool GetValue(std::string *val) const override {
    if (!result_.body.is_string()) return false;
    *val = result_.body.get<std::string>();
    return true;
  }
  bool GetValue(int32_t *val) const override {
    if (!result_.body.is_number_integer()) return false;
    *val = result_.body.get<int32_t>();
    return true;
  }
  bool GetValue(int64_t *val) const override {
    if (!result_.body.is_number_integer()) return false;
    *val = result_.body.get<int64_t>();
    return true;
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
  ecclesia::RedfishTransport::Result result_;
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
                                RedfishInterface *intf) {
  if (json.is_object() && json.size() == 1) {
    // Check if this is a reference.
    auto odata = json.find(PropertyOdataId::Name);
    if (odata != json.end() && odata.value().is_string()) {
      // It is a reference, call GET on it
      return intf->GetUri(odata.value().get<std::string>());
    }
  }
  // Return the object as-is.
  return RedfishVariant(
      std::make_unique<HttpIntfVariantImpl>(intf,
                                            ecclesia::RedfishTransport::Result{
                                                .code = reuse_code,
                                                .body = std::move(json),
                                            }),
      ecclesia::HttpResponseCodeFromInt(reuse_code));
}

class HttpIntfObjectImpl : public RedfishObject {
 public:
  explicit HttpIntfObjectImpl(RedfishInterface *intf,
                              ecclesia::RedfishTransport::Result result)
      : intf_(intf), result_(std::move(result)) {}
  HttpIntfObjectImpl(const HttpIntfObjectImpl &) = delete;
  HttpIntfObjectImpl &operator=(const HttpIntfObjectImpl &) = delete;

  RedfishVariant operator[](const std::string &node_name) const override {
    auto itr = result_.body.find(node_name);
    if (itr == result_.body.end()) {
      return RedfishVariant(std::make_unique<HttpIntfVariantImpl>(
                                intf_,
                                ecclesia::RedfishTransport::Result{
                                    .code = result_.code,
                                    .body = nlohmann::json::value_t::discarded,
                                }),
                            ecclesia::HttpResponseCodeFromInt(result_.code));
    }

    return ResolveReference(result_.code, itr.value(), intf_);
  }
  std::optional<std::string> GetUri() override {
    auto itr = result_.body.find(PropertyOdataId::Name);
    if (itr == result_.body.end()) return std::nullopt;
    return itr.value();
  }
  std::string DebugString() override { return result_.body.dump(); }

  void ForEachProperty(absl::FunctionRef<RedfishIterReturnValue(
                           absl::string_view, RedfishVariant value)>
                           itr_func) {
    for (const auto &items : result_.body.items()) {
      if (itr_func(items.key(),
                   RedfishVariant(
                       std::make_unique<HttpIntfVariantImpl>(
                           intf_,
                           ecclesia::RedfishTransport::Result{
                               .code = result_.code,
                               .body = items.value(),
                           }),
                       ecclesia::HttpResponseCodeFromInt(result_.code))) ==
          RedfishIterReturnValue::kStop) {
        break;
      }
    }
  }

 private:
  RedfishInterface *intf_;
  ecclesia::RedfishTransport::Result result_;
};

// HttpIntfArrayIterableImpl implements the RedfishIterable interface with a
// RedfishTransport::Result containing a JSON array. The JSON array must be
// verified before constructing this class.
class HttpIntfArrayIterableImpl : public RedfishIterable {
 public:
  explicit HttpIntfArrayIterableImpl(RedfishInterface *intf,
                                     ecclesia::RedfishTransport::Result result)
      : intf_(intf), result_(std::move(result)) {}
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
    return ResolveReference(result_.code, result_.body[index], intf_);
  }

 private:
  RedfishInterface *intf_;
  ecclesia::RedfishTransport::Result result_;
};

// HttpIntfCollectionIterableImpl implements the RedfishIterable interface with
// a JSON object representing a Redfish Collection. The Collection object must
// be verified before constructing this class. Redfish Collection objects must
// have "Members@odata.count" and "Members" fields.
class HttpIntfCollectionIterableImpl : public RedfishIterable {
 public:
  explicit HttpIntfCollectionIterableImpl(
      RedfishInterface *intf, ecclesia::RedfishTransport::Result result)
      : intf_(intf), result_(std::move(result)) {}
  HttpIntfCollectionIterableImpl(const HttpIntfCollectionIterableImpl &) =
      delete;
  HttpIntfObjectImpl &operator=(const HttpIntfCollectionIterableImpl &) =
      delete;

  size_t Size() override {
    // Return size based on Members@odata.count.
    auto itr = result_.body.find(libredfish::PropertyMembersCount::Name);
    if (itr == result_.body.end() || !itr.value().is_number()) return 0;
    return itr.value().get<size_t>();
  }

  bool Empty() override {
    // Determine emptiness by checking Members@odata.count.
    auto itr = result_.body.find(libredfish::PropertyMembersCount::Name);
    if (itr == result_.body.end() || !itr.value().is_number()) return true;
    return itr.value().get<size_t>() == 0;
  }

  RedfishVariant operator[](int index) const override {
    // Check the bounds based on the array in the Members property and access
    // the Members array directly.
    auto itr = result_.body.find(libredfish::PropertyMembers::Name);
    if (itr == result_.body.end() || !itr.value().is_array() ||
        itr.value().size() <= index) {
      return RedfishVariant(absl::NotFoundError(
          absl::StrFormat("Index %d not found for json collection", index)));
    }
    return ResolveReference(result_.code, itr.value()[index], intf_);
  }

 private:
  RedfishInterface *intf_;
  ecclesia::RedfishTransport::Result result_;
};

std::unique_ptr<RedfishObject> HttpIntfVariantImpl::AsObject() const {
  if (!result_.body.is_object()) return nullptr;
  return std::make_unique<HttpIntfObjectImpl>(intf_, result_);
}

std::unique_ptr<RedfishIterable> HttpIntfVariantImpl::AsIterable() const {
  if (result_.body.is_array()) {
    return std::make_unique<HttpIntfArrayIterableImpl>(intf_, result_);
  }
  // Check if the object is a Redfish collection.
  if (result_.body.is_object() &&
      result_.body.contains(libredfish::PropertyMembers::Name) &&
      result_.body.contains(libredfish::PropertyMembersCount::Name)) {
    return std::make_unique<HttpIntfCollectionIterableImpl>(intf_, result_);
  }
  return nullptr;
}

class HttpRedfishInterface : public RedfishInterface {
 public:
  HttpRedfishInterface(std::unique_ptr<ecclesia::RedfishTransport> transport,
                       RedfishInterface::TrustedEndpoint trusted,
                       ServiceRoot service_root = ServiceRoot::kRedfish)
      : transport_(std::move(transport)),
        trusted_(trusted),
        service_root_(service_root) {}

  bool IsTrusted() const override {
    absl::ReaderMutexLock mu(&transport_mutex_);
    return trusted_ == kTrusted;
  }

  void UpdateEndpoint(absl::string_view endpoint,
                      TrustedEndpoint trusted) override {
    absl::WriterMutexLock mu(&transport_mutex_);
    trusted_ = trusted;
    absl::string_view stripped_endpoint =
        absl::StripPrefix(endpoint, "unix://");
    if (stripped_endpoint.length() != endpoint.length()) {
      // If we successfully stripped off the unix prefix, then we have a UDS.
      transport_->UpdateToUdsEndpoint(stripped_endpoint);
    } else {
      transport_->UpdateToNetworkEndpoint(endpoint);
    }
  }

  RedfishVariant GetRoot(GetParams params) override {
    if (service_root_ == ServiceRoot::kGoogle) {
      return GetUri(kGoogleServiceRoot, params);
    }

    return GetUri(kServiceRoot, params);
  }

  // GetUri fetches the given URI and resolves any JSON pointers. Note that
  // this method must not resolve any references (i.e. JSON object containing
  // only "@odata.id") to avoid infinite recursion in case a bad Redfish
  // service has a loop in its OData references.
  RedfishVariant GetUri(absl::string_view uri, GetParams params) override {
    absl::ReaderMutexLock mu(&transport_mutex_);
    absl::StatusOr<ecclesia::RedfishTransport::Result> result =
        transport_->Get(uri);
    if (!result.ok()) return RedfishVariant(result.status());

    // Handle JSON pointers if needed. Pointers follow a '#' character at the
    // end of a path.
    std::vector<absl::string_view> json_ptrs =
        absl::StrSplit(uri, absl::MaxSplits('#', 1));
    if (json_ptrs.size() < 2) {
      // No pointers, return the payload as-is.
      int code = result->code;
      return RedfishVariant(
          std::make_unique<HttpIntfVariantImpl>(this, *std::move(result)),
          ecclesia::HttpResponseCodeFromInt(code));
    }
    nlohmann::json resolved_ptr =
        ecclesia::HandleJsonPtr(result->body, json_ptrs[1]);
    result->body = std::move(resolved_ptr);
    int code = result->code;
    return RedfishVariant(
        std::make_unique<HttpIntfVariantImpl>(this, *std::move(result)),
        ecclesia::HttpResponseCodeFromInt(code));
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
    return RedfishVariant(
        std::make_unique<HttpIntfVariantImpl>(this, *std::move(result)),
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
    return RedfishVariant(
        std::make_unique<HttpIntfVariantImpl>(this, *std::move(result)),
        ecclesia::HttpResponseCodeFromInt(code));
  }

 private:
  mutable absl::Mutex transport_mutex_;
  std::unique_ptr<ecclesia::RedfishTransport> transport_
      ABSL_GUARDED_BY(transport_mutex_);
  RedfishInterface::TrustedEndpoint trusted_ ABSL_GUARDED_BY(transport_mutex_);
  const ServiceRoot service_root_;
};

}  // namespace

std::unique_ptr<RedfishInterface> NewHttpInterface(
    std::unique_ptr<ecclesia::RedfishTransport> transport,
    RedfishInterface::TrustedEndpoint trusted, ServiceRoot service_root) {
  return std::make_unique<HttpRedfishInterface>(std::move(transport), trusted,
                                                service_root);
}

}  // namespace libredfish
