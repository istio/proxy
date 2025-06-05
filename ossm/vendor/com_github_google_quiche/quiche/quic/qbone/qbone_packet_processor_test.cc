// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/qbone/qbone_packet_processor.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/qbone/qbone_packet_processor_test_tools.h"
#include "quiche/common/quiche_text_utils.h"

namespace quic::test {
namespace {

using Direction = QbonePacketProcessor::Direction;
using ProcessingResult = QbonePacketProcessor::ProcessingResult;
using OutputInterface = QbonePacketProcessor::OutputInterface;
using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

// clang-format off
static const char kReferenceClientPacketData[] = {
    // IPv6 with zero TOS and flow label.
    0x60, 0x00, 0x00, 0x00,
    // Payload size is 8 bytes.
    0x00, 0x08,
    // Next header is UDP
    17,
    // TTL is 50.
    50,
    // IP address of the sender is fd00:0:0:1::1
    0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // IP address of the receiver is fd00:0:0:5::1
    0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // Source port 12345
    0x30, 0x39,
    // Destination port 443
    0x01, 0xbb,
    // UDP content length is zero
    0x00, 0x00,
    // Checksum is not actually checked in any of the tests, so we leave it as
    // zero
    0x00, 0x00,
};

static const char kReferenceClientPacketDataAF4[] = {
    // IPv6 with 0x80 TOS and zero flow label.
    0x68, 0x00, 0x00, 0x00,
    // Payload size is 8 bytes.
    0x00, 0x08,
    // Next header is UDP
    17,
    // TTL is 50.
    50,
    // IP address of the sender is fd00:0:0:1::1
    0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // IP address of the receiver is fd00:0:0:5::1
    0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // Source port 12345
    0x30, 0x39,
    // Destination port 443
    0x01, 0xbb,
    // UDP content length is zero
    0x00, 0x00,
    // Checksum is not actually checked in any of the tests, so we leave it as
    // zero
    0x00, 0x00,
};

static const char kReferenceClientPacketDataAF3[] = {
    // IPv6 with 0x60 TOS and zero flow label.
    0x66, 0x00, 0x00, 0x00,
    // Payload size is 8 bytes.
    0x00, 0x08,
    // Next header is UDP
    17,
    // TTL is 50.
    50,
    // IP address of the sender is fd00:0:0:1::1
    0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // IP address of the receiver is fd00:0:0:5::1
    0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // Source port 12345
    0x30, 0x39,
    // Destination port 443
    0x01, 0xbb,
    // UDP content length is zero
    0x00, 0x00,
    // Checksum is not actually checked in any of the tests, so we leave it as
    // zero
    0x00, 0x00,
};

static const char kReferenceClientPacketDataAF2[] = {
    // IPv6 with 0x40 TOS and zero flow label.
    0x64, 0x00, 0x00, 0x00,
    // Payload size is 8 bytes.
    0x00, 0x08,
    // Next header is UDP
    17,
    // TTL is 50.
    50,
    // IP address of the sender is fd00:0:0:1::1
    0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // IP address of the receiver is fd00:0:0:5::1
    0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // Source port 12345
    0x30, 0x39,
    // Destination port 443
    0x01, 0xbb,
    // UDP content length is zero
    0x00, 0x00,
    // Checksum is not actually checked in any of the tests, so we leave it as
    // zero
    0x00, 0x00,
};

static const char kReferenceClientPacketDataAF1[] = {
    // IPv6 with 0x20 TOS and zero flow label.
    0x62, 0x00, 0x00, 0x00,
    // Payload size is 8 bytes.
    0x00, 0x08,
    // Next header is UDP
    17,
    // TTL is 50.
    50,
    // IP address of the sender is fd00:0:0:1::1
    0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // IP address of the receiver is fd00:0:0:5::1
    0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // Source port 12345
    0x30, 0x39,
    // Destination port 443
    0x01, 0xbb,
    // UDP content length is zero
    0x00, 0x00,
    // Checksum is not actually checked in any of the tests, so we leave it as
    // zero
    0x00, 0x00,
};

static const char kReferenceNetworkPacketData[] = {
    // IPv6 with zero TOS and flow label.
    0x60, 0x00, 0x00, 0x00,
    // Payload size is 8 bytes.
    0x00, 0x08,
    // Next header is UDP
    17,
    // TTL is 50.
    50,
    // IP address of the sender is fd00:0:0:5::1
    0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // IP address of the receiver is fd00:0:0:1::1
    0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // Source port 443
    0x01, 0xbb,
    // Destination port 12345
    0x30, 0x39,
    // UDP content length is zero
    0x00, 0x00,
    // Checksum is not actually checked in any of the tests, so we leave it as
    // zero
    0x00, 0x00,
};

static const char kReferenceClientSubnetPacketData[] = {
    // IPv6 with zero TOS and flow label.
    0x60, 0x00, 0x00, 0x00,
    // Payload size is 8 bytes.
    0x00, 0x08,
    // Next header is UDP
    17,
    // TTL is 50.
    50,
    // IP address of the sender is fd00:0:0:2::1, which is within the /62 of the
    // client.
    0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // IP address of the receiver is fd00:0:0:5::1
    0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // Source port 12345
    0x30, 0x39,
    // Destination port 443
    0x01, 0xbb,
    // UDP content length is zero
    0x00, 0x00,
    // Checksum is not actually checked in any of the tests, so we leave it as
    // zero
    0x00, 0x00,
};

static const char kReferenceEchoRequestData[] = {
    // IPv6 with zero TOS and flow label.
    0x60, 0x00, 0x00, 0x00,
    // Payload size is 64 bytes.
    0x00, 64,
    // Next header is ICMP
    58,
    // TTL is 127.
    127,
    // IP address of the sender is fd00:0:0:1::1
    0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // IP address of the receiver is fe80::71:626f:6e6f
    0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x71, 0x62, 0x6f, 0x6e, 0x6f,
    // ICMP Type ping request
    128,
    // ICMP Code 0
    0,
    // Checksum is not actually checked in any of the tests, so we leave it as
    // zero
    0x00, 0x00,
    // ICMP Identifier (0xcafe to be memorable)
    0xca, 0xfe,
    // Sequence number
    0x00, 0x01,
    // Data, starting with unix timeval then 0x10..0x37
    0x67, 0x37, 0x8a, 0x63, 0x00, 0x00, 0x00, 0x00,
    0x96, 0x58, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
};

static const char kReferenceEchoReplyData[] = {
    // IPv6 with zero TOS and flow label.
    0x60, 0x00, 0x00, 0x00,
    // Payload size is 64 bytes.
    0x00, 64,
    // Next header is ICMP
    58,
    // TTL is 255.
    255,
    // IP address of the sender is fd00:4:0:1::1
    0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // IP address of the receiver is fd00:0:0:1::1
    0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // ICMP Type ping reply
    129,
    // ICMP Code 0
    0,
    // Checksum
    0x66, 0xb6,
    // ICMP Identifier (0xcafe to be memorable)
    0xca, 0xfe,
    // Sequence number
    0x00, 0x01,
    // Data, starting with unix timeval then 0x10..0x37
    0x67, 0x37, 0x8a, 0x63, 0x00, 0x00, 0x00, 0x00,
    0x96, 0x58, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
};

// clang-format on

static const absl::string_view kReferenceClientPacket(
    kReferenceClientPacketData, ABSL_ARRAYSIZE(kReferenceClientPacketData));

static const absl::string_view kReferenceClientPacketAF4(
    kReferenceClientPacketDataAF4,
    ABSL_ARRAYSIZE(kReferenceClientPacketDataAF4));
static const absl::string_view kReferenceClientPacketAF3(
    kReferenceClientPacketDataAF3,
    ABSL_ARRAYSIZE(kReferenceClientPacketDataAF3));
static const absl::string_view kReferenceClientPacketAF2(
    kReferenceClientPacketDataAF2,
    ABSL_ARRAYSIZE(kReferenceClientPacketDataAF2));
static const absl::string_view kReferenceClientPacketAF1(
    kReferenceClientPacketDataAF1,
    ABSL_ARRAYSIZE(kReferenceClientPacketDataAF1));

static const absl::string_view kReferenceNetworkPacket(
    kReferenceNetworkPacketData, ABSL_ARRAYSIZE(kReferenceNetworkPacketData));

static const absl::string_view kReferenceClientSubnetPacket(
    kReferenceClientSubnetPacketData,
    ABSL_ARRAYSIZE(kReferenceClientSubnetPacketData));

static const absl::string_view kReferenceEchoRequest(
    kReferenceEchoRequestData, ABSL_ARRAYSIZE(kReferenceEchoRequestData));

MATCHER_P(IsIcmpMessage, icmp_type,
          "Checks whether the argument is an ICMP message of supplied type") {
  if (arg.size() < kTotalICMPv6HeaderSize) {
    return false;
  }

  return arg[40] == icmp_type;
}

class MockPacketFilter : public QbonePacketProcessor::Filter {
 public:
  MOCK_METHOD(ProcessingResult, FilterPacket,
              (Direction, absl::string_view, absl::string_view, icmp6_hdr*),
              (override));
};

class QbonePacketProcessorTest : public QuicTest {
 protected:
  QbonePacketProcessorTest() {
    QUICHE_CHECK(client_ip_.FromString("fd00:0:0:1::1"));
    QUICHE_CHECK(self_ip_.FromString("fd00:0:0:4::1"));
    QUICHE_CHECK(network_ip_.FromString("fd00:0:0:5::1"));

    processor_ = std::make_unique<QbonePacketProcessor>(
        self_ip_, client_ip_, /*client_ip_subnet_length=*/62, &output_,
        &stats_);

    // Ignore calls to RecordThroughput
    EXPECT_CALL(stats_, RecordThroughput(_, _, _)).WillRepeatedly(Return());
  }

  void SendPacketFromClient(absl::string_view packet) {
    std::string packet_buffer(packet.data(), packet.size());
    processor_->ProcessPacket(&packet_buffer, Direction::FROM_OFF_NETWORK);
  }

  void SendPacketFromNetwork(absl::string_view packet) {
    std::string packet_buffer(packet.data(), packet.size());
    processor_->ProcessPacket(&packet_buffer, Direction::FROM_NETWORK);
  }

  QuicIpAddress client_ip_;
  QuicIpAddress self_ip_;
  QuicIpAddress network_ip_;

  std::unique_ptr<QbonePacketProcessor> processor_;
  testing::StrictMock<MockPacketProcessorOutput> output_;
  testing::StrictMock<MockPacketProcessorStats> stats_;
};

TEST_F(QbonePacketProcessorTest, EmptyPacket) {
  EXPECT_CALL(stats_, OnPacketDroppedSilently(Direction::FROM_OFF_NETWORK, _));
  EXPECT_CALL(stats_, RecordThroughput(0, Direction::FROM_OFF_NETWORK, _));
  SendPacketFromClient("");

  EXPECT_CALL(stats_, OnPacketDroppedSilently(Direction::FROM_NETWORK, _));
  EXPECT_CALL(stats_, RecordThroughput(0, Direction::FROM_NETWORK, _));
  SendPacketFromNetwork("");
}

TEST_F(QbonePacketProcessorTest, RandomGarbage) {
  EXPECT_CALL(stats_, OnPacketDroppedSilently(Direction::FROM_OFF_NETWORK, _));
  SendPacketFromClient(std::string(1280, 'a'));

  EXPECT_CALL(stats_, OnPacketDroppedSilently(Direction::FROM_NETWORK, _));
  SendPacketFromNetwork(std::string(1280, 'a'));
}

TEST_F(QbonePacketProcessorTest, RandomGarbageWithCorrectLengthFields) {
  std::string packet(40, 'a');
  packet[4] = 0;
  packet[5] = 0;

  EXPECT_CALL(stats_, OnPacketDroppedWithIcmp(Direction::FROM_OFF_NETWORK, _));
  EXPECT_CALL(output_, SendPacketToClient(IsIcmpMessage(ICMP6_DST_UNREACH)));
  SendPacketFromClient(packet);
}

TEST_F(QbonePacketProcessorTest, GoodPacketFromClient) {
  EXPECT_CALL(stats_, OnPacketForwarded(Direction::FROM_OFF_NETWORK, _));
  EXPECT_CALL(output_, SendPacketToNetwork(_));
  SendPacketFromClient(kReferenceClientPacket);
}

TEST_F(QbonePacketProcessorTest, GoodPacketFromClientSubnet) {
  EXPECT_CALL(stats_, OnPacketForwarded(Direction::FROM_OFF_NETWORK, _));
  EXPECT_CALL(output_, SendPacketToNetwork(_));
  SendPacketFromClient(kReferenceClientSubnetPacket);
}

TEST_F(QbonePacketProcessorTest, GoodPacketFromNetwork) {
  EXPECT_CALL(stats_, OnPacketForwarded(Direction::FROM_NETWORK, _));
  EXPECT_CALL(output_, SendPacketToClient(_));
  SendPacketFromNetwork(kReferenceNetworkPacket);
}

TEST_F(QbonePacketProcessorTest, GoodPacketFromNetworkWrongDirection) {
  EXPECT_CALL(stats_, OnPacketDroppedWithIcmp(Direction::FROM_OFF_NETWORK, _));
  EXPECT_CALL(output_, SendPacketToClient(IsIcmpMessage(ICMP6_DST_UNREACH)));
  SendPacketFromClient(kReferenceNetworkPacket);
}

TEST_F(QbonePacketProcessorTest, TtlExpired) {
  std::string packet(kReferenceNetworkPacket);
  packet[7] = 1;

  EXPECT_CALL(stats_, OnPacketDroppedWithIcmp(Direction::FROM_NETWORK, _));
  EXPECT_CALL(output_, SendPacketToNetwork(IsIcmpMessage(ICMP6_TIME_EXCEEDED)));
  SendPacketFromNetwork(packet);
}

TEST_F(QbonePacketProcessorTest, UnknownProtocol) {
  std::string packet(kReferenceNetworkPacket);
  packet[6] = IPPROTO_SCTP;

  EXPECT_CALL(stats_, OnPacketDroppedWithIcmp(Direction::FROM_NETWORK, _));
  EXPECT_CALL(output_, SendPacketToNetwork(IsIcmpMessage(ICMP6_PARAM_PROB)));
  SendPacketFromNetwork(packet);
}

TEST_F(QbonePacketProcessorTest, FilterFromClient) {
  auto filter = std::make_unique<MockPacketFilter>();
  EXPECT_CALL(*filter, FilterPacket(_, _, _, _))
      .WillRepeatedly(Return(ProcessingResult::SILENT_DROP));
  processor_->set_filter(std::move(filter));

  EXPECT_CALL(stats_, OnPacketDroppedSilently(Direction::FROM_OFF_NETWORK, _));
  SendPacketFromClient(kReferenceClientPacket);
}

class TestFilter : public QbonePacketProcessor::Filter {
 public:
  TestFilter(QuicIpAddress client_ip, QuicIpAddress network_ip)
      : client_ip_(client_ip), network_ip_(network_ip) {}
  ProcessingResult FilterPacket(Direction direction,
                                absl::string_view full_packet,
                                absl::string_view payload,
                                icmp6_hdr* icmp_header) override {
    EXPECT_EQ(kIPv6HeaderSize, full_packet.size() - payload.size());
    EXPECT_EQ(IPPROTO_UDP, TransportProtocolFromHeader(full_packet));
    EXPECT_EQ(client_ip_, SourceIpFromHeader(full_packet));
    EXPECT_EQ(network_ip_, DestinationIpFromHeader(full_packet));

    last_tos_ = QbonePacketProcessor::TrafficClassFromHeader(full_packet);
    called_++;
    return ProcessingResult::SILENT_DROP;
  }

  int called() const { return called_; }
  uint8_t last_tos() const { return last_tos_; }

 private:
  int called_ = 0;
  uint8_t last_tos_ = 0;

  QuicIpAddress client_ip_;
  QuicIpAddress network_ip_;
};

// Verify that the parameters are passed correctly into the filter, and that the
// helper functions of the filter class work.
TEST_F(QbonePacketProcessorTest, FilterHelperFunctions) {
  auto filter_owned = std::make_unique<TestFilter>(client_ip_, network_ip_);
  TestFilter* filter = filter_owned.get();
  processor_->set_filter(std::move(filter_owned));

  EXPECT_CALL(stats_, OnPacketDroppedSilently(Direction::FROM_OFF_NETWORK, _));
  SendPacketFromClient(kReferenceClientPacket);
  ASSERT_EQ(1, filter->called());
}

TEST_F(QbonePacketProcessorTest, FilterHelperFunctionsTOS) {
  auto filter_owned = std::make_unique<TestFilter>(client_ip_, network_ip_);
  processor_->set_filter(std::move(filter_owned));

  EXPECT_CALL(stats_, OnPacketDroppedSilently(Direction::FROM_OFF_NETWORK, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(stats_, RecordThroughput(kReferenceClientPacket.size(),
                                       Direction::FROM_OFF_NETWORK, 0));
  SendPacketFromClient(kReferenceClientPacket);

  EXPECT_CALL(stats_, RecordThroughput(kReferenceClientPacketAF4.size(),
                                       Direction::FROM_OFF_NETWORK, 0x80));
  SendPacketFromClient(kReferenceClientPacketAF4);

  EXPECT_CALL(stats_, RecordThroughput(kReferenceClientPacketAF3.size(),
                                       Direction::FROM_OFF_NETWORK, 0x60));
  SendPacketFromClient(kReferenceClientPacketAF3);

  EXPECT_CALL(stats_, RecordThroughput(kReferenceClientPacketAF2.size(),
                                       Direction::FROM_OFF_NETWORK, 0x40));
  SendPacketFromClient(kReferenceClientPacketAF2);

  EXPECT_CALL(stats_, RecordThroughput(kReferenceClientPacketAF1.size(),
                                       Direction::FROM_OFF_NETWORK, 0x20));
  SendPacketFromClient(kReferenceClientPacketAF1);
}

TEST_F(QbonePacketProcessorTest, Icmp6EchoResponseHasRightPayload) {
  auto filter = std::make_unique<MockPacketFilter>();
  EXPECT_CALL(*filter, FilterPacket(_, _, _, _))
      .WillOnce(WithArgs<2, 3>(
          Invoke([](absl::string_view payload, icmp6_hdr* icmp_header) {
            icmp_header->icmp6_type = ICMP6_ECHO_REPLY;
            icmp_header->icmp6_code = 0;
            auto* request_header =
                reinterpret_cast<const icmp6_hdr*>(payload.data());
            icmp_header->icmp6_id = request_header->icmp6_id;
            icmp_header->icmp6_seq = request_header->icmp6_seq;
            return ProcessingResult::ICMP;
          })));
  processor_->set_filter(std::move(filter));

  EXPECT_CALL(stats_, OnPacketDroppedWithIcmp(Direction::FROM_OFF_NETWORK, _));
  EXPECT_CALL(output_, SendPacketToClient(_))
      .WillOnce(Invoke([](absl::string_view packet) {
        // Explicit conversion because otherwise it is treated as a null
        // terminated string.
        absl::string_view expected = absl::string_view(
            kReferenceEchoReplyData, sizeof(kReferenceEchoReplyData));

        EXPECT_THAT(packet, Eq(expected));
        QUIC_LOG(INFO) << "ICMP response:\n"
                       << quiche::QuicheTextUtils::HexDump(packet);
      }));
  SendPacketFromClient(kReferenceEchoRequest);
}

}  // namespace
}  // namespace quic::test
