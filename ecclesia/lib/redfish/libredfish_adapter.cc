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

#include "ecclesia/lib/redfish/libredfish_adapter.h"

#include <cstddef>
#include <cstdlib>
#include <deque>
#include <memory>
#include <pthread.h>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/redfish/interface.h"

extern "C" {
#include "redfishPayload.h"
#include "redfishRawAsync.h"
#include "redfishService.h"
}  // extern "C"

namespace libredfish {
namespace {

struct RequestInfo {
  bool terminate;
  asyncHttpRequest* request;
  asyncRawCallback callback;
  void* callback_context;
};

using RequestQueue = std::deque<RequestInfo>;

// Translate the third_party/libredfish serviceHttpHandler API to our
// HttpClient API.
class LibredfishAdapter {
 public:
  LibredfishAdapter(std::unique_ptr<ecclesia::HttpClient> client)
    : client_(std::move(client)) {}

  static bool StartRequestHandler(redfishService* service,
                                  void* handler_context,
                                  asyncHttpRequest* request,
                                  asyncRawCallback callback,
                                  void* callback_context) {
    ecclesia::InfoLog() << "Url: " << request->url;
    auto adapter = reinterpret_cast<LibredfishAdapter*>(handler_context);

    if (!service || !request) {
      ecclesia::ErrorLog() << "service/request are NULL";
      freeAsyncRequest(request);
      return false;
    }

    absl::WriterMutexLock lock(&adapter->queue_mutex_);

    if (!adapter->queue_thread_) {
      int error = pthread_create(&adapter->queue_thread_, nullptr,
                                 &RequestHandlerThread, adapter);
      if (error != 0) {
        ecclesia::ErrorLog() << "Error starting request handler thread: "
                             << strerror(error);
        freeAsyncRequest(request);
        return false;
      }
    }

    adapter->AddToQueue(request, callback, callback_context);
    return true;
  }

  static void Deleter(void* context) {
    auto adapter = reinterpret_cast<LibredfishAdapter*>(context);
    {
      absl::WriterMutexLock lock(&adapter->queue_mutex_);
      adapter->TerminateQueue();
    }
    if (pthread_self() == adapter->queue_thread_) {
      // The main thread left us to clean things up.
      // There's no one left to pthread_join() us, so detach so we can
      // properly exit.
      int error = pthread_detach(pthread_self());
      if (error != 0) {
        ecclesia::FatalLog() << "Error detaching request handler thread: "
                             << strerror(error);
      }
      adapter->self_terminate = true;
    } else {
      if (adapter->queue_thread_) {
        int error = pthread_join(adapter->queue_thread_, nullptr);
        if (error != 0) {
          ecclesia::FatalLog() << "Error joining request handler thread: "
                               << strerror(error);
        }
      }
      delete adapter;
    }
  }

 private:
  static void* RequestHandlerThread(void* data) {
    auto adapter = reinterpret_cast<LibredfishAdapter*>(data);

    for (;;) {
      auto request_info = adapter->WaitForNextRequest();
      if (request_info.terminate) {
        break;
      }
      adapter->ProcessRequest(request_info);
    }

    if (adapter->self_terminate) {
      delete adapter;
    }

    return nullptr;
  }

  void AddToQueue(asyncHttpRequest* request, asyncRawCallback callback,
                  void* callback_context) ABSL_EXCLUSIVE_LOCKS_REQUIRED(queue_mutex_) {
    request_queue_.emplace_back(RequestInfo{
        false, request, callback, callback_context});
  }

  void TerminateQueue() ABSL_EXCLUSIVE_LOCKS_REQUIRED(queue_mutex_) {
    request_queue_.emplace_back(RequestInfo{true, nullptr, nullptr, nullptr});
  }

  RequestInfo WaitForNextRequest() {
    absl::WriterMutexLock lock(
        &queue_mutex_,
        absl::Condition{
          [](void* data) {
            auto queue = reinterpret_cast<RequestQueue*>(data);
            return !queue->empty();
          },
          &request_queue_});
    auto request_info = request_queue_.front();
    request_queue_.pop_front();
    return request_info;
  }

  void ProcessRequest(const RequestInfo& request_info) {
    asyncHttpRequest* request = request_info.request;
    asyncRawCallback callback = request_info.callback;
    void* callback_context = request_info.callback_context;
    asyncHttpResponse* response;

    if (callback) {
      // Use calloc as it's callback's responsibility to free it.
      response = reinterpret_cast<asyncHttpResponse*>(
          calloc(1, sizeof(asyncHttpResponse)));
      if (!response) {
        callback(request, nullptr, callback_context);
        return;
      }
    } else {
      response = nullptr;
    }


    absl::StatusOr<ecclesia::HttpClient::HttpResponse> client_response;

    ecclesia::HttpClient::HttpRequest rqst;
    rqst.uri = request->url;
    rqst.headers = ConvertRequestHeaders(request->headers);
    switch(request->method) {
      case HTTP_GET: {
        client_response = client_->Get(rqst);
        break;
      }
      case HTTP_POST: {
        rqst.body = std::string{request->body, request->bodySize};
        client_response = client_->Post(rqst);
        break;
      }
      default:
        ecclesia::FatalLog() << "Unsupported method: " << request->method;
    }

    if (!callback) {
      freeAsyncRequest(request);
      return;
    }

    if (!client_response.ok()) {
      ecclesia::InfoLog() << "Request failed: " << client_response.status();
      // See rawAsyncWorkThread.
      response->connectError = 1;
      response->httpResponseCode = 0xffff;
      callback(request, response, callback_context);
      return;
    }

    ecclesia::HttpClient::HttpResponse& resp = client_response.value();
    // It is the callback's responsibility to free request, response.
    // is a work-in-progress. E.g., CURLcode or HTTP code? Here it's HTTP code.
    response->httpResponseCode = resp.code;
    if (resp.code != 200) {
      ecclesia::InfoLog() << "Got error code: " << resp.code;
      // See rawAsyncWorkThread.
      response->connectError = 1;
    } else {
      response->connectError = 0;
      size_t size = resp.body.size();
      auto body = reinterpret_cast<char*>(malloc(size + 1));
      if (!body) {
        // Pass nullptr for response to indicate malloc failure.
        callback(request, nullptr, callback_context);
        freeAsyncResponse(response);
        // Since we've already performed the operation, return true not false.
        return;
      }
      memcpy(body, resp.body.c_str(), size + 1);
      response->body = body;
      response->bodySize = size;
      response->headers = ConvertResponseHeaders(client_response->headers);
    }

    callback(request, response, callback_context);
  }

  // Utility to convert libredfish's representation of headers to ours.
  static ecclesia::HttpClient::HttpHeaders ConvertRequestHeaders(
      const httpHeader* in) {
    ecclesia::HttpClient::HttpHeaders out;
    for (const httpHeader* h = in; h != nullptr; h = h->next) {
      out.emplace_back(absl::StrCat(h->name, ": ", h->value));
    }
    return out;
  }

  // Utility to convert our representation of headers to libredfish's.
  static httpHeader* ConvertResponseHeaders(
      const ecclesia::HttpClient::HttpHeaders& in) {
    httpHeader* first = nullptr;
    httpHeader* last = nullptr;

    // For consistency with default handler, ignore malloc failures.
    for (const auto& h : in) {
      const char* text = h.c_str();
      const char* colon = strchr(text, ':');
      if (!colon) {
        // "HTTP/1.0 200 OK" is provided in a header field, but it doesn't
        // follow the "name: value" format. Ignore it to improve the S/N ratio
        // of this warning.
        if (absl::StartsWith(h, "HTTP/")) {
          // ignore
        } else {
          ecclesia::WarningLog() << "Bad header field: " << h;
        }
        continue;
      }
      char* name = strndup(text, colon - text);
      char* value = strdup(colon[1] == ' ' ? colon + 2 : colon + 1);
      auto hdr = reinterpret_cast<httpHeader*>(malloc(sizeof(httpHeader)));
      if (!name || !value || !hdr) {
        free(name);
        free(value);
        free(hdr);
        break;
      }
      hdr->name = name;
      hdr->value = value;
      hdr->next = nullptr;
      if (!first) {
        first = hdr;
      } else {
        last->next = hdr;
      }
      last = hdr;
    }

    return first;
  }

  pthread_t queue_thread_ = 0;
  bool self_terminate = false;
  std::unique_ptr<ecclesia::HttpClient> client_;
  RequestQueue request_queue_ ABSL_GUARDED_BY(queue_mutex_);
  mutable absl::Mutex queue_mutex_;
};

}  // namespace

serviceHttpHandler NewLibredfishAdapter(
    std::unique_ptr<ecclesia::HttpClient> client) {
  serviceHttpHandler h;
  h.start_request_handler = LibredfishAdapter::StartRequestHandler;
  h.cleanup_request_handler_context = LibredfishAdapter::Deleter;
  h.request_handler_context = new LibredfishAdapter(std::move(client));
  return h;
}

}  // namespace libredfish
