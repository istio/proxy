// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_unacked_packet_map.h"

#include <stdbool.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "absl/base/macros.h"
#include "quiche/quic/core/frames/quic_stream_frame.h"
#include "quiche/quic/core/quic_packet_number.h"
#include "quiche/quic/core/quic_transmission_info.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/quic/test_tools/quic_unacked_packet_map_peer.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace quic {
namespace test {
namespace {

// Default packet length.
const uint32_t kDefaultLength = 1000;

class QuicUnackedPacketMapTest : public QuicTestWithParam<Perspective> {
 protected:
  QuicUnackedPacketMapTest()
      : unacked_packets_(GetParam()),
        now_(QuicTime::Zero() + QuicTime::Delta::FromMilliseconds(1000)) {
    unacked_packets_.SetSessionNotifier(&notifier_);
    EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(notifier_, OnStreamFrameRetransmitted(_))
        .Times(testing::AnyNumber());
  }

  ~QuicUnackedPacketMapTest() override {}

  SerializedPacket CreateRetransmittablePacket(uint64_t packet_number) {
    return CreateRetransmittablePacketForStream(
        packet_number, QuicUtils::GetFirstBidirectionalStreamId(
                           CurrentSupportedVersions()[0].transport_version,
                           Perspective::IS_CLIENT));
  }

  SerializedPacket CreateRetransmittablePacketForStream(
      uint64_t packet_number, QuicStreamId stream_id) {
    return CreateRetransmittablePacketForStream(packet_number, stream_id,
                                                /*fin=*/false, /*offset=*/0,
                                                /*data_length=*/0);
  }

  SerializedPacket CreateRetransmittablePacketForStream(
      uint64_t packet_number, QuicStreamId stream_id, bool fin,
      QuicStreamOffset offset, QuicPacketLength data_length) {
    SerializedPacket packet(QuicPacketNumber(packet_number),
                            PACKET_1BYTE_PACKET_NUMBER, nullptr, kDefaultLength,
                            false, false);
    QuicStreamFrame frame(stream_id, fin, offset, data_length);
    packet.retransmittable_frames.push_back(QuicFrame(frame));
    return packet;
  }

  SerializedPacket CreateRetransmittablePacket(
      uint64_t packet_number, QuicFrames retransmittable_frames) {
    SerializedPacket packet(QuicPacketNumber(packet_number),
                            PACKET_1BYTE_PACKET_NUMBER, nullptr, kDefaultLength,
                            false, false);
    packet.retransmittable_frames = std::move(retransmittable_frames);
    return packet;
  }

  SerializedPacket CreateNonRetransmittablePacket(uint64_t packet_number) {
    return SerializedPacket(QuicPacketNumber(packet_number),
                            PACKET_1BYTE_PACKET_NUMBER, nullptr, kDefaultLength,
                            false, false);
  }

  void VerifyInFlightPackets(uint64_t* packets, size_t num_packets) {
    unacked_packets_.RemoveObsoletePackets();
    if (num_packets == 0) {
      EXPECT_FALSE(unacked_packets_.HasInFlightPackets());
      EXPECT_FALSE(unacked_packets_.HasMultipleInFlightPackets());
      return;
    }
    if (num_packets == 1) {
      EXPECT_TRUE(unacked_packets_.HasInFlightPackets());
      EXPECT_FALSE(unacked_packets_.HasMultipleInFlightPackets());
      ASSERT_TRUE(unacked_packets_.IsUnacked(QuicPacketNumber(packets[0])));
      EXPECT_TRUE(
          unacked_packets_.GetTransmissionInfo(QuicPacketNumber(packets[0]))
              .in_flight);
    }
    for (size_t i = 0; i < num_packets; ++i) {
      ASSERT_TRUE(unacked_packets_.IsUnacked(QuicPacketNumber(packets[i])));
      EXPECT_TRUE(
          unacked_packets_.GetTransmissionInfo(QuicPacketNumber(packets[i]))
              .in_flight);
    }
    size_t in_flight_count = 0;
    for (auto it = unacked_packets_.begin(); it != unacked_packets_.end();
         ++it) {
      if (it->in_flight) {
        ++in_flight_count;
      }
    }
    EXPECT_EQ(num_packets, in_flight_count);
  }

  void VerifyUnackedPackets(uint64_t* packets, size_t num_packets) {
    unacked_packets_.RemoveObsoletePackets();
    if (num_packets == 0) {
      EXPECT_TRUE(unacked_packets_.empty());
      EXPECT_FALSE(unacked_packets_.HasUnackedRetransmittableFrames());
      return;
    }
    EXPECT_FALSE(unacked_packets_.empty());
    for (size_t i = 0; i < num_packets; ++i) {
      EXPECT_TRUE(unacked_packets_.IsUnacked(QuicPacketNumber(packets[i])))
          << packets[i];
    }
    EXPECT_EQ(num_packets, unacked_packets_.GetNumUnackedPacketsDebugOnly());
  }

  void VerifyRetransmittablePackets(uint64_t* packets, size_t num_packets) {
    unacked_packets_.RemoveObsoletePackets();
    size_t num_retransmittable_packets = 0;
    for (auto it = unacked_packets_.begin(); it != unacked_packets_.end();
         ++it) {
      if (unacked_packets_.HasRetransmittableFrames(*it)) {
        ++num_retransmittable_packets;
      }
    }
    EXPECT_EQ(num_packets, num_retransmittable_packets);
    for (size_t i = 0; i < num_packets; ++i) {
      EXPECT_TRUE(unacked_packets_.HasRetransmittableFrames(
          QuicPacketNumber(packets[i])))
          << " packets[" << i << "]:" << packets[i];
    }
  }

  void UpdatePacketState(uint64_t packet_number, SentPacketState state) {
    unacked_packets_
        .GetMutableTransmissionInfo(QuicPacketNumber(packet_number))
        ->state = state;
  }

  void RetransmitAndSendPacket(uint64_t old_packet_number,
                               uint64_t new_packet_number,
                               TransmissionType transmission_type) {
    QUICHE_DCHECK(unacked_packets_.HasRetransmittableFrames(
        QuicPacketNumber(old_packet_number)));
    QuicTransmissionInfo* info = unacked_packets_.GetMutableTransmissionInfo(
        QuicPacketNumber(old_packet_number));
    QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
        CurrentSupportedVersions()[0].transport_version,
        Perspective::IS_CLIENT);
    for (const auto& frame : info->retransmittable_frames) {
      if (frame.type == STREAM_FRAME) {
        stream_id = frame.stream_frame.stream_id;
        break;
      }
    }
    UpdatePacketState(
        old_packet_number,
        QuicUtils::RetransmissionTypeToPacketState(transmission_type));
    info->first_sent_after_loss = QuicPacketNumber(new_packet_number);
    SerializedPacket packet(
        CreateRetransmittablePacketForStream(new_packet_number, stream_id));
    unacked_packets_.AddSentPacket(&packet, transmission_type, now_, true, true,
                                   ECN_NOT_ECT);
  }
  QuicUnackedPacketMap unacked_packets_;
  QuicTime now_;
  StrictMock<MockSessionNotifier> notifier_;
};

INSTANTIATE_TEST_SUITE_P(Tests, QuicUnackedPacketMapTest,
                         ::testing::ValuesIn({Perspective::IS_CLIENT,
                                              Perspective::IS_SERVER}),
                         ::testing::PrintToStringParamName());

TEST_P(QuicUnackedPacketMapTest, RttOnly) {
  // Acks are only tracked for RTT measurement purposes.
  SerializedPacket packet(CreateNonRetransmittablePacket(1));
  unacked_packets_.AddSentPacket(&packet, NOT_RETRANSMISSION, now_, false, true,
                                 ECN_NOT_ECT);

  uint64_t unacked[] = {1};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyInFlightPackets(nullptr, 0);
  VerifyRetransmittablePackets(nullptr, 0);

  unacked_packets_.IncreaseLargestAcked(QuicPacketNumber(1));
  VerifyUnackedPackets(nullptr, 0);
  VerifyInFlightPackets(nullptr, 0);
  VerifyRetransmittablePackets(nullptr, 0);
}

TEST_P(QuicUnackedPacketMapTest, RetransmittableInflightAndRtt) {
  // Simulate a retransmittable packet being sent and acked.
  SerializedPacket packet(CreateRetransmittablePacket(1));
  unacked_packets_.AddSentPacket(&packet, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);

  uint64_t unacked[] = {1};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyInFlightPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyRetransmittablePackets(unacked, ABSL_ARRAYSIZE(unacked));

  unacked_packets_.RemoveRetransmittability(QuicPacketNumber(1));
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyInFlightPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyRetransmittablePackets(nullptr, 0);

  unacked_packets_.IncreaseLargestAcked(QuicPacketNumber(1));
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyInFlightPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyRetransmittablePackets(nullptr, 0);

  unacked_packets_.RemoveFromInFlight(QuicPacketNumber(1));
  VerifyUnackedPackets(nullptr, 0);
  VerifyInFlightPackets(nullptr, 0);
  VerifyRetransmittablePackets(nullptr, 0);
}

TEST_P(QuicUnackedPacketMapTest, StopRetransmission) {
  const QuicStreamId stream_id = 2;
  SerializedPacket packet(CreateRetransmittablePacketForStream(1, stream_id));
  unacked_packets_.AddSentPacket(&packet, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);

  uint64_t unacked[] = {1};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyInFlightPackets(unacked, ABSL_ARRAYSIZE(unacked));
  uint64_t retransmittable[] = {1};
  VerifyRetransmittablePackets(retransmittable,
                               ABSL_ARRAYSIZE(retransmittable));

  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyInFlightPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyRetransmittablePackets(nullptr, 0);
}

TEST_P(QuicUnackedPacketMapTest, StopRetransmissionOnOtherStream) {
  const QuicStreamId stream_id = 2;
  SerializedPacket packet(CreateRetransmittablePacketForStream(1, stream_id));
  unacked_packets_.AddSentPacket(&packet, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);

  uint64_t unacked[] = {1};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyInFlightPackets(unacked, ABSL_ARRAYSIZE(unacked));
  uint64_t retransmittable[] = {1};
  VerifyRetransmittablePackets(retransmittable,
                               ABSL_ARRAYSIZE(retransmittable));

  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyInFlightPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyRetransmittablePackets(retransmittable,
                               ABSL_ARRAYSIZE(retransmittable));
}

TEST_P(QuicUnackedPacketMapTest, StopRetransmissionAfterRetransmission) {
  const QuicStreamId stream_id = 2;
  SerializedPacket packet1(CreateRetransmittablePacketForStream(1, stream_id));
  unacked_packets_.AddSentPacket(&packet1, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);
  RetransmitAndSendPacket(1, 2, LOSS_RETRANSMISSION);

  uint64_t unacked[] = {1, 2};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyInFlightPackets(unacked, ABSL_ARRAYSIZE(unacked));
  std::vector<uint64_t> retransmittable = {1, 2};
  VerifyRetransmittablePackets(&retransmittable[0], retransmittable.size());

  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyInFlightPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyRetransmittablePackets(nullptr, 0);
}

TEST_P(QuicUnackedPacketMapTest, RetransmittedPacket) {
  // Simulate a retransmittable packet being sent, retransmitted, and the first
  // transmission being acked.
  SerializedPacket packet1(CreateRetransmittablePacket(1));
  unacked_packets_.AddSentPacket(&packet1, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);
  RetransmitAndSendPacket(1, 2, LOSS_RETRANSMISSION);

  uint64_t unacked[] = {1, 2};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyInFlightPackets(unacked, ABSL_ARRAYSIZE(unacked));
  std::vector<uint64_t> retransmittable = {1, 2};
  VerifyRetransmittablePackets(&retransmittable[0], retransmittable.size());

  EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  unacked_packets_.RemoveRetransmittability(QuicPacketNumber(1));
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyInFlightPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyRetransmittablePackets(nullptr, 0);

  unacked_packets_.IncreaseLargestAcked(QuicPacketNumber(2));
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyInFlightPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyRetransmittablePackets(nullptr, 0);

  unacked_packets_.RemoveFromInFlight(QuicPacketNumber(2));
  uint64_t unacked2[] = {1};
  VerifyUnackedPackets(unacked2, ABSL_ARRAYSIZE(unacked2));
  VerifyInFlightPackets(unacked2, ABSL_ARRAYSIZE(unacked2));
  VerifyRetransmittablePackets(nullptr, 0);

  unacked_packets_.RemoveFromInFlight(QuicPacketNumber(1));
  VerifyUnackedPackets(nullptr, 0);
  VerifyInFlightPackets(nullptr, 0);
  VerifyRetransmittablePackets(nullptr, 0);
}

TEST_P(QuicUnackedPacketMapTest, RetransmitThreeTimes) {
  // Simulate a retransmittable packet being sent and retransmitted twice.
  SerializedPacket packet1(CreateRetransmittablePacket(1));
  unacked_packets_.AddSentPacket(&packet1, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);
  SerializedPacket packet2(CreateRetransmittablePacket(2));
  unacked_packets_.AddSentPacket(&packet2, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);

  uint64_t unacked[] = {1, 2};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyInFlightPackets(unacked, ABSL_ARRAYSIZE(unacked));
  uint64_t retransmittable[] = {1, 2};
  VerifyRetransmittablePackets(retransmittable,
                               ABSL_ARRAYSIZE(retransmittable));

  // Early retransmit 1 as 3 and send new data as 4.
  unacked_packets_.IncreaseLargestAcked(QuicPacketNumber(2));
  unacked_packets_.RemoveFromInFlight(QuicPacketNumber(2));
  unacked_packets_.RemoveRetransmittability(QuicPacketNumber(2));
  unacked_packets_.RemoveFromInFlight(QuicPacketNumber(1));
  RetransmitAndSendPacket(1, 3, LOSS_RETRANSMISSION);
  SerializedPacket packet4(CreateRetransmittablePacket(4));
  unacked_packets_.AddSentPacket(&packet4, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);

  uint64_t unacked2[] = {1, 3, 4};
  VerifyUnackedPackets(unacked2, ABSL_ARRAYSIZE(unacked2));
  uint64_t pending2[] = {3, 4};
  VerifyInFlightPackets(pending2, ABSL_ARRAYSIZE(pending2));
  std::vector<uint64_t> retransmittable2 = {1, 3, 4};
  VerifyRetransmittablePackets(&retransmittable2[0], retransmittable2.size());

  // Early retransmit 3 (formerly 1) as 5, and remove 1 from unacked.
  unacked_packets_.IncreaseLargestAcked(QuicPacketNumber(4));
  unacked_packets_.RemoveFromInFlight(QuicPacketNumber(4));
  unacked_packets_.RemoveRetransmittability(QuicPacketNumber(4));
  RetransmitAndSendPacket(3, 5, LOSS_RETRANSMISSION);
  SerializedPacket packet6(CreateRetransmittablePacket(6));
  unacked_packets_.AddSentPacket(&packet6, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);

  std::vector<uint64_t> unacked3 = {3, 5, 6};
  std::vector<uint64_t> retransmittable3 = {3, 5, 6};
  VerifyUnackedPackets(&unacked3[0], unacked3.size());
  VerifyRetransmittablePackets(&retransmittable3[0], retransmittable3.size());
  uint64_t pending3[] = {3, 5, 6};
  VerifyInFlightPackets(pending3, ABSL_ARRAYSIZE(pending3));

  // Early retransmit 5 as 7 and ensure in flight packet 3 is not removed.
  unacked_packets_.IncreaseLargestAcked(QuicPacketNumber(6));
  unacked_packets_.RemoveFromInFlight(QuicPacketNumber(6));
  unacked_packets_.RemoveRetransmittability(QuicPacketNumber(6));
  RetransmitAndSendPacket(5, 7, LOSS_RETRANSMISSION);

  std::vector<uint64_t> unacked4 = {3, 5, 7};
  std::vector<uint64_t> retransmittable4 = {3, 5, 7};
  VerifyUnackedPackets(&unacked4[0], unacked4.size());
  VerifyRetransmittablePackets(&retransmittable4[0], retransmittable4.size());
  uint64_t pending4[] = {3, 5, 7};
  VerifyInFlightPackets(pending4, ABSL_ARRAYSIZE(pending4));

  // Remove the older two transmissions from in flight.
  unacked_packets_.RemoveFromInFlight(QuicPacketNumber(3));
  unacked_packets_.RemoveFromInFlight(QuicPacketNumber(5));
  uint64_t pending5[] = {7};
  VerifyInFlightPackets(pending5, ABSL_ARRAYSIZE(pending5));
}

TEST_P(QuicUnackedPacketMapTest, RetransmitFourTimes) {
  // Simulate a retransmittable packet being sent and retransmitted twice.
  SerializedPacket packet1(CreateRetransmittablePacket(1));
  unacked_packets_.AddSentPacket(&packet1, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);
  SerializedPacket packet2(CreateRetransmittablePacket(2));
  unacked_packets_.AddSentPacket(&packet2, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);

  uint64_t unacked[] = {1, 2};
  VerifyUnackedPackets(unacked, ABSL_ARRAYSIZE(unacked));
  VerifyInFlightPackets(unacked, ABSL_ARRAYSIZE(unacked));
  uint64_t retransmittable[] = {1, 2};
  VerifyRetransmittablePackets(retransmittable,
                               ABSL_ARRAYSIZE(retransmittable));

  // Early retransmit 1 as 3.
  unacked_packets_.IncreaseLargestAcked(QuicPacketNumber(2));
  unacked_packets_.RemoveFromInFlight(QuicPacketNumber(2));
  unacked_packets_.RemoveRetransmittability(QuicPacketNumber(2));
  unacked_packets_.RemoveFromInFlight(QuicPacketNumber(1));
  RetransmitAndSendPacket(1, 3, LOSS_RETRANSMISSION);

  uint64_t unacked2[] = {1, 3};
  VerifyUnackedPackets(unacked2, ABSL_ARRAYSIZE(unacked2));
  uint64_t pending2[] = {3};
  VerifyInFlightPackets(pending2, ABSL_ARRAYSIZE(pending2));
  std::vector<uint64_t> retransmittable2 = {1, 3};
  VerifyRetransmittablePackets(&retransmittable2[0], retransmittable2.size());

  // PTO 3 (formerly 1) as 4, and don't remove 1 from unacked.
  RetransmitAndSendPacket(3, 4, PTO_RETRANSMISSION);
  SerializedPacket packet5(CreateRetransmittablePacket(5));
  unacked_packets_.AddSentPacket(&packet5, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);

  uint64_t unacked3[] = {1, 3, 4, 5};
  VerifyUnackedPackets(unacked3, ABSL_ARRAYSIZE(unacked3));
  uint64_t pending3[] = {3, 4, 5};
  VerifyInFlightPackets(pending3, ABSL_ARRAYSIZE(pending3));
  std::vector<uint64_t> retransmittable3 = {1, 3, 4, 5};
  VerifyRetransmittablePackets(&retransmittable3[0], retransmittable3.size());

  // Early retransmit 4 as 6 and ensure in flight packet 3 is removed.
  unacked_packets_.IncreaseLargestAcked(QuicPacketNumber(5));
  unacked_packets_.RemoveFromInFlight(QuicPacketNumber(5));
  unacked_packets_.RemoveRetransmittability(QuicPacketNumber(5));
  unacked_packets_.RemoveFromInFlight(QuicPacketNumber(3));
  unacked_packets_.RemoveFromInFlight(QuicPacketNumber(4));
  RetransmitAndSendPacket(4, 6, LOSS_RETRANSMISSION);

  std::vector<uint64_t> unacked4 = {4, 6};
  VerifyUnackedPackets(&unacked4[0], unacked4.size());
  uint64_t pending4[] = {6};
  VerifyInFlightPackets(pending4, ABSL_ARRAYSIZE(pending4));
  std::vector<uint64_t> retransmittable4 = {4, 6};
  VerifyRetransmittablePackets(&retransmittable4[0], retransmittable4.size());
}

TEST_P(QuicUnackedPacketMapTest, SendWithGap) {
  // Simulate a retransmittable packet being sent, retransmitted, and the first
  // transmission being acked.
  SerializedPacket packet1(CreateRetransmittablePacket(1));
  unacked_packets_.AddSentPacket(&packet1, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);
  SerializedPacket packet3(CreateRetransmittablePacket(3));
  unacked_packets_.AddSentPacket(&packet3, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);
  RetransmitAndSendPacket(3, 5, LOSS_RETRANSMISSION);

  EXPECT_EQ(QuicPacketNumber(1u), unacked_packets_.GetLeastUnacked());
  EXPECT_TRUE(unacked_packets_.IsUnacked(QuicPacketNumber(1)));
  EXPECT_FALSE(unacked_packets_.IsUnacked(QuicPacketNumber(2)));
  EXPECT_TRUE(unacked_packets_.IsUnacked(QuicPacketNumber(3)));
  EXPECT_FALSE(unacked_packets_.IsUnacked(QuicPacketNumber(4)));
  EXPECT_TRUE(unacked_packets_.IsUnacked(QuicPacketNumber(5)));
  EXPECT_EQ(QuicPacketNumber(5u), unacked_packets_.largest_sent_packet());
}

TEST_P(QuicUnackedPacketMapTest, AggregateContiguousAckedStreamFrames) {
  testing::InSequence s;
  EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _)).Times(0);
  unacked_packets_.NotifyAggregatedStreamFrameAcked(QuicTime::Delta::Zero());

  SerializedPacket packet1(
      CreateRetransmittablePacketForStream(1, 3, false, 0, 100));
  unacked_packets_.AddSentPacket(&packet1, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);

  SerializedPacket packet2(
      CreateRetransmittablePacketForStream(2, 3, false, 100, 100));
  unacked_packets_.AddSentPacket(&packet2, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);

  SerializedPacket packet3(
      CreateRetransmittablePacketForStream(3, 3, false, 200, 100));
  unacked_packets_.AddSentPacket(&packet3, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);

  SerializedPacket packet4(
      CreateRetransmittablePacketForStream(4, 3, true, 300, 0));
  unacked_packets_.AddSentPacket(&packet4, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);

  QuicTransmissionInfo* info1 =
      unacked_packets_.GetMutableTransmissionInfo(QuicPacketNumber(1));
  QuicTransmissionInfo* info2 =
      unacked_packets_.GetMutableTransmissionInfo(QuicPacketNumber(2));
  QuicTransmissionInfo* info3 =
      unacked_packets_.GetMutableTransmissionInfo(QuicPacketNumber(3));
  QuicTransmissionInfo* info4 =
      unacked_packets_.GetMutableTransmissionInfo(QuicPacketNumber(4));

  // Verify stream frames are aggregated.
  EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _)).Times(0);
  unacked_packets_.MaybeAggregateAckedStreamFrame(
      QuicPacketNumber(1), QuicTime::Delta::Zero(), QuicTime::Zero(), info1);
  EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _)).Times(0);
  unacked_packets_.MaybeAggregateAckedStreamFrame(
      QuicPacketNumber(2), QuicTime::Delta::Zero(), QuicTime::Zero(), info2);
  EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _)).Times(0);
  unacked_packets_.MaybeAggregateAckedStreamFrame(
      QuicPacketNumber(3), QuicTime::Delta::Zero(), QuicTime::Zero(), info3);

  // Verify aggregated stream frame gets acked since fin is acked.
  EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _)).Times(1);
  unacked_packets_.MaybeAggregateAckedStreamFrame(
      QuicPacketNumber(4), QuicTime::Delta::Zero(), QuicTime::Zero(), info4);
}

// Regression test for b/112930090.
TEST_P(QuicUnackedPacketMapTest, CannotAggregateIfDataLengthOverflow) {
  QuicByteCount kMaxAggregatedDataLength =
      std::numeric_limits<decltype(QuicStreamFrame().data_length)>::max();
  QuicStreamId stream_id = 2;
  uint64_t next_packet_number = 1;

  // acked_stream_length=512 covers the case where a frame will cause the
  // aggregated frame length to be exactly 64K.
  // acked_stream_length=1300 covers the case where a frame will cause the
  // aggregated frame length to exceed 64K.
  for (const QuicPacketLength acked_stream_length : {512, 1300}) {
    ++stream_id;
    QuicStreamOffset offset = 0;
    // Expected length of the aggregated stream frame.
    QuicByteCount aggregated_data_length = 0;

    while (offset < 1e6) {
      uint64_t packet_number = next_packet_number++;
      SerializedPacket packet(CreateRetransmittablePacketForStream(
          packet_number, stream_id, false, offset, acked_stream_length));
      unacked_packets_.AddSentPacket(&packet, NOT_RETRANSMISSION, now_, true,
                                     true, ECN_NOT_ECT);
      QuicTransmissionInfo* info = unacked_packets_.GetMutableTransmissionInfo(
          QuicPacketNumber(packet_number));

      const QuicStreamFrame& aggregated_stream_frame =
          QuicUnackedPacketMapPeer::GetAggregatedStreamFrame(unacked_packets_);
      if (aggregated_stream_frame.data_length + acked_stream_length <=
          kMaxAggregatedDataLength) {
        // Verify the acked stream frame can be aggregated.
        EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _)).Times(0);
        unacked_packets_.MaybeAggregateAckedStreamFrame(
            QuicPacketNumber(packet_number), QuicTime::Delta::Zero(),
            QuicTime::Zero(), info);
        aggregated_data_length += acked_stream_length;
        testing::Mock::VerifyAndClearExpectations(&notifier_);
      } else {
        // Verify the acked stream frame cannot be aggregated because
        // data_length is overflow.
        EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _)).Times(1);
        unacked_packets_.MaybeAggregateAckedStreamFrame(
            QuicPacketNumber(packet_number), QuicTime::Delta::Zero(),
            QuicTime::Zero(), info);
        aggregated_data_length = acked_stream_length;
        testing::Mock::VerifyAndClearExpectations(&notifier_);
      }

      EXPECT_EQ(aggregated_data_length, aggregated_stream_frame.data_length);
      offset += acked_stream_length;
    }

    // Ack the last frame of the stream.
    uint64_t packet_number = next_packet_number++;
    SerializedPacket packet(CreateRetransmittablePacketForStream(
        packet_number, stream_id, true, offset, acked_stream_length));
    unacked_packets_.AddSentPacket(&packet, NOT_RETRANSMISSION, now_, true,
                                   true, ECN_NOT_ECT);
    QuicTransmissionInfo* info = unacked_packets_.GetMutableTransmissionInfo(
        QuicPacketNumber(packet_number));
    EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _)).Times(1);
    unacked_packets_.MaybeAggregateAckedStreamFrame(
        QuicPacketNumber(packet_number), QuicTime::Delta::Zero(),
        QuicTime::Zero(), info);
    testing::Mock::VerifyAndClearExpectations(&notifier_);
  }
}

TEST_P(QuicUnackedPacketMapTest, CannotAggregateAckedControlFrames) {
  testing::InSequence s;
  QuicWindowUpdateFrame window_update(1, 5, 100);
  QuicStreamFrame stream_frame1(3, false, 0, 100);
  QuicStreamFrame stream_frame2(3, false, 100, 100);
  QuicBlockedFrame blocked(2, 5, 0);

  SerializedPacket packet1(CreateRetransmittablePacket(
      1, {QuicFrame(window_update), QuicFrame(stream_frame1),
          QuicFrame(stream_frame2)}));
  unacked_packets_.AddSentPacket(&packet1, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);
  QuicTransmissionInfo* info1 =
      unacked_packets_.GetMutableTransmissionInfo(QuicPacketNumber(1));

  SerializedPacket packet2(CreateRetransmittablePacket(
      2,
      {QuicFrame(blocked), QuicFrame(new QuicGoAwayFrame(
                               3, QUIC_PEER_GOING_AWAY, 5, "Going away."))}));
  unacked_packets_.AddSentPacket(&packet2, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);
  QuicTransmissionInfo* info2 =
      unacked_packets_.GetMutableTransmissionInfo(QuicPacketNumber(2));

  // Verify 2 contiguous stream frames are aggregated.
  EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _)).Times(1);
  unacked_packets_.MaybeAggregateAckedStreamFrame(
      QuicPacketNumber(1), QuicTime::Delta::Zero(), QuicTime::Zero(), info1);
  // Verify aggregated stream frame gets acked.
  EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _)).Times(3);
  unacked_packets_.MaybeAggregateAckedStreamFrame(
      QuicPacketNumber(2), QuicTime::Delta::Zero(), QuicTime::Zero(), info2);

  EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _)).Times(0);
  unacked_packets_.NotifyAggregatedStreamFrameAcked(QuicTime::Delta::Zero());
}

TEST_P(QuicUnackedPacketMapTest, UpdateTransmissionInfoOnFrameAcked) {
  uint64_t next_packet_number = 1;
  do {
    uint64_t packet_number = next_packet_number++;
    SerializedPacket packet(CreateRetransmittablePacket(
        packet_number,
        {QuicFrame(QuicPaddingFrame(static_cast<int>(packet_number) * 100))}));
    unacked_packets_.AddSentPacket(&packet, NOT_RETRANSMISSION, now_, true,
                                   true, ECN_NOT_ECT);
  } while (QuicUnackedPacketMapPeer::GetCapacity(unacked_packets_) >
           QuicUnackedPacketMapPeer::GetSize(unacked_packets_));
  QUIC_DLOG(INFO) << "unacked_packets_ at full capacity: "
                  << QuicUnackedPacketMapPeer::GetCapacity(unacked_packets_);

  QuicPacketNumber largest_sent_packet_before_acked =
      unacked_packets_.largest_sent_packet();
  QuicTransmissionInfo* last_info = unacked_packets_.GetMutableTransmissionInfo(
      largest_sent_packet_before_acked);
  int last_padding_bytes =
      last_info->retransmittable_frames[0].padding_frame.num_padding_bytes;

  EXPECT_CALL(notifier_, OnFrameAcked(_, _, _, _))
      .WillOnce(Invoke([&](const QuicFrame& frame, QuicTime::Delta, QuicTime,
                           bool) {
        EXPECT_EQ(frame.type, PADDING_FRAME);
        EXPECT_EQ(frame.padding_frame.num_padding_bytes, last_padding_bytes);
        // Append one more packet to the unacked packet map.
        SerializedPacket packet(CreateRetransmittablePacket(
            next_packet_number++, {QuicFrame(QuicBlockedFrame(2, 5, 0))}));
        unacked_packets_.AddSentPacket(&packet, NOT_RETRANSMISSION, now_, true,
                                       true, ECN_NOT_ECT);
        return true;
      }));

  QuicTransmissionInfo* last_info_updated = last_info;
  unacked_packets_.NotifyFramesAcked(largest_sent_packet_before_acked,
                                     QuicTime::Delta::Zero(), QuicTime::Zero(),
                                     last_info_updated);
  EXPECT_NE(last_info, last_info_updated);
  EXPECT_EQ(unacked_packets_.GetMutableTransmissionInfo(
                largest_sent_packet_before_acked),
            last_info_updated);
  ASSERT_EQ(last_info_updated->retransmittable_frames.size(), 1u);
  EXPECT_EQ(last_info_updated->retransmittable_frames[0].type, PADDING_FRAME);
  EXPECT_EQ(last_info_updated->retransmittable_frames[0]
                .padding_frame.num_padding_bytes,
            last_padding_bytes);
}

TEST_P(QuicUnackedPacketMapTest, LargestSentPacketMultiplePacketNumberSpaces) {
  unacked_packets_.EnableMultiplePacketNumberSpacesSupport();
  EXPECT_FALSE(
      unacked_packets_
          .GetLargestSentRetransmittableOfPacketNumberSpace(INITIAL_DATA)
          .IsInitialized());
  // Send packet 1.
  SerializedPacket packet1(CreateRetransmittablePacket(1));
  packet1.encryption_level = ENCRYPTION_INITIAL;
  unacked_packets_.AddSentPacket(&packet1, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);
  EXPECT_EQ(QuicPacketNumber(1u), unacked_packets_.largest_sent_packet());
  EXPECT_EQ(QuicPacketNumber(1),
            unacked_packets_.GetLargestSentRetransmittableOfPacketNumberSpace(
                INITIAL_DATA));
  EXPECT_FALSE(
      unacked_packets_
          .GetLargestSentRetransmittableOfPacketNumberSpace(HANDSHAKE_DATA)
          .IsInitialized());
  // Send packet 2.
  SerializedPacket packet2(CreateRetransmittablePacket(2));
  packet2.encryption_level = ENCRYPTION_HANDSHAKE;
  unacked_packets_.AddSentPacket(&packet2, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);
  EXPECT_EQ(QuicPacketNumber(2u), unacked_packets_.largest_sent_packet());
  EXPECT_EQ(QuicPacketNumber(1),
            unacked_packets_.GetLargestSentRetransmittableOfPacketNumberSpace(
                INITIAL_DATA));
  EXPECT_EQ(QuicPacketNumber(2),
            unacked_packets_.GetLargestSentRetransmittableOfPacketNumberSpace(
                HANDSHAKE_DATA));
  EXPECT_FALSE(
      unacked_packets_
          .GetLargestSentRetransmittableOfPacketNumberSpace(APPLICATION_DATA)
          .IsInitialized());
  // Send packet 3.
  SerializedPacket packet3(CreateRetransmittablePacket(3));
  packet3.encryption_level = ENCRYPTION_ZERO_RTT;
  unacked_packets_.AddSentPacket(&packet3, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);
  EXPECT_EQ(QuicPacketNumber(3u), unacked_packets_.largest_sent_packet());
  EXPECT_EQ(QuicPacketNumber(1),
            unacked_packets_.GetLargestSentRetransmittableOfPacketNumberSpace(
                INITIAL_DATA));
  EXPECT_EQ(QuicPacketNumber(2),
            unacked_packets_.GetLargestSentRetransmittableOfPacketNumberSpace(
                HANDSHAKE_DATA));
  EXPECT_EQ(QuicPacketNumber(3),
            unacked_packets_.GetLargestSentRetransmittableOfPacketNumberSpace(
                APPLICATION_DATA));
  // Verify forward secure belongs to the same packet number space as encryption
  // zero rtt.
  EXPECT_EQ(QuicPacketNumber(3),
            unacked_packets_.GetLargestSentRetransmittableOfPacketNumberSpace(
                APPLICATION_DATA));

  // Send packet 4.
  SerializedPacket packet4(CreateRetransmittablePacket(4));
  packet4.encryption_level = ENCRYPTION_FORWARD_SECURE;
  unacked_packets_.AddSentPacket(&packet4, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);
  EXPECT_EQ(QuicPacketNumber(4u), unacked_packets_.largest_sent_packet());
  EXPECT_EQ(QuicPacketNumber(1),
            unacked_packets_.GetLargestSentRetransmittableOfPacketNumberSpace(
                INITIAL_DATA));
  EXPECT_EQ(QuicPacketNumber(2),
            unacked_packets_.GetLargestSentRetransmittableOfPacketNumberSpace(
                HANDSHAKE_DATA));
  EXPECT_EQ(QuicPacketNumber(4),
            unacked_packets_.GetLargestSentRetransmittableOfPacketNumberSpace(
                APPLICATION_DATA));
  // Verify forward secure belongs to the same packet number space as encryption
  // zero rtt.
  EXPECT_EQ(QuicPacketNumber(4),
            unacked_packets_.GetLargestSentRetransmittableOfPacketNumberSpace(
                APPLICATION_DATA));
  EXPECT_TRUE(unacked_packets_.GetLastPacketContent() & (1 << STREAM_FRAME));
  EXPECT_FALSE(unacked_packets_.GetLastPacketContent() & (1 << ACK_FRAME));
}

TEST_P(QuicUnackedPacketMapTest, ReserveInitialCapacityTest) {
  QuicUnackedPacketMap unacked_packets(GetParam());
  ASSERT_EQ(QuicUnackedPacketMapPeer::GetCapacity(unacked_packets), 0u);
  unacked_packets.ReserveInitialCapacity(16);
  QuicStreamId stream_id(1);
  SerializedPacket packet(CreateRetransmittablePacketForStream(1, stream_id));
  unacked_packets.AddSentPacket(&packet, TransmissionType::NOT_RETRANSMISSION,
                                now_, true, true, ECN_NOT_ECT);
  ASSERT_EQ(QuicUnackedPacketMapPeer::GetCapacity(unacked_packets), 16u);
}

TEST_P(QuicUnackedPacketMapTest, DebugString) {
  EXPECT_EQ(unacked_packets_.DebugString(),
            "{size: 0, least_unacked: 1, largest_sent_packet: uninitialized, "
            "largest_acked: uninitialized, bytes_in_flight: 0, "
            "packets_in_flight: 0}");

  SerializedPacket packet1(CreateRetransmittablePacket(1));
  unacked_packets_.AddSentPacket(&packet1, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);
  EXPECT_EQ(
      unacked_packets_.DebugString(),
      "{size: 1, least_unacked: 1, largest_sent_packet: 1, largest_acked: "
      "uninitialized, bytes_in_flight: 1000, packets_in_flight: 1}");

  SerializedPacket packet2(CreateRetransmittablePacket(2));
  unacked_packets_.AddSentPacket(&packet2, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);
  unacked_packets_.RemoveFromInFlight(QuicPacketNumber(1));
  unacked_packets_.IncreaseLargestAcked(QuicPacketNumber(1));
  unacked_packets_.RemoveObsoletePackets();
  EXPECT_EQ(
      unacked_packets_.DebugString(),
      "{size: 1, least_unacked: 2, largest_sent_packet: 2, largest_acked: 1, "
      "bytes_in_flight: 1000, packets_in_flight: 1}");
}

TEST_P(QuicUnackedPacketMapTest, EcnInfoStored) {
  SerializedPacket packet1(CreateRetransmittablePacket(1));
  unacked_packets_.AddSentPacket(&packet1, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_NOT_ECT);
  SerializedPacket packet2(CreateRetransmittablePacket(2));
  unacked_packets_.AddSentPacket(&packet2, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_ECT0);
  SerializedPacket packet3(CreateRetransmittablePacket(3));
  unacked_packets_.AddSentPacket(&packet3, NOT_RETRANSMISSION, now_, true, true,
                                 ECN_ECT1);
  EXPECT_EQ(
      unacked_packets_.GetTransmissionInfo(QuicPacketNumber(1)).ecn_codepoint,
      ECN_NOT_ECT);
  EXPECT_EQ(
      unacked_packets_.GetTransmissionInfo(QuicPacketNumber(2)).ecn_codepoint,
      ECN_ECT0);
  EXPECT_EQ(
      unacked_packets_.GetTransmissionInfo(QuicPacketNumber(3)).ecn_codepoint,
      ECN_ECT1);
}

}  // namespace
}  // namespace test
}  // namespace quic
