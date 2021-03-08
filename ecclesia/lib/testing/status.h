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

// Reusable matchers for unit testing. Useful for matching against absl::Status
// and absl::StatusOr objects in a simpler way. Provides several matchers:
//   * IsOk() -> matches a Status or StatusOr<T> where .ok() is true
//   * IsOkAndHolds(m) -> matches a StatusOr<T> value where .ok() is true and
//     the contained value matches matcher m.
//   * IsStatusXxx() -> matches a Status or StatusOr<T> where the status is
//     not OK and matches a particular code (e.g. IsStatusUnknown will match
//     if the status code is StatusCode::kUnknown).

#ifndef ECCLESIA_LIB_TESTING_STATUS_H_
#define ECCLESIA_LIB_TESTING_STATUS_H_

#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace ecclesia {
namespace internal_status {

// Monomorphic implementation of matcher IsOk() and IsStatusXxx() for a given
// type T. T can be Status, StatusOr<>, or a reference to either of them.
template <typename StatusType>
class IsStatusMonoMatcher : public ::testing::MatcherInterface<StatusType> {
 public:
  explicit IsStatusMonoMatcher(absl::StatusCode code) : code_(code) {}

  void DescribeTo(std::ostream *os) const override {
    *os << "is " << absl::StatusCodeToString(code_);
  }
  void DescribeNegationTo(std::ostream *os) const override {
    *os << "is not " << absl::StatusCodeToString(code_);
  }
  bool MatchAndExplain(StatusType actual_value,
                       ::testing::MatchResultListener *) const override {
    return GetStatus(actual_value).code() == code_;
  }

 private:
  // Overloads to extract the Status from the stored types, whether it is a
  // Status or a StatusOr.
  static const absl::Status &GetStatus(const absl::Status &status) {
    return status;
  }
  template <typename T>
  static const absl::Status &GetStatus(const absl::StatusOr<T> &statusor) {
    return statusor.status();
  }

  absl::StatusCode code_;
};

// Implements IsOk() as a polymorphic matcher.
class IsStatusPolyMatcher {
 public:
  explicit IsStatusPolyMatcher(absl::StatusCode code) : code_(code) {}

  template <typename StatusType>
  operator ::testing::Matcher<StatusType>() const {
    return ::testing::Matcher<StatusType>(
        new IsStatusMonoMatcher<StatusType>(code_));
  }

 private:
  absl::StatusCode code_;
};

// Monomorphic implementation of matcher IsOkAndHolds(m). StatusOrType is a
// reference to StatusOr<T>.
template <typename StatusOrType>
class IsOkAndHoldsMonoMatcher
    : public ::testing::MatcherInterface<StatusOrType> {
 public:
  // The stored value type of the wrapper.
  using value_type =
      typename std::remove_reference<StatusOrType>::type::value_type;

  template <typename InnerMatcher>
  explicit IsOkAndHoldsMonoMatcher(InnerMatcher &&inner_matcher)
      : inner_matcher_(::testing::SafeMatcherCast<const value_type &>(
            std::forward<InnerMatcher>(inner_matcher))) {}

  void DescribeTo(std::ostream *os) const override {
    *os << "is OK and has a value that ";
    inner_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream *os) const override {
    *os << "isn't OK or has a value that ";
    inner_matcher_.DescribeNegationTo(os);
  }

  bool MatchAndExplain(
      StatusOrType actual_value,
      ::testing::MatchResultListener *result_listener) const override {
    if (!actual_value.ok()) {
      *result_listener << "which has status " << actual_value.status();
      return false;
    }

    ::testing::StringMatchResultListener inner_listener;
    const bool matches =
        inner_matcher_.MatchAndExplain(*actual_value, &inner_listener);
    const std::string inner_explanation = inner_listener.str();
    if (!inner_explanation.empty()) {
      *result_listener << "which contains value "
                       << ::testing::PrintToString(*actual_value) << ", "
                       << inner_explanation;
    }
    return matches;
  }

 private:
  const ::testing::Matcher<const value_type &> inner_matcher_;
};

// Implements IsOkAndHolds(m) as a polymorphic matcher.
template <typename InnerMatcher>
class IsOkAndHoldsPolyMatcher {
 public:
  explicit IsOkAndHoldsPolyMatcher(InnerMatcher inner_matcher)
      : inner_matcher_(std::move(inner_matcher)) {}

  // Converts this polymorphic matcher to a monomorphic matcher of the
  // given type.  StatusOrType can be either StatusOr<T> or a
  // reference to StatusOr<T>.
  template <typename StatusOrType>
  operator ::testing::Matcher<StatusOrType>() const {
    return ::testing::Matcher<StatusOrType>(
        new IsOkAndHoldsMonoMatcher<const StatusOrType &>(inner_matcher_));
  }

 private:
  const InnerMatcher inner_matcher_;
};

}  // namespace internal_status

// Creates an IsOk matcher.
inline internal_status::IsStatusPolyMatcher IsOk() {
  return internal_status::IsStatusPolyMatcher(absl::StatusCode::kOk);
}

// Creates an IsOkAndHolds matcher.
template <typename InnerMatcher>
internal_status::IsOkAndHoldsPolyMatcher<
    typename std::decay<InnerMatcher>::type>
IsOkAndHolds(InnerMatcher &&inner_matcher) {
  return internal_status::IsOkAndHoldsPolyMatcher<
      typename std::decay<InnerMatcher>::type>(
      std::forward<InnerMatcher>(inner_matcher));
}

// Creates matchers for each status code.
inline internal_status::IsStatusPolyMatcher IsStatusAborted() {
  return internal_status::IsStatusPolyMatcher(absl::StatusCode::kAborted);
}
inline internal_status::IsStatusPolyMatcher IsStatusAlreadyExists() {
  return internal_status::IsStatusPolyMatcher(absl::StatusCode::kAlreadyExists);
}
inline internal_status::IsStatusPolyMatcher IsStatusCancelled() {
  return internal_status::IsStatusPolyMatcher(absl::StatusCode::kCancelled);
}
inline internal_status::IsStatusPolyMatcher IsStatusDataLoss() {
  return internal_status::IsStatusPolyMatcher(absl::StatusCode::kDataLoss);
}
inline internal_status::IsStatusPolyMatcher IsStatusDeadlineExceeded() {
  return internal_status::IsStatusPolyMatcher(
      absl::StatusCode::kDeadlineExceeded);
}
inline internal_status::IsStatusPolyMatcher IsStatusFailedPrecondition() {
  return internal_status::IsStatusPolyMatcher(
      absl::StatusCode::kFailedPrecondition);
}
inline internal_status::IsStatusPolyMatcher IsStatusInternal() {
  return internal_status::IsStatusPolyMatcher(absl::StatusCode::kInternal);
}
inline internal_status::IsStatusPolyMatcher IsStatusInvalidArgument() {
  return internal_status::IsStatusPolyMatcher(
      absl::StatusCode::kInvalidArgument);
}
inline internal_status::IsStatusPolyMatcher IsStatusNotFound() {
  return internal_status::IsStatusPolyMatcher(absl::StatusCode::kNotFound);
}
inline internal_status::IsStatusPolyMatcher IsStatusOutOfRange() {
  return internal_status::IsStatusPolyMatcher(absl::StatusCode::kOutOfRange);
}
inline internal_status::IsStatusPolyMatcher IsStatusPermissionDenied() {
  return internal_status::IsStatusPolyMatcher(
      absl::StatusCode::kPermissionDenied);
}
inline internal_status::IsStatusPolyMatcher IsStatusResourceExhausted() {
  return internal_status::IsStatusPolyMatcher(
      absl::StatusCode::kResourceExhausted);
}
inline internal_status::IsStatusPolyMatcher IsStatusUnauthenticated() {
  return internal_status::IsStatusPolyMatcher(
      absl::StatusCode::kUnauthenticated);
}
inline internal_status::IsStatusPolyMatcher IsStatusUnavailable() {
  return internal_status::IsStatusPolyMatcher(absl::StatusCode::kUnavailable);
}
inline internal_status::IsStatusPolyMatcher IsStatusUnimplemented() {
  return internal_status::IsStatusPolyMatcher(absl::StatusCode::kUnimplemented);
}
inline internal_status::IsStatusPolyMatcher IsStatusUnknown() {
  return internal_status::IsStatusPolyMatcher(absl::StatusCode::kUnknown);
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_TESTING_STATUS_H_
