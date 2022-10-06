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

#include "ecclesia/lib/complexity_tracker/complexity_tracker.h"

#include <memory>

#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"

namespace ecclesia {

namespace {

class NullApiComplexityContextManagerImpl
    : public ApiComplexityContextManager::ImplInterface {
 public:
  absl::StatusOr<ApiComplexityContext *> GetContext() override {
    return absl::InternalError(
        "Method is not implemented for NullApiComplexityContextManager");
  }
  void ReportContextResult(const ApiComplexityContext &context) override {}
};

}  // namespace

ApiComplexityContextManager::ApiComplexityContextManager()
    : impl_(absl::make_unique<NullApiComplexityContextManagerImpl>()) {}

ApiComplexityContextManager::ApiComplexityContextManager(
    std::unique_ptr<ApiComplexityContextManager::ImplInterface> impl)
    : impl_(std::move(impl)) {}

ApiComplexityContextManager::ReportOnDestroy
ApiComplexityContextManager::PrepareForInboundApi(std::string name) const {
  absl::StatusOr<ApiComplexityContext *> context = impl_->GetContext();
  if (context.ok() && *context != nullptr) {
    (*context)->PrepareForInboundApi(std::move(name));
    return ReportOnDestroy(this, *context);
  }
  return ReportOnDestroy(this, nullptr);
}

void ApiComplexityContextManager::RecordDownstreamCall(
    ApiComplexityContext::CallType call_type) const {
  absl::StatusOr<ApiComplexityContext *> context = impl_->GetContext();
  if (context.ok() && *context != nullptr) {
    (*context)->RecordDownstreamCall(call_type);
  }
}

}  // namespace ecclesia
