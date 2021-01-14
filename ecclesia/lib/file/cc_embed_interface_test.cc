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

#include "ecclesia/lib/file/cc_embed_interface.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/file/all_files.h"
#include "ecclesia/lib/file/flat_files.h"
#include "ecclesia/lib/file/text_files.h"

namespace ecclesia {
namespace {

// NOTE: the binary file blob.bin is a 4k data file that contains the sequence
// 0 to 255, repeated 16 times.

TEST(EmbeddedFile, TextFileFromTextFiles) {
  ASSERT_EQ(kTextFiles.size(), 1);
  EXPECT_EQ(kTextFiles[0].name, "test_data/text.txt");
  EXPECT_EQ(kTextFiles[0].data, "1-1A\n1-1A-2B\n1B-2B-3\n");
}

TEST(EmbeddedFile, TextFileFromAllFiles) {
  ASSERT_EQ(ecclesia_testdata::kAllFiles.size(), 2);
  absl::string_view data = GetEmbeddedFileWithNameOrDie(
      "test_data/text.txt", ecclesia_testdata::kAllFiles);
  EXPECT_EQ(data, "1-1A\n1-1A-2B\n1B-2B-3\n");
}

TEST(EmbeddedFile, BinaryFileFromAllFiles) {
  ASSERT_EQ(ecclesia_testdata::kAllFiles.size(), 2);
  absl::string_view data = GetEmbeddedFileWithNameOrDie(
      "test_data/blob.bin", ecclesia_testdata::kAllFiles);
  ASSERT_EQ(data.size(), 4096);
  for (size_t i = 0; i < 4096; ++i) {
    uint8_t data_byte = static_cast<uint8_t>(data[i]);
    uint8_t expected_byte = static_cast<uint8_t>(i % 256);
    EXPECT_EQ(data_byte, expected_byte);
  }
}

TEST(EmbeddedFile, TextFileFromFlatFiles) {
  ASSERT_EQ(kFlatFiles.size(), 2);
  absl::string_view data = GetEmbeddedFileWithNameOrDie("text.txt", kFlatFiles);
  EXPECT_EQ(data, "1-1A\n1-1A-2B\n1B-2B-3\n");
}

TEST(EmbeddedFile, BinaryFileFromFlatFiles) {
  ASSERT_EQ(kFlatFiles.size(), 2);
  absl::string_view data = GetEmbeddedFileWithNameOrDie("blob.bin", kFlatFiles);
  ASSERT_EQ(data.size(), 4096);
  for (size_t i = 0; i < 4096; ++i) {
    uint8_t data_byte = static_cast<uint8_t>(data[i]);
    uint8_t expected_byte = static_cast<uint8_t>(i % 256);
    EXPECT_EQ(data_byte, expected_byte);
  }
}

TEST(EmbeddedFile, GetWithValidName) {
  // These checks should never return nullopt.
  EXPECT_NE(GetEmbeddedFileWithName("test_data/text.txt", kTextFiles),
            absl::nullopt);
  EXPECT_NE(GetEmbeddedFileWithName("test_data/text.txt",
                                    ecclesia_testdata::kAllFiles),
            absl::nullopt);
  EXPECT_NE(GetEmbeddedFileWithName("test_data/blob.bin",
                                    ecclesia_testdata::kAllFiles),
            absl::nullopt);
  // These checks should never die.
  GetEmbeddedFileWithNameOrDie("test_data/text.txt", kTextFiles);
  GetEmbeddedFileWithNameOrDie("test_data/text.txt",
                               ecclesia_testdata::kAllFiles);
  GetEmbeddedFileWithNameOrDie("test_data/blob.bin",
                               ecclesia_testdata::kAllFiles);
}

TEST(EmbeddedFile, GetWithBadName) {
  EXPECT_EQ(GetEmbeddedFileWithName("test_data/blob.bin", kTextFiles),
            absl::nullopt);
  EXPECT_EQ(GetEmbeddedFileWithName("test_data/blob.txt",
                                    ecclesia_testdata::kAllFiles),
            absl::nullopt);
  EXPECT_EQ(GetEmbeddedFileWithName("test_data/text.bin",
                                    ecclesia_testdata::kAllFiles),
            absl::nullopt);
}

TEST(EmbeddedFileDeathTest, GetWithBadName) {
  ASSERT_DEATH(GetEmbeddedFileWithNameOrDie("test_data/blob.bin", kTextFiles),
               "found no embedded file");
  ASSERT_DEATH(GetEmbeddedFileWithNameOrDie("test_data/blob.txt",
                                            ecclesia_testdata::kAllFiles),
               "found no embedded file");
  ASSERT_DEATH(GetEmbeddedFileWithNameOrDie("test_data/text.bin",
                                            ecclesia_testdata::kAllFiles),
               "found no embedded file");
}

}  // namespace
}  // namespace ecclesia
