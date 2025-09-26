// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/http2/core/priority_write_scheduler.h"

#include "quiche/http2/core/spdy_protocol.h"
#include "quiche/http2/test_tools/spdy_test_utils.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace http2 {
namespace test {

using ::spdy::SpdyPriority;
using ::spdy::SpdyStreamId;
using ::testing::Eq;
using ::testing::Optional;

template <typename StreamIdType>
class PriorityWriteSchedulerPeer {
 public:
  explicit PriorityWriteSchedulerPeer(
      PriorityWriteScheduler<StreamIdType>* scheduler)
      : scheduler_(scheduler) {}

  size_t NumReadyStreams(SpdyPriority priority) const {
    return scheduler_->priority_infos_[priority].ready_list.size();
  }

 private:
  PriorityWriteScheduler<StreamIdType>* scheduler_;
};

namespace {

class PriorityWriteSchedulerTest : public quiche::test::QuicheTest {
 public:
  static constexpr int kLowestPriority =
      PriorityWriteScheduler<SpdyStreamId>::kLowestPriority;

  PriorityWriteSchedulerTest() : peer_(&scheduler_) {}

  PriorityWriteScheduler<SpdyStreamId> scheduler_;
  PriorityWriteSchedulerPeer<SpdyStreamId> peer_;
};

TEST_F(PriorityWriteSchedulerTest, RegisterUnregisterStreams) {
  EXPECT_FALSE(scheduler_.HasReadyStreams());
  EXPECT_FALSE(scheduler_.StreamRegistered(1));
  EXPECT_EQ(0u, scheduler_.NumRegisteredStreams());
  scheduler_.RegisterStream(1, 1);
  EXPECT_TRUE(scheduler_.StreamRegistered(1));
  EXPECT_EQ(1u, scheduler_.NumRegisteredStreams());

  // Try redundant registrations.
  EXPECT_QUICHE_BUG(scheduler_.RegisterStream(1, 1),
                    "Stream 1 already registered");
  EXPECT_EQ(1u, scheduler_.NumRegisteredStreams());

  EXPECT_QUICHE_BUG(scheduler_.RegisterStream(1, 2),
                    "Stream 1 already registered");
  EXPECT_EQ(1u, scheduler_.NumRegisteredStreams());

  scheduler_.RegisterStream(2, 3);
  EXPECT_EQ(2u, scheduler_.NumRegisteredStreams());

  // Verify registration != ready.
  EXPECT_FALSE(scheduler_.HasReadyStreams());

  scheduler_.UnregisterStream(1);
  EXPECT_EQ(1u, scheduler_.NumRegisteredStreams());
  scheduler_.UnregisterStream(2);
  EXPECT_EQ(0u, scheduler_.NumRegisteredStreams());

  // Try redundant unregistration.
  EXPECT_QUICHE_BUG(scheduler_.UnregisterStream(1), "Stream 1 not registered");
  EXPECT_QUICHE_BUG(scheduler_.UnregisterStream(2), "Stream 2 not registered");
  EXPECT_EQ(0u, scheduler_.NumRegisteredStreams());
}

TEST_F(PriorityWriteSchedulerTest, GetStreamPriority) {
  // Unknown streams tolerated due to b/15676312. However, return lowest
  // priority.
  EXPECT_EQ(kLowestPriority, scheduler_.GetStreamPriority(1));

  scheduler_.RegisterStream(1, 3);
  EXPECT_EQ(3, scheduler_.GetStreamPriority(1));

  // Redundant registration shouldn't change stream priority.
  EXPECT_QUICHE_BUG(scheduler_.RegisterStream(1, 4),
                    "Stream 1 already registered");
  EXPECT_EQ(3, scheduler_.GetStreamPriority(1));

  scheduler_.UpdateStreamPriority(1, 5);
  EXPECT_EQ(5, scheduler_.GetStreamPriority(1));

  // Toggling ready state shouldn't change stream priority.
  scheduler_.MarkStreamReady(1, true);
  EXPECT_EQ(5, scheduler_.GetStreamPriority(1));

  // Test changing priority of ready stream.
  EXPECT_EQ(1u, peer_.NumReadyStreams(5));
  scheduler_.UpdateStreamPriority(1, 6);
  EXPECT_EQ(6, scheduler_.GetStreamPriority(1));
  EXPECT_EQ(0u, peer_.NumReadyStreams(5));
  EXPECT_EQ(1u, peer_.NumReadyStreams(6));

  EXPECT_EQ(1u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(6, scheduler_.GetStreamPriority(1));

  scheduler_.UnregisterStream(1);
  EXPECT_EQ(kLowestPriority, scheduler_.GetStreamPriority(1));
}

TEST_F(PriorityWriteSchedulerTest, PopNextReadyStreamAndPriority) {
  scheduler_.RegisterStream(1, 3);
  scheduler_.MarkStreamReady(1, true);
  EXPECT_EQ(std::make_tuple(1u, 3), scheduler_.PopNextReadyStreamAndPriority());
  scheduler_.UnregisterStream(1);
}

TEST_F(PriorityWriteSchedulerTest, UpdateStreamPriority) {
  // For the moment, updating stream priority on a non-registered stream should
  // have no effect. In the future, it will lazily cause the stream to be
  // registered (b/15676312).
  EXPECT_EQ(kLowestPriority, scheduler_.GetStreamPriority(3));
  EXPECT_FALSE(scheduler_.StreamRegistered(3));
  scheduler_.UpdateStreamPriority(3, 1);
  EXPECT_FALSE(scheduler_.StreamRegistered(3));
  EXPECT_EQ(kLowestPriority, scheduler_.GetStreamPriority(3));

  scheduler_.RegisterStream(3, 1);
  EXPECT_EQ(1, scheduler_.GetStreamPriority(3));
  scheduler_.UpdateStreamPriority(3, 2);
  EXPECT_EQ(2, scheduler_.GetStreamPriority(3));

  // Updating priority of stream to current priority value is valid, but has no
  // effect.
  scheduler_.UpdateStreamPriority(3, 2);
  EXPECT_EQ(2, scheduler_.GetStreamPriority(3));

  // Even though stream 4 is marked ready after stream 5, it should be returned
  // first by PopNextReadyStream() since it has higher priority.
  scheduler_.RegisterStream(4, 1);
  scheduler_.MarkStreamReady(3, false);  // priority 2
  EXPECT_TRUE(scheduler_.IsStreamReady(3));
  scheduler_.MarkStreamReady(4, false);  // priority 1
  EXPECT_TRUE(scheduler_.IsStreamReady(4));
  EXPECT_EQ(4u, scheduler_.PopNextReadyStream());
  EXPECT_FALSE(scheduler_.IsStreamReady(4));
  EXPECT_EQ(3u, scheduler_.PopNextReadyStream());
  EXPECT_FALSE(scheduler_.IsStreamReady(3));

  // Verify that lowering priority of stream 4 causes it to be returned later
  // by PopNextReadyStream().
  scheduler_.MarkStreamReady(3, false);  // priority 2
  scheduler_.MarkStreamReady(4, false);  // priority 1
  scheduler_.UpdateStreamPriority(4, 3);
  EXPECT_EQ(3u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(4u, scheduler_.PopNextReadyStream());

  scheduler_.UnregisterStream(3);
}

TEST_F(PriorityWriteSchedulerTest, MarkStreamReadyBack) {
  EXPECT_FALSE(scheduler_.HasReadyStreams());
  EXPECT_QUICHE_BUG(scheduler_.MarkStreamReady(1, false),
                    "Stream 1 not registered");
  EXPECT_FALSE(scheduler_.HasReadyStreams());
  EXPECT_QUICHE_BUG(EXPECT_EQ(0u, scheduler_.PopNextReadyStream()),
                    "No ready streams available");

  // Add a bunch of ready streams to tail of per-priority lists.
  // Expected order: (P2) 4, (P3) 1, 2, 3, (P5) 5.
  scheduler_.RegisterStream(1, 3);
  scheduler_.MarkStreamReady(1, false);
  EXPECT_TRUE(scheduler_.HasReadyStreams());
  scheduler_.RegisterStream(2, 3);
  scheduler_.MarkStreamReady(2, false);
  scheduler_.RegisterStream(3, 3);
  scheduler_.MarkStreamReady(3, false);
  scheduler_.RegisterStream(4, 2);
  scheduler_.MarkStreamReady(4, false);
  scheduler_.RegisterStream(5, 5);
  scheduler_.MarkStreamReady(5, false);

  EXPECT_EQ(4u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(1u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(2u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(3u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(5u, scheduler_.PopNextReadyStream());
  EXPECT_QUICHE_BUG(EXPECT_EQ(0u, scheduler_.PopNextReadyStream()),
                    "No ready streams available");
}

TEST_F(PriorityWriteSchedulerTest, MarkStreamReadyFront) {
  EXPECT_FALSE(scheduler_.HasReadyStreams());
  EXPECT_QUICHE_BUG(scheduler_.MarkStreamReady(1, true),
                    "Stream 1 not registered");
  EXPECT_FALSE(scheduler_.HasReadyStreams());
  EXPECT_QUICHE_BUG(EXPECT_EQ(0u, scheduler_.PopNextReadyStream()),
                    "No ready streams available");

  // Add a bunch of ready streams to head of per-priority lists.
  // Expected order: (P2) 4, (P3) 3, 2, 1, (P5) 5
  scheduler_.RegisterStream(1, 3);
  scheduler_.MarkStreamReady(1, true);
  EXPECT_TRUE(scheduler_.HasReadyStreams());
  scheduler_.RegisterStream(2, 3);
  scheduler_.MarkStreamReady(2, true);
  scheduler_.RegisterStream(3, 3);
  scheduler_.MarkStreamReady(3, true);
  scheduler_.RegisterStream(4, 2);
  scheduler_.MarkStreamReady(4, true);
  scheduler_.RegisterStream(5, 5);
  scheduler_.MarkStreamReady(5, true);

  EXPECT_EQ(4u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(3u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(2u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(1u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(5u, scheduler_.PopNextReadyStream());
  EXPECT_QUICHE_BUG(EXPECT_EQ(0u, scheduler_.PopNextReadyStream()),
                    "No ready streams available");
}

TEST_F(PriorityWriteSchedulerTest, MarkStreamReadyBackAndFront) {
  scheduler_.RegisterStream(1, 4);
  scheduler_.RegisterStream(2, 3);
  scheduler_.RegisterStream(3, 3);
  scheduler_.RegisterStream(4, 3);
  scheduler_.RegisterStream(5, 4);
  scheduler_.RegisterStream(6, 1);

  // Add a bunch of ready streams to per-priority lists, with variety of adding
  // at head and tail.
  // Expected order: (P1) 6, (P3) 4, 2, 3, (P4) 1, 5
  scheduler_.MarkStreamReady(1, true);
  scheduler_.MarkStreamReady(2, true);
  scheduler_.MarkStreamReady(3, false);
  scheduler_.MarkStreamReady(4, true);
  scheduler_.MarkStreamReady(5, false);
  scheduler_.MarkStreamReady(6, true);

  EXPECT_EQ(6u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(4u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(2u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(3u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(1u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(5u, scheduler_.PopNextReadyStream());
  EXPECT_QUICHE_BUG(EXPECT_EQ(0u, scheduler_.PopNextReadyStream()),
                    "No ready streams available");
}

TEST_F(PriorityWriteSchedulerTest, MarkStreamNotReady) {
  // Verify ready state reflected in NumReadyStreams().
  scheduler_.RegisterStream(1, 1);
  EXPECT_EQ(0u, scheduler_.NumReadyStreams());
  scheduler_.MarkStreamReady(1, false);
  EXPECT_EQ(1u, scheduler_.NumReadyStreams());
  scheduler_.MarkStreamNotReady(1);
  EXPECT_EQ(0u, scheduler_.NumReadyStreams());

  // Empty pop should fail.
  EXPECT_QUICHE_BUG(EXPECT_EQ(0u, scheduler_.PopNextReadyStream()),
                    "No ready streams available");

  // Tolerate redundant marking of a stream as not ready.
  scheduler_.MarkStreamNotReady(1);
  EXPECT_EQ(0u, scheduler_.NumReadyStreams());

  // Should only be able to mark registered streams.
  EXPECT_QUICHE_BUG(scheduler_.MarkStreamNotReady(3),
                    "Stream 3 not registered");
}

TEST_F(PriorityWriteSchedulerTest, UnregisterRemovesStream) {
  scheduler_.RegisterStream(3, 4);
  scheduler_.MarkStreamReady(3, false);
  EXPECT_EQ(1u, scheduler_.NumReadyStreams());

  // Unregistering a stream should remove it from set of ready streams.
  scheduler_.UnregisterStream(3);
  EXPECT_EQ(0u, scheduler_.NumReadyStreams());
  EXPECT_QUICHE_BUG(EXPECT_EQ(0u, scheduler_.PopNextReadyStream()),
                    "No ready streams available");
}

TEST_F(PriorityWriteSchedulerTest, ShouldYield) {
  scheduler_.RegisterStream(1, 1);
  scheduler_.RegisterStream(4, 4);
  scheduler_.RegisterStream(5, 4);
  scheduler_.RegisterStream(7, 7);

  // Make sure we don't yield when the list is empty.
  EXPECT_FALSE(scheduler_.ShouldYield(1));

  // Add a low priority stream.
  scheduler_.MarkStreamReady(4, false);
  // 4 should not yield to itself.
  EXPECT_FALSE(scheduler_.ShouldYield(4));
  // 7 should yield as 4 is blocked and a higher priority.
  EXPECT_TRUE(scheduler_.ShouldYield(7));
  // 5 should yield to 4 as they are the same priority.
  EXPECT_TRUE(scheduler_.ShouldYield(5));
  // 1 should not yield as 1 is higher priority.
  EXPECT_FALSE(scheduler_.ShouldYield(1));

  // Add a second stream in that priority class.
  scheduler_.MarkStreamReady(5, false);
  // 4 and 5 are both blocked, but 4 is at the front so should not yield.
  EXPECT_FALSE(scheduler_.ShouldYield(4));
  EXPECT_TRUE(scheduler_.ShouldYield(5));
}

TEST_F(PriorityWriteSchedulerTest, GetLatestEventWithPriority) {
  EXPECT_QUICHE_BUG(
      scheduler_.RecordStreamEventTime(3, absl::FromUnixMicros(5)),
      "Stream 3 not registered");
  EXPECT_QUICHE_BUG(
      EXPECT_FALSE(scheduler_.GetLatestEventWithPriority(4).has_value()),
      "Stream 4 not registered");

  for (int i = 1; i < 5; ++i) {
    scheduler_.RegisterStream(i, i);
  }
  for (int i = 1; i < 5; ++i) {
    EXPECT_FALSE(scheduler_.GetLatestEventWithPriority(i).has_value());
  }
  for (int i = 1; i < 5; ++i) {
    scheduler_.RecordStreamEventTime(i, absl::FromUnixMicros(i * 100));
  }
  EXPECT_FALSE(scheduler_.GetLatestEventWithPriority(1).has_value());
  for (int i = 2; i < 5; ++i) {
    EXPECT_THAT(scheduler_.GetLatestEventWithPriority(i),
                Optional(Eq(absl::FromUnixMicros((i - 1) * 100))));
  }
}

}  // namespace
}  // namespace test
}  // namespace http2
