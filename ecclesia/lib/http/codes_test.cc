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

#include "ecclesia/lib/http/codes.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"

namespace ecclesia {
namespace {

using ::testing::Eq;
using ::testing::Ne;

TEST(StatusCode, AllResponseCodesHaveMessage) {
  // HTTP Status codes are in the 1XX to 5XX range
  // https://en.wikipedia.org/wiki/List_of_HTTP_status_codes#5xx_server_errors
  for (int i = 100; i <= 599; ++i) {
    HttpResponseCode code = HttpResponseCodeFromInt(i);
    if (code == HTTP_CODE_UNDEFINED || code == HTTP_CODE_ERROR) {
      EXPECT_THAT(HttpResponseCodeToReasonPhrase(code),
                  Eq("Internal Server Error"))
          << "HTTP Code: " << i;
    } else {
      EXPECT_THAT(HttpResponseCodeToReasonPhrase(code),
                  Ne("Internal Server Error"))
          << "HTTP Code: " << i;
    }
  }
}

TEST(StatusCode, All200sOk) {
  for (int i = 200; i <= 299; ++i) {
    HttpResponseCode code = HttpResponseCodeFromInt(i);
    if (code == HTTP_CODE_UNDEFINED) continue;
    absl::StatusCode status = HttpResponseCodeToCanonical(code);
    EXPECT_THAT(status, Eq(absl::StatusCode::kOk)) << "Code " << i;
  }
}

TEST(StatusCode, All3xx4xx5xxError) {
  for (int i = 300; i <= 599; ++i) {
    HttpResponseCode code = HttpResponseCodeFromInt(i);
    if (code == HTTP_CODE_UNDEFINED) continue;
    absl::StatusCode status = HttpResponseCodeToCanonical(code);
    EXPECT_THAT(status, Ne(absl::StatusCode::kOk)) << "Code " << i;
  }
}

}  // namespace
}  // namespace ecclesia
