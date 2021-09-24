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

#include "ecclesia/magent/sysmodel/x86/fru.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/magent/lib/eeprom/smbus_eeprom.h"
#include "ecclesia/magent/lib/fru/fru.h"
#include "ecclesia/magent/lib/ipmi/ipmi.h"

namespace ecclesia {

namespace {

constexpr int kFruCommonHeaderSize = 8;
constexpr int kFruBoardInfoAreaSizeIndex = 9;

// Given an input string, returns a new string that contains only the portion
// of the string up to (but not including) the first NUL character. If the
// input string contains no NUL characters than the returned value will be
// equal to the input.
//
// The implementation is intended to be equivalent to:
//    return s.c_str();
// but we want to avoid this because many linting tools will (correctly) flag
// string -> char* -> string conversions as possible bugs.

std::string StringUpToNul(std::string s) { return s.substr(0, s.find('\0')); }

absl::Status ValidateFruCommonHeader(
    absl::Span<const unsigned char> common_header) {
  uint8_t sum = 0;

  for (size_t i = 0; i < common_header.size(); i++) {
    sum += common_header[i];
  }

  if (sum != 0) {
    return absl::InternalError(absl::StrFormat(
        "Fru common header has invalid checksum(should be 0, is %u).", sum));
  }

  return absl::OkStatus();
}

absl::Status GetBoardInfoAreaSize(const SmbusEeprom &eeprom, size_t *size) {
  std::vector<unsigned char> value(1);
  absl::Status status;

  if (1 != eeprom.ReadBytes(kFruBoardInfoAreaSizeIndex,
                            absl::MakeSpan(value.data(), 1))) {
    return absl::InternalError("Failed to read fru BoardInfoArea size.");
  }

  // size is in multple of 8 bytes
  *size = value[0] << 3;

  return status;
}

// Processes fru_data into info if the fru_data is valid.
absl::Status ProcessBoardFromFruImage(absl::Span<unsigned char> fru_data,
                                      FruInfo &info) {
  absl::Status status = ValidateFruCommonHeader(
      absl::MakeSpan(fru_data.data(), kFruCommonHeaderSize));
  if (!status.ok()) {
    return status;
  }

  VectorFruImageSource fru_image(fru_data);
  BoardInfoArea board_info;

  // BoardInfoArea is right after common header,
  // starts from offset: 0 + kFruCommonHeaderSize
  board_info.FillFromImage(fru_image, 0 + kFruCommonHeaderSize);

  info = {
      .product_name =
          StringUpToNul(board_info.product_name().GetDataAsString()),
      .manufacturer =
          StringUpToNul(board_info.manufacturer().GetDataAsString()),
      .serial_number =
          StringUpToNul(board_info.serial_number().GetDataAsString()),
      .part_number = StringUpToNul(board_info.part_number().GetDataAsString())};
  return absl::OkStatus();
}

absl::Status SmbusGetBoardInfo(const SmbusEeprom &eeprom, FruInfo &info) {
  absl::Status status;
  size_t board_info_area_size;
  status = GetBoardInfoAreaSize(eeprom, &board_info_area_size);
  if (!status.ok()) {
    return status;
  }

  // We will read the common header(8 bytes) and BoardInfoArea
  std::vector<unsigned char> fru_data(
      board_info_area_size + kFruCommonHeaderSize, 0);

  // We read from offset 0, this will have common header
  eeprom.ReadBytes(0, absl::MakeSpan(fru_data.data(), fru_data.size()));

  return ProcessBoardFromFruImage(absl::MakeSpan(fru_data), info);
}

}  // namespace

SysmodelFru::SysmodelFru(FruInfo fru_info) : fru_info_(std::move(fru_info)) {}

absl::string_view SysmodelFru::GetManufacturer() const {
  return fru_info_.manufacturer;
}
absl::string_view SysmodelFru::GetSerialNumber() const {
  return fru_info_.serial_number;
}
absl::string_view SysmodelFru::GetPartNumber() const {
  return fru_info_.part_number;
}

std::optional<SysmodelFru> IpmiSysmodelFruReader::Read() {
  if (cached_fru_.has_value()) return cached_fru_;

  // 8 bytes header followed by 64 bytes boardinfo.
  std::vector<uint8_t> data(72);
  absl::Status status = ipmi_intf_->ReadFru(fru_id_, 0, absl::MakeSpan(data));
  if (!status.ok()) {
    return std::nullopt;
  }
  VectorFruImageSource fru_image(absl::MakeSpan(data));
  BoardInfoArea bia;
  if (bia.FillFromImage(fru_image, 8) == 0) {
    return std::nullopt;
  }
  FruInfo fru_info;
  fru_info.manufacturer = bia.manufacturer().GetDataAsString();
  fru_info.product_name = bia.product_name().GetDataAsString();
  fru_info.part_number = bia.part_number().GetDataAsString();
  fru_info.serial_number = bia.serial_number().GetDataAsString();
  cached_fru_.emplace(fru_info);
  return cached_fru_;
}

std::optional<SysmodelFru> SmbusEepromFruReader::Read() {
  // If we have something valid in the cache, return it.
  if (cached_fru_.has_value()) return cached_fru_;

  // Otherwise try to read it into the cache for the first time.
  if (!eeprom_) return std::nullopt;
  FruInfo info;
  absl::Status status = SmbusGetBoardInfo(*eeprom_, info);
  if (!status.ok()) return std::nullopt;
  cached_fru_.emplace(info);
  return cached_fru_;
}

std::optional<SysmodelFru> FileSysmodelFruReader::Read() {
  // If we have something valid in the cache, return it.
  if (cached_fru_.has_value()) return cached_fru_;

  // Otherwise try to read it into the cache for the first time.
  std::ifstream file(filepath_, std::ios::binary);
  if (!file.is_open()) return std::nullopt;

  // Do not skip newlines in binary mode.
  file.unsetf(std::ios::skipws);
  std::vector<unsigned char> fru_data;
  fru_data.reserve(file.tellg());
  fru_data.insert(fru_data.begin(), std::istream_iterator<unsigned char>(file),
                  std::istream_iterator<unsigned char>());

  // Return if we failed to read anything
  if (fru_data.empty()) return std::nullopt;

  FruInfo info;
  absl::Status status =
      ProcessBoardFromFruImage(absl::MakeSpan(fru_data), info);
  if (!status.ok()) return std::nullopt;
  cached_fru_.emplace(info);
  return cached_fru_;
}

absl::flat_hash_map<std::string, std::unique_ptr<SysmodelFruReaderIntf>>
CreateFruReaders(absl::Span<const SysmodelFruReaderFactory> fru_factories) {
  absl::flat_hash_map<std::string, std::unique_ptr<SysmodelFruReaderIntf>>
      frus_map;

  for (const SysmodelFruReaderFactory &factory : fru_factories) {
    frus_map.emplace(factory.Name(), factory.Construct());
  }

  return frus_map;
}

}  // namespace ecclesia
