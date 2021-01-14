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

#ifndef ECCLESIA_LIB_MCEDECODER_SKYLAKE_MCE_DECODE_H_
#define ECCLESIA_LIB_MCEDECODER_SKYLAKE_MCE_DECODE_H_

#include "ecclesia/lib/mcedecoder/dimm_translator.h"
#include "ecclesia/lib/mcedecoder/mce_messages.h"

namespace ecclesia {

// Decode Intel Skylake machine check exception. Add the decoded attributes and
// message and return true after success; otherwise, return false.
bool DecodeSkylakeMce(DimmTranslatorInterface *dimm_translator,
                      MceAttributes *attributes,
                      MceDecodedMessage *decoded_msg);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_MCEDECODER_SKYLAKE_MCE_DECODE_H_
