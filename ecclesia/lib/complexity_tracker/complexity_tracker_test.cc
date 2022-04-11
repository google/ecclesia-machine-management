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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::testing::Ref;
using ::testing::Return;

class TestApiComplexityContextManagerImpl
    : public ApiComplexityContextManager::ImplInterface {
 public:
  MOCK_METHOD(absl::StatusOr<ApiComplexityContext*>, GetContext, (),
              (const, override));
  MOCK_METHOD(void, ReportContextResult, (const ApiComplexityContext& context),
              (const, override));
};

TEST(TestApiComplexityContextManager, TestDefaultContextManager) {
  auto& manager = ApiComplexityContextManager::GetGlobalInstance();
  EXPECT_THAT(manager.GetContext(), ecclesia::IsStatusInternal());
  // For default context manager it is expected that all the APIs below are
  // no-op but should not crash the binary.
  auto report_on_destroy = manager.PrepareForInboundApi("");
  manager.RecordDownstreamCall(ApiComplexityContext::CallType::kCached);
}

TEST(TestApiComplexityContextManager, TestNoReportOnDestroy) {
  auto& manager = ApiComplexityContextManager::GetGlobalInstance();
  EXPECT_THAT(manager.GetContext(), ecclesia::IsStatusInternal());
  // For default context manager it is expected that all the APIs below are
  // no-op but should not crash the binary.
  manager.PrepareForInboundApi("").CancelAutoreport();
  manager.RecordDownstreamCall(ApiComplexityContext::CallType::kCached);
  manager.ReportContextResult();
}

TEST(TestApiComplexityContextManager, TestManagerOverride) {
  ApiComplexityContext context;
  auto impl = absl::make_unique<TestApiComplexityContextManagerImpl>();
  EXPECT_CALL(*impl, GetContext).WillOnce(Return(&context));
  auto& manager = ApiComplexityContextManager::GetGlobalInstance();
  manager.SetImplementation(std::move(impl));
  EXPECT_THAT(manager.GetContext(), ecclesia::IsOkAndHolds(&context));
  // Implementation mocks will not be verified unless we delete it
  manager.SetImplementation(nullptr);
}

TEST(TestApiComplexityContextManager, TestReportContextResult) {
  ApiComplexityContext default_context;
  ApiComplexityContext context_as_a_parameter;
  auto impl = absl::make_unique<TestApiComplexityContextManagerImpl>();
  // When ReportContextResult is called using nullptr it should pull the context
  // using GetContext method
  EXPECT_CALL(*impl, GetContext).WillOnce(Return(&default_context));
  EXPECT_CALL(*impl, ReportContextResult(Ref(default_context))).Times(1);
  // We do not expect extra GetContext when real context is sent to
  // ReportContextResult
  EXPECT_CALL(*impl, ReportContextResult(Ref(context_as_a_parameter))).Times(1);

  auto& manager = ApiComplexityContextManager::GetGlobalInstance();
  manager.SetImplementation(std::move(impl));
  manager.ReportContextResult();
  manager.ReportContextResult(context_as_a_parameter);
  // Implementation mocks will not be verified unless we delete it
  manager.SetImplementation(nullptr);
}

TEST(TestApiComplexityContextManager, ReportOnDestroy) {
  ApiComplexityContext default_context;
  auto impl = absl::make_unique<TestApiComplexityContextManagerImpl>();
  // When ReportContextResult is called using nullptr it should pull the context
  // using GetContext method
  EXPECT_CALL(*impl, GetContext).WillOnce(Return(&default_context));
  EXPECT_CALL(*impl, ReportContextResult).Times(1);

  auto& manager = ApiComplexityContextManager::GetGlobalInstance();
  manager.SetImplementation(std::move(impl));
  {
    // Get reporter on destroy
    ApiComplexityContextManager::ReportOnDestroy reporter =
        manager.PrepareForInboundApi("any");
    // Additionally test assignment with move
    ApiComplexityContextManager::ReportOnDestroy assigned_reported =
        std::move(reporter);
  }
  // Implementation mocks will not be verified unless we delete it
  manager.SetImplementation(nullptr);
}

TEST(TestApiComplexityContextManager, PrepareForInboundApi) {
  ApiComplexityContext context;
  auto impl = absl::make_unique<TestApiComplexityContextManagerImpl>();
  // When ReportContextResult is called using nullptr it should pull the context
  // using GetContext method
  EXPECT_CALL(*impl, GetContext).WillRepeatedly(Return(&context));

  auto& manager = ApiComplexityContextManager::GetGlobalInstance();
  manager.SetImplementation(std::move(impl));

  manager.RecordDownstreamCall(ApiComplexityContext::CallType::kCached);
  manager.RecordDownstreamCall(ApiComplexityContext::CallType::kCached);
  manager.RecordDownstreamCall(ApiComplexityContext::CallType::kUncached);
  EXPECT_EQ(context.cached_calls(), 2);
  EXPECT_EQ(context.uncached_calls(), 1);

  manager.PrepareForInboundApi("test").CancelAutoreport();
  EXPECT_EQ(context.api_name(), "test");
  EXPECT_EQ(context.cached_calls(), 0);
  EXPECT_EQ(context.uncached_calls(), 0);
  // Implementation mocks will not be verified unless we delete it
  manager.SetImplementation(nullptr);
}

}  // namespace
}  // namespace ecclesia
