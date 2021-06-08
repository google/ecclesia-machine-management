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

#include "absl/base/call_once.h"
#include "absl/strings/str_split.h"
#include "absl/strings/substitute.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/redfish/interface.h"
#include "json/writer.h"

namespace ecclesia {

namespace {

absl::once_flag curl_init_once;

constexpr auto kSupportedProtocols = CURLPROTO_HTTP | CURLPROTO_HTTPS;

}  // namespace

std::unique_ptr<LibCurlProxy> LibCurlProxy::CreateInstance() {
  absl::call_once(curl_init_once, curl_global_init, CURL_GLOBAL_ALL);
  return absl::make_unique<LibCurlProxy>();
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
                                        absl::string_view param) {
  return ::curl_easy_setopt(curl, option, param);
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

CurlHttpClient::CurlHttpClient(std::unique_ptr<LibCurl> libcurl,
                               HttpCredential cred)
    : CurlHttpClient(std::move(libcurl), cred, {}) {}

CurlHttpClient::CurlHttpClient(std::unique_ptr<LibCurl> libcurl,
                               HttpCredential cred,
                               CurlHttpClient::Config config)
    : HttpClient(),
      libcurl_(std::move(libcurl)),
      cred_(std::move(cred)),
      config_(std::move(config)),
      host_(cred_.hostname()),
      user_pwd_(absl::StrCat(cred_.username(), ":", cred_.password())) {
  curl_ = libcurl_->curl_easy_init();
}

CurlHttpClient::~CurlHttpClient() { libcurl_->curl_easy_cleanup(curl_); }

std::string CurlHttpClient::ComposeUri(absl::string_view uri) {
  return absl::Substitute("https://[$0]$1", host_, uri);
}

absl::StatusOr<CurlHttpClient::HttpResponse> CurlHttpClient::Get(
    absl::string_view uri) {
  return HttpMethod(Protocol::kGet, ComposeUri(uri), "");
}

absl::StatusOr<CurlHttpClient::HttpResponse> CurlHttpClient::Post(
    absl::string_view uri, absl::string_view post) {
  return HttpMethod(Protocol::kPost, ComposeUri(uri), post);
}

absl::StatusOr<CurlHttpClient::HttpResponse> CurlHttpClient::HttpMethod(
    Protocol cmd, absl::string_view uri, absl::string_view post) {
  absl::MutexLock l(&mu_);
  SetDefaultCurlOpts();

  if (!post.empty() || cmd == Protocol::kPost || cmd == Protocol::kPut) {
    libcurl_->curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, post.size());
    libcurl_->curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, post.data());
  }

  if (cmd == Protocol::kHead) {
    libcurl_->curl_easy_setopt(curl_, CURLOPT_NOBODY, true);
  }

  libcurl_->curl_easy_setopt(curl_, CURLOPT_URL, uri);
  libcurl_->curl_easy_setopt(curl_, CURLOPT_USERPWD, user_pwd_.c_str());

  std::string body;
  std::vector<std::string> headers;

  libcurl_->curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, BodyCallback);
  libcurl_->curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &body);
  libcurl_->curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, HeaderCallback);
  libcurl_->curl_easy_setopt(curl_, CURLOPT_WRITEHEADER, &headers);

  CURLcode code = libcurl_->curl_easy_perform(curl_);

  return HttpClient::HttpResponse{
      .code = code, .body = body, .headers = headers};
}

// userp is set through framework over third_CURLOPT_WRITEDATA
size_t CurlHttpClient::HeaderCallback(void *data, size_t size, size_t nmemb,
                                      void *userp) {
  auto *headers = static_cast<std::vector<std::string> *>(userp);
  char *str = static_cast<char *>(data);
  if (str[0] != '\r' && str[1] != '\n') {
    auto s = std::string(str, size * nmemb);
    absl::RemoveExtraAsciiWhitespace(&s);
    headers->push_back(s);
  }
  return size * nmemb;
}

// userp is set through framework over third_CURLOPT_WRITEHEADER
size_t CurlHttpClient::BodyCallback(void *data, size_t size, size_t nmemb,
                                    void *userp) {
  std::string *body = static_cast<std::string *>(userp);
  const std::string str(static_cast<const char *>(data), size * nmemb);
  *body = std::move(str);
  return size * nmemb;
}

void CurlHttpClient::SetDefaultCurlOpts() {
  if (!curl_) {
    ecclesia::ErrorLog() << "curl_ is nullptr.";
    return;
  }
  libcurl_->curl_easy_reset(curl_);

  libcurl_->curl_easy_setopt(curl_, CURLOPT_ERRORBUFFER, errbuf_);
  libcurl_->curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, (uint64_t)1L);

  libcurl_->curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, false);
  libcurl_->curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, false);

  libcurl_->curl_easy_setopt(curl_, CURLOPT_HTTP_TRANSFER_DECODING,
                             config_.raw ? false : true);
  libcurl_->curl_easy_setopt(curl_, CURLOPT_VERBOSE,
                             config_.verbose ? true : false);
  if (config_.verbose_cb != nullptr) {
    (libcurl_->curl_easy_setopt(curl_, CURLOPT_DEBUGFUNCTION,
                                config_.verbose_cb));
  }

  libcurl_->curl_easy_setopt(curl_, CURLOPT_PROXY, config_.proxy.c_str());
  libcurl_->curl_easy_setopt(curl_, CURLOPT_TIMEOUT,
                             static_cast<uint32_t>(config_.request_timeout));
  libcurl_->curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT,
                             static_cast<uint32_t>(config_.connect_timeout));

  libcurl_->curl_easy_setopt(curl_, CURLOPT_DNS_CACHE_TIMEOUT,
                             static_cast<uint32_t>(config_.dns_timeout));
  libcurl_->curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION,
                             config_.follow_redirect ? true : false);
  libcurl_->curl_easy_setopt(curl_, CURLOPT_PROTOCOLS,
                             static_cast<uint32_t>(kSupportedProtocols));

  libcurl_->curl_easy_setopt(curl_, CURLOPT_REDIR_PROTOCOLS,
                             static_cast<uint32_t>(kSupportedProtocols));
  if (config_.max_recv_speed >= 0) {
    libcurl_->curl_easy_setopt(curl_, CURLOPT_MAX_RECV_SPEED_LARGE,
                               static_cast<curl_off_t>(config_.max_recv_speed));
  }
  switch (config_.resolver) {
    case Resolver::kIPAny:
      libcurl_->curl_easy_setopt(
          curl_, CURLOPT_IPRESOLVE,
          static_cast<uint32_t>(CURL_IPRESOLVE_WHATEVER));
      break;
    case Resolver::kIPv4Only:
      libcurl_->curl_easy_setopt(curl_, CURLOPT_IPRESOLVE,
                                 static_cast<uint32_t>(CURL_IPRESOLVE_V4));
      break;
    case Resolver::kIPv6Only:
      libcurl_->curl_easy_setopt(curl_, CURLOPT_IPRESOLVE,
                                 static_cast<uint32_t>(CURL_IPRESOLVE_V6));
      break;
  }
}

}  // namespace ecclesia
