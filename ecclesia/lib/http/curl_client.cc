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

#include "ecclesia/lib/http/curl_client.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/base/const_init.h"
#include "absl/cleanup/cleanup.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "curl/curl.h"
#include "curl/easy.h"
#include "curl/system.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/http/cred.pb.h"

namespace ecclesia {

namespace {

absl::once_flag curl_init_once;

constexpr auto kSupportedProtocols = CURLPROTO_HTTP | CURLPROTO_HTTPS;

// SetCurlOpts is an overloaded helper function for setting curl options from
// the different credential configs supported.
void SetCurlOpts(LibCurl *libcurl, CURL *curl, const HttpCredential &creds) {
  libcurl->curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
  libcurl->curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false);
  if (!creds.username().empty() && !creds.password().empty()) {
    libcurl->curl_easy_setopt(
        curl, CURLOPT_USERPWD,
        absl::StrCat(creds.username(), ":", creds.password()));
  }
}
void SetCurlOpts(LibCurl *libcurl, CURL *curl, const TlsCredential &creds) {
  libcurl->curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST,
                            creds.verify_hostname());
  libcurl->curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, creds.verify_peer());
  if (creds.verify_peer() && !creds.server_ca_cert_file().empty()) {
    libcurl->curl_easy_setopt(curl, CURLOPT_CAINFO,
                              creds.server_ca_cert_file());
  }
  if (!creds.cert_file().empty()) {
    libcurl->curl_easy_setopt(curl, CURLOPT_SSLCERT, creds.cert_file());
  }
  if (!creds.key_file().empty()) {
    libcurl->curl_easy_setopt(curl, CURLOPT_SSLKEY, creds.key_file());
  }
}

}  // namespace

std::unique_ptr<LibCurlProxy> LibCurlProxy::CreateInstance() {
  absl::call_once(curl_init_once, curl_global_init, CURL_GLOBAL_ALL);
  return std::make_unique<LibCurlProxy>();
}

CURL *LibCurlProxy::curl_easy_init() { return ::curl_easy_init(); }

void LibCurlProxy::curl_easy_reset(CURL *curl) { ::curl_easy_reset(curl); }

CURLcode LibCurlProxy::curl_easy_setopt(CURL *curl, CURLoption option,
                                        bool param) {
  return ::curl_easy_setopt(curl, option, param);
}

CURLcode LibCurlProxy::curl_easy_setopt(CURL *curl, CURLoption option,
                                        uint32_t param) {
  return ::curl_easy_setopt(curl, option, param);
}

CURLcode LibCurlProxy::curl_easy_setopt(CURL *curl, CURLoption option,
                                        uint64_t param) {
  return ::curl_easy_setopt(curl, option, param);
}

CURLcode LibCurlProxy::curl_easy_setopt(CURL *curl, CURLoption option,
                                        size_t (*param)(void *, size_t, size_t,
                                                        FILE *)) {
  return ::curl_easy_setopt(curl, option, param);
}

CURLcode LibCurlProxy::curl_easy_setopt(CURL *curl, CURLoption option,
                                        curl_off_t param) {
  return ::curl_easy_setopt(curl, option, param);
}

CURLcode LibCurlProxy::curl_easy_setopt(CURL *curl, CURLoption option,
                                        const char *param) {
  return ::curl_easy_setopt(curl, option, param);
}

CURLcode LibCurlProxy::curl_easy_setopt(CURL *curl, CURLoption option,
                                        const std::string &param) {
  return ::curl_easy_setopt(curl, option, param.c_str());
}

CURLcode LibCurlProxy::curl_easy_setopt(CURL *curl, CURLoption option,
                                        void *param) {
  return ::curl_easy_setopt(curl, option, param);
}

CURLcode LibCurlProxy::curl_easy_setopt(CURL *curl, CURLoption option,
                                        size_t (*param)(const void *, size_t,
                                                        size_t, void *)) {
  return ::curl_easy_setopt(curl, option, param);
}

CURLcode LibCurlProxy::curl_easy_setopt(
    CURL *curl, CURLoption option,
    int (*param)(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                 curl_off_t ultotal, curl_off_t ulnow)) {
  return ::curl_easy_setopt(curl, option, param);
}

CURLcode LibCurlProxy::curl_easy_perform(CURL *curl) {
  return ::curl_easy_perform(curl);
}

CURLcode LibCurlProxy::curl_easy_getinfo(CURL *curl, CURLINFO info,
                                         uint64_t *value) {
  return ::curl_easy_getinfo(curl, info, value);
}

CURLcode LibCurlProxy::curl_easy_getinfo(CURL *curl, CURLINFO info,
                                         double *value) {
  return ::curl_easy_getinfo(curl, info, value);
}

void LibCurlProxy::curl_easy_cleanup(CURL *curl) {
  return ::curl_easy_cleanup(curl);
}

void LibCurlProxy::curl_free(void *p) { ::curl_free(p); }

CURLSH *LibCurlProxy::curl_share_init() { return ::curl_share_init(); }

void LibCurlProxy::curl_share_cleanup(CURLSH *share) {
  ::curl_share_cleanup(share);
}

CURLSHcode LibCurlProxy::curl_share_setopt(CURLSH *share, CURLSHoption option,
                                           curl_lock_data param) {
  return ::curl_share_setopt(share, option, param);
}

CURLSHcode LibCurlProxy::curl_share_setopt(CURLSH *share, CURLSHoption option,
                                           void (*param)(CURL *, curl_lock_data,
                                                         curl_lock_access,
                                                         void *)) {
  return ::curl_share_setopt(share, option, param);
}

CURLSHcode LibCurlProxy::curl_share_setopt(CURLSH *share, CURLSHoption option,
                                           void *param) {
  return ::curl_share_setopt(share, option, param);
}

CURLSHcode LibCurlProxy::curl_share_setopt(CURLSH *share, CURLSHoption option,
                                           void (*param)(CURL *, curl_lock_data,
                                                         void *)) {
  return ::curl_share_setopt(share, option, param);
}

CurlHttpClient::CurlHttpClient(std::unique_ptr<LibCurl> libcurl,
                               std::variant<HttpCredential, TlsCredential> cred)
    : CurlHttpClient(std::move(libcurl), std::move(cred), {}) {}

CurlHttpClient::CurlHttpClient(std::unique_ptr<LibCurl> libcurl,
                               std::variant<HttpCredential, TlsCredential> cred,
                               CurlHttpClient::Config config)
    : libcurl_(std::move(libcurl)),
      config_(std::move(config)),
      cred_(std::move(cred)) {
  // Setup share interface
  shared_connection_ = libcurl_->curl_share_init();
  // Setup interface to share the actual underlying cached connection
  libcurl_->curl_share_setopt(shared_connection_, CURLSHOPT_SHARE,
                              CURL_LOCK_DATA_CONNECT);
  // Setup locking and unlocking functions so that share interface is thread
  // safe
  libcurl_->curl_share_setopt(shared_connection_, CURLSHOPT_LOCKFUNC,
                              &LockSharedMutex);
  libcurl_->curl_share_setopt(shared_connection_, CURLSHOPT_UNLOCKFUNC,
                              &UnlockSharedMutex);
}

CurlHttpClient::~CurlHttpClient() {
  libcurl_->curl_share_cleanup(shared_connection_);
}

absl::StatusOr<CurlHttpClient::HttpResponse> CurlHttpClient::Get(
    std::unique_ptr<HttpRequest> request) {
  return HttpMethod(Protocol::kGet, std::move(request));
}

absl::StatusOr<CurlHttpClient::HttpResponse> CurlHttpClient::Post(
    std::unique_ptr<HttpRequest> request) {
  return HttpMethod(Protocol::kPost, std::move(request));
}

absl::StatusOr<CurlHttpClient::HttpResponse> CurlHttpClient::Delete(
    std::unique_ptr<HttpRequest> request) {
  return HttpMethod(Protocol::kDelete, std::move(request));
}

absl::StatusOr<CurlHttpClient::HttpResponse> CurlHttpClient::Patch(
    std::unique_ptr<HttpRequest> request) {
  return HttpMethod(Protocol::kPatch, std::move(request));
}

absl::Status CurlHttpClient::GetIncremental(
    std::unique_ptr<HttpRequest> request, IncrementalResponseHandler *handler) {
  return HttpMethod(Protocol::kGet, std::move(request), handler).status();
}

absl::Status CurlHttpClient::PostIncremental(
    std::unique_ptr<HttpRequest> request, IncrementalResponseHandler *handler) {
  return HttpMethod(Protocol::kPost, std::move(request), handler).status();
}

absl::Status CurlHttpClient::DeleteIncremental(
    std::unique_ptr<HttpRequest> request, IncrementalResponseHandler *handler) {
  return HttpMethod(Protocol::kDelete, std::move(request), handler).status();
}

absl::Status CurlHttpClient::PatchIncremental(
    std::unique_ptr<HttpRequest> request, IncrementalResponseHandler *handler) {
  return HttpMethod(Protocol::kPatch, std::move(request), handler).status();
}

class ResponseContext {
 public:
  ResponseContext(LibCurl *libcurl, CURL *curl,
                  HttpClient::IncrementalResponseHandler *handler)
      : libcurl_(libcurl),
        curl_(curl),
        handler_(handler),
        status_(absl::OkStatus()) {}
  HttpClient::HttpResponse &GetResponse() {
    if (!response_.has_value()) {
      uint64_t long_response_code = 0;
      int returned_code = 0;
      if (libcurl_->curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE,
                                      &long_response_code) == CURLE_OK) {
        returned_code = static_cast<int>(long_response_code);
      }
      response_ = HttpClient::HttpResponse{
          .code = returned_code,
          .body = response_body_,
          .headers = response_headers_,
      };
      if (handler_ != nullptr) {
        status_ = handler_->OnResponseHeaders(*response_);
      }
    }
    return *response_;
  }

  void AddHeader(absl::string_view key, absl::string_view value) {
    response_headers_.try_emplace(key, value);
  }

  bool IsIncremental() const { return handler_ != nullptr; }

  absl::Status HandleBodyData(absl::string_view data) {
    if (handler_ == nullptr) {
      // Appending absl::string directly is not supported by some toolchains
      response_body_.append(data.data(), data.size());
    } else if (!IsCancelled()) {
      status_ = handler_->OnBodyData(data);
    }
    return status_;
  }

  bool IsCancelled() const {
    return handler_ != nullptr && handler_->IsCancelled();
  }

  absl::Status status() const { return status_; }

 private:
  LibCurl *libcurl_;
  CURL *curl_;
  HttpClient::IncrementalResponseHandler *handler_;
  absl::Status status_;
  std::optional<HttpClient::HttpResponse> response_;
  std::string response_body_;
  HttpClient::HttpHeaders response_headers_;
};

absl::StatusOr<CurlHttpClient::HttpResponse> CurlHttpClient::HttpMethod(
    Protocol cmd, std::unique_ptr<HttpRequest> request,
    IncrementalResponseHandler *handler) {
  CURL *curl = libcurl_->curl_easy_init();
  if (!curl) return absl::InternalError("Failed to create curl handle");
  absl::Cleanup curl_cleanup([&]() { libcurl_->curl_easy_cleanup(curl); });
  SetDefaultCurlOpts(curl);

  libcurl_->curl_easy_setopt(curl, CURLOPT_URL, request->uri.c_str());

  // Error buffer to write to while curl handle is active
  char errbuf[CURL_ERROR_SIZE];
  libcurl_->curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

  if (!request->unix_socket_path.empty()) {
    libcurl_->curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH,
                               request->unix_socket_path.c_str());
  }

  std::visit([&](auto creds) { SetCurlOpts(libcurl_.get(), curl, creds); },
             cred_);

  switch (cmd) {
    case Protocol::kGet:
      libcurl_->curl_easy_setopt(curl, CURLOPT_HTTPGET,
                                 static_cast<uint64_t>(1));
      break;
    case Protocol::kPost:
      libcurl_->curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                                 request->body.size());
      libcurl_->curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
                                 request->body.data());
      break;
    case Protocol::kDelete:
      libcurl_->curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
      break;
    case Protocol::kPatch:
      libcurl_->curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
      libcurl_->curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                                 request->body.size());
      libcurl_->curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
                                 request->body.data());
      break;
  }

  struct curl_slist *request_headers = NULL;
  absl::Cleanup cleanup = [&request_headers]() {
    curl_slist_free_all(request_headers);
  };
  for (const auto &hdr : request->headers) {
    struct curl_slist *list = curl_slist_append(
        request_headers, absl::StrCat(hdr.first, ":", hdr.second).c_str());
    if (list == nullptr) {
      return absl::ResourceExhaustedError("request header list");
    }
    request_headers = list;
  }
  libcurl_->curl_easy_setopt(curl, CURLOPT_HTTPHEADER, request_headers);

  ResponseContext context(libcurl_.get(), curl, handler);

  libcurl_->curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, BodyCallback);
  libcurl_->curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
  libcurl_->curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
  libcurl_->curl_easy_setopt(curl, CURLOPT_WRITEHEADER, &context);

  if (handler != nullptr) {
    libcurl_->curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION,
                               ProgressCallback);
    libcurl_->curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &context);
  }

  CURLcode code = libcurl_->curl_easy_perform(curl);
  if (!context.status().ok()) {
    return context.status();
  }
  if (code == CURLE_ABORTED_BY_CALLBACK) {
    return absl::CancelledError();
  }
  if (code == CURLE_OPERATION_TIMEDOUT) {
    return absl::UnavailableError("Operation timed out");
  }
  if (code != CURLE_OK) {
    return absl::InternalError(
        absl::StrFormat("cURL failure: %s", curl_easy_strerror(code)));
  }
  if (context.IsCancelled()) {
    return absl::CancelledError();
  }
  return context.GetResponse();
}

// userp is set through framework over third_CURLOPT_WRITEDATA
size_t CurlHttpClient::HeaderCallback(const void *data, size_t size,
                                      size_t nmemb, void *userp) {
  auto *context = static_cast<ResponseContext *>(userp);
  const auto *str = static_cast<const char *>(data);

  if (str[0] != '\r' && str[1] != '\n') {
    auto s = std::string(str, size * nmemb);
    std::vector<std::string> v = absl::StrSplit(s, absl::MaxSplits(':', 1));
    if (v.size() == 2) {
      absl::StripAsciiWhitespace(&v[0]);
      absl::StripAsciiWhitespace(&v[1]);
      context->AddHeader(v[0], v[1]);
    }
  } else if (context->IsIncremental() && !context->IsCancelled()) {
    context->GetResponse();
    if (!context->status().ok()) {
      // Use CURL_WRITEFUNC_ERROR when it is available
      return size * nmemb == 0 ? 1 : 0;
    }
  }

  return size * nmemb;
}

// userp is set through framework over third_CURLOPT_WRITEHEADER
size_t CurlHttpClient::BodyCallback(const void *data, size_t size, size_t nmemb,
                                    void *userp) {
  auto *context = static_cast<ResponseContext *>(userp);
  absl::string_view data_str(static_cast<const char *>(data), size * nmemb);
  absl::Status result = context->HandleBodyData(data_str);
  if (!result.ok()) {
    // Use CURL_WRITEFUNC_ERROR when it is available
    return size * nmemb == 0 ? 1 : 0;
  }
  return size * nmemb;
}

int CurlHttpClient::ProgressCallback(void *userp, curl_off_t dltotal,
                                     curl_off_t dlnow, curl_off_t ultotal,
                                     curl_off_t ulnow) {
  auto *context = static_cast<ResponseContext *>(userp);
  return (context->IsCancelled())
             ? 1  // Any non-zero value will abort the transfer.
             : CURL_PROGRESSFUNC_CONTINUE;
}

ABSL_CONST_INIT absl::Mutex CurlHttpClient::shared_mutex_(absl::kConstInit);

void CurlHttpClient::LockSharedMutex(CURL *handle, curl_lock_data data,
                                     curl_lock_access laccess, void *useptr) {
  shared_mutex_.Lock();
}
void CurlHttpClient::UnlockSharedMutex(CURL *handle, curl_lock_data data,
                                       void *useptr) {
  shared_mutex_.Unlock();
}

void CurlHttpClient::SetDefaultCurlOpts(CURL *curl) const {
  if (!curl) {
    LOG(ERROR) << "curl is nullptr.";
    return;
  }
  libcurl_->curl_easy_reset(curl);
  libcurl_->curl_easy_setopt(curl, CURLOPT_NOSIGNAL, (uint64_t)1L);

  libcurl_->curl_easy_setopt(curl, CURLOPT_HTTP_TRANSFER_DECODING,
                             !config_.raw);
  libcurl_->curl_easy_setopt(curl, CURLOPT_VERBOSE, config_.verbose);
  if (config_.verbose_cb != nullptr) {
    (libcurl_->curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION,
                                config_.verbose_cb));
  }

  libcurl_->curl_easy_setopt(curl, CURLOPT_PROXY, config_.proxy.c_str());
  libcurl_->curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,
                             config_.request_timeout_msec);
  libcurl_->curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS,
                             config_.connect_timeout_msec);

  libcurl_->curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT,
                             static_cast<uint32_t>(config_.dns_timeout));
  libcurl_->curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,
                             config_.follow_redirect);
  libcurl_->curl_easy_setopt(curl, CURLOPT_PROTOCOLS,
                             static_cast<uint32_t>(kSupportedProtocols));

  libcurl_->curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS,
                             static_cast<uint32_t>(kSupportedProtocols));
  if (config_.max_recv_speed >= 0) {
    libcurl_->curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE,
                               static_cast<curl_off_t>(config_.max_recv_speed));
  }
  switch (config_.resolver) {
    case Resolver::kIPAny:
      libcurl_->curl_easy_setopt(
          curl, CURLOPT_IPRESOLVE,
          static_cast<uint32_t>(CURL_IPRESOLVE_WHATEVER));
      break;
    case Resolver::kIPv4Only:
      libcurl_->curl_easy_setopt(curl, CURLOPT_IPRESOLVE,
                                 static_cast<uint32_t>(CURL_IPRESOLVE_V4));
      break;
    case Resolver::kIPv6Only:
      libcurl_->curl_easy_setopt(curl, CURLOPT_IPRESOLVE,
                                 static_cast<uint32_t>(CURL_IPRESOLVE_V6));
      break;
  }

  libcurl_->curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT,
                             config_.low_speed_limit);
  libcurl_->curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME,
                             config_.low_speed_time);

  // Share connections from other curl connections
  libcurl_->curl_easy_setopt(curl, CURLOPT_SHARE, shared_connection_);
}

}  // namespace ecclesia
