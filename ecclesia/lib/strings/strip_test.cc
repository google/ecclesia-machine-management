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

#include "ecclesia/lib/strings/strip.h"

#include "gtest/gtest.h"

namespace ecclesia {
namespace {

TEST(StringStripTest, TrimSuffixWhitespace) {
  EXPECT_EQ(TrimSuffixWhitespace("abc "), "abc");
  EXPECT_EQ(TrimSuffixWhitespace("abc xyz   "), "abc xyz");
  EXPECT_EQ(TrimSuffixWhitespace("abc\t"), "abc");
  EXPECT_EQ(TrimSuffixWhitespace(" abc"), " abc");
  EXPECT_EQ(TrimSuffixWhitespace(" a b c "), " a b c");
  EXPECT_EQ(TrimSuffixWhitespace(""), "");
  EXPECT_EQ(TrimSuffixWhitespace(" "), "");
}

TEST(StringStripTest, TrimPrefixWhitespace) {
  EXPECT_EQ(TrimPrefixWhitespace("abc "), "abc ");
  EXPECT_EQ(TrimPrefixWhitespace(" abc xyz"), "abc xyz");
  EXPECT_EQ(TrimPrefixWhitespace("\tabc\t"), "abc\t");
  EXPECT_EQ(TrimPrefixWhitespace(" abc"), "abc");
  EXPECT_EQ(TrimPrefixWhitespace(" a b c "), "a b c ");
  EXPECT_EQ(TrimPrefixWhitespace(""), "");
  EXPECT_EQ(TrimPrefixWhitespace(" "), "");
}

TEST(StringStripTest, TrimString) {
  EXPECT_EQ(TrimString("abc "), "abc");
  EXPECT_EQ(TrimString(" abc xyz "), "abc xyz");
  EXPECT_EQ(TrimString("\tabc\t"), "abc");
  EXPECT_EQ(TrimString(" abc"), "abc");
  EXPECT_EQ(TrimString(" a b c "), "a b c");
  EXPECT_EQ(TrimString(""), "");
  EXPECT_EQ(TrimString(" "), "");
}

}  // namespace
}  // namespace ecclesia
