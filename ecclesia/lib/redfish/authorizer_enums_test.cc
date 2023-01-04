/*
 * Copyright 2022 Google LLC
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

#include "ecclesia/lib/redfish/authorizer_enums.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace ecclesia {
namespace {

TEST(ResourceEntityToString, EntityAndStringHaveOneToOneMapping) {
  for (int i = 0; i <= static_cast<int>(ResourceEntity::kUndefined); ++i) {
    ResourceEntity entity = static_cast<ResourceEntity>(i);
    EXPECT_EQ(StringToResourceEntity(ResourceEntityToString(entity)), entity);
  }
}

TEST(OperationToString, OperationAndStringHaveOneToOneMapping) {
  for (int i = 0; i <= static_cast<int>(Operation::kUndefined); ++i) {
    Operation operation = static_cast<Operation>(i);
    EXPECT_EQ(StringToOperation(OperationToString(operation)), operation);
  }
}

}  // namespace
}  // namespace ecclesia
