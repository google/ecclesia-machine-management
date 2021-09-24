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

#include "ecclesia/magent/lib/nvme/nvme_device.h"

#include <linux/nvme_ioctl.h>
#include <sys/sysinfo.h>

#include <cstdint>
#include <cstring>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/substitute.h"
#include "ecclesia/lib/testing/status.h"
#include "ecclesia/magent/lib/nvme/controller_registers.h"
#include "ecclesia/magent/lib/nvme/identify_namespace.h"
#include "ecclesia/magent/lib/nvme/mock_nvme_device.h"
#include "ecclesia/magent/lib/nvme/nvme_access.h"
#include "ecclesia/magent/lib/nvme/nvme_types.emb.h"

namespace ecclesia {
namespace {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Pointee;
using ::testing::Return;

const uint8_t kNvmeOpcodeSanitizeNvm = 0x84;
const uint8_t kNvmeOpcodeAdminIdentify = 0x6;
const uint8_t kCnsIdentifyListNs = 0x02;
const int kPartialNsidCount = 2;

class MockNvmeAccessInterface : public NvmeAccessInterface {
 public:
  MOCK_METHOD(absl::Status, ExecuteAdminCommand, (nvme_passthru_cmd * cmd),
              (const, override));
  // MOCK_METHOD(absl::Status, RescanNamespaces, (), (override));
  MOCK_METHOD(absl::Status, ResetSubsystem, (), (override));
  MOCK_METHOD(absl::Status, ResetController, (), (override));
  MOCK_METHOD(absl::StatusOr<ControllerRegisters>, GetControllerRegisters, (),
              (const, override));
};

// Check that operator== works as expected.
TEST(IdentifyNamespaceOperatorEqualsTest, AllFieldsAreConsidered) {
  IdentifyNamespace a{1, 2, 3};
  IdentifyNamespace b{1, 2, 3};
  EXPECT_EQ(a, b);

  IdentifyNamespace c{9, 2, 3};
  EXPECT_NE(a, c);

  IdentifyNamespace d{1, 9, 3};
  EXPECT_NE(a, d);

  IdentifyNamespace e;
  EXPECT_NE(a, e);
}

// Test the typical case: a device with a small number of namespaces.
TEST(EnumerateAllNamespacesTest, ShortList) {
  MockNvmeDevice nvme;

  std::set<uint32_t> expected_set{1, 2, 7};

  EXPECT_CALL(nvme, EnumerateNamespacesAfter(0)).WillOnce(Return(expected_set));

  auto ret = nvme.EnumerateAllNamespaces();
  ASSERT_TRUE(ret.ok());
  EXPECT_THAT(ret.value(), expected_set);
}

// A device which has exactly one commands' worth of NSIDs.
TEST(EnumerateAllNamespacesTest, ExactlyOneCommandsWorthOfNsids) {
  MockNvmeDevice nvme;

  std::set<uint32_t> expected_set;
  for (int i = 0; i < IdentifyListNamespaceFormat::capacity(); ++i) {
    expected_set.insert(i * 7);
  }

  EXPECT_CALL(nvme, EnumerateNamespacesAfter(0)).WillOnce(Return(expected_set));
  EXPECT_CALL(nvme, EnumerateNamespacesAfter(
                        (IdentifyListNamespaceFormat::capacity() - 1) * 7))
      .WillOnce(Return(std::set<uint32_t>()));

  auto ret = nvme.EnumerateAllNamespaces();
  ASSERT_TRUE(ret.ok());
  EXPECT_THAT(ret.value(), expected_set);
}

// A device which has more than two commands' worth of NSIDs.
TEST(EnumerateAllNamespacesTest, ThreeCommandsWorthOfNsids) {
  MockNvmeDevice nvme;

  std::set<uint32_t> set_1, set_2, set_3;
  for (int i = 0; i < IdentifyListNamespaceFormat::capacity(); ++i) {
    set_1.insert(i + IdentifyListNamespaceFormat::capacity() * 0);
  }
  for (int i = 0; i < IdentifyListNamespaceFormat::capacity(); ++i) {
    set_2.insert(i + IdentifyListNamespaceFormat::capacity() * 1);
  }
  for (int i = 0; i < IdentifyListNamespaceFormat::capacity() / 3; ++i) {
    set_3.insert(i + IdentifyListNamespaceFormat::capacity() * 2);
  }
  std::set<uint32_t> expected_set;
  expected_set.insert(set_1.begin(), set_1.end());
  expected_set.insert(set_2.begin(), set_2.end());
  expected_set.insert(set_3.begin(), set_3.end());

  EXPECT_CALL(nvme, EnumerateNamespacesAfter(0)).WillOnce(Return(set_1));
  EXPECT_CALL(nvme, EnumerateNamespacesAfter(*set_1.rbegin()))
      .WillOnce(Return(set_2));
  EXPECT_CALL(nvme, EnumerateNamespacesAfter(*set_2.rbegin()))
      .WillOnce(Return(set_3));

  auto ret = nvme.EnumerateAllNamespaces();
  ASSERT_TRUE(ret.ok());
  EXPECT_THAT(ret.value(), expected_set);
}

absl::Status ReturnShortNsList(nvme_passthru_cmd* cmd) {
  uint32_t* nsid = reinterpret_cast<uint32_t*>(cmd->addr);
  memset(nsid, 0, cmd->data_len);
  nsid[0] = 1;
  return absl::OkStatus();
}

// Test the typical case: a device with a small number of namespaces.
TEST(EnumerateNamespacesAfterTest, ShortList) {
  auto MockAccess = std::make_unique<MockNvmeAccessInterface>();

  EXPECT_CALL(
      *MockAccess,
      ExecuteAdminCommand(AllOf(
          Pointee(Field(&nvme_passthru_cmd::opcode, kNvmeOpcodeAdminIdentify)),
          Pointee(Field(&nvme_passthru_cmd::nsid, 0)),
          Pointee(Field(&nvme_passthru_cmd::cdw10, kCnsIdentifyListNs)))))
      .WillOnce(Invoke(ReturnShortNsList));

  auto nvme = CreateNvmeDevice(std::move(MockAccess));
  std::set<uint32_t> expected_set{1};

  EXPECT_THAT(nvme->EnumerateAllNamespaces(), IsOkAndHolds(expected_set));
}

absl::Status ReturnNsList(nvme_passthru_cmd* cmd) {
  int nsid_count;

  switch (cmd->nsid) {
    case 0:
    case IdentifyListNamespaceFormat::capacity():
      nsid_count = IdentifyListNamespaceFormat::capacity();
      break;
    case IdentifyListNamespaceFormat::capacity() * 2:
      nsid_count = kPartialNsidCount;
      break;
    default:
      return absl::InvalidArgumentError(
          absl::Substitute("Unexpected nsid $0", cmd->nsid));
  }

  uint32_t* nsid = reinterpret_cast<uint32_t*>(cmd->addr);
  memset(nsid, 0, cmd->data_len);
  for (int i = 0; i < nsid_count; i++) {
    nsid[i] = cmd->nsid + i + 1;
  }

  return absl::OkStatus();
}

// A device which has more than two commands' worth of NSIDs.
TEST(EnumerateNamespacesAfterTest, ThreeCommandsWorthOfNsids) {
  auto MockAccess = std::make_unique<MockNvmeAccessInterface>();

  EXPECT_CALL(
      *MockAccess,
      ExecuteAdminCommand(AllOf(
          Pointee(Field(&nvme_passthru_cmd::opcode, kNvmeOpcodeAdminIdentify)),
          Pointee(Field(&nvme_passthru_cmd::cdw10, kCnsIdentifyListNs)))))
      .Times(3)
      .WillRepeatedly(Invoke(ReturnNsList));

  std::set<uint32_t> expected_set;
  for (uint32_t i = 0;
       i < IdentifyListNamespaceFormat::capacity() * 2 + kPartialNsidCount;
       ++i) {
    expected_set.insert(i + 1);
  }

  auto nvme = CreateNvmeDevice(std::move(MockAccess));
  EXPECT_THAT(nvme->EnumerateAllNamespaces(), IsOkAndHolds(expected_set));
}

TEST(EnumerateAllNamespacesAndInfoTest, IdentifyNamespaceErrorPropagates) {
  MockNvmeDevice nvme;

  std::set<uint32_t> namespaces{1, 2, 7};

  EXPECT_CALL(nvme, EnumerateNamespacesAfter(0)).WillOnce(Return(namespaces));
  EXPECT_CALL(nvme, GetNamespaceInfo(1))
      .WillOnce(Return(absl::UnavailableError("BOGUS ERROR")));

  EXPECT_EQ(nvme.EnumerateAllNamespacesAndInfo().status().code(),
            absl::StatusCode::kUnavailable);
}

TEST(EnumerateAllNamespacesAndInfoTest, IdentifySeveralNamespacesWithInfo) {
  MockNvmeDevice nvme;

  std::set<uint32_t> namespaces{1, 2, 7};

  EXPECT_CALL(nvme, EnumerateNamespacesAfter(0)).WillOnce(Return(namespaces));

  std::map<uint32_t, IdentifyNamespace> expected = {
      {1, {0x100000, 1 << 9, 111}},
      {2, {0x200000, 1 << 12, 222}},
      {7, {0x300000, 1 << 15, 333}},
  };

  EXPECT_CALL(nvme, GetNamespaceInfo(1)).WillOnce(Return(expected[1]));
  EXPECT_CALL(nvme, GetNamespaceInfo(2)).WillOnce(Return(expected[2]));
  EXPECT_CALL(nvme, GetNamespaceInfo(7)).WillOnce(Return(expected[7]));

  auto ret = nvme.EnumerateAllNamespacesAndInfo();
  ASSERT_TRUE(ret.ok());
  EXPECT_THAT(ret.value(), expected);
}

TEST(IndexOfFormatWithLbaSizeTest, FindMatching) {
  MockNvmeDevice nvme;

  const char lba1[] = "\x00\x00\x09\x02";
  const char lba2[] = "\x00\x00\x0c\x01";
  const char lba3[] = "\x00\x00\x10\x00";

  const std::vector<LBAFormatView> supported_formats = {
      LBAFormatView(lba1, sizeof(lba1)),
      LBAFormatView(lba2, sizeof(lba2)),
      LBAFormatView(lba3, sizeof(lba3)),
  };

  EXPECT_CALL(nvme, GetSupportedLbaFormats())
      .Times(3)
      .WillRepeatedly(Return(supported_formats));

  auto ret = nvme.IndexOfFormatWithLbaSize(1 << 9);
  ASSERT_TRUE(ret.ok());
  EXPECT_EQ(ret.value(), 0);

  ret = nvme.IndexOfFormatWithLbaSize(1 << 12);
  ASSERT_TRUE(ret.ok());
  EXPECT_EQ(ret.value(), 1);

  ret = nvme.IndexOfFormatWithLbaSize(1 << 16);
  ASSERT_TRUE(ret.ok());
  EXPECT_EQ(ret.value(), 2);
}

TEST(IndexOfFormatWithLbaSizeTest, MustHaveMetadataSizeZero) {
  MockNvmeDevice nvme;

  const char lba1[] = "\x00\x0a\x0c\x00";

  const std::vector<LBAFormatView> supported_formats = {
      LBAFormatView(lba1, sizeof(lba1)),
  };

  EXPECT_CALL(nvme, GetSupportedLbaFormats())
      .WillOnce(Return(supported_formats));

  EXPECT_EQ(nvme.IndexOfFormatWithLbaSize(1 << 12).status().code(),
            absl::StatusCode::kNotFound);
}

TEST(IndexOfFormatWithLbaSizeTest, ReturnsHighestPerformance) {
  MockNvmeDevice nvme;

  const char lba1[] = "\x00\x00\x0c\x01";
  const char lba2[] = "\x00\x00\x0c\x00";
  const char lba3[] = "\x00\x00\x0c\x02";

  const std::vector<LBAFormatView> supported_formats = {
      LBAFormatView(lba1, sizeof(lba1)),
      LBAFormatView(lba2, sizeof(lba2)),
      LBAFormatView(lba3, sizeof(lba3)),
  };

  EXPECT_CALL(nvme, GetSupportedLbaFormats())
      .WillOnce(Return(supported_formats));

  auto ret = nvme.IndexOfFormatWithLbaSize(1 << 12);
  ASSERT_TRUE(ret.ok());
  EXPECT_EQ(ret.value(), 1);
}

TEST(IndexOfFormatWithLbaSizeTest, NoSupportedFormats) {
  MockNvmeDevice nvme;

  const std::vector<LBAFormatView> supported_formats = {};

  EXPECT_CALL(nvme, GetSupportedLbaFormats())
      .WillOnce(Return(supported_formats));

  EXPECT_EQ(nvme.IndexOfFormatWithLbaSize(1 << 12).status().code(),
            absl::StatusCode::kNotFound);
}

TEST(SanitizeTest, ExitFailureModeSuccess) {
  auto MockAccess = std::make_unique<MockNvmeAccessInterface>();
  EXPECT_CALL(
      *MockAccess,
      ExecuteAdminCommand(AllOf(
          Pointee(Field(&nvme_passthru_cmd::opcode, kNvmeOpcodeSanitizeNvm)),
          Pointee(Field(
              &nvme_passthru_cmd::cdw10,
              static_cast<uint32_t>(SanitizeAction::EXIT_FAILURE_MODE))))))
      .WillOnce(Return(absl::OkStatus()));
  auto nvme = CreateNvmeDevice(std::move(MockAccess));
  EXPECT_EQ(absl::OkStatus(), nvme->SanitizeExitFailureMode());
}

TEST(SanitizeTest, CryptoSanitizeSuccess) {
  auto MockAccess = std::make_unique<MockNvmeAccessInterface>();
  EXPECT_CALL(
      *MockAccess,
      ExecuteAdminCommand(AllOf(
          Pointee(Field(&nvme_passthru_cmd::opcode, kNvmeOpcodeSanitizeNvm)),
          Pointee(Field(&nvme_passthru_cmd::cdw10,
                        static_cast<uint32_t>(SanitizeAction::CRYPTO_ERASE))))))
      .WillOnce(Return(absl::OkStatus()));
  auto nvme = CreateNvmeDevice(std::move(MockAccess));
  EXPECT_EQ(absl::OkStatus(), nvme->SanitizeCryptoErase());
}

TEST(SanitizeTest, BlockSanitizeSuccess) {
  auto MockAccess = std::make_unique<MockNvmeAccessInterface>();
  EXPECT_CALL(
      *MockAccess,
      ExecuteAdminCommand(AllOf(
          Pointee(Field(&nvme_passthru_cmd::opcode, kNvmeOpcodeSanitizeNvm)),
          Pointee(Field(&nvme_passthru_cmd::cdw10,
                        static_cast<uint32_t>(SanitizeAction::BLOCK_ERASE))))))
      .WillOnce(Return(absl::OkStatus()));
  auto nvme = CreateNvmeDevice(std::move(MockAccess));
  EXPECT_EQ(absl::OkStatus(), nvme->SanitizeBlockErase());
}

TEST(SanitizeTest, OverWriteSanitizeSuccess) {
  auto MockAccess = std::make_unique<MockNvmeAccessInterface>();
  EXPECT_CALL(
      *MockAccess,
      ExecuteAdminCommand(AllOf(
          Pointee(Field(&nvme_passthru_cmd::opcode, kNvmeOpcodeSanitizeNvm)),
          Pointee(Field(&nvme_passthru_cmd::cdw10,
                        static_cast<uint32_t>(1 << 4) |
                            static_cast<uint32_t>(SanitizeAction::OVERWRITE))),
          Pointee(Field(&nvme_passthru_cmd::cdw11, 0x12)))))
      .WillOnce(Return(absl::OkStatus()));
  auto nvme = CreateNvmeDevice(std::move(MockAccess));
  EXPECT_EQ(absl::OkStatus(), nvme->SanitizeOverwrite(0x12));
}

TEST(SanitizeTest, ExitFailureModeFail) {
  auto MockAccess = std::make_unique<MockNvmeAccessInterface>();
  EXPECT_CALL(
      *MockAccess,
      ExecuteAdminCommand(AllOf(
          Pointee(Field(&nvme_passthru_cmd::opcode, kNvmeOpcodeSanitizeNvm)),
          Pointee(Field(
              &nvme_passthru_cmd::cdw10,
              static_cast<uint32_t>(SanitizeAction::EXIT_FAILURE_MODE))))))
      .WillOnce(Return(absl::InternalError("SOME ERRORS")));
  auto nvme = CreateNvmeDevice(std::move(MockAccess));
  auto status = nvme->SanitizeExitFailureMode();
  EXPECT_EQ(status.code(), absl::StatusCode::kInternal);
  EXPECT_THAT(status.ToString(), HasSubstr("SOME ERRORS"));
}

TEST(SanitizeTest, CryptoSanitizeFail) {
  auto MockAccess = std::make_unique<MockNvmeAccessInterface>();
  EXPECT_CALL(
      *MockAccess,
      ExecuteAdminCommand(AllOf(
          Pointee(Field(&nvme_passthru_cmd::opcode, kNvmeOpcodeSanitizeNvm)),
          Pointee(Field(&nvme_passthru_cmd::cdw10,
                        static_cast<uint32_t>(SanitizeAction::CRYPTO_ERASE))))))
      .WillOnce(Return(absl::InternalError("SOME ERRORS")));
  auto nvme = CreateNvmeDevice(std::move(MockAccess));
  auto status = nvme->SanitizeCryptoErase();
  EXPECT_EQ(status.code(), absl::StatusCode::kInternal);
  EXPECT_THAT(status.ToString(), HasSubstr("SOME ERRORS"));
}

TEST(SanitizeTest, BlockSanitizeFail) {
  auto MockAccess = std::make_unique<MockNvmeAccessInterface>();
  EXPECT_CALL(
      *MockAccess,
      ExecuteAdminCommand(AllOf(
          Pointee(Field(&nvme_passthru_cmd::opcode, kNvmeOpcodeSanitizeNvm)),
          Pointee(Field(&nvme_passthru_cmd::cdw10,
                        static_cast<uint32_t>(SanitizeAction::BLOCK_ERASE))))))
      .WillOnce(Return(absl::InternalError("SOME ERRORS")));
  auto nvme = CreateNvmeDevice(std::move(MockAccess));
  auto status = nvme->SanitizeBlockErase();
  EXPECT_EQ(status.code(), absl::StatusCode::kInternal);
  EXPECT_THAT(status.ToString(), HasSubstr("SOME ERRORS"));
}

TEST(SanitizeTest, OverWriteSanitizeFail) {
  auto MockAccess = std::make_unique<MockNvmeAccessInterface>();
  EXPECT_CALL(
      *MockAccess,
      ExecuteAdminCommand(AllOf(
          Pointee(Field(&nvme_passthru_cmd::opcode, kNvmeOpcodeSanitizeNvm)),
          Pointee(Field(&nvme_passthru_cmd::cdw10,
                        static_cast<uint32_t>(1 << 4) |
                            static_cast<uint32_t>(SanitizeAction::OVERWRITE))),
          Pointee(Field(&nvme_passthru_cmd::cdw11, 0x12345678)))))
      .WillOnce(Return(absl::InternalError("SOME ERRORS")));
  auto nvme = CreateNvmeDevice(std::move(MockAccess));
  auto status = nvme->SanitizeOverwrite(0x12345678);
  EXPECT_EQ(status.code(), absl::StatusCode::kInternal);
  EXPECT_THAT(status.ToString(), HasSubstr("SOME ERRORS"));
}

}  // namespace
}  // namespace ecclesia
