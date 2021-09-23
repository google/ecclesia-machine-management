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

// Routines and definitions to handle the FRU data structures. Ref. "IPMI
// Platform Management FRU Information Storage Definition v1.0, Document
// Revision 1.1, September 27, 1999" by "Intel, Hewlett-Packard, NEC, and Dell."
//
// Does not support internal use areas, but support for these
// can be added if needed.

#include "ecclesia/magent/lib/fru/fru.h"

#include <assert.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/casts.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/codec/text.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/magent/lib/fru/common_header.emb.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {

namespace {

absl::Status GetCustomFieldFrom(const std::vector<FruField> &custom_fields,
                                CustomFieldType type, std::string *value) {
  for (const auto &field : custom_fields) {
    FruCustomField custom_field;
    auto status = DecodeCustomField(field, &custom_field);
    if (status.ok() && custom_field.type == type) {
      *value = custom_field.value;
      return absl::OkStatus();
    }
  }
  return absl::NotFoundError(
      absl::StrFormat("Cannot find field: %d from Fru Area", type));
}

}  // namespace

static size_t kParseFailed = 0;

// Utility function to get a single byte from a FruImageSource.
static bool GetByte(const FruImageSource &fru_image_source, size_t index,
                    uint8_t *out_byte) {
  std::vector<unsigned char> bytes;
  if (!fru_image_source.GetBytes(index, 1, &bytes) || bytes.empty()) {
    return false;
  }
  *out_byte = bytes[0];
  return true;
}

// Writes a field to the end of the given FruArea image buffer.
static void AddFieldToFruAreaImage(const FruField &field,
                                   std::vector<unsigned char> *out_image) {
  const std::vector<unsigned char> data = field.GetData();
  out_image->push_back(
      FruTypelenByteFromTypeAndLength(field.GetType(), data.size()));
  out_image->insert(out_image->end(), data.begin(), data.end());
}

// Writes a vector of fields to the end of the given FruArea image buffer.
static void AddFieldsToFruAreaImage(const std::vector<FruField> &fields,
                                    std::vector<unsigned char> *out_image) {
  for (std::vector<FruField>::const_iterator iter = fields.begin();
       iter != fields.end(); ++iter) {
    AddFieldToFruAreaImage(*iter, out_image);
  }
}

// Reads a single field from the given FruArea image. This updates the given
// offset to point to the index one byte past the end of the string that was
// read. Returns true on success.
static bool ReadFieldFromFruAreaImage(const FruImageSource &image,
                                      size_t *offset, FruField *field) {
  // Make sure image is big enough to read typelen byte.
  size_t current_offset = *offset;
  if (current_offset >= image.Size()) {
    return false;
  }

  // Make sure image is big enough to read string.
  uint8_t field_typelen;
  if (!GetByte(image, current_offset++, &field_typelen)) {
    return false;
  }
  FruField::Type field_type =
      FruField::Type(FruTypeFromTypelenByte(field_typelen));
  size_t field_len = FruLengthFromTypelenByte(field_typelen);
  if (field_len >= image.Size() - current_offset) {
    return false;
  }

  // Return the string we read and update the given offset.
  std::vector<unsigned char> field_bytes;
  if (!image.GetBytes(current_offset, field_len, &field_bytes)) {
    return false;
  }
  current_offset += field_len;

  field->SetData(absl::MakeSpan(field_bytes));
  field->SetType(field_type);
  *offset = current_offset;
  return true;
}

// Reads fields from the given FRU image starting at the given offset,
// continuing until an end of fields indicator byte is found or the end of
// the image is reached. This updates the given offset to point to the index
// one byte past the end of the string that was read. Returns true on success.
// Returns false on error (for example, if end of image is reached before the
// end of fields indicator).
static bool ReadFieldsFromFruAreaImage(const FruImageSource &image,
                                       size_t *offset, size_t offset_limit,
                                       std::vector<FruField> *fields) {
  size_t current_offset = *offset;
  // Get the start byte of the first field.
  uint8_t start_byte;
  if (!GetByte(image, current_offset, &start_byte)) {
    return false;
  }
  // Iterate through fields in FRU image until stop byte is found or error
  // occurs.
  while (start_byte != kFruNoMoreFields) {
    FruField f;
    if (!ReadFieldFromFruAreaImage(image, &current_offset, &f)) {
      return false;
    }
    fields->push_back(f);
    // Verify that the offset of the next field lies within the area.
    if (current_offset >= offset_limit) {
      ErrorLog() << "Missing FRU area stop field before offset " << std::hex
                 << std::showbase << current_offset;
      break;
    }
    // Get the start byte of the next field.
    if (!GetByte(image, current_offset, &start_byte)) {
      return false;
    }
  }
  *offset = current_offset;
  return true;
}

// Utility functions to calculate FRU checksums.
static bool FruChecksumFromImage(const FruImageSource &image, size_t offset,
                                 size_t len, uint8_t *out_checksum) {
  std::vector<unsigned char> buf;
  if (!image.GetBytes(offset, len, &buf)) {
    return false;
  }
  if (buf.size() != len) {
    ErrorLog() << "Failed to read Fru, buf size: " << buf.size()
               << " len: " << len << ". Aborting...";
    abort();
  }

  uint8_t sum = 0;
  for (size_t i = 0; i < len; ++i) {
    sum += buf[i];
  }
  *out_checksum = sum;
  return true;
}

uint8_t FruChecksumFromBytes(const uint8_t *image, size_t offset, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 0; i < len; ++i) {
    sum += *(image + offset + i);
  }
  return sum;
}

static bool FruChecksumFromByteArray(const std::vector<unsigned char> &image,
                                     size_t offset, size_t len,
                                     uint8_t *out_checksum) {
  if (offset + len > image.size()) {
    ErrorLog() << "Got offset " << offset << " and  len " << len
               << " which is greater than image size " << image.size();
    return false;
  }
  uint8_t sum = 0;
  for (size_t i = 0; i < len; ++i) {
    sum += image[offset + i];
  }
  *out_checksum = sum;
  return true;
}

const uint8_t ChassisInfoArea::kFormatVersion;
const uint8_t BoardInfoArea::kFormatVersion;
const uint8_t ProductInfoArea::kFormatVersion;
std::string FruField::GetDataAsString() const {
  absl::StatusOr<std::string> maybe_value;
  switch (type_) {
    case kTypeBcdPlus: {  // BCD plus decoding.
      auto maybe_value = ParseBcdPlus(data_);
      if (!maybe_value.ok()) {
        ErrorLog() << maybe_value.status().message();
        return "";
      }
      return *maybe_value;
    }
    case kType6BitAscii: {  // 6-bits ASCII packed decoding
      auto maybe_value = ParseSixBitAscii(data_);
      if (!maybe_value.ok()) {
        ErrorLog() << maybe_value.status().message();
        return "";
      }
      return *maybe_value;
    }
    default: {
      return std::string(data_.begin(), data_.end());
    }
  }
}

void FruField::SetData(const std::string &data) {
  data_.assign(data.begin(), data.end());
}

absl::Status DecodeCustomField(const FruField &fru_field,
                               FruCustomField *custom_field) {
  const std::string raw_data = fru_field.GetDataAsString();

  // A Platforms-defined custom field uses the first byte to specify field type,
  // and the remaining bytes for actual field value.
  if (raw_data.size() < 2) {
    return absl::UnknownError(
        "Custom field is too short to be a Platforms-defined field");
  }

  const uint8_t type_id = raw_data[0];
  if (type_id >= CustomFieldType::kEnd) {
    return absl::UnknownError(absl::StrFormat(
        "Cannot recognize the field type (%u) from custom field", type_id));
  }

  custom_field->type = static_cast<CustomFieldType>(type_id);
  custom_field->value = raw_data.substr(1);

  return absl::OkStatus();
}

static const uint8_t kInternalUseAreaSize = 72;

absl::Status Fru::FillFromImage(const FruImageSource &fru_image, size_t *size) {
  // Verify given FRU image is big enough.
  if (fru_image.Size() < CommonHeader::IntrinsicSizeInBytes()) {
    return absl::InternalError("FRU image is too small");
  }

  // Fill in CommonHeader for the FRU.
  std::vector<uint8_t> common_header_bytes(
      CommonHeader::IntrinsicSizeInBytes());

  if (!fru_image.GetBytes(0, CommonHeader::IntrinsicSizeInBytes(),
                          &common_header_bytes)) {
    return absl::InternalError("Get common header failed");
  }
  auto common_header = MakeCommonHeaderView(&common_header_bytes);

  // Do some basic checking that the CommonHeader is good.
  if (!ValidateFruCommonHeader(common_header_bytes)) {
    return absl::InternalError("Fru common header is invalid");
  }

  // Parse supported FruAreas.
  std::unique_ptr<ChassisInfoArea> chassis_info_area;
  std::unique_ptr<BoardInfoArea> board_info_area;
  std::unique_ptr<ProductInfoArea> product_info_area;
  std::unique_ptr<MultiRecordArea> multi_record_area;

  size_t end_byte_max = CommonHeader::IntrinsicSizeInBytes();
  size_t end_byte = 0;

  if (common_header.internal_use_area_starting_offset().Read() > 0) {
    end_byte_max =
        std::max(end_byte,
                 FruChunksToBytes(
                     common_header.internal_use_area_starting_offset().Read()) +
                     kInternalUseAreaSize);
  }

  if (common_header.chassis_info_area_starting_offset().Read() > 0) {
    chassis_info_area = absl::make_unique<ChassisInfoArea>();
    end_byte = chassis_info_area->FillFromImage(
        fru_image,
        FruChunksToBytes(
            common_header.chassis_info_area_starting_offset().Read()));
    if (!end_byte) {
      return absl::InternalError("Parse chassis info area failed");
    }
    end_byte_max = std::max(end_byte, end_byte_max);
  }

  if (common_header.board_info_area_starting_offset().Read() > 0) {
    board_info_area = absl::make_unique<BoardInfoArea>();
    end_byte = board_info_area->FillFromImage(
        fru_image, FruChunksToBytes(
                       common_header.board_info_area_starting_offset().Read()));
    if (!end_byte) {
      return absl::InternalError("Parse board info area failed");
    }
    end_byte_max = std::max(end_byte, end_byte_max);
  }

  if (common_header.product_info_area_starting_offset().Read() > 0) {
    product_info_area = absl::make_unique<ProductInfoArea>();
    end_byte = product_info_area->FillFromImage(
        fru_image,
        FruChunksToBytes(
            common_header.product_info_area_starting_offset().Read()));
    if (!end_byte) {
      return absl::InternalError("Parse product info area failed");
    }
    end_byte_max = std::max(end_byte, end_byte_max);
  }

  if (common_header.multirecord_area_starting_offset().Read() > 0) {
    multi_record_area = absl::make_unique<MultiRecordArea>();
    end_byte = multi_record_area->FillFromImage(
        fru_image,
        FruChunksToBytes(
            common_header.multirecord_area_starting_offset().Read()));
    if (!end_byte) {
      return absl::InternalError("Parse multi record area failed");
    }
    end_byte_max = std::max(end_byte, end_byte_max);
  }

  chassis_info_area_.swap(chassis_info_area);
  board_info_area_.swap(board_info_area);
  product_info_area_.swap(product_info_area);
  multi_record_area_.swap(multi_record_area);
  *size = end_byte_max;
  return absl::OkStatus();
}

void Fru::GetImage(std::vector<unsigned char> *fru_image) const {
  // Get images for component areas.
  std::vector<unsigned char> chassis_image;
  std::vector<unsigned char> board_image;
  std::vector<unsigned char> product_image;
  std::vector<unsigned char> multi_record_image;
  if (chassis_info_area_) {
    chassis_info_area_->GetImage(&chassis_image);
  }
  if (board_info_area_) {
    board_info_area_->GetImage(&board_image);
  }
  if (product_info_area_) {
    product_info_area_->GetImage(&product_image);
  }
  if (multi_record_area_) {
    multi_record_area_->GetImage(&multi_record_image);
  }

  // Fill in CommonHeader for the FRU.
  size_t area_offset = 1;  // first info area starts at offset 1
  std::vector<uint8_t> common_header_bytes(
      CommonHeader::IntrinsicSizeInBytes());
  auto common_header = MakeCommonHeaderView(&common_header_bytes);

  common_header.format_version().Write(kFormatVersion);

  if (!chassis_image.empty()) {
    common_header.chassis_info_area_starting_offset().Write(area_offset);
    area_offset += FruBytesToChunks(chassis_image.size());
  }
  if (!board_image.empty()) {
    common_header.board_info_area_starting_offset().Write(area_offset);
    area_offset += FruBytesToChunks(board_image.size());
  }
  if (!product_image.empty()) {
    common_header.product_info_area_starting_offset().Write(area_offset);
    area_offset += FruBytesToChunks(product_image.size());
  }
  if (!multi_record_image.empty()) {
    common_header.multirecord_area_starting_offset().Write(area_offset);
    area_offset += FruBytesToChunks(multi_record_image.size());
  }

  common_header.checksum().Write(static_cast<uint8_t>(-FruChecksumFromBytes(
      common_header_bytes.data(), 0, CommonHeader::IntrinsicSizeInBytes())));

  // Write out the final image.
  fru_image->clear();
  fru_image->insert(
      fru_image->end(), common_header_bytes.data(),
      common_header_bytes.data() + CommonHeader::IntrinsicSizeInBytes());
  fru_image->insert(fru_image->end(), chassis_image.begin(),
                    chassis_image.end());
  fru_image->insert(fru_image->end(), board_image.begin(), board_image.end());
  fru_image->insert(fru_image->end(), product_image.begin(),
                    product_image.end());
  fru_image->insert(fru_image->end(), multi_record_image.begin(),
                    multi_record_image.end());
}
void ChassisInfoArea::GetImage(std::vector<unsigned char> *area_image) const {
  // Fill in data.
  area_image->clear();
  // Format version
  area_image->push_back(kFormatVersion);
  // Length byte to fill in later
  area_image->push_back(0);
  // Chassis type
  area_image->push_back(type_);
  // Part number
  AddFieldToFruAreaImage(part_number_, area_image);
  // Serial number
  AddFieldToFruAreaImage(serial_number_, area_image);
  // Custom fields
  AddFieldsToFruAreaImage(custom_fields_, area_image);
  // No more fields byte
  area_image->push_back(kFruNoMoreFields);
  // Make room for a checksum byte to fill in later
  area_image->push_back(0);
  // Pad to a multiple of 8 bytes
  if (area_image->size() % 8 != 0) {
    size_t padded_len = area_image->size() + (8 - (area_image->size() % 8));
    area_image->resize(padded_len);
  }

  // Compute length.
  (*area_image)[1] = FruBytesToChunks(area_image->size());
  // Compute checksum.
  (*area_image)[area_image->size() - 1] =
      -FruChecksumFromBytes(&((*area_image)[0]), 0, area_image->size());
}

absl::Status ChassisInfoArea::GetCustomField(CustomFieldType type,
                                             std::string *value) const {
  return GetCustomFieldFrom(custom_fields_, type, value);
}

bool Fru::ValidateFruCommonHeader(std::vector<uint8_t> &common_header_bytes) {
  auto common_header = MakeCommonHeaderView(&common_header_bytes);
  uint8_t format_version = common_header.format_version().Read();
  if (format_version != kFormatVersion) {
    ErrorLog() << "FRU common header has unsupported format version ("
               << format_version << ")";
    return false;
  }

  uint8_t common_header_checksum = FruChecksumFromBytes(
      common_header_bytes.data(), 0, CommonHeader::IntrinsicSizeInBytes());
  if (common_header_checksum != 0) {
    ErrorLog() << "FRU common header has invalid checksum (should be 0, is "
               << absl::implicit_cast<uint32_t>(common_header_checksum) << ")";
    return false;
  }

  return true;
}

size_t ChassisInfoArea::FillFromImage(const FruImageSource &fru_image,
                                      size_t area_offset) {
  // Sanity check on given vector: we need at least two bytes at this point to
  // check format version and info area length.
  if (area_offset + 1 >= fru_image.Size()) {
    ErrorLog() << "Given image too small";
    return kParseFailed;
  }

  size_t current_offset = area_offset;

  // Read format version byte.
  uint8_t format_version_byte;
  if (!GetByte(fru_image, current_offset++, &format_version_byte)) {
    return kParseFailed;
  }
  if (format_version_byte != kFormatVersion) {
    ErrorLog() << "Chassis info area has unsupported format version";
    return kParseFailed;
  }

  // Check info area size.
  uint8_t size_byte;
  if (!GetByte(fru_image, current_offset++, &size_byte)) {
    return kParseFailed;
  }
  size_t size = FruChunksToBytes(size_byte);
  if (area_offset + size > fru_image.Size()) {
    ErrorLog() << "Given image too small according to chassis info area "
                  "length field";
    return kParseFailed;
  }

  // Verify info area checksum.
  uint8_t checksum;
  if (!FruChecksumFromImage(fru_image, area_offset, size, &checksum)) {
    ErrorLog() << "Could not compute chassis info area checksum";
    return kParseFailed;
  }
  if (checksum != 0) {
    ErrorLog() << "Chassis info area checksum invalid (should be 0, is "
               << checksum << ")";
    return kParseFailed;
  }

  // Read in values.
  uint8_t type_byte;
  if (!GetByte(fru_image, current_offset++, &type_byte)) {
    return kParseFailed;
  }
  Type type = static_cast<Type>(type_byte);

  // Set the initial value of serial number's offset to be the offset to the
  // beginning of this area's fields.
  size_t serial_number_offset = current_offset;

  std::vector<FruField> fields;
  ReadFieldsFromFruAreaImage(fru_image, &current_offset, area_offset + size,
                             &fields);
  if (fields.size() < kRequiredFieldCount) {
    ErrorLog() << "Field count in chassis info area is too small";
    return kParseFailed;
  }

  type_ = type;
  part_number_ = fields[kPartNumberIndex];
  // All the +1s in the computation of the serial number offset are to account
  // for the 1 byte field length information preceding each field.
  serial_number_offset += part_number_.GetData().size() + 1;
  serial_number_ = fields[kSerialNumberIndex];
  ++serial_number_offset;
  custom_fields_.assign(fields.begin() + kCustomFieldsIndex, fields.end());

  // Add serial number location information.
  variable_fields_location_.push_back(
      {serial_number_offset, serial_number_.GetData().size()});
  // Add checksum location information. Checksum is the last byte of the area.
  variable_fields_location_.push_back({current_offset - 1, 1});
  return area_offset + size;
}

std::string ChassisInfoArea::TypeAsString(Type type) {
  switch (type) {
    case kOther:
      return "Other";
    case kUnknown:
      return "Unknown";
    case kDesktop:
      return "Desktop";
    case kLowProfileDesktop:
      return "Low Profile Desktop";
    case kPizzaBox:
      return "Pizza Box";
    case kMiniTower:
      return "Mini Tower";
    case kTower:
      return "Tower";
    case kPortable:
      return "Portable";
    case kLaptop:
      return "LapTop";
    case kNotebook:
      return "Notebook";
    case kHandHeld:
      return "Hand Held";
    case kDockingStation:
      return "Docking Station";
    case kAllInOne:
      return "All in One";
    case kSubNotebook:
      return "Sub Notebook";
    case kSpaceSaving:
      return "Space-saving";
    case kLunchBox:
      return "Lunch Box";
    case kMainServerChassis:
      return "Main Server Chassis";
    case kExpansionChassis:
      return "Expansion Chassis";
    case kSubChassis:
      return "SubChassis";
    case kBusExpansionChassis:
      return "Bus Expansion Chassis";
    case kPeripheralChassis:
      return "Peripheral Chassis";
    case kRaidChassis:
      return "RAID Chassis";
    case kRackMountChassis:
      return "Rack Mount Chassis";
    default:
      return "";
  }
}

void BoardInfoArea::GetImage(std::vector<unsigned char> *area_image) const {
  // Fill in data.
  area_image->clear();
  // Format version
  area_image->push_back(kFormatVersion);
  // Length byte to fill in later
  area_image->push_back(0);
  // Language code
  area_image->push_back(language_code_);
  // Manufacture date in number of minutes from 0:00 hrs 1/1/1996
  // in little endian byte order.
  uint32_t manufacture_date_mins = (manufacture_date_ - kFruEpoch) / 60;
  area_image->push_back(manufacture_date_mins);
  area_image->push_back(manufacture_date_mins >> 8);
  area_image->push_back(manufacture_date_mins >> 16);
  // Manufacturer
  AddFieldToFruAreaImage(manufacturer_, area_image);
  // Product name
  AddFieldToFruAreaImage(product_name_, area_image);
  // Serial number
  AddFieldToFruAreaImage(serial_number_, area_image);
  // Part number
  AddFieldToFruAreaImage(part_number_, area_image);
  // File ID
  AddFieldToFruAreaImage(file_id_, area_image);
  // Custom fields
  AddFieldsToFruAreaImage(custom_fields_, area_image);
  // No more fields byte
  area_image->push_back(kFruNoMoreFields);
  // Make room for a checksum byte to fill in later
  area_image->push_back(0);
  // Pad to a multiple of 8 bytes
  if (area_image->size() % 8 != 0) {
    size_t padded_len = area_image->size() + (8 - (area_image->size() % 8));
    area_image->resize(padded_len);
  }

  // Compute length.
  (*area_image)[1] = FruBytesToChunks(area_image->size());
  // Compute checksum.
  (*area_image)[area_image->size() - 1] =
      -FruChecksumFromBytes(&((*area_image)[0]), 0, area_image->size());
}

size_t BoardInfoArea::FillFromImage(const FruImageSource &fru_image,
                                    size_t area_offset) {
  // Sanity check on given vector: we need at least two bytes at this point to
  // check format version and info area length.
  if (area_offset + 1 >= fru_image.Size()) {
    ErrorLog() << "Given image too small";
    return kParseFailed;
  }

  size_t current_offset = area_offset;

  // Read format version byte.
  uint8_t format_version_byte;
  if (!GetByte(fru_image, current_offset++, &format_version_byte)) {
    return kParseFailed;
  }
  if (format_version_byte != kFormatVersion) {
    ErrorLog() << "Board info area has unsupported format version";
    return kParseFailed;
  }

  // Check info area size.
  uint8_t size_byte;
  if (!GetByte(fru_image, current_offset++, &size_byte)) {
    return kParseFailed;
  }
  size_t size = FruChunksToBytes(size_byte);
  if (area_offset + size > fru_image.Size()) {
    ErrorLog() << "Given image too small according to board info area "
                  "length field";
    return kParseFailed;
  }

  // Verify info area checksum.
  uint8_t checksum;
  if (!FruChecksumFromImage(fru_image, area_offset, size, &checksum)) {
    ErrorLog() << "Could not compute board info area checksum";
    return kParseFailed;
  }
  if (checksum != 0) {
    ErrorLog() << absl::StrFormat(
        "Board info area checksum invalid (should be 0, is 0x%x).", checksum);
    return kParseFailed;
  }

  // Read in values.
  uint8_t language_code;
  if (!GetByte(fru_image, current_offset++, &language_code)) {
    return kParseFailed;
  }

  // Manufacture date is 3 bytes in little endian order.
  uint8_t manufacture_date_bytes[3] = {0};

  // Add manufacture date location information.
  variable_fields_location_.push_back({current_offset, 3});
  for (size_t i = 0; i < 3; ++i) {
    if (!GetByte(fru_image, current_offset++, &manufacture_date_bytes[i])) {
      return kParseFailed;
    }
  }
  // Number of minutes from 0:00 hrs 1/1/1996.
  uint32_t manufacture_date_val = manufacture_date_bytes[0] |
                                  (manufacture_date_bytes[1] << 8) |
                                  (manufacture_date_bytes[2] << 16);
  time_t manufacture_date = kFruEpoch + (manufacture_date_val * 60);

  // Set the initial value of serial number's offset to be the offset to the
  // beginning of this area's fields.
  size_t serial_number_offset = current_offset;
  std::vector<FruField> fields;
  ReadFieldsFromFruAreaImage(fru_image, &current_offset, area_offset + size,
                             &fields);
  if (fields.size() < kRequiredFieldCount) {
    ErrorLog() << "Field count in board info area is too small";
    return kParseFailed;
  }

  language_code_ = language_code;
  manufacture_date_ = manufacture_date;
  // All the +1s in the computation of the serial number offset are to account
  // for the 1 byte field length information preceding each field.
  manufacturer_ = fields[kManufacturerIndex];
  serial_number_offset += manufacturer_.GetData().size() + 1;
  product_name_ = fields[kProductNameIndex];
  serial_number_offset += product_name_.GetData().size() + 1;
  serial_number_ = fields[kSerialNumberIndex];
  ++serial_number_offset;
  part_number_ = fields[kPartNumberIndex];
  file_id_ = fields[kFileIdIndex];
  custom_fields_.assign(fields.begin() + kCustomFieldsIndex, fields.end());

  // Add serial number location information.
  variable_fields_location_.push_back(
      {serial_number_offset, serial_number_.GetData().size()});
  // Add checksum location information. Checksum is the last byte of the area.
  variable_fields_location_.push_back({current_offset - 1, 1});
  return area_offset + size;
}

bool BoardInfoArea::set_language_code(uint8_t language_code) {
  // Verify given language code is in range.
  assert(language_code <= kMaxLanguageCode);

  language_code_ = language_code;
  return true;
}

bool BoardInfoArea::set_manufacture_date(time_t manufacture_date) {
  // Verify given time is after the FRU epoch.
  if (manufacture_date < kFruEpoch) {
    return false;
  }
  manufacture_date_ = manufacture_date;
  return true;
}

absl::Status BoardInfoArea::GetCustomField(CustomFieldType type,
                                           std::string *value) const {
  return GetCustomFieldFrom(custom_fields_, type, value);
}

void ProductInfoArea::GetImage(std::vector<unsigned char> *area_image) const {
  // Fill in data.
  area_image->clear();
  // Format version
  area_image->push_back(kFormatVersion);
  // Length byte to fill in later
  area_image->push_back(0);
  // Language code
  area_image->push_back(language_code_);
  // Manufacturer
  AddFieldToFruAreaImage(manufacturer_, area_image);
  // Product name
  AddFieldToFruAreaImage(product_name_, area_image);
  // Part number
  AddFieldToFruAreaImage(part_number_, area_image);
  // Product version
  AddFieldToFruAreaImage(product_version_, area_image);
  // Serial number
  AddFieldToFruAreaImage(serial_number_, area_image);
  // Asset tag
  AddFieldToFruAreaImage(asset_tag_, area_image);
  // File ID
  AddFieldToFruAreaImage(file_id_, area_image);
  // Custom fields
  AddFieldsToFruAreaImage(custom_fields_, area_image);
  // No more fields byte
  area_image->push_back(kFruNoMoreFields);
  // Make room for a checksum byte to fill in later
  area_image->push_back(0);
  // Pad to a multiple of 8 bytes
  if (area_image->size() % 8 != 0) {
    size_t padded_len = area_image->size() + (8 - (area_image->size() % 8));
    area_image->resize(padded_len);
  }

  // Compute length.
  (*area_image)[1] = FruBytesToChunks(area_image->size());
  // Compute checksum.
  (*area_image)[area_image->size() - 1] =
      -FruChecksumFromBytes(&((*area_image)[0]), 0, area_image->size());
}

size_t ProductInfoArea::FillFromImage(const FruImageSource &fru_image,
                                      size_t area_offset) {
  // Sanity check on given vector: we need at least two bytes at this point to
  // check format version and info area length.
  if (area_offset + 1 >= fru_image.Size()) {
    ErrorLog() << "Given image too small";
    return kParseFailed;
  }

  size_t current_offset = area_offset;

  // Read format version byte.
  uint8_t format_version_byte;
  if (!GetByte(fru_image, current_offset++, &format_version_byte)) {
    return kParseFailed;
  }
  if (format_version_byte != kFormatVersion) {
    ErrorLog() << "Product info area has unsupported format version";
    return kParseFailed;
  }

  // Check info area size.
  uint8_t size_byte;
  if (!GetByte(fru_image, current_offset++, &size_byte)) {
    return kParseFailed;
  }
  size_t size = FruChunksToBytes(size_byte);
  if (area_offset + size > fru_image.Size()) {
    ErrorLog() << "Given image too small according to product info area "
                  "length field";
    return kParseFailed;
  }

  // Verify info area checksum.
  uint8_t checksum;
  if (!FruChecksumFromImage(fru_image, area_offset, size, &checksum)) {
    ErrorLog() << "Could not compute product info area checksum";
    return kParseFailed;
  }
  if (checksum != 0) {
    ErrorLog() << "Product info area checksum invalid (should be 0, is "
               << checksum << ")";
    return kParseFailed;
  }

  // Read in values.
  uint8_t language_code;
  if (!GetByte(fru_image, current_offset++, &language_code)) {
    return kParseFailed;
  }

  // Set the initial value of serial number's offset to be the offset to the
  // beginning of this area's fields.
  size_t serial_number_offset = current_offset;

  std::vector<FruField> fields;
  ReadFieldsFromFruAreaImage(fru_image, &current_offset, area_offset + size,
                             &fields);
  if (fields.size() < kRequiredFieldCount) {
    ErrorLog() << "Field count in product info area is too small";
    return kParseFailed;
  }

  language_code_ = language_code;
  manufacturer_ = fields[kManufacturerIndex];
  // All the +1s in the computation of the serial number offset are to account
  // for the 1 byte field length information preceding each field.
  serial_number_offset += manufacturer_.GetData().size() + 1;
  product_name_ = fields[kProductNameIndex];
  serial_number_offset += product_name_.GetData().size() + 1;
  part_number_ = fields[kPartNumberIndex];
  serial_number_offset += part_number_.GetData().size() + 1;
  product_version_ = fields[kProductVersionIndex];
  serial_number_offset += product_version_.GetData().size() + 1;
  serial_number_ = fields[kSerialNumberIndex];
  ++serial_number_offset;
  asset_tag_ = fields[kAssetTagIndex];
  file_id_ = fields[kFileIdIndex];
  custom_fields_.assign(fields.begin() + kCustomFieldsIndex, fields.end());

  // Add serial number location information.
  variable_fields_location_.push_back(
      {serial_number_offset, serial_number_.GetData().size()});
  // Add checksum information. Checksum is the last byte in the area image.
  variable_fields_location_.push_back({current_offset - 1, 1});
  return area_offset + size;
}

bool ProductInfoArea::set_language_code(uint8_t language_code) {
  // Verify given language code is in range.
  assert(language_code <= kMaxLanguageCode);

  language_code_ = language_code;
  return true;
}

absl::Status ProductInfoArea::GetCustomField(CustomFieldType type,
                                             std::string *value) const {
  return GetCustomFieldFrom(custom_fields_, type, value);
}

void MultiRecord::GetImage(bool end_of_list,
                           std::vector<unsigned char> *bytes) const {
  bytes->clear();
  bytes->push_back(type_id_);
  uint8_t byte1 = 0;
  if (end_of_list) {
    byte1 |= 0x80;
  }
  byte1 |= record_format_version_;
  bytes->push_back(byte1);

  uint8_t record_checksum;
  FruChecksumFromByteArray(data_, 0, data_.size(), &record_checksum);
  bytes->push_back(data_.size());
  bytes->push_back(-record_checksum);

  uint8_t header_checksum;
  FruChecksumFromByteArray(*bytes, 0, bytes->size(), &header_checksum);
  bytes->push_back(-header_checksum);
  bytes->insert(bytes->end(), data_.begin(), data_.end());
}

bool MultiRecord::FillFromImage(const FruImageSource &fru_image,
                                size_t area_offset, bool *end_of_list) {
  if (fru_image.Size() < area_offset + kMultiRecordHeaderSize) {
    ErrorLog() << "MultiRecord too small.";
    return false;
  }
  std::vector<unsigned char> header;
  if (!fru_image.GetBytes(area_offset, kMultiRecordHeaderSize, &header)) {
    return false;
  }
  uint8_t type_id = header[0];
  *end_of_list = header[1] & 0x80;
  uint8_t record_format_version = header[1] & 0x0F;
  uint8_t data_length = header[2];
  uint8_t record_checksum = header[3];

  // Check header checksum.
  uint8_t header_checksum;
  FruChecksumFromByteArray(header, 0, kMultiRecordHeaderSize, &header_checksum);
  if (header_checksum) {
    ErrorLog() << "MultiRecord Header Checksum failed got: " << std::hex
               << absl::implicit_cast<int>(header_checksum);
    return false;
  }

  if (data_length) {
    std::vector<unsigned char> data;
    if (!fru_image.GetBytes(area_offset + kMultiRecordHeaderSize, data_length,
                            &data)) {
      return false;
    }
    uint8_t record_sum;
    FruChecksumFromByteArray(data, 0, data.size(), &record_sum);
    uint8_t checksum_result = record_checksum + record_sum;
    if (checksum_result) {
      ErrorLog() << "MultiRecord Data Checksum failed got: " << std::hex
                 << absl::implicit_cast<int>(record_checksum) << " and sum "
                 << absl::implicit_cast<int>(record_sum)
                 << "result: " << absl::implicit_cast<int>(checksum_result);
      return false;
    }
    data_ = data;
  } else {
    data_.clear();
  }

  type_id_ = type_id;
  record_format_version_ = record_format_version;
  return true;
}

bool MultiRecordArea::AddMultiRecord(const MultiRecord &multi_record) {
  multi_records_.push_back(multi_record);
  return true;
}

bool MultiRecordArea::GetMultiRecord(size_t position,
                                     MultiRecord *record) const {
  if (position >= multi_records_.size()) {
    ErrorLog() << "Position " << position << "is out of range of record list "
               << multi_records_.size();
    return false;
  }
  *record = multi_records_[position];
  return true;
}

bool MultiRecordArea::RemoveMultiRecord(size_t position) {
  if (position >= multi_records_.size()) {
    ErrorLog() << "Position " << position << "is out of range of record list "
               << multi_records_.size();
    return false;
  }
  auto iter = multi_records_.begin() + position;
  multi_records_.erase(iter);
  return true;
}

bool MultiRecordArea::UpdateMultiRecord(size_t position,
                                        const MultiRecord &record) {
  if (position >= multi_records_.size()) {
    ErrorLog() << "Position " << position << "is out of range of record list "
               << multi_records_.size();
    return false;
  }
  multi_records_[position] = record;
  return true;
}

void MultiRecordArea::GetImage(std::vector<unsigned char> *area_image) const {
  area_image->clear();
  std::vector<unsigned char> record_image;
  if (multi_records_.empty()) {
    return;
  }
  // This is safe because the prior check guarantees the list as at least
  // one element.
  auto end_iter = --multi_records_.end();
  for (auto iter = multi_records_.begin(); iter != end_iter; ++iter) {
    iter->GetImage(false, &record_image);
    area_image->insert(area_image->end(), record_image.begin(),
                       record_image.end());
  }
  multi_records_.back().GetImage(true, &record_image);
  area_image->insert(area_image->end(), record_image.begin(),
                     record_image.end());
}

size_t MultiRecordArea::FillFromImage(const FruImageSource &fru_image,
                                      size_t area_offset) {
  size_t current_offset = area_offset;
  std::vector<MultiRecord> record_list;
  bool end_of_list;
  while (true) {
    MultiRecord record;
    if (!record.FillFromImage(fru_image, current_offset, &end_of_list)) {
      return false;
    }
    record_list.push_back(record);
    current_offset += record.get_size();
    if (end_of_list) {
      multi_records_ = std::move(record_list);
      return current_offset;
    }
  }
}

bool VectorFruImageSource::GetBytes(
    size_t index, size_t num_bytes,
    std::vector<unsigned char> *out_bytes) const {
  if (index + num_bytes > fru_image_.size()) {
    return false;
  }

  out_bytes->assign(fru_image_.begin() + index,
                    fru_image_.begin() + index + num_bytes);
  return true;
}

}  // namespace ecclesia
