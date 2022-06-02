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

#include "ecclesia/lib/ipmi/raw/ipmitool.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ecclesia/lib/ipmi/ipmi_consts.h"
#include "ecclesia/lib/ipmi/ipmi_pair.h"
#include "ecclesia/lib/ipmi/ipmi_request.h"

extern "C" {
#include "include/ipmitool/ipmi_intf.h"
}  // extern "C"

namespace ecclesia {
namespace {

using ::testing::HasSubstr;

// The tests need a pointer to the test object to call into it from outside the
// test object.  There is a c-pointer call table involved in the code-path.
class RawTest;

RawTest *tester;

// Mock implementation of the send/recv method used by the call-table structure
// defined by ipmitool
ipmi_rs *MockSendRecv(ipmi_intf *intf, ipmi_rq *req);

class RawTest : public ::testing::Test {
 public:
  // The actual mock send recv that compares the expectations and returns a
  // response.
  ipmi_rs *SendRecv(ipmi_intf *intf, ipmi_rq *req) {
    EXPECT_EQ(&expected_intf_, intf);

    // The buffer they use is on their stack, so we can't know it ahead of time.
    EXPECT_EQ(expected_request_.msg.netfn, req->msg.netfn);
    EXPECT_EQ(expected_request_.msg.lun, req->msg.lun);
    EXPECT_EQ(expected_request_.msg.cmd, req->msg.cmd);
    EXPECT_EQ(expected_request_.msg.data_len, req->msg.data_len);

    // Verify the buffer was copied over.
    EXPECT_EQ(0, std::memcmp(&expected_request_.msg.data[2], req->msg.data,
                             req->msg.data_len));

    // Support returning a sequence of responses.
    return &returned_response_[which_response_++];
  }

  // Given a buffer for the Raw() command, set up the expected ipmi_rq object.
  void SetUpExpectedRequest(uint8_t buffer[], int size) {
    expected_request_.msg.netfn = buffer[0];
    expected_request_.msg.cmd = buffer[1];
    expected_request_.msg.data = buffer;
    expected_request_.msg.data_len = size - ecclesia::kMinimumIpmiPacketLength;
  }

 protected:
  RawTest() : raw_(&expected_intf_) {
    tester = this;
    expected_intf_.sendrecv = MockSendRecv;
  }
  ~RawTest() override { tester = nullptr; }

  ipmi_intf expected_intf_ = {};
  RawIpmitool raw_;
  ipmi_rq expected_request_ = {};
  ipmi_rs returned_response_[2] = {};
  int which_response_ = 0;
};

ipmi_rs *MockSendRecv(ipmi_intf *intf, ipmi_rq *req) {
  return tester->SendRecv(intf, req);
}

TEST_F(RawTest, DoesMinimumLenCheck) {
  // This test verifies IPMIRaw verifies the buffer length is at least 2 bytes.
  // [netfn][cmd]
  EXPECT_FALSE(raw_.Raw({1}, nullptr).ok());
}

TEST_F(RawTest, SuccessfullySendsCommand) {
  // This test verifies IPMIRaw properly configures the message to the ipmilib.

  // Return success for this test.
  returned_response_[0].ccode = ecclesia::IPMI_OK_CODE;

  std::vector<uint8_t> vec_buffer = {0xff, 0x23, 0xc3};
  uint8_t buffer[] = {0xff, 0x23, 0xc3};
  SetUpExpectedRequest(buffer, 3);

  EXPECT_TRUE(raw_.Raw(vec_buffer, nullptr).ok());
}

TEST_F(RawTest, SuccessfullySendsCommandCapturesResponse) {
  // This test verifies IPMIRaw properly configures the message to the ipmilib
  // and capture the response.

  // Return success for this test.
  returned_response_[0].ccode = ecclesia::IPMI_OK_CODE;

  std::vector<uint8_t> vec_buffer = {0xff, 0x23, 0xc3};
  uint8_t buffer[] = {0xff, 0x23, 0xc3};
  SetUpExpectedRequest(buffer, 3);

  ipmi_rs *resp;
  EXPECT_TRUE(raw_.Raw(vec_buffer, &resp).ok());
  EXPECT_EQ(resp, &returned_response_[0]);
}

TEST_F(RawTest, SuccessfullySendsCommandReceivesTimeout) {
  // This test verifies IPMIRaw properly configures the message to the ipmilib
  // and the ipmilib returns a timeout.

  // Return timeout for this test.
  returned_response_[0].ccode = ecclesia::IPMI_TIMEOUT_COMPLETION_CODE;

  std::vector<uint8_t> vec_buffer = {0xff, 0x23, 0xc3};
  uint8_t buffer[] = {0xff, 0x23, 0xc3};
  SetUpExpectedRequest(buffer, 3);

  auto result = raw_.Raw(vec_buffer, nullptr);
  ASSERT_FALSE(result.ok());
  EXPECT_THAT(result.ToString(), HasSubstr("Timeout from IPMI"));
}

TEST_F(RawTest, SuccessfullySendsCommandReceivesError) {
  // This test verifies IPMIRaw properly configures the message to the ipmilib
  // and the ipmilib returns an error that isn't the timeout, just to make sure
  // we have a catch-all for errors.

  // Return timeout for this test.
  returned_response_[0].ccode = ecclesia::IPMI_INVALID_CMD_COMPLETION_CODE;

  std::vector<uint8_t> vec_buffer = {0xff, 0x23, 0xc3};
  uint8_t buffer[] = {0xff, 0x23, 0xc3};
  SetUpExpectedRequest(buffer, 3);

  auto result = raw_.Raw(vec_buffer, nullptr);
  ASSERT_FALSE(result.ok());
  EXPECT_THAT(
      result.ToString(),
      HasSubstr(absl::StrCat("Unable to send code: ",
                             ecclesia::IPMI_INVALID_CMD_COMPLETION_CODE)));
}

TEST_F(RawTest, SuccessfullySendsCommandReceivesErrorCapturesResponse) {
  // This test verifies IPMIRaw properly configures the message to the ipmilib
  // and the ipmilib returns an error that isn't the timeout, just to make sure
  // we have a catch-all for errors.
  //
  // This additionally verifies it provides a response on failure.

  // Return timeout for this test.
  returned_response_[0].ccode = ecclesia::IPMI_INVALID_CMD_COMPLETION_CODE;

  std::vector<uint8_t> vec_buffer = {0xff, 0x23, 0xc3};
  uint8_t buffer[] = {0xff, 0x23, 0xc3};
  SetUpExpectedRequest(buffer, 3);

  ipmi_rs *resp;

  auto result = raw_.Raw(vec_buffer, &resp);
  ASSERT_FALSE(result.ok());
  EXPECT_THAT(
      result.ToString(),
      HasSubstr(absl::StrCat("Unable to send code: ",
                             ecclesia::IPMI_INVALID_CMD_COMPLETION_CODE)));
  EXPECT_EQ(&returned_response_[0], resp);
}

TEST_F(RawTest, RetriesFailsOnceTriesTwiceReturnsSuccess) {
  // This test verifies IPMIRaw properly configures the message to the ipmilib
  // and the ipmilib returns an error that isn't the timeout, just to make sure
  // we have a catch-all for errors.
  //
  // This additionally verifies it provides a response on failure.

  // Return timeout for this test.
  returned_response_[0].ccode = ecclesia::IPMI_TIMEOUT_COMPLETION_CODE;
  returned_response_[1].ccode =
      ecclesia::IPMI_OK_CODE;  // Success on second call.

  uint8_t buffer[] = {static_cast<uint8_t>(ecclesia::IpmiNetworkFunction::kApp),
                      static_cast<uint8_t>(ecclesia::IpmiCommand::kGetDeviceId),
                      0xc3};

  SetUpExpectedRequest(buffer, 3);

  ecclesia::IpmiRequest request(ecclesia::IpmiPair::kGetDeviceId, {0xc3});

  EXPECT_TRUE(raw_.SendWithRetry(request, 1).ok());
}

TEST_F(RawTest, RetriesFailsAndExitsLoop) {
  // This test verifies the loop exits if it doesn't get success and returns the
  // expected error.

  returned_response_[0].ccode = ecclesia::IPMI_TIMEOUT_COMPLETION_CODE;

  uint8_t buffer[] = {static_cast<uint8_t>(ecclesia::IpmiNetworkFunction::kApp),
                      static_cast<uint8_t>(ecclesia::IpmiCommand::kGetDeviceId),
                      0xc3};

  SetUpExpectedRequest(buffer, 3);

  ecclesia::IpmiRequest request(ecclesia::IpmiPair::kGetDeviceId, {0xc3});

  auto result = raw_.SendWithRetry(request, 0);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(result.status().ToString(),
              HasSubstr("Failed to send IPMI command"));
}

TEST_F(RawTest, VerifySendCommandPassesBytesReturnsOk) {
  // This test verifies that calling send() will work properly call the
  // underlying code.

  // Return success for this test.
  returned_response_[0].ccode = ecclesia::IPMI_OK_CODE;

  uint8_t buffer[] = {static_cast<uint8_t>(ecclesia::IpmiNetworkFunction::kApp),
                      static_cast<uint8_t>(ecclesia::IpmiCommand::kGetDeviceId),
                      0xc3};

  SetUpExpectedRequest(buffer, 3);

  ecclesia::IpmiRequest request(ecclesia::IpmiPair::kGetDeviceId, {0xc3});

  EXPECT_TRUE(raw_.Send(request).ok());
}

}  // namespace
}  // namespace ecclesia
