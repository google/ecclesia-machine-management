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

#ifndef ECCLESIA_MAGENT_SYSMODEL_X86_NVME_MOCK_H_
#define ECCLESIA_MAGENT_SYSMODEL_X86_NVME_MOCK_H_

#include <vector>

#include "gmock/gmock.h"
#include "absl/status/statusor.h"
#include "ecclesia/magent/sysmodel/x86/nvme.h"

namespace ecclesia {

class MockNvmeDiscover : public NvmeDiscoverInterface {
 public:
  MOCK_METHOD(absl::StatusOr<std::vector<NvmeLocation>>, GetAllNvmeLocations,
              (), (const, override));
  MOCK_METHOD(absl::StatusOr<std::vector<NvmePlugin>>, GetAllNvmePlugins, (),
              (const, override));
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_SYSMODEL_X86_NVME_MOCK_H_
