// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/quiche_buffer_allocator.h"

#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/simple_buffer_allocator.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace quiche {
namespace test {
namespace {

TEST(QuicheBuffer, CopyFromEmpty) {
  SimpleBufferAllocator allocator;
  QuicheBuffer buffer = QuicheBuffer::Copy(&allocator, "");
  EXPECT_TRUE(buffer.empty());
}

TEST(QuicheBuffer, Copy) {
  SimpleBufferAllocator allocator;
  QuicheBuffer buffer = QuicheBuffer::Copy(&allocator, "foobar");
  EXPECT_EQ("foobar", buffer.AsStringView());
}

TEST(QuicheBuffer, CopyFromIovecZeroBytes) {
  const int buffer_length = 0;

  SimpleBufferAllocator allocator;
  QuicheBuffer buffer = QuicheBuffer::CopyFromIovec(
      &allocator, nullptr,
      /* iov_count = */ 0, /* iov_offset = */ 0, buffer_length);
  EXPECT_TRUE(buffer.empty());

  constexpr absl::string_view kData("foobar");
  iovec iov = MakeIOVector(kData);

  buffer = QuicheBuffer::CopyFromIovec(&allocator, &iov,
                                       /* iov_count = */ 1,
                                       /* iov_offset = */ 0, buffer_length);
  EXPECT_TRUE(buffer.empty());

  buffer = QuicheBuffer::CopyFromIovec(&allocator, &iov,
                                       /* iov_count = */ 1,
                                       /* iov_offset = */ 3, buffer_length);
  EXPECT_TRUE(buffer.empty());
}

TEST(QuicheBuffer, CopyFromIovecSimple) {
  constexpr absl::string_view kData("foobar");
  iovec iov = MakeIOVector(kData);

  SimpleBufferAllocator allocator;
  QuicheBuffer buffer =
      QuicheBuffer::CopyFromIovec(&allocator, &iov,
                                  /* iov_count = */ 1, /* iov_offset = */ 0,
                                  /* buffer_length = */ 6);
  EXPECT_EQ("foobar", buffer.AsStringView());

  buffer =
      QuicheBuffer::CopyFromIovec(&allocator, &iov,
                                  /* iov_count = */ 1, /* iov_offset = */ 0,
                                  /* buffer_length = */ 3);
  EXPECT_EQ("foo", buffer.AsStringView());

  buffer =
      QuicheBuffer::CopyFromIovec(&allocator, &iov,
                                  /* iov_count = */ 1, /* iov_offset = */ 3,
                                  /* buffer_length = */ 3);
  EXPECT_EQ("bar", buffer.AsStringView());

  buffer =
      QuicheBuffer::CopyFromIovec(&allocator, &iov,
                                  /* iov_count = */ 1, /* iov_offset = */ 1,
                                  /* buffer_length = */ 4);
  EXPECT_EQ("ooba", buffer.AsStringView());
}

TEST(QuicheBuffer, CopyFromIovecMultiple) {
  constexpr absl::string_view kData1("foo");
  constexpr absl::string_view kData2("bar");
  iovec iov[] = {MakeIOVector(kData1), MakeIOVector(kData2)};

  SimpleBufferAllocator allocator;
  QuicheBuffer buffer =
      QuicheBuffer::CopyFromIovec(&allocator, &iov[0],
                                  /* iov_count = */ 2, /* iov_offset = */ 0,
                                  /* buffer_length = */ 6);
  EXPECT_EQ("foobar", buffer.AsStringView());

  buffer =
      QuicheBuffer::CopyFromIovec(&allocator, &iov[0],
                                  /* iov_count = */ 2, /* iov_offset = */ 0,
                                  /* buffer_length = */ 3);
  EXPECT_EQ("foo", buffer.AsStringView());

  buffer =
      QuicheBuffer::CopyFromIovec(&allocator, &iov[0],
                                  /* iov_count = */ 2, /* iov_offset = */ 3,
                                  /* buffer_length = */ 3);
  EXPECT_EQ("bar", buffer.AsStringView());

  buffer =
      QuicheBuffer::CopyFromIovec(&allocator, &iov[0],
                                  /* iov_count = */ 2, /* iov_offset = */ 1,
                                  /* buffer_length = */ 4);
  EXPECT_EQ("ooba", buffer.AsStringView());
}

TEST(QuicheBuffer, CopyFromIovecOffsetTooLarge) {
  constexpr absl::string_view kData1("foo");
  constexpr absl::string_view kData2("bar");
  iovec iov[] = {MakeIOVector(kData1), MakeIOVector(kData2)};

  SimpleBufferAllocator allocator;
  EXPECT_QUICHE_BUG(
      QuicheBuffer::CopyFromIovec(&allocator, &iov[0],
                                  /* iov_count = */ 2, /* iov_offset = */ 10,
                                  /* buffer_length = */ 6),
      "iov_offset larger than iovec total size");
}

TEST(QuicheBuffer, CopyFromIovecTooManyBytesRequested) {
  constexpr absl::string_view kData1("foo");
  constexpr absl::string_view kData2("bar");
  iovec iov[] = {MakeIOVector(kData1), MakeIOVector(kData2)};

  SimpleBufferAllocator allocator;
  EXPECT_QUICHE_BUG(
      QuicheBuffer::CopyFromIovec(&allocator, &iov[0],
                                  /* iov_count = */ 2, /* iov_offset = */ 2,
                                  /* buffer_length = */ 10),
      R"(iov_offset \+ buffer_length larger than iovec total size)");
}

}  // anonymous namespace
}  // namespace test
}  // namespace quiche
