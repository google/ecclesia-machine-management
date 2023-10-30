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

#include "ecclesia/lib/redfish/health_rollup.h"

#include <memory>
#include <optional>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/testing/json_mockup.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

TEST(HealthRollup, NoHealthRollupProperty) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
    "Status": {
      "State": "Enabled",
      "Health": "OK",
      "HealthRollup": "OK"
    }
  })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  absl::StatusOr<HealthRollup> health_rollup = ExtractHealthRollup(*obj);
  ASSERT_TRUE(health_rollup.status().ok());
  EXPECT_THAT(*health_rollup, EqualsProto(HealthRollup()));
}

TEST(HealthRollup, HealthRollupNotCriticalOrWarning) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
    "Status": {
      "State": "Enabled",
      "Health": "OK"
    },
    "Conditions": [
      {
        "MessageId": "ResourceEvent.1.0.ResourceErrorsDetected",
        "Message": "The resource property Foo has detected errors of type NoError.",
        "MessageArgs": ["Foo", "NoError"],
        "Timestamp": "1999-12-31T23:59:59Z",
        "Severity": "Ignored"
      }
    ]
  })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  absl::StatusOr<HealthRollup> health_rollup = ExtractHealthRollup(*obj);
  ASSERT_TRUE(health_rollup.status().ok());
  EXPECT_THAT(*health_rollup, EqualsProto(HealthRollup()));
}

TEST(HealthRollup, HealthRollupNotCriticalOrWarningAcceptOKStatus) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
    "Status": {
      "State": "Enabled",
      "Health": "OK",
      "HealthRollup": "OK",
      "Conditions": [
        {
          "MessageId": "ResourceEvent.1.0.ResourceErrorsDetected",
          "Message": "The resource property Foo has detected errors of type Bar.",
          "MessageArgs": ["Foo", "Bar"],
          "Timestamp": "2022-1-23T12:34:56Z",
          "Severity": "Critical"
        }
      ]
    }
  })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  absl::StatusOr<HealthRollup> health_rollup = ExtractHealthRollup(
      *obj, [](const RedfishObject &) { return std::nullopt; },
      /*parse_with_ok_health=*/true);
  ASSERT_TRUE(health_rollup.status().ok());
  EXPECT_THAT(*health_rollup,
              EqualsProto(R"pb(resource_events {
                                 errors_detected {
                                   resource_identifier: "Foo"
                                   error_type: "Bar"
                                 }
                                 severity: "Critical"
                                 timestamp { seconds: 1642941296 }
                               })pb"));
}

TEST(HealthRollup, HealthRollupPresentButNoConditions) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
    "Status": {
      "State": "Enabled",
      "Health": "Critical",
      "HealthRollup": "Critical"
    }
  })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  absl::StatusOr<HealthRollup> health_rollup = ExtractHealthRollup(*obj);
  EXPECT_THAT(health_rollup.status(), IsStatusFailedPrecondition());
}

TEST(HealthRollup, SingleResourceErrorsEvent) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
    "Status": {
      "State": "Enabled",
      "Health": "OK",
      "HealthRollup": "Critical",
      "Conditions": [
        {
          "MessageId": "ResourceEvent.1.0.ResourceErrorsDetected",
          "Message": "The resource property Foo has detected errors of type Bar.",
          "MessageArgs": ["Foo", "Bar"],
          "Timestamp": "2022-1-23T12:34:56Z",
          "Severity": "Critical"
        }
      ]
    }
  })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  absl::StatusOr<HealthRollup> health_rollup = ExtractHealthRollup(*obj);
  ASSERT_TRUE(health_rollup.status().ok());
  EXPECT_THAT(*health_rollup,
              EqualsProto(R"pb(resource_events {
                                 errors_detected {
                                   resource_identifier: "Foo"
                                   error_type: "Bar"
                                 }
                                 severity: "Critical"
                                 timestamp { seconds: 1642941296 }
                               })pb"));
}

TEST(HealthRollup, SeverityAndTimestampOnlySetIfPresent) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
    "Status": {
      "State": "Enabled",
      "Health": "Critical",
      "HealthRollup": "Critical",
      "Conditions": [
        {
          "MessageId": "ResourceEvent.1.0.ResourceErrorsDetected",
          "Message": "The resource property Foo has detected errors of type Bar.",
          "MessageArgs": ["Foo", "Bar"]
        }
      ]
    }
  })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  absl::StatusOr<HealthRollup> health_rollup = ExtractHealthRollup(*obj);
  ASSERT_TRUE(health_rollup.status().ok());
  EXPECT_THAT(*health_rollup, EqualsProto(R"pb(resource_events {
                                                 errors_detected {
                                                   resource_identifier: "Foo"
                                                   error_type: "Bar"
                                                 }
                                               })pb"));
}

TEST(HealthRollup, UnrecognizedResourceEventType) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
    "Status": {
      "State": "Enabled",
      "Health": "Critical",
      "HealthRollup": "Critical",
      "Conditions": [
        {
          "MessageId": "ResourceEvent.1.0.UnsupportedEventType",
          "Message": "The resource property Foo has an unsupported event message of type Bar with additional arg 3.14159.",
          "MessageArgs": ["Foo", "Bar"]
        }
      ]
    }
  })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  absl::StatusOr<HealthRollup> health_rollup = ExtractHealthRollup(*obj);
  ASSERT_TRUE(health_rollup.status().ok());
  EXPECT_THAT(*health_rollup, EqualsProto(R"pb()pb"));
}

TEST(HealthRollup, UnrecognizedMessageTypeFormat) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
    "Status": {
      "State": "Enabled",
      "Health": "Critical",
      "HealthRollup": "Critical",
      "Conditions": [
        {
          "MessageId": "ResourceEvent.1.0.Too.Many.Fields.ResourceErrorsDetected",
          "Message": "The resource property Foo has detected errors of type Bar.",
          "MessageArgs": ["Foo", "Bar"]
        }
      ]
    }
  })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  absl::StatusOr<HealthRollup> health_rollup = ExtractHealthRollup(*obj);
  EXPECT_THAT(health_rollup.status(), IsStatusInternal());
}

TEST(HealthRollup, UnsupportedNumberOfMessageArgsTooFew) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
    "Status": {
      "State": "Enabled",
      "Health": "Critical",
      "HealthRollup": "Critical",
      "Conditions": [
        {
          "MessageId": "ResourceEvent.1.0.ResourceErrorsDetected",
          "Message": "The resource property Foo has an unsupported event message of type Bar with additional arg 3.14159.",
          "MessageArgs": ["Foo"]
        }
      ]
    }
  })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  absl::StatusOr<HealthRollup> health_rollup = ExtractHealthRollup(*obj);
  ASSERT_TRUE(health_rollup.status().ok());
  EXPECT_THAT(*health_rollup, EqualsProto(R"pb()pb"));
}

TEST(HealthRollup, UnsupportedNumberOfMessageArgsTooMany) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
    "Status": {
      "State": "Enabled",
      "Health": "Critical",
      "HealthRollup": "Critical",
      "Conditions": [
        {
          "MessageId": "ResourceEvent.1.0.ResourceErrorsDetected",
          "Message": "The resource property Foo has an unsupported event message of type Bar with additional arg 3.14159.",
          "MessageArgs": ["Foo", "Bar", "Extra", "Superfluous", "Supernumerary"]
        },
        {
          "MessageId": "ResourceEvent.1.0.ResourceErrorsDetected",
          "Message": "The resource property Baz has detected errors of type Qux.",
          "MessageArgs": ["Baz", "Qux"]
        }
      ]
    }
  })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  // Too many should just ignore the extra fields.
  absl::StatusOr<HealthRollup> health_rollup = ExtractHealthRollup(*obj);
  ASSERT_TRUE(health_rollup.status().ok());
  EXPECT_THAT(*health_rollup, EqualsProto(R"pb(resource_events {
                                                 errors_detected {
                                                   resource_identifier: "Foo"
                                                   error_type: "Bar"
                                                 }
                                               }
                                               resource_events {
                                                 errors_detected {
                                                   resource_identifier: "Baz"
                                                   error_type: "Qux"
                                                 }
                                               })pb"));
}

TEST(HealthRollup, SingleStateChangeEvent) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
    "Status": {
      "State": "Enabled",
      "Health": "Critical",
      "HealthRollup": "Critical",
      "Conditions": [
        {
          "MessageId": "ResourceEvent.1.0.ResourceStateChanged",
          "Message": "The state of resource Foo has changed to Bar.",
          "MessageArgs": ["Foo", "Bar"],
          "Timestamp": "2009-2-13T23:31:30Z",
          "Severity": "Warning"
        }
      ]
    }
  })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  absl::StatusOr<HealthRollup> health_rollup = ExtractHealthRollup(*obj);
  ASSERT_TRUE(health_rollup.status().ok());
  EXPECT_THAT(
      *health_rollup,
      EqualsProto(
          R"pb(resource_events {
                 state_change { resource_identifier: "Foo" state_change: "Bar" }
                 severity: "Warning"
                 timestamp { seconds: 1234567890 }
               })pb"));
}

TEST(HealthRollup, SingleThresholdExceeded) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
    "Status": {
      "State": "Enabled",
      "Health": "OK",
      "HealthRollup": "Critical",
      "Conditions": [
        {
          "MessageId": "ResourceEvent.1.0.ResourceErrorThresholdExceeded",
          "Message": "The resource property Foo has exceeded error threshold of value 3.1415926.",
          "MessageArgs": ["Foo", 3.1415926],
          "Timestamp": "2044-5-01T01:28:21Z",
          "Severity": "Critical"
        }
      ]
    }
  })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  absl::StatusOr<HealthRollup> health_rollup = ExtractHealthRollup(*obj);
  ASSERT_TRUE(health_rollup.status().ok());
  EXPECT_THAT(*health_rollup,
              EqualsProto(R"pb(resource_events {
                                 threshold_exceeded {
                                   resource_identifier: "Foo"
                                   threshold: 3.1415926
                                 }
                                 severity: "Critical"
                                 timestamp { seconds: 2345678901 }
                               })pb"));
}

TEST(HealthRollup, MultipleHealthRollups) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
    "Status": {
      "State": "Enabled",
      "Health": "OK",
      "HealthRollup": "Critical",
      "Conditions": [
        {
          "MessageId": "ResourceEvent.1.0.ResourceErrorThresholdExceeded",
          "Message": "The resource property Foo has exceeded error threshold of value 3.1415926.",
          "MessageArgs": ["Foo", 3.1415926],
          "Timestamp": "2044-5-01T01:28:21Z",
          "Severity": "Critical"
        },
        {
          "MessageId": "ResourceEvent.1.0.ResourceStateChanged",
          "Message": "The state of resource Bar has changed to Baz.",
          "MessageArgs": ["Bar", "Baz"],
          "Timestamp": "2009-2-13T23:31:30Z",
          "Severity": "Warning"
        },
        {
          "MessageId": "ResourceEvent.1.0.ResourceErrorsDetected",
          "Message": "The resource property Foo has detected errors of type Bar.",
          "MessageArgs": ["Foo", "Bar"],
          "Timestamp": "2022-1-23T12:34:56Z",
          "Severity": "Critical"
        },
        {
          "MessageId": "ResourceEvent.1.0.ResourceErrorsDetected",
          "Message": "The resource property Baz has detected errors of type Qux.",
          "MessageArgs": ["Baz", "Qux"],
          "Timestamp": "2022-1-23T12:34:57Z",
          "Severity": "Critical"
        }
      ]
    }
  })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  absl::StatusOr<HealthRollup> health_rollup = ExtractHealthRollup(*obj);
  ASSERT_TRUE(health_rollup.status().ok());
  EXPECT_THAT(*health_rollup, IgnoringRepeatedFieldOrdering(EqualsProto(R"pb(
    resource_events {
      threshold_exceeded { resource_identifier: "Foo" threshold: 3.1415926 }
      severity: "Critical"
      timestamp { seconds: 2345678901 }
    }
    resource_events {
      state_change { resource_identifier: "Bar" state_change: "Baz" }
      severity: "Warning"
      timestamp { seconds: 1234567890 }
    }
    resource_events {
      errors_detected { resource_identifier: "Foo" error_type: "Bar" }
      severity: "Critical"
      timestamp { seconds: 1642941296 }
    }
    resource_events {
      errors_detected { resource_identifier: "Baz" error_type: "Qux" }
      severity: "Critical"
      timestamp { seconds: 1642941297 }
    }
  )pb")));
}

}  // namespace
}  // namespace ecclesia
