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

#include "ecclesia/lib/cache/rcu_view.h"

#include <string>

#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "ecclesia/lib/cache/rcu_snapshot.h"
#include "ecclesia/lib/cache/rcu_store.h"

namespace ecclesia {
namespace {

TEST(RcuDirectView, ViewOfStore) {
  RcuStore<int> store(23);

  RcuDirectView<int> direct_view(store);
  RcuView<int> &view = direct_view;

  auto snapshot_from_store = store.Read();
  auto snapshot_from_view = view.Read();
  EXPECT_EQ(*snapshot_from_store, 23);
  EXPECT_EQ(*snapshot_from_view, 23);
  EXPECT_EQ(snapshot_from_store, snapshot_from_view);
}

// Test class for translating an int store into a string view.
class TestTranslatedRcuView : public TranslatedRcuView<int, std::string> {
 public:
  using TranslatedRcuView<int, std::string>::TranslatedRcuView;
  std::string Translate(const int &from) const override {
    translate_invocations_++;
    return absl::StrCat(from);
  }

  int translate_invocations() const { return translate_invocations_; }

 private:
  // A counter for the number of times Translate has been invoked.
  mutable int translate_invocations_ = 0;
};

TEST(TranslatedRcuView, TranslatedViewOfStore) {
  RcuStore<int> store(23);

  TestTranslatedRcuView translated_view(store);
  EXPECT_EQ(translated_view.translate_invocations(), 0);

  auto snapshot_from_store = store.Read();
  auto snapshot_from_view = translated_view.Read();
  EXPECT_EQ(*snapshot_from_store, 23);
  EXPECT_EQ(*snapshot_from_view, "23");
  EXPECT_EQ(translated_view.translate_invocations(), 1);
}

TEST(TranslatedRcuView, NoStoreUpdateNoAdditionalTranslate) {
  RcuStore<int> store(23);

  TestTranslatedRcuView translated_view(store);
  EXPECT_EQ(translated_view.translate_invocations(), 0);

  auto snapshot_from_store = store.Read();
  auto snapshot_from_view = translated_view.Read();
  EXPECT_EQ(*snapshot_from_store, 23);
  EXPECT_EQ(*snapshot_from_view, "23");
  EXPECT_EQ(translated_view.translate_invocations(), 1);

  auto snapshot_from_view2 = translated_view.Read();
  EXPECT_EQ(*snapshot_from_view2, "23");
  EXPECT_EQ(translated_view.translate_invocations(), 1);
}

TEST(TranslatedRcuView, StoreUpdateAdditionalTranslate) {
  RcuStore<int> store(23);

  TestTranslatedRcuView translated_view(store);
  EXPECT_EQ(translated_view.translate_invocations(), 0);

  auto snapshot_from_store = store.Read();
  auto snapshot_from_view = translated_view.Read();
  EXPECT_EQ(*snapshot_from_store, 23);
  EXPECT_EQ(*snapshot_from_view, "23");
  EXPECT_EQ(translated_view.translate_invocations(), 1);

  store.Update(42);

  auto snapshot_from_view2 = translated_view.Read();
  EXPECT_EQ(*snapshot_from_view2, "42");
  EXPECT_EQ(translated_view.translate_invocations(), 2);
}

// Test class which does an int -> int translation by multiplying the value by
// two. Because it is an int -> int translation we can layer multiple copies of
// this view, allowing us to test nesting.
class DoublingRcuView : public TranslatedRcuView<int, int> {
 public:
  using TranslatedRcuView<int, int>::TranslatedRcuView;
  int Translate(const int &from) const override { return from * 2; }
};

TEST(TranslatedRcuView, NestedTranslatedView) {
  RcuStore<int> store(47);
  DoublingRcuView view_x2(store);
  DoublingRcuView view_x4(static_cast<RcuView<int> &>(view_x2));

  auto snapshot_47x1 = store.Read();
  auto snapshot_47x2 = view_x2.Read();
  auto snapshot_47x4 = view_x4.Read();

  EXPECT_EQ(*snapshot_47x1, 47);
  EXPECT_EQ(*snapshot_47x2, 94);
  EXPECT_EQ(*snapshot_47x4, 188);

  store.Update(48);

  auto snapshot_48x1 = store.Read();
  auto snapshot_48x2 = view_x2.Read();
  auto snapshot_48x4 = view_x4.Read();

  EXPECT_EQ(*snapshot_48x1, 48);
  EXPECT_EQ(*snapshot_48x2, 96);
  EXPECT_EQ(*snapshot_48x4, 192);
}

}  // namespace
}  // namespace ecclesia
