// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_coalesced_packet.h"

#include <string>

#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace quic {
namespace test {
namespace {

TEST(QuicCoalescedPacketTest, MaybeCoalescePacket) {
  QuicCoalescedPacket coalesced;
  EXPECT_EQ("total_length: 0 padding_size: 0 packets: {}",
            coalesced.ToString(0));
  quiche::SimpleBufferAllocator allocator;
  EXPECT_EQ(0u, coalesced.length());
  EXPECT_EQ(0u, coalesced.NumberOfPackets());
  char buffer[1000];
  QuicSocketAddress self_address(QuicIpAddress::Loopback4(), 1);
  QuicSocketAddress peer_address(QuicIpAddress::Loopback4(), 2);
  SerializedPacket packet1(QuicPacketNumber(1), PACKET_4BYTE_PACKET_NUMBER,
                           buffer, 500, false, false);
  packet1.transmission_type = PTO_RETRANSMISSION;
  QuicAckFrame ack_frame(InitAckFrame(1));
  packet1.nonretransmittable_frames.push_back(QuicFrame(&ack_frame));
  packet1.retransmittable_frames.push_back(
      QuicFrame(QuicStreamFrame(1, true, 0, 100)));
  ASSERT_TRUE(coalesced.MaybeCoalescePacket(packet1, self_address, peer_address,
                                            &allocator, 1500, ECN_NOT_ECT, 0));
  EXPECT_EQ(PTO_RETRANSMISSION,
            coalesced.TransmissionTypeOfPacket(ENCRYPTION_INITIAL));
  EXPECT_EQ(1500u, coalesced.max_packet_length());
  EXPECT_EQ(500u, coalesced.length());
  EXPECT_EQ(1u, coalesced.NumberOfPackets());
  EXPECT_EQ(
      "total_length: 1500 padding_size: 1000 packets: {ENCRYPTION_INITIAL}",
      coalesced.ToString(1500));
  EXPECT_EQ(coalesced.ecn_codepoint(), ECN_NOT_ECT);

  // Cannot coalesce packet of the same encryption level.
  SerializedPacket packet2(QuicPacketNumber(2), PACKET_4BYTE_PACKET_NUMBER,
                           buffer, 500, false, false);
  EXPECT_FALSE(coalesced.MaybeCoalescePacket(
      packet2, self_address, peer_address, &allocator, 1500, ECN_NOT_ECT, 0));
  EXPECT_EQ(coalesced.ecn_codepoint(), ECN_NOT_ECT);

  SerializedPacket packet3(QuicPacketNumber(3), PACKET_4BYTE_PACKET_NUMBER,
                           buffer, 500, false, false);
  packet3.nonretransmittable_frames.push_back(QuicFrame(QuicPaddingFrame(100)));
  packet3.encryption_level = ENCRYPTION_ZERO_RTT;
  packet3.transmission_type = LOSS_RETRANSMISSION;
  ASSERT_TRUE(coalesced.MaybeCoalescePacket(packet3, self_address, peer_address,
                                            &allocator, 1500, ECN_NOT_ECT, 0));
  EXPECT_EQ(1500u, coalesced.max_packet_length());
  EXPECT_EQ(1000u, coalesced.length());
  EXPECT_EQ(2u, coalesced.NumberOfPackets());
  EXPECT_EQ(LOSS_RETRANSMISSION,
            coalesced.TransmissionTypeOfPacket(ENCRYPTION_ZERO_RTT));
  EXPECT_EQ(
      "total_length: 1500 padding_size: 500 packets: {ENCRYPTION_INITIAL, "
      "ENCRYPTION_ZERO_RTT}",
      coalesced.ToString(1500));
  EXPECT_EQ(coalesced.ecn_codepoint(), ECN_NOT_ECT);

  SerializedPacket packet4(QuicPacketNumber(4), PACKET_4BYTE_PACKET_NUMBER,
                           buffer, 500, false, false);
  packet4.encryption_level = ENCRYPTION_FORWARD_SECURE;
  // Cannot coalesce packet of changed self/peer address.
  EXPECT_FALSE(coalesced.MaybeCoalescePacket(
      packet4, QuicSocketAddress(QuicIpAddress::Loopback4(), 3), peer_address,
      &allocator, 1500, ECN_NOT_ECT, 0));

  // Packet does not fit.
  SerializedPacket packet5(QuicPacketNumber(5), PACKET_4BYTE_PACKET_NUMBER,
                           buffer, 501, false, false);
  packet5.encryption_level = ENCRYPTION_FORWARD_SECURE;
  EXPECT_FALSE(coalesced.MaybeCoalescePacket(
      packet5, self_address, peer_address, &allocator, 1500, ECN_NOT_ECT, 0));
  EXPECT_EQ(1500u, coalesced.max_packet_length());
  EXPECT_EQ(1000u, coalesced.length());
  EXPECT_EQ(2u, coalesced.NumberOfPackets());
  EXPECT_EQ(coalesced.ecn_codepoint(), ECN_NOT_ECT);

  // Max packet number length changed.
  SerializedPacket packet6(QuicPacketNumber(6), PACKET_4BYTE_PACKET_NUMBER,
                           buffer, 100, false, false);
  packet6.encryption_level = ENCRYPTION_FORWARD_SECURE;
  EXPECT_QUIC_BUG(
      coalesced.MaybeCoalescePacket(packet6, self_address, peer_address,
                                    &allocator, 1000, ECN_NOT_ECT, 0),
      "Max packet length changes in the middle of the write path");
  EXPECT_EQ(1500u, coalesced.max_packet_length());
  EXPECT_EQ(1000u, coalesced.length());
  EXPECT_EQ(2u, coalesced.NumberOfPackets());
  EXPECT_EQ(coalesced.ecn_codepoint(), ECN_NOT_ECT);
}

TEST(QuicCoalescedPacketTest, CopyEncryptedBuffers) {
  QuicCoalescedPacket coalesced;
  quiche::SimpleBufferAllocator allocator;
  QuicSocketAddress self_address(QuicIpAddress::Loopback4(), 1);
  QuicSocketAddress peer_address(QuicIpAddress::Loopback4(), 2);
  std::string buffer(500, 'a');
  std::string buffer2(500, 'b');
  SerializedPacket packet1(QuicPacketNumber(1), PACKET_4BYTE_PACKET_NUMBER,
                           buffer.data(), 500,
                           /*has_ack=*/false, /*has_stop_waiting=*/false);
  packet1.encryption_level = ENCRYPTION_ZERO_RTT;
  SerializedPacket packet2(QuicPacketNumber(2), PACKET_4BYTE_PACKET_NUMBER,
                           buffer2.data(), 500,
                           /*has_ack=*/false, /*has_stop_waiting=*/false);
  packet2.encryption_level = ENCRYPTION_FORWARD_SECURE;

  ASSERT_TRUE(coalesced.MaybeCoalescePacket(packet1, self_address, peer_address,
                                            &allocator, 1500, ECN_NOT_ECT, 0));
  ASSERT_TRUE(coalesced.MaybeCoalescePacket(packet2, self_address, peer_address,
                                            &allocator, 1500, ECN_NOT_ECT, 0));
  EXPECT_EQ(1000u, coalesced.length());
  EXPECT_EQ(coalesced.ecn_codepoint(), ECN_NOT_ECT);

  char copy_buffer[1000];
  size_t length_copied = 0;
  EXPECT_FALSE(
      coalesced.CopyEncryptedBuffers(copy_buffer, 900, &length_copied));
  ASSERT_TRUE(
      coalesced.CopyEncryptedBuffers(copy_buffer, 1000, &length_copied));
  EXPECT_EQ(1000u, length_copied);
  char expected[1000];
  memset(expected, 'a', 500);
  memset(expected + 500, 'b', 500);
  quiche::test::CompareCharArraysWithHexError("copied buffers", copy_buffer,
                                              length_copied, expected, 1000);
}

TEST(QuicCoalescedPacketTest, NeuterInitialPacket) {
  QuicCoalescedPacket coalesced;
  EXPECT_EQ("total_length: 0 padding_size: 0 packets: {}",
            coalesced.ToString(0));
  // Noop when neutering initial packet on a empty coalescer.
  coalesced.NeuterInitialPacket();
  EXPECT_EQ("total_length: 0 padding_size: 0 packets: {}",
            coalesced.ToString(0));

  quiche::SimpleBufferAllocator allocator;
  EXPECT_EQ(0u, coalesced.length());
  char buffer[1000];
  QuicSocketAddress self_address(QuicIpAddress::Loopback4(), 1);
  QuicSocketAddress peer_address(QuicIpAddress::Loopback4(), 2);
  SerializedPacket packet1(QuicPacketNumber(1), PACKET_4BYTE_PACKET_NUMBER,
                           buffer, 500, false, false);
  packet1.transmission_type = PTO_RETRANSMISSION;
  QuicAckFrame ack_frame(InitAckFrame(1));
  packet1.nonretransmittable_frames.push_back(QuicFrame(&ack_frame));
  packet1.retransmittable_frames.push_back(
      QuicFrame(QuicStreamFrame(1, true, 0, 100)));
  ASSERT_TRUE(coalesced.MaybeCoalescePacket(packet1, self_address, peer_address,
                                            &allocator, 1500, ECN_NOT_ECT, 0));
  EXPECT_EQ(PTO_RETRANSMISSION,
            coalesced.TransmissionTypeOfPacket(ENCRYPTION_INITIAL));
  EXPECT_EQ(1500u, coalesced.max_packet_length());
  EXPECT_EQ(500u, coalesced.length());
  EXPECT_EQ(
      "total_length: 1500 padding_size: 1000 packets: {ENCRYPTION_INITIAL}",
      coalesced.ToString(1500));
  EXPECT_EQ(coalesced.ecn_codepoint(), ECN_NOT_ECT);
  coalesced.NeuterInitialPacket();
  EXPECT_EQ(0u, coalesced.max_packet_length());
  EXPECT_EQ(0u, coalesced.length());
  EXPECT_EQ("total_length: 0 padding_size: 0 packets: {}",
            coalesced.ToString(0));
  EXPECT_EQ(coalesced.ecn_codepoint(), ECN_NOT_ECT);

  // Coalesce initial packet again.
  ASSERT_TRUE(coalesced.MaybeCoalescePacket(packet1, self_address, peer_address,
                                            &allocator, 1500, ECN_NOT_ECT, 0));

  SerializedPacket packet2(QuicPacketNumber(3), PACKET_4BYTE_PACKET_NUMBER,
                           buffer, 500, false, false);
  packet2.nonretransmittable_frames.push_back(QuicFrame(QuicPaddingFrame(100)));
  packet2.encryption_level = ENCRYPTION_ZERO_RTT;
  packet2.transmission_type = LOSS_RETRANSMISSION;
  ASSERT_TRUE(coalesced.MaybeCoalescePacket(packet2, self_address, peer_address,
                                            &allocator, 1500, ECN_NOT_ECT, 0));
  EXPECT_EQ(1500u, coalesced.max_packet_length());
  EXPECT_EQ(1000u, coalesced.length());
  EXPECT_EQ(LOSS_RETRANSMISSION,
            coalesced.TransmissionTypeOfPacket(ENCRYPTION_ZERO_RTT));
  EXPECT_EQ(
      "total_length: 1500 padding_size: 500 packets: {ENCRYPTION_INITIAL, "
      "ENCRYPTION_ZERO_RTT}",
      coalesced.ToString(1500));
  EXPECT_EQ(coalesced.ecn_codepoint(), ECN_NOT_ECT);

  // Neuter initial packet.
  coalesced.NeuterInitialPacket();
  EXPECT_EQ(1500u, coalesced.max_packet_length());
  EXPECT_EQ(500u, coalesced.length());
  EXPECT_EQ(
      "total_length: 1500 padding_size: 1000 packets: {ENCRYPTION_ZERO_RTT}",
      coalesced.ToString(1500));
  EXPECT_EQ(coalesced.ecn_codepoint(), ECN_NOT_ECT);

  SerializedPacket packet3(QuicPacketNumber(5), PACKET_4BYTE_PACKET_NUMBER,
                           buffer, 501, false, false);
  packet3.encryption_level = ENCRYPTION_FORWARD_SECURE;
  EXPECT_TRUE(coalesced.MaybeCoalescePacket(packet3, self_address, peer_address,
                                            &allocator, 1500, ECN_NOT_ECT, 0));
  EXPECT_EQ(1500u, coalesced.max_packet_length());
  EXPECT_EQ(1001u, coalesced.length());
  EXPECT_EQ(coalesced.ecn_codepoint(), ECN_NOT_ECT);
  // Neuter initial packet.
  coalesced.NeuterInitialPacket();
  EXPECT_EQ(1500u, coalesced.max_packet_length());
  EXPECT_EQ(1001u, coalesced.length());
  EXPECT_EQ(coalesced.ecn_codepoint(), ECN_NOT_ECT);
}

TEST(QuicCoalescedPacketTest, DoNotCoalesceDifferentEcn) {
  QuicCoalescedPacket coalesced;
  EXPECT_EQ("total_length: 0 padding_size: 0 packets: {}",
            coalesced.ToString(0));
  quiche::SimpleBufferAllocator allocator;
  EXPECT_EQ(0u, coalesced.length());
  EXPECT_EQ(0u, coalesced.NumberOfPackets());
  char buffer[1000];
  QuicSocketAddress self_address(QuicIpAddress::Loopback4(), 1);
  QuicSocketAddress peer_address(QuicIpAddress::Loopback4(), 2);
  SerializedPacket packet1(QuicPacketNumber(1), PACKET_4BYTE_PACKET_NUMBER,
                           buffer, 500, false, false);
  packet1.transmission_type = PTO_RETRANSMISSION;
  QuicAckFrame ack_frame(InitAckFrame(1));
  packet1.nonretransmittable_frames.push_back(QuicFrame(&ack_frame));
  packet1.retransmittable_frames.push_back(
      QuicFrame(QuicStreamFrame(1, true, 0, 100)));
  ASSERT_TRUE(coalesced.MaybeCoalescePacket(packet1, self_address, peer_address,
                                            &allocator, 1500, ECN_ECT1, 0));
  EXPECT_EQ(coalesced.ecn_codepoint(), ECN_ECT1);

  SerializedPacket packet2(QuicPacketNumber(2), PACKET_4BYTE_PACKET_NUMBER,
                           buffer, 500, false, false);
  packet2.nonretransmittable_frames.push_back(QuicFrame(QuicPaddingFrame(100)));
  packet2.encryption_level = ENCRYPTION_ZERO_RTT;
  packet2.transmission_type = LOSS_RETRANSMISSION;
  EXPECT_FALSE(coalesced.MaybeCoalescePacket(
      packet2, self_address, peer_address, &allocator, 1500, ECN_NOT_ECT, 0));
  EXPECT_EQ(coalesced.ecn_codepoint(), ECN_ECT1);
}

}  // namespace
}  // namespace test
}  // namespace quic
