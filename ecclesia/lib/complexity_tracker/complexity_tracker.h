/*
 * Copyright 2022 Google LLC
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

#ifndef ECCLESIA_LIB_COMPLEXITY_TRACKER_COMPLEXITY_TRACKER_H_
#define ECCLESIA_LIB_COMPLEXITY_TRACKER_COMPLEXITY_TRACKER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"

namespace ecclesia {

// Class is used to track how many outbound calls are made per every inbound
// call. Cached and uncached calls are tracked separately. Objects are
// instantiated and returned by ApiComplexityContextManagerInterface interface
// implementations. Instances are supposed to have a lifetime equals/longer
// than inbound API processing time.
// The primary idea is to have ApiComplexityContext as a thread local global
// variable with an expectation that RPC server processes APIs using thread
// pools libraries. See ApiComplexityContextManagerInterface description for
// more details.
class ApiComplexityContext {
 public:
  enum class CallType { kCachedRedfish, kUncachedRedfish, kUncachedGsys };

  // Resets context for new inbound API processing has started. Inbound API
  // handlers need to call this method in order to reset counters.
  // Why?
  //  Sometimes the same thread can be re-used without destroying for multiple
  // APIs. The result is the same ApiComplexityContext instance to be used.
  void PrepareForInboundApi(std::string name) {
    api_name_ = std::move(name);
    uncached_redfish_calls_ = 0;
    cached_redfish_calls_ = 0;
    uncached_gsys_calls_ = 0;
  }
  // Increments outbound calls counter.
  void RecordDownstreamCall(CallType call_type) {
    switch (call_type) {
      case CallType::kUncachedRedfish:
        uncached_redfish_calls_++;
        break;
      case CallType::kCachedRedfish:
        cached_redfish_calls_++;
        break;
      case CallType::kUncachedGsys:
        uncached_gsys_calls_++;
        break;
    }
  }

  // accessors for cached/uncached counters and current API name
  const uint64_t uncached_redfish_calls() const {
    return uncached_redfish_calls_;
  }
  const uint64_t cached_redfish_calls() const { return cached_redfish_calls_; }
  const uint64_t uncached_gsys_calls() const { return uncached_gsys_calls_; }

  absl::string_view api_name() const { return api_name_; }

 private:
  std::string api_name_ = "";
  uint64_t uncached_redfish_calls_ = 0;
  uint64_t cached_redfish_calls_ = 0;
  uint64_t uncached_gsys_calls_ = 0;
};

// Defines an API to create/retrieve/change ApiComplexityContext.
// Implementation depends on the server threading model. The purpose is to avoid
// passing the tramp_data pattern from the API handler to the code that invokes
// the downstream clients. And use GetContext() method that returns
// 'per handler' context.
// The implementation requirements are:
//  - GetContext method must return a pointer to the same context for the
//    duration of the API call processing.
//  - GetContext method must return pointers to different contexts if there
//    are two inbound APIs that are processed in parallel. Each processing
//    thread should use its own context object.
// An example:
// For the RPC server that uses thread per API handler the implementation can
// use thread local context variable.
class ApiComplexityContextManager {
 public:
  class ImplInterface {
   public:
    virtual ~ImplInterface() = default;
    virtual absl::StatusOr<ApiComplexityContext*> GetContext() = 0;
    virtual void ReportContextResult(const ApiComplexityContext& context) = 0;
  };
  // Class is used as a return value from PrepareForInboundApi method. It
  // simplifies results reporting using "report on destroy" approach.
  //
  // Usage to report complexity metric when report_on_destroy variable leaves a
  // scope:
  // {
  //    ReportOnDestroy report_on_destroy =
  //         manager.PrepareForInboundApi("api_name");
  //    .....
  //    // code that calls redfish/other clients
  //    .....
  // } // At this point result are automatically reported using ImplInterface
  //
  // Usage for manual usage reporing:
  //    manager.PrepareForInboundApi("api_name").CancelAutoreport();
  //    .....
  //    // code that calls redfish/other clients
  //    .....
  //    manager.ReportContextResult();

  class ReportOnDestroy {
   public:
    ReportOnDestroy(const ApiComplexityContextManager* manager,
                    const ApiComplexityContext* context)
        : manager_(*manager), context_(context) {}

    // copy and assignment are not allowed
    ReportOnDestroy(const ReportOnDestroy&) = delete;
    ReportOnDestroy& operator=(const ReportOnDestroy&) = delete;

    // Move forces other instance to 'forget' reporting on destroy
    ReportOnDestroy(ReportOnDestroy&& other)
        : manager_(other.manager_), context_(other.context_) {
      other.report_on_destroy = false;
    }

    ReportOnDestroy& operator=(ReportOnDestroy&& other) {
      manager_ = other.manager_;
      context_ = other.context_;
      report_on_destroy = other.report_on_destroy;
      other.report_on_destroy = false;
      return *this;
    }

    // Allows unused result
    void CancelAutoreport() { report_on_destroy = false; }

    // On destroy delete context
    ~ReportOnDestroy() {
      if (context_ != nullptr && report_on_destroy) {
        manager_.get().ReportContextResult(*context_);
      }
    }

   private:
    bool report_on_destroy = true;
    std::reference_wrapper<const ApiComplexityContextManager> manager_;
    const ApiComplexityContext* context_;
  };

  ApiComplexityContextManager();
  ApiComplexityContextManager(std::unique_ptr<ImplInterface> impl);

  // Returns context local to a thread/fiber. It should return the same pointer
  // when called multiple times from the same handler, regardless of the stack
  // frame it is called from.
  absl::StatusOr<ApiComplexityContext*> GetContext() const {
    return impl_->GetContext();
  }

  // Reports values stored in context. Reporting destination depends on the
  // implementation. In this case GetContext method is used to get the context
  // object to report.
  void ReportContextResult(const ApiComplexityContext& context) const {
    return impl_->ReportContextResult(context);
  }

  // Reports context returned by GetContext method.
  void ReportContextResult() const {
    auto context = impl_->GetContext();
    if (context.ok() && *context != nullptr) {
      impl_->ReportContextResult(**context);
    }
  }

  // Resets counters to track a new API.
  // Why: Threading implementation can reuse threads and that can lead to the
  // stale context.
  [[nodiscard]] ReportOnDestroy PrepareForInboundApi(std::string name) const;

  // Increments downstream call counter depending on the call type.
  void RecordDownstreamCall(ApiComplexityContext::CallType call_type) const;

 private:
  std::unique_ptr<ImplInterface> impl_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_COMPLEXITY_TRACKER_COMPLEXITY_TRACKER_H_
