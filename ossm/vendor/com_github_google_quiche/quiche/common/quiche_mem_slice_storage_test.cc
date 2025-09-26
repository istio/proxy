// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/quiche_mem_slice_storage.h"

#include <string>

#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/simple_buffer_allocator.h"

namespace quiche {
namespace test {
namespace {

class QuicheMemSliceStorageImplTest : public QuicheTest {
 public:
  QuicheMemSliceStorageImplTest() = default;
};

TEST_F(QuicheMemSliceStorageImplTest, EmptyIov) {
  QuicheMemSliceStorage storage(nullptr, 0, nullptr, 1024);
  EXPECT_TRUE(storage.ToSpan().empty());
}

TEST_F(QuicheMemSliceStorageImplTest, SingleIov) {
  SimpleBufferAllocator allocator;
  std::string body(3, 'c');
  struct iovec iov = {const_cast<char*>(body.data()), body.length()};
  QuicheMemSliceStorage storage(&iov, 1, &allocator, 1024);
  auto span = storage.ToSpan();
  EXPECT_EQ("ccc", span[0].AsStringView());
  EXPECT_NE(static_cast<const void*>(span[0].data()), body.data());
}

TEST_F(QuicheMemSliceStorageImplTest, MultipleIovInSingleSlice) {
  SimpleBufferAllocator allocator;
  std::string body1(3, 'a');
  std::string body2(4, 'b');
  struct iovec iov[] = {{const_cast<char*>(body1.data()), body1.length()},
                        {const_cast<char*>(body2.data()), body2.length()}};

  QuicheMemSliceStorage storage(iov, 2, &allocator, 1024);
  auto span = storage.ToSpan();
  EXPECT_EQ("aaabbbb", span[0].AsStringView());
}

TEST_F(QuicheMemSliceStorageImplTest, MultipleIovInMultipleSlice) {
  SimpleBufferAllocator allocator;
  std::string body1(4, 'a');
  std::string body2(4, 'b');
  struct iovec iov[] = {{const_cast<char*>(body1.data()), body1.length()},
                        {const_cast<char*>(body2.data()), body2.length()}};

  QuicheMemSliceStorage storage(iov, 2, &allocator, 4);
  auto span = storage.ToSpan();
  EXPECT_EQ("aaaa", span[0].AsStringView());
  EXPECT_EQ("bbbb", span[1].AsStringView());
}

}  // namespace
}  // namespace test
}  // namespace quiche
