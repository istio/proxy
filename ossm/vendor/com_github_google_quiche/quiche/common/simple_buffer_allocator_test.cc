// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/simple_buffer_allocator.h"

#include <utility>

#include "quiche/common/platform/api/quiche_test.h"

namespace quiche {
namespace {

TEST(SimpleBufferAllocatorTest, NewDelete) {
  SimpleBufferAllocator alloc;
  char* buf = alloc.New(4);
  EXPECT_NE(nullptr, buf);
  alloc.Delete(buf);
}

TEST(SimpleBufferAllocatorTest, DeleteNull) {
  SimpleBufferAllocator alloc;
  alloc.Delete(nullptr);
}

TEST(SimpleBufferAllocatorTest, MoveBuffersConstructor) {
  SimpleBufferAllocator alloc;
  QuicheBuffer buffer1(&alloc, 16);

  EXPECT_NE(buffer1.data(), nullptr);
  EXPECT_EQ(buffer1.size(), 16u);

  QuicheBuffer buffer2(std::move(buffer1));
  EXPECT_EQ(buffer1.data(), nullptr);  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(buffer1.size(), 0u);
  EXPECT_NE(buffer2.data(), nullptr);
  EXPECT_EQ(buffer2.size(), 16u);
}

TEST(SimpleBufferAllocatorTest, MoveBuffersAssignment) {
  SimpleBufferAllocator alloc;
  QuicheBuffer buffer1(&alloc, 16);
  QuicheBuffer buffer2;

  EXPECT_NE(buffer1.data(), nullptr);
  EXPECT_EQ(buffer1.size(), 16u);
  EXPECT_EQ(buffer2.data(), nullptr);
  EXPECT_EQ(buffer2.size(), 0u);

  buffer2 = std::move(buffer1);
  EXPECT_EQ(buffer1.data(), nullptr);  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(buffer1.size(), 0u);
  EXPECT_NE(buffer2.data(), nullptr);
  EXPECT_EQ(buffer2.size(), 16u);
}

TEST(SimpleBufferAllocatorTest, CopyBuffer) {
  SimpleBufferAllocator alloc;
  const absl::string_view original = "Test string";
  QuicheBuffer copy = QuicheBuffer::Copy(&alloc, original);
  EXPECT_EQ(copy.AsStringView(), original);
}

}  // namespace
}  // namespace quiche
