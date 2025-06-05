// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_messages.h"

#include <vector>

#include "absl/hash/hash.h"
#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace moqt::test {
namespace {

TEST(MoqtMessagesTest, FullTrackNameConstructors) {
  FullTrackName name1({"foo", "bar"});
  std::vector<absl::string_view> list = {"foo", "bar"};
  FullTrackName name2(list);
  EXPECT_EQ(name1, name2);
  EXPECT_EQ(absl::HashOf(name1), absl::HashOf(name2));
}

TEST(MoqtMessagesTest, FullTrackNameOrder) {
  FullTrackName name1({"a", "b"});
  FullTrackName name2({"a", "b", "c"});
  FullTrackName name3({"b", "a"});
  EXPECT_LT(name1, name2);
  EXPECT_LT(name2, name3);
  EXPECT_LT(name1, name3);
}

TEST(MoqtMessagesTest, FullTrackNameInNamespace) {
  FullTrackName name1({"a", "b"});
  FullTrackName name2({"a", "b", "c"});
  FullTrackName name3({"d", "b"});
  EXPECT_TRUE(name2.InNamespace(name1));
  EXPECT_FALSE(name1.InNamespace(name2));
  EXPECT_TRUE(name1.InNamespace(name1));
  EXPECT_FALSE(name2.InNamespace(name3));
}

TEST(MoqtMessagesTest, FullTrackNameToString) {
  FullTrackName name1({"a", "b"});
  EXPECT_EQ(name1.ToString(), R"({"a", "b"})");

  FullTrackName name2({"\xff", "\x61"});
  EXPECT_EQ(name2.ToString(), R"({"\xff", "a"})");
}

}  // namespace
}  // namespace moqt::test
