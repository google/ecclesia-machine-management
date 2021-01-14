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

#include "ecclesia/lib/file/path.h"

#include "gtest/gtest.h"

namespace ecclesia {
namespace {

TEST(GetBasename, Works) {
  EXPECT_EQ(GetBasename("/dir1/dir2/dir3"), "dir3");
  EXPECT_EQ(GetBasename("dir1/dir2/dir3"), "dir3");
  EXPECT_EQ(GetBasename("/dir1/dir2/dir3/"), "dir3");
  EXPECT_EQ(GetBasename("dir1/dir2/dir3/"), "dir3");
  EXPECT_EQ(GetBasename("dir1"), "dir1");
  EXPECT_EQ(GetBasename("//////dir1//////"), "dir1");
  EXPECT_EQ(GetBasename(""), "");
  EXPECT_EQ(GetBasename("/"), "");
}

TEST(GetDirname, Works) {
  EXPECT_EQ(GetDirname("/dir1/dir2/dir3"), "/dir1/dir2");
  EXPECT_EQ(GetDirname("dir1/dir2/dir3"), "dir1/dir2");
  EXPECT_EQ(GetDirname("/dir1/dir2/dir3/"), "/dir1/dir2");
  EXPECT_EQ(GetDirname("dir1/dir2/dir3/"), "dir1/dir2");
  EXPECT_EQ(GetDirname("dir1"), "");
  EXPECT_EQ(GetDirname("//////dir1//////"), "/");
  EXPECT_EQ(GetDirname(""), "");
  EXPECT_EQ(GetDirname("/"), "/");
}

TEST(JoinFilePaths, SimpleJoin) {
  EXPECT_EQ(JoinFilePaths("a", "b", "c"), "a/b/c");
  EXPECT_EQ(JoinFilePaths("/a", "b", "c"), "/a/b/c");
}

TEST(JoinFilePaths, PathsWithTrailingSlashes) {
  EXPECT_EQ(JoinFilePaths("a", "b/", "c/"), "a/b/c");
  EXPECT_EQ(JoinFilePaths("/a", "b", "c/"), "/a/b/c");
}

TEST(JoinFilePaths, AbsolutePathsOverridePriorParameters) {
  EXPECT_EQ(JoinFilePaths("base", "/abs1", "sub", "/abs2/sub", "final/"),
            "/abs2/sub/final");
  EXPECT_EQ(JoinFilePaths("/base", "/abs1", "sub", "/abs2/sub/", "final"),
            "/abs2/sub/final");
}

TEST(JoinFilePaths, PathIsRoot) {
  EXPECT_EQ(JoinFilePaths("/", "b", "c/"), "/b/c");
}

TEST(JoinFilePaths, EmptyPathsAreNoops) {
  EXPECT_EQ(JoinFilePaths("", "a", "b/", "", "c/"), "a/b/c");
  EXPECT_EQ(JoinFilePaths("/a", "", "b", "c/", ""), "/a/b/c");
}

TEST(JoinFilePaths, ConsecutiveSlashesArePruned) {
  EXPECT_EQ(JoinFilePaths("a", "b//c//d", "e/", "f//g"), "a/b/c/d/e/f/g");
}

}  // namespace
}  // namespace ecclesia
