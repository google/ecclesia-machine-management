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

#ifndef ECCLESIA_MAGENT_SYSMODEL_X86_FRU_H_
#define ECCLESIA_MAGENT_SYSMODEL_X86_FRU_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/magent/lib/eeprom/smbus_eeprom.h"
#include "ecclesia/magent/lib/ipmi/ipmi.h"

namespace ecclesia {

struct FruInfo {
  std::string product_name;
  std::string manufacturer;
  std::string serial_number;
  std::string part_number;
};

class SysmodelFru {
 public:
  SysmodelFru(FruInfo fru_info);

  // Allow the object to be copyable
  // Make sure that copy construction is relatively light weight.
  // In cases where it is not feasible to copy construct data members,it may
  // make sense to wrap the data member in a shared_ptr.
  SysmodelFru(const SysmodelFru &sysmodel_fru) = default;
  SysmodelFru &operator=(const SysmodelFru &sysmodel_fru) = default;

  absl::string_view GetManufacturer() const;
  absl::string_view GetSerialNumber() const;
  absl::string_view GetPartNumber() const;

 private:
  FruInfo fru_info_;
};

// An interface class for reading System Model FRUs.
class SysmodelFruReaderIntf {
 public:
  virtual ~SysmodelFruReaderIntf() {}

  // Returns a SysmodelFru instance if available.
  virtual std::optional<SysmodelFru> Read() = 0;
};

// FileSysmodelFruReader provides a caching interface for reading FRUs from a
// file. If it is successful in reading the FRU, it will return a copy of that
// successful read for the rest of its lifetime.
class FileSysmodelFruReader : public SysmodelFruReaderIntf {
 public:
  FileSysmodelFruReader(std::string filepath)
      : filepath_(std::move(filepath)) {}

  std::optional<SysmodelFru> Read() override;

 private:
  std::string filepath_;
  // Stores the cached FRU that was read.
  std::optional<SysmodelFru> cached_fru_;
};

// This class provides a caching interface for reading a FRU via the IPMI
// interface.
class IpmiSysmodelFruReader : public SysmodelFruReaderIntf {
 public:
  // An IPMI FRU can be uniquely identified by its fru_id and read from the IPMI
  // interface.
  explicit IpmiSysmodelFruReader(IpmiInterface *ipmi_intf, uint16_t fru_id)
      : ipmi_intf_(ipmi_intf), fru_id_(fru_id) {}

  std::optional<SysmodelFru> Read() override;

 private:
  IpmiInterface *const ipmi_intf_;
  const uint16_t fru_id_;
  // Stores the cached FRU that was read.
  std::optional<SysmodelFru> cached_fru_;
};

// SmbusEepromFruReader provides a caching interface for reading FRUs from a
// SMBUS eeprom. If it is successful in reading the FRU, it will return a copy
// of that successful read for the rest of its lifetime.
class SmbusEepromFruReader : public SysmodelFruReaderIntf {
 public:
  SmbusEepromFruReader(std::unique_ptr<SmbusEeprom> eeprom)
      : eeprom_(std::move(eeprom)) {}

  // If the FRU contents are cached, the cached content is returned. Otherwise
  // performs the low level FRU read, and if successful, populates the cache
  // and returns the read.
  std::optional<SysmodelFru> Read() override;

 private:
  std::unique_ptr<SmbusEeprom> eeprom_;
  // Stores the cached FRU that was read.
  std::optional<SysmodelFru> cached_fru_;
};

// SysmodelFruReaderFactory wraps a lambda for constructing a SysmodelFruReader
// instance.
class SysmodelFruReaderFactory {
 public:
  using FactoryFunction =
      std::function<std::unique_ptr<SysmodelFruReaderIntf>()>;
  SysmodelFruReaderFactory(std::string name, FactoryFunction factory)
      : name_(std::move(name)), factory_(std::move(factory)) {}

  // Returns the name of the associated SysmodelFruReaderIntf.
  absl::string_view Name() const { return name_; }
  // Invokes the FactoryFunction to construct a SysmodelFruReaderIntf instance.
  std::unique_ptr<SysmodelFruReaderIntf> Construct() const {
    return factory_();
  }

 private:
  std::string name_;
  FactoryFunction factory_;
};

// This method generates a map of FruReader names to FruReader instances. The
// FruReader name is the same as that of the FruReader factory. Thus the factory
// names must be unique.
absl::flat_hash_map<std::string, std::unique_ptr<SysmodelFruReaderIntf>>
CreateFruReaders(absl::Span<const SysmodelFruReaderFactory> fru_factories);

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_SYSMODEL_X86_FRU_H_
