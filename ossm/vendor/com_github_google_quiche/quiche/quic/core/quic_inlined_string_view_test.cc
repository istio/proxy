// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_inlined_string_view.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quic {
namespace {

TEST(QuicInlinedStringViewTest, DefaultConstructor) {
  QuicInlinedStringView<24> view;
  EXPECT_EQ(view.data(), nullptr);
  EXPECT_EQ(view.size(), 0);
  EXPECT_EQ(view.view(), absl::string_view());
  EXPECT_TRUE(view.IsInlined());

  QuicInlinedStringView<24> view_from_empty("");
  EXPECT_EQ(view_from_empty.data(), nullptr);
  EXPECT_EQ(view_from_empty.size(), 0);
  EXPECT_EQ(view_from_empty.view(), absl::string_view());
  EXPECT_TRUE(view_from_empty.IsInlined());
}

TEST(QuicInlinedStringViewTest, RangeTest) {
  constexpr size_t kBufSize = 32;
  for (size_t size = 1; size < 1024; ++size) {
    std::string example(size, 'a');
    QuicInlinedStringView<kBufSize> view(example);
    EXPECT_NE(view.data(), nullptr);
    EXPECT_EQ(view.size(), size);
    EXPECT_EQ(view.view(), example);
    EXPECT_EQ(view.IsInlined(), size < kBufSize);
  }
}

// Test 16 bytes specifically, since on 64-bit platforms, that is where the size
// byte overlaps with the inlined marker.
TEST(QuicInlinedStringViewTest, RangeTest16) {
  constexpr size_t kBufSize = 16;
  for (size_t size = 1; size < 1024; ++size) {
    std::string example(size, 'a');
    QuicInlinedStringView<kBufSize> view(example);
    EXPECT_NE(view.data(), nullptr);
    EXPECT_EQ(view.size(), size);
    EXPECT_EQ(view.view(), example);
    EXPECT_EQ(view.IsInlined(), size < kBufSize);
  }
}

TEST(QuicInlinedStringViewTest, CopyConstructor) {
  QuicInlinedStringView<24> view_inlined("aaa");
  ASSERT_TRUE(view_inlined.IsInlined());
  QuicInlinedStringView<24> view_inlined_copy = view_inlined;
  EXPECT_EQ(view_inlined.view(), view_inlined_copy.view());
  EXPECT_NE(reinterpret_cast<uintptr_t>(view_inlined.data()),
            reinterpret_cast<uintptr_t>(view_inlined_copy.data()));

  QuicInlinedStringView<24> view_external("aaaaaaaaaaaaaaaaaaaaaaaaa");
  ASSERT_FALSE(view_external.IsInlined());
  QuicInlinedStringView<24> view_external_copy = view_external;
  EXPECT_EQ(view_external.view(), view_external_copy.view());
  EXPECT_EQ(reinterpret_cast<uintptr_t>(view_external.data()),
            reinterpret_cast<uintptr_t>(view_external_copy.data()));
}

TEST(QuicInlinedStringViewTest, IsEmptyAfterClear) {
  QuicInlinedStringView<24> view("foo");
  view.clear();
  ASSERT_TRUE(view.empty());
  ASSERT_EQ(view.size(), 0);
  ASSERT_TRUE(view.IsInlined());
}

TEST(QuicInlinedStringViewTest,
     NonEmptyStringHasDifferentDataPointerWhenInlined) {
  absl::string_view view = "foo";
  QuicInlinedStringView<24> quic_view(view);
  EXPECT_TRUE(quic_view.IsInlined());
  EXPECT_NE(view.data(), quic_view.data());
}

TEST(QuicInlinedStringViewTest,
     NonEmptyStringHasSameDataPointerWhenNotInlined) {
  std::string big_string(300, 'a');
  QuicInlinedStringView<24> quic_view(big_string);
  EXPECT_FALSE(quic_view.IsInlined());
  EXPECT_EQ(quic_view.data(), big_string.data());
}

}  // namespace
}  // namespace quic
