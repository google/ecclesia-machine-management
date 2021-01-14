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

#ifndef ECCLESIA_LIB_IO_PCI_MOCKS_H_
#define ECCLESIA_LIB_IO_PCI_MOCKS_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/codec/endian.h"
#include "ecclesia/lib/io/pci/config.h"
#include "ecclesia/lib/io/pci/discovery.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/pci.h"
#include "ecclesia/lib/io/pci/region.h"

namespace ecclesia {

// Defines a fake PCI region backed by a simple block of static memory
// represented by a fixed-size string. Reads and writes will only fail if the
// offsets are out of range.
class FakePciRegion : public PciRegion {
 public:
  explicit FakePciRegion(std::string memory)
      : PciRegion(memory.size()), memory_(std::move(memory)) {}

  // Do not allow them to be copied, as writes into a copy will not be
  // reflected into the other instances which isn't what people would normally
  // expect from a normal region.
  FakePciRegion(const FakePciRegion &) = delete;
  FakePciRegion &operator=(const FakePciRegion &) = delete;

  // Create a region that looks like config space for a PCI endpoint.
  struct EndpointConfigParams {
    uint16_t vid = 0;
    uint16_t did = 0;
    uint8_t revision_id = 0;
    uint32_t class_code = 0;
    uint16_t ssvid = 0;
    uint16_t ssid = 0;
  };
  static FakePciRegion EndpointConfig(EndpointConfigParams params) {
    std::string region(4096, '\0');
    LittleEndian::Store16(params.vid, &region[PciConfigSpace::kVidOffset]);
    LittleEndian::Store16(params.did, &region[PciConfigSpace::kDidOffset]);
    LittleEndian::Store32(params.revision_id | (params.class_code << 8),
                          &region[PciConfigSpace::kRevisionIdOffset]);
    LittleEndian::Store8(PciType0ConfigSpace::kHeaderType,
                         &region[PciConfigSpace::kHeaderTypeOffset]);
    LittleEndian::Store16(params.ssvid,
                          &region[PciType0ConfigSpace::kSubsysVendorIdOffset]);
    LittleEndian::Store16(params.ssid,
                          &region[PciType0ConfigSpace::kSubsysIdOffset]);
    return FakePciRegion(std::move(region));
  }

 private:
  absl::StatusOr<uint8_t> Read8(OffsetType offset) const override {
    if (offset >= Size()) return absl::OutOfRangeError("invalid offset");
    return LittleEndian::Load8(&memory_[offset]);
  }
  absl::Status Write8(OffsetType offset, uint8_t data) override {
    if (offset >= Size()) return absl::OutOfRangeError("invalid offset");
    LittleEndian::Store8(data, &memory_[offset]);
    return absl::OkStatus();
  }

  absl::StatusOr<uint16_t> Read16(OffsetType offset) const override {
    if (offset + 1 >= Size()) return absl::OutOfRangeError("invalid offset");
    return LittleEndian::Load16(&memory_[offset]);
  }
  absl::Status Write16(OffsetType offset, uint16_t data) override {
    if (offset + 1 >= Size()) return absl::OutOfRangeError("invalid offset");
    LittleEndian::Store16(data, &memory_[offset]);
    return absl::OkStatus();
  }

  absl::StatusOr<uint32_t> Read32(OffsetType offset) const override {
    if (offset + 3 >= Size()) return absl::OutOfRangeError("invalid offset");
    return LittleEndian::Load32(&memory_[offset]);
  }
  absl::Status Write32(OffsetType offset, uint32_t data) override {
    if (offset + 3 >= Size()) return absl::OutOfRangeError("invalid offset");
    LittleEndian::Store32(data, &memory_[offset]);
    return absl::OkStatus();
  }

  std::string memory_;
};

class MockPciDevice : public PciDevice {
 public:
  MockPciDevice(const PciLocation &location,
                std::unique_ptr<PciRegion> config_region,
                std::unique_ptr<PciResources> resources_intf)
      : PciDevice(location, std::move(config_region),
                  std::move(resources_intf)) {}

  MockPciDevice(const PciLocation &location)
      : PciDevice(location, nullptr, nullptr) {}

  MockPciDevice()
      : PciDevice(PciLocation::Make<0, 0, 0, 0>(), nullptr, nullptr) {}

  MOCK_METHOD(PciConfigSpace *, ConfigSpace, (), (const, override));

  MOCK_METHOD(absl::StatusOr<PcieLinkCapabilities>, PcieLinkMaxCapabilities, (),
              (const, override));

  MOCK_METHOD(absl::StatusOr<PcieLinkCapabilities>, PcieLinkCurrentCapabilities,
              (), (const, override));
};

class MockPciTopology : public PciTopologyInterface {
 public:
  MOCK_METHOD(absl::StatusOr<PciNodeMap>, EnumerateAllNodes, (),
              (const, override));

  MOCK_METHOD(std::unique_ptr<PciDevice>, CreateDevice, (const PciLocation &),
              (const, override));

  MOCK_METHOD(absl::StatusOr<std::vector<PciAcpiPath>>, EnumeratePciAcpiPaths,
              (), (const, override));
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_PCI_MOCKS_H_
