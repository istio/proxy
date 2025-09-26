// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/web_transport/test_tools/in_memory_stream.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_stream.h"
#include "quiche/web_transport/web_transport.h"

namespace webtransport::test {
namespace {

using ::testing::ElementsAre;

TEST(InMemoryStreamTest, ReadSpan) {
  InMemoryStream stream(0);
  char buffer[4] = {'\0'};

  Stream::ReadResult result = stream.Read(absl::MakeSpan(buffer));
  EXPECT_EQ(result.bytes_read, 0);
  EXPECT_FALSE(result.fin);

  stream.Receive("test");
  result = stream.Read(absl::MakeSpan(buffer));
  EXPECT_EQ(result.bytes_read, 4);
  EXPECT_FALSE(result.fin);
  EXPECT_THAT(buffer, ElementsAre('t', 'e', 's', 't'));
}

TEST(InMemoryStreamTest, ReadString) {
  InMemoryStream stream(0);
  std::string buffer = "> ";

  stream.Receive("test");
  Stream::ReadResult result = stream.Read(&buffer);
  EXPECT_EQ(result.bytes_read, 4);
  EXPECT_EQ(buffer, "> test");
}

TEST(InMemoryStreamTest, ReadFin) {
  InMemoryStream stream(0);
  char buffer[1];

  stream.Receive("ab", /*fin=*/true);
  Stream::ReadResult result = stream.Read(absl::MakeSpan(buffer));
  EXPECT_EQ(result.bytes_read, 1);
  EXPECT_FALSE(result.fin);
  EXPECT_EQ(buffer[0], 'a');

  result = stream.Read(absl::MakeSpan(buffer));
  EXPECT_EQ(result.bytes_read, 1);
  EXPECT_TRUE(result.fin);
  EXPECT_EQ(buffer[0], 'b');
}

TEST(InMemoryStreamTest, Peek) {
  std::string chunk_a(8192, 'a');
  std::string chunk_b(8192, 'a');

  InMemoryStream stream(0);
  stream.Receive(chunk_a);
  stream.Receive(chunk_b, /*fin=*/true);

  Stream::PeekResult result = stream.PeekNextReadableRegion();
  EXPECT_EQ(result.peeked_data[0], 'a');
  EXPECT_TRUE(result.all_data_received);

  std::string merged_result;
  bool fin_reached = quiche::ProcessAllReadableRegions(
      stream,
      [&](absl::string_view chunk) { absl::StrAppend(&merged_result, chunk); });
  EXPECT_EQ(merged_result, absl::StrCat(chunk_a, chunk_b));
  EXPECT_TRUE(fin_reached);
}

}  // namespace
}  // namespace webtransport::test
