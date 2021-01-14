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

#include "ecclesia/lib/redfish/raw.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/redfish/interface.h"

extern "C" {
#include "redfishPayload.h"
#include "redfishService.h"
}  // extern "C"

namespace libredfish {
namespace {

// Redfish version for the service root.
constexpr char kRedfishServiceVersionRoot[] = "/redfish/v1";

struct RedfishPayloadDeleter {
  void operator()(redfishPayload *payload) {
    if (payload) cleanupPayload(payload);
  }
};
using PayloadUniquePtr = std::unique_ptr<redfishPayload, RedfishPayloadDeleter>;

struct RedfishServiceDeleter {
  void operator()(redfishService *service) {
    if (service) cleanupServiceEnumerator(service);
  }
};
using ServiceUniquePtr = std::unique_ptr<redfishService, RedfishServiceDeleter>;

struct FreeDeleter {
  inline void operator()(void *ptr) const { free(ptr); }
};
using MallocChar = std::unique_ptr<char, FreeDeleter>;

class JsonValue {
 public:
  explicit JsonValue(int val) : json_(json_integer(val)) {}
  explicit JsonValue(bool val) : json_(json_boolean(val)) {}
  explicit JsonValue(const char *val) : json_(json_string(val)) {}
  explicit JsonValue(std::string val) : json_(json_string(val.data())) {}
  explicit JsonValue(double val) : json_(json_real(val)) {}
  JsonValue(const JsonValue &) = delete;
  JsonValue &operator=(const JsonValue &) = delete;

  ~JsonValue() { json_decref(json_); }

  json_t *get() { return json_; }

 private:
  json_t *json_;
};

// RawPayload provides a wrapper for interacting with responses from a
// Redfish query. The provided methods will either provide additional info or
// allow data to be extracted. RawPayload is an interface containing the union
// interface of all types exported by RedfishVariant.
class RawPayload {
  // ConstructorPasskey for implementing the Passkey Idiom in order to restrict
  // access to RawPayload's ctor while still allowing it to be used with
  // std::make_shared.
  class ConstructorPasskey {
   private:
    ConstructorPasskey() {}
    friend RawPayload;
  };

 public:
  // This constructor is effectively private due to the ConstructorPasskey.
  // NewShared should be used instead to construct new instances of RawPayload.
  RawPayload(PayloadUniquePtr payload, ConstructorPasskey)
      : payload_(std::move(ecclesia::DieIfNull(payload))) {}
  static std::shared_ptr<RawPayload> NewShared(PayloadUniquePtr payload) {
    if (!payload) return nullptr;
    return std::make_shared<RawPayload>(std::move(payload),
                                        ConstructorPasskey{});
  }
  RawPayload(const RawPayload &) = delete;
  RawPayload &operator=(const RawPayload &) = delete;

  std::string DebugString() {
    MallocChar output(payloadToString(payload_.get(), /*prettyprint=*/true));
    if (!output) return "(null output)";
    std::string ret(output.get());
    return ret;
  }

  absl::optional<std::string> GetUri() {
    MallocChar output(getPayloadUri(payload_.get()));
    if (!output) return absl::nullopt;
    std::string ret(output.get());
    if (ret.empty()) return absl::nullopt;
    return ret;
  }

  std::shared_ptr<RawPayload> GetNode(const std::string &node_name) {
    PayloadUniquePtr payload(
        getPayloadByNodeName(payload_.get(), node_name.c_str()));
    if (!payload) return nullptr;
    return RawPayload::NewShared(std::move(payload));
  }

  // Returns true if the current payload is a Collection or Array.
  bool IsIndexable() {
    return isPayloadCollection(payload_.get()) ||
           isPayloadArray(payload_.get());
  }
  size_t Size() {
    if (isPayloadCollection(payload_.get()))
      return getCollectionSize(payload_.get());
    if (isPayloadArray(payload_.get())) return getArraySize(payload_.get());
    return 0;
  }
  bool Empty() { return Size() == 0; }

  std::shared_ptr<RawPayload> GetIndex(int index) {
    return RawPayload::NewShared(
        PayloadUniquePtr(getPayloadByIndex(payload_.get(), index)));
  }

  bool GetValue(std::string *val) {
    MallocChar raw_ret(getPayloadStringValue(payload_.get()));
    if (!raw_ret) return false;
    *val = std::string(raw_ret.get());
    return true;
  }
  bool GetValue(int32_t *val) {
    // The current libredfish API cannot distinguish between the field being
    // not of type int and whether the int32 value is 0.
    *val = getPayloadIntValue(payload_.get());
    return true;
  }
  bool GetValue(int64_t *val) {
    // The current libredfish API cannot distinguish between the field being
    // not of type int and whether the int64 value is 0.
    *val = getPayloadLongLongValue(payload_.get());
    return true;
  }
  bool GetValue(bool *val) {
    bool is_bool;
    *val = getPayloadBoolValue(payload_.get(), &is_bool);
    if (!is_bool) return false;
    return true;
  }
  bool GetValue(double *val) {
    bool is_double;
    *val = getPayloadDoubleValue(payload_.get(), &is_double);
    if (!is_double) return false;
    return true;
  }

 private:
  PayloadUniquePtr payload_;
};

class RawVariantImpl : public RedfishVariant::ImplIntf {
 public:
  RawVariantImpl() : payload_(nullptr) {}
  explicit RawVariantImpl(std::shared_ptr<RawPayload> payload)
      : payload_(payload) {}
  std::unique_ptr<RedfishObject> AsObject() override;
  std::unique_ptr<RedfishIterable> AsIterable() override;
  bool GetValue(std::string *val) override {
    if (!payload_) return false;
    return payload_->GetValue(val);
  }
  bool GetValue(int32_t *val) override {
    if (!payload_) return false;
    return payload_->GetValue(val);
  }
  bool GetValue(int64_t *val) override {
    if (!payload_) return false;
    return payload_->GetValue(val);
  }
  bool GetValue(double *val) override {
    if (!payload_) return false;
    return payload_->GetValue(val);
  }
  bool GetValue(bool *val) override {
    if (!payload_) return false;
    return payload_->GetValue(val);
  }
  std::string DebugString() override {
    if (!payload_) return "(null payload)";
    return payload_->DebugString();
  }

 private:
  std::shared_ptr<RawPayload> payload_;
};

class RawObject : public RedfishObject {
 public:
  explicit RawObject(std::shared_ptr<RawPayload> payload) : payload_(payload) {}
  RawObject(const RawObject &) = delete;
  RawObject &operator=(const RawObject &) = delete;

  RedfishVariant GetNode(const std::string &node_name) const override {
    return RedfishVariant(
        absl::make_unique<RawVariantImpl>(payload_->GetNode(node_name)));
  }
  absl::optional<std::string> GetUri() override { return payload_->GetUri(); }

 private:
  std::shared_ptr<RawPayload> payload_;
};

class RawIterable : public RedfishIterable {
 public:
  explicit RawIterable(std::shared_ptr<RawPayload> payload)
      : payload_(payload) {}
  RawIterable(const RawIterable &) = delete;
  RawObject &operator=(const RawIterable &) = delete;

  size_t Size() override { return payload_->Size(); }
  bool Empty() override { return payload_->Empty(); }
  RedfishVariant GetIndex(int index) override {
    return RedfishVariant(
        absl::make_unique<RawVariantImpl>(payload_->GetIndex(index)));
  }

 private:
  std::shared_ptr<RawPayload> payload_;
};

std::unique_ptr<RedfishObject> RawVariantImpl::AsObject() {
  if (!payload_) return nullptr;
  return absl::make_unique<RawObject>(payload_);
}

std::unique_ptr<RedfishIterable> RawVariantImpl::AsIterable() {
  if (!payload_) return nullptr;
  if (!payload_->IsIndexable()) return nullptr;
  return absl::make_unique<RawIterable>(payload_);
}

// RawIntf provides an interaface wrapper for the redfishService C type.
class RawIntf : public RedfishInterface {
 public:
  explicit RawIntf(ServiceUniquePtr service, TrustedEndpoint trusted)
      : service_(std::move(service)), trusted_(trusted) {}
  RawIntf(const RawIntf &) = delete;
  RawIntf operator=(const RawIntf &) = delete;

  bool IsTrusted() const override {
    absl::ReaderMutexLock lock(&service_mutex_);
    return trusted_ == kTrusted;
  }

  void UpdateEndpoint(absl::string_view endpoint,
                      TrustedEndpoint trusted) override {
    absl::WriterMutexLock lock(&service_mutex_);
    updateServiceHost(service_.get(), endpoint.data());
    trusted_ = trusted;
  }

  RedfishVariant GetRoot() override {
    // Ideally we would use getRedfishServiceRoot from libredfish, but for some
    // reason it stores some state which breaks if the connection goes down
    // and cannot be fixed. As a workaround, just manually get root assuming
    // we will always be using kRedfishServiceVersionRoot.
    // See https://github.com/DMTF/libredfish/issues/133
    return GetUri(kRedfishServiceVersionRoot);
  }

  RedfishVariant GetUri(absl::string_view uri) override {
    absl::ReaderMutexLock lock(&service_mutex_);
    if (!service_) {
      return RedfishVariant();
    }
    return RedfishVariant(
        absl::make_unique<RawVariantImpl>(RawPayload::NewShared(
            PayloadUniquePtr(getPayloadByUri(service_.get(), uri.data())))));
  }

  RedfishVariant PostUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) override {
    absl::ReaderMutexLock lock(&service_mutex_);
    if (!service_) {
      return RedfishVariant();
    }

    json_t *post_payload = json_object();
    for (const auto &kv_pair : kv_span) {
      absl::visit(
          [&](auto val) {
            JsonValue jValue(val);
            json_object_set(post_payload, kv_pair.first.data(), jValue.get());
          },
          kv_pair.second);
    }

    MallocChar content(json_dumps(post_payload, 0));
    json_decref(post_payload);
    json_t *value =
        postUriFromService(service_.get(), uri.data(), content.get(), 0, NULL);

    if (!value) {
      return RedfishVariant();
    }

    return RedfishVariant(
        absl::make_unique<RawVariantImpl>(RawPayload::NewShared(
            PayloadUniquePtr(createRedfishPayload(value, service_.get())))));
  }

  RedfishVariant PostUri(absl::string_view uri,
                         absl::string_view data) override {
    absl::ReaderMutexLock lock(&service_mutex_);
    if (!service_) {
      return RedfishVariant();
    }

    json_t *value =
        postUriFromService(service_.get(), uri.data(), data.begin(), 0, NULL);
    if (!value) {
      return RedfishVariant();
    }

    return RedfishVariant(
        absl::make_unique<RawVariantImpl>(RawPayload::NewShared(
            PayloadUniquePtr(createRedfishPayload(value, service_.get())))));
  }

 private:
  mutable absl::Mutex service_mutex_;
  ServiceUniquePtr service_ ABSL_GUARDED_BY(service_mutex_);
  TrustedEndpoint trusted_ ABSL_GUARDED_BY(service_mutex_);
};

}  // namespace

// Constructor method for creating a RawIntf.
std::unique_ptr<RedfishInterface> NewRawInterface(
    const std::string &endpoint,
    libredfish::RedfishInterface::TrustedEndpoint trusted) {
  // createServiceEnumerator only returns NULL if calloc fails, regardless of
  // whether the endpoint is valid or reachable.
  ServiceUniquePtr service(
      createServiceEnumerator(endpoint.c_str(), nullptr, nullptr, 0));
  return absl::make_unique<RawIntf>(std::move(service), trusted);
}

// Constructor method for creating a RawInterface with auth session.
std::unique_ptr<RedfishInterface> NewRawSessionAuthInterface(
    const PasswordArgs &connectionArgs) {
  enumeratorAuthentication auth;
  auth.authType = REDFISH_AUTH_SESSION;

  std::string username_buf = connectionArgs.username;
  std::string password_buf = connectionArgs.password;
  auth.authCodes.userPass.username = &username_buf[0];
  auth.authCodes.userPass.password = &password_buf[0];

  ServiceUniquePtr service(createServiceEnumerator(
      connectionArgs.endpoint.c_str(), nullptr, &auth, 0));
  return absl::make_unique<RawIntf>(std::move(service),
                                    RedfishInterface::kTrusted);
}

std::unique_ptr<RedfishInterface> NewRawBasicAuthInterface(
    const PasswordArgs &connectionArgs) {
  enumeratorAuthentication auth;
  auth.authType = REDFISH_AUTH_BASIC;

  std::string username_buf = connectionArgs.username;
  std::string password_buf = connectionArgs.password;
  auth.authCodes.userPass.username = username_buf.data();
  auth.authCodes.userPass.password = password_buf.data();

  ServiceUniquePtr service(createServiceEnumerator(
      connectionArgs.endpoint.c_str(), nullptr, &auth, 0));
  return absl::make_unique<RawIntf>(std::move(service),
                                    RedfishInterface::kTrusted);
}

std::unique_ptr<RedfishInterface> NewRawTlsAuthInterface(
    const TlsArgs &connectionArgs) {
  enumeratorAuthentication auth;
  auth.authType = REDFISH_AUTH_TLS;

  auth.authCodes.authTls.verifyPeer = connectionArgs.verify_peer;
  auth.authCodes.authTls.verifyHostname = connectionArgs.verify_hostname;
  auth.authCodes.authTls.caCertFile = connectionArgs.ca_cert_file.has_value()
                                          ? connectionArgs.ca_cert_file->c_str()
                                          : nullptr;
  auth.authCodes.authTls.clientCertFile = connectionArgs.cert_file.c_str();
  auth.authCodes.authTls.clientKeyFile = connectionArgs.key_file.c_str();

  ServiceUniquePtr service(createServiceEnumerator(
      connectionArgs.endpoint.c_str(), nullptr, &auth, 0));
  return absl::make_unique<RawIntf>(std::move(service),
                                    RedfishInterface::kTrusted);
}

}  // namespace libredfish
