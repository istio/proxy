// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_subscribe_windows.h"

#include <cstdint>
#include <optional>

#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/common/platform/api/quiche_export.h"

namespace moqt {

namespace test {

class QUICHE_EXPORT SubscribeWindowTest : public quic::test::QuicTest {
 public:
  SubscribeWindowTest() {}

  const uint64_t subscribe_id_ = 2;
  const Location start_{4, 0};
  const uint64_t end_ = 5;
};

TEST_F(SubscribeWindowTest, Queries) {
  SubscribeWindow window(start_, end_);
  EXPECT_TRUE(window.InWindow(Location(4, 0)));
  EXPECT_TRUE(window.InWindow(Location(5, UINT64_MAX)));
  EXPECT_FALSE(window.InWindow(Location(6, 0)));
  EXPECT_FALSE(window.InWindow(Location(3, 12)));
}

TEST_F(SubscribeWindowTest, AddQueryRemoveStreamIdSubgroup) {
  SendStreamMap stream_map;
  stream_map.AddStream(DataStreamIndex{4, 0}, 2);
  EXPECT_EQ(stream_map.GetStreamFor(DataStreamIndex(5, 0)), std::nullopt);
  stream_map.AddStream(DataStreamIndex{5, 0}, 6);
  stream_map.AddStream(DataStreamIndex{5, 1}, 7);
  EXPECT_QUIC_BUG(stream_map.AddStream(DataStreamIndex{5, 0}, 6),
                  "Stream already added");
  EXPECT_EQ(stream_map.GetStreamFor(DataStreamIndex(4, 0)), 2);
  stream_map.RemoveStream(DataStreamIndex{5, 1});
  EXPECT_EQ(stream_map.GetStreamFor(DataStreamIndex(5, 1)), std::nullopt);
}

TEST_F(SubscribeWindowTest, UpdateStartEnd) {
  SubscribeWindow window(start_, end_);
  EXPECT_TRUE(window.TruncateStart(start_.Next()));
  EXPECT_TRUE(window.TruncateEnd(end_ - 1));
  EXPECT_FALSE(window.InWindow(start_));
  EXPECT_FALSE(window.InWindow(Location(end_, 0)));
  // Widens start_ again.
  EXPECT_FALSE(window.TruncateStart(start_));
  // Widens end_ again.
  EXPECT_FALSE(window.TruncateEnd(end_));
  EXPECT_TRUE(window.TruncateEnd(Location(end_ - 1, 10)));
  EXPECT_TRUE(window.InWindow(Location(end_ - 1, 10)));
  EXPECT_FALSE(window.InWindow(Location(end_ - 1, 11)));
}

}  // namespace test

}  // namespace moqt
