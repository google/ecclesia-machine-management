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

#include "ecclesia/magent/lib/ipmi/ipmitool.h"

#include <sys/socket.h>

#include <any>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/magent/lib/fru/fru.h"
#include "ecclesia/magent/lib/ipmi/ipmi.h"
#include "ecclesia/magent/proto/config.pb.h"

extern "C" {
#include "include/ipmitool/ipmi.h"
#include "include/ipmitool/ipmi_fru.h"
#include "include/ipmitool/ipmi_intf.h"
#include "include/ipmitool/ipmi_sdr.h"
#include "include/ipmitool/ipmi_sol.h"

extern const struct valstr completion_code_vals[];

extern int read_fru_area(struct ipmi_intf *intf, struct fru_info *fru,
                         uint8_t id, uint32_t offset, uint32_t length,
                         uint8_t *frubuf);
extern void fru_area_print_board(struct ipmi_intf *intf, struct fru_info *fru,
                                 uint8_t id, uint32_t offset);

extern char *get_fru_area_str(uint8_t *data, uint32_t *offset);

// These two global variables are defined in ipmitool/src/ipmitool.c or
// ipmitool/src/ipmievd.c, we are not including either one,
// So we will define them here.
int csv_output = 0;
int verbose = 0;

}  // extern "C"

#include "ecclesia/magent/lib/ipmi/ipmitool_interface.h"

namespace ecclesia {

namespace {

struct FreeDeleter {
  inline void operator()(void *ptr) const { free(ptr); }
};

template <typename T>
using SdrRecordUniquePtr = std::unique_ptr<T, FreeDeleter>;

}  // namespace

// IPMI Completion Codes.
constexpr uint8_t IPMI_OK_CODE = 0x00;
constexpr uint8_t IPMI_INVALID_CMD_COMPLETION_CODE = 0xC1;
constexpr uint8_t IPMI_TIMEOUT_COMPLETION_CODE = 0xC3;
constexpr uint8_t IPMI_UNKNOWN_ERR_COMPLETION_CODE = 0xff;

void IpmitoolInterface::SessionSetKgkey(std::any intf, const uint8_t *kgkey) {
  if (!intf.has_value()) {
    FatalLog() << "intf is empty.";
    return;
  }
  return ipmi_intf_session_set_kgkey(std::any_cast<struct ipmi_intf *>(intf),
                                     kgkey);
}

void IpmitoolInterface::SessionSetPrivlvl(std::any intf, uint8_t privlvl) {
  if (!intf.has_value()) {
    FatalLog() << "intf is empty.";
    return;
  }
  return ipmi_intf_session_set_privlvl(std::any_cast<struct ipmi_intf *>(intf),
                                       privlvl);
}

void IpmitoolInterface::SessionSetLookupbit(std::any intf, uint8_t lookupbit) {
  if (!intf.has_value()) {
    FatalLog() << "intf is empty.";
    return;
  }
  return ipmi_intf_session_set_lookupbit(
      std::any_cast<struct ipmi_intf *>(intf), lookupbit);
}

void IpmitoolInterface::SessionSetSolEscapeChar(std::any intf,
                                                char sol_escape_char) {
  if (!intf.has_value()) {
    FatalLog() << "intf is empty.";
    return;
  }
  return ipmi_intf_session_set_sol_escape_char(
      std::any_cast<struct ipmi_intf *>(intf), sol_escape_char);
}

void IpmitoolInterface::SessionSetCipherSuiteId(
    std::any intf, enum cipher_suite_ids cipher_suite_id) {
  if (!intf.has_value()) {
    FatalLog() << "intf is empty.";
    return;
  }
  ipmi_intf_session_set_cipher_suite_id(std::any_cast<struct ipmi_intf *>(intf),
                                        cipher_suite_id);
}

void IpmitoolInterface::SessionSetRetry(std::any intf, int retry) {
  if (!intf.has_value()) {
    FatalLog() << "intf is empty.";
    return;
  }
  return ipmi_intf_session_set_retry(std::any_cast<struct ipmi_intf *>(intf),
                                     retry);
}

void IpmitoolInterface::SessionSetTimeout(std::any intf, uint32_t timeout) {
  if (!intf.has_value()) {
    FatalLog() << "intf is empty.";
    return;
  }
  return ipmi_intf_session_set_timeout(std::any_cast<struct ipmi_intf *>(intf),
                                       timeout);
}

void IpmitoolInterface::SessionSetHostname(std::any intf, char *hostname) {
  if (!intf.has_value()) {
    FatalLog() << "intf is empty.";
    return;
  }
  return ipmi_intf_session_set_hostname(std::any_cast<struct ipmi_intf *>(intf),
                                        hostname);
}

void IpmitoolInterface::SessionSetPort(std::any intf, int port) {
  if (!intf.has_value()) {
    FatalLog() << "intf is empty.";
    return;
  }
  return ipmi_intf_session_set_port(std::any_cast<struct ipmi_intf *>(intf),
                                    port);
}

void IpmitoolInterface::SessionSetUsername(std::any intf, char *username) {
  if (!intf.has_value()) {
    FatalLog() << "intf is empty.";
    return;
  }
  return ipmi_intf_session_set_username(std::any_cast<struct ipmi_intf *>(intf),
                                        username);
}

void IpmitoolInterface::SessionSetPassword(std::any intf, char *password) {
  if (!intf.has_value()) {
    FatalLog() << "intf is empty.";
    return;
  }
  return ipmi_intf_session_set_password(std::any_cast<struct ipmi_intf *>(intf),
                                        password);
}

std::any IpmitoolInterface::SdrStart(std::any intf, int use_builtin) {
  if (!intf.has_value()) {
    FatalLog() << "intf is empty.";
    return nullptr;
  }
  return ipmi_sdr_start(std::any_cast<struct ipmi_intf *>(intf), use_builtin);
}

std::any IpmitoolInterface::SdrGetNextHeader(std::any intf, std::any i) {
  if (!intf.has_value()) {
    FatalLog() << "intf is empty.";
    return nullptr;
  }
  return ipmi_sdr_get_next_header(std::any_cast<struct ipmi_intf *>(intf),
                                  std::any_cast<struct ipmi_sdr_iterator *>(i));
}

uint8_t *IpmitoolInterface::SdrGetRecord(std::any intf, std::any header,
                                         std::any i) {
  if (!intf.has_value()) {
    FatalLog() << "intf is empty.";
    return nullptr;
  }
  if (!header.has_value()) {
    ErrorLog() << "header is empty.";
    return nullptr;
  }
  if (!i.has_value()) {
    ErrorLog() << "iterator is empty.";
    return nullptr;
  }
  return ipmi_sdr_get_record(std::any_cast<struct ipmi_intf *>(intf),
                             std::any_cast<struct sdr_get_rs *>(header),
                             std::any_cast<struct ipmi_sdr_iterator *>(i));
}

void IpmitoolInterface::SdrEnd(std::any intf, std::any i) {
  if (!intf.has_value()) {
    FatalLog() << "intf is empty.";
    return;
  }
  ipmi_sdr_end(std::any_cast<struct ipmi_sdr_iterator *>(i));
}

IpmiSensor::Type ReadIpmiSensorTypeInternal(
    const sdr_record_common_sensor &common,
    absl::string_view printable_sensor) {
  // See section 42.2 IpmiSensor Type Codes and Data in the IPMI spec
  switch (common.sensor.type) {
    case 0x01:
      return SENSOR_TYPE_THERMAL;
    case 0x02:
      return SENSOR_TYPE_VOLTAGE;
    case 0x03:
      return SENSOR_TYPE_CURRENT;
    case 0x04:
      return SENSOR_TYPE_FANTACH;
    case 0x1b:
    case 0x25:
      return SENSOR_TYPE_PRESENCE;
    case 0x28:  // Management Subsys Health
    case 0xc0:  // OEM Reserved (0xc0 - 0xff)
      return SENSOR_TYPE_OEM_STATE;
    // A generic module/board sensor. Profile sensor implementation in gBMC uses
    // this type, but is not relevant to gsys API or internal handling.
    case 0x15:
    // The following sensor type codes have been seen in Google production
    // but we have not assigned a monitoring type to them. These sensor
    // types will not be inferred from these codes.
    case 0x0b:  // "Other", used for power, but check unit to confirm
    case 0x0c:  // Memory
    case 0x21:  // Slot/Connector sensor
    default:
      break;
  }

  // Fallback to looking at the sensor unit to infer the sensor type
  // See section 43.17 Sensor Unit Type Codes from the IPMI spec
  switch (common.unit.type.base) {
    case 1:
      return SENSOR_TYPE_THERMAL;
    case 4:
      return SENSOR_TYPE_VOLTAGE;
    case 5:
      return SENSOR_TYPE_CURRENT;
    case 6:
      return SENSOR_TYPE_POWER;
    case 18:
      return SENSOR_TYPE_FANTACH;
    case 22:
      return SENSOR_TYPE_TIME;
    case 41:
      return SENSOR_TYPE_FANTACH;
    default:
      break;
  }
  WarningLog() << absl::StrFormat(
      "ipmi sensor type could not be inferred, unknown sensor type 0x%x "
      "unit 0x%x for %s",
      common.sensor.type, common.unit.type.base, printable_sensor);
  return SENSOR_TYPE_THERMAL;
}

class IpmiFullCompactSensor {
 public:
  // Default/copy constructors are deleted since it contains unique_ptr which
  // allows only move semantics.
  IpmiFullCompactSensor() = delete;
  IpmiFullCompactSensor(const IpmiFullCompactSensor &) = delete;

  // Move constructor, standard semantics
  IpmiFullCompactSensor(IpmiFullCompactSensor &&) = default;

  static std::unique_ptr<IpmiFullCompactSensor> Create(
      struct sdr_record_common_sensor *sensor, uint8_t sdr_record_type) {
    if (sdr_record_type == SDR_RECORD_TYPE_FULL_SENSOR ||
        sdr_record_type == SDR_RECORD_TYPE_COMPACT_SENSOR) {
      return absl::WrapUnique(
          new IpmiFullCompactSensor(sensor, sdr_record_type));
    }

    WarningLog() << "unsupported sensor, must be either full or compact";

    return nullptr;
  }
  std::string GetName() const { return name_; }
  IpmiSensor::Unit GetUnit() const { return unit_; }
  IpmiSensor::Type GetType() const { return type_; }
  IpmiInterface::EntityIdentifier GetEntityIdentifier() const {
    return {common_->entity.id, common_->entity.instance};
  }
  bool IsSettable() const { return common_->sensor.init.settable; }
  uint8_t GetLun() const { return common_->keys.lun; }
  uint8_t GetId() const { return common_->keys.sensor_num; }
  SensorNum GetSensorNum() const { return {GetLun(), GetId()}; }

  bool IsFullType() const {
    return (sdr_record_type_ == SDR_RECORD_TYPE_FULL_SENSOR);
  }

  absl::StatusOr<double> ReadValue(struct ipmi_intf *ipmi) const {
    struct sensor_reading *read = ipmi_sdr_read_sensor_value(
        ipmi, common_, sdr_record_type_, kSendorReadPrecision);

    if (read == nullptr) {
      return absl::InternalError(
          absl::StrFormat("not supported %s", ToPrintableString()));
    }

    if (read->s_reading_unavailable) {
      return absl::InternalError(absl::StrFormat(
          "reading for %s is unavailable", ToPrintableString()));
    }

    if (read->s_scanning_disabled) {
      return absl::InternalError(
          absl::StrFormat("scanning for %s is disabled", ToPrintableString()));
    }

    if (!read->s_reading_valid) {
      return absl::InternalError(
          absl::StrFormat("error on reading for %s", ToPrintableString()));
    }

    double val = read->s_has_analog_value ? read->s_a_val : read->s_reading;
    switch (type_) {
      case SENSOR_TYPE_OEM_STATE:
        val = (read->s_data2 & kStateByteAssertedBit) ? 1.0 : 0.0;
        break;
      case SENSOR_TYPE_PRESENCE:
        // Usage: sensor type code 0x1b (Cable/Interconnect), 0x25 (Entity
        // Presence). Details see IPMI spec 42.2 section.
        //
        // 1Bh: 00h -- cable/interconnect is connected
        //      01h -- Configuration error.
        // In this case we only check Configuration error bit (& with 2). We
        // will fallback to "cable is connected" when error bit is not set.
        //
        // 25h: 00h -- entity present
        //      01h -- entity absent
        //      02h -- entity disabled
        // In this case we are supposed to also check the 'entity disabled' bit,
        // but bmc is not using the bit currently. So similarly we only check
        // the entity absent bit (& with 2). Like case 1Bh, we fallback to
        // "present" when absent bit is not set.
        //
        // What 'val' stands for is as the following table
        //  __________________________________________________
        //  | response.data[2] |    val    |      state      |
        //  |        2         |     0     |     absent      |
        //  |      not 2       |     1     |     present     |
        //  --------------------------------------------------
        val = (read->s_data2 & kPresenceSensorErrorBit) ? 0.0 : 1.0;
        break;
      default:
        break;
    }

    // We now assume the value is valid
    return val;
  }

  std::string ToPrintableString() const {
    return absl::StrFormat("%d.%d(%s) %s sensor", GetLun(), GetId(), GetName(),
                           IsFullType() ? "full" : "compact");
  }

 private:
  IpmiFullCompactSensor(struct sdr_record_common_sensor *sensor,
                        uint8_t sdr_record_type)
      : common_(sensor), sdr_record_type_(sdr_record_type) {
    if (sdr_record_type_ == SDR_RECORD_TYPE_FULL_SENSOR) {
      auto *full = reinterpret_cast<struct sdr_record_full_sensor *>(sensor);
      sensor_ = SdrRecordUniquePtr<struct sdr_record_full_sensor>(full);
      name_ = std::string(reinterpret_cast<const char *>(full->id_string),
                          full->id_code & 0x1f);
    } else if (sdr_record_type_ == SDR_RECORD_TYPE_COMPACT_SENSOR) {
      auto *compact =
          reinterpret_cast<struct sdr_record_compact_sensor *>(sensor);
      sensor_ = SdrRecordUniquePtr<struct sdr_record_compact_sensor>(compact);
      name_ = std::string(reinterpret_cast<const char *>(compact->id_string),
                          compact->id_code & 0x1f);
    }

    // NOTE: ReadTypeInternal() and ReadUnitInternal() needs to be called
    // after setting common_, sdr_record_type_, name_.
    type_ = ReadIpmiSensorTypeInternal(*common_, ToPrintableString());
    unit_ = ReadUnitInternal();
  }

  IpmiSensor::Unit ReadUnitInternal() {
    // See section 43.17 IpmiSensor Unit Type Codes from the IPMI spec
    switch (common_->unit.type.base) {
      case 0:
        return SENSOR_UNIT_UNSPECIFIED;
      case 1:
        return SENSOR_UNIT_DEGREES;
      case 4:
        return SENSOR_UNIT_VOLTS;
      case 5:
        return SENSOR_UNIT_AMPS;
      case 6:
        return SENSOR_UNIT_WATTS;
      case 18:
        return SENSOR_UNIT_RPM;
      case 22:
        return SENSOR_UNIT_SECONDS;
      case 41:
        return SENSOR_UNIT_RPM;
      default:
        break;
    }
    WarningLog() << absl::StrFormat("unknown sensor unit 0x%x for %s",
                                    common_->unit.type.base,
                                    ToPrintableString());
    return SENSOR_UNIT_UNSPECIFIED;
  }

  struct sdr_record_full_sensor *GetFullSensorPtr() const {
    if (IsFullType()) {
      return absl::get<SdrRecordUniquePtr<struct sdr_record_full_sensor>>(
                 sensor_)
          .get();
    }
    return nullptr;
  }

  // When reading a sensor with ipmi_sdr_read_sensor_value(),
  // s_a_str of struct sensor_reading, which is a return value, contains
  // analog value as a string format. `kSendorReadPrecision` specifies how
  // many digits under decimal point will be shown.
  // However, s_a_str is NOT used. So this will NOT affect.
  static constexpr int kSendorReadPrecision = 3;

  static constexpr int kStateByteAssertedBit = 0x02;
  static constexpr int kPresenceSensorErrorBit = 0x02;

  absl::variant<SdrRecordUniquePtr<struct sdr_record_full_sensor>,
                SdrRecordUniquePtr<struct sdr_record_compact_sensor>>
      sensor_;

  struct sdr_record_common_sensor *common_;
  uint8_t sdr_record_type_;

  std::string name_;
  IpmiSensor::Unit unit_;
  IpmiSensor::Type type_;
};

class IpmitoolImpl : public IpmiInterface {
 public:
  struct FreeDeleter {
    inline void operator()(void *ptr) const { free(ptr); }
  };

  explicit IpmitoolImpl(
      absl::optional<ecclesia::MagentConfig::IpmiCredential> cred)
      : cred_(std::move(cred)), intf_(GetIpmiIntf()) {}

  std::vector<BmcFruInterfaceInfo> GetAllFrus() override {
    if (frus_cache_.empty()) {
      if (!FindAllFrus().ok()) return {};
    }

    std::vector<BmcFruInterfaceInfo> frus;
    for (const auto &fru_pair : frus_cache_) {
      BmcFruInterfaceInfo fru;
      fru.record_id = fru_pair.first;
      auto entity = fru_pair.second->entity;
      EntityIdentifier fru_entity = {
          entity.id,
          static_cast<uint8_t>((entity.logical << 7) | entity.instance)};
      fru.entity = fru_entity;
      fru.name = ReadFruName(fru_pair.second.get());
      fru.fru_id = fru_pair.second->device_id;
      frus.push_back(fru);
    }
    return frus;
  }

  std::vector<BmcSensorInterfaceInfo> GetAllSensors() override {
    if (fullcompact_sensors_cache_.empty()) {
      if (!FindAllSensors().ok()) return {};
    }
    std::vector<BmcSensorInterfaceInfo> sensors;
    for (const auto &[sensor_num, sensor] : fullcompact_sensors_cache_) {
      BmcSensorInterfaceInfo info;
      info.id = sensor_num;
      info.name = sensor->GetName();
      info.type = sensor->GetType();
      info.unit = sensor->GetUnit();
      info.settable = sensor->IsSettable();
      info.entity_id = sensor->GetEntityIdentifier();
      sensors.push_back(info);
    }
    return sensors;
  }

  absl::StatusOr<BmcSensorInterfaceInfo> GetSensor(
      SensorNum sensor_num) override {
    if (!intf_) {
      return absl::InternalError("Ipmi interface: intf_ is nullptr.");
    }

    ECCLESIA_ASSIGN_OR_RETURN(IpmiFullCompactSensor * sensor,
                              GrabSensorInternal(intf_, sensor_num));

    return BmcSensorInterfaceInfo{.id = sensor_num,
                                  .name = sensor->GetName(),
                                  .type = sensor->GetType(),
                                  .unit = sensor->GetUnit(),
                                  .settable = sensor->IsSettable(),
                                  .entity_id = sensor->GetEntityIdentifier()};
  }

  absl::StatusOr<double> ReadSensor(SensorNum sensor_num) override {
    if (!intf_) {
      return absl::InternalError("Ipmi interface: intf_ is nullptr.");
    }

    ECCLESIA_ASSIGN_OR_RETURN(IpmiFullCompactSensor * sensor,
                              GrabSensorInternal(intf_, sensor_num));

    return sensor->ReadValue(intf_);
  }

  absl::Status ReadFru(uint16_t fru_id, size_t offset,
                       absl::Span<unsigned char> data) override {
    struct fru_info fru {};

    uint8_t access;
    absl::Status status;

    ECCLESIA_RETURN_IF_ERROR(GetFruInfo(intf_, fru_id, &fru.size, &access));
    fru.access = access;

    // Maximum output message size for KCS/SMIC is 38 with 2 utility bytes,
    // a byte for completion code and 35 bytes of data.
    // Maximum output message size for BT is 40 with 4 utility bytes, a byte
    // for completion code and 35 bytes of data.
    fru.max_read_size = 35;

    if (read_fru_area(intf_, &fru, fru_id, offset, data.size(), data.data())) {
      return absl::UnknownError(absl::StrFormat(
          "Failed to read_fru_area for fru_id: %d, offset: %d, len: %d.\n",
          fru_id, offset, data.size()));
    }

    return absl::OkStatus();
  }

  absl::Status GetFruSize(uint16_t fru_id, uint16_t *size) override {
    return GetFruInfo(intf_, fru_id, size, nullptr);
  }

 private:
  // A map of FRU numbers to SDR records for them. This map is only modified
  // during construction and then serves as a cache of the read FRU
  // information.
  absl::flat_hash_map<uint16_t,
                      SdrRecordUniquePtr<struct sdr_record_fru_locator>>
      frus_cache_;
  // A map of IPMI sensor number to SDR records types FULL and COMPACT.
  // This map is only modified during construction and then serves as a cache
  // of the read FRU information.
  absl::flat_hash_map<SensorNum, std::unique_ptr<IpmiFullCompactSensor>>
      fullcompact_sensors_cache_;

  absl::optional<ecclesia::MagentConfig::IpmiCredential> cred_;
  ipmi_intf *intf_;
  IpmitoolInterface ipmitool_intf_;

  ipmi_intf *GetIpmiIntf() {
    if (cred_ == absl::nullopt) {
      ErrorLog() << "Fail to create ipmi interface due to invalid credential.";
      return nullptr;
    }

    ipmi_intf *intf = ipmi_intf_load(cred_->mutable_ipmi_interface()->data());

    if (intf == nullptr) {
      ErrorLog() << "Fail to create ipmi interface due to internal error.";
      return nullptr;
    }

    // Close any currently-active session.
    if (intf->close) {
      intf->close(intf);
    }

    ipmitool_intf_.SessionSetRetry(intf, 5);
    ipmitool_intf_.SessionSetTimeout(intf, 30);

    ipmitool_intf_.SessionSetHostname(intf,
                                      cred_->mutable_ipmi_hostname()->data());
    ipmitool_intf_.SessionSetPort(intf, cred_->ipmi_port());
    ipmitool_intf_.SessionSetUsername(intf,
                                      cred_->mutable_ipmi_username()->data());
    ipmitool_intf_.SessionSetPassword(intf,
                                      cred_->mutable_ipmi_password()->data());

    ConfigureLanPlusInterface(intf);

    return intf;
  }

  absl::Status Raw(absl::Span<uint8_t> buffer, ipmi_rs **resp) {
    if (!intf_) {
      FatalLog() << "Ipmi interface: intf_ is nullptr.";
    }

    const uint8_t *bytes = buffer.data();
    uint32_t len = buffer.size();

    if (len < kMinimumIpmiPacketLength) {
      return absl::InvalidArgumentError("Invalid number of bytes to raw call");
    }

    uint32_t data_len = len - kMinimumIpmiPacketLength;
    uint8_t data[kMaximumPipelineBandwidth]{};
    ipmi_rq request{};

    // Skip beyond netfn and command.
    if (data_len > 0) {
      std::memcpy(data, &bytes[kMinimumIpmiPacketLength], data_len);
    }

    ipmitool_intf_.SessionSetTimeout(intf_, 15);
    ipmitool_intf_.SessionSetRetry(intf_, 1);

    request.msg.netfn = bytes[0];
    request.msg.lun = 0x00;
    request.msg.cmd = bytes[1];
    request.msg.data = data;
    request.msg.data_len = data_len;

    ipmi_rs *response = intf_->sendrecv(intf_, &request);
    if (nullptr == response) {
      return absl::InternalError("response was NULL from intf->sendrecv");
    }

    if (resp) {
      *resp = response;
    }

    if (response->ccode > 0) {
      if (IPMI_TIMEOUT_COMPLETION_CODE == response->ccode)
        return absl::InternalError("Timeout from IPMI");
      else
        return absl::InternalError(absl::StrCat(
            "Unable to send code: ", IpmiResponseToString(response->ccode)));
    }

    return absl::OkStatus();
  }

  absl::Status SendWithRetry(const IpmiRequest &request, int retries,
                             IpmiResponse *response) {
    ipmi_rs *resp;
    int tries = retries + 1;
    std::vector<uint8_t> buffer(kMinimumIpmiPacketLength + request.data.size());
    buffer[0] = static_cast<uint8_t>(request.network_function);
    buffer[1] = static_cast<uint8_t>(request.command);
    if (!request.data.empty()) {
      std::memcpy(&buffer[kMinimumIpmiPacketLength], request.data.data(),
                  request.data.size());
    }

    int count = 0;
    absl::Status result;
    while (count < tries) {
      result = Raw(absl::MakeSpan(buffer), &resp);
      if (result.ok()) break;
      count++;
    }

    if (!result.ok()) {
      return absl::InternalError(
          absl::StrCat("Failed to send IPMI command after ", count, " tries."));
    }

    response->ccode = resp->ccode;
    response->data =
        std::vector<uint8_t>(resp->data, resp->data + resp->data_len);

    return absl::OkStatus();
  }

  absl::Status Send(const IpmiRequest &request, IpmiResponse *response) {
    return SendWithRetry(request, 0, response);
  }

  // read Fru size and access.
  absl::Status GetFruInfo(ipmi_intf *intf_, uint16_t fru_id, uint16_t *size,
                          uint8_t *access) {
    uint8_t buffer[4]{};
    IpmiResponse rsp;

    IpmiRequest req(kGetFruInfo, absl::MakeSpan(buffer, 4));

    ECCLESIA_RETURN_IF_ERROR(Send(req, &rsp));

    if (rsp.ccode > 0) {
      return absl::InternalError(
          absl::StrFormat(" Device not present (%s)\n",
                          val2str(rsp.ccode, completion_code_vals)));
    }

    if (size) {
      *size = (rsp.data[1] << 8) | rsp.data[0];
    }
    if (access) {
      *access = rsp.data[2] & 0x1;
    }

    return absl::OkStatus();
  }

  // A helper function to print the Board info given a FRU ID and its name.
  void PrintBoardInfo(uint16_t fru_id, uint8_t fru_id_string[16]) {
    std::vector<uint8_t> data(72);
    absl::Status status = ReadFru(fru_id, 0, absl::MakeSpan(data));
    if (!status.ok()) {
      ErrorLog() << "ERROR: " << status.message() << '\n';
    }

    VectorFruImageSource fru_image(absl::MakeSpan(data));
    BoardInfoArea bia;
    bia.FillFromImage(fru_image, 8);

    InfoLog() << "FRU Device Description: " << fru_id_string << " (ID "
              << (int)fru_id << ")";
    time_t t = bia.manufacture_date();
    InfoLog() << "Board Mfg Date        : " << asctime(localtime(&t));
    InfoLog() << "Board Mfg             : "
              << bia.manufacturer().GetDataAsString();
    InfoLog() << "Board Product         : "
              << bia.product_name().GetDataAsString();
    InfoLog() << "Board Serial          : "
              << bia.serial_number().GetDataAsString();
    InfoLog() << "Board Part Number     : "
              << bia.part_number().GetDataAsString();
  }

  absl::Status FindAllFrus() {
    if (!intf_) {
      return absl::InternalError("Ipmi interface: intf_ is nullptr.");
    }

    struct ipmi_sdr_iterator *itr = nullptr;
    if ((itr = std::any_cast<struct ipmi_sdr_iterator *>(
             ipmitool_intf_.SdrStart(intf_, 0))) == nullptr) {
      return absl::InternalError("Unable to open SDR for reading.");
    }

    absl::Status status;
    struct sdr_get_rs *header;
    struct sdr_record_fru_locator *fru;
    while ((header = std::any_cast<struct sdr_get_rs *>(
                ipmitool_intf_.SdrGetNextHeader(intf_, itr))) != nullptr) {
      if (header->type == SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR) {
        fru = reinterpret_cast<struct sdr_record_fru_locator *>(
            ipmitool_intf_.SdrGetRecord(intf_, header, itr));
        if (fru == nullptr || !fru->logical) {
          if (fru) {
            free(fru);
            fru = nullptr;
          }
          InfoLog() << "Fail to get logical frus.";
          continue;
        }
        PrintBoardInfo(fru->device_id, fru->id_string);

        // We need this line because sdr_get_rs is packed.
        uint16_t id = header->id;
        frus_cache_.emplace(
            id, SdrRecordUniquePtr<struct sdr_record_fru_locator>(fru));
      }
    }
    // Frees the memory allocated by SdrStart
    ipmitool_intf_.SdrEnd(intf_, itr);

    return status;
  }

  absl::Status FindAllSensors() {
    if (!intf_) {
      return absl::InternalError("Ipmi interface: intf_ is nullptr.");
    }

    struct ipmi_sdr_iterator *itr = nullptr;
    if ((itr = std::any_cast<struct ipmi_sdr_iterator *>(
             ipmitool_intf_.SdrStart(intf_, 0))) == nullptr) {
      return absl::InternalError("Unable to open SDR for reading.");
    }

    absl::Status status;
    struct sdr_get_rs *header;
    while ((header = std::any_cast<struct sdr_get_rs *>(
                ipmitool_intf_.SdrGetNextHeader(intf_, itr))) != nullptr) {
      if (header->type == SDR_RECORD_TYPE_FULL_SENSOR ||
          header->type == SDR_RECORD_TYPE_COMPACT_SENSOR) {
        struct sdr_record_common_sensor *rec;
        rec = reinterpret_cast<struct sdr_record_common_sensor *>(
            ipmitool_intf_.SdrGetRecord(intf_, header, itr));
        if (rec == nullptr) {
          InfoLog() << "Fail to get SDR record";
          continue;
        }

        std::unique_ptr<IpmiFullCompactSensor> sensor =
            IpmiFullCompactSensor::Create(rec, header->type);
        if (sensor == nullptr) {
          continue;
        }

        fullcompact_sensors_cache_.emplace(sensor->GetSensorNum(),
                                           std::move(sensor));
      }
    }
    // Frees the memory allocated by ipmi_sdr_start
    ipmitool_intf_.SdrEnd(intf_, itr);

    return status;
  }

  std::string IpmiResponseToString(uint8_t code) {
    const struct valstr *curr = &completion_code_vals[0];

    // completion_code_vals is a null-entry terminated array.
    while (curr->str != nullptr) {
      if (curr->val == code) return curr->str;

      curr++;
    }

    return "unknown response code";
  }

  void ConfigureLanPlusInterface(ipmi_intf *intf) {
    // Default is name-only lookup, from ipmitool's ipmi_main.c
    constexpr uint8_t kIpmiDefaultLookupBit = 0x10;

    // Default is empty, from ipmitool's ipmi_main.c
    uint8_t kgkey[IPMI_KG_BUFFER_SIZE] = {0};

    // The following values are all defaults taken from the implementation in
    // google3/v1_8_18_c3939da/lib/ipmi_main.c.
    ipmitool_intf_.SessionSetKgkey(intf, kgkey);
    ipmitool_intf_.SessionSetPrivlvl(intf, IPMI_SESSION_PRIV_ADMIN);
    ipmitool_intf_.SessionSetLookupbit(intf, kIpmiDefaultLookupBit);
    ipmitool_intf_.SessionSetSolEscapeChar(intf, SOL_ESCAPE_CHARACTER_DEFAULT);
    // Default from table 22-19 of the IPMIv2 spec, from ipmitool's ipmi_main.c
    ipmitool_intf_.SessionSetCipherSuiteId(intf, IPMI_LANPLUS_CIPHER_SUITE_3);
    intf->devnum = 0;
    intf->devfile = nullptr;
    intf->ai_family = AF_UNSPEC;
    intf->my_addr = IPMI_BMC_SLAVE_ADDR;
    intf->target_addr = IPMI_BMC_SLAVE_ADDR;
  }

  std::string ReadFruName(struct sdr_record_fru_locator *fru) {
    return std::string(reinterpret_cast<const char *>(fru->id_string),
                       fru->id_code & 0x1f);
  }

  // Find the SDR for the specified sensor.
  absl::Status GrabSensorSdrInternal(struct ipmi_intf *ipmi,
                                     SensorNum sensor_num) {
    // Fake an iterator.
    struct ipmi_sdr_iterator itr, *sdr_list_itr = nullptr;
    memset(&itr, 0x00, sizeof(itr));

    // Try to grab some details (could just call get_device_id ourselves, but
    // this is just as straightforward.
    sdr_list_itr = ipmi_sdr_start(ipmi, 0);
    if (sdr_list_itr == nullptr) {
      return absl::InternalError("Unable to open SDR for reading");
    }

    itr.reservation = sdr_list_itr->reservation;
    itr.use_built_in = sdr_list_itr->use_built_in;
    itr.next = sensor_num.id;

    // store the SDR id used for logging before freeing sdr_list_itr
    int sdr_id = sdr_list_itr->next;

    // Frees the memory allocated by ipmi_sdr_start
    ipmi_sdr_end(sdr_list_itr);

    // Returns itr's current, and sets itr->next.
    struct sdr_get_rs *header;
    while ((header = ipmi_sdr_get_next_header(ipmi, &itr)) != nullptr) {
      if (header->type != SDR_RECORD_TYPE_FULL_SENSOR &&
          header->type != SDR_RECORD_TYPE_COMPACT_SENSOR &&
          header->type != SDR_RECORD_TYPE_EVENTONLY_SENSOR) {
        continue;
      }

      struct sdr_record_common_sensor *rec;
      // Allocates memory of size (header->length+1) to hold raw sdr data
      // in ipmi_sdr_get_record
      rec = reinterpret_cast<struct sdr_record_common_sensor *>(
          ipmi_sdr_get_record(ipmi, header, &itr));

      if (rec == nullptr) {
        continue;
      }

      if (header->type == SDR_RECORD_TYPE_FULL_SENSOR ||
          header->type == SDR_RECORD_TYPE_COMPACT_SENSOR) {
        auto sensor = IpmiFullCompactSensor::Create(rec, header->type);
        if (sensor == nullptr) {
          continue;
        }
        if (sensor->GetId() != sensor_num.id ||
            sensor->GetLun() != sensor_num.lun) {
          continue;
        }
        InfoLog() << absl::StrFormat("Found %s at SDR id %d",
                                     sensor->ToPrintableString(), sdr_id);
        fullcompact_sensors_cache_.emplace(sensor->GetSensorNum(),
                                           std::move(sensor));
      } else {
        return absl::InternalError(absl::StrFormat(
            "Unexpected header type %d for SDR id %d", header->type, sdr_id));
      }

      return absl::OkStatus();
    }
    return absl::NotFoundError(absl::StrFormat("SDR for %d:%d not found",
                                               sensor_num.lun, sensor_num.id));
  }

  absl::StatusOr<IpmiFullCompactSensor *> GrabSensorInternal(
      struct ipmi_intf *ipmi, SensorNum sensor_num) {
    // Look up the sdr_record from the sensor map, and look it up via ipmi if
    // it's missing.
    auto sensor_iter = fullcompact_sensors_cache_.find(sensor_num);
    if (sensor_iter == fullcompact_sensors_cache_.end()) {
      auto result = GrabSensorSdrInternal(ipmi, sensor_num);
      if (result.ok()) {
        sensor_iter = fullcompact_sensors_cache_.find(sensor_num);
      }
    }

    if (sensor_iter == fullcompact_sensors_cache_.end()) {
      return absl::NotFoundError(absl::StrFormat(
          "ipmi has no sensor number %d.%d", sensor_num.lun, sensor_num.id));
    }

    return sensor_iter->second.get();
  }
};

Ipmitool::Ipmitool(absl::optional<ecclesia::MagentConfig::IpmiCredential> cred)
    : ipmi_impl_(absl::make_unique<IpmitoolImpl>(cred)) {}

}  // namespace ecclesia
