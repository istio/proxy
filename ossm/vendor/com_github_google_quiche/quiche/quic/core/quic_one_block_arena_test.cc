// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_one_block_arena.h"

#include <cstdint>
#include <vector>

#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

namespace quic::test {
namespace {

static const uint32_t kMaxAlign = 8;

struct TestObject {
  uint32_t value;
};

class QuicOneBlockArenaTest : public QuicTest {};

TEST_F(QuicOneBlockArenaTest, AllocateSuccess) {
  QuicOneBlockArena<1024> arena;
  QuicArenaScopedPtr<TestObject> ptr = arena.New<TestObject>();
  EXPECT_TRUE(ptr.is_from_arena());
}

TEST_F(QuicOneBlockArenaTest, Exhaust) {
  QuicOneBlockArena<1024> arena;
  for (size_t i = 0; i < 1024 / kMaxAlign; ++i) {
    QuicArenaScopedPtr<TestObject> ptr = arena.New<TestObject>();
    EXPECT_TRUE(ptr.is_from_arena());
  }
  QuicArenaScopedPtr<TestObject> ptr;
  EXPECT_QUIC_BUG(ptr = arena.New<TestObject>(),
                  "Ran out of space in QuicOneBlockArena");
  EXPECT_FALSE(ptr.is_from_arena());
}

TEST_F(QuicOneBlockArenaTest, NoOverlaps) {
  QuicOneBlockArena<1024> arena;
  std::vector<QuicArenaScopedPtr<TestObject>> objects;
  QuicIntervalSet<uintptr_t> used;
  for (size_t i = 0; i < 1024 / kMaxAlign; ++i) {
    QuicArenaScopedPtr<TestObject> ptr = arena.New<TestObject>();
    EXPECT_TRUE(ptr.is_from_arena());

    uintptr_t begin = reinterpret_cast<uintptr_t>(ptr.get());
    uintptr_t end = begin + sizeof(TestObject);
    EXPECT_FALSE(used.Contains(begin));
    EXPECT_FALSE(used.Contains(end - 1));
    used.Add(begin, end);
  }
}

}  // namespace
}  // namespace quic::test
