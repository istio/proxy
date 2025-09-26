// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_chaos_protector.h"

#include <cstddef>
#include <memory>
#include <optional>

#include "absl/strings/string_view.h"
#include "quiche/quic/core/frames/quic_crypto_frame.h"
#include "quiche/quic/core/quic_connection_id.h"
#include "quiche/quic/core/quic_framer.h"
#include "quiche/quic/core/quic_interval_set.h"
#include "quiche/quic/core/quic_packet_number.h"
#include "quiche/quic/core/quic_packets.h"
#include "quiche/quic/core/quic_stream_frame_data_producer.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/mock_random.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/quic/test_tools/simple_quic_framer.h"

namespace quic {
namespace test {

// Sequence of frames to be chaos protected.
enum class InputFramesPattern {
  kCryptoAndPadding,
  kCryptoCryptoAndPadding,
  kReorderedCryptoCryptoAndPadding,
  kAckCryptoAndPadding,
};

class QuicChaosProtectorTest : public QuicTestWithParam<ParsedQuicVersion>,
                               public QuicStreamFrameDataProducer {
 public:
  QuicChaosProtectorTest()
      : version_(GetParam()),
        framer_({version_}, QuicTime::Zero(), Perspective::IS_CLIENT,
                kQuicDefaultConnectionIdLength),
        validation_framer_({version_}),
        random_(/*base=*/3),
        level_(ENCRYPTION_INITIAL),
        crypto_offset_(0),
        crypto_data_length_(100),
        num_padding_bytes_(50),
        packet_size_(1000),
        packet_buffer_(std::make_unique<char[]>(packet_size_)) {
    ReCreateChaosProtector();
  }

  void TearDown() override {
    // Verify that the output crypto frames are disjoint and when concatenated,
    // the crypto data covers the range
    // [crypto_offset_, crypto_offset_+crypto_data_length_).
    QuicIntervalSet<QuicStreamOffset> crypto_data_intervals;
    for (size_t i = 0; i < validation_framer_.crypto_frames().size(); ++i) {
      const QuicCryptoFrame& frame = *validation_framer_.crypto_frames()[i];
      QuicInterval<QuicStreamOffset> interval(frame.offset,
                                              frame.offset + frame.data_length);
      ASSERT_TRUE(crypto_data_intervals.IsDisjoint(interval));
      crypto_data_intervals.Add(interval);
      for (QuicStreamOffset j = 0; j < frame.data_length; ++j) {
        EXPECT_EQ(frame.data_buffer[j],
                  static_cast<char>((frame.offset + j) & 0xFF))
            << "i = " << i << ", j = " << j << ", offset = " << frame.offset
            << ", data_length = " << frame.data_length;
      }
    }
    EXPECT_EQ(crypto_data_intervals.Size(), 1u);
    EXPECT_EQ(*crypto_data_intervals.begin(),
              QuicInterval<QuicStreamOffset>(
                  crypto_offset_, crypto_offset_ + crypto_data_length_));
  }

  void ReCreateChaosProtector() {
    chaos_protector_ = std::make_unique<QuicChaosProtector>(
        packet_size_, level_, SetupHeaderAndFramers(), &random_);
  }

  // From QuicStreamFrameDataProducer.
  WriteStreamDataResult WriteStreamData(QuicStreamId /*id*/,
                                        QuicStreamOffset /*offset*/,
                                        QuicByteCount /*data_length*/,
                                        QuicDataWriter* /*writer*/) override {
    ADD_FAILURE() << "This should never be called";
    return STREAM_MISSING;
  }

  // From QuicStreamFrameDataProducer.
  bool WriteCryptoData(EncryptionLevel level, QuicStreamOffset offset,
                       QuicByteCount data_length,
                       QuicDataWriter* writer) override {
    EXPECT_EQ(level, level);
    for (QuicByteCount i = 0; i < data_length; i++) {
      EXPECT_TRUE(writer->WriteUInt8(static_cast<uint8_t>((offset + i) & 0xFF)))
          << i;
    }
    return true;
  }

 protected:
  QuicFramer* SetupHeaderAndFramers() {
    // Setup header.
    header_.destination_connection_id = TestConnectionId();
    header_.destination_connection_id_included = CONNECTION_ID_PRESENT;
    header_.source_connection_id = EmptyQuicConnectionId();
    header_.source_connection_id_included = CONNECTION_ID_PRESENT;
    header_.reset_flag = false;
    header_.version_flag = true;
    header_.has_possible_stateless_reset_token = false;
    header_.packet_number_length = PACKET_4BYTE_PACKET_NUMBER;
    header_.version = version_;
    header_.packet_number = QuicPacketNumber(1);
    header_.form = IETF_QUIC_LONG_HEADER_PACKET;
    header_.long_packet_type = INITIAL;
    header_.retry_token_length_length =
        quiche::VARIABLE_LENGTH_INTEGER_LENGTH_1;
    header_.length_length = quiche::kQuicheDefaultLongHeaderLengthLength;
    // Setup validation framer.
    validation_framer_.framer()->SetInitialObfuscators(
        header_.destination_connection_id);
    // Setup framer.
    framer_.SetInitialObfuscators(header_.destination_connection_id);
    framer_.set_data_producer(this);
    return &framer_;
  }

  void BuildEncryptAndParse() {
    QuicFrames frames;
    switch (input_frames_pattern_) {
      case InputFramesPattern::kCryptoAndPadding: {
        frames.push_back(QuicFrame(
            new QuicCryptoFrame(level_, crypto_offset_, crypto_data_length_)));
        frames.push_back(QuicFrame(QuicPaddingFrame(num_padding_bytes_)));
        break;
      }
      case InputFramesPattern::kCryptoCryptoAndPadding: {
        const QuicByteCount first_crypto_frame_length = crypto_data_length_ / 4;
        frames.push_back(QuicFrame(new QuicCryptoFrame(
            level_, crypto_offset_, first_crypto_frame_length)));
        frames.push_back(QuicFrame(new QuicCryptoFrame(
            level_, crypto_offset_ + first_crypto_frame_length,
            crypto_data_length_ - first_crypto_frame_length)));
        frames.push_back(QuicFrame(QuicPaddingFrame(num_padding_bytes_)));
        break;
      }
      case InputFramesPattern::kReorderedCryptoCryptoAndPadding: {
        const QuicByteCount first_crypto_frame_length = crypto_data_length_ / 4;
        frames.push_back(QuicFrame(new QuicCryptoFrame(
            level_, crypto_offset_ + first_crypto_frame_length,
            crypto_data_length_ - first_crypto_frame_length)));
        frames.push_back(QuicFrame(new QuicCryptoFrame(
            level_, crypto_offset_, first_crypto_frame_length)));
        frames.push_back(QuicFrame(QuicPaddingFrame(num_padding_bytes_)));
        break;
      }
      case InputFramesPattern::kAckCryptoAndPadding: {
        QuicAckFrame* ack_frame = new QuicAckFrame();
        ack_frame->largest_acked = QuicPacketNumber(1);
        ack_frame->packets.Add(ack_frame->largest_acked);
        frames.push_back(QuicFrame(ack_frame));
        frames.push_back(QuicFrame(
            new QuicCryptoFrame(level_, crypto_offset_, crypto_data_length_)));
        frames.push_back(QuicFrame(QuicPaddingFrame(num_padding_bytes_)));
        break;
      }
    }

    std::optional<size_t> length = chaos_protector_->BuildDataPacket(
        header_, frames, packet_buffer_.get());
    DeleteFrames(&frames);
    ASSERT_TRUE(length.has_value());
    ASSERT_GT(length.value(), 0u);
    size_t encrypted_length = framer_.EncryptInPlace(
        level_, header_.packet_number,
        GetStartOfEncryptedData(framer_.transport_version(), header_),
        length.value(), packet_size_, packet_buffer_.get());
    ASSERT_GT(encrypted_length, 0u);
    ASSERT_TRUE(validation_framer_.ProcessPacket(QuicEncryptedPacket(
        absl::string_view(packet_buffer_.get(), encrypted_length))));
  }

  void ResetOffset(QuicStreamOffset offset) {
    crypto_offset_ = offset;
    ReCreateChaosProtector();
  }

  void ResetLength(QuicByteCount length) {
    crypto_data_length_ = length;
    ReCreateChaosProtector();
  }

  ParsedQuicVersion version_;
  QuicPacketHeader header_;
  QuicFramer framer_;
  SimpleQuicFramer validation_framer_;
  ::testing::NiceMock<MockRandom> random_;
  EncryptionLevel level_;
  InputFramesPattern input_frames_pattern_ =
      InputFramesPattern::kCryptoAndPadding;
  QuicStreamOffset crypto_offset_;
  QuicByteCount crypto_data_length_;
  int num_padding_bytes_;
  size_t packet_size_;
  std::unique_ptr<char[]> packet_buffer_;
  std::unique_ptr<QuicChaosProtector> chaos_protector_;
};

namespace {

ParsedQuicVersionVector TestVersions() {
  ParsedQuicVersionVector versions;
  for (const ParsedQuicVersion& version : AllSupportedVersions()) {
    if (version.UsesCryptoFrames()) {
      versions.push_back(version);
    }
  }
  return versions;
}

INSTANTIATE_TEST_SUITE_P(QuicChaosProtectorTests, QuicChaosProtectorTest,
                         ::testing::ValuesIn(TestVersions()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicChaosProtectorTest, Main) {
  BuildEncryptAndParse();
  ASSERT_EQ(validation_framer_.crypto_frames().size(), 6u);
  EXPECT_EQ(validation_framer_.crypto_frames()[0]->offset, 0u);
  EXPECT_EQ(validation_framer_.crypto_frames()[0]->data_length, 1u);
  EXPECT_EQ(validation_framer_.ping_frames().size(), 5u);
  ASSERT_EQ(validation_framer_.padding_frames().size(), 9u);
  EXPECT_EQ(validation_framer_.padding_frames()[0].num_padding_bytes, 3);
}

TEST_P(QuicChaosProtectorTest, DifferentRandom) {
  random_.ResetBase(4);
  BuildEncryptAndParse();
  EXPECT_EQ(validation_framer_.crypto_frames().size(), 4u);
  EXPECT_EQ(validation_framer_.ping_frames().size(), 6u);
  EXPECT_EQ(validation_framer_.padding_frames().size(), 8u);
}

TEST_P(QuicChaosProtectorTest, RandomnessZero) {
  random_.ResetBase(0);
  BuildEncryptAndParse();
  ASSERT_EQ(validation_framer_.crypto_frames().size(), 2u);
  EXPECT_EQ(validation_framer_.crypto_frames()[0]->offset, 1);
  EXPECT_EQ(validation_framer_.crypto_frames()[0]->data_length,
            crypto_data_length_ - 1);
  EXPECT_EQ(validation_framer_.crypto_frames()[1]->offset, crypto_offset_);
  EXPECT_EQ(validation_framer_.crypto_frames()[1]->data_length, 1);
  EXPECT_EQ(validation_framer_.ping_frames().size(), 2u);
  EXPECT_EQ(validation_framer_.padding_frames().size(), 1u);
}

TEST_P(QuicChaosProtectorTest, Offset) {
  ResetOffset(123);
  BuildEncryptAndParse();
  ASSERT_EQ(validation_framer_.crypto_frames().size(), 6u);
  EXPECT_EQ(validation_framer_.crypto_frames()[0]->offset, crypto_offset_);
  EXPECT_EQ(validation_framer_.crypto_frames()[0]->data_length, 1u);
  EXPECT_EQ(validation_framer_.ping_frames().size(), 5u);
  ASSERT_EQ(validation_framer_.padding_frames().size(), 8u);
  EXPECT_EQ(validation_framer_.padding_frames()[0].num_padding_bytes, 3);
}

TEST_P(QuicChaosProtectorTest, OffsetAndRandomnessZero) {
  ResetOffset(123);
  random_.ResetBase(0);
  BuildEncryptAndParse();
  ASSERT_EQ(validation_framer_.crypto_frames().size(), 2u);
  EXPECT_EQ(validation_framer_.crypto_frames()[0]->offset, crypto_offset_ + 1);
  EXPECT_EQ(validation_framer_.crypto_frames()[0]->data_length,
            crypto_data_length_ - 1);
  EXPECT_EQ(validation_framer_.crypto_frames()[1]->offset, crypto_offset_);
  EXPECT_EQ(validation_framer_.crypto_frames()[1]->data_length, 1);
  EXPECT_EQ(validation_framer_.ping_frames().size(), 2u);
  EXPECT_EQ(validation_framer_.padding_frames().size(), 1u);
}

TEST_P(QuicChaosProtectorTest, ZeroRemainingBytesAfterSplit) {
  QuicPacketLength new_length = 63;
  num_padding_bytes_ = QuicFramer::GetMinCryptoFrameSize(
      crypto_offset_ + new_length, new_length);
  ResetLength(new_length);
  BuildEncryptAndParse();

  ASSERT_EQ(validation_framer_.crypto_frames().size(), 2u);
  EXPECT_EQ(validation_framer_.crypto_frames()[0]->offset, crypto_offset_);
  EXPECT_EQ(validation_framer_.crypto_frames()[0]->data_length, 4);
  EXPECT_EQ(validation_framer_.crypto_frames()[1]->offset, crypto_offset_ + 4);
  EXPECT_EQ(validation_framer_.crypto_frames()[1]->data_length,
            crypto_data_length_ - 4);
  EXPECT_EQ(validation_framer_.ping_frames().size(), 0u);
}

TEST_P(QuicChaosProtectorTest, CryptoCryptoAndPadding) {
  input_frames_pattern_ = InputFramesPattern::kCryptoCryptoAndPadding;
  random_.ResetBase(38);
  BuildEncryptAndParse();
  EXPECT_EQ(validation_framer_.crypto_frames().size(), 6u);
  EXPECT_EQ(validation_framer_.ping_frames().size(), 4u);
  EXPECT_EQ(validation_framer_.padding_frames().size(), 4u);
}

TEST_P(QuicChaosProtectorTest, ReorderedCryptoCryptoAndPadding) {
  input_frames_pattern_ = InputFramesPattern::kReorderedCryptoCryptoAndPadding;
  random_.ResetBase(38);
  BuildEncryptAndParse();
  EXPECT_EQ(validation_framer_.crypto_frames().size(), 6u);
  EXPECT_EQ(validation_framer_.ping_frames().size(), 4u);
  EXPECT_EQ(validation_framer_.padding_frames().size(), 4u);
}

TEST_P(QuicChaosProtectorTest, AckCryptoAndPadding) {
  input_frames_pattern_ = InputFramesPattern::kAckCryptoAndPadding;
  random_.ResetBase(37);
  BuildEncryptAndParse();
  EXPECT_EQ(validation_framer_.crypto_frames().size(), 3u);
  EXPECT_EQ(validation_framer_.ping_frames().size(), 3u);
  EXPECT_EQ(validation_framer_.padding_frames().size(), 4u);
  ASSERT_EQ(validation_framer_.ack_frames().size(), 1u);
  // Chaos protector does not insert padding before ACK, or recorder ACK frames.
  EXPECT_EQ(validation_framer_.frame_types()[0], ACK_FRAME);
}

}  // namespace
}  // namespace test
}  // namespace quic
