// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/http2/test_tools/hpack_block_collector.h"

#include <string>

// Tests of HpackBlockCollector. Not intended to be comprehensive, as
// HpackBlockCollector is itself support for testing HpackBlockDecoder, and
// should be pretty thoroughly exercised via the tests of HpackBlockDecoder.

#include "quiche/http2/test_tools/hpack_block_builder.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace http2 {
namespace test {
namespace {

TEST(HpackBlockCollectorTest, Clear) {
  HpackBlockCollector collector;
  EXPECT_TRUE(collector.IsClear());
  EXPECT_TRUE(collector.IsNotPending());

  collector.OnIndexedHeader(234);
  EXPECT_FALSE(collector.IsClear());
  EXPECT_TRUE(collector.IsNotPending());

  collector.Clear();
  EXPECT_TRUE(collector.IsClear());
  EXPECT_TRUE(collector.IsNotPending());

  collector.OnDynamicTableSizeUpdate(0);
  EXPECT_FALSE(collector.IsClear());
  EXPECT_TRUE(collector.IsNotPending());

  collector.Clear();
  collector.OnStartLiteralHeader(HpackEntryType::kIndexedLiteralHeader, 1);
  EXPECT_FALSE(collector.IsClear());
  EXPECT_FALSE(collector.IsNotPending());
}

TEST(HpackBlockCollectorTest, IndexedHeader) {
  HpackBlockCollector a;
  a.OnIndexedHeader(123);
  EXPECT_TRUE(a.ValidateSoleIndexedHeader(123));

  HpackBlockCollector b;
  EXPECT_FALSE(a.VerifyEq(b));

  b.OnIndexedHeader(1);
  EXPECT_TRUE(b.ValidateSoleIndexedHeader(1));
  EXPECT_FALSE(a.VerifyEq(b));

  b.Clear();
  b.OnIndexedHeader(123);
  EXPECT_TRUE(a.VerifyEq(b));

  b.OnIndexedHeader(234);
  EXPECT_FALSE(b.VerifyEq(a));
  a.OnIndexedHeader(234);
  EXPECT_TRUE(b.VerifyEq(a));

  std::string expected;
  {
    HpackBlockBuilder hbb;
    hbb.AppendIndexedHeader(123);
    hbb.AppendIndexedHeader(234);
    EXPECT_EQ(3u, hbb.size());
    expected = hbb.buffer();
  }
  std::string actual;
  {
    HpackBlockBuilder hbb;
    a.AppendToHpackBlockBuilder(&hbb);
    EXPECT_EQ(3u, hbb.size());
    actual = hbb.buffer();
  }
  EXPECT_EQ(expected, actual);
}

TEST(HpackBlockCollectorTest, DynamicTableSizeUpdate) {
  HpackBlockCollector a;
  a.OnDynamicTableSizeUpdate(0);
  EXPECT_TRUE(a.ValidateSoleDynamicTableSizeUpdate(0));

  HpackBlockCollector b;
  EXPECT_FALSE(a.VerifyEq(b));

  b.OnDynamicTableSizeUpdate(1);
  EXPECT_TRUE(b.ValidateSoleDynamicTableSizeUpdate(1));
  EXPECT_FALSE(a.VerifyEq(b));

  b.Clear();
  b.OnDynamicTableSizeUpdate(0);
  EXPECT_TRUE(a.VerifyEq(b));

  b.OnDynamicTableSizeUpdate(4096);
  EXPECT_FALSE(b.VerifyEq(a));
  a.OnDynamicTableSizeUpdate(4096);
  EXPECT_TRUE(b.VerifyEq(a));

  std::string expected;
  {
    HpackBlockBuilder hbb;
    hbb.AppendDynamicTableSizeUpdate(0);
    hbb.AppendDynamicTableSizeUpdate(4096);
    EXPECT_EQ(4u, hbb.size());
    expected = hbb.buffer();
  }
  std::string actual;
  {
    HpackBlockBuilder hbb;
    a.AppendToHpackBlockBuilder(&hbb);
    EXPECT_EQ(4u, hbb.size());
    actual = hbb.buffer();
  }
  EXPECT_EQ(expected, actual);
}

}  // namespace
}  // namespace test
}  // namespace http2
