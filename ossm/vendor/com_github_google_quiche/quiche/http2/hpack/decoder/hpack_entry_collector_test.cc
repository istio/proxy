// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/http2/test_tools/hpack_entry_collector.h"

// Tests of HpackEntryCollector.

#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/platform/api/quiche_test.h"

using ::testing::HasSubstr;

namespace http2 {
namespace test {
namespace {

TEST(HpackEntryCollectorTest, Clear) {
  HpackEntryCollector collector;
  QUICHE_VLOG(1) << collector;
  EXPECT_THAT(collector.ToString(), HasSubstr("!started"));
  EXPECT_TRUE(collector.IsClear());
  collector.set_header_type(HpackEntryType::kIndexedLiteralHeader);
  EXPECT_FALSE(collector.IsClear());
  QUICHE_VLOG(1) << collector;
  collector.Clear();
  EXPECT_TRUE(collector.IsClear());
  collector.set_index(123);
  EXPECT_FALSE(collector.IsClear());
  QUICHE_VLOG(1) << collector;
  collector.Clear();
  EXPECT_TRUE(collector.IsClear());
  collector.set_name(HpackStringCollector("name", true));
  EXPECT_FALSE(collector.IsClear());
  QUICHE_VLOG(1) << collector;
  collector.Clear();
  EXPECT_TRUE(collector.IsClear());
  collector.set_value(HpackStringCollector("value", false));
  EXPECT_FALSE(collector.IsClear());
  QUICHE_VLOG(1) << collector;
}

// EXPECT_FATAL_FAILURE can not access variables in the scope of a test body,
// including the this variable so can not access non-static members. So, we
// define this test outside of the test body.
void IndexedHeaderErrorTest() {
  HpackEntryCollector collector;
  collector.OnIndexedHeader(1);
  // The next statement will fail because the collector
  // has already been used.
  collector.OnIndexedHeader(234);
}

TEST(HpackEntryCollectorTest, IndexedHeader) {
  HpackEntryCollector collector;
  collector.OnIndexedHeader(123);
  QUICHE_VLOG(1) << collector;
  EXPECT_FALSE(collector.IsClear());
  EXPECT_TRUE(collector.IsComplete());
  EXPECT_TRUE(collector.ValidateIndexedHeader(123));
  EXPECT_THAT(collector.ToString(), HasSubstr("IndexedHeader"));
  EXPECT_THAT(collector.ToString(), HasSubstr("Complete"));
  EXPECT_FATAL_FAILURE(IndexedHeaderErrorTest(), "Value of: started_");
}

void LiteralValueErrorTest() {
  HpackEntryCollector collector;
  collector.OnStartLiteralHeader(HpackEntryType::kIndexedLiteralHeader, 1);
  // OnNameStart is not expected because an index was specified for the name.
  collector.OnNameStart(false, 10);
}

TEST(HpackEntryCollectorTest, LiteralValueHeader) {
  HpackEntryCollector collector;
  collector.OnStartLiteralHeader(HpackEntryType::kIndexedLiteralHeader, 4);
  QUICHE_VLOG(1) << collector;
  EXPECT_FALSE(collector.IsClear());
  EXPECT_FALSE(collector.IsComplete());
  EXPECT_THAT(collector.ToString(), HasSubstr("!ended"));
  collector.OnValueStart(true, 5);
  QUICHE_VLOG(1) << collector;
  collector.OnValueData("value", 5);
  collector.OnValueEnd();
  QUICHE_VLOG(1) << collector;
  EXPECT_FALSE(collector.IsClear());
  EXPECT_TRUE(collector.IsComplete());
  EXPECT_TRUE(collector.ValidateLiteralValueHeader(
      HpackEntryType::kIndexedLiteralHeader, 4, true, "value"));
  EXPECT_THAT(collector.ToString(), HasSubstr("IndexedLiteralHeader"));
  EXPECT_THAT(collector.ToString(), HasSubstr("Complete"));
  EXPECT_FATAL_FAILURE(LiteralValueErrorTest(),
                       "Value of: LiteralNameExpected");
}

void LiteralNameValueHeaderErrorTest() {
  HpackEntryCollector collector;
  collector.OnStartLiteralHeader(HpackEntryType::kNeverIndexedLiteralHeader, 0);
  // OnValueStart is not expected until the name has ended.
  collector.OnValueStart(false, 10);
}

TEST(HpackEntryCollectorTest, LiteralNameValueHeader) {
  HpackEntryCollector collector;
  collector.OnStartLiteralHeader(HpackEntryType::kUnindexedLiteralHeader, 0);
  QUICHE_VLOG(1) << collector;
  EXPECT_FALSE(collector.IsClear());
  EXPECT_FALSE(collector.IsComplete());
  collector.OnNameStart(false, 4);
  collector.OnNameData("na", 2);
  QUICHE_VLOG(1) << collector;
  collector.OnNameData("me", 2);
  collector.OnNameEnd();
  collector.OnValueStart(true, 5);
  QUICHE_VLOG(1) << collector;
  collector.OnValueData("Value", 5);
  collector.OnValueEnd();
  QUICHE_VLOG(1) << collector;
  EXPECT_FALSE(collector.IsClear());
  EXPECT_TRUE(collector.IsComplete());
  EXPECT_TRUE(collector.ValidateLiteralNameValueHeader(
      HpackEntryType::kUnindexedLiteralHeader, false, "name", true, "Value"));
  EXPECT_FATAL_FAILURE(LiteralNameValueHeaderErrorTest(),
                       "Value of: name_.HasEnded");
}

void DynamicTableSizeUpdateErrorTest() {
  HpackEntryCollector collector;
  collector.OnDynamicTableSizeUpdate(123);
  EXPECT_FALSE(collector.IsClear());
  EXPECT_TRUE(collector.IsComplete());
  EXPECT_TRUE(collector.ValidateDynamicTableSizeUpdate(123));
  // The next statement will fail because the collector
  // has already been used.
  collector.OnDynamicTableSizeUpdate(234);
}

TEST(HpackEntryCollectorTest, DynamicTableSizeUpdate) {
  HpackEntryCollector collector;
  collector.OnDynamicTableSizeUpdate(8192);
  QUICHE_VLOG(1) << collector;
  EXPECT_FALSE(collector.IsClear());
  EXPECT_TRUE(collector.IsComplete());
  EXPECT_TRUE(collector.ValidateDynamicTableSizeUpdate(8192));
  EXPECT_EQ(collector,
            HpackEntryCollector(HpackEntryType::kDynamicTableSizeUpdate, 8192));
  EXPECT_NE(collector,
            HpackEntryCollector(HpackEntryType::kIndexedHeader, 8192));
  EXPECT_NE(collector,
            HpackEntryCollector(HpackEntryType::kDynamicTableSizeUpdate, 8191));
  EXPECT_FATAL_FAILURE(DynamicTableSizeUpdateErrorTest(), "Value of: started_");
}

}  // namespace
}  // namespace test
}  // namespace http2
