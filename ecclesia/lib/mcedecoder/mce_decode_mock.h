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

#ifndef ECCLESIA_LIB_MCEDECODER_MCE_DECODE_MOCK_H_
#define ECCLESIA_LIB_MCEDECODER_MCE_DECODE_MOCK_H_

#include "gmock/gmock.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/mcedecoder/mce_decode.h"
#include "ecclesia/lib/mcedecoder/mce_messages.h"

namespace ecclesia {

class MockMceDecoder : public MceDecoderInterface {
 public:
  MOCK_METHOD(absl::StatusOr<MceDecodedMessage>, DecodeMceMessage,
              (const MceLogMessage& raw_msg), (override));
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_MCEDECODER_MCE_DECODE_MOCK_H_
