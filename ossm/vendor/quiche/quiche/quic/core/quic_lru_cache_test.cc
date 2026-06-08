// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_lru_cache.h"

#include <memory>
#include <utility>

#include "quiche/quic/platform/api/quic_test.h"

namespace quic {
namespace test {
namespace {

struct CachedItem {
  explicit CachedItem(uint32_t new_value) : value(new_value) {}

  uint32_t value;
};

TEST(QuicLRUCacheTest, InsertAndLookup) {
  QuicLRUCache<int, CachedItem> cache(5);
  EXPECT_EQ(cache.end(), cache.Lookup(1));
  EXPECT_EQ(0u, cache.Size());
  EXPECT_EQ(5u, cache.MaxSize());

  // Check that item 1 was properly inserted.
  auto item1 = std::make_unique<CachedItem>(11);
  cache.Insert(1, std::move(item1));
  EXPECT_EQ(1u, cache.Size());
  EXPECT_EQ(11u, cache.Lookup(1)->second->value);

  // Check that item 2 overrides item 1.
  auto item2 = std::make_unique<CachedItem>(12);
  cache.Insert(1, std::move(item2));
  EXPECT_EQ(1u, cache.Size());
  EXPECT_EQ(12u, cache.Lookup(1)->second->value);

  auto item3 = std::make_unique<CachedItem>(13);
  cache.Insert(3, std::move(item3));
  EXPECT_EQ(2u, cache.Size());
  auto iter = cache.Lookup(3);
  ASSERT_NE(cache.end(), iter);
  EXPECT_EQ(13u, iter->second->value);
  cache.Erase(iter);
  ASSERT_EQ(cache.end(), cache.Lookup(3));
  EXPECT_EQ(1u, cache.Size());

  // No memory leakage.
  cache.Clear();
  EXPECT_EQ(0u, cache.Size());
}

TEST(QuicLRUCacheTest, Eviction) {
  QuicLRUCache<int, CachedItem> cache(3);

  for (size_t i = 1; i <= 4; ++i) {
    auto item = std::make_unique<CachedItem>(10 + i);
    cache.Insert(i, std::move(item));
  }

  EXPECT_EQ(3u, cache.Size());
  EXPECT_EQ(3u, cache.MaxSize());

  // Make sure item 1 is evicted.
  EXPECT_EQ(cache.end(), cache.Lookup(1));
  EXPECT_EQ(14u, cache.Lookup(4)->second->value);

  EXPECT_EQ(12u, cache.Lookup(2)->second->value);
  auto item5 = std::make_unique<CachedItem>(15);
  cache.Insert(5, std::move(item5));
  // Make sure item 3 is evicted.
  EXPECT_EQ(cache.end(), cache.Lookup(3));
  EXPECT_EQ(15u, cache.Lookup(5)->second->value);

  // No memory leakage.
  cache.Clear();
  EXPECT_EQ(0u, cache.Size());
}

TEST(QuicLRUCacheTest, UpdateMaxSize) {
  QuicLRUCache<int, CachedItem> cache(3);

  // Insert 3 items, filling the cache.
  for (size_t i = 1; i <= 3; ++i) {
    auto item = std::make_unique<CachedItem>(10 + i);
    cache.Insert(i, std::move(item));
  }
  EXPECT_EQ(3u, cache.Size());
  EXPECT_EQ(3u, cache.MaxSize());

  // Update max size to a larger value.
  cache.UpdateMaxSize(5);
  EXPECT_EQ(3u, cache.Size());
  EXPECT_EQ(5u, cache.MaxSize());

  // Insert more items, up to the new max size.
  for (size_t i = 4; i <= 5; ++i) {
    auto item = std::make_unique<CachedItem>(10 + i);
    cache.Insert(i, std::move(item));
  }
  EXPECT_EQ(5u, cache.Size());
  EXPECT_EQ(5u, cache.MaxSize());

  // Update max size to a smaller value, causing evictions.
  cache.UpdateMaxSize(2);
  EXPECT_EQ(2u, cache.Size());
  EXPECT_EQ(2u, cache.MaxSize());
  // Items 1, 2, and 3 should be evicted.
  EXPECT_EQ(cache.end(), cache.Lookup(1));
  EXPECT_EQ(cache.end(), cache.Lookup(2));
  EXPECT_EQ(cache.end(), cache.Lookup(3));
  // Items 4 and 5 should remain.
  EXPECT_NE(cache.end(), cache.Lookup(4));
  EXPECT_NE(cache.end(), cache.Lookup(5));

  // Insert an item after reducing max size to 2. This should evict item 4.
  auto item6 = std::make_unique<CachedItem>(16);
  cache.Insert(6, std::move(item6));
  EXPECT_EQ(2u, cache.Size());
  EXPECT_EQ(cache.end(), cache.Lookup(4));
  EXPECT_NE(cache.end(), cache.Lookup(5));
  EXPECT_NE(cache.end(), cache.Lookup(6));

  // Update max size to 0.
  cache.UpdateMaxSize(0);
  EXPECT_EQ(0u, cache.Size());
  EXPECT_EQ(0u, cache.MaxSize());
  EXPECT_EQ(cache.end(), cache.Lookup(5));
  EXPECT_EQ(cache.end(), cache.Lookup(6));
}

TEST(QuicLRUCacheTest, InsertWithZeroMaxSize) {
  QuicLRUCache<int, CachedItem> cache(0);
  EXPECT_EQ(0u, cache.Size());
  EXPECT_EQ(0u, cache.MaxSize());

  // Attempt to insert an item.
  auto item1 = std::make_unique<CachedItem>(1);
  cache.Insert(1, std::move(item1));

  // The cache should still be empty.
  EXPECT_EQ(0u, cache.Size());
  EXPECT_EQ(0u, cache.MaxSize());
  EXPECT_EQ(cache.end(), cache.Lookup(1));
}

}  // namespace
}  // namespace test
}  // namespace quic
