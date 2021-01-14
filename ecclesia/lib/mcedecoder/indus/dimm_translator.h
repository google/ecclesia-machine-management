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

#ifndef ECCLESIA_LIB_MCEDECODER_INDUS_INDUS_DIMM_TRANSLATOR_H_
#define ECCLESIA_LIB_MCEDECODER_INDUS_INDUS_DIMM_TRANSLATOR_H_

#include "absl/status/statusor.h"
#include "ecclesia/lib/mcedecoder/dimm_translator.h"

namespace ecclesia {

// DIMM translator class for Indus server which has 2 CPU sockets, each CPU with
// 2 IMCs, each IMC with 3 IMC channels and each IMC channel with 2 slots.
class IndusDimmTranslator : public DimmTranslatorInterface {
 public:
  // imc_channel is 0~5, channel_slot is 0~1. If imc_id (0 or 1) and
  // imc_channel_id (0~2) are given instead, then the input imc_channel = imc_id
  // * 3 + imc_channel_id.
  absl::StatusOr<int> GetGldn(const DimmSlotId &dimm_slot) const override;

  // A valid GLDN is 0~23.
  absl::StatusOr<DimmSlotId> GldnToSlot(int gldn) const override;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_MCEDECODER_INDUS_INDUS_DIMM_TRANSLATOR_H_
