// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/http/quic_header_list.h"

#include <string>

#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_test.h"

using ::testing::ElementsAre;
using ::testing::Pair;

namespace quic::test {

class QuicHeaderListTest : public QuicTest {};

// This test verifies that QuicHeaderList accumulates header pairs in order.
TEST_F(QuicHeaderListTest, OnHeader) {
  QuicHeaderList headers;
  headers.OnHeader("foo", "bar");
  headers.OnHeader("april", "fools");
  headers.OnHeader("beep", "");

  EXPECT_THAT(headers, ElementsAre(Pair("foo", "bar"), Pair("april", "fools"),
                                   Pair("beep", "")));
}

TEST_F(QuicHeaderListTest, DebugString) {
  QuicHeaderList headers;
  headers.OnHeader("foo", "bar");
  headers.OnHeader("april", "fools");
  headers.OnHeader("beep", "");

  EXPECT_EQ("{ foo=bar, april=fools, beep=, }", headers.DebugString());
}

// This test verifies that QuicHeaderList is copyable and assignable.
TEST_F(QuicHeaderListTest, IsCopyableAndAssignable) {
  QuicHeaderList headers;
  headers.OnHeader("foo", "bar");
  headers.OnHeader("april", "fools");
  headers.OnHeader("beep", "");

  QuicHeaderList headers2(headers);
  QuicHeaderList headers3 = headers;

  EXPECT_THAT(headers2, ElementsAre(Pair("foo", "bar"), Pair("april", "fools"),
                                    Pair("beep", "")));
  EXPECT_THAT(headers3, ElementsAre(Pair("foo", "bar"), Pair("april", "fools"),
                                    Pair("beep", "")));
}

}  // namespace quic::test
