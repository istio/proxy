// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_time_wait_list_manager.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "quiche/quic/core/crypto/quic_random.h"
#include "quiche/quic/core/quic_alarm.h"
#include "quiche/quic/core/quic_alarm_factory.h"
#include "quiche/quic/core/quic_arena_scoped_ptr.h"
#include "quiche/quic/core/quic_connection_id.h"
#include "quiche/quic/core/quic_framer.h"
#include "quiche/quic/core/quic_one_block_arena.h"
#include "quiche/quic/core/quic_packet_writer.h"
#include "quiche/quic/core/quic_packets.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/mock_clock.h"
#include "quiche/quic/test_tools/mock_quic_session_visitor.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/quic/test_tools/quic_time_wait_list_manager_peer.h"

using testing::_;
using testing::Args;
using testing::Assign;
using testing::DoAll;
using testing::Matcher;
using testing::NiceMock;
using testing::Return;
using testing::ReturnPointee;
using testing::StrictMock;
using testing::Truly;

namespace quic {
namespace test {
namespace {

const size_t kTestPacketSize = 100;

class FramerVisitorCapturingPublicReset : public NoOpFramerVisitor {
 public:
  FramerVisitorCapturingPublicReset(QuicConnectionId connection_id)
      : connection_id_(connection_id) {}
  ~FramerVisitorCapturingPublicReset() override = default;

  bool IsValidStatelessResetToken(
      const StatelessResetToken& token) const override {
    return token == QuicUtils::GenerateStatelessResetToken(connection_id_);
  }

  void OnAuthenticatedIetfStatelessResetPacket(
      const QuicIetfStatelessResetPacket& packet) override {
    stateless_reset_packet_ = packet;
  }

  QuicIetfStatelessResetPacket stateless_reset_packet() {
    return stateless_reset_packet_;
  }

 private:
  QuicIetfStatelessResetPacket stateless_reset_packet_;
  QuicConnectionId connection_id_;
};

class MockAlarmFactory;
class MockAlarm : public QuicAlarm {
 public:
  explicit MockAlarm(QuicArenaScopedPtr<Delegate> delegate, int alarm_index,
                     MockAlarmFactory* factory)
      : QuicAlarm(std::move(delegate)),
        alarm_index_(alarm_index),
        factory_(factory) {}
  virtual ~MockAlarm() {}

  void SetImpl() override;
  void CancelImpl() override;

 private:
  int alarm_index_;
  MockAlarmFactory* factory_;
};

class MockAlarmFactory : public QuicAlarmFactory {
 public:
  ~MockAlarmFactory() override {}

  // Creates a new platform-specific alarm which will be configured to notify
  // |delegate| when the alarm fires. Returns an alarm allocated on the heap.
  // Caller takes ownership of the new alarm, which will not yet be "set" to
  // fire.
  QuicAlarm* CreateAlarm(QuicAlarm::Delegate* delegate) override {
    return new MockAlarm(QuicArenaScopedPtr<QuicAlarm::Delegate>(delegate),
                         alarm_index_++, this);
  }
  QuicArenaScopedPtr<QuicAlarm> CreateAlarm(
      QuicArenaScopedPtr<QuicAlarm::Delegate> delegate,
      QuicConnectionArena* arena) override {
    if (arena != nullptr) {
      return arena->New<MockAlarm>(std::move(delegate), alarm_index_++, this);
    }
    return QuicArenaScopedPtr<MockAlarm>(
        new MockAlarm(std::move(delegate), alarm_index_++, this));
  }
  MOCK_METHOD(void, OnAlarmSet, (int, QuicTime), ());
  MOCK_METHOD(void, OnAlarmCancelled, (int), ());

 private:
  int alarm_index_ = 0;
};

void MockAlarm::SetImpl() { factory_->OnAlarmSet(alarm_index_, deadline()); }

void MockAlarm::CancelImpl() { factory_->OnAlarmCancelled(alarm_index_); }

class QuicTimeWaitListManagerTest : public QuicTest {
 protected:
  QuicTimeWaitListManagerTest()
      : time_wait_list_manager_(&writer_, &visitor_, &clock_, &alarm_factory_),
        connection_id_(TestConnectionId(45)),
        peer_address_(TestPeerIPAddress(), kTestPort),
        writer_is_blocked_(false) {}

  ~QuicTimeWaitListManagerTest() override = default;

  void SetUp() override {
    EXPECT_CALL(writer_, IsWriteBlocked())
        .WillRepeatedly(ReturnPointee(&writer_is_blocked_));
  }

  void AddConnectionId(QuicConnectionId connection_id,
                       QuicTimeWaitListManager::TimeWaitAction action) {
    AddConnectionId(connection_id, action, nullptr);
  }

  void AddStatelessConnectionId(QuicConnectionId connection_id) {
    std::vector<std::unique_ptr<QuicEncryptedPacket>> termination_packets;
    termination_packets.push_back(std::unique_ptr<QuicEncryptedPacket>(
        new QuicEncryptedPacket(nullptr, 0, false)));
    time_wait_list_manager_.AddConnectionIdToTimeWait(
        QuicTimeWaitListManager::SEND_TERMINATION_PACKETS,
        TimeWaitConnectionInfo(false, &termination_packets, {connection_id}));
  }

  void AddConnectionId(
      QuicConnectionId connection_id,
      QuicTimeWaitListManager::TimeWaitAction action,
      std::vector<std::unique_ptr<QuicEncryptedPacket>>* packets) {
    time_wait_list_manager_.AddConnectionIdToTimeWait(
        action,
        TimeWaitConnectionInfo(/*ietf_quic=*/true, packets, {connection_id}));
  }

  bool IsConnectionIdInTimeWait(QuicConnectionId connection_id) {
    return time_wait_list_manager_.IsConnectionIdInTimeWait(connection_id);
  }

  void ProcessPacket(QuicConnectionId connection_id) {
    time_wait_list_manager_.ProcessPacket(
        self_address_, peer_address_, connection_id, GOOGLE_QUIC_Q043_PACKET,
        kTestPacketSize);
  }

  QuicEncryptedPacket* ConstructEncryptedPacket(
      QuicConnectionId destination_connection_id,
      QuicConnectionId source_connection_id, uint64_t packet_number) {
    return quic::test::ConstructEncryptedPacket(destination_connection_id,
                                                source_connection_id, false,
                                                false, packet_number, "data");
  }

  MockClock clock_;
  MockAlarmFactory alarm_factory_;
  NiceMock<MockPacketWriter> writer_;
  StrictMock<MockQuicSessionVisitor> visitor_;
  QuicTimeWaitListManager time_wait_list_manager_;
  QuicConnectionId connection_id_;
  QuicSocketAddress self_address_;
  QuicSocketAddress peer_address_;
  bool writer_is_blocked_;
};

bool ValidPublicResetPacketPredicate(
    QuicConnectionId expected_connection_id,
    const std::tuple<const char*, int>& packet_buffer) {
  FramerVisitorCapturingPublicReset visitor(expected_connection_id);
  QuicFramer framer(AllSupportedVersions(), QuicTime::Zero(),
                    Perspective::IS_CLIENT, kQuicDefaultConnectionIdLength);
  framer.set_visitor(&visitor);
  QuicEncryptedPacket encrypted(std::get<0>(packet_buffer),
                                std::get<1>(packet_buffer));
  framer.ProcessPacket(encrypted);

  QuicIetfStatelessResetPacket stateless_reset =
      visitor.stateless_reset_packet();

  StatelessResetToken expected_stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(expected_connection_id);

  return stateless_reset.stateless_reset_token ==
         expected_stateless_reset_token;
}

Matcher<const std::tuple<const char*, int>> PublicResetPacketEq(
    QuicConnectionId connection_id) {
  return Truly(
      [connection_id](const std::tuple<const char*, int> packet_buffer) {
        return ValidPublicResetPacketPredicate(connection_id, packet_buffer);
      });
}

TEST_F(QuicTimeWaitListManagerTest, CheckConnectionIdInTimeWait) {
  EXPECT_FALSE(IsConnectionIdInTimeWait(connection_id_));
  AddConnectionId(connection_id_, QuicTimeWaitListManager::DO_NOTHING);
  EXPECT_EQ(1u, time_wait_list_manager_.num_connections());
  EXPECT_TRUE(IsConnectionIdInTimeWait(connection_id_));
}

TEST_F(QuicTimeWaitListManagerTest, CheckStatelessConnectionIdInTimeWait) {
  EXPECT_FALSE(IsConnectionIdInTimeWait(connection_id_));
  AddStatelessConnectionId(connection_id_);
  EXPECT_EQ(1u, time_wait_list_manager_.num_connections());
  EXPECT_TRUE(IsConnectionIdInTimeWait(connection_id_));
}

TEST_F(QuicTimeWaitListManagerTest, SendVersionNegotiationPacket) {
  std::unique_ptr<QuicEncryptedPacket> packet(
      QuicFramer::BuildVersionNegotiationPacket(
          connection_id_, EmptyQuicConnectionId(), /*ietf_quic=*/false,
          /*use_length_prefix=*/false, AllSupportedVersions()));
  EXPECT_CALL(writer_, WritePacket(_, packet->length(), self_address_.host(),
                                   peer_address_, _, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));

  time_wait_list_manager_.SendVersionNegotiationPacket(
      connection_id_, EmptyQuicConnectionId(), /*ietf_quic=*/false,
      /*use_length_prefix=*/false, AllSupportedVersions(), self_address_,
      peer_address_);
  EXPECT_EQ(0u, time_wait_list_manager_.num_connections());
}

TEST_F(QuicTimeWaitListManagerTest,
       SendIetfVersionNegotiationPacketWithoutLengthPrefix) {
  std::unique_ptr<QuicEncryptedPacket> packet(
      QuicFramer::BuildVersionNegotiationPacket(
          connection_id_, EmptyQuicConnectionId(), /*ietf_quic=*/true,
          /*use_length_prefix=*/false, AllSupportedVersions()));
  EXPECT_CALL(writer_, WritePacket(_, packet->length(), self_address_.host(),
                                   peer_address_, _, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));

  time_wait_list_manager_.SendVersionNegotiationPacket(
      connection_id_, EmptyQuicConnectionId(), /*ietf_quic=*/true,
      /*use_length_prefix=*/false, AllSupportedVersions(), self_address_,
      peer_address_);
  EXPECT_EQ(0u, time_wait_list_manager_.num_connections());
}

TEST_F(QuicTimeWaitListManagerTest, SendIetfVersionNegotiationPacket) {
  std::unique_ptr<QuicEncryptedPacket> packet(
      QuicFramer::BuildVersionNegotiationPacket(
          connection_id_, EmptyQuicConnectionId(), /*ietf_quic=*/true,
          /*use_length_prefix=*/true, AllSupportedVersions()));
  EXPECT_CALL(writer_, WritePacket(_, packet->length(), self_address_.host(),
                                   peer_address_, _, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));

  time_wait_list_manager_.SendVersionNegotiationPacket(
      connection_id_, EmptyQuicConnectionId(), /*ietf_quic=*/true,
      /*use_length_prefix=*/true, AllSupportedVersions(), self_address_,
      peer_address_);
  EXPECT_EQ(0u, time_wait_list_manager_.num_connections());
}

TEST_F(QuicTimeWaitListManagerTest,
       SendIetfVersionNegotiationPacketWithClientConnectionId) {
  std::unique_ptr<QuicEncryptedPacket> packet(
      QuicFramer::BuildVersionNegotiationPacket(
          connection_id_, TestConnectionId(0x33), /*ietf_quic=*/true,
          /*use_length_prefix=*/true, AllSupportedVersions()));
  EXPECT_CALL(writer_, WritePacket(_, packet->length(), self_address_.host(),
                                   peer_address_, _, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));

  time_wait_list_manager_.SendVersionNegotiationPacket(
      connection_id_, TestConnectionId(0x33), /*ietf_quic=*/true,
      /*use_length_prefix=*/true, AllSupportedVersions(), self_address_,
      peer_address_);
  EXPECT_EQ(0u, time_wait_list_manager_.num_connections());
}

TEST_F(QuicTimeWaitListManagerTest, SendConnectionClose) {
  const size_t kConnectionCloseLength = 100;
  std::vector<std::unique_ptr<QuicEncryptedPacket>> termination_packets;
  termination_packets.push_back(
      std::unique_ptr<QuicEncryptedPacket>(new QuicEncryptedPacket(
          new char[kConnectionCloseLength], kConnectionCloseLength, true)));
  AddConnectionId(connection_id_,
                  QuicTimeWaitListManager::SEND_CONNECTION_CLOSE_PACKETS,
                  &termination_packets);
  EXPECT_CALL(writer_, WritePacket(_, kConnectionCloseLength,
                                   self_address_.host(), peer_address_, _, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));

  ProcessPacket(connection_id_);
}

TEST_F(QuicTimeWaitListManagerTest, SendTwoConnectionCloses) {
  const size_t kConnectionCloseLength = 100;
  std::vector<std::unique_ptr<QuicEncryptedPacket>> termination_packets;
  termination_packets.push_back(
      std::unique_ptr<QuicEncryptedPacket>(new QuicEncryptedPacket(
          new char[kConnectionCloseLength], kConnectionCloseLength, true)));
  termination_packets.push_back(
      std::unique_ptr<QuicEncryptedPacket>(new QuicEncryptedPacket(
          new char[kConnectionCloseLength], kConnectionCloseLength, true)));
  AddConnectionId(connection_id_,
                  QuicTimeWaitListManager::SEND_CONNECTION_CLOSE_PACKETS,
                  &termination_packets);
  EXPECT_CALL(writer_, WritePacket(_, kConnectionCloseLength,
                                   self_address_.host(), peer_address_, _, _))
      .Times(2)
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));

  ProcessPacket(connection_id_);
}

TEST_F(QuicTimeWaitListManagerTest, SendPublicReset) {
  AddConnectionId(connection_id_,
                  QuicTimeWaitListManager::SEND_STATELESS_RESET);
  EXPECT_CALL(writer_,
              WritePacket(_, _, self_address_.host(), peer_address_, _, _))
      .With(Args<0, 1>(PublicResetPacketEq(connection_id_)))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 0)));

  ProcessPacket(connection_id_);
}

TEST_F(QuicTimeWaitListManagerTest, SendPublicResetWithExponentialBackOff) {
  AddConnectionId(connection_id_,
                  QuicTimeWaitListManager::SEND_STATELESS_RESET);
  EXPECT_EQ(1u, time_wait_list_manager_.num_connections());
  for (int packet_number = 1; packet_number < 101; ++packet_number) {
    if ((packet_number & (packet_number - 1)) == 0) {
      EXPECT_CALL(writer_, WritePacket(_, _, _, _, _, _))
          .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));
    }
    ProcessPacket(connection_id_);
    // Send public reset with exponential back off.
    if ((packet_number & (packet_number - 1)) == 0) {
      EXPECT_TRUE(QuicTimeWaitListManagerPeer::ShouldSendResponse(
          &time_wait_list_manager_, packet_number));
    } else {
      EXPECT_FALSE(QuicTimeWaitListManagerPeer::ShouldSendResponse(
          &time_wait_list_manager_, packet_number));
    }
  }
}

TEST_F(QuicTimeWaitListManagerTest, NoPublicResetForStatelessConnections) {
  AddStatelessConnectionId(connection_id_);

  EXPECT_CALL(writer_,
              WritePacket(_, _, self_address_.host(), peer_address_, _, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));

  ProcessPacket(connection_id_);
}

TEST_F(QuicTimeWaitListManagerTest, CleanUpOldConnectionIds) {
  const size_t kConnectionIdCount = 100;
  const size_t kOldConnectionIdCount = 31;

  // Add connection_ids such that their expiry time is time_wait_period_.
  for (uint64_t conn_id = 1; conn_id <= kOldConnectionIdCount; ++conn_id) {
    QuicConnectionId connection_id = TestConnectionId(conn_id);
    AddConnectionId(connection_id, QuicTimeWaitListManager::DO_NOTHING);
  }
  EXPECT_EQ(kOldConnectionIdCount, time_wait_list_manager_.num_connections());

  // Add remaining connection_ids such that their add time is
  // 2 * time_wait_period_.
  const QuicTime::Delta time_wait_period =
      QuicTimeWaitListManagerPeer::time_wait_period(&time_wait_list_manager_);
  clock_.AdvanceTime(time_wait_period);
  for (uint64_t conn_id = kOldConnectionIdCount + 1;
       conn_id <= kConnectionIdCount; ++conn_id) {
    QuicConnectionId connection_id = TestConnectionId(conn_id);
    AddConnectionId(connection_id, QuicTimeWaitListManager::DO_NOTHING);
  }
  EXPECT_EQ(kConnectionIdCount, time_wait_list_manager_.num_connections());

  QuicTime::Delta offset = QuicTime::Delta::FromMicroseconds(39);
  // Now set the current time as time_wait_period + offset usecs.
  clock_.AdvanceTime(offset);
  // After all the old connection_ids are cleaned up, check the next alarm
  // interval.
  QuicTime next_alarm_time = clock_.Now() + time_wait_period - offset;
  EXPECT_CALL(alarm_factory_, OnAlarmSet(_, next_alarm_time));

  time_wait_list_manager_.CleanUpOldConnectionIds();
  for (uint64_t conn_id = 1; conn_id <= kConnectionIdCount; ++conn_id) {
    QuicConnectionId connection_id = TestConnectionId(conn_id);
    EXPECT_EQ(conn_id > kOldConnectionIdCount,
              IsConnectionIdInTimeWait(connection_id))
        << "kOldConnectionIdCount: " << kOldConnectionIdCount
        << " connection_id: " << connection_id;
  }
  EXPECT_EQ(kConnectionIdCount - kOldConnectionIdCount,
            time_wait_list_manager_.num_connections());
}

TEST_F(QuicTimeWaitListManagerTest,
       CleanUpOldConnectionIdsForMultipleConnectionIdsPerConnection) {
  connection_id_ = TestConnectionId(7);
  const size_t kConnectionCloseLength = 100;
  std::vector<std::unique_ptr<QuicEncryptedPacket>> termination_packets;
  termination_packets.push_back(
      std::unique_ptr<QuicEncryptedPacket>(new QuicEncryptedPacket(
          new char[kConnectionCloseLength], kConnectionCloseLength, true)));

  // Add a CONNECTION_CLOSE termination packet.
  std::vector<QuicConnectionId> active_connection_ids{connection_id_,
                                                      TestConnectionId(8)};
  time_wait_list_manager_.AddConnectionIdToTimeWait(
      QuicTimeWaitListManager::SEND_CONNECTION_CLOSE_PACKETS,
      TimeWaitConnectionInfo(/*ietf_quic=*/true, &termination_packets,
                             active_connection_ids, QuicTime::Delta::Zero()));

  EXPECT_TRUE(
      time_wait_list_manager_.IsConnectionIdInTimeWait(TestConnectionId(7)));
  EXPECT_TRUE(
      time_wait_list_manager_.IsConnectionIdInTimeWait(TestConnectionId(8)));

  // Remove these IDs.
  const QuicTime::Delta time_wait_period =
      QuicTimeWaitListManagerPeer::time_wait_period(&time_wait_list_manager_);
  clock_.AdvanceTime(time_wait_period);
  time_wait_list_manager_.CleanUpOldConnectionIds();

  EXPECT_FALSE(
      time_wait_list_manager_.IsConnectionIdInTimeWait(TestConnectionId(7)));
  EXPECT_FALSE(
      time_wait_list_manager_.IsConnectionIdInTimeWait(TestConnectionId(8)));
}

TEST_F(QuicTimeWaitListManagerTest, SendQueuedPackets) {
  QuicConnectionId connection_id = TestConnectionId(1);
  AddConnectionId(connection_id, QuicTimeWaitListManager::SEND_STATELESS_RESET);
  std::unique_ptr<QuicEncryptedPacket> packet(ConstructEncryptedPacket(
      connection_id, EmptyQuicConnectionId(), /*packet_number=*/234));
  // Let first write through.
  EXPECT_CALL(writer_,
              WritePacket(_, _, self_address_.host(), peer_address_, _, _))
      .With(Args<0, 1>(PublicResetPacketEq(connection_id)))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, packet->length())));
  ProcessPacket(connection_id);

  // write block for the next packet.
  EXPECT_CALL(writer_,
              WritePacket(_, _, self_address_.host(), peer_address_, _, _))
      .With(Args<0, 1>(PublicResetPacketEq(connection_id)))
      .WillOnce(DoAll(Assign(&writer_is_blocked_, true),
                      Return(WriteResult(WRITE_STATUS_BLOCKED, EAGAIN))));
  EXPECT_CALL(visitor_, OnWriteBlocked(&time_wait_list_manager_));
  ProcessPacket(connection_id);
  // 3rd packet. No public reset should be sent;
  ProcessPacket(connection_id);

  // write packet should not be called since we are write blocked but the
  // should be queued.
  QuicConnectionId other_connection_id = TestConnectionId(2);
  AddConnectionId(other_connection_id,
                  QuicTimeWaitListManager::SEND_STATELESS_RESET);
  std::unique_ptr<QuicEncryptedPacket> other_packet(ConstructEncryptedPacket(
      other_connection_id, EmptyQuicConnectionId(), /*packet_number=*/23423));
  EXPECT_CALL(writer_, WritePacket(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(visitor_, OnWriteBlocked(&time_wait_list_manager_));
  ProcessPacket(other_connection_id);
  EXPECT_EQ(2u, time_wait_list_manager_.num_connections());

  // Now expect all the write blocked public reset packets to be sent again.
  writer_is_blocked_ = false;
  EXPECT_CALL(writer_,
              WritePacket(_, _, self_address_.host(), peer_address_, _, _))
      .With(Args<0, 1>(PublicResetPacketEq(connection_id)))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, packet->length())));
  EXPECT_CALL(writer_,
              WritePacket(_, _, self_address_.host(), peer_address_, _, _))
      .With(Args<0, 1>(PublicResetPacketEq(other_connection_id)))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, packet->length())));
  time_wait_list_manager_.OnBlockedWriterCanWrite();
}

TEST_F(QuicTimeWaitListManagerTest, AddConnectionIdTwice) {
  // Add connection_ids such that their expiry time is time_wait_period_.
  AddConnectionId(connection_id_, QuicTimeWaitListManager::DO_NOTHING);
  EXPECT_TRUE(IsConnectionIdInTimeWait(connection_id_));
  const size_t kConnectionCloseLength = 100;
  std::vector<std::unique_ptr<QuicEncryptedPacket>> termination_packets;
  termination_packets.push_back(
      std::unique_ptr<QuicEncryptedPacket>(new QuicEncryptedPacket(
          new char[kConnectionCloseLength], kConnectionCloseLength, true)));
  AddConnectionId(connection_id_,
                  QuicTimeWaitListManager::SEND_TERMINATION_PACKETS,
                  &termination_packets);
  EXPECT_TRUE(IsConnectionIdInTimeWait(connection_id_));
  EXPECT_EQ(1u, time_wait_list_manager_.num_connections());

  EXPECT_CALL(writer_, WritePacket(_, kConnectionCloseLength,
                                   self_address_.host(), peer_address_, _, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));

  ProcessPacket(connection_id_);

  const QuicTime::Delta time_wait_period =
      QuicTimeWaitListManagerPeer::time_wait_period(&time_wait_list_manager_);

  QuicTime::Delta offset = QuicTime::Delta::FromMicroseconds(39);
  clock_.AdvanceTime(offset + time_wait_period);
  // Now set the current time as time_wait_period + offset usecs.
  QuicTime next_alarm_time = clock_.Now() + time_wait_period;
  EXPECT_CALL(alarm_factory_, OnAlarmSet(_, next_alarm_time));

  time_wait_list_manager_.CleanUpOldConnectionIds();
  EXPECT_FALSE(IsConnectionIdInTimeWait(connection_id_));
  EXPECT_EQ(0u, time_wait_list_manager_.num_connections());
}

TEST_F(QuicTimeWaitListManagerTest, AddOverlappingConnectionIdSet) {
  QuicConnectionId cid1 = TestConnectionId(1);
  QuicConnectionId cid2 = TestConnectionId(2);
  QuicConnectionId cid3 = TestConnectionId(3);

  time_wait_list_manager_.AddConnectionIdToTimeWait(
      QuicTimeWaitListManager::SEND_STATELESS_RESET,
      TimeWaitConnectionInfo(false, nullptr, {cid1, cid2}));
  time_wait_list_manager_.AddConnectionIdToTimeWait(
      QuicTimeWaitListManager::SEND_STATELESS_RESET,
      TimeWaitConnectionInfo(false, nullptr, {cid1, cid3}));

  EXPECT_TRUE(time_wait_list_manager_.IsConnectionIdInTimeWait(cid1));
  EXPECT_TRUE(time_wait_list_manager_.IsConnectionIdInTimeWait(cid2));
  EXPECT_TRUE(time_wait_list_manager_.IsConnectionIdInTimeWait(cid3));
  EXPECT_EQ(time_wait_list_manager_.num_connections(), 2u);
}

TEST_F(QuicTimeWaitListManagerTest, ConnectionIdsOrderedByTime) {
  // Simple randomization: the values of connection_ids are randomly swapped.
  // If the container is broken, the test will be 50% flaky.
  const uint64_t conn_id1 = QuicRandom::GetInstance()->RandUint64() % 2;
  const QuicConnectionId connection_id1 = TestConnectionId(conn_id1);
  const QuicConnectionId connection_id2 = TestConnectionId(1 - conn_id1);

  // 1 will hash lower than 2, but we add it later. They should come out in the
  // add order, not hash order.
  AddConnectionId(connection_id1, QuicTimeWaitListManager::DO_NOTHING);
  clock_.AdvanceTime(QuicTime::Delta::FromMicroseconds(10));
  AddConnectionId(connection_id2, QuicTimeWaitListManager::DO_NOTHING);
  EXPECT_EQ(2u, time_wait_list_manager_.num_connections());

  const QuicTime::Delta time_wait_period =
      QuicTimeWaitListManagerPeer::time_wait_period(&time_wait_list_manager_);
  clock_.AdvanceTime(time_wait_period - QuicTime::Delta::FromMicroseconds(9));

  EXPECT_CALL(alarm_factory_, OnAlarmSet(_, _));

  time_wait_list_manager_.CleanUpOldConnectionIds();
  EXPECT_FALSE(IsConnectionIdInTimeWait(connection_id1));
  EXPECT_TRUE(IsConnectionIdInTimeWait(connection_id2));
  EXPECT_EQ(1u, time_wait_list_manager_.num_connections());
}

TEST_F(QuicTimeWaitListManagerTest, MaxConnectionsTest) {
  // Basically, shut off time-based eviction.
  SetQuicFlag(quic_time_wait_list_seconds, 10000000000);
  SetQuicFlag(quic_time_wait_list_max_connections, 5);

  uint64_t current_conn_id = 0;
  const int64_t kMaxConnections =
      GetQuicFlag(quic_time_wait_list_max_connections);
  // Add exactly the maximum number of connections
  for (int64_t i = 0; i < kMaxConnections; ++i) {
    ++current_conn_id;
    QuicConnectionId current_connection_id = TestConnectionId(current_conn_id);
    EXPECT_FALSE(IsConnectionIdInTimeWait(current_connection_id));
    AddConnectionId(current_connection_id, QuicTimeWaitListManager::DO_NOTHING);
    EXPECT_EQ(current_conn_id, time_wait_list_manager_.num_connections());
    EXPECT_TRUE(IsConnectionIdInTimeWait(current_connection_id));
  }

  // Now keep adding.  Since we're already at the max, every new connection-id
  // will evict the oldest one.
  for (int64_t i = 0; i < kMaxConnections; ++i) {
    ++current_conn_id;
    QuicConnectionId current_connection_id = TestConnectionId(current_conn_id);
    const QuicConnectionId id_to_evict =
        TestConnectionId(current_conn_id - kMaxConnections);
    EXPECT_TRUE(IsConnectionIdInTimeWait(id_to_evict));
    EXPECT_FALSE(IsConnectionIdInTimeWait(current_connection_id));
    AddConnectionId(current_connection_id, QuicTimeWaitListManager::DO_NOTHING);
    EXPECT_EQ(static_cast<size_t>(kMaxConnections),
              time_wait_list_manager_.num_connections());
    EXPECT_FALSE(IsConnectionIdInTimeWait(id_to_evict));
    EXPECT_TRUE(IsConnectionIdInTimeWait(current_connection_id));
  }
}

TEST_F(QuicTimeWaitListManagerTest, ZeroMaxConnections) {
  // Basically, shut off time-based eviction.
  SetQuicFlag(quic_time_wait_list_seconds, 10000000000);
  // Keep time wait list empty.
  SetQuicFlag(quic_time_wait_list_max_connections, 0);

  uint64_t current_conn_id = 0;
  // Add exactly the maximum number of connections
  for (int64_t i = 0; i < 10; ++i) {
    ++current_conn_id;
    QuicConnectionId current_connection_id = TestConnectionId(current_conn_id);
    EXPECT_FALSE(IsConnectionIdInTimeWait(current_connection_id));
    AddConnectionId(current_connection_id, QuicTimeWaitListManager::DO_NOTHING);
    // Verify time wait list always has 1 connection.
    EXPECT_EQ(1u, time_wait_list_manager_.num_connections());
    EXPECT_TRUE(IsConnectionIdInTimeWait(current_connection_id));
  }
}

// Regression test for b/116200989.
TEST_F(QuicTimeWaitListManagerTest,
       SendStatelessResetInResponseToShortHeaders) {
  // This test mimics a scenario where an ENCRYPTION_INITIAL connection close is
  // added as termination packet for an IETF connection ID. However, a short
  // header packet is received later.
  const size_t kConnectionCloseLength = 100;
  std::vector<std::unique_ptr<QuicEncryptedPacket>> termination_packets;
  termination_packets.push_back(
      std::unique_ptr<QuicEncryptedPacket>(new QuicEncryptedPacket(
          new char[kConnectionCloseLength], kConnectionCloseLength, true)));
  time_wait_list_manager_.AddConnectionIdToTimeWait(
      QuicTimeWaitListManager::SEND_TERMINATION_PACKETS,
      TimeWaitConnectionInfo(/*ietf_quic=*/true, &termination_packets,
                             {connection_id_}));

  // Termination packet is not encrypted, instead, send stateless reset.
  EXPECT_CALL(writer_,
              WritePacket(_, _, self_address_.host(), peer_address_, _, _))
      .With(Args<0, 1>(PublicResetPacketEq(connection_id_)))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 0)));
  // Processes IETF short header packet.
  time_wait_list_manager_.ProcessPacket(
      self_address_, peer_address_, connection_id_,
      IETF_QUIC_SHORT_HEADER_PACKET, kTestPacketSize);
}

TEST_F(QuicTimeWaitListManagerTest,
       SendConnectionClosePacketsInResponseToShortHeaders) {
  const size_t kConnectionCloseLength = 100;
  std::vector<std::unique_ptr<QuicEncryptedPacket>> termination_packets;
  termination_packets.push_back(
      std::unique_ptr<QuicEncryptedPacket>(new QuicEncryptedPacket(
          new char[kConnectionCloseLength], kConnectionCloseLength, true)));
  // Add a CONNECTION_CLOSE termination packet.
  time_wait_list_manager_.AddConnectionIdToTimeWait(
      QuicTimeWaitListManager::SEND_CONNECTION_CLOSE_PACKETS,
      TimeWaitConnectionInfo(/*ietf_quic=*/true, &termination_packets,
                             {connection_id_}));
  EXPECT_CALL(writer_, WritePacket(_, kConnectionCloseLength,
                                   self_address_.host(), peer_address_, _, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));

  // Processes IETF short header packet.
  time_wait_list_manager_.ProcessPacket(
      self_address_, peer_address_, connection_id_,
      IETF_QUIC_SHORT_HEADER_PACKET, kTestPacketSize);
}

TEST_F(QuicTimeWaitListManagerTest,
       SendConnectionClosePacketsForMultipleConnectionIds) {
  connection_id_ = TestConnectionId(7);
  const size_t kConnectionCloseLength = 100;
  std::vector<std::unique_ptr<QuicEncryptedPacket>> termination_packets;
  termination_packets.push_back(
      std::unique_ptr<QuicEncryptedPacket>(new QuicEncryptedPacket(
          new char[kConnectionCloseLength], kConnectionCloseLength, true)));

  // Add a CONNECTION_CLOSE termination packet.
  std::vector<QuicConnectionId> active_connection_ids{connection_id_,
                                                      TestConnectionId(8)};
  time_wait_list_manager_.AddConnectionIdToTimeWait(
      QuicTimeWaitListManager::SEND_CONNECTION_CLOSE_PACKETS,
      TimeWaitConnectionInfo(/*ietf_quic=*/true, &termination_packets,
                             active_connection_ids, QuicTime::Delta::Zero()));

  EXPECT_CALL(writer_, WritePacket(_, kConnectionCloseLength,
                                   self_address_.host(), peer_address_, _, _))
      .Times(2)
      .WillRepeatedly(Return(WriteResult(WRITE_STATUS_OK, 1)));
  // Processes IETF short header packet.
  for (auto const& cid : active_connection_ids) {
    time_wait_list_manager_.ProcessPacket(self_address_, peer_address_, cid,
                                          IETF_QUIC_SHORT_HEADER_PACKET,
                                          kTestPacketSize);
  }
}

// Regression test for b/184053898.
TEST_F(QuicTimeWaitListManagerTest, DonotCrashOnNullStatelessReset) {
  // Received a packet with length <
  // QuicFramer::GetMinStatelessResetPacketLength(), and this will result in a
  // null stateless reset.
  time_wait_list_manager_.SendPublicReset(
      self_address_, peer_address_, TestConnectionId(1),
      /*ietf_quic=*/true,
      /*received_packet_length=*/
      QuicFramer::GetMinStatelessResetPacketLength() - 1);
}

TEST_F(QuicTimeWaitListManagerTest, SendOrQueueNullPacket) {
  QuicTimeWaitListManagerPeer::SendOrQueuePacket(&time_wait_list_manager_,
                                                 nullptr);
}

TEST_F(QuicTimeWaitListManagerTest, TooManyPendingPackets) {
  SetQuicFlag(quic_time_wait_list_max_pending_packets, 5);
  const size_t kNumOfUnProcessablePackets = 2048;
  EXPECT_CALL(visitor_, OnWriteBlocked(&time_wait_list_manager_))
      .Times(testing::AnyNumber());
  // Write block for the next packets.
  EXPECT_CALL(writer_,
              WritePacket(_, _, self_address_.host(), peer_address_, _, _))
      .With(Args<0, 1>(PublicResetPacketEq(TestConnectionId(1))))
      .WillOnce(DoAll(Assign(&writer_is_blocked_, true),
                      Return(WriteResult(WRITE_STATUS_BLOCKED, EAGAIN))));
  for (size_t i = 0; i < kNumOfUnProcessablePackets; ++i) {
    time_wait_list_manager_.SendPublicReset(
        self_address_, peer_address_, TestConnectionId(1),
        /*ietf_quic=*/true,
        /*received_packet_length=*/
        QuicFramer::GetMinStatelessResetPacketLength() + 1);
  }
  // Verify pending packet queue size is limited.
  EXPECT_EQ(5u, QuicTimeWaitListManagerPeer::PendingPacketsQueueSize(
                    &time_wait_list_manager_));
}

TEST(TimeWaitActionTest, Stringify) {
  EXPECT_EQ(absl::StrCat(QuicTimeWaitListManager::SEND_TERMINATION_PACKETS),
            "SEND_TERMINATION_PACKETS");
  EXPECT_EQ(
      absl::StrCat(QuicTimeWaitListManager::SEND_CONNECTION_CLOSE_PACKETS),
      "SEND_CONNECTION_CLOSE_PACKETS");
  EXPECT_EQ(absl::StrCat(QuicTimeWaitListManager::SEND_STATELESS_RESET),
            "SEND_STATELESS_RESET");
  EXPECT_EQ(absl::StrCat(QuicTimeWaitListManager::DO_NOTHING), "DO_NOTHING");
  EXPECT_EQ(
      absl::StrCat(static_cast<QuicTimeWaitListManager::TimeWaitAction>(0xff)),
      "Unknown TimeWaitAction (255)");
}

}  // namespace
}  // namespace test
}  // namespace quic
