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

#include "ecclesia/lib/mcedecoder/mce_decode.h"

#include <memory>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/mcedecoder/cpu_topology.h"
#include "ecclesia/lib/mcedecoder/cpu_topology_mock.h"
#include "ecclesia/lib/mcedecoder/dimm_translator.h"
#include "ecclesia/lib/mcedecoder/indus/dimm_translator.h"
#include "ecclesia/lib/mcedecoder/mce_messages.h"

namespace ecclesia {
namespace {

using ::testing::Return;

TEST(MceDecodeTest, DecodeCorrectableMemoryControllerWriteError) {
  const int lpu_id = 54;
  const int bank = 7;
  const int socket_id = 1;
  auto cpu_topology = absl::make_unique<MockCpuTopology>();
  EXPECT_CALL(*cpu_topology, GetSocketIdForLpu(lpu_id))
      .WillOnce(Return(socket_id));
  auto dimm_translator = absl::make_unique<IndusDimmTranslator>();

  MceDecoder mce_decoder(CpuVendor::kIntel, CpuIdentifier::kSkylake,
                         std::move(cpu_topology), std::move(dimm_translator));

  MceLogMessage raw_msg{
      0, lpu_id, bank, 0, 0x9c000040010400a1, 0x35a4456040, 0x200414a228001086};
  auto maybe_decoded_msg = mce_decoder.DecodeMceMessage(raw_msg);
  ASSERT_TRUE(maybe_decoded_msg.ok());
  MceDecodedMessage decoded_msg = *maybe_decoded_msg;

  EXPECT_EQ(decoded_msg.mce_bucket.bank, bank);
  EXPECT_EQ(decoded_msg.mce_bucket.socket, socket_id);
  EXPECT_FALSE(decoded_msg.mce_bucket.mce_corrupt);
  EXPECT_FALSE(decoded_msg.mce_bucket.uncorrectable);
  EXPECT_FALSE(decoded_msg.mce_bucket.processor_context_corrupted);

  EXPECT_TRUE(decoded_msg.cpu_errors.empty());

  EXPECT_EQ(decoded_msg.mem_errors.size(), 1);
  EXPECT_EQ(decoded_msg.mem_errors[0].error_count, 1);
  EXPECT_EQ(decoded_msg.mem_errors[0].mem_error_bucket.gldn, 14);
  EXPECT_TRUE(decoded_msg.mem_errors[0].mem_error_bucket.correctable);
}

TEST(MceDecodeTest, DecodeUnCorrectableMemoryControllerReadError) {
  const int lpu_id = 56;
  const int bank = 7;
  const int socket_id = 1;
  auto cpu_topology = absl::make_unique<MockCpuTopology>();
  EXPECT_CALL(*cpu_topology, GetSocketIdForLpu(lpu_id))
      .WillOnce(Return(socket_id));
  auto dimm_translator = absl::make_unique<IndusDimmTranslator>();
  MceDecoder mce_decoder(CpuVendor::kIntel, CpuIdentifier::kSkylake,
                         std::move(cpu_topology), std::move(dimm_translator));

  MceLogMessage raw_msg{
      0, lpu_id, bank, 0, 0xbc00000001010090, 0x34fe426040, 0x200001c080602086};
  auto maybe_decoded_msg = mce_decoder.DecodeMceMessage(raw_msg);
  ASSERT_TRUE(maybe_decoded_msg.ok());
  MceDecodedMessage decoded_msg = *maybe_decoded_msg;

  EXPECT_EQ(decoded_msg.mce_bucket.bank, bank);
  EXPECT_EQ(decoded_msg.mce_bucket.socket, socket_id);
  EXPECT_FALSE(decoded_msg.mce_bucket.mce_corrupt);
  EXPECT_TRUE(decoded_msg.mce_bucket.uncorrectable);
  EXPECT_FALSE(decoded_msg.mce_bucket.processor_context_corrupted);

  EXPECT_TRUE(decoded_msg.cpu_errors.empty());

  EXPECT_EQ(decoded_msg.mem_errors.size(), 1);
  EXPECT_EQ(decoded_msg.mem_errors[0].error_count, 1);
  EXPECT_EQ(decoded_msg.mem_errors[0].mem_error_bucket.gldn, 12);
  EXPECT_FALSE(decoded_msg.mem_errors[0].mem_error_bucket.correctable);
}

TEST(MceDecodeTest, DecodeCorrectableMultipleMemError) {
  const int lpu_id = 6;
  const int bank = 13;
  const int socket_id = 0;
  auto cpu_topology = absl::make_unique<MockCpuTopology>();
  EXPECT_CALL(*cpu_topology, GetSocketIdForLpu(lpu_id))
      .WillOnce(Return(socket_id));
  auto dimm_translator = absl::make_unique<IndusDimmTranslator>();
  MceDecoder mce_decoder(CpuVendor::kIntel, CpuIdentifier::kSkylake,
                         std::move(cpu_topology), std::move(dimm_translator));

  MceLogMessage raw_msg{
      0, lpu_id, bank, 0, 0xc80000c100800090, 0, 0xd129e00204404400};
  auto maybe_decoded_msg = mce_decoder.DecodeMceMessage(raw_msg);
  ASSERT_TRUE(maybe_decoded_msg.ok());
  MceDecodedMessage decoded_msg = *maybe_decoded_msg;

  EXPECT_EQ(decoded_msg.mce_bucket.bank, bank);
  EXPECT_EQ(decoded_msg.mce_bucket.socket, socket_id);
  EXPECT_FALSE(decoded_msg.mce_bucket.mce_corrupt);
  EXPECT_FALSE(decoded_msg.mce_bucket.uncorrectable);
  EXPECT_FALSE(decoded_msg.mce_bucket.processor_context_corrupted);

  EXPECT_TRUE(decoded_msg.cpu_errors.empty());

  EXPECT_EQ(decoded_msg.mem_errors.size(), 3);
  EXPECT_EQ(decoded_msg.mem_errors[0].error_count, 1);
  EXPECT_EQ(decoded_msg.mem_errors[0].mem_error_bucket.gldn, 10);
  EXPECT_TRUE(decoded_msg.mem_errors[0].mem_error_bucket.correctable);
  EXPECT_EQ(decoded_msg.mem_errors[1].error_count, 1);
  EXPECT_EQ(decoded_msg.mem_errors[1].mem_error_bucket.gldn, 10);
  EXPECT_TRUE(decoded_msg.mem_errors[1].mem_error_bucket.correctable);
  EXPECT_EQ(decoded_msg.mem_errors[2].error_count, 1);
  EXPECT_EQ(decoded_msg.mem_errors[2].mem_error_bucket.gldn, 10);
  EXPECT_TRUE(decoded_msg.mem_errors[2].mem_error_bucket.correctable);
}

TEST(MceDecodeTest, DecodeUnCorrectableCpuCacheError) {
  const int lpu_id = 59;
  const int bank = 1;
  const int socket_id = 1;
  auto cpu_topology = absl::make_unique<MockCpuTopology>();
  EXPECT_CALL(*cpu_topology, GetSocketIdForLpu(lpu_id))
      .WillOnce(Return(socket_id));
  auto dimm_translator = absl::make_unique<IndusDimmTranslator>();
  MceDecoder mce_decoder(CpuVendor::kIntel, CpuIdentifier::kSkylake,
                         std::move(cpu_topology), std::move(dimm_translator));

  MceLogMessage raw_msg{0,           lpu_id, bank, 7, 0xbd80000000100134,
                        0x166fab040, 0x86};
  auto maybe_decoded_msg = mce_decoder.DecodeMceMessage(raw_msg);
  ASSERT_TRUE(maybe_decoded_msg.ok());
  MceDecodedMessage decoded_msg = *maybe_decoded_msg;

  EXPECT_EQ(decoded_msg.mce_bucket.bank, bank);
  EXPECT_EQ(decoded_msg.mce_bucket.socket, socket_id);
  EXPECT_FALSE(decoded_msg.mce_bucket.mce_corrupt);
  EXPECT_TRUE(decoded_msg.mce_bucket.uncorrectable);
  EXPECT_FALSE(decoded_msg.mce_bucket.processor_context_corrupted);

  EXPECT_TRUE(decoded_msg.mem_errors.empty());

  EXPECT_EQ(decoded_msg.cpu_errors.size(), 1);
  EXPECT_EQ(decoded_msg.cpu_errors[0].error_count, 1);
  EXPECT_EQ(decoded_msg.cpu_errors[0].cpu_error_bucket.socket, socket_id);
  EXPECT_EQ(decoded_msg.cpu_errors[0].cpu_error_bucket.lpu_id, lpu_id);
  EXPECT_FALSE(decoded_msg.cpu_errors[0].cpu_error_bucket.correctable);
  EXPECT_FALSE(decoded_msg.cpu_errors[0].cpu_error_bucket.whitelisted);
}

TEST(MceDecodeTest, DecodeUnCorrectableInstructionFetchError) {
  const int lpu_id = 18;
  const int bank = 0;
  const int socket_id = 0;
  auto cpu_topology = absl::make_unique<MockCpuTopology>();
  EXPECT_CALL(*cpu_topology, GetSocketIdForLpu(lpu_id))
      .WillOnce(Return(socket_id));
  auto dimm_translator = absl::make_unique<IndusDimmTranslator>();
  MceDecoder mce_decoder(CpuVendor::kIntel, CpuIdentifier::kSkylake,
                         std::move(cpu_topology), std::move(dimm_translator));

  MceLogMessage raw_msg{0,           lpu_id, bank, 7, 0xbd800000000c0150,
                        0x16a616040, 0x86};
  auto maybe_decoded_msg = mce_decoder.DecodeMceMessage(raw_msg);
  ASSERT_TRUE(maybe_decoded_msg.ok());
  MceDecodedMessage decoded_msg = *maybe_decoded_msg;

  EXPECT_EQ(decoded_msg.mce_bucket.bank, bank);
  EXPECT_EQ(decoded_msg.mce_bucket.socket, socket_id);
  EXPECT_FALSE(decoded_msg.mce_bucket.mce_corrupt);
  EXPECT_TRUE(decoded_msg.mce_bucket.uncorrectable);
  EXPECT_FALSE(decoded_msg.mce_bucket.processor_context_corrupted);

  EXPECT_TRUE(decoded_msg.mem_errors.empty());

  EXPECT_EQ(decoded_msg.cpu_errors.size(), 1);
  EXPECT_EQ(decoded_msg.cpu_errors[0].error_count, 1);
  EXPECT_EQ(decoded_msg.cpu_errors[0].cpu_error_bucket.socket, socket_id);
  EXPECT_EQ(decoded_msg.cpu_errors[0].cpu_error_bucket.lpu_id, lpu_id);
  EXPECT_FALSE(decoded_msg.cpu_errors[0].cpu_error_bucket.correctable);
  EXPECT_FALSE(decoded_msg.cpu_errors[0].cpu_error_bucket.whitelisted);
}

TEST(MceDecodeTest, DecodeCorruptedMce) {
  const int lpu_id = 18;
  const int bank = 0;
  const int socket_id = 0;
  auto cpu_topology = absl::make_unique<MockCpuTopology>();
  EXPECT_CALL(*cpu_topology, GetSocketIdForLpu(lpu_id))
      .WillOnce(Return(socket_id));
  auto dimm_translator = absl::make_unique<IndusDimmTranslator>();
  MceDecoder mce_decoder(CpuVendor::kIntel, CpuIdentifier::kSkylake,
                         std::move(cpu_topology), std::move(dimm_translator));

  MceLogMessage raw_msg{0,           lpu_id, bank, 7, 0x4d800000000c0150,
                        0x16a616040, 0x86};
  auto maybe_decoded_msg = mce_decoder.DecodeMceMessage(raw_msg);
  ASSERT_TRUE(maybe_decoded_msg.ok());
  MceDecodedMessage decoded_msg = *maybe_decoded_msg;

  EXPECT_EQ(decoded_msg.mce_bucket.bank, bank);
  EXPECT_EQ(decoded_msg.mce_bucket.socket, socket_id);
  EXPECT_TRUE(decoded_msg.mce_bucket.mce_corrupt);

  EXPECT_TRUE(decoded_msg.cpu_errors.empty());
  EXPECT_TRUE(decoded_msg.mem_errors.empty());
}

}  // namespace
}  // namespace ecclesia
