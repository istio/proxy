// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_framer.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "absl/memory/memory.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/crypto/null_decrypter.h"
#include "quiche/quic/core/crypto/null_encrypter.h"
#include "quiche/quic/core/crypto/quic_decrypter.h"
#include "quiche/quic/core/crypto/quic_encrypter.h"
#include "quiche/quic/core/frames/quic_reset_stream_at_frame.h"
#include "quiche/quic/core/quic_connection_id.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/quic/core/quic_packets.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_ip_address.h"
#include "quiche/quic/platform/api/quic_ip_address_family.h"
#include "quiche/quic/platform/api/quic_logging.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_framer_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/quic/test_tools/simple_data_producer.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

using testing::_;
using testing::ContainerEq;
using testing::Optional;
using testing::Return;

namespace quic {
namespace test {
namespace {

const uint64_t kEpoch = UINT64_C(1) << 32;
const uint64_t kMask = kEpoch - 1;
const uint8_t kPacket0ByteConnectionId = 0;
const uint8_t kPacket8ByteConnectionId = 8;
constexpr size_t kTagSize = 16;

const StatelessResetToken kTestStatelessResetToken{
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f};

// Use fields in which each byte is distinct to ensure that every byte is
// framed correctly. The values are otherwise arbitrary.
QuicConnectionId FramerTestConnectionId() {
  return TestConnectionId(UINT64_C(0xFEDCBA9876543210));
}

QuicConnectionId FramerTestConnectionIdPlusOne() {
  return TestConnectionId(UINT64_C(0xFEDCBA9876543211));
}

QuicConnectionId FramerTestConnectionIdNineBytes() {
  uint8_t connection_id_bytes[9] = {0xFE, 0xDC, 0xBA, 0x98, 0x76,
                                    0x54, 0x32, 0x10, 0x42};
  return QuicConnectionId(reinterpret_cast<char*>(connection_id_bytes),
                          sizeof(connection_id_bytes));
}

const QuicPacketNumber kPacketNumber = QuicPacketNumber(UINT64_C(0x12345678));
const QuicPacketNumber kSmallLargestObserved =
    QuicPacketNumber(UINT16_C(0x1234));
const QuicPacketNumber kSmallMissingPacket = QuicPacketNumber(UINT16_C(0x1233));
const QuicPacketNumber kLeastUnacked = QuicPacketNumber(UINT64_C(0x012345670));
const QuicStreamId kStreamId = UINT64_C(0x01020304);
// Note that the high 4 bits of the stream offset must be less than 0x40
// in order to ensure that the value can be encoded using VarInt62 encoding.
const QuicStreamOffset kStreamOffset = UINT64_C(0x3A98FEDC32107654);
const QuicPublicResetNonceProof kNonceProof = UINT64_C(0xABCDEF0123456789);

// In testing that we can ack the full range of packets...
// This is the largest packet number that can be represented in IETF QUIC
// varint62 format.
const QuicPacketNumber kLargestIetfLargestObserved =
    QuicPacketNumber(UINT64_C(0x3fffffffffffffff));
// Encodings for the two bits in a VarInt62 that
// describe the length of the VarInt61. For binary packet
// formats in this file, the convention is to code the
// first byte as
//   kVarInt62FourBytes + 0x<value_in_that_byte>
const uint8_t kVarInt62OneByte = 0x00;
const uint8_t kVarInt62TwoBytes = 0x40;
const uint8_t kVarInt62FourBytes = 0x80;
const uint8_t kVarInt62EightBytes = 0xc0;

class TestEncrypter : public QuicEncrypter {
 public:
  ~TestEncrypter() override {}
  bool SetKey(absl::string_view /*key*/) override { return true; }
  bool SetNoncePrefix(absl::string_view /*nonce_prefix*/) override {
    return true;
  }
  bool SetIV(absl::string_view /*iv*/) override { return true; }
  bool SetHeaderProtectionKey(absl::string_view /*key*/) override {
    return true;
  }
  bool EncryptPacket(uint64_t packet_number, absl::string_view associated_data,
                     absl::string_view plaintext, char* output,
                     size_t* output_length,
                     size_t /*max_output_length*/) override {
    packet_number_ = QuicPacketNumber(packet_number);
    associated_data_ = std::string(associated_data);
    plaintext_ = std::string(plaintext);
    memcpy(output, plaintext.data(), plaintext.length());
    *output_length = plaintext.length();
    return true;
  }
  std::string GenerateHeaderProtectionMask(
      absl::string_view /*sample*/) override {
    return std::string(5, 0);
  }
  size_t GetKeySize() const override { return 0; }
  size_t GetNoncePrefixSize() const override { return 0; }
  size_t GetIVSize() const override { return 0; }
  size_t GetMaxPlaintextSize(size_t ciphertext_size) const override {
    return ciphertext_size;
  }
  size_t GetCiphertextSize(size_t plaintext_size) const override {
    return plaintext_size;
  }
  QuicPacketCount GetConfidentialityLimit() const override {
    return std::numeric_limits<QuicPacketCount>::max();
  }
  absl::string_view GetKey() const override { return absl::string_view(); }
  absl::string_view GetNoncePrefix() const override {
    return absl::string_view();
  }

  QuicPacketNumber packet_number_;
  std::string associated_data_;
  std::string plaintext_;
};

class TestDecrypter : public QuicDecrypter {
 public:
  ~TestDecrypter() override {}
  bool SetKey(absl::string_view /*key*/) override { return true; }
  bool SetNoncePrefix(absl::string_view /*nonce_prefix*/) override {
    return true;
  }
  bool SetIV(absl::string_view /*iv*/) override { return true; }
  bool SetHeaderProtectionKey(absl::string_view /*key*/) override {
    return true;
  }
  bool SetPreliminaryKey(absl::string_view /*key*/) override {
    QUIC_BUG(quic_bug_10486_1) << "should not be called";
    return false;
  }
  bool SetDiversificationNonce(const DiversificationNonce& /*key*/) override {
    return true;
  }
  bool DecryptPacket(uint64_t packet_number, absl::string_view associated_data,
                     absl::string_view ciphertext, char* output,
                     size_t* output_length,
                     size_t /*max_output_length*/) override {
    packet_number_ = QuicPacketNumber(packet_number);
    associated_data_ = std::string(associated_data);
    ciphertext_ = std::string(ciphertext);
    memcpy(output, ciphertext.data(), ciphertext.length());
    *output_length = ciphertext.length();
    return true;
  }
  std::string GenerateHeaderProtectionMask(
      QuicDataReader* /*sample_reader*/) override {
    return std::string(5, 0);
  }
  size_t GetKeySize() const override { return 0; }
  size_t GetNoncePrefixSize() const override { return 0; }
  size_t GetIVSize() const override { return 0; }
  absl::string_view GetKey() const override { return absl::string_view(); }
  absl::string_view GetNoncePrefix() const override {
    return absl::string_view();
  }
  // Use a distinct value starting with 0xFFFFFF, which is never used by TLS.
  uint32_t cipher_id() const override { return 0xFFFFFFF2; }
  QuicPacketCount GetIntegrityLimit() const override {
    return std::numeric_limits<QuicPacketCount>::max();
  }
  QuicPacketNumber packet_number_;
  std::string associated_data_;
  std::string ciphertext_;
};

std::unique_ptr<QuicEncryptedPacket> EncryptPacketWithTagAndPhase(
    const QuicPacket& packet, uint8_t tag, bool phase) {
  std::string packet_data = std::string(packet.AsStringPiece());
  if (phase) {
    packet_data[0] |= FLAGS_KEY_PHASE_BIT;
  } else {
    packet_data[0] &= ~FLAGS_KEY_PHASE_BIT;
  }

  TaggingEncrypter crypter(tag);
  const size_t packet_size = crypter.GetCiphertextSize(packet_data.size());
  char* buffer = new char[packet_size];
  size_t buf_len = 0;
  if (!crypter.EncryptPacket(0, absl::string_view(), packet_data, buffer,
                             &buf_len, packet_size)) {
    delete[] buffer;
    return nullptr;
  }

  return std::make_unique<QuicEncryptedPacket>(buffer, buf_len,
                                               /*owns_buffer=*/true);
}

class TestQuicVisitor : public QuicFramerVisitorInterface {
 public:
  TestQuicVisitor()
      : error_count_(0),
        version_mismatch_(0),
        packet_count_(0),
        frame_count_(0),
        complete_packets_(0),
        derive_next_key_count_(0),
        decrypted_first_packet_in_key_phase_count_(0),
        accept_packet_(true),
        accept_public_header_(true) {}

  ~TestQuicVisitor() override {}

  void OnError(QuicFramer* f) override {
    QUIC_DLOG(INFO) << "QuicFramer Error: " << QuicErrorCodeToString(f->error())
                    << " (" << f->error() << ")";
    ++error_count_;
  }

  void OnPacket() override {}

  void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) override {
    version_negotiation_packet_ =
        std::make_unique<QuicVersionNegotiationPacket>((packet));
    EXPECT_EQ(0u, framer_->current_received_frame_type());
  }

  void OnRetryPacket(QuicConnectionId original_connection_id,
                     QuicConnectionId new_connection_id,
                     absl::string_view retry_token,
                     absl::string_view retry_integrity_tag,
                     absl::string_view retry_without_tag) override {
    on_retry_packet_called_ = true;
    retry_original_connection_id_ =
        std::make_unique<QuicConnectionId>(original_connection_id);
    retry_new_connection_id_ =
        std::make_unique<QuicConnectionId>(new_connection_id);
    retry_token_ = std::make_unique<std::string>(std::string(retry_token));
    retry_token_integrity_tag_ =
        std::make_unique<std::string>(std::string(retry_integrity_tag));
    retry_without_tag_ =
        std::make_unique<std::string>(std::string(retry_without_tag));
    EXPECT_EQ(0u, framer_->current_received_frame_type());
  }

  bool OnProtocolVersionMismatch(ParsedQuicVersion received_version) override {
    QUIC_DLOG(INFO) << "QuicFramer Version Mismatch, version: "
                    << received_version;
    ++version_mismatch_;
    EXPECT_EQ(0u, framer_->current_received_frame_type());
    return false;
  }

  bool OnUnauthenticatedPublicHeader(const QuicPacketHeader& header) override {
    header_ = std::make_unique<QuicPacketHeader>((header));
    EXPECT_EQ(0u, framer_->current_received_frame_type());
    return accept_public_header_;
  }

  bool OnUnauthenticatedHeader(const QuicPacketHeader& /*header*/) override {
    EXPECT_EQ(0u, framer_->current_received_frame_type());
    return true;
  }

  void OnDecryptedPacket(size_t /*length*/,
                         EncryptionLevel /*level*/) override {
    EXPECT_EQ(0u, framer_->current_received_frame_type());
  }

  bool OnPacketHeader(const QuicPacketHeader& header) override {
    ++packet_count_;
    header_ = std::make_unique<QuicPacketHeader>((header));
    EXPECT_EQ(0u, framer_->current_received_frame_type());
    return accept_packet_;
  }

  void OnCoalescedPacket(const QuicEncryptedPacket& packet) override {
    coalesced_packets_.push_back(packet.Clone());
  }

  void OnUndecryptablePacket(const QuicEncryptedPacket& packet,
                             EncryptionLevel decryption_level,
                             bool has_decryption_key) override {
    undecryptable_packets_.push_back(packet.Clone());
    undecryptable_decryption_levels_.push_back(decryption_level);
    undecryptable_has_decryption_keys_.push_back(has_decryption_key);
  }

  bool OnStreamFrame(const QuicStreamFrame& frame) override {
    ++frame_count_;
    // Save a copy of the data so it is valid after the packet is processed.
    std::string* string_data =
        new std::string(frame.data_buffer, frame.data_length);
    stream_data_.push_back(absl::WrapUnique(string_data));
    stream_frames_.push_back(std::make_unique<QuicStreamFrame>(
        frame.stream_id, frame.fin, frame.offset, *string_data));
    if (VersionHasIetfQuicFrames(transport_version_)) {
      // Low order bits of type encode flags, ignore them for this test.
      EXPECT_TRUE(IS_IETF_STREAM_FRAME(framer_->current_received_frame_type()));
    } else {
      EXPECT_EQ(0u, framer_->current_received_frame_type());
    }
    return true;
  }

  bool OnCryptoFrame(const QuicCryptoFrame& frame) override {
    ++frame_count_;
    // Save a copy of the data so it is valid after the packet is processed.
    std::string* string_data =
        new std::string(frame.data_buffer, frame.data_length);
    crypto_data_.push_back(absl::WrapUnique(string_data));
    crypto_frames_.push_back(std::make_unique<QuicCryptoFrame>(
        frame.level, frame.offset, *string_data));
    if (VersionHasIetfQuicFrames(transport_version_)) {
      EXPECT_EQ(IETF_CRYPTO, framer_->current_received_frame_type());
    } else {
      EXPECT_EQ(0u, framer_->current_received_frame_type());
    }
    return true;
  }

  bool OnAckFrameStart(QuicPacketNumber largest_acked,
                       QuicTime::Delta ack_delay_time) override {
    ++frame_count_;
    QuicAckFrame ack_frame;
    ack_frame.largest_acked = largest_acked;
    ack_frame.ack_delay_time = ack_delay_time;
    ack_frames_.push_back(std::make_unique<QuicAckFrame>(ack_frame));
    if (VersionHasIetfQuicFrames(transport_version_)) {
      EXPECT_TRUE(IETF_ACK == framer_->current_received_frame_type() ||
                  IETF_ACK_ECN == framer_->current_received_frame_type() ||
                  IETF_ACK_RECEIVE_TIMESTAMPS ==
                      framer_->current_received_frame_type());
    } else {
      EXPECT_EQ(0u, framer_->current_received_frame_type());
    }
    return true;
  }

  bool OnAckRange(QuicPacketNumber start, QuicPacketNumber end) override {
    QUICHE_DCHECK(!ack_frames_.empty());
    ack_frames_[ack_frames_.size() - 1]->packets.AddRange(start, end);
    if (VersionHasIetfQuicFrames(transport_version_)) {
      EXPECT_TRUE(IETF_ACK == framer_->current_received_frame_type() ||
                  IETF_ACK_ECN == framer_->current_received_frame_type() ||
                  IETF_ACK_RECEIVE_TIMESTAMPS ==
                      framer_->current_received_frame_type());
    } else {
      EXPECT_EQ(0u, framer_->current_received_frame_type());
    }
    return true;
  }

  bool OnAckTimestamp(QuicPacketNumber packet_number,
                      QuicTime timestamp) override {
    ack_frames_[ack_frames_.size() - 1]->received_packet_times.push_back(
        std::make_pair(packet_number, timestamp));
    if (VersionHasIetfQuicFrames(transport_version_)) {
      EXPECT_TRUE(IETF_ACK == framer_->current_received_frame_type() ||
                  IETF_ACK_ECN == framer_->current_received_frame_type() ||
                  IETF_ACK_RECEIVE_TIMESTAMPS ==
                      framer_->current_received_frame_type());
    } else {
      EXPECT_EQ(0u, framer_->current_received_frame_type());
    }
    return true;
  }

  bool OnAckFrameEnd(
      QuicPacketNumber /*start*/,
      const std::optional<QuicEcnCounts>& /*ecn_counts*/) override {
    return true;
  }

  bool OnStopWaitingFrame(const QuicStopWaitingFrame& frame) override {
    ++frame_count_;
    stop_waiting_frames_.push_back(
        std::make_unique<QuicStopWaitingFrame>(frame));
    EXPECT_EQ(0u, framer_->current_received_frame_type());
    return true;
  }

  bool OnPaddingFrame(const QuicPaddingFrame& frame) override {
    padding_frames_.push_back(std::make_unique<QuicPaddingFrame>(frame));
    if (VersionHasIetfQuicFrames(transport_version_)) {
      EXPECT_EQ(IETF_PADDING, framer_->current_received_frame_type());
    } else {
      EXPECT_EQ(0u, framer_->current_received_frame_type());
    }
    return true;
  }

  bool OnPingFrame(const QuicPingFrame& frame) override {
    ++frame_count_;
    ping_frames_.push_back(std::make_unique<QuicPingFrame>(frame));
    if (VersionHasIetfQuicFrames(transport_version_)) {
      EXPECT_EQ(IETF_PING, framer_->current_received_frame_type());
    } else {
      EXPECT_EQ(0u, framer_->current_received_frame_type());
    }
    return true;
  }

  bool OnMessageFrame(const QuicMessageFrame& frame) override {
    ++frame_count_;
    message_frames_.push_back(
        std::make_unique<QuicMessageFrame>(frame.data, frame.message_length));
    if (VersionHasIetfQuicFrames(transport_version_)) {
      EXPECT_TRUE(IETF_EXTENSION_MESSAGE_NO_LENGTH_V99 ==
                      framer_->current_received_frame_type() ||
                  IETF_EXTENSION_MESSAGE_V99 ==
                      framer_->current_received_frame_type());
    } else {
      EXPECT_EQ(0u, framer_->current_received_frame_type());
    }
    return true;
  }

  bool OnHandshakeDoneFrame(const QuicHandshakeDoneFrame& frame) override {
    ++frame_count_;
    handshake_done_frames_.push_back(
        std::make_unique<QuicHandshakeDoneFrame>(frame));
    QUICHE_DCHECK(VersionHasIetfQuicFrames(transport_version_));
    EXPECT_EQ(IETF_HANDSHAKE_DONE, framer_->current_received_frame_type());
    return true;
  }

  bool OnAckFrequencyFrame(const QuicAckFrequencyFrame& frame) override {
    ++frame_count_;
    ack_frequency_frames_.emplace_back(
        std::make_unique<QuicAckFrequencyFrame>(frame));
    QUICHE_DCHECK(VersionHasIetfQuicFrames(transport_version_));
    EXPECT_EQ(IETF_ACK_FREQUENCY, framer_->current_received_frame_type());
    return true;
  }

  bool OnImmediateAckFrame(const QuicImmediateAckFrame& frame) override {
    ++frame_count_;
    immediate_ack_frames_.emplace_back(
        std::make_unique<QuicImmediateAckFrame>(frame));
    QUICHE_DCHECK(VersionHasIetfQuicFrames(transport_version_));
    EXPECT_EQ(IETF_IMMEDIATE_ACK, framer_->current_received_frame_type());
    return true;
  }

  bool OnResetStreamAtFrame(const QuicResetStreamAtFrame& frame) override {
    ++frame_count_;
    reset_stream_at_frames_.push_back(
        std::make_unique<QuicResetStreamAtFrame>(frame));
    EXPECT_TRUE(VersionHasIetfQuicFrames(transport_version_));
    EXPECT_EQ(IETF_RESET_STREAM_AT, framer_->current_received_frame_type());
    return true;
  }

  void OnPacketComplete() override { ++complete_packets_; }

  bool OnRstStreamFrame(const QuicRstStreamFrame& frame) override {
    rst_stream_frame_ = frame;
    if (VersionHasIetfQuicFrames(transport_version_)) {
      EXPECT_EQ(IETF_RST_STREAM, framer_->current_received_frame_type());
    } else {
      EXPECT_EQ(0u, framer_->current_received_frame_type());
    }
    return true;
  }

  bool OnConnectionCloseFrame(const QuicConnectionCloseFrame& frame) override {
    connection_close_frame_ = frame;
    if (VersionHasIetfQuicFrames(transport_version_)) {
      EXPECT_NE(GOOGLE_QUIC_CONNECTION_CLOSE, frame.close_type);
      if (frame.close_type == IETF_QUIC_TRANSPORT_CONNECTION_CLOSE) {
        EXPECT_EQ(IETF_CONNECTION_CLOSE,
                  framer_->current_received_frame_type());
      } else {
        EXPECT_EQ(IETF_APPLICATION_CLOSE,
                  framer_->current_received_frame_type());
      }
    } else {
      EXPECT_EQ(0u, framer_->current_received_frame_type());
    }
    return true;
  }

  bool OnStopSendingFrame(const QuicStopSendingFrame& frame) override {
    stop_sending_frame_ = frame;
    EXPECT_EQ(IETF_STOP_SENDING, framer_->current_received_frame_type());
    EXPECT_TRUE(VersionHasIetfQuicFrames(transport_version_));
    return true;
  }

  bool OnPathChallengeFrame(const QuicPathChallengeFrame& frame) override {
    path_challenge_frame_ = frame;
    EXPECT_EQ(IETF_PATH_CHALLENGE, framer_->current_received_frame_type());
    EXPECT_TRUE(VersionHasIetfQuicFrames(transport_version_));
    return true;
  }

  bool OnPathResponseFrame(const QuicPathResponseFrame& frame) override {
    path_response_frame_ = frame;
    EXPECT_EQ(IETF_PATH_RESPONSE, framer_->current_received_frame_type());
    EXPECT_TRUE(VersionHasIetfQuicFrames(transport_version_));
    return true;
  }

  bool OnGoAwayFrame(const QuicGoAwayFrame& frame) override {
    goaway_frame_ = frame;
    EXPECT_FALSE(VersionHasIetfQuicFrames(transport_version_));
    EXPECT_EQ(0u, framer_->current_received_frame_type());
    return true;
  }

  bool OnMaxStreamsFrame(const QuicMaxStreamsFrame& frame) override {
    max_streams_frame_ = frame;
    EXPECT_TRUE(VersionHasIetfQuicFrames(transport_version_));
    EXPECT_TRUE(IETF_MAX_STREAMS_UNIDIRECTIONAL ==
                    framer_->current_received_frame_type() ||
                IETF_MAX_STREAMS_BIDIRECTIONAL ==
                    framer_->current_received_frame_type());
    return true;
  }

  bool OnStreamsBlockedFrame(const QuicStreamsBlockedFrame& frame) override {
    streams_blocked_frame_ = frame;
    EXPECT_TRUE(VersionHasIetfQuicFrames(transport_version_));
    EXPECT_TRUE(IETF_STREAMS_BLOCKED_UNIDIRECTIONAL ==
                    framer_->current_received_frame_type() ||
                IETF_STREAMS_BLOCKED_BIDIRECTIONAL ==
                    framer_->current_received_frame_type());
    return true;
  }

  bool OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame) override {
    window_update_frame_ = frame;
    if (VersionHasIetfQuicFrames(transport_version_)) {
      EXPECT_TRUE(IETF_MAX_DATA == framer_->current_received_frame_type() ||
                  IETF_MAX_STREAM_DATA ==
                      framer_->current_received_frame_type());
    } else {
      EXPECT_EQ(0u, framer_->current_received_frame_type());
    }
    return true;
  }

  bool OnBlockedFrame(const QuicBlockedFrame& frame) override {
    blocked_frame_ = frame;
    if (VersionHasIetfQuicFrames(transport_version_)) {
      EXPECT_TRUE(IETF_DATA_BLOCKED == framer_->current_received_frame_type() ||
                  IETF_STREAM_DATA_BLOCKED ==
                      framer_->current_received_frame_type());
    } else {
      EXPECT_EQ(0u, framer_->current_received_frame_type());
    }
    return true;
  }

  bool OnNewConnectionIdFrame(const QuicNewConnectionIdFrame& frame) override {
    new_connection_id_ = frame;
    EXPECT_EQ(IETF_NEW_CONNECTION_ID, framer_->current_received_frame_type());
    EXPECT_TRUE(VersionHasIetfQuicFrames(transport_version_));
    return true;
  }

  bool OnRetireConnectionIdFrame(
      const QuicRetireConnectionIdFrame& frame) override {
    EXPECT_EQ(IETF_RETIRE_CONNECTION_ID,
              framer_->current_received_frame_type());
    EXPECT_TRUE(VersionHasIetfQuicFrames(transport_version_));
    retire_connection_id_ = frame;
    return true;
  }

  bool OnNewTokenFrame(const QuicNewTokenFrame& frame) override {
    new_token_ = frame;
    EXPECT_EQ(IETF_NEW_TOKEN, framer_->current_received_frame_type());
    EXPECT_TRUE(VersionHasIetfQuicFrames(transport_version_));
    return true;
  }

  bool IsValidStatelessResetToken(
      const StatelessResetToken& token) const override {
    EXPECT_EQ(0u, framer_->current_received_frame_type());
    return token == kTestStatelessResetToken;
  }

  void OnAuthenticatedIetfStatelessResetPacket(
      const QuicIetfStatelessResetPacket& packet) override {
    stateless_reset_packet_ =
        std::make_unique<QuicIetfStatelessResetPacket>(packet);
    EXPECT_EQ(0u, framer_->current_received_frame_type());
  }

  void OnKeyUpdate(KeyUpdateReason reason) override {
    key_update_reasons_.push_back(reason);
  }

  void OnDecryptedFirstPacketInKeyPhase() override {
    decrypted_first_packet_in_key_phase_count_++;
  }

  std::unique_ptr<QuicDecrypter> AdvanceKeysAndCreateCurrentOneRttDecrypter()
      override {
    derive_next_key_count_++;
    return std::make_unique<StrictTaggingDecrypter>(derive_next_key_count_);
  }
  std::unique_ptr<QuicEncrypter> CreateCurrentOneRttEncrypter() override {
    return std::make_unique<TaggingEncrypter>(derive_next_key_count_);
  }

  void set_framer(QuicFramer* framer) {
    framer_ = framer;
    transport_version_ = framer->transport_version();
  }

  size_t key_update_count() const { return key_update_reasons_.size(); }

  // Counters from the visitor_ callbacks.
  int error_count_;
  int version_mismatch_;
  int packet_count_;
  int frame_count_;
  int complete_packets_;
  std::vector<KeyUpdateReason> key_update_reasons_;
  int derive_next_key_count_;
  int decrypted_first_packet_in_key_phase_count_;
  bool accept_packet_;
  bool accept_public_header_;

  std::unique_ptr<QuicPacketHeader> header_;
  std::unique_ptr<QuicIetfStatelessResetPacket> stateless_reset_packet_;
  std::unique_ptr<QuicVersionNegotiationPacket> version_negotiation_packet_;
  std::unique_ptr<QuicConnectionId> retry_original_connection_id_;
  std::unique_ptr<QuicConnectionId> retry_new_connection_id_;
  std::unique_ptr<std::string> retry_token_;
  std::unique_ptr<std::string> retry_token_integrity_tag_;
  std::unique_ptr<std::string> retry_without_tag_;
  bool on_retry_packet_called_ = false;
  std::vector<std::unique_ptr<QuicStreamFrame>> stream_frames_;
  std::vector<std::unique_ptr<QuicCryptoFrame>> crypto_frames_;
  std::vector<std::unique_ptr<QuicAckFrame>> ack_frames_;
  std::vector<std::unique_ptr<QuicStopWaitingFrame>> stop_waiting_frames_;
  std::vector<std::unique_ptr<QuicPaddingFrame>> padding_frames_;
  std::vector<std::unique_ptr<QuicPingFrame>> ping_frames_;
  std::vector<std::unique_ptr<QuicMessageFrame>> message_frames_;
  std::vector<std::unique_ptr<QuicHandshakeDoneFrame>> handshake_done_frames_;
  std::vector<std::unique_ptr<QuicAckFrequencyFrame>> ack_frequency_frames_;
  std::vector<std::unique_ptr<QuicImmediateAckFrame>> immediate_ack_frames_;
  std::vector<std::unique_ptr<QuicResetStreamAtFrame>> reset_stream_at_frames_;
  std::vector<std::unique_ptr<QuicEncryptedPacket>> coalesced_packets_;
  std::vector<std::unique_ptr<QuicEncryptedPacket>> undecryptable_packets_;
  std::vector<EncryptionLevel> undecryptable_decryption_levels_;
  std::vector<bool> undecryptable_has_decryption_keys_;
  QuicRstStreamFrame rst_stream_frame_;
  QuicConnectionCloseFrame connection_close_frame_;
  QuicStopSendingFrame stop_sending_frame_;
  QuicGoAwayFrame goaway_frame_;
  QuicPathChallengeFrame path_challenge_frame_;
  QuicPathResponseFrame path_response_frame_;
  QuicWindowUpdateFrame window_update_frame_;
  QuicBlockedFrame blocked_frame_;
  QuicStreamsBlockedFrame streams_blocked_frame_;
  QuicMaxStreamsFrame max_streams_frame_;
  QuicNewConnectionIdFrame new_connection_id_;
  QuicRetireConnectionIdFrame retire_connection_id_;
  QuicNewTokenFrame new_token_;
  std::vector<std::unique_ptr<std::string>> stream_data_;
  std::vector<std::unique_ptr<std::string>> crypto_data_;
  QuicTransportVersion transport_version_;
  QuicFramer* framer_;
};

// Simple struct for defining a packet's content, and associated
// parse error.
struct PacketFragment {
  std::string error_if_missing;
  std::vector<unsigned char> fragment;
};

using PacketFragments = std::vector<struct PacketFragment>;

class QuicFramerTest : public QuicTestWithParam<ParsedQuicVersion> {
 public:
  QuicFramerTest()
      : encrypter_(new test::TestEncrypter()),
        decrypter_(new test::TestDecrypter()),
        version_(GetParam()),
        start_(QuicTime::Zero() + QuicTime::Delta::FromMicroseconds(0x10)),
        framer_(AllSupportedVersions(), start_, Perspective::IS_SERVER,
                kQuicDefaultConnectionIdLength) {
    framer_.set_version(version_);
    if (framer_.version().KnowsWhichDecrypterToUse()) {
      framer_.InstallDecrypter(ENCRYPTION_INITIAL,
                               std::unique_ptr<QuicDecrypter>(decrypter_));
    } else {
      framer_.SetDecrypter(ENCRYPTION_INITIAL,
                           std::unique_ptr<QuicDecrypter>(decrypter_));
    }
    framer_.SetEncrypter(ENCRYPTION_INITIAL,
                         std::unique_ptr<QuicEncrypter>(encrypter_));

    framer_.set_visitor(&visitor_);
    visitor_.set_framer(&framer_);
  }

  void SetDecrypterLevel(EncryptionLevel level) {
    if (!framer_.version().KnowsWhichDecrypterToUse()) {
      return;
    }
    decrypter_ = new TestDecrypter();
    framer_.InstallDecrypter(level, std::unique_ptr<QuicDecrypter>(decrypter_));
  }

  // Helper function to get unsigned char representation of the handshake
  // protocol byte at position |pos| of the current QUIC version number.
  unsigned char GetQuicVersionByte(int pos) {
    return (CreateQuicVersionLabel(version_) >> 8 * (3 - pos)) & 0xff;
  }

  // Helper functions to take a v1 long header packet and make it v2. These are
  // not needed for short header packets, but if sent, this function will exit
  // cleanly. It needs to be called twice for coalesced packets (see references
  // to length_of_first_coalesced_packet below for examples of how to do this).
  inline void ReviseFirstByteByVersion(unsigned char packet_ietf[]) {
    if (version_.UsesV2PacketTypes() && (packet_ietf[0] >= 0x80)) {
      packet_ietf[0] = (packet_ietf[0] + 0x10) | 0xc0;
    }
  }
  inline void ReviseFirstByteByVersion(PacketFragments& packet_ietf) {
    ReviseFirstByteByVersion(&packet_ietf[0].fragment[0]);
  }

  bool CheckEncryption(QuicPacketNumber packet_number, QuicPacket* packet) {
    if (packet_number != encrypter_->packet_number_) {
      QUIC_LOG(ERROR) << "Encrypted incorrect packet number.  expected "
                      << packet_number
                      << " actual: " << encrypter_->packet_number_;
      return false;
    }
    if (packet->AssociatedData(framer_.transport_version()) !=
        encrypter_->associated_data_) {
      QUIC_LOG(ERROR) << "Encrypted incorrect associated data.  expected "
                      << packet->AssociatedData(framer_.transport_version())
                      << " actual: " << encrypter_->associated_data_;
      return false;
    }
    if (packet->Plaintext(framer_.transport_version()) !=
        encrypter_->plaintext_) {
      QUIC_LOG(ERROR) << "Encrypted incorrect plaintext data.  expected "
                      << packet->Plaintext(framer_.transport_version())
                      << " actual: " << encrypter_->plaintext_;
      return false;
    }
    return true;
  }

  bool CheckDecryption(const QuicEncryptedPacket& encrypted,
                       bool includes_version,
                       bool includes_diversification_nonce,
                       uint8_t destination_connection_id_length,
                       uint8_t source_connection_id_length) {
    return CheckDecryption(
        encrypted, includes_version, includes_diversification_nonce,
        destination_connection_id_length, source_connection_id_length,
        quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0, 0,
        quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0);
  }

  bool CheckDecryption(
      const QuicEncryptedPacket& encrypted, bool includes_version,
      bool includes_diversification_nonce,
      uint8_t destination_connection_id_length,
      uint8_t source_connection_id_length,
      quiche::QuicheVariableLengthIntegerLength retry_token_length_length,
      size_t retry_token_length,
      quiche::QuicheVariableLengthIntegerLength length_length) {
    if (visitor_.header_->packet_number != decrypter_->packet_number_) {
      QUIC_LOG(ERROR) << "Decrypted incorrect packet number.  expected "
                      << visitor_.header_->packet_number
                      << " actual: " << decrypter_->packet_number_;
      return false;
    }
    absl::string_view associated_data =
        QuicFramer::GetAssociatedDataFromEncryptedPacket(
            framer_.transport_version(), encrypted,
            destination_connection_id_length, source_connection_id_length,
            includes_version, includes_diversification_nonce,
            PACKET_4BYTE_PACKET_NUMBER, retry_token_length_length,
            retry_token_length, length_length);
    if (associated_data != decrypter_->associated_data_) {
      QUIC_LOG(ERROR) << "Decrypted incorrect associated data.  expected "
                      << absl::BytesToHexString(associated_data) << " actual: "
                      << absl::BytesToHexString(decrypter_->associated_data_);
      return false;
    }
    absl::string_view ciphertext(
        encrypted.AsStringPiece().substr(GetStartOfEncryptedData(
            framer_.transport_version(), destination_connection_id_length,
            source_connection_id_length, includes_version,
            includes_diversification_nonce, PACKET_4BYTE_PACKET_NUMBER,
            retry_token_length_length, retry_token_length, length_length)));
    if (ciphertext != decrypter_->ciphertext_) {
      QUIC_LOG(ERROR) << "Decrypted incorrect ciphertext data.  expected "
                      << absl::BytesToHexString(ciphertext) << " actual: "
                      << absl::BytesToHexString(decrypter_->ciphertext_)
                      << " associated data: "
                      << absl::BytesToHexString(associated_data);
      return false;
    }
    return true;
  }

  char* AsChars(unsigned char* data) { return reinterpret_cast<char*>(data); }

  // Creates a new QuicEncryptedPacket by concatenating the various
  // packet fragments in |fragments|.
  std::unique_ptr<QuicEncryptedPacket> AssemblePacketFromFragments(
      const PacketFragments& fragments) {
    char* buffer = new char[kMaxOutgoingPacketSize + 1];
    size_t len = 0;
    for (const auto& fragment : fragments) {
      memcpy(buffer + len, fragment.fragment.data(), fragment.fragment.size());
      len += fragment.fragment.size();
    }
    return std::make_unique<QuicEncryptedPacket>(buffer, len, true);
  }

  void CheckFramingBoundaries(const PacketFragments& fragments,
                              QuicErrorCode error_code) {
    std::unique_ptr<QuicEncryptedPacket> packet(
        AssemblePacketFromFragments(fragments));
    // Check all the various prefixes of |packet| for the expected
    // parse error and error code.
    for (size_t i = 0; i < packet->length(); ++i) {
      std::string expected_error;
      size_t len = 0;
      for (const auto& fragment : fragments) {
        len += fragment.fragment.size();
        if (i < len) {
          expected_error = fragment.error_if_missing;
          break;
        }
      }

      if (expected_error.empty()) continue;

      CheckProcessingFails(*packet, i, expected_error, error_code);
    }
  }

  void CheckProcessingFails(const QuicEncryptedPacket& packet, size_t len,
                            std::string expected_error,
                            QuicErrorCode error_code) {
    QuicEncryptedPacket encrypted(packet.data(), len, false);
    EXPECT_FALSE(framer_.ProcessPacket(encrypted)) << "len: " << len;
    EXPECT_EQ(expected_error, framer_.detailed_error()) << "len: " << len;
    EXPECT_EQ(error_code, framer_.error()) << "len: " << len;
  }

  void CheckProcessingFails(unsigned char* packet, size_t len,
                            std::string expected_error,
                            QuicErrorCode error_code) {
    QuicEncryptedPacket encrypted(AsChars(packet), len, false);
    EXPECT_FALSE(framer_.ProcessPacket(encrypted)) << "len: " << len;
    EXPECT_EQ(expected_error, framer_.detailed_error()) << "len: " << len;
    EXPECT_EQ(error_code, framer_.error()) << "len: " << len;
  }

  // Checks if the supplied string matches data in the supplied StreamFrame.
  void CheckStreamFrameData(std::string str, QuicStreamFrame* frame) {
    EXPECT_EQ(str, std::string(frame->data_buffer, frame->data_length));
  }

  void CheckCalculatePacketNumber(uint64_t expected_packet_number,
                                  QuicPacketNumber last_packet_number) {
    uint64_t wire_packet_number = expected_packet_number & kMask;
    EXPECT_EQ(expected_packet_number,
              QuicFramerPeer::CalculatePacketNumberFromWire(
                  &framer_, PACKET_4BYTE_PACKET_NUMBER, last_packet_number,
                  wire_packet_number))
        << "last_packet_number: " << last_packet_number
        << " wire_packet_number: " << wire_packet_number;
  }

  std::unique_ptr<QuicPacket> BuildDataPacket(const QuicPacketHeader& header,
                                              const QuicFrames& frames) {
    return BuildUnsizedDataPacket(&framer_, header, frames);
  }

  std::unique_ptr<QuicPacket> BuildDataPacket(const QuicPacketHeader& header,
                                              const QuicFrames& frames,
                                              size_t packet_size) {
    return BuildUnsizedDataPacket(&framer_, header, frames, packet_size);
  }

  // N starts at 1.
  QuicStreamId GetNthStreamid(QuicTransportVersion transport_version,
                              Perspective perspective, bool bidirectional,
                              int n) {
    if (bidirectional) {
      return QuicUtils::GetFirstBidirectionalStreamId(transport_version,
                                                      perspective) +
             ((n - 1) * QuicUtils::StreamIdDelta(transport_version));
    }
    // Unidirectional
    return QuicUtils::GetFirstUnidirectionalStreamId(transport_version,
                                                     perspective) +
           ((n - 1) * QuicUtils::StreamIdDelta(transport_version));
  }

  QuicTime CreationTimePlus(uint64_t offset_us) {
    return framer_.creation_time() +
           QuicTime::Delta::FromMicroseconds(offset_us);
  }

  test::TestEncrypter* encrypter_;
  test::TestDecrypter* decrypter_;
  ParsedQuicVersion version_;
  QuicTime start_;
  QuicFramer framer_;
  test::TestQuicVisitor visitor_;
  quiche::SimpleBufferAllocator allocator_;
};

// Multiple test cases of QuicFramerTest use byte arrays to define packets for
// testing, and these byte arrays contain the QUIC version. This macro explodes
// the 32-bit version into four bytes in network order. Since it uses methods of
// QuicFramerTest, it is only valid to use this in a QuicFramerTest.
#define QUIC_VERSION_BYTES                                             \
  GetQuicVersionByte(0), GetQuicVersionByte(1), GetQuicVersionByte(2), \
      GetQuicVersionByte(3)

// Run all framer tests with all supported versions of QUIC.
INSTANTIATE_TEST_SUITE_P(QuicFramerTests, QuicFramerTest,
                         ::testing::ValuesIn(AllSupportedVersions()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicFramerTest, CalculatePacketNumberFromWireNearEpochStart) {
  // A few quick manual sanity checks.
  CheckCalculatePacketNumber(UINT64_C(1), QuicPacketNumber());
  CheckCalculatePacketNumber(kEpoch + 1, QuicPacketNumber(kMask));
  CheckCalculatePacketNumber(kEpoch, QuicPacketNumber(kMask));
  for (uint64_t j = 0; j < 10; j++) {
    CheckCalculatePacketNumber(j, QuicPacketNumber());
    CheckCalculatePacketNumber(kEpoch - 1 - j, QuicPacketNumber());
  }

  // Cases where the last number was close to the start of the range.
  for (QuicPacketNumber last = QuicPacketNumber(1); last < QuicPacketNumber(10);
       last++) {
    // Small numbers should not wrap (even if they're out of order).
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(j, last);
    }

    // Large numbers should not wrap either (because we're near 0 already).
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(kEpoch - 1 - j, last);
    }
  }
}

TEST_P(QuicFramerTest, CalculatePacketNumberFromWireNearEpochEnd) {
  // Cases where the last number was close to the end of the range
  for (uint64_t i = 0; i < 10; i++) {
    QuicPacketNumber last = QuicPacketNumber(kEpoch - i);

    // Small numbers should wrap.
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(kEpoch + j, last);
    }

    // Large numbers should not (even if they're out of order).
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(kEpoch - 1 - j, last);
    }
  }
}

// Next check where we're in a non-zero epoch to verify we handle
// reverse wrapping, too.
TEST_P(QuicFramerTest, CalculatePacketNumberFromWireNearPrevEpoch) {
  const uint64_t prev_epoch = 1 * kEpoch;
  const uint64_t cur_epoch = 2 * kEpoch;
  // Cases where the last number was close to the start of the range
  for (uint64_t i = 0; i < 10; i++) {
    QuicPacketNumber last = QuicPacketNumber(cur_epoch + i);
    // Small number should not wrap (even if they're out of order).
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(cur_epoch + j, last);
    }

    // But large numbers should reverse wrap.
    for (uint64_t j = 0; j < 10; j++) {
      uint64_t num = kEpoch - 1 - j;
      CheckCalculatePacketNumber(prev_epoch + num, last);
    }
  }
}

TEST_P(QuicFramerTest, CalculatePacketNumberFromWireNearNextEpoch) {
  const uint64_t cur_epoch = 2 * kEpoch;
  const uint64_t next_epoch = 3 * kEpoch;
  // Cases where the last number was close to the end of the range
  for (uint64_t i = 0; i < 10; i++) {
    QuicPacketNumber last = QuicPacketNumber(next_epoch - 1 - i);

    // Small numbers should wrap.
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(next_epoch + j, last);
    }

    // but large numbers should not (even if they're out of order).
    for (uint64_t j = 0; j < 10; j++) {
      uint64_t num = kEpoch - 1 - j;
      CheckCalculatePacketNumber(cur_epoch + num, last);
    }
  }
}

TEST_P(QuicFramerTest, CalculatePacketNumberFromWireNearNextMax) {
  const uint64_t max_number = std::numeric_limits<uint64_t>::max();
  const uint64_t max_epoch = max_number & ~kMask;

  // Cases where the last number was close to the end of the range
  for (uint64_t i = 0; i < 10; i++) {
    // Subtract 1, because the expected next packet number is 1 more than the
    // last packet number.
    QuicPacketNumber last = QuicPacketNumber(max_number - i - 1);

    // Small numbers should not wrap, because they have nowhere to go.
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(max_epoch + j, last);
    }

    // Large numbers should not wrap either.
    for (uint64_t j = 0; j < 10; j++) {
      uint64_t num = kEpoch - 1 - j;
      CheckCalculatePacketNumber(max_epoch + num, last);
    }
  }
}

TEST_P(QuicFramerTest, EmptyPacket) {
  char packet[] = {0x00};
  QuicEncryptedPacket encrypted(packet, 0, false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_THAT(framer_.error(), IsError(QUIC_INVALID_PACKET_HEADER));
}

TEST_P(QuicFramerTest, LargePacket) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[kMaxIncomingPacketSize + 1] = {
    // type (short header 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,
  };
  // clang-format on

  const size_t header_size = GetPacketHeaderSize(
      framer_.transport_version(), kPacket8ByteConnectionId,
      kPacket0ByteConnectionId, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_4BYTE_PACKET_NUMBER, quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0, 0,
      quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0);

  memset(packet + header_size, 0, kMaxIncomingPacketSize - header_size);

  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));

  ASSERT_TRUE(visitor_.header_.get());
  // Make sure we've parsed the packet header, so we can send an error.
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  // Make sure the correct error is propagated.
  EXPECT_THAT(framer_.error(), IsError(QUIC_PACKET_TOO_LARGE));
  EXPECT_EQ("Packet too large.", framer_.detailed_error());
  // Make sure the packet wasn't visited.
  EXPECT_EQ(0, visitor_.packet_count_);
}

TEST_P(QuicFramerTest, LongPacketHeader) {
  // clang-format off
  PacketFragments packet = {
    // type (long header with packet type ZERO_RTT)
    {"Unable to read first byte.",
     {0xD3}},
    // version tag
    {"Unable to read protocol version.",
     {QUIC_VERSION_BYTES}},
    // connection_id length
    {"Unable to read ConnectionId length.",
     {0x50}},
    // connection_id
    {"Unable to read destination connection ID.",
     {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
    // packet number
    {"Unable to read packet number.",
     {0x12, 0x34, 0x56, 0x78}},
  };
  // clang-format on

  if (QuicVersionHasLongHeaderLengths(framer_.transport_version())) {
    return;
  }

  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_THAT(framer_.error(), IsError(QUIC_MISSING_PAYLOAD));
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_TRUE(visitor_.header_->version_flag);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(packet, QUIC_INVALID_PACKET_HEADER);

  PacketHeaderFormat format;
  QuicLongHeaderType long_packet_type = INVALID_PACKET_TYPE;
  bool version_flag;
  QuicConnectionId destination_connection_id, source_connection_id;
  QuicVersionLabel version_label;
  std::string detailed_error;
  bool use_length_prefix;
  std::optional<absl::string_view> retry_token;
  ParsedQuicVersion parsed_version = UnsupportedQuicVersion();
  const QuicErrorCode error_code = QuicFramer::ParsePublicHeaderDispatcher(
      *encrypted, kQuicDefaultConnectionIdLength, &format, &long_packet_type,
      &version_flag, &use_length_prefix, &version_label, &parsed_version,
      &destination_connection_id, &source_connection_id, &retry_token,
      &detailed_error);
  EXPECT_THAT(error_code, IsQuicNoError());
  EXPECT_EQ("", detailed_error);
  EXPECT_FALSE(retry_token.has_value());
  EXPECT_FALSE(use_length_prefix);
  EXPECT_EQ(IETF_QUIC_LONG_HEADER_PACKET, format);
  EXPECT_TRUE(version_flag);
  EXPECT_EQ(kQuicDefaultConnectionIdLength, destination_connection_id.length());
  EXPECT_EQ(FramerTestConnectionId(), destination_connection_id);
  EXPECT_EQ(EmptyQuicConnectionId(), source_connection_id);
}

TEST_P(QuicFramerTest, LongPacketHeaderWithBothConnectionIds) {
  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  // clang-format off
  unsigned char packet[] = {
    // public flags (long header with packet type ZERO_RTT_PROTECTED and
    // 4-byte packet number)
    0xD3,
    // version
    QUIC_VERSION_BYTES,
    // connection ID lengths
    0x55,
    // destination connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // source connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
    // packet number
    0x12, 0x34, 0x56, 0x00,
    // padding frame
    0x00,
  };
  unsigned char packet49[] = {
    // public flags (long header with packet type ZERO_RTT_PROTECTED and
    // 4-byte packet number)
    0xD3,
    // version
    QUIC_VERSION_BYTES,
    // destination connection ID length
    0x08,
    // destination connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // source connection ID length
    0x08,
    // source connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
    // long header packet length
    0x05,
    // packet number
    0x12, 0x34, 0x56, 0x00,
    // padding frame
    0x00,
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_length = ABSL_ARRAYSIZE(packet);
  if (framer_.version().HasLongHeaderLengths()) {
    ReviseFirstByteByVersion(packet49);
    p = packet49;
    p_length = ABSL_ARRAYSIZE(packet49);
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_length, false);
  PacketHeaderFormat format = GOOGLE_QUIC_PACKET;
  QuicLongHeaderType long_packet_type = INVALID_PACKET_TYPE;
  bool version_flag = false;
  QuicConnectionId destination_connection_id, source_connection_id;
  QuicVersionLabel version_label = 0;
  std::string detailed_error = "";
  bool use_length_prefix;
  std::optional<absl::string_view> retry_token;
  ParsedQuicVersion parsed_version = UnsupportedQuicVersion();
  const QuicErrorCode error_code = QuicFramer::ParsePublicHeaderDispatcher(
      encrypted, kQuicDefaultConnectionIdLength, &format, &long_packet_type,
      &version_flag, &use_length_prefix, &version_label, &parsed_version,
      &destination_connection_id, &source_connection_id, &retry_token,
      &detailed_error);
  EXPECT_THAT(error_code, IsQuicNoError());
  EXPECT_FALSE(retry_token.has_value());
  EXPECT_EQ(framer_.version().HasLengthPrefixedConnectionIds(),
            use_length_prefix);
  EXPECT_EQ("", detailed_error);
  EXPECT_EQ(IETF_QUIC_LONG_HEADER_PACKET, format);
  EXPECT_TRUE(version_flag);
  EXPECT_EQ(FramerTestConnectionId(), destination_connection_id);
  EXPECT_EQ(FramerTestConnectionIdPlusOne(), source_connection_id);
}

TEST_P(QuicFramerTest, AllZeroPacketParsingFails) {
  unsigned char packet[1200] = {};
  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);
  PacketHeaderFormat format = GOOGLE_QUIC_PACKET;
  QuicLongHeaderType long_packet_type = INVALID_PACKET_TYPE;
  bool version_flag = false;
  QuicConnectionId destination_connection_id, source_connection_id;
  QuicVersionLabel version_label = 0;
  std::string detailed_error = "";
  bool use_length_prefix;
  std::optional<absl::string_view> retry_token;
  ParsedQuicVersion parsed_version = UnsupportedQuicVersion();
  const QuicErrorCode error_code = QuicFramer::ParsePublicHeaderDispatcher(
      encrypted, kQuicDefaultConnectionIdLength, &format, &long_packet_type,
      &version_flag, &use_length_prefix, &version_label, &parsed_version,
      &destination_connection_id, &source_connection_id, &retry_token,
      &detailed_error);
  EXPECT_EQ(error_code, QUIC_INVALID_PACKET_HEADER);
  EXPECT_EQ(detailed_error, "Invalid flags.");
}

TEST_P(QuicFramerTest, ParsePublicHeader) {
  // clang-format off
  unsigned char packet[] = {
      // public flags (long header with packet type HANDSHAKE and
      // 4-byte packet number)
      0xE3,
      // version
      QUIC_VERSION_BYTES,
      // connection ID lengths
      0x50,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // long header packet length
      0x05,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // padding frame
      0x00,
  };
  unsigned char packet49[] = {
    // public flags (long header with packet type HANDSHAKE and
    // 4-byte packet number)
    0xE3,
    // version
    QUIC_VERSION_BYTES,
    // destination connection ID length
    0x08,
    // destination connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // source connection ID length
    0x00,
    // long header packet length
    0x05,
    // packet number
    0x12, 0x34, 0x56, 0x78,
    // padding frame
    0x00,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_length = ABSL_ARRAYSIZE(packet);
  if (framer_.version().HasLongHeaderLengths()) {
    ReviseFirstByteByVersion(packet49);
    p = packet49;
    p_length = ABSL_ARRAYSIZE(packet49);
  }

  uint8_t first_byte = 0x33;
  PacketHeaderFormat format = GOOGLE_QUIC_PACKET;
  bool version_present = false, has_length_prefix = false;
  QuicVersionLabel version_label = 0;
  ParsedQuicVersion parsed_version = UnsupportedQuicVersion();
  QuicConnectionId destination_connection_id = EmptyQuicConnectionId(),
                   source_connection_id = EmptyQuicConnectionId();
  QuicLongHeaderType long_packet_type = INVALID_PACKET_TYPE;
  quiche::QuicheVariableLengthIntegerLength retry_token_length_length =
      quiche::VARIABLE_LENGTH_INTEGER_LENGTH_4;
  absl::string_view retry_token;
  std::string detailed_error = "foobar";

  QuicDataReader reader(AsChars(p), p_length);
  const QuicErrorCode parse_error = QuicFramer::ParsePublicHeader(
      &reader, kQuicDefaultConnectionIdLength, /*ietf_format=*/true,
      &first_byte, &format, &version_present, &has_length_prefix,
      &version_label, &parsed_version, &destination_connection_id,
      &source_connection_id, &long_packet_type, &retry_token_length_length,
      &retry_token, &detailed_error);
  EXPECT_THAT(parse_error, IsQuicNoError());
  EXPECT_EQ("", detailed_error);
  EXPECT_EQ(p[0], first_byte);
  EXPECT_TRUE(version_present);
  EXPECT_EQ(framer_.version().HasLengthPrefixedConnectionIds(),
            has_length_prefix);
  EXPECT_EQ(CreateQuicVersionLabel(framer_.version()), version_label);
  EXPECT_EQ(framer_.version(), parsed_version);
  EXPECT_EQ(FramerTestConnectionId(), destination_connection_id);
  EXPECT_EQ(EmptyQuicConnectionId(), source_connection_id);
  EXPECT_EQ(quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0,
            retry_token_length_length);
  EXPECT_EQ(absl::string_view(), retry_token);
  EXPECT_EQ(IETF_QUIC_LONG_HEADER_PACKET, format);
  EXPECT_EQ(HANDSHAKE, long_packet_type);
}

TEST_P(QuicFramerTest, ParsePublicHeaderProxBadSourceConnectionIdLength) {
  if (!framer_.version().HasLengthPrefixedConnectionIds()) {
    return;
  }
  // clang-format off
  unsigned char packet[] = {
    // public flags (long header with packet type HANDSHAKE and
    // 4-byte packet number)
    0xE3,
    // version
    'P', 'R', 'O', 'X',
    // destination connection ID length
    0x08,
    // destination connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // source connection ID length (bogus)
    0xEE,
    // long header packet length
    0x05,
    // packet number
    0x12, 0x34, 0x56, 0x78,
    // padding frame
    0x00,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_length = ABSL_ARRAYSIZE(packet);

  uint8_t first_byte = 0x33;
  PacketHeaderFormat format = GOOGLE_QUIC_PACKET;
  bool version_present = false, has_length_prefix = false;
  QuicVersionLabel version_label = 0;
  ParsedQuicVersion parsed_version = UnsupportedQuicVersion();
  QuicConnectionId destination_connection_id = EmptyQuicConnectionId(),
                   source_connection_id = EmptyQuicConnectionId();
  QuicLongHeaderType long_packet_type = INVALID_PACKET_TYPE;
  quiche::QuicheVariableLengthIntegerLength retry_token_length_length =
      quiche::VARIABLE_LENGTH_INTEGER_LENGTH_4;
  absl::string_view retry_token;
  std::string detailed_error = "foobar";

  QuicDataReader reader(AsChars(p), p_length);
  const QuicErrorCode parse_error = QuicFramer::ParsePublicHeader(
      &reader, kQuicDefaultConnectionIdLength,
      /*ietf_format=*/true, &first_byte, &format, &version_present,
      &has_length_prefix, &version_label, &parsed_version,
      &destination_connection_id, &source_connection_id, &long_packet_type,
      &retry_token_length_length, &retry_token, &detailed_error);
  EXPECT_THAT(parse_error, IsQuicNoError());
  EXPECT_EQ("", detailed_error);
  EXPECT_EQ(p[0], first_byte);
  EXPECT_TRUE(version_present);
  EXPECT_TRUE(has_length_prefix);
  EXPECT_EQ(0x50524F58u, version_label);  // "PROX"
  EXPECT_EQ(UnsupportedQuicVersion(), parsed_version);
  EXPECT_EQ(FramerTestConnectionId(), destination_connection_id);
  EXPECT_EQ(EmptyQuicConnectionId(), source_connection_id);
  EXPECT_EQ(quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0,
            retry_token_length_length);
  EXPECT_EQ(absl::string_view(), retry_token);
  EXPECT_EQ(IETF_QUIC_LONG_HEADER_PACKET, format);
}

TEST_P(QuicFramerTest, ClientConnectionIdFromShortHeaderToClient) {
  if (!framer_.version().SupportsClientConnectionIds()) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  QuicFramerPeer::SetLastSerializedServerConnectionId(&framer_,
                                                      TestConnectionId(0x33));
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  framer_.SetExpectedClientConnectionIdLength(kQuicDefaultConnectionIdLength);
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x13, 0x37, 0x42, 0x33,
    // padding frame
    0x00,
  };
  // clang-format on
  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_THAT(framer_.error(), IsQuicNoError());
  EXPECT_EQ("", framer_.detailed_error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
}

// In short header packets from client to server, the client connection ID
// is omitted, but the framer adds it to the header struct using its
// last serialized client connection ID. This test ensures that this
// mechanism behaves as expected.
TEST_P(QuicFramerTest, ClientConnectionIdFromShortHeaderToServer) {
  if (!framer_.version().SupportsClientConnectionIds()) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x13, 0x37, 0x42, 0x33,
    // padding frame
    0x00,
  };
  // clang-format on
  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_THAT(framer_.error(), IsQuicNoError());
  EXPECT_EQ("", framer_.detailed_error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
}

TEST_P(QuicFramerTest, PacketHeaderWith0ByteConnectionId) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  QuicFramerPeer::SetLastSerializedServerConnectionId(&framer_,
                                                      FramerTestConnectionId());
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  // clang-format off
  PacketFragments packet = {
        // type (short header, 4 byte packet number)
        {"Unable to read first byte.",
         {0x43}},
        // connection_id
        // packet number
        {"Unable to read packet number.",
         {0x12, 0x34, 0x56, 0x78}},
   };

  PacketFragments packet_hp = {
        // type (short header, 4 byte packet number)
        {"Unable to read first byte.",
         {0x43}},
        // connection_id
        // packet number
        {"",
         {0x12, 0x34, 0x56, 0x78}},
   };
  // clang-format on

  PacketFragments& fragments =
      framer_.version().HasHeaderProtection() ? packet_hp : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_THAT(framer_.error(), IsError(QUIC_MISSING_PAYLOAD));
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_FALSE(visitor_.header_->version_flag);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, PacketHeaderWithVersionFlag) {
  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  // clang-format off
  PacketFragments packet = {
      // type (long header with packet type ZERO_RTT_PROTECTED and 4 bytes
      // packet number)
      {"Unable to read first byte.",
       {0xD3}},
      // version tag
      {"Unable to read protocol version.",
       {QUIC_VERSION_BYTES}},
      // connection_id length
      {"Unable to read ConnectionId length.",
       {0x50}},
      // connection_id
      {"Unable to read destination connection ID.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x12, 0x34, 0x56, 0x78}},
  };

  PacketFragments packet49 = {
      // type (long header with packet type ZERO_RTT_PROTECTED and 4 bytes
      // packet number)
      {"Unable to read first byte.",
       {0xD3}},
      // version tag
      {"Unable to read protocol version.",
       {QUIC_VERSION_BYTES}},
      // destination connection ID length
      {"Unable to read destination connection ID.",
       {0x08}},
      // destination connection ID
      {"Unable to read destination connection ID.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // source connection ID length
      {"Unable to read source connection ID.",
       {0x00}},
      // long header packet length
      {"Unable to read long header payload length.",
       {0x04}},
      // packet number
      {"Long header payload length longer than packet.",
       {0x12, 0x34, 0x56, 0x78}},
  };
  // clang-format on

  ReviseFirstByteByVersion(packet49);
  PacketFragments& fragments =
      framer_.version().HasLongHeaderLengths() ? packet49 : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_THAT(framer_.error(), IsError(QUIC_MISSING_PAYLOAD));
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_TRUE(visitor_.header_->version_flag);
  EXPECT_EQ(GetParam(), visitor_.header_->version);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, PacketHeaderWith4BytePacketNumber) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  QuicFramerPeer::SetLargestPacketNumber(&framer_, kPacketNumber - 2);

  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"Unable to read first byte.",
       {0x43}},
      // connection_id
      {"Unable to read destination connection ID.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x12, 0x34, 0x56, 0x78}},
  };

  PacketFragments packet_hp = {
      // type (short header, 4 byte packet number)
      {"Unable to read first byte.",
       {0x43}},
      // connection_id
      {"Unable to read destination connection ID.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.version().HasHeaderProtection() ? packet_hp : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_THAT(framer_.error(), IsError(QUIC_MISSING_PAYLOAD));
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_FALSE(visitor_.header_->version_flag);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, PacketHeaderWith2BytePacketNumber) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  QuicFramerPeer::SetLargestPacketNumber(&framer_, kPacketNumber - 2);

  // clang-format off
  PacketFragments packet = {
      // type (short header, 2 byte packet number)
      {"Unable to read first byte.",
       {0x41}},
      // connection_id
      {"Unable to read destination connection ID.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x56, 0x78}},
  };

  PacketFragments packet_hp = {
      // type (short header, 2 byte packet number)
      {"Unable to read first byte.",
       {0x41}},
      // connection_id
      {"Unable to read destination connection ID.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x56, 0x78}},
      // padding
      {"", {0x00, 0x00}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.version().HasHeaderProtection() ? packet_hp : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  if (framer_.version().HasHeaderProtection()) {
    EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
    EXPECT_THAT(framer_.error(), IsQuicNoError());
  } else {
    EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
    EXPECT_THAT(framer_.error(), IsError(QUIC_MISSING_PAYLOAD));
  }
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_FALSE(visitor_.header_->version_flag);
  EXPECT_EQ(PACKET_2BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, PacketHeaderWith1BytePacketNumber) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  QuicFramerPeer::SetLargestPacketNumber(&framer_, kPacketNumber - 2);

  // clang-format off
  PacketFragments packet = {
      // type (8 byte connection_id and 1 byte packet number)
      {"Unable to read first byte.",
       {0x40}},
      // connection_id
      {"Unable to read destination connection ID.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x78}},
  };

  PacketFragments packet_hp = {
      // type (8 byte connection_id and 1 byte packet number)
      {"Unable to read first byte.",
       {0x40}},
      // connection_id
      {"Unable to read destination connection ID.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x78}},
      // padding
      {"", {0x00, 0x00, 0x00}},
  };

  // clang-format on

  PacketFragments& fragments =
      framer_.version().HasHeaderProtection() ? packet_hp : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  if (framer_.version().HasHeaderProtection()) {
    EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
    EXPECT_THAT(framer_.error(), IsQuicNoError());
  } else {
    EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
    EXPECT_THAT(framer_.error(), IsError(QUIC_MISSING_PAYLOAD));
  }
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_FALSE(visitor_.header_->version_flag);
  EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, PacketNumberDecreasesThenIncreases) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // Test the case when a packet is received from the past and future packet
  // numbers are still calculated relative to the largest received packet.
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber - 2;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  QuicEncryptedPacket encrypted(data->data(), data->length(), false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_EQ(PACKET_4BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber - 2, visitor_.header_->packet_number);

  // Receive a 1 byte packet number.
  header.packet_number = kPacketNumber;
  header.packet_number_length = PACKET_1BYTE_PACKET_NUMBER;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  QuicEncryptedPacket encrypted1(data->data(), data->length(), false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted1));
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  // Process a 2 byte packet number 256 packets ago.
  header.packet_number = kPacketNumber - 256;
  header.packet_number_length = PACKET_2BYTE_PACKET_NUMBER;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  QuicEncryptedPacket encrypted2(data->data(), data->length(), false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted2));
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_EQ(PACKET_2BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber - 256, visitor_.header_->packet_number);

  // Process another 1 byte packet number and ensure it works.
  header.packet_number = kPacketNumber - 1;
  header.packet_number_length = PACKET_1BYTE_PACKET_NUMBER;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  QuicEncryptedPacket encrypted3(data->data(), data->length(), false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted3));
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_->destination_connection_id);
  EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber - 1, visitor_.header_->packet_number);
}

TEST_P(QuicFramerTest, PacketWithDiversificationNonce) {
  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  // clang-format off
  unsigned char packet[] = {
    // type: Long header with packet type ZERO_RTT_PROTECTED and 1 byte packet
    // number.
    0xD0,
    // version tag
    QUIC_VERSION_BYTES,
    // connection_id length
    0x05,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78,
    // nonce
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,

    // frame type (padding)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet49[] = {
    // type: Long header with packet type ZERO_RTT_PROTECTED and 1 byte packet
    // number.
    0xD0,
    // version tag
    QUIC_VERSION_BYTES,
    // destination connection ID length
    0x00,
    // source connection ID length
    0x08,
    // source connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // long header packet length
    0x26,
    // packet number
    0x78,
    // nonce
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,

    // frame type (padding)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  if (framer_.version().handshake_protocol != PROTOCOL_QUIC_CRYPTO) {
    return;
  }

  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (framer_.version().HasLongHeaderLengths()) {
    p = packet49;
    p_size = ABSL_ARRAYSIZE(packet49);
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  ASSERT_TRUE(visitor_.header_->nonce != nullptr);
  for (char i = 0; i < 32; ++i) {
    EXPECT_EQ(i, (*visitor_.header_->nonce)[static_cast<size_t>(i)]);
  }
  EXPECT_EQ(1u, visitor_.padding_frames_.size());
  EXPECT_EQ(5, visitor_.padding_frames_[0]->num_padding_bytes);
}

TEST_P(QuicFramerTest, LargePublicFlagWithMismatchedVersions) {
  // clang-format off
  unsigned char packet[] = {
    // type (long header, ZERO_RTT_PROTECTED, 4-byte packet number)
    0xD3,
    // version tag
    'Q', '0', '0', '0',
    // connection_id length
    0x50,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet49[] = {
    // type (long header, ZERO_RTT_PROTECTED, 4-byte packet number)
    0xD3,
    // version tag
    'Q', '0', '0', '0',
    // destination connection ID length
    0x08,
    // destination connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // source connection ID length
    0x00,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (framer_.version().HasLongHeaderLengths()) {
    p = packet49;
    p_size = ABSL_ARRAYSIZE(packet49);
  }
  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(0, visitor_.frame_count_);
  EXPECT_EQ(1, visitor_.version_mismatch_);
}

TEST_P(QuicFramerTest, PaddingFrame) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // paddings
    0x00, 0x00,
    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };

  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // paddings
    0x00, 0x00,
    // frame type - IETF_STREAM with FIN, LEN, and OFFSET bits set.
    0x08 | 0x01 | 0x02 | 0x04,

    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // offset
    kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    kVarInt62OneByte + 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
    p_size = ABSL_ARRAYSIZE(packet_ietf);
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(2u, visitor_.padding_frames_.size());
  EXPECT_EQ(2, visitor_.padding_frames_[0]->num_padding_bytes);
  EXPECT_EQ(2, visitor_.padding_frames_[1]->num_padding_bytes);
  EXPECT_EQ(kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());
}

TEST_P(QuicFramerTest, StreamFrame) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFF}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type - IETF_STREAM with FIN, LEN, and OFFSET bits set.
      {"",
       { 0x08 | 0x01 | 0x02 | 0x04 }},
      // stream id
      {"Unable to read IETF_STREAM frame stream id/count.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      // offset
      {"Unable to read stream data offset.",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // data length
      {"Unable to read stream data length.",
       {kVarInt62OneByte + 0x0c}},
      // data
      {"Unable to read frame data.",
       { 'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };
  // clang-format on

  PacketFragments& fragments =
      VersionHasIetfQuicFrames(framer_.transport_version()) ? packet_ietf
                                                            : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  CheckFramingBoundaries(fragments, QUIC_INVALID_STREAM_DATA);
}

// Test an empty (no data) stream frame.
TEST_P(QuicFramerTest, EmptyStreamFrame) {
  // Only the IETF QUIC spec explicitly says that empty
  // stream frames are supported.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type - IETF_STREAM with FIN, LEN, and OFFSET bits set.
      {"",
       { 0x08 | 0x01 | 0x02 | 0x04 }},
      // stream id
      {"Unable to read IETF_STREAM frame stream id/count.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      // offset
      {"Unable to read stream data offset.",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // data length
      {"Unable to read stream data length.",
       {kVarInt62OneByte + 0x00}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  EXPECT_EQ(visitor_.stream_frames_[0].get()->data_length, 0u);

  CheckFramingBoundaries(packet, QUIC_INVALID_STREAM_DATA);
}

TEST_P(QuicFramerTest, MissingDiversificationNonce) {
  if (framer_.version().handshake_protocol != PROTOCOL_QUIC_CRYPTO) {
    // TLS does not use diversification nonces.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  decrypter_ = new test::TestDecrypter();
  if (framer_.version().KnowsWhichDecrypterToUse()) {
    framer_.InstallDecrypter(
        ENCRYPTION_INITIAL,
        std::make_unique<NullDecrypter>(Perspective::IS_CLIENT));
    framer_.InstallDecrypter(ENCRYPTION_ZERO_RTT,
                             std::unique_ptr<QuicDecrypter>(decrypter_));
  } else {
    framer_.SetDecrypter(ENCRYPTION_INITIAL, std::make_unique<NullDecrypter>(
                                                 Perspective::IS_CLIENT));
    framer_.SetAlternativeDecrypter(
        ENCRYPTION_ZERO_RTT, std::unique_ptr<QuicDecrypter>(decrypter_), false);
  }

  // clang-format off
  unsigned char packet[] = {
        // type (long header, ZERO_RTT_PROTECTED, 4-byte packet number)
        0xD3,
        // version tag
        QUIC_VERSION_BYTES,
        // connection_id length
        0x05,
        // connection_id
        0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
        // packet number
        0x12, 0x34, 0x56, 0x78,
        // padding frame
        0x00,
    };

  unsigned char packet49[] = {
        // type (long header, ZERO_RTT_PROTECTED, 4-byte packet number)
        0xD3,
        // version tag
        QUIC_VERSION_BYTES,
        // destination connection ID length
        0x00,
        // source connection ID length
        0x08,
        // source connection ID
        0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
        // IETF long header payload length
        0x05,
        // packet number
        0x12, 0x34, 0x56, 0x78,
        // padding frame
        0x00,
    };
  // clang-format on

  unsigned char* p = packet;
  size_t p_length = ABSL_ARRAYSIZE(packet);
  if (framer_.version().HasLongHeaderLengths()) {
    p = packet49;
    p_length = ABSL_ARRAYSIZE(packet49);
  }
  QuicEncryptedPacket encrypted(AsChars(p), p_length, false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  if (framer_.version().HasHeaderProtection()) {
    EXPECT_THAT(framer_.error(), IsError(QUIC_DECRYPTION_FAILURE));
    EXPECT_EQ("Unable to decrypt ENCRYPTION_ZERO_RTT header protection.",
              framer_.detailed_error());
  } else {
    // Cannot read diversification nonce.
    EXPECT_THAT(framer_.error(), IsError(QUIC_INVALID_PACKET_HEADER));
    EXPECT_EQ("Unable to read nonce.", framer_.detailed_error());
  }
}

TEST_P(QuicFramerTest, StreamFrame2ByteStreamId) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (stream frame with fin)
       {"",
        {0xFD}},
       // stream id
       {"Unable to read stream_id.",
        {0x03, 0x04}},
       // offset
       {"Unable to read offset.",
        {0x3A, 0x98, 0xFE, 0xDC,
         0x32, 0x10, 0x76, 0x54}},
       {"Unable to read frame data.",
        {
          // data length
          0x00, 0x0c,
          // data
          'h',  'e',  'l',  'l',
          'o',  ' ',  'w',  'o',
          'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_STREAM frame with LEN, FIN, and OFFSET bits set)
      {"",
       {0x08 | 0x01 | 0x02 | 0x04}},
      // stream id
      {"Unable to read IETF_STREAM frame stream id/count.",
       {kVarInt62TwoBytes + 0x03, 0x04}},
      // offset
      {"Unable to read stream data offset.",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // data length
      {"Unable to read stream data length.",
       {kVarInt62OneByte + 0x0c}},
      // data
      {"Unable to read frame data.",
       { 'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };
  // clang-format on

  PacketFragments& fragments =
      VersionHasIetfQuicFrames(framer_.transport_version()) ? packet_ietf
                                                            : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  // Stream ID should be the last 2 bytes of kStreamId.
  EXPECT_EQ(0x0000FFFF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  CheckFramingBoundaries(fragments, QUIC_INVALID_STREAM_DATA);
}

TEST_P(QuicFramerTest, StreamFrame1ByteStreamId) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFC}},
      // stream id
      {"Unable to read stream_id.",
       {0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_STREAM frame with LEN, FIN, and OFFSET bits set)
      {"",
       {0x08 | 0x01 | 0x02 | 0x04}},
      // stream id
      {"Unable to read IETF_STREAM frame stream id/count.",
       {kVarInt62OneByte + 0x04}},
      // offset
      {"Unable to read stream data offset.",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // data length
      {"Unable to read stream data length.",
       {kVarInt62OneByte + 0x0c}},
      // data
      {"Unable to read frame data.",
       { 'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };
  // clang-format on

  PacketFragments& fragments =
      VersionHasIetfQuicFrames(framer_.transport_version()) ? packet_ietf
                                                            : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  // Stream ID should be the last 1 byte of kStreamId.
  EXPECT_EQ(0x000000FF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  CheckFramingBoundaries(fragments, QUIC_INVALID_STREAM_DATA);
}

TEST_P(QuicFramerTest, StreamFrameWithVersion) {
  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  // clang-format off
  PacketFragments packet = {
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      {"",
       {0xD3}},
      // version tag
      {"",
       {QUIC_VERSION_BYTES}},
      // connection_id length
      {"",
       {0x50}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFE}},
      // stream id
      {"Unable to read stream_id.",
       {0x02, 0x03, 0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet49 = {
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      {"",
       {0xD3}},
      // version tag
      {"",
       {QUIC_VERSION_BYTES}},
      // destination connection ID length
      {"",
       {0x08}},
      // destination connection ID
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // source connection ID length
      {"",
       {0x00}},
      // long header packet length
      {"",
       {0x1E}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFE}},
      // stream id
      {"Long header payload length longer than packet.",
       {0x02, 0x03, 0x04}},
      // offset
      {"Long header payload length longer than packet.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Long header payload length longer than packet.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet_ietf = {
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      {"",
       {0xD3}},
      // version tag
      {"",
       {QUIC_VERSION_BYTES}},
      // destination connection ID length
      {"",
       {0x08}},
      // destination connection ID
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // source connection ID length
      {"",
       {0x00}},
      // long header packet length
      {"",
       {0x1E}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      {"",
       {0x08 | 0x01 | 0x02 | 0x04}},
      // stream id
      {"Long header payload length longer than packet.",
       {kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04}},
      // offset
      {"Long header payload length longer than packet.",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // data length
      {"Long header payload length longer than packet.",
       {kVarInt62OneByte + 0x0c}},
      // data
      {"Long header payload length longer than packet.",
       { 'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };
  // clang-format on

  quiche::QuicheVariableLengthIntegerLength retry_token_length_length =
      quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0;
  size_t retry_token_length = 0;
  quiche::QuicheVariableLengthIntegerLength length_length =
      QuicVersionHasLongHeaderLengths(framer_.transport_version())
          ? quiche::VARIABLE_LENGTH_INTEGER_LENGTH_1
          : quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0;

  ReviseFirstByteByVersion(packet_ietf);
  PacketFragments& fragments =
      VersionHasIetfQuicFrames(framer_.transport_version())
          ? packet_ietf
          : (framer_.version().HasLongHeaderLengths() ? packet49 : packet);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId,
      retry_token_length_length, retry_token_length, length_length));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  // Stream ID should be the last 3 bytes of kStreamId.
  EXPECT_EQ(0x00FFFFFF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  CheckFramingBoundaries(fragments, framer_.version().HasLongHeaderLengths()
                                        ? QUIC_INVALID_PACKET_HEADER
                                        : QUIC_INVALID_STREAM_DATA);
}

TEST_P(QuicFramerTest, RejectPacket) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  visitor_.accept_packet_ = false;

  // clang-format off
  unsigned char packet[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (STREAM Frame with FIN, LEN, and OFFSET bits set)
      0x10 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  ASSERT_EQ(0u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
}

TEST_P(QuicFramerTest, RejectPublicHeader) {
  visitor_.accept_public_header_ = false;

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 1 byte packet number)
    0x40,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x01,
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_FALSE(visitor_.header_->packet_number.IsInitialized());
}

TEST_P(QuicFramerTest, AckFrameOneAckBlock) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short packet, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (ack frame)
      // (one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       {0x45}},
      // largest acked
      {"Unable to read largest acked.",
       {0x12, 0x34}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x12, 0x34}},
      // num timestamps.
      {"Unable to read num received packets.",
       {0x00}}
  };

  PacketFragments packet_ietf = {
      // type (short packet, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (IETF_ACK)
       // (one ack block, 2 byte largest observed, 2 byte block length)
       // IETF-Quic ignores the bit-fields in the ack type, all of
       // that information is encoded elsewhere in the frame.
       {"",
        {0x02}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62TwoBytes  + 0x12, 0x34}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
      // Ack block count (0 -- no blocks after the first)
      {"Unable to read ack block count.",
       {kVarInt62OneByte + 0x00}},
       // first ack block length - 1.
       // IETF Quic defines the ack block's value as the "number of
       // packets that preceed the largest packet number in the block"
       // which for the 1st ack block is the largest acked field,
       // above. This means that if we are acking just packet 0x1234
       // then the 1st ack block will be 0.
       {"Unable to read first ack block length.",
        {kVarInt62TwoBytes + 0x12, 0x33}}
  };
  // clang-format on

  PacketFragments& fragments =
      VersionHasIetfQuicFrames(framer_.transport_version()) ? packet_ietf
                                                            : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(kSmallLargestObserved, LargestAcked(frame));
  ASSERT_EQ(4660u, frame.packets.NumPacketsSlow());

  CheckFramingBoundaries(fragments, QUIC_INVALID_ACK_DATA);
}

// This test checks that the ack frame processor correctly identifies
// and handles the case where the first ack block is larger than the
// largest_acked packet.
TEST_P(QuicFramerTest, FirstAckFrameUnderflow) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (ack frame)
      // (one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       {0x45}},
      // largest acked
      {"Unable to read largest acked.",
       {0x12, 0x34}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x88, 0x88}},
      // num timestamps.
      {"Underflow with first ack block length 34952 largest acked is 4660.",
       {0x00}}
  };

  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (IETF_ACK)
       {"",
        {0x02}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62TwoBytes  + 0x12, 0x34}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Ack block count (0 -- no blocks after the first)
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 0x00}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62TwoBytes + 0x28, 0x88}}
  };
  // clang-format on

  PacketFragments& fragments =
      VersionHasIetfQuicFrames(framer_.transport_version()) ? packet_ietf
                                                            : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  CheckFramingBoundaries(fragments, QUIC_INVALID_ACK_DATA);
}

// This test checks that the ack frame processor correctly identifies
// and handles the case where the third ack block's gap is larger than the
// available space in the ack range.
TEST_P(QuicFramerTest, ThirdAckBlockUnderflowGap) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Test originally written for development of IETF QUIC. The test may
    // also apply to Google QUIC. If so, the test should be extended to
    // include Google QUIC (frame formats, etc). See b/141858819.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (IETF_ACK frame)
       {"",
        {0x02}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62OneByte  + 63}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Ack block count (2 -- 2 blocks after the first)
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 0x02}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62OneByte + 13}},  // Ack 14 packets, range 50..63 (inclusive)

       {"Unable to read gap block value.",
        {kVarInt62OneByte + 9}},  // Gap 10 packets, 40..49 (inclusive)
       {"Unable to read ack block value.",
        {kVarInt62OneByte + 9}},  // Ack 10 packets, 30..39 (inclusive)
       {"Unable to read gap block value.",
        {kVarInt62OneByte + 29}},  // A gap of 30 packets (0..29 inclusive)
                                   // should be too big, leaving no room
                                   // for the ack.
       {"Underflow with gap block length 30 previous ack block start is 30.",
        {kVarInt62OneByte + 10}},  // Don't care
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(
      framer_.detailed_error(),
      "Underflow with gap block length 30 previous ack block start is 30.");
  CheckFramingBoundaries(packet_ietf, QUIC_INVALID_ACK_DATA);
}

// This test checks that the ack frame processor correctly identifies
// and handles the case where the third ack block's length is larger than the
// available space in the ack range.
TEST_P(QuicFramerTest, ThirdAckBlockUnderflowAck) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Test originally written for development of IETF QUIC. The test may
    // also apply to Google QUIC. If so, the test should be extended to
    // include Google QUIC (frame formats, etc). See b/141858819.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (IETF_ACK frame)
       {"",
        {0x02}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62OneByte  + 63}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Ack block count (2 -- 2 blocks after the first)
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 0x02}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62OneByte + 13}},  // only 50 packet numbers "left"

       {"Unable to read gap block value.",
        {kVarInt62OneByte + 10}},  // Only 40 packet numbers left
       {"Unable to read ack block value.",
        {kVarInt62OneByte + 10}},  // only 30 packet numbers left.
       {"Unable to read gap block value.",
        {kVarInt62OneByte + 1}},  // Gap is OK, 29 packet numbers left
      {"Unable to read ack block value.",
        {kVarInt62OneByte + 30}},  // Use up all 30, should be an error
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(framer_.detailed_error(),
            "Underflow with ack block length 31 latest ack block end is 25.");
  CheckFramingBoundaries(packet_ietf, QUIC_INVALID_ACK_DATA);
}

// Tests a variety of ack block wrap scenarios. For example, if the
// N-1th block causes packet 0 to be acked, then a gap would wrap
// around to 0x3fffffff ffffffff... Make sure we detect this
// condition.
TEST_P(QuicFramerTest, AckBlockUnderflowGapWrap) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Test originally written for development of IETF QUIC. The test may
    // also apply to Google QUIC. If so, the test should be extended to
    // include Google QUIC (frame formats, etc). See b/141858819.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (IETF_ACK frame)
       {"",
        {0x02}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62OneByte  + 10}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Ack block count (1 -- 1 blocks after the first)
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 1}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62OneByte + 9}},  // Ack packets 1..10 (inclusive)

       {"Unable to read gap block value.",
        {kVarInt62OneByte + 1}},  // Gap of 2 packets (-1...0), should wrap
       {"Underflow with gap block length 2 previous ack block start is 1.",
        {kVarInt62OneByte + 9}},  // irrelevant
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(framer_.detailed_error(),
            "Underflow with gap block length 2 previous ack block start is 1.");
  CheckFramingBoundaries(packet_ietf, QUIC_INVALID_ACK_DATA);
}

// As AckBlockUnderflowGapWrap, but in this test, it's the ack
// component of the ack-block that causes the wrap, not the gap.
TEST_P(QuicFramerTest, AckBlockUnderflowAckWrap) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Test originally written for development of IETF QUIC. The test may
    // also apply to Google QUIC. If so, the test should be extended to
    // include Google QUIC (frame formats, etc). See b/141858819.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (IETF_ACK frame)
       {"",
        {0x02}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62OneByte  + 10}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Ack block count (1 -- 1 blocks after the first)
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 1}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62OneByte + 6}},  // Ack packets 4..10 (inclusive)

       {"Unable to read gap block value.",
        {kVarInt62OneByte + 1}},  // Gap of 2 packets (2..3)
       {"Unable to read ack block value.",
        {kVarInt62OneByte + 9}},  // Should wrap.
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(framer_.detailed_error(),
            "Underflow with ack block length 10 latest ack block end is 1.");
  CheckFramingBoundaries(packet_ietf, QUIC_INVALID_ACK_DATA);
}

// An ack block that acks the entire range, 1...0x3fffffffffffffff
TEST_P(QuicFramerTest, AckBlockAcksEverything) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Test originally written for development of IETF QUIC. The test may
    // also apply to Google QUIC. If so, the test should be extended to
    // include Google QUIC (frame formats, etc). See b/141858819.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (IETF_ACK frame)
       {"",
        {0x02}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62EightBytes  + 0x3f, 0xff, 0xff, 0xff,
         0xff, 0xff, 0xff, 0xff}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Ack block count No additional blocks
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 0}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62EightBytes  + 0x3f, 0xff, 0xff, 0xff,
         0xff, 0xff, 0xff, 0xfe}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(1u, frame.packets.NumIntervals());
  EXPECT_EQ(kLargestIetfLargestObserved, LargestAcked(frame));
  EXPECT_EQ(kLargestIetfLargestObserved.ToUint64(),
            frame.packets.NumPacketsSlow());
}

// This test looks for a malformed ack where
//  - There is a largest-acked value (that is, the frame is acking
//    something,
//  - But the length of the first ack block is 0 saying that no frames
//    are being acked with the largest-acked value or there are no
//    additional ack blocks.
//
TEST_P(QuicFramerTest, AckFrameFirstAckBlockLengthZero) {
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Not applicable to version 99 -- first ack block contains the
    // number of packets that preceed the largest_acked packet.
    // A value of 0 means no packets preceed --- that the block's
    // length is 1. Therefore the condition that this test checks can
    // not arise.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       { 0x43 }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (ack frame)
      // (more than one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       { 0x65 }},
      // largest acked
      {"Unable to read largest acked.",
       { 0x12, 0x34 }},
      // Zero delta time.
      {"Unable to read ack delay time.",
       { 0x00, 0x00 }},
      // num ack blocks ranges.
      {"Unable to read num of ack blocks.",
       { 0x01 }},
      // first ack block length.
      {"Unable to read first ack block length.",
       { 0x00, 0x00 }},
      // gap to next block.
      { "First block length is zero.",
        { 0x01 }},
      // ack block length.
      { "First block length is zero.",
        { 0x0e, 0xaf }},
      // Number of timestamps.
      { "First block length is zero.",
        { 0x00 }},
  };

  // clang-format on
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_THAT(framer_.error(), IsError(QUIC_INVALID_ACK_DATA));

  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(packet, QUIC_INVALID_ACK_DATA);
}

TEST_P(QuicFramerTest, AckFrameOneAckBlockMaxLength) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x56, 0x78, 0x9A, 0xBC}},
      // frame type (ack frame)
      // (one ack block, 4 byte largest observed, 2 byte block length)
      {"",
       {0x49}},
      // largest acked
      {"Unable to read largest acked.",
       {0x12, 0x34, 0x56, 0x78}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x12, 0x34}},
      // num timestamps.
      {"Unable to read num received packets.",
       {0x00}}
  };

  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x56, 0x78, 0x9A, 0xBC}},
       // frame type (IETF_ACK frame)
       {"",
        {0x02}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62FourBytes  + 0x12, 0x34, 0x56, 0x78}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Number of ack blocks after first
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 0x00}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62TwoBytes  + 0x12, 0x33}}
  };
  // clang-format on

  PacketFragments& fragments =
      VersionHasIetfQuicFrames(framer_.transport_version()) ? packet_ietf
                                                            : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(kPacketNumber, LargestAcked(frame));
  ASSERT_EQ(4660u, frame.packets.NumPacketsSlow());

  CheckFramingBoundaries(fragments, QUIC_INVALID_ACK_DATA);
}

// Tests ability to handle multiple ackblocks after the first ack
// block. Non-version-99 tests include multiple timestamps as well.
TEST_P(QuicFramerTest, AckFrameTwoTimeStampsMultipleAckBlocks) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       { 0x43 }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (ack frame)
      // (more than one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       { 0x65 }},
      // largest acked
      {"Unable to read largest acked.",
       { 0x12, 0x34 }},
      // Zero delta time.
      {"Unable to read ack delay time.",
       { 0x00, 0x00 }},
      // num ack blocks ranges.
      {"Unable to read num of ack blocks.",
       { 0x04 }},
      // first ack block length.
      {"Unable to read first ack block length.",
       { 0x00, 0x01 }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x01 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x0e, 0xaf }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0xff }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x00, 0x00 }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x91 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x01, 0xea }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x05 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x00, 0x04 }},
      // Number of timestamps.
      { "Unable to read num received packets.",
        { 0x02 }},
      // Delta from largest observed.
      { "Unable to read sequence delta in received packets.",
        { 0x01 }},
      // Delta time.
      { "Unable to read time delta in received packets.",
        { 0x76, 0x54, 0x32, 0x10 }},
      // Delta from largest observed.
      { "Unable to read sequence delta in received packets.",
        { 0x02 }},
      // Delta time.
      { "Unable to read incremental time delta in received packets.",
        { 0x32, 0x10 }},
  };

  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       { 0x43 }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (IETF_ACK_RECEIVE_TIMESTAMPS frame)
      {"",
       { 0x22 }},
       // largest acked
       {"Unable to read largest acked.",
        { kVarInt62TwoBytes + 0x12, 0x34 }},   // = 4660
       // Zero delta time.
       {"Unable to read ack delay time.",
        { kVarInt62OneByte + 0x00 }},
       // number of additional ack blocks
       {"Unable to read ack block count.",
        { kVarInt62OneByte + 0x03 }},
       // first ack block length.
       {"Unable to read first ack block length.",
        { kVarInt62OneByte + 0x00 }},  // 1st block length = 1

       // Additional ACK Block #1
       // gap to next block.
       { "Unable to read gap block value.",
         { kVarInt62OneByte + 0x00 }},   // gap of 1 packet
       // ack block length.
       { "Unable to read ack block value.",
         { kVarInt62TwoBytes + 0x0e, 0xae }},   // 3759

       // pre-version-99 test includes an ack block of 0 length. this
       // can not happen in version 99. ergo the second block is not
       // present in the v99 test and the gap length of the next block
       // is the sum of the two gaps in the pre-version-99 tests.
       // Additional ACK Block #2
       // gap to next block.
       { "Unable to read gap block value.",
         { kVarInt62TwoBytes + 0x01, 0x8f }},  // Gap is 400 (0x190) pkts
       // ack block length.
       { "Unable to read ack block value.",
         { kVarInt62TwoBytes + 0x01, 0xe9 }},  // block is 389 (x1ea) pkts

       // Additional ACK Block #3
       // gap to next block.
       { "Unable to read gap block value.",
         { kVarInt62OneByte + 0x04 }},   // Gap is 5 packets.
       // ack block length.
       { "Unable to read ack block value.",
         { kVarInt62OneByte + 0x03 }},   // block is 3 packets.

       // Receive Timestamps.
       { "Unable to read receive timestamp range count.",
         { kVarInt62OneByte + 0x01 }},
       { "Unable to read receive timestamp gap.",
         { kVarInt62OneByte + 0x01 }},
       { "Unable to read receive timestamp count.",
         { kVarInt62OneByte + 0x02 }},
       { "Unable to read receive timestamp delta.",
         { kVarInt62FourBytes + 0x36, 0x54, 0x32, 0x10 }},
       { "Unable to read receive timestamp delta.",
         { kVarInt62TwoBytes + 0x32, 0x10 }},
  };

  // clang-format on
  PacketFragments& fragments =
      VersionHasIetfQuicFrames(framer_.transport_version()) ? packet_ietf
                                                            : packet;

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));

  framer_.set_process_timestamps(true);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(kSmallLargestObserved, LargestAcked(frame));
  ASSERT_EQ(4254u, frame.packets.NumPacketsSlow());
  EXPECT_EQ(4u, frame.packets.NumIntervals());
  EXPECT_EQ(2u, frame.received_packet_times.size());
}

TEST_P(QuicFramerTest, AckFrameMultipleReceiveTimestampRanges) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       { 0x43 }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (IETF_ACK_RECEIVE_TIMESTAMPS frame)
      {"",
       { 0x22 }},
       // largest acked
       {"Unable to read largest acked.",
        { kVarInt62TwoBytes + 0x12, 0x34 }},   // = 4660
       // Zero delta time.
       {"Unable to read ack delay time.",
        { kVarInt62OneByte + 0x00 }},
       // number of additional ack blocks
       {"Unable to read ack block count.",
        { kVarInt62OneByte + 0x00 }},
       // first ack block length.
       {"Unable to read first ack block length.",
        { kVarInt62OneByte + 0x00 }},  // 1st block length = 1

       // Receive Timestamps.
       { "Unable to read receive timestamp range count.",
         { kVarInt62OneByte + 0x03 }},

       // Timestamp range 1 (three packets).
       { "Unable to read receive timestamp gap.",
         { kVarInt62OneByte + 0x02 }},
       { "Unable to read receive timestamp count.",
         { kVarInt62OneByte + 0x03 }},
       { "Unable to read receive timestamp delta.",
         { kVarInt62FourBytes + 0x29, 0xff, 0xff, 0xff}},
       { "Unable to read receive timestamp delta.",
         { kVarInt62TwoBytes + 0x11, 0x11 }},
       { "Unable to read receive timestamp delta.",
         { kVarInt62OneByte + 0x01}},

       // Timestamp range 2 (one packet).
       { "Unable to read receive timestamp gap.",
         { kVarInt62OneByte + 0x05 }},
       { "Unable to read receive timestamp count.",
         { kVarInt62OneByte + 0x01 }},
       { "Unable to read receive timestamp delta.",
         { kVarInt62TwoBytes + 0x10, 0x00 }},

       // Timestamp range 3 (two packets).
       { "Unable to read receive timestamp gap.",
         { kVarInt62OneByte + 0x08 }},
       { "Unable to read receive timestamp count.",
         { kVarInt62OneByte + 0x02 }},
       { "Unable to read receive timestamp delta.",
         { kVarInt62OneByte + 0x10 }},
       { "Unable to read receive timestamp delta.",
         { kVarInt62TwoBytes + 0x01, 0x00 }},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));

  framer_.set_process_timestamps(true);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];

  EXPECT_THAT(frame.received_packet_times,
              ContainerEq(PacketTimeVector{
                  // Timestamp Range 1.
                  {LargestAcked(frame) - 2, CreationTimePlus(0x29ffffff)},
                  {LargestAcked(frame) - 3, CreationTimePlus(0x29ffeeee)},
                  {LargestAcked(frame) - 4, CreationTimePlus(0x29ffeeed)},
                  // Timestamp Range 2.
                  {LargestAcked(frame) - 11, CreationTimePlus(0x29ffdeed)},
                  // Timestamp Range 3.
                  {LargestAcked(frame) - 21, CreationTimePlus(0x29ffdedd)},
                  {LargestAcked(frame) - 22, CreationTimePlus(0x29ffdddd)},
              }));
}

TEST_P(QuicFramerTest, AckFrameReceiveTimestampWithExponent) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       { 0x43 }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (IETF_ACK_RECEIVE_TIMESTAMPS frame)
      {"",
       { 0x22 }},
       // largest acked
       {"Unable to read largest acked.",
        { kVarInt62TwoBytes + 0x12, 0x34 }},   // = 4660
       // Zero delta time.
       {"Unable to read ack delay time.",
        { kVarInt62OneByte + 0x00 }},
       // number of additional ack blocks
       {"Unable to read ack block count.",
        { kVarInt62OneByte + 0x00 }},
       // first ack block length.
       {"Unable to read first ack block length.",
        { kVarInt62OneByte + 0x00 }},  // 1st block length = 1

       // Receive Timestamps.
       { "Unable to read receive timestamp range count.",
         { kVarInt62OneByte + 0x01 }},
       { "Unable to read receive timestamp gap.",
         { kVarInt62OneByte + 0x00 }},
       { "Unable to read receive timestamp count.",
         { kVarInt62OneByte + 0x03 }},
       { "Unable to read receive timestamp delta.",
         { kVarInt62TwoBytes + 0x29, 0xff}},
       { "Unable to read receive timestamp delta.",
         { kVarInt62TwoBytes + 0x11, 0x11 }},
       { "Unable to read receive timestamp delta.",
         { kVarInt62OneByte + 0x01}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));

  framer_.set_receive_timestamps_exponent(3);
  framer_.set_process_timestamps(true);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];

  EXPECT_THAT(frame.received_packet_times,
              ContainerEq(PacketTimeVector{
                  // Timestamp Range 1.
                  {LargestAcked(frame), CreationTimePlus(0x29ff << 3)},
                  {LargestAcked(frame) - 1, CreationTimePlus(0x18ee << 3)},
                  {LargestAcked(frame) - 2, CreationTimePlus(0x18ed << 3)},
              }));
}

TEST_P(QuicFramerTest, AckFrameReceiveTimestampGapTooHigh) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       { 0x43 }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (IETF_ACK_RECEIVE_TIMESTAMPS frame)
      {"",
       { 0x22 }},
       // largest acked
       {"Unable to read largest acked.",
        { kVarInt62TwoBytes + 0x12, 0x34 }},   // = 4660
       // Zero delta time.
       {"Unable to read ack delay time.",
        { kVarInt62OneByte + 0x00 }},
       // number of additional ack blocks
       {"Unable to read ack block count.",
        { kVarInt62OneByte + 0x00 }},
       // first ack block length.
       {"Unable to read first ack block length.",
        { kVarInt62OneByte + 0x00 }},  // 1st block length = 1

       // Receive Timestamps.
       { "Unable to read receive timestamp range count.",
         { kVarInt62OneByte + 0x01 }},
       { "Unable to read receive timestamp gap.",
         { kVarInt62FourBytes + 0x12, 0x34, 0x56, 0x79 }},
       { "Unable to read receive timestamp count.",
         { kVarInt62OneByte + 0x01 }},
       { "Unable to read receive timestamp delta.",
         { kVarInt62TwoBytes + 0x29, 0xff}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));

  framer_.set_process_timestamps(true);
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_TRUE(absl::StartsWith(framer_.detailed_error(),
                               "Receive timestamp gap too high."));
}

TEST_P(QuicFramerTest, AckFrameReceiveTimestampCountTooHigh) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       { 0x43 }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (IETF_ACK_RECEIVE_TIMESTAMPS frame)
      {"",
       { 0x22 }},
       // largest acked
       {"Unable to read largest acked.",
        { kVarInt62TwoBytes + 0x12, 0x34 }},   // = 4660
       // Zero delta time.
       {"Unable to read ack delay time.",
        { kVarInt62OneByte + 0x00 }},
       // number of additional ack blocks
       {"Unable to read ack block count.",
        { kVarInt62OneByte + 0x00 }},
       // first ack block length.
       {"Unable to read first ack block length.",
        { kVarInt62OneByte + 0x00 }},  // 1st block length = 1

       // Receive Timestamps.
       { "Unable to read receive timestamp range count.",
         { kVarInt62OneByte + 0x01 }},
       { "Unable to read receive timestamp gap.",
         { kVarInt62OneByte + 0x02 }},
       { "Unable to read receive timestamp count.",
         { kVarInt62OneByte + 0x02 }},
       { "Unable to read receive timestamp delta.",
         { kVarInt62OneByte + 0x0a}},
       { "Unable to read receive timestamp delta.",
         { kVarInt62OneByte + 0x0b}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));

  framer_.set_process_timestamps(true);
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_TRUE(absl::StartsWith(framer_.detailed_error(),
                               "Receive timestamp delta too high."));
}

TEST_P(QuicFramerTest, AckFrameReceiveTimestampDeltaTooHigh) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       { 0x43 }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (IETF_ACK_RECEIVE_TIMESTAMPS frame)
      {"",
       { 0x22 }},
       // largest acked
       {"Unable to read largest acked.",
        { kVarInt62TwoBytes + 0x12, 0x34 }},   // = 4660
       // Zero delta time.
       {"Unable to read ack delay time.",
        { kVarInt62OneByte + 0x00 }},
       // number of additional ack blocks
       {"Unable to read ack block count.",
        { kVarInt62OneByte + 0x00 }},
       // first ack block length.
       {"Unable to read first ack block length.",
        { kVarInt62OneByte + 0x00 }},  // 1st block length = 1

       // Receive Timestamps.
       { "Unable to read receive timestamp range count.",
         { kVarInt62OneByte + 0x01 }},
       { "Unable to read receive timestamp gap.",
         { kVarInt62OneByte + 0x02 }},
       { "Unable to read receive timestamp count.",
         { kVarInt62FourBytes + 0x12, 0x34, 0x56, 0x77 }},
       { "Unable to read receive timestamp delta.",
         { kVarInt62TwoBytes + 0x29, 0xff}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));

  framer_.set_process_timestamps(true);
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_TRUE(absl::StartsWith(framer_.detailed_error(),
                               "Receive timestamp count too high."));
}

TEST_P(QuicFramerTest, AckFrameTimeStampDeltaTooHigh) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 1 byte largest observed, 1 byte block length)
      0x40,
      // largest acked
      0x01,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x01,
      // num timestamps.
      0x01,
      // Delta from largest observed.
      0x01,
      // Delta time.
      0x10, 0x32, 0x54, 0x76,
  };
  // clang-format on
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    // ACK Timestamp is not a feature of IETF QUIC.
    return;
  }
  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_TRUE(absl::StartsWith(framer_.detailed_error(),
                               "delta_from_largest_observed too high"));
}

TEST_P(QuicFramerTest, AckFrameTimeStampSecondDeltaTooHigh) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 1 byte largest observed, 1 byte block length)
      0x40,
      // largest acked
      0x03,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x03,
      // num timestamps.
      0x02,
      // Delta from largest observed.
      0x01,
      // Delta time.
      0x10, 0x32, 0x54, 0x76,
      // Delta from largest observed.
      0x03,
      // Delta time.
      0x10, 0x32,
  };
  // clang-format on
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    // ACK Timestamp is not a feature of IETF QUIC.
    return;
  }
  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_TRUE(absl::StartsWith(framer_.detailed_error(),
                               "delta_from_largest_observed too high"));
}

TEST_P(QuicFramerTest, NewStopWaitingFrame) {
  if (VersionHasIetfQuicFrames(version_.transport_version)) {
    // The Stop Waiting frame is not in IETF QUIC
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stop waiting frame)
      {"",
       {0x06}},
      // least packet number awaiting an ack, delta from packet number.
      {"Unable to read least unacked delta.",
        {0x00, 0x00, 0x00, 0x08}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.stop_waiting_frames_.size());
  const QuicStopWaitingFrame& frame = *visitor_.stop_waiting_frames_[0];
  EXPECT_EQ(kLeastUnacked, frame.least_unacked);

  CheckFramingBoundaries(packet, QUIC_INVALID_STOP_WAITING_DATA);
}

TEST_P(QuicFramerTest, InvalidNewStopWaitingFrame) {
  // The Stop Waiting frame is not in IETF QUIC
  if (VersionHasIetfQuicFrames(version_.transport_version)) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,
    // frame type (stop waiting frame)
    0x06,
    // least packet number awaiting an ack, delta from packet number.
    0x57, 0x78, 0x9A, 0xA8,
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_THAT(framer_.error(), IsError(QUIC_INVALID_STOP_WAITING_DATA));
  EXPECT_EQ("Invalid unacked delta.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, RstStreamFrame) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (rst stream frame)
      {"",
       {0x01}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
      // sent byte offset
      {"Unable to read rst stream sent byte offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // error code QUIC_STREAM_CANCELLED
      {"Unable to read rst stream error code.",
       {0x00, 0x00, 0x00, 0x06}}
  };

  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_RST_STREAM frame)
      {"",
       {0x04}},
      // stream id
      {"Unable to read IETF_RST_STREAM frame stream id/count.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      // application error code H3_REQUEST_CANCELLED gets translated to
      // QuicRstStreamErrorCode::QUIC_STREAM_CANCELLED.
      {"Unable to read rst stream error code.",
       {kVarInt62TwoBytes + 0x01, 0x0c}},
      // Final Offset
      {"Unable to read rst stream sent byte offset.",
       {kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54}}
  };
  // clang-format on

  PacketFragments& fragments =
      VersionHasIetfQuicFrames(framer_.transport_version()) ? packet_ietf
                                                            : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(kStreamId, visitor_.rst_stream_frame_.stream_id);
  EXPECT_EQ(QUIC_STREAM_CANCELLED, visitor_.rst_stream_frame_.error_code);
  EXPECT_EQ(kStreamOffset, visitor_.rst_stream_frame_.byte_offset);
  CheckFramingBoundaries(fragments, QUIC_INVALID_RST_STREAM_DATA);
}

TEST_P(QuicFramerTest, ConnectionCloseFrame) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (connection close frame)
      {"",
       {0x02}},
      // error code
      {"Unable to read connection close error code.",
       {0x00, 0x00, 0x00, 0x11}},
      {"Unable to read connection close error details.",
       {
         // error details length
         0x0, 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };

  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF Transport CONNECTION_CLOSE frame)
      {"",
       {0x1c}},
      // error code
      {"Unable to read connection close error code.",
       {kVarInt62TwoBytes + 0x00, 0x11}},
      {"Unable to read connection close frame type.",
       {kVarInt62TwoBytes + 0x12, 0x34 }},
      {"Unable to read connection close error details.",
       {
         // error details length
         kVarInt62OneByte + 0x11,
         // error details with QuicErrorCode serialized
         '1',  '1',  '5',  ':',
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };
  // clang-format on

  PacketFragments& fragments =
      VersionHasIetfQuicFrames(framer_.transport_version()) ? packet_ietf
                                                            : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  EXPECT_EQ(0x11u, static_cast<unsigned>(
                       visitor_.connection_close_frame_.wire_error_code));
  EXPECT_EQ("because I can", visitor_.connection_close_frame_.error_details);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    EXPECT_EQ(0x1234u,
              visitor_.connection_close_frame_.transport_close_frame_type);
    EXPECT_EQ(115u, visitor_.connection_close_frame_.quic_error_code);
  } else {
    // For Google QUIC frame, |quic_error_code| and |wire_error_code| has the
    // same value.
    EXPECT_EQ(0x11u, static_cast<unsigned>(
                         visitor_.connection_close_frame_.quic_error_code));
  }

  ASSERT_EQ(0u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(fragments, QUIC_INVALID_CONNECTION_CLOSE_DATA);
}

TEST_P(QuicFramerTest, ConnectionCloseFrameWithUnknownErrorCode) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (connection close frame)
      {"",
       {0x02}},
      // error code larger than QUIC_LAST_ERROR
      {"Unable to read connection close error code.",
       {0x00, 0x00, 0xC0, 0xDE}},
      {"Unable to read connection close error details.",
       {
         // error details length
         0x0, 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };

  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF Transport CONNECTION_CLOSE frame)
      {"",
       {0x1c}},
      // error code
      {"Unable to read connection close error code.",
       {kVarInt62FourBytes + 0x00, 0x00, 0xC0, 0xDE}},
      {"Unable to read connection close frame type.",
       {kVarInt62TwoBytes + 0x12, 0x34 }},
      {"Unable to read connection close error details.",
       {
         // error details length
         kVarInt62OneByte + 0x11,
         // error details with QuicErrorCode larger than QUIC_LAST_ERROR
         '8',  '4',  '9',  ':',
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };
  // clang-format on

  PacketFragments& fragments =
      VersionHasIetfQuicFrames(framer_.transport_version()) ? packet_ietf
                                                            : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  EXPECT_EQ("because I can", visitor_.connection_close_frame_.error_details);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    EXPECT_EQ(0x1234u,
              visitor_.connection_close_frame_.transport_close_frame_type);
    EXPECT_EQ(0xC0DEu, visitor_.connection_close_frame_.wire_error_code);
    EXPECT_EQ(849u, visitor_.connection_close_frame_.quic_error_code);
  } else {
    // For Google QUIC frame, |quic_error_code| and |wire_error_code| has the
    // same value.
    EXPECT_EQ(0xC0DEu, visitor_.connection_close_frame_.wire_error_code);
    EXPECT_EQ(0xC0DEu, visitor_.connection_close_frame_.quic_error_code);
  }

  ASSERT_EQ(0u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(fragments, QUIC_INVALID_CONNECTION_CLOSE_DATA);
}

// As above, but checks that for Google-QUIC, if there happens
// to be an ErrorCode string at the start of the details, it is
// NOT extracted/parsed/folded/spindled/and/mutilated.
TEST_P(QuicFramerTest, ConnectionCloseFrameWithExtractedInfoIgnoreGCuic) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet = {
    // type (short header, 4 byte packet number)
    {"",
     {0x43}},
    // connection_id
    {"",
     {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
    // packet number
    {"",
     {0x12, 0x34, 0x56, 0x78}},
    // frame type (connection close frame)
    {"",
     {0x02}},
    // error code
    {"Unable to read connection close error code.",
     {0x00, 0x00, 0x00, 0x11}},
    {"Unable to read connection close error details.",
     {
       // error details length
       0x0, 0x13,
       // error details
      '1',  '7',  '7',  '6',
      '7',  ':',  'b',  'e',
      'c',  'a',  'u',  's',
      'e',  ' ',  'I',  ' ',
      'c',  'a',  'n'}
    }
  };

  PacketFragments packet_ietf = {
    // type (short header, 4 byte packet number)
    {"",
     {0x43}},
    // connection_id
    {"",
     {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
    // packet number
    {"",
     {0x12, 0x34, 0x56, 0x78}},
    // frame type (IETF Transport CONNECTION_CLOSE frame)
    {"",
     {0x1c}},
    // error code
    {"Unable to read connection close error code.",
     {kVarInt62OneByte + 0x11}},
    {"Unable to read connection close frame type.",
     {kVarInt62TwoBytes + 0x12, 0x34 }},
    {"Unable to read connection close error details.",
     {
       // error details length
       kVarInt62OneByte + 0x13,
       // error details
      '1',  '7',  '7',  '6',
      '7',  ':',  'b',  'e',
      'c',  'a',  'u',  's',
      'e',  ' ',  'I',  ' ',
      'c',  'a',  'n'}
    }
  };
  // clang-format on

  PacketFragments& fragments =
      VersionHasIetfQuicFrames(framer_.transport_version()) ? packet_ietf
                                                            : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  EXPECT_EQ(0x11u, static_cast<unsigned>(
                       visitor_.connection_close_frame_.wire_error_code));

  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    EXPECT_EQ(0x1234u,
              visitor_.connection_close_frame_.transport_close_frame_type);
    EXPECT_EQ(17767u, visitor_.connection_close_frame_.quic_error_code);
    EXPECT_EQ("because I can", visitor_.connection_close_frame_.error_details);
  } else {
    EXPECT_EQ(0x11u, visitor_.connection_close_frame_.quic_error_code);
    // Error code is not prepended in GQUIC, so it is not removed and should
    // remain in the reason phrase.
    EXPECT_EQ("17767:because I can",
              visitor_.connection_close_frame_.error_details);
  }

  ASSERT_EQ(0u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(fragments, QUIC_INVALID_CONNECTION_CLOSE_DATA);
}

// Test the CONNECTION_CLOSE/Application variant.
TEST_P(QuicFramerTest, ApplicationCloseFrame) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is only in IETF QUIC.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_CONNECTION_CLOSE/Application frame)
      {"",
       {0x1d}},
      // error code
      {"Unable to read connection close error code.",
       {kVarInt62TwoBytes + 0x00, 0x11}},
      {"Unable to read connection close error details.",
       {
         // error details length
         kVarInt62OneByte + 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(IETF_QUIC_APPLICATION_CONNECTION_CLOSE,
            visitor_.connection_close_frame_.close_type);
  EXPECT_EQ(122u, visitor_.connection_close_frame_.quic_error_code);
  EXPECT_EQ(0x11u, visitor_.connection_close_frame_.wire_error_code);
  EXPECT_EQ("because I can", visitor_.connection_close_frame_.error_details);

  ASSERT_EQ(0u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(packet_ietf, QUIC_INVALID_CONNECTION_CLOSE_DATA);
}

// Check that we can extract an error code from an application close.
TEST_P(QuicFramerTest, ApplicationCloseFrameExtract) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is only in IETF QUIC.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_CONNECTION_CLOSE/Application frame)
      {"",
       {0x1d}},
      // error code
      {"Unable to read connection close error code.",
       {kVarInt62OneByte + 0x11}},
      {"Unable to read connection close error details.",
       {
       // error details length
       kVarInt62OneByte + 0x13,
       // error details
       '1',  '7',  '7',  '6',
       '7',  ':',  'b',  'e',
       'c',  'a',  'u',  's',
       'e',  ' ',  'I',  ' ',
       'c',  'a',  'n'}
      }
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(IETF_QUIC_APPLICATION_CONNECTION_CLOSE,
            visitor_.connection_close_frame_.close_type);
  EXPECT_EQ(17767u, visitor_.connection_close_frame_.quic_error_code);
  EXPECT_EQ(0x11u, visitor_.connection_close_frame_.wire_error_code);
  EXPECT_EQ("because I can", visitor_.connection_close_frame_.error_details);

  ASSERT_EQ(0u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(packet_ietf, QUIC_INVALID_CONNECTION_CLOSE_DATA);
}

TEST_P(QuicFramerTest, GoAwayFrame) {
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is not in IETF QUIC.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (go away frame)
      {"",
       {0x03}},
      // error code
      {"Unable to read go away error code.",
       {0x00, 0x00, 0x00, 0x09}},
      // stream id
      {"Unable to read last good stream id.",
       {0x01, 0x02, 0x03, 0x04}},
      // stream id
      {"Unable to read goaway reason.",
       {
         // error details length
         0x0, 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(kStreamId, visitor_.goaway_frame_.last_good_stream_id);
  EXPECT_EQ(0x9u, visitor_.goaway_frame_.error_code);
  EXPECT_EQ("because I can", visitor_.goaway_frame_.reason_phrase);

  CheckFramingBoundaries(packet, QUIC_INVALID_GOAWAY_DATA);
}

TEST_P(QuicFramerTest, GoAwayFrameWithUnknownErrorCode) {
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is not in IETF QUIC.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (go away frame)
      {"",
       {0x03}},
      // error code larger than QUIC_LAST_ERROR
      {"Unable to read go away error code.",
       {0x00, 0x00, 0xC0, 0xDE}},
      // stream id
      {"Unable to read last good stream id.",
       {0x01, 0x02, 0x03, 0x04}},
      // stream id
      {"Unable to read goaway reason.",
       {
         // error details length
         0x0, 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(kStreamId, visitor_.goaway_frame_.last_good_stream_id);
  EXPECT_EQ(0xC0DE, visitor_.goaway_frame_.error_code);
  EXPECT_EQ("because I can", visitor_.goaway_frame_.reason_phrase);

  CheckFramingBoundaries(packet, QUIC_INVALID_GOAWAY_DATA);
}

TEST_P(QuicFramerTest, WindowUpdateFrame) {
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is not in IETF QUIC, see MaxDataFrame and MaxStreamDataFrame
    // for IETF QUIC equivalents.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (window update frame)
      {"",
       {0x04}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
      // byte offset
      {"Unable to read window byte_offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
  };

  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(kStreamId, visitor_.window_update_frame_.stream_id);
  EXPECT_EQ(kStreamOffset, visitor_.window_update_frame_.max_data);

  CheckFramingBoundaries(packet, QUIC_INVALID_WINDOW_UPDATE_DATA);
}

TEST_P(QuicFramerTest, MaxDataFrame) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is available only in IETF QUIC.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_MAX_DATA frame)
      {"",
       {0x10}},
      // byte offset
      {"Can not read MAX_DATA byte-offset",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(QuicUtils::GetInvalidStreamId(framer_.transport_version()),
            visitor_.window_update_frame_.stream_id);
  EXPECT_EQ(kStreamOffset, visitor_.window_update_frame_.max_data);

  CheckFramingBoundaries(packet_ietf, QUIC_INVALID_MAX_DATA_FRAME_DATA);
}

TEST_P(QuicFramerTest, MaxStreamDataFrame) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame available only in IETF QUIC.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_MAX_STREAM_DATA frame)
      {"",
       {0x11}},
      // stream id
      {"Unable to read IETF_MAX_STREAM_DATA frame stream id/count.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      // byte offset
      {"Can not read MAX_STREAM_DATA byte-count",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(kStreamId, visitor_.window_update_frame_.stream_id);
  EXPECT_EQ(kStreamOffset, visitor_.window_update_frame_.max_data);

  CheckFramingBoundaries(packet_ietf, QUIC_INVALID_MAX_STREAM_DATA_FRAME_DATA);
}

TEST_P(QuicFramerTest, BlockedFrame) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (blocked frame)
      {"",
       {0x05}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
  };

  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_STREAM_BLOCKED frame)
      {"",
       {0x15}},
      // stream id
      {"Unable to read IETF_STREAM_DATA_BLOCKED frame stream id/count.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      // Offset
      {"Can not read stream blocked offset.",
       {kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54}},
  };
  // clang-format on

  PacketFragments& fragments =
      VersionHasIetfQuicFrames(framer_.transport_version()) ? packet_ietf
                                                            : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    EXPECT_EQ(kStreamOffset, visitor_.blocked_frame_.offset);
  } else {
    EXPECT_EQ(0u, visitor_.blocked_frame_.offset);
  }
  EXPECT_EQ(kStreamId, visitor_.blocked_frame_.stream_id);

  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    CheckFramingBoundaries(fragments, QUIC_INVALID_STREAM_BLOCKED_DATA);
  } else {
    CheckFramingBoundaries(fragments, QUIC_INVALID_BLOCKED_DATA);
  }
}

TEST_P(QuicFramerTest, PingFrame) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
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
    };

  unsigned char packet_ietf[] = {
     // type (short header, 4 byte packet number)
     0x43,
     // connection_id
     0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
     // packet number
     0x12, 0x34, 0x56, 0x78,

     // frame type (IETF_PING frame)
     0x01,
    };
  // clang-format on

  QuicEncryptedPacket encrypted(
      AsChars(VersionHasIetfQuicFrames(framer_.transport_version())
                  ? packet_ietf
                  : packet),
      VersionHasIetfQuicFrames(framer_.transport_version())
          ? ABSL_ARRAYSIZE(packet_ietf)
          : ABSL_ARRAYSIZE(packet),
      false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(1u, visitor_.ping_frames_.size());

  // No need to check the PING frame boundaries because it has no payload.
}

TEST_P(QuicFramerTest, HandshakeDoneFrame) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
     // type (short header, 4 byte packet number)
     0x43,
     // connection_id
     0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
     // packet number
     0x12, 0x34, 0x56, 0x78,

     // frame type (Handshake done frame)
     0x1e,
    };
  // clang-format on

  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }

  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(1u, visitor_.handshake_done_frames_.size());
}

TEST_P(QuicFramerTest, ParseAckFrequencyFrame) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
     // type (short header, 4 byte packet number)
     0x43,
     // connection_id
     0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
     // packet number
     0x12, 0x34, 0x56, 0x78,

     // ack frequency frame type (which needs two bytes as it is > 0x3F)
     0x40, 0xAF,
     // sequence_number
     0x11,
     // packet_tolerance
     0x02,
     // max_ack_delay_us = 2'5000 us
     0x80, 0x00, 0x61, 0xA8,
     // ignore_order
     0x01
  };
  // clang-format on

  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }

  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  ASSERT_EQ(1u, visitor_.ack_frequency_frames_.size());
  const auto& frame = visitor_.ack_frequency_frames_.front();
  EXPECT_EQ(17u, frame->sequence_number);
  EXPECT_EQ(2u, frame->packet_tolerance);
  EXPECT_EQ(2'5000u, frame->max_ack_delay.ToMicroseconds());
  EXPECT_EQ(true, frame->ignore_order);
}

TEST_P(QuicFramerTest, ParseImmediateAckFrame) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
     // type (short header, 4 byte packet number)
     0x43,
     // connection_id
     0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
     // packet number
     0x12, 0x34, 0x56, 0x78,

     // immediate ack frame type
     0x1F,
  };
  // clang-format on

  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }

  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  ASSERT_EQ(1u, visitor_.immediate_ack_frames_.size());
}

TEST_P(QuicFramerTest, ParseResetStreamAtFrame) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // type = RESET_STREAM_AT
      0x24,
      // stream ID
      0x00,
      // application error code
      0x1e,
      // final size
      0x20,
      // reliable size
      0x10,
  };
  // clang-format on

  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  framer_.set_process_reset_stream_at(true);

  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  ASSERT_EQ(visitor_.reset_stream_at_frames_.size(), 1);
  const QuicResetStreamAtFrame& frame = *visitor_.reset_stream_at_frames_[0];
  EXPECT_EQ(frame.stream_id, 0x00);
  EXPECT_EQ(frame.error, 0x1e);
  EXPECT_EQ(frame.final_offset, 0x20);
  EXPECT_EQ(frame.reliable_offset, 0x10);
}

TEST_P(QuicFramerTest, ParseInvalidResetStreamAtFrame) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // type = RESET_STREAM_AT
      0x24,
      // stream ID
      0x00,
      // application error code
      0x1e,
      // final size
      0x20,
      // reliable size
      0x30,
  };
  // clang-format on

  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  framer_.set_process_reset_stream_at(true);

  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(framer_.error(), QUIC_INVALID_FRAME_DATA);
  EXPECT_EQ(visitor_.reset_stream_at_frames_.size(), 0);
}

TEST_P(QuicFramerTest, MessageFrame) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
       // type (short header, 4 byte packet number)
       {"",
        {0x43}},
       // connection_id
       {"",
        {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
       // packet number
       {"",
        {0x12, 0x34, 0x56, 0x78}},
       // message frame type.
       {"",
        { 0x21 }},
       // message length
       {"Unable to read message length",
        {0x07}},
       // message data
       {"Unable to read message data",
        {'m', 'e', 's', 's', 'a', 'g', 'e'}},
        // message frame no length.
        {"",
         { 0x20 }},
        // message data
        {{},
         {'m', 'e', 's', 's', 'a', 'g', 'e', '2'}},
   };
  PacketFragments packet_ietf = {
       // type (short header, 4 byte packet number)
       {"",
        {0x43}},
       // connection_id
       {"",
        {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
       // packet number
       {"",
        {0x12, 0x34, 0x56, 0x78}},
       // message frame type.
       {"",
        { 0x31 }},
       // message length
       {"Unable to read message length",
        {0x07}},
       // message data
       {"Unable to read message data",
        {'m', 'e', 's', 's', 'a', 'g', 'e'}},
        // message frame no length.
        {"",
         { 0x30 }},
        // message data
        {{},
         {'m', 'e', 's', 's', 'a', 'g', 'e', '2'}},
   };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted;
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    encrypted = AssemblePacketFromFragments(packet_ietf);
  } else {
    encrypted = AssemblePacketFromFragments(packet);
  }
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  ASSERT_EQ(2u, visitor_.message_frames_.size());
  EXPECT_EQ(7u, visitor_.message_frames_[0]->message_length);
  EXPECT_EQ(8u, visitor_.message_frames_[1]->message_length);

  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    CheckFramingBoundaries(packet_ietf, QUIC_INVALID_MESSAGE_DATA);
  } else {
    CheckFramingBoundaries(packet, QUIC_INVALID_MESSAGE_DATA);
  }
}

TEST_P(QuicFramerTest, IetfStatelessResetPacket) {
  // clang-format off
  unsigned char packet[] = {
      // type (short packet, 1 byte packet number)
      0x50,
      // Random bytes
      0x01, 0x11, 0x02, 0x22, 0x03, 0x33, 0x04, 0x44,
      0x01, 0x11, 0x02, 0x22, 0x03, 0x33, 0x04, 0x44,
      0x01, 0x11, 0x02, 0x22, 0x03, 0x33, 0x04, 0x44,
      0x01, 0x11, 0x02, 0x22, 0x03, 0x33, 0x04, 0x44,
      // stateless reset token
      0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
      0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
  };
  // clang-format on
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicFramerPeer::SetLastSerializedServerConnectionId(&framer_,
                                                      TestConnectionId(0x33));
  decrypter_ = new test::TestDecrypter();
  if (framer_.version().KnowsWhichDecrypterToUse()) {
    framer_.InstallDecrypter(
        ENCRYPTION_INITIAL,
        std::make_unique<NullDecrypter>(Perspective::IS_CLIENT));
    framer_.InstallDecrypter(ENCRYPTION_ZERO_RTT,
                             std::unique_ptr<QuicDecrypter>(decrypter_));
  } else {
    framer_.SetDecrypter(ENCRYPTION_INITIAL, std::make_unique<NullDecrypter>(
                                                 Perspective::IS_CLIENT));
    framer_.SetAlternativeDecrypter(
        ENCRYPTION_ZERO_RTT, std::unique_ptr<QuicDecrypter>(decrypter_), false);
  }
  // This packet cannot be decrypted because diversification nonce is missing.
  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  ASSERT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.stateless_reset_packet_.get());
  EXPECT_EQ(kTestStatelessResetToken,
            visitor_.stateless_reset_packet_->stateless_reset_token);
}

TEST_P(QuicFramerTest, IetfStatelessResetPacketInvalidStatelessResetToken) {
  // clang-format off
  unsigned char packet[] = {
      // type (short packet, 1 byte packet number)
      0x50,
      // Random bytes
      0x01, 0x11, 0x02, 0x22, 0x03, 0x33, 0x04, 0x44,
      0x01, 0x11, 0x02, 0x22, 0x03, 0x33, 0x04, 0x44,
      0x01, 0x11, 0x02, 0x22, 0x03, 0x33, 0x04, 0x44,
      0x01, 0x11, 0x02, 0x22, 0x03, 0x33, 0x04, 0x44,
      // stateless reset token
      0xB6, 0x69, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  // clang-format on
  QuicFramerPeer::SetLastSerializedServerConnectionId(&framer_,
                                                      TestConnectionId(0x33));
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  decrypter_ = new test::TestDecrypter();
  if (framer_.version().KnowsWhichDecrypterToUse()) {
    framer_.InstallDecrypter(
        ENCRYPTION_INITIAL,
        std::make_unique<NullDecrypter>(Perspective::IS_CLIENT));
    framer_.InstallDecrypter(ENCRYPTION_ZERO_RTT,
                             std::unique_ptr<QuicDecrypter>(decrypter_));
  } else {
    framer_.SetDecrypter(ENCRYPTION_INITIAL, std::make_unique<NullDecrypter>(
                                                 Perspective::IS_CLIENT));
    framer_.SetAlternativeDecrypter(
        ENCRYPTION_ZERO_RTT, std::unique_ptr<QuicDecrypter>(decrypter_), false);
  }
  // This packet cannot be decrypted because diversification nonce is missing.
  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_THAT(framer_.error(), IsError(QUIC_DECRYPTION_FAILURE));
  ASSERT_FALSE(visitor_.stateless_reset_packet_);
}

TEST_P(QuicFramerTest, VersionNegotiationPacketClient) {
  // clang-format off
  PacketFragments packet = {
      // type (long header)
      {"",
       {0x8F}},
      // version tag
      {"",
       {0x00, 0x00, 0x00, 0x00}},
      {"",
       {0x05}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // Supported versions
      {"Unable to read supported version in negotiation.",
       {QUIC_VERSION_BYTES,
        'Q', '2', '.', '0'}},
  };

  PacketFragments packet49 = {
      // type (long header)
      {"",
       {0x8F}},
      // version tag
      {"",
       {0x00, 0x00, 0x00, 0x00}},
      {"",
       {0x08}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      {"",
       {0x00}},
      // Supported versions
      {"Unable to read supported version in negotiation.",
       {QUIC_VERSION_BYTES,
        'Q', '2', '.', '0'}},
  };
  // clang-format on

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  PacketFragments& fragments =
      framer_.version().HasLongHeaderLengths() ? packet49 : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  ASSERT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.version_negotiation_packet_.get());
  EXPECT_EQ(1u, visitor_.version_negotiation_packet_->versions.size());
  EXPECT_EQ(GetParam(), visitor_.version_negotiation_packet_->versions[0]);

  // Remove the last version from the packet so that every truncated
  // version of the packet is invalid, otherwise checking boundaries
  // is annoyingly complicated.
  for (size_t i = 0; i < 4; ++i) {
    fragments.back().fragment.pop_back();
  }
  CheckFramingBoundaries(fragments, QUIC_INVALID_VERSION_NEGOTIATION_PACKET);
}

TEST_P(QuicFramerTest, VersionNegotiationPacketServer) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // clang-format off
  unsigned char packet[] = {
      // public flags (long header with all ignored bits set)
      0xFF,
      // version
      0x00, 0x00, 0x00, 0x00,
      // connection ID lengths
      0x50,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
      // supported versions
      QUIC_VERSION_BYTES,
      'Q', '2', '.', '0',
  };
  unsigned char packet2[] = {
      // public flags (long header with all ignored bits set)
      0xFF,
      // version
      0x00, 0x00, 0x00, 0x00,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
      // source connection ID length
      0x00,
      // supported versions
      QUIC_VERSION_BYTES,
      'Q', '2', '.', '0',
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_length = ABSL_ARRAYSIZE(packet);
  if (framer_.version().HasLengthPrefixedConnectionIds()) {
    p = packet2;
    p_length = ABSL_ARRAYSIZE(packet2);
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_length, false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_THAT(framer_.error(),
              IsError(QUIC_INVALID_VERSION_NEGOTIATION_PACKET));
  EXPECT_EQ("Server received version negotiation packet.",
            framer_.detailed_error());
  EXPECT_FALSE(visitor_.version_negotiation_packet_.get());
}

TEST_P(QuicFramerTest, ParseIetfRetryPacket) {
  if (!framer_.version().SupportsRetry()) {
    return;
  }
  // IETF RETRY is only sent from client to server.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  // clang-format off
  unsigned char packet[] = {
      // public flags (long header with packet type RETRY and ODCIL=8)
      0xF5,
      // version
      QUIC_VERSION_BYTES,
      // connection ID lengths
      0x05,
      // source connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
      // original destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // retry token
      'H', 'e', 'l', 'l', 'o', ' ', 't', 'h', 'i', 's',
      ' ', 'i', 's', ' ', 'R', 'E', 'T', 'R', 'Y', '!',
  };
  unsigned char packet49[] = {
      // public flags (long header with packet type RETRY)
      0xF0,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x00,
      // source connection ID length
      0x08,
      // source connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
      // original destination connection ID length
      0x08,
      // original destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // retry token
      'H', 'e', 'l', 'l', 'o', ' ', 't', 'h', 'i', 's',
      ' ', 'i', 's', ' ', 'R', 'E', 'T', 'R', 'Y', '!',
  };
  unsigned char packet_with_tag[] = {
      // public flags (long header with packet type RETRY)
      0xF0,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x00,
      // source connection ID length
      0x08,
      // source connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
      // retry token
      'H', 'e', 'l', 'l', 'o', ' ', 't', 'h', 'i', 's',
      ' ', 'i', 's', ' ', 'R', 'E', 'T', 'R', 'Y', '!',
      // retry token integrity tag
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_length = ABSL_ARRAYSIZE(packet);
  if (framer_.version().UsesTls()) {
    ReviseFirstByteByVersion(packet_with_tag);
    p = packet_with_tag;
    p_length = ABSL_ARRAYSIZE(packet_with_tag);
  } else if (framer_.version().HasLongHeaderLengths()) {
    p = packet49;
    p_length = ABSL_ARRAYSIZE(packet49);
  }
  QuicEncryptedPacket encrypted(AsChars(p), p_length, false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());

  ASSERT_TRUE(visitor_.on_retry_packet_called_);
  ASSERT_TRUE(visitor_.retry_new_connection_id_.get());
  ASSERT_TRUE(visitor_.retry_token_.get());

  if (framer_.version().UsesTls()) {
    ASSERT_TRUE(visitor_.retry_token_integrity_tag_.get());
    static const unsigned char expected_integrity_tag[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    };
    quiche::test::CompareCharArraysWithHexError(
        "retry integrity tag", visitor_.retry_token_integrity_tag_->data(),
        visitor_.retry_token_integrity_tag_->length(),
        reinterpret_cast<const char*>(expected_integrity_tag),
        ABSL_ARRAYSIZE(expected_integrity_tag));
    ASSERT_TRUE(visitor_.retry_without_tag_.get());
    quiche::test::CompareCharArraysWithHexError(
        "retry without tag", visitor_.retry_without_tag_->data(),
        visitor_.retry_without_tag_->length(),
        reinterpret_cast<const char*>(packet_with_tag), 35);
  } else {
    ASSERT_TRUE(visitor_.retry_original_connection_id_.get());
    EXPECT_EQ(FramerTestConnectionId(),
              *visitor_.retry_original_connection_id_.get());
  }

  EXPECT_EQ(FramerTestConnectionIdPlusOne(),
            *visitor_.retry_new_connection_id_.get());
  EXPECT_EQ("Hello this is RETRY!", *visitor_.retry_token_.get());

  // IETF RETRY is only sent from client to server, the rest of this test
  // ensures that the server correctly drops them without acting on them.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // Reset our visitor state to default settings.
  visitor_.retry_original_connection_id_.reset();
  visitor_.retry_new_connection_id_.reset();
  visitor_.retry_token_.reset();
  visitor_.retry_token_integrity_tag_.reset();
  visitor_.retry_without_tag_.reset();
  visitor_.on_retry_packet_called_ = false;

  EXPECT_FALSE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsError(QUIC_INVALID_PACKET_HEADER));
  EXPECT_EQ("Client-initiated RETRY is invalid.", framer_.detailed_error());

  EXPECT_FALSE(visitor_.on_retry_packet_called_);
  EXPECT_FALSE(visitor_.retry_new_connection_id_.get());
  EXPECT_FALSE(visitor_.retry_token_.get());
  EXPECT_FALSE(visitor_.retry_token_integrity_tag_.get());
  EXPECT_FALSE(visitor_.retry_without_tag_.get());
}

TEST_P(QuicFramerTest, BuildPaddingFramePacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  // clang-format off
  unsigned char packet[kMaxOutgoingPacketSize] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet_ietf[kMaxOutgoingPacketSize] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  unsigned char* p = packet;
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
  }

  uint64_t header_size = GetPacketHeaderSize(
      framer_.transport_version(), kPacket8ByteConnectionId,
      kPacket0ByteConnectionId, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_4BYTE_PACKET_NUMBER, quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0, 0,
      quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0);
  memset(p + header_size + 1, 0x00, kMaxOutgoingPacketSize - header_size - 1);

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildStreamFramePacketWithNewPaddingFrame) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  QuicStreamFrame stream_frame(kStreamId, true, kStreamOffset,
                               absl::string_view("hello world!"));
  QuicPaddingFrame padding_frame(2);
  QuicFrames frames = {QuicFrame(padding_frame), QuicFrame(stream_frame),
                       QuicFrame(padding_frame)};

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // paddings
    0x00, 0x00,
    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };

  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // paddings
    0x00, 0x00,
    // frame type (IETF_STREAM with FIN, LEN, and OFFSET bits set)
    0x08 | 0x01 | 0x02 | 0x04,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // offset
    kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    kVarInt62OneByte + 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
    p_size = ABSL_ARRAYSIZE(packet_ietf);
  }
  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, Build4ByteSequenceNumberPaddingFramePacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number_length = PACKET_4BYTE_PACKET_NUMBER;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  // clang-format off
  unsigned char packet[kMaxOutgoingPacketSize] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet_ietf[kMaxOutgoingPacketSize] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  unsigned char* p = packet;
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
  }

  uint64_t header_size = GetPacketHeaderSize(
      framer_.transport_version(), kPacket8ByteConnectionId,
      kPacket0ByteConnectionId, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_4BYTE_PACKET_NUMBER, quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0, 0,
      quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0);
  memset(p + header_size + 1, 0x00, kMaxOutgoingPacketSize - header_size - 1);

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, Build2ByteSequenceNumberPaddingFramePacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number_length = PACKET_2BYTE_PACKET_NUMBER;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  // clang-format off
  unsigned char packet[kMaxOutgoingPacketSize] = {
    // type (short header, 2 byte packet number)
    0x41,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet_ietf[kMaxOutgoingPacketSize] = {
    // type (short header, 2 byte packet number)
    0x41,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  unsigned char* p = packet;
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
  }

  uint64_t header_size = GetPacketHeaderSize(
      framer_.transport_version(), kPacket8ByteConnectionId,
      kPacket0ByteConnectionId, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_2BYTE_PACKET_NUMBER, quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0, 0,
      quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0);
  memset(p + header_size + 1, 0x00, kMaxOutgoingPacketSize - header_size - 1);

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, Build1ByteSequenceNumberPaddingFramePacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number_length = PACKET_1BYTE_PACKET_NUMBER;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  // clang-format off
  unsigned char packet[kMaxOutgoingPacketSize] = {
    // type (short header, 1 byte packet number)
    0x40,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet_ietf[kMaxOutgoingPacketSize] = {
    // type (short header, 1 byte packet number)
    0x40,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  unsigned char* p = packet;
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
  }

  uint64_t header_size = GetPacketHeaderSize(
      framer_.transport_version(), kPacket8ByteConnectionId,
      kPacket0ByteConnectionId, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_1BYTE_PACKET_NUMBER, quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0, 0,
      quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0);
  memset(p + header_size + 1, 0x00, kMaxOutgoingPacketSize - header_size - 1);

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildStreamFramePacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  if (QuicVersionHasLongHeaderLengths(framer_.transport_version())) {
    header.length_length = quiche::VARIABLE_LENGTH_INTEGER_LENGTH_2;
  }

  QuicStreamFrame stream_frame(kStreamId, true, kStreamOffset,
                               absl::string_view("hello world!"));

  QuicFrames frames = {QuicFrame(stream_frame)};

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stream frame with fin and no length)
    0xDF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_STREAM frame with FIN and OFFSET, no length)
    0x08 | 0x01 | 0x04,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // offset
    kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
    p_size = ABSL_ARRAYSIZE(packet_ietf);
  }
  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildStreamFramePacketWithVersionFlag) {
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = true;
  header.long_packet_type = ZERO_RTT_PROTECTED;
  header.packet_number = kPacketNumber;
  if (QuicVersionHasLongHeaderLengths(framer_.transport_version())) {
    header.length_length = quiche::VARIABLE_LENGTH_INTEGER_LENGTH_2;
  }

  QuicStreamFrame stream_frame(kStreamId, true, kStreamOffset,
                               absl::string_view("hello world!"));
  QuicFrames frames = {QuicFrame(stream_frame)};

  // clang-format off
  unsigned char packet[] = {
      // type (long header with packet type ZERO_RTT_PROTECTED)
      0xD3,
      // version tag
      QUIC_VERSION_BYTES,
      // connection_id length
      0x50,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (stream frame with fin and no length)
      0xDF,
      // stream id
      0x01, 0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data
      'h',  'e',  'l',  'l',  'o',  ' ',  'w',  'o',  'r', 'l', 'd', '!',
  };

  unsigned char packet49[] = {
      // type (long header with packet type ZERO_RTT_PROTECTED)
      0xD3,
      // version tag
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // length
      0x40, 0x1D,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (stream frame with fin and no length)
      0xDF,
      // stream id
      0x01, 0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data
      'h',  'e',  'l',  'l',  'o',  ' ',  'w',  'o',  'r', 'l', 'd', '!',
  };

  unsigned char packet_ietf[] = {
      // type (long header with packet type ZERO_RTT_PROTECTED)
      0xD3,
      // version tag
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // length
      0x40, 0x1D,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (IETF_STREAM frame with fin and offset, no length)
      0x08 | 0x01 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data
      'h',  'e',  'l',  'l',  'o',  ' ',  'w',  'o',  'r', 'l', 'd', '!',
  };
  // clang-format on

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    ReviseFirstByteByVersion(packet_ietf);
    p = packet_ietf;
    p_size = ABSL_ARRAYSIZE(packet_ietf);
  } else if (framer_.version().HasLongHeaderLengths()) {
    p = packet49;
    p_size = ABSL_ARRAYSIZE(packet49);
  }
  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildCryptoFramePacket) {
  if (!QuicVersionUsesCryptoFrames(framer_.transport_version())) {
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  SimpleDataProducer data_producer;
  framer_.set_data_producer(&data_producer);

  absl::string_view crypto_frame_contents("hello world!");
  QuicCryptoFrame crypto_frame(ENCRYPTION_INITIAL, kStreamOffset,
                               crypto_frame_contents.length());
  data_producer.SaveCryptoData(ENCRYPTION_INITIAL, kStreamOffset,
                               crypto_frame_contents);

  QuicFrames frames = {QuicFrame(&crypto_frame)};

  // clang-format off
  unsigned char packet48[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (QuicFrameType CRYPTO_FRAME)
    0x08,
    // offset
    kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // length
    kVarInt62OneByte + 12,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_CRYPTO frame)
    0x06,
    // offset
    kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // length
    kVarInt62OneByte + 12,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };
  // clang-format on

  unsigned char* packet = packet48;
  size_t packet_size = ABSL_ARRAYSIZE(packet48);
  if (framer_.version().HasIetfQuicFrames()) {
    packet = packet_ietf;
    packet_size = ABSL_ARRAYSIZE(packet_ietf);
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  quiche::test::CompareCharArraysWithHexError("constructed packet",
                                              data->data(), data->length(),
                                              AsChars(packet), packet_size);
}

TEST_P(QuicFramerTest, CryptoFrame) {
  if (!QuicVersionUsesCryptoFrames(framer_.transport_version())) {
    // CRYPTO frames aren't supported prior to v48.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet48 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (QuicFrameType CRYPTO_FRAME)
      {"",
       {0x08}},
      // offset
      {"",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // data length
      {"Invalid data length.",
       {kVarInt62OneByte + 12}},
      // data
      {"Unable to read frame data.",
       {'h',  'e',  'l',  'l',
        'o',  ' ',  'w',  'o',
        'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_CRYPTO frame)
      {"",
       {0x06}},
      // offset
      {"",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // data length
      {"Invalid data length.",
       {kVarInt62OneByte + 12}},
      // data
      {"Unable to read frame data.",
       {'h',  'e',  'l',  'l',
        'o',  ' ',  'w',  'o',
        'r',  'l',  'd',  '!'}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.version().HasIetfQuicFrames() ? packet_ietf : packet48;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));
  ASSERT_EQ(1u, visitor_.crypto_frames_.size());
  QuicCryptoFrame* frame = visitor_.crypto_frames_[0].get();
  EXPECT_EQ(ENCRYPTION_FORWARD_SECURE, frame->level);
  EXPECT_EQ(kStreamOffset, frame->offset);
  EXPECT_EQ("hello world!",
            std::string(frame->data_buffer, frame->data_length));

  CheckFramingBoundaries(fragments, QUIC_INVALID_FRAME_DATA);
}

TEST_P(QuicFramerTest, BuildOldVersionNegotiationPacket) {
  SetQuicFlag(quic_disable_version_negotiation_grease_randomness, true);
  // clang-format off
  unsigned char packet[] = {
      // public flags (version, 8 byte connection_id)
      0x0D,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // supported versions
      0xDA, 0x5A, 0x3A, 0x3A,
      QUIC_VERSION_BYTES,
  };
  QuicConnectionId connection_id = FramerTestConnectionId();
  std::unique_ptr<QuicEncryptedPacket> data(
      QuicFramer::BuildVersionNegotiationPacket(
          connection_id, EmptyQuicConnectionId(), /*ietf_quic=*/false,
          /*use_length_prefix=*/false,
          SupportedVersions(GetParam())));
  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet),
      ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildVersionNegotiationPacket) {
  SetQuicFlag(quic_disable_version_negotiation_grease_randomness, true);
  // clang-format off
  unsigned char packet[] = {
      // type (long header)
      0xC0,
      // version tag
      0x00, 0x00, 0x00, 0x00,
      // connection_id length
      0x05,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // supported versions
      0xDA, 0x5A, 0x3A, 0x3A,
      QUIC_VERSION_BYTES,
  };
  unsigned char packet49[] = {
      // type (long header)
      0xC0,
      // version tag
      0x00, 0x00, 0x00, 0x00,
      // destination connection ID length
      0x00,
      // source connection ID length
      0x08,
      // source connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // supported versions
      0xDA, 0x5A, 0x3A, 0x3A,
      QUIC_VERSION_BYTES,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (framer_.version().HasLongHeaderLengths()) {
    p = packet49;
    p_size = ABSL_ARRAYSIZE(packet49);
  }

  QuicConnectionId connection_id = FramerTestConnectionId();
  std::unique_ptr<QuicEncryptedPacket> data(
      QuicFramer::BuildVersionNegotiationPacket(
          connection_id, EmptyQuicConnectionId(), /*ietf_quic=*/true,
          framer_.version().HasLengthPrefixedConnectionIds(),
          SupportedVersions(GetParam())));
  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildVersionNegotiationPacketWithClientConnectionId) {
  if (!framer_.version().SupportsClientConnectionIds()) {
    return;
  }

  SetQuicFlag(quic_disable_version_negotiation_grease_randomness, true);

  // clang-format off
  unsigned char packet[] = {
      // type (long header)
      0xC0,
      // version tag
      0x00, 0x00, 0x00, 0x00,
      // client/destination connection ID
      0x08,
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
      // server/source connection ID
      0x08,
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // supported versions
      0xDA, 0x5A, 0x3A, 0x3A,
      QUIC_VERSION_BYTES,
  };
  // clang-format on

  QuicConnectionId server_connection_id = FramerTestConnectionId();
  QuicConnectionId client_connection_id = FramerTestConnectionIdPlusOne();
  std::unique_ptr<QuicEncryptedPacket> data(
      QuicFramer::BuildVersionNegotiationPacket(
          server_connection_id, client_connection_id, true, true,
          SupportedVersions(GetParam())));
  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet),
      ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildAckFramePacketOneAckBlock) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  // Use kSmallLargestObserved to make this test finished in a short time.
  QuicAckFrame ack_frame = InitAckFrame(kSmallLargestObserved);
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();

  QuicFrames frames = {QuicFrame(&ack_frame)};

  // clang-format off
  unsigned char packet[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 2 byte largest observed, 2 byte block length)
      0x45,
      // largest acked
      0x12, 0x34,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x12, 0x34,
      // num timestamps.
      0x00,
  };

  unsigned char packet_ietf[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (IETF_ACK frame)
      0x02,
      // largest acked
      kVarInt62TwoBytes + 0x12, 0x34,
      // Zero delta time.
      kVarInt62OneByte + 0x00,
      // Number of additional ack blocks.
      kVarInt62OneByte + 0x00,
      // first ack block length.
      kVarInt62TwoBytes + 0x12, 0x33,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
    p_size = ABSL_ARRAYSIZE(packet_ietf);
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildAckReceiveTimestampsFrameMultipleRanges) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrame ack_frame = InitAckFrame(kSmallLargestObserved);
  ack_frame.received_packet_times = PacketTimeVector{
      // Timestamp Range 3.
      {kSmallLargestObserved - 22, CreationTimePlus(0x29ffdddd)},
      {kSmallLargestObserved - 21, CreationTimePlus(0x29ffdedd)},
      // Timestamp Range 2.
      {kSmallLargestObserved - 11, CreationTimePlus(0x29ffdeed)},
      // Timestamp Range 1.
      {kSmallLargestObserved - 4, CreationTimePlus(0x29ffeeed)},
      {kSmallLargestObserved - 3, CreationTimePlus(0x29ffeeee)},
      {kSmallLargestObserved - 2, CreationTimePlus(0x29ffffff)},
  };
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();
  QuicFrames frames = {QuicFrame(&ack_frame)};

  unsigned char packet_ietf[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE,
      0xDC,
      0xBA,
      0x98,
      0x76,
      0x54,
      0x32,
      0x10,
      // packet number
      0x12,
      0x34,
      0x56,
      0x78,

      // frame type (IETF_ACK_RECEIVE_TIMESTAMPS frame)
      0x22,
      // largest acked
      kVarInt62TwoBytes + 0x12,
      0x34,  // = 4660
      // Zero delta time.
      kVarInt62OneByte + 0x00,
      // number of additional ack blocks
      kVarInt62OneByte + 0x00,
      // first ack block length.
      kVarInt62TwoBytes + 0x12,
      0x33,

      // Receive Timestamps.

      // Timestamp Range Count
      kVarInt62OneByte + 0x03,

      // Timestamp range 1 (three packets).
      // Gap
      kVarInt62OneByte + 0x02,
      // Timestamp Range Count
      kVarInt62OneByte + 0x03,
      // Timestamp Delta
      kVarInt62FourBytes + 0x29,
      0xff,
      0xff,
      0xff,
      // Timestamp Delta
      kVarInt62TwoBytes + 0x11,
      0x11,
      // Timestamp Delta
      kVarInt62OneByte + 0x01,

      // Timestamp range 2 (one packet).
      // Gap
      kVarInt62OneByte + 0x05,
      // Timestamp Range Count
      kVarInt62OneByte + 0x01,
      // Timestamp Delta
      kVarInt62TwoBytes + 0x10,
      0x00,

      // Timestamp range 3 (two packets).
      // Gap
      kVarInt62OneByte + 0x08,
      // Timestamp Range Count
      kVarInt62OneByte + 0x02,
      // Timestamp Delta
      kVarInt62OneByte + 0x10,
      // Timestamp Delta
      kVarInt62TwoBytes + 0x01,
      0x00,
  };
  // clang-format on

  framer_.set_process_timestamps(true);
  framer_.set_max_receive_timestamps_per_ack(8);
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet_ietf),
      ABSL_ARRAYSIZE(packet_ietf));
}

TEST_P(QuicFramerTest, BuildAckReceiveTimestampsFrameExceedsMaxTimestamps) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrame ack_frame = InitAckFrame(kSmallLargestObserved);
  ack_frame.received_packet_times = PacketTimeVector{
      // Timestamp Range 3 (not included because max receive timestamps = 4).
      {kSmallLargestObserved - 20, CreationTimePlus(0x29ffdddd)},
      // Timestamp Range 2.
      {kSmallLargestObserved - 10, CreationTimePlus(0x29ffdedd)},
      {kSmallLargestObserved - 9, CreationTimePlus(0x29ffdeed)},
      // Timestamp Range 1.
      {kSmallLargestObserved - 2, CreationTimePlus(0x29ffeeed)},
      {kSmallLargestObserved - 1, CreationTimePlus(0x29ffeeee)},
      {kSmallLargestObserved, CreationTimePlus(0x29ffffff)},
  };
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();
  QuicFrames frames = {QuicFrame(&ack_frame)};

  unsigned char packet_ietf[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE,
      0xDC,
      0xBA,
      0x98,
      0x76,
      0x54,
      0x32,
      0x10,
      // packet number
      0x12,
      0x34,
      0x56,
      0x78,

      // frame type (IETF_ACK_RECEIVE_TIMESTAMPS frame)
      0x22,
      // largest acked
      kVarInt62TwoBytes + 0x12,
      0x34,  // = 4660
      // Zero delta time.
      kVarInt62OneByte + 0x00,
      // number of additional ack blocks
      kVarInt62OneByte + 0x00,
      // first ack block length.
      kVarInt62TwoBytes + 0x12,
      0x33,

      // Receive Timestamps.

      // Timestamp Range Count
      kVarInt62OneByte + 0x02,

      // Timestamp range 1 (three packets).
      // Gap
      kVarInt62OneByte + 0x00,
      // Timestamp Range Count
      kVarInt62OneByte + 0x03,
      // Timestamp Delta
      kVarInt62FourBytes + 0x29,
      0xff,
      0xff,
      0xff,
      // Timestamp Delta
      kVarInt62TwoBytes + 0x11,
      0x11,
      // Timestamp Delta
      kVarInt62OneByte + 0x01,

      // Timestamp range 2 (one packet).
      // Gap
      kVarInt62OneByte + 0x05,
      // Timestamp Range Count
      kVarInt62OneByte + 0x01,
      // Timestamp Delta
      kVarInt62TwoBytes + 0x10,
      0x00,
  };
  // clang-format on

  framer_.set_process_timestamps(true);
  framer_.set_max_receive_timestamps_per_ack(4);
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet_ietf),
      ABSL_ARRAYSIZE(packet_ietf));
}

TEST_P(QuicFramerTest, BuildAckReceiveTimestampsFrameWithExponentEncoding) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrame ack_frame = InitAckFrame(kSmallLargestObserved);
  ack_frame.received_packet_times = PacketTimeVector{
      // Timestamp Range 2.
      {kSmallLargestObserved - 12, CreationTimePlus((0x06c00 << 3) + 0x03)},
      {kSmallLargestObserved - 11, CreationTimePlus((0x28e00 << 3) + 0x00)},
      // Timestamp Range 1.
      {kSmallLargestObserved - 5, CreationTimePlus((0x29f00 << 3) + 0x00)},
      {kSmallLargestObserved - 4, CreationTimePlus((0x29f00 << 3) + 0x01)},
      {kSmallLargestObserved - 3, CreationTimePlus((0x29f00 << 3) + 0x02)},
      {kSmallLargestObserved - 2, CreationTimePlus((0x29f00 << 3) + 0x03)},
  };
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();

  QuicFrames frames = {QuicFrame(&ack_frame)};

  unsigned char packet_ietf[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE,
      0xDC,
      0xBA,
      0x98,
      0x76,
      0x54,
      0x32,
      0x10,
      // packet number
      0x12,
      0x34,
      0x56,
      0x78,

      // frame type (IETF_ACK_RECEIVE_TIMESTAMPS frame)
      0x22,
      // largest acked
      kVarInt62TwoBytes + 0x12,
      0x34,  // = 4660
      // Zero delta time.
      kVarInt62OneByte + 0x00,
      // number of additional ack blocks
      kVarInt62OneByte + 0x00,
      // first ack block length.
      kVarInt62TwoBytes + 0x12,
      0x33,

      // Receive Timestamps.

      // Timestamp Range Count
      kVarInt62OneByte + 0x02,

      // Timestamp range 1 (three packets).
      // Gap
      kVarInt62OneByte + 0x02,
      // Timestamp Range Count
      kVarInt62OneByte + 0x04,
      // Timestamp Delta
      kVarInt62FourBytes + 0x00,
      0x02,
      0x9f,
      0x01,  // round up
      // Timestamp Delta
      kVarInt62OneByte + 0x00,
      // Timestamp Delta
      kVarInt62OneByte + 0x00,
      // Timestamp Delta
      kVarInt62OneByte + 0x01,

      // Timestamp range 2 (one packet).
      // Gap
      kVarInt62OneByte + 0x04,
      // Timestamp Range Count
      kVarInt62OneByte + 0x02,
      // Timestamp Delta
      kVarInt62TwoBytes + 0x11,
      0x00,
      // Timestamp Delta
      kVarInt62FourBytes + 0x00,
      0x02,
      0x21,
      0xff,
  };
  // clang-format on

  framer_.set_process_timestamps(true);
  framer_.set_max_receive_timestamps_per_ack(8);
  framer_.set_receive_timestamps_exponent(3);
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet_ietf),
      ABSL_ARRAYSIZE(packet_ietf));
}

TEST_P(QuicFramerTest, BuildAndProcessAckReceiveTimestampsWithMultipleRanges) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<StrictTaggingDecrypter>(/*key=*/0));
  framer_.SetKeyUpdateSupportForConnection(true);
  framer_.set_process_timestamps(true);
  framer_.set_max_receive_timestamps_per_ack(8);

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrame ack_frame = InitAckFrame(kSmallLargestObserved);
  ack_frame.received_packet_times = PacketTimeVector{
      {kSmallLargestObserved - 1201, CreationTimePlus(0x8bcaef234)},
      {kSmallLargestObserved - 1200, CreationTimePlus(0x8bcdef123)},
      {kSmallLargestObserved - 1000, CreationTimePlus(0xaacdef123)},
      {kSmallLargestObserved - 4, CreationTimePlus(0xabcdea125)},
      {kSmallLargestObserved - 2, CreationTimePlus(0xabcdee124)},
      {kSmallLargestObserved - 1, CreationTimePlus(0xabcdef123)},
      {kSmallLargestObserved, CreationTimePlus(0xabcdef123)},
  };
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();
  QuicFrames frames = {QuicFrame(&ack_frame)};

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      EncryptPacketWithTagAndPhase(*data, 0, false));
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_THAT(framer_.error(), IsQuicNoError());

  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_THAT(frame.received_packet_times,
              ContainerEq(PacketTimeVector{
                  {kSmallLargestObserved, CreationTimePlus(0xabcdef123)},
                  {kSmallLargestObserved - 1, CreationTimePlus(0xabcdef123)},
                  {kSmallLargestObserved - 2, CreationTimePlus(0xabcdee124)},
                  {kSmallLargestObserved - 4, CreationTimePlus(0xabcdea125)},
                  {kSmallLargestObserved - 1000, CreationTimePlus(0xaacdef123)},
                  {kSmallLargestObserved - 1200, CreationTimePlus(0x8bcdef123)},
                  {kSmallLargestObserved - 1201, CreationTimePlus(0x8bcaef234)},
              }));
}

TEST_P(QuicFramerTest,
       BuildAndProcessAckReceiveTimestampsExceedsMaxTimestamps) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<StrictTaggingDecrypter>(/*key=*/0));
  framer_.SetKeyUpdateSupportForConnection(true);
  framer_.set_process_timestamps(true);
  framer_.set_max_receive_timestamps_per_ack(2);

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrame ack_frame = InitAckFrame(kSmallLargestObserved);
  ack_frame.received_packet_times = PacketTimeVector{
      {kSmallLargestObserved - 1201, CreationTimePlus(0x8bcaef234)},
      {kSmallLargestObserved - 1200, CreationTimePlus(0x8bcdef123)},
      {kSmallLargestObserved - 1000, CreationTimePlus(0xaacdef123)},
      {kSmallLargestObserved - 5, CreationTimePlus(0xabcdea125)},
      {kSmallLargestObserved - 3, CreationTimePlus(0xabcded124)},
      {kSmallLargestObserved - 2, CreationTimePlus(0xabcdee124)},
      {kSmallLargestObserved - 1, CreationTimePlus(0xabcdef123)},
  };
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();
  QuicFrames frames = {QuicFrame(&ack_frame)};

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      EncryptPacketWithTagAndPhase(*data, 0, false));
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_THAT(framer_.error(), IsQuicNoError());

  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_THAT(frame.received_packet_times,
              ContainerEq(PacketTimeVector{
                  {kSmallLargestObserved - 1, CreationTimePlus(0xabcdef123)},
                  {kSmallLargestObserved - 2, CreationTimePlus(0xabcdee124)},
              }));
}

TEST_P(QuicFramerTest,
       BuildAndProcessAckReceiveTimestampsWithExponentNoTruncation) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<StrictTaggingDecrypter>(/*key=*/0));
  framer_.SetKeyUpdateSupportForConnection(true);
  framer_.set_process_timestamps(true);
  framer_.set_max_receive_timestamps_per_ack(8);
  framer_.set_receive_timestamps_exponent(3);

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrame ack_frame = InitAckFrame(kSmallLargestObserved);
  ack_frame.received_packet_times = PacketTimeVector{
      {kSmallLargestObserved - 8, CreationTimePlus(0x1add << 3)},
      {kSmallLargestObserved - 7, CreationTimePlus(0x29ed << 3)},
      {kSmallLargestObserved - 3, CreationTimePlus(0x29fe << 3)},
      {kSmallLargestObserved - 2, CreationTimePlus(0x29ff << 3)},
  };
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();
  QuicFrames frames = {QuicFrame(&ack_frame)};

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      EncryptPacketWithTagAndPhase(*data, 0, false));
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_THAT(framer_.error(), IsQuicNoError());

  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_THAT(frame.received_packet_times,
              ContainerEq(PacketTimeVector{
                  {kSmallLargestObserved - 2, CreationTimePlus(0x29ff << 3)},
                  {kSmallLargestObserved - 3, CreationTimePlus(0x29fe << 3)},
                  {kSmallLargestObserved - 7, CreationTimePlus(0x29ed << 3)},
                  {kSmallLargestObserved - 8, CreationTimePlus(0x1add << 3)},
              }));
}

TEST_P(QuicFramerTest,
       BuildAndProcessAckReceiveTimestampsWithExponentTruncation) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<StrictTaggingDecrypter>(/*key=*/0));
  framer_.SetKeyUpdateSupportForConnection(true);
  framer_.set_process_timestamps(true);
  framer_.set_max_receive_timestamps_per_ack(8);
  framer_.set_receive_timestamps_exponent(3);

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrame ack_frame = InitAckFrame(kSmallLargestObserved);
  ack_frame.received_packet_times = PacketTimeVector{
      {kSmallLargestObserved - 10, CreationTimePlus((0x1001 << 3) + 1)},
      {kSmallLargestObserved - 9, CreationTimePlus((0x2995 << 3) - 1)},
      {kSmallLargestObserved - 8, CreationTimePlus((0x2995 << 3) + 0)},
      {kSmallLargestObserved - 7, CreationTimePlus((0x2995 << 3) + 1)},
      {kSmallLargestObserved - 6, CreationTimePlus((0x2995 << 3) + 2)},
      {kSmallLargestObserved - 3, CreationTimePlus((0x2995 << 3) + 3)},
      {kSmallLargestObserved - 2, CreationTimePlus((0x2995 << 3) + 4)},
  };
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();
  QuicFrames frames = {QuicFrame(&ack_frame)};

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      EncryptPacketWithTagAndPhase(*data, 0, false));
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_THAT(framer_.error(), IsQuicNoError());

  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_THAT(frame.received_packet_times,
              ContainerEq(PacketTimeVector{
                  {kSmallLargestObserved - 2, CreationTimePlus(0x2996 << 3)},
                  {kSmallLargestObserved - 3, CreationTimePlus(0x2996 << 3)},
                  {kSmallLargestObserved - 6, CreationTimePlus(0x2996 << 3)},
                  {kSmallLargestObserved - 7, CreationTimePlus(0x2996 << 3)},
                  {kSmallLargestObserved - 8, CreationTimePlus(0x2995 << 3)},
                  {kSmallLargestObserved - 9, CreationTimePlus(0x2995 << 3)},
                  {kSmallLargestObserved - 10, CreationTimePlus(0x1002 << 3)},
              }));
}

TEST_P(QuicFramerTest, AckReceiveTimestamps) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<StrictTaggingDecrypter>(/*key=*/0));
  framer_.SetKeyUpdateSupportForConnection(true);
  framer_.set_process_timestamps(true);
  framer_.set_max_receive_timestamps_per_ack(8);
  framer_.set_receive_timestamps_exponent(3);

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  // Use kSmallLargestObserved to make this test finished in a short time.
  QuicAckFrame ack_frame = InitAckFrame(kSmallLargestObserved);
  ack_frame.received_packet_times = PacketTimeVector{
      {kSmallLargestObserved - 5, CreationTimePlus((0x29ff << 3))},
      {kSmallLargestObserved - 4, CreationTimePlus((0x29ff << 3))},
      {kSmallLargestObserved - 3, CreationTimePlus((0x29ff << 3))},
      {kSmallLargestObserved - 2, CreationTimePlus((0x29ff << 3))},
  };
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();
  QuicFrames frames = {QuicFrame(&ack_frame)};

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      EncryptPacketWithTagAndPhase(*data, 0, false));
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_THAT(framer_.error(), IsQuicNoError());

  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_THAT(frame.received_packet_times,
              ContainerEq(PacketTimeVector{
                  {kSmallLargestObserved - 2, CreationTimePlus(0x29ff << 3)},
                  {kSmallLargestObserved - 3, CreationTimePlus(0x29ff << 3)},
                  {kSmallLargestObserved - 4, CreationTimePlus(0x29ff << 3)},
                  {kSmallLargestObserved - 5, CreationTimePlus(0x29ff << 3)},
              }));
}

TEST_P(QuicFramerTest, AckReceiveTimestampsPacketOutOfOrder) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<StrictTaggingDecrypter>(/*key=*/0));
  framer_.SetKeyUpdateSupportForConnection(true);
  framer_.set_process_timestamps(true);
  framer_.set_max_receive_timestamps_per_ack(8);
  framer_.set_receive_timestamps_exponent(3);

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  // Use kSmallLargestObserved to make this test finished in a short time.
  QuicAckFrame ack_frame = InitAckFrame(kSmallLargestObserved);

  // The packet numbers below are out of order, this is impossible because we
  // don't record out of order packets in received_packet_times. The test is
  // intended to ensure this error is raised when it happens.
  ack_frame.received_packet_times = PacketTimeVector{
      {kSmallLargestObserved - 5, CreationTimePlus((0x29ff << 3))},
      {kSmallLargestObserved - 2, CreationTimePlus((0x29ff << 3))},
      {kSmallLargestObserved - 4, CreationTimePlus((0x29ff << 3))},
      {kSmallLargestObserved - 3, CreationTimePlus((0x29ff << 3))},
  };
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();
  QuicFrames frames = {QuicFrame(&ack_frame)};

  EXPECT_QUIC_BUG(BuildDataPacket(header, frames),
                  "Packet number and/or receive time not in order.");
}

// If there's insufficient room for IETF ack receive timestamps, don't write any
// timestamp ranges.
TEST_P(QuicFramerTest, IetfAckReceiveTimestampsTruncate) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<StrictTaggingDecrypter>(/*key=*/0));
  framer_.SetKeyUpdateSupportForConnection(true);
  framer_.set_process_timestamps(true);
  framer_.set_max_receive_timestamps_per_ack(8192);
  framer_.set_receive_timestamps_exponent(3);

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  // Use kSmallLargestObserved to make this test finished in a short time.
  QuicAckFrame ack_frame = InitAckFrame(kSmallLargestObserved);
  for (QuicPacketNumber i(1); i <= kSmallLargestObserved; i += 2) {
    ack_frame.received_packet_times.push_back(
        {i, CreationTimePlus((0x29ff << 3))});
  }

  ack_frame.ack_delay_time = QuicTime::Delta::Zero();
  QuicFrames frames = {QuicFrame(&ack_frame)};

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      EncryptPacketWithTagAndPhase(*data, 0, false));
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_THAT(framer_.error(), IsQuicNoError());

  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_TRUE(frame.received_packet_times.empty());
}

// If there are too many ack ranges, they will be truncated to make room for a
// timestamp range count of 0.
TEST_P(QuicFramerTest, IetfAckReceiveTimestampsAckRangeTruncation) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  framer_.set_process_timestamps(true);
  framer_.set_max_receive_timestamps_per_ack(8);
  framer_.set_receive_timestamps_exponent(3);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrame ack_frame;
  // Create a packet with just the ack.
  ack_frame = MakeAckFrameWithGaps(/*gap_size=*/0xffffffff,
                                   /*max_num_gaps=*/200,
                                   /*largest_acked=*/kMaxIetfVarInt);
  ack_frame.received_packet_times = PacketTimeVector{
      {QuicPacketNumber(kMaxIetfVarInt) - 2, CreationTimePlus((0x29ff << 3))},
  };
  QuicFrames frames = {QuicFrame(&ack_frame)};
  // Build an ACK packet.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> raw_ack_packet(BuildDataPacket(header, frames));
  ASSERT_TRUE(raw_ack_packet != nullptr);
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length =
      framer_.EncryptPayload(ENCRYPTION_INITIAL, header.packet_number,
                             *raw_ack_packet, buffer, kMaxOutgoingPacketSize);
  ASSERT_NE(0u, encrypted_length);
  // Now make sure we can turn our ack packet back into an ack frame.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  ASSERT_TRUE(framer_.ProcessPacket(
      QuicEncryptedPacket(buffer, encrypted_length, false)));
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  QuicAckFrame& processed_ack_frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(QuicPacketNumber(kMaxIetfVarInt),
            LargestAcked(processed_ack_frame));
  // Verify ACK ranges in the frame gets truncated.
  ASSERT_LT(processed_ack_frame.packets.NumPacketsSlow(),
            ack_frame.packets.NumIntervals());
  EXPECT_EQ(158u, processed_ack_frame.packets.NumPacketsSlow());
  EXPECT_LT(processed_ack_frame.packets.NumIntervals(),
            ack_frame.packets.NumIntervals());
  EXPECT_EQ(QuicPacketNumber(kMaxIetfVarInt),
            processed_ack_frame.packets.Max());
  // But the receive timestamps are not truncated because they are small.
  EXPECT_FALSE(processed_ack_frame.received_packet_times.empty());
}

TEST_P(QuicFramerTest, BuildAckFramePacketOneAckBlockMaxLength) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrame ack_frame = InitAckFrame(kPacketNumber);
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();

  QuicFrames frames = {QuicFrame(&ack_frame)};

  // clang-format off
  unsigned char packet[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 4 byte largest observed, 4 byte block length)
      0x4A,
      // largest acked
      0x12, 0x34, 0x56, 0x78,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x12, 0x34, 0x56, 0x78,
      // num timestamps.
      0x00,
  };


  unsigned char packet_ietf[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (IETF_ACK frame)
      0x02,
      // largest acked
      kVarInt62FourBytes + 0x12, 0x34, 0x56, 0x78,
      // Zero delta time.
      kVarInt62OneByte + 0x00,
      // Nr. of additional ack blocks
      kVarInt62OneByte + 0x00,
      // first ack block length.
      kVarInt62FourBytes + 0x12, 0x34, 0x56, 0x77,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
    p_size = ABSL_ARRAYSIZE(packet_ietf);
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildAckFramePacketMultipleAckBlocks) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  // Use kSmallLargestObserved to make this test finished in a short time.
  QuicAckFrame ack_frame =
      InitAckFrame({{QuicPacketNumber(1), QuicPacketNumber(5)},
                    {QuicPacketNumber(10), QuicPacketNumber(500)},
                    {QuicPacketNumber(900), kSmallMissingPacket},
                    {kSmallMissingPacket + 1, kSmallLargestObserved + 1}});
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();

  QuicFrames frames = {QuicFrame(&ack_frame)};

  // clang-format off
  unsigned char packet[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (has ack blocks, 2 byte largest observed, 2 byte block length)
      0x65,
      // largest acked
      0x12, 0x34,
      // Zero delta time.
      0x00, 0x00,
      // num ack blocks ranges.
      0x04,
      // first ack block length.
      0x00, 0x01,
      // gap to next block.
      0x01,
      // ack block length.
      0x0e, 0xaf,
      // gap to next block.
      0xff,
      // ack block length.
      0x00, 0x00,
      // gap to next block.
      0x91,
      // ack block length.
      0x01, 0xea,
      // gap to next block.
      0x05,
      // ack block length.
      0x00, 0x04,
      // num timestamps.
      0x00,
  };

  unsigned char packet_ietf[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (IETF_ACK frame)
      0x02,
      // largest acked
      kVarInt62TwoBytes + 0x12, 0x34,
      // Zero delta time.
      kVarInt62OneByte + 0x00,
      // num additional ack blocks.
      kVarInt62OneByte + 0x03,
      // first ack block length.
      kVarInt62OneByte + 0x00,

      // gap to next block.
      kVarInt62OneByte + 0x00,
      // ack block length.
      kVarInt62TwoBytes + 0x0e, 0xae,

      // gap to next block.
      kVarInt62TwoBytes + 0x01, 0x8f,
      // ack block length.
      kVarInt62TwoBytes + 0x01, 0xe9,

      // gap to next block.
      kVarInt62OneByte + 0x04,
      // ack block length.
      kVarInt62OneByte + 0x03,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
    p_size = ABSL_ARRAYSIZE(packet_ietf);
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildAckFramePacketMaxAckBlocks) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  // Use kSmallLargestObservedto make this test finished in a short time.
  QuicAckFrame ack_frame;
  ack_frame.largest_acked = kSmallLargestObserved;
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();
  // 300 ack blocks.
  for (size_t i = 2; i < 2 * 300; i += 2) {
    ack_frame.packets.Add(QuicPacketNumber(i));
  }
  ack_frame.packets.AddRange(QuicPacketNumber(600), kSmallLargestObserved + 1);

  QuicFrames frames = {QuicFrame(&ack_frame)};

  // clang-format off
  unsigned char packet[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (ack frame)
      // (has ack blocks, 2 byte largest observed, 2 byte block length)
      0x65,
      // largest acked
      0x12, 0x34,
      // Zero delta time.
      0x00, 0x00,
      // num ack blocks ranges.
      0xff,
      // first ack block length.
      0x0f, 0xdd,
      // 255 = 4 * 63 + 3
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      // num timestamps.
      0x00,
  };

  unsigned char packet_ietf[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (IETF_ACK frame)
      0x02,
      // largest acked
      kVarInt62TwoBytes + 0x12, 0x34,
      // Zero delta time.
      kVarInt62OneByte + 0x00,
      // num ack blocks ranges.
      kVarInt62TwoBytes + 0x01, 0x2b,
      // first ack block length.
      kVarInt62TwoBytes + 0x0f, 0xdc,
      // 255 added blocks of gap_size == 1, ack_size == 1
#define V99AddedBLOCK kVarInt62OneByte + 0x00, kVarInt62OneByte + 0x00
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,

      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,

      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,

#undef V99AddedBLOCK
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
    p_size = ABSL_ARRAYSIZE(packet_ietf);
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildRstFramePacketQuic) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicRstStreamFrame rst_frame;
  rst_frame.stream_id = kStreamId;
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    rst_frame.ietf_error_code = 0x01;
  } else {
    rst_frame.error_code = static_cast<QuicRstStreamErrorCode>(0x05060708);
  }
  rst_frame.byte_offset = 0x0807060504030201;

  // clang-format off
  unsigned char packet[] = {
    // type (short packet, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (rst stream frame)
    0x01,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // sent byte offset
    0x08, 0x07, 0x06, 0x05,
    0x04, 0x03, 0x02, 0x01,
    // error code
    0x05, 0x06, 0x07, 0x08,
  };

  unsigned char packet_ietf[] = {
    // type (short packet, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_RST_STREAM frame)
    0x04,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // error code
    kVarInt62OneByte + 0x01,
    // sent byte offset
    kVarInt62EightBytes + 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01
  };
  // clang-format on

  QuicFrames frames = {QuicFrame(&rst_frame)};

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
    p_size = ABSL_ARRAYSIZE(packet_ietf);
  }
  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildCloseFramePacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicConnectionCloseFrame close_frame(framer_.transport_version(),
                                       QUIC_INTERNAL_ERROR, NO_IETF_QUIC_ERROR,
                                       "because I can", 0x05);
  QuicFrames frames = {QuicFrame(&close_frame)};

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (connection close frame)
    0x02,
    // error code
    0x00, 0x00, 0x00, 0x01,
    // error details length
    0x00, 0x0d,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_CONNECTION_CLOSE frame)
    0x1c,
    // error code
    kVarInt62OneByte + 0x01,
    // Frame type within the CONNECTION_CLOSE frame
    kVarInt62OneByte + 0x05,
    // error details length
    kVarInt62OneByte + 0x0f,
    // error details
    '1',  ':',  'b',  'e',
    'c',  'a',  'u',  's',
    'e',  ' ',  'I',  ' ',
    'c',  'a',  'n',
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
    p_size = ABSL_ARRAYSIZE(packet_ietf);
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildCloseFramePacketExtendedInfo) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicConnectionCloseFrame close_frame(
      framer_.transport_version(),
      static_cast<QuicErrorCode>(
          VersionHasIetfQuicFrames(framer_.transport_version()) ? 0x01
                                                                : 0x05060708),
      NO_IETF_QUIC_ERROR, "because I can", 0x05);
  // Set this so that it is "there" for both Google QUIC and IETF QUIC
  // framing. It better not show up for Google QUIC!
  close_frame.quic_error_code = static_cast<QuicErrorCode>(0x4567);

  QuicFrames frames = {QuicFrame(&close_frame)};

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (connection close frame)
    0x02,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // error details length
    0x00, 0x0d,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_CONNECTION_CLOSE frame)
    0x1c,
    // IETF error code INTERNAL_ERROR = 0x01 corresponding to
    // QuicErrorCode::QUIC_INTERNAL_ERROR = 0x01.
    kVarInt62OneByte + 0x01,
    // Frame type within the CONNECTION_CLOSE frame
    kVarInt62OneByte + 0x05,
    // error details length
    kVarInt62OneByte + 0x13,
    // error details
    '1',  '7',  '7',  '6',
    '7',  ':',  'b',  'e',
    'c',  'a',  'u',  's',
    'e',  ' ',  'I',  ' ',
    'c',  'a',  'n'
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
    p_size = ABSL_ARRAYSIZE(packet_ietf);
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildTruncatedCloseFramePacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicConnectionCloseFrame close_frame(framer_.transport_version(),
                                       QUIC_INTERNAL_ERROR, NO_IETF_QUIC_ERROR,
                                       std::string(2048, 'A'), 0x05);
  QuicFrames frames = {QuicFrame(&close_frame)};

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (connection close frame)
    0x02,
    // error code
    0x00, 0x00, 0x00, 0x01,
    // error details length
    0x01, 0x00,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };

  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_CONNECTION_CLOSE frame)
    0x1c,
    // error code
    kVarInt62OneByte + 0x01,
    // Frame type within the CONNECTION_CLOSE frame
    kVarInt62OneByte + 0x05,
    // error details length
    kVarInt62TwoBytes + 0x01, 0x00,
    // error details (truncated to 256 bytes)
    '1',  ':',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
    p_size = ABSL_ARRAYSIZE(packet_ietf);
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildApplicationCloseFramePacket) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is only for IETF QUIC.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicConnectionCloseFrame app_close_frame;
  app_close_frame.wire_error_code = 0x11;
  app_close_frame.error_details = "because I can";
  app_close_frame.close_type = IETF_QUIC_APPLICATION_CONNECTION_CLOSE;

  QuicFrames frames = {QuicFrame(&app_close_frame)};

  // clang-format off

  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_APPLICATION_CLOSE frame)
    0x1d,
    // error code
    kVarInt62OneByte + 0x11,
    // error details length
    kVarInt62OneByte + 0x0f,
    // error details, note that it includes an extended error code.
    '0',  ':',  'b',  'e',
    'c',  'a',  'u',  's',
    'e',  ' ',  'I',  ' ',
    'c',  'a',  'n',
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet_ietf),
      ABSL_ARRAYSIZE(packet_ietf));
}

TEST_P(QuicFramerTest, BuildTruncatedApplicationCloseFramePacket) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is only for IETF QUIC.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicConnectionCloseFrame app_close_frame;
  app_close_frame.wire_error_code = 0x11;
  app_close_frame.error_details = std::string(2048, 'A');
  app_close_frame.close_type = IETF_QUIC_APPLICATION_CONNECTION_CLOSE;
  // Setting to missing ensures that if it is missing, the extended
  // code is not added to the text message.
  app_close_frame.quic_error_code = QUIC_IETF_GQUIC_ERROR_MISSING;

  QuicFrames frames = {QuicFrame(&app_close_frame)};

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_APPLICATION_CLOSE frame)
    0x1d,
    // error code
    kVarInt62OneByte + 0x11,
    // error details length
    kVarInt62TwoBytes + 0x01, 0x00,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet_ietf),
      ABSL_ARRAYSIZE(packet_ietf));
}

TEST_P(QuicFramerTest, BuildGoAwayPacket) {
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is only for Google QUIC.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicGoAwayFrame goaway_frame;
  goaway_frame.error_code = static_cast<QuicErrorCode>(0x05060708);
  goaway_frame.last_good_stream_id = kStreamId;
  goaway_frame.reason_phrase = "because I can";

  QuicFrames frames = {QuicFrame(&goaway_frame)};

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (go away frame)
    0x03,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // error details length
    0x00, 0x0d,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  // clang-format on
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet),
      ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildTruncatedGoAwayPacket) {
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is only for Google QUIC.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicGoAwayFrame goaway_frame;
  goaway_frame.error_code = static_cast<QuicErrorCode>(0x05060708);
  goaway_frame.last_good_stream_id = kStreamId;
  goaway_frame.reason_phrase = std::string(2048, 'A');

  QuicFrames frames = {QuicFrame(&goaway_frame)};

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (go away frame)
    0x03,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // error details length
    0x01, 0x00,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet),
      ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildWindowUpdatePacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicWindowUpdateFrame window_update_frame;
  window_update_frame.stream_id = kStreamId;
  window_update_frame.max_data = 0x1122334455667788;

  QuicFrames frames = {QuicFrame(window_update_frame)};

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (window update frame)
    0x04,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // byte offset
    0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
  };

  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_MAX_STREAM_DATA frame)
    0x11,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // byte offset
    kVarInt62EightBytes + 0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
    p_size = ABSL_ARRAYSIZE(packet_ietf);
  }

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildMaxStreamDataPacket) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is only for IETF QUIC.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicWindowUpdateFrame window_update_frame;
  window_update_frame.stream_id = kStreamId;
  window_update_frame.max_data = 0x1122334455667788;

  QuicFrames frames = {QuicFrame(window_update_frame)};

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_MAX_STREAM_DATA frame)
    0x11,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // byte offset
    kVarInt62EightBytes + 0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet_ietf),
      ABSL_ARRAYSIZE(packet_ietf));
}

TEST_P(QuicFramerTest, BuildMaxDataPacket) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is only for IETF QUIC.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicWindowUpdateFrame window_update_frame;
  window_update_frame.stream_id =
      QuicUtils::GetInvalidStreamId(framer_.transport_version());
  window_update_frame.max_data = 0x1122334455667788;

  QuicFrames frames = {QuicFrame(window_update_frame)};

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_MAX_DATA frame)
    0x10,
    // byte offset
    kVarInt62EightBytes + 0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet_ietf),
      ABSL_ARRAYSIZE(packet_ietf));
}

TEST_P(QuicFramerTest, BuildBlockedPacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicBlockedFrame blocked_frame;
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    // For IETF QUIC, the stream ID must be <invalid> for the frame
    // to be a BLOCKED frame. if it's valid, it will be a
    // STREAM_BLOCKED frame.
    blocked_frame.stream_id =
        QuicUtils::GetInvalidStreamId(framer_.transport_version());
  } else {
    blocked_frame.stream_id = kStreamId;
  }
  blocked_frame.offset = kStreamOffset;

  QuicFrames frames = {QuicFrame(blocked_frame)};

  // clang-format off
  unsigned char packet[] = {
    // type (short packet, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (blocked frame)
    0x05,
    // stream id
    0x01, 0x02, 0x03, 0x04,
  };

  unsigned char packet_ietf[] = {
    // type (short packet, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_DATA_BLOCKED frame)
    0x14,
    // Offset
    kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
    p_size = ABSL_ARRAYSIZE(packet_ietf);
  }

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildPingPacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPingFrame())};

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
  };

  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_PING frame)
    0x01,
  };
  // clang-format on

  unsigned char* p = packet;
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildHandshakeDonePacket) {
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicHandshakeDoneFrame())};

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (Handshake done frame)
    0x1e,
  };
  // clang-format on
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet),
      ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildAckFrequencyPacket) {
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrequencyFrame ack_frequency_frame;
  ack_frequency_frame.sequence_number = 3;
  ack_frequency_frame.packet_tolerance = 5;
  ack_frequency_frame.max_ack_delay = QuicTime::Delta::FromMicroseconds(0x3fff);
  ack_frequency_frame.ignore_order = false;
  QuicFrames frames = {QuicFrame(&ack_frequency_frame)};

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (Ack Frequency frame)
    0x40, 0xaf,
    // sequence number
    0x03,
    // packet tolerance
    0x05,
    // max_ack_delay_us
    0x7f, 0xff,
    // ignore_oder
    0x00
  };
  // clang-format on
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet),
      ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildImmediateAckPacket) {
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicImmediateAckFrame immediate_ack_frame;
  QuicFrames frames = {QuicFrame(immediate_ack_frame)};

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (Immediate Ack frame)
    0x1f,
  };
  // clang-format on
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet),
      ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildResetStreamAtPacket) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicResetStreamAtFrame frame;
  frame.stream_id = 0x00;
  frame.error = 0x1e;
  frame.final_offset = 0x20;
  frame.reliable_offset = 0x10;
  QuicFrames frames = {QuicFrame(&frame)};

  framer_.set_process_reset_stream_at(true);
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  // clang-format off
  unsigned char packet[] = {
      // type (short header, 4 byte packet number)
      0x43,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // type = RESET_STREAM_AT
      0x24,
      // stream ID
      0x00,
      // application error code
      0x1e,
      // final size
      0x20,
      // reliable size
      0x10,
  };
  // clang-format on

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet),
      ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildMessagePacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicMessageFrame frame(1, MemSliceFromString("message"));
  QuicMessageFrame frame2(2, MemSliceFromString("message2"));
  QuicFrames frames = {QuicFrame(&frame), QuicFrame(&frame2)};

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (message frame)
    0x21,
    // Length
    0x07,
    // Message Data
    'm', 'e', 's', 's', 'a', 'g', 'e',
    // frame type (message frame no length)
    0x20,
    // Message Data
    'm', 'e', 's', 's', 'a', 'g', 'e', '2'
  };

  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_MESSAGE frame)
    0x31,
    // Length
    0x07,
    // Message Data
    'm', 'e', 's', 's', 'a', 'g', 'e',
    // frame type (message frame no length)
    0x30,
    // Message Data
    'm', 'e', 's', 's', 'a', 'g', 'e', '2'
  };
  // clang-format on

  unsigned char* p = packet;
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      ABSL_ARRAYSIZE(packet));
}

// Test that the MTU discovery packet is serialized correctly as a PING packet.
TEST_P(QuicFramerTest, BuildMtuDiscoveryPacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicMtuDiscoveryFrame())};

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
  };

  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_PING frame)
    0x01,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
  }

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildPublicResetPacket) {
  QuicPublicResetPacket reset_packet;
  reset_packet.connection_id = FramerTestConnectionId();
  reset_packet.nonce_proof = kNonceProof;

  // clang-format off
  unsigned char packet[] = {
    // public flags (public reset, 8 byte ConnectionId)
    0x0E,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // message tag (kPRST)
    'P', 'R', 'S', 'T',
    // num_entries (1) + padding
    0x01, 0x00, 0x00, 0x00,
    // tag kRNON
    'R', 'N', 'O', 'N',
    // end offset 8
    0x08, 0x00, 0x00, 0x00,
    // nonce proof
    0x89, 0x67, 0x45, 0x23,
    0x01, 0xEF, 0xCD, 0xAB,
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> data(
      framer_.BuildPublicResetPacket(reset_packet));
  ASSERT_TRUE(data != nullptr);
  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet),
      ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildPublicResetPacketWithClientAddress) {
  QuicPublicResetPacket reset_packet;
  reset_packet.connection_id = FramerTestConnectionId();
  reset_packet.nonce_proof = kNonceProof;
  reset_packet.client_address =
      QuicSocketAddress(QuicIpAddress::Loopback4(), 0x1234);

  // clang-format off
  unsigned char packet[] = {
      // public flags (public reset, 8 byte ConnectionId)
      0x0E,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98,
      0x76, 0x54, 0x32, 0x10,
      // message tag (kPRST)
      'P', 'R', 'S', 'T',
      // num_entries (2) + padding
      0x02, 0x00, 0x00, 0x00,
      // tag kRNON
      'R', 'N', 'O', 'N',
      // end offset 8
      0x08, 0x00, 0x00, 0x00,
      // tag kCADR
      'C', 'A', 'D', 'R',
      // end offset 16
      0x10, 0x00, 0x00, 0x00,
      // nonce proof
      0x89, 0x67, 0x45, 0x23,
      0x01, 0xEF, 0xCD, 0xAB,
      // client address
      0x02, 0x00,
      0x7F, 0x00, 0x00, 0x01,
      0x34, 0x12,
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> data(
      framer_.BuildPublicResetPacket(reset_packet));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet),
      ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildPublicResetPacketWithEndpointId) {
  QuicPublicResetPacket reset_packet;
  reset_packet.connection_id = FramerTestConnectionId();
  reset_packet.nonce_proof = kNonceProof;
  reset_packet.endpoint_id = "FakeServerId";

  // The tag value map in CryptoHandshakeMessage is a std::map, so the two tags
  // in the packet, kRNON and kEPID, have unspecified ordering w.r.t each other.
  // clang-format off
  unsigned char packet_variant1[] = {
      // public flags (public reset, 8 byte ConnectionId)
      0x0E,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98,
      0x76, 0x54, 0x32, 0x10,
      // message tag (kPRST)
      'P', 'R', 'S', 'T',
      // num_entries (2) + padding
      0x02, 0x00, 0x00, 0x00,
      // tag kRNON
      'R', 'N', 'O', 'N',
      // end offset 8
      0x08, 0x00, 0x00, 0x00,
      // tag kEPID
      'E', 'P', 'I', 'D',
      // end offset 20
      0x14, 0x00, 0x00, 0x00,
      // nonce proof
      0x89, 0x67, 0x45, 0x23,
      0x01, 0xEF, 0xCD, 0xAB,
      // Endpoint ID
      'F', 'a', 'k', 'e', 'S', 'e', 'r', 'v', 'e', 'r', 'I', 'd',
  };
  unsigned char packet_variant2[] = {
      // public flags (public reset, 8 byte ConnectionId)
      0x0E,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98,
      0x76, 0x54, 0x32, 0x10,
      // message tag (kPRST)
      'P', 'R', 'S', 'T',
      // num_entries (2) + padding
      0x02, 0x00, 0x00, 0x00,
      // tag kEPID
      'E', 'P', 'I', 'D',
      // end offset 12
      0x0C, 0x00, 0x00, 0x00,
      // tag kRNON
      'R', 'N', 'O', 'N',
      // end offset 20
      0x14, 0x00, 0x00, 0x00,
      // Endpoint ID
      'F', 'a', 'k', 'e', 'S', 'e', 'r', 'v', 'e', 'r', 'I', 'd',
      // nonce proof
      0x89, 0x67, 0x45, 0x23,
      0x01, 0xEF, 0xCD, 0xAB,
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> data(
      framer_.BuildPublicResetPacket(reset_packet));
  ASSERT_TRUE(data != nullptr);

  // Variant 1 ends with char 'd'. Variant 1 ends with char 0xAB.
  if ('d' == data->data()[data->length() - 1]) {
    quiche::test::CompareCharArraysWithHexError(
        "constructed packet", data->data(), data->length(),
        AsChars(packet_variant1), ABSL_ARRAYSIZE(packet_variant1));
  } else {
    quiche::test::CompareCharArraysWithHexError(
        "constructed packet", data->data(), data->length(),
        AsChars(packet_variant2), ABSL_ARRAYSIZE(packet_variant2));
  }
}

TEST_P(QuicFramerTest, BuildIetfStatelessResetPacket) {
  // clang-format off
    unsigned char packet[] = {
      // 1st byte 01XX XXXX
      0x40,
      // At least 4 bytes of random bytes.
      0x00, 0x00, 0x00, 0x00,
      // stateless reset token
      0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
      0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f
    };
  // clang-format on

  // Build the minimal stateless reset packet.
  std::unique_ptr<QuicEncryptedPacket> data(
      framer_.BuildIetfStatelessResetPacket(
          FramerTestConnectionId(),
          QuicFramer::GetMinStatelessResetPacketLength() + 1,
          kTestStatelessResetToken));
  ASSERT_TRUE(data);
  EXPECT_EQ(QuicFramer::GetMinStatelessResetPacketLength(), data->length());
  // Verify the first 2 bits are 01.
  EXPECT_FALSE(data->data()[0] & FLAGS_LONG_HEADER);
  EXPECT_TRUE(data->data()[0] & FLAGS_FIXED_BIT);
  // Verify stateless reset token.
  quiche::test::CompareCharArraysWithHexError(
      "constructed packet",
      data->data() + data->length() - kStatelessResetTokenLength,
      kStatelessResetTokenLength,
      AsChars(packet) + ABSL_ARRAYSIZE(packet) - kStatelessResetTokenLength,
      kStatelessResetTokenLength);

  // Packets with length <= minimal stateless reset does not trigger stateless
  // reset.
  std::unique_ptr<QuicEncryptedPacket> data2(
      framer_.BuildIetfStatelessResetPacket(
          FramerTestConnectionId(),
          QuicFramer::GetMinStatelessResetPacketLength(),
          kTestStatelessResetToken));
  ASSERT_FALSE(data2);

  // Do not send stateless reset >= minimal stateless reset + 1 + max
  // connection ID length.
  std::unique_ptr<QuicEncryptedPacket> data3(
      framer_.BuildIetfStatelessResetPacket(FramerTestConnectionId(), 1000,
                                            kTestStatelessResetToken));
  ASSERT_TRUE(data3);
  EXPECT_EQ(QuicFramer::GetMinStatelessResetPacketLength() + 1 +
                kQuicMaxConnectionIdWithLengthPrefixLength,
            data3->length());
}

TEST_P(QuicFramerTest, BuildIetfStatelessResetPacketCallerProvidedRandomBytes) {
  // clang-format off
    unsigned char packet[] = {
      // 1st byte 01XX XXXX
      0x7c,
      // Random bytes
      0x7c, 0x7c, 0x7c, 0x7c,
      // stateless reset token
      0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
      0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f
    };
  // clang-format on

  // Build the minimal stateless reset packet with caller-provided random bytes.
  MockRandom random;
  auto generate_random_bytes = [](void* data, size_t len) {
    std::string bytes(len, 0x7c);
    memcpy(data, bytes.data(), bytes.size());
  };
  EXPECT_CALL(random, InsecureRandBytes(_, _))
      .WillOnce(testing::Invoke(generate_random_bytes));
  std::unique_ptr<QuicEncryptedPacket> data(
      framer_.BuildIetfStatelessResetPacket(
          FramerTestConnectionId(),
          QuicFramer::GetMinStatelessResetPacketLength() + 1,
          kTestStatelessResetToken, &random));
  ASSERT_TRUE(data);
  EXPECT_EQ(QuicFramer::GetMinStatelessResetPacketLength(), data->length());
  // Verify the first 2 bits are 01.
  EXPECT_FALSE(data->data()[0] & FLAGS_LONG_HEADER);
  EXPECT_TRUE(data->data()[0] & FLAGS_FIXED_BIT);
  // Verify the entire packet.
  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet),
      ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, EncryptPacket) {
  QuicPacketNumber packet_number = kPacketNumber;
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  unsigned char packet50[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
    'q',  'r',  's',  't',
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (framer_.version().HasHeaderProtection()) {
    p = packet50;
    p_size = ABSL_ARRAYSIZE(packet50);
  }

  std::unique_ptr<QuicPacket> raw(new QuicPacket(
      AsChars(p), p_size, false, kPacket8ByteConnectionId,
      kPacket0ByteConnectionId, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_4BYTE_PACKET_NUMBER, quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0, 0,
      quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0));
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length = framer_.EncryptPayload(
      ENCRYPTION_INITIAL, packet_number, *raw, buffer, kMaxOutgoingPacketSize);

  ASSERT_NE(0u, encrypted_length);
  EXPECT_TRUE(CheckEncryption(packet_number, raw.get()));
}

// Regression test for b/158014497.
TEST_P(QuicFramerTest, EncryptEmptyPacket) {
  auto packet = std::make_unique<QuicPacket>(
      new char[100], 0, true, kPacket8ByteConnectionId,
      kPacket0ByteConnectionId,
      /*includes_version=*/true,
      /*includes_diversification_nonce=*/true, PACKET_1BYTE_PACKET_NUMBER,
      quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0,
      /*retry_token_length=*/0, quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0);
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length = 1;
  EXPECT_QUIC_BUG(
      {
        encrypted_length =
            framer_.EncryptPayload(ENCRYPTION_INITIAL, kPacketNumber, *packet,
                                   buffer, kMaxOutgoingPacketSize);
        EXPECT_EQ(0u, encrypted_length);
      },
      "packet is shorter than associated data length");
}

TEST_P(QuicFramerTest, EncryptPacketWithVersionFlag) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketNumber packet_number = kPacketNumber;
  // clang-format off
  unsigned char packet[] = {
    // type (long header with packet type ZERO_RTT_PROTECTED)
    0xD3,
    // version tag
    'Q', '.', '1', '0',
    // connection_id length
    0x50,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  unsigned char packet50[] = {
    // type (long header with packet type ZERO_RTT_PROTECTED)
    0xD3,
    // version tag
    'Q', '.', '1', '0',
    // destination connection ID length
    0x08,
    // destination connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // source connection ID length
    0x00,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
    'q',  'r',  's',  't',
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  // TODO(ianswett): see todo in previous test.
  if (framer_.version().HasHeaderProtection()) {
    p = packet50;
    p_size = ABSL_ARRAYSIZE(packet50);
  }

  std::unique_ptr<QuicPacket> raw(new QuicPacket(
      AsChars(p), p_size, false, kPacket8ByteConnectionId,
      kPacket0ByteConnectionId, kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_4BYTE_PACKET_NUMBER, quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0, 0,
      quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0));
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length = framer_.EncryptPayload(
      ENCRYPTION_INITIAL, packet_number, *raw, buffer, kMaxOutgoingPacketSize);

  ASSERT_NE(0u, encrypted_length);
  EXPECT_TRUE(CheckEncryption(packet_number, raw.get()));
}

TEST_P(QuicFramerTest, AckTruncationLargePacket) {
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This test is not applicable to this version; the range count is
    // effectively unlimited
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrame ack_frame;
  // Create a packet with just the ack.
  ack_frame = MakeAckFrameWithAckBlocks(300, 0u);
  QuicFrames frames = {QuicFrame(&ack_frame)};

  // Build an ack packet with truncation due to limit in number of nack ranges.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> raw_ack_packet(BuildDataPacket(header, frames));
  ASSERT_TRUE(raw_ack_packet != nullptr);
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length =
      framer_.EncryptPayload(ENCRYPTION_INITIAL, header.packet_number,
                             *raw_ack_packet, buffer, kMaxOutgoingPacketSize);
  ASSERT_NE(0u, encrypted_length);
  // Now make sure we can turn our ack packet back into an ack frame.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  ASSERT_TRUE(framer_.ProcessPacket(
      QuicEncryptedPacket(buffer, encrypted_length, false)));
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  QuicAckFrame& processed_ack_frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(QuicPacketNumber(600u), LargestAcked(processed_ack_frame));
  ASSERT_EQ(256u, processed_ack_frame.packets.NumPacketsSlow());
  EXPECT_EQ(QuicPacketNumber(90u), processed_ack_frame.packets.Min());
  EXPECT_EQ(QuicPacketNumber(600u), processed_ack_frame.packets.Max());
}

// Regression test for b/150386368.
TEST_P(QuicFramerTest, IetfAckFrameTruncation) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrame ack_frame;
  // Create a packet with just the ack.
  ack_frame = MakeAckFrameWithGaps(/*gap_size=*/0xffffffff,
                                   /*max_num_gaps=*/200,
                                   /*largest_acked=*/kMaxIetfVarInt);
  ack_frame.ecn_counters = QuicEcnCounts(100, 10000, 1000000);
  QuicFrames frames = {QuicFrame(&ack_frame)};
  // Build an ACK packet.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> raw_ack_packet(BuildDataPacket(header, frames));
  ASSERT_TRUE(raw_ack_packet != nullptr);
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length =
      framer_.EncryptPayload(ENCRYPTION_INITIAL, header.packet_number,
                             *raw_ack_packet, buffer, kMaxOutgoingPacketSize);
  ASSERT_NE(0u, encrypted_length);
  // Now make sure we can turn our ack packet back into an ack frame.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  ASSERT_TRUE(framer_.ProcessPacket(
      QuicEncryptedPacket(buffer, encrypted_length, false)));
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  QuicAckFrame& processed_ack_frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(QuicPacketNumber(kMaxIetfVarInt),
            LargestAcked(processed_ack_frame));
  // Verify ACK frame gets truncated.
  ASSERT_LT(processed_ack_frame.packets.NumPacketsSlow(),
            ack_frame.packets.NumIntervals());
  EXPECT_EQ(157u, processed_ack_frame.packets.NumPacketsSlow());
  EXPECT_LT(processed_ack_frame.packets.NumIntervals(),
            ack_frame.packets.NumIntervals());
  EXPECT_EQ(QuicPacketNumber(kMaxIetfVarInt),
            processed_ack_frame.packets.Max());
}

TEST_P(QuicFramerTest, AckTruncationSmallPacket) {
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This test is not applicable to this version; the range count is
    // effectively unlimited
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  // Create a packet with just the ack.
  QuicAckFrame ack_frame;
  ack_frame = MakeAckFrameWithAckBlocks(300, 0u);
  QuicFrames frames = {QuicFrame(&ack_frame)};

  // Build an ack packet with truncation due to limit in number of nack ranges.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> raw_ack_packet(
      BuildDataPacket(header, frames, 500));
  ASSERT_TRUE(raw_ack_packet != nullptr);
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length =
      framer_.EncryptPayload(ENCRYPTION_INITIAL, header.packet_number,
                             *raw_ack_packet, buffer, kMaxOutgoingPacketSize);
  ASSERT_NE(0u, encrypted_length);
  // Now make sure we can turn our ack packet back into an ack frame.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  ASSERT_TRUE(framer_.ProcessPacket(
      QuicEncryptedPacket(buffer, encrypted_length, false)));
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  QuicAckFrame& processed_ack_frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(QuicPacketNumber(600u), LargestAcked(processed_ack_frame));
  ASSERT_EQ(240u, processed_ack_frame.packets.NumPacketsSlow());
  EXPECT_EQ(QuicPacketNumber(122u), processed_ack_frame.packets.Min());
  EXPECT_EQ(QuicPacketNumber(600u), processed_ack_frame.packets.Max());
}

TEST_P(QuicFramerTest, CleanTruncation) {
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This test is not applicable to this version; the range count is
    // effectively unlimited
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrame ack_frame = InitAckFrame(201);

  // Create a packet with just the ack.
  QuicFrames frames = {QuicFrame(&ack_frame)};
  if (framer_.version().HasHeaderProtection()) {
    frames.push_back(QuicFrame(QuicPaddingFrame(12)));
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> raw_ack_packet(BuildDataPacket(header, frames));
  ASSERT_TRUE(raw_ack_packet != nullptr);

  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length =
      framer_.EncryptPayload(ENCRYPTION_INITIAL, header.packet_number,
                             *raw_ack_packet, buffer, kMaxOutgoingPacketSize);
  ASSERT_NE(0u, encrypted_length);

  // Now make sure we can turn our ack packet back into an ack frame.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  ASSERT_TRUE(framer_.ProcessPacket(
      QuicEncryptedPacket(buffer, encrypted_length, false)));

  // Test for clean truncation of the ack by comparing the length of the
  // original packets to the re-serialized packets.
  frames.clear();
  frames.push_back(QuicFrame(visitor_.ack_frames_[0].get()));
  if (framer_.version().HasHeaderProtection()) {
    frames.push_back(QuicFrame(*visitor_.padding_frames_[0].get()));
  }

  size_t original_raw_length = raw_ack_packet->length();
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  raw_ack_packet = BuildDataPacket(header, frames);
  ASSERT_TRUE(raw_ack_packet != nullptr);
  EXPECT_EQ(original_raw_length, raw_ack_packet->length());
  ASSERT_TRUE(raw_ack_packet != nullptr);
}

TEST_P(QuicFramerTest, StopPacketProcessing) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',

    // frame type (ack frame)
    0x40,
    // least packet number awaiting an ack
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xA0,
    // largest observed packet number
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xBF,
    // num missing packets
    0x01,
    // missing packet
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xBE,
  };

  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_STREAM frame with fin, length, and offset bits set)
    0x08 | 0x01 | 0x02 | 0x04,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // offset
    kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    kVarInt62TwoBytes + 0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',

    // frame type (ack frame)
    0x0d,
    // largest observed packet number
    kVarInt62FourBytes + 0x12, 0x34, 0x56, 0x78,
    // Delta time
    kVarInt62OneByte + 0x00,
    // Ack Block count
    kVarInt62OneByte + 0x01,
    // First block size (one packet)
    kVarInt62OneByte + 0x00,

    // Next gap size & ack. Missing all preceding packets
    kVarInt62FourBytes + 0x12, 0x34, 0x56, 0x77,
    kVarInt62OneByte + 0x00,
  };
  // clang-format on

  MockFramerVisitor visitor;
  framer_.set_visitor(&visitor);
  EXPECT_CALL(visitor, OnPacket());
  EXPECT_CALL(visitor, OnPacketHeader(_));
  EXPECT_CALL(visitor, OnStreamFrame(_)).WillOnce(Return(false));
  EXPECT_CALL(visitor, OnPacketComplete());
  EXPECT_CALL(visitor, OnUnauthenticatedPublicHeader(_)).WillOnce(Return(true));
  EXPECT_CALL(visitor, OnUnauthenticatedHeader(_)).WillOnce(Return(true));
  EXPECT_CALL(visitor, OnDecryptedPacket(_, _));

  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
    p_size = ABSL_ARRAYSIZE(packet_ietf);
  }
  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_THAT(framer_.error(), IsQuicNoError());
}

static char kTestString[] = "At least 20 characters.";
static QuicStreamId kTestQuicStreamId = 1;

MATCHER_P(ExpectedStreamFrame, version, "") {
  return (arg.stream_id == kTestQuicStreamId ||
          QuicUtils::IsCryptoStreamId(version.transport_version,
                                      arg.stream_id)) &&
         !arg.fin && arg.offset == 0 &&
         std::string(arg.data_buffer, arg.data_length) == kTestString;
  // FIN is hard-coded false in ConstructEncryptedPacket.
  // Offset 0 is hard-coded in ConstructEncryptedPacket.
}

// Verify that the packet returned by ConstructEncryptedPacket() can be properly
// parsed by the framer.
TEST_P(QuicFramerTest, ConstructEncryptedPacket) {
  // Since we are using ConstructEncryptedPacket, we have to set the framer's
  // crypto to be Null.
  if (framer_.version().KnowsWhichDecrypterToUse()) {
    framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                             std::make_unique<StrictTaggingDecrypter>(
                                 (uint8_t)ENCRYPTION_FORWARD_SECURE));
  } else {
    framer_.SetDecrypter(ENCRYPTION_FORWARD_SECURE,
                         std::make_unique<StrictTaggingDecrypter>(
                             (uint8_t)ENCRYPTION_FORWARD_SECURE));
  }
  ParsedQuicVersionVector versions;
  versions.push_back(framer_.version());
  std::unique_ptr<QuicEncryptedPacket> packet(ConstructEncryptedPacket(
      TestConnectionId(), EmptyQuicConnectionId(), false, false,
      kTestQuicStreamId, kTestString, CONNECTION_ID_PRESENT,
      CONNECTION_ID_ABSENT, PACKET_4BYTE_PACKET_NUMBER, &versions));

  MockFramerVisitor visitor;
  framer_.set_visitor(&visitor);
  EXPECT_CALL(visitor, OnPacket()).Times(1);
  EXPECT_CALL(visitor, OnUnauthenticatedPublicHeader(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(visitor, OnUnauthenticatedHeader(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(visitor, OnPacketHeader(_)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(visitor, OnDecryptedPacket(_, _)).Times(1);
  EXPECT_CALL(visitor, OnError(_)).Times(0);
  EXPECT_CALL(visitor, OnStreamFrame(_)).Times(0);
  if (!QuicVersionUsesCryptoFrames(framer_.version().transport_version)) {
    EXPECT_CALL(visitor, OnStreamFrame(ExpectedStreamFrame(framer_.version())))
        .Times(1);
  } else {
    EXPECT_CALL(visitor, OnCryptoFrame(_)).Times(1);
  }
  EXPECT_CALL(visitor, OnPacketComplete()).Times(1);

  EXPECT_TRUE(framer_.ProcessPacket(*packet));
  EXPECT_THAT(framer_.error(), IsQuicNoError());
}

// Verify that the packet returned by ConstructMisFramedEncryptedPacket()
// does cause the framer to return an error.
TEST_P(QuicFramerTest, ConstructMisFramedEncryptedPacket) {
  // Since we are using ConstructEncryptedPacket, we have to set the framer's
  // crypto to be Null.
  if (framer_.version().KnowsWhichDecrypterToUse()) {
    framer_.InstallDecrypter(
        ENCRYPTION_FORWARD_SECURE,
        std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_FORWARD_SECURE));
  }
  std::unique_ptr<QuicEncryptedPacket> packet(ConstructMisFramedEncryptedPacket(
      TestConnectionId(), EmptyQuicConnectionId(), false, false,
      kTestQuicStreamId, kTestString, CONNECTION_ID_PRESENT,
      CONNECTION_ID_ABSENT, PACKET_4BYTE_PACKET_NUMBER, framer_.version(),
      Perspective::IS_CLIENT));

  MockFramerVisitor visitor;
  framer_.set_visitor(&visitor);
  EXPECT_CALL(visitor, OnPacket()).Times(1);
  EXPECT_CALL(visitor, OnUnauthenticatedPublicHeader(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(visitor, OnUnauthenticatedHeader(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(visitor, OnPacketHeader(_)).Times(1);
  EXPECT_CALL(visitor, OnDecryptedPacket(_, _)).Times(1);
  EXPECT_CALL(visitor, OnError(_)).Times(1);
  EXPECT_CALL(visitor, OnStreamFrame(_)).Times(0);
  EXPECT_CALL(visitor, OnPacketComplete()).Times(0);

  EXPECT_FALSE(framer_.ProcessPacket(*packet));
  EXPECT_THAT(framer_.error(), IsError(QUIC_INVALID_FRAME_DATA));
}

TEST_P(QuicFramerTest, IetfBlockedFrame) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_DATA_BLOCKED)
      {"",
       {0x14}},
      // blocked offset
      {"Can not read blocked offset.",
       {kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(kStreamOffset, visitor_.blocked_frame_.offset);

  CheckFramingBoundaries(packet_ietf, QUIC_INVALID_BLOCKED_DATA);
}

TEST_P(QuicFramerTest, BuildIetfBlockedPacket) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicBlockedFrame frame;
  frame.stream_id = QuicUtils::GetInvalidStreamId(framer_.transport_version());
  frame.offset = kStreamOffset;
  QuicFrames frames = {QuicFrame(frame)};

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_DATA_BLOCKED)
    0x14,
    // Offset
    kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet_ietf),
      ABSL_ARRAYSIZE(packet_ietf));
}

TEST_P(QuicFramerTest, IetfStreamBlockedFrame) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_STREAM_DATA_BLOCKED)
      {"",
       {0x15}},
      // blocked offset
      {"Unable to read IETF_STREAM_DATA_BLOCKED frame stream id/count.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      {"Can not read stream blocked offset.",
       {kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(kStreamId, visitor_.blocked_frame_.stream_id);
  EXPECT_EQ(kStreamOffset, visitor_.blocked_frame_.offset);

  CheckFramingBoundaries(packet_ietf, QUIC_INVALID_STREAM_BLOCKED_DATA);
}

TEST_P(QuicFramerTest, BuildIetfStreamBlockedPacket) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicBlockedFrame frame;
  frame.stream_id = kStreamId;
  frame.offset = kStreamOffset;
  QuicFrames frames = {QuicFrame(frame)};

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_STREAM_DATA_BLOCKED)
    0x15,
    // Stream ID
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // Offset
    kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet_ietf),
      ABSL_ARRAYSIZE(packet_ietf));
}

TEST_P(QuicFramerTest, BiDiMaxStreamsFrame) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_MAX_STREAMS_BIDIRECTIONAL)
      {"",
       {0x12}},
      // max. streams
      {"Unable to read IETF_MAX_STREAMS_BIDIRECTIONAL frame stream id/count.",
       {kVarInt62OneByte + 0x03}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(3u, visitor_.max_streams_frame_.stream_count);
  EXPECT_FALSE(visitor_.max_streams_frame_.unidirectional);
  CheckFramingBoundaries(packet_ietf, QUIC_MAX_STREAMS_DATA);
}

TEST_P(QuicFramerTest, UniDiMaxStreamsFrame) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // Test runs in client mode, no connection id
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_MAX_STREAMS_UNIDIRECTIONAL)
      {"",
       {0x13}},
      // max. streams
      {"Unable to read IETF_MAX_STREAMS_UNIDIRECTIONAL frame stream id/count.",
       {kVarInt62OneByte + 0x03}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket0ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(3u, visitor_.max_streams_frame_.stream_count);
  EXPECT_TRUE(visitor_.max_streams_frame_.unidirectional);
  CheckFramingBoundaries(packet_ietf, QUIC_MAX_STREAMS_DATA);
}

TEST_P(QuicFramerTest, ServerUniDiMaxStreamsFrame) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_MAX_STREAMS_UNIDIRECTIONAL)
      {"",
       {0x13}},
      // max. streams
      {"Unable to read IETF_MAX_STREAMS_UNIDIRECTIONAL frame stream id/count.",
       {kVarInt62OneByte + 0x03}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(3u, visitor_.max_streams_frame_.stream_count);
  EXPECT_TRUE(visitor_.max_streams_frame_.unidirectional);
  CheckFramingBoundaries(packet_ietf, QUIC_MAX_STREAMS_DATA);
}

TEST_P(QuicFramerTest, ClientUniDiMaxStreamsFrame) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // Test runs in client mode, no connection id
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_MAX_STREAMS_UNIDIRECTIONAL)
      {"",
       {0x13}},
      // max. streams
      {"Unable to read IETF_MAX_STREAMS_UNIDIRECTIONAL frame stream id/count.",
       {kVarInt62OneByte + 0x03}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket0ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(3u, visitor_.max_streams_frame_.stream_count);
  EXPECT_TRUE(visitor_.max_streams_frame_.unidirectional);
  CheckFramingBoundaries(packet_ietf, QUIC_MAX_STREAMS_DATA);
}

// The following four tests ensure that the framer can deserialize a stream
// count that is large enough to cause the resulting stream ID to exceed the
// current implementation limit(32 bits). The intent is that when this happens,
// the stream limit is pegged to the maximum supported value. There are four
// tests, for the four combinations of uni- and bi-directional, server- and
// client- initiated.
TEST_P(QuicFramerTest, BiDiMaxStreamsFrameTooBig) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x9A, 0xBC,
    // frame type (IETF_MAX_STREAMS_BIDIRECTIONAL)
    0x12,

    // max. streams. Max stream ID allowed is 0xffffffff
    // This encodes a count of 0x40000000, leading to stream
    // IDs in the range 0x1 00000000 to 0x1 00000003.
    kVarInt62EightBytes + 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet_ietf),
                                ABSL_ARRAYSIZE(packet_ietf), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0x40000000u, visitor_.max_streams_frame_.stream_count);
  EXPECT_FALSE(visitor_.max_streams_frame_.unidirectional);
}

TEST_P(QuicFramerTest, ClientBiDiMaxStreamsFrameTooBig) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // Test runs in client mode, no connection id
    // packet number
    0x12, 0x34, 0x9A, 0xBC,
    // frame type (IETF_MAX_STREAMS_BIDIRECTIONAL)
    0x12,

    // max. streams. Max stream ID allowed is 0xffffffff
    // This encodes a count of 0x40000000, leading to stream
    // IDs in the range 0x1 00000000 to 0x1 00000003.
    kVarInt62EightBytes + 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet_ietf),
                                ABSL_ARRAYSIZE(packet_ietf), false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket0ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0x40000000u, visitor_.max_streams_frame_.stream_count);
  EXPECT_FALSE(visitor_.max_streams_frame_.unidirectional);
}

TEST_P(QuicFramerTest, ServerUniDiMaxStreamsFrameTooBig) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x9A, 0xBC,
    // frame type (IETF_MAX_STREAMS_UNIDIRECTIONAL)
    0x13,

    // max. streams. Max stream ID allowed is 0xffffffff
    // This encodes a count of 0x40000000, leading to stream
    // IDs in the range 0x1 00000000 to 0x1 00000003.
    kVarInt62EightBytes + 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet_ietf),
                                ABSL_ARRAYSIZE(packet_ietf), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0x40000000u, visitor_.max_streams_frame_.stream_count);
  EXPECT_TRUE(visitor_.max_streams_frame_.unidirectional);
}

TEST_P(QuicFramerTest, ClientUniDiMaxStreamsFrameTooBig) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // Test runs in client mode, no connection id
    // packet number
    0x12, 0x34, 0x9A, 0xBC,
    // frame type (IETF_MAX_STREAMS_UNDIRECTIONAL)
    0x13,

    // max. streams. Max stream ID allowed is 0xffffffff
    // This encodes a count of 0x40000000, leading to stream
    // IDs in the range 0x1 00000000 to 0x1 00000003.
    kVarInt62EightBytes + 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet_ietf),
                                ABSL_ARRAYSIZE(packet_ietf), false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket0ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0x40000000u, visitor_.max_streams_frame_.stream_count);
  EXPECT_TRUE(visitor_.max_streams_frame_.unidirectional);
}

// Specifically test that count==0 is accepted.
TEST_P(QuicFramerTest, MaxStreamsFrameZeroCount) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x9A, 0xBC,
    // frame type (IETF_MAX_STREAMS_BIDIRECTIONAL)
    0x12,
    // max. streams == 0.
    kVarInt62OneByte + 0x00
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet_ietf),
                                ABSL_ARRAYSIZE(packet_ietf), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
}

TEST_P(QuicFramerTest, ServerBiDiStreamsBlockedFrame) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_MAX_STREAMS_UNIDIRECTIONAL frame)
      {"",
       {0x13}},
      // stream count
      {"Unable to read IETF_MAX_STREAMS_UNIDIRECTIONAL frame stream id/count.",
       {kVarInt62OneByte + 0x00}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0u, visitor_.max_streams_frame_.stream_count);
  EXPECT_TRUE(visitor_.max_streams_frame_.unidirectional);

  CheckFramingBoundaries(packet_ietf, QUIC_MAX_STREAMS_DATA);
}

TEST_P(QuicFramerTest, BiDiStreamsBlockedFrame) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_STREAMS_BLOCKED_BIDIRECTIONAL frame)
      {"",
       {0x16}},
      // stream id
      {"Unable to read IETF_STREAMS_BLOCKED_BIDIRECTIONAL "
       "frame stream id/count.",
       {kVarInt62OneByte + 0x03}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(3u, visitor_.streams_blocked_frame_.stream_count);
  EXPECT_FALSE(visitor_.streams_blocked_frame_.unidirectional);

  CheckFramingBoundaries(packet_ietf, QUIC_STREAMS_BLOCKED_DATA);
}

TEST_P(QuicFramerTest, UniDiStreamsBlockedFrame) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_STREAMS_BLOCKED_UNIDIRECTIONAL frame)
      {"",
       {0x17}},
      // stream id
      {"Unable to read IETF_STREAMS_BLOCKED_UNIDIRECTIONAL "
       "frame stream id/count.",
       {kVarInt62OneByte + 0x03}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(3u, visitor_.streams_blocked_frame_.stream_count);
  EXPECT_TRUE(visitor_.streams_blocked_frame_.unidirectional);
  CheckFramingBoundaries(packet_ietf, QUIC_STREAMS_BLOCKED_DATA);
}

TEST_P(QuicFramerTest, ClientUniDiStreamsBlockedFrame) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // Test runs in client mode, no connection id
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_STREAMS_BLOCKED_UNIDIRECTIONAL frame)
      {"",
       {0x17}},
      // stream id
      {"Unable to read IETF_STREAMS_BLOCKED_UNIDIRECTIONAL "
       "frame stream id/count.",
       {kVarInt62OneByte + 0x03}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket0ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(3u, visitor_.streams_blocked_frame_.stream_count);
  EXPECT_TRUE(visitor_.streams_blocked_frame_.unidirectional);
  CheckFramingBoundaries(packet_ietf, QUIC_STREAMS_BLOCKED_DATA);
}

// Check that when we get a STREAMS_BLOCKED frame that specifies too large
// a stream count, we reject with an appropriate error. There is no need to
// check for different combinations of Uni/Bi directional and client/server
// initiated; the logic does not take these into account.
TEST_P(QuicFramerTest, StreamsBlockedFrameTooBig) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // Test runs in client mode, no connection id
    // packet number
    0x12, 0x34, 0x9A, 0xBC,
    // frame type (IETF_STREAMS_BLOCKED_BIDIRECTIONAL)
    0x16,

    // max. streams. Max stream ID allowed is 0xffffffff
    // This encodes a count of 0x40000000, leading to stream
    // IDs in the range 0x1 00000000 to 0x1 00000003.
    kVarInt62EightBytes + 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x01
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet_ietf),
                                ABSL_ARRAYSIZE(packet_ietf), false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsError(QUIC_STREAMS_BLOCKED_DATA));
  EXPECT_EQ(framer_.detailed_error(),
            "STREAMS_BLOCKED stream count exceeds implementation limit.");
}

// Specifically test that count==0 is accepted.
TEST_P(QuicFramerTest, StreamsBlockedFrameZeroCount) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }

  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_STREAMS_BLOCKED_UNIDIRECTIONAL frame)
      {"",
       {0x17}},
      // stream id
      {"Unable to read IETF_STREAMS_BLOCKED_UNIDIRECTIONAL "
       "frame stream id/count.",
       {kVarInt62OneByte + 0x00}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0u, visitor_.streams_blocked_frame_.stream_count);
  EXPECT_TRUE(visitor_.streams_blocked_frame_.unidirectional);

  CheckFramingBoundaries(packet_ietf, QUIC_STREAMS_BLOCKED_DATA);
}

TEST_P(QuicFramerTest, BuildBiDiStreamsBlockedPacket) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicStreamsBlockedFrame frame;
  frame.stream_count = 3;
  frame.unidirectional = false;

  QuicFrames frames = {QuicFrame(frame)};

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_STREAMS_BLOCKED_BIDIRECTIONAL frame)
    0x16,
    // Stream count
    kVarInt62OneByte + 0x03
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet_ietf),
      ABSL_ARRAYSIZE(packet_ietf));
}

TEST_P(QuicFramerTest, BuildUniStreamsBlockedPacket) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicStreamsBlockedFrame frame;
  frame.stream_count = 3;
  frame.unidirectional = true;

  QuicFrames frames = {QuicFrame(frame)};

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_STREAMS_BLOCKED_UNIDIRECTIONAL frame)
    0x17,
    // Stream count
    kVarInt62OneByte + 0x03
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet_ietf),
      ABSL_ARRAYSIZE(packet_ietf));
}

TEST_P(QuicFramerTest, BuildBiDiMaxStreamsPacket) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicMaxStreamsFrame frame;
  frame.stream_count = 3;
  frame.unidirectional = false;

  QuicFrames frames = {QuicFrame(frame)};

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_MAX_STREAMS_BIDIRECTIONAL frame)
    0x12,
    // Stream count
    kVarInt62OneByte + 0x03
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet_ietf),
      ABSL_ARRAYSIZE(packet_ietf));
}

TEST_P(QuicFramerTest, BuildUniDiMaxStreamsPacket) {
  // This frame is only for IETF QUIC.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }

  // This test runs in client mode.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicMaxStreamsFrame frame;
  frame.stream_count = 3;
  frame.unidirectional = true;

  QuicFrames frames = {QuicFrame(frame)};

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_MAX_STREAMS_UNIDIRECTIONAL frame)
    0x13,
    // Stream count
    kVarInt62OneByte + 0x03
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet_ietf),
      ABSL_ARRAYSIZE(packet_ietf));
}

TEST_P(QuicFramerTest, NewConnectionIdFrame) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is only for IETF QUIC.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_NEW_CONNECTION_ID frame)
      {"",
       {0x18}},
      // error code
      {"Unable to read new connection ID frame sequence number.",
       {kVarInt62OneByte + 0x11}},
      {"Unable to read new connection ID frame retire_prior_to.",
       {kVarInt62OneByte + 0x09}},
      {"Unable to read new connection ID frame connection id.",
       {0x08}},  // connection ID length
      {"Unable to read new connection ID frame connection id.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11}},
      {"Can not read new connection ID frame reset token.",
       {0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
        0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(FramerTestConnectionIdPlusOne(),
            visitor_.new_connection_id_.connection_id);
  EXPECT_EQ(0x11u, visitor_.new_connection_id_.sequence_number);
  EXPECT_EQ(0x09u, visitor_.new_connection_id_.retire_prior_to);
  EXPECT_EQ(kTestStatelessResetToken,
            visitor_.new_connection_id_.stateless_reset_token);

  ASSERT_EQ(0u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(packet_ietf, QUIC_INVALID_NEW_CONNECTION_ID_DATA);
}

TEST_P(QuicFramerTest, NewConnectionIdFrameVariableLength) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is only for IETF QUIC.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_NEW_CONNECTION_ID frame)
      {"",
       {0x18}},
      // error code
      {"Unable to read new connection ID frame sequence number.",
       {kVarInt62OneByte + 0x11}},
      {"Unable to read new connection ID frame retire_prior_to.",
       {kVarInt62OneByte + 0x0a}},
      {"Unable to read new connection ID frame connection id.",
       {0x09}},  // connection ID length
      {"Unable to read new connection ID frame connection id.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0x42}},
      {"Can not read new connection ID frame reset token.",
       {0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
        0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(FramerTestConnectionIdNineBytes(),
            visitor_.new_connection_id_.connection_id);
  EXPECT_EQ(0x11u, visitor_.new_connection_id_.sequence_number);
  EXPECT_EQ(0x0au, visitor_.new_connection_id_.retire_prior_to);
  EXPECT_EQ(kTestStatelessResetToken,
            visitor_.new_connection_id_.stateless_reset_token);

  ASSERT_EQ(0u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(packet_ietf, QUIC_INVALID_NEW_CONNECTION_ID_DATA);
}

// Verifies that parsing a NEW_CONNECTION_ID frame with a length above the
// specified maximum fails.
TEST_P(QuicFramerTest, InvalidLongNewConnectionIdFrame) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // The NEW_CONNECTION_ID frame is only for IETF QUIC.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_NEW_CONNECTION_ID frame)
      {"",
       {0x18}},
      // error code
      {"Unable to read new connection ID frame sequence number.",
       {kVarInt62OneByte + 0x11}},
      {"Unable to read new connection ID frame retire_prior_to.",
       {kVarInt62OneByte + 0x0b}},
      {"Unable to read new connection ID frame connection id.",
       {0x40}},  // connection ID length
      {"Unable to read new connection ID frame connection id.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
        0xF0, 0xD2, 0xB4, 0x96, 0x78, 0x5A, 0x3C, 0x1E,
        0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
        0xF0, 0xD2, 0xB4, 0x96, 0x78, 0x5A, 0x3C, 0x1E,
        0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
        0xF0, 0xD2, 0xB4, 0x96, 0x78, 0x5A, 0x3C, 0x1E,
        0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
        0xF0, 0xD2, 0xB4, 0x96, 0x78, 0x5A, 0x3C, 0x1E}},
      {"Can not read new connection ID frame reset token.",
       {0xb5, 0x69, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_THAT(framer_.error(), IsError(QUIC_INVALID_NEW_CONNECTION_ID_DATA));
  EXPECT_EQ("Invalid new connection ID length for version.",
            framer_.detailed_error());
}

// Verifies that parsing a NEW_CONNECTION_ID frame with an invalid
// retire-prior-to fails.
TEST_P(QuicFramerTest, InvalidRetirePriorToNewConnectionIdFrame) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is only for IETF QUIC only.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_NEW_CONNECTION_ID frame)
      {"",
       {0x18}},
      // sequence number
      {"Unable to read new connection ID frame sequence number.",
       {kVarInt62OneByte + 0x11}},
      {"Unable to read new connection ID frame retire_prior_to.",
       {kVarInt62OneByte + 0x1b}},
      {"Unable to read new connection ID frame connection id length.",
       {0x08}},  // connection ID length
      {"Unable to read new connection ID frame connection id.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11}},
      {"Can not read new connection ID frame reset token.",
       {0xb5, 0x69, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_THAT(framer_.error(), IsError(QUIC_INVALID_NEW_CONNECTION_ID_DATA));
  EXPECT_EQ("Retire_prior_to > sequence_number.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, BuildNewConnectionIdFramePacket) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is only for IETF QUIC only.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicNewConnectionIdFrame frame;
  frame.sequence_number = 0x11;
  frame.retire_prior_to = 0x0c;
  // Use this value to force a 4-byte encoded variable length connection ID
  // in the frame.
  frame.connection_id = FramerTestConnectionIdPlusOne();
  frame.stateless_reset_token = kTestStatelessResetToken;

  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_NEW_CONNECTION_ID frame)
    0x18,
    // sequence number
    kVarInt62OneByte + 0x11,
    // retire_prior_to
    kVarInt62OneByte + 0x0c,
    // new connection id length
    0x08,
    // new connection id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
    // stateless reset token
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet_ietf),
      ABSL_ARRAYSIZE(packet_ietf));
}

TEST_P(QuicFramerTest, NewTokenFrame) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is only for IETF QUIC only.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_NEW_TOKEN frame)
      {"",
       {0x07}},
      // Length
      {"Unable to read new token length.",
       {kVarInt62OneByte + 0x08}},
      {"Unable to read new token data.",
       {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}}
  };
  // clang-format on
  uint8_t expected_token_value[] = {0x00, 0x01, 0x02, 0x03,
                                    0x04, 0x05, 0x06, 0x07};

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(sizeof(expected_token_value), visitor_.new_token_.token.length());
  EXPECT_EQ(0, memcmp(expected_token_value, visitor_.new_token_.token.data(),
                      sizeof(expected_token_value)));

  CheckFramingBoundaries(packet, QUIC_INVALID_NEW_TOKEN);
}

TEST_P(QuicFramerTest, BuildNewTokenFramePacket) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is only for IETF QUIC only.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  uint8_t expected_token_value[] = {0x00, 0x01, 0x02, 0x03,
                                    0x04, 0x05, 0x06, 0x07};

  QuicNewTokenFrame frame(0,
                          absl::string_view((const char*)(expected_token_value),
                                            sizeof(expected_token_value)));

  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_NEW_TOKEN frame)
    0x07,
    // Length and token
    kVarInt62OneByte + 0x08,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet),
      ABSL_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, IetfStopSendingFrame) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Stop sending frame is IETF QUIC only.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_STOP_SENDING frame)
      {"",
       {0x05}},
      // stream id
      {"Unable to read IETF_STOP_SENDING frame stream id/count.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      {"Unable to read stop sending application error code.",
       {kVarInt62FourBytes + 0x00, 0x00, 0x76, 0x54}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(kStreamId, visitor_.stop_sending_frame_.stream_id);
  EXPECT_EQ(QUIC_STREAM_UNKNOWN_APPLICATION_ERROR_CODE,
            visitor_.stop_sending_frame_.error_code);
  EXPECT_EQ(static_cast<uint64_t>(0x7654),
            visitor_.stop_sending_frame_.ietf_error_code);

  CheckFramingBoundaries(packet_ietf, QUIC_INVALID_STOP_SENDING_FRAME_DATA);
}

TEST_P(QuicFramerTest, BuildIetfStopSendingPacket) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Stop sending frame is IETF QUIC only.
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicStopSendingFrame frame;
  frame.stream_id = kStreamId;
  frame.error_code = QUIC_STREAM_ENCODER_STREAM_ERROR;
  frame.ietf_error_code =
      static_cast<uint64_t>(QuicHttpQpackErrorCode::ENCODER_STREAM_ERROR);
  QuicFrames frames = {QuicFrame(frame)};

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_STOP_SENDING frame)
    0x05,
    // Stream ID
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // Application error code
    kVarInt62TwoBytes + 0x02, 0x01,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet_ietf),
      ABSL_ARRAYSIZE(packet_ietf));
}

TEST_P(QuicFramerTest, IetfPathChallengeFrame) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Path Challenge frame is IETF QUIC only.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_PATH_CHALLENGE)
      {"",
       {0x1a}},
      // data
      {"Can not read path challenge data.",
       {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(QuicPathFrameBuffer({{0, 1, 2, 3, 4, 5, 6, 7}}),
            visitor_.path_challenge_frame_.data_buffer);

  CheckFramingBoundaries(packet_ietf, QUIC_INVALID_PATH_CHALLENGE_DATA);
}

TEST_P(QuicFramerTest, BuildIetfPathChallengePacket) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Path Challenge frame is IETF QUIC only.
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicPathChallengeFrame frame;
  frame.data_buffer = QuicPathFrameBuffer({{0, 1, 2, 3, 4, 5, 6, 7}});
  QuicFrames frames = {QuicFrame(frame)};

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_PATH_CHALLENGE)
    0x1a,
    // Data
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet_ietf),
      ABSL_ARRAYSIZE(packet_ietf));
}

TEST_P(QuicFramerTest, IetfPathResponseFrame) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Path response frame is IETF QUIC only.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (IETF_PATH_RESPONSE)
      {"",
       {0x1b}},
      // data
      {"Can not read path response data.",
       {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(QuicPathFrameBuffer({{0, 1, 2, 3, 4, 5, 6, 7}}),
            visitor_.path_response_frame_.data_buffer);

  CheckFramingBoundaries(packet_ietf, QUIC_INVALID_PATH_RESPONSE_DATA);
}

TEST_P(QuicFramerTest, BuildIetfPathResponsePacket) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Path response frame is IETF QUIC only
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicPathResponseFrame frame;
  frame.data_buffer = QuicPathFrameBuffer({{0, 1, 2, 3, 4, 5, 6, 7}});
  QuicFrames frames = {QuicFrame(frame)};

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_PATH_RESPONSE)
    0x1b,
    // Data
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet_ietf),
      ABSL_ARRAYSIZE(packet_ietf));
}

TEST_P(QuicFramerTest, GetRetransmittableControlFrameSize) {
  QuicRstStreamFrame rst_stream(1, 3, QUIC_STREAM_CANCELLED, 1024);
  EXPECT_EQ(QuicFramer::GetRstStreamFrameSize(framer_.transport_version(),
                                              rst_stream),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&rst_stream)));

  std::string error_detail(2048, 'e');
  QuicConnectionCloseFrame connection_close(framer_.transport_version(),
                                            QUIC_NETWORK_IDLE_TIMEOUT,
                                            NO_IETF_QUIC_ERROR, error_detail,
                                            /*transport_close_frame_type=*/0);

  EXPECT_EQ(QuicFramer::GetConnectionCloseFrameSize(framer_.transport_version(),
                                                    connection_close),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&connection_close)));

  QuicGoAwayFrame goaway(2, QUIC_PEER_GOING_AWAY, 3, error_detail);
  EXPECT_EQ(QuicFramer::GetMinGoAwayFrameSize() + 256,
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&goaway)));

  QuicWindowUpdateFrame window_update(3, 3, 1024);
  EXPECT_EQ(QuicFramer::GetWindowUpdateFrameSize(framer_.transport_version(),
                                                 window_update),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(window_update)));

  QuicBlockedFrame blocked(4, 3, 1024);
  EXPECT_EQ(
      QuicFramer::GetBlockedFrameSize(framer_.transport_version(), blocked),
      QuicFramer::GetRetransmittableControlFrameSize(
          framer_.transport_version(), QuicFrame(blocked)));

  // Following frames are IETF QUIC frames only.
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }

  QuicNewConnectionIdFrame new_connection_id(5, TestConnectionId(), 1,
                                             kTestStatelessResetToken, 1);
  EXPECT_EQ(QuicFramer::GetNewConnectionIdFrameSize(new_connection_id),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&new_connection_id)));

  QuicMaxStreamsFrame max_streams(6, 3, /*unidirectional=*/false);
  EXPECT_EQ(QuicFramer::GetMaxStreamsFrameSize(framer_.transport_version(),
                                               max_streams),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(max_streams)));

  QuicStreamsBlockedFrame streams_blocked(7, 3, /*unidirectional=*/false);
  EXPECT_EQ(QuicFramer::GetStreamsBlockedFrameSize(framer_.transport_version(),
                                                   streams_blocked),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(streams_blocked)));

  QuicPathFrameBuffer buffer = {
      {0x80, 0x91, 0xa2, 0xb3, 0xc4, 0xd5, 0xe5, 0xf7}};
  QuicPathResponseFrame path_response_frame(8, buffer);
  EXPECT_EQ(QuicFramer::GetPathResponseFrameSize(path_response_frame),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(path_response_frame)));

  QuicPathChallengeFrame path_challenge_frame(9, buffer);
  EXPECT_EQ(QuicFramer::GetPathChallengeFrameSize(path_challenge_frame),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(path_challenge_frame)));

  QuicStopSendingFrame stop_sending_frame(10, 3, QUIC_STREAM_CANCELLED);
  EXPECT_EQ(QuicFramer::GetStopSendingFrameSize(stop_sending_frame),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(stop_sending_frame)));
}

// A set of tests to ensure that bad frame-type encodings
// are properly detected and handled.
// First, four tests to see that unknown frame types generate
// a QUIC_INVALID_FRAME_DATA error with detailed information
// "Illegal frame type." This regardless of the encoding of the type
// (1/2/4/8 bytes).
// This only for version 99.
TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorUnknown1Byte) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Only IETF QUIC encodes frame types such that this test is relevant.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (unknown value, single-byte encoding)
      {"",
       {0x38}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsError(QUIC_INVALID_FRAME_DATA));
  EXPECT_EQ("Illegal frame type.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorUnknown2Bytes) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Only IETF QUIC encodes frame types such that this test is relevant.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (unknown value, two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x01, 0x38}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsError(QUIC_INVALID_FRAME_DATA));
  EXPECT_EQ("Illegal frame type.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorUnknown4Bytes) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Only IETF QUIC encodes frame types such that this test is relevant.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (unknown value, four-byte encoding)
      {"",
       {kVarInt62FourBytes + 0x01, 0x00, 0x00, 0x38}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsError(QUIC_INVALID_FRAME_DATA));
  EXPECT_EQ("Illegal frame type.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorUnknown8Bytes) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Only IETF QUIC encodes frame types such that this test is relevant.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (unknown value, eight-byte encoding)
      {"",
       {kVarInt62EightBytes + 0x01, 0x00, 0x00, 0x01, 0x02, 0x34, 0x56, 0x38}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsError(QUIC_INVALID_FRAME_DATA));
  EXPECT_EQ("Illegal frame type.", framer_.detailed_error());
}

// Three tests to check that known frame types that are not minimally
// encoded generate IETF_QUIC_PROTOCOL_VIOLATION errors with detailed
// information "Frame type not minimally encoded."
// Look at the frame-type encoded in 2, 4, and 8 bytes.
TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorKnown2Bytes) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Only IETF QUIC encodes frame types such that this test is relevant.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (Blocked, two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x08}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsError(IETF_QUIC_PROTOCOL_VIOLATION));
  EXPECT_EQ("Frame type not minimally encoded.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorKnown4Bytes) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Only IETF QUIC encodes frame types such that this test is relevant.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (Blocked, four-byte encoding)
      {"",
       {kVarInt62FourBytes + 0x00, 0x00, 0x00, 0x08}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsError(IETF_QUIC_PROTOCOL_VIOLATION));
  EXPECT_EQ("Frame type not minimally encoded.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorKnown8Bytes) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Only IETF QUIC encodes frame types such that this test is relevant.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (Blocked, eight-byte encoding)
      {"",
       {kVarInt62EightBytes + 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsError(IETF_QUIC_PROTOCOL_VIOLATION));
  EXPECT_EQ("Frame type not minimally encoded.", framer_.detailed_error());
}

// Tests to check that all known IETF frame types that are not minimally
// encoded generate IETF_QUIC_PROTOCOL_VIOLATION errors with detailed
// information "Frame type not minimally encoded."
// Just look at 2-byte encoding.
TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorKnown2BytesAllTypes) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Only IETF QUIC encodes frame types such that this test is relevant.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  // clang-format off
  PacketFragments packets[] = {
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x00}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x01}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x02}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x03}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x04}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x05}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x06}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x07}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x08}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x09}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x0a}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x0b}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x0c}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x0d}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x0e}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x0f}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x10}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x11}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x12}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x13}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x14}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x15}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x16}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x17}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x18}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x20}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x21}}
    },
  };
  // clang-format on

  for (PacketFragments& packet : packets) {
    std::unique_ptr<QuicEncryptedPacket> encrypted(
        AssemblePacketFromFragments(packet));

    EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

    EXPECT_THAT(framer_.error(), IsError(IETF_QUIC_PROTOCOL_VIOLATION));
    EXPECT_EQ("Frame type not minimally encoded.", framer_.detailed_error());
  }
}

TEST_P(QuicFramerTest, RetireConnectionIdFrame) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is only for version 99.
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  PacketFragments packet_ietf = {
      // type (short header, 4 byte packet number)
      {"",
       {0x43}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF_RETIRE_CONNECTION_ID frame)
      {"",
       {0x19}},
      // Sequence number
      {"Unable to read retire connection ID frame sequence number.",
       {kVarInt62TwoBytes + 0x11, 0x22}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet_ietf));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      kPacket8ByteConnectionId, kPacket0ByteConnectionId));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(0x1122u, visitor_.retire_connection_id_.sequence_number);

  ASSERT_EQ(0u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(packet_ietf, QUIC_INVALID_RETIRE_CONNECTION_ID_DATA);
}

TEST_P(QuicFramerTest, BuildRetireConnectionIdFramePacket) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    // This frame is only for version 99.
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicRetireConnectionIdFrame frame;
  frame.sequence_number = 0x1122;

  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_RETIRE_CONNECTION_ID frame)
    0x19,
    // sequence number
    kVarInt62TwoBytes + 0x11, 0x22
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(packet_ietf),
      ABSL_ARRAYSIZE(packet_ietf));
}

TEST_P(QuicFramerTest, AckFrameWithInvalidLargestObserved) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (ack frame)
    0x45,
    // largest observed
    0x00, 0x00,
    // Zero delta time.
    0x00, 0x00,
    // first ack block length.
    0x00, 0x00,
    // num timestamps.
    0x00
  };

  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_ACK frame)
    0x02,
    // Largest acked
    kVarInt62OneByte + 0x00,
    // Zero delta time.
    kVarInt62OneByte + 0x00,
    // Ack block count 0
    kVarInt62OneByte + 0x00,
    // First ack block length
    kVarInt62OneByte + 0x00,
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
    p_size = ABSL_ARRAYSIZE(packet_ietf);
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(framer_.detailed_error(), "Largest acked is 0.");
}

TEST_P(QuicFramerTest, FirstAckBlockJustUnderFlow) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (ack frame)
    0x45,
    // largest observed
    0x00, 0x02,
    // Zero delta time.
    0x00, 0x00,
    // first ack block length.
    0x00, 0x03,
    // num timestamps.
    0x00
  };

  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_ACK frame)
    0x02,
    // Largest acked
    kVarInt62OneByte + 0x02,
    // Zero delta time.
    kVarInt62OneByte + 0x00,
    // Ack block count 0
    kVarInt62OneByte + 0x00,
    // First ack block length
    kVarInt62OneByte + 0x02,
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
    p_size = ABSL_ARRAYSIZE(packet_ietf);
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(framer_.detailed_error(),
            "Underflow with first ack block length 3 largest acked is 2.");
}

TEST_P(QuicFramerTest, ThirdAckBlockJustUnderflow) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (ack frame)
    0x60,
    // largest observed
    0x0A,
    // Zero delta time.
    0x00, 0x00,
    // Num of ack blocks
    0x02,
    // first ack block length.
    0x02,
    // gap to next block
    0x01,
    // ack block length
    0x01,
    // gap to next block
    0x01,
    // ack block length
    0x06,
    // num timestamps.
    0x00
  };

  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_ACK frame)
    0x02,
    // Largest acked
    kVarInt62OneByte + 0x0A,
    // Zero delta time.
    kVarInt62OneByte + 0x00,
    // Ack block count 2
    kVarInt62OneByte + 0x02,
    // First ack block length
    kVarInt62OneByte + 0x01,
    // gap to next block length
    kVarInt62OneByte + 0x00,
    // ack block length
    kVarInt62OneByte + 0x00,
    // gap to next block length
    kVarInt62OneByte + 0x00,
    // ack block length
    kVarInt62OneByte + 0x05,
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    p = packet_ietf;
    p_size = ABSL_ARRAYSIZE(packet_ietf);
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    EXPECT_EQ(framer_.detailed_error(),
              "Underflow with ack block length 6 latest ack block end is 5.");
  } else {
    EXPECT_EQ(framer_.detailed_error(),
              "Underflow with ack block length 6, end of block is 6.");
  }
}

TEST_P(QuicFramerTest, CoalescedPacket) {
  if (!QuicVersionHasLongHeaderLengths(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  // clang-format off
  unsigned char packet[] = {
    // first coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (stream frame with fin)
      0xFE,
      // stream id
      0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data length
      0x00, 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
    // second coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x79,
      // frame type (stream frame with fin)
      0xFE,
      // stream id
      0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data length
      0x00, 0x0c,
      // data
      'H',  'E',  'L',  'L',
      'O',  '_',  'W',  'O',
      'R',  'L',  'D',  '?',
  };
  unsigned char packet_ietf[] = {
    // first coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      0x08 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
    // second coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x79,
      // frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      0x08 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'H',  'E',  'L',  'L',
      'O',  '_',  'W',  'O',
      'R',  'L',  'D',  '?',
  };
  // clang-format on
  const size_t first_packet_ietf_size = 46;
  // If the first packet changes, the attempt to fix the first byte of the
  // second packet will fail.
  EXPECT_EQ(packet_ietf[first_packet_ietf_size], 0xD3);

  unsigned char* p = packet;
  size_t p_length = ABSL_ARRAYSIZE(packet);
  if (framer_.version().HasIetfQuicFrames()) {
    ReviseFirstByteByVersion(packet_ietf);
    ReviseFirstByteByVersion(&packet_ietf[first_packet_ietf_size]);
    p = packet_ietf;
    p_length = ABSL_ARRAYSIZE(packet_ietf);
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_length, false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());

  // Stream ID should be the last 3 bytes of kStreamId.
  EXPECT_EQ(0x00FFFFFF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  ASSERT_EQ(visitor_.coalesced_packets_.size(), 1u);
  EXPECT_TRUE(framer_.ProcessPacket(*visitor_.coalesced_packets_[0].get()));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());

  ASSERT_EQ(2u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());

  // Stream ID should be the last 3 bytes of kStreamId.
  EXPECT_EQ(0x00FFFFFF & kStreamId, visitor_.stream_frames_[1]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[1]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[1]->offset);
  CheckStreamFrameData("HELLO_WORLD?", visitor_.stream_frames_[1].get());
}

TEST_P(QuicFramerTest, CoalescedPacketWithUdpPadding) {
  if (!framer_.version().HasLongHeaderLengths()) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  // clang-format off
  unsigned char packet[] = {
    // first coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (stream frame with fin)
      0xFE,
      // stream id
      0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data length
      0x00, 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
    // padding
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
  };
  unsigned char packet_ietf[] = {
    // first coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      0x08 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
      // padding
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_length = ABSL_ARRAYSIZE(packet);
  if (framer_.version().HasIetfQuicFrames()) {
    ReviseFirstByteByVersion(packet_ietf);
    p = packet_ietf;
    p_length = ABSL_ARRAYSIZE(packet_ietf);
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_length, false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());

  // Stream ID should be the last 3 bytes of kStreamId.
  EXPECT_EQ(0x00FFFFFF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  EXPECT_EQ(visitor_.coalesced_packets_.size(), 0u);
}

TEST_P(QuicFramerTest, CoalescedPacketWithDifferentVersion) {
  if (!QuicVersionHasLongHeaderLengths(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  // clang-format off
  unsigned char packet[] = {
    // first coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (stream frame with fin)
      0xFE,
      // stream id
      0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data length
      0x00, 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
    // second coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // garbage version
      'G', 'A', 'B', 'G',
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x79,
      // frame type (stream frame with fin)
      0xFE,
      // stream id
      0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data length
      0x00, 0x0c,
      // data
      'H',  'E',  'L',  'L',
      'O',  '_',  'W',  'O',
      'R',  'L',  'D',  '?',
  };
  unsigned char packet_ietf[] = {
    // first coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      0x08 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
    // second coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // garbage version
      'G', 'A', 'B', 'G',
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x79,
      // frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      0x08 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'H',  'E',  'L',  'L',
      'O',  '_',  'W',  'O',
      'R',  'L',  'D',  '?',
  };
  // clang-format on
  const size_t first_packet_ietf_size = 46;
  // If the first packet changes, the attempt to fix the first byte of the
  // second packet will fail.
  EXPECT_EQ(packet_ietf[first_packet_ietf_size], 0xD3);

  unsigned char* p = packet;
  size_t p_length = ABSL_ARRAYSIZE(packet);
  if (framer_.version().HasIetfQuicFrames()) {
    ReviseFirstByteByVersion(packet_ietf);
    ReviseFirstByteByVersion(&packet_ietf[first_packet_ietf_size]);
    p = packet_ietf;
    p_length = ABSL_ARRAYSIZE(packet_ietf);
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_length, false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());

  // Stream ID should be the last 3 bytes of kStreamId.
  EXPECT_EQ(0x00FFFFFF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  ASSERT_EQ(visitor_.coalesced_packets_.size(), 1u);
  EXPECT_TRUE(framer_.ProcessPacket(*visitor_.coalesced_packets_[0].get()));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  // Verify version mismatch gets reported.
  EXPECT_EQ(1, visitor_.version_mismatch_);
}

TEST_P(QuicFramerTest, UndecryptablePacketWithoutDecrypter) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  if (!framer_.version().KnowsWhichDecrypterToUse()) {
    // We create a bad client decrypter by using initial encryption with a
    // bogus connection ID; it should fail to decrypt everything.
    QuicConnectionId bogus_connection_id = TestConnectionId(0xbad);
    CrypterPair bogus_crypters;
    CryptoUtils::CreateInitialObfuscators(Perspective::IS_CLIENT,
                                          framer_.version(),
                                          bogus_connection_id, &bogus_crypters);
    // This removes all other decrypters.
    framer_.SetDecrypter(ENCRYPTION_FORWARD_SECURE,
                         std::move(bogus_crypters.decrypter));
  }

  // clang-format off
  unsigned char packet[] = {
    // public flags (long header with packet type HANDSHAKE and
    // 4-byte packet number)
    0xE3,
    // version
    QUIC_VERSION_BYTES,
    // connection ID lengths
    0x05,
    // source connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // long header packet length
    0x05,
    // packet number
    0x12, 0x34, 0x56, 0x00,
    // padding frames
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  unsigned char packet49[] = {
    // public flags (long header with packet type HANDSHAKE and
    // 4-byte packet number)
    0xE3,
    // version
    QUIC_VERSION_BYTES,
    // destination connection ID length
    0x00,
    // source connection ID length
    0x08,
    // source connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // long header packet length
    0x24,
    // packet number
    0x12, 0x34, 0x56, 0x00,
    // padding frames
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_length = ABSL_ARRAYSIZE(packet);
  if (framer_.version().HasLongHeaderLengths()) {
    ReviseFirstByteByVersion(packet49);
    p = packet49;
    p_length = ABSL_ARRAYSIZE(packet49);
  }
  // First attempt decryption without the handshake crypter.
  EXPECT_FALSE(
      framer_.ProcessPacket(QuicEncryptedPacket(AsChars(p), p_length, false)));
  EXPECT_THAT(framer_.error(), IsError(QUIC_DECRYPTION_FAILURE));
  ASSERT_EQ(1u, visitor_.undecryptable_packets_.size());
  ASSERT_EQ(1u, visitor_.undecryptable_decryption_levels_.size());
  ASSERT_EQ(1u, visitor_.undecryptable_has_decryption_keys_.size());
  quiche::test::CompareCharArraysWithHexError(
      "undecryptable packet", visitor_.undecryptable_packets_[0]->data(),
      visitor_.undecryptable_packets_[0]->length(), AsChars(p), p_length);
  if (framer_.version().KnowsWhichDecrypterToUse()) {
    EXPECT_EQ(ENCRYPTION_HANDSHAKE,
              visitor_.undecryptable_decryption_levels_[0]);
  }
  EXPECT_FALSE(visitor_.undecryptable_has_decryption_keys_[0]);
}

TEST_P(QuicFramerTest, UndecryptablePacketWithDecrypter) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  // We create a bad client decrypter by using initial encryption with a
  // bogus connection ID; it should fail to decrypt everything.
  QuicConnectionId bogus_connection_id = TestConnectionId(0xbad);
  CrypterPair bad_handshake_crypters;
  CryptoUtils::CreateInitialObfuscators(Perspective::IS_CLIENT,
                                        framer_.version(), bogus_connection_id,
                                        &bad_handshake_crypters);
  if (framer_.version().KnowsWhichDecrypterToUse()) {
    framer_.InstallDecrypter(ENCRYPTION_HANDSHAKE,
                             std::move(bad_handshake_crypters.decrypter));
  } else {
    framer_.SetDecrypter(ENCRYPTION_HANDSHAKE,
                         std::move(bad_handshake_crypters.decrypter));
  }

  // clang-format off
  unsigned char packet[] = {
    // public flags (long header with packet type HANDSHAKE and
    // 4-byte packet number)
    0xE3,
    // version
    QUIC_VERSION_BYTES,
    // connection ID lengths
    0x05,
    // source connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // long header packet length
    0x05,
    // packet number
    0x12, 0x34, 0x56, 0x00,
    // padding frames
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  unsigned char packet49[] = {
    // public flags (long header with packet type HANDSHAKE and
    // 4-byte packet number)
    0xE3,
    // version
    QUIC_VERSION_BYTES,
    // destination connection ID length
    0x00,
    // source connection ID length
    0x08,
    // source connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // long header packet length
    0x24,
    // packet number
    0x12, 0x34, 0x56, 0x00,
    // padding frames
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_length = ABSL_ARRAYSIZE(packet);
  if (framer_.version().HasLongHeaderLengths()) {
    ReviseFirstByteByVersion(packet49);
    p = packet49;
    p_length = ABSL_ARRAYSIZE(packet49);
  }

  EXPECT_FALSE(
      framer_.ProcessPacket(QuicEncryptedPacket(AsChars(p), p_length, false)));
  EXPECT_THAT(framer_.error(), IsError(QUIC_DECRYPTION_FAILURE));
  ASSERT_EQ(1u, visitor_.undecryptable_packets_.size());
  ASSERT_EQ(1u, visitor_.undecryptable_decryption_levels_.size());
  ASSERT_EQ(1u, visitor_.undecryptable_has_decryption_keys_.size());
  quiche::test::CompareCharArraysWithHexError(
      "undecryptable packet", visitor_.undecryptable_packets_[0]->data(),
      visitor_.undecryptable_packets_[0]->length(), AsChars(p), p_length);
  if (framer_.version().KnowsWhichDecrypterToUse()) {
    EXPECT_EQ(ENCRYPTION_HANDSHAKE,
              visitor_.undecryptable_decryption_levels_[0]);
  }
  EXPECT_EQ(framer_.version().KnowsWhichDecrypterToUse(),
            visitor_.undecryptable_has_decryption_keys_[0]);
}

TEST_P(QuicFramerTest, UndecryptableCoalescedPacket) {
  if (!QuicVersionHasLongHeaderLengths(framer_.transport_version())) {
    return;
  }
  ASSERT_TRUE(framer_.version().KnowsWhichDecrypterToUse());
  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  // We create a bad client decrypter by using initial encryption with a
  // bogus connection ID; it should fail to decrypt everything.
  QuicConnectionId bogus_connection_id = TestConnectionId(0xbad);
  CrypterPair bad_handshake_crypters;
  CryptoUtils::CreateInitialObfuscators(Perspective::IS_CLIENT,
                                        framer_.version(), bogus_connection_id,
                                        &bad_handshake_crypters);
  framer_.InstallDecrypter(ENCRYPTION_HANDSHAKE,
                           std::move(bad_handshake_crypters.decrypter));
  // clang-format off
  unsigned char packet[] = {
    // first coalesced packet
      // public flags (long header with packet type HANDSHAKE and
      // 4-byte packet number)
      0xE3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (stream frame with fin)
      0xFE,
      // stream id
      0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data length
      0x00, 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
    // second coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x79,
      // frame type (stream frame with fin)
      0xFE,
      // stream id
      0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data length
      0x00, 0x0c,
      // data
      'H',  'E',  'L',  'L',
      'O',  '_',  'W',  'O',
      'R',  'L',  'D',  '?',
  };
  unsigned char packet_ietf[] = {
    // first coalesced packet
      // public flags (long header with packet type HANDSHAKE and
      // 4-byte packet number)
      0xE3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      0x08 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
    // second coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x79,
      // frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      0x08 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'H',  'E',  'L',  'L',
      'O',  '_',  'W',  'O',
      'R',  'L',  'D',  '?',
  };
  // clang-format on
  const size_t length_of_first_coalesced_packet = 46;
  // If the first packet changes, the attempt to fix the first byte of the
  // second packet will fail.
  EXPECT_EQ(packet_ietf[length_of_first_coalesced_packet], 0xD3);

  unsigned char* p = packet;
  size_t p_length = ABSL_ARRAYSIZE(packet);
  if (framer_.version().HasIetfQuicFrames()) {
    ReviseFirstByteByVersion(packet_ietf);
    ReviseFirstByteByVersion(&packet_ietf[length_of_first_coalesced_packet]);
    p = packet_ietf;
    p_length = ABSL_ARRAYSIZE(packet_ietf);
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_length, false);

  EXPECT_FALSE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsError(QUIC_DECRYPTION_FAILURE));

  ASSERT_EQ(1u, visitor_.undecryptable_packets_.size());
  ASSERT_EQ(1u, visitor_.undecryptable_decryption_levels_.size());
  ASSERT_EQ(1u, visitor_.undecryptable_has_decryption_keys_.size());
  // Make sure we only receive the first undecryptable packet and not the
  // full packet including the second coalesced packet.
  quiche::test::CompareCharArraysWithHexError(
      "undecryptable packet", visitor_.undecryptable_packets_[0]->data(),
      visitor_.undecryptable_packets_[0]->length(), AsChars(p),
      length_of_first_coalesced_packet);
  EXPECT_EQ(ENCRYPTION_HANDSHAKE, visitor_.undecryptable_decryption_levels_[0]);
  EXPECT_TRUE(visitor_.undecryptable_has_decryption_keys_[0]);

  // Make sure the second coalesced packet is parsed correctly.
  ASSERT_EQ(visitor_.coalesced_packets_.size(), 1u);
  EXPECT_TRUE(framer_.ProcessPacket(*visitor_.coalesced_packets_[0].get()));

  ASSERT_TRUE(visitor_.header_.get());

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());

  // Stream ID should be the last 3 bytes of kStreamId.
  EXPECT_EQ(0x00FFFFFF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("HELLO_WORLD?", visitor_.stream_frames_[0].get());
}

TEST_P(QuicFramerTest, MismatchedCoalescedPacket) {
  if (!QuicVersionHasLongHeaderLengths(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  // clang-format off
  unsigned char packet[] = {
    // first coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (stream frame with fin)
      0xFE,
      // stream id
      0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data length
      0x00, 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
    // second coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x79,
      // frame type (stream frame with fin)
      0xFE,
      // stream id
      0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data length
      0x00, 0x0c,
      // data
      'H',  'E',  'L',  'L',
      'O',  '_',  'W',  'O',
      'R',  'L',  'D',  '?',
  };
  unsigned char packet_ietf[] = {
    // first coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      0x08 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
    // second coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x79,
      // frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      0x08 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'H',  'E',  'L',  'L',
      'O',  '_',  'W',  'O',
      'R',  'L',  'D',  '?',
  };
  // clang-format on
  const size_t length_of_first_coalesced_packet = 46;
  // If the first packet changes, the attempt to fix the first byte of the
  // second packet will fail.
  EXPECT_EQ(packet_ietf[length_of_first_coalesced_packet], 0xD3);

  unsigned char* p = packet;
  size_t p_length = ABSL_ARRAYSIZE(packet);
  if (framer_.version().HasIetfQuicFrames()) {
    ReviseFirstByteByVersion(packet_ietf);
    ReviseFirstByteByVersion(&packet_ietf[length_of_first_coalesced_packet]);
    p = packet_ietf;
    p_length = ABSL_ARRAYSIZE(packet_ietf);
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_length, false);

  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());

  // Stream ID should be the last 3 bytes of kStreamId.
  EXPECT_EQ(0x00FFFFFF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  ASSERT_EQ(visitor_.coalesced_packets_.size(), 0u);
}

TEST_P(QuicFramerTest, InvalidCoalescedPacket) {
  if (!QuicVersionHasLongHeaderLengths(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  // clang-format off
  unsigned char packet[] = {
    // first coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (stream frame with fin)
      0xFE,
      // stream id
      0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data length
      0x00, 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
    // second coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version would be here but we cut off the invalid coalesced header.
  };
  unsigned char packet_ietf[] = {
    // first coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version
      QUIC_VERSION_BYTES,
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      0x08 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
    // second coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xD3,
      // version would be here but we cut off the invalid coalesced header.
  };
  // clang-format on
  const size_t length_of_first_coalesced_packet = 46;
  // If the first packet changes, the attempt to fix the first byte of the
  // second packet will fail.
  EXPECT_EQ(packet_ietf[length_of_first_coalesced_packet], 0xD3);

  unsigned char* p = packet;
  size_t p_length = ABSL_ARRAYSIZE(packet);
  if (framer_.version().HasIetfQuicFrames()) {
    ReviseFirstByteByVersion(packet_ietf);
    ReviseFirstByteByVersion(&packet_ietf[length_of_first_coalesced_packet]);
    p = packet_ietf;
    p_length = ABSL_ARRAYSIZE(packet_ietf);
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_length, false);

  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  ASSERT_TRUE(visitor_.header_.get());

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());

  // Stream ID should be the last 3 bytes of kStreamId.
  EXPECT_EQ(0x00FFFFFF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  ASSERT_EQ(visitor_.coalesced_packets_.size(), 0u);
}

// Some IETF implementations send an initial followed by zeroes instead of
// padding inside the initial. We need to make sure that we still process
// the initial correctly and ignore the zeroes.
TEST_P(QuicFramerTest, CoalescedPacketWithZeroesRoundTrip) {
  if (!QuicVersionHasLongHeaderLengths(framer_.transport_version()) ||
      !framer_.version().UsesInitialObfuscators()) {
    return;
  }
  ASSERT_TRUE(framer_.version().KnowsWhichDecrypterToUse());
  QuicConnectionId connection_id = FramerTestConnectionId();
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  CrypterPair client_crypters;
  CryptoUtils::CreateInitialObfuscators(Perspective::IS_CLIENT,
                                        framer_.version(), connection_id,
                                        &client_crypters);
  framer_.SetEncrypter(ENCRYPTION_INITIAL,
                       std::move(client_crypters.encrypter));

  QuicPacketHeader header;
  header.destination_connection_id = connection_id;
  header.version_flag = true;
  header.packet_number = kPacketNumber;
  header.packet_number_length = PACKET_4BYTE_PACKET_NUMBER;
  header.long_packet_type = INITIAL;
  header.length_length = quiche::VARIABLE_LENGTH_INTEGER_LENGTH_2;
  header.retry_token_length_length = quiche::VARIABLE_LENGTH_INTEGER_LENGTH_1;
  QuicFrames frames = {QuicFrame(QuicPingFrame()),
                       QuicFrame(QuicPaddingFrame(3))};

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_NE(nullptr, data);

  // Add zeroes after the valid initial packet.
  unsigned char packet[kMaxOutgoingPacketSize] = {};
  size_t encrypted_length =
      framer_.EncryptPayload(ENCRYPTION_INITIAL, header.packet_number, *data,
                             AsChars(packet), ABSL_ARRAYSIZE(packet));
  ASSERT_NE(0u, encrypted_length);

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  CrypterPair server_crypters;
  CryptoUtils::CreateInitialObfuscators(Perspective::IS_SERVER,
                                        framer_.version(), connection_id,
                                        &server_crypters);
  framer_.InstallDecrypter(ENCRYPTION_INITIAL,
                           std::move(server_crypters.decrypter));

  // Make sure the first long header initial packet parses correctly.
  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);

  // Make sure we discard the subsequent zeroes.
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_TRUE(visitor_.coalesced_packets_.empty());
}

TEST_P(QuicFramerTest, ClientReceivesWrongVersion) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  // clang-format off
  unsigned char packet[] = {
       // public flags (long header with packet type INITIAL)
       0xC3,
       // version that is different from the framer's version
       'Q', '0', '4', '3',
       // connection ID lengths
       0x05,
       // source connection ID
       0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
       // packet number
       0x01,
       // padding frame
       0x00,
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsError(QUIC_PACKET_WRONG_VERSION));
  EXPECT_EQ("Client received unexpected version.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, PacketHeaderWithVariableLengthConnectionId) {
  if (!framer_.version().AllowsVariableLengthConnectionIds()) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  uint8_t connection_id_bytes[9] = {0xFE, 0xDC, 0xBA, 0x98, 0x76,
                                    0x54, 0x32, 0x10, 0x42};
  QuicConnectionId connection_id(reinterpret_cast<char*>(connection_id_bytes),
                                 sizeof(connection_id_bytes));
  QuicFramerPeer::SetLargestPacketNumber(&framer_, kPacketNumber - 2);
  QuicFramerPeer::SetExpectedServerConnectionIDLength(&framer_,
                                                      connection_id.length());

  // clang-format off
  PacketFragments packet = {
      // type (8 byte connection_id and 1 byte packet number)
      {"Unable to read first byte.",
       {0x40}},
      // connection_id
      {"Unable to read destination connection ID.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0x42}},
      // packet number
      {"Unable to read packet number.",
       {0x78}},
  };

  PacketFragments packet_with_padding = {
      // type (8 byte connection_id and 1 byte packet number)
      {"Unable to read first byte.",
       {0x40}},
      // connection_id
      {"Unable to read destination connection ID.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0x42}},
      // packet number
      {"",
       {0x78}},
      // padding
      {"", {0x00, 0x00, 0x00}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.version().HasHeaderProtection() ? packet_with_padding : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  if (framer_.version().HasHeaderProtection()) {
    EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
    EXPECT_THAT(framer_.error(), IsQuicNoError());
  } else {
    EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
    EXPECT_THAT(framer_.error(), IsError(QUIC_MISSING_PAYLOAD));
  }
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(connection_id, visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_FALSE(visitor_.header_->version_flag);
  EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, MultiplePacketNumberSpaces) {
  framer_.EnableMultiplePacketNumberSpacesSupport();

  // clang-format off
  unsigned char long_header_packet[] = {
       // public flags (long header with packet type ZERO_RTT_PROTECTED and
       // 4-byte packet number)
       0xD3,
       // version
       QUIC_VERSION_BYTES,
       // destination connection ID length
       0x50,
       // destination connection ID
       0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
       // packet number
       0x12, 0x34, 0x56, 0x78,
       // padding frame
       0x00,
   };
  unsigned char long_header_packet_ietf[] = {
       // public flags (long header with packet type ZERO_RTT_PROTECTED and
       // 4-byte packet number)
       0xD3,
       // version
       QUIC_VERSION_BYTES,
       // destination connection ID length
       0x08,
       // destination connection ID
       0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
       // source connection ID length
       0x00,
       // long header packet length
       0x05,
       // packet number
       0x12, 0x34, 0x56, 0x78,
       // padding frame
       0x00,
  };
  // clang-format on

  if (framer_.version().KnowsWhichDecrypterToUse()) {
    framer_.InstallDecrypter(ENCRYPTION_ZERO_RTT,
                             std::make_unique<TestDecrypter>());
    framer_.RemoveDecrypter(ENCRYPTION_INITIAL);
  } else {
    framer_.SetDecrypter(ENCRYPTION_ZERO_RTT,
                         std::make_unique<TestDecrypter>());
  }
  if (!QuicVersionHasLongHeaderLengths(framer_.transport_version())) {
    EXPECT_TRUE(framer_.ProcessPacket(
        QuicEncryptedPacket(AsChars(long_header_packet),
                            ABSL_ARRAYSIZE(long_header_packet), false)));
  } else {
    ReviseFirstByteByVersion(long_header_packet_ietf);
    EXPECT_TRUE(framer_.ProcessPacket(
        QuicEncryptedPacket(AsChars(long_header_packet_ietf),
                            ABSL_ARRAYSIZE(long_header_packet_ietf), false)));
  }

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  EXPECT_FALSE(
      QuicFramerPeer::GetLargestDecryptedPacketNumber(&framer_, INITIAL_DATA)
          .IsInitialized());
  EXPECT_FALSE(
      QuicFramerPeer::GetLargestDecryptedPacketNumber(&framer_, HANDSHAKE_DATA)
          .IsInitialized());
  EXPECT_EQ(kPacketNumber, QuicFramerPeer::GetLargestDecryptedPacketNumber(
                               &framer_, APPLICATION_DATA));

  // clang-format off
  unsigned char short_header_packet[] = {
     // type (short header, 1 byte packet number)
     0x40,
     // connection_id
     0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
     // packet number
     0x79,
     // padding frame
     0x00, 0x00, 0x00,
  };
  // clang-format on

  QuicEncryptedPacket short_header_encrypted(
      AsChars(short_header_packet), ABSL_ARRAYSIZE(short_header_packet), false);
  if (framer_.version().KnowsWhichDecrypterToUse()) {
    framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                             std::make_unique<TestDecrypter>());
    framer_.RemoveDecrypter(ENCRYPTION_ZERO_RTT);
  } else {
    framer_.SetDecrypter(ENCRYPTION_FORWARD_SECURE,
                         std::make_unique<TestDecrypter>());
  }
  EXPECT_TRUE(framer_.ProcessPacket(short_header_encrypted));

  EXPECT_THAT(framer_.error(), IsQuicNoError());
  EXPECT_FALSE(
      QuicFramerPeer::GetLargestDecryptedPacketNumber(&framer_, INITIAL_DATA)
          .IsInitialized());
  EXPECT_FALSE(
      QuicFramerPeer::GetLargestDecryptedPacketNumber(&framer_, HANDSHAKE_DATA)
          .IsInitialized());
  EXPECT_EQ(kPacketNumber + 1, QuicFramerPeer::GetLargestDecryptedPacketNumber(
                                   &framer_, APPLICATION_DATA));
}

TEST_P(QuicFramerTest, IetfRetryPacketRejected) {
  if (!framer_.version().KnowsWhichDecrypterToUse() ||
      framer_.version().SupportsRetry()) {
    return;
  }

  // clang-format off
  PacketFragments packet = {
    // public flags (IETF Retry packet, 0-length original destination CID)
    {"Unable to read first byte.",
     {0xf0}},
    // version tag
    {"Unable to read protocol version.",
     {QUIC_VERSION_BYTES}},
    // connection_id length
    {"RETRY not supported in this version.",
     {0x00}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_THAT(framer_.error(), IsError(QUIC_INVALID_PACKET_HEADER));
  CheckFramingBoundaries(packet, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, RetryPacketRejectedWithMultiplePacketNumberSpaces) {
  if (framer_.version().SupportsRetry()) {
    return;
  }
  framer_.EnableMultiplePacketNumberSpacesSupport();

  // clang-format off
  PacketFragments packet = {
    // public flags (IETF Retry packet, 0-length original destination CID)
    {"Unable to read first byte.",
     {0xf0}},
    // version tag
    {"Unable to read protocol version.",
     {QUIC_VERSION_BYTES}},
    // connection_id length
    {"RETRY not supported in this version.",
     {0x00}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_THAT(framer_.error(), IsError(QUIC_INVALID_PACKET_HEADER));
  CheckFramingBoundaries(packet, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, WriteClientVersionNegotiationProbePacket) {
  // clang-format off
  static const uint8_t expected_packet[1200] = {
    // IETF long header with fixed bit set, type initial, all-0 encrypted bits.
    0xc0,
    // Version, part of the IETF space reserved for negotiation.
    0xca, 0xba, 0xda, 0xda,
    // Destination connection ID length 8.
    0x08,
    // 8-byte destination connection ID.
    0x56, 0x4e, 0x20, 0x70, 0x6c, 0x7a, 0x20, 0x21,
    // Source connection ID length 0.
    0x00,
    // 8 bytes of zeroes followed by 8 bytes of ones to ensure that this does
    // not parse with any known version.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    // zeroes to pad to 16 byte boundary.
    0x00,
    // A polite greeting in case a human sees this in tcpdump.
    0x54, 0x68, 0x69, 0x73, 0x20, 0x70, 0x61, 0x63,
    0x6b, 0x65, 0x74, 0x20, 0x6f, 0x6e, 0x6c, 0x79,
    0x20, 0x65, 0x78, 0x69, 0x73, 0x74, 0x73, 0x20,
    0x74, 0x6f, 0x20, 0x74, 0x72, 0x69, 0x67, 0x67,
    0x65, 0x72, 0x20, 0x49, 0x45, 0x54, 0x46, 0x20,
    0x51, 0x55, 0x49, 0x43, 0x20, 0x76, 0x65, 0x72,
    0x73, 0x69, 0x6f, 0x6e, 0x20, 0x6e, 0x65, 0x67,
    0x6f, 0x74, 0x69, 0x61, 0x74, 0x69, 0x6f, 0x6e,
    0x2e, 0x20, 0x50, 0x6c, 0x65, 0x61, 0x73, 0x65,
    0x20, 0x72, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x64,
    0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x61, 0x20,
    0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x20,
    0x4e, 0x65, 0x67, 0x6f, 0x74, 0x69, 0x61, 0x74,
    0x69, 0x6f, 0x6e, 0x20, 0x70, 0x61, 0x63, 0x6b,
    0x65, 0x74, 0x20, 0x69, 0x6e, 0x64, 0x69, 0x63,
    0x61, 0x74, 0x69, 0x6e, 0x67, 0x20, 0x77, 0x68,
    0x61, 0x74, 0x20, 0x76, 0x65, 0x72, 0x73, 0x69,
    0x6f, 0x6e, 0x73, 0x20, 0x79, 0x6f, 0x75, 0x20,
    0x73, 0x75, 0x70, 0x70, 0x6f, 0x72, 0x74, 0x2e,
    0x20, 0x54, 0x68, 0x61, 0x6e, 0x6b, 0x20, 0x79,
    0x6f, 0x75, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x68,
    0x61, 0x76, 0x65, 0x20, 0x61, 0x20, 0x6e, 0x69,
    0x63, 0x65, 0x20, 0x64, 0x61, 0x79, 0x2e, 0x00,
  };
  // clang-format on
  char packet[1200];
  char destination_connection_id_bytes[] = {0x56, 0x4e, 0x20, 0x70,
                                            0x6c, 0x7a, 0x20, 0x21};
  EXPECT_TRUE(QuicFramer::WriteClientVersionNegotiationProbePacket(
      packet, sizeof(packet), destination_connection_id_bytes,
      sizeof(destination_connection_id_bytes)));
  quiche::test::CompareCharArraysWithHexError(
      "constructed packet", packet, sizeof(packet),
      reinterpret_cast<const char*>(expected_packet), sizeof(expected_packet));
  QuicEncryptedPacket encrypted(reinterpret_cast<const char*>(packet),
                                sizeof(packet), false);
  if (!framer_.version().HasLengthPrefixedConnectionIds()) {
    // We can only parse the connection ID with a parser expecting
    // length-prefixed connection IDs.
    EXPECT_FALSE(framer_.ProcessPacket(encrypted));
    return;
  }
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  ASSERT_TRUE(visitor_.header_.get());
  QuicConnectionId probe_payload_connection_id(
      reinterpret_cast<const char*>(destination_connection_id_bytes),
      sizeof(destination_connection_id_bytes));
  EXPECT_EQ(probe_payload_connection_id,
            visitor_.header_.get()->destination_connection_id);
}

TEST_P(QuicFramerTest, DispatcherParseOldClientVersionNegotiationProbePacket) {
  // clang-format off
  static const uint8_t packet[1200] = {
    // IETF long header with fixed bit set, type initial, all-0 encrypted bits.
    0xc0,
    // Version, part of the IETF space reserved for negotiation.
    0xca, 0xba, 0xda, 0xba,
    // Destination connection ID length 8, source connection ID length 0.
    0x50,
    // 8-byte destination connection ID.
    0x56, 0x4e, 0x20, 0x70, 0x6c, 0x7a, 0x20, 0x21,
    // 8 bytes of zeroes followed by 8 bytes of ones to ensure that this does
    // not parse with any known version.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    // 2 bytes of zeroes to pad to 16 byte boundary.
    0x00, 0x00,
    // A polite greeting in case a human sees this in tcpdump.
    0x54, 0x68, 0x69, 0x73, 0x20, 0x70, 0x61, 0x63,
    0x6b, 0x65, 0x74, 0x20, 0x6f, 0x6e, 0x6c, 0x79,
    0x20, 0x65, 0x78, 0x69, 0x73, 0x74, 0x73, 0x20,
    0x74, 0x6f, 0x20, 0x74, 0x72, 0x69, 0x67, 0x67,
    0x65, 0x72, 0x20, 0x49, 0x45, 0x54, 0x46, 0x20,
    0x51, 0x55, 0x49, 0x43, 0x20, 0x76, 0x65, 0x72,
    0x73, 0x69, 0x6f, 0x6e, 0x20, 0x6e, 0x65, 0x67,
    0x6f, 0x74, 0x69, 0x61, 0x74, 0x69, 0x6f, 0x6e,
    0x2e, 0x20, 0x50, 0x6c, 0x65, 0x61, 0x73, 0x65,
    0x20, 0x72, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x64,
    0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x61, 0x20,
    0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x20,
    0x4e, 0x65, 0x67, 0x6f, 0x74, 0x69, 0x61, 0x74,
    0x69, 0x6f, 0x6e, 0x20, 0x70, 0x61, 0x63, 0x6b,
    0x65, 0x74, 0x20, 0x69, 0x6e, 0x64, 0x69, 0x63,
    0x61, 0x74, 0x69, 0x6e, 0x67, 0x20, 0x77, 0x68,
    0x61, 0x74, 0x20, 0x76, 0x65, 0x72, 0x73, 0x69,
    0x6f, 0x6e, 0x73, 0x20, 0x79, 0x6f, 0x75, 0x20,
    0x73, 0x75, 0x70, 0x70, 0x6f, 0x72, 0x74, 0x2e,
    0x20, 0x54, 0x68, 0x61, 0x6e, 0x6b, 0x20, 0x79,
    0x6f, 0x75, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x68,
    0x61, 0x76, 0x65, 0x20, 0x61, 0x20, 0x6e, 0x69,
    0x63, 0x65, 0x20, 0x64, 0x61, 0x79, 0x2e, 0x00,
  };
  // clang-format on
  char expected_destination_connection_id_bytes[] = {0x56, 0x4e, 0x20, 0x70,
                                                     0x6c, 0x7a, 0x20, 0x21};
  QuicConnectionId expected_destination_connection_id(
      reinterpret_cast<const char*>(expected_destination_connection_id_bytes),
      sizeof(expected_destination_connection_id_bytes));

  QuicEncryptedPacket encrypted(reinterpret_cast<const char*>(packet),
                                sizeof(packet));
  PacketHeaderFormat format = GOOGLE_QUIC_PACKET;
  QuicLongHeaderType long_packet_type = INVALID_PACKET_TYPE;
  bool version_present = false, has_length_prefix = true;
  QuicVersionLabel version_label = 33;
  ParsedQuicVersion parsed_version = UnsupportedQuicVersion();
  QuicConnectionId destination_connection_id = TestConnectionId(1);
  QuicConnectionId source_connection_id = TestConnectionId(2);
  std::optional<absl::string_view> retry_token;
  std::string detailed_error = "foobar";
  QuicErrorCode header_parse_result = QuicFramer::ParsePublicHeaderDispatcher(
      encrypted, kQuicDefaultConnectionIdLength, &format, &long_packet_type,
      &version_present, &has_length_prefix, &version_label, &parsed_version,
      &destination_connection_id, &source_connection_id, &retry_token,
      &detailed_error);
  EXPECT_THAT(header_parse_result, IsQuicNoError());
  EXPECT_EQ(IETF_QUIC_LONG_HEADER_PACKET, format);
  EXPECT_TRUE(version_present);
  EXPECT_FALSE(has_length_prefix);
  EXPECT_EQ(0xcabadaba, version_label);
  EXPECT_EQ(expected_destination_connection_id, destination_connection_id);
  EXPECT_EQ(EmptyQuicConnectionId(), source_connection_id);
  EXPECT_FALSE(retry_token.has_value());
  EXPECT_EQ("", detailed_error);
}

TEST_P(QuicFramerTest, DispatcherParseClientVersionNegotiationProbePacket) {
  // clang-format off
  static const uint8_t packet[1200] = {
    // IETF long header with fixed bit set, type initial, all-0 encrypted bits.
    0xc0,
    // Version, part of the IETF space reserved for negotiation.
    0xca, 0xba, 0xda, 0xba,
    // Destination connection ID length 8.
    0x08,
    // 8-byte destination connection ID.
    0x56, 0x4e, 0x20, 0x70, 0x6c, 0x7a, 0x20, 0x21,
    // Source connection ID length 0.
    0x00,
    // 8 bytes of zeroes followed by 8 bytes of ones to ensure that this does
    // not parse with any known version.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    // 1 byte of zeroes to pad to 16 byte boundary.
    0x00,
    // A polite greeting in case a human sees this in tcpdump.
    0x54, 0x68, 0x69, 0x73, 0x20, 0x70, 0x61, 0x63,
    0x6b, 0x65, 0x74, 0x20, 0x6f, 0x6e, 0x6c, 0x79,
    0x20, 0x65, 0x78, 0x69, 0x73, 0x74, 0x73, 0x20,
    0x74, 0x6f, 0x20, 0x74, 0x72, 0x69, 0x67, 0x67,
    0x65, 0x72, 0x20, 0x49, 0x45, 0x54, 0x46, 0x20,
    0x51, 0x55, 0x49, 0x43, 0x20, 0x76, 0x65, 0x72,
    0x73, 0x69, 0x6f, 0x6e, 0x20, 0x6e, 0x65, 0x67,
    0x6f, 0x74, 0x69, 0x61, 0x74, 0x69, 0x6f, 0x6e,
    0x2e, 0x20, 0x50, 0x6c, 0x65, 0x61, 0x73, 0x65,
    0x20, 0x72, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x64,
    0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x61, 0x20,
    0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x20,
    0x4e, 0x65, 0x67, 0x6f, 0x74, 0x69, 0x61, 0x74,
    0x69, 0x6f, 0x6e, 0x20, 0x70, 0x61, 0x63, 0x6b,
    0x65, 0x74, 0x20, 0x69, 0x6e, 0x64, 0x69, 0x63,
    0x61, 0x74, 0x69, 0x6e, 0x67, 0x20, 0x77, 0x68,
    0x61, 0x74, 0x20, 0x76, 0x65, 0x72, 0x73, 0x69,
    0x6f, 0x6e, 0x73, 0x20, 0x79, 0x6f, 0x75, 0x20,
    0x73, 0x75, 0x70, 0x70, 0x6f, 0x72, 0x74, 0x2e,
    0x20, 0x54, 0x68, 0x61, 0x6e, 0x6b, 0x20, 0x79,
    0x6f, 0x75, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x68,
    0x61, 0x76, 0x65, 0x20, 0x61, 0x20, 0x6e, 0x69,
    0x63, 0x65, 0x20, 0x64, 0x61, 0x79, 0x2e, 0x00,
  };
  // clang-format on
  char expected_destination_connection_id_bytes[] = {0x56, 0x4e, 0x20, 0x70,
                                                     0x6c, 0x7a, 0x20, 0x21};
  QuicConnectionId expected_destination_connection_id(
      reinterpret_cast<const char*>(expected_destination_connection_id_bytes),
      sizeof(expected_destination_connection_id_bytes));

  QuicEncryptedPacket encrypted(reinterpret_cast<const char*>(packet),
                                sizeof(packet));
  PacketHeaderFormat format = GOOGLE_QUIC_PACKET;
  QuicLongHeaderType long_packet_type = INVALID_PACKET_TYPE;
  bool version_present = false, has_length_prefix = false;
  QuicVersionLabel version_label = 33;
  ParsedQuicVersion parsed_version = UnsupportedQuicVersion();
  QuicConnectionId destination_connection_id = TestConnectionId(1);
  QuicConnectionId source_connection_id = TestConnectionId(2);
  std::optional<absl::string_view> retry_token;
  std::string detailed_error = "foobar";
  QuicErrorCode header_parse_result = QuicFramer::ParsePublicHeaderDispatcher(
      encrypted, kQuicDefaultConnectionIdLength, &format, &long_packet_type,
      &version_present, &has_length_prefix, &version_label, &parsed_version,
      &destination_connection_id, &source_connection_id, &retry_token,
      &detailed_error);
  EXPECT_THAT(header_parse_result, IsQuicNoError());
  EXPECT_EQ(IETF_QUIC_LONG_HEADER_PACKET, format);
  EXPECT_TRUE(version_present);
  EXPECT_TRUE(has_length_prefix);
  EXPECT_EQ(0xcabadaba, version_label);
  EXPECT_EQ(expected_destination_connection_id, destination_connection_id);
  EXPECT_EQ(EmptyQuicConnectionId(), source_connection_id);
  EXPECT_EQ("", detailed_error);
}

TEST_P(QuicFramerTest, DispatcherParseClientInitialPacketNumber) {
  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  // clang-format off
  PacketFragments packet = {
      // Type (Long header, INITIAL, 2B packet number)
      {"Unable to read first byte.",
       {0xC1}},
      // Version
      {"Unable to read protocol version.",
       {QUIC_VERSION_BYTES}},
      // Length-prefixed Destination connection_id
      {"Unable to read destination connection ID.",
       {0x08, 0x56, 0x4e, 0x20, 0x70, 0x6c, 0x7a, 0x20, 0x21}},
      // Length-prefixed Source connection_id
      {"Unable to read source connection ID.",
       {0x00}},
      // Retry token
      {"",
       {0x00}},
      // Length
      {"",
       {kVarInt62TwoBytes + 0x03, 0x04}},
      // Packet number
      {"Unable to read packet number.",
       {0x00, 0x02}},
      // Packet payload (padding)
      {"",
       std::vector<uint8_t>(static_cast<size_t>(kDefaultMaxPacketSize - 20), 0)}
  };
  // clang-format on

  ReviseFirstByteByVersion(packet);
  SetDecrypterLevel(ENCRYPTION_INITIAL);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  ASSERT_EQ(encrypted->length(), kDefaultMaxPacketSize);
  PacketHeaderFormat format;
  QuicLongHeaderType long_packet_type = INVALID_PACKET_TYPE;
  bool version_flag;
  bool use_length_prefix;
  QuicVersionLabel version_label;
  std::optional<absl::string_view> retry_token;
  ParsedQuicVersion parsed_version = UnsupportedQuicVersion();
  QuicConnectionId destination_connection_id, source_connection_id;
  std::string detailed_error;
  MockConnectionIdGenerator generator;
  EXPECT_CALL(generator, ConnectionIdLength(_)).Times(0);
  EXPECT_EQ(QUIC_NO_ERROR,
            QuicFramer::ParsePublicHeaderDispatcherShortHeaderLengthUnknown(
                *encrypted, &format, &long_packet_type, &version_flag,
                &use_length_prefix, &version_label, &parsed_version,
                &destination_connection_id, &source_connection_id, &retry_token,
                &detailed_error, generator));
  EXPECT_EQ(parsed_version, version_);
  EXPECT_EQ(format, IETF_QUIC_LONG_HEADER_PACKET);
  EXPECT_EQ(destination_connection_id.length(), 8);
  EXPECT_EQ(long_packet_type, INITIAL);
  EXPECT_TRUE(version_flag);
  EXPECT_TRUE(use_length_prefix);
  EXPECT_EQ(version_label, CreateQuicVersionLabel(version_));

  EXPECT_EQ(source_connection_id.length(), 0);
  EXPECT_TRUE(retry_token.value_or("").empty());
  EXPECT_EQ(detailed_error, "");

  std::optional<uint64_t> packet_number;
  EXPECT_EQ(QUIC_NO_ERROR,
            QuicFramer::TryDecryptInitialPacketDispatcher(
                *encrypted, parsed_version, format, long_packet_type,
                destination_connection_id, source_connection_id, retry_token,
                /*largest_decrypted_inital_packet_number=*/QuicPacketNumber(),
                *decrypter_, &packet_number));
  EXPECT_THAT(packet_number, Optional(2));
}

TEST_P(QuicFramerTest,
       DispatcherParseClientInitialPacketNumberFromCoalescedPacket) {
  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_INITIAL);
  // clang-format off
  unsigned char packet[] = {
    // first coalesced packet
      // Type (Long header, INITIAL, 4B packet number)
      0xC3,
      // Version
      QUIC_VERSION_BYTES,
      // Destination connection ID length
      0x08,
      // Destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // Source connection ID length
      0x00,
      // Retry token
      0x00,
      // Packet length
      0x1E,
      // Packet number
      0x12, 0x34, 0x56, 0x78,
      // Frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      0x08 | 0x01 | 0x02 | 0x04,
      // Stream id
      kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04,
      // Offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // Data length
      kVarInt62OneByte + 0x0c,
      // Data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
    // second coalesced packet
      // Type (Long header, ZERO_RTT_PROTECTED, 4B packet number)
      0xD3,
      // Version
      QUIC_VERSION_BYTES,
      // Destination connection ID length
      0x08,
      // Destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // Source connection ID length
      0x00,
      // Packet length
      0x1E,
      // Packet number
      0x12, 0x34, 0x56, 0x79,
      // Frame type (IETF_STREAM frame with FIN, LEN, and OFFSET bits set)
      0x08 | 0x01 | 0x02 | 0x04,
      // Stream id
      kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04,
      // Offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // Data length
      kVarInt62OneByte + 0x0c,
      // Data
      'H',  'E',  'L',  'L',
      'O',  '_',  'W',  'O',
      'R',  'L',  'D',  '?',
  };
  // clang-format on
  const size_t first_packet_size = 47;
  // If the first packet changes, the attempt to fix the first byte of the
  // second packet will fail.
  ASSERT_EQ(packet[first_packet_size], 0xD3);

  ReviseFirstByteByVersion(packet);
  ReviseFirstByteByVersion(&packet[first_packet_size]);
  unsigned char* p = packet;
  size_t p_length = ABSL_ARRAYSIZE(packet);

  QuicEncryptedPacket encrypted(AsChars(p), p_length, false);
  PacketHeaderFormat format;
  QuicLongHeaderType long_packet_type = INVALID_PACKET_TYPE;
  bool version_flag;
  bool use_length_prefix;
  QuicVersionLabel version_label;
  std::optional<absl::string_view> retry_token;
  ParsedQuicVersion parsed_version = UnsupportedQuicVersion();
  QuicConnectionId destination_connection_id, source_connection_id;
  std::string detailed_error;
  MockConnectionIdGenerator generator;
  EXPECT_CALL(generator, ConnectionIdLength(_)).Times(0);
  EXPECT_EQ(QUIC_NO_ERROR,
            QuicFramer::ParsePublicHeaderDispatcherShortHeaderLengthUnknown(
                encrypted, &format, &long_packet_type, &version_flag,
                &use_length_prefix, &version_label, &parsed_version,
                &destination_connection_id, &source_connection_id, &retry_token,
                &detailed_error, generator));
  EXPECT_EQ(parsed_version, version_);
  EXPECT_EQ(format, IETF_QUIC_LONG_HEADER_PACKET);
  EXPECT_EQ(destination_connection_id.length(), 8);
  EXPECT_EQ(long_packet_type, INITIAL);
  EXPECT_TRUE(version_flag);
  EXPECT_TRUE(use_length_prefix);
  EXPECT_EQ(version_label, CreateQuicVersionLabel(version_));

  EXPECT_EQ(source_connection_id.length(), 0);
  EXPECT_TRUE(retry_token.value_or("").empty());
  EXPECT_EQ(detailed_error, "");

  std::optional<uint64_t> packet_number;
  EXPECT_EQ(QUIC_NO_ERROR,
            QuicFramer::TryDecryptInitialPacketDispatcher(
                encrypted, parsed_version, format, long_packet_type,
                destination_connection_id, source_connection_id, retry_token,
                /*largest_decrypted_inital_packet_number=*/QuicPacketNumber(),
                *decrypter_, &packet_number));
  EXPECT_THAT(packet_number, Optional(0x12345678));
}

TEST_P(QuicFramerTest, ParseServerVersionNegotiationProbeResponse) {
  // clang-format off
  const uint8_t packet[] = {
    // IETF long header with fixed bit set, type initial, all-0 encrypted bits.
    0xc0,
    // Version of 0, indicating version negotiation.
    0x00, 0x00, 0x00, 0x00,
    // Destination connection ID length 0, source connection ID length 8.
    0x00, 0x08,
    // 8-byte source connection ID.
    0x56, 0x4e, 0x20, 0x70, 0x6c, 0x7a, 0x20, 0x21,
    // A few supported versions.
    0xaa, 0xaa, 0xaa, 0xaa,
    QUIC_VERSION_BYTES,
  };
  // clang-format on
  char probe_payload_bytes[] = {0x56, 0x4e, 0x20, 0x70, 0x6c, 0x7a, 0x20, 0x21};
  char parsed_probe_payload_bytes[255] = {};
  uint8_t parsed_probe_payload_length = sizeof(parsed_probe_payload_bytes);
  std::string parse_detailed_error = "";
  EXPECT_TRUE(QuicFramer::ParseServerVersionNegotiationProbeResponse(
      reinterpret_cast<const char*>(packet), sizeof(packet),
      reinterpret_cast<char*>(parsed_probe_payload_bytes),
      &parsed_probe_payload_length, &parse_detailed_error));
  EXPECT_EQ("", parse_detailed_error);
  quiche::test::CompareCharArraysWithHexError(
      "parsed probe", parsed_probe_payload_bytes, parsed_probe_payload_length,
      probe_payload_bytes, sizeof(probe_payload_bytes));
}

TEST_P(QuicFramerTest, ParseClientVersionNegotiationProbePacket) {
  char packet[1200];
  char input_destination_connection_id_bytes[] = {0x56, 0x4e, 0x20, 0x70,
                                                  0x6c, 0x7a, 0x20, 0x21};
  ASSERT_TRUE(QuicFramer::WriteClientVersionNegotiationProbePacket(
      packet, sizeof(packet), input_destination_connection_id_bytes,
      sizeof(input_destination_connection_id_bytes)));
  char parsed_destination_connection_id_bytes[255] = {0};
  uint8_t parsed_destination_connection_id_length =
      sizeof(parsed_destination_connection_id_bytes);
  ASSERT_TRUE(ParseClientVersionNegotiationProbePacket(
      packet, sizeof(packet), parsed_destination_connection_id_bytes,
      &parsed_destination_connection_id_length));
  quiche::test::CompareCharArraysWithHexError(
      "parsed destination connection ID",
      parsed_destination_connection_id_bytes,
      parsed_destination_connection_id_length,
      input_destination_connection_id_bytes,
      sizeof(input_destination_connection_id_bytes));
}

TEST_P(QuicFramerTest, WriteServerVersionNegotiationProbeResponse) {
  char packet[1200];
  size_t packet_length = sizeof(packet);
  char input_source_connection_id_bytes[] = {0x56, 0x4e, 0x20, 0x70,
                                             0x6c, 0x7a, 0x20, 0x21};
  ASSERT_TRUE(WriteServerVersionNegotiationProbeResponse(
      packet, &packet_length, input_source_connection_id_bytes,
      sizeof(input_source_connection_id_bytes)));
  char parsed_source_connection_id_bytes[255] = {0};
  uint8_t parsed_source_connection_id_length =
      sizeof(parsed_source_connection_id_bytes);
  std::string detailed_error;
  ASSERT_TRUE(QuicFramer::ParseServerVersionNegotiationProbeResponse(
      packet, packet_length, parsed_source_connection_id_bytes,
      &parsed_source_connection_id_length, &detailed_error))
      << detailed_error;
  quiche::test::CompareCharArraysWithHexError(
      "parsed destination connection ID", parsed_source_connection_id_bytes,
      parsed_source_connection_id_length, input_source_connection_id_bytes,
      sizeof(input_source_connection_id_bytes));
}

TEST_P(QuicFramerTest, ClientConnectionIdFromLongHeaderToClient) {
  SetDecrypterLevel(ENCRYPTION_HANDSHAKE);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  // clang-format off
  unsigned char packet[] = {
    // public flags (long header with packet type HANDSHAKE and
    // 4-byte packet number)
    0xE3,
    // version
    QUIC_VERSION_BYTES,
    // connection ID lengths
    0x50,
    // destination connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // long header packet length
    0x05,
    // packet number
    0x12, 0x34, 0x56, 0x00,
    // padding frame
    0x00,
  };
  unsigned char packet49[] = {
    // public flags (long header with packet type HANDSHAKE and
    // 4-byte packet number)
    0xE3,
    // version
    QUIC_VERSION_BYTES,
    // destination connection ID length
    0x08,
    // destination connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // source connection ID length
    0x00,
    // long header packet length
    0x05,
    // packet number
    0x12, 0x34, 0x56, 0x00,
    // padding frame
    0x00,
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_length = ABSL_ARRAYSIZE(packet);
  if (framer_.version().HasLongHeaderLengths()) {
    ReviseFirstByteByVersion(packet49);
    p = packet49;
    p_length = ABSL_ARRAYSIZE(packet49);
  }
  const bool parse_success =
      framer_.ProcessPacket(QuicEncryptedPacket(AsChars(p), p_length, false));
  if (!framer_.version().AllowsVariableLengthConnectionIds()) {
    EXPECT_FALSE(parse_success);
    EXPECT_THAT(framer_.error(), IsError(QUIC_INVALID_PACKET_HEADER));
    EXPECT_EQ("Invalid ConnectionId length.", framer_.detailed_error());
    return;
  }
  EXPECT_TRUE(parse_success);
  EXPECT_THAT(framer_.error(), IsQuicNoError());
  EXPECT_EQ("", framer_.detailed_error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_.get()->destination_connection_id);
}

TEST_P(QuicFramerTest, ClientConnectionIdFromLongHeaderToServer) {
  SetDecrypterLevel(ENCRYPTION_HANDSHAKE);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // clang-format off
  unsigned char packet[] = {
    // public flags (long header with packet type HANDSHAKE and
    // 4-byte packet number)
    0xE3,
    // version
    QUIC_VERSION_BYTES,
    // connection ID lengths
    0x05,
    // source connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // long header packet length
    0x05,
    // packet number
    0x12, 0x34, 0x56, 0x00,
    // padding frame
    0x00,
  };
  unsigned char packet49[] = {
    // public flags (long header with packet type HANDSHAKE and
    // 4-byte packet number)
    0xE3,
    // version
    QUIC_VERSION_BYTES,
    // connection ID lengths
    0x00, 0x08,
    // source connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // long header packet length
    0x05,
    // packet number
    0x12, 0x34, 0x56, 0x00,
    // padding frame
    0x00,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_length = ABSL_ARRAYSIZE(packet);
  if (framer_.version().HasLongHeaderLengths()) {
    ReviseFirstByteByVersion(packet49);
    p = packet49;
    p_length = ABSL_ARRAYSIZE(packet49);
  }
  const bool parse_success =
      framer_.ProcessPacket(QuicEncryptedPacket(AsChars(p), p_length, false));
  if (!framer_.version().AllowsVariableLengthConnectionIds()) {
    EXPECT_FALSE(parse_success);
    EXPECT_THAT(framer_.error(), IsError(QUIC_INVALID_PACKET_HEADER));
    EXPECT_EQ("Invalid ConnectionId length.", framer_.detailed_error());
    return;
  }
  if (!framer_.version().SupportsClientConnectionIds()) {
    EXPECT_FALSE(parse_success);
    EXPECT_THAT(framer_.error(), IsError(QUIC_INVALID_PACKET_HEADER));
    EXPECT_EQ("Client connection ID not supported in this version.",
              framer_.detailed_error());
    return;
  }
  EXPECT_TRUE(parse_success);
  EXPECT_THAT(framer_.error(), IsQuicNoError());
  EXPECT_EQ("", framer_.detailed_error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(FramerTestConnectionId(),
            visitor_.header_.get()->source_connection_id);
}

TEST_P(QuicFramerTest, ProcessAndValidateIetfConnectionIdLengthClient) {
  char connection_id_lengths = 0x05;
  QuicDataReader reader(&connection_id_lengths, 1);

  bool should_update_expected_server_connection_id_length = false;
  uint8_t expected_server_connection_id_length = 8;
  uint8_t destination_connection_id_length = 0;
  uint8_t source_connection_id_length = 8;
  std::string detailed_error = "";

  EXPECT_TRUE(QuicFramerPeer::ProcessAndValidateIetfConnectionIdLength(
      &reader, framer_.version(), Perspective::IS_CLIENT,
      should_update_expected_server_connection_id_length,
      &expected_server_connection_id_length, &destination_connection_id_length,
      &source_connection_id_length, &detailed_error));
  EXPECT_EQ(8, expected_server_connection_id_length);
  EXPECT_EQ(0, destination_connection_id_length);
  EXPECT_EQ(8, source_connection_id_length);
  EXPECT_EQ("", detailed_error);

  QuicDataReader reader2(&connection_id_lengths, 1);
  should_update_expected_server_connection_id_length = true;
  expected_server_connection_id_length = 33;
  EXPECT_TRUE(QuicFramerPeer::ProcessAndValidateIetfConnectionIdLength(
      &reader2, framer_.version(), Perspective::IS_CLIENT,
      should_update_expected_server_connection_id_length,
      &expected_server_connection_id_length, &destination_connection_id_length,
      &source_connection_id_length, &detailed_error));
  EXPECT_EQ(8, expected_server_connection_id_length);
  EXPECT_EQ(0, destination_connection_id_length);
  EXPECT_EQ(8, source_connection_id_length);
  EXPECT_EQ("", detailed_error);
}

TEST_P(QuicFramerTest, ProcessAndValidateIetfConnectionIdLengthServer) {
  char connection_id_lengths = 0x50;
  QuicDataReader reader(&connection_id_lengths, 1);

  bool should_update_expected_server_connection_id_length = false;
  uint8_t expected_server_connection_id_length = 8;
  uint8_t destination_connection_id_length = 8;
  uint8_t source_connection_id_length = 0;
  std::string detailed_error = "";

  EXPECT_TRUE(QuicFramerPeer::ProcessAndValidateIetfConnectionIdLength(
      &reader, framer_.version(), Perspective::IS_SERVER,
      should_update_expected_server_connection_id_length,
      &expected_server_connection_id_length, &destination_connection_id_length,
      &source_connection_id_length, &detailed_error));
  EXPECT_EQ(8, expected_server_connection_id_length);
  EXPECT_EQ(8, destination_connection_id_length);
  EXPECT_EQ(0, source_connection_id_length);
  EXPECT_EQ("", detailed_error);

  QuicDataReader reader2(&connection_id_lengths, 1);
  should_update_expected_server_connection_id_length = true;
  expected_server_connection_id_length = 33;
  EXPECT_TRUE(QuicFramerPeer::ProcessAndValidateIetfConnectionIdLength(
      &reader2, framer_.version(), Perspective::IS_SERVER,
      should_update_expected_server_connection_id_length,
      &expected_server_connection_id_length, &destination_connection_id_length,
      &source_connection_id_length, &detailed_error));
  EXPECT_EQ(8, expected_server_connection_id_length);
  EXPECT_EQ(8, destination_connection_id_length);
  EXPECT_EQ(0, source_connection_id_length);
  EXPECT_EQ("", detailed_error);
}

TEST_P(QuicFramerTest, TestExtendedErrorCodeParser) {
  if (VersionHasIetfQuicFrames(framer_.transport_version())) {
    // Extended error codes only in IETF QUIC
    return;
  }
  QuicConnectionCloseFrame frame;

  frame.error_details = "this has no error code info in it";
  MaybeExtractQuicErrorCode(&frame);
  EXPECT_THAT(frame.quic_error_code, IsError(QUIC_IETF_GQUIC_ERROR_MISSING));
  EXPECT_EQ("this has no error code info in it", frame.error_details);

  frame.error_details = "1234this does not have the colon in it";
  MaybeExtractQuicErrorCode(&frame);
  EXPECT_THAT(frame.quic_error_code, IsError(QUIC_IETF_GQUIC_ERROR_MISSING));
  EXPECT_EQ("1234this does not have the colon in it", frame.error_details);

  frame.error_details = "1a234:this has a colon, but a malformed error number";
  MaybeExtractQuicErrorCode(&frame);
  EXPECT_THAT(frame.quic_error_code, IsError(QUIC_IETF_GQUIC_ERROR_MISSING));
  EXPECT_EQ("1a234:this has a colon, but a malformed error number",
            frame.error_details);

  frame.error_details = "1234:this is good";
  MaybeExtractQuicErrorCode(&frame);
  EXPECT_EQ(1234u, frame.quic_error_code);
  EXPECT_EQ("this is good", frame.error_details);

  frame.error_details =
      "1234 :this is not good, space between last digit and colon";
  MaybeExtractQuicErrorCode(&frame);
  EXPECT_THAT(frame.quic_error_code, IsError(QUIC_IETF_GQUIC_ERROR_MISSING));
  EXPECT_EQ("1234 :this is not good, space between last digit and colon",
            frame.error_details);

  frame.error_details = "123456789";
  MaybeExtractQuicErrorCode(&frame);
  EXPECT_THAT(
      frame.quic_error_code,
      IsError(QUIC_IETF_GQUIC_ERROR_MISSING));  // Not good, all numbers, no :
  EXPECT_EQ("123456789", frame.error_details);

  frame.error_details = "1234:";
  MaybeExtractQuicErrorCode(&frame);
  EXPECT_EQ(1234u,
            frame.quic_error_code);  // corner case.
  EXPECT_EQ("", frame.error_details);

  frame.error_details = "1234:5678";
  MaybeExtractQuicErrorCode(&frame);
  EXPECT_EQ(1234u,
            frame.quic_error_code);  // another corner case.
  EXPECT_EQ("5678", frame.error_details);

  frame.error_details = "12345 6789:";
  MaybeExtractQuicErrorCode(&frame);
  EXPECT_THAT(frame.quic_error_code,
              IsError(QUIC_IETF_GQUIC_ERROR_MISSING));  // Not good
  EXPECT_EQ("12345 6789:", frame.error_details);

  frame.error_details = ":no numbers, is not good";
  MaybeExtractQuicErrorCode(&frame);
  EXPECT_THAT(frame.quic_error_code, IsError(QUIC_IETF_GQUIC_ERROR_MISSING));
  EXPECT_EQ(":no numbers, is not good", frame.error_details);

  frame.error_details = "qwer:also no numbers, is not good";
  MaybeExtractQuicErrorCode(&frame);
  EXPECT_THAT(frame.quic_error_code, IsError(QUIC_IETF_GQUIC_ERROR_MISSING));
  EXPECT_EQ("qwer:also no numbers, is not good", frame.error_details);

  frame.error_details = " 1234:this is not good, space before first digit";
  MaybeExtractQuicErrorCode(&frame);
  EXPECT_THAT(frame.quic_error_code, IsError(QUIC_IETF_GQUIC_ERROR_MISSING));
  EXPECT_EQ(" 1234:this is not good, space before first digit",
            frame.error_details);

  frame.error_details = "1234:";
  MaybeExtractQuicErrorCode(&frame);
  EXPECT_EQ(1234u,
            frame.quic_error_code);  // this is good
  EXPECT_EQ("", frame.error_details);

  // Value does not fit in uint32_t.
  frame.error_details = "12345678901:";
  MaybeExtractQuicErrorCode(&frame);
  EXPECT_THAT(frame.quic_error_code, IsError(QUIC_IETF_GQUIC_ERROR_MISSING));
  EXPECT_EQ("12345678901:", frame.error_details);
}

// Regression test for crbug/1029636.
TEST_P(QuicFramerTest, OverlyLargeAckDelay) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet_ietf[] = {
    // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (IETF_ACK frame)
    0x02,
    // largest acked
    kVarInt62FourBytes + 0x12, 0x34, 0x56, 0x78,
    // ack delay time.
    kVarInt62EightBytes + 0x31, 0x00, 0x00, 0x00, 0xF3, 0xA0, 0x81, 0xE0,
    // Nr. of additional ack blocks
    kVarInt62OneByte + 0x00,
    // first ack block length.
    kVarInt62FourBytes + 0x12, 0x34, 0x56, 0x77,
  };
  // clang-format on

  framer_.ProcessPacket(QuicEncryptedPacket(
      AsChars(packet_ietf), ABSL_ARRAYSIZE(packet_ietf), false));
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  // Verify ack_delay_time is set correctly.
  EXPECT_EQ(QuicTime::Delta::Infinite(),
            visitor_.ack_frames_[0]->ack_delay_time);
}

TEST_P(QuicFramerTest, KeyUpdate) {
  if (!framer_.version().UsesTls()) {
    // Key update is only used in QUIC+TLS.
    return;
  }
  ASSERT_TRUE(framer_.version().KnowsWhichDecrypterToUse());
  // Doesn't use SetDecrypterLevel since we want to use StrictTaggingDecrypter
  // instead of TestDecrypter.
  framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<StrictTaggingDecrypter>(/*key=*/0));
  framer_.SetKeyUpdateSupportForConnection(true);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      EncryptPacketWithTagAndPhase(*data, 0, false));
  ASSERT_TRUE(encrypted);

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  // Processed valid packet with phase=0, key=1: no key update.
  EXPECT_EQ(0u, visitor_.key_update_count());
  EXPECT_EQ(0, visitor_.derive_next_key_count_);
  EXPECT_EQ(1, visitor_.decrypted_first_packet_in_key_phase_count_);

  header.packet_number += 1;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  ASSERT_TRUE(data != nullptr);
  encrypted = EncryptPacketWithTagAndPhase(*data, 1, true);
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  // Processed valid packet with phase=1, key=2: key update should have
  // occurred.
  ASSERT_EQ(1u, visitor_.key_update_count());
  EXPECT_EQ(KeyUpdateReason::kRemote, visitor_.key_update_reasons_[0]);
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(2, visitor_.decrypted_first_packet_in_key_phase_count_);

  header.packet_number += 1;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  ASSERT_TRUE(data != nullptr);
  encrypted = EncryptPacketWithTagAndPhase(*data, 1, true);
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  // Processed another valid packet with phase=1, key=2: no key update.
  EXPECT_EQ(1u, visitor_.key_update_count());
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(2, visitor_.decrypted_first_packet_in_key_phase_count_);

  // Process another key update.
  header.packet_number += 1;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  ASSERT_TRUE(data != nullptr);
  encrypted = EncryptPacketWithTagAndPhase(*data, 2, false);
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  ASSERT_EQ(2u, visitor_.key_update_count());
  EXPECT_EQ(KeyUpdateReason::kRemote, visitor_.key_update_reasons_[1]);
  EXPECT_EQ(2, visitor_.derive_next_key_count_);
  EXPECT_EQ(3, visitor_.decrypted_first_packet_in_key_phase_count_);
}

TEST_P(QuicFramerTest, KeyUpdateOldPacketAfterUpdate) {
  if (!framer_.version().UsesTls()) {
    // Key update is only used in QUIC+TLS.
    return;
  }
  ASSERT_TRUE(framer_.version().KnowsWhichDecrypterToUse());
  // Doesn't use SetDecrypterLevel since we want to use StrictTaggingDecrypter
  // instead of TestDecrypter.
  framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<StrictTaggingDecrypter>(/*key=*/0));
  framer_.SetKeyUpdateSupportForConnection(true);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  // Process packet N with phase 0.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      EncryptPacketWithTagAndPhase(*data, 0, false));
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(0u, visitor_.key_update_count());
  EXPECT_EQ(0, visitor_.derive_next_key_count_);
  EXPECT_EQ(1, visitor_.decrypted_first_packet_in_key_phase_count_);

  // Process packet N+2 with phase 1.
  header.packet_number = kPacketNumber + 2;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  ASSERT_TRUE(data != nullptr);
  encrypted = EncryptPacketWithTagAndPhase(*data, 1, true);
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(1u, visitor_.key_update_count());
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(2, visitor_.decrypted_first_packet_in_key_phase_count_);

  // Process packet N+1 with phase 0. (Receiving packet from previous phase
  // after packet from new phase was received.)
  header.packet_number = kPacketNumber + 1;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  ASSERT_TRUE(data != nullptr);
  encrypted = EncryptPacketWithTagAndPhase(*data, 0, false);
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // Packet should decrypt and key update count should not change.
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(1u, visitor_.key_update_count());
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(2, visitor_.decrypted_first_packet_in_key_phase_count_);
}

TEST_P(QuicFramerTest, KeyUpdateOldPacketAfterDiscardPreviousOneRttKeys) {
  if (!framer_.version().UsesTls()) {
    // Key update is only used in QUIC+TLS.
    return;
  }
  ASSERT_TRUE(framer_.version().KnowsWhichDecrypterToUse());
  // Doesn't use SetDecrypterLevel since we want to use StrictTaggingDecrypter
  // instead of TestDecrypter.
  framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<StrictTaggingDecrypter>(/*key=*/0));
  framer_.SetKeyUpdateSupportForConnection(true);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  // Process packet N with phase 0.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      EncryptPacketWithTagAndPhase(*data, 0, false));
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(0u, visitor_.key_update_count());
  EXPECT_EQ(0, visitor_.derive_next_key_count_);
  EXPECT_EQ(1, visitor_.decrypted_first_packet_in_key_phase_count_);

  // Process packet N+2 with phase 1.
  header.packet_number = kPacketNumber + 2;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  ASSERT_TRUE(data != nullptr);
  encrypted = EncryptPacketWithTagAndPhase(*data, 1, true);
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(1u, visitor_.key_update_count());
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(2, visitor_.decrypted_first_packet_in_key_phase_count_);

  // Discard keys for previous key phase.
  framer_.DiscardPreviousOneRttKeys();

  // Process packet N+1 with phase 0. (Receiving packet from previous phase
  // after packet from new phase was received.)
  header.packet_number = kPacketNumber + 1;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  ASSERT_TRUE(data != nullptr);
  encrypted = EncryptPacketWithTagAndPhase(*data, 0, false);
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // Packet should not decrypt and key update count should not change.
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(1u, visitor_.key_update_count());
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(2, visitor_.decrypted_first_packet_in_key_phase_count_);
}

TEST_P(QuicFramerTest, KeyUpdatePacketsOutOfOrder) {
  if (!framer_.version().UsesTls()) {
    // Key update is only used in QUIC+TLS.
    return;
  }
  ASSERT_TRUE(framer_.version().KnowsWhichDecrypterToUse());
  // Doesn't use SetDecrypterLevel since we want to use StrictTaggingDecrypter
  // instead of TestDecrypter.
  framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<StrictTaggingDecrypter>(/*key=*/0));
  framer_.SetKeyUpdateSupportForConnection(true);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  // Process packet N with phase 0.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      EncryptPacketWithTagAndPhase(*data, 0, false));
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(0u, visitor_.key_update_count());
  EXPECT_EQ(0, visitor_.derive_next_key_count_);
  EXPECT_EQ(1, visitor_.decrypted_first_packet_in_key_phase_count_);

  // Process packet N+2 with phase 1.
  header.packet_number = kPacketNumber + 2;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  ASSERT_TRUE(data != nullptr);
  encrypted = EncryptPacketWithTagAndPhase(*data, 1, true);
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(1u, visitor_.key_update_count());
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(2, visitor_.decrypted_first_packet_in_key_phase_count_);

  // Process packet N+1 with phase 1. (Receiving packet from new phase out of
  // order.)
  header.packet_number = kPacketNumber + 1;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  ASSERT_TRUE(data != nullptr);
  encrypted = EncryptPacketWithTagAndPhase(*data, 1, true);
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // Packet should decrypt and key update count should not change.
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(1u, visitor_.key_update_count());
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(2, visitor_.decrypted_first_packet_in_key_phase_count_);
}

TEST_P(QuicFramerTest, KeyUpdateWrongKey) {
  if (!framer_.version().UsesTls()) {
    // Key update is only used in QUIC+TLS.
    return;
  }
  ASSERT_TRUE(framer_.version().KnowsWhichDecrypterToUse());
  // Doesn't use SetDecrypterLevel since we want to use StrictTaggingDecrypter
  // instead of TestDecrypter.
  framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<StrictTaggingDecrypter>(/*key=*/0));
  framer_.SetKeyUpdateSupportForConnection(true);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      EncryptPacketWithTagAndPhase(*data, 0, false));
  ASSERT_TRUE(encrypted);

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  // Processed valid packet with phase=0, key=1: no key update.
  EXPECT_EQ(0u, visitor_.key_update_count());
  EXPECT_EQ(0, visitor_.derive_next_key_count_);
  EXPECT_EQ(1, visitor_.decrypted_first_packet_in_key_phase_count_);
  EXPECT_EQ(0u, framer_.PotentialPeerKeyUpdateAttemptCount());

  header.packet_number += 1;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  ASSERT_TRUE(data != nullptr);
  encrypted = EncryptPacketWithTagAndPhase(*data, 2, true);
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // Packet with phase=1 but key=3, should not process and should not cause key
  // update, but next decrypter key should have been created to attempt to
  // decode it.
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(0u, visitor_.key_update_count());
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(1, visitor_.decrypted_first_packet_in_key_phase_count_);
  EXPECT_EQ(1u, framer_.PotentialPeerKeyUpdateAttemptCount());

  header.packet_number += 1;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  ASSERT_TRUE(data != nullptr);
  encrypted = EncryptPacketWithTagAndPhase(*data, 0, true);
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // Packet with phase=1 but key=1, should not process and should not cause key
  // update.
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(0u, visitor_.key_update_count());
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(1, visitor_.decrypted_first_packet_in_key_phase_count_);
  EXPECT_EQ(2u, framer_.PotentialPeerKeyUpdateAttemptCount());

  header.packet_number += 1;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  ASSERT_TRUE(data != nullptr);
  encrypted = EncryptPacketWithTagAndPhase(*data, 1, false);
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // Packet with phase=0 but key=2, should not process and should not cause key
  // update.
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(0u, visitor_.key_update_count());
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(1, visitor_.decrypted_first_packet_in_key_phase_count_);
  EXPECT_EQ(2u, framer_.PotentialPeerKeyUpdateAttemptCount());

  header.packet_number += 1;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  ASSERT_TRUE(data != nullptr);
  encrypted = EncryptPacketWithTagAndPhase(*data, 0, false);
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // Packet with phase=0 and key=0, should process and reset
  // potential_peer_key_update_attempt_count_.
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(0u, visitor_.key_update_count());
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(1, visitor_.decrypted_first_packet_in_key_phase_count_);
  EXPECT_EQ(0u, framer_.PotentialPeerKeyUpdateAttemptCount());
}

TEST_P(QuicFramerTest, KeyUpdateReceivedWhenNotEnabled) {
  if (!framer_.version().UsesTls()) {
    // Key update is only used in QUIC+TLS.
    return;
  }
  ASSERT_TRUE(framer_.version().KnowsWhichDecrypterToUse());
  // Doesn't use SetDecrypterLevel since we want to use StrictTaggingDecrypter
  // instead of TestDecrypter.
  framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<StrictTaggingDecrypter>(/*key=*/0));

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      EncryptPacketWithTagAndPhase(*data, 1, true));
  ASSERT_TRUE(encrypted);

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // Received a packet with key phase updated even though framer hasn't had key
  // update enabled (SetNextOneRttCrypters never called). Should fail to
  // process.
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(0u, visitor_.key_update_count());
  EXPECT_EQ(0, visitor_.derive_next_key_count_);
  EXPECT_EQ(0, visitor_.decrypted_first_packet_in_key_phase_count_);
}

TEST_P(QuicFramerTest, KeyUpdateLocallyInitiated) {
  if (!framer_.version().UsesTls()) {
    // Key update is only used in QUIC+TLS.
    return;
  }
  ASSERT_TRUE(framer_.version().KnowsWhichDecrypterToUse());
  // Doesn't use SetDecrypterLevel since we want to use StrictTaggingDecrypter
  // instead of TestDecrypter.
  framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<StrictTaggingDecrypter>(/*key=*/0));
  framer_.SetKeyUpdateSupportForConnection(true);

  EXPECT_TRUE(framer_.DoKeyUpdate(KeyUpdateReason::kLocalForTests));
  // Key update count should be updated, but haven't received packet from peer
  // with new key phase.
  ASSERT_EQ(1u, visitor_.key_update_count());
  EXPECT_EQ(KeyUpdateReason::kLocalForTests, visitor_.key_update_reasons_[0]);
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(0, visitor_.decrypted_first_packet_in_key_phase_count_);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  // Process packet N with phase 1.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      EncryptPacketWithTagAndPhase(*data, 1, true));
  ASSERT_TRUE(encrypted);

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // Packet should decrypt and key update count should not change and
  // OnDecryptedFirstPacketInKeyPhase should have been called.
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(1u, visitor_.key_update_count());
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(1, visitor_.decrypted_first_packet_in_key_phase_count_);

  // Process packet N-1 with phase 0. (Receiving packet from previous phase
  // after packet from new phase was received.)
  header.packet_number = kPacketNumber - 1;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  ASSERT_TRUE(data != nullptr);
  encrypted = EncryptPacketWithTagAndPhase(*data, 0, false);
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // Packet should decrypt and key update count should not change.
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(1u, visitor_.key_update_count());
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(1, visitor_.decrypted_first_packet_in_key_phase_count_);

  // Process packet N+1 with phase 0 and key 1. This should not decrypt even
  // though it's using the previous key, since the packet number is higher than
  // a packet number received using the current key.
  header.packet_number = kPacketNumber + 1;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  ASSERT_TRUE(data != nullptr);
  encrypted = EncryptPacketWithTagAndPhase(*data, 0, false);
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // Packet should not decrypt and key update count should not change.
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(1u, visitor_.key_update_count());
  EXPECT_EQ(2, visitor_.derive_next_key_count_);
  EXPECT_EQ(1, visitor_.decrypted_first_packet_in_key_phase_count_);
}

TEST_P(QuicFramerTest, KeyUpdateLocallyInitiatedReceivedOldPacket) {
  if (!framer_.version().UsesTls()) {
    // Key update is only used in QUIC+TLS.
    return;
  }
  ASSERT_TRUE(framer_.version().KnowsWhichDecrypterToUse());
  // Doesn't use SetDecrypterLevel since we want to use StrictTaggingDecrypter
  // instead of TestDecrypter.
  framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<StrictTaggingDecrypter>(/*key=*/0));
  framer_.SetKeyUpdateSupportForConnection(true);

  EXPECT_TRUE(framer_.DoKeyUpdate(KeyUpdateReason::kLocalForTests));
  // Key update count should be updated, but haven't received packet
  // from peer with new key phase.
  EXPECT_EQ(1u, visitor_.key_update_count());
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(0, visitor_.decrypted_first_packet_in_key_phase_count_);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  // Process packet N with phase 0. (Receiving packet from previous phase
  // after locally initiated key update, but before any packet from new phase
  // was received.)
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> data = BuildDataPacket(header, frames);
  ASSERT_TRUE(data != nullptr);
  std::unique_ptr<QuicEncryptedPacket> encrypted =
      EncryptPacketWithTagAndPhase(*data, 0, false);
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // Packet should decrypt and key update count should not change and
  // OnDecryptedFirstPacketInKeyPhase should not have been called since the
  // packet was from the previous key phase.
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(1u, visitor_.key_update_count());
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(0, visitor_.decrypted_first_packet_in_key_phase_count_);

  // Process packet N+1 with phase 1.
  header.packet_number = kPacketNumber + 1;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  ASSERT_TRUE(data != nullptr);
  encrypted = EncryptPacketWithTagAndPhase(*data, 1, true);
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // Packet should decrypt and key update count should not change, but
  // OnDecryptedFirstPacketInKeyPhase should have been called.
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(1u, visitor_.key_update_count());
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(1, visitor_.decrypted_first_packet_in_key_phase_count_);

  // Process packet N+2 with phase 0 and key 1. This should not decrypt even
  // though it's using the previous key, since the packet number is higher than
  // a packet number received using the current key.
  header.packet_number = kPacketNumber + 2;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  ASSERT_TRUE(data != nullptr);
  encrypted = EncryptPacketWithTagAndPhase(*data, 0, false);
  ASSERT_TRUE(encrypted);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  // Packet should not decrypt and key update count should not change.
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(1u, visitor_.key_update_count());
  EXPECT_EQ(2, visitor_.derive_next_key_count_);
  EXPECT_EQ(1, visitor_.decrypted_first_packet_in_key_phase_count_);
}

TEST_P(QuicFramerTest, KeyUpdateOnFirstReceivedPacket) {
  if (!framer_.version().UsesTls()) {
    // Key update is only used in QUIC+TLS.
    return;
  }
  ASSERT_TRUE(framer_.version().KnowsWhichDecrypterToUse());
  // Doesn't use SetDecrypterLevel since we want to use StrictTaggingDecrypter
  // instead of TestDecrypter.
  framer_.InstallDecrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<StrictTaggingDecrypter>(/*key=*/0));
  framer_.SetKeyUpdateSupportForConnection(true);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = QuicPacketNumber(123);

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      EncryptPacketWithTagAndPhase(*data, /*tag=*/1, /*phase=*/true));
  ASSERT_TRUE(encrypted);

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  // Processed valid packet with phase=1, key=1: do key update.
  EXPECT_EQ(1u, visitor_.key_update_count());
  EXPECT_EQ(1, visitor_.derive_next_key_count_);
  EXPECT_EQ(1, visitor_.decrypted_first_packet_in_key_phase_count_);
}

TEST_P(QuicFramerTest, ErrorWhenUnexpectedFrameTypeEncountered) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version()) ||
      !QuicVersionHasLongHeaderLengths(framer_.transport_version()) ||
      !framer_.version().HasLongHeaderLengths()) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_ZERO_RTT);
  // clang-format off
  unsigned char packet[] = {
    // public flags (long header with packet type ZERO_RTT_PROTECTED and
    // 4-byte packet number)
    0xD3,
    // version
    QUIC_VERSION_BYTES,
    // destination connection ID length
    0x08,
    // destination connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // source connection ID length
    0x08,
    // source connection ID
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
    // long header packet length
    0x05,
    // packet number
    0x12, 0x34, 0x56, 0x00,
    // unexpected ietf ack frame type in 0-RTT packet
    0x02,
  };
  // clang-format on

  ReviseFirstByteByVersion(packet);
  QuicEncryptedPacket encrypted(AsChars(packet), ABSL_ARRAYSIZE(packet), false);

  EXPECT_FALSE(framer_.ProcessPacket(encrypted));

  EXPECT_THAT(framer_.error(), IsError(IETF_QUIC_PROTOCOL_VIOLATION));
  EXPECT_EQ(
      "IETF frame type IETF_ACK is unexpected at encryption level "
      "ENCRYPTION_ZERO_RTT",
      framer_.detailed_error());
}

TEST_P(QuicFramerTest, ShortHeaderWithNonDefaultConnectionIdLength) {
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);
  // clang-format off
  unsigned char packet[kMaxIncomingPacketSize + 1] = {
     // type (short header, 4 byte packet number)
    0x43,
    // connection_id
    0x28, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0x48,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  MockConnectionIdGenerator generator;
    EXPECT_CALL(generator, ConnectionIdLength(0x28)).WillOnce(Return(9));
  unsigned char* p = packet;
  size_t p_size = ABSL_ARRAYSIZE(packet);

  const size_t header_size = GetPacketHeaderSize(
      framer_.transport_version(), kPacket8ByteConnectionId + 1,
      kPacket0ByteConnectionId, !kIncludeVersion,
      !kIncludeDiversificationNonce, PACKET_4BYTE_PACKET_NUMBER,
      quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0, 0,
      quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0) + 1;
  // Add one because it's a 9 byte connection ID.

  memset(p + header_size, 0, kMaxIncomingPacketSize - header_size);

  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  PacketHeaderFormat format;
  QuicLongHeaderType long_packet_type = INVALID_PACKET_TYPE;
  bool version_flag;
  QuicConnectionId destination_connection_id, source_connection_id;
  QuicVersionLabel version_label;
  std::string detailed_error;
  bool use_length_prefix;
  std::optional<absl::string_view> retry_token;
  ParsedQuicVersion parsed_version = UnsupportedQuicVersion();
  EXPECT_EQ(QUIC_NO_ERROR,
      QuicFramer::ParsePublicHeaderDispatcherShortHeaderLengthUnknown(
          encrypted, &format, &long_packet_type, &version_flag,
          &use_length_prefix, &version_label, &parsed_version,
          &destination_connection_id, &source_connection_id, &retry_token,
          &detailed_error, generator));
  EXPECT_EQ(format, IETF_QUIC_SHORT_HEADER_PACKET);
  EXPECT_EQ(destination_connection_id.length(), 9);
  EXPECT_EQ(long_packet_type, INVALID_PACKET_TYPE);
  EXPECT_FALSE(version_flag);
  EXPECT_FALSE(use_length_prefix);
  EXPECT_EQ(version_label, 0);
  EXPECT_EQ(parsed_version, UnsupportedQuicVersion());
  EXPECT_EQ(source_connection_id.length(), 0);
  EXPECT_FALSE(retry_token.has_value());
  EXPECT_EQ(detailed_error, "");
}

TEST_P(QuicFramerTest, ReportEcnCountsIfPresent) {
  if (!VersionHasIetfQuicFrames(framer_.transport_version())) {
    return;
  }
  SetDecrypterLevel(ENCRYPTION_FORWARD_SECURE);

  QuicPacketHeader header;
  header.destination_connection_id = FramerTestConnectionId();
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  for (bool ecn_marks : { false, true }) {
    // Add some padding, because TestEncrypter doesn't add an authentication
    // tag. For a small packet, this will cause QuicFramer to fail to get a
    // header protection sample.
    QuicPaddingFrame padding_frame(kTagSize);
    // Create a packet with just an ack.
    QuicAckFrame ack_frame = InitAckFrame(5);
    if (ecn_marks) {
      ack_frame.ecn_counters = QuicEcnCounts(100, 10000, 1000000);
    } else {
      ack_frame.ecn_counters = std::nullopt;
    }
    QuicFrames frames = {QuicFrame(padding_frame), QuicFrame(&ack_frame)};
    // Build an ACK packet.
    QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
    std::unique_ptr<QuicPacket> raw_ack_packet(BuildDataPacket(header, frames));
    ASSERT_TRUE(raw_ack_packet != nullptr);
    char buffer[kMaxOutgoingPacketSize];
    size_t encrypted_length =
        framer_.EncryptPayload(ENCRYPTION_INITIAL, header.packet_number,
                               *raw_ack_packet, buffer, kMaxOutgoingPacketSize);
    ASSERT_NE(0u, encrypted_length);
    // Now make sure we can turn our ack packet back into an ack frame.
    QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
    MockFramerVisitor visitor;
    framer_.set_visitor(&visitor);
    EXPECT_CALL(visitor, OnPacket()).Times(1);
    EXPECT_CALL(visitor, OnUnauthenticatedPublicHeader(_))
        .Times(1)
        .WillOnce(Return(true));
    EXPECT_CALL(visitor, OnUnauthenticatedHeader(_))
        .Times(1)
        .WillOnce(Return(true));
    EXPECT_CALL(visitor, OnPacketHeader(_)).Times(1);
    EXPECT_CALL(visitor, OnDecryptedPacket(_, _)).Times(1);
    EXPECT_CALL(visitor, OnAckFrameStart(_, _)).Times(1).WillOnce(Return(true));
    EXPECT_CALL(visitor, OnAckRange(_, _)).Times(1).WillOnce(Return(true));
    EXPECT_CALL(visitor, OnAckFrameEnd(_, ack_frame.ecn_counters))
        .Times(1).WillOnce(Return(true));
    EXPECT_CALL(visitor, OnPacketComplete()).Times(1);
    ASSERT_TRUE(framer_.ProcessPacket(
                    QuicEncryptedPacket(buffer, encrypted_length, false)));
  }
}

}  // namespace
}  // namespace test
}  // namespace quic
