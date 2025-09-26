// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/http/http_header_block.h"

#include <memory>
#include <string>
#include <utility>

#include "quiche/http2/test_tools/spdy_test_utils.h"
#include "quiche/common/platform/api/quiche_test.h"

using ::testing::ElementsAre;

namespace quiche {
namespace test {

class ValueProxyPeer {
 public:
  static absl::string_view key(HttpHeaderBlock::ValueProxy* p) {
    return p->key_;
  }
};

std::pair<absl::string_view, absl::string_view> Pair(absl::string_view k,
                                                     absl::string_view v) {
  return std::make_pair(k, v);
}

// This test verifies that HttpHeaderBlock behaves correctly when empty.
TEST(HttpHeaderBlockTest, EmptyBlock) {
  HttpHeaderBlock block;
  EXPECT_TRUE(block.empty());
  EXPECT_EQ(0u, block.size());
  EXPECT_EQ(block.end(), block.find("foo"));
  EXPECT_FALSE(block.contains("foo"));
  EXPECT_TRUE(block.end() == block.begin());

  // Should have no effect.
  block.erase("bar");
}

TEST(HttpHeaderBlockTest, KeyMemoryReclaimedOnLookup) {
  HttpHeaderBlock block;
  absl::string_view copied_key1;
  {
    auto proxy1 = block["some key name"];
    copied_key1 = ValueProxyPeer::key(&proxy1);
  }
  absl::string_view copied_key2;
  {
    auto proxy2 = block["some other key name"];
    copied_key2 = ValueProxyPeer::key(&proxy2);
  }
  // Because proxy1 was never used to modify the block, the memory used for the
  // key could be reclaimed and used for the second call to operator[].
  // Therefore, we expect the pointers of the two absl::string_views to be
  // equal.
  EXPECT_EQ(copied_key1.data(), copied_key2.data());

  {
    auto proxy1 = block["some key name"];
    block["some other key name"] = "some value";
  }
  // Nothing should blow up when proxy1 is destructed, and we should be able to
  // modify and access the HttpHeaderBlock.
  block["key"] = "value";
  EXPECT_EQ("value", block["key"]);
  EXPECT_EQ("some value", block["some other key name"]);
  EXPECT_TRUE(block.find("some key name") == block.end());
}

// This test verifies that headers can be set in a variety of ways.
TEST(HttpHeaderBlockTest, AddHeaders) {
  HttpHeaderBlock block;
  block["foo"] = std::string(300, 'x');
  block["bar"] = "baz";
  block["qux"] = "qux1";
  block["qux"] = "qux2";
  block.insert(std::make_pair("key", "value"));

  EXPECT_EQ(Pair("foo", std::string(300, 'x')), *block.find("foo"));
  EXPECT_EQ("baz", block["bar"]);
  std::string qux("qux");
  EXPECT_EQ("qux2", block[qux]);
  ASSERT_NE(block.end(), block.find("key"));
  ASSERT_TRUE(block.contains("key"));
  EXPECT_EQ(Pair("key", "value"), *block.find("key"));

  block.erase("key");
  EXPECT_EQ(block.end(), block.find("key"));
}

// This test verifies that HttpHeaderBlock can be copied using Clone().
TEST(HttpHeaderBlockTest, CopyBlocks) {
  HttpHeaderBlock block1;
  block1["foo"] = std::string(300, 'x');
  block1["bar"] = "baz";
  block1.insert(std::make_pair("qux", "qux1"));

  HttpHeaderBlock block2 = block1.Clone();
  HttpHeaderBlock block3(block1.Clone());

  EXPECT_EQ(block1, block2);
  EXPECT_EQ(block1, block3);
}

TEST(HttpHeaderBlockTest, Equality) {
  // Test equality and inequality operators.
  HttpHeaderBlock block1;
  block1["foo"] = "bar";

  HttpHeaderBlock block2;
  block2["foo"] = "bar";

  HttpHeaderBlock block3;
  block3["baz"] = "qux";

  EXPECT_EQ(block1, block2);
  EXPECT_NE(block1, block3);

  block2["baz"] = "qux";
  EXPECT_NE(block1, block2);
}

HttpHeaderBlock ReturnTestHeaderBlock() {
  HttpHeaderBlock block;
  block["foo"] = "bar";
  block.insert(std::make_pair("foo2", "baz"));
  return block;
}

// Test that certain methods do not crash on moved-from instances.
TEST(HttpHeaderBlockTest, MovedFromIsValid) {
  HttpHeaderBlock block1;
  block1["foo"] = "bar";

  HttpHeaderBlock block2(std::move(block1));
  EXPECT_THAT(block2, ElementsAre(Pair("foo", "bar")));

  block1["baz"] = "qux";  // NOLINT  testing post-move behavior

  HttpHeaderBlock block3(std::move(block1));

  block1["foo"] = "bar";  // NOLINT  testing post-move behavior

  HttpHeaderBlock block4(std::move(block1));

  block1.clear();  // NOLINT  testing post-move behavior
  EXPECT_TRUE(block1.empty());

  block1["foo"] = "bar";
  EXPECT_THAT(block1, ElementsAre(Pair("foo", "bar")));

  HttpHeaderBlock block5 = ReturnTestHeaderBlock();
  block5.AppendValueOrAddHeader("foo", "bar2");
  EXPECT_THAT(block5, ElementsAre(Pair("foo", std::string("bar\0bar2", 8)),
                                  Pair("foo2", "baz")));
}

// This test verifies that headers can be appended to no matter how they were
// added originally.
TEST(HttpHeaderBlockTest, AppendHeaders) {
  HttpHeaderBlock block;
  block["foo"] = "foo";
  block.AppendValueOrAddHeader("foo", "bar");
  EXPECT_EQ(Pair("foo", std::string("foo\0bar", 7)), *block.find("foo"));

  block.insert(std::make_pair("foo", "baz"));
  EXPECT_EQ("baz", block["foo"]);
  EXPECT_EQ(Pair("foo", "baz"), *block.find("foo"));

  // Try all four methods of adding an entry.
  block["cookie"] = "key1=value1";
  block.AppendValueOrAddHeader("h1", "h1v1");
  block.insert(std::make_pair("h2", "h2v1"));

  block.AppendValueOrAddHeader("h3", "h3v2");
  block.AppendValueOrAddHeader("h2", "h2v2");
  block.AppendValueOrAddHeader("h1", "h1v2");
  block.AppendValueOrAddHeader("cookie", "key2=value2");

  block.AppendValueOrAddHeader("cookie", "key3=value3");
  block.AppendValueOrAddHeader("h1", "h1v3");
  block.AppendValueOrAddHeader("h2", "h2v3");
  block.AppendValueOrAddHeader("h3", "h3v3");
  block.AppendValueOrAddHeader("h4", "singleton");

  // Check for Set-Cookie header folding.
  block.AppendValueOrAddHeader("set-cookie", "yummy");
  block.AppendValueOrAddHeader("set-cookie", "scrumptious");

  EXPECT_EQ("key1=value1; key2=value2; key3=value3", block["cookie"]);
  EXPECT_EQ("baz", block["foo"]);
  EXPECT_EQ(std::string("h1v1\0h1v2\0h1v3", 14), block["h1"]);
  EXPECT_EQ(std::string("h2v1\0h2v2\0h2v3", 14), block["h2"]);
  EXPECT_EQ(std::string("h3v2\0h3v3", 9), block["h3"]);
  EXPECT_EQ("singleton", block["h4"]);
  EXPECT_EQ(std::string("yummy\0scrumptious", 17), block["set-cookie"]);
}

TEST(HttpHeaderBlockTest, CompareValueToStringPiece) {
  HttpHeaderBlock block;
  block["foo"] = "foo";
  block.AppendValueOrAddHeader("foo", "bar");
  const auto& val = block["foo"];
  const char expected[] = "foo\0bar";
  EXPECT_TRUE(absl::string_view(expected, 7) == val);
  EXPECT_TRUE(val == absl::string_view(expected, 7));
  EXPECT_FALSE(absl::string_view(expected, 3) == val);
  EXPECT_FALSE(val == absl::string_view(expected, 3));
  const char not_expected[] = "foo\0barextra";
  EXPECT_FALSE(absl::string_view(not_expected, 12) == val);
  EXPECT_FALSE(val == absl::string_view(not_expected, 12));

  const auto& val2 = block["foo2"];
  EXPECT_FALSE(absl::string_view(expected, 7) == val2);
  EXPECT_FALSE(val2 == absl::string_view(expected, 7));
  EXPECT_FALSE(absl::string_view("") == val2);
  EXPECT_FALSE(val2 == absl::string_view(""));
}

// This test demonstrates that the HttpHeaderBlock data structure does not
// place any limitations on the characters present in the header names.
TEST(HttpHeaderBlockTest, UpperCaseNames) {
  HttpHeaderBlock block;
  block["Foo"] = "foo";
  block.AppendValueOrAddHeader("Foo", "bar");
  EXPECT_NE(block.end(), block.find("foo"));
  EXPECT_EQ(Pair("Foo", std::string("foo\0bar", 7)), *block.find("Foo"));

  // The map is case insensitive, so updating "foo" modifies the entry
  // previously added.
  block.AppendValueOrAddHeader("foo", "baz");
  EXPECT_THAT(block,
              ElementsAre(Pair("Foo", std::string("foo\0bar\0baz", 11))));
}

namespace {
size_t HttpHeaderBlockSize(const HttpHeaderBlock& block) {
  size_t size = 0;
  for (const auto& pair : block) {
    size += pair.first.size() + pair.second.size();
  }
  return size;
}
}  // namespace

// Tests HttpHeaderBlock SizeEstimate().
TEST(HttpHeaderBlockTest, TotalBytesUsed) {
  HttpHeaderBlock block;
  const size_t value_size = 300;
  block["foo"] = std::string(value_size, 'x');
  EXPECT_EQ(block.TotalBytesUsed(), HttpHeaderBlockSize(block));
  block.insert(std::make_pair("key", std::string(value_size, 'x')));
  EXPECT_EQ(block.TotalBytesUsed(), HttpHeaderBlockSize(block));
  block.AppendValueOrAddHeader("abc", std::string(value_size, 'x'));
  EXPECT_EQ(block.TotalBytesUsed(), HttpHeaderBlockSize(block));

  // Replace value for existing key.
  block["foo"] = std::string(value_size, 'x');
  EXPECT_EQ(block.TotalBytesUsed(), HttpHeaderBlockSize(block));
  block.insert(std::make_pair("key", std::string(value_size, 'x')));
  EXPECT_EQ(block.TotalBytesUsed(), HttpHeaderBlockSize(block));
  // Add value for existing key.
  block.AppendValueOrAddHeader("abc", std::string(value_size, 'x'));
  EXPECT_EQ(block.TotalBytesUsed(), HttpHeaderBlockSize(block));

  // Copies/clones HttpHeaderBlock.
  size_t block_size = block.TotalBytesUsed();
  HttpHeaderBlock block_copy = std::move(block);
  EXPECT_EQ(block_size, block_copy.TotalBytesUsed());

  // Erases key.
  block_copy.erase("foo");
  EXPECT_EQ(block_copy.TotalBytesUsed(), HttpHeaderBlockSize(block_copy));
  block_copy.erase("key");
  EXPECT_EQ(block_copy.TotalBytesUsed(), HttpHeaderBlockSize(block_copy));
  block_copy.erase("abc");
  EXPECT_EQ(block_copy.TotalBytesUsed(), HttpHeaderBlockSize(block_copy));
}

// The order of header fields is preserved.  Note that all pseudo-header fields
// must appear before regular header fields, both in HTTP/2 and HTTP/3, see
// https://www.rfc-editor.org/rfc/rfc9113.html#name-http-control-data and
// https://www.rfc-editor.org/rfc/rfc9114.html#name-http-control-data.  It is
// the responsibility of the higher layer to add header fields in the correct
// order.
TEST(HttpHeaderBlockTest, OrderPreserved) {
  HttpHeaderBlock block;
  block[":method"] = "GET";
  block["foo"] = "bar";
  block[":path"] = "/";

  EXPECT_THAT(block, ElementsAre(Pair(":method", "GET"), Pair("foo", "bar"),
                                 Pair(":path", "/")));
}

TEST(HttpHeaderBlockTest, InsertReturnValue) {
  HttpHeaderBlock block;
  EXPECT_EQ(HttpHeaderBlock::InsertResult::kInserted,
            block.insert({"foo", "bar"}));
  EXPECT_EQ(HttpHeaderBlock::InsertResult::kReplaced,
            block.insert({"foo", "baz"}));
}

}  // namespace test
}  // namespace quiche
