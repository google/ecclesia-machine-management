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

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <variant>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "curl/curl.h"
#include "curl/system.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/http/cred.pb.h"

namespace ecclesia {

// A thin wrapper for the libcurl C functions, for dependency injection.
// We use exactly the same function names as libcurl, assuming callers are
// familiar with libcurl C functions.
class LibCurl {
 public:
  virtual ~LibCurl() {}

  virtual CURL* curl_easy_init() = 0;

  virtual void curl_easy_reset(CURL* curl) = 0;

  virtual CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                                    bool param) = 0;
  virtual CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                                    uint32_t param) = 0;
  virtual CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                                    uint64_t param) = 0;
  virtual CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                                    size_t (*param)(void*, size_t, size_t,
                                                    FILE*)) = 0;

  virtual CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                                    curl_off_t param) = 0;

  virtual CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                                    const char* param) = 0;

  virtual CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                                    const std::string& param) = 0;
  virtual CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                                    void* param) = 0;
  virtual CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                                    size_t (*param)(const void*, size_t, size_t,
                                                    void*)) = 0;
  virtual CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                                    size_t (*param)(char*, size_t, size_t,
                                                    void*)) = 0;

  virtual CURLcode curl_easy_setopt(
      CURL* curl, CURLoption option,
      int (*param)(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                   curl_off_t ultotal, curl_off_t ulnow)) = 0;

  virtual CURLcode curl_easy_perform(CURL* curl) = 0;

  virtual CURLcode curl_easy_getinfo(CURL* curl, CURLINFO info,
                                     uint64_t* value) = 0;

  virtual CURLcode curl_easy_getinfo(CURL* curl, CURLINFO info,
                                     double* value) = 0;

  virtual void curl_easy_cleanup(CURL* curl) = 0;
  virtual void curl_free(void* p) = 0;

  virtual CURLSH* curl_share_init() = 0;
  virtual void curl_share_cleanup(CURLSH* share) = 0;

  virtual CURLSHcode curl_share_setopt(CURLSH* share, CURLSHoption option,
                                       curl_lock_data param) = 0;
  virtual CURLSHcode curl_share_setopt(CURLSH* share, CURLSHoption option,
                                       void (*param)(CURL*, curl_lock_data,
                                                     curl_lock_access,
                                                     void*)) = 0;
  virtual CURLSHcode curl_share_setopt(CURLSH* share, CURLSHoption option,
                                       void* param) = 0;
  virtual CURLSHcode curl_share_setopt(CURLSH* share, CURLSHoption option,
                                       void (*param)(CURL*, curl_lock_data,
                                                     void*)) = 0;

  virtual void curl_slist_free_all(struct curl_slist* list) = 0;

  virtual struct curl_slist* curl_slist_append(struct curl_slist* list,
                                               const char* data) = 0;
};

class LibCurlProxy : public LibCurl {
 public:
  static std::unique_ptr<LibCurlProxy> CreateInstance();

  CURL* curl_easy_init() override;

  void curl_easy_reset(CURL* curl) override;

  CURLcode curl_easy_setopt(CURL* curl, CURLoption option, bool param) override;

  CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                            uint32_t param) override;

  CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                            uint64_t param) override;

  CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                            curl_off_t param) override;

  CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                            const char* param) override;

  CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                            const std::string& param) override;

  CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                            void* param) override;

  CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                            size_t (*param)(const void*, size_t, size_t,
                                            void*)) override;

  CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                            size_t (*param)(void*, size_t, size_t,
                                            FILE*)) override;
  CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                            int (*param)(void* clientp, curl_off_t dltotal,
                                         curl_off_t dlnow, curl_off_t ultotal,
                                         curl_off_t ulnow)) override;
  CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                            size_t (*param)(char*, size_t, size_t,
                                            void*)) override;

  CURLcode curl_easy_perform(CURL* curl) override;

  CURLcode curl_easy_getinfo(CURL* curl, CURLINFO info,
                             uint64_t* value) override;

  CURLcode curl_easy_getinfo(CURL* curl, CURLINFO info, double* value) override;

  void curl_easy_cleanup(CURL* curl) override;

  void curl_free(void* p) override;

  CURLSH* curl_share_init() override;

  void curl_share_cleanup(CURLSH* share) override;

  CURLSHcode curl_share_setopt(CURLSH* share, CURLSHoption option,
                               curl_lock_data param) override;
  CURLSHcode curl_share_setopt(CURLSH* share, CURLSHoption option,
                               void (*param)(CURL*, curl_lock_data,
                                             curl_lock_access, void*)) override;
  CURLSHcode curl_share_setopt(CURLSH* share, CURLSHoption option,
                               void* param) override;
  CURLSHcode curl_share_setopt(CURLSH* share, CURLSHoption option,
                               void (*param)(CURL*, curl_lock_data,
                                             void*)) override;

  void curl_slist_free_all(struct curl_slist* list) override;

  struct curl_slist* curl_slist_append(struct curl_slist* list,
                                       const char* data) override;
};

// The cURL client is NOT threadsafe as the underlying curl operations modify
// internal connection state in the curl handle.
class CurlHttpClient : public HttpClient {
 public:
  struct Config {
    // These are the values we use to parse to curl_easy_setopt.
    // Defaults are set in this file but can be overwritten by clients.

    // CURLOPT_HTTP_TRANSFER_DECODING
    bool raw = false;
    // CURLOPT_VERBOSE
    bool verbose = false;
    // CURLOPT_DEBUGFUNCTION
    curl_debug_callback verbose_cb = nullptr;
    // CURLOPT_PROXY
    std::string proxy;
    // CURLOPT_TIMEOUT_MS, in milliseconds.
    uint64_t request_timeout_msec = 5000;
    // CURLOPT_CONNECTTIMEOUT_MS, in milliseconds.
    uint64_t connect_timeout_msec = 5000;
    // CURLOPT_DNS_CACHE_TIMEOUT, in sec.
    int dns_timeout = 60;
    // CURLOPT_FOLLOWLOCATION
    bool follow_redirect = true;
    // CURLOPT_MAX_RECV_SPEED_LARGE
    int max_recv_speed = -1;
    // CURLOPT_IPRESOLVE
    HttpClient::Resolver resolver = Resolver::kIPAny;
    // CURLOPT_LOW_SPEED_LIMIT (bytes / sec)
    uint64_t low_speed_limit = 0;  // disabled by default.
    // CURLOPT_LOW_SPEED_TIME in seconds
    uint64_t low_speed_time = 0;  // disabled by default.
  };

  CurlHttpClient(std::unique_ptr<LibCurl> libcurl,
                 std::variant<HttpCredential, TlsCredential> cred);
  CurlHttpClient(std::unique_ptr<LibCurl> libcurl,
                 std::variant<HttpCredential, TlsCredential> cred,
                 Config config);
  ~CurlHttpClient() override;

  CurlHttpClient(const CurlHttpClient&) = delete;
  CurlHttpClient& operator=(const CurlHttpClient&) = delete;
  CurlHttpClient(CurlHttpClient&& other) = delete;
  CurlHttpClient& operator=(CurlHttpClient&& other) = delete;

  absl::StatusOr<HttpResponse> Get(
      std::unique_ptr<HttpRequest> request) override;
  absl::StatusOr<HttpResponse> Post(
      std::unique_ptr<HttpRequest> request) override;
  absl::StatusOr<HttpResponse> Delete(
      std::unique_ptr<HttpRequest> request) override;
  absl::StatusOr<HttpResponse> Patch(
      std::unique_ptr<HttpRequest> request) override;
  absl::StatusOr<HttpResponse> Put(
      std::unique_ptr<HttpRequest> request) override;
  absl::Status GetIncremental(std::unique_ptr<HttpRequest> request,
                              IncrementalResponseHandler* handler) override;
  absl::Status PostIncremental(std::unique_ptr<HttpRequest> request,
                               IncrementalResponseHandler* handler) override;
  absl::Status DeleteIncremental(std::unique_ptr<HttpRequest> request,
                                 IncrementalResponseHandler* handler) override;
  absl::Status PatchIncremental(std::unique_ptr<HttpRequest> request,
                                IncrementalResponseHandler* handler) override;

  Config GetConfig() const { return config_; }

 private:
  absl::StatusOr<HttpResponse> HttpMethod(
      Protocol cmd, std::unique_ptr<HttpRequest> request,
      IncrementalResponseHandler* handler = nullptr);
  void SetDefaultCurlOpts(CURL* curl) const;
  static size_t HeaderCallback(const void* data, size_t size, size_t nmemb,
                               void* userp);
  static size_t BodyCallback(const void* data, size_t size, size_t nmemb,
                             void* userp);
  static int ProgressCallback(void* userp, curl_off_t dltotal, curl_off_t dlnow,
                              curl_off_t ultotal, curl_off_t ulnow);

  // The following mutexes are used to statically lock the CURLSH pointer
  // shared_connection_.
  static absl::Mutex data_share_mutex_;
  static absl::Mutex data_connect_mutex_;

  // Locking and unlocking functions to be passed to Libcurl share interface
  // The parameters are required by share interface but are unused in the actual
  // function definition.
  static void LockSharedMutex(CURL* handle, curl_lock_data data,
                              curl_lock_access laccess, void* useptr);
  static void UnlockSharedMutex(CURL* handle, curl_lock_data data,
                                void* useptr);

  std::unique_ptr<LibCurl> libcurl_;
  Config config_;
  // Credentials can be either HTTP or mTLS auth.
  const std::variant<HttpCredential, TlsCredential> cred_;

  // CURL share interface (https://curl.se/libcurl/c/libcurl-share.html)
  // The share interface lets us capitalize on the fact that we're connecting to
  // the same endpoints. By doing so, we can save CPU usage on setting and
  // finding connections (e.g. TCP handshake)
  CURLSH* shared_connection_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_HTTP_CURL_CLIENT_H_
