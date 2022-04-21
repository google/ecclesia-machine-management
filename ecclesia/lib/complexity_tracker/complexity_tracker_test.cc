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
              (override));
  MOCK_METHOD(void, ReportContextResult, (const ApiComplexityContext& context),
              (override));
};

TEST(TestApiComplexityContextManager, TestDefaultContextManager) {
  ApiComplexityContextManager manager;
  EXPECT_THAT(manager.GetContext(), ecclesia::IsStatusInternal());
  // For default context manager it is expected that all the APIs below are
  // no-op but should not crash the binary.
  auto report_on_destroy = manager.PrepareForInboundApi("");
  manager.RecordDownstreamCall(ApiComplexityContext::CallType::kCachedRedfish);
}

TEST(TestApiComplexityContextManager, TestNoReportOnDestroy) {
  ApiComplexityContextManager manager;
  EXPECT_THAT(manager.GetContext(), ecclesia::IsStatusInternal());
  // For default context manager it is expected that all the APIs below are
  // no-op but should not crash the binary.
  manager.PrepareForInboundApi("").CancelAutoreport();
  manager.RecordDownstreamCall(ApiComplexityContext::CallType::kCachedRedfish);
  manager.ReportContextResult();
}

TEST(TestApiComplexityContextManager, TestManagerOverride) {
  ApiComplexityContext context;
  auto impl = absl::make_unique<TestApiComplexityContextManagerImpl>();
  EXPECT_CALL(*impl, GetContext).WillOnce(Return(&context));
  ApiComplexityContextManager manager(std::move(impl));
  EXPECT_THAT(manager.GetContext(), ecclesia::IsOkAndHolds(&context));
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

  ApiComplexityContextManager manager(std::move(impl));
  manager.ReportContextResult();
  manager.ReportContextResult(context_as_a_parameter);
}

TEST(TestApiComplexityContextManager, ReportOnDestroy) {
  ApiComplexityContext default_context;
  auto impl = absl::make_unique<TestApiComplexityContextManagerImpl>();
  // When ReportContextResult is called using nullptr it should pull the context
  // using GetContext method
  EXPECT_CALL(*impl, GetContext).WillOnce(Return(&default_context));
  EXPECT_CALL(*impl, ReportContextResult).Times(1);

  ApiComplexityContextManager manager(std::move(impl));
  {
    // Get reporter on destroy
    ApiComplexityContextManager::ReportOnDestroy reporter =
        manager.PrepareForInboundApi("any");
    // Additionally test assignment with move
    ApiComplexityContextManager::ReportOnDestroy assigned_reported =
        std::move(reporter);
  }
}

TEST(TestApiComplexityContextManager, PrepareForInboundApi) {
  ApiComplexityContext context;
  auto impl = absl::make_unique<TestApiComplexityContextManagerImpl>();
  // When ReportContextResult is called using nullptr it should pull the context
  // using GetContext method
  EXPECT_CALL(*impl, GetContext).WillRepeatedly(Return(&context));

  ApiComplexityContextManager manager(std::move(impl));

  manager.RecordDownstreamCall(ApiComplexityContext::CallType::kCachedRedfish);
  manager.RecordDownstreamCall(ApiComplexityContext::CallType::kCachedRedfish);
  manager.RecordDownstreamCall(ApiComplexityContext::CallType::kCachedRedfish);
  manager.RecordDownstreamCall(
      ApiComplexityContext::CallType::kUncachedRedfish);
  manager.RecordDownstreamCall(
      ApiComplexityContext::CallType::kUncachedRedfish);
  manager.RecordDownstreamCall(ApiComplexityContext::CallType::kUncachedGsys);
  EXPECT_EQ(context.cached_redfish_calls(), 3);
  EXPECT_EQ(context.uncached_redfish_calls(), 2);
  EXPECT_EQ(context.uncached_gsys_calls(), 1);

  manager.PrepareForInboundApi("test").CancelAutoreport();
  EXPECT_EQ(context.api_name(), "test");
  EXPECT_EQ(context.cached_redfish_calls(), 0);
  EXPECT_EQ(context.uncached_redfish_calls(), 0);
  EXPECT_EQ(context.uncached_gsys_calls(), 0);
}

}  // namespace
}  // namespace ecclesia
