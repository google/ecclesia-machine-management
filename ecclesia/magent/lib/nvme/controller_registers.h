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

#ifndef ECCLESIA_MAGENT_LIB_NVME_CONTROLLER_REGISTERS_H_
#define ECCLESIA_MAGENT_LIB_NVME_CONTROLLER_REGISTERS_H_

#include <assert.h>

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/magent/lib/nvme/controller_registers.emb.h"
#include "runtime/cpp/emboss_cpp_util.h"

namespace ecclesia {

// Class to read the controller registers for an NVMe device
class ControllerRegisters {
 public:
  // buf is the first 4k bytes of the memory region pointed by the
  // PCI BAR0 and BAR1 registers for the NVMe device.
  // Note that this region is typically exported in linux as the resource0 file
  // for the PCI device in sysfs.
  // Reference:https://www.kernel.org/doc/Documentation/filesystems/sysfs-pci.txt
  static absl::StatusOr<ControllerRegisters> Parse(absl::string_view buf) {
    auto message_view =
        MakeControllerRegistersStructureView(buf.data(), buf.size());
    if (!message_view.Ok()) {
      return absl::InvalidArgumentError("Error parsing the buf.");
    }
    return ControllerRegisters(buf);
  }

  ControllerRegisters(const ControllerRegisters &) = delete;
  ControllerRegisters &operator=(const ControllerRegisters &) = delete;

  ControllerRegisters(ControllerRegisters &&) = default;
  ControllerRegisters &operator=(ControllerRegisters &&) = default;

  ControllerRegistersStructureView GetMessageView() const {
    auto message_view =
        MakeControllerRegistersStructureView(data_.data(), data_.size());
    assert(message_view.Ok());
    return message_view;
  }

 protected:
  explicit ControllerRegisters(absl::string_view buf) : data_(buf) {}

 private:
  std::string data_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_NVME_CONTROLLER_REGISTERS_H_
