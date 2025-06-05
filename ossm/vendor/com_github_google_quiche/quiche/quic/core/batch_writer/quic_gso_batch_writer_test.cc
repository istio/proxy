// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/batch_writer/quic_gso_batch_writer.h"

#include <sys/socket.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "quiche/quic/core/flow_label.h"
#include "quiche/quic/platform/api/quic_ip_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_mock_syscall_wrapper.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;

namespace quic {
namespace test {
namespace {

size_t PacketLength(const msghdr* msg) {
  size_t length = 0;
  for (size_t i = 0; i < msg->msg_iovlen; ++i) {
    length += msg->msg_iov[i].iov_len;
  }
  return length;
}

uint64_t MillisToNanos(uint64_t milliseconds) { return milliseconds * 1000000; }

class QUICHE_EXPORT TestQuicGsoBatchWriter : public QuicGsoBatchWriter {
 public:
  using QuicGsoBatchWriter::batch_buffer;
  using QuicGsoBatchWriter::buffered_writes;
  using QuicGsoBatchWriter::CanBatch;
  using QuicGsoBatchWriter::CanBatchResult;
  using QuicGsoBatchWriter::GetReleaseTime;
  using QuicGsoBatchWriter::MaxSegments;
  using QuicGsoBatchWriter::QuicGsoBatchWriter;
  using QuicGsoBatchWriter::ReleaseTime;

  static std::unique_ptr<TestQuicGsoBatchWriter>
  NewInstanceWithReleaseTimeSupport() {
    return std::unique_ptr<TestQuicGsoBatchWriter>(new TestQuicGsoBatchWriter(
        std::make_unique<QuicBatchWriterBuffer>(),
        /*fd=*/-1, CLOCK_MONOTONIC, ReleaseTimeForceEnabler()));
  }

  uint64_t NowInNanosForReleaseTime() const override {
    return MillisToNanos(forced_release_time_ms_);
  }

  void ForceReleaseTimeMs(uint64_t forced_release_time_ms) {
    forced_release_time_ms_ = forced_release_time_ms;
  }

 private:
  uint64_t forced_release_time_ms_ = 1;
};

// TestBufferedWrite is a copy-constructible BufferedWrite.
struct QUICHE_EXPORT TestBufferedWrite : public BufferedWrite {
  using BufferedWrite::BufferedWrite;
  TestBufferedWrite(const TestBufferedWrite& other)
      : BufferedWrite(other.buffer, other.buf_len, other.self_address,
                      other.peer_address,
                      other.options ? other.options->Clone()
                                    : std::unique_ptr<PerPacketOptions>(),
                      QuicPacketWriterParams(), other.release_time) {}
};

// Pointed to by all instances of |BatchCriteriaTestData|. Content not used.
static char unused_packet_buffer[kMaxOutgoingPacketSize];

struct QUICHE_EXPORT BatchCriteriaTestData {
  BatchCriteriaTestData(size_t buf_len, const QuicIpAddress& self_address,
                        const QuicSocketAddress& peer_address,
                        uint64_t release_time, bool can_batch, bool must_flush)
      : buffered_write(unused_packet_buffer, buf_len, self_address,
                       peer_address, std::unique_ptr<PerPacketOptions>(),
                       QuicPacketWriterParams(), release_time),
        can_batch(can_batch),
        must_flush(must_flush) {}

  TestBufferedWrite buffered_write;
  // Expected value of CanBatchResult.can_batch when batching |buffered_write|.
  bool can_batch;
  // Expected value of CanBatchResult.must_flush when batching |buffered_write|.
  bool must_flush;
};

std::vector<BatchCriteriaTestData> BatchCriteriaTestData_SizeDecrease() {
  const QuicIpAddress self_addr;
  const QuicSocketAddress peer_addr;
  std::vector<BatchCriteriaTestData> test_data_table = {
      // clang-format off
  // buf_len   self_addr   peer_addr   t_rel   can_batch       must_flush
    {1350,     self_addr,  peer_addr,  0,      true,           false},
    {1350,     self_addr,  peer_addr,  0,      true,           false},
    {1350,     self_addr,  peer_addr,  0,      true,           false},
    {39,       self_addr,  peer_addr,  0,      true,           true},
    {39,       self_addr,  peer_addr,  0,      false,          true},
    {1350,     self_addr,  peer_addr,  0,      false,          true},
      // clang-format on
  };
  return test_data_table;
}

std::vector<BatchCriteriaTestData> BatchCriteriaTestData_SizeIncrease() {
  const QuicIpAddress self_addr;
  const QuicSocketAddress peer_addr;
  std::vector<BatchCriteriaTestData> test_data_table = {
      // clang-format off
  // buf_len   self_addr   peer_addr   t_rel   can_batch       must_flush
    {1350,     self_addr,  peer_addr,  0,      true,           false},
    {1350,     self_addr,  peer_addr,  0,      true,           false},
    {1350,     self_addr,  peer_addr,  0,      true,           false},
    {1351,     self_addr,  peer_addr,  0,      false,          true},
      // clang-format on
  };
  return test_data_table;
}

std::vector<BatchCriteriaTestData> BatchCriteriaTestData_AddressChange() {
  const QuicIpAddress self_addr1 = QuicIpAddress::Loopback4();
  const QuicIpAddress self_addr2 = QuicIpAddress::Loopback6();
  const QuicSocketAddress peer_addr1(self_addr1, 666);
  const QuicSocketAddress peer_addr2(self_addr1, 777);
  const QuicSocketAddress peer_addr3(self_addr2, 666);
  const QuicSocketAddress peer_addr4(self_addr2, 777);
  std::vector<BatchCriteriaTestData> test_data_table = {
      // clang-format off
  // buf_len   self_addr   peer_addr    t_rel  can_batch       must_flush
    {1350,     self_addr1, peer_addr1,  0,     true,           false},
    {1350,     self_addr1, peer_addr1,  0,     true,           false},
    {1350,     self_addr1, peer_addr1,  0,     true,           false},
    {1350,     self_addr2, peer_addr1,  0,     false,          true},
    {1350,     self_addr1, peer_addr2,  0,     false,          true},
    {1350,     self_addr1, peer_addr3,  0,     false,          true},
    {1350,     self_addr1, peer_addr4,  0,     false,          true},
    {1350,     self_addr1, peer_addr4,  0,     false,          true},
      // clang-format on
  };
  return test_data_table;
}

std::vector<BatchCriteriaTestData> BatchCriteriaTestData_ReleaseTime1() {
  const QuicIpAddress self_addr;
  const QuicSocketAddress peer_addr;
  std::vector<BatchCriteriaTestData> test_data_table = {
      // clang-format off
  // buf_len   self_addr   peer_addr   t_rel   can_batch       must_flush
    {1350,     self_addr,  peer_addr,  5,      true,           false},
    {1350,     self_addr,  peer_addr,  5,      true,           false},
    {1350,     self_addr,  peer_addr,  5,      true,           false},
    {1350,     self_addr,  peer_addr,  9,      false,          true},
      // clang-format on
  };
  return test_data_table;
}

std::vector<BatchCriteriaTestData> BatchCriteriaTestData_ReleaseTime2() {
  const QuicIpAddress self_addr;
  const QuicSocketAddress peer_addr;
  std::vector<BatchCriteriaTestData> test_data_table = {
      // clang-format off
  // buf_len   self_addr   peer_addr   t_rel   can_batch       must_flush
    {1350,     self_addr,  peer_addr,  0,      true,           false},
    {1350,     self_addr,  peer_addr,  0,      true,           false},
    {1350,     self_addr,  peer_addr,  0,      true,           false},
    {1350,     self_addr,  peer_addr,  9,      false,          true},
      // clang-format on
  };
  return test_data_table;
}

std::vector<BatchCriteriaTestData> BatchCriteriaTestData_MaxSegments(
    size_t gso_size) {
  const QuicIpAddress self_addr;
  const QuicSocketAddress peer_addr;
  std::vector<BatchCriteriaTestData> test_data_table;
  size_t max_segments = TestQuicGsoBatchWriter::MaxSegments(gso_size);
  for (size_t i = 0; i < max_segments; ++i) {
    bool is_last_in_batch = (i + 1 == max_segments);
    test_data_table.push_back({gso_size, self_addr, peer_addr,
                               /*release_time=*/0, true, is_last_in_batch});
  }
  test_data_table.push_back(
      {gso_size, self_addr, peer_addr, /*release_time=*/0, false, true});
  return test_data_table;
}

class QuicGsoBatchWriterTest : public QuicTest {
 protected:
  WriteResult WritePacket(QuicGsoBatchWriter* writer, size_t packet_size) {
    return writer->WritePacket(&packet_buffer_[0], packet_size, self_address_,
                               peer_address_, nullptr,
                               QuicPacketWriterParams());
  }

  WriteResult WritePacketWithParams(QuicGsoBatchWriter* writer,
                                    QuicPacketWriterParams& params) {
    return writer->WritePacket(&packet_buffer_[0], 1350, self_address_,
                               peer_address_, nullptr, params);
  }

  QuicIpAddress self_address_ = QuicIpAddress::Any4();
  QuicSocketAddress peer_address_{QuicIpAddress::Any4(), 443};
  char packet_buffer_[1500];
  StrictMock<MockQuicSyscallWrapper> mock_syscalls_;
  ScopedGlobalSyscallWrapperOverride syscall_override_{&mock_syscalls_};
};

TEST_F(QuicGsoBatchWriterTest, BatchCriteria) {
  std::unique_ptr<TestQuicGsoBatchWriter> writer;

  std::vector<std::vector<BatchCriteriaTestData>> test_data_tables;
  test_data_tables.emplace_back(BatchCriteriaTestData_SizeDecrease());
  test_data_tables.emplace_back(BatchCriteriaTestData_SizeIncrease());
  test_data_tables.emplace_back(BatchCriteriaTestData_AddressChange());
  test_data_tables.emplace_back(BatchCriteriaTestData_ReleaseTime1());
  test_data_tables.emplace_back(BatchCriteriaTestData_ReleaseTime2());
  test_data_tables.emplace_back(BatchCriteriaTestData_MaxSegments(1));
  test_data_tables.emplace_back(BatchCriteriaTestData_MaxSegments(2));
  test_data_tables.emplace_back(BatchCriteriaTestData_MaxSegments(1350));

  for (size_t i = 0; i < test_data_tables.size(); ++i) {
    writer = TestQuicGsoBatchWriter::NewInstanceWithReleaseTimeSupport();

    const auto& test_data_table = test_data_tables[i];
    for (size_t j = 0; j < test_data_table.size(); ++j) {
      const BatchCriteriaTestData& test_data = test_data_table[j];
      SCOPED_TRACE(testing::Message() << "i=" << i << ", j=" << j);
      QuicPacketWriterParams params;
      params.release_time_delay = QuicTime::Delta::FromMicroseconds(
          test_data.buffered_write.release_time);
      TestQuicGsoBatchWriter::CanBatchResult result = writer->CanBatch(
          test_data.buffered_write.buffer, test_data.buffered_write.buf_len,
          test_data.buffered_write.self_address,
          test_data.buffered_write.peer_address, nullptr, params,
          test_data.buffered_write.release_time);

      ASSERT_EQ(test_data.can_batch, result.can_batch);
      ASSERT_EQ(test_data.must_flush, result.must_flush);

      if (result.can_batch) {
        ASSERT_TRUE(writer->batch_buffer()
                        .PushBufferedWrite(
                            test_data.buffered_write.buffer,
                            test_data.buffered_write.buf_len,
                            test_data.buffered_write.self_address,
                            test_data.buffered_write.peer_address, nullptr,
                            params, test_data.buffered_write.release_time)
                        .succeeded);
      }
    }
  }
}

TEST_F(QuicGsoBatchWriterTest, WriteSuccess) {
  TestQuicGsoBatchWriter writer(/*fd=*/-1);

  ASSERT_EQ(WriteResult(WRITE_STATUS_OK, 0), WritePacket(&writer, 1000));

  EXPECT_CALL(mock_syscalls_, Sendmsg(_, _, _))
      .WillOnce(Invoke([](int /*sockfd*/, const msghdr* msg, int /*flags*/) {
        EXPECT_EQ(1100u, PacketLength(msg));
        return 1100;
      }));
  ASSERT_EQ(WriteResult(WRITE_STATUS_OK, 1100), WritePacket(&writer, 100));
  ASSERT_EQ(0u, writer.batch_buffer().SizeInUse());
  ASSERT_EQ(0u, writer.buffered_writes().size());
}

TEST_F(QuicGsoBatchWriterTest, WriteBlockDataNotBuffered) {
  TestQuicGsoBatchWriter writer(/*fd=*/-1);

  ASSERT_EQ(WriteResult(WRITE_STATUS_OK, 0), WritePacket(&writer, 100));
  ASSERT_EQ(WriteResult(WRITE_STATUS_OK, 0), WritePacket(&writer, 100));

  EXPECT_CALL(mock_syscalls_, Sendmsg(_, _, _))
      .WillOnce(Invoke([](int /*sockfd*/, const msghdr* msg, int /*flags*/) {
        EXPECT_EQ(200u, PacketLength(msg));
        errno = EWOULDBLOCK;
        return -1;
      }));
  ASSERT_EQ(WriteResult(WRITE_STATUS_BLOCKED, EWOULDBLOCK),
            WritePacket(&writer, 150));
  ASSERT_EQ(200u, writer.batch_buffer().SizeInUse());
  ASSERT_EQ(2u, writer.buffered_writes().size());
}

TEST_F(QuicGsoBatchWriterTest, WriteBlockDataBuffered) {
  TestQuicGsoBatchWriter writer(/*fd=*/-1);

  ASSERT_EQ(WriteResult(WRITE_STATUS_OK, 0), WritePacket(&writer, 100));
  ASSERT_EQ(WriteResult(WRITE_STATUS_OK, 0), WritePacket(&writer, 100));

  EXPECT_CALL(mock_syscalls_, Sendmsg(_, _, _))
      .WillOnce(Invoke([](int /*sockfd*/, const msghdr* msg, int /*flags*/) {
        EXPECT_EQ(250u, PacketLength(msg));
        errno = EWOULDBLOCK;
        return -1;
      }));
  ASSERT_EQ(WriteResult(WRITE_STATUS_BLOCKED_DATA_BUFFERED, EWOULDBLOCK),
            WritePacket(&writer, 50));

  EXPECT_TRUE(writer.IsWriteBlocked());

  ASSERT_EQ(250u, writer.batch_buffer().SizeInUse());
  ASSERT_EQ(3u, writer.buffered_writes().size());
}

TEST_F(QuicGsoBatchWriterTest, WriteErrorWithoutDataBuffered) {
  TestQuicGsoBatchWriter writer(/*fd=*/-1);

  ASSERT_EQ(WriteResult(WRITE_STATUS_OK, 0), WritePacket(&writer, 100));
  ASSERT_EQ(WriteResult(WRITE_STATUS_OK, 0), WritePacket(&writer, 100));

  EXPECT_CALL(mock_syscalls_, Sendmsg(_, _, _))
      .WillOnce(Invoke([](int /*sockfd*/, const msghdr* msg, int /*flags*/) {
        EXPECT_EQ(200u, PacketLength(msg));
        errno = EPERM;
        return -1;
      }));
  WriteResult error_result = WritePacket(&writer, 150);
  ASSERT_EQ(WriteResult(WRITE_STATUS_ERROR, EPERM), error_result);

  ASSERT_EQ(3u, error_result.dropped_packets);
  ASSERT_EQ(0u, writer.batch_buffer().SizeInUse());
  ASSERT_EQ(0u, writer.buffered_writes().size());
}

TEST_F(QuicGsoBatchWriterTest, WriteErrorAfterDataBuffered) {
  TestQuicGsoBatchWriter writer(/*fd=*/-1);

  ASSERT_EQ(WriteResult(WRITE_STATUS_OK, 0), WritePacket(&writer, 100));
  ASSERT_EQ(WriteResult(WRITE_STATUS_OK, 0), WritePacket(&writer, 100));

  EXPECT_CALL(mock_syscalls_, Sendmsg(_, _, _))
      .WillOnce(Invoke([](int /*sockfd*/, const msghdr* msg, int /*flags*/) {
        EXPECT_EQ(250u, PacketLength(msg));
        errno = EPERM;
        return -1;
      }));
  WriteResult error_result = WritePacket(&writer, 50);
  ASSERT_EQ(WriteResult(WRITE_STATUS_ERROR, EPERM), error_result);

  ASSERT_EQ(3u, error_result.dropped_packets);
  ASSERT_EQ(0u, writer.batch_buffer().SizeInUse());
  ASSERT_EQ(0u, writer.buffered_writes().size());
}

TEST_F(QuicGsoBatchWriterTest, FlushError) {
  TestQuicGsoBatchWriter writer(/*fd=*/-1);

  ASSERT_EQ(WriteResult(WRITE_STATUS_OK, 0), WritePacket(&writer, 100));
  ASSERT_EQ(WriteResult(WRITE_STATUS_OK, 0), WritePacket(&writer, 100));

  EXPECT_CALL(mock_syscalls_, Sendmsg(_, _, _))
      .WillOnce(Invoke([](int /*sockfd*/, const msghdr* msg, int /*flags*/) {
        EXPECT_EQ(200u, PacketLength(msg));
        errno = EINVAL;
        return -1;
      }));
  WriteResult error_result = writer.Flush();
  ASSERT_EQ(WriteResult(WRITE_STATUS_ERROR, EINVAL), error_result);

  ASSERT_EQ(2u, error_result.dropped_packets);
  ASSERT_EQ(0u, writer.batch_buffer().SizeInUse());
  ASSERT_EQ(0u, writer.buffered_writes().size());
}

TEST_F(QuicGsoBatchWriterTest, ReleaseTime) {
  const WriteResult write_buffered(WRITE_STATUS_OK, 0);

  auto writer = TestQuicGsoBatchWriter::NewInstanceWithReleaseTimeSupport();

  QuicPacketWriterParams params;
  EXPECT_TRUE(params.release_time_delay.IsZero());
  EXPECT_FALSE(params.allow_burst);
  EXPECT_EQ(MillisToNanos(1),
            writer->GetReleaseTime(params).actual_release_time);

  // The 1st packet has no delay.
  WriteResult result = WritePacketWithParams(writer.get(), params);
  ASSERT_EQ(write_buffered, result);
  EXPECT_EQ(MillisToNanos(1), writer->buffered_writes().back().release_time);
  EXPECT_EQ(result.send_time_offset, QuicTime::Delta::Zero());

  // The 2nd packet has some delay, but allows burst.
  params.release_time_delay = QuicTime::Delta::FromMilliseconds(3);
  params.allow_burst = true;
  result = WritePacketWithParams(writer.get(), params);
  ASSERT_EQ(write_buffered, result);
  EXPECT_EQ(MillisToNanos(1), writer->buffered_writes().back().release_time);
  EXPECT_EQ(result.send_time_offset, QuicTime::Delta::FromMilliseconds(-3));

  // The 3rd packet has more delay and does not allow burst.
  // The first 2 packets are flushed due to different release time.
  EXPECT_CALL(mock_syscalls_, Sendmsg(_, _, _))
      .WillOnce(Invoke([](int /*sockfd*/, const msghdr* msg, int /*flags*/) {
        EXPECT_EQ(2700u, PacketLength(msg));
        errno = 0;
        return 0;
      }));
  params.release_time_delay = QuicTime::Delta::FromMilliseconds(5);
  params.allow_burst = false;
  result = WritePacketWithParams(writer.get(), params);
  ASSERT_EQ(WriteResult(WRITE_STATUS_OK, 2700), result);
  EXPECT_EQ(MillisToNanos(6), writer->buffered_writes().back().release_time);
  EXPECT_EQ(result.send_time_offset, QuicTime::Delta::Zero());

  // The 4th packet has same delay, but allows burst.
  params.allow_burst = true;
  result = WritePacketWithParams(writer.get(), params);
  ASSERT_EQ(write_buffered, result);
  EXPECT_EQ(MillisToNanos(6), writer->buffered_writes().back().release_time);
  EXPECT_EQ(result.send_time_offset, QuicTime::Delta::Zero());

  // The 5th packet has same delay, allows burst, but is shorter.
  // Packets 3,4 and 5 are flushed.
  EXPECT_CALL(mock_syscalls_, Sendmsg(_, _, _))
      .WillOnce(Invoke([](int /*sockfd*/, const msghdr* msg, int /*flags*/) {
        EXPECT_EQ(3000u, PacketLength(msg));
        errno = 0;
        return 0;
      }));
  params.allow_burst = true;
  EXPECT_EQ(MillisToNanos(6),
            writer->GetReleaseTime(params).actual_release_time);
  ASSERT_EQ(WriteResult(WRITE_STATUS_OK, 3000),
            writer->WritePacket(&packet_buffer_[0], 300, self_address_,
                                peer_address_, nullptr, params));
  EXPECT_TRUE(writer->buffered_writes().empty());

  // Pretend 1ms has elapsed and the 6th packet has 1ms less delay. In other
  // words, the release time should still be the same as packets 3-5.
  writer->ForceReleaseTimeMs(2);
  params.release_time_delay = QuicTime::Delta::FromMilliseconds(4);
  result = WritePacketWithParams(writer.get(), params);
  ASSERT_EQ(write_buffered, result);
  EXPECT_EQ(MillisToNanos(6), writer->buffered_writes().back().release_time);
  EXPECT_EQ(result.send_time_offset, QuicTime::Delta::Zero());
}

TEST_F(QuicGsoBatchWriterTest, EcnCodepoint) {
  const WriteResult write_buffered(WRITE_STATUS_OK, 0);

  auto writer = TestQuicGsoBatchWriter::NewInstanceWithReleaseTimeSupport();

  QuicPacketWriterParams params;
  EXPECT_TRUE(params.release_time_delay.IsZero());
  EXPECT_FALSE(params.allow_burst);
  params.ecn_codepoint = ECN_ECT0;

  // The 1st packet has no delay.
  WriteResult result = WritePacketWithParams(writer.get(), params);
  ASSERT_EQ(write_buffered, result);
  EXPECT_EQ(MillisToNanos(1), writer->buffered_writes().back().release_time);
  EXPECT_EQ(result.send_time_offset, QuicTime::Delta::Zero());

  // The 2nd packet should be buffered.
  params.allow_burst = true;
  result = WritePacketWithParams(writer.get(), params);
  ASSERT_EQ(write_buffered, result);

  // The 3rd packet changes the ECN codepoint.
  // The first 2 packets are flushed due to different codepoint.
  params.ecn_codepoint = ECN_ECT1;
  EXPECT_CALL(mock_syscalls_, Sendmsg(_, _, _))
      .WillOnce(Invoke([](int /*sockfd*/, const msghdr* msg, int /*flags*/) {
        const int kEct0 = 0x02;
        EXPECT_EQ(2700u, PacketLength(msg));
        msghdr mutable_msg;
        memcpy(&mutable_msg, msg, sizeof(*msg));
        for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&mutable_msg); cmsg != NULL;
             cmsg = CMSG_NXTHDR(&mutable_msg, cmsg)) {
          if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_TOS) {
            EXPECT_EQ(*reinterpret_cast<int*> CMSG_DATA(cmsg), kEct0);
            break;
          }
        }
        errno = 0;
        return 0;
      }));
  result = WritePacketWithParams(writer.get(), params);
  ASSERT_EQ(WriteResult(WRITE_STATUS_OK, 2700), result);
}

TEST_F(QuicGsoBatchWriterTest, EcnCodepointIPv6) {
  const WriteResult write_buffered(WRITE_STATUS_OK, 0);

  self_address_ = QuicIpAddress::Any6();
  peer_address_ = QuicSocketAddress(QuicIpAddress::Any6(), 443);
  auto writer = TestQuicGsoBatchWriter::NewInstanceWithReleaseTimeSupport();

  QuicPacketWriterParams params;
  EXPECT_TRUE(params.release_time_delay.IsZero());
  EXPECT_FALSE(params.allow_burst);
  params.ecn_codepoint = ECN_ECT0;

  // The 1st packet has no delay.
  WriteResult result = WritePacketWithParams(writer.get(), params);
  ASSERT_EQ(write_buffered, result);
  EXPECT_EQ(MillisToNanos(1), writer->buffered_writes().back().release_time);
  EXPECT_EQ(result.send_time_offset, QuicTime::Delta::Zero());

  // The 2nd packet should be buffered.
  params.allow_burst = true;
  result = WritePacketWithParams(writer.get(), params);
  ASSERT_EQ(write_buffered, result);

  // The 3rd packet changes the ECN codepoint.
  // The first 2 packets are flushed due to different codepoint.
  params.ecn_codepoint = ECN_ECT1;
  EXPECT_CALL(mock_syscalls_, Sendmsg(_, _, _))
      .WillOnce(Invoke([](int /*sockfd*/, const msghdr* msg, int /*flags*/) {
        const int kEct0 = 0x02;
        EXPECT_EQ(2700u, PacketLength(msg));
        msghdr mutable_msg;
        memcpy(&mutable_msg, msg, sizeof(*msg));
        for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&mutable_msg); cmsg != NULL;
             cmsg = CMSG_NXTHDR(&mutable_msg, cmsg)) {
          if (cmsg->cmsg_level == IPPROTO_IPV6 &&
              cmsg->cmsg_type == IPV6_TCLASS) {
            EXPECT_EQ(*reinterpret_cast<int*> CMSG_DATA(cmsg), kEct0);
            break;
          }
        }
        errno = 0;
        return 0;
      }));
  result = WritePacketWithParams(writer.get(), params);
  ASSERT_EQ(WriteResult(WRITE_STATUS_OK, 2700), result);
}

TEST_F(QuicGsoBatchWriterTest, FlowLabelIPv6) {
  const WriteResult write_buffered(WRITE_STATUS_OK, 0);

  self_address_ = QuicIpAddress::Any6();
  peer_address_ = QuicSocketAddress(QuicIpAddress::Any6(), 443);
  auto writer = TestQuicGsoBatchWriter::NewInstanceWithReleaseTimeSupport();

  QuicPacketWriterParams params;
  EXPECT_TRUE(params.release_time_delay.IsZero());
  EXPECT_FALSE(params.allow_burst);

  for (uint32_t i = 1; i < 5; ++i) {
    // Generate flow label which are on both side of zero to test
    // coverage when the in-memory label is larger than 20 bits.
    params.flow_label = i - 2;
    WriteResult result = WritePacketWithParams(writer.get(), params);
    ASSERT_EQ(write_buffered, result);

    EXPECT_CALL(mock_syscalls_, Sendmsg(_, _, _))
        .WillOnce(
            Invoke([&params](int /*sockfd*/, const msghdr* msg, int /*flags*/) {
              EXPECT_EQ(1350u, PacketLength(msg));
              msghdr mutable_msg;
              memcpy(&mutable_msg, msg, sizeof(*msg));
              bool found_flow_label = false;
              for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&mutable_msg);
                   cmsg != NULL; cmsg = CMSG_NXTHDR(&mutable_msg, cmsg)) {
                if (cmsg->cmsg_level == IPPROTO_IPV6 &&
                    cmsg->cmsg_type == IPV6_FLOWINFO) {
                  found_flow_label = true;
                  uint32_t cmsg_flow_label =
                      ntohl(*reinterpret_cast<uint32_t*> CMSG_DATA(cmsg));
                  EXPECT_EQ(params.flow_label & 0xFFFFF, cmsg_flow_label);
                  break;
                }
              }
              // As long as the flow label is not zero, it should be present.
              EXPECT_EQ(params.flow_label != 0, found_flow_label);
              errno = 0;
              return 0;
            }));
    WriteResult error_result = writer->Flush();
    ASSERT_EQ(WriteResult(WRITE_STATUS_OK, 1350), error_result);
  }
}

}  // namespace
}  // namespace test
}  // namespace quic
