// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_sent_packet_manager.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/congestion_control/loss_detection_interface.h"
#include "quiche/quic/core/congestion_control/rtt_stats.h"
#include "quiche/quic/core/crypto/crypto_protocol.h"
#include "quiche/quic/core/crypto/quic_random.h"
#include "quiche/quic/core/frames/quic_ack_frame.h"
#include "quiche/quic/core/frames/quic_ack_frequency_frame.h"
#include "quiche/quic/core/frames/quic_datagram_frame.h"
#include "quiche/quic/core/frames/quic_frame.h"
#include "quiche/quic/core/frames/quic_path_challenge_frame.h"
#include "quiche/quic/core/frames/quic_ping_frame.h"
#include "quiche/quic/core/frames/quic_stream_frame.h"
#include "quiche/quic/core/quic_bandwidth.h"
#include "quiche/quic/core/quic_constants.h"
#include "quiche/quic/core/quic_packet_number.h"
#include "quiche/quic/core/quic_packets.h"
#include "quiche/quic/core/quic_tag.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_unacked_packet_map.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/mock_clock.h"
#include "quiche/quic/test_tools/quic_config_peer.h"
#include "quiche/quic/test_tools/quic_sent_packet_manager_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_buffer_allocator.h"
#include "quiche/common/quiche_mem_slice.h"
#include "quiche/common/simple_buffer_allocator.h"

using testing::_;
using testing::AnyNumber;
using testing::InvokeWithoutArgs;
using testing::IsEmpty;
using testing::Pointwise;
using testing::Return;
using testing::StrictMock;
using testing::WithArgs;

namespace quic {
namespace test {
namespace {
// Default packet length.
const uint32_t kDefaultLength = 1000;

// Stream ID for data sent in CreatePacket().
const QuicStreamId kStreamId = 7;

// The compiler won't allow std::nullopt as an argument.
const std::optional<QuicEcnCounts> kEmptyCounts = std::nullopt;

// Matcher to check that the packet number matches the second argument.
MATCHER(PacketNumberEq, "") {
  return std::get<0>(arg).packet_number == QuicPacketNumber(std::get<1>(arg));
}

class MockDebugDelegate : public QuicSentPacketManager::DebugDelegate {
 public:
  MOCK_METHOD(void, OnSpuriousPacketRetransmission,
              (TransmissionType transmission_type, QuicByteCount byte_size),
              (override));
  MOCK_METHOD(void, OnPacketLoss,
              (QuicPacketNumber lost_packet_number,
               EncryptionLevel encryption_level,
               TransmissionType transmission_type, QuicTime detection_time),
              (override));
  MOCK_METHOD(void, OnIncomingAck,
              (QuicPacketNumber ack_packet_number,
               EncryptionLevel ack_decrypted_level,
               const QuicAckFrame& ack_frame, QuicTime ack_receive_time,
               QuicPacketNumber largest_observed, bool rtt_updated,
               QuicPacketNumber least_unacked_sent_packet),
              (override));
};

class QuicSentPacketManagerTest : public QuicTest {
 public:
  bool RetransmitCryptoPacket(uint64_t packet_number) {
    EXPECT_CALL(
        *send_algorithm_,
        OnPacketSent(_, BytesInFlight(), QuicPacketNumber(packet_number),
                     kDefaultLength, HAS_RETRANSMITTABLE_DATA));
    SerializedPacket packet(CreatePacket(packet_number, false));
    packet.retransmittable_frames.push_back(
        QuicFrame(QuicStreamFrame(1, false, 0, absl::string_view())));
    packet.has_crypto_handshake = IS_HANDSHAKE;
    manager_.OnPacketSent(&packet, clock_.Now(), HANDSHAKE_RETRANSMISSION,
                          HAS_RETRANSMITTABLE_DATA, true, ECN_NOT_ECT);
    return true;
  }

  bool RetransmitDataPacket(uint64_t packet_number, TransmissionType type,
                            EncryptionLevel level) {
    EXPECT_CALL(
        *send_algorithm_,
        OnPacketSent(_, BytesInFlight(), QuicPacketNumber(packet_number),
                     kDefaultLength, HAS_RETRANSMITTABLE_DATA));
    SerializedPacket packet(CreatePacket(packet_number, true));
    packet.encryption_level = level;
    manager_.OnPacketSent(&packet, clock_.Now(), type, HAS_RETRANSMITTABLE_DATA,
                          true, ECN_NOT_ECT);
    return true;
  }

  bool RetransmitDataPacket(uint64_t packet_number, TransmissionType type) {
    return RetransmitDataPacket(packet_number, type, ENCRYPTION_INITIAL);
  }

 protected:
  const CongestionControlType kInitialCongestionControlType = kCubicBytes;
  QuicSentPacketManagerTest()
      : manager_(Perspective::IS_SERVER, &clock_, QuicRandom::GetInstance(),
                 &stats_, kInitialCongestionControlType),
        send_algorithm_(new StrictMock<MockSendAlgorithm>),
        network_change_visitor_(new StrictMock<MockNetworkChangeVisitor>) {
    QuicSentPacketManagerPeer::SetSendAlgorithm(&manager_, send_algorithm_);
    // Advance the time 1s so the send times are never QuicTime::Zero.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1000));
    manager_.SetNetworkChangeVisitor(network_change_visitor_.get());
    manager_.SetSessionNotifier(&notifier_);

    EXPECT_CALL(*send_algorithm_, GetCongestionControlType())
        .WillRepeatedly(Return(kInitialCongestionControlType));
    EXPECT_CALL(*send_algorithm_, BandwidthEstimate())
        .Times(AnyNumber())
        .WillRepeatedly(Return(QuicBandwidth::Zero()));
    EXPECT_CALL(*send_algorithm_, InSlowStart()).Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, InRecovery()).Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, OnPacketNeutered(_)).Times(AnyNumber());
    EXPECT_CALL(*network_change_visitor_, OnPathMtuIncreased(1000))
        .Times(AnyNumber());
    EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(notifier_, HasUnackedCryptoData())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(notifier_, OnStreamFrameRetransmitted(_)).Times(AnyNumber());
    EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _))
        .WillRepeatedly(Return(true));
  }

  ~QuicSentPacketManagerTest() override {}

  QuicByteCount BytesInFlight() { return manager_.GetBytesInFlight(); }
  void VerifyUnackedPackets(uint64_t* packets, size_t num_packets) {
    if (num_packets == 0) {
      EXPECT_TRUE(manager_.unacked_packets().empty());
      EXPECT_EQ(0u, QuicSentPacketManagerPeer::GetNumRetransmittablePackets(
                        &manager_));
      return;
    }

    EXPECT_FALSE(manager_.unacked_packets().empty());
    EXPECT_EQ(QuicPacketNumber(packets[0]), manager_.GetLeastUnacked());
    for (size_t i = 0; i < num_packets; ++i) {
      EXPECT_TRUE(
          manager_.unacked_packets().IsUnacked(QuicPacketNumber(packets[i])))
          << packets[i];
    }
  }

  void VerifyRetransmittablePackets(uint64_t* packets, size_t num_packets) {
    EXPECT_EQ(
        num_packets,
        QuicSentPacketManagerPeer::GetNumRetransmittablePackets(&manager_));
    for (size_t i = 0; i < num_packets; ++i) {
      EXPECT_TRUE(QuicSentPacketManagerPeer::HasRetransmittableFrames(
          &manager_, packets[i]))
          << " packets[" << i << "]:" << packets[i];
    }
  }

  void ExpectAck(uint64_t largest_observed) {
    EXPECT_CALL(
        *send_algorithm_,
        // Ensure the AckedPacketVector argument contains largest_observed.
        OnCongestionEvent(true, _, _,
                          Pointwise(PacketNumberEq(), {largest_observed}),
                          IsEmpty(), _, _));
    EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  }

  void ExpectUpdatedRtt(uint64_t /*largest_observed*/) {
    EXPECT_CALL(*send_algorithm_,
                OnCongestionEvent(true, _, _, IsEmpty(), IsEmpty(), _, _));
    EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  }

  void ExpectAckAndLoss(bool rtt_updated, uint64_t largest_observed,
                        uint64_t lost_packet) {
    EXPECT_CALL(
        *send_algorithm_,
        OnCongestionEvent(rtt_updated, _, _,
                          Pointwise(PacketNumberEq(), {largest_observed}),
                          Pointwise(PacketNumberEq(), {lost_packet}), _, _));
    EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  }

  // |packets_acked| and |packets_lost| should be in packet number order.
  void ExpectAcksAndLosses(bool rtt_updated, uint64_t* packets_acked,
                           size_t num_packets_acked, uint64_t* packets_lost,
                           size_t num_packets_lost) {
    std::vector<QuicPacketNumber> ack_vector;
    for (size_t i = 0; i < num_packets_acked; ++i) {
      ack_vector.push_back(QuicPacketNumber(packets_acked[i]));
    }
    std::vector<QuicPacketNumber> lost_vector;
    for (size_t i = 0; i < num_packets_lost; ++i) {
      lost_vector.push_back(QuicPacketNumber(packets_lost[i]));
    }
    EXPECT_CALL(*send_algorithm_,
                OnCongestionEvent(
                    rtt_updated, _, _, Pointwise(PacketNumberEq(), ack_vector),
                    Pointwise(PacketNumberEq(), lost_vector), _, _));
    EXPECT_CALL(*network_change_visitor_, OnCongestionChange())
        .Times(AnyNumber());
  }

  void RetransmitAndSendPacket(uint64_t old_packet_number,
                               uint64_t new_packet_number) {
    RetransmitAndSendPacket(old_packet_number, new_packet_number,
                            PTO_RETRANSMISSION);
  }

  void RetransmitAndSendPacket(uint64_t old_packet_number,
                               uint64_t new_packet_number,
                               TransmissionType transmission_type) {
    bool is_lost = false;
    if (transmission_type == HANDSHAKE_RETRANSMISSION ||
        transmission_type == PTO_RETRANSMISSION) {
      EXPECT_CALL(notifier_, RetransmitFrames(_, _))
          .WillOnce(
              WithArgs<1>([this, new_packet_number](TransmissionType type) {
                return RetransmitDataPacket(new_packet_number, type);
              }));
    } else {
      EXPECT_CALL(notifier_, OnFrameLost(_)).Times(1);
      is_lost = true;
    }
    QuicSentPacketManagerPeer::MarkForRetransmission(
        &manager_, old_packet_number, transmission_type);
    if (!is_lost) {
      return;
    }
    EXPECT_CALL(
        *send_algorithm_,
        OnPacketSent(_, BytesInFlight(), QuicPacketNumber(new_packet_number),
                     kDefaultLength, HAS_RETRANSMITTABLE_DATA));
    SerializedPacket packet(CreatePacket(new_packet_number, true));
    manager_.OnPacketSent(&packet, clock_.Now(), transmission_type,
                          HAS_RETRANSMITTABLE_DATA, true, ECN_NOT_ECT);
  }

  SerializedPacket CreateDataPacket(uint64_t packet_number) {
    return CreatePacket(packet_number, true);
  }

  SerializedPacket CreatePacket(uint64_t packet_number, bool retransmittable) {
    SerializedPacket packet(QuicPacketNumber(packet_number),
                            PACKET_4BYTE_PACKET_NUMBER, nullptr, kDefaultLength,
                            false, false);
    if (retransmittable) {
      packet.retransmittable_frames.push_back(
          QuicFrame(QuicStreamFrame(kStreamId, false, 0, absl::string_view())));
    }
    return packet;
  }

  SerializedPacket CreatePingPacket(uint64_t packet_number) {
    SerializedPacket packet(QuicPacketNumber(packet_number),
                            PACKET_4BYTE_PACKET_NUMBER, nullptr, kDefaultLength,
                            false, false);
    packet.retransmittable_frames.push_back(QuicFrame(QuicPingFrame()));
    return packet;
  }

  void SendDataPacket(uint64_t packet_number) {
    SendDataPacket(packet_number, ENCRYPTION_INITIAL, ECN_NOT_ECT);
  }

  void SendDataPacket(uint64_t packet_number,
                      EncryptionLevel encryption_level) {
    SendDataPacket(packet_number, encryption_level, ECN_NOT_ECT);
  }

  void SendDataPacket(uint64_t packet_number, EncryptionLevel encryption_level,
                      QuicEcnCodepoint ecn_codepoint) {
    EXPECT_CALL(*send_algorithm_,
                OnPacketSent(_, BytesInFlight(),
                             QuicPacketNumber(packet_number), _, _));
    SerializedPacket packet(CreateDataPacket(packet_number));
    packet.encryption_level = encryption_level;
    manager_.OnPacketSent(&packet, clock_.Now(), NOT_RETRANSMISSION,
                          HAS_RETRANSMITTABLE_DATA, true, ecn_codepoint);
  }

  void SendPingPacket(uint64_t packet_number,
                      EncryptionLevel encryption_level) {
    EXPECT_CALL(*send_algorithm_,
                OnPacketSent(_, BytesInFlight(),
                             QuicPacketNumber(packet_number), _, _));
    SerializedPacket packet(CreatePingPacket(packet_number));
    packet.encryption_level = encryption_level;
    manager_.OnPacketSent(&packet, clock_.Now(), NOT_RETRANSMISSION,
                          HAS_RETRANSMITTABLE_DATA, true, ECN_NOT_ECT);
  }

  void SendCryptoPacket(uint64_t packet_number) {
    EXPECT_CALL(
        *send_algorithm_,
        OnPacketSent(_, BytesInFlight(), QuicPacketNumber(packet_number),
                     kDefaultLength, HAS_RETRANSMITTABLE_DATA));
    SerializedPacket packet(CreatePacket(packet_number, false));
    packet.retransmittable_frames.push_back(
        QuicFrame(QuicStreamFrame(1, false, 0, absl::string_view())));
    packet.has_crypto_handshake = IS_HANDSHAKE;
    manager_.OnPacketSent(&packet, clock_.Now(), NOT_RETRANSMISSION,
                          HAS_RETRANSMITTABLE_DATA, true, ECN_NOT_ECT);
    EXPECT_CALL(notifier_, HasUnackedCryptoData()).WillRepeatedly(Return(true));
  }

  void SendAckPacket(uint64_t packet_number, uint64_t largest_acked) {
    SendAckPacket(packet_number, largest_acked, ENCRYPTION_INITIAL);
  }

  void SendAckPacket(uint64_t packet_number, uint64_t largest_acked,
                     EncryptionLevel level) {
    EXPECT_CALL(
        *send_algorithm_,
        OnPacketSent(_, BytesInFlight(), QuicPacketNumber(packet_number),
                     kDefaultLength, NO_RETRANSMITTABLE_DATA));
    SerializedPacket packet(CreatePacket(packet_number, false));
    packet.largest_acked = QuicPacketNumber(largest_acked);
    packet.encryption_level = level;
    manager_.OnPacketSent(&packet, clock_.Now(), NOT_RETRANSMISSION,
                          NO_RETRANSMITTABLE_DATA, true, ECN_NOT_ECT);
  }

  quiche::SimpleBufferAllocator allocator_;
  QuicSentPacketManager manager_;
  MockClock clock_;
  QuicConnectionStats stats_;
  MockSendAlgorithm* send_algorithm_;
  std::unique_ptr<MockNetworkChangeVisitor> network_change_visitor_;
  StrictMock<MockSessionNotifier> notifier_;
};

TEST_F(QuicSentPacketManagerTest, IsUnacked) {
  VerifyUnackedPackets(nullptr, 0);
  SendDataPacket(1);

  uint64_t unacked[] = {1};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  uint64_t retransmittable[] = {1};
  VerifyRetransmittablePackets(retransmittable,
                               ABSL_ARRAYSIZE(retransmittable));
}

TEST_F(QuicSentPacketManagerTest, IsUnAckedRetransmit) {
  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);

  EXPECT_TRUE(QuicSentPacketManagerPeer::IsRetransmission(&manager_, 2));
  uint64_t unacked[] = {1, 2};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  std::vector<uint64_t> retransmittable = {1, 2};
  VerifyRetransmittablePackets(&retransmittable[0], retransmittable.size());
}

TEST_F(QuicSentPacketManagerTest, RetransmitThenAck) {
  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);

  // Ack 2 but not 1.
  ExpectAck(2);
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  // Packet 1 is unacked, pending, but not retransmittable.
  uint64_t unacked[] = {1};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  EXPECT_TRUE(manager_.HasInFlightPackets());
  VerifyRetransmittablePackets(nullptr, 0);
}

TEST_F(QuicSentPacketManagerTest, RetransmitThenAckBeforeSend) {
  SendDataPacket(1);
  EXPECT_CALL(notifier_, RetransmitFrames(_, _))
      .WillOnce(WithArgs<1>([this](TransmissionType type) {
        return RetransmitDataPacket(2, type);
      }));
  QuicSentPacketManagerPeer::MarkForRetransmission(&manager_, 1,
                                                   PTO_RETRANSMISSION);
  // Ack 1.
  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));

  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  uint64_t unacked[] = {2};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  // We do not know packet 2 is a spurious retransmission until it gets acked.
  VerifyRetransmittablePackets(nullptr, 0);
  EXPECT_EQ(0u, stats_.packets_spuriously_retransmitted);
}

TEST_F(QuicSentPacketManagerTest, RetransmitThenStopRetransmittingBeforeSend) {
  SendDataPacket(1);
  EXPECT_CALL(notifier_, RetransmitFrames(_, _)).WillRepeatedly(Return(true));
  QuicSentPacketManagerPeer::MarkForRetransmission(&manager_, 1,
                                                   PTO_RETRANSMISSION);

  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));

  uint64_t unacked[] = {1};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyRetransmittablePackets(nullptr, 0);
  EXPECT_EQ(0u, stats_.packets_spuriously_retransmitted);
}

TEST_F(QuicSentPacketManagerTest, RetransmitThenAckPrevious) {
  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);
  QuicTime::Delta rtt = QuicTime::Delta::FromMilliseconds(15);
  clock_.AdvanceTime(rtt);

  // Ack 1 but not 2.
  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  // 2 remains unacked, but no packets have retransmittable data.
  uint64_t unacked[] = {2};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  EXPECT_TRUE(manager_.HasInFlightPackets());
  VerifyRetransmittablePackets(nullptr, 0);
  // Ack 2 causes 2 be considered as spurious retransmission.
  EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _)).WillOnce(Return(false));
  ExpectAck(2);
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(2),
                                   ENCRYPTION_INITIAL, kEmptyCounts));

  EXPECT_EQ(1u, stats_.packets_spuriously_retransmitted);
}

TEST_F(QuicSentPacketManagerTest, RetransmitThenAckPreviousThenNackRetransmit) {
  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);
  QuicTime::Delta rtt = QuicTime::Delta::FromMilliseconds(15);
  clock_.AdvanceTime(rtt);

  // First, ACK packet 1 which makes packet 2 non-retransmittable.
  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));

  SendDataPacket(3);
  SendDataPacket(4);
  SendDataPacket(5);
  clock_.AdvanceTime(rtt);

  // Next, NACK packet 2 three times.
  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(notifier_, OnFrameLost(_)).Times(1);
  ExpectAckAndLoss(true, 3, 2);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(4));
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(2),
                                   ENCRYPTION_INITIAL, kEmptyCounts));

  ExpectAck(4);
  manager_.OnAckFrameStart(QuicPacketNumber(4), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(5));
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(3),
                                   ENCRYPTION_INITIAL, kEmptyCounts));

  ExpectAck(5);
  manager_.OnAckFrameStart(QuicPacketNumber(5), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(6));
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(4),
                                   ENCRYPTION_INITIAL, kEmptyCounts));

  uint64_t unacked[] = {2};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  EXPECT_FALSE(manager_.HasInFlightPackets());
  VerifyRetransmittablePackets(nullptr, 0);

  // Verify that the retransmission alarm would not fire,
  // since there is no retransmittable data outstanding.
  EXPECT_EQ(QuicTime::Zero(), manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest,
       DISABLED_RetransmitTwiceThenAckPreviousBeforeSend) {
  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);

  // Fire the RTO, which will mark 2 for retransmission (but will not send it).
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.OnRetransmissionTimeout();

  // Ack 1 but not 2, before 2 is able to be sent.
  // Since 1 has been retransmitted, it has already been lost, and so the
  // send algorithm is not informed that it has been ACK'd.
  ExpectUpdatedRtt(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));

  // Since 2 was marked for retransmit, when 1 is acked, 2 is kept for RTT.
  uint64_t unacked[] = {2};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  EXPECT_FALSE(manager_.HasInFlightPackets());
  VerifyRetransmittablePackets(nullptr, 0);

  // Verify that the retransmission alarm would not fire,
  // since there is no retransmittable data outstanding.
  EXPECT_EQ(QuicTime::Zero(), manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, RetransmitTwiceThenAckFirst) {
  StrictMock<MockDebugDelegate> debug_delegate;
  EXPECT_CALL(debug_delegate, OnSpuriousPacketRetransmission(PTO_RETRANSMISSION,
                                                             kDefaultLength))
      .Times(1);
  manager_.SetDebugDelegate(&debug_delegate);

  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);
  RetransmitAndSendPacket(2, 3);
  QuicTime::Delta rtt = QuicTime::Delta::FromMilliseconds(15);
  clock_.AdvanceTime(rtt);

  // Ack 1 but not 2 or 3.
  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_CALL(debug_delegate, OnIncomingAck(_, _, _, _, _, _, _)).Times(1);
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  // Frames in packets 2 and 3 are acked.
  EXPECT_CALL(notifier_, IsFrameOutstanding(_))
      .Times(2)
      .WillRepeatedly(Return(false));

  // 2 and 3 remain unacked, but no packets have retransmittable data.
  uint64_t unacked[] = {2, 3};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  EXPECT_TRUE(manager_.HasInFlightPackets());
  VerifyRetransmittablePackets(nullptr, 0);

  // Ensure packet 2 is lost when 4 is sent and 3 and 4 are acked.
  SendDataPacket(4);
  // No new data gets acked in packet 3.
  EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _))
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  uint64_t acked[] = {3, 4};
  ExpectAcksAndLosses(true, acked, ABSL_ARRAYSIZE(acked), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(4), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(5));
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_CALL(debug_delegate, OnIncomingAck(_, _, _, _, _, _, _)).Times(1);
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(2),
                                   ENCRYPTION_INITIAL, kEmptyCounts));

  uint64_t unacked2[] = {2};
  VerifyUnackedPackets(unacked2, ABSL_ARRAYSIZE(unacked2));
  EXPECT_TRUE(manager_.HasInFlightPackets());

  SendDataPacket(5);
  ExpectAckAndLoss(true, 5, 2);
  EXPECT_CALL(debug_delegate,
              OnPacketLoss(QuicPacketNumber(2), _, LOSS_RETRANSMISSION, _));
  // Frames in all packets are acked.
  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  // Notify session that stream frame in packet 2 gets lost although it is
  // not outstanding.
  EXPECT_CALL(notifier_, OnFrameLost(_)).Times(1);
  manager_.OnAckFrameStart(QuicPacketNumber(5), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(6));
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_CALL(debug_delegate, OnIncomingAck(_, _, _, _, _, _, _)).Times(1);
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(3),
                                   ENCRYPTION_INITIAL, kEmptyCounts));

  uint64_t unacked3[] = {2};
  VerifyUnackedPackets(unacked3, ABSL_ARRAYSIZE(unacked3));
  EXPECT_FALSE(manager_.HasInFlightPackets());
  // Spurious retransmission is detected when packet 3 gets acked. We cannot
  // know packet 2 is a spurious until it gets acked.
  EXPECT_EQ(1u, stats_.packets_spuriously_retransmitted);
  EXPECT_EQ(1u, stats_.packets_lost);
  EXPECT_LT(0.0, stats_.total_loss_detection_response_time);
  EXPECT_LE(1u, stats_.sent_packets_max_sequence_reordering);
}

TEST_F(QuicSentPacketManagerTest, AckOriginalTransmission) {
  auto loss_algorithm = std::make_unique<MockLossAlgorithm>();
  QuicSentPacketManagerPeer::SetLossAlgorithm(&manager_, loss_algorithm.get());

  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);

  // Ack original transmission, but that wasn't lost via fast retransmit,
  // so no call on OnSpuriousRetransmission is expected.
  {
    ExpectAck(1);
    EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _));
    manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                             clock_.Now());
    manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
    EXPECT_EQ(PACKETS_NEWLY_ACKED,
              manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                     ENCRYPTION_INITIAL, kEmptyCounts));
  }

  SendDataPacket(3);
  SendDataPacket(4);
  // Ack 4, which causes 3 to be retransmitted.
  {
    ExpectAck(4);
    EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _));
    manager_.OnAckFrameStart(QuicPacketNumber(4), QuicTime::Delta::Infinite(),
                             clock_.Now());
    manager_.OnAckRange(QuicPacketNumber(4), QuicPacketNumber(5));
    manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
    EXPECT_EQ(PACKETS_NEWLY_ACKED,
              manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(2),
                                     ENCRYPTION_INITIAL, kEmptyCounts));
    RetransmitAndSendPacket(3, 5, LOSS_RETRANSMISSION);
  }

  // Ack 3, which causes SpuriousRetransmitDetected to be called.
  {
    uint64_t acked[] = {3};
    ExpectAcksAndLosses(false, acked, ABSL_ARRAYSIZE(acked), nullptr, 0);
    EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _));
    EXPECT_CALL(*loss_algorithm,
                SpuriousLossDetected(_, _, _, QuicPacketNumber(3),
                                     QuicPacketNumber(4)));
    manager_.OnAckFrameStart(QuicPacketNumber(4), QuicTime::Delta::Infinite(),
                             clock_.Now());
    manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(5));
    manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
    EXPECT_EQ(0u, stats_.packet_spuriously_detected_lost);
    EXPECT_EQ(PACKETS_NEWLY_ACKED,
              manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(3),
                                     ENCRYPTION_INITIAL, kEmptyCounts));
    EXPECT_EQ(1u, stats_.packet_spuriously_detected_lost);
    // Ack 3 will not cause 5 be considered as a spurious retransmission. Ack
    // 5 will cause 5 be considered as a spurious retransmission as no new
    // data gets acked.
    ExpectAck(5);
    EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _));
    EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _)).WillOnce(Return(false));
    manager_.OnAckFrameStart(QuicPacketNumber(5), QuicTime::Delta::Infinite(),
                             clock_.Now());
    manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(6));
    manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
    EXPECT_EQ(PACKETS_NEWLY_ACKED,
              manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(4),
                                     ENCRYPTION_INITIAL, kEmptyCounts));
  }
}

TEST_F(QuicSentPacketManagerTest, GetLeastUnacked) {
  EXPECT_EQ(QuicPacketNumber(1u), manager_.GetLeastUnacked());
}

TEST_F(QuicSentPacketManagerTest, GetLeastUnackedUnacked) {
  SendDataPacket(1);
  EXPECT_EQ(QuicPacketNumber(1u), manager_.GetLeastUnacked());
}

TEST_F(QuicSentPacketManagerTest, AckAckAndUpdateRtt) {
  EXPECT_FALSE(manager_.GetLargestPacketPeerKnowsIsAcked(ENCRYPTION_INITIAL)
                   .IsInitialized());
  SendDataPacket(1);
  SendAckPacket(2, 1);

  // Now ack the ack and expect an RTT update.
  uint64_t acked[] = {1, 2};
  ExpectAcksAndLosses(true, acked, ABSL_ARRAYSIZE(acked), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(2),
                           QuicTime::Delta::FromMilliseconds(5), clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  EXPECT_EQ(QuicPacketNumber(1),
            manager_.GetLargestPacketPeerKnowsIsAcked(ENCRYPTION_INITIAL));

  SendAckPacket(3, 3);

  // Now ack the ack and expect only an RTT update.
  uint64_t acked2[] = {3};
  ExpectAcksAndLosses(true, acked2, ABSL_ARRAYSIZE(acked2), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(4));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(2),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  EXPECT_EQ(QuicPacketNumber(3u),
            manager_.GetLargestPacketPeerKnowsIsAcked(ENCRYPTION_INITIAL));
}

TEST_F(QuicSentPacketManagerTest, Rtt) {
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(20);
  SendDataPacket(1);
  clock_.AdvanceTime(expected_rtt);

  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  EXPECT_EQ(expected_rtt, manager_.GetRttStats()->latest_rtt());
}

TEST_F(QuicSentPacketManagerTest, RttWithInvalidDelta) {
  // Expect that the RTT is equal to the local time elapsed, since the
  // ack_delay_time is larger than the local time elapsed
  // and is hence invalid.
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(10);
  SendDataPacket(1);
  clock_.AdvanceTime(expected_rtt);

  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1),
                           QuicTime::Delta::FromMilliseconds(11), clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  EXPECT_EQ(expected_rtt, manager_.GetRttStats()->latest_rtt());
}

TEST_F(QuicSentPacketManagerTest, RttWithInfiniteDelta) {
  // Expect that the RTT is equal to the local time elapsed, since the
  // ack_delay_time is infinite, and is hence invalid.
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(10);
  SendDataPacket(1);
  clock_.AdvanceTime(expected_rtt);

  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  EXPECT_EQ(expected_rtt, manager_.GetRttStats()->latest_rtt());
}

TEST_F(QuicSentPacketManagerTest, RttWithDeltaExceedingLimit) {
  // Initialize min and smoothed rtt to 10ms.
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(10),
                       QuicTime::Delta::Zero(), QuicTime::Zero());

  QuicTime::Delta send_delta = QuicTime::Delta::FromMilliseconds(100);
  QuicTime::Delta ack_delay =
      QuicTime::Delta::FromMilliseconds(5) + manager_.peer_max_ack_delay();
  ASSERT_GT(send_delta - rtt_stats->min_rtt(), ack_delay);
  SendDataPacket(1);
  clock_.AdvanceTime(send_delta);

  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), ack_delay, clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_FORWARD_SECURE, kEmptyCounts));

  QuicTime::Delta expected_rtt_sample =
      send_delta - manager_.peer_max_ack_delay();
  EXPECT_EQ(expected_rtt_sample, manager_.GetRttStats()->latest_rtt());
}

TEST_F(QuicSentPacketManagerTest, RttZeroDelta) {
  // Expect that the RTT is the time between send and receive since the
  // ack_delay_time is zero.
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(10);
  SendDataPacket(1);
  clock_.AdvanceTime(expected_rtt);

  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Zero(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  EXPECT_EQ(expected_rtt, manager_.GetRttStats()->latest_rtt());
}

TEST_F(QuicSentPacketManagerTest, CryptoHandshakeTimeout) {
  // Send 2 crypto packets and 3 data packets.
  const size_t kNumSentCryptoPackets = 2;
  for (size_t i = 1; i <= kNumSentCryptoPackets; ++i) {
    SendCryptoPacket(i);
  }
  const size_t kNumSentDataPackets = 3;
  for (size_t i = 1; i <= kNumSentDataPackets; ++i) {
    SendDataPacket(kNumSentCryptoPackets + i);
  }
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());
  EXPECT_EQ(5 * kDefaultLength, manager_.GetBytesInFlight());

  // The first retransmits 2 packets.
  EXPECT_CALL(notifier_, RetransmitFrames(_, _))
      .Times(2)
      .WillOnce(
          InvokeWithoutArgs([this]() { return RetransmitCryptoPacket(6); }))
      .WillOnce(
          InvokeWithoutArgs([this]() { return RetransmitCryptoPacket(7); }));
  manager_.OnRetransmissionTimeout();
  // Expect all 4 handshake packets to be in flight and 3 data packets.
  EXPECT_EQ(7 * kDefaultLength, manager_.GetBytesInFlight());
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());

  // The second retransmits 2 packets.
  EXPECT_CALL(notifier_, RetransmitFrames(_, _))
      .Times(2)
      .WillOnce(
          InvokeWithoutArgs([this]() { return RetransmitCryptoPacket(8); }))
      .WillOnce(
          InvokeWithoutArgs([this]() { return RetransmitCryptoPacket(9); }));
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(9 * kDefaultLength, manager_.GetBytesInFlight());
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());

  // Now ack the two crypto packets and the speculatively encrypted request,
  // and ensure the first four crypto packets get abandoned, but not lost.
  // Crypto packets remain in flight, so any that aren't acked will be lost.
  uint64_t acked[] = {3, 4, 5, 8, 9};
  uint64_t lost[] = {1, 2, 6};
  ExpectAcksAndLosses(true, acked, ABSL_ARRAYSIZE(acked), lost,
                      ABSL_ARRAYSIZE(lost));
  EXPECT_CALL(notifier_, OnFrameLost(_)).Times(3);
  EXPECT_CALL(notifier_, HasUnackedCryptoData()).WillRepeatedly(Return(false));
  manager_.OnAckFrameStart(QuicPacketNumber(9), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(8), QuicPacketNumber(10));
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(6));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));

  EXPECT_FALSE(manager_.HasUnackedCryptoPackets());
}

TEST_F(QuicSentPacketManagerTest, CryptoHandshakeSpuriousRetransmission) {
  // Send 1 crypto packet.
  SendCryptoPacket(1);
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());

  // Retransmit the crypto packet as 2.
  EXPECT_CALL(notifier_, RetransmitFrames(_, _))
      .WillOnce(
          InvokeWithoutArgs([this]() { return RetransmitCryptoPacket(2); }));
  manager_.OnRetransmissionTimeout();

  // Retransmit the crypto packet as 3.
  EXPECT_CALL(notifier_, RetransmitFrames(_, _))
      .WillOnce(
          InvokeWithoutArgs([this]() { return RetransmitCryptoPacket(3); }));
  manager_.OnRetransmissionTimeout();

  // Now ack the second crypto packet, and ensure the first gets removed, but
  // the third does not.
  uint64_t acked[] = {2};
  ExpectAcksAndLosses(true, acked, ABSL_ARRAYSIZE(acked), nullptr, 0);
  EXPECT_CALL(notifier_, HasUnackedCryptoData()).WillRepeatedly(Return(false));
  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));

  EXPECT_FALSE(manager_.HasUnackedCryptoPackets());
  uint64_t unacked[] = {1, 3};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
}

TEST_F(QuicSentPacketManagerTest, CryptoHandshakeTimeoutUnsentDataPacket) {
  // Send 2 crypto packets and 1 data packet.
  const size_t kNumSentCryptoPackets = 2;
  for (size_t i = 1; i <= kNumSentCryptoPackets; ++i) {
    SendCryptoPacket(i);
  }
  SendDataPacket(3);
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());

  // Retransmit 2 crypto packets, but not the serialized packet.
  EXPECT_CALL(notifier_, RetransmitFrames(_, _))
      .Times(2)
      .WillOnce(
          InvokeWithoutArgs([this]() { return RetransmitCryptoPacket(4); }))
      .WillOnce(
          InvokeWithoutArgs([this]() { return RetransmitCryptoPacket(5); }));
  manager_.OnRetransmissionTimeout();
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());
}

TEST_F(QuicSentPacketManagerTest,
       CryptoHandshakeRetransmissionThenNeuterAndAck) {
  // Send 1 crypto packet.
  SendCryptoPacket(1);

  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());

  // Retransmit the crypto packet as 2.
  EXPECT_CALL(notifier_, RetransmitFrames(_, _))
      .WillOnce(
          InvokeWithoutArgs([this]() { return RetransmitCryptoPacket(2); }));
  manager_.OnRetransmissionTimeout();
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());

  // Retransmit the crypto packet as 3.
  EXPECT_CALL(notifier_, RetransmitFrames(_, _))
      .WillOnce(
          InvokeWithoutArgs([this]() { return RetransmitCryptoPacket(3); }));
  manager_.OnRetransmissionTimeout();
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());

  // Now neuter all unacked unencrypted packets, which occurs when the
  // connection goes forward secure.
  EXPECT_CALL(notifier_, HasUnackedCryptoData()).WillRepeatedly(Return(false));
  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  manager_.NeuterUnencryptedPackets();
  EXPECT_FALSE(manager_.HasUnackedCryptoPackets());
  uint64_t unacked[] = {1, 2, 3};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyRetransmittablePackets(nullptr, 0);
  EXPECT_FALSE(manager_.HasUnackedCryptoPackets());
  EXPECT_FALSE(manager_.HasInFlightPackets());

  // Ensure both packets get discarded when packet 2 is acked.
  uint64_t acked[] = {3};
  ExpectAcksAndLosses(true, acked, ABSL_ARRAYSIZE(acked), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(4));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  VerifyUnackedPackets(nullptr, 0);
  VerifyRetransmittablePackets(nullptr, 0);
}

TEST_F(QuicSentPacketManagerTest, GetTransmissionTime) {
  EXPECT_EQ(QuicTime::Zero(), manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, GetTransmissionTimeCryptoHandshake) {
  QuicTime crypto_packet_send_time = clock_.Now();
  SendCryptoPacket(1);

  // Check the min.
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->set_initial_rtt(QuicTime::Delta::FromMilliseconds(1));
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromMilliseconds(10),
            manager_.GetRetransmissionTime());

  // Test with a standard smoothed RTT.
  rtt_stats->set_initial_rtt(QuicTime::Delta::FromMilliseconds(100));

  QuicTime::Delta srtt = rtt_stats->initial_rtt();
  QuicTime expected_time = clock_.Now() + 1.5 * srtt;
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());

  // Retransmit the packet by invoking the retransmission timeout.
  clock_.AdvanceTime(1.5 * srtt);
  EXPECT_CALL(notifier_, RetransmitFrames(_, _))
      .WillOnce(
          InvokeWithoutArgs([this]() { return RetransmitCryptoPacket(2); }));
  // When session decides what to write, crypto_packet_send_time gets updated.
  crypto_packet_send_time = clock_.Now();
  manager_.OnRetransmissionTimeout();

  // The retransmission time should now be twice as far in the future.
  expected_time = crypto_packet_send_time + srtt * 2 * 1.5;
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());

  // Retransmit the packet for the 2nd time.
  clock_.AdvanceTime(2 * 1.5 * srtt);
  EXPECT_CALL(notifier_, RetransmitFrames(_, _))
      .WillOnce(
          InvokeWithoutArgs([this]() { return RetransmitCryptoPacket(3); }));
  // When session decides what to write, crypto_packet_send_time gets updated.
  crypto_packet_send_time = clock_.Now();
  manager_.OnRetransmissionTimeout();

  // Verify exponential backoff of the retransmission timeout.
  expected_time = crypto_packet_send_time + srtt * 4 * 1.5;
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest,
       GetConservativeTransmissionTimeCryptoHandshake) {
  QuicConfig config;
  QuicTagVector options;
  options.push_back(kCONH);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  // Calling SetFromConfig requires mocking out some send algorithm methods.
  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(10 * kDefaultTCPMSS));

  QuicTime crypto_packet_send_time = clock_.Now();
  SendCryptoPacket(1);

  // Check the min.
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->set_initial_rtt(QuicTime::Delta::FromMilliseconds(1));
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromMilliseconds(25),
            manager_.GetRetransmissionTime());

  // Test with a standard smoothed RTT.
  rtt_stats->set_initial_rtt(QuicTime::Delta::FromMilliseconds(100));

  QuicTime::Delta srtt = rtt_stats->initial_rtt();
  QuicTime expected_time = clock_.Now() + 2 * srtt;
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());

  // Retransmit the packet by invoking the retransmission timeout.
  clock_.AdvanceTime(2 * srtt);
  EXPECT_CALL(notifier_, RetransmitFrames(_, _))
      .WillOnce(
          InvokeWithoutArgs([this]() { return RetransmitCryptoPacket(2); }));
  crypto_packet_send_time = clock_.Now();
  manager_.OnRetransmissionTimeout();

  // The retransmission time should now be twice as far in the future.
  expected_time = crypto_packet_send_time + srtt * 2 * 2;
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, GetLossDelay) {
  auto loss_algorithm = std::make_unique<MockLossAlgorithm>();
  QuicSentPacketManagerPeer::SetLossAlgorithm(&manager_, loss_algorithm.get());

  EXPECT_CALL(*loss_algorithm, GetLossTimeout())
      .WillRepeatedly(Return(QuicTime::Zero()));
  SendDataPacket(1);
  SendDataPacket(2);

  // Handle an ack which causes the loss algorithm to be evaluated and
  // set the loss timeout.
  ExpectAck(2);
  EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _));
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));

  QuicTime timeout(clock_.Now() + QuicTime::Delta::FromMilliseconds(10));
  EXPECT_CALL(*loss_algorithm, GetLossTimeout())
      .WillRepeatedly(Return(timeout));
  EXPECT_EQ(timeout, manager_.GetRetransmissionTime());

  // Fire the retransmission timeout and ensure the loss detection algorithm
  // is invoked.
  EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _));
  manager_.OnRetransmissionTimeout();
}

TEST_F(QuicSentPacketManagerTest, NegotiateIetfLossDetectionFromOptions) {
  EXPECT_TRUE(
      QuicSentPacketManagerPeer::AdaptiveReorderingThresholdEnabled(&manager_));
  EXPECT_FALSE(
      QuicSentPacketManagerPeer::AdaptiveTimeThresholdEnabled(&manager_));
  EXPECT_EQ(kDefaultLossDelayShift,
            QuicSentPacketManagerPeer::GetReorderingShift(&manager_));

  QuicConfig config;
  QuicTagVector options;
  options.push_back(kILD0);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);

  EXPECT_EQ(3, QuicSentPacketManagerPeer::GetReorderingShift(&manager_));
  EXPECT_FALSE(
      QuicSentPacketManagerPeer::AdaptiveReorderingThresholdEnabled(&manager_));
}

TEST_F(QuicSentPacketManagerTest,
       NegotiateIetfLossDetectionOneFourthRttFromOptions) {
  EXPECT_TRUE(
      QuicSentPacketManagerPeer::AdaptiveReorderingThresholdEnabled(&manager_));
  EXPECT_FALSE(
      QuicSentPacketManagerPeer::AdaptiveTimeThresholdEnabled(&manager_));
  EXPECT_EQ(kDefaultLossDelayShift,
            QuicSentPacketManagerPeer::GetReorderingShift(&manager_));

  QuicConfig config;
  QuicTagVector options;
  options.push_back(kILD1);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);

  EXPECT_EQ(kDefaultLossDelayShift,
            QuicSentPacketManagerPeer::GetReorderingShift(&manager_));
  EXPECT_FALSE(
      QuicSentPacketManagerPeer::AdaptiveReorderingThresholdEnabled(&manager_));
}

TEST_F(QuicSentPacketManagerTest,
       NegotiateIetfLossDetectionAdaptiveReorderingThreshold) {
  EXPECT_TRUE(
      QuicSentPacketManagerPeer::AdaptiveReorderingThresholdEnabled(&manager_));
  EXPECT_FALSE(
      QuicSentPacketManagerPeer::AdaptiveTimeThresholdEnabled(&manager_));
  EXPECT_EQ(kDefaultLossDelayShift,
            QuicSentPacketManagerPeer::GetReorderingShift(&manager_));

  QuicConfig config;
  QuicTagVector options;
  options.push_back(kILD2);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);

  EXPECT_EQ(3, QuicSentPacketManagerPeer::GetReorderingShift(&manager_));
  EXPECT_TRUE(
      QuicSentPacketManagerPeer::AdaptiveReorderingThresholdEnabled(&manager_));
}

TEST_F(QuicSentPacketManagerTest,
       NegotiateIetfLossDetectionAdaptiveReorderingThreshold2) {
  EXPECT_TRUE(
      QuicSentPacketManagerPeer::AdaptiveReorderingThresholdEnabled(&manager_));
  EXPECT_FALSE(
      QuicSentPacketManagerPeer::AdaptiveTimeThresholdEnabled(&manager_));
  EXPECT_EQ(kDefaultLossDelayShift,
            QuicSentPacketManagerPeer::GetReorderingShift(&manager_));

  QuicConfig config;
  QuicTagVector options;
  options.push_back(kILD3);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kDefaultLossDelayShift,
            QuicSentPacketManagerPeer::GetReorderingShift(&manager_));
  EXPECT_TRUE(
      QuicSentPacketManagerPeer::AdaptiveReorderingThresholdEnabled(&manager_));
}

TEST_F(QuicSentPacketManagerTest,
       NegotiateIetfLossDetectionAdaptiveReorderingAndTimeThreshold) {
  EXPECT_TRUE(
      QuicSentPacketManagerPeer::AdaptiveReorderingThresholdEnabled(&manager_));
  EXPECT_FALSE(
      QuicSentPacketManagerPeer::AdaptiveTimeThresholdEnabled(&manager_));
  EXPECT_EQ(kDefaultLossDelayShift,
            QuicSentPacketManagerPeer::GetReorderingShift(&manager_));

  QuicConfig config;
  QuicTagVector options;
  options.push_back(kILD4);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);

  EXPECT_EQ(kDefaultLossDelayShift,
            QuicSentPacketManagerPeer::GetReorderingShift(&manager_));
  EXPECT_TRUE(
      QuicSentPacketManagerPeer::AdaptiveReorderingThresholdEnabled(&manager_));
  EXPECT_TRUE(
      QuicSentPacketManagerPeer::AdaptiveTimeThresholdEnabled(&manager_));
}

TEST_F(QuicSentPacketManagerTest, NegotiateCongestionControlFromOptions) {
  QuicConfig config;
  QuicTagVector options;

  options.push_back(kRENO);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kRenoBytes, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                            ->GetCongestionControlType());

  options.clear();
  options.push_back(kTBBR);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kBBR, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                      ->GetCongestionControlType());

  options.clear();
  options.push_back(kBYTE);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kCubicBytes, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                             ->GetCongestionControlType());
  options.clear();
  options.push_back(kRENO);
  options.push_back(kBYTE);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kRenoBytes, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                            ->GetCongestionControlType());

  options.clear();
  options.push_back(kPRGC);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  // The server does nothing on kPRGC.
  EXPECT_EQ(kRenoBytes, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                            ->GetCongestionControlType());

  options.clear();
  options.push_back(kCQBC);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  // The server does nothing on kCQBC.
  EXPECT_EQ(kRenoBytes, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                            ->GetCongestionControlType());
}

TEST_F(QuicSentPacketManagerTest, NegotiateClientCongestionControlFromOptions) {
  QuicConfig config;
  QuicTagVector options;

  // No change if the server receives client options.
  const SendAlgorithmInterface* mock_sender =
      QuicSentPacketManagerPeer::GetSendAlgorithm(manager_);
  options.push_back(kRENO);
  config.SetClientConnectionOptions(options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(mock_sender, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_));

  // Change the congestion control on the client with client options.
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kRenoBytes, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                            ->GetCongestionControlType());

  options.clear();
  options.push_back(kTBBR);
  config.SetClientConnectionOptions(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kBBR, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                      ->GetCongestionControlType());

  options.clear();
  options.push_back(kBYTE);
  config.SetClientConnectionOptions(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kCubicBytes, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                             ->GetCongestionControlType());

  options.clear();
  options.push_back(kRENO);
  options.push_back(kBYTE);
  config.SetClientConnectionOptions(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kRenoBytes, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                            ->GetCongestionControlType());

  // Prague Cubic is currently only supported on the client.
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_SERVER);
  options.clear();
  options.push_back(kPRGC);
  config.SetClientConnectionOptions(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  // This is the server, so the algorithm didn't change.
  EXPECT_EQ(kRenoBytes, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                            ->GetCongestionControlType());

  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  options.clear();
  options.push_back(kPRGC);
  config.SetClientConnectionOptions(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kPragueCubic, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                              ->GetCongestionControlType());

  options.clear();
  options.push_back(kCQBC);
  config.SetClientConnectionOptions(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kCubicBytes, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                             ->GetCongestionControlType());

  // Test that kPRGC is not overridden by other options.
  options.clear();
  options.push_back(kPRGC);
  options.push_back(kTBBR);
  config.SetClientConnectionOptions(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kPragueCubic, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                              ->GetCongestionControlType());

  // Test that kCQBC is not overridden by other options.
  options.clear();
  options.push_back(kCQBC);
  options.push_back(kTBBR);
  config.SetClientConnectionOptions(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kCubicBytes, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                             ->GetCongestionControlType());
}

TEST_F(QuicSentPacketManagerTest, UseInitialRoundTripTimeToSend) {
  QuicTime::Delta initial_rtt = QuicTime::Delta::FromMilliseconds(325);
  EXPECT_NE(initial_rtt, manager_.GetRttStats()->smoothed_rtt());

  QuicConfig config;
  config.SetInitialRoundTripTimeUsToSend(initial_rtt.ToMicroseconds());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);

  EXPECT_EQ(QuicTime::Delta::Zero(), manager_.GetRttStats()->smoothed_rtt());
  EXPECT_EQ(initial_rtt, manager_.GetRttStats()->initial_rtt());
}

TEST_F(QuicSentPacketManagerTest, ResumeConnectionState) {
  // The sent packet manager should use the RTT from CachedNetworkParameters if
  // it is provided.
  const QuicTime::Delta kRtt = QuicTime::Delta::FromMilliseconds(123);
  CachedNetworkParameters cached_network_params;
  cached_network_params.set_min_rtt_ms(kRtt.ToMilliseconds());

  SendAlgorithmInterface::NetworkParams params;
  params.bandwidth = QuicBandwidth::Zero();
  params.allow_cwnd_to_decrease = false;
  params.rtt = kRtt;
  params.is_rtt_trusted = true;

  EXPECT_CALL(*send_algorithm_, AdjustNetworkParameters(params));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .Times(testing::AnyNumber());
  manager_.ResumeConnectionState(cached_network_params, false);
  EXPECT_EQ(kRtt, manager_.GetRttStats()->initial_rtt());
}

TEST_F(QuicSentPacketManagerTest, ConnectionMigrationUnspecifiedChange) {
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  QuicTime::Delta default_init_rtt = rtt_stats->initial_rtt();
  rtt_stats->set_initial_rtt(default_init_rtt * 2);
  EXPECT_EQ(2 * default_init_rtt, rtt_stats->initial_rtt());

  QuicSentPacketManagerPeer::SetConsecutivePtoCount(&manager_, 1);
  EXPECT_EQ(1u, manager_.GetConsecutivePtoCount());

  EXPECT_CALL(*send_algorithm_, OnConnectionMigration());
  EXPECT_EQ(nullptr,
            manager_.OnConnectionMigration(/*reset_send_algorithm=*/false));

  EXPECT_EQ(default_init_rtt, rtt_stats->initial_rtt());
  EXPECT_EQ(0u, manager_.GetConsecutivePtoCount());
}

TEST_F(QuicSentPacketManagerTest,
       NoInflightBytesAfterConnectionMigrationWithResetSendAlgorithm) {
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, 0u, QuicPacketNumber(1), _, _))
      .Times(1);

  SerializedPacket packet(QuicPacketNumber(1), PACKET_4BYTE_PACKET_NUMBER,
                          nullptr, kDefaultLength, false, false);
  manager_.OnPacketSent(&packet, clock_.Now(), NOT_RETRANSMISSION,
                        HAS_RETRANSMITTABLE_DATA, true, ECN_NOT_ECT);
  EXPECT_EQ(BytesInFlight(), kDefaultLength);

  if (GetQuicReloadableFlag(quic_neuter_packets_on_migration)) {
    EXPECT_CALL(*send_algorithm_, OnPacketNeutered(QuicPacketNumber(1)))
        .Times(1);
  } else {
    EXPECT_CALL(*send_algorithm_, OnPacketNeutered(QuicPacketNumber(1)))
        .Times(0);
  }

  std::unique_ptr<SendAlgorithmInterface> old_send_algorithm =
      manager_.OnConnectionMigration(/*reset_send_algorithm=*/true);
  EXPECT_EQ(old_send_algorithm.get(), send_algorithm_);
  EXPECT_EQ(BytesInFlight(), 0u);
}

// Regression test for b/323150773.
TEST_F(QuicSentPacketManagerTest,
       NoInflightBytesAfterConnectionMigrationWithResetBBR2Sender) {
  if (!GetQuicReloadableFlag(quic_neuter_packets_on_migration)) {
    return;
  }
  manager_.SetSendAlgorithm(CongestionControlType::kBBRv2);

  SerializedPacket packet(QuicPacketNumber(1), PACKET_4BYTE_PACKET_NUMBER,
                          nullptr, kDefaultLength, false, false);
  packet.encryption_level = ENCRYPTION_FORWARD_SECURE;
  manager_.OnPacketSent(&packet, clock_.Now(), NOT_RETRANSMISSION,
                        HAS_RETRANSMITTABLE_DATA, true, ECN_NOT_ECT);
  EXPECT_EQ(BytesInFlight(), kDefaultLength);

  std::unique_ptr<SendAlgorithmInterface> old_send_algorithm =
      manager_.OnConnectionMigration(/*reset_send_algorithm=*/true);
  EXPECT_EQ(BytesInFlight(), 0u);

  // Restore the old send algorithm and receive an ack for packet 1.
  manager_.SetSendAlgorithm(old_send_algorithm.release());

  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());

  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_FORWARD_SECURE, kEmptyCounts));
}

// Tests that ResetCongestionControlUponPeerAddressChange() resets send
// algorithm and RTT. And unACK'ed packets are handled correctly.
TEST_F(QuicSentPacketManagerTest,
       ConnectionMigrationUnspecifiedChangeResetSendAlgorithm) {
  auto loss_algorithm = std::make_unique<MockLossAlgorithm>();
  QuicSentPacketManagerPeer::SetLossAlgorithm(&manager_, loss_algorithm.get());

  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  QuicTime::Delta default_init_rtt = rtt_stats->initial_rtt();
  rtt_stats->set_initial_rtt(default_init_rtt * 2);
  EXPECT_EQ(2 * default_init_rtt, rtt_stats->initial_rtt());

  QuicSentPacketManagerPeer::SetConsecutivePtoCount(&manager_, 1);
  EXPECT_EQ(1u, manager_.GetConsecutivePtoCount());

  SendDataPacket(1, ENCRYPTION_FORWARD_SECURE);

  RttStats old_rtt_stats;
  old_rtt_stats.CloneFrom(*manager_.GetRttStats());

  // Packet1 will be mark for retransmission upon migration.
  EXPECT_CALL(notifier_, OnFrameLost(_));
  std::unique_ptr<SendAlgorithmInterface> old_send_algorithm =
      manager_.OnConnectionMigration(/*reset_send_algorithm=*/true);

  EXPECT_NE(old_send_algorithm.get(), manager_.GetSendAlgorithm());
  EXPECT_EQ(old_send_algorithm->GetCongestionControlType(),
            manager_.GetSendAlgorithm()->GetCongestionControlType());
  EXPECT_EQ(default_init_rtt, rtt_stats->initial_rtt());
  EXPECT_EQ(0u, manager_.GetConsecutivePtoCount());
  // Packets sent earlier shouldn't be regarded as in flight.
  EXPECT_EQ(0u, BytesInFlight());

  // Replace the new send algorithm with the mock object.
  manager_.SetSendAlgorithm(old_send_algorithm.release());

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  // Application retransmit the data as LOSS_RETRANSMISSION.
  RetransmitDataPacket(2, LOSS_RETRANSMISSION, ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kDefaultLength, BytesInFlight());

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  // Receiving an ACK for packet1 20s later shouldn't update the RTT, and
  // shouldn't be treated as spurious retransmission.
  EXPECT_CALL(
      *send_algorithm_,
      OnCongestionEvent(/*rtt_updated=*/false, kDefaultLength, _, _, _, _, _))
      .WillOnce(testing::WithArg<3>([](const AckedPacketVector& acked_packets) {
        EXPECT_EQ(1u, acked_packets.size());
        EXPECT_EQ(QuicPacketNumber(1), acked_packets[0].packet_number);
        // The bytes in packet1 shouldn't contribute to congestion control.
        EXPECT_EQ(0u, acked_packets[0].bytes_acked);
      }));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*loss_algorithm, SpuriousLossDetected(_, _, _, _, _)).Times(0u);
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_FORWARD_SECURE, kEmptyCounts));
  EXPECT_TRUE(manager_.GetRttStats()->latest_rtt().IsZero());

  // Receiving an ACK for packet2 should update RTT and congestion control.
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(3));
  EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(
      *send_algorithm_,
      OnCongestionEvent(/*rtt_updated=*/true, kDefaultLength, _, _, _, _, _))
      .WillOnce(testing::WithArg<3>([](const AckedPacketVector& acked_packets) {
        EXPECT_EQ(1u, acked_packets.size());
        EXPECT_EQ(QuicPacketNumber(2), acked_packets[0].packet_number);
        // The bytes in packet2 should contribute to congestion control.
        EXPECT_EQ(kDefaultLength, acked_packets[0].bytes_acked);
      }));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(2),
                                   ENCRYPTION_FORWARD_SECURE, kEmptyCounts));
  EXPECT_EQ(0u, BytesInFlight());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10),
            manager_.GetRttStats()->latest_rtt());

  SendDataPacket(3, ENCRYPTION_FORWARD_SECURE);
  // Trigger loss timeout and mark packet3 for retransmission.
  EXPECT_CALL(*loss_algorithm, GetLossTimeout())
      .WillOnce(Return(clock_.Now() + QuicTime::Delta::FromMilliseconds(10)));
  EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _))
      .WillOnce(WithArgs<5>([](LostPacketVector* packet_lost) {
        packet_lost->emplace_back(QuicPacketNumber(3u), kDefaultLength);
        return LossDetectionInterface::DetectionStats();
      }));
  EXPECT_CALL(notifier_, OnFrameLost(_));
  EXPECT_CALL(*send_algorithm_,
              OnCongestionEvent(false, kDefaultLength, _, _, _, _, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(0u, BytesInFlight());

  // Migrate again with unACK'ed but not in-flight packet.
  // Packet3 shouldn't be marked for retransmission again as it is not in
  // flight.
  old_send_algorithm =
      manager_.OnConnectionMigration(/*reset_send_algorithm=*/true);

  EXPECT_NE(old_send_algorithm.get(), manager_.GetSendAlgorithm());
  EXPECT_EQ(old_send_algorithm->GetCongestionControlType(),
            manager_.GetSendAlgorithm()->GetCongestionControlType());
  EXPECT_EQ(default_init_rtt, rtt_stats->initial_rtt());
  EXPECT_EQ(0u, manager_.GetConsecutivePtoCount());
  EXPECT_EQ(0u, BytesInFlight());
  EXPECT_TRUE(manager_.GetRttStats()->latest_rtt().IsZero());

  manager_.SetSendAlgorithm(old_send_algorithm.release());

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(30));
  // Receiving an ACK for packet3 shouldn't update RTT. Though packet 3 was
  // marked lost, this spurious retransmission shouldn't be reported to the loss
  // algorithm.
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(4));
  EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*loss_algorithm, SpuriousLossDetected(_, _, _, _, _)).Times(0u);
  EXPECT_CALL(*send_algorithm_,
              OnCongestionEvent(/*rtt_updated=*/false, 0, _, _, _, _, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(3),
                                   ENCRYPTION_FORWARD_SECURE, kEmptyCounts));
  EXPECT_EQ(0u, BytesInFlight());
  EXPECT_TRUE(manager_.GetRttStats()->latest_rtt().IsZero());

  SendDataPacket(4, ENCRYPTION_FORWARD_SECURE);
  // Trigger loss timeout and mark packet4 for retransmission.
  EXPECT_CALL(*loss_algorithm, GetLossTimeout())
      .WillOnce(Return(clock_.Now() + QuicTime::Delta::FromMilliseconds(10)));
  EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _))
      .WillOnce(WithArgs<5>([](LostPacketVector* packet_lost) {
        packet_lost->emplace_back(QuicPacketNumber(4u), kDefaultLength);
        return LossDetectionInterface::DetectionStats();
      }));
  EXPECT_CALL(notifier_, OnFrameLost(_));
  EXPECT_CALL(*send_algorithm_,
              OnCongestionEvent(false, kDefaultLength, _, _, _, _, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(0u, BytesInFlight());

  // Application retransmit the data as LOSS_RETRANSMISSION.
  RetransmitDataPacket(5, LOSS_RETRANSMISSION, ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kDefaultLength, BytesInFlight());

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(30));
  // Receiving an ACK for packet4 should update RTT, but not bytes in flight.
  // This spurious retransmission should be reported to the loss algorithm.
  manager_.OnAckFrameStart(QuicPacketNumber(4), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(4), QuicPacketNumber(5));
  EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*loss_algorithm, SpuriousLossDetected(_, _, _, _, _));
  EXPECT_CALL(
      *send_algorithm_,
      OnCongestionEvent(/*rtt_updated=*/true, kDefaultLength, _, _, _, _, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(3),
                                   ENCRYPTION_FORWARD_SECURE, kEmptyCounts));
  EXPECT_EQ(kDefaultLength, BytesInFlight());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(30),
            manager_.GetRttStats()->latest_rtt());

  // Migrate again with in-flight packet5 whose retransmittable frames are all
  // ACKed. Packet5 should be marked for retransmission but nothing to
  // retransmit.
  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillOnce(Return(false));
  EXPECT_CALL(notifier_, OnFrameLost(_)).Times(0u);
  old_send_algorithm =
      manager_.OnConnectionMigration(/*reset_send_algorithm=*/true);
  EXPECT_EQ(default_init_rtt, rtt_stats->initial_rtt());
  EXPECT_EQ(0u, manager_.GetConsecutivePtoCount());
  EXPECT_EQ(0u, BytesInFlight());
  EXPECT_TRUE(manager_.GetRttStats()->latest_rtt().IsZero());

  manager_.SetSendAlgorithm(old_send_algorithm.release());

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  // Receiving an ACK for packet5 shouldn't update RTT. Though packet 5 was
  // marked for retransmission, this spurious retransmission shouldn't be
  // reported to the loss algorithm.
  manager_.OnAckFrameStart(QuicPacketNumber(5), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(5), QuicPacketNumber(6));
  EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*loss_algorithm, SpuriousLossDetected(_, _, _, _, _)).Times(0u);
  EXPECT_CALL(*send_algorithm_,
              OnCongestionEvent(/*rtt_updated=*/false, 0, _, _, _, _, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(3),
                                   ENCRYPTION_FORWARD_SECURE, kEmptyCounts));
  EXPECT_EQ(0u, BytesInFlight());
  EXPECT_TRUE(manager_.GetRttStats()->latest_rtt().IsZero());
}

TEST_F(QuicSentPacketManagerTest, PathMtuIncreased) {
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, BytesInFlight(), QuicPacketNumber(1), _, _));
  SerializedPacket packet(QuicPacketNumber(1), PACKET_4BYTE_PACKET_NUMBER,
                          nullptr, kDefaultLength + 100, false, false);
  manager_.OnPacketSent(&packet, clock_.Now(), NOT_RETRANSMISSION,
                        HAS_RETRANSMITTABLE_DATA, true, ECN_NOT_ECT);

  // Ack the large packet and expect the path MTU to increase.
  ExpectAck(1);
  EXPECT_CALL(*network_change_visitor_,
              OnPathMtuIncreased(kDefaultLength + 100));
  QuicAckFrame ack_frame = InitAckFrame(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
}

TEST_F(QuicSentPacketManagerTest, OnAckRangeSlowPath) {
  // Send packets 1 - 20.
  for (size_t i = 1; i <= 20; ++i) {
    SendDataPacket(i);
  }
  // Ack [5, 7), [10, 12), [15, 17).
  uint64_t acked1[] = {5, 6, 10, 11, 15, 16};
  uint64_t lost1[] = {1, 2, 3, 4, 7, 8, 9, 12, 13};
  ExpectAcksAndLosses(true, acked1, ABSL_ARRAYSIZE(acked1), lost1,
                      ABSL_ARRAYSIZE(lost1));
  EXPECT_CALL(notifier_, OnFrameLost(_)).Times(AnyNumber());
  manager_.OnAckFrameStart(QuicPacketNumber(16), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(15), QuicPacketNumber(17));
  manager_.OnAckRange(QuicPacketNumber(10), QuicPacketNumber(12));
  manager_.OnAckRange(QuicPacketNumber(5), QuicPacketNumber(7));
  // Make sure empty range does not harm.
  manager_.OnAckRange(QuicPacketNumber(4), QuicPacketNumber(4));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));

  // Ack [4, 8), [9, 13), [14, 21).
  uint64_t acked2[] = {4, 7, 9, 12, 14, 17, 18, 19, 20};
  ExpectAcksAndLosses(true, acked2, ABSL_ARRAYSIZE(acked2), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(20), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(14), QuicPacketNumber(21));
  manager_.OnAckRange(QuicPacketNumber(9), QuicPacketNumber(13));
  manager_.OnAckRange(QuicPacketNumber(4), QuicPacketNumber(8));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(2),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
}

TEST_F(QuicSentPacketManagerTest, TolerateReneging) {
  // Send packets 1 - 20.
  for (size_t i = 1; i <= 20; ++i) {
    SendDataPacket(i);
  }
  // Ack [5, 7), [10, 12), [15, 17).
  uint64_t acked1[] = {5, 6, 10, 11, 15, 16};
  uint64_t lost1[] = {1, 2, 3, 4, 7, 8, 9, 12, 13};
  ExpectAcksAndLosses(true, acked1, ABSL_ARRAYSIZE(acked1), lost1,
                      ABSL_ARRAYSIZE(lost1));
  EXPECT_CALL(notifier_, OnFrameLost(_)).Times(AnyNumber());
  manager_.OnAckFrameStart(QuicPacketNumber(16), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(15), QuicPacketNumber(17));
  manager_.OnAckRange(QuicPacketNumber(10), QuicPacketNumber(12));
  manager_.OnAckRange(QuicPacketNumber(5), QuicPacketNumber(7));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));

  // Making sure reneged ACK does not harm. Ack [4, 8), [9, 13).
  uint64_t acked2[] = {4, 7, 9, 12};
  ExpectAcksAndLosses(true, acked2, ABSL_ARRAYSIZE(acked2), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(12), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(9), QuicPacketNumber(13));
  manager_.OnAckRange(QuicPacketNumber(4), QuicPacketNumber(8));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(2),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  EXPECT_EQ(QuicPacketNumber(16), manager_.GetLargestObserved());
}

TEST_F(QuicSentPacketManagerTest, MultiplePacketNumberSpaces) {
  manager_.EnableMultiplePacketNumberSpacesSupport();
  const QuicUnackedPacketMap* unacked_packets =
      QuicSentPacketManagerPeer::GetUnackedPacketMap(&manager_);
  EXPECT_FALSE(
      unacked_packets
          ->GetLargestSentRetransmittableOfPacketNumberSpace(INITIAL_DATA)
          .IsInitialized());
  EXPECT_FALSE(
      manager_.GetLargestAckedPacket(ENCRYPTION_INITIAL).IsInitialized());
  // Send packet 1.
  SendDataPacket(1, ENCRYPTION_INITIAL);
  EXPECT_EQ(QuicPacketNumber(1),
            unacked_packets->GetLargestSentRetransmittableOfPacketNumberSpace(
                INITIAL_DATA));
  EXPECT_FALSE(
      unacked_packets
          ->GetLargestSentRetransmittableOfPacketNumberSpace(HANDSHAKE_DATA)
          .IsInitialized());
  // Ack packet 1.
  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  EXPECT_EQ(QuicPacketNumber(1),
            manager_.GetLargestAckedPacket(ENCRYPTION_INITIAL));
  EXPECT_FALSE(
      manager_.GetLargestAckedPacket(ENCRYPTION_HANDSHAKE).IsInitialized());
  // Send packets 2 and 3.
  SendDataPacket(2, ENCRYPTION_HANDSHAKE);
  SendDataPacket(3, ENCRYPTION_HANDSHAKE);
  EXPECT_EQ(QuicPacketNumber(1),
            unacked_packets->GetLargestSentRetransmittableOfPacketNumberSpace(
                INITIAL_DATA));
  EXPECT_EQ(QuicPacketNumber(3),
            unacked_packets->GetLargestSentRetransmittableOfPacketNumberSpace(
                HANDSHAKE_DATA));
  EXPECT_FALSE(
      unacked_packets
          ->GetLargestSentRetransmittableOfPacketNumberSpace(APPLICATION_DATA)
          .IsInitialized());
  // Ack packet 2.
  ExpectAck(2);
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(2),
                                   ENCRYPTION_HANDSHAKE, kEmptyCounts));
  EXPECT_EQ(QuicPacketNumber(2),
            manager_.GetLargestAckedPacket(ENCRYPTION_HANDSHAKE));
  EXPECT_FALSE(
      manager_.GetLargestAckedPacket(ENCRYPTION_ZERO_RTT).IsInitialized());
  // Ack packet 3.
  ExpectAck(3);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(4));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(3),
                                   ENCRYPTION_HANDSHAKE, kEmptyCounts));
  EXPECT_EQ(QuicPacketNumber(3),
            manager_.GetLargestAckedPacket(ENCRYPTION_HANDSHAKE));
  EXPECT_FALSE(
      manager_.GetLargestAckedPacket(ENCRYPTION_ZERO_RTT).IsInitialized());
  // Send packets 4 and 5.
  SendDataPacket(4, ENCRYPTION_ZERO_RTT);
  SendDataPacket(5, ENCRYPTION_ZERO_RTT);
  EXPECT_EQ(QuicPacketNumber(1),
            unacked_packets->GetLargestSentRetransmittableOfPacketNumberSpace(
                INITIAL_DATA));
  EXPECT_EQ(QuicPacketNumber(3),
            unacked_packets->GetLargestSentRetransmittableOfPacketNumberSpace(
                HANDSHAKE_DATA));
  EXPECT_EQ(QuicPacketNumber(5),
            unacked_packets->GetLargestSentRetransmittableOfPacketNumberSpace(
                APPLICATION_DATA));
  // Ack packet 5.
  ExpectAck(5);
  manager_.OnAckFrameStart(QuicPacketNumber(5), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(5), QuicPacketNumber(6));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(4),
                                   ENCRYPTION_FORWARD_SECURE, kEmptyCounts));
  EXPECT_EQ(QuicPacketNumber(3),
            manager_.GetLargestAckedPacket(ENCRYPTION_HANDSHAKE));
  EXPECT_EQ(QuicPacketNumber(5),
            manager_.GetLargestAckedPacket(ENCRYPTION_ZERO_RTT));
  EXPECT_EQ(QuicPacketNumber(5),
            manager_.GetLargestAckedPacket(ENCRYPTION_FORWARD_SECURE));

  // Send packets 6 - 8.
  SendDataPacket(6, ENCRYPTION_FORWARD_SECURE);
  SendDataPacket(7, ENCRYPTION_FORWARD_SECURE);
  SendDataPacket(8, ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(QuicPacketNumber(1),
            unacked_packets->GetLargestSentRetransmittableOfPacketNumberSpace(
                INITIAL_DATA));
  EXPECT_EQ(QuicPacketNumber(3),
            unacked_packets->GetLargestSentRetransmittableOfPacketNumberSpace(
                HANDSHAKE_DATA));
  EXPECT_EQ(QuicPacketNumber(8),
            unacked_packets->GetLargestSentRetransmittableOfPacketNumberSpace(
                APPLICATION_DATA));
  // Ack all packets.
  uint64_t acked[] = {4, 6, 7, 8};
  ExpectAcksAndLosses(true, acked, ABSL_ARRAYSIZE(acked), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(8), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(4), QuicPacketNumber(9));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(5),
                                   ENCRYPTION_FORWARD_SECURE, kEmptyCounts));
  EXPECT_EQ(QuicPacketNumber(3),
            manager_.GetLargestAckedPacket(ENCRYPTION_HANDSHAKE));
  EXPECT_EQ(QuicPacketNumber(8),
            manager_.GetLargestAckedPacket(ENCRYPTION_ZERO_RTT));
  EXPECT_EQ(QuicPacketNumber(8),
            manager_.GetLargestAckedPacket(ENCRYPTION_FORWARD_SECURE));
}

TEST_F(QuicSentPacketManagerTest, PacketsGetAckedInWrongPacketNumberSpace) {
  manager_.EnableMultiplePacketNumberSpacesSupport();
  // Send packet 1.
  SendDataPacket(1, ENCRYPTION_INITIAL);
  // Send packets 2 and 3.
  SendDataPacket(2, ENCRYPTION_HANDSHAKE);
  SendDataPacket(3, ENCRYPTION_HANDSHAKE);

  // ACK packets 2 and 3 in the wrong packet number space.
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(4));
  EXPECT_EQ(PACKETS_ACKED_IN_WRONG_PACKET_NUMBER_SPACE,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
}

TEST_F(QuicSentPacketManagerTest, PacketsGetAckedInWrongPacketNumberSpace2) {
  manager_.EnableMultiplePacketNumberSpacesSupport();
  // Send packet 1.
  SendDataPacket(1, ENCRYPTION_INITIAL);
  // Send packets 2 and 3.
  SendDataPacket(2, ENCRYPTION_HANDSHAKE);
  SendDataPacket(3, ENCRYPTION_HANDSHAKE);

  // ACK packet 1 in the wrong packet number space.
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(4));
  EXPECT_EQ(PACKETS_ACKED_IN_WRONG_PACKET_NUMBER_SPACE,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_HANDSHAKE, kEmptyCounts));
}

TEST_F(QuicSentPacketManagerTest,
       ToleratePacketsGetAckedInWrongPacketNumberSpace) {
  manager_.EnableMultiplePacketNumberSpacesSupport();
  // Send packet 1.
  SendDataPacket(1, ENCRYPTION_INITIAL);
  // Ack packet 1.
  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));

  // Send packets 2 and 3.
  SendDataPacket(2, ENCRYPTION_HANDSHAKE);
  SendDataPacket(3, ENCRYPTION_HANDSHAKE);

  // Packet 1 gets acked in the wrong packet number space. Since packet 1 has
  // been acked in the correct packet number space, tolerate it.
  uint64_t acked[] = {2, 3};
  ExpectAcksAndLosses(true, acked, ABSL_ARRAYSIZE(acked), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(4));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(2),
                                   ENCRYPTION_HANDSHAKE, kEmptyCounts));
}

TEST_F(QuicSentPacketManagerTest, ComputingProbeTimeout) {
  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(10 * kDefaultTCPMSS));
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(100),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  QuicTime::Delta srtt = rtt_stats->smoothed_rtt();

  SendDataPacket(1, ENCRYPTION_FORWARD_SECURE);
  // Verify PTO is correctly set.
  QuicTime::Delta expected_pto_delay =
      srtt + kPtoRttvarMultiplier * rtt_stats->mean_deviation() +
      QuicTime::Delta::FromMilliseconds(GetDefaultDelayedAckTimeMs());
  QuicTime packet1_sent_time = clock_.Now();
  EXPECT_EQ(clock_.Now() + expected_pto_delay,
            manager_.GetRetransmissionTime());

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendDataPacket(2, ENCRYPTION_FORWARD_SECURE);
  // Verify PTO is set based on left edge.
  QuicTime deadline = packet1_sent_time + expected_pto_delay;
  EXPECT_EQ(deadline, manager_.GetRetransmissionTime());
  EXPECT_EQ(0u, stats_.pto_count);

  // Invoke PTO.
  clock_.AdvanceTime(deadline - clock_.Now());
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(QuicTime::Delta::Zero(), manager_.TimeUntilSend(clock_.Now()));
  EXPECT_EQ(1u, stats_.pto_count);
  EXPECT_EQ(0u, stats_.max_consecutive_rto_with_forward_progress);

  EXPECT_CALL(notifier_, RetransmitFrames(_, _))
      .WillOnce(WithArgs<1>([this](TransmissionType type) {
        return RetransmitDataPacket(3, type, ENCRYPTION_FORWARD_SECURE);
      }));
  manager_.MaybeSendProbePacket();
  // Verify PTO period gets set to twice the current value.
  QuicTime sent_time = clock_.Now();
  EXPECT_EQ(sent_time + expected_pto_delay * 2,
            manager_.GetRetransmissionTime());

  // Received ACK for packets 1 and 2.
  uint64_t acked[] = {1, 2};
  ExpectAcksAndLosses(true, acked, ABSL_ARRAYSIZE(acked), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_FORWARD_SECURE, kEmptyCounts));
  expected_pto_delay =
      rtt_stats->SmoothedOrInitialRtt() +
      std::max(kPtoRttvarMultiplier * rtt_stats->mean_deviation(),
               QuicTime::Delta::FromMilliseconds(1)) +
      QuicTime::Delta::FromMilliseconds(GetDefaultDelayedAckTimeMs());

  // Verify PTO is correctly re-armed based on sent time of packet 4.
  EXPECT_EQ(sent_time + expected_pto_delay, manager_.GetRetransmissionTime());
  EXPECT_EQ(1u, stats_.max_consecutive_rto_with_forward_progress);
}

TEST_F(QuicSentPacketManagerTest, SendOneProbePacket) {
  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(10 * kDefaultTCPMSS));

  SendDataPacket(1, ENCRYPTION_FORWARD_SECURE);
  QuicTime packet1_sent_time = clock_.Now();
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendDataPacket(2, ENCRYPTION_FORWARD_SECURE);

  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(100),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  QuicTime::Delta srtt = rtt_stats->smoothed_rtt();
  // Verify PTO period is correctly set.
  QuicTime::Delta expected_pto_delay =
      srtt + kPtoRttvarMultiplier * rtt_stats->mean_deviation() +
      QuicTime::Delta::FromMilliseconds(GetDefaultDelayedAckTimeMs());
  // Verify PTO is set based on left edge.
  QuicTime deadline = packet1_sent_time + expected_pto_delay;
  EXPECT_EQ(deadline, manager_.GetRetransmissionTime());

  // Invoke PTO.
  clock_.AdvanceTime(deadline - clock_.Now());
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(QuicTime::Delta::Zero(), manager_.TimeUntilSend(clock_.Now()));

  // Verify one probe packet gets sent.
  EXPECT_CALL(notifier_, RetransmitFrames(_, _))
      .WillOnce(WithArgs<1>([this](TransmissionType type) {
        return RetransmitDataPacket(3, type, ENCRYPTION_FORWARD_SECURE);
      }));
  manager_.MaybeSendProbePacket();
}

TEST_F(QuicSentPacketManagerTest, DisableHandshakeModeClient) {
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  manager_.EnableMultiplePacketNumberSpacesSupport();
  // Send CHLO.
  SendCryptoPacket(1);
  EXPECT_NE(QuicTime::Zero(), manager_.GetRetransmissionTime());
  // Ack packet 1.
  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  EXPECT_EQ(0u, manager_.GetBytesInFlight());
  // Verify retransmission timeout is not zero because handshake is not
  // confirmed although there is no in flight packet.
  EXPECT_NE(QuicTime::Zero(), manager_.GetRetransmissionTime());
  // Fire PTO.
  EXPECT_EQ(QuicSentPacketManager::PTO_MODE,
            manager_.OnRetransmissionTimeout());
  // Send handshake packet.
  SendDataPacket(2, ENCRYPTION_HANDSHAKE);
  // Ack packet 2.
  ExpectAck(2);
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(2),
                                   ENCRYPTION_HANDSHAKE, kEmptyCounts));
  // Verify retransmission timeout is zero because server has successfully
  // processed HANDSHAKE packet.
  EXPECT_EQ(QuicTime::Zero(), manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, DisableHandshakeModeServer) {
  manager_.EnableIetfPtoAndLossDetection();
  // Send SHLO.
  SendCryptoPacket(1);
  EXPECT_NE(QuicTime::Zero(), manager_.GetRetransmissionTime());
  // Ack packet 1.
  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  EXPECT_EQ(0u, manager_.GetBytesInFlight());
  // Verify retransmission timeout is not set on server side because there is
  // nothing in flight.
  EXPECT_EQ(QuicTime::Zero(), manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, PtoTimeoutRttVarMultiple) {
  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(10 * kDefaultTCPMSS));
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(100),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  QuicTime::Delta srtt = rtt_stats->smoothed_rtt();

  SendDataPacket(1, ENCRYPTION_FORWARD_SECURE);
  // Verify PTO is correctly set based on 2 times rtt var.
  QuicTime::Delta expected_pto_delay =
      srtt + kPtoRttvarMultiplier * rtt_stats->mean_deviation() +
      QuicTime::Delta::FromMilliseconds(GetDefaultDelayedAckTimeMs());
  EXPECT_EQ(clock_.Now() + expected_pto_delay,
            manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, IW10ForUpAndDown) {
  QuicConfig config;
  QuicTagVector options;
  options.push_back(kBWS5);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, SetInitialCongestionWindowInPackets(10));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);

  EXPECT_EQ(10u, manager_.initial_congestion_window());
}

TEST_F(QuicSentPacketManagerTest, ServerCongestionWindowDoubledWithIW2X) {
  SetQuicReloadableFlag(quic_allow_client_enabled_2x_initial_cwnd, true);
  QuicConfig config;
  QuicConfigPeer::SetReceivedConnectionOptions(&config, {kIW2X});
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, SetInitialCongestionWindowInPackets(
                                    kInitialCongestionWindow * 2));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);

  EXPECT_EQ(manager_.initial_congestion_window(), kInitialCongestionWindow * 2);
}

TEST_F(QuicSentPacketManagerTest,
       ServerCongestionWindowIsDefaultWithIW2XAndNoFlag) {
  SetQuicReloadableFlag(quic_allow_client_enabled_2x_initial_cwnd, false);
  QuicConfig config;
  QuicConfigPeer::SetReceivedConnectionOptions(&config, {kIW2X});
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, SetInitialCongestionWindowInPackets(_))
      .Times(0);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);

  EXPECT_EQ(manager_.initial_congestion_window(), kInitialCongestionWindow);
}

TEST_F(QuicSentPacketManagerTest,
       ClientCongestionWindowIsDefaultWithIW2XAndNoFlag) {
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  SetQuicReloadableFlag(quic_allow_client_enabled_2x_initial_cwnd, false);
  QuicConfig config;
  config.SetConnectionOptionsToSend({kIW2X});
  config.SetClientConnectionOptions({});

  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_,
              SetInitialCongestionWindowInPackets(kInitialCongestionWindow * 2))
      .Times(0);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);

  EXPECT_EQ(manager_.initial_congestion_window(), kInitialCongestionWindow);
}

TEST_F(QuicSentPacketManagerTest, ClientMultiplePacketNumberSpacePtoTimeout) {
  manager_.EnableMultiplePacketNumberSpacesSupport();
  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(10 * kDefaultTCPMSS));
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(100),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  QuicTime::Delta srtt = rtt_stats->smoothed_rtt();
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);

  // Send packet 1.
  SendDataPacket(1, ENCRYPTION_INITIAL);
  // Verify PTO is correctly set.
  QuicTime::Delta expected_pto_delay =
      srtt + kPtoRttvarMultiplier * rtt_stats->mean_deviation() +
      QuicTime::Delta::Zero();
  EXPECT_EQ(clock_.Now() + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Discard initial key and send packet 2 in handshake.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  manager_.NeuterUnencryptedPackets();

  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(true));
  SendDataPacket(2, ENCRYPTION_HANDSHAKE);
  // Verify PTO is correctly set based on sent time of packet 2.
  EXPECT_EQ(clock_.Now() + expected_pto_delay,
            manager_.GetRetransmissionTime());
  // Invoke PTO.
  clock_.AdvanceTime(expected_pto_delay);
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(QuicTime::Delta::Zero(), manager_.TimeUntilSend(clock_.Now()));
  EXPECT_EQ(1u, stats_.pto_count);
  EXPECT_EQ(1u, stats_.crypto_retransmit_count);

  // Verify probe packet gets sent.
  EXPECT_CALL(notifier_, RetransmitFrames(_, _))
      .WillOnce(WithArgs<1>([this](TransmissionType type) {
        return RetransmitDataPacket(3, type, ENCRYPTION_HANDSHAKE);
      }));
  manager_.MaybeSendProbePacket();
  // Verify PTO period gets set to twice the current value.
  const QuicTime packet3_sent_time = clock_.Now();
  EXPECT_EQ(packet3_sent_time + expected_pto_delay * 2,
            manager_.GetRetransmissionTime());

  // Send packet 4 in application data with 0-RTT.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendDataPacket(4, ENCRYPTION_ZERO_RTT);
  const QuicTime packet4_sent_time = clock_.Now();
  // Verify PTO timeout is still based on packet 3.
  EXPECT_EQ(packet3_sent_time + expected_pto_delay * 2,
            manager_.GetRetransmissionTime());

  // Send packet 5 in handshake.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendDataPacket(5, ENCRYPTION_HANDSHAKE);
  const QuicTime packet5_sent_time = clock_.Now();
  // Verify PTO timeout is now based on packet 5 because packet 4 should be
  // ignored.
  EXPECT_EQ(clock_.Now() + expected_pto_delay * 2,
            manager_.GetRetransmissionTime());

  // Send packet 6 in 1-RTT.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendDataPacket(6, ENCRYPTION_FORWARD_SECURE);
  // Verify PTO timeout is now based on packet 5.
  EXPECT_EQ(packet5_sent_time + expected_pto_delay * 2,
            manager_.GetRetransmissionTime());

  // Send packet 7 in handshake.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  const QuicTime packet7_sent_time = clock_.Now();
  SendDataPacket(7, ENCRYPTION_HANDSHAKE);

  expected_pto_delay =
      srtt + kPtoRttvarMultiplier * rtt_stats->mean_deviation();
  // Verify PTO timeout is now based on packet 7.
  EXPECT_EQ(packet7_sent_time + expected_pto_delay * 2,
            manager_.GetRetransmissionTime());

  // Neuter handshake key.
  manager_.SetHandshakeConfirmed();
  // Forward progress has been made, verify PTO counter gets reset. PTO timeout
  // is armed by left edge.
  expected_pto_delay =
      srtt + kPtoRttvarMultiplier * rtt_stats->mean_deviation() +
      QuicTime::Delta::FromMilliseconds(GetDefaultDelayedAckTimeMs());
  EXPECT_EQ(packet4_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, ServerMultiplePacketNumberSpacePtoTimeout) {
  manager_.EnableMultiplePacketNumberSpacesSupport();
  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(10 * kDefaultTCPMSS));
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(100),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  QuicTime::Delta srtt = rtt_stats->smoothed_rtt();

  // Send packet 1.
  SendDataPacket(1, ENCRYPTION_INITIAL);
  const QuicTime packet1_sent_time = clock_.Now();
  // Verify PTO is correctly set.
  QuicTime::Delta expected_pto_delay =
      srtt + kPtoRttvarMultiplier * rtt_stats->mean_deviation() +
      QuicTime::Delta::Zero();
  EXPECT_EQ(packet1_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Send packet 2 in handshake.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendDataPacket(2, ENCRYPTION_HANDSHAKE);
  const QuicTime packet2_sent_time = clock_.Now();
  // Verify PTO timeout is still based on packet 1.
  EXPECT_EQ(packet1_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Discard initial keys.
  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  manager_.NeuterUnencryptedPackets();

  // Send packet 3 in 1-RTT.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendDataPacket(3, ENCRYPTION_FORWARD_SECURE);
  // Verify PTO timeout is based on packet 2.
  const QuicTime packet3_sent_time = clock_.Now();
  EXPECT_EQ(packet2_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Send packet 4 in handshake.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendDataPacket(4, ENCRYPTION_HANDSHAKE);
  // Verify PTO timeout is based on packet 4 as application data is ignored.
  EXPECT_EQ(clock_.Now() + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Discard handshake keys.
  manager_.SetHandshakeConfirmed();
  expected_pto_delay =
      srtt + kPtoRttvarMultiplier * rtt_stats->mean_deviation() +
      QuicTime::Delta::FromMilliseconds(GetDefaultDelayedAckTimeMs());
  // Verify PTO timeout is now based on packet 3 as handshake is
  // complete/confirmed.
  EXPECT_EQ(packet3_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, ComputingProbeTimeoutByLeftEdge) {
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(10 * kDefaultTCPMSS));
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(100),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  QuicTime::Delta srtt = rtt_stats->smoothed_rtt();

  SendDataPacket(1, ENCRYPTION_FORWARD_SECURE);
  // Verify PTO is correctly set.
  QuicTime::Delta expected_pto_delay =
      srtt + kPtoRttvarMultiplier * rtt_stats->mean_deviation() +
      QuicTime::Delta::FromMilliseconds(GetDefaultDelayedAckTimeMs());
  const QuicTime packet1_sent_time = clock_.Now();
  EXPECT_EQ(packet1_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendDataPacket(2, ENCRYPTION_FORWARD_SECURE);
  // Verify PTO is still based on packet 1.
  EXPECT_EQ(packet1_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());
  EXPECT_EQ(0u, stats_.pto_count);

  // Invoke PTO.
  clock_.AdvanceTime(expected_pto_delay);
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(QuicTime::Delta::Zero(), manager_.TimeUntilSend(clock_.Now()));
  EXPECT_EQ(1u, stats_.pto_count);

  EXPECT_CALL(notifier_, RetransmitFrames(_, _))
      .WillOnce(WithArgs<1>([this](TransmissionType type) {
        return RetransmitDataPacket(3, type, ENCRYPTION_FORWARD_SECURE);
      }));
  manager_.MaybeSendProbePacket();
  // Verify PTO period gets set to twice the current value and based on packet3.
  QuicTime packet3_sent_time = clock_.Now();
  EXPECT_EQ(packet3_sent_time + expected_pto_delay * 2,
            manager_.GetRetransmissionTime());

  // Received ACK for packets 1 and 2.
  uint64_t acked[] = {1, 2};
  ExpectAcksAndLosses(true, acked, ABSL_ARRAYSIZE(acked), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_FORWARD_SECURE, kEmptyCounts));
  expected_pto_delay =
      rtt_stats->SmoothedOrInitialRtt() +
      std::max(kPtoRttvarMultiplier * rtt_stats->mean_deviation(),
               QuicTime::Delta::FromMilliseconds(1)) +
      QuicTime::Delta::FromMilliseconds(GetDefaultDelayedAckTimeMs());

  // Verify PTO is correctly re-armed based on sent time of packet 4.
  EXPECT_EQ(packet3_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, ComputingProbeTimeoutByLeftEdge2) {
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(10 * kDefaultTCPMSS));
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(100),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  QuicTime::Delta srtt = rtt_stats->smoothed_rtt();

  SendDataPacket(1, ENCRYPTION_FORWARD_SECURE);
  // Verify PTO is correctly set.
  QuicTime::Delta expected_pto_delay =
      srtt + kPtoRttvarMultiplier * rtt_stats->mean_deviation() +
      QuicTime::Delta::FromMilliseconds(GetDefaultDelayedAckTimeMs());
  const QuicTime packet1_sent_time = clock_.Now();
  EXPECT_EQ(packet1_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Sent a packet 10ms before PTO expiring.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(
      expected_pto_delay.ToMilliseconds() - 10));
  SendDataPacket(2, ENCRYPTION_FORWARD_SECURE);
  // Verify PTO expands to packet 2 sent time + 1.5 * srtt.
  expected_pto_delay = kFirstPtoSrttMultiplier * rtt_stats->smoothed_rtt();
  EXPECT_EQ(clock_.Now() + expected_pto_delay,
            manager_.GetRetransmissionTime());
  EXPECT_EQ(0u, stats_.pto_count);

  // Invoke PTO.
  clock_.AdvanceTime(expected_pto_delay);
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(QuicTime::Delta::Zero(), manager_.TimeUntilSend(clock_.Now()));
  EXPECT_EQ(1u, stats_.pto_count);

  EXPECT_CALL(notifier_, RetransmitFrames(_, _))
      .WillOnce(WithArgs<1>([this](TransmissionType type) {
        return RetransmitDataPacket(3, type, ENCRYPTION_FORWARD_SECURE);
      }));
  manager_.MaybeSendProbePacket();
  // Verify PTO period gets set to twice the expected value and based on
  // packet3 (right edge).
  expected_pto_delay =
      srtt + kPtoRttvarMultiplier * rtt_stats->mean_deviation() +
      QuicTime::Delta::FromMilliseconds(GetDefaultDelayedAckTimeMs());
  QuicTime packet3_sent_time = clock_.Now();
  EXPECT_EQ(packet3_sent_time + expected_pto_delay * 2,
            manager_.GetRetransmissionTime());

  // Received ACK for packets 1 and 2.
  uint64_t acked[] = {1, 2};
  ExpectAcksAndLosses(true, acked, ABSL_ARRAYSIZE(acked), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_FORWARD_SECURE, kEmptyCounts));
  expected_pto_delay =
      rtt_stats->SmoothedOrInitialRtt() +
      std::max(kPtoRttvarMultiplier * rtt_stats->mean_deviation(),
               QuicTime::Delta::FromMilliseconds(1)) +
      QuicTime::Delta::FromMilliseconds(GetDefaultDelayedAckTimeMs());

  // Verify PTO is correctly re-armed based on sent time of packet 3 (left
  // edge).
  EXPECT_EQ(packet3_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest,
       ComputingProbeTimeoutByLeftEdgeMultiplePacketNumberSpaces) {
  manager_.EnableMultiplePacketNumberSpacesSupport();
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(10 * kDefaultTCPMSS));
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(100),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  QuicTime::Delta srtt = rtt_stats->smoothed_rtt();

  // Send packet 1.
  SendDataPacket(1, ENCRYPTION_INITIAL);
  const QuicTime packet1_sent_time = clock_.Now();
  // Verify PTO is correctly set.
  QuicTime::Delta expected_pto_delay =
      srtt + kPtoRttvarMultiplier * rtt_stats->mean_deviation() +
      QuicTime::Delta::Zero();
  EXPECT_EQ(packet1_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Send packet 2 in handshake.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendDataPacket(2, ENCRYPTION_HANDSHAKE);
  const QuicTime packet2_sent_time = clock_.Now();
  // Verify PTO timeout is still based on packet 1.
  EXPECT_EQ(packet1_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Discard initial keys.
  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  manager_.NeuterUnencryptedPackets();

  // Send packet 3 in 1-RTT.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendDataPacket(3, ENCRYPTION_FORWARD_SECURE);
  // Verify PTO timeout is based on packet 2.
  const QuicTime packet3_sent_time = clock_.Now();
  EXPECT_EQ(packet2_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Send packet 4 in handshake.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendDataPacket(4, ENCRYPTION_HANDSHAKE);
  // Verify PTO timeout is based on packet 4 as application data is ignored.
  EXPECT_EQ(clock_.Now() + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Discard handshake keys.
  manager_.SetHandshakeConfirmed();
  // Verify PTO timeout is now based on packet 3 as handshake is
  // complete/confirmed.
  expected_pto_delay =
      srtt + kPtoRttvarMultiplier * rtt_stats->mean_deviation() +
      QuicTime::Delta::FromMilliseconds(GetDefaultDelayedAckTimeMs());
  EXPECT_EQ(packet3_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendDataPacket(5, ENCRYPTION_FORWARD_SECURE);
  // Verify PTO timeout is still based on packet 3.
  EXPECT_EQ(packet3_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest,
       ComputingProbeTimeoutByLeftEdge2MultiplePacketNumberSpaces) {
  manager_.EnableMultiplePacketNumberSpacesSupport();
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(10 * kDefaultTCPMSS));
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(100),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  QuicTime::Delta srtt = rtt_stats->smoothed_rtt();

  // Send packet 1.
  SendDataPacket(1, ENCRYPTION_INITIAL);
  const QuicTime packet1_sent_time = clock_.Now();
  // Verify PTO is correctly set.
  QuicTime::Delta expected_pto_delay =
      srtt + kPtoRttvarMultiplier * rtt_stats->mean_deviation() +
      QuicTime::Delta::Zero();
  EXPECT_EQ(packet1_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Send packet 2 in handshake.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendDataPacket(2, ENCRYPTION_HANDSHAKE);
  const QuicTime packet2_sent_time = clock_.Now();
  // Verify PTO timeout is still based on packet 1.
  EXPECT_EQ(packet1_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Discard initial keys.
  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  manager_.NeuterUnencryptedPackets();

  // Send packet 3 in 1-RTT.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendDataPacket(3, ENCRYPTION_FORWARD_SECURE);
  // Verify PTO timeout is based on packet 2.
  const QuicTime packet3_sent_time = clock_.Now();
  EXPECT_EQ(packet2_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Send packet 4 in handshake.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendDataPacket(4, ENCRYPTION_HANDSHAKE);
  // Verify PTO timeout is based on packet 4 as application data is ignored.
  EXPECT_EQ(clock_.Now() + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Discard handshake keys.
  manager_.SetHandshakeConfirmed();
  // Verify PTO timeout is now based on packet 3 as handshake is
  // complete/confirmed.
  expected_pto_delay =
      srtt + kPtoRttvarMultiplier * rtt_stats->mean_deviation() +
      QuicTime::Delta::FromMilliseconds(GetDefaultDelayedAckTimeMs());
  EXPECT_EQ(packet3_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Send packet 5 10ms before PTO expiring.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(
      expected_pto_delay.ToMilliseconds() - 10));
  SendDataPacket(5, ENCRYPTION_FORWARD_SECURE);
  // Verify PTO timeout expands to packet 5 sent time + 1.5 * srtt.
  EXPECT_EQ(clock_.Now() + kFirstPtoSrttMultiplier * rtt_stats->smoothed_rtt(),
            manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, SetHandshakeConfirmed) {
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  manager_.EnableMultiplePacketNumberSpacesSupport();

  SendDataPacket(1, ENCRYPTION_INITIAL);

  SendDataPacket(2, ENCRYPTION_HANDSHAKE);

  EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _))
      .WillOnce([](const QuicFrame& /*frame*/, QuicTime::Delta ack_delay_time,
                   QuicTime receive_timestamp, bool) {
        EXPECT_TRUE(ack_delay_time.IsZero());
        EXPECT_EQ(receive_timestamp, QuicTime::Zero());
        return true;
      });

  EXPECT_CALL(*send_algorithm_, OnPacketNeutered(QuicPacketNumber(2))).Times(1);
  manager_.SetHandshakeConfirmed();
}

// Regresstion test for b/148841700.
TEST_F(QuicSentPacketManagerTest, NeuterUnencryptedPackets) {
  SendCryptoPacket(1);
  SendPingPacket(2, ENCRYPTION_INITIAL);
  // Crypto data has been discarded but ping does not.
  EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _))
      .Times(2)
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));

  EXPECT_CALL(*send_algorithm_, OnPacketNeutered(QuicPacketNumber(1))).Times(1);
  manager_.NeuterUnencryptedPackets();
}

TEST_F(QuicSentPacketManagerTest, MarkInitialPacketsForRetransmission) {
  SendCryptoPacket(1);
  SendPingPacket(2, ENCRYPTION_HANDSHAKE);
  // Only the INITIAL packet will be retransmitted.
  EXPECT_CALL(notifier_, OnFrameLost(_)).Times(1);
  manager_.MarkInitialPacketsForRetransmission();
}

TEST_F(QuicSentPacketManagerTest, NoPacketThresholdDetectionForRuntPackets) {
  EXPECT_TRUE(
      QuicSentPacketManagerPeer::UsePacketThresholdForRuntPackets(&manager_));

  QuicConfig config;
  QuicTagVector options;
  options.push_back(kRUNT);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);

  EXPECT_FALSE(
      QuicSentPacketManagerPeer::UsePacketThresholdForRuntPackets(&manager_));
}

TEST_F(QuicSentPacketManagerTest, GetPathDegradingDelayDefaultPTO) {
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  QuicTime::Delta expected_delay = 4 * manager_.GetPtoDelay();
  EXPECT_EQ(expected_delay, manager_.GetPathDegradingDelay());
}

TEST_F(QuicSentPacketManagerTest, ClientsIgnorePings) {
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  QuicConfig client_config;
  QuicTagVector options;
  QuicTagVector client_options;
  client_options.push_back(kIGNP);
  client_config.SetConnectionOptionsToSend(options);
  client_config.SetClientConnectionOptions(client_options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(client_config);

  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(10 * kDefaultTCPMSS));
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));

  SendPingPacket(1, ENCRYPTION_INITIAL);
  // Verify PING only packet is not considered in flight.
  EXPECT_EQ(QuicTime::Zero(), manager_.GetRetransmissionTime());
  SendDataPacket(2, ENCRYPTION_INITIAL);
  EXPECT_NE(QuicTime::Zero(), manager_.GetRetransmissionTime());

  uint64_t acked[] = {1};
  ExpectAcksAndLosses(/*rtt_updated=*/false, acked, ABSL_ARRAYSIZE(acked),
                      nullptr, 0);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(90));
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  // Verify no RTT samples for PING only packet.
  EXPECT_TRUE(rtt_stats->smoothed_rtt().IsZero());

  ExpectAck(2);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(2),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(100), rtt_stats->smoothed_rtt());
}

// Regression test for b/154050235.
TEST_F(QuicSentPacketManagerTest, ExponentialBackoffWithNoRttMeasurement) {
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  manager_.EnableMultiplePacketNumberSpacesSupport();
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(kInitialRttMs),
            rtt_stats->initial_rtt());
  EXPECT_TRUE(rtt_stats->smoothed_rtt().IsZero());

  SendCryptoPacket(1);
  QuicTime::Delta expected_pto_delay =
      QuicTime::Delta::FromMilliseconds(3 * kInitialRttMs);
  EXPECT_EQ(clock_.Now() + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Invoke PTO.
  clock_.AdvanceTime(expected_pto_delay);
  manager_.OnRetransmissionTimeout();

  EXPECT_CALL(notifier_, RetransmitFrames(_, _)).WillOnce(WithArgs<1>([this]() {
    return RetransmitCryptoPacket(3);
  }));
  manager_.MaybeSendProbePacket();
  // Verify exponential backoff of the PTO timeout.
  EXPECT_EQ(clock_.Now() + 2 * expected_pto_delay,
            manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, PtoDelayWithTinyInitialRtt) {
  manager_.EnableMultiplePacketNumberSpacesSupport();
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  // Assume client provided a tiny initial RTT.
  rtt_stats->set_initial_rtt(QuicTime::Delta::FromMicroseconds(1));
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(1), rtt_stats->initial_rtt());
  EXPECT_TRUE(rtt_stats->smoothed_rtt().IsZero());

  SendCryptoPacket(1);
  QuicTime::Delta expected_pto_delay = QuicTime::Delta::FromMilliseconds(10);
  // Verify kMinHandshakeTimeoutMs is respected.
  EXPECT_EQ(clock_.Now() + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Invoke PTO.
  clock_.AdvanceTime(expected_pto_delay);
  manager_.OnRetransmissionTimeout();

  EXPECT_CALL(notifier_, RetransmitFrames(_, _)).WillOnce(WithArgs<1>([this]() {
    return RetransmitCryptoPacket(3);
  }));
  manager_.MaybeSendProbePacket();
  // Verify exponential backoff of the PTO timeout.
  EXPECT_EQ(clock_.Now() + 2 * expected_pto_delay,
            manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, HandshakeAckCausesInitialKeyDropping) {
  manager_.EnableMultiplePacketNumberSpacesSupport();
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  // Send INITIAL packet 1.
  SendDataPacket(1, ENCRYPTION_INITIAL);
  QuicTime::Delta expected_pto_delay =
      QuicTime::Delta::FromMilliseconds(3 * kInitialRttMs);
  EXPECT_EQ(clock_.Now() + expected_pto_delay,
            manager_.GetRetransmissionTime());
  // Send HANDSHAKE ack.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendAckPacket(2, /*largest_acked=*/1, ENCRYPTION_HANDSHAKE);
  // Sending HANDSHAKE packet causes dropping of INITIAL key.
  EXPECT_CALL(notifier_, HasUnackedCryptoData()).WillRepeatedly(Return(false));
  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  manager_.NeuterUnencryptedPackets();
  // There is no in flight packets.
  EXPECT_FALSE(manager_.HasInFlightPackets());
  // Verify PTO timer gets rearmed from now because of anti-amplification.
  EXPECT_EQ(clock_.Now() + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Invoke PTO.
  clock_.AdvanceTime(expected_pto_delay);
  manager_.OnRetransmissionTimeout();
  // Verify nothing to probe (and connection will send PING for current
  // encryption level).
  EXPECT_CALL(notifier_, RetransmitFrames(_, _)).Times(0);
  manager_.MaybeSendProbePacket();
}

// Regression test for b/156487311
TEST_F(QuicSentPacketManagerTest, ClearLastInflightPacketsSentTime) {
  manager_.EnableMultiplePacketNumberSpacesSupport();
  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(10 * kDefaultTCPMSS));

  // Send INITIAL 1.
  SendDataPacket(1, ENCRYPTION_INITIAL);
  // Send HANDSHAKE 2.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendDataPacket(2, ENCRYPTION_HANDSHAKE);
  SendDataPacket(3, ENCRYPTION_HANDSHAKE);
  SendDataPacket(4, ENCRYPTION_HANDSHAKE);
  const QuicTime packet2_sent_time = clock_.Now();

  // Send half RTT 5.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  SendDataPacket(5, ENCRYPTION_FORWARD_SECURE);

  // Received ACK for INITIAL 1.
  ExpectAck(1);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(90));
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  const QuicTime::Delta pto_delay =
      rtt_stats->smoothed_rtt() +
      kPtoRttvarMultiplier * rtt_stats->mean_deviation() +
      QuicTime::Delta::Zero();
  // Verify PTO is armed based on handshake data.
  EXPECT_EQ(packet2_sent_time + pto_delay, manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, MaybeRetransmitInitialData) {
  manager_.EnableMultiplePacketNumberSpacesSupport();
  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(10 * kDefaultTCPMSS));
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(100),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  QuicTime::Delta srtt = rtt_stats->smoothed_rtt();

  // Send packet 1.
  SendDataPacket(1, ENCRYPTION_INITIAL);
  QuicTime packet1_sent_time = clock_.Now();

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  // Send packets 2 and 3.
  SendDataPacket(2, ENCRYPTION_HANDSHAKE);
  QuicTime packet2_sent_time = clock_.Now();
  SendDataPacket(3, ENCRYPTION_HANDSHAKE);
  // Verify PTO is correctly set based on packet 1.
  QuicTime::Delta expected_pto_delay =
      srtt + kPtoRttvarMultiplier * rtt_stats->mean_deviation() +
      QuicTime::Delta::Zero();
  EXPECT_EQ(packet1_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Assume connection is going to send INITIAL ACK.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  EXPECT_CALL(notifier_, RetransmitFrames(_, _))
      .WillOnce(WithArgs<1>([this](TransmissionType type) {
        return RetransmitDataPacket(4, type, ENCRYPTION_INITIAL);
      }));
  manager_.RetransmitDataOfSpaceIfAny(INITIAL_DATA);
  // Verify PTO is re-armed based on packet 2.
  EXPECT_EQ(packet2_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());

  // Connection is going to send another INITIAL ACK.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  EXPECT_CALL(notifier_, RetransmitFrames(_, _))
      .WillOnce(WithArgs<1>([this](TransmissionType type) {
        return RetransmitDataPacket(5, type, ENCRYPTION_INITIAL);
      }));
  manager_.RetransmitDataOfSpaceIfAny(INITIAL_DATA);
  // Verify PTO does not change.
  EXPECT_EQ(packet2_sent_time + expected_pto_delay,
            manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, SendPathChallengeAndGetAck) {
  QuicPacketNumber packet_number(1);
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, BytesInFlight(), packet_number, _, _));
  SerializedPacket packet(packet_number, PACKET_4BYTE_PACKET_NUMBER, nullptr,
                          kDefaultLength, false, false);
  QuicPathFrameBuffer path_frame_buffer{0, 1, 2, 3, 4, 5, 6, 7};
  packet.nonretransmittable_frames.push_back(
      QuicFrame(QuicPathChallengeFrame(0, path_frame_buffer)));
  packet.encryption_level = ENCRYPTION_FORWARD_SECURE;
  manager_.OnPacketSent(&packet, clock_.Now(), NOT_RETRANSMISSION,
                        NO_RETRANSMITTABLE_DATA, false, ECN_NOT_ECT);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  EXPECT_CALL(
      *send_algorithm_,
      OnCongestionEvent(/*rtt_updated=*/false, _, _,
                        Pointwise(PacketNumberEq(), {1}), IsEmpty(), _, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());

  // Get ACK for the packet.
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_FORWARD_SECURE, kEmptyCounts));
}

SerializedPacket MakePacketWithAckFrequencyFrame(
    int packet_number, int ack_frequency_sequence_number,
    QuicTime::Delta max_ack_delay) {
  auto* ack_frequency_frame = new QuicAckFrequencyFrame();
  ack_frequency_frame->requested_max_ack_delay = max_ack_delay;
  ack_frequency_frame->sequence_number = ack_frequency_sequence_number;
  SerializedPacket packet(QuicPacketNumber(packet_number),
                          PACKET_4BYTE_PACKET_NUMBER, nullptr, kDefaultLength,
                          /*has_ack=*/false,
                          /*has_stop_waiting=*/false);
  packet.retransmittable_frames.push_back(QuicFrame(ack_frequency_frame));
  packet.has_ack_frequency = true;
  packet.encryption_level = ENCRYPTION_FORWARD_SECURE;
  return packet;
}

TEST_F(QuicSentPacketManagerTest,
       PeerMaxAckDelayUpdatedFromAckFrequencyFrameOneAtATime) {
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange())
      .Times(AnyNumber());

  auto initial_peer_max_ack_delay = manager_.peer_max_ack_delay();
  auto one_ms = QuicTime::Delta::FromMilliseconds(1);
  auto plus_1_ms_delay = initial_peer_max_ack_delay + one_ms;
  auto minus_1_ms_delay = initial_peer_max_ack_delay - one_ms;

  // Send and Ack frame1.
  SerializedPacket packet1 = MakePacketWithAckFrequencyFrame(
      /*packet_number=*/1, /*ack_frequency_sequence_number=*/1,
      plus_1_ms_delay);
  // Higher on the fly max_ack_delay changes peer_max_ack_delay.
  manager_.OnPacketSent(&packet1, clock_.Now(), NOT_RETRANSMISSION,
                        HAS_RETRANSMITTABLE_DATA, /*measure_rtt=*/true,
                        ECN_NOT_ECT);
  EXPECT_EQ(manager_.peer_max_ack_delay(), plus_1_ms_delay);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                         ENCRYPTION_FORWARD_SECURE, kEmptyCounts);
  EXPECT_EQ(manager_.peer_max_ack_delay(), plus_1_ms_delay);

  // Send and Ack frame2.
  SerializedPacket packet2 = MakePacketWithAckFrequencyFrame(
      /*packet_number=*/2, /*ack_frequency_sequence_number=*/2,
      minus_1_ms_delay);
  // Lower on the fly max_ack_delay does not change peer_max_ack_delay.
  manager_.OnPacketSent(&packet2, clock_.Now(), NOT_RETRANSMISSION,
                        HAS_RETRANSMITTABLE_DATA, /*measure_rtt=*/true,
                        ECN_NOT_ECT);
  EXPECT_EQ(manager_.peer_max_ack_delay(), plus_1_ms_delay);
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(3));
  manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(2),
                         ENCRYPTION_FORWARD_SECURE, kEmptyCounts);
  EXPECT_EQ(manager_.peer_max_ack_delay(), minus_1_ms_delay);
}

TEST_F(QuicSentPacketManagerTest,
       PeerMaxAckDelayUpdatedFromInOrderAckFrequencyFrames) {
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange())
      .Times(AnyNumber());

  auto initial_peer_max_ack_delay = manager_.peer_max_ack_delay();
  auto one_ms = QuicTime::Delta::FromMilliseconds(1);
  auto extra_1_ms = initial_peer_max_ack_delay + one_ms;
  auto extra_2_ms = initial_peer_max_ack_delay + 2 * one_ms;
  auto extra_3_ms = initial_peer_max_ack_delay + 3 * one_ms;
  SerializedPacket packet1 = MakePacketWithAckFrequencyFrame(
      /*packet_number=*/1, /*ack_frequency_sequence_number=*/1, extra_1_ms);
  SerializedPacket packet2 = MakePacketWithAckFrequencyFrame(
      /*packet_number=*/2, /*ack_frequency_sequence_number=*/2, extra_3_ms);
  SerializedPacket packet3 = MakePacketWithAckFrequencyFrame(
      /*packet_number=*/3, /*ack_frequency_sequence_number=*/3, extra_2_ms);

  // Send frame1, farme2, frame3.
  manager_.OnPacketSent(&packet1, clock_.Now(), NOT_RETRANSMISSION,
                        HAS_RETRANSMITTABLE_DATA, /*measure_rtt=*/true,
                        ECN_NOT_ECT);
  EXPECT_EQ(manager_.peer_max_ack_delay(), extra_1_ms);
  manager_.OnPacketSent(&packet2, clock_.Now(), NOT_RETRANSMISSION,
                        HAS_RETRANSMITTABLE_DATA, /*measure_rtt=*/true,
                        ECN_NOT_ECT);
  EXPECT_EQ(manager_.peer_max_ack_delay(), extra_3_ms);
  manager_.OnPacketSent(&packet3, clock_.Now(), NOT_RETRANSMISSION,
                        HAS_RETRANSMITTABLE_DATA, /*measure_rtt=*/true,
                        ECN_NOT_ECT);
  EXPECT_EQ(manager_.peer_max_ack_delay(), extra_3_ms);

  // Ack frame1, farme2, frame3.
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                         ENCRYPTION_FORWARD_SECURE, kEmptyCounts);
  EXPECT_EQ(manager_.peer_max_ack_delay(), extra_3_ms);
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(3));
  manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                         ENCRYPTION_FORWARD_SECURE, kEmptyCounts);
  EXPECT_EQ(manager_.peer_max_ack_delay(), extra_3_ms);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(4));
  manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                         ENCRYPTION_FORWARD_SECURE, kEmptyCounts);
  EXPECT_EQ(manager_.peer_max_ack_delay(), extra_2_ms);
}

TEST_F(QuicSentPacketManagerTest,
       PeerMaxAckDelayUpdatedFromOutOfOrderAckedAckFrequencyFrames) {
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange())
      .Times(AnyNumber());

  auto initial_peer_max_ack_delay = manager_.peer_max_ack_delay();
  auto one_ms = QuicTime::Delta::FromMilliseconds(1);
  auto extra_1_ms = initial_peer_max_ack_delay + one_ms;
  auto extra_2_ms = initial_peer_max_ack_delay + 2 * one_ms;
  auto extra_3_ms = initial_peer_max_ack_delay + 3 * one_ms;
  auto extra_4_ms = initial_peer_max_ack_delay + 4 * one_ms;
  SerializedPacket packet1 = MakePacketWithAckFrequencyFrame(
      /*packet_number=*/1, /*ack_frequency_sequence_number=*/1, extra_4_ms);
  SerializedPacket packet2 = MakePacketWithAckFrequencyFrame(
      /*packet_number=*/2, /*ack_frequency_sequence_number=*/2, extra_3_ms);
  SerializedPacket packet3 = MakePacketWithAckFrequencyFrame(
      /*packet_number=*/3, /*ack_frequency_sequence_number=*/3, extra_2_ms);
  SerializedPacket packet4 = MakePacketWithAckFrequencyFrame(
      /*packet_number=*/4, /*ack_frequency_sequence_number=*/4, extra_1_ms);

  // Send frame1, farme2, frame3, frame4.
  manager_.OnPacketSent(&packet1, clock_.Now(), NOT_RETRANSMISSION,
                        HAS_RETRANSMITTABLE_DATA, /*measure_rtt=*/true,
                        ECN_NOT_ECT);
  manager_.OnPacketSent(&packet2, clock_.Now(), NOT_RETRANSMISSION,
                        HAS_RETRANSMITTABLE_DATA, /*measure_rtt=*/true,
                        ECN_NOT_ECT);
  manager_.OnPacketSent(&packet3, clock_.Now(), NOT_RETRANSMISSION,
                        HAS_RETRANSMITTABLE_DATA, /*measure_rtt=*/true,
                        ECN_NOT_ECT);
  manager_.OnPacketSent(&packet4, clock_.Now(), NOT_RETRANSMISSION,
                        NO_RETRANSMITTABLE_DATA, /*measure_rtt=*/true,
                        ECN_NOT_ECT);
  EXPECT_EQ(manager_.peer_max_ack_delay(), extra_4_ms);

  // Ack frame3.
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(4));
  manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                         ENCRYPTION_FORWARD_SECURE, kEmptyCounts);
  EXPECT_EQ(manager_.peer_max_ack_delay(), extra_2_ms);
  // Acking frame1 do not affect peer_max_ack_delay after frame3 is acked.
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(4));
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                         ENCRYPTION_FORWARD_SECURE, kEmptyCounts);
  EXPECT_EQ(manager_.peer_max_ack_delay(), extra_2_ms);
  // Acking frame2 do not affect peer_max_ack_delay after frame3 is acked.
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(4));
  manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                         ENCRYPTION_FORWARD_SECURE, kEmptyCounts);
  EXPECT_EQ(manager_.peer_max_ack_delay(), extra_2_ms);
  // Acking frame4 updates peer_max_ack_delay.
  manager_.OnAckFrameStart(QuicPacketNumber(4), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(5));
  manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                         ENCRYPTION_FORWARD_SECURE, kEmptyCounts);
  EXPECT_EQ(manager_.peer_max_ack_delay(), extra_1_ms);
}

TEST_F(QuicSentPacketManagerTest, ClearDataInDatagramFrameAfterPacketSent) {
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);

  QuicDatagramFrame* datagram_frame = nullptr;
  {
    quiche::QuicheMemSlice slice(quiche::QuicheBuffer(&allocator_, 1024));
    datagram_frame = new QuicDatagramFrame(/*datagram_id=*/1, std::move(slice));
    EXPECT_FALSE(datagram_frame->datagram_data.empty());
    EXPECT_EQ(datagram_frame->datagram_length, 1024);

    SerializedPacket packet(QuicPacketNumber(1), PACKET_4BYTE_PACKET_NUMBER,
                            /*encrypted_buffer=*/nullptr, kDefaultLength,
                            /*has_ack=*/false,
                            /*has_stop_waiting*/ false);
    packet.encryption_level = ENCRYPTION_FORWARD_SECURE;
    packet.retransmittable_frames.push_back(QuicFrame(datagram_frame));
    packet.has_datagram = true;
    manager_.OnPacketSent(&packet, clock_.Now(), NOT_RETRANSMISSION,
                          HAS_RETRANSMITTABLE_DATA, /*measure_rtt=*/true,
                          ECN_NOT_ECT);
  }

  EXPECT_TRUE(datagram_frame->datagram_data.empty());
  EXPECT_EQ(datagram_frame->datagram_length, 0);
}

// TODO(b/389762349): Re-enable these tests when sending AckFrequency is
// restored.
#if 0
TEST_F(QuicSentPacketManagerTest, BuildAckFrequencyFrame) {
  SetQuicReloadableFlag(quic_can_send_ack_frequency, true);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  QuicConfig config;
  QuicConfigPeer::SetReceivedMinAckDelayMs(&config, /*min_ack_delay_ms=*/1);
  manager_.SetFromConfig(config);
  manager_.SetHandshakeConfirmed();

  // Set up RTTs.
  auto* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(80),
                       /*ack_delay=*/QuicTime::Delta::Zero(),
                       /*now=*/QuicTime::Zero());
  // Make sure srtt and min_rtt are different.
  rtt_stats->UpdateRtt(
      QuicTime::Delta::FromMilliseconds(160),
      /*ack_delay=*/QuicTime::Delta::Zero(),
      /*now=*/QuicTime::Zero() + QuicTime::Delta::FromMilliseconds(24));

  auto frame = manager_.GetUpdatedAckFrequencyFrame();
  EXPECT_EQ(frame.requested_max_ack_delay,
            std::max(rtt_stats->min_rtt() * 0.25,
                     QuicTime::Delta::FromMilliseconds(1u)));
  EXPECT_EQ(frame.ack_eliciting_threshold, 10u);
}
#endif

TEST_F(QuicSentPacketManagerTest, SmoothedRttIgnoreAckDelay) {
  QuicConfig config;
  QuicTagVector options;
  options.push_back(kMAD0);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(10 * kDefaultTCPMSS));
  manager_.SetFromConfig(config);

  SendDataPacket(1);
  // Ack 1.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(300));
  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1),
                           QuicTime::Delta::FromMilliseconds(100),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  // Verify that ack_delay is ignored in the first measurement.
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(300),
            manager_.GetRttStats()->latest_rtt());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(300),
            manager_.GetRttStats()->smoothed_rtt());

  SendDataPacket(2);
  // Ack 2.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(300));
  ExpectAck(2);
  manager_.OnAckFrameStart(QuicPacketNumber(2),
                           QuicTime::Delta::FromMilliseconds(100),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(2),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(300),
            manager_.GetRttStats()->latest_rtt());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(300),
            manager_.GetRttStats()->smoothed_rtt());

  SendDataPacket(3);
  // Ack 3.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(300));
  ExpectAck(3);
  manager_.OnAckFrameStart(QuicPacketNumber(3),
                           QuicTime::Delta::FromMilliseconds(50), clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(4));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(3),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(300),
            manager_.GetRttStats()->latest_rtt());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(300),
            manager_.GetRttStats()->smoothed_rtt());

  SendDataPacket(4);
  // Ack 4.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(200));
  ExpectAck(4);
  manager_.OnAckFrameStart(QuicPacketNumber(4),
                           QuicTime::Delta::FromMilliseconds(300),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(4), QuicPacketNumber(5));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(4),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  // Verify that large erroneous ack_delay does not change Smoothed RTT.
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(200),
            manager_.GetRttStats()->latest_rtt());
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(287500),
            manager_.GetRttStats()->smoothed_rtt());
}

TEST_F(QuicSentPacketManagerTest, IgnorePeerMaxAckDelayDuringHandshake) {
  manager_.EnableMultiplePacketNumberSpacesSupport();
  // 100ms RTT.
  const QuicTime::Delta kTestRTT = QuicTime::Delta::FromMilliseconds(100);

  // Server sends INITIAL 1 and HANDSHAKE 2.
  SendDataPacket(1, ENCRYPTION_INITIAL);
  SendDataPacket(2, ENCRYPTION_HANDSHAKE);

  // Receive client ACK for INITIAL 1 after one RTT.
  clock_.AdvanceTime(kTestRTT);
  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_INITIAL, kEmptyCounts));
  EXPECT_EQ(kTestRTT, manager_.GetRttStats()->latest_rtt());

  // Assume the cert verification on client takes 50ms, such that the HANDSHAKE
  // packet is queued for 50ms.
  const QuicTime::Delta queuing_delay = QuicTime::Delta::FromMilliseconds(50);
  clock_.AdvanceTime(queuing_delay);
  // Ack 2.
  ExpectAck(2);
  manager_.OnAckFrameStart(QuicPacketNumber(2), queuing_delay, clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(2),
                                   ENCRYPTION_HANDSHAKE, kEmptyCounts));
  EXPECT_EQ(kTestRTT, manager_.GetRttStats()->latest_rtt());
}

// TODO(b/389762349): Re-enable these tests when sending AckFrequency is
// restored.
#if 0
TEST_F(QuicSentPacketManagerTest, BuildAckFrequencyFrameWithSRTT) {
  SetQuicReloadableFlag(quic_can_send_ack_frequency, true);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  QuicConfig config;
  QuicConfigPeer::SetReceivedMinAckDelayMs(&config, /*min_ack_delay_ms=*/1);
  QuicTagVector quic_tag_vector;
  quic_tag_vector.push_back(kAFF1);  // SRTT enabling tag.
  QuicConfigPeer::SetReceivedConnectionOptions(&config, quic_tag_vector);
  manager_.SetFromConfig(config);
  manager_.SetHandshakeConfirmed();

  // Set up RTTs.
  auto* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(80),
                       /*ack_delay=*/QuicTime::Delta::Zero(),
                       /*now=*/QuicTime::Zero());
  // Make sure srtt and min_rtt are different.
  rtt_stats->UpdateRtt(
      QuicTime::Delta::FromMilliseconds(160),
      /*ack_delay=*/QuicTime::Delta::Zero(),
      /*now=*/QuicTime::Zero() + QuicTime::Delta::FromMilliseconds(24));

  auto frame = manager_.GetUpdatedAckFrequencyFrame();
  EXPECT_EQ(frame.requested_max_ack_delay,
            std::max(rtt_stats->SmoothedOrInitialRtt() * 0.25,
                     QuicTime::Delta::FromMilliseconds(1u)));
}
#endif

TEST_F(QuicSentPacketManagerTest, SetInitialRtt) {
  // Upper bounds.
  manager_.SetInitialRtt(
      QuicTime::Delta::FromMicroseconds(kMaxInitialRoundTripTimeUs + 1), false);
  EXPECT_EQ(manager_.GetRttStats()->initial_rtt().ToMicroseconds(),
            kMaxInitialRoundTripTimeUs);

  manager_.SetInitialRtt(
      QuicTime::Delta::FromMicroseconds(kMaxInitialRoundTripTimeUs + 1), true);
  EXPECT_EQ(manager_.GetRttStats()->initial_rtt().ToMicroseconds(),
            kMaxInitialRoundTripTimeUs);

  EXPECT_GT(kMinUntrustedInitialRoundTripTimeUs,
            kMinTrustedInitialRoundTripTimeUs);

  // Lower bounds for untrusted rtt.
  manager_.SetInitialRtt(QuicTime::Delta::FromMicroseconds(
                             kMinUntrustedInitialRoundTripTimeUs - 1),
                         false);
  EXPECT_EQ(manager_.GetRttStats()->initial_rtt().ToMicroseconds(),
            kMinUntrustedInitialRoundTripTimeUs);

  // Lower bounds for trusted rtt.
  manager_.SetInitialRtt(QuicTime::Delta::FromMicroseconds(
                             kMinUntrustedInitialRoundTripTimeUs - 1),
                         true);
  EXPECT_EQ(manager_.GetRttStats()->initial_rtt().ToMicroseconds(),
            kMinUntrustedInitialRoundTripTimeUs - 1);

  manager_.SetInitialRtt(
      QuicTime::Delta::FromMicroseconds(kMinTrustedInitialRoundTripTimeUs - 1),
      true);
  EXPECT_EQ(manager_.GetRttStats()->initial_rtt().ToMicroseconds(),
            kMinTrustedInitialRoundTripTimeUs);
}

TEST_F(QuicSentPacketManagerTest, GetAvailableCongestionWindow) {
  SendDataPacket(1);
  EXPECT_EQ(kDefaultLength, manager_.GetBytesInFlight());

  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillOnce(Return(kDefaultLength + 10));
  EXPECT_EQ(10u, manager_.GetAvailableCongestionWindowInBytes());

  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillOnce(Return(kDefaultLength));
  EXPECT_EQ(0u, manager_.GetAvailableCongestionWindowInBytes());

  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillOnce(Return(kDefaultLength - 10));
  EXPECT_EQ(0u, manager_.GetAvailableCongestionWindowInBytes());
}

TEST_F(QuicSentPacketManagerTest, EcnCountsAreStored) {
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(true));
  manager_.EnableECT1();
  std::optional<QuicEcnCounts> ecn_counts1, ecn_counts2, ecn_counts3;
  ecn_counts1 = {1, 0, 3};
  ecn_counts2 = {0, 3, 1};
  ecn_counts3 = {0, 2, 0};
  SendDataPacket(1, ENCRYPTION_INITIAL, ECN_ECT0);
  SendDataPacket(2, ENCRYPTION_INITIAL, ECN_ECT0);
  SendDataPacket(3, ENCRYPTION_INITIAL, ECN_ECT0);
  SendDataPacket(4, ENCRYPTION_INITIAL, ECN_ECT0);
  SendDataPacket(5, ENCRYPTION_HANDSHAKE, ECN_ECT1);
  SendDataPacket(6, ENCRYPTION_HANDSHAKE, ECN_ECT1);
  SendDataPacket(7, ENCRYPTION_HANDSHAKE, ECN_ECT1);
  SendDataPacket(8, ENCRYPTION_HANDSHAKE, ECN_ECT1);
  SendDataPacket(9, ENCRYPTION_FORWARD_SECURE, ECN_ECT1);
  SendDataPacket(10, ENCRYPTION_FORWARD_SECURE, ECN_ECT1);
  MockDebugDelegate debug_delegate;
  manager_.SetDebugDelegate(&debug_delegate);
  bool correct_report = false;
  EXPECT_CALL(debug_delegate, OnIncomingAck(_, _, _, _, _, _, _))
      .WillOnce([&](QuicPacketNumber /*ack_packet_number*/,
                    EncryptionLevel /*ack_decrypted_level*/,
                    const QuicAckFrame& ack_frame,
                    QuicTime /*ack_receive_time*/,
                    QuicPacketNumber /*largest_observed*/, bool /*rtt_updated*/,
                    QuicPacketNumber /*least_unacked_sent_packet*/) {
        correct_report = (ack_frame.ecn_counters == ecn_counts1);
      });
  manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1), ENCRYPTION_INITIAL,
                         ecn_counts1);
  EXPECT_TRUE(correct_report);
  correct_report = false;
  EXPECT_CALL(debug_delegate, OnIncomingAck(_, _, _, _, _, _, _))
      .WillOnce([&](QuicPacketNumber /*ack_packet_number*/,
                    EncryptionLevel /*ack_decrypted_level*/,
                    const QuicAckFrame& ack_frame,
                    QuicTime /*ack_receive_time*/,
                    QuicPacketNumber /*largest_observed*/, bool /*rtt_updated*/,
                    QuicPacketNumber /*least_unacked_sent_packet*/) {
        correct_report = (ack_frame.ecn_counters == ecn_counts2);
      });
  manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(2),
                         ENCRYPTION_HANDSHAKE, ecn_counts2);
  EXPECT_TRUE(correct_report);
  correct_report = false;
  EXPECT_CALL(debug_delegate, OnIncomingAck(_, _, _, _, _, _, _))
      .WillOnce([&](QuicPacketNumber /*ack_packet_number*/,
                    EncryptionLevel /*ack_decrypted_level*/,
                    const QuicAckFrame& ack_frame,
                    QuicTime /*ack_receive_time*/,
                    QuicPacketNumber /*largest_observed*/, bool /*rtt_updated*/,
                    QuicPacketNumber /*least_unacked_sent_packet*/) {
        correct_report = (ack_frame.ecn_counters == ecn_counts3);
      });
  manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(3),
                         ENCRYPTION_FORWARD_SECURE, ecn_counts3);
  EXPECT_TRUE(correct_report);
  EXPECT_EQ(
      *QuicSentPacketManagerPeer::GetPeerEcnCounts(&manager_, INITIAL_DATA),
      ecn_counts1);
  EXPECT_EQ(
      *QuicSentPacketManagerPeer::GetPeerEcnCounts(&manager_, HANDSHAKE_DATA),
      ecn_counts2);
  EXPECT_EQ(
      *QuicSentPacketManagerPeer::GetPeerEcnCounts(&manager_, APPLICATION_DATA),
      ecn_counts3);
}

TEST_F(QuicSentPacketManagerTest, EcnCountsReceived) {
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(true));
  manager_.EnableECT1();
  // Basic ECN reporting test. The reported counts are equal to the total sent,
  // but more than the total acked. This is legal per the spec.
  for (uint64_t i = 1; i <= 3; ++i) {
    SendDataPacket(i, ENCRYPTION_FORWARD_SECURE, ECN_ECT1);
  }
  // Ack the last two packets, but report 3 counts (ack of 1 was lost).
  EXPECT_CALL(*network_change_visitor_, OnInFlightEcnPacketAcked()).Times(2);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(4));
  EXPECT_CALL(*send_algorithm_,
              OnCongestionEvent(_, _, _, Pointwise(PacketNumberEq(), {2, 3}),
                                IsEmpty(), 2, 1))
      .Times(1);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange()).Times(1);
  std::optional<QuicEcnCounts> ecn_counts = QuicEcnCounts();
  ecn_counts->ect1 = QuicPacketCount(2);
  ecn_counts->ce = QuicPacketCount(1);
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_FORWARD_SECURE, ecn_counts));
}

TEST_F(QuicSentPacketManagerTest, PeerDecrementsEcnCounts) {
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(true));
  manager_.EnableECT1();
  for (uint64_t i = 1; i <= 5; ++i) {
    SendDataPacket(i, ENCRYPTION_FORWARD_SECURE, ECN_ECT1);
  }
  // Ack all three packets).
  EXPECT_CALL(*network_change_visitor_, OnInFlightEcnPacketAcked()).Times(3);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(4));
  EXPECT_CALL(*send_algorithm_,
              OnCongestionEvent(_, _, _, Pointwise(PacketNumberEq(), {1, 2, 3}),
                                IsEmpty(), 2, 1))
      .Times(1);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange()).Times(1);
  std::optional<QuicEcnCounts> ecn_counts = QuicEcnCounts();
  ecn_counts->ect1 = QuicPacketCount(2);
  ecn_counts->ce = QuicPacketCount(1);
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_FORWARD_SECURE, ecn_counts));
  // New ack, counts decline
  EXPECT_CALL(*network_change_visitor_, OnInFlightEcnPacketAcked()).Times(1);
  manager_.OnAckFrameStart(QuicPacketNumber(4), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(4), QuicPacketNumber(5));
  EXPECT_CALL(*network_change_visitor_, OnInvalidEcnFeedback());
  EXPECT_CALL(*send_algorithm_,
              OnCongestionEvent(_, _, _, Pointwise(PacketNumberEq(), {4}),
                                IsEmpty(), 0, 0))
      .Times(1);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange()).Times(1);
  ecn_counts = QuicEcnCounts();
  ecn_counts->ect1 = QuicPacketCount(3);
  ecn_counts->ce = QuicPacketCount(0);  // Reduced CE count
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(2),
                                   ENCRYPTION_FORWARD_SECURE, ecn_counts));
}

TEST_F(QuicSentPacketManagerTest, TooManyEcnCountsReported) {
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(true));
  manager_.EnableECT1();
  for (uint64_t i = 1; i <= 3; ++i) {
    SendDataPacket(i, ENCRYPTION_FORWARD_SECURE, ECN_ECT1);
  }
  // Ack the last two packets, but report 3 counts (ack of 1 was lost).
  EXPECT_CALL(*network_change_visitor_, OnInFlightEcnPacketAcked()).Times(2);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(4));
  std::optional<QuicEcnCounts> ecn_counts = QuicEcnCounts();
  // Report 4 counts, but only 3 packets were sent.
  ecn_counts->ect1 = QuicPacketCount(3);
  ecn_counts->ce = QuicPacketCount(1);
  EXPECT_CALL(*network_change_visitor_, OnInvalidEcnFeedback());
  EXPECT_CALL(*send_algorithm_,
              OnCongestionEvent(_, _, _, Pointwise(PacketNumberEq(), {2, 3}),
                                IsEmpty(), 0, 0))
      .Times(1);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange()).Times(1);

  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_FORWARD_SECURE, ecn_counts));
}

TEST_F(QuicSentPacketManagerTest, PeerReportsWrongCodepoint) {
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(true));
  manager_.EnableECT1();
  for (uint64_t i = 1; i <= 3; ++i) {
    SendDataPacket(i, ENCRYPTION_FORWARD_SECURE, ECN_ECT1);
  }
  // Ack the last two packets, but report 3 counts (ack of 1 was lost).
  EXPECT_CALL(*network_change_visitor_, OnInFlightEcnPacketAcked()).Times(2);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(4));
  std::optional<QuicEcnCounts> ecn_counts = QuicEcnCounts();
  // Report the wrong codepoint.
  ecn_counts->ect0 = QuicPacketCount(2);
  ecn_counts->ce = QuicPacketCount(1);
  EXPECT_CALL(*network_change_visitor_, OnInvalidEcnFeedback());
  EXPECT_CALL(*send_algorithm_,
              OnCongestionEvent(_, _, _, Pointwise(PacketNumberEq(), {2, 3}),
                                IsEmpty(), 0, 0))
      .Times(1);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange()).Times(1);

  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_FORWARD_SECURE, ecn_counts));
}

TEST_F(QuicSentPacketManagerTest, TooFewEcnCountsReported) {
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(true));
  manager_.EnableECT1();
  for (uint64_t i = 1; i <= 3; ++i) {
    SendDataPacket(i, ENCRYPTION_FORWARD_SECURE, ECN_ECT1);
  }
  // Ack the last two packets, but report 3 counts (ack of 1 was lost).
  EXPECT_CALL(*network_change_visitor_, OnInFlightEcnPacketAcked()).Times(2);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(4));
  EXPECT_CALL(*network_change_visitor_, OnInvalidEcnFeedback());
  EXPECT_CALL(*send_algorithm_,
              OnCongestionEvent(_, _, _, Pointwise(PacketNumberEq(), {2, 3}),
                                IsEmpty(), 0, 0))
      .Times(1);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange()).Times(1);
  std::optional<QuicEcnCounts> ecn_counts = QuicEcnCounts();
  // 2 ECN packets were newly acked, but only one count was reported.
  ecn_counts->ect1 = QuicPacketCount(1);
  ecn_counts->ce = QuicPacketCount(0);
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_FORWARD_SECURE, ecn_counts));
}

TEST_F(QuicSentPacketManagerTest,
       EcnCountsNotValidatedIfLargestAckedUnchanged) {
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(true));
  manager_.EnableECT1();
  for (uint64_t i = 1; i <= 3; ++i) {
    SendDataPacket(i, ENCRYPTION_FORWARD_SECURE, ECN_ECT1);
  }
  // Ack two packets.
  EXPECT_CALL(*network_change_visitor_, OnInFlightEcnPacketAcked()).Times(2);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(4));
  EXPECT_CALL(*send_algorithm_,
              OnCongestionEvent(_, _, _, Pointwise(PacketNumberEq(), {2, 3}),
                                IsEmpty(), 2, 1))
      .Times(1);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange()).Times(1);
  std::optional<QuicEcnCounts> ecn_counts = QuicEcnCounts();
  ecn_counts->ect1 = QuicPacketCount(2);
  ecn_counts->ce = QuicPacketCount(1);
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_FORWARD_SECURE, ecn_counts));
  // Ack the first packet, which will not update largest_acked.
  EXPECT_CALL(*network_change_visitor_, OnInFlightEcnPacketAcked()).Times(1);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(4));
  EXPECT_CALL(*send_algorithm_,
              OnCongestionEvent(_, _, _, Pointwise(PacketNumberEq(), {1}),
                                IsEmpty(), 0, 0))
      .Times(1);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange()).Times(1);
  ecn_counts = QuicEcnCounts();
  // Counts decline, but there's no validation because largest_acked didn't
  // change.
  ecn_counts->ect1 = QuicPacketCount(2);
  ecn_counts->ce = QuicPacketCount(0);  // Reduced CE count
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(2),
                                   ENCRYPTION_FORWARD_SECURE, ecn_counts));
}

TEST_F(QuicSentPacketManagerTest, EcnAckedButNoMarksReported) {
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(true));
  manager_.EnableECT1();
  for (uint64_t i = 1; i <= 3; ++i) {
    SendDataPacket(i, ENCRYPTION_FORWARD_SECURE, ECN_ECT1);
  }
  // Ack the last two packets, but report 3 counts (ack of 1 was lost).
  EXPECT_CALL(*network_change_visitor_, OnInFlightEcnPacketAcked()).Times(2);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(4));
  EXPECT_CALL(*network_change_visitor_, OnInvalidEcnFeedback());
  EXPECT_CALL(*send_algorithm_,
              OnCongestionEvent(_, _, _, Pointwise(PacketNumberEq(), {2, 3}),
                                IsEmpty(), 0, 0))
      .Times(1);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange()).Times(1);
  std::optional<QuicEcnCounts> ecn_counts = std::nullopt;
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), QuicPacketNumber(1),
                                   ENCRYPTION_FORWARD_SECURE, ecn_counts));
}

// Test that the path degrading delay is set correctly when the path degrading
// connection option is set.
TEST_F(QuicSentPacketManagerTest, GetPathDegradingDelayUsingPTO) {
  QuicConfig client_config;
  QuicTagVector all_path_dergradation_options = {kPDE2, kPDE3, kPDE5};
  uint8_t pto_count = 2;
  for (QuicTag current_dergradation_option : all_path_dergradation_options) {
    QuicTagVector client_options;
    client_options.push_back(current_dergradation_option);
    QuicSentPacketManagerPeer::SetPerspective(&manager_,
                                              Perspective::IS_CLIENT);
    client_config.SetClientConnectionOptions(client_options);
    EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
    EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
    manager_.SetFromConfig(client_config);
    QuicTime::Delta expected_delay = pto_count * manager_.GetPtoDelay();
    EXPECT_EQ(expected_delay, manager_.GetPathDegradingDelay());
    pto_count++;
    if (pto_count == 4) {
      pto_count++;
    }
  }
}

static constexpr float kDefaultOverhead = 0.05f;

TEST_F(QuicSentPacketManagerTest, DefaultOverhead) {
  manager_.EnableOverheadMeasurement();
  EXPECT_NEAR(manager_.GetOverheadEstimate(), kDefaultOverhead, 1e-6);
}

TEST_F(QuicSentPacketManagerTest, OverheadFromStreamFrames) {
  manager_.EnableOverheadMeasurement();
  EXPECT_CALL(*send_algorithm_, OnPacketSent).Times(AnyNumber());
  std::string buffer(kDefaultLength / 2, '\0');
  for (int i = 1; i < 1000; ++i) {
    SerializedPacket packet(QuicPacketNumber(i), PACKET_4BYTE_PACKET_NUMBER,
                            nullptr, kDefaultLength, false, false);
    packet.encryption_level = ENCRYPTION_FORWARD_SECURE;
    packet.retransmittable_frames.push_back(
        QuicFrame(QuicStreamFrame(kStreamId, false, 0, buffer)));
    manager_.OnPacketSent(&packet, clock_.Now(), NOT_RETRANSMISSION,
                          HAS_RETRANSMITTABLE_DATA, true, ECN_NOT_ECT);
  }
  EXPECT_NEAR(manager_.GetOverheadEstimate(), 0.5, 0.01);
}

TEST_F(QuicSentPacketManagerTest, OverheadFromDatagramFrames) {
  manager_.EnableOverheadMeasurement();
  EXPECT_CALL(*send_algorithm_, OnPacketSent).Times(AnyNumber());
  std::string buffer(kDefaultLength / 2, '\0');
  for (int i = 1; i < 1000; ++i) {
    SerializedPacket packet(QuicPacketNumber(i), PACKET_4BYTE_PACKET_NUMBER,
                            nullptr, kDefaultLength, false, false);
    packet.encryption_level = ENCRYPTION_FORWARD_SECURE;
    packet.retransmittable_frames.push_back(QuicFrame(
        new QuicDatagramFrame(i, quiche::QuicheMemSlice::Copy(buffer))));
    manager_.OnPacketSent(&packet, clock_.Now(), NOT_RETRANSMISSION,
                          HAS_RETRANSMITTABLE_DATA, true, ECN_NOT_ECT);
  }
  EXPECT_NEAR(manager_.GetOverheadEstimate(), 0.5, 0.01);
}

TEST_F(QuicSentPacketManagerTest, IgnoreNon1RttFrames) {
  manager_.EnableOverheadMeasurement();
  EXPECT_CALL(*send_algorithm_, OnPacketSent).Times(AnyNumber());
  std::string buffer(kDefaultLength / 2, '\0');
  for (int i = 1; i < 1000; ++i) {
    SerializedPacket packet(QuicPacketNumber(i), PACKET_4BYTE_PACKET_NUMBER,
                            nullptr, kDefaultLength, false, false);
    packet.encryption_level = ENCRYPTION_INITIAL;
    packet.retransmittable_frames.push_back(
        QuicFrame(QuicStreamFrame(kStreamId, false, 0, buffer)));
    manager_.OnPacketSent(&packet, clock_.Now(), NOT_RETRANSMISSION,
                          HAS_RETRANSMITTABLE_DATA, true, ECN_NOT_ECT);
  }
  EXPECT_NEAR(manager_.GetOverheadEstimate(), kDefaultOverhead, 1e-6);
}

}  // namespace
}  // namespace test
}  // namespace quic
