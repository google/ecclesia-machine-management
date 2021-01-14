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

#include "ecclesia/magent/lib/event_reader/mced_reader.h"

#include <sys/socket.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "ecclesia/magent/lib/event_reader/event_reader.h"

namespace ecclesia {
namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::StrictMock;

class TestMcedaemonSocket : public McedaemonSocketInterface {
 public:
  MOCK_METHOD(int, CallSocket, (int domain, int type, int protocol));
  MOCK_METHOD(FILE *, CallFdopen, (int fd, const char *mode));
  MOCK_METHOD(char *, CallFgets, (char *s, int size, FILE *stream));
  MOCK_METHOD(int, CallFclose, (FILE * stream));
  MOCK_METHOD(int, CallConnect,
              (int sockfd, const struct sockaddr *addr, socklen_t addrlen));
  MOCK_METHOD(int, CallClose, (int fd));
};

ACTION_P(ReadString, val) { return strncpy(arg0, val, 1024); }

TEST(McedaemonReaderTest, SocketFailure) {
  StrictMock<TestMcedaemonSocket> test_socket;

  EXPECT_CALL(test_socket, CallSocket(_, _, _)).WillRepeatedly(Return(-1));
  McedaemonReader mced_reader("dummy_socket", &test_socket);
  // Wait for the reader loop to start fetching the mces
  absl::SleepFor(absl::Seconds(5));
  EXPECT_FALSE(mced_reader.ReadEvent());
}

TEST(McedaemonReaderTest, ReadFailure) {
  StrictMock<TestMcedaemonSocket> test_socket;
  int fake_fd = 1;
  FILE *fake_file = reinterpret_cast<FILE *>(2);

  EXPECT_CALL(test_socket, CallSocket(_, _, _)).WillRepeatedly(Return(fake_fd));
  EXPECT_CALL(test_socket, CallConnect(fake_fd, _, _))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(test_socket, CallFdopen(fake_fd, _))
      .WillRepeatedly(Return(fake_file));
  EXPECT_CALL(test_socket, CallFgets(_, _, fake_file))
      .WillRepeatedly(ReturnNull());
  EXPECT_CALL(test_socket, CallFclose(fake_file)).WillRepeatedly(Return(0));

  McedaemonReader mced_reader("dummy_socket", &test_socket);
  // Wait for the reader loop to start fetching the mces
  absl::SleepFor(absl::Seconds(5));
  EXPECT_FALSE(mced_reader.ReadEvent());
}

TEST(McedaemonReaderTest, ReadSuccess) {
  ::testing::InSequence s;

  StrictMock<TestMcedaemonSocket> test_socket;
  int fake_fd = 1;
  FILE *fake_file = reinterpret_cast<FILE *>(2);

  EXPECT_CALL(test_socket, CallSocket(_, _, _)).WillRepeatedly(Return(fake_fd));
  EXPECT_CALL(test_socket, CallConnect(fake_fd, _, _)).WillOnce(Return(0));
  EXPECT_CALL(test_socket, CallFdopen(fake_fd, _)).WillOnce(Return(fake_file));

  std::string mced_string =
      "%B=-9 %c=4 %S=3 %p=0x00000004 %v=2 %A=0x00830f00 %b=12 %s=0x1234567890 "
      "%a=0x876543210 %m=0x1111111122222222 %y=0x000000004a000142 "
      "%i=0x00000018013b1700 %g=0x0000000012059349 %G=0x40248739 "
      "%t=0x0000000000000004 %T=0x0000000004000000 %C=0x0020 "
      "%I=0x00000032413312da\n";

  EXPECT_CALL(test_socket, CallFgets(_, _, fake_file))
      .WillOnce(ReadString(mced_string.c_str()))
      .WillRepeatedly(Return(nullptr));

  EXPECT_CALL(test_socket, CallFclose(fake_file)).WillOnce(Return(0));

  McedaemonReader mced_reader("dummy_socket", &test_socket);
  absl::SleepFor(absl::Seconds(5));
  auto mce_record = mced_reader.ReadEvent();
  ASSERT_TRUE(mce_record);
  MachineCheck expected{.mci_status = 0x1234567890,
                        .mci_address = 0x876543210,
                        .mci_misc = 0x1111111122222222,
                        .mcg_status = 0x12059349,
                        .tsc = 0x4000000,
                        .time = absl::FromUnixMicros(4),
                        .ip = 0x32413312da,
                        .boot = -9,
                        .cpu = 4,
                        .cpuid_eax = 0x00830f00,
                        .init_apic_id = 0x4,
                        .socket = 3,
                        .mcg_cap = 0x40248739,
                        .cs = 0x20,
                        .bank = 12,
                        .vendor = 2};
  MachineCheck mce = std::get<MachineCheck>(mce_record.value().record);
  EXPECT_EQ(mce.mci_status, expected.mci_status);
  EXPECT_EQ(mce.mci_address, expected.mci_address);
  EXPECT_EQ(mce.mci_misc, expected.mci_misc);
  EXPECT_EQ(mce.mcg_status, expected.mcg_status);
  EXPECT_EQ(mce.tsc, expected.tsc);
  EXPECT_EQ(mce.time, expected.time);
  EXPECT_EQ(mce.ip, expected.ip);
  EXPECT_EQ(mce.boot, expected.boot);
  EXPECT_EQ(mce.cpu, expected.cpu);
  EXPECT_EQ(mce.cpuid_eax, expected.cpuid_eax);
  EXPECT_EQ(mce.init_apic_id, expected.init_apic_id);
  EXPECT_EQ(mce.socket, expected.socket);
  EXPECT_EQ(mce.mcg_cap, expected.mcg_cap);
  EXPECT_EQ(mce.cs, expected.cs);
  EXPECT_EQ(mce.bank, expected.bank);
  EXPECT_EQ(mce.vendor, expected.vendor);
}

}  // namespace

}  // namespace ecclesia
