// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_connection_alarms.h"

#include <string>

#include "quiche/quic/core/quic_one_block_arena.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/test_tools/mock_clock.h"
#include "quiche/quic/test_tools/mock_quic_connection_alarms.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quic::test {

class QuicAlarmMultiplexerPeer {
 public:
  static MockAlarmFactory::TestAlarm* GetNowAlarm(
      QuicAlarmMultiplexer& multiplexer) {
    return static_cast<MockAlarmFactory::TestAlarm*>(
        multiplexer.now_alarm_.get());
  }
  static MockAlarmFactory::TestAlarm* GetLaterAlarm(
      QuicAlarmMultiplexer& multiplexer) {
    return static_cast<MockAlarmFactory::TestAlarm*>(
        multiplexer.later_alarm_.get());
  }
};

namespace {

using ::testing::HasSubstr;
using ::testing::Not;

class QuicAlarmMultiplexerTest : public quiche::test::QuicheTest {
 public:
  QuicAlarmMultiplexerTest()
      : clock_(delegate_.clock()),
        multiplexer_(&delegate_, arena_, alarm_factory_),
        now_alarm_(QuicAlarmMultiplexerPeer::GetNowAlarm(multiplexer_)),
        later_alarm_(QuicAlarmMultiplexerPeer::GetLaterAlarm(multiplexer_)) {
    clock_->AdvanceTime(QuicTimeDelta::FromSeconds(1234));
  }

 protected:
  MockConnectionAlarmsDelegate delegate_;
  MockClock* clock_;
  QuicConnectionArena arena_;
  MockAlarmFactory alarm_factory_;
  QuicAlarmMultiplexer multiplexer_;

  MockAlarmFactory::TestAlarm* now_alarm_;
  MockAlarmFactory::TestAlarm* later_alarm_;
};

TEST_F(QuicAlarmMultiplexerTest, SetUpdateCancel) {
  EXPECT_FALSE(multiplexer_.IsSet(QuicAlarmSlot::kSend));
  EXPECT_FALSE(multiplexer_.IsPermanentlyCancelled());
  EXPECT_EQ(multiplexer_.GetDeadline(QuicAlarmSlot::kSend), QuicTime::Zero());

  const QuicTime time1 = clock_->Now();
  const QuicTime time2 = time1 + QuicTimeDelta::FromMilliseconds(10);

  multiplexer_.Set(QuicAlarmSlot::kSend, time1);
  EXPECT_TRUE(multiplexer_.IsSet(QuicAlarmSlot::kSend));
  EXPECT_EQ(multiplexer_.GetDeadline(QuicAlarmSlot::kSend), time1);

  multiplexer_.Update(QuicAlarmSlot::kSend, time2, QuicTimeDelta::Zero());
  EXPECT_TRUE(multiplexer_.IsSet(QuicAlarmSlot::kSend));
  EXPECT_EQ(multiplexer_.GetDeadline(QuicAlarmSlot::kSend), time2);

  multiplexer_.Cancel(QuicAlarmSlot::kSend);
  EXPECT_FALSE(multiplexer_.IsSet(QuicAlarmSlot::kSend));
  EXPECT_FALSE(multiplexer_.IsPermanentlyCancelled());
  EXPECT_EQ(multiplexer_.GetDeadline(QuicAlarmSlot::kSend), QuicTime::Zero());

  // Test set-via-update.
  multiplexer_.Update(QuicAlarmSlot::kSend, time1, QuicTimeDelta::Zero());
  EXPECT_TRUE(multiplexer_.IsSet(QuicAlarmSlot::kSend));
  EXPECT_EQ(multiplexer_.GetDeadline(QuicAlarmSlot::kSend), time1);

  // Test granularity.
  multiplexer_.Update(QuicAlarmSlot::kSend, time2,
                      QuicTimeDelta::FromSeconds(1000));
  EXPECT_TRUE(multiplexer_.IsSet(QuicAlarmSlot::kSend));
  EXPECT_EQ(multiplexer_.GetDeadline(QuicAlarmSlot::kSend), time1);

  // Test cancel-via-update.
  multiplexer_.Update(QuicAlarmSlot::kSend, QuicTime::Zero(),
                      QuicTimeDelta::Zero());
  EXPECT_FALSE(multiplexer_.IsSet(QuicAlarmSlot::kSend));
}

TEST_F(QuicAlarmMultiplexerTest, PermanentlyCancel) {
  const QuicTime time = clock_->Now();

  multiplexer_.Set(QuicAlarmSlot::kSend, time);
  EXPECT_TRUE(multiplexer_.IsSet(QuicAlarmSlot::kSend));
  EXPECT_FALSE(multiplexer_.IsPermanentlyCancelled());
  EXPECT_TRUE(now_alarm_->IsSet());

  multiplexer_.CancelAllAlarms();
  EXPECT_FALSE(multiplexer_.IsSet(QuicAlarmSlot::kSend));
  EXPECT_TRUE(multiplexer_.IsPermanentlyCancelled());
  EXPECT_FALSE(now_alarm_->IsSet());
  EXPECT_TRUE(now_alarm_->IsPermanentlyCancelled());

  EXPECT_QUICHE_BUG(multiplexer_.Set(QuicAlarmSlot::kSend, time),
                    "permanently cancelled");
  EXPECT_QUICHE_BUG(
      multiplexer_.Update(QuicAlarmSlot::kSend, time, QuicTimeDelta::Zero()),
      "permanently cancelled");
}

TEST_F(QuicAlarmMultiplexerTest, SingleAlarmScheduledForNow) {
  multiplexer_.Set(QuicAlarmSlot::kMtuDiscovery, clock_->Now());
  EXPECT_EQ(now_alarm_->deadline(), clock_->Now());
  EXPECT_FALSE(later_alarm_->IsSet());
}

TEST_F(QuicAlarmMultiplexerTest, SingleAlarmScheduledForPast) {
  multiplexer_.Set(QuicAlarmSlot::kMtuDiscovery,
                   clock_->Now() - QuicTimeDelta::FromMilliseconds(100));
  EXPECT_EQ(now_alarm_->deadline(), clock_->Now());
  EXPECT_FALSE(later_alarm_->IsSet());
}

TEST_F(QuicAlarmMultiplexerTest, SingleAlarmScheduledForFuture) {
  multiplexer_.Set(QuicAlarmSlot::kMtuDiscovery,
                   clock_->Now() + QuicTimeDelta::FromMilliseconds(100));
  EXPECT_FALSE(now_alarm_->IsSet());
  EXPECT_EQ(later_alarm_->deadline(),
            clock_->Now() + QuicTimeDelta::FromMilliseconds(100));
}

TEST_F(QuicAlarmMultiplexerTest, MultipleAlarmsNowAndFuture) {
  multiplexer_.Set(QuicAlarmSlot::kMtuDiscovery, clock_->Now());
  multiplexer_.Set(QuicAlarmSlot::kAck,
                   clock_->Now() + QuicTimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(now_alarm_->IsSet());
  EXPECT_EQ(later_alarm_->deadline(),
            clock_->Now() + QuicTimeDelta::FromMilliseconds(100));
}

TEST_F(QuicAlarmMultiplexerTest, FireSingleAlarmNow) {
  multiplexer_.Set(QuicAlarmSlot::kPing, clock_->Now());
  ASSERT_TRUE(now_alarm_->IsSet());
  EXPECT_CALL(delegate_, OnPingAlarm());
  now_alarm_->Fire();
  EXPECT_FALSE(multiplexer_.IsSet(QuicAlarmSlot::kPing));
  EXPECT_FALSE(now_alarm_->IsSet());
}

TEST_F(QuicAlarmMultiplexerTest, FireSingleAlarmFuture) {
  const QuicTime start = clock_->Now();
  const QuicTime end = start + QuicTimeDelta::FromMilliseconds(100);
  multiplexer_.Set(QuicAlarmSlot::kPing, end);
  ASSERT_TRUE(later_alarm_->IsSet());

  // Ensure that even if we fire the platform alarm prematurely, this works
  // correctly.
  EXPECT_CALL(delegate_, OnPingAlarm()).Times(0);
  later_alarm_->Fire();
  EXPECT_TRUE(multiplexer_.IsSet(QuicAlarmSlot::kPing));
  EXPECT_TRUE(later_alarm_->IsSet());

  clock_->AdvanceTime(end - start);
  ASSERT_EQ(later_alarm_->deadline(), end);
  EXPECT_CALL(delegate_, OnPingAlarm()).Times(1);
  later_alarm_->Fire();
  EXPECT_FALSE(multiplexer_.IsSet(QuicAlarmSlot::kPing));
  EXPECT_FALSE(now_alarm_->IsSet());
  EXPECT_FALSE(later_alarm_->IsSet());
}

TEST_F(QuicAlarmMultiplexerTest, AlarmReschedulesItself) {
  multiplexer_.Set(QuicAlarmSlot::kPing, clock_->Now());
  ASSERT_TRUE(now_alarm_->IsSet());
  EXPECT_CALL(delegate_, OnPingAlarm()).Times(1).WillRepeatedly([&] {
    multiplexer_.Set(QuicAlarmSlot::kPing, clock_->Now());
  });
  now_alarm_->Fire();
  EXPECT_TRUE(multiplexer_.IsSet(QuicAlarmSlot::kPing));
}

TEST_F(QuicAlarmMultiplexerTest, FireMultipleAlarmsNow) {
  multiplexer_.Set(QuicAlarmSlot::kPing, clock_->Now());
  multiplexer_.Set(QuicAlarmSlot::kRetransmission, clock_->Now());
  ASSERT_TRUE(now_alarm_->IsSet());
  EXPECT_CALL(delegate_, OnPingAlarm());
  EXPECT_CALL(delegate_, OnRetransmissionAlarm());
  now_alarm_->Fire();
}

TEST_F(QuicAlarmMultiplexerTest, FireMultipleAlarmsLater) {
  QuicTimeDelta delay = QuicTimeDelta::FromMilliseconds(10);
  multiplexer_.Set(QuicAlarmSlot::kPing, clock_->Now() + delay);
  multiplexer_.Set(QuicAlarmSlot::kRetransmission, clock_->Now() + delay);
  ASSERT_TRUE(later_alarm_->IsSet());

  later_alarm_->Fire();
  ASSERT_TRUE(later_alarm_->IsSet());

  clock_->AdvanceTime(delay);
  EXPECT_CALL(delegate_, OnPingAlarm());
  EXPECT_CALL(delegate_, OnRetransmissionAlarm());
  later_alarm_->Fire();
}

TEST_F(QuicAlarmMultiplexerTest, FireMultipleAlarmsLaterDifferentDelays) {
  QuicTimeDelta delay = QuicTimeDelta::FromMilliseconds(10);
  multiplexer_.Set(QuicAlarmSlot::kPing, clock_->Now() + delay);
  multiplexer_.Set(QuicAlarmSlot::kRetransmission, clock_->Now() + 2 * delay);
  ASSERT_TRUE(later_alarm_->IsSet());

  EXPECT_CALL(delegate_, OnPingAlarm()).Times(0);
  EXPECT_CALL(delegate_, OnRetransmissionAlarm()).Times(0);
  later_alarm_->Fire();
  ASSERT_TRUE(later_alarm_->IsSet());

  clock_->AdvanceTime(delay);
  EXPECT_CALL(delegate_, OnPingAlarm()).Times(1);
  EXPECT_CALL(delegate_, OnRetransmissionAlarm()).Times(0);
  later_alarm_->Fire();
  ASSERT_TRUE(later_alarm_->IsSet());

  clock_->AdvanceTime(delay);
  EXPECT_CALL(delegate_, OnPingAlarm()).Times(0);
  EXPECT_CALL(delegate_, OnRetransmissionAlarm()).Times(1);
  later_alarm_->Fire();
  EXPECT_FALSE(later_alarm_->IsSet());
}

TEST_F(QuicAlarmMultiplexerTest, FireMultipleAlarmsLaterDifferentDelaysAtOnce) {
  QuicTimeDelta delay = QuicTimeDelta::FromMilliseconds(10);
  multiplexer_.Set(QuicAlarmSlot::kMtuDiscovery, clock_->Now() + delay);
  multiplexer_.Set(QuicAlarmSlot::kAck, clock_->Now() + 2 * delay);
  ASSERT_TRUE(later_alarm_->IsSet());

  clock_->AdvanceTime(2 * delay);
  testing::Sequence seq;
  EXPECT_CALL(delegate_, OnMtuDiscoveryAlarm()).InSequence(seq);
  EXPECT_CALL(delegate_, OnAckAlarm()).InSequence(seq);
  later_alarm_->Fire();
  EXPECT_FALSE(later_alarm_->IsSet());
}

TEST_F(QuicAlarmMultiplexerTest, DeferUpdates) {
  QuicTimeDelta delay = QuicTimeDelta::FromMilliseconds(10);
  multiplexer_.DeferUnderlyingAlarmScheduling();
  multiplexer_.Set(QuicAlarmSlot::kMtuDiscovery, clock_->Now());
  multiplexer_.Set(QuicAlarmSlot::kAck, clock_->Now() + delay);
  EXPECT_FALSE(now_alarm_->IsSet());
  EXPECT_FALSE(later_alarm_->IsSet());
  multiplexer_.ResumeUnderlyingAlarmScheduling();
  EXPECT_TRUE(now_alarm_->IsSet());
  EXPECT_TRUE(later_alarm_->IsSet());
}

TEST_F(QuicAlarmMultiplexerTest, DeferUpdatesAlreadySet) {
  QuicTime deadline1 = clock_->Now() + QuicTimeDelta::FromMilliseconds(50);
  QuicTime deadline2 = clock_->Now() + QuicTimeDelta::FromMilliseconds(10);
  multiplexer_.Set(QuicAlarmSlot::kAck, deadline1);
  EXPECT_EQ(later_alarm_->deadline(), deadline1);

  multiplexer_.DeferUnderlyingAlarmScheduling();
  multiplexer_.Set(QuicAlarmSlot::kSend, deadline2);
  EXPECT_EQ(later_alarm_->deadline(), deadline1);

  multiplexer_.ResumeUnderlyingAlarmScheduling();
  EXPECT_EQ(later_alarm_->deadline(), deadline2);
}

TEST_F(QuicAlarmMultiplexerTest, DebugString) {
  multiplexer_.Set(QuicAlarmSlot::kMtuDiscovery, clock_->Now());
  multiplexer_.Set(QuicAlarmSlot::kPing,
                   clock_->Now() + QuicTimeDelta::FromMilliseconds(123));
  std::string debug_view = multiplexer_.DebugString();
  EXPECT_THAT(debug_view, HasSubstr("MtuDiscovery"));
  EXPECT_THAT(debug_view, HasSubstr("Ping"));
  EXPECT_THAT(debug_view, Not(HasSubstr("Ack")));
}

}  // namespace
}  // namespace quic::test
