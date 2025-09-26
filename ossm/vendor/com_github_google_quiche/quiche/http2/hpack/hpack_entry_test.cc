// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/http2/hpack/hpack_entry.h"

#include "absl/hash/hash.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace spdy {

namespace {

TEST(HpackLookupEntryTest, EntryNamesDiffer) {
  HpackLookupEntry entry1{"header", "value"};
  HpackLookupEntry entry2{"HEADER", "value"};

  EXPECT_FALSE(entry1 == entry2);
  EXPECT_NE(absl::Hash<HpackLookupEntry>()(entry1),
            absl::Hash<HpackLookupEntry>()(entry2));
}

TEST(HpackLookupEntryTest, EntryValuesDiffer) {
  HpackLookupEntry entry1{"header", "value"};
  HpackLookupEntry entry2{"header", "VALUE"};

  EXPECT_FALSE(entry1 == entry2);
  EXPECT_NE(absl::Hash<HpackLookupEntry>()(entry1),
            absl::Hash<HpackLookupEntry>()(entry2));
}

TEST(HpackLookupEntryTest, EntriesEqual) {
  HpackLookupEntry entry1{"name", "value"};
  HpackLookupEntry entry2{"name", "value"};

  EXPECT_TRUE(entry1 == entry2);
  EXPECT_EQ(absl::Hash<HpackLookupEntry>()(entry1),
            absl::Hash<HpackLookupEntry>()(entry2));
}

TEST(HpackEntryTest, BasicEntry) {
  HpackEntry entry("header-name", "header value");

  EXPECT_EQ("header-name", entry.name());
  EXPECT_EQ("header value", entry.value());

  EXPECT_EQ(55u, entry.Size());
  EXPECT_EQ(55u, HpackEntry::Size("header-name", "header value"));
}

}  // namespace

}  // namespace spdy
