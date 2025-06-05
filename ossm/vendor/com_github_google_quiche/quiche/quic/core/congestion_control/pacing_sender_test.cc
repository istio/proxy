// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/congestion_control/pacing_sender.h"

#include <memory>
#include <utility>

#include "quiche/quic/core/quic_constants.h"
#include "quiche/quic/core/quic_packets.h"
#include "quiche/quic/platform/api/quic_flag_utils.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_logging.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/mock_clock.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

using testing::_;
using testing::AtMost;
using testing::Return;
using testing::StrictMock;

namespace quic {
namespace test {

const QuicByteCount kBytesInFlight = 1024;
const int kInitialBurstPackets = 10;

class TestPacingSender : public PacingSender {
 public:
  using PacingSender::lumpy_tokens;
  using PacingSender::PacingSender;

  QuicTime ideal_next_packet_send_time() const {
    return GetNextReleaseTime().release_time;
  }
};

class PacingSenderTest : public QuicTest {
 protected:
  PacingSenderTest()
      : zero_time_(QuicTime::Delta::Zero()),
        infinite_time_(QuicTime::Delta::Infinite()),
        packet_number_(1),
        mock_sender_(new StrictMock<MockSendAlgorithm>()),
        pacing_sender_(new TestPacingSender) {
    pacing_sender_->set_sender(mock_sender_.get());
    // Pick arbitrary time.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(9));
  }

  ~PacingSenderTest() override {}

  void InitPacingRate(QuicPacketCount burst_size, QuicBandwidth bandwidth) {
    mock_sender_ = std::make_unique<StrictMock<MockSendAlgorithm>>();
    pacing_sender_ = std::make_unique<TestPacingSender>();
    pacing_sender_->set_sender(mock_sender_.get());
    EXPECT_CALL(*mock_sender_, PacingRate(_)).WillRepeatedly(Return(bandwidth));
    EXPECT_CALL(*mock_sender_, BandwidthEstimate())
        .WillRepeatedly(Return(bandwidth));
    if (burst_size == 0) {
      EXPECT_CALL(*mock_sender_, OnCongestionEvent(_, _, _, _, _, _, _));
      LostPacketVector lost_packets;
      lost_packets.push_back(
          LostPacket(QuicPacketNumber(1), kMaxOutgoingPacketSize));
      AckedPacketVector empty;
      pacing_sender_->OnCongestionEvent(true, 1234, clock_.Now(), empty,
                                        lost_packets, 0, 0);
    } else if (burst_size != kInitialBurstPackets) {
      QUIC_LOG(FATAL) << "Unsupported burst_size " << burst_size
                      << " specificied, only 0 and " << kInitialBurstPackets
                      << " are supported.";
    }
  }

  void CheckPacketIsSentImmediately(HasRetransmittableData retransmittable_data,
                                    QuicByteCount prior_in_flight,
                                    bool in_recovery, QuicPacketCount cwnd) {
    // In order for the packet to be sendable, the underlying sender must
    // permit it to be sent immediately.
    for (int i = 0; i < 2; ++i) {
      EXPECT_CALL(*mock_sender_, CanSend(prior_in_flight))
          .WillOnce(Return(true));
      // Verify that the packet can be sent immediately.
      EXPECT_EQ(zero_time_,
                pacing_sender_->TimeUntilSend(clock_.Now(), prior_in_flight))
          << "Next packet to send is " << packet_number_;
    }

    // Actually send the packet.
    if (prior_in_flight == 0 &&
        !GetQuicReloadableFlag(quic_pacing_remove_non_initial_burst)) {
      EXPECT_CALL(*mock_sender_, InRecovery()).WillOnce(Return(in_recovery));
    }
    EXPECT_CALL(*mock_sender_,
                OnPacketSent(clock_.Now(), prior_in_flight, packet_number_,
                             kMaxOutgoingPacketSize, retransmittable_data));
    EXPECT_CALL(*mock_sender_, GetCongestionWindow())
        .WillRepeatedly(Return(cwnd * kDefaultTCPMSS));
    EXPECT_CALL(*mock_sender_,
                CanSend(prior_in_flight + kMaxOutgoingPacketSize))
        .Times(AtMost(1))
        .WillRepeatedly(Return((prior_in_flight + kMaxOutgoingPacketSize) <
                               (cwnd * kDefaultTCPMSS)));
    pacing_sender_->OnPacketSent(clock_.Now(), prior_in_flight,
                                 packet_number_++, kMaxOutgoingPacketSize,
                                 retransmittable_data);
  }

  void CheckPacketIsSentImmediately() {
    CheckPacketIsSentImmediately(HAS_RETRANSMITTABLE_DATA, kBytesInFlight,
                                 false, 10);
  }

  void CheckPacketIsDelayed(QuicTime::Delta delay) {
    // In order for the packet to be sendable, the underlying sender must
    // permit it to be sent immediately.
    for (int i = 0; i < 2; ++i) {
      EXPECT_CALL(*mock_sender_, CanSend(kBytesInFlight))
          .WillOnce(Return(true));
      // Verify that the packet is delayed.
      EXPECT_EQ(delay.ToMicroseconds(),
                pacing_sender_->TimeUntilSend(clock_.Now(), kBytesInFlight)
                    .ToMicroseconds());
    }
  }

  void UpdateRtt() {
    EXPECT_CALL(*mock_sender_,
                OnCongestionEvent(true, kBytesInFlight, _, _, _, _, _));
    AckedPacketVector empty_acked;
    LostPacketVector empty_lost;
    pacing_sender_->OnCongestionEvent(true, kBytesInFlight, clock_.Now(),
                                      empty_acked, empty_lost, 0, 0);
  }

  void OnApplicationLimited() { pacing_sender_->OnApplicationLimited(); }

  const QuicTime::Delta zero_time_;
  const QuicTime::Delta infinite_time_;
  MockClock clock_;
  QuicPacketNumber packet_number_;
  std::unique_ptr<StrictMock<MockSendAlgorithm>> mock_sender_;
  std::unique_ptr<TestPacingSender> pacing_sender_;
};

TEST_F(PacingSenderTest, NoSend) {
  for (int i = 0; i < 2; ++i) {
    EXPECT_CALL(*mock_sender_, CanSend(kBytesInFlight)).WillOnce(Return(false));
    EXPECT_EQ(infinite_time_,
              pacing_sender_->TimeUntilSend(clock_.Now(), kBytesInFlight));
  }
}

TEST_F(PacingSenderTest, SendNow) {
  for (int i = 0; i < 2; ++i) {
    EXPECT_CALL(*mock_sender_, CanSend(kBytesInFlight)).WillOnce(Return(true));
    EXPECT_EQ(zero_time_,
              pacing_sender_->TimeUntilSend(clock_.Now(), kBytesInFlight));
  }
}

TEST_F(PacingSenderTest, VariousSending) {
  // Configure pacing rate of 1 packet per 1 ms, no initial burst.
  InitPacingRate(
      0, QuicBandwidth::FromBytesAndTimeDelta(
             kMaxOutgoingPacketSize, QuicTime::Delta::FromMilliseconds(1)));

  // Now update the RTT and verify that packets are actually paced.
  UpdateRtt();

  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();

  // The first packet was a "make up", then we sent two packets "into the
  // future", so the delay should be 2.
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));

  // Wake up on time.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(2));
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));

  // Wake up late.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(4));
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));

  // Wake up really late.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(8));
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));

  // Wake up really late again, but application pause partway through.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(8));
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  OnApplicationLimited();
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(100));
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));
  // Wake up early, but after enough time has passed to permit a send.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
  CheckPacketIsSentImmediately();
}

TEST_F(PacingSenderTest, InitialBurst) {
  // Configure pacing rate of 1 packet per 1 ms.
  InitPacingRate(
      10, QuicBandwidth::FromBytesAndTimeDelta(
              kMaxOutgoingPacketSize, QuicTime::Delta::FromMilliseconds(1)));

  // Update the RTT and verify that the first 10 packets aren't paced.
  UpdateRtt();

  // Send 10 packets, and verify that they are not paced.
  for (int i = 0; i < kInitialBurstPackets; ++i) {
    CheckPacketIsSentImmediately();
  }

  // The first packet was a "make up", then we sent two packets "into the
  // future", so the delay should be 2ms.
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));

  if (GetQuicReloadableFlag(quic_pacing_remove_non_initial_burst)) {
    // Can send some packets immediately to make up for 5ms of lost time.
    for (int i = 0; i < 6; ++i) {
      CheckPacketIsSentImmediately();
    }
    CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(3));
    return;
  }

  CheckPacketIsSentImmediately();
  // Next time TimeUntilSend is called with no bytes in flight, pacing should
  // allow a packet to be sent, and when it's sent, the tokens are refilled.
  CheckPacketIsSentImmediately(HAS_RETRANSMITTABLE_DATA, 0, false, 10);
  for (int i = 0; i < kInitialBurstPackets - 1; ++i) {
    CheckPacketIsSentImmediately();
  }

  // The first packet was a "make up", then we sent two packets "into the
  // future", so the delay should be 2ms.
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));
}

TEST_F(PacingSenderTest, InitialBurstNoRttMeasurement) {
  // Configure pacing rate of 1 packet per 1 ms.
  InitPacingRate(
      10, QuicBandwidth::FromBytesAndTimeDelta(
              kMaxOutgoingPacketSize, QuicTime::Delta::FromMilliseconds(1)));

  // Send 10 packets, and verify that they are not paced.
  for (int i = 0; i < kInitialBurstPackets; ++i) {
    CheckPacketIsSentImmediately();
  }

  // The first packet was a "make up", then we sent two packets "into the
  // future", so the delay should be 2ms.
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));

  if (GetQuicReloadableFlag(quic_pacing_remove_non_initial_burst)) {
    // Can send some packets immediately to make up for 5ms of lost time.
    for (int i = 0; i < 6; ++i) {
      CheckPacketIsSentImmediately();
    }
    CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(3));
    return;
  }

  CheckPacketIsSentImmediately();

  // Next time TimeUntilSend is called with no bytes in flight, the tokens
  // should be refilled and there should be no delay.
  CheckPacketIsSentImmediately(HAS_RETRANSMITTABLE_DATA, 0, false, 10);
  // Send 10 packets, and verify that they are not paced.
  for (int i = 0; i < kInitialBurstPackets - 1; ++i) {
    CheckPacketIsSentImmediately();
  }

  // The first packet was a "make up", then we sent two packets "into the
  // future", so the delay should be 2ms.
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));
}

TEST_F(PacingSenderTest, FastSending) {
  // Ensure the pacing sender paces, even when the inter-packet spacing(0.5ms)
  // is less than the pacing granularity(1ms).
  InitPacingRate(10, QuicBandwidth::FromBytesAndTimeDelta(
                         2 * kMaxOutgoingPacketSize,
                         QuicTime::Delta::FromMilliseconds(1)));
  // Update the RTT and verify that the first 10 packets aren't paced.
  UpdateRtt();

  // Send 10 packets, and verify that they are not paced.
  for (int i = 0; i < kInitialBurstPackets; ++i) {
    CheckPacketIsSentImmediately();
  }

  CheckPacketIsSentImmediately();  // Make up
  CheckPacketIsSentImmediately();  // Lumpy token
  CheckPacketIsSentImmediately();  // "In the future" but within granularity.
  CheckPacketIsSentImmediately();  // Lumpy token
  CheckPacketIsDelayed(QuicTime::Delta::FromMicroseconds(2000));

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));

  if (GetQuicReloadableFlag(quic_pacing_remove_non_initial_burst)) {
    // Can send some packets immediately to make up for 5ms of lost time.
    for (int i = 0; i < 10; ++i) {
      CheckPacketIsSentImmediately();
    }
    CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));
    return;
  }

  CheckPacketIsSentImmediately();

  // Next time TimeUntilSend is called with no bytes in flight, the tokens
  // should be refilled and there should be no delay.
  CheckPacketIsSentImmediately(HAS_RETRANSMITTABLE_DATA, 0, false, 10);
  for (int i = 0; i < kInitialBurstPackets - 1; ++i) {
    CheckPacketIsSentImmediately();
  }

  // The first packet was a "make up", then we sent two packets "into the
  // future", so the delay should be 1.5ms.
  CheckPacketIsSentImmediately();  // Make up
  CheckPacketIsSentImmediately();  // Lumpy token
  CheckPacketIsSentImmediately();  // "In the future" but within granularity.
  CheckPacketIsSentImmediately();  // Lumpy token
  CheckPacketIsDelayed(QuicTime::Delta::FromMicroseconds(2000));
}

TEST_F(PacingSenderTest, NoBurstEnteringRecovery) {
  // Configure pacing rate of 1 packet per 1 ms with no burst tokens.
  InitPacingRate(
      0, QuicBandwidth::FromBytesAndTimeDelta(
             kMaxOutgoingPacketSize, QuicTime::Delta::FromMilliseconds(1)));
  // Sending a packet will set burst tokens.
  CheckPacketIsSentImmediately();

  // Losing a packet will set clear burst tokens.
  LostPacketVector lost_packets;
  lost_packets.push_back(
      LostPacket(QuicPacketNumber(1), kMaxOutgoingPacketSize));
  AckedPacketVector empty_acked;
  EXPECT_CALL(*mock_sender_, OnCongestionEvent(true, kMaxOutgoingPacketSize, _,
                                               testing::IsEmpty(), _, _, _));
  pacing_sender_->OnCongestionEvent(true, kMaxOutgoingPacketSize, clock_.Now(),
                                    empty_acked, lost_packets, 0, 0);
  // One packet is sent immediately, because of 1ms pacing granularity.
  CheckPacketIsSentImmediately();
  // Ensure packets are immediately paced.
  EXPECT_CALL(*mock_sender_, CanSend(kMaxOutgoingPacketSize))
      .WillOnce(Return(true));
  // Verify the next packet is paced and delayed 2ms due to granularity.
  EXPECT_EQ(
      QuicTime::Delta::FromMilliseconds(2),
      pacing_sender_->TimeUntilSend(clock_.Now(), kMaxOutgoingPacketSize));
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));
}

TEST_F(PacingSenderTest, NoBurstInRecovery) {
  // Configure pacing rate of 1 packet per 1 ms with no burst tokens.
  InitPacingRate(
      0, QuicBandwidth::FromBytesAndTimeDelta(
             kMaxOutgoingPacketSize, QuicTime::Delta::FromMilliseconds(1)));

  UpdateRtt();

  // Ensure only one packet is sent immediately and the rest are paced.
  CheckPacketIsSentImmediately(HAS_RETRANSMITTABLE_DATA, 0, true, 10);
  CheckPacketIsSentImmediately();
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));
}

TEST_F(PacingSenderTest, CwndLimited) {
  // Configure pacing rate of 1 packet per 1 ms, no initial burst.
  InitPacingRate(
      0, QuicBandwidth::FromBytesAndTimeDelta(
             kMaxOutgoingPacketSize, QuicTime::Delta::FromMilliseconds(1)));

  UpdateRtt();

  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  // Packet 3 will be delayed 2ms.
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));

  // Wake up on time.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(2));
  // After sending packet 3, cwnd is limited.
  // This test is slightly odd because bytes_in_flight is calculated using
  // kMaxOutgoingPacketSize and CWND is calculated using kDefaultTCPMSS,
  // which is 8 bytes larger, so 3 packets can be sent for a CWND of 2.
  CheckPacketIsSentImmediately(HAS_RETRANSMITTABLE_DATA,
                               2 * kMaxOutgoingPacketSize, false, 2);

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(100));
  // Verify pacing sender stops making up for lost time after sending packet 3.
  // Packet 6 will be delayed 2ms.
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));
}

TEST_F(PacingSenderTest, LumpyPacingWithInitialBurstToken) {
  // Set lumpy size to be 3, and cwnd faction to 0.5
  SetQuicFlag(quic_lumpy_pacing_size, 3);
  SetQuicFlag(quic_lumpy_pacing_cwnd_fraction, 0.5f);
  // Configure pacing rate of 1 packet per 1 ms.
  InitPacingRate(
      10, QuicBandwidth::FromBytesAndTimeDelta(
              kMaxOutgoingPacketSize, QuicTime::Delta::FromMilliseconds(1)));
  UpdateRtt();

  // Send 10 packets, and verify that they are not paced.
  for (int i = 0; i < kInitialBurstPackets; ++i) {
    CheckPacketIsSentImmediately();
  }

  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  // Packet 14 will be delayed 3ms.
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(3));

  // Wake up on time.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(3));
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  // Packet 17 will be delayed 3ms.
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(3));

  // Application throttles sending.
  OnApplicationLimited();
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(100));
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  // Packet 20 will be delayed 3ms.
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(3));

  // Wake up on time.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(3));
  CheckPacketIsSentImmediately();
  // After sending packet 21, cwnd is limited.
  // This test is slightly odd because bytes_in_flight is calculated using
  // kMaxOutgoingPacketSize and CWND is calculated using kDefaultTCPMSS,
  // which is 8 bytes larger, so 21 packets can be sent for a CWND of 20.
  CheckPacketIsSentImmediately(HAS_RETRANSMITTABLE_DATA,
                               20 * kMaxOutgoingPacketSize, false, 20);

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(100));
  // Suppose cwnd size is 5, so that lumpy size becomes 2.
  CheckPacketIsSentImmediately(HAS_RETRANSMITTABLE_DATA, kBytesInFlight, false,
                               5);
  CheckPacketIsSentImmediately();
  // Packet 24 will be delayed 2ms.
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));
}

TEST_F(PacingSenderTest, NoLumpyPacingForLowBandwidthFlows) {
  // Set lumpy size to be 3, and cwnd fraction to 0.5
  SetQuicFlag(quic_lumpy_pacing_size, 3);
  SetQuicFlag(quic_lumpy_pacing_cwnd_fraction, 0.5f);

  // Configure pacing rate of 1 packet per 100 ms.
  QuicTime::Delta inter_packet_delay = QuicTime::Delta::FromMilliseconds(100);
  InitPacingRate(kInitialBurstPackets,
                 QuicBandwidth::FromBytesAndTimeDelta(kMaxOutgoingPacketSize,
                                                      inter_packet_delay));
  UpdateRtt();

  // Send kInitialBurstPackets packets, and verify that they are not paced.
  for (int i = 0; i < kInitialBurstPackets; ++i) {
    CheckPacketIsSentImmediately();
  }

  // The first packet after burst token exhausted is also sent immediately,
  // because ideal_next_packet_send_time has not been set yet.
  CheckPacketIsSentImmediately();

  for (int i = 0; i < 200; ++i) {
    CheckPacketIsDelayed(inter_packet_delay);
  }
}

// Regression test for b/184471302 to ensure that ACKs received back-to-back
// don't cause bursts in sending.
TEST_F(PacingSenderTest, NoBurstsForLumpyPacingWithAckAggregation) {
  // Configure pacing rate of 1 packet per millisecond.
  QuicTime::Delta inter_packet_delay = QuicTime::Delta::FromMilliseconds(1);
  InitPacingRate(kInitialBurstPackets,
                 QuicBandwidth::FromBytesAndTimeDelta(kMaxOutgoingPacketSize,
                                                      inter_packet_delay));
  UpdateRtt();

  // Send kInitialBurstPackets packets, and verify that they are not paced.
  for (int i = 0; i < kInitialBurstPackets; ++i) {
    CheckPacketIsSentImmediately();
  }
  // The last packet of the burst causes the sender to be CWND limited.
  CheckPacketIsSentImmediately(HAS_RETRANSMITTABLE_DATA,
                               10 * kMaxOutgoingPacketSize, false, 10);

  // The last sent packet made the connection CWND limited, so no lumpy tokens
  // should be available.
  EXPECT_EQ(0u, pacing_sender_->lumpy_tokens());
  CheckPacketIsSentImmediately(HAS_RETRANSMITTABLE_DATA,
                               10 * kMaxOutgoingPacketSize, false, 10);
  EXPECT_EQ(0u, pacing_sender_->lumpy_tokens());
  CheckPacketIsDelayed(2 * inter_packet_delay);
}

TEST_F(PacingSenderTest, IdealNextPacketSendTimeWithLumpyPacing) {
  // Set lumpy size to be 3, and cwnd faction to 0.5
  SetQuicFlag(quic_lumpy_pacing_size, 3);
  SetQuicFlag(quic_lumpy_pacing_cwnd_fraction, 0.5f);

  // Configure pacing rate of 1 packet per millisecond.
  QuicTime::Delta inter_packet_delay = QuicTime::Delta::FromMilliseconds(1);
  InitPacingRate(kInitialBurstPackets,
                 QuicBandwidth::FromBytesAndTimeDelta(kMaxOutgoingPacketSize,
                                                      inter_packet_delay));

  // Send kInitialBurstPackets packets, and verify that they are not paced.
  for (int i = 0; i < kInitialBurstPackets; ++i) {
    CheckPacketIsSentImmediately();
  }

  CheckPacketIsSentImmediately();
  EXPECT_EQ(pacing_sender_->ideal_next_packet_send_time(),
            clock_.Now() + inter_packet_delay);
  EXPECT_EQ(pacing_sender_->lumpy_tokens(), 2u);

  CheckPacketIsSentImmediately();
  EXPECT_EQ(pacing_sender_->ideal_next_packet_send_time(),
            clock_.Now() + 2 * inter_packet_delay);
  EXPECT_EQ(pacing_sender_->lumpy_tokens(), 1u);

  CheckPacketIsSentImmediately();
  EXPECT_EQ(pacing_sender_->ideal_next_packet_send_time(),
            clock_.Now() + 3 * inter_packet_delay);
  EXPECT_EQ(pacing_sender_->lumpy_tokens(), 0u);

  CheckPacketIsDelayed(3 * inter_packet_delay);

  // Wake up on time.
  clock_.AdvanceTime(3 * inter_packet_delay);
  CheckPacketIsSentImmediately();
  EXPECT_EQ(pacing_sender_->ideal_next_packet_send_time(),
            clock_.Now() + inter_packet_delay);
  EXPECT_EQ(pacing_sender_->lumpy_tokens(), 2u);

  CheckPacketIsSentImmediately();
  EXPECT_EQ(pacing_sender_->ideal_next_packet_send_time(),
            clock_.Now() + 2 * inter_packet_delay);
  EXPECT_EQ(pacing_sender_->lumpy_tokens(), 1u);

  CheckPacketIsSentImmediately();
  EXPECT_EQ(pacing_sender_->ideal_next_packet_send_time(),
            clock_.Now() + 3 * inter_packet_delay);
  EXPECT_EQ(pacing_sender_->lumpy_tokens(), 0u);

  CheckPacketIsDelayed(3 * inter_packet_delay);

  // Wake up late.
  clock_.AdvanceTime(4.5 * inter_packet_delay);
  CheckPacketIsSentImmediately();
  EXPECT_EQ(pacing_sender_->ideal_next_packet_send_time(),
            clock_.Now() - 0.5 * inter_packet_delay);
  EXPECT_EQ(pacing_sender_->lumpy_tokens(), 2u);

  CheckPacketIsSentImmediately();
  EXPECT_EQ(pacing_sender_->ideal_next_packet_send_time(),
            clock_.Now() + 0.5 * inter_packet_delay);
  EXPECT_EQ(pacing_sender_->lumpy_tokens(), 1u);

  CheckPacketIsSentImmediately();
  EXPECT_EQ(pacing_sender_->ideal_next_packet_send_time(),
            clock_.Now() + 1.5 * inter_packet_delay);
  EXPECT_EQ(pacing_sender_->lumpy_tokens(), 0u);

  CheckPacketIsDelayed(1.5 * inter_packet_delay);
}

}  // namespace test
}  // namespace quic
