// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/test_tools/mock_streams.h"

#include <array>
#include <string>

#include "absl/types/span.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_stream.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace quiche::test {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(MockWriteStreamTest, DefaultWrite) {
  MockWriteStream stream;
  QUICHE_EXPECT_OK(quiche::WriteIntoStream(stream, "test"));
  EXPECT_EQ(stream.data(), "test");
  EXPECT_FALSE(stream.fin_written());
}

TEST(ReadStreamFromStringTest, ReadIntoSpan) {
  std::string source = "abcdef";
  std::array<char, 3> buffer;
  ReadStreamFromString stream(&source);
  EXPECT_EQ(stream.ReadableBytes(), 6);

  stream.Read(absl::MakeSpan(buffer));
  EXPECT_THAT(buffer, ElementsAre('a', 'b', 'c'));
  EXPECT_EQ(stream.ReadableBytes(), 3);

  stream.Read(absl::MakeSpan(buffer));
  EXPECT_THAT(buffer, ElementsAre('d', 'e', 'f'));
  EXPECT_EQ(stream.ReadableBytes(), 0);
  EXPECT_THAT(source, IsEmpty());
}

TEST(ReadStreamFromStringTest, ReadIntoString) {
  std::string source = "abcdef";
  std::string destination;
  ReadStreamFromString stream(&source);
  stream.Read(&destination);
  EXPECT_EQ(destination, "abcdef");
  EXPECT_THAT(source, IsEmpty());
}

TEST(ReadStreamFromStringTest, PeekAndSkip) {
  std::string source = "abcdef";
  ReadStreamFromString stream(&source);
  EXPECT_EQ(stream.PeekNextReadableRegion().peeked_data, "abcdef");
  stream.SkipBytes(2);
  EXPECT_EQ(stream.PeekNextReadableRegion().peeked_data, "cdef");
  EXPECT_EQ(source, "cdef");
}

}  // namespace
}  // namespace quiche::test
