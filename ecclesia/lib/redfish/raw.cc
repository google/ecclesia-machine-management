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
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/http/codes.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/libredfish_adapter.h"

extern "C" {
#include "redfishPayload.h"
#include "redfishService.h"
}  // extern "C"

namespace libredfish {

const RedfishRawInterfaceOptions kDefaultRedfishRawInterfaceOptions{
    .default_timeout = absl::Seconds(5),
};

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

// Helper function which serializes a Span of Redfish property -> value map
// pairs into a string that can be sent over the wire.
MallocChar KvSpanToJsonCharBuffer(
    absl::Span<const std::pair<std::string, RedfishInterface::ValueVariant>>
        kv_span) {
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
  return content;
}

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
      : payload_(std::move(payload)) {}
  std::unique_ptr<RedfishObject> AsObject() const override;
  std::unique_ptr<RedfishIterable> AsIterable() const override;
  bool GetValue(std::string *val) const override {
    if (!payload_) return false;
    return payload_->GetValue(val);
  }
  bool GetValue(int32_t *val) const override {
    if (!payload_) return false;
    return payload_->GetValue(val);
  }
  bool GetValue(int64_t *val) const override {
    if (!payload_) return false;
    return payload_->GetValue(val);
  }
  bool GetValue(double *val) const override {
    if (!payload_) return false;
    return payload_->GetValue(val);
  }
  bool GetValue(bool *val) const override {
    if (!payload_) return false;
    return payload_->GetValue(val);
  }
  bool GetValue(absl::Time *val) const override {
    if (!payload_) {
      return false;
    }
    std::string dt_string;
    if (!payload_->GetValue(&dt_string)) {
      return false;
    }
    return absl::ParseTime("%Y-%m-%dT%H:%M:%S%Z", dt_string, val, nullptr);
  }
  std::string DebugString() const override {
    if (!payload_) return "(null payload)";
    return payload_->DebugString();
  }

 private:
  std::shared_ptr<RawPayload> payload_;
};

class RawObject : public RedfishObject {
 public:
  explicit RawObject(std::shared_ptr<RawPayload> payload)
      : payload_(std::move(payload)) {}
  RawObject(const RawObject &) = delete;
  RawObject &operator=(const RawObject &) = delete;

  RedfishVariant operator[](const std::string &node_name) const override {
    return RedfishVariant(
        absl::make_unique<RawVariantImpl>(payload_->GetNode(node_name)));
  }
  absl::optional<std::string> GetUri() override { return payload_->GetUri(); }
  std::string DebugString() override { return payload_->DebugString(); }

 private:
  std::shared_ptr<RawPayload> payload_;
};

class RawIterable : public RedfishIterable {
 public:
  explicit RawIterable(std::shared_ptr<RawPayload> payload)
      : payload_(std::move(payload)) {}
  RawIterable(const RawIterable &) = delete;
  RawObject &operator=(const RawIterable &) = delete;

  size_t Size() override { return payload_->Size(); }
  bool Empty() override { return payload_->Empty(); }
  RedfishVariant operator[](int index) const override {
    return RedfishVariant(
        absl::make_unique<RawVariantImpl>(payload_->GetIndex(index)));
  }

 private:
  std::shared_ptr<RawPayload> payload_;
};

std::unique_ptr<RedfishObject> RawVariantImpl::AsObject() const {
  if (!payload_) return nullptr;
  return absl::make_unique<RawObject>(payload_);
}

std::unique_ptr<RedfishIterable> RawVariantImpl::AsIterable() const {
  if (!payload_) return nullptr;
  if (!payload_->IsIndexable()) return nullptr;
  return absl::make_unique<RawIterable>(payload_);
}

// Struct to hold the results of a Redfish HTTP operation.
struct LibredfishCallbackResult {
  bool success;
  uint16_t http_code;
  PayloadUniquePtr payload;
};

// Context object to be used for interfacing with the async worker in the
// libredfish library. Implements a producer and consumer function to pass a
// LibredfishCallbackResult one time. Interacting between a single producer
// and consumer is threadsafe as long as each function is only invoked once.
class AsyncLibredfishChannel {
 public:
  AsyncLibredfishChannel() {}

  // Producer: Sets the payload and notifies the consumer. Intended to be
  // invoked once.
  void SetPayloadAndNotify(LibredfishCallbackResult payload) {
    payload_ = std::move(payload);
    notification_.Notify();
  }

  // Consumer: Waits for data to be ready and consumes the payload. Intended to
  // be invoked once.
  LibredfishCallbackResult WaitForPayloadAndConsume() {
    // Block forever for the notification. We rely on the timeout in the
    // redfishAsyncOptions to trigger if there are any HTTP issues. We cannot
    // change WaitForNotification to WaitForNotificationWithTimeout without
    // updating the LibredfishAsyncToSyncCallback function to use shared_ptr,
    // otherwise we risk having the main thread time out on the worker thread,
    // deleting this context, and having the worker thread hold a dangling ptr.
    notification_.WaitForNotification();
    return std::move(payload_);
  }

 private:
  absl::Notification notification_;
  LibredfishCallbackResult payload_;
};

// Callback passed to libredfish to invoke in the libredfish worker thread to
// pass back the HTTP result to the main thread.
void LibredfishAsyncToSyncCallback(bool success, uint16_t http_code,
                                   redfishPayload *payload, void *context) {
  AsyncLibredfishChannel *async_channel =
      static_cast<AsyncLibredfishChannel *>(context);
  async_channel->SetPayloadAndNotify(
      LibredfishCallbackResult{.success = success,
                               .http_code = http_code,
                               .payload = PayloadUniquePtr(payload)});
  // After we've notified the main thread, there are no guarantees that *context
  // will remain a valid pointer.
}

// RawIntf provides an interaface wrapper for the redfishService C type.
class RawIntf : public RedfishInterface {
 public:
  explicit RawIntf(ServiceUniquePtr service, TrustedEndpoint trusted,
                   const RedfishRawInterfaceOptions &options)
      : options_(options), service_(std::move(service)), trusted_(trusted) {}
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
      return RedfishVariant(
          absl::FailedPreconditionError("not connected to a Redifsh service"));
    }
    return RedfishVariant(
        absl::make_unique<RawVariantImpl>(RawPayload::NewShared(
            PayloadUniquePtr(getPayloadByUri(service_.get(), uri.data())))));
  }

  RedfishVariant PostUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) override {
    MallocChar content = KvSpanToJsonCharBuffer(kv_span);
    return PostUri(uri, absl::string_view(content.get()));
  }

  RedfishVariant PostUri(absl::string_view uri,
                         absl::string_view data) override {
    absl::ReaderMutexLock lock(&service_mutex_);
    if (!service_) {
      return RedfishVariant(
          absl::FailedPreconditionError("Not connected to a Redifsh service."));
    }

    PayloadUniquePtr payload(
        createRedfishPayloadFromString(data.data(), service_.get()));

    AsyncLibredfishChannel channel;
    redfishAsyncOptions async_options = ConvertOptions(options_);
    // Actually start the request, the channel becomes the (void * context)
    // parameter in the request callback.
    if (!postUriFromServiceAsync(service_.get(), uri.data(), payload.get(),
                                 &async_options, LibredfishAsyncToSyncCallback,
                                 &channel)) {
      return RedfishVariant(
          absl::UnavailableError("Unable to start the async Redfish POST."));
    }

    // Return the result as a variant.
    LibredfishCallbackResult async_payload = channel.WaitForPayloadAndConsume();
    return RedfishVariant(
        absl::make_unique<RawVariantImpl>(
            RawPayload::NewShared(std::move(async_payload.payload))),
        ecclesia::HttpResponseCodeFromInt(async_payload.http_code));
  }

  RedfishVariant PatchUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) override {
    absl::ReaderMutexLock lock(&service_mutex_);
    if (!service_) {
      return RedfishVariant(
          absl::FailedPreconditionError("Not connected to a Redifsh service."));
    }

    MallocChar content = KvSpanToJsonCharBuffer(kv_span);
    PayloadUniquePtr payload(
        createRedfishPayloadFromString(content.get(), service_.get()));

    AsyncLibredfishChannel channel;
    redfishAsyncOptions async_options = ConvertOptions(options_);
    // Actually start the request, the channel becomes the (void * context)
    // parameter in the request callback.
    if (!patchUriFromServiceAsync(service_.get(), uri.data(), payload.get(),
                                  &async_options, LibredfishAsyncToSyncCallback,
                                  &channel)) {
      return RedfishVariant(
          absl::UnavailableError("Unable to start the async Redfish PATCH."));
    }

    // Return the result as a variant.
    LibredfishCallbackResult async_payload = channel.WaitForPayloadAndConsume();
    return RedfishVariant(
        absl::make_unique<RawVariantImpl>(
            RawPayload::NewShared(std::move(async_payload.payload))),
        ecclesia::HttpResponseCodeFromInt(async_payload.http_code));
  }

 private:
  const RedfishRawInterfaceOptions options_;
  mutable absl::Mutex service_mutex_;
  ServiceUniquePtr service_ ABSL_GUARDED_BY(service_mutex_);
  TrustedEndpoint trusted_ ABSL_GUARDED_BY(service_mutex_);
};

}  // namespace

// Constructor method for creating a RawIntf.
std::unique_ptr<RedfishInterface> NewRawInterface(
    const std::string &endpoint,
    libredfish::RedfishInterface::TrustedEndpoint trusted,
    std::unique_ptr<ecclesia::HttpClient> client,
    const RedfishRawInterfaceOptions &options) {
  serviceHttpHandler handler{};
  if (client) {
    handler = NewLibredfishAdapter(std::move(client), options);
  }

  // createServiceEnumerator only returns NULL if calloc fails, regardless of
  // whether the endpoint is valid or reachable.
  // Handler is consumed even on failure.
  ServiceUniquePtr service(createServiceEnumeratorExt(endpoint.c_str(), nullptr,
                                                      nullptr, 0, &handler));
  return absl::make_unique<RawIntf>(std::move(service), trusted, options);
}

// Constructor method for creating a RawInterface with auth session.
std::unique_ptr<RedfishInterface> NewRawSessionAuthInterface(
    const PasswordArgs &connectionArgs,
    std::unique_ptr<ecclesia::HttpClient> client,
    const RedfishRawInterfaceOptions &options) {
  serviceHttpHandler handler{};
  if (client) {
    handler = NewLibredfishAdapter(std::move(client), options);
  }

  enumeratorAuthentication auth;
  auth.authType = REDFISH_AUTH_SESSION;

  std::string username_buf = connectionArgs.username;
  std::string password_buf = connectionArgs.password;
  auth.authCodes.userPass.username = &username_buf[0];
  auth.authCodes.userPass.password = &password_buf[0];

  // Note: handler is consumed even on failure.
  ServiceUniquePtr service(createServiceEnumeratorExt(
      connectionArgs.endpoint.c_str(), nullptr, &auth, 0, &handler));
  if (service == nullptr) {
    return nullptr;
  }
  return absl::make_unique<RawIntf>(std::move(service),
                                    RedfishInterface::kTrusted, options);
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
                                    RedfishInterface::kTrusted,
                                    kDefaultRedfishRawInterfaceOptions);
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
                                    RedfishInterface::kTrusted,
                                    kDefaultRedfishRawInterfaceOptions);
}

}  // namespace libredfish
