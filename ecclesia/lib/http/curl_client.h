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

#ifndef ECCLESIA_LIB_HTTP_CURL_CLIENT_H_
#define ECCLESIA_LIB_HTTP_CURL_CLIENT_H_

#include "absl/base/attributes.h"
#include "absl/synchronization/mutex.h"
#include "curl/curl.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/redfish/interface.h"

namespace ecclesia {

// A thin wrapper for the libcurl C functions, for dependency injection.
// We use exactly the same function names as libcurl, assuming callers are
// familiar with libcurl C functions.
class LibCurl {
 public:
  virtual ~LibCurl() {}

  virtual CURL *curl_easy_init() = 0;

  virtual void curl_easy_reset(CURL *curl) = 0;

  virtual CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                                    bool param) = 0;
  virtual CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                                    uint32_t param) = 0;
  virtual CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                                    uint64_t param) = 0;
  virtual CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                                    size_t (*param)(void *, size_t, size_t,
                                                    FILE *)) = 0;

  virtual CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                                    curl_off_t param) = 0;

  virtual CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                                    const char *param) = 0;

  virtual CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                                    absl::string_view param) = 0;
  virtual CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                                    void *param) = 0;
  virtual CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                                    size_t (*param)(const void *, size_t,
                                                    size_t, void *)) = 0;

  virtual CURLcode curl_easy_setopt(
      CURL *curl, CURLoption option,
      int (*param)(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                   curl_off_t ultotal, curl_off_t ulnow)) = 0;

  virtual CURLcode curl_easy_perform(CURL *curl) = 0;

  virtual CURLcode curl_easy_getinfo(CURL *curl, CURLINFO info,
                                     uint64_t *value) = 0;

  virtual CURLcode curl_easy_getinfo(CURL *curl, CURLINFO info,
                                     double *value) = 0;

  virtual void curl_easy_cleanup(CURL *curl) = 0;
  virtual void curl_free(void *p) = 0;
};

class LibCurlProxy : public LibCurl {
 public:
  static std::unique_ptr<LibCurlProxy> CreateInstance();

  CURL *curl_easy_init() override;

  void curl_easy_reset(CURL *curl) override;

  CURLcode curl_easy_setopt(CURL *curl, CURLoption option, bool param) override;

  CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                            uint32_t param) override;

  CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                            uint64_t param) override;

  CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                            curl_off_t param) override;

  CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                            const char *param) override;

  CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                            absl::string_view param) override;

  CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                            void *param) override;

  CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                            size_t (*param)(const void *, size_t, size_t,
                                            void *)) override;

  CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                            size_t (*param)(void *, size_t, size_t,
                                            FILE *)) override;
  CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                            int (*param)(void *clientp, curl_off_t dltotal,
                                         curl_off_t dlnow, curl_off_t ultotal,
                                         curl_off_t ulnow)) override;

  CURLcode curl_easy_perform(CURL *curl) override;

  CURLcode curl_easy_getinfo(CURL *curl, CURLINFO info,
                             uint64_t *value) override;

  CURLcode curl_easy_getinfo(CURL *curl, CURLINFO info, double *value) override;

  void curl_easy_cleanup(CURL *curl) override;

  void curl_free(void *p) override;
};

class CurlHttpClient : public HttpClient {
 public:
  struct Config {
    // These are the values we use to parse to curl_easy_setopt

    // CURLOPT_HTTP_TRANSFER_DECODING
    bool raw = false;
    // CURLOPT_VERBOSE
    bool verbose = false;
    // CURLOPT_DEBUGFUNCTION
    curl_debug_callback verbose_cb = nullptr;
    // CURLOPT_PROXY
    std::string proxy;
    // CURLOPT_TIMEOUT, in sec.
    int request_timeout = 5;
    // CURLOPT_CONNECTTIMEOUT, in sec.
    int connect_timeout = 5;
    // CURLOPT_DNS_CACHE_TIMEOUT, in sec.
    int dns_timeout = 60;
    // CURLOPT_FOLLOWLOCATION
    bool follow_redirect = false;
    // CURLOPT_MAX_RECV_SPEED_LARGE
    int max_recv_speed = -1;
    // CURLOPT_IPRESOLVE
    HttpClient::Resolver resolver = Resolver::kIPAny;
  };

  CurlHttpClient(std::unique_ptr<LibCurl> libcurl, HttpCredential cred);
  CurlHttpClient(std::unique_ptr<LibCurl> libcurl, HttpCredential cred,
                 Config config);
  ~CurlHttpClient() override;

  CurlHttpClient(const CurlHttpClient &) = delete;
  CurlHttpClient &operator=(const CurlHttpClient &) = delete;
  CurlHttpClient(CurlHttpClient &&other) = delete;
  CurlHttpClient &operator=(CurlHttpClient &&other) = delete;

  absl::StatusOr<HttpResponse> Get(
      std::unique_ptr<HttpRequest> request) override;
  absl::StatusOr<HttpResponse> Post(
      std::unique_ptr<HttpRequest> request) override;
  absl::StatusOr<HttpResponse> Delete(
      std::unique_ptr<HttpRequest> request) override;
  absl::StatusOr<HttpResponse> Patch(
      std::unique_ptr<HttpRequest> request) override;

  // Helper methods for simple cases.
  // "path" is an absolute Redfish path, e.g., "/redfish/v1".
  absl::StatusOr<HttpResponse> GetPath(absl::string_view path);
  absl::StatusOr<HttpResponse> PostPath(absl::string_view path,
                                        absl::string_view post);
  absl::StatusOr<HttpResponse> DeletePath(absl::string_view path);
  absl::StatusOr<HttpResponse> PatchPath(absl::string_view path,
                                         absl::string_view patch);

  Config GetConfig() const { return config_; }

 private:
  absl::StatusOr<std::unique_ptr<HttpClient::HttpRequest>>
      InitRequest(absl::string_view path);
  absl::StatusOr<HttpResponse> HttpMethod(Protocol cmd,
                                          std::unique_ptr<HttpRequest> request)
      ABSL_LOCKS_EXCLUDED(mu_);
  void SetDefaultCurlOpts() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  static size_t HeaderCallback(const void *data, size_t size, size_t nmemb,
                               void *userp);
  static size_t BodyCallback(const void *data, size_t size, size_t nmemb,
                             void *userp);

  std::string ComposeUri(absl::string_view uri);

  std::unique_ptr<LibCurl> libcurl_;
  HttpCredential cred_;
  Config config_;
  const std::string host_;
  const std::string user_pwd_;

  CURL *curl_ ABSL_GUARDED_BY(mu_);
  char errbuf_[CURL_ERROR_SIZE] ABSL_GUARDED_BY(mu_) = {};
  absl::Mutex mu_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_HTTP_CURL_CLIENT_H_
