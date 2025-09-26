// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/vectorized_io_utils.h"

#include <cstddef>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quiche {
namespace {

using ::testing::ElementsAre;

TEST(VectorizedIoUtils, GatherStringViewSpanEmpty) {
  std::vector<absl::string_view> views = {"a", "b", "c"};
  size_t bytes_copied = GatherStringViewSpan(views, absl::Span<char>());
  EXPECT_EQ(bytes_copied, 0);
}

TEST(VectorizedIoUtils, GatherStringViewSpanSingle) {
  std::vector<absl::string_view> views = {"test"};
  char buffer_small[2];
  size_t bytes_copied =
      GatherStringViewSpan(views, absl::MakeSpan(buffer_small));
  EXPECT_EQ(bytes_copied, 2);
  EXPECT_THAT(buffer_small, ElementsAre('t', 'e'));

  char buffer_exact[4];
  bytes_copied = GatherStringViewSpan(views, absl::MakeSpan(buffer_exact));
  EXPECT_EQ(bytes_copied, 4);
  EXPECT_THAT(buffer_exact, ElementsAre('t', 'e', 's', 't'));

  char buffer_large[6] = {'\0'};
  bytes_copied = GatherStringViewSpan(views, absl::MakeSpan(buffer_large));
  EXPECT_EQ(bytes_copied, 4);
  EXPECT_THAT(buffer_large, ElementsAre('t', 'e', 's', 't', '\0', '\0'));
}

TEST(VectorizedIoUtils, GatherStringViewSpanMultiple) {
  const std::vector<absl::string_view> views = {"foo", ",", "bar"};
  constexpr absl::string_view kViewsJoined = "foo,bar";
  char buffer[kViewsJoined.size()];
  for (size_t i = 0; i < sizeof(buffer); ++i) {
    size_t bytes_copied =
        GatherStringViewSpan(views, absl::Span<char>(buffer, i));
    absl::string_view result(buffer, bytes_copied);
    EXPECT_EQ(result, kViewsJoined.substr(0, i));
  }
}

TEST(VectorizedIoUtils, GatherStringViewSpanEmptyElement) {
  const std::vector<absl::string_view> views = {"foo", "", "bar"};
  constexpr absl::string_view kViewsJoined = "foobar";
  char buffer[kViewsJoined.size()];
  size_t bytes_copied = GatherStringViewSpan(views, absl::MakeSpan(buffer));
  absl::string_view result(buffer, bytes_copied);
  EXPECT_EQ(result, kViewsJoined);
}

TEST(VectorizedIoUtils, GatherStringViewSpanLarge) {
  const std::string a(8192, 'a');
  const std::string b(8192, 'b');
  const std::vector<absl::string_view> views = {a, b};
  const std::string joined = a + b;
  char buffer[8192 * 2];
  size_t bytes_copied = GatherStringViewSpan(views, absl::MakeSpan(buffer));
  EXPECT_EQ(bytes_copied, 8192 * 2);
  absl::string_view result(buffer, bytes_copied);
  EXPECT_EQ(result, joined);
}

}  // namespace
}  // namespace quiche
