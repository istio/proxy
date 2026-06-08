// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_types.h"

#include "absl/strings/str_cat.h"
#include "quiche/quic/platform/api/quic_test.h"

namespace quic {
namespace {

TEST(PacketHeaderFormatTest, Stringify) {
  EXPECT_EQ(absl::StrCat(IETF_QUIC_LONG_HEADER_PACKET),
            "IETF_QUIC_LONG_HEADER_PACKET");
  EXPECT_EQ(absl::StrCat(IETF_QUIC_SHORT_HEADER_PACKET),
            "IETF_QUIC_SHORT_HEADER_PACKET");
  EXPECT_EQ(absl::StrCat(GOOGLE_QUIC_Q043_PACKET), "GOOGLE_QUIC_Q043_PACKET");
  EXPECT_EQ(absl::StrCat(static_cast<PacketHeaderFormat>(0xff)),
            "Unknown (255)");
}

}  // namespace
}  // namespace quic
