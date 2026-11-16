// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_packet_reader.h"

#include <cstddef>
#include <string>
#include <vector>

#include "absl/base/optimization.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_packets.h"
#include "quiche/quic/core/quic_process_packet_interface.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_udp_socket.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/platform/api/quic_test_loopback.h"
#include "quiche/quic/test_tools/mock_clock.h"

#if defined(__linux__)
#include <netinet/udp.h>
#include <sys/socket.h>

#ifndef UDP_SEGMENT
#define UDP_SEGMENT 103
#endif
#endif  // defined(__linux__)

namespace quic {
namespace test {
namespace {

using ::testing::ElementsAre;

constexpr int kSocketBufferSize = 1024 * 1024;
// How long to wait for a socket to become readable, and how many times.
constexpr QuicTime::Delta kReadTimeout = QuicTime::Delta::FromMilliseconds(100);
constexpr int kMaxReadAttempts = 50;

// Records the packets that a QuicPacketReader dispatches.
class RecordingProcessor : public ProcessPacketInterface {
 public:
  struct Packet {
    QuicSocketAddress self_address;
    QuicSocketAddress peer_address;
    std::string payload;
  };

  void ProcessPacket(const QuicSocketAddress& self_address,
                     const QuicSocketAddress& peer_address,
                     const QuicReceivedPacket& packet) override {
    packets_.push_back({self_address, peer_address,
                        std::string(packet.data(), packet.length())});
  }

  const std::vector<Packet>& packets() const { return packets_; }

  // Returns the payloads of the packets, in the order they were dispatched.
  std::vector<std::string> payloads() const {
    std::vector<std::string> payloads;
    for (const Packet& packet : packets_) {
      payloads.push_back(packet.payload);
    }
    return payloads;
  }

 private:
  std::vector<Packet> packets_;
};

// Sends datagrams from a client socket to a server socket over loopback.
class QuicPacketReaderTest : public QuicTest {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        CreateLoopbackSocket(&reader_fd_, &server_address_));
    ASSERT_NO_FATAL_FAILURE(
        CreateLoopbackSocket(&writer_fd_, &client_address_));
  }

  void TearDown() override {
    socket_api_.Destroy(writer_fd_);
    socket_api_.Destroy(reader_fd_);
  }

  // Creates a UDP socket bound to an ephemeral loopback port.
  void CreateLoopbackSocket(QuicUdpSocketFd* fd, QuicSocketAddress* address) {
    const QuicSocketAddress loopback(TestLoopback(), /*port=*/0);
    *fd = socket_api_.Create(loopback.host().AddressFamilyToInt(),
                             kSocketBufferSize, kSocketBufferSize);
    ASSERT_NE(*fd, kQuicInvalidSocketFd);
    ASSERT_TRUE(socket_api_.Bind(*fd, loopback));
    ASSERT_EQ(address->FromSocket(*fd), 0);
  }

  // Sends |data| from the client socket to the server socket in one write.
  bool Send(absl::string_view data) {
    QuicUdpPacketInfo packet_info;
    packet_info.SetPeerAddress(server_address_);
    return socket_api_
               .WritePacket(writer_fd_, data.data(), data.length(), packet_info)
               .status == WRITE_STATUS_OK;
  }

  // Lets |reader| read from the server socket until |processor_| has received
  // at least |num_packets| packets, or the reads time out.
  void ReadPackets(QuicPacketReader& reader, size_t num_packets) {
    for (int attempt = 0; attempt < kMaxReadAttempts &&
                          processor_.packets().size() < num_packets;
         ++attempt) {
      if (socket_api_.WaitUntilReadable(reader_fd_, kReadTimeout)) {
        reader.ReadAndDispatchPackets(reader_fd_, server_address_.port(),
                                      clock_, &processor_,
                                      /*packets_dropped=*/nullptr);
      }
    }
  }

  // Reads one datagram, or one GRO train, from the server socket without a
  // QuicPacketReader. The result points into |read_packet_buffer_|.
  QuicUdpSocketApi::ReadPacketResult ReadDatagram() {
    QuicUdpSocketApi::ReadPacketResult result;
    result.packet_buffer = {read_packet_buffer_.data(),
                            read_packet_buffer_.size()};
    result.control_buffer = {read_control_buffer_,
                             sizeof(read_control_buffer_)};
    for (int attempt = 0; attempt < kMaxReadAttempts && !result.ok; ++attempt) {
      if (socket_api_.WaitUntilReadable(reader_fd_, kReadTimeout)) {
        result.Reset(read_packet_buffer_.size());
        socket_api_.ReadPacket(
            reader_fd_,
            QuicUdpPacketInfoBitMask({QuicUdpPacketInfoBit::IS_GRO}), &result);
      }
    }
    return result;
  }

  // Expects every packet to have been sent from the client to the server.
  void ExpectPacketAddresses() {
    for (const RecordingProcessor::Packet& packet : processor_.packets()) {
      EXPECT_EQ(packet.self_address, server_address_);
      EXPECT_EQ(packet.peer_address, client_address_);
    }
  }

  QuicUdpSocketApi socket_api_;
  // The socket which the QuicPacketReader reads from.
  QuicUdpSocketFd reader_fd_ = kQuicInvalidSocketFd;
  // The socket which sends datagrams to the receiver socket.
  QuicUdpSocketFd writer_fd_ = kQuicInvalidSocketFd;
  QuicSocketAddress server_address_;
  QuicSocketAddress client_address_;
  MockClock clock_;
  RecordingProcessor processor_;
  std::string read_packet_buffer_ = std::string(kGroPacketBufferSize, '\0');
  ABSL_CACHELINE_ALIGNED char
      read_control_buffer_[kDefaultUdpPacketControlBufferSize];
};

TEST_F(QuicPacketReaderTest, DispatchesEachDatagram) {
  QuicPacketReader reader;
  EXPECT_FALSE(reader.gro_enabled());

  ASSERT_TRUE(Send("a"));
  ASSERT_TRUE(Send("bb"));
  ASSERT_TRUE(Send("ccc"));
  ReadPackets(reader, 3);

  EXPECT_THAT(processor_.payloads(), ElementsAre("a", "bb", "ccc"));
  ExpectPacketAddresses();
}

// A GRO reader dispatches datagrams that are not GRO trains as they are, for
// example on a socket without UDP GRO enabled.
TEST_F(QuicPacketReaderTest, GroReaderDispatchesEachDatagram) {
  QuicPacketReader reader({.enable_gro = true});
  EXPECT_TRUE(reader.gro_enabled());

  ASSERT_TRUE(Send("a"));
  ASSERT_TRUE(Send("bb"));
  ASSERT_TRUE(Send("ccc"));
  ReadPackets(reader, 3);

  EXPECT_THAT(processor_.payloads(), ElementsAre("a", "bb", "ccc"));
  ExpectPacketAddresses();
}

#if defined(__linux__)
TEST_F(QuicPacketReaderTest, GroReaderDispatchesEachPacketInGroTrain) {
  if (!socket_api_.EnableUdpGro(reader_fd_)) {
    GTEST_SKIP() << "UDP GRO is not supported.";
  }

  // Have the kernel split each write into datagrams of up to |segment_size|
  // bytes (UDP GSO). Over loopback, a socket with UDP GRO enabled receives the
  // datagrams of a write as one GRO train.
  const int segment_size = 1000;
  if (setsockopt(writer_fd_, SOL_UDP, UDP_SEGMENT, &segment_size,
                 sizeof(segment_size)) != 0) {
    GTEST_SKIP() << "UDP GSO is not supported.";
  }

  // Three full size packets and a smaller trailing packet.
  const std::vector<std::string> packets = {
      std::string(segment_size, 'a'), std::string(segment_size, 'b'),
      std::string(segment_size, 'c'), std::string(segment_size / 2, 'd')};
  std::string train;
  for (const std::string& packet : packets) {
    train += packet;
  }

  // Check that the kernel delivers a write as one GRO train, so that the reader
  // gets a train to split below.
  ASSERT_TRUE(Send(train));
  QuicUdpSocketApi::ReadPacketResult result = ReadDatagram();
  ASSERT_TRUE(result.ok);
  if (!result.packet_info.HasValue(QuicUdpPacketInfoBit::IS_GRO)) {
    GTEST_SKIP() << "The kernel did not coalesce the datagrams.";
  }
  EXPECT_EQ(result.packet_info.gso_size(), static_cast<size_t>(segment_size));
  EXPECT_EQ(result.packet_buffer.buffer_len, train.size());

  // Now read the train with the GRO-enabled reader.
  QuicPacketReader reader({.enable_gro = true});
  ASSERT_TRUE(Send(train));
  ReadPackets(reader, packets.size());

  EXPECT_EQ(processor_.payloads(), packets);
  ExpectPacketAddresses();
}
#endif  // defined(__linux__)

}  // namespace
}  // namespace test
}  // namespace quic
