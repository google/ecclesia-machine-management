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

// This class provides ipmitool interface, enables dependency injection
// We do not want to pollute our .h files with  ipmitool C header files.

#ifndef ECCLESIA_MAGENT_LIB_IPMI_IPMITOOL_INTERFACE_H_
#define ECCLESIA_MAGENT_LIB_IPMI_IPMITOOL_INTERFACE_H_

#include <any>
#include <cstdint>

namespace ecclesia {

class IpmitoolInterface {
 public:
  virtual ~IpmitoolInterface() {}

  // expected type for intf: struct ipmi_intf *
  virtual void SessionSetKgkey(std::any intf, const uint8_t *kgkey);

  // expected type for intf: struct ipmi_intf *
  virtual void SessionSetPrivlvl(std::any intf, uint8_t privlvl);

  // expected type for intf: struct ipmi_intf *
  virtual void SessionSetLookupbit(std::any intf, uint8_t lookupbit);

  // expected type for intf: struct ipmi_intf *
  virtual void SessionSetSolEscapeChar(std::any intf, char sol_escape_char);

  virtual void SessionSetCipherSuiteId(std::any intf, uint8_t cipher_suite_id);

  // expected type for intf: struct ipmi_intf *
  virtual void SessionSetRetry(std::any intf, int retry);

  // expected type for intf: struct ipmi_intf *
  virtual void SessionSetTimeout(std::any intf, uint32_t timeout);

  // expected type for intf: struct ipmi_intf *
  virtual void SessionSetHostname(std::any intf, char *hostname);

  // expected type for intf: struct ipmi_intf *
  virtual void SessionSetPort(std::any intf, int port);

  // expected type for intf: struct ipmi_intf *
  virtual void SessionSetUsername(std::any intf, char *username);

  // expected type for intf: struct ipmi_intf *
  virtual void SessionSetPassword(std::any intf, char *password);

  // expected type for intf: struct ipmi_intf *
  // expected return type: struct ipmi_sdr_iterator *
  virtual std::any SdrStart(std::any intf, int use_builtin);

  // expected type for intf: struct ipmi_intf *
  // expected type for i: struct ipmi_sdr_iterator *
  // expected return type: struct sdr_get_rs *
  virtual std::any SdrGetNextHeader(std::any intf, std::any i);

  // expected type for intf: struct ipmi_intf *
  // expected type for header: struct sdr_get_rs *
  // expected type for i: struct ipmi_sdr_iterator *
  virtual uint8_t *SdrGetRecord(std::any intf, std::any header, std::any i);

  // expected type for intf: struct ipmi_intf *
  // expected type for i: struct ipmi_sdr_iterator *
  virtual void SdrEnd(std::any intf, std::any i);
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_IPMI_IPMITOOL_INTERFACE_H_
