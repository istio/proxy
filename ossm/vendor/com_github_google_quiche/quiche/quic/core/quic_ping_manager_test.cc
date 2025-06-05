// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_ping_manager.h"

#include "quiche/quic/core/quic_connection_alarms.h"
#include "quiche/quic/core/quic_one_block_arena.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/mock_quic_connection_alarms.h"
#include "quiche/quic/test_tools/quic_connection_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

namespace quic {
namespace test {

class QuicPingManagerPeer {
 public:
  static void SetPerspective(QuicPingManager* manager,
                             Perspective perspective) {
    manager->perspective_ = perspective;
  }
};

namespace {

const bool kShouldKeepAlive = true;
const bool kHasInflightPackets = true;

class MockDelegate : public QuicPingManager::Delegate {
 public:
  MOCK_METHOD(void, OnKeepAliveTimeout, (), (override));
  MOCK_METHOD(void, OnRetransmittableOnWireTimeout, (), (override));
};

class QuicPingManagerTest : public QuicTest {
 public:
  QuicPingManagerTest()
      : alarms_(&connection_alarms_delegate_, arena_, alarm_factory_),
        alarm_(&alarms_, QuicAlarmSlot::kPing),
        manager_(Perspective::IS_CLIENT, &delegate_, alarm_) {
    clock_.AdvanceTime(QuicTime::Delta::FromSeconds(1));
    ON_CALL(connection_alarms_delegate_, OnPingAlarm()).WillByDefault([&] {
      manager_.OnAlarm();
    });
  }

 protected:
  testing::StrictMock<MockDelegate> delegate_;
  MockConnectionAlarmsDelegate connection_alarms_delegate_;
  MockClock clock_;
  QuicConnectionArena arena_;
  MockAlarmFactory alarm_factory_;
  QuicAlarmMultiplexer alarms_;
  QuicTestAlarmProxy alarm_;
  QuicPingManager manager_;
};

TEST_F(QuicPingManagerTest, KeepAliveTimeout) {
  EXPECT_FALSE(alarm_->IsSet());

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  // Set alarm with in flight packets.
  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    kHasInflightPackets);
  EXPECT_TRUE(alarm_->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kPingTimeoutSecs),
            alarm_->deadline() - clock_.ApproximateNow());

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  // Reset alarm with no in flight packets.
  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    !kHasInflightPackets);
  EXPECT_TRUE(alarm_->IsSet());
  // Verify the deadline is set slightly less than 15 seconds in the future,
  // because of the 1s alarm granularity.
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kPingTimeoutSecs) -
                QuicTime::Delta::FromMilliseconds(5),
            alarm_->deadline() - clock_.ApproximateNow());

  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(kPingTimeoutSecs));
  EXPECT_CALL(delegate_, OnKeepAliveTimeout());
  alarm_->Fire();
  EXPECT_FALSE(alarm_->IsSet());
  // Reset alarm with in flight packets.
  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    kHasInflightPackets);
  EXPECT_TRUE(alarm_->IsSet());

  // Verify alarm is not armed if !kShouldKeepAlive.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  manager_.SetAlarm(clock_.ApproximateNow(), !kShouldKeepAlive,
                    kHasInflightPackets);
  EXPECT_FALSE(alarm_->IsSet());
}

TEST_F(QuicPingManagerTest, CustomizedKeepAliveTimeout) {
  EXPECT_FALSE(alarm_->IsSet());

  // Set customized keep-alive timeout.
  manager_.set_keep_alive_timeout(QuicTime::Delta::FromSeconds(10));

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  // Set alarm with in flight packets.
  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    kHasInflightPackets);
  EXPECT_TRUE(alarm_->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(10),
            alarm_->deadline() - clock_.ApproximateNow());

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  // Set alarm with no in flight packets.
  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    !kHasInflightPackets);
  EXPECT_TRUE(alarm_->IsSet());
  // The deadline is set slightly less than 10 seconds in the future, because
  // of the 1s alarm granularity.
  EXPECT_EQ(
      QuicTime::Delta::FromSeconds(10) - QuicTime::Delta::FromMilliseconds(5),
      alarm_->deadline() - clock_.ApproximateNow());

  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(10));
  EXPECT_CALL(delegate_, OnKeepAliveTimeout());
  alarm_->Fire();
  EXPECT_FALSE(alarm_->IsSet());
  // Reset alarm with in flight packets.
  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    kHasInflightPackets);
  EXPECT_TRUE(alarm_->IsSet());

  // Verify alarm is not armed if !kShouldKeepAlive.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  manager_.SetAlarm(clock_.ApproximateNow(), !kShouldKeepAlive,
                    kHasInflightPackets);
  EXPECT_FALSE(alarm_->IsSet());
}

TEST_F(QuicPingManagerTest, RetransmittableOnWireTimeout) {
  const QuicTime::Delta kRtransmittableOnWireTimeout =
      QuicTime::Delta::FromMilliseconds(50);
  manager_.set_initial_retransmittable_on_wire_timeout(
      kRtransmittableOnWireTimeout);

  EXPECT_FALSE(alarm_->IsSet());

  // Set alarm with in flight packets.
  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    kHasInflightPackets);
  // Verify alarm is in keep-alive mode.
  EXPECT_TRUE(alarm_->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kPingTimeoutSecs),
            alarm_->deadline() - clock_.ApproximateNow());

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  // Set alarm with no in flight packets.
  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    !kHasInflightPackets);
  EXPECT_TRUE(alarm_->IsSet());
  // Verify alarm is in retransmittable-on-wire mode.
  EXPECT_EQ(kRtransmittableOnWireTimeout,
            alarm_->deadline() - clock_.ApproximateNow());

  clock_.AdvanceTime(kRtransmittableOnWireTimeout);
  EXPECT_CALL(delegate_, OnRetransmittableOnWireTimeout());
  alarm_->Fire();
  EXPECT_FALSE(alarm_->IsSet());
  // Reset alarm with in flight packets.
  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    kHasInflightPackets);
  // Verify the alarm is in keep-alive mode.
  ASSERT_TRUE(alarm_->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kPingTimeoutSecs),
            alarm_->deadline() - clock_.ApproximateNow());
}

TEST_F(QuicPingManagerTest, RetransmittableOnWireTimeoutExponentiallyBackOff) {
  const int kMaxAggressiveRetransmittableOnWireCount = 5;
  SetQuicFlag(quic_max_aggressive_retransmittable_on_wire_ping_count,
              kMaxAggressiveRetransmittableOnWireCount);
  const QuicTime::Delta initial_retransmittable_on_wire_timeout =
      QuicTime::Delta::FromMilliseconds(200);
  manager_.set_initial_retransmittable_on_wire_timeout(
      initial_retransmittable_on_wire_timeout);

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_FALSE(alarm_->IsSet());
  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    kHasInflightPackets);
  // Verify alarm is in keep-alive mode.
  EXPECT_TRUE(alarm_->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kPingTimeoutSecs),
            alarm_->deadline() - clock_.ApproximateNow());

  // Verify no exponential backoff on the first few retransmittable on wire
  // timeouts.
  for (int i = 0; i <= kMaxAggressiveRetransmittableOnWireCount; ++i) {
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
    // Reset alarm with no in flight packets.
    manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                      !kHasInflightPackets);
    EXPECT_TRUE(alarm_->IsSet());
    // Verify alarm is in retransmittable-on-wire mode.
    EXPECT_EQ(initial_retransmittable_on_wire_timeout,
              alarm_->deadline() - clock_.ApproximateNow());
    clock_.AdvanceTime(initial_retransmittable_on_wire_timeout);
    EXPECT_CALL(delegate_, OnRetransmittableOnWireTimeout());
    alarm_->Fire();
    EXPECT_FALSE(alarm_->IsSet());
    // Reset alarm with in flight packets.
    manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                      kHasInflightPackets);
  }

  QuicTime::Delta retransmittable_on_wire_timeout =
      initial_retransmittable_on_wire_timeout;

  // Verify subsequent retransmittable-on-wire timeout is exponentially backed
  // off.
  while (retransmittable_on_wire_timeout * 2 <
         QuicTime::Delta::FromSeconds(kPingTimeoutSecs)) {
    retransmittable_on_wire_timeout = retransmittable_on_wire_timeout * 2;
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
    manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                      !kHasInflightPackets);
    EXPECT_TRUE(alarm_->IsSet());
    EXPECT_EQ(retransmittable_on_wire_timeout,
              alarm_->deadline() - clock_.ApproximateNow());

    clock_.AdvanceTime(retransmittable_on_wire_timeout);
    EXPECT_CALL(delegate_, OnRetransmittableOnWireTimeout());
    alarm_->Fire();
    EXPECT_FALSE(alarm_->IsSet());
    // Reset alarm with in flight packets.
    manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                      kHasInflightPackets);
  }

  // Verify alarm is in keep-alive mode.
  EXPECT_TRUE(alarm_->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kPingTimeoutSecs),
            alarm_->deadline() - clock_.ApproximateNow());

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  // Reset alarm with no in flight packets
  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    !kHasInflightPackets);
  EXPECT_TRUE(alarm_->IsSet());
  // Verify alarm is in keep-alive mode because retransmittable-on-wire deadline
  // is later.
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kPingTimeoutSecs) -
                QuicTime::Delta::FromMilliseconds(5),
            alarm_->deadline() - clock_.ApproximateNow());
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(kPingTimeoutSecs) -
                     QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(delegate_, OnKeepAliveTimeout());
  alarm_->Fire();
  EXPECT_FALSE(alarm_->IsSet());
}

TEST_F(QuicPingManagerTest,
       ResetRetransmitableOnWireTimeoutExponentiallyBackOff) {
  const int kMaxAggressiveRetransmittableOnWireCount = 3;
  SetQuicFlag(quic_max_aggressive_retransmittable_on_wire_ping_count,
              kMaxAggressiveRetransmittableOnWireCount);
  const QuicTime::Delta initial_retransmittable_on_wire_timeout =
      QuicTime::Delta::FromMilliseconds(200);
  manager_.set_initial_retransmittable_on_wire_timeout(
      initial_retransmittable_on_wire_timeout);

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_FALSE(alarm_->IsSet());
  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    kHasInflightPackets);
  // Verify alarm is in keep-alive mode.
  EXPECT_TRUE(alarm_->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kPingTimeoutSecs),
            alarm_->deadline() - clock_.ApproximateNow());

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    !kHasInflightPackets);
  EXPECT_TRUE(alarm_->IsSet());
  // Verify alarm is in retransmittable-on-wire mode.
  EXPECT_EQ(initial_retransmittable_on_wire_timeout,
            alarm_->deadline() - clock_.ApproximateNow());

  EXPECT_CALL(delegate_, OnRetransmittableOnWireTimeout());
  clock_.AdvanceTime(initial_retransmittable_on_wire_timeout);
  alarm_->Fire();

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    !kHasInflightPackets);
  EXPECT_TRUE(alarm_->IsSet());
  EXPECT_EQ(initial_retransmittable_on_wire_timeout,
            alarm_->deadline() - clock_.ApproximateNow());

  manager_.reset_consecutive_retransmittable_on_wire_count();
  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    !kHasInflightPackets);
  EXPECT_EQ(initial_retransmittable_on_wire_timeout,
            alarm_->deadline() - clock_.ApproximateNow());
  EXPECT_CALL(delegate_, OnRetransmittableOnWireTimeout());
  clock_.AdvanceTime(initial_retransmittable_on_wire_timeout);
  alarm_->Fire();

  for (int i = 0; i < kMaxAggressiveRetransmittableOnWireCount; i++) {
    manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                      !kHasInflightPackets);
    EXPECT_TRUE(alarm_->IsSet());
    EXPECT_EQ(initial_retransmittable_on_wire_timeout,
              alarm_->deadline() - clock_.ApproximateNow());
    clock_.AdvanceTime(initial_retransmittable_on_wire_timeout);
    EXPECT_CALL(delegate_, OnRetransmittableOnWireTimeout());
    alarm_->Fire();
    // Reset alarm with in flight packets.
    manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                      kHasInflightPackets);
    // Advance 5ms to receive next packet.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  }

  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    !kHasInflightPackets);
  EXPECT_TRUE(alarm_->IsSet());
  EXPECT_EQ(initial_retransmittable_on_wire_timeout * 2,
            alarm_->deadline() - clock_.ApproximateNow());

  clock_.AdvanceTime(2 * initial_retransmittable_on_wire_timeout);
  EXPECT_CALL(delegate_, OnRetransmittableOnWireTimeout());
  alarm_->Fire();

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  manager_.reset_consecutive_retransmittable_on_wire_count();
  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    !kHasInflightPackets);
  EXPECT_TRUE(alarm_->IsSet());
  EXPECT_EQ(initial_retransmittable_on_wire_timeout,
            alarm_->deadline() - clock_.ApproximateNow());
}

TEST_F(QuicPingManagerTest, RetransmittableOnWireLimit) {
  static constexpr int kMaxRetransmittableOnWirePingCount = 3;
  SetQuicFlag(quic_max_retransmittable_on_wire_ping_count,
              kMaxRetransmittableOnWirePingCount);
  static constexpr QuicTime::Delta initial_retransmittable_on_wire_timeout =
      QuicTime::Delta::FromMilliseconds(200);
  static constexpr QuicTime::Delta kShortDelay =
      QuicTime::Delta::FromMilliseconds(5);
  ASSERT_LT(kShortDelay * 10, initial_retransmittable_on_wire_timeout);
  manager_.set_initial_retransmittable_on_wire_timeout(
      initial_retransmittable_on_wire_timeout);

  clock_.AdvanceTime(kShortDelay);
  EXPECT_FALSE(alarm_->IsSet());
  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    kHasInflightPackets);

  EXPECT_TRUE(alarm_->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kPingTimeoutSecs),
            alarm_->deadline() - clock_.ApproximateNow());

  for (int i = 0; i <= kMaxRetransmittableOnWirePingCount; i++) {
    clock_.AdvanceTime(kShortDelay);
    manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                      !kHasInflightPackets);
    EXPECT_TRUE(alarm_->IsSet());
    EXPECT_EQ(initial_retransmittable_on_wire_timeout,
              alarm_->deadline() - clock_.ApproximateNow());
    clock_.AdvanceTime(initial_retransmittable_on_wire_timeout);
    EXPECT_CALL(delegate_, OnRetransmittableOnWireTimeout());
    alarm_->Fire();
    manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                      kHasInflightPackets);
  }

  manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                    !kHasInflightPackets);
  EXPECT_TRUE(alarm_->IsSet());
  // Verify alarm is in keep-alive mode.
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kPingTimeoutSecs),
            alarm_->deadline() - clock_.ApproximateNow());
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(kPingTimeoutSecs));
  EXPECT_CALL(delegate_, OnKeepAliveTimeout());
  alarm_->Fire();
  EXPECT_FALSE(alarm_->IsSet());
}

TEST_F(QuicPingManagerTest, MaxRetransmittableOnWireDelayShift) {
  QuicPingManagerPeer::SetPerspective(&manager_, Perspective::IS_SERVER);
  const int kMaxAggressiveRetransmittableOnWireCount = 3;
  SetQuicFlag(quic_max_aggressive_retransmittable_on_wire_ping_count,
              kMaxAggressiveRetransmittableOnWireCount);
  const QuicTime::Delta initial_retransmittable_on_wire_timeout =
      QuicTime::Delta::FromMilliseconds(200);
  manager_.set_initial_retransmittable_on_wire_timeout(
      initial_retransmittable_on_wire_timeout);

  for (int i = 0; i <= kMaxAggressiveRetransmittableOnWireCount; i++) {
    manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                      !kHasInflightPackets);
    EXPECT_TRUE(alarm_->IsSet());
    EXPECT_EQ(initial_retransmittable_on_wire_timeout,
              alarm_->deadline() - clock_.ApproximateNow());
    clock_.AdvanceTime(initial_retransmittable_on_wire_timeout);
    EXPECT_CALL(delegate_, OnRetransmittableOnWireTimeout());
    alarm_->Fire();
    manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                      kHasInflightPackets);
  }
  for (int i = 1; i <= 20; ++i) {
    manager_.SetAlarm(clock_.ApproximateNow(), kShouldKeepAlive,
                      !kHasInflightPackets);
    EXPECT_TRUE(alarm_->IsSet());
    if (i <= 10) {
      EXPECT_EQ(initial_retransmittable_on_wire_timeout * (1 << i),
                alarm_->deadline() - clock_.ApproximateNow());
    } else {
      // Verify shift is capped.
      EXPECT_EQ(initial_retransmittable_on_wire_timeout * (1 << 10),
                alarm_->deadline() - clock_.ApproximateNow());
    }
    clock_.AdvanceTime(alarm_->deadline() - clock_.ApproximateNow());
    EXPECT_CALL(delegate_, OnRetransmittableOnWireTimeout());
    alarm_->Fire();
  }
}

}  // namespace

}  // namespace test
}  // namespace quic
