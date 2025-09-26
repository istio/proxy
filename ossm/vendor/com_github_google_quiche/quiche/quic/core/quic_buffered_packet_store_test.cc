// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_buffered_packet_store.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/connection_id_generator.h"
#include "quiche/quic/core/crypto/transport_parameters.h"
#include "quiche/quic/core/frames/quic_frame.h"
#include "quiche/quic/core/frames/quic_padding_frame.h"
#include "quiche/quic/core/quic_connection_id.h"
#include "quiche/quic/core/quic_constants.h"
#include "quiche/quic/core/quic_dispatcher.h"
#include "quiche/quic/core/quic_dispatcher_stats.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/quic/core/quic_framer.h"
#include "quiche/quic/core/quic_packet_number.h"
#include "quiche/quic/core/quic_packet_writer.h"
#include "quiche/quic/core/quic_packets.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/platform/api/quic_ip_address.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/first_flight.h"
#include "quiche/quic/test_tools/mock_clock.h"
#include "quiche/quic/test_tools/mock_connection_id_generator.h"
#include "quiche/quic/test_tools/quic_buffered_packet_store_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/quiche_endian.h"

namespace quic {
static const size_t kDefaultMaxConnectionsInStore = 100;
static const size_t kMaxConnectionsWithoutCHLO =
    kDefaultMaxConnectionsInStore / 2;

namespace test {
namespace {

const std::optional<ParsedClientHello> kNoParsedChlo;
const std::optional<ParsedClientHello> kDefaultParsedChlo =
    std::make_optional<ParsedClientHello>();

using BufferedPacket = QuicBufferedPacketStore::BufferedPacket;
using BufferedPacketList = QuicBufferedPacketStore::BufferedPacketList;
using EnqueuePacketResult = QuicBufferedPacketStore::EnqueuePacketResult;
using ::testing::_;
using ::testing::A;
using ::testing::Conditional;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Ne;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::Truly;

EnqueuePacketResult EnqueuePacketToStore(
    QuicBufferedPacketStore& store, QuicConnectionId connection_id,
    PacketHeaderFormat form, QuicLongHeaderType long_packet_type,
    const QuicReceivedPacket& packet, QuicSocketAddress self_address,
    QuicSocketAddress peer_address, const ParsedQuicVersion& version,
    std::optional<ParsedClientHello> parsed_chlo,
    ConnectionIdGeneratorInterface& connection_id_generator) {
  ReceivedPacketInfo packet_info(self_address, peer_address, packet);
  packet_info.destination_connection_id = connection_id;
  packet_info.form = form;
  packet_info.long_packet_type = long_packet_type;
  packet_info.version = version;
  return store.EnqueuePacket(packet_info, std::move(parsed_chlo),
                             connection_id_generator);
}

class QuicBufferedPacketStoreVisitor
    : public QuicBufferedPacketStore::VisitorInterface {
 public:
  QuicBufferedPacketStoreVisitor() {}

  ~QuicBufferedPacketStoreVisitor() override {}

  void OnExpiredPackets(BufferedPacketList early_arrived_packets) override {
    last_expired_packet_queue_ = std::move(early_arrived_packets);
  }

  HandleCidCollisionResult HandleConnectionIdCollision(
      const QuicConnectionId& /*original_connection_id*/,
      const QuicConnectionId& /*replaced_connection_id*/,
      const QuicSocketAddress& /*self_address*/,
      const QuicSocketAddress& /*peer_address*/, ParsedQuicVersion /*version*/,
      const ParsedClientHello* /*parsed_chlo*/) override {
    return HandleCidCollisionResult::kOk;
  }

  // The packets queue for most recently expirect connection.
  BufferedPacketList last_expired_packet_queue_;
};

class QuicBufferedPacketStoreTest : public QuicTest {
 public:
  QuicBufferedPacketStoreTest()
      : store_(&visitor_, &clock_, &alarm_factory_, stats_),
        self_address_(QuicIpAddress::Any6(), 65535),
        peer_address_(QuicIpAddress::Any6(), 65535),
        packet_content_("some encrypted content"),
        packet_time_(QuicTime::Zero() + QuicTime::Delta::FromMicroseconds(42)),
        packet_(packet_content_.data(), packet_content_.size(), packet_time_),
        invalid_version_(UnsupportedQuicVersion()),
        valid_version_(CurrentSupportedVersions().front()) {
    store_.set_writer(&mock_packet_writer_);

    EXPECT_CALL(mock_packet_writer_, IsWriteBlocked())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(mock_packet_writer_, WritePacket(_, _, _, _, _, _))
        .WillRepeatedly(
            [&](const char* buffer, size_t buf_len, const QuicIpAddress&,
                const QuicSocketAddress&, PerPacketOptions*,
                const QuicPacketWriterParams&) {
              // This packet is sent by the store and "received" by the client.
              client_received_packets_.push_back(
                  std::make_unique<ClientReceivedPacket>(
                      buffer, buf_len, peer_address_, self_address_));
              return WriteResult(WRITE_STATUS_OK, buf_len);
            });
  }

 protected:
  QuicDispatcherStats stats_;
  QuicBufferedPacketStoreVisitor visitor_;
  MockClock clock_;
  MockAlarmFactory alarm_factory_;
  // Mock the sending of the INITIAL ACK packets.
  MockPacketWriter mock_packet_writer_;
  QuicBufferedPacketStore store_;
  QuicSocketAddress self_address_;
  QuicSocketAddress peer_address_;
  std::string packet_content_;
  QuicTime packet_time_;
  QuicReceivedPacket packet_;
  const ParsedQuicVersion invalid_version_;
  const ParsedQuicVersion valid_version_;
  MockConnectionIdGenerator connection_id_generator_;

  // A packet that is sent by the store and "received" by the client.
  struct ClientReceivedPacket {
    ClientReceivedPacket(const char* buffer, size_t buf_len,
                         const QuicSocketAddress& client_address,
                         const QuicSocketAddress& server_address)
        : self_address(client_address),
          peer_address(server_address),
          packet(QuicReceivedPacket(buffer, buf_len, QuicTime::Zero()).Clone()),
          packet_info(self_address, peer_address, *packet) {
      std::string detailed_error;
      MockConnectionIdGenerator unused_generator;
      absl::string_view destination_connection_id, source_connection_id;
      if (QuicFramer::ParsePublicHeaderDispatcherShortHeaderLengthUnknown(
              *packet, &packet_info.form, &packet_info.long_packet_type,
              &packet_info.version_flag, &packet_info.use_length_prefix,
              &packet_info.version_label, &packet_info.version,
              &destination_connection_id, &source_connection_id,
              &packet_info.retry_token, &detailed_error,
              unused_generator) != QUIC_NO_ERROR) {
        ADD_FAILURE() << "Failed to parse packet header: " << detailed_error;
      }
      packet_info.destination_connection_id =
          QuicConnectionId(destination_connection_id);
      packet_info.source_connection_id = QuicConnectionId(source_connection_id);
    }

    const QuicSocketAddress self_address;
    const QuicSocketAddress peer_address;
    std::unique_ptr<QuicReceivedPacket> packet;
    ReceivedPacketInfo packet_info;
  };
  std::vector<std::unique_ptr<ClientReceivedPacket>> client_received_packets_;
};

TEST_F(QuicBufferedPacketStoreTest, SimpleEnqueueAndDeliverPacket) {
  QuicConnectionId connection_id = TestConnectionId(1);
  EnqueuePacketToStore(store_, connection_id, GOOGLE_QUIC_Q043_PACKET,
                       INVALID_PACKET_TYPE, packet_, self_address_,
                       peer_address_, invalid_version_, kNoParsedChlo,
                       connection_id_generator_);
  EXPECT_TRUE(store_.HasBufferedPackets(connection_id));
  auto packets = store_.DeliverPackets(connection_id);
  const std::list<BufferedPacket>& queue = packets.buffered_packets;
  ASSERT_EQ(1u, queue.size());
  ASSERT_FALSE(packets.parsed_chlo.has_value());
  // There is no valid version because CHLO has not arrived.
  EXPECT_EQ(invalid_version_, packets.version);
  // Check content of the only packet in the queue.
  EXPECT_EQ(packet_content_, queue.front().packet->AsStringPiece());
  EXPECT_EQ(packet_time_, queue.front().packet->receipt_time());
  EXPECT_EQ(peer_address_, queue.front().peer_address);
  EXPECT_EQ(self_address_, queue.front().self_address);
  // No more packets on connection 1 should remain in the store.
  EXPECT_TRUE(store_.DeliverPackets(connection_id).buffered_packets.empty());
  EXPECT_FALSE(store_.HasBufferedPackets(connection_id));
}

TEST_F(QuicBufferedPacketStoreTest, SimpleEnqueueAckSent) {
  const QuicConnectionId kDCID = TestConnectionId(1);
  const std::string crypto_data = "crypto_data";
  ParsedQuicVersionVector versions = {ParsedQuicVersion::RFCv1()};
  std::unique_ptr<QuicEncryptedPacket> client_initial_packet(
      ConstructEncryptedPacket(
          kDCID, QuicConnectionId(), /*version_flag=*/true,
          /*reset_flag=*/false, /*packet_number=*/1, crypto_data,
          /*full_padding=*/true, CONNECTION_ID_PRESENT, CONNECTION_ID_PRESENT,
          PACKET_4BYTE_PACKET_NUMBER, &versions, Perspective::IS_CLIENT));
  QuicReceivedPacket received_client_initial(
      client_initial_packet->data(), client_initial_packet->length(),
      QuicTime::Zero(), false, 0, true, nullptr, 0, false, ECN_ECT1);
  ReceivedPacketInfo packet_info(self_address_, peer_address_,
                                 received_client_initial);
  std::string detailed_error;
  absl::string_view destination_connection_id, source_connection_id;
  ASSERT_EQ(QuicFramer::ParsePublicHeaderDispatcherShortHeaderLengthUnknown(
                received_client_initial, &packet_info.form,
                &packet_info.long_packet_type, &packet_info.version_flag,
                &packet_info.use_length_prefix, &packet_info.version_label,
                &packet_info.version, &destination_connection_id,
                &source_connection_id, &packet_info.retry_token,
                &detailed_error, connection_id_generator_),
            QUIC_NO_ERROR)
      << detailed_error;
  packet_info.destination_connection_id =
      QuicConnectionId(destination_connection_id);
  packet_info.source_connection_id = QuicConnectionId(source_connection_id);
  store_.EnqueuePacket(packet_info, kNoParsedChlo, connection_id_generator_);

  const BufferedPacketList* buffered_list = store_.GetPacketList(kDCID);
  ASSERT_NE(buffered_list, nullptr);
  ASSERT_EQ(buffered_list->dispatcher_sent_packets.size(), 1);
  EXPECT_EQ(buffered_list->dispatcher_sent_packets[0].largest_acked,
            QuicPacketNumber(1));
  ASSERT_EQ(client_received_packets_.size(), 1u);

  // Decrypt the packet, and verify it reports ECN.
  QuicFramer client_framer(ParsedQuicVersionVector{ParsedQuicVersion::RFCv1()},
                           QuicTime::Zero(), Perspective::IS_CLIENT, 8);
  client_framer.SetInitialObfuscators(kDCID);
  MockFramerVisitor mock_framer_visitor;
  client_framer.set_visitor(&mock_framer_visitor);
  EXPECT_CALL(mock_framer_visitor, OnPacket()).Times(1);
  EXPECT_CALL(mock_framer_visitor, OnAckFrameStart(_, _))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_framer_visitor, OnAckRange(_, _)).WillOnce(Return(true));
  std::optional<QuicEcnCounts> counts = QuicEcnCounts(0, 1, 0);
  EXPECT_CALL(mock_framer_visitor, OnAckFrameEnd(_, counts))
      .WillOnce(Return(true));
  client_framer.ProcessPacket(*client_received_packets_[0]->packet);
}

TEST_F(QuicBufferedPacketStoreTest, DifferentPacketAddressOnOneConnection) {
  QuicSocketAddress addr_with_new_port(QuicIpAddress::Any4(), 256);
  QuicConnectionId connection_id = TestConnectionId(1);
  EnqueuePacketToStore(store_, connection_id, GOOGLE_QUIC_Q043_PACKET,
                       INVALID_PACKET_TYPE, packet_, self_address_,
                       peer_address_, invalid_version_, kNoParsedChlo,
                       connection_id_generator_);
  EnqueuePacketToStore(store_, connection_id, GOOGLE_QUIC_Q043_PACKET,
                       INVALID_PACKET_TYPE, packet_, self_address_,
                       addr_with_new_port, invalid_version_, kNoParsedChlo,
                       connection_id_generator_);
  std::list<BufferedPacket> queue =
      store_.DeliverPackets(connection_id).buffered_packets;
  ASSERT_EQ(2u, queue.size());
  // The address migration path should be preserved.
  EXPECT_EQ(peer_address_, queue.front().peer_address);
  EXPECT_EQ(addr_with_new_port, queue.back().peer_address);
}

TEST_F(QuicBufferedPacketStoreTest,
       EnqueueAndDeliverMultiplePacketsOnMultipleConnections) {
  size_t num_connections = 10;
  for (uint64_t conn_id = 1; conn_id <= num_connections; ++conn_id) {
    QuicConnectionId connection_id = TestConnectionId(conn_id);
    EnqueuePacketToStore(store_, connection_id, GOOGLE_QUIC_Q043_PACKET,
                         INVALID_PACKET_TYPE, packet_, self_address_,
                         peer_address_, invalid_version_, kNoParsedChlo,
                         connection_id_generator_);
    EnqueuePacketToStore(store_, connection_id, GOOGLE_QUIC_Q043_PACKET,
                         INVALID_PACKET_TYPE, packet_, self_address_,
                         peer_address_, invalid_version_, kNoParsedChlo,
                         connection_id_generator_);
  }

  // Deliver packets in reversed order.
  for (uint64_t conn_id = num_connections; conn_id > 0; --conn_id) {
    QuicConnectionId connection_id = TestConnectionId(conn_id);
    std::list<BufferedPacket> queue =
        store_.DeliverPackets(connection_id).buffered_packets;
    ASSERT_EQ(2u, queue.size());
  }
}

// Tests that for one connection, only limited number of packets can be
// buffered.
TEST_F(QuicBufferedPacketStoreTest,
       FailToBufferTooManyPacketsOnExistingConnection) {
  // Max number of packets that can be buffered per connection.
  const size_t kMaxPacketsPerConnection = kDefaultMaxUndecryptablePackets;
  QuicConnectionId connection_id = TestConnectionId(1);
  EXPECT_EQ(QuicBufferedPacketStore::SUCCESS,
            EnqueuePacketToStore(store_, connection_id,
                                 IETF_QUIC_LONG_HEADER_PACKET, INITIAL, packet_,
                                 self_address_, peer_address_, valid_version_,
                                 kDefaultParsedChlo, connection_id_generator_));
  for (size_t i = 1; i <= kMaxPacketsPerConnection; ++i) {
    // All packets will be buffered except the last one.
    EnqueuePacketResult result = EnqueuePacketToStore(
        store_, connection_id, GOOGLE_QUIC_Q043_PACKET, INVALID_PACKET_TYPE,
        packet_, self_address_, peer_address_, invalid_version_, kNoParsedChlo,
        connection_id_generator_);
    if (i != kMaxPacketsPerConnection) {
      EXPECT_EQ(EnqueuePacketResult::SUCCESS, result);
    } else {
      EXPECT_EQ(EnqueuePacketResult::TOO_MANY_PACKETS, result);
    }
  }

  // Verify |kMaxPacketsPerConnection| packets are buffered.
  EXPECT_EQ(store_.DeliverPackets(connection_id).buffered_packets.size(),
            kMaxPacketsPerConnection);
}

TEST_F(QuicBufferedPacketStoreTest, ReachNonChloConnectionUpperLimit) {
  // Tests that store can only keep early arrived packets for limited number of
  // connections.
  const size_t kNumConnections = kMaxConnectionsWithoutCHLO + 1;
  for (uint64_t conn_id = 1; conn_id <= kNumConnections; ++conn_id) {
    QuicConnectionId connection_id = TestConnectionId(conn_id);
    EnqueuePacketResult result = EnqueuePacketToStore(
        store_, connection_id, GOOGLE_QUIC_Q043_PACKET, INVALID_PACKET_TYPE,
        packet_, self_address_, peer_address_, invalid_version_, kNoParsedChlo,
        connection_id_generator_);
    if (conn_id <= kMaxConnectionsWithoutCHLO) {
      EXPECT_EQ(EnqueuePacketResult::SUCCESS, result);
    } else {
      EXPECT_EQ(EnqueuePacketResult::TOO_MANY_CONNECTIONS, result);
    }
  }
  // Store only keeps early arrived packets upto |kNumConnections| connections.
  for (uint64_t conn_id = 1; conn_id <= kNumConnections; ++conn_id) {
    QuicConnectionId connection_id = TestConnectionId(conn_id);
    std::list<BufferedPacket> queue =
        store_.DeliverPackets(connection_id).buffered_packets;
    if (conn_id <= kMaxConnectionsWithoutCHLO) {
      EXPECT_EQ(1u, queue.size());
    } else {
      EXPECT_EQ(0u, queue.size());
    }
  }
}

TEST_F(QuicBufferedPacketStoreTest,
       FullStoreFailToBufferDataPacketOnNewConnection) {
  // Send enough CHLOs so that store gets full before number of connections
  // without CHLO reaches its upper limit.
  size_t num_chlos =
      kDefaultMaxConnectionsInStore - kMaxConnectionsWithoutCHLO + 1;
  for (uint64_t conn_id = 1; conn_id <= num_chlos; ++conn_id) {
    EXPECT_EQ(EnqueuePacketResult::SUCCESS,
              EnqueuePacketToStore(store_, TestConnectionId(conn_id),
                                   GOOGLE_QUIC_Q043_PACKET, INVALID_PACKET_TYPE,
                                   packet_, self_address_, peer_address_,
                                   valid_version_, kDefaultParsedChlo,
                                   connection_id_generator_));
  }

  // Send data packets on another |kMaxConnectionsWithoutCHLO| connections.
  // Store should only be able to buffer till it's full.
  for (uint64_t conn_id = num_chlos + 1;
       conn_id <= (kDefaultMaxConnectionsInStore + 1); ++conn_id) {
    QuicConnectionId connection_id = TestConnectionId(conn_id);
    EnqueuePacketResult result = EnqueuePacketToStore(
        store_, connection_id, GOOGLE_QUIC_Q043_PACKET, INVALID_PACKET_TYPE,
        packet_, self_address_, peer_address_, valid_version_,
        kDefaultParsedChlo, connection_id_generator_);
    if (conn_id <= kDefaultMaxConnectionsInStore) {
      EXPECT_EQ(EnqueuePacketResult::SUCCESS, result);
    } else {
      EXPECT_EQ(EnqueuePacketResult::TOO_MANY_CONNECTIONS, result);
    }
  }
}

TEST_F(QuicBufferedPacketStoreTest, BasicGeneratorBuffering) {
  EXPECT_EQ(EnqueuePacketResult::SUCCESS,
            EnqueuePacketToStore(
                store_, TestConnectionId(1), GOOGLE_QUIC_Q043_PACKET,
                INVALID_PACKET_TYPE, packet_, self_address_, peer_address_,
                valid_version_, kDefaultParsedChlo, connection_id_generator_));
  QuicConnectionId delivered_conn_id;
  BufferedPacketList packet_list =
      store_.DeliverPacketsForNextConnection(&delivered_conn_id);
  EXPECT_EQ(1u, packet_list.buffered_packets.size());
  EXPECT_EQ(delivered_conn_id, TestConnectionId(1));
  EXPECT_EQ(packet_list.connection_id_generator, nullptr);
}

TEST_F(QuicBufferedPacketStoreTest, GeneratorIgnoredForNonChlo) {
  MockConnectionIdGenerator generator2;
  EXPECT_EQ(EnqueuePacketResult::SUCCESS,
            EnqueuePacketToStore(
                store_, TestConnectionId(1), GOOGLE_QUIC_Q043_PACKET,
                INVALID_PACKET_TYPE, packet_, self_address_, peer_address_,
                valid_version_, kDefaultParsedChlo, connection_id_generator_));
  EXPECT_EQ(EnqueuePacketResult::SUCCESS,
            EnqueuePacketToStore(store_, TestConnectionId(1),
                                 GOOGLE_QUIC_Q043_PACKET, INVALID_PACKET_TYPE,
                                 packet_, self_address_, peer_address_,
                                 valid_version_, kNoParsedChlo, generator2));
  QuicConnectionId delivered_conn_id;
  BufferedPacketList packet_list =
      store_.DeliverPacketsForNextConnection(&delivered_conn_id);
  EXPECT_EQ(2u, packet_list.buffered_packets.size());
  EXPECT_EQ(delivered_conn_id, TestConnectionId(1));
  EXPECT_EQ(packet_list.connection_id_generator, nullptr);
}

TEST_F(QuicBufferedPacketStoreTest, EnqueueChloOnTooManyDifferentConnections) {
  // Buffer data packets on different connections upto limit.
  for (uint64_t conn_id = 1; conn_id <= kMaxConnectionsWithoutCHLO; ++conn_id) {
    QuicConnectionId connection_id = TestConnectionId(conn_id);
    EXPECT_EQ(EnqueuePacketResult::SUCCESS,
              // connection_id_generator_ will be ignored because the chlo has
              // not been parsed.
              EnqueuePacketToStore(
                  store_, connection_id, GOOGLE_QUIC_Q043_PACKET,
                  INVALID_PACKET_TYPE, packet_, self_address_, peer_address_,
                  invalid_version_, kNoParsedChlo, connection_id_generator_));
  }

  // Buffer CHLOs on other connections till store is full.
  for (size_t i = kMaxConnectionsWithoutCHLO + 1;
       i <= kDefaultMaxConnectionsInStore + 1; ++i) {
    QuicConnectionId connection_id = TestConnectionId(i);
    EnqueuePacketResult rs = EnqueuePacketToStore(
        store_, connection_id, GOOGLE_QUIC_Q043_PACKET, INVALID_PACKET_TYPE,
        packet_, self_address_, peer_address_, valid_version_,
        kDefaultParsedChlo, connection_id_generator_);
    if (i <= kDefaultMaxConnectionsInStore) {
      EXPECT_EQ(EnqueuePacketResult::SUCCESS, rs);
      EXPECT_TRUE(store_.HasChloForConnection(connection_id));
    } else {
      // Last CHLO can't be buffered because store is full.
      EXPECT_EQ(EnqueuePacketResult::TOO_MANY_CONNECTIONS, rs);
      EXPECT_FALSE(store_.HasChloForConnection(connection_id));
    }
  }

  // But buffering a CHLO belonging to a connection already has data packet
  // buffered in the store should success. This is the connection should be
  // delivered at last.
  EXPECT_EQ(EnqueuePacketResult::SUCCESS,
            EnqueuePacketToStore(
                store_, TestConnectionId(1), GOOGLE_QUIC_Q043_PACKET,
                INVALID_PACKET_TYPE, packet_, self_address_, peer_address_,
                valid_version_, kDefaultParsedChlo, connection_id_generator_));
  EXPECT_TRUE(store_.HasChloForConnection(TestConnectionId(1)));

  QuicConnectionId delivered_conn_id;
  for (size_t i = 0;
       i < kDefaultMaxConnectionsInStore - kMaxConnectionsWithoutCHLO + 1;
       ++i) {
    BufferedPacketList packet_list =
        store_.DeliverPacketsForNextConnection(&delivered_conn_id);
    if (i < kDefaultMaxConnectionsInStore - kMaxConnectionsWithoutCHLO) {
      // Only CHLO is buffered.
      EXPECT_EQ(1u, packet_list.buffered_packets.size());
      EXPECT_EQ(TestConnectionId(i + kMaxConnectionsWithoutCHLO + 1),
                delivered_conn_id);
    } else {
      EXPECT_EQ(2u, packet_list.buffered_packets.size());
      EXPECT_EQ(TestConnectionId(1u), delivered_conn_id);
    }
    EXPECT_EQ(packet_list.connection_id_generator, nullptr);
  }
  EXPECT_FALSE(store_.HasChlosBuffered());
}

// Tests that store expires long-staying connections appropriately for
// connections both with and without CHLOs.
TEST_F(QuicBufferedPacketStoreTest, PacketQueueExpiredBeforeDelivery) {
  QuicConnectionId connection_id = TestConnectionId(1);
  EnqueuePacketToStore(store_, connection_id, GOOGLE_QUIC_Q043_PACKET,
                       INVALID_PACKET_TYPE, packet_, self_address_,
                       peer_address_, invalid_version_, kNoParsedChlo,
                       connection_id_generator_);
  EXPECT_EQ(EnqueuePacketResult::SUCCESS,
            EnqueuePacketToStore(store_, connection_id, GOOGLE_QUIC_Q043_PACKET,
                                 INVALID_PACKET_TYPE, packet_, self_address_,
                                 peer_address_, valid_version_,
                                 kDefaultParsedChlo, connection_id_generator_));
  QuicConnectionId connection_id2 = TestConnectionId(2);
  EXPECT_EQ(EnqueuePacketResult::SUCCESS,
            EnqueuePacketToStore(
                store_, connection_id2, GOOGLE_QUIC_Q043_PACKET,
                INVALID_PACKET_TYPE, packet_, self_address_, peer_address_,
                invalid_version_, kNoParsedChlo, connection_id_generator_));

  // CHLO on connection 3 arrives 1ms later.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
  QuicConnectionId connection_id3 = TestConnectionId(3);
  // Use different client address to differentiate packets from different
  // connections.
  QuicSocketAddress another_client_address(QuicIpAddress::Any4(), 255);
  EnqueuePacketToStore(store_, connection_id3, GOOGLE_QUIC_Q043_PACKET,
                       INVALID_PACKET_TYPE, packet_, self_address_,
                       another_client_address, valid_version_,
                       kDefaultParsedChlo, connection_id_generator_);

  // Advance clock to the time when connection 1 and 2 expires.
  clock_.AdvanceTime(
      QuicBufferedPacketStorePeer::expiration_alarm(&store_)->deadline() -
      clock_.ApproximateNow());
  ASSERT_GE(clock_.ApproximateNow(),
            QuicBufferedPacketStorePeer::expiration_alarm(&store_)->deadline());
  // Fire alarm to remove long-staying connection 1 and 2 packets.
  alarm_factory_.FireAlarm(
      QuicBufferedPacketStorePeer::expiration_alarm(&store_));
  EXPECT_EQ(1u, visitor_.last_expired_packet_queue_.buffered_packets.size());
  EXPECT_FALSE(store_.HasBufferedPackets(connection_id));
  EXPECT_FALSE(store_.HasBufferedPackets(connection_id2));

  // Try to deliver packets, but packet queue has been removed so no
  // packets can be returned.
  ASSERT_EQ(0u, store_.DeliverPackets(connection_id).buffered_packets.size());
  ASSERT_EQ(0u, store_.DeliverPackets(connection_id2).buffered_packets.size());
  QuicConnectionId delivered_conn_id;
  BufferedPacketList packet_list =
      store_.DeliverPacketsForNextConnection(&delivered_conn_id);

  // Connection 3 is the next to be delivered as connection 1 already expired.
  EXPECT_EQ(connection_id3, delivered_conn_id);
  EXPECT_EQ(packet_list.connection_id_generator, nullptr);
  ASSERT_EQ(1u, packet_list.buffered_packets.size());
  // Packets in connection 3 should use another peer address.
  EXPECT_EQ(another_client_address,
            packet_list.buffered_packets.front().peer_address);

  // Test the alarm is reset by enqueueing 2 packets for 4th connection and wait
  // for them to expire.
  QuicConnectionId connection_id4 = TestConnectionId(4);
  EnqueuePacketToStore(store_, connection_id4, GOOGLE_QUIC_Q043_PACKET,
                       INVALID_PACKET_TYPE, packet_, self_address_,
                       peer_address_, invalid_version_, kNoParsedChlo,
                       connection_id_generator_);
  EnqueuePacketToStore(store_, connection_id4, GOOGLE_QUIC_Q043_PACKET,
                       INVALID_PACKET_TYPE, packet_, self_address_,
                       peer_address_, invalid_version_, kNoParsedChlo,
                       connection_id_generator_);
  clock_.AdvanceTime(
      QuicBufferedPacketStorePeer::expiration_alarm(&store_)->deadline() -
      clock_.ApproximateNow());
  alarm_factory_.FireAlarm(
      QuicBufferedPacketStorePeer::expiration_alarm(&store_));
  // |last_expired_packet_queue_| should be updated.
  EXPECT_EQ(2u, visitor_.last_expired_packet_queue_.buffered_packets.size());
}

TEST_F(QuicBufferedPacketStoreTest, SimpleDiscardPackets) {
  QuicConnectionId connection_id = TestConnectionId(1);

  // Enqueue some packets
  EnqueuePacketToStore(store_, connection_id, GOOGLE_QUIC_Q043_PACKET,
                       INVALID_PACKET_TYPE, packet_, self_address_,
                       peer_address_, invalid_version_, kNoParsedChlo,
                       connection_id_generator_);
  EnqueuePacketToStore(store_, connection_id, GOOGLE_QUIC_Q043_PACKET,
                       INVALID_PACKET_TYPE, packet_, self_address_,
                       peer_address_, invalid_version_, kNoParsedChlo,
                       connection_id_generator_);
  EXPECT_TRUE(store_.HasBufferedPackets(connection_id));
  EXPECT_FALSE(store_.HasChlosBuffered());

  // Dicard the packets
  store_.DiscardPackets(connection_id);

  // No packets on connection 1 should remain in the store
  EXPECT_TRUE(store_.DeliverPackets(connection_id).buffered_packets.empty());
  EXPECT_FALSE(store_.HasBufferedPackets(connection_id));
  EXPECT_FALSE(store_.HasChlosBuffered());

  // Check idempotency
  store_.DiscardPackets(connection_id);
  EXPECT_TRUE(store_.DeliverPackets(connection_id).buffered_packets.empty());
  EXPECT_FALSE(store_.HasBufferedPackets(connection_id));
  EXPECT_FALSE(store_.HasChlosBuffered());
}

TEST_F(QuicBufferedPacketStoreTest, DiscardWithCHLOs) {
  QuicConnectionId connection_id = TestConnectionId(1);

  // Enqueue some packets, which include a CHLO
  EnqueuePacketToStore(store_, connection_id, GOOGLE_QUIC_Q043_PACKET,
                       INVALID_PACKET_TYPE, packet_, self_address_,
                       peer_address_, invalid_version_, kNoParsedChlo,
                       connection_id_generator_);
  EnqueuePacketToStore(store_, connection_id, GOOGLE_QUIC_Q043_PACKET,
                       INVALID_PACKET_TYPE, packet_, self_address_,
                       peer_address_, valid_version_, kDefaultParsedChlo,
                       connection_id_generator_);
  EnqueuePacketToStore(store_, connection_id, GOOGLE_QUIC_Q043_PACKET,
                       INVALID_PACKET_TYPE, packet_, self_address_,
                       peer_address_, invalid_version_, kNoParsedChlo,
                       connection_id_generator_);
  EXPECT_TRUE(store_.HasBufferedPackets(connection_id));
  EXPECT_TRUE(store_.HasChlosBuffered());

  // Dicard the packets
  store_.DiscardPackets(connection_id);

  // No packets on connection 1 should remain in the store
  EXPECT_TRUE(store_.DeliverPackets(connection_id).buffered_packets.empty());
  EXPECT_FALSE(store_.HasBufferedPackets(connection_id));
  EXPECT_FALSE(store_.HasChlosBuffered());

  // Check idempotency
  store_.DiscardPackets(connection_id);
  EXPECT_TRUE(store_.DeliverPackets(connection_id).buffered_packets.empty());
  EXPECT_FALSE(store_.HasBufferedPackets(connection_id));
  EXPECT_FALSE(store_.HasChlosBuffered());
}

TEST_F(QuicBufferedPacketStoreTest, MultipleDiscardPackets) {
  QuicConnectionId connection_id_1 = TestConnectionId(1);
  QuicConnectionId connection_id_2 = TestConnectionId(2);

  // Enqueue some packets for two connection IDs
  EnqueuePacketToStore(store_, connection_id_1, GOOGLE_QUIC_Q043_PACKET,
                       INVALID_PACKET_TYPE, packet_, self_address_,
                       peer_address_, invalid_version_, kNoParsedChlo,
                       connection_id_generator_);
  EnqueuePacketToStore(store_, connection_id_1, GOOGLE_QUIC_Q043_PACKET,
                       INVALID_PACKET_TYPE, packet_, self_address_,
                       peer_address_, invalid_version_, kNoParsedChlo,
                       connection_id_generator_);

  ParsedClientHello parsed_chlo;
  parsed_chlo.alpns.push_back("h3");
  parsed_chlo.sni = TestHostname();
  EnqueuePacketToStore(store_, connection_id_2, IETF_QUIC_LONG_HEADER_PACKET,
                       INITIAL, packet_, self_address_, peer_address_,
                       valid_version_, parsed_chlo, connection_id_generator_);
  EXPECT_TRUE(store_.HasBufferedPackets(connection_id_1));
  EXPECT_TRUE(store_.HasBufferedPackets(connection_id_2));
  EXPECT_TRUE(store_.HasChlosBuffered());

  // Discard the packets for connection 1
  store_.DiscardPackets(connection_id_1);

  // No packets on connection 1 should remain in the store
  EXPECT_TRUE(store_.DeliverPackets(connection_id_1).buffered_packets.empty());
  EXPECT_FALSE(store_.HasBufferedPackets(connection_id_1));
  EXPECT_TRUE(store_.HasChlosBuffered());

  // Packets on connection 2 should remain
  EXPECT_TRUE(store_.HasBufferedPackets(connection_id_2));
  auto packets = store_.DeliverPackets(connection_id_2);
  EXPECT_EQ(1u, packets.buffered_packets.size());
  ASSERT_EQ(1u, packets.parsed_chlo->alpns.size());
  EXPECT_EQ("h3", packets.parsed_chlo->alpns[0]);
  EXPECT_EQ(TestHostname(), packets.parsed_chlo->sni);
  // Since connection_id_2's chlo arrives, verify version is set.
  EXPECT_EQ(valid_version_, packets.version);

  EXPECT_FALSE(store_.HasChlosBuffered());
  // Discard the packets for connection 2
  store_.DiscardPackets(connection_id_2);
  EXPECT_FALSE(store_.HasChlosBuffered());
}

TEST_F(QuicBufferedPacketStoreTest, DiscardPacketsEmpty) {
  // Check that DiscardPackets on an unknown connection ID is safe and does
  // nothing.
  QuicConnectionId connection_id = TestConnectionId(11235);
  EXPECT_FALSE(store_.HasBufferedPackets(connection_id));
  EXPECT_FALSE(store_.HasChlosBuffered());
  store_.DiscardPackets(connection_id);
  EXPECT_FALSE(store_.HasBufferedPackets(connection_id));
  EXPECT_FALSE(store_.HasChlosBuffered());
}

TEST_F(QuicBufferedPacketStoreTest, IngestPacketForTlsChloExtraction) {
  QuicConnectionId connection_id = TestConnectionId(1);
  std::vector<std::string> alpns;
  std::vector<uint16_t> supported_groups;
  std::vector<uint16_t> cert_compression_algos;
  std::string sni;
  bool resumption_attempted = false;
  bool early_data_attempted = false;
  QuicConfig config;
  std::optional<uint8_t> tls_alert;

  EXPECT_FALSE(store_.HasBufferedPackets(connection_id));
  EnqueuePacketToStore(store_, connection_id, GOOGLE_QUIC_Q043_PACKET,
                       INVALID_PACKET_TYPE, packet_, self_address_,
                       peer_address_, valid_version_, kNoParsedChlo,
                       connection_id_generator_);
  EXPECT_TRUE(store_.HasBufferedPackets(connection_id));

  // The packet in 'packet_' is not a TLS CHLO packet.
  EXPECT_FALSE(store_.IngestPacketForTlsChloExtraction(
      connection_id, valid_version_, packet_, &supported_groups,
      &cert_compression_algos, &alpns, &sni, &resumption_attempted,
      &early_data_attempted, &tls_alert));

  store_.DiscardPackets(connection_id);

  // Force the TLS CHLO to span multiple packets.
  constexpr auto kCustomParameterId =
      static_cast<TransportParameters::TransportParameterId>(0xff33);
  std::string kCustomParameterValue(2000, '-');
  config.custom_transport_parameters_to_send()[kCustomParameterId] =
      kCustomParameterValue;
  auto packets = GetFirstFlightOfPackets(valid_version_, config);
  ASSERT_EQ(packets.size(), 2u);

  EnqueuePacketToStore(store_, connection_id, GOOGLE_QUIC_Q043_PACKET,
                       INVALID_PACKET_TYPE, *packets[0], self_address_,
                       peer_address_, valid_version_, kNoParsedChlo,
                       connection_id_generator_);
  EnqueuePacketToStore(store_, connection_id, GOOGLE_QUIC_Q043_PACKET,
                       INVALID_PACKET_TYPE, *packets[1], self_address_,
                       peer_address_, valid_version_, kNoParsedChlo,
                       connection_id_generator_);

  EXPECT_TRUE(store_.HasBufferedPackets(connection_id));
  EXPECT_FALSE(store_.IngestPacketForTlsChloExtraction(
      connection_id, valid_version_, *packets[0], &supported_groups,
      &cert_compression_algos, &alpns, &sni, &resumption_attempted,
      &early_data_attempted, &tls_alert));
  EXPECT_TRUE(store_.IngestPacketForTlsChloExtraction(
      connection_id, valid_version_, *packets[1], &supported_groups,
      &cert_compression_algos, &alpns, &sni, &resumption_attempted,
      &early_data_attempted, &tls_alert));

  EXPECT_THAT(alpns, ElementsAre(AlpnForVersion(valid_version_)));
  EXPECT_FALSE(supported_groups.empty());
  EXPECT_EQ(sni, TestHostname());

  EXPECT_FALSE(resumption_attempted);
  EXPECT_FALSE(early_data_attempted);
}

TEST_F(QuicBufferedPacketStoreTest, DeliverInitialPacketsFirst) {
  QuicConfig config;
  QuicConnectionId connection_id = TestConnectionId(1);

  // Force the TLS CHLO to span multiple packets.
  constexpr auto kCustomParameterId =
      static_cast<TransportParameters::TransportParameterId>(0xff33);
  std::string custom_parameter_value(2000, '-');
  config.custom_transport_parameters_to_send()[kCustomParameterId] =
      custom_parameter_value;
  auto initial_packets = GetFirstFlightOfPackets(valid_version_, config);
  ASSERT_THAT(initial_packets, SizeIs(2));

  // Verify that the packets generated are INITIAL packets.
  EXPECT_THAT(
      initial_packets,
      Each(Truly([](const std::unique_ptr<QuicReceivedPacket>& packet) {
        QuicLongHeaderType long_packet_type = INVALID_PACKET_TYPE;
        PacketHeaderFormat unused_format;
        bool unused_version_flag;
        bool unused_use_length_prefix;
        QuicVersionLabel unused_version_label;
        ParsedQuicVersion unused_parsed_version = UnsupportedQuicVersion();
        absl::string_view unused_destination_connection_id;
        absl::string_view unused_source_connection_id;
        std::optional<absl::string_view> unused_retry_token;
        std::string unused_detailed_error;
        QuicErrorCode error_code = QuicFramer::ParsePublicHeaderDispatcher(
            *packet, kQuicDefaultConnectionIdLength, &unused_format,
            &long_packet_type, &unused_version_flag, &unused_use_length_prefix,
            &unused_version_label, &unused_parsed_version,
            &unused_destination_connection_id, &unused_source_connection_id,
            &unused_retry_token, &unused_detailed_error);
        return error_code == QUIC_NO_ERROR && long_packet_type == INITIAL;
      })));

  QuicLongHeaderType long_packet_type = INVALID_PACKET_TYPE;
  PacketHeaderFormat packet_format;
  bool unused_version_flag;
  bool unused_use_length_prefix;
  QuicVersionLabel unused_version_label;
  ParsedQuicVersion unused_parsed_version = UnsupportedQuicVersion();
  absl::string_view unused_destination_connection_id;
  absl::string_view unused_source_connection_id;
  std::optional<absl::string_view> unused_retry_token;
  std::string unused_detailed_error;
  QuicErrorCode error_code = QUIC_NO_ERROR;

  // Verify that packet_ is not an INITIAL packet.
  error_code = QuicFramer::ParsePublicHeaderDispatcher(
      packet_, kQuicDefaultConnectionIdLength, &packet_format,
      &long_packet_type, &unused_version_flag, &unused_use_length_prefix,
      &unused_version_label, &unused_parsed_version,
      &unused_destination_connection_id, &unused_source_connection_id,
      &unused_retry_token, &unused_detailed_error);
  EXPECT_THAT(error_code, IsQuicNoError());
  EXPECT_NE(long_packet_type, INITIAL);

  EnqueuePacketToStore(store_, connection_id, packet_format, long_packet_type,
                       packet_, self_address_, peer_address_, valid_version_,
                       kNoParsedChlo, connection_id_generator_);
  EnqueuePacketToStore(store_, connection_id, IETF_QUIC_LONG_HEADER_PACKET,
                       INITIAL, *initial_packets[0], self_address_,
                       peer_address_, valid_version_, kNoParsedChlo,
                       connection_id_generator_);
  EnqueuePacketToStore(store_, connection_id, IETF_QUIC_LONG_HEADER_PACKET,
                       INITIAL, *initial_packets[1], self_address_,
                       peer_address_, valid_version_, kNoParsedChlo,
                       connection_id_generator_);

  BufferedPacketList delivered_packets = store_.DeliverPackets(connection_id);
  EXPECT_THAT(delivered_packets.buffered_packets, SizeIs(3));

  QuicLongHeaderType previous_packet_type = INITIAL;
  for (const auto& packet : delivered_packets.buffered_packets) {
    error_code = QuicFramer::ParsePublicHeaderDispatcher(
        *packet.packet, kQuicDefaultConnectionIdLength, &packet_format,
        &long_packet_type, &unused_version_flag, &unused_use_length_prefix,
        &unused_version_label, &unused_parsed_version,
        &unused_destination_connection_id, &unused_source_connection_id,
        &unused_retry_token, &unused_detailed_error);
    EXPECT_THAT(error_code, IsQuicNoError());

    // INITIAL packets should not follow a non-INITIAL packet.
    EXPECT_THAT(long_packet_type,
                Conditional(previous_packet_type == INITIAL,
                            A<QuicLongHeaderType>(), Ne(INITIAL)));
    previous_packet_type = long_packet_type;
  }
}

// Test for b/316633326.
TEST_F(QuicBufferedPacketStoreTest, BufferedPacketRetainsEcn) {
  QuicConnectionId connection_id = TestConnectionId(1);
  QuicReceivedPacket ect1_packet(packet_content_.data(), packet_content_.size(),
                                 packet_time_, false, 0, true, nullptr, 0,
                                 false, ECN_ECT1);
  EnqueuePacketToStore(store_, connection_id, GOOGLE_QUIC_Q043_PACKET,
                       INVALID_PACKET_TYPE, ect1_packet, self_address_,
                       peer_address_, valid_version_, kNoParsedChlo,
                       connection_id_generator_);
  BufferedPacketList delivered_packets = store_.DeliverPackets(connection_id);
  EXPECT_THAT(delivered_packets.buffered_packets, SizeIs(1));
  for (const auto& packet : delivered_packets.buffered_packets) {
    EXPECT_EQ(packet.packet->ecn_codepoint(), ECN_ECT1);
  }
}

TEST_F(QuicBufferedPacketStoreTest, InitialAckHasClientConnectionId) {
  const QuicConnectionId kDCID = TestConnectionId(1);
  const QuicConnectionId kSCID = TestConnectionId(42);
  const std::string crypto_data = "crypto_data";
  ParsedQuicVersionVector versions = {ParsedQuicVersion::RFCv1()};
  std::unique_ptr<QuicEncryptedPacket> client_initial_packet(
      ConstructEncryptedPacket(
          kDCID, kSCID, /*version_flag=*/true, /*reset_flag=*/false,
          /*packet_number=*/1, crypto_data, /*full_padding=*/true,
          CONNECTION_ID_PRESENT, CONNECTION_ID_PRESENT,
          PACKET_4BYTE_PACKET_NUMBER, &versions, Perspective::IS_CLIENT));

  QuicReceivedPacket received_client_initial(client_initial_packet->data(),
                                             client_initial_packet->length(),
                                             QuicTime::Zero());
  ReceivedPacketInfo packet_info(self_address_, peer_address_,
                                 received_client_initial);
  std::string detailed_error;
  absl::string_view destination_connection_id, source_connection_id;
  ASSERT_EQ(QuicFramer::ParsePublicHeaderDispatcherShortHeaderLengthUnknown(
                received_client_initial, &packet_info.form,
                &packet_info.long_packet_type, &packet_info.version_flag,
                &packet_info.use_length_prefix, &packet_info.version_label,
                &packet_info.version, &destination_connection_id,
                &source_connection_id, &packet_info.retry_token,
                &detailed_error, connection_id_generator_),
            QUIC_NO_ERROR)
      << detailed_error;
  packet_info.destination_connection_id =
      QuicConnectionId(destination_connection_id);
  packet_info.source_connection_id = QuicConnectionId(source_connection_id);
  store_.EnqueuePacket(packet_info, kNoParsedChlo, connection_id_generator_);
  ASSERT_EQ(client_received_packets_.size(), 1u);

  const ReceivedPacketInfo& client_received_packet_info =
      client_received_packets_[0]->packet_info;
  // From the client's perspective, the destination connection ID is kSCID and
  // the source connection ID is kDCID.
  EXPECT_EQ(client_received_packet_info.destination_connection_id, kSCID);
  EXPECT_EQ(client_received_packet_info.source_connection_id, kDCID);
}

TEST_F(QuicBufferedPacketStoreTest, EmptyBufferedPacketList) {
  BufferedPacketList packet_list;
  EXPECT_TRUE(packet_list.buffered_packets.empty());
  EXPECT_FALSE(packet_list.parsed_chlo.has_value());
  EXPECT_FALSE(packet_list.version.IsKnown());
  EXPECT_TRUE(packet_list.original_connection_id.IsEmpty());
  EXPECT_FALSE(packet_list.replaced_connection_id.has_value());
}

}  // namespace
}  // namespace test
}  // namespace quic
