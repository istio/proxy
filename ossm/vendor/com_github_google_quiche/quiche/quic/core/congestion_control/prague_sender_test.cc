// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/congestion_control/prague_sender.h"

#include <cstdint>
#include <optional>

#include "quiche/quic/core/congestion_control/cubic_bytes.h"
#include "quiche/quic/core/congestion_control/rtt_stats.h"
#include "quiche/quic/core/congestion_control/send_algorithm_interface.h"
#include "quiche/quic/core/quic_clock.h"
#include "quiche/quic/core/quic_connection_stats.h"
#include "quiche/quic/core/quic_constants.h"
#include "quiche/quic/core/quic_packet_number.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/mock_clock.h"

namespace quic::test {

// TODO(ianswett): A number of theses tests were written with the assumption of
// an initial CWND of 10. They have carefully calculated values which should be
// updated to be based on kInitialCongestionWindow.
const uint32_t kInitialCongestionWindowPackets = 10;
const uint32_t kMaxCongestionWindowPackets = 200;
const QuicTime::Delta kRtt = QuicTime::Delta::FromMilliseconds(10);

class PragueSenderPeer : public PragueSender {
 public:
  explicit PragueSenderPeer(const QuicClock* clock)
      : PragueSender(clock, &rtt_stats_, kInitialCongestionWindowPackets,
                     kMaxCongestionWindowPackets, &stats_) {}

  QuicTimeDelta rtt_virt() const { return rtt_virt_; }
  bool InReducedRttDependenceMode() const { return reduce_rtt_dependence_; }
  float alpha() const { return *prague_alpha_; }

  RttStats rtt_stats_;
  QuicConnectionStats stats_;
};

class PragueSenderTest : public QuicTest {
 protected:
  PragueSenderTest()
      : one_ms_(QuicTime::Delta::FromMilliseconds(1)),
        sender_(&clock_),
        packet_number_(1),
        acked_packet_number_(0),
        bytes_in_flight_(0),
        cubic_(&clock_) {
    EXPECT_TRUE(sender_.EnableECT1());
  }

  int SendAvailableSendWindow() {
    return SendAvailableSendWindow(kDefaultTCPMSS);
  }

  int SendAvailableSendWindow(QuicPacketLength /*packet_length*/) {
    // Send as long as TimeUntilSend returns Zero.
    int packets_sent = 0;
    bool can_send = sender_.CanSend(bytes_in_flight_);
    while (can_send) {
      sender_.OnPacketSent(clock_.Now(), bytes_in_flight_,
                           QuicPacketNumber(packet_number_++), kDefaultTCPMSS,
                           HAS_RETRANSMITTABLE_DATA);
      ++packets_sent;
      bytes_in_flight_ += kDefaultTCPMSS;
      can_send = sender_.CanSend(bytes_in_flight_);
    }
    return packets_sent;
  }

  // Normal is that TCP acks every other segment.
  void AckNPackets(int n, int ce) {
    EXPECT_LE(ce, n);
    sender_.rtt_stats_.UpdateRtt(kRtt, QuicTime::Delta::Zero(), clock_.Now());
    AckedPacketVector acked_packets;
    LostPacketVector lost_packets;
    for (int i = 0; i < n; ++i) {
      ++acked_packet_number_;
      acked_packets.push_back(
          AckedPacket(QuicPacketNumber(acked_packet_number_), kDefaultTCPMSS,
                      QuicTime::Zero()));
    }
    sender_.OnCongestionEvent(true, bytes_in_flight_, clock_.Now(),
                              acked_packets, lost_packets, n - ce, ce);
    bytes_in_flight_ -= n * kDefaultTCPMSS;
    clock_.AdvanceTime(one_ms_);
  }

  void LoseNPackets(int n) { LoseNPackets(n, kDefaultTCPMSS); }

  void LoseNPackets(int n, QuicPacketLength packet_length) {
    AckedPacketVector acked_packets;
    LostPacketVector lost_packets;
    for (int i = 0; i < n; ++i) {
      ++acked_packet_number_;
      lost_packets.push_back(
          LostPacket(QuicPacketNumber(acked_packet_number_), packet_length));
    }
    sender_.OnCongestionEvent(false, bytes_in_flight_, clock_.Now(),
                              acked_packets, lost_packets, 0, 0);
    bytes_in_flight_ -= n * packet_length;
  }

  // Does not increment acked_packet_number_.
  void LosePacket(uint64_t packet_number) {
    AckedPacketVector acked_packets;
    LostPacketVector lost_packets;
    lost_packets.push_back(
        LostPacket(QuicPacketNumber(packet_number), kDefaultTCPMSS));
    sender_.OnCongestionEvent(false, bytes_in_flight_, clock_.Now(),
                              acked_packets, lost_packets, 0, 0);
    bytes_in_flight_ -= kDefaultTCPMSS;
  }

  void MaybeUpdateAlpha(float& alpha, QuicTime& last_update, uint64_t& ect,
                        uint64_t& ce) {
    if (clock_.Now() - last_update > kPragueRttVirtMin) {
      float frac = static_cast<float>(ce) / static_cast<float>(ect + ce);
      alpha = (1 - kPragueEwmaGain) * alpha + kPragueEwmaGain * frac;
      last_update = clock_.Now();
      ect = 0;
      ce = 0;
    }
  }

  const QuicTime::Delta one_ms_;
  MockClock clock_;
  PragueSenderPeer sender_;
  uint64_t packet_number_;
  uint64_t acked_packet_number_;
  QuicByteCount bytes_in_flight_;
  // Since CubicBytes is not mockable, this copy will verify that PragueSender
  // is getting results equivalent to the expected calls to CubicBytes.
  CubicBytes cubic_;
};

TEST_F(PragueSenderTest, EcnResponseInCongestionAvoidance) {
  int num_sent = SendAvailableSendWindow();

  // Make sure we fall out of slow start.
  QuicByteCount expected_cwnd = sender_.GetCongestionWindow();
  LoseNPackets(1);
  expected_cwnd = cubic_.CongestionWindowAfterPacketLoss(expected_cwnd);
  EXPECT_EQ(expected_cwnd, sender_.GetCongestionWindow());

  // Ack the rest of the outstanding packets to get out of recovery.
  for (int i = 1; i < num_sent; ++i) {
    AckNPackets(1, 0);
  }
  // Exiting recovery; cwnd should not have increased.
  EXPECT_EQ(expected_cwnd, sender_.GetCongestionWindow());
  EXPECT_EQ(0u, bytes_in_flight_);
  // Send a new window of data and ack all; cubic growth should occur.
  num_sent = SendAvailableSendWindow();

  // Ack packets until the CWND increases.
  QuicByteCount original_cwnd = sender_.GetCongestionWindow();
  while (sender_.GetCongestionWindow() == original_cwnd) {
    AckNPackets(1, 0);
    expected_cwnd = cubic_.CongestionWindowAfterAck(
        kDefaultTCPMSS, expected_cwnd, kRtt, clock_.Now());
    EXPECT_EQ(expected_cwnd, sender_.GetCongestionWindow());
    SendAvailableSendWindow();
  }
  // Bytes in flight may be larger than the CWND if the CWND isn't an exact
  // multiple of the packet sizes being sent.
  EXPECT_GE(bytes_in_flight_, sender_.GetCongestionWindow());

  // Advance time 2 seconds waiting for an ack.
  clock_.AdvanceTime(kRtt);

  // First CE mark. Should be treated as a loss. Alpha = 1 so it is the full
  // Cubic loss response.
  original_cwnd = sender_.GetCongestionWindow();
  AckNPackets(2, 1);
  // Process the "loss", then the ack.
  expected_cwnd = cubic_.CongestionWindowAfterPacketLoss(expected_cwnd);
  QuicByteCount expected_ssthresh = expected_cwnd;
  QuicByteCount loss_reduction = original_cwnd - expected_cwnd;
  expected_cwnd = cubic_.CongestionWindowAfterAck(
      kDefaultTCPMSS / 2, expected_cwnd, kRtt, clock_.Now());
  expected_cwnd = cubic_.CongestionWindowAfterAck(
      kDefaultTCPMSS / 2, expected_cwnd, kRtt, clock_.Now());
  EXPECT_EQ(expected_cwnd, sender_.GetCongestionWindow());
  EXPECT_EQ(expected_ssthresh, sender_.GetSlowStartThreshold());

  // Second CE mark is ignored.
  AckNPackets(1, 1);
  EXPECT_EQ(expected_cwnd, sender_.GetCongestionWindow());

  // Since there was a full loss response, a subsequent loss should incorporate
  // that.
  LoseNPackets(1);
  expected_cwnd =
      cubic_.CongestionWindowAfterPacketLoss(expected_cwnd + loss_reduction);
  EXPECT_EQ(expected_cwnd, sender_.GetCongestionWindow());
  EXPECT_EQ(expected_cwnd, sender_.GetSlowStartThreshold());

  // With 10ms inputs, rtt_virt_ should be at the minimum value.
  EXPECT_EQ(sender_.rtt_virt().ToMilliseconds(), 25);
}

TEST_F(PragueSenderTest, EcnResponseInSlowStart) {
  SendAvailableSendWindow();
  AckNPackets(1, 1);
  EXPECT_FALSE(sender_.InSlowStart());
}

TEST_F(PragueSenderTest, ReducedRttDependence) {
  float expected_alpha;
  uint64_t num_ect = 0;
  uint64_t num_ce = 0;
  std::optional<QuicTime> last_alpha_update;
  std::optional<QuicTime> last_decrease;
  // While trying to get to 50 RTTs, check that alpha is being updated properly,
  // and is applied to CE response.
  while (!sender_.InReducedRttDependenceMode()) {
    int num_sent = SendAvailableSendWindow();
    clock_.AdvanceTime(kRtt);
    for (int i = 0; (i < num_sent - 1); ++i) {
      if (last_alpha_update.has_value()) {
        ++num_ect;
        MaybeUpdateAlpha(expected_alpha, last_alpha_update.value(), num_ect,
                         num_ce);
      }
      AckNPackets(1, 0);
    }
    QuicByteCount cwnd = sender_.GetCongestionWindow();
    num_ce++;
    if (last_alpha_update.has_value()) {
      MaybeUpdateAlpha(expected_alpha, last_alpha_update.value(), num_ect,
                       num_ce);
    } else {
      // First CE mark starts the update
      expected_alpha = 1.0;
      last_alpha_update = clock_.Now();
    }
    AckNPackets(1, 1);
    bool simulated_loss = false;
    if (!last_decrease.has_value() ||
        (clock_.Now() - last_decrease.value() > sender_.rtt_virt())) {
      QuicByteCount new_cwnd = cubic_.CongestionWindowAfterPacketLoss(cwnd);
      // Add one byte to fix a rounding error.
      QuicByteCount reduction = (cwnd - new_cwnd) * expected_alpha;
      cwnd -= reduction;
      last_decrease = clock_.Now();
      simulated_loss = true;
    }
    EXPECT_EQ(expected_alpha, sender_.alpha());
    EXPECT_EQ(cwnd, sender_.GetCongestionWindow());
    // This is the one spot where PragueSender has to manually update ssthresh.
    if (simulated_loss) {
      EXPECT_EQ(cwnd, sender_.GetSlowStartThreshold());
    }
  }
  SendAvailableSendWindow();
  // Next ack should be scaled by 1/M^2 = 1/2.5^2
  QuicByteCount expected_cwnd = sender_.GetCongestionWindow();
  QuicByteCount expected_increase =
      cubic_.CongestionWindowAfterAck(kDefaultTCPMSS, expected_cwnd, kRtt,
                                      clock_.Now()) -
      expected_cwnd;
  expected_increase = static_cast<float>(expected_increase) / (2.5 * 2.5);
  AckNPackets(1, 0);
  EXPECT_EQ(expected_cwnd + expected_increase, sender_.GetCongestionWindow());
}

}  // namespace quic::test
