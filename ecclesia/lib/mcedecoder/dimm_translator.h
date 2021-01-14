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

#ifndef ECCLESIA_LIB_MCEDECODER_DIMM_TRANSLATOR_H_
#define ECCLESIA_LIB_MCEDECODER_DIMM_TRANSLATOR_H_

#include "absl/status/statusor.h"

namespace ecclesia {

// A DIMM slot can be uniquely identified by CPU socket ID, the IMC channel ID
// within CPU socket and the channel slot within a IMC channel.
struct DimmSlotId {
  int socket_id;
  int imc_channel;
  int channel_slot;
};

// Interface class of DIMM translator.
class DimmTranslatorInterface {
 public:
  virtual ~DimmTranslatorInterface() {}
  // Get Google Logical DIMM Number (GLDN) from the socket ID, IMC channel
  // number and channel slot ID. Returns a not-OK status if there is not GLDN
  // associated with the slot, which generally means that the slot ID that was
  // passed does not match any actually existing slot on the platform.
  virtual absl::StatusOr<int> GetGldn(const DimmSlotId &dimm_slot) const = 0;

  // This method translates a GLDN to a DIMM slot. Returns a non-OK status if
  // the platform has no slot that would have that GLDN associated with it.
  virtual absl::StatusOr<DimmSlotId> GldnToSlot(int gldn) const = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_MCEDECODER_DIMM_TRANSLATOR_H_
