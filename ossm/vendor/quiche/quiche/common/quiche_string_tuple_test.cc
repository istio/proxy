// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/quiche_string_tuple.h"

#include <optional>
#include <utility>
#include <vector>

#include "absl/hash/hash.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quiche::test {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

class QuicheStringTupleTest : public QuicheTest {};

TEST_F(QuicheStringTupleTest, DefaultState) {
  QuicheStringTuple<> tuple;
  EXPECT_EQ(tuple.size(), 0);
  EXPECT_TRUE(tuple.empty());
  EXPECT_FALSE(tuple.Pop());
  EXPECT_EQ(tuple.ValueAt(0), std::nullopt);
  EXPECT_EQ(tuple.ValueAt(0xffffffff), std::nullopt);
  EXPECT_EQ(tuple.begin(), tuple.end());
}

TEST_F(QuicheStringTupleTest, BasicOperations) {
  QuicheStringTuple<> tuple;
  EXPECT_TRUE(tuple.Add("foo"));
  EXPECT_EQ(tuple.size(), 1);
  EXPECT_TRUE(tuple.Add("bar"));
  EXPECT_EQ(tuple.size(), 2);

  EXPECT_EQ(tuple[0], "foo");
  EXPECT_EQ(tuple[1], "bar");
  EXPECT_EQ(tuple.front(), "foo");
  EXPECT_EQ(tuple.back(), "bar");

  EXPECT_TRUE(tuple.Pop());
  EXPECT_EQ(tuple.size(), 1);
  EXPECT_TRUE(tuple.Pop());
  EXPECT_EQ(tuple.size(), 0);
  EXPECT_FALSE(tuple.Pop());
}

TEST_F(QuicheStringTupleTest, EmptyElements) {
  QuicheStringTuple<> tuple({"a", "", "bc", "d", "", "e", "f"});
  EXPECT_THAT(tuple, ElementsAre("a", "", "bc", "d", "", "e", "f"));
}

TEST_F(QuicheStringTupleTest, Vector) {
  QuicheStringTuple<> tuple;
  EXPECT_TRUE(tuple.Add("foo"));
  EXPECT_TRUE(tuple.Add(""));
  EXPECT_TRUE(tuple.Add("bar"));
  std::vector<absl::string_view> vec(tuple.begin(), tuple.end());
  EXPECT_THAT(vec, ElementsAre("foo", "", "bar"));
}

TEST_F(QuicheStringTupleTest, Iterators) {
  QuicheStringTuple<> tuple;
  EXPECT_TRUE(tuple.Add("foo"));
  EXPECT_TRUE(tuple.Add("bar"));

  auto it = tuple.begin();
  EXPECT_EQ(*it, "foo");
  ++it;
  EXPECT_EQ(*it, "bar");
  it++;
  EXPECT_EQ(it, tuple.end());
  --it;
  EXPECT_EQ(*it, "bar");
  it--;
  EXPECT_EQ(*it, "foo");
}

TEST_F(QuicheStringTupleTest, Limits) {
  QuicheStringTuple<6> tuple;
  EXPECT_TRUE(tuple.Add("foo"));
  EXPECT_TRUE(tuple.Add("bar"));
  EXPECT_EQ(tuple.BytesLeft(), 0);
  EXPECT_FALSE(tuple.Add("x"));
  EXPECT_TRUE(tuple.Add(""));
  EXPECT_THAT(tuple, ElementsAre("foo", "bar", ""));
}

TEST_F(QuicheStringTupleTest, Copy) {
  QuicheStringTuple<> tuple({"foo", "bar"});
  QuicheStringTuple<> tuple2 = tuple;
  EXPECT_THAT(tuple, ElementsAre("foo", "bar"));
  EXPECT_THAT(tuple2, ElementsAre("foo", "bar"));
  EXPECT_EQ(tuple[0], tuple2[0]);
  EXPECT_NE(tuple[0].data(), tuple2[0].data());
}

TEST_F(QuicheStringTupleTest, Move) {
  QuicheStringTuple<> tuple({"foo", "bar"});
  QuicheStringTuple<> tuple2 = std::move(tuple);
  EXPECT_THAT(tuple, IsEmpty());  // NOLINT(bugprone-use-after-move)
  EXPECT_THAT(tuple2, ElementsAre("foo", "bar"));
}

TEST_F(QuicheStringTupleTest, OutOfBoundsAccess) {
  QuicheStringTuple<> tuple({"foo", "bar"});
  EXPECT_EQ(tuple.ValueAt(2), std::nullopt);
  EXPECT_QUICHE_BUG(tuple[2], "out-of-bounds");

  EXPECT_QUICHE_BUG(*tuple.end(), "out-of-bounds");
}

TEST_F(QuicheStringTupleTest, InitializerListTooLarge) {
  QuicheStringTuple<6> tuple({"foo", "bar"});
  EXPECT_QUICHE_BUG(QuicheStringTuple<5> tuple2({"foo", "bar"}), "exceeds");
}

TEST_F(QuicheStringTupleTest, Ordering) {
  QuicheStringTuple<> a({"a"});
  QuicheStringTuple<> ab({"a", "b"});
  QuicheStringTuple<> ac({"a", "c"});
  QuicheStringTuple<> bb({"b", "b"});
  EXPECT_EQ(ab, ab);
  EXPECT_NE(a, ab);
  EXPECT_LT(a, ab);
  EXPECT_LT(ab, ac);
  EXPECT_LT(ab, bb);

  EXPECT_NE(QuicheStringTuple<>({"ab", "c"}), QuicheStringTuple<>({"a", "bc"}));
}

TEST_F(QuicheStringTupleTest, Hashing) {
  QuicheStringTuple<> tuple1({"foo", "bar"});
  QuicheStringTuple<> tuple2({"foo", "bar"});
  EXPECT_EQ(absl::HashOf(tuple1), absl::HashOf(tuple2));
}

TEST_F(QuicheStringTupleTest, Stringify) {
  QuicheStringTuple<> tuple({"foo", "\xff", "bar"});
  EXPECT_EQ(absl::StrCat(tuple), R"({"foo", "\xff", "bar"})");
}

TEST_F(QuicheStringTupleTest, IsPrefix) {
  QuicheStringTuple<> a({"a"});
  QuicheStringTuple<> ab({"a", "b"});
  QuicheStringTuple<> abc({"a", "b", "c"});
  QuicheStringTuple<> ac({"a", "c"});

  EXPECT_TRUE(abc.IsPrefix(abc));
  EXPECT_TRUE(abc.IsPrefix(ab));
  EXPECT_TRUE(abc.IsPrefix(a));
  EXPECT_FALSE(abc.IsPrefix(ac));
  EXPECT_FALSE(a.IsPrefix(abc));
}

TEST_F(QuicheStringTupleTest, AppendTuple) {
  QuicheStringTuple<> prefix({"a", "bc"});
  QuicheStringTuple<> suffix({"def", "ghijk"});
  ASSERT_TRUE(prefix.Append(suffix));
  EXPECT_THAT(prefix, ElementsAre("a", "bc", "def", "ghijk"));
}

TEST_F(QuicheStringTupleTest, AppendSpan) {
  QuicheStringTuple<> prefix({"a", "bc"});
  std::vector<absl::string_view> suffix({"def", "ghijk"});
  ASSERT_TRUE(prefix.Append(suffix));
  EXPECT_THAT(prefix, ElementsAre("a", "bc", "def", "ghijk"));
}

TEST_F(QuicheStringTupleTest, RemovePrefix) {
  QuicheStringTuple<> prefix({"a", "bc"});
  QuicheStringTuple<> full({"a", "bc", "def", "ghijk", "lmn"});
  ASSERT_FALSE(prefix.ConsumePrefix(full));
  ASSERT_TRUE(full.ConsumePrefix(prefix));
  EXPECT_THAT(full, ElementsAre("def", "ghijk", "lmn"));
}

TEST_F(QuicheStringTupleTest, RemovePrefixThatIsTheSameTuple) {
  QuicheStringTuple<> prefix({"a", "bc"});
  QuicheStringTuple<> full({"a", "bc"});
  ASSERT_TRUE(full.ConsumePrefix(prefix));
  EXPECT_THAT(full, IsEmpty());
}

}  // namespace
}  // namespace quiche::test
