// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_idle_network_detector.h"

#include "quiche/quic/core/quic_connection_alarms.h"
#include "quiche/quic/core/quic_one_block_arena.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/mock_quic_connection_alarms.h"
#include "quiche/quic/test_tools/quic_connection_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

namespace quic {
namespace test {

class QuicIdleNetworkDetectorTestPeer {
 public:
  static QuicAlarmProxy GetAlarm(QuicIdleNetworkDetector* detector) {
    return detector->alarm_;
  }
};

namespace {

class MockDelegate : public QuicIdleNetworkDetector::Delegate {
 public:
  MOCK_METHOD(void, OnHandshakeTimeout, (), (override));
  MOCK_METHOD(void, OnIdleNetworkDetected, (), (override));
  MOCK_METHOD(void, OnMemoryReductionTimeout, (), (override));
};

class QuicIdleNetworkDetectorTest : public QuicTest {
 public:
  QuicIdleNetworkDetectorTest()
      : alarms_(&connection_alarms_delegate_, arena_, alarm_factory_),
        alarm_(&alarms_, QuicAlarmSlot::kIdleNetworkDetector),
        detector_(&delegate_, clock_.Now() + QuicTimeDelta::FromSeconds(1),
                  alarm_) {
    clock_.AdvanceTime(QuicTime::Delta::FromSeconds(1));
    ON_CALL(connection_alarms_delegate_, OnIdleDetectorAlarm())
        .WillByDefault([&] { detector_.OnAlarm(); });
  }

 protected:
  testing::StrictMock<MockDelegate> delegate_;
  MockConnectionAlarmsDelegate connection_alarms_delegate_;
  QuicConnectionArena arena_;
  MockAlarmFactory alarm_factory_;
  QuicAlarmMultiplexer alarms_;
  QuicTestAlarmProxy alarm_;
  MockClock clock_;
  QuicIdleNetworkDetector detector_;
};

TEST_F(QuicIdleNetworkDetectorTest,
       IdleNetworkDetectedBeforeHandshakeCompletes) {
  EXPECT_FALSE(alarm_->IsSet());
  detector_.SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::FromSeconds(30),
      /*idle_network_timeout=*/QuicTime::Delta::FromSeconds(20));
  EXPECT_TRUE(alarm_->IsSet());
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(20),
            alarm_->deadline());

  // No network activity for 20s.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(20));
  EXPECT_CALL(delegate_, OnIdleNetworkDetected());
  alarm_->Fire();
}

TEST_F(QuicIdleNetworkDetectorTest, HandshakeTimeout) {
  EXPECT_FALSE(alarm_->IsSet());
  detector_.SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::FromSeconds(30),
      /*idle_network_timeout=*/QuicTime::Delta::FromSeconds(20));
  EXPECT_TRUE(alarm_->IsSet());

  // Has network activity after 15s.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(15));
  detector_.OnPacketReceived(clock_.Now());
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(15),
            alarm_->deadline());
  // Handshake does not complete for another 15s.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(15));
  EXPECT_CALL(delegate_, OnHandshakeTimeout());
  alarm_->Fire();
}

TEST_F(QuicIdleNetworkDetectorTest,
       IdleNetworkDetectedAfterHandshakeCompletes) {
  EXPECT_FALSE(alarm_->IsSet());
  detector_.SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::FromSeconds(30),
      /*idle_network_timeout=*/QuicTime::Delta::FromSeconds(20));
  EXPECT_TRUE(alarm_->IsSet());
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(20),
            alarm_->deadline());

  // Handshake completes in 200ms.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(200));
  detector_.OnPacketReceived(clock_.Now());
  detector_.SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::Infinite(),
      /*idle_network_timeout=*/QuicTime::Delta::FromSeconds(600));
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(600),
            alarm_->deadline());

  // No network activity for 600s.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(600));
  EXPECT_CALL(delegate_, OnIdleNetworkDetected());
  alarm_->Fire();
}

TEST_F(QuicIdleNetworkDetectorTest,
       DoNotExtendIdleDeadlineOnConsecutiveSentPackets) {
  EXPECT_FALSE(alarm_->IsSet());
  detector_.SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::FromSeconds(30),
      /*idle_network_timeout=*/QuicTime::Delta::FromSeconds(20));
  EXPECT_TRUE(alarm_->IsSet());

  // Handshake completes in 200ms.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(200));
  detector_.OnPacketReceived(clock_.Now());
  detector_.SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::Infinite(),
      QuicTime::Delta::FromSeconds(600));
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(600),
            alarm_->deadline());

  // Sent packets after 200ms.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(200));
  detector_.OnPacketSent(clock_.Now(), QuicTime::Delta::Zero());
  const QuicTime packet_sent_time = clock_.Now();
  EXPECT_EQ(packet_sent_time + QuicTime::Delta::FromSeconds(600),
            alarm_->deadline());

  // Sent another packet after 200ms
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(200));
  detector_.OnPacketSent(clock_.Now(), QuicTime::Delta::Zero());
  // Verify network deadline does not extend.
  EXPECT_EQ(packet_sent_time + QuicTime::Delta::FromSeconds(600),
            alarm_->deadline());

  // No network activity for 600s.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(600) -
                     QuicTime::Delta::FromMilliseconds(200));
  EXPECT_CALL(delegate_, OnIdleNetworkDetected());
  alarm_->Fire();
}

TEST_F(QuicIdleNetworkDetectorTest, ShorterIdleTimeoutOnSentPacket) {
  detector_.enable_shorter_idle_timeout_on_sent_packet();
  QuicTime::Delta idle_network_timeout = QuicTime::Delta::Zero();
  idle_network_timeout = QuicTime::Delta::FromSeconds(30);
  detector_.SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::Infinite(), idle_network_timeout);
  EXPECT_TRUE(alarm_->IsSet());
  const QuicTime deadline = alarm_->deadline();
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(30), deadline);

  // Send a packet after 15s and 2s PTO delay.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(15));
  detector_.OnPacketSent(clock_.Now(), QuicTime::Delta::FromSeconds(2));
  EXPECT_TRUE(alarm_->IsSet());
  // Verify alarm does not get extended because deadline is > PTO delay.
  EXPECT_EQ(deadline, alarm_->deadline());

  // Send another packet near timeout and 2 s PTO delay.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(14));
  detector_.OnPacketSent(clock_.Now(), QuicTime::Delta::FromSeconds(2));
  EXPECT_TRUE(alarm_->IsSet());
  // Verify alarm does not get extended although it is shorter than PTO.
  EXPECT_EQ(deadline, alarm_->deadline());

  // Receive a packet after 1s.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(1));
  detector_.OnPacketReceived(clock_.Now());
  EXPECT_TRUE(alarm_->IsSet());
  // Verify idle timeout gets extended by 30s.
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(30),
            alarm_->deadline());

  // Send a packet near timeout.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(29));
  detector_.OnPacketSent(clock_.Now(), QuicTime::Delta::FromSeconds(2));
  EXPECT_TRUE(alarm_->IsSet());
  // Verify idle timeout gets extended by 1s.
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(2), alarm_->deadline());
}

TEST_F(QuicIdleNetworkDetectorTest, NoAlarmAfterStopped) {
  detector_.StopDetection();

  EXPECT_QUIC_BUG(
      detector_.SetTimeouts(
          /*handshake_timeout=*/QuicTime::Delta::FromSeconds(30),
          /*idle_network_timeout=*/QuicTime::Delta::FromSeconds(20)),
      "SetAlarm called after stopped");
  EXPECT_FALSE(alarm_->IsSet());
}

TEST_F(QuicIdleNetworkDetectorTest, MemoryReductionTimeout) {
  detector_.SetMemoryReductionTimeout(QuicTime::Delta::FromSeconds(200));
  // Memory reduction timeout is only set along with idle network timeout alarm.
  EXPECT_FALSE(alarm_->IsSet());

  detector_.SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::FromSeconds(30),
      /*idle_network_timeout=*/QuicTime::Delta::FromSeconds(20));
  EXPECT_TRUE(alarm_->IsSet());
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(20),
            alarm_->deadline());

  // Handshake completes in 200ms.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(200));
  detector_.OnPacketReceived(clock_.Now());
  detector_.SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::Infinite(),
      QuicTime::Delta::FromSeconds(600));
  // Verify memory reduction alarm is set.
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(200),
            alarm_->deadline());

  // Fires the memory reduction alarm.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(200));
  EXPECT_CALL(delegate_, OnMemoryReductionTimeout());
  alarm_->Fire();
  // Verifies that idle timeout alarm is set to tigger in (600s - 200s) = 400s.
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(400),
            alarm_->deadline());

  // Fires the idle network alarm.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(400));
  EXPECT_CALL(delegate_, OnIdleNetworkDetected());
  alarm_->Fire();
}

TEST_F(QuicIdleNetworkDetectorTest,
       MemoryReductionTimeoutTooCloseToIdleTimeoutIgnored) {
  detector_.SetMemoryReductionTimeout(QuicTime::Delta::FromSeconds(590));
  // Handshake completes.
  detector_.OnPacketReceived(clock_.Now());
  detector_.SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::Infinite(),
      QuicTime::Delta::FromSeconds(600));
  // Verify memory reduction alarm is NOT set.
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(600),
            alarm_->deadline());
}

TEST_F(QuicIdleNetworkDetectorTest,
       MemoryReductionTimeoutIgnoredBeforeHandshakeCompletes) {
  detector_.SetMemoryReductionTimeout(QuicTime::Delta::FromSeconds(5));
  // Handshake not yet completed.
  detector_.OnPacketReceived(clock_.Now());
  detector_.SetTimeouts(QuicTime::Delta::FromSeconds(30),
                        QuicTime::Delta::FromSeconds(200));
  // Verify memory reduction alarm is NOT set.
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(30),
            alarm_->deadline());
}

TEST_F(QuicIdleNetworkDetectorTest,
       MemoryReductionTimeoutDisabledByShorterIdleTimeoutOnSentPacket) {
  detector_.SetMemoryReductionTimeout(QuicTime::Delta::FromSeconds(200));
  detector_.enable_shorter_idle_timeout_on_sent_packet();

  // Handshake completes.
  detector_.OnPacketReceived(clock_.Now());
  detector_.SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::Infinite(),
      QuicTime::Delta::FromSeconds(600));
  // Verify memory reduction alarm is NOT set.
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(600),
            alarm_->deadline());
}

}  // namespace

}  // namespace test
}  // namespace quic
