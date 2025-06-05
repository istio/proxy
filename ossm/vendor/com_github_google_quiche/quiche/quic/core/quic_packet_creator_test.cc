// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_packet_creator.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/crypto/quic_decrypter.h"
#include "quiche/quic/core/crypto/quic_encrypter.h"
#include "quiche/quic/core/frames/quic_frame.h"
#include "quiche/quic/core/frames/quic_stream_frame.h"
#include "quiche/quic/core/quic_connection_id.h"
#include "quiche/quic/core/quic_data_writer.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_framer_peer.h"
#include "quiche/quic/test_tools/quic_packet_creator_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/quic/test_tools/simple_data_producer.h"
#include "quiche/quic/test_tools/simple_quic_framer.h"
#include "quiche/common/simple_buffer_allocator.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace quic {
namespace test {
namespace {

const QuicPacketNumber kPacketNumber = QuicPacketNumber(UINT64_C(0x12345678));
// Use fields in which each byte is distinct to ensure that every byte is
// framed correctly. The values are otherwise arbitrary.
QuicConnectionId CreateTestConnectionId() {
  return TestConnectionId(UINT64_C(0xFEDCBA9876543210));
}

// Run tests with combinations of {ParsedQuicVersion,
// ToggleVersionSerialization}.
struct TestParams {
  TestParams(ParsedQuicVersion version, bool version_serialization)
      : version(version), version_serialization(version_serialization) {}

  ParsedQuicVersion version;
  bool version_serialization;
};

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const TestParams& p) {
  return absl::StrCat(ParsedQuicVersionToString(p.version), "_",
                      (p.version_serialization ? "Include" : "No"), "Version");
}

// Constructs various test permutations.
std::vector<TestParams> GetTestParams() {
  std::vector<TestParams> params;
  ParsedQuicVersionVector all_supported_versions = AllSupportedVersions();
  for (size_t i = 0; i < all_supported_versions.size(); ++i) {
    params.push_back(TestParams(all_supported_versions[i], true));
    params.push_back(TestParams(all_supported_versions[i], false));
  }
  return params;
}

class MockDebugDelegate : public QuicPacketCreator::DebugDelegate {
 public:
  ~MockDebugDelegate() override = default;

  MOCK_METHOD(void, OnFrameAddedToPacket, (const QuicFrame& frame), (override));

  MOCK_METHOD(void, OnStreamFrameCoalesced, (const QuicStreamFrame& frame),
              (override));
};

class TestPacketCreator : public QuicPacketCreator {
 public:
  TestPacketCreator(QuicConnectionId connection_id, QuicFramer* framer,
                    DelegateInterface* delegate, SimpleDataProducer* producer)
      : QuicPacketCreator(connection_id, framer, delegate),
        producer_(producer),
        version_(framer->version()) {}

  bool ConsumeDataToFillCurrentPacket(QuicStreamId id, absl::string_view data,
                                      QuicStreamOffset offset, bool fin,
                                      bool needs_full_padding,
                                      TransmissionType transmission_type,
                                      QuicFrame* frame) {
    // Save data before data is consumed.
    if (!data.empty()) {
      producer_->SaveStreamData(id, data);
    }
    return QuicPacketCreator::ConsumeDataToFillCurrentPacket(
        id, data.length(), offset, fin, needs_full_padding, transmission_type,
        frame);
  }

  void StopSendingVersion() { set_encryption_level(ENCRYPTION_FORWARD_SECURE); }

  SimpleDataProducer* producer_;
  ParsedQuicVersion version_;
};

class QuicPacketCreatorTest : public QuicTestWithParam<TestParams> {
 public:
  void ClearSerializedPacketForTests(SerializedPacket /*serialized_packet*/) {
    // serialized packet self-clears on destruction.
  }

  void SaveSerializedPacket(SerializedPacket serialized_packet) {
    serialized_packet_.reset(CopySerializedPacket(
        serialized_packet, &allocator_, /*copy_buffer=*/true));
  }

  void DeleteSerializedPacket() { serialized_packet_ = nullptr; }

 protected:
  QuicPacketCreatorTest()
      : connection_id_(TestConnectionId(2)),
        server_framer_(SupportedVersions(GetParam().version), QuicTime::Zero(),
                       Perspective::IS_SERVER, connection_id_.length()),
        client_framer_(SupportedVersions(GetParam().version), QuicTime::Zero(),
                       Perspective::IS_CLIENT, connection_id_.length()),
        data_("foo"),
        creator_(connection_id_, &client_framer_, &delegate_, &producer_) {
    EXPECT_CALL(delegate_, GetPacketBuffer())
        .WillRepeatedly(Return(QuicPacketBuffer()));
    EXPECT_CALL(delegate_, GetSerializedPacketFate(_, _))
        .WillRepeatedly(Return(SEND_TO_WRITER));
    creator_.SetEncrypter(
        ENCRYPTION_INITIAL,
        std::make_unique<TaggingEncrypter>(ENCRYPTION_INITIAL));
    creator_.SetEncrypter(
        ENCRYPTION_HANDSHAKE,
        std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
    creator_.SetEncrypter(
        ENCRYPTION_ZERO_RTT,
        std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
    creator_.SetEncrypter(
        ENCRYPTION_FORWARD_SECURE,
        std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
    client_framer_.set_visitor(&framer_visitor_);
    server_framer_.set_visitor(&framer_visitor_);
    client_framer_.set_data_producer(&producer_);
    if (server_framer_.version().KnowsWhichDecrypterToUse()) {
      server_framer_.InstallDecrypter(ENCRYPTION_INITIAL,
                                      std::make_unique<TaggingDecrypter>());
      server_framer_.InstallDecrypter(ENCRYPTION_ZERO_RTT,
                                      std::make_unique<TaggingDecrypter>());
      server_framer_.InstallDecrypter(ENCRYPTION_HANDSHAKE,
                                      std::make_unique<TaggingDecrypter>());
      server_framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                                      std::make_unique<TaggingDecrypter>());
    } else {
      server_framer_.SetDecrypter(ENCRYPTION_INITIAL,
                                  std::make_unique<TaggingDecrypter>());
      server_framer_.SetAlternativeDecrypter(
          ENCRYPTION_FORWARD_SECURE, std::make_unique<TaggingDecrypter>(),
          false);
    }
  }

  ~QuicPacketCreatorTest() override {}

  SerializedPacket SerializeAllFrames(const QuicFrames& frames) {
    SerializedPacket packet = QuicPacketCreatorPeer::SerializeAllFrames(
        &creator_, frames, buffer_, kMaxOutgoingPacketSize);
    EXPECT_EQ(QuicPacketCreatorPeer::GetEncryptionLevel(&creator_),
              packet.encryption_level);
    return packet;
  }

  void ProcessPacket(const SerializedPacket& packet) {
    QuicEncryptedPacket encrypted_packet(packet.encrypted_buffer,
                                         packet.encrypted_length);
    server_framer_.ProcessPacket(encrypted_packet);
  }

  void CheckStreamFrame(const QuicFrame& frame, QuicStreamId stream_id,
                        const std::string& data, QuicStreamOffset offset,
                        bool fin) {
    EXPECT_EQ(STREAM_FRAME, frame.type);
    EXPECT_EQ(stream_id, frame.stream_frame.stream_id);
    char buf[kMaxOutgoingPacketSize];
    QuicDataWriter writer(kMaxOutgoingPacketSize, buf, quiche::HOST_BYTE_ORDER);
    if (frame.stream_frame.data_length > 0) {
      producer_.WriteStreamData(stream_id, frame.stream_frame.offset,
                                frame.stream_frame.data_length, &writer);
    }
    EXPECT_EQ(data, absl::string_view(buf, frame.stream_frame.data_length));
    EXPECT_EQ(offset, frame.stream_frame.offset);
    EXPECT_EQ(fin, frame.stream_frame.fin);
  }

  // Returns the number of bytes consumed by the header of packet, including
  // the version.
  size_t GetPacketHeaderOverhead(QuicTransportVersion version) {
    return GetPacketHeaderSize(
        version, creator_.GetDestinationConnectionIdLength(),
        creator_.GetSourceConnectionIdLength(),
        QuicPacketCreatorPeer::SendVersionInPacket(&creator_),
        !kIncludeDiversificationNonce,
        QuicPacketCreatorPeer::GetPacketNumberLength(&creator_),
        QuicPacketCreatorPeer::GetRetryTokenLengthLength(&creator_), 0,
        QuicPacketCreatorPeer::GetLengthLength(&creator_));
  }

  // Returns the number of bytes of overhead that will be added to a packet
  // of maximum length.
  size_t GetEncryptionOverhead() {
    return creator_.max_packet_length() -
           client_framer_.GetMaxPlaintextSize(creator_.max_packet_length());
  }

  // Returns the number of bytes consumed by the non-data fields of a stream
  // frame, assuming it is the last frame in the packet
  size_t GetStreamFrameOverhead(QuicTransportVersion version) {
    return QuicFramer::GetMinStreamFrameSize(
        version, GetNthClientInitiatedStreamId(1), kOffset, true,
        /* data_length= */ 0);
  }

  bool IsDefaultTestConfiguration() {
    TestParams p = GetParam();
    return p.version == AllSupportedVersions()[0] && p.version_serialization;
  }

  QuicStreamId GetNthClientInitiatedStreamId(int n) const {
    return QuicUtils::GetFirstBidirectionalStreamId(
               creator_.transport_version(), Perspective::IS_CLIENT) +
           n * 2;
  }

  void TestChaosProtection(bool enabled);

  static constexpr QuicStreamOffset kOffset = 0u;

  char buffer_[kMaxOutgoingPacketSize];
  QuicConnectionId connection_id_;
  QuicFrames frames_;
  QuicFramer server_framer_;
  QuicFramer client_framer_;
  StrictMock<MockFramerVisitor> framer_visitor_;
  StrictMock<MockPacketCreatorDelegate> delegate_;
  std::string data_;
  TestPacketCreator creator_;
  std::unique_ptr<SerializedPacket> serialized_packet_;
  SimpleDataProducer producer_;
  quiche::SimpleBufferAllocator allocator_;
};

// Run all packet creator tests with all supported versions of QUIC, and with
// and without version in the packet header, as well as doing a run for each
// length of truncated connection id.
INSTANTIATE_TEST_SUITE_P(QuicPacketCreatorTests, QuicPacketCreatorTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicPacketCreatorTest, SerializeFrames) {
  ParsedQuicVersion version = client_framer_.version();
  for (int i = ENCRYPTION_INITIAL; i < NUM_ENCRYPTION_LEVELS; ++i) {
    EncryptionLevel level = static_cast<EncryptionLevel>(i);
    bool has_ack = false, has_stream = false;
    creator_.set_encryption_level(level);
    size_t payload_len = 0;
    if (level != ENCRYPTION_ZERO_RTT) {
      frames_.push_back(QuicFrame(new QuicAckFrame(InitAckFrame(1))));
      has_ack = true;
      payload_len += version.UsesTls() ? 12 : 6;
    }
    if (level != ENCRYPTION_INITIAL && level != ENCRYPTION_HANDSHAKE) {
      QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
          client_framer_.transport_version(), Perspective::IS_CLIENT);
      frames_.push_back(QuicFrame(
          QuicStreamFrame(stream_id, false, 0u, absl::string_view())));
      has_stream = true;
      payload_len += 2;
    }
    SerializedPacket serialized = SerializeAllFrames(frames_);
    EXPECT_EQ(level, serialized.encryption_level);
    if (level != ENCRYPTION_ZERO_RTT) {
      delete frames_[0].ack_frame;
    }
    frames_.clear();
    ASSERT_GT(payload_len, 0);  // Must have a frame!
    size_t min_payload = version.UsesTls() ? 3 : 7;
    bool need_padding =
        (version.HasHeaderProtection() && (payload_len < min_payload));
    {
      InSequence s;
      EXPECT_CALL(framer_visitor_, OnPacket());
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
      EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
      EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
      if (need_padding) {
        EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
      }
      if (has_ack) {
        EXPECT_CALL(framer_visitor_, OnAckFrameStart(_, _))
            .WillOnce(Return(true));
        EXPECT_CALL(framer_visitor_,
                    OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2)))
            .WillOnce(Return(true));
        EXPECT_CALL(framer_visitor_, OnAckFrameEnd(QuicPacketNumber(1), _))
            .WillOnce(Return(true));
      }
      if (has_stream) {
        EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
      }
      EXPECT_CALL(framer_visitor_, OnPacketComplete());
    }
    ProcessPacket(serialized);
  }
}

TEST_P(QuicPacketCreatorTest, SerializeConnectionClose) {
  QuicConnectionCloseFrame* frame = new QuicConnectionCloseFrame(
      creator_.transport_version(), QUIC_NO_ERROR, NO_IETF_QUIC_ERROR, "error",
      /*transport_close_frame_type=*/0);

  QuicFrames frames;
  frames.push_back(QuicFrame(frame));
  SerializedPacket serialized = SerializeAllFrames(frames);
  EXPECT_EQ(ENCRYPTION_INITIAL, serialized.encryption_level);
  ASSERT_EQ(QuicPacketNumber(1u), serialized.packet_number);
  ASSERT_EQ(QuicPacketNumber(1u), creator_.packet_number());

  InSequence s;
  EXPECT_CALL(framer_visitor_, OnPacket());
  EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
  EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
  EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnConnectionCloseFrame(_));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  ProcessPacket(serialized);
}

TEST_P(QuicPacketCreatorTest, SerializePacketWithPadding) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);

  creator_.AddFrame(QuicFrame(QuicWindowUpdateFrame()), NOT_RETRANSMISSION);
  creator_.AddFrame(QuicFrame(QuicPaddingFrame()), NOT_RETRANSMISSION);
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  creator_.FlushCurrentPacket();
  ASSERT_TRUE(serialized_packet_->encrypted_buffer);

  EXPECT_EQ(kDefaultMaxPacketSize, serialized_packet_->encrypted_length);

  DeleteSerializedPacket();
}

TEST_P(QuicPacketCreatorTest, SerializeLargerPacketWithPadding) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  const QuicByteCount packet_size = 100 + kDefaultMaxPacketSize;
  creator_.SetMaxPacketLength(packet_size);

  creator_.AddFrame(QuicFrame(QuicWindowUpdateFrame()), NOT_RETRANSMISSION);
  creator_.AddFrame(QuicFrame(QuicPaddingFrame()), NOT_RETRANSMISSION);
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  creator_.FlushCurrentPacket();
  ASSERT_TRUE(serialized_packet_->encrypted_buffer);

  EXPECT_EQ(packet_size, serialized_packet_->encrypted_length);

  DeleteSerializedPacket();
}

TEST_P(QuicPacketCreatorTest, IncreaseMaxPacketLengthWithFramesPending) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  const QuicByteCount packet_size = 100 + kDefaultMaxPacketSize;

  // Since the creator has a frame queued, the packet size will not change.
  creator_.AddFrame(QuicFrame(QuicWindowUpdateFrame()), NOT_RETRANSMISSION);
  creator_.SetMaxPacketLength(packet_size);
  creator_.AddFrame(QuicFrame(QuicPaddingFrame()), NOT_RETRANSMISSION);
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  creator_.FlushCurrentPacket();
  ASSERT_TRUE(serialized_packet_->encrypted_buffer);

  EXPECT_EQ(kDefaultMaxPacketSize, serialized_packet_->encrypted_length);

  DeleteSerializedPacket();

  // Now that the previous packet was generated, the next on will use
  // the new larger size.
  creator_.AddFrame(QuicFrame(QuicWindowUpdateFrame()), NOT_RETRANSMISSION);
  creator_.AddFrame(QuicFrame(QuicPaddingFrame()), NOT_RETRANSMISSION);
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  creator_.FlushCurrentPacket();
  ASSERT_TRUE(serialized_packet_->encrypted_buffer);
  EXPECT_EQ(packet_size, serialized_packet_->encrypted_length);

  EXPECT_EQ(packet_size, serialized_packet_->encrypted_length);

  DeleteSerializedPacket();
}

TEST_P(QuicPacketCreatorTest, ConsumeCryptoDataToFillCurrentPacket) {
  std::string data = "crypto data";
  QuicFrame frame;
  ASSERT_TRUE(creator_.ConsumeCryptoDataToFillCurrentPacket(
      ENCRYPTION_INITIAL, data.length(), 0,
      /*needs_full_padding=*/true, NOT_RETRANSMISSION, &frame));
  EXPECT_EQ(frame.crypto_frame->data_length, data.length());
  EXPECT_TRUE(creator_.HasPendingFrames());
}

TEST_P(QuicPacketCreatorTest, ConsumeDataToFillCurrentPacket) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  QuicFrame frame;
  QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      client_framer_.transport_version(), Perspective::IS_CLIENT);
  const std::string data("test");
  ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
      stream_id, data, 0u, false, false, NOT_RETRANSMISSION, &frame));
  size_t consumed = frame.stream_frame.data_length;
  EXPECT_EQ(4u, consumed);
  CheckStreamFrame(frame, stream_id, "test", 0u, false);
  EXPECT_TRUE(creator_.HasPendingFrames());
}

TEST_P(QuicPacketCreatorTest, ConsumeDataFin) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  QuicFrame frame;
  QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      client_framer_.transport_version(), Perspective::IS_CLIENT);
  const std::string data("test");
  ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
      stream_id, data, 0u, true, false, NOT_RETRANSMISSION, &frame));
  size_t consumed = frame.stream_frame.data_length;
  EXPECT_EQ(4u, consumed);
  CheckStreamFrame(frame, stream_id, "test", 0u, true);
  EXPECT_TRUE(creator_.HasPendingFrames());
}

TEST_P(QuicPacketCreatorTest, ConsumeDataFinOnly) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  QuicFrame frame;
  QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      client_framer_.transport_version(), Perspective::IS_CLIENT);
  ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
      stream_id, {}, 0u, true, false, NOT_RETRANSMISSION, &frame));
  size_t consumed = frame.stream_frame.data_length;
  EXPECT_EQ(0u, consumed);
  CheckStreamFrame(frame, stream_id, std::string(), 0u, true);
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_TRUE(absl::StartsWith(creator_.GetPendingFramesInfo(),
                               "type { STREAM_FRAME }"));
}

TEST_P(QuicPacketCreatorTest, CreateAllFreeBytesForStreamFrames) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  const size_t overhead =
      GetPacketHeaderOverhead(client_framer_.transport_version()) +
      GetEncryptionOverhead();
  for (size_t i = overhead +
                  QuicPacketCreator::MinPlaintextPacketSize(
                      client_framer_.version(),
                      QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));
       i < overhead + 100; ++i) {
    SCOPED_TRACE(i);
    creator_.SetMaxPacketLength(i);
    const bool should_have_room =
        i >
        overhead + GetStreamFrameOverhead(client_framer_.transport_version());
    ASSERT_EQ(should_have_room,
              creator_.HasRoomForStreamFrame(GetNthClientInitiatedStreamId(1),
                                             kOffset, /* data_size=*/0xffff));
    if (should_have_room) {
      QuicFrame frame;
      const std::string data("testdata");
      EXPECT_CALL(delegate_, OnSerializedPacket(_))
          .WillRepeatedly(Invoke(
              this, &QuicPacketCreatorTest::ClearSerializedPacketForTests));
      ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
          GetNthClientInitiatedStreamId(1), data, kOffset, false, false,
          NOT_RETRANSMISSION, &frame));
      size_t bytes_consumed = frame.stream_frame.data_length;
      EXPECT_LT(0u, bytes_consumed);
      creator_.FlushCurrentPacket();
    }
  }
}

TEST_P(QuicPacketCreatorTest, StreamFrameConsumption) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  // Compute the total overhead for a single frame in packet.
  const size_t overhead =
      GetPacketHeaderOverhead(client_framer_.transport_version()) +
      GetEncryptionOverhead() +
      GetStreamFrameOverhead(client_framer_.transport_version());
  size_t capacity = kDefaultMaxPacketSize - overhead;
  // Now, test various sizes around this size.
  for (int delta = -5; delta <= 5; ++delta) {
    std::string data(capacity + delta, 'A');
    size_t bytes_free = delta > 0 ? 0 : 0 - delta;
    QuicFrame frame;
    ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
        GetNthClientInitiatedStreamId(1), data, kOffset, false, false,
        NOT_RETRANSMISSION, &frame));

    // BytesFree() returns bytes available for the next frame, which will
    // be two bytes smaller since the stream frame would need to be grown.
    EXPECT_EQ(2u, creator_.ExpansionOnNewFrame());
    size_t expected_bytes_free = bytes_free < 3 ? 0 : bytes_free - 2;
    EXPECT_EQ(expected_bytes_free, creator_.BytesFree()) << "delta: " << delta;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
    creator_.FlushCurrentPacket();
    ASSERT_TRUE(serialized_packet_->encrypted_buffer);
    DeleteSerializedPacket();
  }
}

TEST_P(QuicPacketCreatorTest, CryptoStreamFramePacketPadding) {
  // This test serializes crypto payloads slightly larger than a packet, which
  // Causes the multi-packet ClientHello check to fail.
  SetQuicFlag(quic_enforce_single_packet_chlo, false);
  // Compute the total overhead for a single frame in packet.
  size_t overhead =
      GetPacketHeaderOverhead(client_framer_.transport_version()) +
      GetEncryptionOverhead();
  if (QuicVersionUsesCryptoFrames(client_framer_.transport_version())) {
    overhead +=
        QuicFramer::GetMinCryptoFrameSize(kOffset, kMaxOutgoingPacketSize);
  } else {
    overhead += QuicFramer::GetMinStreamFrameSize(
        client_framer_.transport_version(), GetNthClientInitiatedStreamId(1),
        kOffset, false, 0);
  }
  ASSERT_GT(kMaxOutgoingPacketSize, overhead);
  size_t capacity = kDefaultMaxPacketSize - overhead;
  // Now, test various sizes around this size.
  for (int delta = -5; delta <= 5; ++delta) {
    SCOPED_TRACE(delta);
    std::string data(capacity + delta, 'A');
    size_t bytes_free = delta > 0 ? 0 : 0 - delta;

    QuicFrame frame;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillRepeatedly(
            Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
    if (client_framer_.version().CanSendCoalescedPackets()) {
      EXPECT_CALL(delegate_, GetSerializedPacketFate(_, _))
          .WillRepeatedly(Return(COALESCE));
    }
    if (!QuicVersionUsesCryptoFrames(client_framer_.transport_version())) {
      ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
          QuicUtils::GetCryptoStreamId(client_framer_.transport_version()),
          data, kOffset, false, true, NOT_RETRANSMISSION, &frame));
      size_t bytes_consumed = frame.stream_frame.data_length;
      EXPECT_LT(0u, bytes_consumed);
    } else {
      producer_.SaveCryptoData(ENCRYPTION_INITIAL, kOffset, data);
      ASSERT_TRUE(creator_.ConsumeCryptoDataToFillCurrentPacket(
          ENCRYPTION_INITIAL, data.length(), kOffset,
          /*needs_full_padding=*/true, NOT_RETRANSMISSION, &frame));
      size_t bytes_consumed = frame.crypto_frame->data_length;
      EXPECT_LT(0u, bytes_consumed);
    }
    creator_.FlushCurrentPacket();
    ASSERT_TRUE(serialized_packet_->encrypted_buffer);
    // If there is not enough space in the packet to fit a padding frame
    // (1 byte) and to expand the stream frame (another 2 bytes) the packet
    // will not be padded.
    // Padding is skipped when we try to send coalesced packets.
    if (client_framer_.version().CanSendCoalescedPackets()) {
      EXPECT_EQ(kDefaultMaxPacketSize - bytes_free,
                serialized_packet_->encrypted_length);
    } else {
      EXPECT_EQ(kDefaultMaxPacketSize, serialized_packet_->encrypted_length);
    }
    DeleteSerializedPacket();
  }
}

TEST_P(QuicPacketCreatorTest, NonCryptoStreamFramePacketNonPadding) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  // Compute the total overhead for a single frame in packet.
  const size_t overhead =
      GetPacketHeaderOverhead(client_framer_.transport_version()) +
      GetEncryptionOverhead() +
      GetStreamFrameOverhead(client_framer_.transport_version());
  ASSERT_GT(kDefaultMaxPacketSize, overhead);
  size_t capacity = kDefaultMaxPacketSize - overhead;
  // Now, test various sizes around this size.
  for (int delta = -5; delta <= 5; ++delta) {
    std::string data(capacity + delta, 'A');
    size_t bytes_free = delta > 0 ? 0 : 0 - delta;

    QuicFrame frame;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
    ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
        GetNthClientInitiatedStreamId(1), data, kOffset, false, false,
        NOT_RETRANSMISSION, &frame));
    size_t bytes_consumed = frame.stream_frame.data_length;
    EXPECT_LT(0u, bytes_consumed);
    creator_.FlushCurrentPacket();
    ASSERT_TRUE(serialized_packet_->encrypted_buffer);
    if (bytes_free > 0) {
      EXPECT_EQ(kDefaultMaxPacketSize - bytes_free,
                serialized_packet_->encrypted_length);
    } else {
      EXPECT_EQ(kDefaultMaxPacketSize, serialized_packet_->encrypted_length);
    }
    DeleteSerializedPacket();
  }
}

// Test that the path challenge connectivity probing packet is serialized
// correctly as a padded PATH CHALLENGE packet.
TEST_P(QuicPacketCreatorTest, BuildPathChallengePacket) {
  if (!VersionHasIetfQuicFrames(creator_.transport_version())) {
    // This frame is only for IETF QUIC.
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = CreateTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  ::testing::NiceMock<MockRandom> randomizer;
  QuicPathFrameBuffer payload;
  randomizer.RandBytes(payload.data(), payload.size());

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // Path Challenge Frame type (IETF_PATH_CHALLENGE)
    0x1a,
    // 8 "random" bytes, MockRandom makes lots of r's
    'r', 'r', 'r', 'r', 'r', 'r', 'r', 'r',
    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  std::unique_ptr<char[]> buffer(new char[kMaxOutgoingPacketSize]);

  size_t length = creator_.BuildPaddedPathChallengePacket(
      header, buffer.get(), ABSL_ARRAYSIZE(packet), payload,
      ENCRYPTION_INITIAL);
  EXPECT_EQ(length, ABSL_ARRAYSIZE(packet));

  // Payload has the random bytes that were generated. Copy them into packet,
  // above, before checking that the generated packet is correct.
  EXPECT_EQ(kQuicPathFrameBufferSize, payload.size());

  QuicPacket data(creator_.transport_version(), buffer.release(), length, true,
                  header);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data.data(), data.length(),
      reinterpret_cast<char*>(packet), ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicPacketCreatorTest, BuildConnectivityProbingPacket) {
  QuicPacketHeader header;
  header.destination_connection_id = CreateTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type
    0x07,
    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_PING frame)
    0x01,
    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  unsigned char* p = packet;
  size_t packet_size = ABSL_ARRAYSIZE(packet);
  if (creator_.version().HasIetfQuicFrames()) {
    p = packet99;
    packet_size = ABSL_ARRAYSIZE(packet99);
  }

  std::unique_ptr<char[]> buffer(new char[kMaxOutgoingPacketSize]);

  size_t length = creator_.BuildConnectivityProbingPacket(
      header, buffer.get(), packet_size, ENCRYPTION_INITIAL);

  EXPECT_NE(0u, length);
  QuicPacket data(creator_.transport_version(), buffer.release(), length, true,
                  header);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data.data(), data.length(),
      reinterpret_cast<char*>(p), packet_size);
}

// Several tests that the path response connectivity probing packet is
// serialized correctly as either a padded and unpadded PATH RESPONSE
// packet. Also generates packets with 1 and 3 PATH_RESPONSES in them to
// exercised the single- and multiple- payload cases.
TEST_P(QuicPacketCreatorTest, BuildPathResponsePacket1ResponseUnpadded) {
  if (!VersionHasIetfQuicFrames(creator_.transport_version())) {
    // This frame is only for IETF QUIC.
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = CreateTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  QuicPathFrameBuffer payload0 = {
      {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}};

  // Build 1 PATH RESPONSE, not padded
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // Path Response Frame type (IETF_PATH_RESPONSE)
    0x1b,
    // 8 "random" bytes
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
  };
  // clang-format on
  std::unique_ptr<char[]> buffer(new char[kMaxOutgoingPacketSize]);
  quiche::QuicheCircularDeque<QuicPathFrameBuffer> payloads;
  payloads.push_back(payload0);
  size_t length = creator_.BuildPathResponsePacket(
      header, buffer.get(), ABSL_ARRAYSIZE(packet), payloads,
      /*is_padded=*/false, ENCRYPTION_INITIAL);
  EXPECT_EQ(length, ABSL_ARRAYSIZE(packet));
  QuicPacket data(creator_.transport_version(), buffer.release(), length, true,
                  header);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data.data(), data.length(),
      reinterpret_cast<char*>(packet), ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicPacketCreatorTest, BuildPathResponsePacket1ResponsePadded) {
  if (!VersionHasIetfQuicFrames(creator_.transport_version())) {
    // This frame is only for IETF QUIC.
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = CreateTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  QuicPathFrameBuffer payload0 = {
      {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}};

  // Build 1 PATH RESPONSE, padded
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // Path Response Frame type (IETF_PATH_RESPONSE)
    0x1b,
    // 8 "random" bytes
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    // Padding type and pad
    0x00, 0x00, 0x00, 0x00, 0x00
  };
  // clang-format on
  std::unique_ptr<char[]> buffer(new char[kMaxOutgoingPacketSize]);
  quiche::QuicheCircularDeque<QuicPathFrameBuffer> payloads;
  payloads.push_back(payload0);
  size_t length = creator_.BuildPathResponsePacket(
      header, buffer.get(), ABSL_ARRAYSIZE(packet), payloads,
      /*is_padded=*/true, ENCRYPTION_INITIAL);
  EXPECT_EQ(length, ABSL_ARRAYSIZE(packet));
  QuicPacket data(creator_.transport_version(), buffer.release(), length, true,
                  header);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data.data(), data.length(),
      reinterpret_cast<char*>(packet), ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicPacketCreatorTest, BuildPathResponsePacket3ResponsesUnpadded) {
  if (!VersionHasIetfQuicFrames(creator_.transport_version())) {
    // This frame is only for IETF QUIC.
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = CreateTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  QuicPathFrameBuffer payload0 = {
      {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}};
  QuicPathFrameBuffer payload1 = {
      {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18}};
  QuicPathFrameBuffer payload2 = {
      {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28}};

  // Build one packet with 3 PATH RESPONSES, no padding
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // 3 path response frames (IETF_PATH_RESPONSE type byte and payload)
    0x1b, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x1b, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x1b, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
  };
  // clang-format on

  std::unique_ptr<char[]> buffer(new char[kMaxOutgoingPacketSize]);
  quiche::QuicheCircularDeque<QuicPathFrameBuffer> payloads;
  payloads.push_back(payload0);
  payloads.push_back(payload1);
  payloads.push_back(payload2);
  size_t length = creator_.BuildPathResponsePacket(
      header, buffer.get(), ABSL_ARRAYSIZE(packet), payloads,
      /*is_padded=*/false, ENCRYPTION_INITIAL);
  EXPECT_EQ(length, ABSL_ARRAYSIZE(packet));
  QuicPacket data(creator_.transport_version(), buffer.release(), length, true,
                  header);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data.data(), data.length(),
      reinterpret_cast<char*>(packet), ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicPacketCreatorTest, BuildPathResponsePacket3ResponsesPadded) {
  if (!VersionHasIetfQuicFrames(creator_.transport_version())) {
    // This frame is only for IETF QUIC.
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = CreateTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  QuicPathFrameBuffer payload0 = {
      {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}};
  QuicPathFrameBuffer payload1 = {
      {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18}};
  QuicPathFrameBuffer payload2 = {
      {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28}};

  // Build one packet with 3 PATH RESPONSES, with padding
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // 3 path response frames (IETF_PATH_RESPONSE byte and payload)
    0x1b, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x1b, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x1b, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    // Padding
    0x00, 0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  std::unique_ptr<char[]> buffer(new char[kMaxOutgoingPacketSize]);
  quiche::QuicheCircularDeque<QuicPathFrameBuffer> payloads;
  payloads.push_back(payload0);
  payloads.push_back(payload1);
  payloads.push_back(payload2);
  size_t length = creator_.BuildPathResponsePacket(
      header, buffer.get(), ABSL_ARRAYSIZE(packet), payloads,
      /*is_padded=*/true, ENCRYPTION_INITIAL);
  EXPECT_EQ(length, ABSL_ARRAYSIZE(packet));
  QuicPacket data(creator_.transport_version(), buffer.release(), length, true,
                  header);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data.data(), data.length(),
      reinterpret_cast<char*>(packet), ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicPacketCreatorTest, SerializeConnectivityProbingPacket) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);

  std::unique_ptr<SerializedPacket> encrypted;
  if (VersionHasIetfQuicFrames(creator_.transport_version())) {
    QuicPathFrameBuffer payload = {
        {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xfe}};
    encrypted =
        creator_.SerializePathChallengeConnectivityProbingPacket(payload);
  } else {
    encrypted = creator_.SerializeConnectivityProbingPacket();
  }
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    if (VersionHasIetfQuicFrames(creator_.transport_version())) {
      EXPECT_CALL(framer_visitor_, OnPathChallengeFrame(_));
      EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
    } else {
      EXPECT_CALL(framer_visitor_, OnPingFrame(_));
      EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
    }
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  // QuicFramerPeer::SetPerspective(&client_framer_, Perspective::IS_SERVER);
  server_framer_.ProcessPacket(QuicEncryptedPacket(
      encrypted->encrypted_buffer, encrypted->encrypted_length));
}

TEST_P(QuicPacketCreatorTest, SerializePathChallengeProbePacket) {
  if (!VersionHasIetfQuicFrames(creator_.transport_version())) {
    return;
  }
  QuicPathFrameBuffer payload = {
      {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee}};

  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);

  std::unique_ptr<SerializedPacket> encrypted(
      creator_.SerializePathChallengeConnectivityProbingPacket(payload));
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnPathChallengeFrame(_));
    EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  // QuicFramerPeer::SetPerspective(&client_framer_, Perspective::IS_SERVER);
  server_framer_.ProcessPacket(QuicEncryptedPacket(
      encrypted->encrypted_buffer, encrypted->encrypted_length));
}

TEST_P(QuicPacketCreatorTest, SerializePathResponseProbePacket1PayloadPadded) {
  if (!VersionHasIetfQuicFrames(creator_.transport_version())) {
    return;
  }
  QuicPathFrameBuffer payload0 = {
      {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee}};

  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);

  quiche::QuicheCircularDeque<QuicPathFrameBuffer> payloads;
  payloads.push_back(payload0);

  std::unique_ptr<SerializedPacket> encrypted(
      creator_.SerializePathResponseConnectivityProbingPacket(payloads, true));
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnPathResponseFrame(_));
    EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  server_framer_.ProcessPacket(QuicEncryptedPacket(
      encrypted->encrypted_buffer, encrypted->encrypted_length));
}

TEST_P(QuicPacketCreatorTest,
       SerializePathResponseProbePacket1PayloadUnPadded) {
  if (!VersionHasIetfQuicFrames(creator_.transport_version())) {
    return;
  }
  QuicPathFrameBuffer payload0 = {
      {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee}};

  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);

  quiche::QuicheCircularDeque<QuicPathFrameBuffer> payloads;
  payloads.push_back(payload0);

  std::unique_ptr<SerializedPacket> encrypted(
      creator_.SerializePathResponseConnectivityProbingPacket(payloads, false));
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnPathResponseFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  server_framer_.ProcessPacket(QuicEncryptedPacket(
      encrypted->encrypted_buffer, encrypted->encrypted_length));
}

TEST_P(QuicPacketCreatorTest, SerializePathResponseProbePacket2PayloadsPadded) {
  if (!VersionHasIetfQuicFrames(creator_.transport_version())) {
    return;
  }
  QuicPathFrameBuffer payload0 = {
      {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee}};
  QuicPathFrameBuffer payload1 = {
      {0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee, 0xde}};

  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);

  quiche::QuicheCircularDeque<QuicPathFrameBuffer> payloads;
  payloads.push_back(payload0);
  payloads.push_back(payload1);

  std::unique_ptr<SerializedPacket> encrypted(
      creator_.SerializePathResponseConnectivityProbingPacket(payloads, true));
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnPathResponseFrame(_)).Times(2);
    EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  server_framer_.ProcessPacket(QuicEncryptedPacket(
      encrypted->encrypted_buffer, encrypted->encrypted_length));
}

TEST_P(QuicPacketCreatorTest,
       SerializePathResponseProbePacket2PayloadsUnPadded) {
  if (!VersionHasIetfQuicFrames(creator_.transport_version())) {
    return;
  }
  QuicPathFrameBuffer payload0 = {
      {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee}};
  QuicPathFrameBuffer payload1 = {
      {0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee, 0xde}};

  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);

  quiche::QuicheCircularDeque<QuicPathFrameBuffer> payloads;
  payloads.push_back(payload0);
  payloads.push_back(payload1);

  std::unique_ptr<SerializedPacket> encrypted(
      creator_.SerializePathResponseConnectivityProbingPacket(payloads, false));
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnPathResponseFrame(_)).Times(2);
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  server_framer_.ProcessPacket(QuicEncryptedPacket(
      encrypted->encrypted_buffer, encrypted->encrypted_length));
}

TEST_P(QuicPacketCreatorTest, SerializePathResponseProbePacket3PayloadsPadded) {
  if (!VersionHasIetfQuicFrames(creator_.transport_version())) {
    return;
  }
  QuicPathFrameBuffer payload0 = {
      {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee}};
  QuicPathFrameBuffer payload1 = {
      {0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee, 0xde}};
  QuicPathFrameBuffer payload2 = {
      {0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee, 0xde, 0xad}};

  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);

  quiche::QuicheCircularDeque<QuicPathFrameBuffer> payloads;
  payloads.push_back(payload0);
  payloads.push_back(payload1);
  payloads.push_back(payload2);

  std::unique_ptr<SerializedPacket> encrypted(
      creator_.SerializePathResponseConnectivityProbingPacket(payloads, true));
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnPathResponseFrame(_)).Times(3);
    EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  server_framer_.ProcessPacket(QuicEncryptedPacket(
      encrypted->encrypted_buffer, encrypted->encrypted_length));
}

TEST_P(QuicPacketCreatorTest,
       SerializePathResponseProbePacket3PayloadsUnpadded) {
  if (!VersionHasIetfQuicFrames(creator_.transport_version())) {
    return;
  }
  QuicPathFrameBuffer payload0 = {
      {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee}};
  QuicPathFrameBuffer payload1 = {
      {0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee, 0xde}};
  QuicPathFrameBuffer payload2 = {
      {0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee, 0xde, 0xad}};

  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);

  quiche::QuicheCircularDeque<QuicPathFrameBuffer> payloads;
  payloads.push_back(payload0);
  payloads.push_back(payload1);
  payloads.push_back(payload2);

  std::unique_ptr<SerializedPacket> encrypted(
      creator_.SerializePathResponseConnectivityProbingPacket(payloads, false));
  InSequence s;
  EXPECT_CALL(framer_visitor_, OnPacket());
  EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
  EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
  EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnPathResponseFrame(_)).Times(3);
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  server_framer_.ProcessPacket(QuicEncryptedPacket(
      encrypted->encrypted_buffer, encrypted->encrypted_length));
}

TEST_P(QuicPacketCreatorTest, SerializeLargePacketNumberConnectionClosePacket) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  std::unique_ptr<SerializedPacket> encrypted(
      creator_.SerializeLargePacketNumberConnectionClosePacket(
          QuicPacketNumber(1), QUIC_CLIENT_LOST_NETWORK_ACCESS,
          "QuicPacketCreatorTest"));

  InSequence s;
  EXPECT_CALL(framer_visitor_, OnPacket());
  EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
  EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
  EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnConnectionCloseFrame(_));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  server_framer_.ProcessPacket(QuicEncryptedPacket(
      encrypted->encrypted_buffer, encrypted->encrypted_length));
}

TEST_P(QuicPacketCreatorTest, UpdatePacketSequenceNumberLengthLeastAwaiting) {
  if (!GetParam().version.SendsVariableLengthPacketNumberInLongHeader()) {
    EXPECT_EQ(PACKET_4BYTE_PACKET_NUMBER,
              QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));
    creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  } else {
    EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER,
              QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));
  }

  QuicPacketCreatorPeer::SetPacketNumber(&creator_, 64);
  creator_.UpdatePacketNumberLength(QuicPacketNumber(2),
                                    10000 / kDefaultMaxPacketSize);
  EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));

  QuicPacketCreatorPeer::SetPacketNumber(&creator_, 64 * 256);
  creator_.UpdatePacketNumberLength(QuicPacketNumber(2),
                                    10000 / kDefaultMaxPacketSize);
  EXPECT_EQ(PACKET_2BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));

  QuicPacketCreatorPeer::SetPacketNumber(&creator_, 64 * 256 * 256);
  creator_.UpdatePacketNumberLength(QuicPacketNumber(2),
                                    10000 / kDefaultMaxPacketSize);
  EXPECT_EQ(PACKET_4BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));

  QuicPacketCreatorPeer::SetPacketNumber(&creator_,
                                         UINT64_C(64) * 256 * 256 * 256 * 256);
  creator_.UpdatePacketNumberLength(QuicPacketNumber(2),
                                    10000 / kDefaultMaxPacketSize);
  EXPECT_EQ(PACKET_6BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));
}

TEST_P(QuicPacketCreatorTest, UpdatePacketSequenceNumberLengthCwnd) {
  QuicPacketCreatorPeer::SetPacketNumber(&creator_, 1);
  if (!GetParam().version.SendsVariableLengthPacketNumberInLongHeader()) {
    EXPECT_EQ(PACKET_4BYTE_PACKET_NUMBER,
              QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));
    creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  } else {
    EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER,
              QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));
  }

  creator_.UpdatePacketNumberLength(QuicPacketNumber(1),
                                    10000 / kDefaultMaxPacketSize);
  EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));

  creator_.UpdatePacketNumberLength(QuicPacketNumber(1),
                                    10000 * 256 / kDefaultMaxPacketSize);
  EXPECT_EQ(PACKET_2BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));

  creator_.UpdatePacketNumberLength(QuicPacketNumber(1),
                                    10000 * 256 * 256 / kDefaultMaxPacketSize);
  EXPECT_EQ(PACKET_4BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));

  creator_.UpdatePacketNumberLength(
      QuicPacketNumber(1),
      UINT64_C(1000) * 256 * 256 * 256 * 256 / kDefaultMaxPacketSize);
  EXPECT_EQ(PACKET_6BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));
}

TEST_P(QuicPacketCreatorTest, SkipNPacketNumbers) {
  QuicPacketCreatorPeer::SetPacketNumber(&creator_, 1);
  if (!GetParam().version.SendsVariableLengthPacketNumberInLongHeader()) {
    EXPECT_EQ(PACKET_4BYTE_PACKET_NUMBER,
              QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));
    creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  } else {
    EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER,
              QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));
  }
  creator_.SkipNPacketNumbers(63, QuicPacketNumber(2),
                              10000 / kDefaultMaxPacketSize);
  EXPECT_EQ(QuicPacketNumber(64), creator_.packet_number());
  EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));

  creator_.SkipNPacketNumbers(64 * 255, QuicPacketNumber(2),
                              10000 / kDefaultMaxPacketSize);
  EXPECT_EQ(QuicPacketNumber(64 * 256), creator_.packet_number());
  EXPECT_EQ(PACKET_2BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));

  creator_.SkipNPacketNumbers(64 * 256 * 255, QuicPacketNumber(2),
                              10000 / kDefaultMaxPacketSize);
  EXPECT_EQ(QuicPacketNumber(64 * 256 * 256), creator_.packet_number());
  EXPECT_EQ(PACKET_4BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));
}

TEST_P(QuicPacketCreatorTest, SerializeFrame) {
  if (!GetParam().version_serialization) {
    creator_.StopSendingVersion();
  }
  std::string data("test data");
  if (!QuicVersionUsesCryptoFrames(client_framer_.transport_version())) {
    QuicStreamFrame stream_frame(
        QuicUtils::GetCryptoStreamId(client_framer_.transport_version()),
        /*fin=*/false, 0u, absl::string_view());
    frames_.push_back(QuicFrame(stream_frame));
  } else {
    producer_.SaveCryptoData(ENCRYPTION_INITIAL, 0, data);
    frames_.push_back(
        QuicFrame(new QuicCryptoFrame(ENCRYPTION_INITIAL, 0, data.length())));
  }
  SerializedPacket serialized = SerializeAllFrames(frames_);

  QuicPacketHeader header;
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_))
        .WillOnce(DoAll(SaveArg<0>(&header), Return(true)));
    if (QuicVersionUsesCryptoFrames(client_framer_.transport_version())) {
      EXPECT_CALL(framer_visitor_, OnCryptoFrame(_));
    } else {
      EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    }
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized);
  EXPECT_EQ(GetParam().version_serialization, header.version_flag);
}

TEST_P(QuicPacketCreatorTest, SerializeFrameShortData) {
  if (!GetParam().version_serialization) {
    creator_.StopSendingVersion();
  }
  std::string data("Hello World!");
  if (!QuicVersionUsesCryptoFrames(client_framer_.transport_version())) {
    QuicStreamFrame stream_frame(
        QuicUtils::GetCryptoStreamId(client_framer_.transport_version()),
        /*fin=*/false, 0u, absl::string_view());
    frames_.push_back(QuicFrame(stream_frame));
  } else {
    producer_.SaveCryptoData(ENCRYPTION_INITIAL, 0, data);
    frames_.push_back(
        QuicFrame(new QuicCryptoFrame(ENCRYPTION_INITIAL, 0, data.length())));
  }
  SerializedPacket serialized = SerializeAllFrames(frames_);

  QuicPacketHeader header;
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_))
        .WillOnce(DoAll(SaveArg<0>(&header), Return(true)));
    if (QuicVersionUsesCryptoFrames(client_framer_.transport_version())) {
      EXPECT_CALL(framer_visitor_, OnCryptoFrame(_));
    } else {
      EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    }
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized);
  EXPECT_EQ(GetParam().version_serialization, header.version_flag);
}

void QuicPacketCreatorTest::TestChaosProtection(bool enabled) {
  if (!GetParam().version.UsesCryptoFrames()) {
    return;
  }
  ::testing::NiceMock<MockRandom> mock_random(2);
  QuicPacketCreatorPeer::SetRandom(&creator_, &mock_random);
  std::string data("ChAoS_ThEoRy!");
  producer_.SaveCryptoData(ENCRYPTION_INITIAL, 0, data);
  frames_.push_back(
      QuicFrame(new QuicCryptoFrame(ENCRYPTION_INITIAL, 0, data.length())));
  frames_.push_back(QuicFrame(QuicPaddingFrame(33)));
  SerializedPacket serialized = SerializeAllFrames(frames_);
  EXPECT_CALL(framer_visitor_, OnPacket());
  EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
  EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
  EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  if (enabled) {
    EXPECT_CALL(framer_visitor_, OnCryptoFrame(_)).Times(AtLeast(3));
    EXPECT_CALL(framer_visitor_, OnPaddingFrame(_)).Times(AtLeast(2));
    EXPECT_CALL(framer_visitor_, OnPingFrame(_)).Times(AtLeast(2));
  } else {
    EXPECT_CALL(framer_visitor_, OnCryptoFrame(_)).Times(1);
    EXPECT_CALL(framer_visitor_, OnPaddingFrame(_)).Times(1);
    EXPECT_CALL(framer_visitor_, OnPingFrame(_)).Times(0);
  }
  EXPECT_CALL(framer_visitor_, OnPacketComplete());
  ProcessPacket(serialized);
}

TEST_P(QuicPacketCreatorTest, ChaosProtectionEnabled) {
  TestChaosProtection(true);
}

TEST_P(QuicPacketCreatorTest, ChaosProtectionDisabled) {
  SetQuicFlag(quic_enable_chaos_protection, false);
  TestChaosProtection(false);
}

TEST_P(QuicPacketCreatorTest, ConsumeDataLargerThanOneStreamFrame) {
  if (!GetParam().version_serialization) {
    creator_.StopSendingVersion();
  }
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  // A string larger than fits into a frame.
  QuicFrame frame;
  size_t payload_length = creator_.max_packet_length();
  const std::string too_long_payload(payload_length, 'a');
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      client_framer_.transport_version(), Perspective::IS_CLIENT);
  ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
      stream_id, too_long_payload, 0u, true, false, NOT_RETRANSMISSION,
      &frame));
  size_t consumed = frame.stream_frame.data_length;
  // The entire payload could not be consumed.
  EXPECT_GT(payload_length, consumed);
  creator_.FlushCurrentPacket();
  DeleteSerializedPacket();
}

TEST_P(QuicPacketCreatorTest, AddFrameAndFlush) {
  if (!GetParam().version_serialization) {
    creator_.StopSendingVersion();
  }
  const size_t max_plaintext_size =
      client_framer_.GetMaxPlaintextSize(creator_.max_packet_length());
  EXPECT_FALSE(creator_.HasPendingFrames());
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      client_framer_.transport_version(), Perspective::IS_CLIENT);
  if (!QuicVersionUsesCryptoFrames(client_framer_.transport_version())) {
    stream_id =
        QuicUtils::GetCryptoStreamId(client_framer_.transport_version());
  }
  EXPECT_FALSE(creator_.HasPendingStreamFramesOfStream(stream_id));
  EXPECT_EQ(max_plaintext_size -
                GetPacketHeaderSize(
                    client_framer_.transport_version(),
                    creator_.GetDestinationConnectionIdLength(),
                    creator_.GetSourceConnectionIdLength(),
                    QuicPacketCreatorPeer::SendVersionInPacket(&creator_),
                    !kIncludeDiversificationNonce,
                    QuicPacketCreatorPeer::GetPacketNumberLength(&creator_),
                    QuicPacketCreatorPeer::GetRetryTokenLengthLength(&creator_),
                    0, QuicPacketCreatorPeer::GetLengthLength(&creator_)),
            creator_.BytesFree());
  StrictMock<MockDebugDelegate> debug;
  creator_.set_debug_delegate(&debug);

  // Add a variety of frame types and then a padding frame.
  QuicAckFrame ack_frame(InitAckFrame(10u));
  EXPECT_CALL(debug, OnFrameAddedToPacket(_));
  EXPECT_TRUE(creator_.AddFrame(QuicFrame(&ack_frame), NOT_RETRANSMISSION));
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingStreamFramesOfStream(stream_id));

  QuicFrame frame;
  const std::string data("test");
  EXPECT_CALL(debug, OnFrameAddedToPacket(_));
  ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
      stream_id, data, 0u, false, false, NOT_RETRANSMISSION, &frame));
  size_t consumed = frame.stream_frame.data_length;
  EXPECT_EQ(4u, consumed);
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_TRUE(creator_.HasPendingStreamFramesOfStream(stream_id));

  QuicPaddingFrame padding_frame;
  EXPECT_CALL(debug, OnFrameAddedToPacket(_));
  EXPECT_TRUE(creator_.AddFrame(QuicFrame(padding_frame), NOT_RETRANSMISSION));
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_EQ(0u, creator_.BytesFree());

  // Packet is full. Creator will flush.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  EXPECT_FALSE(creator_.AddFrame(QuicFrame(&ack_frame), NOT_RETRANSMISSION));

  // Ensure the packet is successfully created.
  ASSERT_TRUE(serialized_packet_->encrypted_buffer);
  ASSERT_FALSE(serialized_packet_->retransmittable_frames.empty());
  const QuicFrames& retransmittable =
      serialized_packet_->retransmittable_frames;
  ASSERT_EQ(1u, retransmittable.size());
  EXPECT_EQ(STREAM_FRAME, retransmittable[0].type);
  EXPECT_TRUE(serialized_packet_->has_ack);
  EXPECT_EQ(QuicPacketNumber(10u), serialized_packet_->largest_acked);
  DeleteSerializedPacket();

  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingStreamFramesOfStream(stream_id));
  EXPECT_EQ(max_plaintext_size -
                GetPacketHeaderSize(
                    client_framer_.transport_version(),
                    creator_.GetDestinationConnectionIdLength(),
                    creator_.GetSourceConnectionIdLength(),
                    QuicPacketCreatorPeer::SendVersionInPacket(&creator_),
                    !kIncludeDiversificationNonce,
                    QuicPacketCreatorPeer::GetPacketNumberLength(&creator_),
                    QuicPacketCreatorPeer::GetRetryTokenLengthLength(&creator_),
                    0, QuicPacketCreatorPeer::GetLengthLength(&creator_)),
            creator_.BytesFree());
}

TEST_P(QuicPacketCreatorTest, SerializeAndSendStreamFrame) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  if (!GetParam().version_serialization) {
    creator_.StopSendingVersion();
  }
  EXPECT_FALSE(creator_.HasPendingFrames());

  const std::string data("test");
  producer_.SaveStreamData(GetNthClientInitiatedStreamId(0), data);
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  size_t num_bytes_consumed;
  StrictMock<MockDebugDelegate> debug;
  creator_.set_debug_delegate(&debug);
  EXPECT_CALL(debug, OnFrameAddedToPacket(_));
  creator_.CreateAndSerializeStreamFrame(
      GetNthClientInitiatedStreamId(0), data.length(), 0, 0, true,
      NOT_RETRANSMISSION, &num_bytes_consumed);
  EXPECT_EQ(4u, num_bytes_consumed);

  // Ensure the packet is successfully created.
  ASSERT_TRUE(serialized_packet_->encrypted_buffer);
  ASSERT_FALSE(serialized_packet_->retransmittable_frames.empty());
  const QuicFrames& retransmittable =
      serialized_packet_->retransmittable_frames;
  ASSERT_EQ(1u, retransmittable.size());
  EXPECT_EQ(STREAM_FRAME, retransmittable[0].type);
  DeleteSerializedPacket();

  EXPECT_FALSE(creator_.HasPendingFrames());
}

TEST_P(QuicPacketCreatorTest, SerializeStreamFrameWithPadding) {
  // Regression test to check that CreateAndSerializeStreamFrame uses a
  // correctly formatted stream frame header when appending padding.

  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  if (!GetParam().version_serialization) {
    creator_.StopSendingVersion();
  }
  EXPECT_FALSE(creator_.HasPendingFrames());

  // Send zero bytes of stream data. This requires padding.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  size_t num_bytes_consumed;
  creator_.CreateAndSerializeStreamFrame(GetNthClientInitiatedStreamId(0), 0, 0,
                                         0, true, NOT_RETRANSMISSION,
                                         &num_bytes_consumed);
  EXPECT_EQ(0u, num_bytes_consumed);

  // Check that a packet is created.
  ASSERT_TRUE(serialized_packet_->encrypted_buffer);
  ASSERT_FALSE(serialized_packet_->retransmittable_frames.empty());
  ASSERT_EQ(serialized_packet_->packet_number_length,
            PACKET_1BYTE_PACKET_NUMBER);
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    if (client_framer_.version().HasHeaderProtection()) {
      EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
      EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    } else {
      EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    }
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(*serialized_packet_);
}

TEST_P(QuicPacketCreatorTest, AddUnencryptedStreamDataClosesConnection) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (!IsDefaultTestConfiguration()) {
    return;
  }

  creator_.set_encryption_level(ENCRYPTION_INITIAL);
  QuicStreamFrame stream_frame(GetNthClientInitiatedStreamId(0),
                               /*fin=*/false, 0u, absl::string_view());
  EXPECT_QUIC_BUG(
      {
        EXPECT_CALL(delegate_, OnUnrecoverableError(_, _));
        creator_.AddFrame(QuicFrame(stream_frame), NOT_RETRANSMISSION);
      },
      "Cannot send stream data with level: ENCRYPTION_INITIAL");
}

TEST_P(QuicPacketCreatorTest, SendStreamDataWithEncryptionHandshake) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (!IsDefaultTestConfiguration()) {
    return;
  }

  creator_.set_encryption_level(ENCRYPTION_HANDSHAKE);
  QuicStreamFrame stream_frame(GetNthClientInitiatedStreamId(0),
                               /*fin=*/false, 0u, absl::string_view());
  EXPECT_QUIC_BUG(
      {
        EXPECT_CALL(delegate_, OnUnrecoverableError(_, _));
        creator_.AddFrame(QuicFrame(stream_frame), NOT_RETRANSMISSION);
      },
      "Cannot send stream data with level: ENCRYPTION_HANDSHAKE");
}

TEST_P(QuicPacketCreatorTest, ChloTooLarge) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (!IsDefaultTestConfiguration()) {
    return;
  }

  // This test only matters when the crypto handshake is sent in stream frames.
  // TODO(b/128596274): Re-enable when this check is supported for CRYPTO
  // frames.
  if (QuicVersionUsesCryptoFrames(client_framer_.transport_version())) {
    return;
  }

  CryptoHandshakeMessage message;
  message.set_tag(kCHLO);
  message.set_minimum_size(kMaxOutgoingPacketSize);
  CryptoFramer framer;
  std::unique_ptr<QuicData> message_data;
  message_data = framer.ConstructHandshakeMessage(message);

  QuicFrame frame;
  EXPECT_CALL(delegate_, OnUnrecoverableError(QUIC_CRYPTO_CHLO_TOO_LARGE, _));
  EXPECT_QUIC_BUG(
      creator_.ConsumeDataToFillCurrentPacket(
          QuicUtils::GetCryptoStreamId(client_framer_.transport_version()),
          absl::string_view(message_data->data(), message_data->length()), 0u,
          false, false, NOT_RETRANSMISSION, &frame),
      "Client hello won't fit in a single packet.");
}

TEST_P(QuicPacketCreatorTest, PendingPadding) {
  EXPECT_EQ(0u, creator_.pending_padding_bytes());
  creator_.AddPendingPadding(kMaxNumRandomPaddingBytes * 10);
  EXPECT_EQ(kMaxNumRandomPaddingBytes * 10, creator_.pending_padding_bytes());

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillRepeatedly(
          Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  // Flush all paddings.
  while (creator_.pending_padding_bytes() > 0) {
    creator_.FlushCurrentPacket();
    {
      InSequence s;
      EXPECT_CALL(framer_visitor_, OnPacket());
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
      EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
      EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
      EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
      EXPECT_CALL(framer_visitor_, OnPacketComplete());
    }
    // Packet only contains padding.
    ProcessPacket(*serialized_packet_);
  }
  EXPECT_EQ(0u, creator_.pending_padding_bytes());
}

TEST_P(QuicPacketCreatorTest, FullPaddingDoesNotConsumePendingPadding) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  creator_.AddPendingPadding(kMaxNumRandomPaddingBytes);
  QuicFrame frame;
  QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      client_framer_.transport_version(), Perspective::IS_CLIENT);
  const std::string data("test");
  ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
      stream_id, data, 0u, false,
      /*needs_full_padding=*/true, NOT_RETRANSMISSION, &frame));
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  creator_.FlushCurrentPacket();
  EXPECT_EQ(kMaxNumRandomPaddingBytes, creator_.pending_padding_bytes());
}

TEST_P(QuicPacketCreatorTest, ConsumeDataAndRandomPadding) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  const QuicByteCount kStreamFramePayloadSize = 100u;
  // Set the packet size be enough for one stream frame with 0 stream offset +
  // 1.
  QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      client_framer_.transport_version(), Perspective::IS_CLIENT);
  size_t length =
      GetPacketHeaderOverhead(client_framer_.transport_version()) +
      GetEncryptionOverhead() +
      QuicFramer::GetMinStreamFrameSize(
          client_framer_.transport_version(), stream_id, 0,
          /*last_frame_in_packet=*/true, kStreamFramePayloadSize + 1) +
      kStreamFramePayloadSize + 1;
  creator_.SetMaxPacketLength(length);
  creator_.AddPendingPadding(kMaxNumRandomPaddingBytes);
  QuicByteCount pending_padding_bytes = creator_.pending_padding_bytes();
  QuicFrame frame;
  char buf[kStreamFramePayloadSize + 1] = {};
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillRepeatedly(
          Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  // Send stream frame of size kStreamFramePayloadSize.
  creator_.ConsumeDataToFillCurrentPacket(
      stream_id, absl::string_view(buf, kStreamFramePayloadSize), 0u, false,
      false, NOT_RETRANSMISSION, &frame);
  creator_.FlushCurrentPacket();
  // 1 byte padding is sent.
  EXPECT_EQ(pending_padding_bytes - 1, creator_.pending_padding_bytes());
  // Send stream frame of size kStreamFramePayloadSize + 1.
  creator_.ConsumeDataToFillCurrentPacket(
      stream_id, absl::string_view(buf, kStreamFramePayloadSize + 1),
      kStreamFramePayloadSize, false, false, NOT_RETRANSMISSION, &frame);
  // No padding is sent.
  creator_.FlushCurrentPacket();
  EXPECT_EQ(pending_padding_bytes - 1, creator_.pending_padding_bytes());
  // Flush all paddings.
  while (creator_.pending_padding_bytes() > 0) {
    creator_.FlushCurrentPacket();
  }
  EXPECT_EQ(0u, creator_.pending_padding_bytes());
}

TEST_P(QuicPacketCreatorTest, FlushWithExternalBuffer) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  char* buffer = new char[kMaxOutgoingPacketSize];
  QuicPacketBuffer external_buffer = {buffer,
                                      [](const char* p) { delete[] p; }};
  EXPECT_CALL(delegate_, GetPacketBuffer()).WillOnce(Return(external_buffer));

  QuicFrame frame;
  QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      client_framer_.transport_version(), Perspective::IS_CLIENT);
  const std::string data("test");
  ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
      stream_id, data, 0u, false,
      /*needs_full_padding=*/true, NOT_RETRANSMISSION, &frame));

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke([&external_buffer](SerializedPacket serialized_packet) {
        EXPECT_EQ(external_buffer.buffer, serialized_packet.encrypted_buffer);
      }));
  creator_.FlushCurrentPacket();
}

// Test for error found in
// https://bugs.chromium.org/p/chromium/issues/detail?id=859949 where a gap
// length that crosses an IETF VarInt length boundary would cause a
// failure. While this test is not applicable to versions other than version 99,
// it should still work. Hence, it is not made version-specific.
TEST_P(QuicPacketCreatorTest, IetfAckGapErrorRegression) {
  QuicAckFrame ack_frame =
      InitAckFrame({{QuicPacketNumber(60), QuicPacketNumber(61)},
                    {QuicPacketNumber(125), QuicPacketNumber(126)}});
  frames_.push_back(QuicFrame(&ack_frame));
  SerializeAllFrames(frames_);
}

TEST_P(QuicPacketCreatorTest, AddMessageFrame) {
  if (client_framer_.version().UsesTls()) {
    creator_.SetMaxDatagramFrameSize(kMaxAcceptedDatagramFrameSize);
  }
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .Times(3)
      .WillRepeatedly(
          Invoke(this, &QuicPacketCreatorTest::ClearSerializedPacketForTests));
  // Verify that there is enough room for the largest message payload.
  EXPECT_TRUE(creator_.HasRoomForMessageFrame(
      creator_.GetCurrentLargestMessagePayload()));
  std::string large_message(creator_.GetCurrentLargestMessagePayload(), 'a');
  QuicMessageFrame* message_frame =
      new QuicMessageFrame(1, MemSliceFromString(large_message));
  EXPECT_TRUE(creator_.AddFrame(QuicFrame(message_frame), NOT_RETRANSMISSION));
  EXPECT_TRUE(creator_.HasPendingFrames());
  creator_.FlushCurrentPacket();

  QuicMessageFrame* frame2 =
      new QuicMessageFrame(2, MemSliceFromString("message"));
  EXPECT_TRUE(creator_.AddFrame(QuicFrame(frame2), NOT_RETRANSMISSION));
  EXPECT_TRUE(creator_.HasPendingFrames());
  // Verify if a new frame is added, 1 byte message length will be added.
  EXPECT_EQ(1u, creator_.ExpansionOnNewFrame());
  QuicMessageFrame* frame3 =
      new QuicMessageFrame(3, MemSliceFromString("message2"));
  EXPECT_TRUE(creator_.AddFrame(QuicFrame(frame3), NOT_RETRANSMISSION));
  EXPECT_EQ(1u, creator_.ExpansionOnNewFrame());
  creator_.FlushCurrentPacket();

  QuicFrame frame;
  QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      client_framer_.transport_version(), Perspective::IS_CLIENT);
  const std::string data("test");
  EXPECT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
      stream_id, data, 0u, false, false, NOT_RETRANSMISSION, &frame));
  QuicMessageFrame* frame4 =
      new QuicMessageFrame(4, MemSliceFromString("message"));
  EXPECT_TRUE(creator_.AddFrame(QuicFrame(frame4), NOT_RETRANSMISSION));
  EXPECT_TRUE(creator_.HasPendingFrames());
  // Verify there is not enough room for largest payload.
  EXPECT_FALSE(creator_.HasRoomForMessageFrame(
      creator_.GetCurrentLargestMessagePayload()));
  // Add largest message will causes the flush of the stream frame.
  QuicMessageFrame frame5(5, MemSliceFromString(large_message));
  EXPECT_FALSE(creator_.AddFrame(QuicFrame(&frame5), NOT_RETRANSMISSION));
  EXPECT_FALSE(creator_.HasPendingFrames());
}

TEST_P(QuicPacketCreatorTest, MessageFrameConsumption) {
  if (client_framer_.version().UsesTls()) {
    creator_.SetMaxDatagramFrameSize(kMaxAcceptedDatagramFrameSize);
  }
  std::string message_data(kDefaultMaxPacketSize, 'a');
  // Test all possible encryption levels of message frames.
  for (EncryptionLevel level :
       {ENCRYPTION_ZERO_RTT, ENCRYPTION_FORWARD_SECURE}) {
    creator_.set_encryption_level(level);
    // Test all possible sizes of message frames.
    for (size_t message_size = 0;
         message_size <= creator_.GetCurrentLargestMessagePayload();
         ++message_size) {
      QuicMessageFrame* frame =
          new QuicMessageFrame(0, MemSliceFromString(absl::string_view(
                                      message_data.data(), message_size)));
      EXPECT_TRUE(creator_.AddFrame(QuicFrame(frame), NOT_RETRANSMISSION));
      EXPECT_TRUE(creator_.HasPendingFrames());

      size_t expansion_bytes = message_size >= 64 ? 2 : 1;
      EXPECT_EQ(expansion_bytes, creator_.ExpansionOnNewFrame());
      // Verify BytesFree returns bytes available for the next frame, which
      // should subtract the message length.
      size_t expected_bytes_free =
          creator_.GetCurrentLargestMessagePayload() - message_size <
                  expansion_bytes
              ? 0
              : creator_.GetCurrentLargestMessagePayload() - expansion_bytes -
                    message_size;
      EXPECT_EQ(expected_bytes_free, creator_.BytesFree());
      EXPECT_LE(creator_.GetGuaranteedLargestMessagePayload(),
                creator_.GetCurrentLargestMessagePayload());
      EXPECT_CALL(delegate_, OnSerializedPacket(_))
          .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
      creator_.FlushCurrentPacket();
      ASSERT_TRUE(serialized_packet_->encrypted_buffer);
      DeleteSerializedPacket();
    }
  }
}

TEST_P(QuicPacketCreatorTest, GetGuaranteedLargestMessagePayload) {
  ParsedQuicVersion version = GetParam().version;
  if (version.UsesTls()) {
    creator_.SetMaxDatagramFrameSize(kMaxAcceptedDatagramFrameSize);
  }
  QuicPacketLength expected_largest_payload = 1215;
  if (version.HasLongHeaderLengths()) {
    expected_largest_payload -= 2;
  }
  if (version.HasLengthPrefixedConnectionIds()) {
    expected_largest_payload -= 1;
  }
  EXPECT_EQ(expected_largest_payload,
            creator_.GetGuaranteedLargestMessagePayload());
  EXPECT_TRUE(creator_.HasRoomForMessageFrame(
      creator_.GetGuaranteedLargestMessagePayload()));

  // Now test whether SetMaxDatagramFrameSize works.
  creator_.SetMaxDatagramFrameSize(expected_largest_payload + 1 +
                                   kQuicFrameTypeSize);
  EXPECT_EQ(expected_largest_payload,
            creator_.GetGuaranteedLargestMessagePayload());
  EXPECT_TRUE(creator_.HasRoomForMessageFrame(
      creator_.GetGuaranteedLargestMessagePayload()));

  creator_.SetMaxDatagramFrameSize(expected_largest_payload +
                                   kQuicFrameTypeSize);
  EXPECT_EQ(expected_largest_payload,
            creator_.GetGuaranteedLargestMessagePayload());
  EXPECT_TRUE(creator_.HasRoomForMessageFrame(
      creator_.GetGuaranteedLargestMessagePayload()));

  creator_.SetMaxDatagramFrameSize(expected_largest_payload - 1 +
                                   kQuicFrameTypeSize);
  EXPECT_EQ(expected_largest_payload - 1,
            creator_.GetGuaranteedLargestMessagePayload());
  EXPECT_TRUE(creator_.HasRoomForMessageFrame(
      creator_.GetGuaranteedLargestMessagePayload()));

  constexpr QuicPacketLength kFrameSizeLimit = 1000;
  constexpr QuicPacketLength kPayloadSizeLimit =
      kFrameSizeLimit - kQuicFrameTypeSize;
  creator_.SetMaxDatagramFrameSize(kFrameSizeLimit);
  EXPECT_EQ(creator_.GetGuaranteedLargestMessagePayload(), kPayloadSizeLimit);
  EXPECT_TRUE(creator_.HasRoomForMessageFrame(kPayloadSizeLimit));
  EXPECT_FALSE(creator_.HasRoomForMessageFrame(kPayloadSizeLimit + 1));
}

TEST_P(QuicPacketCreatorTest, GetCurrentLargestMessagePayload) {
  ParsedQuicVersion version = GetParam().version;
  if (version.UsesTls()) {
    creator_.SetMaxDatagramFrameSize(kMaxAcceptedDatagramFrameSize);
  }
  QuicPacketLength expected_largest_payload = 1215;
  if (version.SendsVariableLengthPacketNumberInLongHeader()) {
    expected_largest_payload += 3;
  }
  if (version.HasLongHeaderLengths()) {
    expected_largest_payload -= 2;
  }
  if (version.HasLengthPrefixedConnectionIds()) {
    expected_largest_payload -= 1;
  }
  EXPECT_EQ(expected_largest_payload,
            creator_.GetCurrentLargestMessagePayload());

  // Now test whether SetMaxDatagramFrameSize works.
  creator_.SetMaxDatagramFrameSize(expected_largest_payload + 1 +
                                   kQuicFrameTypeSize);
  EXPECT_EQ(expected_largest_payload,
            creator_.GetCurrentLargestMessagePayload());

  creator_.SetMaxDatagramFrameSize(expected_largest_payload +
                                   kQuicFrameTypeSize);
  EXPECT_EQ(expected_largest_payload,
            creator_.GetCurrentLargestMessagePayload());

  creator_.SetMaxDatagramFrameSize(expected_largest_payload - 1 +
                                   kQuicFrameTypeSize);
  EXPECT_EQ(expected_largest_payload - 1,
            creator_.GetCurrentLargestMessagePayload());
}

TEST_P(QuicPacketCreatorTest, PacketTransmissionType) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);

  QuicAckFrame temp_ack_frame = InitAckFrame(1);
  QuicFrame ack_frame(&temp_ack_frame);
  ASSERT_FALSE(QuicUtils::IsRetransmittableFrame(ack_frame.type));

  QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      client_framer_.transport_version(), Perspective::IS_CLIENT);
  QuicFrame stream_frame(QuicStreamFrame(stream_id,
                                         /*fin=*/false, 0u,
                                         absl::string_view()));
  ASSERT_TRUE(QuicUtils::IsRetransmittableFrame(stream_frame.type));

  QuicFrame stream_frame_2(QuicStreamFrame(stream_id,
                                           /*fin=*/false, 1u,
                                           absl::string_view()));

  QuicFrame padding_frame{QuicPaddingFrame()};
  ASSERT_FALSE(QuicUtils::IsRetransmittableFrame(padding_frame.type));

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));

  EXPECT_TRUE(creator_.AddFrame(ack_frame, LOSS_RETRANSMISSION));
  ASSERT_EQ(serialized_packet_, nullptr);

  EXPECT_TRUE(creator_.AddFrame(stream_frame, PTO_RETRANSMISSION));
  ASSERT_EQ(serialized_packet_, nullptr);

  EXPECT_TRUE(creator_.AddFrame(stream_frame_2, PATH_RETRANSMISSION));
  ASSERT_EQ(serialized_packet_, nullptr);

  EXPECT_TRUE(creator_.AddFrame(padding_frame, PTO_RETRANSMISSION));
  creator_.FlushCurrentPacket();
  ASSERT_TRUE(serialized_packet_->encrypted_buffer);

  // The last retransmittable frame on packet is a stream frame, the packet's
  // transmission type should be the same as the stream frame's.
  EXPECT_EQ(serialized_packet_->transmission_type, PATH_RETRANSMISSION);
  DeleteSerializedPacket();
}

TEST_P(QuicPacketCreatorTest,
       PacketBytesRetransmitted_AddFrame_Retransmission) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);

  QuicAckFrame temp_ack_frame = InitAckFrame(1);
  QuicFrame ack_frame(&temp_ack_frame);
  EXPECT_TRUE(creator_.AddFrame(ack_frame, LOSS_RETRANSMISSION));

  QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      client_framer_.transport_version(), Perspective::IS_CLIENT);

  QuicFrame stream_frame;
  const std::string data("data");
  // ConsumeDataToFillCurrentPacket calls AddFrame
  ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
      stream_id, data, 0u, false, false, PTO_RETRANSMISSION, &stream_frame));
  EXPECT_EQ(4u, stream_frame.stream_frame.data_length);

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));

  creator_.FlushCurrentPacket();
  ASSERT_TRUE(serialized_packet_->encrypted_buffer);
  ASSERT_FALSE(serialized_packet_->bytes_not_retransmitted.has_value());

  DeleteSerializedPacket();
}

TEST_P(QuicPacketCreatorTest,
       PacketBytesRetransmitted_AddFrame_NotRetransmission) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);

  QuicAckFrame temp_ack_frame = InitAckFrame(1);
  QuicFrame ack_frame(&temp_ack_frame);
  EXPECT_TRUE(creator_.AddFrame(ack_frame, NOT_RETRANSMISSION));

  QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      client_framer_.transport_version(), Perspective::IS_CLIENT);

  QuicFrame stream_frame;
  const std::string data("data");
  // ConsumeDataToFillCurrentPacket calls AddFrame
  ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
      stream_id, data, 0u, false, false, NOT_RETRANSMISSION, &stream_frame));
  EXPECT_EQ(4u, stream_frame.stream_frame.data_length);

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));

  creator_.FlushCurrentPacket();
  ASSERT_TRUE(serialized_packet_->encrypted_buffer);
  ASSERT_FALSE(serialized_packet_->bytes_not_retransmitted.has_value());

  DeleteSerializedPacket();
}

TEST_P(QuicPacketCreatorTest, PacketBytesRetransmitted_AddFrame_MixedFrames) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);

  QuicAckFrame temp_ack_frame = InitAckFrame(1);
  QuicFrame ack_frame(&temp_ack_frame);
  EXPECT_TRUE(creator_.AddFrame(ack_frame, NOT_RETRANSMISSION));

  QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      client_framer_.transport_version(), Perspective::IS_CLIENT);

  QuicFrame stream_frame;
  const std::string data("data");
  // ConsumeDataToFillCurrentPacket calls AddFrame
  ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
      stream_id, data, 0u, false, false, NOT_RETRANSMISSION, &stream_frame));
  EXPECT_EQ(4u, stream_frame.stream_frame.data_length);

  QuicFrame stream_frame2;
  // ConsumeDataToFillCurrentPacket calls AddFrame
  ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
      stream_id, data, 0u, false, false, LOSS_RETRANSMISSION, &stream_frame2));
  EXPECT_EQ(4u, stream_frame2.stream_frame.data_length);

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));

  creator_.FlushCurrentPacket();
  ASSERT_TRUE(serialized_packet_->encrypted_buffer);
  ASSERT_TRUE(serialized_packet_->bytes_not_retransmitted.has_value());
  ASSERT_GE(serialized_packet_->bytes_not_retransmitted.value(), 4u);

  DeleteSerializedPacket();
}

TEST_P(QuicPacketCreatorTest,
       PacketBytesRetransmitted_CreateAndSerializeStreamFrame_Retransmission) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);

  const std::string data("test");
  producer_.SaveStreamData(GetNthClientInitiatedStreamId(0), data);
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  size_t num_bytes_consumed;
  // Retransmission frame adds to packet's bytes_retransmitted
  creator_.CreateAndSerializeStreamFrame(
      GetNthClientInitiatedStreamId(0), data.length(), 0, 0, true,
      LOSS_RETRANSMISSION, &num_bytes_consumed);
  EXPECT_EQ(4u, num_bytes_consumed);

  ASSERT_TRUE(serialized_packet_->encrypted_buffer);
  ASSERT_FALSE(serialized_packet_->bytes_not_retransmitted.has_value());
  DeleteSerializedPacket();

  EXPECT_FALSE(creator_.HasPendingFrames());
}

TEST_P(
    QuicPacketCreatorTest,
    PacketBytesRetransmitted_CreateAndSerializeStreamFrame_NotRetransmission) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);

  const std::string data("test");
  producer_.SaveStreamData(GetNthClientInitiatedStreamId(0), data);
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  size_t num_bytes_consumed;
  // Non-retransmission frame does not add to packet's bytes_retransmitted
  creator_.CreateAndSerializeStreamFrame(
      GetNthClientInitiatedStreamId(0), data.length(), 0, 0, true,
      NOT_RETRANSMISSION, &num_bytes_consumed);
  EXPECT_EQ(4u, num_bytes_consumed);

  ASSERT_TRUE(serialized_packet_->encrypted_buffer);
  ASSERT_FALSE(serialized_packet_->bytes_not_retransmitted.has_value());
  DeleteSerializedPacket();

  EXPECT_FALSE(creator_.HasPendingFrames());
}

TEST_P(QuicPacketCreatorTest, RetryToken) {
  if (!GetParam().version_serialization ||
      !QuicVersionHasLongHeaderLengths(client_framer_.transport_version())) {
    return;
  }

  char retry_token_bytes[] = {1, 2,  3,  4,  5,  6,  7,  8,
                              9, 10, 11, 12, 13, 14, 15, 16};

  creator_.SetRetryToken(
      std::string(retry_token_bytes, sizeof(retry_token_bytes)));

  frames_.push_back(QuicFrame(QuicPingFrame()));
  SerializedPacket serialized = SerializeAllFrames(frames_);

  QuicPacketHeader header;
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_))
        .WillOnce(DoAll(SaveArg<0>(&header), Return(true)));
    if (client_framer_.version().HasHeaderProtection()) {
      EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
    }
    EXPECT_CALL(framer_visitor_, OnPingFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized);
  ASSERT_TRUE(header.version_flag);
  ASSERT_EQ(header.long_packet_type, INITIAL);
  ASSERT_EQ(header.retry_token.length(), sizeof(retry_token_bytes));
  quiche::test::CompareCharArraysWithHexError(
      "retry token", header.retry_token.data(), header.retry_token.length(),
      retry_token_bytes, sizeof(retry_token_bytes));
}

TEST_P(QuicPacketCreatorTest, GetConnectionId) {
  EXPECT_EQ(TestConnectionId(2), creator_.GetDestinationConnectionId());
  EXPECT_EQ(EmptyQuicConnectionId(), creator_.GetSourceConnectionId());
}

TEST_P(QuicPacketCreatorTest, ClientConnectionId) {
  if (!client_framer_.version().SupportsClientConnectionIds()) {
    return;
  }
  EXPECT_EQ(TestConnectionId(2), creator_.GetDestinationConnectionId());
  EXPECT_EQ(EmptyQuicConnectionId(), creator_.GetSourceConnectionId());
  creator_.SetClientConnectionId(TestConnectionId(0x33));
  EXPECT_EQ(TestConnectionId(2), creator_.GetDestinationConnectionId());
  EXPECT_EQ(TestConnectionId(0x33), creator_.GetSourceConnectionId());
}

TEST_P(QuicPacketCreatorTest, CoalesceStreamFrames) {
  InSequence s;
  if (!GetParam().version_serialization) {
    creator_.StopSendingVersion();
  }
  const size_t max_plaintext_size =
      client_framer_.GetMaxPlaintextSize(creator_.max_packet_length());
  EXPECT_FALSE(creator_.HasPendingFrames());
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  QuicStreamId stream_id1 = QuicUtils::GetFirstBidirectionalStreamId(
      client_framer_.transport_version(), Perspective::IS_CLIENT);
  QuicStreamId stream_id2 = GetNthClientInitiatedStreamId(1);
  EXPECT_FALSE(creator_.HasPendingStreamFramesOfStream(stream_id1));
  EXPECT_EQ(max_plaintext_size -
                GetPacketHeaderSize(
                    client_framer_.transport_version(),
                    creator_.GetDestinationConnectionIdLength(),
                    creator_.GetSourceConnectionIdLength(),
                    QuicPacketCreatorPeer::SendVersionInPacket(&creator_),
                    !kIncludeDiversificationNonce,
                    QuicPacketCreatorPeer::GetPacketNumberLength(&creator_),
                    QuicPacketCreatorPeer::GetRetryTokenLengthLength(&creator_),
                    0, QuicPacketCreatorPeer::GetLengthLength(&creator_)),
            creator_.BytesFree());
  StrictMock<MockDebugDelegate> debug;
  creator_.set_debug_delegate(&debug);

  QuicFrame frame;
  const std::string data1("test");
  EXPECT_CALL(debug, OnFrameAddedToPacket(_));
  ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
      stream_id1, data1, 0u, false, false, NOT_RETRANSMISSION, &frame));
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_TRUE(creator_.HasPendingStreamFramesOfStream(stream_id1));

  const std::string data2("coalesce");
  // frame will be coalesced with the first frame.
  const auto previous_size = creator_.PacketSize();
  QuicStreamFrame target(stream_id1, true, 0, data1.length() + data2.length());
  EXPECT_CALL(debug, OnStreamFrameCoalesced(target));
  ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
      stream_id1, data2, 4u, true, false, NOT_RETRANSMISSION, &frame));
  EXPECT_EQ(frame.stream_frame.data_length,
            creator_.PacketSize() - previous_size);

  // frame is for another stream, so it won't be coalesced.
  const auto length = creator_.BytesFree() - 10u;
  const std::string data3(length, 'x');
  EXPECT_CALL(debug, OnFrameAddedToPacket(_));
  ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
      stream_id2, data3, 0u, false, false, NOT_RETRANSMISSION, &frame));
  EXPECT_TRUE(creator_.HasPendingStreamFramesOfStream(stream_id2));

  // The packet doesn't have enough free bytes for all data, but will still be
  // able to consume and coalesce part of them.
  EXPECT_CALL(debug, OnStreamFrameCoalesced(_));
  const std::string data4("somerandomdata");
  ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
      stream_id2, data4, length, false, false, NOT_RETRANSMISSION, &frame));

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  creator_.FlushCurrentPacket();
  EXPECT_CALL(framer_visitor_, OnPacket());
  EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
  EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
  EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  // The packet should only have 2 stream frames.
  EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
  EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());
  ProcessPacket(*serialized_packet_);
}

TEST_P(QuicPacketCreatorTest, SaveNonRetransmittableFrames) {
  QuicAckFrame ack_frame(InitAckFrame(1));
  frames_.push_back(QuicFrame(&ack_frame));
  frames_.push_back(QuicFrame(QuicPaddingFrame(-1)));
  SerializedPacket serialized = SerializeAllFrames(frames_);
  ASSERT_EQ(2u, serialized.nonretransmittable_frames.size());
  EXPECT_EQ(ACK_FRAME, serialized.nonretransmittable_frames[0].type);
  EXPECT_EQ(PADDING_FRAME, serialized.nonretransmittable_frames[1].type);
  // Verify full padding frame is translated to a padding frame with actual
  // bytes of padding.
  EXPECT_LT(
      0,
      serialized.nonretransmittable_frames[1].padding_frame.num_padding_bytes);
  frames_.clear();

  // Serialize another packet with the same frames.
  SerializedPacket packet = QuicPacketCreatorPeer::SerializeAllFrames(
      &creator_, serialized.nonretransmittable_frames, buffer_,
      kMaxOutgoingPacketSize);
  // Verify the packet length of both packets are equal.
  EXPECT_EQ(serialized.encrypted_length, packet.encrypted_length);
}

TEST_P(QuicPacketCreatorTest, SerializeCoalescedPacket) {
  QuicCoalescedPacket coalesced;
  quiche::SimpleBufferAllocator allocator;
  QuicSocketAddress self_address(QuicIpAddress::Loopback4(), 1);
  QuicSocketAddress peer_address(QuicIpAddress::Loopback4(), 2);
  for (size_t i = ENCRYPTION_INITIAL; i < NUM_ENCRYPTION_LEVELS; ++i) {
    EncryptionLevel level = static_cast<EncryptionLevel>(i);
    creator_.set_encryption_level(level);
    QuicAckFrame ack_frame(InitAckFrame(1));
    if (level != ENCRYPTION_ZERO_RTT) {
      frames_.push_back(QuicFrame(&ack_frame));
    }
    if (level != ENCRYPTION_INITIAL && level != ENCRYPTION_HANDSHAKE) {
      frames_.push_back(
          QuicFrame(QuicStreamFrame(1, false, 0u, absl::string_view())));
    }
    SerializedPacket serialized = SerializeAllFrames(frames_);
    EXPECT_EQ(level, serialized.encryption_level);
    frames_.clear();
    ASSERT_TRUE(coalesced.MaybeCoalescePacket(
        serialized, self_address, peer_address, &allocator,
        creator_.max_packet_length(), ECN_NOT_ECT, 0));
  }
  char buffer[kMaxOutgoingPacketSize];
  size_t coalesced_length = creator_.SerializeCoalescedPacket(
      coalesced, buffer, kMaxOutgoingPacketSize);
  // Verify packet is padded to full.
  ASSERT_EQ(coalesced.max_packet_length(), coalesced_length);
  if (!QuicVersionHasLongHeaderLengths(server_framer_.transport_version())) {
    return;
  }
  // Verify packet process.
  std::unique_ptr<QuicEncryptedPacket> packets[NUM_ENCRYPTION_LEVELS];
  packets[ENCRYPTION_INITIAL] =
      std::make_unique<QuicEncryptedPacket>(buffer, coalesced_length);
  for (size_t i = ENCRYPTION_INITIAL; i < NUM_ENCRYPTION_LEVELS; ++i) {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    if (i < ENCRYPTION_FORWARD_SECURE) {
      // Save coalesced packet.
      EXPECT_CALL(framer_visitor_, OnCoalescedPacket(_))
          .WillOnce(Invoke([i, &packets](const QuicEncryptedPacket& packet) {
            packets[i + 1] = packet.Clone();
          }));
    }
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_, _));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    if (i != ENCRYPTION_ZERO_RTT) {
      if (i != ENCRYPTION_INITIAL) {
        EXPECT_CALL(framer_visitor_, OnPaddingFrame(_))
            .Times(testing::AtMost(1));
      }
      EXPECT_CALL(framer_visitor_, OnAckFrameStart(_, _))
          .WillOnce(Return(true));
      EXPECT_CALL(framer_visitor_,
                  OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2)))
          .WillOnce(Return(true));
      EXPECT_CALL(framer_visitor_, OnAckFrameEnd(_, _)).WillOnce(Return(true));
    }
    if (i == ENCRYPTION_INITIAL) {
      // Verify padding is added.
      EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
    }
    if (i == ENCRYPTION_ZERO_RTT) {
      EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
    }
    if (i != ENCRYPTION_INITIAL && i != ENCRYPTION_HANDSHAKE) {
      EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    }
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
    server_framer_.ProcessPacket(*packets[i]);
  }
}

TEST_P(QuicPacketCreatorTest, SoftMaxPacketLength) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  QuicByteCount previous_max_packet_length = creator_.max_packet_length();
  const size_t overhead =
      GetPacketHeaderOverhead(client_framer_.transport_version()) +
      QuicPacketCreator::MinPlaintextPacketSize(
          client_framer_.version(),
          QuicPacketCreatorPeer::GetPacketNumberLength(&creator_)) +
      GetEncryptionOverhead();
  // Make sure a length which cannot accommodate header (includes header
  // protection minimal length) gets rejected.
  creator_.SetSoftMaxPacketLength(overhead - 1);
  EXPECT_EQ(previous_max_packet_length, creator_.max_packet_length());

  creator_.SetSoftMaxPacketLength(overhead);
  EXPECT_EQ(overhead, creator_.max_packet_length());

  // Verify creator has room for stream frame because max_packet_length_ gets
  // restored.
  ASSERT_TRUE(creator_.HasRoomForStreamFrame(
      GetNthClientInitiatedStreamId(1), kMaxIetfVarInt,
      std::numeric_limits<uint32_t>::max()));
  EXPECT_EQ(previous_max_packet_length, creator_.max_packet_length());

  // Same for message frame.
  creator_.SetSoftMaxPacketLength(overhead);
  if (client_framer_.version().UsesTls()) {
    creator_.SetMaxDatagramFrameSize(kMaxAcceptedDatagramFrameSize);
  }
  // Verify GetCurrentLargestMessagePayload is based on the actual
  // max_packet_length.
  EXPECT_LT(1u, creator_.GetCurrentLargestMessagePayload());
  EXPECT_EQ(overhead, creator_.max_packet_length());
  ASSERT_TRUE(creator_.HasRoomForMessageFrame(
      creator_.GetCurrentLargestMessagePayload()));
  EXPECT_EQ(previous_max_packet_length, creator_.max_packet_length());

  // Verify creator can consume crypto data because max_packet_length_ gets
  // restored.
  creator_.SetSoftMaxPacketLength(overhead);
  EXPECT_EQ(overhead, creator_.max_packet_length());
  const std::string data = "crypto data";
  QuicFrame frame;
  if (!QuicVersionUsesCryptoFrames(client_framer_.transport_version())) {
    ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
        QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), data,
        kOffset, false, true, NOT_RETRANSMISSION, &frame));
    size_t bytes_consumed = frame.stream_frame.data_length;
    EXPECT_LT(0u, bytes_consumed);
  } else {
    producer_.SaveCryptoData(ENCRYPTION_INITIAL, kOffset, data);
    ASSERT_TRUE(creator_.ConsumeCryptoDataToFillCurrentPacket(
        ENCRYPTION_INITIAL, data.length(), kOffset,
        /*needs_full_padding=*/true, NOT_RETRANSMISSION, &frame));
    size_t bytes_consumed = frame.crypto_frame->data_length;
    EXPECT_LT(0u, bytes_consumed);
  }
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  creator_.FlushCurrentPacket();

  // Verify ACK frame can be consumed.
  creator_.SetSoftMaxPacketLength(overhead);
  EXPECT_EQ(overhead, creator_.max_packet_length());
  QuicAckFrame ack_frame(InitAckFrame(10u));
  EXPECT_TRUE(creator_.AddFrame(QuicFrame(&ack_frame), NOT_RETRANSMISSION));
  EXPECT_TRUE(creator_.HasPendingFrames());
}

TEST_P(QuicPacketCreatorTest,
       ChangingEncryptionLevelRemovesSoftMaxPacketLength) {
  if (!client_framer_.version().CanSendCoalescedPackets()) {
    return;
  }
  // First set encryption level to forward secure which has the shortest header.
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  const QuicByteCount previous_max_packet_length = creator_.max_packet_length();
  const size_t min_acceptable_packet_size =
      GetPacketHeaderOverhead(client_framer_.transport_version()) +
      QuicPacketCreator::MinPlaintextPacketSize(
          client_framer_.version(),
          QuicPacketCreatorPeer::GetPacketNumberLength(&creator_)) +
      GetEncryptionOverhead();
  // Then set the soft max packet length to the lowest allowed value.
  creator_.SetSoftMaxPacketLength(min_acceptable_packet_size);
  // Make sure that the low value was accepted.
  EXPECT_EQ(creator_.max_packet_length(), min_acceptable_packet_size);
  // Now set the encryption level to handshake which increases the header size.
  creator_.set_encryption_level(ENCRYPTION_HANDSHAKE);
  // Make sure that adding a frame removes the the soft max packet length.
  QuicAckFrame ack_frame(InitAckFrame(1));
  frames_.push_back(QuicFrame(&ack_frame));
  SerializedPacket serialized = SerializeAllFrames(frames_);
  EXPECT_EQ(serialized.encryption_level, ENCRYPTION_HANDSHAKE);
  EXPECT_EQ(creator_.max_packet_length(), previous_max_packet_length);
}

TEST_P(QuicPacketCreatorTest, MinPayloadLength) {
  ParsedQuicVersion version = client_framer_.version();
  for (QuicPacketNumberLength pn_length :
       {PACKET_1BYTE_PACKET_NUMBER, PACKET_2BYTE_PACKET_NUMBER,
        PACKET_3BYTE_PACKET_NUMBER, PACKET_4BYTE_PACKET_NUMBER}) {
    if (!version.HasHeaderProtection()) {
      EXPECT_EQ(creator_.MinPlaintextPacketSize(version, pn_length), 0);
    } else {
      EXPECT_EQ(creator_.MinPlaintextPacketSize(version, pn_length),
                (version.UsesTls() ? 4 : 8) - pn_length);
    }
  }
}

// A variant of StreamFrameConsumption that tests when expansion of the stream
// frame puts it at or over the max length, but the packet is supposed to be
// padded to max length.
TEST_P(QuicPacketCreatorTest, PadWhenAlmostMaxLength) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  // Compute the total overhead for a single frame in packet.
  const size_t overhead =
      GetPacketHeaderOverhead(client_framer_.transport_version()) +
      GetEncryptionOverhead() +
      GetStreamFrameOverhead(client_framer_.transport_version());
  size_t capacity = kDefaultMaxPacketSize - overhead;
  for (size_t bytes_free = 1; bytes_free <= 2; bytes_free++) {
    std::string data(capacity - bytes_free, 'A');

    QuicFrame frame;
    ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
        GetNthClientInitiatedStreamId(1), data, kOffset, false,
        /*needs_full_padding=*/true, NOT_RETRANSMISSION, &frame));

    // BytesFree() returns bytes available for the next frame, which will
    // be two bytes smaller since the stream frame would need to be grown.
    EXPECT_EQ(2u, creator_.ExpansionOnNewFrame());
    EXPECT_EQ(0u, creator_.BytesFree());
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
    creator_.FlushCurrentPacket();
    EXPECT_EQ(serialized_packet_->encrypted_length, kDefaultMaxPacketSize);
    DeleteSerializedPacket();
  }
}

TEST_P(QuicPacketCreatorTest, MorePendingPaddingThanBytesFree) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  // Compute the total overhead for a single frame in packet.
  const size_t overhead =
      GetPacketHeaderOverhead(client_framer_.transport_version()) +
      GetEncryptionOverhead() +
      GetStreamFrameOverhead(client_framer_.transport_version());
  size_t capacity = kDefaultMaxPacketSize - overhead;
  const size_t pending_padding = 10;
  std::string data(capacity - pending_padding, 'A');
  QuicFrame frame;
  // The stream frame means that BytesFree() will be less than the
  // available space, because of the frame length field.
  ASSERT_TRUE(creator_.ConsumeDataToFillCurrentPacket(
      GetNthClientInitiatedStreamId(1), data, kOffset, false,
      /*needs_full_padding=*/false, NOT_RETRANSMISSION, &frame));
  creator_.AddPendingPadding(pending_padding);
  EXPECT_EQ(2u, creator_.ExpansionOnNewFrame());
  // BytesFree() does not know about pending_padding because that's added
  // when flushed.
  EXPECT_EQ(pending_padding - 2u, creator_.BytesFree());
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  creator_.FlushCurrentPacket();
  /* Without the fix, the packet is not full-length. */
  EXPECT_EQ(serialized_packet_->encrypted_length, kDefaultMaxPacketSize);
  DeleteSerializedPacket();
}

class MockDelegate : public QuicPacketCreator::DelegateInterface {
 public:
  MockDelegate() {}
  MockDelegate(const MockDelegate&) = delete;
  MockDelegate& operator=(const MockDelegate&) = delete;
  ~MockDelegate() override {}

  MOCK_METHOD(bool, ShouldGeneratePacket,
              (HasRetransmittableData retransmittable, IsHandshake handshake),
              (override));
  MOCK_METHOD(void, MaybeBundleOpportunistically,
              (TransmissionType transmission_type), (override));
  MOCK_METHOD(QuicByteCount, GetFlowControlSendWindowSize, (QuicStreamId),
              (override));
  MOCK_METHOD(QuicPacketBuffer, GetPacketBuffer, (), (override));
  MOCK_METHOD(void, OnSerializedPacket, (SerializedPacket), (override));
  MOCK_METHOD(void, OnUnrecoverableError, (QuicErrorCode, const std::string&),
              (override));
  MOCK_METHOD(SerializedPacketFate, GetSerializedPacketFate,
              (bool, EncryptionLevel), (override));

  void SetCanWriteAnything() {
    EXPECT_CALL(*this, ShouldGeneratePacket(_, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*this, ShouldGeneratePacket(NO_RETRANSMITTABLE_DATA, _))
        .WillRepeatedly(Return(true));
  }

  void SetCanNotWrite() {
    EXPECT_CALL(*this, ShouldGeneratePacket(_, _))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*this, ShouldGeneratePacket(NO_RETRANSMITTABLE_DATA, _))
        .WillRepeatedly(Return(false));
  }

  // Use this when only ack frames should be allowed to be written.
  void SetCanWriteOnlyNonRetransmittable() {
    EXPECT_CALL(*this, ShouldGeneratePacket(_, _))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*this, ShouldGeneratePacket(NO_RETRANSMITTABLE_DATA, _))
        .WillRepeatedly(Return(true));
  }
};

// Simple struct for describing the contents of a packet.
// Useful in conjunction with a SimpleQuicFrame for validating that a packet
// contains the expected frames.
struct PacketContents {
  PacketContents()
      : num_ack_frames(0),
        num_connection_close_frames(0),
        num_goaway_frames(0),
        num_rst_stream_frames(0),
        num_stop_waiting_frames(0),
        num_stream_frames(0),
        num_crypto_frames(0),
        num_ping_frames(0),
        num_mtu_discovery_frames(0),
        num_padding_frames(0) {}

  size_t num_ack_frames;
  size_t num_connection_close_frames;
  size_t num_goaway_frames;
  size_t num_rst_stream_frames;
  size_t num_stop_waiting_frames;
  size_t num_stream_frames;
  size_t num_crypto_frames;
  size_t num_ping_frames;
  size_t num_mtu_discovery_frames;
  size_t num_padding_frames;
};

class MultiplePacketsTestPacketCreator : public QuicPacketCreator {
 public:
  MultiplePacketsTestPacketCreator(
      QuicConnectionId connection_id, QuicFramer* framer,
      QuicRandom* random_generator,
      QuicPacketCreator::DelegateInterface* delegate,
      SimpleDataProducer* producer)
      : QuicPacketCreator(connection_id, framer, random_generator, delegate),
        ack_frame_(InitAckFrame(1)),
        delegate_(static_cast<MockDelegate*>(delegate)),
        producer_(producer) {}

  bool ConsumeRetransmittableControlFrame(const QuicFrame& frame,
                                          bool bundle_ack) {
    QuicFrames frames;
    if (bundle_ack) {
      frames.push_back(QuicFrame(&ack_frame_));
    }
    EXPECT_CALL(*delegate_, MaybeBundleOpportunistically(_))
        .WillOnce(Invoke([this, frames = std::move(frames)] {
          FlushAckFrame(frames);
          return QuicFrames();
        }));
    return QuicPacketCreator::ConsumeRetransmittableControlFrame(frame);
  }

  QuicConsumedData ConsumeDataFastPath(QuicStreamId id,
                                       absl::string_view data) {
    // Save data before data is consumed.
    if (!data.empty()) {
      producer_->SaveStreamData(id, data);
    }
    return QuicPacketCreator::ConsumeDataFastPath(id, data.length(),
                                                  /* offset = */ 0,
                                                  /* fin = */ true, 0);
  }

  QuicConsumedData ConsumeData(QuicStreamId id, absl::string_view data,
                               QuicStreamOffset offset,
                               StreamSendingState state) {
    // Save data before data is consumed.
    if (!data.empty()) {
      producer_->SaveStreamData(id, data);
    }
    EXPECT_CALL(*delegate_, MaybeBundleOpportunistically(_)).Times(1);
    return QuicPacketCreator::ConsumeData(id, data.length(), offset, state);
  }

  MessageStatus AddMessageFrame(QuicMessageId message_id,
                                quiche::QuicheMemSlice message) {
    if (!has_ack() && delegate_->ShouldGeneratePacket(NO_RETRANSMITTABLE_DATA,
                                                      NOT_HANDSHAKE)) {
      EXPECT_CALL(*delegate_, MaybeBundleOpportunistically(_)).Times(1);
    }
    return QuicPacketCreator::AddMessageFrame(message_id,
                                              absl::MakeSpan(&message, 1));
  }

  size_t ConsumeCryptoData(EncryptionLevel level, absl::string_view data,
                           QuicStreamOffset offset) {
    producer_->SaveCryptoData(level, offset, data);
    EXPECT_CALL(*delegate_, MaybeBundleOpportunistically(_)).Times(1);
    return QuicPacketCreator::ConsumeCryptoData(level, data.length(), offset);
  }

  QuicAckFrame ack_frame_;
  MockDelegate* delegate_;
  SimpleDataProducer* producer_;
};

class QuicPacketCreatorMultiplePacketsTest : public QuicTest {
 public:
  QuicPacketCreatorMultiplePacketsTest()
      : framer_(AllSupportedVersions(), QuicTime::Zero(),
                Perspective::IS_CLIENT, kQuicDefaultConnectionIdLength),
        creator_(TestConnectionId(), &framer_, &random_creator_, &delegate_,
                 &producer_),
        ack_frame_(InitAckFrame(1)) {
    EXPECT_CALL(delegate_, GetPacketBuffer())
        .WillRepeatedly(Return(QuicPacketBuffer()));
    EXPECT_CALL(delegate_, GetSerializedPacketFate(_, _))
        .WillRepeatedly(Return(SEND_TO_WRITER));
    EXPECT_CALL(delegate_, GetFlowControlSendWindowSize(_))
        .WillRepeatedly(Return(std::numeric_limits<QuicByteCount>::max()));
    creator_.SetEncrypter(
        ENCRYPTION_FORWARD_SECURE,
        std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
    creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
    framer_.set_data_producer(&producer_);
    if (simple_framer_.framer()->version().KnowsWhichDecrypterToUse()) {
      simple_framer_.framer()->InstallDecrypter(
          ENCRYPTION_FORWARD_SECURE, std::make_unique<TaggingDecrypter>());
    }
    creator_.AttachPacketFlusher();
  }

  ~QuicPacketCreatorMultiplePacketsTest() override {}

  void SavePacket(SerializedPacket packet) {
    QUICHE_DCHECK(packet.release_encrypted_buffer == nullptr);
    packet.encrypted_buffer = CopyBuffer(packet);
    packet.release_encrypted_buffer = [](const char* p) { delete[] p; };
    packets_.push_back(std::move(packet));
  }

 protected:
  QuicRstStreamFrame* CreateRstStreamFrame() {
    return new QuicRstStreamFrame(1, 1, QUIC_STREAM_NO_ERROR, 0);
  }

  QuicGoAwayFrame* CreateGoAwayFrame() {
    return new QuicGoAwayFrame(2, QUIC_NO_ERROR, 1, std::string());
  }

  void CheckPacketContains(const PacketContents& contents,
                           size_t packet_index) {
    ASSERT_GT(packets_.size(), packet_index);
    const SerializedPacket& packet = packets_[packet_index];
    size_t num_retransmittable_frames =
        contents.num_connection_close_frames + contents.num_goaway_frames +
        contents.num_rst_stream_frames + contents.num_stream_frames +
        contents.num_crypto_frames + contents.num_ping_frames;
    size_t num_frames =
        contents.num_ack_frames + contents.num_stop_waiting_frames +
        contents.num_mtu_discovery_frames + contents.num_padding_frames +
        num_retransmittable_frames;

    if (num_retransmittable_frames == 0) {
      ASSERT_TRUE(packet.retransmittable_frames.empty());
    } else {
      EXPECT_EQ(num_retransmittable_frames,
                packet.retransmittable_frames.size());
    }

    ASSERT_TRUE(packet.encrypted_buffer != nullptr);
    ASSERT_TRUE(simple_framer_.ProcessPacket(
        QuicEncryptedPacket(packet.encrypted_buffer, packet.encrypted_length)));
    size_t num_padding_frames = 0;
    if (contents.num_padding_frames == 0) {
      num_padding_frames = simple_framer_.padding_frames().size();
    }
    EXPECT_EQ(num_frames + num_padding_frames, simple_framer_.num_frames());
    EXPECT_EQ(contents.num_ack_frames, simple_framer_.ack_frames().size());
    EXPECT_EQ(contents.num_connection_close_frames,
              simple_framer_.connection_close_frames().size());
    EXPECT_EQ(contents.num_goaway_frames,
              simple_framer_.goaway_frames().size());
    EXPECT_EQ(contents.num_rst_stream_frames,
              simple_framer_.rst_stream_frames().size());
    EXPECT_EQ(contents.num_stream_frames,
              simple_framer_.stream_frames().size());
    EXPECT_EQ(contents.num_crypto_frames,
              simple_framer_.crypto_frames().size());
    EXPECT_EQ(contents.num_stop_waiting_frames,
              simple_framer_.stop_waiting_frames().size());
    if (contents.num_padding_frames != 0) {
      EXPECT_EQ(contents.num_padding_frames,
                simple_framer_.padding_frames().size());
    }

    // From the receiver's perspective, MTU discovery frames are ping frames.
    EXPECT_EQ(contents.num_ping_frames + contents.num_mtu_discovery_frames,
              simple_framer_.ping_frames().size());
  }

  void CheckPacketHasSingleStreamFrame(size_t packet_index) {
    ASSERT_GT(packets_.size(), packet_index);
    const SerializedPacket& packet = packets_[packet_index];
    ASSERT_FALSE(packet.retransmittable_frames.empty());
    EXPECT_EQ(1u, packet.retransmittable_frames.size());
    ASSERT_TRUE(packet.encrypted_buffer != nullptr);
    ASSERT_TRUE(simple_framer_.ProcessPacket(
        QuicEncryptedPacket(packet.encrypted_buffer, packet.encrypted_length)));
    EXPECT_EQ(1u, simple_framer_.num_frames());
    EXPECT_EQ(1u, simple_framer_.stream_frames().size());
  }

  void CheckAllPacketsHaveSingleStreamFrame() {
    for (size_t i = 0; i < packets_.size(); i++) {
      CheckPacketHasSingleStreamFrame(i);
    }
  }

  void TestChaosProtection(bool chaos_protection_enabled,
                           size_t crypto_data_length, int num_packets);
  void SetupInitialCrypto(size_t crypto_data_length, int num_ack_blocks,
                          bool chaos_protection_enabled);
  void CheckPackets(int num_ack_blocks, int num_packets,
                    bool chaos_protection_expected);

  QuicFramer framer_;
  ::testing::NiceMock<MockRandom> random_creator_;
  StrictMock<MockDelegate> delegate_;
  MultiplePacketsTestPacketCreator creator_;
  SimpleQuicFramer simple_framer_;
  std::vector<SerializedPacket> packets_;
  QuicAckFrame ack_frame_;
  struct iovec iov_;
  quiche::SimpleBufferAllocator allocator_;

 private:
  std::unique_ptr<char[]> data_array_;
  SimpleDataProducer producer_;
};

TEST_F(QuicPacketCreatorMultiplePacketsTest, AddControlFrame_NotWritable) {
  delegate_.SetCanNotWrite();

  QuicRstStreamFrame* rst_frame = CreateRstStreamFrame();
  const bool consumed =
      creator_.ConsumeRetransmittableControlFrame(QuicFrame(rst_frame),
                                                  /*bundle_ack=*/false);
  EXPECT_FALSE(consumed);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());
  delete rst_frame;
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       WrongEncryptionLevelForStreamDataFastPath) {
  creator_.set_encryption_level(ENCRYPTION_HANDSHAKE);
  delegate_.SetCanWriteAnything();
  const std::string data(10000, '?');
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).Times(0);
  EXPECT_QUIC_BUG(
      {
        EXPECT_CALL(delegate_, OnUnrecoverableError(_, _));
        creator_.ConsumeDataFastPath(
            QuicUtils::GetFirstBidirectionalStreamId(
                framer_.transport_version(), Perspective::IS_CLIENT),
            data);
      },
      "");
}

TEST_F(QuicPacketCreatorMultiplePacketsTest, AddControlFrame_OnlyAckWritable) {
  delegate_.SetCanWriteOnlyNonRetransmittable();

  QuicRstStreamFrame* rst_frame = CreateRstStreamFrame();
  const bool consumed =
      creator_.ConsumeRetransmittableControlFrame(QuicFrame(rst_frame),
                                                  /*bundle_ack=*/false);
  EXPECT_FALSE(consumed);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());
  delete rst_frame;
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       AddControlFrame_WritableAndShouldNotFlush) {
  delegate_.SetCanWriteAnything();

  creator_.ConsumeRetransmittableControlFrame(QuicFrame(CreateRstStreamFrame()),
                                              /*bundle_ack=*/false);
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_TRUE(creator_.HasPendingRetransmittableFrames());
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       AddControlFrame_NotWritableBatchThenFlush) {
  delegate_.SetCanNotWrite();

  QuicRstStreamFrame* rst_frame = CreateRstStreamFrame();
  const bool consumed =
      creator_.ConsumeRetransmittableControlFrame(QuicFrame(rst_frame),
                                                  /*bundle_ack=*/false);
  EXPECT_FALSE(consumed);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());
  delete rst_frame;
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       AddControlFrame_WritableAndShouldFlush) {
  delegate_.SetCanWriteAnything();

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));

  creator_.ConsumeRetransmittableControlFrame(QuicFrame(CreateRstStreamFrame()),
                                              /*bundle_ack=*/false);
  creator_.Flush();
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  PacketContents contents;
  contents.num_rst_stream_frames = 1;
  CheckPacketContains(contents, 0);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest, ConsumeCryptoData) {
  delegate_.SetCanWriteAnything();

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
  std::string data = "crypto data";
  size_t consumed_bytes =
      creator_.ConsumeCryptoData(ENCRYPTION_INITIAL, data, 0);
  creator_.Flush();
  EXPECT_EQ(data.length(), consumed_bytes);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  PacketContents contents;
  contents.num_crypto_frames = 1;
  contents.num_padding_frames = 1;
  CheckPacketContains(contents, 0);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       ConsumeCryptoDataCheckShouldGeneratePacket) {
  delegate_.SetCanNotWrite();

  EXPECT_CALL(delegate_, OnSerializedPacket(_)).Times(0);
  std::string data = "crypto data";
  size_t consumed_bytes =
      creator_.ConsumeCryptoData(ENCRYPTION_INITIAL, data, 0);
  creator_.Flush();
  EXPECT_EQ(0u, consumed_bytes);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());
}

// Tests the case that after bundling data, send window reduced to be shorter
// than data.
TEST_F(QuicPacketCreatorMultiplePacketsTest,
       ConsumeDataAdjustWriteLengthAfterBundledData) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  creator_.SetTransmissionType(NOT_RETRANSMISSION);
  delegate_.SetCanWriteAnything();

  const std::string data(1000, 'D');
  QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      framer_.transport_version(), Perspective::IS_CLIENT);

  EXPECT_CALL(delegate_, GetFlowControlSendWindowSize(stream_id))
      .WillOnce(Return(data.length() - 1));

  QuicConsumedData consumed = creator_.ConsumeData(stream_id, data, 0u, FIN);

  EXPECT_EQ(consumed.bytes_consumed, data.length() - 1);
  EXPECT_FALSE(consumed.fin_consumed);
}

// Tests the case that after bundling data, send window is exactly as big as
// data length.
TEST_F(QuicPacketCreatorMultiplePacketsTest,
       ConsumeDataDoesNotAdjustWriteLengthAfterBundledData) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  creator_.SetTransmissionType(NOT_RETRANSMISSION);
  delegate_.SetCanWriteAnything();

  const std::string data(1000, 'D');
  QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      framer_.transport_version(), Perspective::IS_CLIENT);

  EXPECT_CALL(delegate_, GetFlowControlSendWindowSize(stream_id))
      .WillOnce(Return(data.length()));

  QuicConsumedData consumed = creator_.ConsumeData(stream_id, data, 0u, FIN);

  EXPECT_EQ(consumed.bytes_consumed, data.length());
  EXPECT_TRUE(consumed.fin_consumed);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest, ConsumeData_NotWritable) {
  delegate_.SetCanNotWrite();

  QuicConsumedData consumed = creator_.ConsumeData(
      QuicUtils::GetFirstBidirectionalStreamId(framer_.transport_version(),
                                               Perspective::IS_CLIENT),
      "foo", 0, FIN);
  EXPECT_EQ(0u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       ConsumeData_WritableAndShouldNotFlush) {
  delegate_.SetCanWriteAnything();

  QuicConsumedData consumed = creator_.ConsumeData(
      QuicUtils::GetFirstBidirectionalStreamId(framer_.transport_version(),
                                               Perspective::IS_CLIENT),
      "foo", 0, FIN);
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_TRUE(creator_.HasPendingRetransmittableFrames());
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       ConsumeData_WritableAndShouldFlush) {
  delegate_.SetCanWriteAnything();

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
  QuicConsumedData consumed = creator_.ConsumeData(
      QuicUtils::GetFirstBidirectionalStreamId(framer_.transport_version(),
                                               Perspective::IS_CLIENT),
      "foo", 0, FIN);
  creator_.Flush();
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  PacketContents contents;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, 0);
}

// Test the behavior of ConsumeData when the data consumed is for the crypto
// handshake stream.  Ensure that the packet is always sent and padded even if
// the creator operates in batch mode.
TEST_F(QuicPacketCreatorMultiplePacketsTest, ConsumeData_Handshake) {
  delegate_.SetCanWriteAnything();

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
  const std::string data = "foo bar";
  size_t consumed_bytes = 0;
  if (QuicVersionUsesCryptoFrames(framer_.transport_version())) {
    consumed_bytes =
        creator_.ConsumeCryptoData(ENCRYPTION_FORWARD_SECURE, data, 0);
  } else {
    consumed_bytes =
        creator_
            .ConsumeData(
                QuicUtils::GetCryptoStreamId(framer_.transport_version()), data,
                0, NO_FIN)
            .bytes_consumed;
  }
  EXPECT_EQ(7u, consumed_bytes);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  PacketContents contents;
  if (QuicVersionUsesCryptoFrames(framer_.transport_version())) {
    contents.num_crypto_frames = 1;
  } else {
    contents.num_stream_frames = 1;
  }
  contents.num_padding_frames = 1;
  CheckPacketContains(contents, 0);

  ASSERT_EQ(1u, packets_.size());
  ASSERT_EQ(kDefaultMaxPacketSize, creator_.max_packet_length());
  EXPECT_EQ(kDefaultMaxPacketSize, packets_[0].encrypted_length);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       ChaosProtection_Enabled_OnePacket) {
  TestChaosProtection(
      /*chaos_protection_enabled=*/true, /*crypto_data_length=*/1000,
      /*num_packets=*/1);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       ChaosProtection_Enabled_TwoPackets) {
  // 1505 bytes is the usual size of the ClientHello when post-quantum
  // cryptography is enabled.
  TestChaosProtection(
      /*chaos_protection_enabled=*/true, /*crypto_data_length=*/1505,
      /*num_packets=*/2);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       ChaosProtection_Enabled_ThreePackets) {
  TestChaosProtection(
      /*chaos_protection_enabled=*/true, /*crypto_data_length=*/3000,
      /*num_packets=*/3);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       ChaosProtection_Disabled_OnePacket) {
  TestChaosProtection(
      /*chaos_protection_enabled=*/false, /*crypto_data_length=*/1000,
      /*num_packets=*/1);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       ChaosProtection_Disabled_TwoPackets) {
  // 1505 bytes is the usual size of the ClientHello when post-quantum
  // cryptography is enabled.
  TestChaosProtection(
      /*chaos_protection_enabled=*/false, /*crypto_data_length=*/1505,
      /*num_packets=*/2);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       ChaosProtection_Disabled_ThreePackets) {
  TestChaosProtection(
      /*chaos_protection_enabled=*/false, /*crypto_data_length=*/3000,
      /*num_packets=*/3);
}

void QuicPacketCreatorMultiplePacketsTest::TestChaosProtection(
    bool chaos_protection_enabled, size_t crypto_data_length, int num_packets) {
  if (!framer_.version().UsesCryptoFrames()) {
    return;
  }
  SetupInitialCrypto(/*crypto_data_length=*/0, /*num_ack_blocks=*/0,
                     chaos_protection_enabled);
  std::vector<uint8_t> data_bytes(crypto_data_length);
  for (size_t i = 0; i < data_bytes.size(); ++i) {
    data_bytes[i] = (i & 0xFF);
  }
  const std::string data(reinterpret_cast<char*>(data_bytes.data()),
                         crypto_data_length);
  EXPECT_EQ(creator_.ConsumeCryptoData(ENCRYPTION_INITIAL, data, /*offset=*/0),
            crypto_data_length);
  creator_.Flush();
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());
  EXPECT_EQ(kDefaultMaxPacketSize, creator_.max_packet_length());

  QuicIntervalSet<QuicStreamOffset> crypto_data_intervals;
  int num_crypto_frames = 0;
  bool first_packet = true;
  QuicStreamOffset max_crypto_first_packet = 0;
  QuicStreamOffset min_crypto_subsequent_packets =
      std::numeric_limits<QuicStreamOffset>::max();
  for (const auto& packet : packets_) {
    EXPECT_EQ(packet.encrypted_length, kDefaultMaxPacketSize);
    ASSERT_TRUE(packet.encrypted_buffer != nullptr);
    simple_framer_.Reset();
    ASSERT_TRUE(simple_framer_.ProcessPacket(
        QuicEncryptedPacket(packet.encrypted_buffer, packet.encrypted_length)));
    for (const auto& frame : simple_framer_.crypto_frames()) {
      if (first_packet) {
        QuicStreamOffset max_crypto = frame->data_length + frame->offset;
        if (max_crypto > max_crypto_first_packet) {
          max_crypto_first_packet = max_crypto;
        }
      } else {
        QuicStreamOffset min_crypto = frame->offset;
        if (min_crypto < min_crypto_subsequent_packets) {
          min_crypto_subsequent_packets = min_crypto;
        }
      }
      num_crypto_frames++;
      QuicInterval<QuicStreamOffset> interval(
          frame->offset, frame->offset + frame->data_length);
      // Check that we don't repeat the same crypto data in different frames.
      ASSERT_TRUE(crypto_data_intervals.IsDisjoint(interval));
      crypto_data_intervals.Add(interval);
      for (QuicStreamOffset i = 0; i < frame->data_length; ++i) {
        // Check the crypto data itself is correct.
        EXPECT_EQ(frame->data_buffer[i],
                  static_cast<char>((frame->offset + i) & 0xFF))
            << "i = " << i << ", offset = " << frame->offset
            << ", data_length = " << frame->data_length;
      }
    }
    first_packet = false;
  }
  // Make sure that the combination of all crypto frames covers the entire data.
  EXPECT_EQ(crypto_data_intervals.Size(), 1u);
  EXPECT_EQ(*crypto_data_intervals.begin(),
            QuicInterval<QuicStreamOffset>(0, crypto_data_length));

  EXPECT_EQ(packets_.size(), num_packets);
  if (chaos_protection_enabled) {
    EXPECT_GT(num_crypto_frames, packets_.size() + 1);
  } else {
    EXPECT_EQ(num_crypto_frames, packets_.size());
  }
  // Check that multi-packet chaos protection was performed if and only if it
  // was expected.
  EXPECT_EQ(chaos_protection_enabled && num_packets > 1,
            max_crypto_first_packet > min_crypto_subsequent_packets);
}

void QuicPacketCreatorMultiplePacketsTest::SetupInitialCrypto(
    size_t crypto_data_length, int num_ack_blocks,
    bool chaos_protection_enabled) {
  SetQuicFlag(quic_enable_chaos_protection, chaos_protection_enabled);
  random_creator_.ResetBase(4);
  creator_.SetEncrypter(ENCRYPTION_INITIAL,
                        std::make_unique<TaggingEncrypter>(ENCRYPTION_INITIAL));
  creator_.set_encryption_level(ENCRYPTION_INITIAL);
  if (simple_framer_.framer()->version().KnowsWhichDecrypterToUse()) {
    simple_framer_.framer()->InstallDecrypter(
        ENCRYPTION_INITIAL, std::make_unique<TaggingDecrypter>());
  }
  delegate_.SetCanWriteAnything();

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillRepeatedly(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));

  if (num_ack_blocks > 0) {
    std::vector<QuicAckBlock> ack_blocks;
    for (int i = 1; i <= num_ack_blocks; ++i) {
      ack_blocks.push_back(
          {QuicPacketNumber(3 * i), QuicPacketNumber(3 * i + 1)});
    }
    ack_frame_ = InitAckFrame(ack_blocks);
    EXPECT_TRUE(creator_.AddFrame(QuicFrame(&ack_frame_), NOT_RETRANSMISSION));
    EXPECT_TRUE(creator_.HasPendingFrames());
  } else {
    EXPECT_FALSE(creator_.HasPendingFrames());
  }
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  if (crypto_data_length > 0) {
    const std::string data(crypto_data_length, '?');
    EXPECT_EQ(
        creator_.ConsumeCryptoData(ENCRYPTION_INITIAL, data, /*offset=*/0),
        data.size());
  }
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());
  EXPECT_EQ(kDefaultMaxPacketSize, creator_.max_packet_length());
}

void QuicPacketCreatorMultiplePacketsTest::CheckPackets(
    int num_ack_blocks, int num_packets, bool chaos_protection_expected) {
  ASSERT_EQ(packets_.size(), num_packets);
  // Check first packet.
  EXPECT_EQ(packets_[0].encrypted_length, kDefaultMaxPacketSize);
  ASSERT_TRUE(packets_[0].encrypted_buffer != nullptr);
  ASSERT_TRUE(simple_framer_.ProcessPacket(QuicEncryptedPacket(
      packets_[0].encrypted_buffer, packets_[0].encrypted_length)));
  EXPECT_GE(simple_framer_.crypto_frames().size(), 1u);
  EXPECT_EQ(simple_framer_.ack_frames().size(), num_ack_blocks > 0 ? 1u : 0u);
  QuicStreamOffset max_crypto_first_packet = 0;
  for (const auto& frame : simple_framer_.crypto_frames()) {
    QuicStreamOffset max_crypto = frame->data_length + frame->offset;
    if (max_crypto > max_crypto_first_packet) {
      max_crypto_first_packet = max_crypto;
    }
  }
  // Check subsequent packets.
  QuicStreamOffset min_crypto_subsequent_packets =
      std::numeric_limits<QuicStreamOffset>::max();
  for (int i = 1; i < num_packets; ++i) {
    simple_framer_.Reset();
    EXPECT_EQ(packets_[i].encrypted_length, kDefaultMaxPacketSize);
    ASSERT_TRUE(packets_[i].encrypted_buffer != nullptr);
    ASSERT_TRUE(simple_framer_.ProcessPacket(QuicEncryptedPacket(
        packets_[i].encrypted_buffer, packets_[i].encrypted_length)));
    EXPECT_GE(simple_framer_.crypto_frames().size(), 1u);
    EXPECT_EQ(simple_framer_.ack_frames().size(), 0u);
    for (const auto& frame : simple_framer_.crypto_frames()) {
      QuicStreamOffset min_crypto = frame->offset;
      if (min_crypto < min_crypto_subsequent_packets) {
        min_crypto_subsequent_packets = min_crypto;
      }
    }
  }
  EXPECT_EQ(chaos_protection_expected,
            max_crypto_first_packet > min_crypto_subsequent_packets);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest, ChaosProtectionWithPriorAcks) {
  // Ensure that multi-packet chaos protection takes into account any pending
  // non-retransmittable frames.
  if (!framer_.version().UsesCryptoFrames()) {
    return;
  }
  static constexpr int kNumAckBlocks = 100;
  // Size the crypto data such that it could fit in one packet by itself but
  // can't fit with the ack frame.
  static constexpr size_t kCryptoDataSize =
      kDefaultMaxPacketSize - 2 * kNumAckBlocks;
  SetupInitialCrypto(kCryptoDataSize, kNumAckBlocks,
                     /*chaos_protection_enabled=*/true);
  CheckPackets(kNumAckBlocks, /*num_packets=*/2,
               /*chaos_protection_expected=*/true);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest, ChaosProtectionFirstPacketFull) {
  // Ensure that chaos protection returns disabled early when the packet has
  // more pending data than the amount of crypto data per packet.
  if (!framer_.version().UsesCryptoFrames()) {
    return;
  }
  static constexpr int kNumAckBlocks = (kDefaultMaxPacketSize - 100) / 2;
  static constexpr size_t kCryptoDataSize = 2000;
  SetupInitialCrypto(kCryptoDataSize, kNumAckBlocks,
                     /*chaos_protection_enabled=*/true);
  CheckPackets(kNumAckBlocks, /*num_packets=*/3,
               /*chaos_protection_expected=*/false);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest, ChaosProtectionCantFitFirstFrame) {
  // Ensure that chaos protection disables itself if we can't fit the first
  // frame in the first packet.
  if (!framer_.version().UsesCryptoFrames()) {
    return;
  }
  static constexpr int kNumAckBlocks = (kDefaultMaxPacketSize - 100) / 2;
  static constexpr size_t kCryptoDataSize = 2400;
  SetupInitialCrypto(kCryptoDataSize, kNumAckBlocks,
                     /*chaos_protection_enabled=*/true);
  CheckPackets(kNumAckBlocks, /*num_packets=*/3,
               /*chaos_protection_expected=*/false);
}

// Test the behavior of ConsumeData when the data is for the crypto handshake
// stream, but padding is disabled.
TEST_F(QuicPacketCreatorMultiplePacketsTest,
       ConsumeData_Handshake_PaddingDisabled) {
  creator_.set_fully_pad_crypto_handshake_packets(false);

  delegate_.SetCanWriteAnything();

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
  const std::string data = "foo";
  size_t bytes_consumed = 0;
  if (QuicVersionUsesCryptoFrames(framer_.transport_version())) {
    bytes_consumed =
        creator_.ConsumeCryptoData(ENCRYPTION_FORWARD_SECURE, data, 0);
  } else {
    bytes_consumed =
        creator_
            .ConsumeData(
                QuicUtils::GetCryptoStreamId(framer_.transport_version()), data,
                0, NO_FIN)
            .bytes_consumed;
  }
  EXPECT_EQ(3u, bytes_consumed);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  PacketContents contents;
  if (QuicVersionUsesCryptoFrames(framer_.transport_version())) {
    contents.num_crypto_frames = 1;
  } else {
    contents.num_stream_frames = 1;
  }
  contents.num_padding_frames = 0;
  CheckPacketContains(contents, 0);

  ASSERT_EQ(1u, packets_.size());

  // Packet is not fully padded, but we want to future packets to be larger.
  ASSERT_EQ(kDefaultMaxPacketSize, creator_.max_packet_length());
  size_t expected_packet_length = 31;
  if (QuicVersionUsesCryptoFrames(framer_.transport_version())) {
    // The framing of CRYPTO frames is slightly different than that of stream
    // frames, so the expected packet length differs slightly.
    expected_packet_length = 32;
  }
  EXPECT_EQ(expected_packet_length, packets_[0].encrypted_length);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest, ConsumeData_EmptyData) {
  delegate_.SetCanWriteAnything();

  EXPECT_QUIC_BUG(creator_.ConsumeData(
                      QuicUtils::QuicUtils::GetFirstBidirectionalStreamId(
                          framer_.transport_version(), Perspective::IS_CLIENT),
                      {}, 0, NO_FIN),
                  "Attempt to consume empty data without FIN.");
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       ConsumeDataMultipleTimes_WritableAndShouldNotFlush) {
  delegate_.SetCanWriteAnything();

  creator_.ConsumeData(QuicUtils::GetFirstBidirectionalStreamId(
                           framer_.transport_version(), Perspective::IS_CLIENT),
                       "foo", 0, FIN);
  QuicConsumedData consumed = creator_.ConsumeData(3, "quux", 3, NO_FIN);
  EXPECT_EQ(4u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_TRUE(creator_.HasPendingRetransmittableFrames());
}

TEST_F(QuicPacketCreatorMultiplePacketsTest, ConsumeData_BatchOperations) {
  delegate_.SetCanWriteAnything();

  creator_.ConsumeData(QuicUtils::GetFirstBidirectionalStreamId(
                           framer_.transport_version(), Perspective::IS_CLIENT),
                       "foo", 0, NO_FIN);
  QuicConsumedData consumed = creator_.ConsumeData(
      QuicUtils::GetFirstBidirectionalStreamId(framer_.transport_version(),
                                               Perspective::IS_CLIENT),
      "quux", 3, FIN);
  EXPECT_EQ(4u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_TRUE(creator_.HasPendingRetransmittableFrames());

  // Now both frames will be flushed out.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
  creator_.Flush();
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  PacketContents contents;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, 0);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       ConsumeData_FramesPreviouslyQueued) {
  // Set the packet size be enough for two stream frames with 0 stream offset,
  // but not enough for a stream frame of 0 offset and one with non-zero offset.
  size_t length =
      TaggingEncrypter(0x00).GetCiphertextSize(0) +
      GetPacketHeaderSize(
          framer_.transport_version(),
          creator_.GetDestinationConnectionIdLength(),
          creator_.GetSourceConnectionIdLength(),
          QuicPacketCreatorPeer::SendVersionInPacket(&creator_),
          !kIncludeDiversificationNonce,
          QuicPacketCreatorPeer::GetPacketNumberLength(&creator_),
          QuicPacketCreatorPeer::GetRetryTokenLengthLength(&creator_), 0,
          QuicPacketCreatorPeer::GetLengthLength(&creator_)) +
      // Add an extra 3 bytes for the payload and 1 byte so
      // BytesFree is larger than the GetMinStreamFrameSize.
      QuicFramer::GetMinStreamFrameSize(framer_.transport_version(), 1, 0,
                                        false, 3) +
      3 +
      QuicFramer::GetMinStreamFrameSize(framer_.transport_version(), 1, 0, true,
                                        1) +
      1;
  creator_.SetMaxPacketLength(length);
  delegate_.SetCanWriteAnything();
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(
            Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(
            Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
  }
  // Queue enough data to prevent a stream frame with a non-zero offset from
  // fitting.
  QuicConsumedData consumed = creator_.ConsumeData(
      QuicUtils::GetFirstBidirectionalStreamId(framer_.transport_version(),
                                               Perspective::IS_CLIENT),
      "foo", 0, NO_FIN);
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_TRUE(creator_.HasPendingRetransmittableFrames());

  // This frame will not fit with the existing frame, causing the queued frame
  // to be serialized, and it will be added to a new open packet.
  consumed = creator_.ConsumeData(
      QuicUtils::GetFirstBidirectionalStreamId(framer_.transport_version(),
                                               Perspective::IS_CLIENT),
      "bar", 3, FIN);
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_TRUE(creator_.HasPendingRetransmittableFrames());

  creator_.FlushCurrentPacket();
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  PacketContents contents;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, 0);
  CheckPacketContains(contents, 1);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest, ConsumeDataFastPath) {
  delegate_.SetCanWriteAnything();
  creator_.SetTransmissionType(LOSS_RETRANSMISSION);

  const std::string data(10000, '?');
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillRepeatedly(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
  QuicConsumedData consumed = creator_.ConsumeDataFastPath(
      QuicUtils::GetFirstBidirectionalStreamId(framer_.transport_version(),
                                               Perspective::IS_CLIENT),
      data);
  EXPECT_EQ(10000u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  PacketContents contents;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, 0);
  EXPECT_FALSE(packets_.empty());
  SerializedPacket& packet = packets_.back();
  EXPECT_TRUE(!packet.retransmittable_frames.empty());
  EXPECT_EQ(LOSS_RETRANSMISSION, packet.transmission_type);
  EXPECT_EQ(STREAM_FRAME, packet.retransmittable_frames.front().type);
  const QuicStreamFrame& stream_frame =
      packet.retransmittable_frames.front().stream_frame;
  EXPECT_EQ(10000u, stream_frame.data_length + stream_frame.offset);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest, ConsumeDataLarge) {
  delegate_.SetCanWriteAnything();

  const std::string data(10000, '?');
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillRepeatedly(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
  QuicConsumedData consumed = creator_.ConsumeData(
      QuicUtils::GetFirstBidirectionalStreamId(framer_.transport_version(),
                                               Perspective::IS_CLIENT),
      data, 0, FIN);
  EXPECT_EQ(10000u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  PacketContents contents;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, 0);
  EXPECT_FALSE(packets_.empty());
  SerializedPacket& packet = packets_.back();
  EXPECT_TRUE(!packet.retransmittable_frames.empty());
  EXPECT_EQ(STREAM_FRAME, packet.retransmittable_frames.front().type);
  const QuicStreamFrame& stream_frame =
      packet.retransmittable_frames.front().stream_frame;
  EXPECT_EQ(10000u, stream_frame.data_length + stream_frame.offset);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest, ConsumeDataLargeSendAckFalse) {
  delegate_.SetCanNotWrite();

  QuicRstStreamFrame* rst_frame = CreateRstStreamFrame();
  const bool success =
      creator_.ConsumeRetransmittableControlFrame(QuicFrame(rst_frame),
                                                  /*bundle_ack=*/true);
  EXPECT_FALSE(success);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  delegate_.SetCanWriteAnything();

  creator_.ConsumeRetransmittableControlFrame(QuicFrame(rst_frame),
                                              /*bundle_ack=*/false);

  const std::string data(10000, '?');
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillRepeatedly(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
  creator_.ConsumeRetransmittableControlFrame(QuicFrame(CreateRstStreamFrame()),
                                              /*bundle_ack=*/true);
  QuicConsumedData consumed = creator_.ConsumeData(
      QuicUtils::GetFirstBidirectionalStreamId(framer_.transport_version(),
                                               Perspective::IS_CLIENT),
      data, 0, FIN);
  creator_.Flush();

  EXPECT_EQ(10000u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  EXPECT_FALSE(packets_.empty());
  SerializedPacket& packet = packets_.back();
  EXPECT_TRUE(!packet.retransmittable_frames.empty());
  EXPECT_EQ(STREAM_FRAME, packet.retransmittable_frames.front().type);
  const QuicStreamFrame& stream_frame =
      packet.retransmittable_frames.front().stream_frame;
  EXPECT_EQ(10000u, stream_frame.data_length + stream_frame.offset);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest, ConsumeDataLargeSendAckTrue) {
  delegate_.SetCanNotWrite();
  delegate_.SetCanWriteAnything();

  const std::string data(10000, '?');
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillRepeatedly(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
  QuicConsumedData consumed = creator_.ConsumeData(
      QuicUtils::GetFirstBidirectionalStreamId(framer_.transport_version(),
                                               Perspective::IS_CLIENT),
      data, 0, FIN);
  creator_.Flush();

  EXPECT_EQ(10000u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  EXPECT_FALSE(packets_.empty());
  SerializedPacket& packet = packets_.back();
  EXPECT_TRUE(!packet.retransmittable_frames.empty());
  EXPECT_EQ(STREAM_FRAME, packet.retransmittable_frames.front().type);
  const QuicStreamFrame& stream_frame =
      packet.retransmittable_frames.front().stream_frame;
  EXPECT_EQ(10000u, stream_frame.data_length + stream_frame.offset);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest, NotWritableThenBatchOperations) {
  delegate_.SetCanNotWrite();

  QuicRstStreamFrame* rst_frame = CreateRstStreamFrame();
  const bool consumed =
      creator_.ConsumeRetransmittableControlFrame(QuicFrame(rst_frame),
                                                  /*bundle_ack=*/true);
  EXPECT_FALSE(consumed);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());
  EXPECT_FALSE(creator_.HasPendingStreamFramesOfStream(3));

  delegate_.SetCanWriteAnything();

  EXPECT_TRUE(
      creator_.ConsumeRetransmittableControlFrame(QuicFrame(rst_frame),
                                                  /*bundle_ack=*/false));
  // Send some data and a control frame
  creator_.ConsumeData(3, "quux", 0, NO_FIN);
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    creator_.ConsumeRetransmittableControlFrame(QuicFrame(CreateGoAwayFrame()),
                                                /*bundle_ack=*/false);
  }
  EXPECT_TRUE(creator_.HasPendingStreamFramesOfStream(3));

  // All five frames will be flushed out in a single packet.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
  creator_.Flush();
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());
  EXPECT_FALSE(creator_.HasPendingStreamFramesOfStream(3));

  PacketContents contents;
  // ACK will be flushed by connection.
  contents.num_ack_frames = 0;
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    contents.num_goaway_frames = 1;
  } else {
    contents.num_goaway_frames = 0;
  }
  contents.num_rst_stream_frames = 1;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, 0);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest, NotWritableThenBatchOperations2) {
  delegate_.SetCanNotWrite();

  QuicRstStreamFrame* rst_frame = CreateRstStreamFrame();
  const bool success =
      creator_.ConsumeRetransmittableControlFrame(QuicFrame(rst_frame),
                                                  /*bundle_ack=*/true);
  EXPECT_FALSE(success);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  delegate_.SetCanWriteAnything();

  {
    InSequence dummy;
    // All five frames will be flushed out in a single packet
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(
            Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(
            Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
  }
  EXPECT_TRUE(
      creator_.ConsumeRetransmittableControlFrame(QuicFrame(rst_frame),
                                                  /*bundle_ack=*/false));
  // Send enough data to exceed one packet
  size_t data_len = kDefaultMaxPacketSize + 100;
  const std::string data(data_len, '?');
  QuicConsumedData consumed = creator_.ConsumeData(3, data, 0, FIN);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    creator_.ConsumeRetransmittableControlFrame(QuicFrame(CreateGoAwayFrame()),
                                                /*bundle_ack=*/false);
  }

  creator_.Flush();
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  // The first packet should have the queued data and part of the stream data.
  PacketContents contents;
  // ACK will be sent by connection.
  contents.num_ack_frames = 0;
  contents.num_rst_stream_frames = 1;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, 0);

  // The second should have the remainder of the stream data.
  PacketContents contents2;
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    contents2.num_goaway_frames = 1;
  } else {
    contents2.num_goaway_frames = 0;
  }
  contents2.num_stream_frames = 1;
  CheckPacketContains(contents2, 1);
}

// Regression test of b/120493795.
TEST_F(QuicPacketCreatorMultiplePacketsTest, PacketTransmissionType) {
  delegate_.SetCanWriteAnything();

  // The first ConsumeData will fill the packet without flush.
  creator_.SetTransmissionType(LOSS_RETRANSMISSION);

  size_t data_len = 1220;
  const std::string data(data_len, '?');
  QuicStreamId stream1_id = QuicUtils::GetFirstBidirectionalStreamId(
      framer_.transport_version(), Perspective::IS_CLIENT);
  QuicConsumedData consumed = creator_.ConsumeData(stream1_id, data, 0, NO_FIN);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  ASSERT_EQ(0u, creator_.BytesFree())
      << "Test setup failed: Please increase data_len to "
      << data_len + creator_.BytesFree() << " bytes.";

  // The second ConsumeData can not be added to the packet and will flush.
  creator_.SetTransmissionType(NOT_RETRANSMISSION);

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));

  QuicStreamId stream2_id = stream1_id + 4;

  consumed = creator_.ConsumeData(stream2_id, data, 0, NO_FIN);
  EXPECT_EQ(data_len, consumed.bytes_consumed);

  // Ensure the packet is successfully created.
  ASSERT_EQ(1u, packets_.size());
  ASSERT_TRUE(packets_[0].encrypted_buffer);
  ASSERT_EQ(1u, packets_[0].retransmittable_frames.size());
  EXPECT_EQ(stream1_id,
            packets_[0].retransmittable_frames[0].stream_frame.stream_id);

  // Since the second frame was not added, the packet's transmission type
  // should be the first frame's type.
  EXPECT_EQ(packets_[0].transmission_type, LOSS_RETRANSMISSION);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest, TestConnectionIdLength) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  creator_.SetServerConnectionIdLength(0);
  EXPECT_EQ(0, creator_.GetDestinationConnectionIdLength());

  for (size_t i = 1; i < 10; i++) {
    creator_.SetServerConnectionIdLength(i);
    EXPECT_EQ(0, creator_.GetDestinationConnectionIdLength());
  }
}

// Test whether SetMaxPacketLength() works in the situation when the queue is
// empty, and we send three packets worth of data.
TEST_F(QuicPacketCreatorMultiplePacketsTest, SetMaxPacketLength_Initial) {
  delegate_.SetCanWriteAnything();

  // Send enough data for three packets.
  size_t data_len = 3 * kDefaultMaxPacketSize + 1;
  size_t packet_len = kDefaultMaxPacketSize + 100;
  ASSERT_LE(packet_len, kMaxOutgoingPacketSize);
  creator_.SetMaxPacketLength(packet_len);
  EXPECT_EQ(packet_len, creator_.max_packet_length());

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .Times(3)
      .WillRepeatedly(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
  const std::string data(data_len, '?');
  QuicConsumedData consumed = creator_.ConsumeData(
      QuicUtils::GetFirstBidirectionalStreamId(framer_.transport_version(),
                                               Perspective::IS_CLIENT),
      data,
      /*offset=*/0, FIN);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  // We expect three packets, and first two of them have to be of packet_len
  // size.  We check multiple packets (instead of just one) because we want to
  // ensure that |max_packet_length_| does not get changed incorrectly by the
  // creator after first packet is serialized.
  ASSERT_EQ(3u, packets_.size());
  EXPECT_EQ(packet_len, packets_[0].encrypted_length);
  EXPECT_EQ(packet_len, packets_[1].encrypted_length);
  CheckAllPacketsHaveSingleStreamFrame();
}

// Test whether SetMaxPacketLength() works in the situation when we first write
// data, then change packet size, then write data again.
TEST_F(QuicPacketCreatorMultiplePacketsTest, SetMaxPacketLength_Middle) {
  delegate_.SetCanWriteAnything();

  // We send enough data to overflow default packet length, but not the altered
  // one.
  size_t data_len = kDefaultMaxPacketSize;
  size_t packet_len = kDefaultMaxPacketSize + 100;
  ASSERT_LE(packet_len, kMaxOutgoingPacketSize);

  // We expect to see three packets in total.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .Times(3)
      .WillRepeatedly(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));

  // Send two packets before packet size change.
  const std::string data(data_len, '?');
  QuicConsumedData consumed = creator_.ConsumeData(
      QuicUtils::GetFirstBidirectionalStreamId(framer_.transport_version(),
                                               Perspective::IS_CLIENT),
      data,
      /*offset=*/0, NO_FIN);
  creator_.Flush();
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  // Make sure we already have two packets.
  ASSERT_EQ(2u, packets_.size());

  // Increase packet size.
  creator_.SetMaxPacketLength(packet_len);
  EXPECT_EQ(packet_len, creator_.max_packet_length());

  // Send a packet after packet size change.
  creator_.AttachPacketFlusher();
  consumed = creator_.ConsumeData(
      QuicUtils::GetFirstBidirectionalStreamId(framer_.transport_version(),
                                               Perspective::IS_CLIENT),
      data, data_len, FIN);
  creator_.Flush();
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  // We expect first data chunk to get fragmented, but the second one to fit
  // into a single packet.
  ASSERT_EQ(3u, packets_.size());
  EXPECT_EQ(kDefaultMaxPacketSize, packets_[0].encrypted_length);
  EXPECT_LE(kDefaultMaxPacketSize, packets_[2].encrypted_length);
  CheckAllPacketsHaveSingleStreamFrame();
}

// Test whether SetMaxPacketLength() works correctly when we force the change of
// the packet size in the middle of the batched packet.
TEST_F(QuicPacketCreatorMultiplePacketsTest,
       SetMaxPacketLength_MidpacketFlush) {
  delegate_.SetCanWriteAnything();

  size_t first_write_len = kDefaultMaxPacketSize / 2;
  size_t packet_len = kDefaultMaxPacketSize + 100;
  size_t second_write_len = packet_len + 1;
  ASSERT_LE(packet_len, kMaxOutgoingPacketSize);

  // First send half of the packet worth of data.  We are in the batch mode, so
  // should not cause packet serialization.
  const std::string first_write(first_write_len, '?');
  QuicConsumedData consumed = creator_.ConsumeData(
      QuicUtils::GetFirstBidirectionalStreamId(framer_.transport_version(),
                                               Perspective::IS_CLIENT),
      first_write,
      /*offset=*/0, NO_FIN);
  EXPECT_EQ(first_write_len, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_TRUE(creator_.HasPendingRetransmittableFrames());

  // Make sure we have no packets so far.
  ASSERT_EQ(0u, packets_.size());

  // Expect a packet to be flushed.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));

  // Increase packet size after flushing all frames.
  // Ensure it's immediately enacted.
  creator_.FlushCurrentPacket();
  creator_.SetMaxPacketLength(packet_len);
  EXPECT_EQ(packet_len, creator_.max_packet_length());
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  // We expect to see exactly one packet serialized after that, because we send
  // a value somewhat exceeding new max packet size, and the tail data does not
  // get serialized because we are still in the batch mode.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));

  // Send a more than a packet worth of data to the same stream.  This should
  // trigger serialization of one packet, and queue another one.
  const std::string second_write(second_write_len, '?');
  consumed = creator_.ConsumeData(
      QuicUtils::GetFirstBidirectionalStreamId(framer_.transport_version(),
                                               Perspective::IS_CLIENT),
      second_write,
      /*offset=*/first_write_len, FIN);
  EXPECT_EQ(second_write_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_TRUE(creator_.HasPendingRetransmittableFrames());

  // We expect the first packet to be underfilled, and the second packet be up
  // to the new max packet size.
  ASSERT_EQ(2u, packets_.size());
  EXPECT_GT(kDefaultMaxPacketSize, packets_[0].encrypted_length);
  EXPECT_EQ(packet_len, packets_[1].encrypted_length);

  CheckAllPacketsHaveSingleStreamFrame();
}

// Test sending a connectivity probing packet.
TEST_F(QuicPacketCreatorMultiplePacketsTest,
       GenerateConnectivityProbingPacket) {
  delegate_.SetCanWriteAnything();

  std::unique_ptr<SerializedPacket> probing_packet;
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    QuicPathFrameBuffer payload = {
        {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xfe}};
    probing_packet =
        creator_.SerializePathChallengeConnectivityProbingPacket(payload);
  } else {
    probing_packet = creator_.SerializeConnectivityProbingPacket();
  }

  ASSERT_TRUE(simple_framer_.ProcessPacket(QuicEncryptedPacket(
      probing_packet->encrypted_buffer, probing_packet->encrypted_length)));

  EXPECT_EQ(2u, simple_framer_.num_frames());
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    EXPECT_EQ(1u, simple_framer_.path_challenge_frames().size());
  } else {
    EXPECT_EQ(1u, simple_framer_.ping_frames().size());
  }
  EXPECT_EQ(1u, simple_framer_.padding_frames().size());
}

// Test sending an MTU probe, without any surrounding data.
TEST_F(QuicPacketCreatorMultiplePacketsTest,
       GenerateMtuDiscoveryPacket_Simple) {
  delegate_.SetCanWriteAnything();

  const size_t target_mtu = kDefaultMaxPacketSize + 100;
  static_assert(target_mtu < kMaxOutgoingPacketSize,
                "The MTU probe used by the test exceeds maximum packet size");

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));

  creator_.GenerateMtuDiscoveryPacket(target_mtu);

  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());
  ASSERT_EQ(1u, packets_.size());
  EXPECT_EQ(target_mtu, packets_[0].encrypted_length);

  PacketContents contents;
  contents.num_mtu_discovery_frames = 1;
  contents.num_padding_frames = 1;
  CheckPacketContains(contents, 0);
}

// Test sending an MTU probe.  Surround it with data, to ensure that it resets
// the MTU to the value before the probe was sent.
TEST_F(QuicPacketCreatorMultiplePacketsTest,
       GenerateMtuDiscoveryPacket_SurroundedByData) {
  delegate_.SetCanWriteAnything();

  const size_t target_mtu = kDefaultMaxPacketSize + 100;
  static_assert(target_mtu < kMaxOutgoingPacketSize,
                "The MTU probe used by the test exceeds maximum packet size");

  // Send enough data so it would always cause two packets to be sent.
  const size_t data_len = target_mtu + 1;

  // Send a total of five packets: two packets before the probe, the probe
  // itself, and two packets after the probe.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .Times(5)
      .WillRepeatedly(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));

  // Send data before the MTU probe.
  const std::string data(data_len, '?');
  QuicConsumedData consumed = creator_.ConsumeData(
      QuicUtils::GetFirstBidirectionalStreamId(framer_.transport_version(),
                                               Perspective::IS_CLIENT),
      data,
      /*offset=*/0, NO_FIN);
  creator_.Flush();
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  // Send the MTU probe.
  creator_.GenerateMtuDiscoveryPacket(target_mtu);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  // Send data after the MTU probe.
  creator_.AttachPacketFlusher();
  consumed = creator_.ConsumeData(
      QuicUtils::GetFirstBidirectionalStreamId(framer_.transport_version(),
                                               Perspective::IS_CLIENT),
      data,
      /*offset=*/data_len, FIN);
  creator_.Flush();
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  ASSERT_EQ(5u, packets_.size());
  EXPECT_EQ(kDefaultMaxPacketSize, packets_[0].encrypted_length);
  EXPECT_EQ(target_mtu, packets_[2].encrypted_length);
  EXPECT_EQ(kDefaultMaxPacketSize, packets_[3].encrypted_length);

  PacketContents probe_contents;
  probe_contents.num_mtu_discovery_frames = 1;
  probe_contents.num_padding_frames = 1;

  CheckPacketHasSingleStreamFrame(0);
  CheckPacketHasSingleStreamFrame(1);
  CheckPacketContains(probe_contents, 2);
  CheckPacketHasSingleStreamFrame(3);
  CheckPacketHasSingleStreamFrame(4);
}

// Regression test for b/31486443.
TEST_F(QuicPacketCreatorMultiplePacketsTest,
       ConnectionCloseFrameLargerThanPacketSize) {
  delegate_.SetCanWriteAnything();
  char buf[2000] = {};
  absl::string_view error_details(buf, 2000);
  const QuicErrorCode kQuicErrorCode = QUIC_PACKET_WRITE_ERROR;

  QuicConnectionCloseFrame* frame = new QuicConnectionCloseFrame(
      framer_.transport_version(), kQuicErrorCode, NO_IETF_QUIC_ERROR,
      std::string(error_details),
      /*transport_close_frame_type=*/0);
  creator_.ConsumeRetransmittableControlFrame(QuicFrame(frame),
                                              /*bundle_ack=*/false);
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_TRUE(creator_.HasPendingRetransmittableFrames());
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       RandomPaddingAfterFinSingleStreamSinglePacket) {
  const QuicByteCount kStreamFramePayloadSize = 100u;
  char buf[kStreamFramePayloadSize] = {};
  const QuicStreamId kDataStreamId = 5;
  // Set the packet size be enough for one stream frame with 0 stream offset and
  // max size of random padding.
  size_t length =
      TaggingEncrypter(0x00).GetCiphertextSize(0) +
      GetPacketHeaderSize(
          framer_.transport_version(),
          creator_.GetDestinationConnectionIdLength(),
          creator_.GetSourceConnectionIdLength(),
          QuicPacketCreatorPeer::SendVersionInPacket(&creator_),
          !kIncludeDiversificationNonce,
          QuicPacketCreatorPeer::GetPacketNumberLength(&creator_),
          QuicPacketCreatorPeer::GetRetryTokenLengthLength(&creator_), 0,
          QuicPacketCreatorPeer::GetLengthLength(&creator_)) +
      QuicFramer::GetMinStreamFrameSize(
          framer_.transport_version(), kDataStreamId, 0,
          /*last_frame_in_packet=*/false,
          kStreamFramePayloadSize + kMaxNumRandomPaddingBytes) +
      kStreamFramePayloadSize + kMaxNumRandomPaddingBytes;
  creator_.SetMaxPacketLength(length);
  delegate_.SetCanWriteAnything();
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
  QuicConsumedData consumed = creator_.ConsumeData(
      kDataStreamId, absl::string_view(buf, kStreamFramePayloadSize), 0,
      FIN_AND_PADDING);
  creator_.Flush();
  EXPECT_EQ(kStreamFramePayloadSize, consumed.bytes_consumed);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  EXPECT_EQ(1u, packets_.size());
  PacketContents contents;
  // The packet has both stream and padding frames.
  contents.num_padding_frames = 1;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, 0);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       RandomPaddingAfterFinSingleStreamMultiplePackets) {
  const QuicByteCount kStreamFramePayloadSize = 100u;
  char buf[kStreamFramePayloadSize] = {};
  const QuicStreamId kDataStreamId = 5;
  // Set the packet size be enough for one stream frame with 0 stream offset +
  // 1. One or more packets will accommodate.
  size_t length =
      TaggingEncrypter(0x00).GetCiphertextSize(0) +
      GetPacketHeaderSize(
          framer_.transport_version(),
          creator_.GetDestinationConnectionIdLength(),
          creator_.GetSourceConnectionIdLength(),
          QuicPacketCreatorPeer::SendVersionInPacket(&creator_),
          !kIncludeDiversificationNonce,
          QuicPacketCreatorPeer::GetPacketNumberLength(&creator_),
          QuicPacketCreatorPeer::GetRetryTokenLengthLength(&creator_), 0,
          QuicPacketCreatorPeer::GetLengthLength(&creator_)) +
      QuicFramer::GetMinStreamFrameSize(
          framer_.transport_version(), kDataStreamId, 0,
          /*last_frame_in_packet=*/false, kStreamFramePayloadSize + 1) +
      kStreamFramePayloadSize + 1;
  creator_.SetMaxPacketLength(length);
  delegate_.SetCanWriteAnything();
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillRepeatedly(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
  QuicConsumedData consumed = creator_.ConsumeData(
      kDataStreamId, absl::string_view(buf, kStreamFramePayloadSize), 0,
      FIN_AND_PADDING);
  creator_.Flush();
  EXPECT_EQ(kStreamFramePayloadSize, consumed.bytes_consumed);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  EXPECT_LE(1u, packets_.size());
  PacketContents contents;
  // The first packet has both stream and padding frames.
  contents.num_stream_frames = 1;
  contents.num_padding_frames = 1;
  CheckPacketContains(contents, 0);

  for (size_t i = 1; i < packets_.size(); ++i) {
    // Following packets only have paddings.
    contents.num_stream_frames = 0;
    contents.num_padding_frames = 1;
    CheckPacketContains(contents, i);
  }
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       RandomPaddingAfterFinMultipleStreamsMultiplePackets) {
  const QuicByteCount kStreamFramePayloadSize = 100u;
  char buf[kStreamFramePayloadSize] = {};
  const QuicStreamId kDataStreamId1 = 5;
  const QuicStreamId kDataStreamId2 = 6;
  // Set the packet size be enough for first frame with 0 stream offset + second
  // frame + 1 byte payload. two or more packets will accommodate.
  size_t length =
      TaggingEncrypter(0x00).GetCiphertextSize(0) +
      GetPacketHeaderSize(
          framer_.transport_version(),
          creator_.GetDestinationConnectionIdLength(),
          creator_.GetSourceConnectionIdLength(),
          QuicPacketCreatorPeer::SendVersionInPacket(&creator_),
          !kIncludeDiversificationNonce,
          QuicPacketCreatorPeer::GetPacketNumberLength(&creator_),
          QuicPacketCreatorPeer::GetRetryTokenLengthLength(&creator_), 0,
          QuicPacketCreatorPeer::GetLengthLength(&creator_)) +
      QuicFramer::GetMinStreamFrameSize(
          framer_.transport_version(), kDataStreamId1, 0,
          /*last_frame_in_packet=*/false, kStreamFramePayloadSize) +
      kStreamFramePayloadSize +
      QuicFramer::GetMinStreamFrameSize(framer_.transport_version(),
                                        kDataStreamId1, 0,
                                        /*last_frame_in_packet=*/false, 1) +
      1;
  creator_.SetMaxPacketLength(length);
  delegate_.SetCanWriteAnything();
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillRepeatedly(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
  QuicConsumedData consumed = creator_.ConsumeData(
      kDataStreamId1, absl::string_view(buf, kStreamFramePayloadSize), 0,
      FIN_AND_PADDING);
  EXPECT_EQ(kStreamFramePayloadSize, consumed.bytes_consumed);
  consumed = creator_.ConsumeData(
      kDataStreamId2, absl::string_view(buf, kStreamFramePayloadSize), 0,
      FIN_AND_PADDING);
  EXPECT_EQ(kStreamFramePayloadSize, consumed.bytes_consumed);
  creator_.Flush();
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingRetransmittableFrames());

  EXPECT_LE(2u, packets_.size());
  PacketContents contents;
  // The first packet has two stream frames.
  contents.num_stream_frames = 2;
  CheckPacketContains(contents, 0);

  // The second packet has one stream frame and padding frames.
  contents.num_stream_frames = 1;
  contents.num_padding_frames = 1;
  CheckPacketContains(contents, 1);

  for (size_t i = 2; i < packets_.size(); ++i) {
    // Following packets only have paddings.
    contents.num_stream_frames = 0;
    contents.num_padding_frames = 1;
    CheckPacketContains(contents, i);
  }
}

TEST_F(QuicPacketCreatorMultiplePacketsTest, AddMessageFrame) {
  if (framer_.version().UsesTls()) {
    creator_.SetMaxDatagramFrameSize(kMaxAcceptedDatagramFrameSize);
  }
  delegate_.SetCanWriteAnything();
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));

  creator_.ConsumeData(QuicUtils::GetFirstBidirectionalStreamId(
                           framer_.transport_version(), Perspective::IS_CLIENT),
                       "foo", 0, FIN);
  EXPECT_EQ(MESSAGE_STATUS_SUCCESS,
            creator_.AddMessageFrame(1, MemSliceFromString("message")));
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_TRUE(creator_.HasPendingRetransmittableFrames());

  // Add a message which causes the flush of current packet.
  EXPECT_EQ(MESSAGE_STATUS_SUCCESS,
            creator_.AddMessageFrame(
                2, MemSliceFromString(std::string(
                       creator_.GetCurrentLargestMessagePayload(), 'a'))));
  EXPECT_TRUE(creator_.HasPendingRetransmittableFrames());

  // Failed to send messages which cannot fit into one packet.
  EXPECT_EQ(MESSAGE_STATUS_TOO_LARGE,
            creator_.AddMessageFrame(
                3, MemSliceFromString(std::string(
                       creator_.GetCurrentLargestMessagePayload() + 10, 'a'))));
}

TEST_F(QuicPacketCreatorMultiplePacketsTest, ConnectionId) {
  creator_.SetServerConnectionId(TestConnectionId(0x1337));
  EXPECT_EQ(TestConnectionId(0x1337), creator_.GetDestinationConnectionId());
  EXPECT_EQ(EmptyQuicConnectionId(), creator_.GetSourceConnectionId());
  if (!framer_.version().SupportsClientConnectionIds()) {
    return;
  }
  creator_.SetClientConnectionId(TestConnectionId(0x33));
  EXPECT_EQ(TestConnectionId(0x1337), creator_.GetDestinationConnectionId());
  EXPECT_EQ(TestConnectionId(0x33), creator_.GetSourceConnectionId());
}

// Regresstion test for b/159812345.
TEST_F(QuicPacketCreatorMultiplePacketsTest, ExtraPaddingNeeded) {
  if (!framer_.version().HasHeaderProtection()) {
    return;
  }
  delegate_.SetCanWriteAnything();
  // If the packet number length > 1, we won't get padding.
  EXPECT_EQ(QuicPacketCreatorPeer::GetPacketNumberLength(&creator_),
            PACKET_1BYTE_PACKET_NUMBER);
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(
          Invoke(this, &QuicPacketCreatorMultiplePacketsTest::SavePacket));
  // with no data and no offset, this is a 2B STREAM frame.
  creator_.ConsumeData(QuicUtils::GetFirstBidirectionalStreamId(
                           framer_.transport_version(), Perspective::IS_CLIENT),
                       "", 0, FIN);
  creator_.Flush();
  ASSERT_FALSE(packets_[0].nonretransmittable_frames.empty());
  QuicFrame padding = packets_[0].nonretransmittable_frames[0];
  // Verify stream frame expansion is excluded.
  EXPECT_EQ(padding.padding_frame.num_padding_bytes, 1);
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       PeerAddressContextWithSameAddress) {
  QuicConnectionId client_connection_id = TestConnectionId(1);
  QuicConnectionId server_connection_id = TestConnectionId(2);
  QuicSocketAddress peer_addr(QuicIpAddress::Any4(), 12345);
  creator_.SetDefaultPeerAddress(peer_addr);
  creator_.SetClientConnectionId(client_connection_id);
  creator_.SetServerConnectionId(server_connection_id);
  // Send some stream data.
  EXPECT_CALL(delegate_, ShouldGeneratePacket(_, _))
      .WillRepeatedly(Return(true));
  EXPECT_EQ(3u, creator_
                    .ConsumeData(QuicUtils::GetFirstBidirectionalStreamId(
                                     creator_.transport_version(),
                                     Perspective::IS_CLIENT),
                                 "foo", 0, NO_FIN)
                    .bytes_consumed);
  EXPECT_TRUE(creator_.HasPendingFrames());
  {
    // Set the same address via context which should not trigger flush.
    QuicPacketCreator::ScopedPeerAddressContext context(
        &creator_, peer_addr, client_connection_id, server_connection_id);
    ASSERT_EQ(client_connection_id, creator_.GetClientConnectionId());
    ASSERT_EQ(server_connection_id, creator_.GetServerConnectionId());
    EXPECT_TRUE(creator_.HasPendingFrames());
    // Queue another STREAM_FRAME.
    EXPECT_EQ(3u, creator_
                      .ConsumeData(QuicUtils::GetFirstBidirectionalStreamId(
                                       creator_.transport_version(),
                                       Perspective::IS_CLIENT),
                                   "foo", 0, FIN)
                      .bytes_consumed);
  }
  // After exiting the scope, the last queued frame should be flushed.
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke([=](SerializedPacket packet) {
        EXPECT_EQ(peer_addr, packet.peer_address);
        ASSERT_EQ(2u, packet.retransmittable_frames.size());
        EXPECT_EQ(STREAM_FRAME, packet.retransmittable_frames.front().type);
        EXPECT_EQ(STREAM_FRAME, packet.retransmittable_frames.back().type);
      }));
  creator_.FlushCurrentPacket();
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       PeerAddressContextWithDifferentAddress) {
  QuicSocketAddress peer_addr(QuicIpAddress::Any4(), 12345);
  creator_.SetDefaultPeerAddress(peer_addr);
  // Send some stream data.
  EXPECT_CALL(delegate_, ShouldGeneratePacket(_, _))
      .WillRepeatedly(Return(true));
  EXPECT_EQ(3u, creator_
                    .ConsumeData(QuicUtils::GetFirstBidirectionalStreamId(
                                     creator_.transport_version(),
                                     Perspective::IS_CLIENT),
                                 "foo", 0, NO_FIN)
                    .bytes_consumed);

  QuicSocketAddress peer_addr1(QuicIpAddress::Any4(), 12346);
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke([=](SerializedPacket packet) {
        EXPECT_EQ(peer_addr, packet.peer_address);
        ASSERT_EQ(1u, packet.retransmittable_frames.size());
        EXPECT_EQ(STREAM_FRAME, packet.retransmittable_frames.front().type);
      }))
      .WillOnce(Invoke([=](SerializedPacket packet) {
        EXPECT_EQ(peer_addr1, packet.peer_address);
        ASSERT_EQ(1u, packet.retransmittable_frames.size());
        EXPECT_EQ(STREAM_FRAME, packet.retransmittable_frames.front().type);
      }));
  EXPECT_TRUE(creator_.HasPendingFrames());
  {
    QuicConnectionId client_connection_id = TestConnectionId(1);
    QuicConnectionId server_connection_id = TestConnectionId(2);
    // Set a different address via context which should trigger flush.
    QuicPacketCreator::ScopedPeerAddressContext context(
        &creator_, peer_addr1, client_connection_id, server_connection_id);
    ASSERT_EQ(client_connection_id, creator_.GetClientConnectionId());
    ASSERT_EQ(server_connection_id, creator_.GetServerConnectionId());
    EXPECT_FALSE(creator_.HasPendingFrames());
    // Queue another STREAM_FRAME.
    EXPECT_EQ(3u, creator_
                      .ConsumeData(QuicUtils::GetFirstBidirectionalStreamId(
                                       creator_.transport_version(),
                                       Perspective::IS_CLIENT),
                                   "foo", 0, FIN)
                      .bytes_consumed);
    EXPECT_TRUE(creator_.HasPendingFrames());
  }
  // After exiting the scope, the last queued frame should be flushed.
  EXPECT_FALSE(creator_.HasPendingFrames());
}

TEST_F(QuicPacketCreatorMultiplePacketsTest,
       NestedPeerAddressContextWithDifferentAddress) {
  QuicConnectionId client_connection_id1 = creator_.GetClientConnectionId();
  QuicConnectionId server_connection_id1 = creator_.GetServerConnectionId();
  QuicSocketAddress peer_addr(QuicIpAddress::Any4(), 12345);
  creator_.SetDefaultPeerAddress(peer_addr);
  QuicPacketCreator::ScopedPeerAddressContext context(
      &creator_, peer_addr, client_connection_id1, server_connection_id1);
  ASSERT_EQ(client_connection_id1, creator_.GetClientConnectionId());
  ASSERT_EQ(server_connection_id1, creator_.GetServerConnectionId());

  // Send some stream data.
  EXPECT_CALL(delegate_, ShouldGeneratePacket(_, _))
      .WillRepeatedly(Return(true));
  EXPECT_EQ(3u, creator_
                    .ConsumeData(QuicUtils::GetFirstBidirectionalStreamId(
                                     creator_.transport_version(),
                                     Perspective::IS_CLIENT),
                                 "foo", 0, NO_FIN)
                    .bytes_consumed);
  EXPECT_TRUE(creator_.HasPendingFrames());

  QuicSocketAddress peer_addr1(QuicIpAddress::Any4(), 12346);
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke([=, this](SerializedPacket packet) {
        EXPECT_EQ(peer_addr, packet.peer_address);
        ASSERT_EQ(1u, packet.retransmittable_frames.size());
        EXPECT_EQ(STREAM_FRAME, packet.retransmittable_frames.front().type);

        QuicConnectionId client_connection_id2 = TestConnectionId(3);
        QuicConnectionId server_connection_id2 = TestConnectionId(4);
        // Set up another context with a different address.
        QuicPacketCreator::ScopedPeerAddressContext context(
            &creator_, peer_addr1, client_connection_id2,
            server_connection_id2);
        ASSERT_EQ(client_connection_id2, creator_.GetClientConnectionId());
        ASSERT_EQ(server_connection_id2, creator_.GetServerConnectionId());
        EXPECT_CALL(delegate_, ShouldGeneratePacket(_, _))
            .WillRepeatedly(Return(true));
        EXPECT_EQ(3u, creator_
                          .ConsumeData(QuicUtils::GetFirstBidirectionalStreamId(
                                           creator_.transport_version(),
                                           Perspective::IS_CLIENT),
                                       "foo", 0, NO_FIN)
                          .bytes_consumed);
        EXPECT_TRUE(creator_.HasPendingFrames());
        // This should trigger another OnSerializedPacket() with the 2nd
        // address.
        creator_.FlushCurrentPacket();
      }))
      .WillOnce(Invoke([=](SerializedPacket packet) {
        EXPECT_EQ(peer_addr1, packet.peer_address);
        ASSERT_EQ(1u, packet.retransmittable_frames.size());
        EXPECT_EQ(STREAM_FRAME, packet.retransmittable_frames.front().type);
      }));
  creator_.FlushCurrentPacket();
}

}  // namespace
}  // namespace test
}  // namespace quic
