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

#ifndef ECCLESIA_LIB_STRINGS_STRIP_TEST_H_
#define ECCLESIA_LIB_STRINGS_STRIP_TEST_H_

#include "ecclesia/lib/strings/strip.h"

#include "gtest/gtest.h"

namespace ecclesia {
namespace {

TEST(StringStripTest, TrimSuffixWhiteSpaces) {
  EXPECT_EQ(TrimSuffixWhiteSpaces("abc "), "abc");
  EXPECT_EQ(TrimSuffixWhiteSpaces("abc xyz   "), "abc xyz");
  EXPECT_EQ(TrimSuffixWhiteSpaces("abc\t"), "abc");
  EXPECT_EQ(TrimSuffixWhiteSpaces(" abc"), " abc");
  EXPECT_EQ(TrimSuffixWhiteSpaces(" a b c "), " a b c");
  EXPECT_EQ(TrimSuffixWhiteSpaces(""), "");
  EXPECT_EQ(TrimSuffixWhiteSpaces(" "), "");
}

TEST(StringStripTest, TrimPrefixWhiteSpaces) {
  EXPECT_EQ(TrimPrefixWhiteSpaces("abc "), "abc ");
  EXPECT_EQ(TrimPrefixWhiteSpaces(" abc xyz"), "abc xyz");
  EXPECT_EQ(TrimPrefixWhiteSpaces("\tabc\t"), "abc\t");
  EXPECT_EQ(TrimPrefixWhiteSpaces(" abc"), "abc");
  EXPECT_EQ(TrimPrefixWhiteSpaces(" a b c "), "a b c ");
  EXPECT_EQ(TrimPrefixWhiteSpaces(""), "");
  EXPECT_EQ(TrimPrefixWhiteSpaces(" "), "");
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

#endif  // ECCLESIA_LIB_STRINGS_STRIP_TEST_H_
