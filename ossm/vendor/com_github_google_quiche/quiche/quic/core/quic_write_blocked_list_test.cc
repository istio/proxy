// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_write_blocked_list.h"

#include <optional>
#include <tuple>

#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"

using spdy::kV3HighestPriority;
using spdy::kV3LowestPriority;

namespace quic {
namespace test {
namespace {

constexpr bool kStatic = true;
constexpr bool kNotStatic = false;

constexpr bool kIncremental = true;
constexpr bool kNotIncremental = false;

class QuicWriteBlockedListTest : public QuicTest {
 protected:
  void SetUp() override {
    // Delay construction of QuicWriteBlockedList object to allow constructor of
    // derived test classes to manipulate reloadable flags that are latched in
    // QuicWriteBlockedList constructor.
    write_blocked_list_.emplace();
  }

  bool HasWriteBlockedDataStreams() const {
    return write_blocked_list_->HasWriteBlockedDataStreams();
  }

  bool HasWriteBlockedSpecialStream() const {
    return write_blocked_list_->HasWriteBlockedSpecialStream();
  }

  size_t NumBlockedSpecialStreams() const {
    return write_blocked_list_->NumBlockedSpecialStreams();
  }

  size_t NumBlockedStreams() const {
    return write_blocked_list_->NumBlockedStreams();
  }

  bool ShouldYield(QuicStreamId id) const {
    return write_blocked_list_->ShouldYield(id);
  }

  QuicStreamPriority GetPriorityOfStream(QuicStreamId id) const {
    return write_blocked_list_->GetPriorityOfStream(id);
  }

  QuicStreamId PopFront() { return write_blocked_list_->PopFront(); }

  void RegisterStream(QuicStreamId stream_id, bool is_static_stream,
                      const HttpStreamPriority& priority) {
    write_blocked_list_->RegisterStream(stream_id, is_static_stream,
                                        QuicStreamPriority(priority));
  }

  void UnregisterStream(QuicStreamId stream_id) {
    write_blocked_list_->UnregisterStream(stream_id);
  }

  void UpdateStreamPriority(QuicStreamId stream_id,
                            const HttpStreamPriority& new_priority) {
    write_blocked_list_->UpdateStreamPriority(stream_id,
                                              QuicStreamPriority(new_priority));
  }

  void UpdateBytesForStream(QuicStreamId stream_id, size_t bytes) {
    write_blocked_list_->UpdateBytesForStream(stream_id, bytes);
  }

  void AddStream(QuicStreamId stream_id) {
    write_blocked_list_->AddStream(stream_id);
  }

  bool IsStreamBlocked(QuicStreamId stream_id) const {
    return write_blocked_list_->IsStreamBlocked(stream_id);
  }

 private:
  std::optional<QuicWriteBlockedList> write_blocked_list_;
};

TEST_F(QuicWriteBlockedListTest, PriorityOrder) {
  // Mark streams blocked in roughly reverse priority order, and
  // verify that streams are sorted.
  RegisterStream(40, kNotStatic, {kV3LowestPriority, kNotIncremental});
  RegisterStream(23, kNotStatic, {kV3HighestPriority, kIncremental});
  RegisterStream(17, kNotStatic, {kV3HighestPriority, kNotIncremental});
  RegisterStream(1, kStatic, {kV3HighestPriority, kNotIncremental});
  RegisterStream(3, kStatic, {kV3HighestPriority, kNotIncremental});

  EXPECT_EQ(kV3LowestPriority, GetPriorityOfStream(40).http().urgency);
  EXPECT_EQ(kNotIncremental, GetPriorityOfStream(40).http().incremental);

  EXPECT_EQ(kV3HighestPriority, GetPriorityOfStream(23).http().urgency);
  EXPECT_EQ(kIncremental, GetPriorityOfStream(23).http().incremental);

  EXPECT_EQ(kV3HighestPriority, GetPriorityOfStream(17).http().urgency);
  EXPECT_EQ(kNotIncremental, GetPriorityOfStream(17).http().incremental);

  AddStream(40);
  EXPECT_TRUE(IsStreamBlocked(40));
  AddStream(23);
  EXPECT_TRUE(IsStreamBlocked(23));
  AddStream(17);
  EXPECT_TRUE(IsStreamBlocked(17));
  AddStream(3);
  EXPECT_TRUE(IsStreamBlocked(3));
  AddStream(1);
  EXPECT_TRUE(IsStreamBlocked(1));

  EXPECT_EQ(5u, NumBlockedStreams());
  EXPECT_TRUE(HasWriteBlockedSpecialStream());
  EXPECT_EQ(2u, NumBlockedSpecialStreams());
  EXPECT_TRUE(HasWriteBlockedDataStreams());

  // Static streams are highest priority, regardless of priority value.
  EXPECT_EQ(1u, PopFront());
  EXPECT_EQ(1u, NumBlockedSpecialStreams());
  EXPECT_FALSE(IsStreamBlocked(1));

  EXPECT_EQ(3u, PopFront());
  EXPECT_EQ(0u, NumBlockedSpecialStreams());
  EXPECT_FALSE(IsStreamBlocked(3));

  // Streams with same priority are popped in the order they were inserted.
  EXPECT_EQ(23u, PopFront());
  EXPECT_FALSE(IsStreamBlocked(23));
  EXPECT_EQ(17u, PopFront());
  EXPECT_FALSE(IsStreamBlocked(17));

  // Low priority stream appears last.
  EXPECT_EQ(40u, PopFront());
  EXPECT_FALSE(IsStreamBlocked(40));

  EXPECT_EQ(0u, NumBlockedStreams());
  EXPECT_FALSE(HasWriteBlockedSpecialStream());
  EXPECT_FALSE(HasWriteBlockedDataStreams());
}

TEST_F(QuicWriteBlockedListTest, SingleStaticStream) {
  RegisterStream(5, kStatic, {kV3HighestPriority, kNotIncremental});
  AddStream(5);

  EXPECT_EQ(1u, NumBlockedStreams());
  EXPECT_TRUE(HasWriteBlockedSpecialStream());
  EXPECT_EQ(5u, PopFront());
  EXPECT_EQ(0u, NumBlockedStreams());
  EXPECT_FALSE(HasWriteBlockedSpecialStream());
}

TEST_F(QuicWriteBlockedListTest, StaticStreamsComeFirst) {
  RegisterStream(5, kNotStatic, {kV3HighestPriority, kNotIncremental});
  RegisterStream(3, kStatic, {kV3LowestPriority, kNotIncremental});
  AddStream(5);
  AddStream(3);

  EXPECT_EQ(2u, NumBlockedStreams());
  EXPECT_TRUE(HasWriteBlockedSpecialStream());
  EXPECT_TRUE(HasWriteBlockedDataStreams());

  EXPECT_EQ(3u, PopFront());
  EXPECT_EQ(5u, PopFront());

  EXPECT_EQ(0u, NumBlockedStreams());
  EXPECT_FALSE(HasWriteBlockedSpecialStream());
  EXPECT_FALSE(HasWriteBlockedDataStreams());
}

TEST_F(QuicWriteBlockedListTest, NoDuplicateEntries) {
  // Test that QuicWriteBlockedList doesn't allow duplicate entries.
  // Try to add a stream to the write blocked list multiple times at the same
  // priority.
  const QuicStreamId kBlockedId = 5;
  RegisterStream(kBlockedId, kNotStatic, {kV3HighestPriority, kNotIncremental});
  AddStream(kBlockedId);
  AddStream(kBlockedId);
  AddStream(kBlockedId);

  // This should only result in one blocked stream being added.
  EXPECT_EQ(1u, NumBlockedStreams());
  EXPECT_TRUE(HasWriteBlockedDataStreams());

  // There should only be one stream to pop off the front.
  EXPECT_EQ(kBlockedId, PopFront());
  EXPECT_EQ(0u, NumBlockedStreams());
  EXPECT_FALSE(HasWriteBlockedDataStreams());
}

TEST_F(QuicWriteBlockedListTest, IncrementalStreamsRoundRobin) {
  const QuicStreamId id1 = 5;
  const QuicStreamId id2 = 7;
  const QuicStreamId id3 = 9;
  RegisterStream(id1, kNotStatic, {kV3LowestPriority, kIncremental});
  RegisterStream(id2, kNotStatic, {kV3LowestPriority, kIncremental});
  RegisterStream(id3, kNotStatic, {kV3LowestPriority, kIncremental});

  AddStream(id1);
  AddStream(id2);
  AddStream(id3);

  EXPECT_EQ(id1, PopFront());
  const size_t kLargeWriteSize = 1000 * 1000 * 1000;
  UpdateBytesForStream(id1, kLargeWriteSize);
  AddStream(id1);

  EXPECT_EQ(id2, PopFront());
  UpdateBytesForStream(id2, kLargeWriteSize);
  EXPECT_EQ(id3, PopFront());
  UpdateBytesForStream(id3, kLargeWriteSize);

  AddStream(id3);
  AddStream(id2);

  EXPECT_EQ(id1, PopFront());
  UpdateBytesForStream(id1, kLargeWriteSize);
  EXPECT_EQ(id3, PopFront());
  UpdateBytesForStream(id3, kLargeWriteSize);
  AddStream(id3);

  EXPECT_EQ(id2, PopFront());
  EXPECT_EQ(id3, PopFront());
}

class QuicWriteBlockedListParameterizedTest
    : public QuicWriteBlockedListTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  QuicWriteBlockedListParameterizedTest()
      : priority_respect_incremental_(std::get<0>(GetParam())),
        disable_batch_write_(std::get<1>(GetParam())) {
    SetQuicReloadableFlag(quic_priority_respect_incremental,
                          priority_respect_incremental_);
    SetQuicReloadableFlag(quic_disable_batch_write, disable_batch_write_);
  }

  const bool priority_respect_incremental_;
  const bool disable_batch_write_;
};

INSTANTIATE_TEST_SUITE_P(
    BatchWrite, QuicWriteBlockedListParameterizedTest,
    ::testing::Combine(::testing::Bool(), ::testing::Bool()),
    [](const testing::TestParamInfo<
        QuicWriteBlockedListParameterizedTest::ParamType>& info) {
      return absl::StrCat(std::get<0>(info.param) ? "RespectIncrementalTrue"
                                                  : "RespectIncrementalFalse",
                          std::get<1>(info.param) ? "DisableBatchWriteTrue"
                                                  : "DisableBatchWriteFalse");
    });

// If reloadable_flag_quic_disable_batch_write is false, writes are batched.
TEST_P(QuicWriteBlockedListParameterizedTest, BatchingWrites) {
  if (disable_batch_write_) {
    return;
  }

  const QuicStreamId id1 = 5;
  const QuicStreamId id2 = 7;
  const QuicStreamId id3 = 9;
  RegisterStream(id1, kNotStatic, {kV3LowestPriority, kIncremental});
  RegisterStream(id2, kNotStatic, {kV3LowestPriority, kIncremental});
  RegisterStream(id3, kNotStatic, {kV3HighestPriority, kIncremental});

  AddStream(id1);
  AddStream(id2);
  EXPECT_EQ(2u, NumBlockedStreams());

  // The first stream we push back should stay at the front until 16k is
  // written.
  EXPECT_EQ(id1, PopFront());
  UpdateBytesForStream(id1, 15999);
  AddStream(id1);
  EXPECT_EQ(2u, NumBlockedStreams());
  EXPECT_EQ(id1, PopFront());

  // Once 16k is written the first stream will yield to the next.
  UpdateBytesForStream(id1, 1);
  AddStream(id1);
  EXPECT_EQ(2u, NumBlockedStreams());
  EXPECT_EQ(id2, PopFront());

  // Set the new stream to have written all but one byte.
  UpdateBytesForStream(id2, 15999);
  AddStream(id2);
  EXPECT_EQ(2u, NumBlockedStreams());

  // Ensure higher priority streams are popped first.
  AddStream(id3);
  EXPECT_EQ(id3, PopFront());

  // Higher priority streams will always be popped first, even if using their
  // byte quota
  UpdateBytesForStream(id3, 20000);
  AddStream(id3);
  EXPECT_EQ(id3, PopFront());

  // Once the higher priority stream is out of the way, id2 will resume its 16k
  // write, with only 1 byte remaining of its guaranteed write allocation.
  EXPECT_EQ(id2, PopFront());
  UpdateBytesForStream(id2, 1);
  AddStream(id2);
  EXPECT_EQ(2u, NumBlockedStreams());
  EXPECT_EQ(id1, PopFront());
}

// If reloadable_flag_quic_disable_batch_write is true, writes are performed
// round-robin regardless of how little data is written on each stream.
TEST_P(QuicWriteBlockedListParameterizedTest, RoundRobin) {
  if (!disable_batch_write_) {
    return;
  }

  const QuicStreamId id1 = 5;
  const QuicStreamId id2 = 7;
  const QuicStreamId id3 = 9;
  RegisterStream(id1, kNotStatic, {kV3LowestPriority, kIncremental});
  RegisterStream(id2, kNotStatic, {kV3LowestPriority, kIncremental});
  RegisterStream(id3, kNotStatic, {kV3LowestPriority, kIncremental});

  AddStream(id1);
  AddStream(id2);
  AddStream(id3);

  EXPECT_EQ(id1, PopFront());
  AddStream(id1);

  EXPECT_EQ(id2, PopFront());
  EXPECT_EQ(id3, PopFront());

  AddStream(id3);
  AddStream(id2);

  EXPECT_EQ(id1, PopFront());
  EXPECT_EQ(id3, PopFront());
  AddStream(id3);

  EXPECT_EQ(id2, PopFront());
  EXPECT_EQ(id3, PopFront());
}

TEST_P(QuicWriteBlockedListParameterizedTest,
       NonIncrementalStreamsKeepWriting) {
  if (!priority_respect_incremental_) {
    return;
  }

  const QuicStreamId id1 = 1;
  const QuicStreamId id2 = 2;
  const QuicStreamId id3 = 3;
  const QuicStreamId id4 = 4;
  RegisterStream(id1, kNotStatic, {kV3LowestPriority, kNotIncremental});
  RegisterStream(id2, kNotStatic, {kV3LowestPriority, kNotIncremental});
  RegisterStream(id3, kNotStatic, {kV3LowestPriority, kNotIncremental});
  RegisterStream(id4, kNotStatic, {kV3HighestPriority, kNotIncremental});

  AddStream(id1);
  AddStream(id2);
  AddStream(id3);

  // A non-incremental stream can continue writing as long as it has data.
  EXPECT_EQ(id1, PopFront());
  const size_t kLargeWriteSize = 1000 * 1000 * 1000;
  UpdateBytesForStream(id1, kLargeWriteSize);
  AddStream(id1);

  EXPECT_EQ(id1, PopFront());
  UpdateBytesForStream(id1, kLargeWriteSize);
  AddStream(id1);

  EXPECT_EQ(id1, PopFront());
  UpdateBytesForStream(id1, kLargeWriteSize);
  AddStream(id1);

  EXPECT_EQ(id1, PopFront());
  UpdateBytesForStream(id1, kLargeWriteSize);
  AddStream(id1);

  // A higher priority stream takes precedence.
  AddStream(id4);
  EXPECT_EQ(id4, PopFront());

  // When it is the turn of the lower urgency bucket again, writing of the first
  // stream will continue.
  EXPECT_EQ(id1, PopFront());
  UpdateBytesForStream(id1, kLargeWriteSize);

  // When there is no more data on the first stream, write can start on the
  // second stream.
  EXPECT_EQ(id2, PopFront());
  UpdateBytesForStream(id2, kLargeWriteSize);
  AddStream(id2);

  // Write continues without limit.
  EXPECT_EQ(id2, PopFront());
  UpdateBytesForStream(id2, kLargeWriteSize);
  AddStream(id2);

  // Stream 1 is not the most recently written one, therefore it gets to the end
  // of the dequeue.
  AddStream(id1);

  EXPECT_EQ(id2, PopFront());
  UpdateBytesForStream(id2, kLargeWriteSize);

  EXPECT_EQ(id3, PopFront());
  UpdateBytesForStream(id2, kLargeWriteSize);
  AddStream(id3);

  EXPECT_EQ(id3, PopFront());
  UpdateBytesForStream(id2, kLargeWriteSize);

  // When there is no data to write either on stream 2 or stream 3, stream 1 can
  // resume.
  EXPECT_EQ(id1, PopFront());
  UpdateBytesForStream(id1, kLargeWriteSize);
}

TEST_P(QuicWriteBlockedListParameterizedTest,
       IncrementalAndNonIncrementalStreams) {
  if (!priority_respect_incremental_) {
    return;
  }

  const QuicStreamId id1 = 1;
  const QuicStreamId id2 = 2;
  RegisterStream(id1, kNotStatic, {kV3LowestPriority, kNotIncremental});
  RegisterStream(id2, kNotStatic, {kV3LowestPriority, kIncremental});

  AddStream(id1);
  AddStream(id2);

  // A non-incremental stream can continue writing as long as it has data.
  EXPECT_EQ(id1, PopFront());
  const size_t kSmallWriteSize = 1000;
  UpdateBytesForStream(id1, kSmallWriteSize);
  AddStream(id1);

  EXPECT_EQ(id1, PopFront());
  UpdateBytesForStream(id1, kSmallWriteSize);
  AddStream(id1);

  EXPECT_EQ(id1, PopFront());
  UpdateBytesForStream(id1, kSmallWriteSize);

  EXPECT_EQ(id2, PopFront());
  UpdateBytesForStream(id2, kSmallWriteSize);
  AddStream(id2);
  AddStream(id1);

  if (!disable_batch_write_) {
    // Small writes do not exceed the batch limit.
    // Writes continue even on an incremental stream.
    EXPECT_EQ(id2, PopFront());
    UpdateBytesForStream(id2, kSmallWriteSize);
    AddStream(id2);

    EXPECT_EQ(id2, PopFront());
    UpdateBytesForStream(id2, kSmallWriteSize);
  }

  EXPECT_EQ(id1, PopFront());
  const size_t kLargeWriteSize = 1000 * 1000 * 1000;
  UpdateBytesForStream(id1, kLargeWriteSize);
  AddStream(id1);

  EXPECT_EQ(id1, PopFront());
  UpdateBytesForStream(id1, kLargeWriteSize);
  AddStream(id1);

  EXPECT_EQ(id1, PopFront());
  UpdateBytesForStream(id1, kLargeWriteSize);
  AddStream(id2);
  AddStream(id1);

  // When batch writing is disabled, stream 2 immediately yields to stream 1,
  // which is the non-incremental stream with most recent writes.
  // When batch writing is enabled, stream 2 only yields to stream 1 after
  // exceeding the batching limit.
  if (!disable_batch_write_) {
    EXPECT_EQ(id2, PopFront());
    UpdateBytesForStream(id2, kLargeWriteSize);
    AddStream(id2);
  }

  EXPECT_EQ(id1, PopFront());
  UpdateBytesForStream(id1, kLargeWriteSize);
}

TEST_F(QuicWriteBlockedListTest, Ceding) {
  RegisterStream(15, kNotStatic, {kV3HighestPriority, kNotIncremental});
  RegisterStream(16, kNotStatic, {kV3HighestPriority, kNotIncremental});
  RegisterStream(5, kNotStatic, {5, kNotIncremental});
  RegisterStream(4, kNotStatic, {5, kNotIncremental});
  RegisterStream(7, kNotStatic, {7, kNotIncremental});
  RegisterStream(1, kStatic, {kV3HighestPriority, kNotIncremental});
  RegisterStream(3, kStatic, {kV3HighestPriority, kNotIncremental});

  // When nothing is on the list, nothing yields.
  EXPECT_FALSE(ShouldYield(5));

  AddStream(5);
  // 5 should not yield to itself.
  EXPECT_FALSE(ShouldYield(5));
  // 4 and 7 are equal or lower priority and should yield to 5.
  EXPECT_TRUE(ShouldYield(4));
  EXPECT_TRUE(ShouldYield(7));
  // Stream 15 and static streams should preempt 5.
  EXPECT_FALSE(ShouldYield(15));
  EXPECT_FALSE(ShouldYield(3));
  EXPECT_FALSE(ShouldYield(1));

  // Block a high priority stream.
  AddStream(15);
  // 16 should yield (same priority) but static streams will still not.
  EXPECT_TRUE(ShouldYield(16));
  EXPECT_FALSE(ShouldYield(3));
  EXPECT_FALSE(ShouldYield(1));

  // Block a static stream.  All non-static streams should yield.
  AddStream(3);
  EXPECT_TRUE(ShouldYield(16));
  EXPECT_TRUE(ShouldYield(15));
  EXPECT_FALSE(ShouldYield(3));
  EXPECT_FALSE(ShouldYield(1));

  // Block the other static stream.  All other streams should yield.
  AddStream(1);
  EXPECT_TRUE(ShouldYield(16));
  EXPECT_TRUE(ShouldYield(15));
  EXPECT_TRUE(ShouldYield(3));
  EXPECT_FALSE(ShouldYield(1));
}

TEST_F(QuicWriteBlockedListTest, UnregisterStream) {
  RegisterStream(40, kNotStatic, {kV3LowestPriority, kNotIncremental});
  RegisterStream(23, kNotStatic, {6, kNotIncremental});
  RegisterStream(12, kNotStatic, {3, kNotIncremental});
  RegisterStream(17, kNotStatic, {kV3HighestPriority, kNotIncremental});
  RegisterStream(1, kStatic, {kV3HighestPriority, kNotIncremental});
  RegisterStream(3, kStatic, {kV3HighestPriority, kNotIncremental});

  AddStream(40);
  AddStream(23);
  AddStream(12);
  AddStream(17);
  AddStream(1);
  AddStream(3);

  UnregisterStream(23);
  UnregisterStream(1);

  EXPECT_EQ(3u, PopFront());
  EXPECT_EQ(17u, PopFront());
  EXPECT_EQ(12u, PopFront());
  EXPECT_EQ(40, PopFront());
}

TEST_F(QuicWriteBlockedListTest, UnregisterNotRegisteredStream) {
  EXPECT_QUICHE_BUG(UnregisterStream(1), "Stream 1 not registered");

  RegisterStream(2, kNotStatic, {kV3HighestPriority, kIncremental});
  UnregisterStream(2);
  EXPECT_QUICHE_BUG(UnregisterStream(2), "Stream 2 not registered");
}

TEST_F(QuicWriteBlockedListTest, UpdateStreamPriority) {
  RegisterStream(40, kNotStatic, {kV3LowestPriority, kNotIncremental});
  RegisterStream(23, kNotStatic, {6, kIncremental});
  RegisterStream(17, kNotStatic, {kV3HighestPriority, kNotIncremental});
  RegisterStream(1, kStatic, {2, kNotIncremental});
  RegisterStream(3, kStatic, {kV3HighestPriority, kNotIncremental});

  EXPECT_EQ(kV3LowestPriority, GetPriorityOfStream(40).http().urgency);
  EXPECT_EQ(kNotIncremental, GetPriorityOfStream(40).http().incremental);

  EXPECT_EQ(6, GetPriorityOfStream(23).http().urgency);
  EXPECT_EQ(kIncremental, GetPriorityOfStream(23).http().incremental);

  EXPECT_EQ(kV3HighestPriority, GetPriorityOfStream(17).http().urgency);
  EXPECT_EQ(kNotIncremental, GetPriorityOfStream(17).http().incremental);

  UpdateStreamPriority(40, {3, kIncremental});
  UpdateStreamPriority(23, {kV3HighestPriority, kNotIncremental});
  UpdateStreamPriority(17, {5, kNotIncremental});

  EXPECT_EQ(3, GetPriorityOfStream(40).http().urgency);
  EXPECT_EQ(kIncremental, GetPriorityOfStream(40).http().incremental);

  EXPECT_EQ(kV3HighestPriority, GetPriorityOfStream(23).http().urgency);
  EXPECT_EQ(kNotIncremental, GetPriorityOfStream(23).http().incremental);

  EXPECT_EQ(5, GetPriorityOfStream(17).http().urgency);
  EXPECT_EQ(kNotIncremental, GetPriorityOfStream(17).http().incremental);

  AddStream(40);
  AddStream(23);
  AddStream(17);
  AddStream(1);
  AddStream(3);

  EXPECT_EQ(1u, PopFront());
  EXPECT_EQ(3u, PopFront());
  EXPECT_EQ(23u, PopFront());
  EXPECT_EQ(40u, PopFront());
  EXPECT_EQ(17u, PopFront());
}

// UpdateStreamPriority() must not be called for static streams.
TEST_F(QuicWriteBlockedListTest, UpdateStaticStreamPriority) {
  RegisterStream(2, kStatic, {kV3LowestPriority, kNotIncremental});
  EXPECT_QUICHE_DEBUG_DEATH(
      UpdateStreamPriority(2, {kV3HighestPriority, kNotIncremental}),
      "IsRegistered");
}

TEST_F(QuicWriteBlockedListTest, UpdateStreamPrioritySameUrgency) {
  // Streams with same urgency are returned by PopFront() in the order they were
  // added by AddStream().
  RegisterStream(1, kNotStatic, {6, kNotIncremental});
  RegisterStream(2, kNotStatic, {6, kNotIncremental});

  AddStream(1);
  AddStream(2);

  EXPECT_EQ(1u, PopFront());
  EXPECT_EQ(2u, PopFront());

  // Calling UpdateStreamPriority() on the first stream does not change the
  // order.
  RegisterStream(3, kNotStatic, {6, kNotIncremental});
  RegisterStream(4, kNotStatic, {6, kNotIncremental});

  EXPECT_EQ(6, GetPriorityOfStream(3).http().urgency);
  EXPECT_EQ(kNotIncremental, GetPriorityOfStream(3).http().incremental);

  UpdateStreamPriority(3, {6, kIncremental});

  EXPECT_EQ(6, GetPriorityOfStream(3).http().urgency);
  EXPECT_EQ(kIncremental, GetPriorityOfStream(3).http().incremental);

  AddStream(3);
  AddStream(4);

  EXPECT_EQ(3u, PopFront());
  EXPECT_EQ(4u, PopFront());

  // Calling UpdateStreamPriority() on the second stream does not change the
  // order.
  RegisterStream(5, kNotStatic, {6, kIncremental});
  RegisterStream(6, kNotStatic, {6, kIncremental});

  EXPECT_EQ(6, GetPriorityOfStream(6).http().urgency);
  EXPECT_EQ(kIncremental, GetPriorityOfStream(6).http().incremental);

  UpdateStreamPriority(6, {6, kNotIncremental});

  EXPECT_EQ(6, GetPriorityOfStream(6).http().urgency);
  EXPECT_EQ(kNotIncremental, GetPriorityOfStream(6).http().incremental);

  AddStream(5);
  AddStream(6);

  EXPECT_EQ(5u, PopFront());
  EXPECT_EQ(6u, PopFront());
}

}  // namespace
}  // namespace test
}  // namespace quic
