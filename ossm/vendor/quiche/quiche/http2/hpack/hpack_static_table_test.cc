// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/http2/hpack/hpack_static_table.h"

#include <set>
#include <vector>

#include "absl/strings/string_view.h"
#include "quiche/http2/hpack/hpack_constants.h"
#include "quiche/http2/hpack/hpack_header_table.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace spdy {

namespace test {

namespace {

class HpackStaticTableTest : public quiche::test::QuicheTest {
 protected:
  HpackStaticTableTest() : table_() {}

  HpackStaticTable table_;
};

// Check that an initialized instance has the right number of entries.
TEST_F(HpackStaticTableTest, Initialize) {
  EXPECT_FALSE(table_.IsInitialized());
  table_.Initialize(HpackStaticTableVector().data(),
                    HpackStaticTableVector().size());
  EXPECT_TRUE(table_.IsInitialized());

  const HpackHeaderTable::StaticEntryTable& static_entries =
      table_.GetStaticEntries();
  EXPECT_EQ(kStaticTableSize, static_entries.size());

  const HpackHeaderTable::NameValueToEntryMap& static_index =
      table_.GetStaticIndex();
  EXPECT_EQ(kStaticTableSize, static_index.size());

  const HpackHeaderTable::NameToEntryMap& static_name_index =
      table_.GetStaticNameIndex();
  // Count distinct names in static table.
  std::set<absl::string_view> names;
  for (const auto& entry : static_entries) {
    names.insert(entry.name());
  }
  EXPECT_EQ(names.size(), static_name_index.size());
}

// Test that ObtainHpackStaticTable returns the same instance every time.
TEST_F(HpackStaticTableTest, IsSingleton) {
  const HpackStaticTable* static_table_one = &ObtainHpackStaticTable();
  const HpackStaticTable* static_table_two = &ObtainHpackStaticTable();
  EXPECT_EQ(static_table_one, static_table_two);
}

}  // namespace

}  // namespace test

}  // namespace spdy
