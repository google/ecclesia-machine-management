/*
 * Copyright 2021 Google LLC
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

// GMock support for mocking ipmitool
//
// This one file should handle all of our ipmitool mocking.
// All callers need to do is:
//  * #include this file
//  * call MockIpmitool::SetUp() in your SetUp() method
//  * call MockIpmitool::TearDown() in your TearDown() method
//  * call EXPECT_IPMITOOL_CALL(ipmitool_func, arguments ...) whenever you
//    expect an ipmitool call (it works mostly like EXPECT_CALL)
//  * use the defined ACTIONs to stub out the behaviore of the ipmitool
//    function calls. there is none in this case.

#ifndef ECCLESIA_LIB_IPMI_IPMITOOL_MOCK_H_
#define ECCLESIA_LIB_IPMI_IPMITOOL_MOCK_H_

#include <cstdint>
#include <memory>

#include "gmock/gmock.h"
#include "absl/memory/memory.h"

// Forward declarations
struct ipm_devid_rsp;
struct ipmi_intf;
struct ipmi_rq;
struct ipmi_rs;
struct ipmi_sdr_iterator;
struct sdr_get_rs;
struct sdr_record_common_sensor;
struct sdr_record_full_sensor;
struct sel_event_record;
struct sel_info;
struct sensor_reading;

namespace ecclesia {

// MockIpmitool allows us to mock calls to ipmitool's functions.
class MockIpmitoolFactory;

class MockIpmitool {
 public:
  MOCK_METHOD(struct ipmi_intf *, ipmi_intf_load, (char *));

  MOCK_METHOD(void, ipmi_cleanup, (struct ipmi_intf *));

  MOCK_METHOD(struct ipmi_sdr_iterator *, ipmi_sdr_start,
              (struct ipmi_intf * intf, int use_builtin));

  MOCK_METHOD(struct sdr_get_rs *, ipmi_sdr_get_next_header,
              (struct ipmi_intf * intf, struct ipmi_sdr_iterator *itr));

  MOCK_METHOD(uint8_t *, ipmi_sdr_get_record,
              (struct ipmi_intf * intf, struct sdr_get_rs *header,
               struct ipmi_sdr_iterator *itr));

  MOCK_METHOD(void, ipmi_sdr_end, (struct ipmi_sdr_iterator * itr));

  MOCK_METHOD(struct ipmi_rs *, ipmi_sdr_get_sensor_reading_ipmb,
              (struct ipmi_intf * intf, uint8_t sensor, uint8_t target,
               uint8_t lun, uint8_t channel));

  MOCK_METHOD(double, sdr_convert_sensor_reading,
              (struct sdr_record_full_sensor * sensor, uint8_t val));

  // This function is declared in ipmi_sensor.h, implemented in ipmi_sdr.c
  MOCK_METHOD(uint8_t, sdr_convert_sensor_value_to_raw,
              (struct sdr_record_full_sensor * sensor, double val));

  MOCK_METHOD(struct ipmi_rs *, ipmi_sdr_set_sensor_reading_ipmb,
              (struct ipmi_intf * intf, uint8_t sensor_num, uint8_t operation,
               uint8_t raw_val, uint8_t event_assert, uint8_t event_deassert,
               uint8_t target, uint8_t lun, uint8_t channel));

  MOCK_METHOD(struct sensor_reading *, ipmi_sdr_read_sensor_value,
              (struct ipmi_intf * intf, struct sdr_record_common_sensor *sensor,
               uint8_t sdr_record_type, int precision));

  MOCK_METHOD(struct ipmi_rs *, ipmi_sdr_get_sensor_thresholds,
              (struct ipmi_intf * intf, uint8_t sensor, uint8_t target,
               uint8_t lun, uint8_t channel));

  MOCK_METHOD(struct ipmi_rs *, ipmi_sdr_get_sensor_event_status,
              (struct ipmi_intf * intf, uint8_t sensor, uint8_t target,
               uint8_t lun, uint8_t channel));

  MOCK_METHOD(struct ipmi_rs *, ipmi_oem_send,
              (struct ipmi_intf * intf, uint8_t command,
               const uint8_t *msg_data, uint8_t msg_data_len));

  MOCK_METHOD(struct ipmi_rs *, ipmi_sendrecv,
              (struct ipmi_intf * intf, struct ipmi_rq *rq));

  MOCK_METHOD(int, ipmi_chassis_power_control,
              (struct ipmi_intf * intf, uint8_t command));

  MOCK_METHOD(struct ipm_devid_rsp *, ipmi_get_deviceid,
              (struct ipmi_intf * intf));

  MOCK_METHOD(int, ipmi_sel_clear, (struct ipmi_intf * intf));

  MOCK_METHOD(int, ipmi_sel_get_info,
              (struct ipmi_intf * intf, struct sel_info *info));

  MOCK_METHOD(uint16_t, ipmi_sel_get_std_entry,
              (struct ipmi_intf * intf, uint16_t id,
               struct sel_event_record *evt));

 protected:
  MockIpmitool() {}
  virtual ~MockIpmitool() {}
};

class MockIpmitoolFactory {
 public:
  // Get a pointer to the global instance of the Mock.
  static testing::StrictMock<MockIpmitool> *Get() {
    return GetUniquePtr().get();
  }

 private:
  static std::unique_ptr<testing::StrictMock<MockIpmitool>> &GetUniquePtr() {
    static auto kInstance =
        std::make_unique<testing::StrictMock<MockIpmitool>>();
    return kInstance;
  }
};

extern "C" {
struct ipmi_intf *ipmi_intf_load(char *name) {
  return MockIpmitoolFactory::Get()->ipmi_intf_load(name);
}

void ipmi_cleanup(struct ipmi_intf *intf) {
  MockIpmitoolFactory::Get()->ipmi_cleanup(intf);
}

struct ipmi_sdr_iterator *ipmi_sdr_start(struct ipmi_intf *intf,
                                         int use_builtin) {
  return MockIpmitoolFactory::Get()->ipmi_sdr_start(intf, use_builtin);
}

struct sdr_get_rs *ipmi_sdr_get_next_header(struct ipmi_intf *intf,
                                            struct ipmi_sdr_iterator *itr) {
  return MockIpmitoolFactory::Get()->ipmi_sdr_get_next_header(intf, itr);
}

// Returns raw sdr record
uint8_t *ipmi_sdr_get_record(struct ipmi_intf *intf,  // NOLINT
                             struct sdr_get_rs *header,
                             struct ipmi_sdr_iterator *itr) {
  return MockIpmitoolFactory::Get()->ipmi_sdr_get_record(intf, header, itr);
}

// Frees the memory allocated from ipmi_sdr_start
void ipmi_sdr_end(struct ipmi_sdr_iterator *itr) {
  return MockIpmitoolFactory::Get()->ipmi_sdr_end(itr);
}

struct ipmi_rs *ipmi_sdr_get_sensor_reading_ipmb(struct ipmi_intf *intf,
                                                 uint8_t sensor, uint8_t target,
                                                 uint8_t lun, uint8_t channel) {
  return MockIpmitoolFactory::Get()->ipmi_sdr_get_sensor_reading_ipmb(
      intf, sensor, target, lun, channel);
}

struct sensor_reading *ipmi_sdr_read_sensor_value(  // NOLINT
    struct ipmi_intf *intf, struct sdr_record_common_sensor *sensor,
    uint8_t sdr_record_type, int precision) {
  return MockIpmitoolFactory::Get()->ipmi_sdr_read_sensor_value(
      intf, sensor, sdr_record_type, precision);
}

double sdr_convert_sensor_reading(struct sdr_record_full_sensor *sensor,
                                  uint8_t val) {
  return MockIpmitoolFactory::Get()->sdr_convert_sensor_reading(sensor, val);
}

// This function is declared in ipmi_sensor.h, implemented in ipmi_sdr.c
struct ipmi_rs *ipmi_sdr_set_sensor_reading_ipmb(  // NOLINT
    struct ipmi_intf *intf, uint8_t sensor, uint8_t operation, uint8_t raw_val,
    uint8_t event_assert, uint8_t event_deassert, uint8_t target, uint8_t lun,
    uint8_t channel) {
  return MockIpmitoolFactory::Get()->ipmi_sdr_set_sensor_reading_ipmb(
      intf, sensor, operation, raw_val, event_assert, event_deassert, target,
      lun, channel);
}

struct ipmi_rs *ipmi_sdr_get_sensor_thresholds(  // NOLINT
    struct ipmi_intf *intf, uint8_t sensor, uint8_t target, uint8_t lun,
    uint8_t channel) {
  return MockIpmitoolFactory::Get()->ipmi_sdr_get_sensor_thresholds(
      intf, sensor, target, lun, channel);
}

uint8_t sdr_convert_sensor_value_to_raw(  // NOLINT
    struct sdr_record_full_sensor *sensor, double reading) {
  return MockIpmitoolFactory::Get()->sdr_convert_sensor_value_to_raw(sensor,
                                                                     reading);
}

// This function is declared in ipmi_sdr.h, implemented in ipmi_sdr.c
struct ipmi_rs *ipmi_sdr_get_sensor_event_status(  // NOLINT
    struct ipmi_intf *intf, uint8_t sensor, uint8_t target, uint8_t lun,
    uint8_t channel) {
  return MockIpmitoolFactory::Get()->ipmi_sdr_get_sensor_event_status(
      intf, sensor, target, lun, channel);
}

struct ipmi_rs *ipmi_oem_send(struct ipmi_intf *intf,  // NOLINT
                              uint8_t command, const uint8_t *msg_data,
                              uint8_t msg_data_len) {
  return MockIpmitoolFactory::Get()->ipmi_oem_send(intf, command, msg_data,
                                                   msg_data_len);
}

struct ipmi_rs *ipmi_sendrecv(struct ipmi_intf *intf,  // NOLINT
                              struct ipmi_rq *rq) {
  return MockIpmitoolFactory::Get()->ipmi_sendrecv(intf, rq);
}

int ipmi_chassis_power_control(struct ipmi_intf *intf,  // NOLINT
                               uint8_t command) {
  return MockIpmitoolFactory::Get()->ipmi_chassis_power_control(intf, command);
}

struct ipm_devid_rsp *ipmi_get_deviceid(struct ipmi_intf *intf) {  // NOLINT
  return MockIpmitoolFactory::Get()->ipmi_get_deviceid(intf);
}

int ipmi_sel_get_info(struct ipmi_intf *intf, struct sel_info *info) {
  return MockIpmitoolFactory::Get()->ipmi_sel_get_info(intf, info);
}

uint16_t ipmi_sel_get_std_entry(struct ipmi_intf *intf, uint16_t id,
                                struct sel_event_record *evt) {
  return MockIpmitoolFactory::Get()->ipmi_sel_get_std_entry(intf, id, evt);
}

int ipmi_sel_clear(struct ipmi_intf *intf) {
  return MockIpmitoolFactory::Get()->ipmi_sel_clear(intf);
}

}  // extern "C"

}  // namespace ecclesia

// This is a convenient way to wrap around EXPECT_CALL
#define EXPECT_IPMITOOL_CALL(name, ...) \
  EXPECT_CALL(*ecclesia::MockIpmitoolFactory::Get(), name(__VA_ARGS__))

#endif  // ECCLESIA_LIB_IPMI_IPMITOOL_MOCK_H_
