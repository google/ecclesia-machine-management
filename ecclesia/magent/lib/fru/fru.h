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

#ifndef ECCLESIA_MAGENT_LIB_FRU_FRU_H_
#define ECCLESIA_MAGENT_LIB_FRU_FRU_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"

namespace ecclesia {

// Some general FRU-related utility methods and constants.
// These are exposed for testing purposes.
const uint8_t kFruNoMoreFields = 0xc1;
inline size_t FruChunksToBytes(uint8_t chunks) { return chunks * 8; }
inline uint8_t FruBytesToChunks(size_t bytes) { return (bytes + 7) / 8; }
inline uint8_t FruTypeFromTypelenByte(uint8_t typelen) { return typelen >> 6; }
inline size_t FruLengthFromTypelenByte(uint8_t typelen) {
  return typelen & 0x3f;
}
// Encode a type/length byte from the given length byte, setting the
// type to 0b11 (indicating interpretation depends on language code, which
// for English means 8-bit ASCII + Latin 1) or 0b00 (indicating unspecified
// type) iff the given length is zero.
inline uint8_t FruTypelenByteFromLength(uint8_t length) {
  return length == 0 ? 0x00 : (0x03 << 6) | (length & 0x3f);
}
inline uint8_t FruTypelenByteFromTypeAndLength(uint8_t type, uint8_t length) {
  return length == 0 ? 0x00 : (type << 6) | (length & 0x3f);
}
uint8_t FruChecksumFromBytes(const uint8_t *image, size_t offset, size_t len);

// Forward declarations.
class BoardInfoArea;
class ChassisInfoArea;
class FruImageSource;
class MultiRecordArea;
class ProductInfoArea;

// This class represents the data encapsulated by a single FRU field (e.g.
// "product name," "serial number," etc. In most cases, fields contain
// printable English strings, which can be retrieved with GetDataAsString. For
// fields with more complicated contents, the GetType and GetData methods are
// provided.
class FruField {
 public:
  enum Type {
    kTypeBinary = 0x00,
    kTypeBcdPlus = 0x01,
    kType6BitAscii = 0x02,
    kTypeLanguageBased = 0x03
  };

  FruField() : type_(kTypeBinary), data_() {}
  ~FruField() {}

  // True if this FruField's contents are equal to that of the given FruField.
  bool Equals(const FruField &other) const {
    return GetType() == other.GetType() && GetData() == other.GetData();
  }

  // Accessors and mutators.
  Type GetType() const { return type_; }
  void SetType(Type type) { type_ = type; }
  std::vector<unsigned char> GetData() const { return data_; }
  void SetData(absl::Span<unsigned char> data) {
    data_.resize(data.size());
    std::copy(data.begin(), data.end(), data_.begin());
  }

  // Convenience methods for retrieving and setting the data contained in this
  // FruField in string form.
  std::string GetDataAsString() const;
  void SetData(const std::string &data);

 private:
  Type type_;
  std::vector<unsigned char> data_;
};

enum CustomFieldType {
  kUnknownCustomFieldType = 0,
  kFabId = 1,
  kFirmwareId = 2,
  kEnd
};

// Platforms-defined FRU custom field.
struct FruCustomField {
  CustomFieldType type;
  std::string value;
};

// Decodes a custom FRU field using Platforms-defined format.
absl::Status DecodeCustomField(const FruField &fru_field,
                               FruCustomField *custom_field);

class Fru {
 public:
  Fru() {}  // Makes a new, empty Fru
  Fru(const Fru &) = delete;
  Fru &operator=(const Fru &) = delete;

  ~Fru() {}

  // Fills in the fields of the Fru by reading the given image. Return the
  // offset of the last byte written to on success or the detailed error status
  absl::Status FillFromImage(const FruImageSource &fru_image, size_t *size);

  // Outputs the underlying raw FRU data to the given buffer (resizing it as
  // necessary). Any existing data in the given buffer will be overwritten.
  // NOTE: Since not all info areas are supported, GetImage will not necessarily
  // output the same data as that read into FillFromImage, even if nothing but
  // const methods are called after FillFromImage.
  void GetImage(std::vector<unsigned char> *fru_image) const;

  // Returns a pointer to the requested info area, or NULL if it doesn't exist.
  // Fru maintains ownership of the returned pointer.
  ChassisInfoArea *GetChassisInfoArea() { return chassis_info_area_.get(); }
  BoardInfoArea *GetBoardInfoArea() { return board_info_area_.get(); }
  ProductInfoArea *GetProductInfoArea() { return product_info_area_.get(); }
  MultiRecordArea *GetMultiRecordArea() { return multi_record_area_.get(); }

  // Replaces the indicated info area with the given one. Takes ownership.
  void SetChassisInfoArea(ChassisInfoArea *chassis_info_area) {
    chassis_info_area_.reset(chassis_info_area);
  }
  void SetBoardInfoArea(BoardInfoArea *board_info_area) {
    board_info_area_.reset(board_info_area);
  }
  void SetProductInfoArea(ProductInfoArea *product_info_area) {
    product_info_area_.reset(product_info_area);
  }
  void SetMultiRecordArea(MultiRecordArea *multi_record_area) {
    multi_record_area_.reset(multi_record_area);
  }

  // Does some basic checking to validate the contents of a FRU common header.
  static bool ValidateFruCommonHeader(
      std::vector<uint8_t> &common_header_bytes);

 private:
  static constexpr uint8_t kFormatVersion = 1;  // The only supported version.

  std::unique_ptr<ChassisInfoArea> chassis_info_area_;
  std::unique_ptr<BoardInfoArea> board_info_area_;
  std::unique_ptr<ProductInfoArea> product_info_area_;
  std::unique_ptr<MultiRecordArea> multi_record_area_;
};

// Pure virtual base class for the various FRU info areas.
class FruArea {
 public:
  // Location information of a field (such as serial number, checksum etc.).
  struct FieldLocation {
    size_t offset;
    size_t size;
  };

  // Outputs the underlying raw bytes for this FruArea to the given buffer
  // (resizing it as necessary). Any existing data in the given buffer will
  // be overwritten.
  virtual void GetImage(std::vector<unsigned char> *out_image) const = 0;

  // Fills in the fields of this FruArea from the data contained in the
  // given buffer. The buffer is read starting at the given offset. Returns 0
  // and changes nothing on error. Returns the offset of the last byte written
  // to on success.
  virtual size_t FillFromImage(const FruImageSource &fru_image,
                               size_t area_offset) = 0;

  // Get location information for variable fields in the area.
  virtual std::vector<FieldLocation> GetVariableFieldsLocation() const = 0;

  // Get Platforms-defined custom field from the area.  If multiple fields match
  // the type, returns the first one.
  virtual absl::Status GetCustomField(CustomFieldType type,
                                      std::string *value) const = 0;

 protected:
  FruArea() {}
  FruArea(const FruArea &) = delete;
  FruArea &operator=(const FruArea &) = delete;

  virtual ~FruArea() {}

  // Some constants common to all FruAreas.
  // 1/1/96 0:00 GMT in Unix epoch seconds.
  static constexpr time_t kFruEpoch = 820454400;
  static constexpr uint8_t kMaxLanguageCode = 136;
};

// Class to parse and/or build the chassis info area of a FRU.
class ChassisInfoArea : public FruArea {
 public:
  enum Type {
    kOther = 0x01,
    kUnknown,
    kDesktop,
    kLowProfileDesktop,
    kPizzaBox,
    kMiniTower,
    kTower,
    kPortable,
    kLaptop,
    kNotebook,
    kHandHeld,
    kDockingStation,
    kAllInOne,
    kSubNotebook,
    kSpaceSaving,
    kLunchBox,
    kMainServerChassis,
    kExpansionChassis,
    kSubChassis,
    kBusExpansionChassis,
    kPeripheralChassis,
    kRaidChassis,
    kRackMountChassis
  };

  ChassisInfoArea() : type_(kUnknown) {}
  ChassisInfoArea(const ChassisInfoArea &) = delete;
  ChassisInfoArea &operator=(const ChassisInfoArea &) = delete;

  ~ChassisInfoArea() override {}

  // Outputs the underlying raw bytes for this InfoArea to the given buffer
  // (resizing it as necessary). Any existing data in the given buffer will
  // be overwritten.
  // NOTE: Since custom fields are not supported, GetImage will not necessarily
  // output the same data as that read into FillFromImage, even if nothing but
  // const methods are called after FillFromImage.
  void GetImage(std::vector<unsigned char> *area_image) const override;

  // Fills in the fields of this InfoArea from the data contained in the
  // given buffer. The buffer is read starting at the given offset. Returns 0
  // and changes nothing on error. Returns the offset of the last byte written
  // to on success.
  size_t FillFromImage(const FruImageSource &fru_image,
                       size_t area_offset) override;

  // Given a ChassisInfoArea::Type, returns a string representation of that
  // Type or the empty string if the given type is invalid or unknown.
  static std::string TypeAsString(Type type);

  // Accessors and mutators.
  Type type() const { return type_; }
  void set_type(Type type) { type_ = type; }
  const FruField &part_number() const { return part_number_; }
  void set_part_number(const FruField &part_number) {
    part_number_ = part_number;
  }
  const FruField &serial_number() const { return serial_number_; }
  void set_serial_number(const FruField &serial_number) {
    serial_number_ = serial_number;
  }
  const std::vector<FruField> &custom_fields() const { return custom_fields_; }
  void set_custom_fields(const std::vector<FruField> &custom_fields) {
    custom_fields_ = custom_fields;
  }

  // Variable fields in the Chassis area are: serial number and checksum. Gets
  // their location information.
  std::vector<FieldLocation> GetVariableFieldsLocation() const override {
    return variable_fields_location_;
  }

  absl::Status GetCustomField(CustomFieldType type,
                              std::string *value) const override;

 private:
  enum FieldIndices {
    kPartNumberIndex = 0,
    kSerialNumberIndex,
    kCustomFieldsIndex,
    kFieldIndexEnd
  };
  static constexpr uint8_t kFormatVersion = 1;  // The only supported version.
  static constexpr int kRequiredFieldCount = kFieldIndexEnd - 1;

  Type type_;
  FruField part_number_;
  FruField serial_number_;
  std::vector<FruField> custom_fields_;
  std::vector<FieldLocation> variable_fields_location_;
};

// Class to parse and/or build the board info area of a FRU.
class BoardInfoArea : public FruArea {
 public:
  BoardInfoArea() : language_code_(0), manufacture_date_(kFruEpoch) {}
  BoardInfoArea(const BoardInfoArea &) = delete;
  BoardInfoArea &operator=(const BoardInfoArea &) = delete;
  ~BoardInfoArea() override {}

  // Outputs the underlying raw bytes for this InfoArea to the given buffer
  // (resizing it as necessary). Any existing data in the given buffer will
  // be overwritten.
  // NOTE: Since custom fields are not supported, GetImage will not necessarily
  // output the same data as that read into FillFromImage, even if nothing but
  // const methods are called after FillFromImage.
  void GetImage(std::vector<unsigned char> *area_image) const override;

  // Fills in the fields of this InfoArea from the data contained in the
  // given buffer. The buffer is read starting at the given offset. Returns 0
  // and changes nothing on error. Returns the offset of the last byte written
  // to on success.
  size_t FillFromImage(const FruImageSource &fru_image,
                       size_t area_offset) override;

  // Accessors and mutators. Mutators with bool return type will return false
  // on error (for example, if an invalid value is passed in). In case of
  // error, nothing is changed.
  uint8_t language_code() const { return language_code_; }
  bool set_language_code(uint8_t language_code);
  time_t manufacture_date() const { return manufacture_date_; }
  bool set_manufacture_date(time_t manufacture_date);
  const FruField &manufacturer() const { return manufacturer_; }
  void set_manufacturer(const FruField &manufacturer) {
    manufacturer_ = manufacturer;
  }
  const FruField &product_name() const { return product_name_; }
  void set_product_name(const FruField &product_name) {
    product_name_ = product_name;
  }
  const FruField &serial_number() const { return serial_number_; }
  void set_serial_number(const FruField &serial_number) {
    serial_number_ = serial_number;
  }
  const FruField &part_number() const { return part_number_; }
  void set_part_number(const FruField &part_number) {
    part_number_ = part_number;
  }
  const FruField &file_id() const { return file_id_; }
  void set_file_id(const FruField &file_id) { file_id_ = file_id; }
  const std::vector<FruField> &custom_fields() const { return custom_fields_; }
  void set_custom_fields(const std::vector<FruField> &custom_fields) {
    custom_fields_ = custom_fields;
  }

  // Variable fields in the Board area are: manufacture date, serial number and
  // checksum. Gets their location information.
  std::vector<FieldLocation> GetVariableFieldsLocation() const override {
    return variable_fields_location_;
  }

  absl::Status GetCustomField(CustomFieldType type,
                              std::string *value) const override;

 private:
  enum FieldIndices {
    kManufacturerIndex = 0,
    kProductNameIndex,
    kSerialNumberIndex,
    kPartNumberIndex,
    kFileIdIndex,
    kCustomFieldsIndex,
    kFieldIndexEnd
  };
  static constexpr uint8_t kFormatVersion = 1;  // The only supported version.
  static constexpr int kRequiredFieldCount = kFieldIndexEnd - 1;

  uint8_t language_code_;
  time_t manufacture_date_;
  FruField manufacturer_;
  FruField product_name_;
  FruField serial_number_;
  FruField part_number_;
  FruField file_id_;
  std::vector<FruField> custom_fields_;
  std::vector<FieldLocation> variable_fields_location_;
};

// Class to parse and/or build the product info area of a FRU.
class ProductInfoArea : public FruArea {
 public:
  ProductInfoArea() : language_code_(0) {}
  ProductInfoArea(const ProductInfoArea &) = delete;
  ProductInfoArea &operator=(const ProductInfoArea &) = delete;
  ~ProductInfoArea() override {}

  // Outputs the underlying raw bytes for this InfoArea to the given buffer
  // (resizing it as necessary). Any existing data in the given buffer will
  // be overwritten.
  // NOTE: Since custom fields are not supported, GetImage will not necessarily
  // output the same data as that read into FillFromImage, even if nothing but
  // const methods are called after FillFromImage.
  void GetImage(std::vector<unsigned char> *area_image) const override;

  // Fills in the fields of this InfoArea from the data contained in the
  // given buffer. The buffer is read starting at the given offset. Returns 0
  // and changes nothing on error. Returns the offset of the last byte written
  // to on success.
  size_t FillFromImage(const FruImageSource &fru_image,
                       size_t area_offset) override;

  // Accessors and mutators. Mutators with bool return type will return false
  // on error (for example, if an invalid value is passed in). In case of
  // error, nothing is changed.
  uint8_t language_code() const { return language_code_; }
  bool set_language_code(uint8_t language_code);
  const FruField &manufacturer() const { return manufacturer_; }
  void set_manufacturer(const FruField &manufacturer) {
    manufacturer_ = manufacturer;
  }
  const FruField &product_name() const { return product_name_; }
  void set_product_name(const FruField &product_name) {
    product_name_ = product_name;
  }
  const FruField &part_number() const { return part_number_; }
  void set_part_number(const FruField &part_number) {
    part_number_ = part_number;
  }
  const FruField &product_version() const { return product_version_; }
  void set_product_version(const FruField &product_version) {
    product_version_ = product_version;
  }
  const FruField &serial_number() const { return serial_number_; }
  void set_serial_number(const FruField &serial_number) {
    serial_number_ = serial_number;
  }
  const FruField &asset_tag() const { return asset_tag_; }
  void set_asset_tag(const FruField &asset_tag) { asset_tag_ = asset_tag; }
  const FruField &file_id() const { return file_id_; }
  void set_file_id(const FruField &file_id) { file_id_ = file_id; }
  const std::vector<FruField> &custom_fields() const { return custom_fields_; }
  void set_custom_fields(const std::vector<FruField> &custom_fields) {
    custom_fields_ = custom_fields;
  }

  // Variable fields in the product area are: serial number and checksum. Gets
  // their location information.
  std::vector<FieldLocation> GetVariableFieldsLocation() const override {
    return variable_fields_location_;
  }

  absl::Status GetCustomField(CustomFieldType type,
                              std::string *value) const override;

 private:
  enum FieldIndices {
    kManufacturerIndex = 0,
    kProductNameIndex,
    kPartNumberIndex,
    kProductVersionIndex,
    kSerialNumberIndex,
    kAssetTagIndex,
    kFileIdIndex,
    kCustomFieldsIndex,
    kFieldIndexEnd
  };
  static constexpr uint8_t kFormatVersion = 1;  // The only supported version.
  static constexpr int kRequiredFieldCount = kFieldIndexEnd - 1;

  uint8_t language_code_;
  FruField manufacturer_;
  FruField product_name_;
  FruField part_number_;
  FruField product_version_;
  FruField serial_number_;
  FruField asset_tag_;
  FruField file_id_;
  std::vector<FruField> custom_fields_;
  std::vector<FieldLocation> variable_fields_location_;
};

class MultiRecord {
 public:
  static constexpr uint8_t kRecordFormatVersion = 2;
  MultiRecord()
      : type_id_(0),
        record_format_version_(kRecordFormatVersion),
        data_(std::vector<unsigned char>()) {}

  MultiRecord(const MultiRecord &rhs) = default;
  MultiRecord &operator=(const MultiRecord &rhs) = default;

  uint8_t get_type() const { return type_id_; }
  void set_type(uint8_t type) { type_id_ = type; }
  void get_data(std::vector<unsigned char> *data) const { *data = data_; }
  bool set_data(absl::Span<unsigned char> data) {
    if (data.size() > 255) return false;
    data_.resize(data.size());
    std::copy(data.begin(), data.end(), data_.begin());
    return true;
  }
  static constexpr size_t kMultiRecordHeaderSize = 5;
  size_t get_size() { return data_.size() + kMultiRecordHeaderSize; }

  // Fills in the fields of this InfoArea from the data contained in the
  // given buffer. The buffer is read starting at the given offset. Returns
  // false and changes nothing on error.
  bool FillFromImage(const FruImageSource &fru_image, size_t area_offset,
                     bool *end_of_list);

  // Outputs the underlying raw bytes for this InfoArea to the given buffer
  // (resizing it as necessary). Any existing data in the given buffer will
  // be overwritten.
  void GetImage(bool end_of_list, std::vector<unsigned char> *bytes) const;

 private:
  uint8_t type_id_;
  uint8_t record_format_version_;
  std::vector<unsigned char> data_;
};

// Class to parse and/or build the multi record area of a FRU.
class MultiRecordArea : public FruArea {
 public:
  MultiRecordArea() {}
  MultiRecordArea(const MultiRecordArea &) = delete;
  MultiRecordArea &operator=(const MultiRecordArea &) = delete;

  ~MultiRecordArea() override {}

  // Outputs the underlying raw bytes for this InfoArea to the given buffer
  // (resizing it as necessary). Any existing data in the given buffer will
  // be overwritten.
  void GetImage(std::vector<unsigned char> *area_image) const override;

  // Fills in the fields of this InfoArea from the data contained in the
  // given buffer. The buffer is read starting at the given offset. Returns 0
  // and changes nothing on error. Returns the offset of the last byte written
  // to on success.
  size_t FillFromImage(const FruImageSource &fru_image,
                       size_t area_offset) override;

  // Accessors and mutators. Mutators with bool return type will return false
  // on error (for example, if an invalid value is passed in). In case of
  // error, nothing is changed.

  bool AddMultiRecord(const MultiRecord &multi_record);
  bool GetMultiRecord(size_t position, MultiRecord *record) const;
  bool RemoveMultiRecord(size_t position);
  bool UpdateMultiRecord(size_t position, const MultiRecord &record);
  size_t GetMultiRecordCount() const { return multi_records_.size(); }

  std::vector<FieldLocation> GetVariableFieldsLocation() const override {
    return variable_fields_location_;
  }

  absl::Status GetCustomField(CustomFieldType type,
                              std::string *value) const override {
    return absl::UnimplementedError(
        "Multirecord area does not have custom field");
  }

 private:
  std::vector<MultiRecord> multi_records_;
  std::vector<FieldLocation> variable_fields_location_;
};

// This class encapsulates the source of the actual FRU image bytes read in
// the *::FillFromImage functions to allow for buffering, lazy reads, etc.
// A default, vector-based implementation is also provided here.
//
// The details of the below interface are useful if you want to implement your
// own FruImageSource, but are not useful for general users of the Fru class.
class FruImageSource {
 public:
  FruImageSource() {}
  FruImageSource(const FruImageSource &) = delete;
  FruImageSource &operator=(const FruImageSource &) = delete;
  virtual ~FruImageSource() {}

  // Returns the size (total number of available bytes) of this FruImageSource.
  virtual size_t Size() const = 0;

  // Reads the specified number of bytes starting at the given index into the
  // given vector. Returns true on success.
  virtual bool GetBytes(size_t index, size_t num_bytes,
                        std::vector<unsigned char> *out_bytes) const = 0;
};

// A vector-based FruImageSource.
class VectorFruImageSource : public FruImageSource {
 public:
  explicit VectorFruImageSource(absl::Span<unsigned char> fru_image) {
    fru_image_.resize(fru_image.size());
    std::copy(fru_image.begin(), fru_image.end(), fru_image_.begin());
  }
  VectorFruImageSource(const VectorFruImageSource &) = delete;
  VectorFruImageSource &operator=(VectorFruImageSource &) = delete;
  ~VectorFruImageSource() override {}

  // Returns the size (total number of available bytes) of this FruImageSource.
  size_t Size() const override { return fru_image_.size(); }

  // Reads the specified number of bytes starting at the given index into the
  // given vector. Returns true on success.
  bool GetBytes(size_t index, size_t num_bytes,
                std::vector<unsigned char> *out_bytes) const override;

 private:
  std::vector<unsigned char> fru_image_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_FRU_FRU_H_
