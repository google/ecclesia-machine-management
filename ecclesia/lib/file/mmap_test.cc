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

#include "ecclesia/lib/file/mmap.h"

#include <cstdint>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;

class MappedMemoryTest : public ::testing::Test {
 protected:
  MappedMemoryTest() : fs_(GetTestTempdirPath()) {}

  TestFilesystem fs_;
};

TEST_F(MappedMemoryTest, ReadOnlyWorksOnSimpleFile) {
  fs_.CreateFile("/a.txt", "0123456789\n");
  auto maybe_mmap = MappedMemory::Create(fs_.GetTruePath("/a.txt"), 0, 11,
                                         MappedMemory::Type::kReadOnly);
  ASSERT_THAT(maybe_mmap, IsOk());

  MappedMemory mmap = std::move(*maybe_mmap);
  EXPECT_EQ(mmap.MemoryAsStringView(), "0123456789\n");

  // The span should be the same as the string view.
  EXPECT_THAT(mmap.MemoryAsStringView(),
              ElementsAreArray(mmap.MemoryAsReadOnlySpan()));

  // This should also work even if an unsigned type is used in the span.
  EXPECT_THAT(mmap.MemoryAsStringView(),
              ElementsAreArray(mmap.MemoryAsReadOnlySpan<unsigned char>()));
  EXPECT_THAT(mmap.MemoryAsStringView(),
              ElementsAreArray(mmap.MemoryAsReadOnlySpan<uint8_t>()));

  // The writable span should be empty.
  EXPECT_THAT(mmap.MemoryAsReadWriteSpan(), IsEmpty());
}

TEST_F(MappedMemoryTest, ReadOnlyFailsOnMissingFile) {
  EXPECT_THAT(MappedMemory::Create(fs_.GetTruePath("/b.txt"), 0, 11,
                                   MappedMemory::Type::kReadOnly),
              Not(IsOk()));
}

TEST_F(MappedMemoryTest, ReadOnlyWorksOnSmallerFile) {
  fs_.CreateFile("/c.txt", "0123456789\n");
  auto maybe_mmap = MappedMemory::Create(fs_.GetTruePath("/c.txt"), 0, 64,
                                         MappedMemory::Type::kReadOnly);
  ASSERT_THAT(maybe_mmap, IsOk());

  MappedMemory mmap = std::move(*maybe_mmap);
  EXPECT_EQ(mmap.MemoryAsStringView().size(), 64);
  EXPECT_EQ(mmap.MemoryAsStringView().substr(0, 11), "0123456789\n");
  EXPECT_EQ(mmap.MemoryAsStringView().substr(11, 64 - 11),
            std::string(64 - 11, '\0'));
}

TEST_F(MappedMemoryTest, ReadOnlyWorksWithOffset) {
  fs_.CreateFile("/d.txt", "0123456789\n");
  auto maybe_mmap = MappedMemory::Create(fs_.GetTruePath("/d.txt"), 5, 6,
                                         MappedMemory::Type::kReadOnly);
  ASSERT_THAT(maybe_mmap, IsOk());

  MappedMemory mmap = std::move(*maybe_mmap);
  EXPECT_EQ(mmap.MemoryAsStringView(), "56789\n");
}

TEST_F(MappedMemoryTest, ReadWriteWorksOnSimpleFile) {
  fs_.CreateFile("/e.txt", "0123456789\n");
  auto maybe_mmap = MappedMemory::Create(fs_.GetTruePath("/e.txt"), 0, 11,
                                         MappedMemory::Type::kReadWrite);
  ASSERT_THAT(maybe_mmap, IsOk());

  MappedMemory mmap = std::move(*maybe_mmap);
  EXPECT_EQ(mmap.MemoryAsStringView(), "0123456789\n");

  // The span should be the same as the string view.
  EXPECT_THAT(mmap.MemoryAsStringView(),
              ElementsAreArray(mmap.MemoryAsReadOnlySpan()));

  // This should also work even if an unsigned type is used in the span.
  EXPECT_THAT(mmap.MemoryAsStringView(),
              ElementsAreArray(mmap.MemoryAsReadOnlySpan<unsigned char>()));
  EXPECT_THAT(mmap.MemoryAsStringView(),
              ElementsAreArray(mmap.MemoryAsReadOnlySpan<uint8_t>()));

  // The writable span should also match the string view.
  EXPECT_THAT(mmap.MemoryAsStringView(),
              ElementsAreArray(mmap.MemoryAsReadWriteSpan()));
  EXPECT_THAT(mmap.MemoryAsStringView(),
              ElementsAreArray(mmap.MemoryAsReadWriteSpan<unsigned char>()));
  EXPECT_THAT(mmap.MemoryAsStringView(),
              ElementsAreArray(mmap.MemoryAsReadWriteSpan<uint8_t>()));
}

TEST_F(MappedMemoryTest, ReadWriteFailsOnMissingFile) {
  EXPECT_THAT(MappedMemory::Create(fs_.GetTruePath("/f.txt"), 0, 11,
                                   MappedMemory::Type::kReadWrite),
              Not(IsOk()));
}

TEST_F(MappedMemoryTest, ReadWriteWorksOnSmallerFile) {
  fs_.CreateFile("/g.txt", "0123456789\n");
  auto maybe_mmap = MappedMemory::Create(fs_.GetTruePath("/g.txt"), 0, 64,
                                         MappedMemory::Type::kReadWrite);
  ASSERT_THAT(maybe_mmap, IsOk());

  MappedMemory mmap = std::move(*maybe_mmap);
  EXPECT_EQ(mmap.MemoryAsStringView().size(), 64);
  EXPECT_EQ(mmap.MemoryAsStringView().substr(0, 11), "0123456789\n");
  EXPECT_EQ(mmap.MemoryAsStringView().substr(11, 64 - 11),
            std::string(64 - 11, '\0'));
}

TEST_F(MappedMemoryTest, ReadWriteWorksWithOffset) {
  fs_.CreateFile("/h.txt", "0123456789\n");
  auto maybe_mmap = MappedMemory::Create(fs_.GetTruePath("/h.txt"), 5, 6,
                                         MappedMemory::Type::kReadWrite);
  ASSERT_THAT(maybe_mmap, IsOk());

  MappedMemory mmap = std::move(*maybe_mmap);
  EXPECT_EQ(mmap.MemoryAsStringView(), "56789\n");
}

TEST_F(MappedMemoryTest, ReadWriteChangesAreSaved) {
  fs_.CreateFile("/i.txt", "0123456789\n");

  // Create a read-write mapping and use it to modify the file.
  {
    auto maybe_mmap = MappedMemory::Create(fs_.GetTruePath("/i.txt"), 0, 11,
                                           MappedMemory::Type::kReadWrite);
    ASSERT_THAT(maybe_mmap, IsOk());

    MappedMemory mmap = std::move(*maybe_mmap);
    EXPECT_EQ(mmap.MemoryAsStringView(), "0123456789\n");
    absl::Span<char> mmap_span = mmap.MemoryAsReadWriteSpan();
    mmap_span[0] = 'a';
    mmap_span[4] = 'b';
    mmap_span[9] = 'c';
    EXPECT_EQ(mmap.MemoryAsStringView(), "a123b5678c\n");
  }

  // Now create a read-only mapping, reading the file should give back the
  // changed values from modifying the read-write.
  {
    auto maybe_mmap = MappedMemory::Create(fs_.GetTruePath("/i.txt"), 0, 11,
                                           MappedMemory::Type::kReadOnly);
    ASSERT_THAT(maybe_mmap, IsOk());

    MappedMemory mmap = std::move(*maybe_mmap);
    EXPECT_EQ(mmap.MemoryAsStringView(), "a123b5678c\n");
  }
}

TEST_F(MappedMemoryTest, ReadWriteChangesExtendFile) {
  fs_.CreateFile("/i.txt", "0123456789\n");

  // Create a read-write mapping and use it to extend the file.
  {
    auto maybe_mmap = MappedMemory::Create(fs_.GetTruePath("/i.txt"), 0, 14,
                                           MappedMemory::Type::kReadWrite);
    ASSERT_THAT(maybe_mmap, IsOk());

    MappedMemory mmap = std::move(*maybe_mmap);
    EXPECT_EQ(mmap.MemoryAsStringView().substr(0, 11), "0123456789\n");
    EXPECT_EQ(mmap.MemoryAsStringView().substr(11, 3), std::string(3, '\0'));
    absl::Span<char> mmap_span = mmap.MemoryAsReadWriteSpan();
    mmap_span[11] = 'a';
    mmap_span[12] = 'b';
    mmap_span[13] = 'c';
    EXPECT_EQ(mmap.MemoryAsStringView(), "0123456789\nabc");
  }

  // Now create a read-only mapping, reading the file should give back the
  // changed values from modifying the read-write.
  {
    auto maybe_mmap = MappedMemory::Create(fs_.GetTruePath("/i.txt"), 0, 14,
                                           MappedMemory::Type::kReadOnly);
    ASSERT_THAT(maybe_mmap, IsOk());

    MappedMemory mmap = std::move(*maybe_mmap);
    EXPECT_EQ(mmap.MemoryAsStringView(), "0123456789\nabc");
  }
}

}  // namespace
}  // namespace ecclesia
