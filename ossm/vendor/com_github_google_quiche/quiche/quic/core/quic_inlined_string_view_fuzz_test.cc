// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <string>

#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_inlined_string_view.h"
#include "quiche/quic/core/quic_stream_send_buffer_inlining.h"
#include "quiche/common/platform/api/quiche_fuzztest.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quic {
namespace {

// Defines methods that test properties of `QuicInlinedStringView<kSize>`.
template <size_t kSize>
class Properties {
 public:
  static void AccessorsAreCorrect(absl::string_view view) {
    QuicInlinedStringView<kSize> quic_view(view);

    // Copying the memory that `quic_view` points to enables ASAN to catch
    // out-of-bounds accesses.
    std::string copy(quic_view.view());

    EXPECT_EQ(quic_view.empty(), view.empty());
    EXPECT_EQ(quic_view.size(), view.size());
    EXPECT_EQ(quic_view.view(), view);
    EXPECT_EQ(quic_view.IsInlined(), view.size() < kSize);
    EXPECT_EQ(quic_view.data() == nullptr, view.empty());

    QuicInlinedStringView<kSize> quic_view_copy(quic_view);
    EXPECT_EQ(quic_view_copy.empty(), view.empty());
    EXPECT_EQ(quic_view_copy.size(), view.size());
    EXPECT_EQ(quic_view_copy.view(), view);
    EXPECT_EQ(quic_view_copy.IsInlined(), view.size() < kSize);
    EXPECT_EQ(quic_view_copy.data() == nullptr, view.empty());
  }

  static void IsEmptyAfterClear(absl::string_view view) {
    QuicInlinedStringView<kSize> quic_view(view);
    quic_view.clear();
    EXPECT_TRUE(quic_view.empty());
    EXPECT_EQ(quic_view.size(), 0);
    EXPECT_TRUE(quic_view.IsInlined());
  }

  static void NonEmptyStringHasDifferentDataPointerWhenInlined(
      absl::string_view view) {
    QuicInlinedStringView<kSize> quic_view(view);
    const bool pointers_equal = view.data() == quic_view.data();
    EXPECT_EQ(!pointers_equal, quic_view.IsInlined());
  }
};

// Below, we fuzz each property with different values of `kSize`.
// - The value of 16 matches the `kSize` used by `BufferedSliceInlining`.
// - The value of 24 was chosen to demonstrate that these properties hold for at
//   least one other value.
// - The value of 254 was chosen because it's the largest value supported by
//   `QuicInlinedStringView`.
constexpr size_t kRealisticSize =
    decltype(BufferedSliceInlining::slice)::kBufferSize;
static_assert(kRealisticSize == 16);

void AccessorsAreCorrect16(absl::string_view view) {
  Properties<16>::AccessorsAreCorrect(view);
}
void AccessorsAreCorrect24(absl::string_view view) {
  Properties<24>::AccessorsAreCorrect(view);
}
void AccessorsAreCorrect254(absl::string_view view) {
  Properties<254>::AccessorsAreCorrect(view);
}
FUZZ_TEST(QuicInlinedStringViewFuzzTest, AccessorsAreCorrect16);
FUZZ_TEST(QuicInlinedStringViewFuzzTest, AccessorsAreCorrect24);
FUZZ_TEST(QuicInlinedStringViewFuzzTest, AccessorsAreCorrect254);

void IsEmptyAfterClear16(absl::string_view view) {
  Properties<16>::IsEmptyAfterClear(view);
}
void IsEmptyAfterClear24(absl::string_view view) {
  Properties<24>::IsEmptyAfterClear(view);
}
void IsEmptyAfterClear254(absl::string_view view) {
  Properties<254>::IsEmptyAfterClear(view);
}
FUZZ_TEST(QuicInlinedStringViewFuzzTest, IsEmptyAfterClear16);
FUZZ_TEST(QuicInlinedStringViewFuzzTest, IsEmptyAfterClear24);
FUZZ_TEST(QuicInlinedStringViewFuzzTest, IsEmptyAfterClear254);

void NonEmptyStringHasDifferentDataPointerWhenInlined16(
    absl::string_view view) {
  Properties<16>::NonEmptyStringHasDifferentDataPointerWhenInlined(view);
}
void NonEmptyStringHasDifferentDataPointerWhenInlined24(
    absl::string_view view) {
  Properties<24>::NonEmptyStringHasDifferentDataPointerWhenInlined(view);
}
void NonEmptyStringHasDifferentDataPointerWhenInlined254(
    absl::string_view view) {
  Properties<254>::NonEmptyStringHasDifferentDataPointerWhenInlined(view);
}
FUZZ_TEST(QuicInlinedStringViewFuzzTest,
          NonEmptyStringHasDifferentDataPointerWhenInlined16)
    .WithDomains(fuzztest::Arbitrary<std::string>().WithMinSize(1));
FUZZ_TEST(QuicInlinedStringViewFuzzTest,
          NonEmptyStringHasDifferentDataPointerWhenInlined24)
    .WithDomains(fuzztest::Arbitrary<std::string>().WithMinSize(1));
FUZZ_TEST(QuicInlinedStringViewFuzzTest,
          NonEmptyStringHasDifferentDataPointerWhenInlined254)
    .WithDomains(fuzztest::Arbitrary<std::string>().WithMinSize(1));

}  // namespace
}  // namespace quic
