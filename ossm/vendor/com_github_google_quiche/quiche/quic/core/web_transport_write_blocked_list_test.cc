// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/web_transport_write_blocked_list.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <vector>

#include "absl/algorithm/container.h"
#include "quiche/quic/core/quic_stream_priority.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quic::test {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

class WebTransportWriteBlockedListTest : public ::quiche::test::QuicheTest {
 protected:
  void RegisterStaticStream(QuicStreamId id) {
    list_.RegisterStream(id, /*is_static_stream=*/true, QuicStreamPriority());
  }
  void RegisterHttpStream(QuicStreamId id,
                          int urgency = HttpStreamPriority::kDefaultUrgency) {
    HttpStreamPriority priority;
    priority.urgency = urgency;
    list_.RegisterStream(id, /*is_static_stream=*/false,
                         QuicStreamPriority(priority));
  }
  void RegisterWebTransportDataStream(QuicStreamId id,
                                      WebTransportStreamPriority priority) {
    list_.RegisterStream(id, /*is_static_stream=*/false,
                         QuicStreamPriority(priority));
  }

  std::vector<QuicStreamId> PopAll() {
    std::vector<QuicStreamId> result;
    size_t expected_count = list_.NumBlockedStreams();
    while (list_.NumBlockedStreams() > 0) {
      EXPECT_TRUE(list_.HasWriteBlockedDataStreams() ||
                  list_.HasWriteBlockedSpecialStream());
      result.push_back(list_.PopFront());
      EXPECT_EQ(list_.NumBlockedStreams(), --expected_count);
    }
    return result;
  }

  WebTransportWriteBlockedList list_;
};

TEST_F(WebTransportWriteBlockedListTest, BasicHttpStreams) {
  RegisterHttpStream(1);
  RegisterHttpStream(2);
  RegisterHttpStream(3, HttpStreamPriority::kDefaultUrgency + 1);
  RegisterStaticStream(4);

  EXPECT_EQ(list_.GetPriorityOfStream(1), QuicStreamPriority());
  EXPECT_EQ(list_.GetPriorityOfStream(2), QuicStreamPriority());
  EXPECT_EQ(list_.GetPriorityOfStream(3).http().urgency, 4);

  EXPECT_EQ(list_.NumBlockedStreams(), 0);
  EXPECT_EQ(list_.NumBlockedSpecialStreams(), 0);
  list_.AddStream(1);
  list_.AddStream(2);
  list_.AddStream(3);
  list_.AddStream(4);
  EXPECT_EQ(list_.NumBlockedStreams(), 4);
  EXPECT_EQ(list_.NumBlockedSpecialStreams(), 1);

  EXPECT_THAT(PopAll(), ElementsAre(4, 3, 1, 2));
  EXPECT_EQ(list_.NumBlockedStreams(), 0);
  EXPECT_EQ(list_.NumBlockedSpecialStreams(), 0);

  list_.AddStream(2);
  list_.AddStream(3);
  list_.AddStream(4);
  list_.AddStream(1);
  EXPECT_THAT(PopAll(), ElementsAre(4, 3, 2, 1));
}

TEST_F(WebTransportWriteBlockedListTest, RegisterDuplicateStream) {
  RegisterHttpStream(1);
  EXPECT_QUICHE_BUG(RegisterHttpStream(1), "already registered");
}

TEST_F(WebTransportWriteBlockedListTest, UnregisterMissingStream) {
  EXPECT_QUICHE_BUG(list_.UnregisterStream(1), "not found");
}

TEST_F(WebTransportWriteBlockedListTest, GetPriorityMissingStream) {
  EXPECT_QUICHE_BUG(list_.GetPriorityOfStream(1), "not found");
}

TEST_F(WebTransportWriteBlockedListTest, PopFrontMissing) {
  RegisterHttpStream(1);
  list_.AddStream(1);
  EXPECT_EQ(list_.PopFront(), 1);
  EXPECT_QUICHE_BUG(list_.PopFront(), "no streams scheduled");
}

TEST_F(WebTransportWriteBlockedListTest, HasWriteBlockedDataStreams) {
  RegisterStaticStream(1);
  RegisterHttpStream(2);

  EXPECT_FALSE(list_.HasWriteBlockedDataStreams());
  list_.AddStream(1);
  EXPECT_FALSE(list_.HasWriteBlockedDataStreams());
  list_.AddStream(2);
  EXPECT_TRUE(list_.HasWriteBlockedDataStreams());
  EXPECT_EQ(list_.PopFront(), 1);
  EXPECT_TRUE(list_.HasWriteBlockedDataStreams());
  EXPECT_EQ(list_.PopFront(), 2);
  EXPECT_FALSE(list_.HasWriteBlockedDataStreams());
}

TEST_F(WebTransportWriteBlockedListTest, NestedStreams) {
  RegisterHttpStream(1);
  RegisterHttpStream(2);
  RegisterWebTransportDataStream(3, WebTransportStreamPriority{1, 0, 0});
  RegisterWebTransportDataStream(4, WebTransportStreamPriority{1, 0, 0});
  RegisterWebTransportDataStream(5, WebTransportStreamPriority{2, 0, 0});
  RegisterWebTransportDataStream(6, WebTransportStreamPriority{2, 0, 0});
  EXPECT_EQ(list_.NumBlockedStreams(), 0);

  list_.AddStream(3);
  list_.AddStream(5);
  list_.AddStream(4);
  list_.AddStream(6);
  EXPECT_EQ(list_.NumBlockedStreams(), 4);
  EXPECT_THAT(PopAll(), ElementsAre(3, 5, 4, 6));
  EXPECT_EQ(list_.NumBlockedStreams(), 0);

  list_.AddStream(3);
  list_.AddStream(4);
  list_.AddStream(5);
  EXPECT_EQ(list_.NumBlockedStreams(), 3);
  EXPECT_THAT(PopAll(), ElementsAre(3, 5, 4));
  EXPECT_EQ(list_.NumBlockedStreams(), 0);

  list_.AddStream(4);
  list_.AddStream(5);
  list_.AddStream(6);
  EXPECT_EQ(list_.NumBlockedStreams(), 3);
  EXPECT_THAT(PopAll(), ElementsAre(4, 5, 6));
  EXPECT_EQ(list_.NumBlockedStreams(), 0);

  list_.AddStream(6);
  list_.AddStream(3);
  list_.AddStream(4);
  list_.AddStream(5);
  EXPECT_EQ(list_.NumBlockedStreams(), 4);
  EXPECT_THAT(PopAll(), ElementsAre(6, 3, 5, 4));
  EXPECT_EQ(list_.NumBlockedStreams(), 0);

  list_.AddStream(6);
  list_.AddStream(5);
  list_.AddStream(4);
  list_.AddStream(3);
  EXPECT_EQ(list_.NumBlockedStreams(), 4);
  EXPECT_THAT(PopAll(), ElementsAre(6, 4, 5, 3));
  EXPECT_EQ(list_.NumBlockedStreams(), 0);
}

TEST_F(WebTransportWriteBlockedListTest, NestedStreamsWithHigherPriorityGroup) {
  RegisterHttpStream(1, HttpStreamPriority::kDefaultUrgency + 1);
  RegisterHttpStream(2);
  RegisterWebTransportDataStream(3, WebTransportStreamPriority{1, 0, 0});
  RegisterWebTransportDataStream(4, WebTransportStreamPriority{1, 0, 0});
  RegisterWebTransportDataStream(5, WebTransportStreamPriority{2, 0, 0});
  RegisterWebTransportDataStream(6, WebTransportStreamPriority{2, 0, 0});
  EXPECT_EQ(list_.NumBlockedStreams(), 0);

  list_.AddStream(3);
  list_.AddStream(5);
  list_.AddStream(4);
  list_.AddStream(6);
  EXPECT_EQ(list_.NumBlockedStreams(), 4);
  EXPECT_THAT(PopAll(), ElementsAre(3, 4, 5, 6));
  EXPECT_EQ(list_.NumBlockedStreams(), 0);

  list_.AddStream(3);
  list_.AddStream(4);
  list_.AddStream(5);
  EXPECT_EQ(list_.NumBlockedStreams(), 3);
  EXPECT_THAT(PopAll(), ElementsAre(3, 4, 5));
  EXPECT_EQ(list_.NumBlockedStreams(), 0);

  list_.AddStream(4);
  list_.AddStream(5);
  list_.AddStream(6);
  EXPECT_EQ(list_.NumBlockedStreams(), 3);
  EXPECT_THAT(PopAll(), ElementsAre(4, 5, 6));
  EXPECT_EQ(list_.NumBlockedStreams(), 0);

  list_.AddStream(6);
  list_.AddStream(3);
  list_.AddStream(4);
  list_.AddStream(5);
  EXPECT_EQ(list_.NumBlockedStreams(), 4);
  EXPECT_THAT(PopAll(), ElementsAre(3, 4, 6, 5));
  EXPECT_EQ(list_.NumBlockedStreams(), 0);

  list_.AddStream(6);
  list_.AddStream(5);
  list_.AddStream(4);
  list_.AddStream(3);
  EXPECT_EQ(list_.NumBlockedStreams(), 4);
  EXPECT_THAT(PopAll(), ElementsAre(4, 3, 6, 5));
  EXPECT_EQ(list_.NumBlockedStreams(), 0);
}

TEST_F(WebTransportWriteBlockedListTest, NestedStreamVsControlStream) {
  RegisterHttpStream(1);
  RegisterWebTransportDataStream(2, WebTransportStreamPriority{1, 0, 0});

  list_.AddStream(2);
  list_.AddStream(1);
  EXPECT_THAT(PopAll(), ElementsAre(1, 2));

  list_.AddStream(1);
  list_.AddStream(2);
  EXPECT_THAT(PopAll(), ElementsAre(1, 2));
}

TEST_F(WebTransportWriteBlockedListTest, NestedStreamsSendOrder) {
  RegisterHttpStream(1);
  RegisterWebTransportDataStream(2, WebTransportStreamPriority{1, 0, 0});
  RegisterWebTransportDataStream(3, WebTransportStreamPriority{1, 0, 100});
  RegisterWebTransportDataStream(4, WebTransportStreamPriority{1, 0, -100});

  list_.AddStream(4);
  list_.AddStream(3);
  list_.AddStream(2);
  list_.AddStream(1);
  EXPECT_THAT(PopAll(), ElementsAre(1, 3, 2, 4));
}

TEST_F(WebTransportWriteBlockedListTest, NestedStreamsDifferentGroups) {
  RegisterHttpStream(1);
  RegisterWebTransportDataStream(2, WebTransportStreamPriority{1, 0, 0});
  RegisterWebTransportDataStream(3, WebTransportStreamPriority{1, 1, 100});
  RegisterWebTransportDataStream(4, WebTransportStreamPriority{1, 7, -100});

  list_.AddStream(4);
  list_.AddStream(3);
  list_.AddStream(2);
  list_.AddStream(1);
  EXPECT_THAT(PopAll(), ElementsAre(1, 4, 3, 2));

  list_.AddStream(1);
  list_.AddStream(2);
  list_.AddStream(3);
  list_.AddStream(4);
  EXPECT_THAT(PopAll(), ElementsAre(1, 2, 3, 4));
}

TEST_F(WebTransportWriteBlockedListTest, NestedStreamsDifferentSession) {
  RegisterWebTransportDataStream(1, WebTransportStreamPriority{10, 0, 0});
  RegisterWebTransportDataStream(2, WebTransportStreamPriority{11, 0, 100});
  RegisterWebTransportDataStream(3, WebTransportStreamPriority{12, 0, -100});

  list_.AddStream(3);
  list_.AddStream(2);
  list_.AddStream(1);
  EXPECT_THAT(PopAll(), ElementsAre(3, 2, 1));

  list_.AddStream(1);
  list_.AddStream(2);
  list_.AddStream(3);
  EXPECT_THAT(PopAll(), ElementsAre(1, 2, 3));
}

TEST_F(WebTransportWriteBlockedListTest, UnregisterScheduledStreams) {
  RegisterHttpStream(1);
  RegisterHttpStream(2);
  RegisterWebTransportDataStream(3, WebTransportStreamPriority{1, 0, 0});
  RegisterWebTransportDataStream(4, WebTransportStreamPriority{1, 0, 0});
  RegisterWebTransportDataStream(5, WebTransportStreamPriority{2, 0, 0});
  RegisterWebTransportDataStream(6, WebTransportStreamPriority{2, 0, 0});

  EXPECT_EQ(list_.NumBlockedStreams(), 0);
  for (QuicStreamId id : {1, 2, 3, 4, 5, 6}) {
    list_.AddStream(id);
  }
  EXPECT_EQ(list_.NumBlockedStreams(), 6);

  list_.UnregisterStream(1);
  EXPECT_EQ(list_.NumBlockedStreams(), 5);
  list_.UnregisterStream(3);
  EXPECT_EQ(list_.NumBlockedStreams(), 4);
  list_.UnregisterStream(4);
  EXPECT_EQ(list_.NumBlockedStreams(), 3);
  list_.UnregisterStream(5);
  EXPECT_EQ(list_.NumBlockedStreams(), 2);
  list_.UnregisterStream(6);
  EXPECT_EQ(list_.NumBlockedStreams(), 1);
  list_.UnregisterStream(2);
  EXPECT_EQ(list_.NumBlockedStreams(), 0);
}

TEST_F(WebTransportWriteBlockedListTest, UnregisterUnscheduledStreams) {
  RegisterHttpStream(1);
  RegisterHttpStream(2);
  RegisterWebTransportDataStream(3, WebTransportStreamPriority{1, 0, 0});
  RegisterWebTransportDataStream(4, WebTransportStreamPriority{1, 0, 0});
  RegisterWebTransportDataStream(5, WebTransportStreamPriority{2, 0, 0});
  RegisterWebTransportDataStream(6, WebTransportStreamPriority{2, 0, 0});

  EXPECT_EQ(list_.NumRegisteredHttpStreams(), 2);
  EXPECT_EQ(list_.NumRegisteredGroups(), 2);
  list_.UnregisterStream(1);
  EXPECT_EQ(list_.NumRegisteredHttpStreams(), 1);
  EXPECT_EQ(list_.NumRegisteredGroups(), 2);
  list_.UnregisterStream(3);
  EXPECT_EQ(list_.NumRegisteredHttpStreams(), 1);
  EXPECT_EQ(list_.NumRegisteredGroups(), 2);
  list_.UnregisterStream(4);
  EXPECT_EQ(list_.NumRegisteredHttpStreams(), 1);
  EXPECT_EQ(list_.NumRegisteredGroups(), 1);

  list_.UnregisterStream(5);
  EXPECT_EQ(list_.NumRegisteredHttpStreams(), 1);
  EXPECT_EQ(list_.NumRegisteredGroups(), 1);
  list_.UnregisterStream(6);
  EXPECT_EQ(list_.NumRegisteredHttpStreams(), 1);
  EXPECT_EQ(list_.NumRegisteredGroups(), 0);
  list_.UnregisterStream(2);
  EXPECT_EQ(list_.NumRegisteredHttpStreams(), 0);
  EXPECT_EQ(list_.NumRegisteredGroups(), 0);

  RegisterHttpStream(1);
  RegisterHttpStream(2);
  RegisterWebTransportDataStream(3, WebTransportStreamPriority{1, 0, 0});
  RegisterWebTransportDataStream(4, WebTransportStreamPriority{1, 0, 0});
  RegisterWebTransportDataStream(5, WebTransportStreamPriority{2, 0, 0});
  RegisterWebTransportDataStream(6, WebTransportStreamPriority{2, 0, 0});
}

TEST_F(WebTransportWriteBlockedListTest, IsStreamBlocked) {
  RegisterHttpStream(1);
  RegisterWebTransportDataStream(2, WebTransportStreamPriority{1, 0, 0});
  RegisterWebTransportDataStream(3, WebTransportStreamPriority{9, 0, 0});

  EXPECT_FALSE(list_.IsStreamBlocked(1));
  EXPECT_FALSE(list_.IsStreamBlocked(2));
  EXPECT_FALSE(list_.IsStreamBlocked(3));

  list_.AddStream(3);
  EXPECT_FALSE(list_.IsStreamBlocked(1));
  EXPECT_FALSE(list_.IsStreamBlocked(2));
  EXPECT_TRUE(list_.IsStreamBlocked(3));

  list_.AddStream(1);
  EXPECT_TRUE(list_.IsStreamBlocked(1));
  EXPECT_FALSE(list_.IsStreamBlocked(2));
  EXPECT_TRUE(list_.IsStreamBlocked(3));

  ASSERT_EQ(list_.PopFront(), 1);
  EXPECT_FALSE(list_.IsStreamBlocked(1));
  EXPECT_FALSE(list_.IsStreamBlocked(2));
  EXPECT_TRUE(list_.IsStreamBlocked(3));
}

TEST_F(WebTransportWriteBlockedListTest, UpdatePriorityHttp) {
  RegisterHttpStream(1);
  RegisterHttpStream(2);
  RegisterHttpStream(3);

  list_.AddStream(1);
  list_.AddStream(2);
  list_.AddStream(3);
  EXPECT_THAT(PopAll(), ElementsAre(1, 2, 3));

  list_.UpdateStreamPriority(
      2, QuicStreamPriority(
             HttpStreamPriority{HttpStreamPriority::kMaximumUrgency, false}));

  list_.AddStream(1);
  list_.AddStream(2);
  list_.AddStream(3);
  EXPECT_THAT(PopAll(), ElementsAre(2, 1, 3));
}

TEST_F(WebTransportWriteBlockedListTest, UpdatePriorityWebTransport) {
  RegisterWebTransportDataStream(1, WebTransportStreamPriority{0, 0, 0});
  RegisterWebTransportDataStream(2, WebTransportStreamPriority{0, 0, 0});
  RegisterWebTransportDataStream(3, WebTransportStreamPriority{0, 0, 0});

  list_.AddStream(1);
  list_.AddStream(2);
  list_.AddStream(3);
  EXPECT_THAT(PopAll(), ElementsAre(1, 2, 3));

  list_.UpdateStreamPriority(
      2, QuicStreamPriority(WebTransportStreamPriority{0, 0, 1}));

  list_.AddStream(1);
  list_.AddStream(2);
  list_.AddStream(3);
  EXPECT_THAT(PopAll(), ElementsAre(2, 1, 3));
}

TEST_F(WebTransportWriteBlockedListTest, UpdatePriorityControlStream) {
  RegisterHttpStream(1);
  RegisterHttpStream(2);
  RegisterWebTransportDataStream(3, WebTransportStreamPriority{1, 0, 0});
  RegisterWebTransportDataStream(4, WebTransportStreamPriority{2, 0, 0});

  list_.AddStream(3);
  list_.AddStream(4);
  EXPECT_THAT(PopAll(), ElementsAre(3, 4));
  list_.AddStream(4);
  list_.AddStream(3);
  EXPECT_THAT(PopAll(), ElementsAre(4, 3));

  list_.UpdateStreamPriority(
      2, QuicStreamPriority(
             HttpStreamPriority{HttpStreamPriority::kMaximumUrgency, false}));

  list_.AddStream(3);
  list_.AddStream(4);
  EXPECT_THAT(PopAll(), ElementsAre(4, 3));
  list_.AddStream(4);
  list_.AddStream(3);
  EXPECT_THAT(PopAll(), ElementsAre(4, 3));
}

TEST_F(WebTransportWriteBlockedListTest, ShouldYield) {
  RegisterHttpStream(1);
  RegisterWebTransportDataStream(2, WebTransportStreamPriority{1, 0, 0});
  RegisterWebTransportDataStream(3, WebTransportStreamPriority{1, 0, 0});
  RegisterWebTransportDataStream(4, WebTransportStreamPriority{1, 0, 10});

  EXPECT_FALSE(list_.ShouldYield(1));
  EXPECT_FALSE(list_.ShouldYield(2));
  EXPECT_FALSE(list_.ShouldYield(3));
  EXPECT_FALSE(list_.ShouldYield(4));

  list_.AddStream(1);
  EXPECT_FALSE(list_.ShouldYield(1));
  EXPECT_TRUE(list_.ShouldYield(2));
  EXPECT_TRUE(list_.ShouldYield(3));
  EXPECT_TRUE(list_.ShouldYield(4));
  PopAll();

  list_.AddStream(2);
  EXPECT_FALSE(list_.ShouldYield(1));
  EXPECT_FALSE(list_.ShouldYield(2));
  EXPECT_TRUE(list_.ShouldYield(3));
  EXPECT_FALSE(list_.ShouldYield(4));
  PopAll();

  list_.AddStream(4);
  EXPECT_FALSE(list_.ShouldYield(1));
  EXPECT_TRUE(list_.ShouldYield(2));
  EXPECT_TRUE(list_.ShouldYield(3));
  EXPECT_FALSE(list_.ShouldYield(4));
  PopAll();
}

TEST_F(WebTransportWriteBlockedListTest, RemoveOneStreamFromActiveGroup) {
  RegisterHttpStream(1);
  RegisterWebTransportDataStream(2, WebTransportStreamPriority{1, 0, 0});
  RegisterWebTransportDataStream(3, WebTransportStreamPriority{1, 0, 1});
  RegisterHttpStream(4, HttpStreamPriority::kDefaultUrgency - 1);
  RegisterWebTransportDataStream(5, WebTransportStreamPriority{4, 0, 0});

  list_.AddStream(3);
  list_.AddStream(4);

  list_.UnregisterStream(3);
  EXPECT_EQ(list_.PopFront(), 4);
  list_.UnregisterStream(2);
  list_.UnregisterStream(1);
}

TEST_F(WebTransportWriteBlockedListTest, RandomizedTest) {
  RegisterHttpStream(1);
  RegisterHttpStream(2, HttpStreamPriority::kMinimumUrgency);
  RegisterHttpStream(3, HttpStreamPriority::kMaximumUrgency);
  RegisterWebTransportDataStream(4, WebTransportStreamPriority{1, 0, 0});
  RegisterWebTransportDataStream(5, WebTransportStreamPriority{2, 0, +1});
  RegisterWebTransportDataStream(6, WebTransportStreamPriority{2, 0, -1});
  RegisterWebTransportDataStream(7, WebTransportStreamPriority{3, 8, 0});
  RegisterWebTransportDataStream(8, WebTransportStreamPriority{3, 8, 100});
  RegisterWebTransportDataStream(9, WebTransportStreamPriority{3, 8, 20000});
  RegisterHttpStream(10, HttpStreamPriority::kDefaultUrgency + 1);
  // The priorities of the streams above are arranged so that the priorities of
  // all streams above are strictly ordered (i.e. there are no streams that
  // would be round-robined).
  constexpr std::array<QuicStreamId, 10> order = {3, 9, 8, 7, 10,
                                                  1, 4, 2, 5, 6};

  SimpleRandom random;
  for (int i = 0; i < 1000; ++i) {
    // Shuffle the streams.
    std::vector<QuicStreamId> pushed_streams(order.begin(), order.end());
    for (int j = pushed_streams.size() - 1; j > 0; --j) {
      std::swap(pushed_streams[j],
                pushed_streams[random.RandUint64() % (j + 1)]);
    }

    size_t stream_count = 1 + random.RandUint64() % order.size();
    pushed_streams.resize(stream_count);

    for (QuicStreamId id : pushed_streams) {
      list_.AddStream(id);
    }

    std::vector<QuicStreamId> expected_streams;
    absl::c_copy_if(
        order, std::back_inserter(expected_streams), [&](QuicStreamId id) {
          return absl::c_find(pushed_streams, id) != pushed_streams.end();
        });
    ASSERT_THAT(PopAll(), ElementsAreArray(expected_streams));
  }
}

}  // namespace
}  // namespace quic::test
