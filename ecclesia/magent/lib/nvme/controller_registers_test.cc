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

#include "ecclesia/magent/lib/nvme/controller_registers.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "ecclesia/magent/lib/nvme/controller_registers.emb.h"
#include "runtime/cpp/emboss_cpp_util.h"
#include "runtime/cpp/emboss_prelude.h"
#include "runtime/cpp/emboss_text_util.h"

namespace ecclesia {
namespace {

// Test Expectation: Output of the nvme_cli to compare against
// mval3:/export/hda3# ./nvme show-regs /mnt/devtmpfs/nvme0
// cap     : 2078030fff
// version : 10200
// cc      : 460001
// csts    : 1
// nssr    : 0
// intms   : 0
// intmc   : 0
// aqa     : 1f001f

constexpr int kPageSize = 4096;

// The first 4K bytes of the resource0 file of the pci device that represents
// an NVMe drive. Obtained via mem_dump.
// example command: /usr/local/iotools/mem_dump 0x00000000ee610000 4096
constexpr unsigned char kNvmePciResource0[kPageSize] = {
    /*0x0000*/ 0xff, 0x0f, 0x03, 0x78, 0x20, 0x00, 0x00, 0x00,
    /*0x0008*/ 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    /*0x0010*/ 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x46, 0x00,
    /*0x0018*/ 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    /*0x0020*/ 0x00, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x1f, 0x00,
};

TEST(ControllerRegistersTest, ParseControllerRegisters) {
  auto ret = ControllerRegisters::Parse(
      std::string(kNvmePciResource0, kNvmePciResource0 + kPageSize));
  ASSERT_TRUE(ret.ok());
  ControllerRegisters ctrlr_regs = std::move(ret.value());

  auto actual_view = ctrlr_regs.GetMessageView();
  ASSERT_TRUE(actual_view.Ok());
  // First compare the raw values as read from nvme_cli
  EXPECT_EQ(actual_view.cap().raw().Read(), 0x2078030fff);
  EXPECT_EQ(actual_view.vs().raw().Read(), 0x10200);
  EXPECT_EQ(actual_view.cc().raw().Read(), 0x460001);
  EXPECT_EQ(actual_view.csts().raw().Read(), 0x1);
  EXPECT_EQ(actual_view.aqa().raw().Read(), 0x1f001f);

  std::array<uint8_t, ControllerRegistersStructure::MaxSizeInBytes()> buffer =
      {};
  auto expected_view = MakeControllerRegistersStructureView(&buffer);
  ASSERT_TRUE(::emboss::UpdateFromText(expected_view,
                                       R"(
                    { cap: {
                        mqes: 0xfff
                        cqr:     true
                        ams:     0x1
                        to:      0x78
                        dstrd:   0
                        nssrs:   false
                        css:     0x1
                        bps:     false
                        mpsmin:  0
                        mpsmax:  0
                        pmrs:   false
                        cmbs:   false
                      }

                      vs: {
                        mnr: 2
                        mjr: 1
                      }
                      cc: {
                        en:      true
                        css:     0x0
                        mps:     0x0
                        ams:     0x0
                        shn:     0x0
                        iosqes:  0x6
                        iocqes:  0x4
                      }
                    csts: {
                      rdy:    true
                      cfs:    false
                      shst:   0
                      nssro:  false
                      pp:     false
                    }
                    aqa: {
                      asqs:  0x1f
                      acqs:  0x1f
                    }
                  }
              )"));
  EXPECT_TRUE(actual_view.Equals(expected_view));
}

}  // namespace

}  // namespace ecclesia
