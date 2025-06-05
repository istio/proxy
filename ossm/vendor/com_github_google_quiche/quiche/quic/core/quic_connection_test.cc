// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_connection.h"

#include <errno.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/congestion_control/loss_detection_interface.h"
#include "quiche/quic/core/congestion_control/send_algorithm_interface.h"
#include "quiche/quic/core/crypto/null_decrypter.h"
#include "quiche/quic/core/crypto/null_encrypter.h"
#include "quiche/quic/core/crypto/quic_decrypter.h"
#include "quiche/quic/core/frames/quic_connection_close_frame.h"
#include "quiche/quic/core/frames/quic_new_connection_id_frame.h"
#include "quiche/quic/core/frames/quic_path_response_frame.h"
#include "quiche/quic/core/frames/quic_reset_stream_at_frame.h"
#include "quiche/quic/core/frames/quic_rst_stream_frame.h"
#include "quiche/quic/core/quic_connection_id.h"
#include "quiche/quic/core/quic_constants.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/quic/core/quic_packet_creator.h"
#include "quiche/quic/core/quic_packets.h"
#include "quiche/quic/core/quic_path_validator.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_ip_address.h"
#include "quiche/quic/platform/api/quic_ip_address_family.h"
#include "quiche/quic/platform/api/quic_logging.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/mock_clock.h"
#include "quiche/quic/test_tools/mock_connection_id_generator.h"
#include "quiche/quic/test_tools/mock_random.h"
#include "quiche/quic/test_tools/quic_coalesced_packet_peer.h"
#include "quiche/quic/test_tools/quic_config_peer.h"
#include "quiche/quic/test_tools/quic_connection_peer.h"
#include "quiche/quic/test_tools/quic_framer_peer.h"
#include "quiche/quic/test_tools/quic_packet_creator_peer.h"
#include "quiche/quic/test_tools/quic_path_validator_peer.h"
#include "quiche/quic/test_tools/quic_sent_packet_manager_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/quic/test_tools/simple_data_producer.h"
#include "quiche/quic/test_tools/simple_session_notifier.h"
#include "quiche/common/simple_buffer_allocator.h"

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::DoAll;
using testing::DoDefault;
using testing::ElementsAre;
using testing::IgnoreResult;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Ref;
using testing::Return;
using testing::SaveArg;
using testing::SetArgPointee;
using testing::StrictMock;

namespace quic {
namespace test {
namespace {

const char data1[] = "foo data";
const char data2[] = "bar data";

const bool kHasStopWaiting = true;

const int kDefaultRetransmissionTimeMs = 500;

DiversificationNonce kTestDiversificationNonce = {
    'a', 'b', 'a', 'b', 'a', 'b', 'a', 'b', 'a', 'b', 'a',
    'b', 'a', 'b', 'a', 'b', 'a', 'b', 'a', 'b', 'a', 'b',
    'a', 'b', 'a', 'b', 'a', 'b', 'a', 'b', 'a', 'b',
};

const StatelessResetToken kTestStatelessResetToken{
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f};

const QuicSocketAddress kPeerAddress =
    QuicSocketAddress(QuicIpAddress::Loopback6(),
                      /*port=*/12345);
const QuicSocketAddress kSelfAddress =
    QuicSocketAddress(QuicIpAddress::Loopback6(),
                      /*port=*/443);
const QuicSocketAddress kServerPreferredAddress = QuicSocketAddress(
    []() {
      QuicIpAddress address;
      address.FromString("2604:31c0::");
      return address;
    }(),
    /*port=*/443);

QuicStreamId GetNthClientInitiatedStreamId(int n,
                                           QuicTransportVersion version) {
  return QuicUtils::GetFirstBidirectionalStreamId(version,
                                                  Perspective::IS_CLIENT) +
         n * 2;
}

QuicLongHeaderType EncryptionlevelToLongHeaderType(EncryptionLevel level) {
  switch (level) {
    case ENCRYPTION_INITIAL:
      return INITIAL;
    case ENCRYPTION_HANDSHAKE:
      return HANDSHAKE;
    case ENCRYPTION_ZERO_RTT:
      return ZERO_RTT_PROTECTED;
    case ENCRYPTION_FORWARD_SECURE:
      QUICHE_DCHECK(false);
      return INVALID_PACKET_TYPE;
    default:
      QUICHE_DCHECK(false);
      return INVALID_PACKET_TYPE;
  }
}

// A TaggingEncrypterWithConfidentialityLimit is a TaggingEncrypter that allows
// specifying the confidentiality limit on the maximum number of packets that
// may be encrypted per key phase in TLS+QUIC.
class TaggingEncrypterWithConfidentialityLimit : public TaggingEncrypter {
 public:
  TaggingEncrypterWithConfidentialityLimit(
      uint8_t tag, QuicPacketCount confidentiality_limit)
      : TaggingEncrypter(tag), confidentiality_limit_(confidentiality_limit) {}

  QuicPacketCount GetConfidentialityLimit() const override {
    return confidentiality_limit_;
  }

 private:
  QuicPacketCount confidentiality_limit_;
};

class StrictTaggingDecrypterWithIntegrityLimit : public StrictTaggingDecrypter {
 public:
  StrictTaggingDecrypterWithIntegrityLimit(uint8_t tag,
                                           QuicPacketCount integrity_limit)
      : StrictTaggingDecrypter(tag), integrity_limit_(integrity_limit) {}

  QuicPacketCount GetIntegrityLimit() const override {
    return integrity_limit_;
  }

 private:
  QuicPacketCount integrity_limit_;
};

class TestConnectionHelper : public QuicConnectionHelperInterface {
 public:
  TestConnectionHelper(MockClock* clock, MockRandom* random_generator)
      : clock_(clock), random_generator_(random_generator) {
    clock_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
  }
  TestConnectionHelper(const TestConnectionHelper&) = delete;
  TestConnectionHelper& operator=(const TestConnectionHelper&) = delete;

  // QuicConnectionHelperInterface
  const QuicClock* GetClock() const override { return clock_; }

  QuicRandom* GetRandomGenerator() override { return random_generator_; }

  quiche::QuicheBufferAllocator* GetStreamSendBufferAllocator() override {
    return &buffer_allocator_;
  }

 private:
  MockClock* clock_;
  MockRandom* random_generator_;
  quiche::SimpleBufferAllocator buffer_allocator_;
};

class TestConnection : public QuicConnection {
 public:
  TestConnection(QuicConnectionId connection_id,
                 QuicSocketAddress initial_self_address,
                 QuicSocketAddress initial_peer_address,
                 TestConnectionHelper* helper, TestAlarmFactory* alarm_factory,
                 TestPacketWriter* writer, Perspective perspective,
                 ParsedQuicVersion version,
                 ConnectionIdGeneratorInterface& generator)
      : QuicConnection(connection_id, initial_self_address,
                       initial_peer_address, helper, alarm_factory, writer,
                       /* owns_writer= */ false, perspective,
                       SupportedVersions(version), generator),
        notifier_(nullptr) {
    writer->set_perspective(perspective);
    SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                 std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
    SetDataProducer(&producer_);
    ON_CALL(*this, OnSerializedPacket(_))
        .WillByDefault([this](SerializedPacket packet) {
          QuicConnection::OnSerializedPacket(std::move(packet));
        });
  }
  TestConnection(const TestConnection&) = delete;
  TestConnection& operator=(const TestConnection&) = delete;

  MOCK_METHOD(void, OnSerializedPacket, (SerializedPacket packet), (override));

  void OnEffectivePeerMigrationValidated(bool is_migration_linkable) override {
    QuicConnection::OnEffectivePeerMigrationValidated(is_migration_linkable);
    if (is_migration_linkable) {
      num_linkable_client_migration_++;
    } else {
      num_unlinkable_client_migration_++;
    }
  }

  uint32_t num_unlinkable_client_migration() const {
    return num_unlinkable_client_migration_;
  }

  uint32_t num_linkable_client_migration() const {
    return num_linkable_client_migration_;
  }

  void SetSendAlgorithm(SendAlgorithmInterface* send_algorithm) {
    QuicConnectionPeer::SetSendAlgorithm(this, send_algorithm);
  }

  void SetLossAlgorithm(LossDetectionInterface* loss_algorithm) {
    QuicConnectionPeer::SetLossAlgorithm(this, loss_algorithm);
  }

  void SendPacket(EncryptionLevel /*level*/, uint64_t packet_number,
                  std::unique_ptr<QuicPacket> packet,
                  HasRetransmittableData retransmittable, bool has_ack,
                  bool has_pending_frames) {
    ScopedPacketFlusher flusher(this);
    char buffer[kMaxOutgoingPacketSize];
    size_t encrypted_length =
        QuicConnectionPeer::GetFramer(this)->EncryptPayload(
            ENCRYPTION_INITIAL, QuicPacketNumber(packet_number), *packet,
            buffer, kMaxOutgoingPacketSize);
    SerializedPacket serialized_packet(
        QuicPacketNumber(packet_number), PACKET_4BYTE_PACKET_NUMBER, buffer,
        encrypted_length, has_ack, has_pending_frames);
    serialized_packet.peer_address = kPeerAddress;
    if (retransmittable == HAS_RETRANSMITTABLE_DATA) {
      serialized_packet.retransmittable_frames.push_back(
          QuicFrame(QuicPingFrame()));
    }
    OnSerializedPacket(std::move(serialized_packet));
  }

  QuicConsumedData SaveAndSendStreamData(QuicStreamId id,
                                         absl::string_view data,
                                         QuicStreamOffset offset,
                                         StreamSendingState state) {
    return SaveAndSendStreamData(id, data, offset, state, NOT_RETRANSMISSION);
  }

  QuicConsumedData SaveAndSendStreamData(QuicStreamId id,
                                         absl::string_view data,
                                         QuicStreamOffset offset,
                                         StreamSendingState state,
                                         TransmissionType transmission_type) {
    ScopedPacketFlusher flusher(this);
    producer_.SaveStreamData(id, data);
    if (notifier_ != nullptr) {
      return notifier_->WriteOrBufferData(id, data.length(), state,
                                          transmission_type);
    }
    return QuicConnection::SendStreamData(id, data.length(), offset, state);
  }

  QuicConsumedData SendStreamDataWithString(QuicStreamId id,
                                            absl::string_view data,
                                            QuicStreamOffset offset,
                                            StreamSendingState state) {
    ScopedPacketFlusher flusher(this);
    if (!QuicUtils::IsCryptoStreamId(transport_version(), id) &&
        this->encryption_level() == ENCRYPTION_INITIAL) {
      this->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
      if (perspective() == Perspective::IS_CLIENT && !IsHandshakeComplete()) {
        OnHandshakeComplete();
      }
      if (version().SupportsAntiAmplificationLimit()) {
        QuicConnectionPeer::SetAddressValidated(this);
      }
    }
    return SaveAndSendStreamData(id, data, offset, state);
  }

  QuicConsumedData SendApplicationDataAtLevel(EncryptionLevel encryption_level,
                                              QuicStreamId id,
                                              absl::string_view data,
                                              QuicStreamOffset offset,
                                              StreamSendingState state) {
    ScopedPacketFlusher flusher(this);
    QUICHE_DCHECK(encryption_level >= ENCRYPTION_ZERO_RTT);
    SetEncrypter(encryption_level,
                 std::make_unique<TaggingEncrypter>(encryption_level));
    SetDefaultEncryptionLevel(encryption_level);
    return SaveAndSendStreamData(id, data, offset, state);
  }

  QuicConsumedData SendStreamData3() {
    return SendStreamDataWithString(
        GetNthClientInitiatedStreamId(1, transport_version()), "food", 0,
        NO_FIN);
  }

  QuicConsumedData SendStreamData5() {
    return SendStreamDataWithString(
        GetNthClientInitiatedStreamId(2, transport_version()), "food2", 0,
        NO_FIN);
  }

  // Ensures the connection can write stream data before writing.
  QuicConsumedData EnsureWritableAndSendStreamData5() {
    EXPECT_TRUE(CanWrite(HAS_RETRANSMITTABLE_DATA));
    return SendStreamData5();
  }

  // The crypto stream has special semantics so that it is not blocked by a
  // congestion window limitation, and also so that it gets put into a separate
  // packet (so that it is easier to reason about a crypto frame not being
  // split needlessly across packet boundaries).  As a result, we have separate
  // tests for some cases for this stream.
  QuicConsumedData SendCryptoStreamData() {
    return SendCryptoStreamDataAtLevel(ENCRYPTION_INITIAL);
  }

  QuicConsumedData SendCryptoStreamDataAtLevel(
      EncryptionLevel encryption_level) {
    QuicStreamOffset offset = 0;
    absl::string_view data("chlo");
    if (!QuicVersionUsesCryptoFrames(transport_version())) {
      return SendCryptoDataWithString(data, offset);
    }
    producer_.SaveCryptoData(encryption_level, offset, data);
    size_t bytes_written;
    if (notifier_) {
      bytes_written =
          notifier_->WriteCryptoData(encryption_level, data.length(), offset);
    } else {
      bytes_written = QuicConnection::SendCryptoData(encryption_level,
                                                     data.length(), offset);
    }
    return QuicConsumedData(bytes_written, /*fin_consumed*/ false);
  }

  QuicConsumedData SendCryptoDataWithString(absl::string_view data,
                                            QuicStreamOffset offset) {
    return SendCryptoDataWithString(data, offset, ENCRYPTION_INITIAL);
  }

  QuicConsumedData SendCryptoDataWithString(absl::string_view data,
                                            QuicStreamOffset offset,
                                            EncryptionLevel encryption_level) {
    if (!QuicVersionUsesCryptoFrames(transport_version())) {
      return SendStreamDataWithString(
          QuicUtils::GetCryptoStreamId(transport_version()), data, offset,
          NO_FIN);
    }
    producer_.SaveCryptoData(encryption_level, offset, data);
    size_t bytes_written;
    if (notifier_) {
      bytes_written =
          notifier_->WriteCryptoData(encryption_level, data.length(), offset);
    } else {
      bytes_written = QuicConnection::SendCryptoData(encryption_level,
                                                     data.length(), offset);
    }
    return QuicConsumedData(bytes_written, /*fin_consumed*/ false);
  }

  void set_version(ParsedQuicVersion version) {
    QuicConnectionPeer::GetFramer(this)->set_version(version);
  }

  void SetSupportedVersions(const ParsedQuicVersionVector& versions) {
    QuicConnectionPeer::GetFramer(this)->SetSupportedVersions(versions);
    writer()->SetSupportedVersions(versions);
  }

  // This should be called before setting customized encrypters/decrypters for
  // connection and peer creator.
  void set_perspective(Perspective perspective) {
    writer()->set_perspective(perspective);
    QuicConnectionPeer::ResetPeerIssuedConnectionIdManager(this);
    QuicConnectionPeer::SetPerspective(this, perspective);
    QuicSentPacketManagerPeer::SetPerspective(
        QuicConnectionPeer::GetSentPacketManager(this), perspective);
    QuicConnectionPeer::GetFramer(this)->SetInitialObfuscators(
        TestConnectionId());
  }

  // Enable path MTU discovery.  Assumes that the test is performed from the
  // server perspective and the higher value of MTU target is used.
  void EnablePathMtuDiscovery(MockSendAlgorithm* send_algorithm) {
    ASSERT_EQ(Perspective::IS_SERVER, perspective());

    if (GetQuicReloadableFlag(quic_enable_mtu_discovery_at_server)) {
      OnConfigNegotiated();
    } else {
      QuicConfig config;
      QuicTagVector connection_options;
      connection_options.push_back(kMTUH);
      config.SetInitialReceivedConnectionOptions(connection_options);
      EXPECT_CALL(*send_algorithm, SetFromConfig(_, _));
      EXPECT_CALL(*send_algorithm, EnableECT1()).WillOnce(Return(false));
      EXPECT_CALL(*send_algorithm, EnableECT0()).WillOnce(Return(false));
      SetFromConfig(config);
    }

    // Normally, the pacing would be disabled in the test, but calling
    // SetFromConfig enables it.  Set nearly-infinite bandwidth to make the
    // pacing algorithm work.
    EXPECT_CALL(*send_algorithm, PacingRate(_))
        .WillRepeatedly(Return(QuicBandwidth::Infinite()));
  }

  QuicTestAlarmProxy GetAckAlarm() {
    return QuicTestAlarmProxy(QuicConnectionPeer::GetAckAlarm(this));
  }

  QuicTestAlarmProxy GetPingAlarm() {
    return QuicTestAlarmProxy(QuicConnectionPeer::GetPingAlarm(this));
  }

  QuicTestAlarmProxy GetRetransmissionAlarm() {
    return QuicTestAlarmProxy(QuicConnectionPeer::GetRetransmissionAlarm(this));
  }

  QuicTestAlarmProxy GetSendAlarm() {
    return QuicTestAlarmProxy(QuicConnectionPeer::GetSendAlarm(this));
  }

  QuicTestAlarmProxy GetTimeoutAlarm() {
    return QuicTestAlarmProxy(
        QuicConnectionPeer::GetIdleNetworkDetectorAlarm(this));
  }

  QuicTestAlarmProxy GetMtuDiscoveryAlarm() {
    return QuicTestAlarmProxy(QuicConnectionPeer::GetMtuDiscoveryAlarm(this));
  }

  QuicTestAlarmProxy GetProcessUndecryptablePacketsAlarm() {
    return QuicTestAlarmProxy(
        QuicConnectionPeer::GetProcessUndecryptablePacketsAlarm(this));
  }

  QuicTestAlarmProxy GetDiscardPreviousOneRttKeysAlarm() {
    return QuicTestAlarmProxy(
        QuicConnectionPeer::GetDiscardPreviousOneRttKeysAlarm(this));
  }

  QuicTestAlarmProxy GetDiscardZeroRttDecryptionKeysAlarm() {
    return QuicTestAlarmProxy(
        QuicConnectionPeer::GetDiscardZeroRttDecryptionKeysAlarm(this));
  }

  QuicTestAlarmProxy GetBlackholeDetectorAlarm() {
    return QuicTestAlarmProxy(
        QuicConnectionPeer::GetBlackholeDetectorAlarm(this));
  }

  TestAlarmFactory::TestAlarm* GetRetirePeerIssuedConnectionIdAlarm() {
    return reinterpret_cast<TestAlarmFactory::TestAlarm*>(
        QuicConnectionPeer::GetRetirePeerIssuedConnectionIdAlarm(this));
  }

  TestAlarmFactory::TestAlarm* GetRetireSelfIssuedConnectionIdAlarm() {
    return reinterpret_cast<TestAlarmFactory::TestAlarm*>(
        QuicConnectionPeer::GetRetireSelfIssuedConnectionIdAlarm(this));
  }

  QuicTestAlarmProxy GetMultiPortProbingAlarm() {
    return QuicTestAlarmProxy(
        QuicConnectionPeer::GetMultiPortProbingAlarm(this));
  }

  void PathDegradingTimeout() {
    QUICHE_DCHECK(PathDegradingDetectionInProgress());
    GetBlackholeDetectorAlarm()->Fire();
  }

  bool PathDegradingDetectionInProgress() {
    return QuicConnectionPeer::GetPathDegradingDeadline(this).IsInitialized();
  }

  bool BlackholeDetectionInProgress() {
    return QuicConnectionPeer::GetBlackholeDetectionDeadline(this)
        .IsInitialized();
  }

  bool PathMtuReductionDetectionInProgress() {
    return QuicConnectionPeer::GetPathMtuReductionDetectionDeadline(this)
        .IsInitialized();
  }

  QuicByteCount GetBytesInFlight() {
    return QuicConnectionPeer::GetSentPacketManager(this)->GetBytesInFlight();
  }

  void set_notifier(SimpleSessionNotifier* notifier) { notifier_ = notifier; }

  void ReturnEffectivePeerAddressForNextPacket(const QuicSocketAddress& addr) {
    next_effective_peer_addr_ = std::make_unique<QuicSocketAddress>(addr);
  }

  void SendOrQueuePacket(SerializedPacket packet) override {
    QuicConnection::SendOrQueuePacket(std::move(packet));
    self_address_on_default_path_while_sending_packet_ = self_address();
  }

  QuicSocketAddress self_address_on_default_path_while_sending_packet() {
    return self_address_on_default_path_while_sending_packet_;
  }

  SimpleDataProducer* producer() { return &producer_; }

  using QuicConnection::active_effective_peer_migration_type;
  using QuicConnection::IsCurrentPacketConnectivityProbing;
  using QuicConnection::SelectMutualVersion;
  using QuicConnection::set_defer_send_in_response_to_packets;

 protected:
  QuicSocketAddress GetEffectivePeerAddressFromCurrentPacket() const override {
    if (next_effective_peer_addr_) {
      return *std::move(next_effective_peer_addr_);
    }
    return QuicConnection::GetEffectivePeerAddressFromCurrentPacket();
  }

 private:
  TestPacketWriter* writer() {
    return static_cast<TestPacketWriter*>(QuicConnection::writer());
  }

  SimpleDataProducer producer_;

  SimpleSessionNotifier* notifier_;

  std::unique_ptr<QuicSocketAddress> next_effective_peer_addr_;

  QuicSocketAddress self_address_on_default_path_while_sending_packet_;

  uint32_t num_unlinkable_client_migration_ = 0;

  uint32_t num_linkable_client_migration_ = 0;
};

enum class AckResponse { kDefer, kImmediate };

// Run tests with combinations of {ParsedQuicVersion, AckResponse}.
struct TestParams {
  TestParams(ParsedQuicVersion version, AckResponse ack_response)
      : version(version), ack_response(ack_response) {}

  ParsedQuicVersion version;
  AckResponse ack_response;
};

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const TestParams& p) {
  return absl::StrCat(
      ParsedQuicVersionToString(p.version), "_",
      (p.ack_response == AckResponse::kDefer ? "defer" : "immediate"));
}

// Constructs various test permutations.
std::vector<TestParams> GetTestParams() {
  QuicFlagSaver flags;
  std::vector<TestParams> params;
  ParsedQuicVersionVector all_supported_versions = AllSupportedVersions();
  for (size_t i = 0; i < all_supported_versions.size(); ++i) {
    for (AckResponse ack_response :
         {AckResponse::kDefer, AckResponse::kImmediate}) {
      params.push_back(TestParams(all_supported_versions[i], ack_response));
    }
  }
  return params;
}

class QuicConnectionTest : public QuicTestWithParam<TestParams> {
 public:
  // For tests that do silent connection closes, no such packet is generated. In
  // order to verify the contents of the OnConnectionClosed upcall, EXPECTs
  // should invoke this method, saving the frame, and then the test can verify
  // the contents.
  void SaveConnectionCloseFrame(const QuicConnectionCloseFrame& frame,
                                ConnectionCloseSource /*source*/) {
    saved_connection_close_frame_ = frame;
    connection_close_frame_count_++;
  }

 protected:
  QuicConnectionTest()
      : connection_id_(TestConnectionId()),
        framer_(SupportedVersions(version()), QuicTime::Zero(),
                Perspective::IS_CLIENT, connection_id_.length()),
        send_algorithm_(new StrictMock<MockSendAlgorithm>),
        loss_algorithm_(new MockLossAlgorithm()),
        helper_(new TestConnectionHelper(&clock_, &random_generator_)),
        alarm_factory_(new TestAlarmFactory()),
        peer_framer_(SupportedVersions(version()), QuicTime::Zero(),
                     Perspective::IS_SERVER, connection_id_.length()),
        peer_creator_(connection_id_, &peer_framer_,
                      /*delegate=*/nullptr),
        writer_(
            new TestPacketWriter(version(), &clock_, Perspective::IS_CLIENT)),
        connection_(connection_id_, kSelfAddress, kPeerAddress, helper_.get(),
                    alarm_factory_.get(), writer_.get(), Perspective::IS_CLIENT,
                    version(), connection_id_generator_),
        creator_(QuicConnectionPeer::GetPacketCreator(&connection_)),
        manager_(QuicConnectionPeer::GetSentPacketManager(&connection_)),
        frame1_(0, false, 0, absl::string_view(data1)),
        frame2_(0, false, 3, absl::string_view(data2)),
        crypto_frame_(ENCRYPTION_INITIAL, 0, absl::string_view(data1)),
        packet_number_length_(PACKET_4BYTE_PACKET_NUMBER),
        connection_id_included_(CONNECTION_ID_PRESENT),
        notifier_(&connection_),
        connection_close_frame_count_(0) {
    QUIC_DVLOG(2) << "QuicConnectionTest(" << PrintToString(GetParam()) << ")";
    connection_.set_defer_send_in_response_to_packets(GetParam().ack_response ==
                                                      AckResponse::kDefer);
    framer_.SetInitialObfuscators(TestConnectionId());
    connection_.InstallInitialCrypters(TestConnectionId());
    CrypterPair crypters;
    CryptoUtils::CreateInitialObfuscators(Perspective::IS_SERVER, version(),
                                          TestConnectionId(), &crypters);
    peer_creator_.SetEncrypter(ENCRYPTION_INITIAL,
                               std::move(crypters.encrypter));
    if (version().KnowsWhichDecrypterToUse()) {
      peer_framer_.InstallDecrypter(ENCRYPTION_INITIAL,
                                    std::move(crypters.decrypter));
    } else {
      peer_framer_.SetDecrypter(ENCRYPTION_INITIAL,
                                std::move(crypters.decrypter));
    }
    for (EncryptionLevel level :
         {ENCRYPTION_ZERO_RTT, ENCRYPTION_FORWARD_SECURE}) {
      peer_creator_.SetEncrypter(level,
                                 std::make_unique<TaggingEncrypter>(level));
    }
    QuicFramerPeer::SetLastSerializedServerConnectionId(
        QuicConnectionPeer::GetFramer(&connection_), connection_id_);
    QuicFramerPeer::SetLastWrittenPacketNumberLength(
        QuicConnectionPeer::GetFramer(&connection_), packet_number_length_);
    QuicStreamId stream_id;
    if (QuicVersionUsesCryptoFrames(version().transport_version)) {
      stream_id = QuicUtils::GetFirstBidirectionalStreamId(
          version().transport_version, Perspective::IS_CLIENT);
    } else {
      stream_id = QuicUtils::GetCryptoStreamId(version().transport_version);
    }
    frame1_.stream_id = stream_id;
    frame2_.stream_id = stream_id;
    connection_.set_visitor(&visitor_);
    connection_.SetSessionNotifier(&notifier_);
    connection_.set_notifier(&notifier_);
    connection_.SetSendAlgorithm(send_algorithm_);
    connection_.SetLossAlgorithm(loss_algorithm_.get());
    EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, OnPacketNeutered(_)).Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
        .WillRepeatedly(Return(kDefaultTCPMSS));
    EXPECT_CALL(*send_algorithm_, PacingRate(_))
        .WillRepeatedly(Return(QuicBandwidth::Zero()));
    EXPECT_CALL(*send_algorithm_, BandwidthEstimate())
        .Times(AnyNumber())
        .WillRepeatedly(Return(QuicBandwidth::Zero()));
    EXPECT_CALL(*send_algorithm_, PopulateConnectionStats(_))
        .Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, InSlowStart()).Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, InRecovery()).Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, GetCongestionControlType())
        .Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, GetCongestionControlType())
        .Times(AnyNumber());
    EXPECT_CALL(visitor_, WillingAndAbleToWrite())
        .WillRepeatedly(
            Invoke(&notifier_, &SimpleSessionNotifier::WillingToWrite));
    EXPECT_CALL(visitor_, OnPacketDecrypted(_)).Times(AnyNumber());
    EXPECT_CALL(visitor_, OnCanWrite())
        .WillRepeatedly(Invoke(&notifier_, &SimpleSessionNotifier::OnCanWrite));
    EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(visitor_, OnCongestionWindowChange(_)).Times(AnyNumber());
    EXPECT_CALL(visitor_, OnPacketReceived(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_)).Times(AnyNumber());
    EXPECT_CALL(visitor_, MaybeBundleOpportunistically()).Times(AnyNumber());
    EXPECT_CALL(visitor_, GetFlowControlSendWindowSize(_)).Times(AnyNumber());
    EXPECT_CALL(visitor_, OnOneRttPacketAcknowledged())
        .Times(testing::AtMost(1));
    EXPECT_CALL(*loss_algorithm_, GetLossTimeout())
        .WillRepeatedly(Return(QuicTime::Zero()));
    EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
        .Times(AnyNumber());
    EXPECT_CALL(visitor_, GetHandshakeState())
        .WillRepeatedly(Return(HANDSHAKE_START));
    if (connection_.version().KnowsWhichDecrypterToUse()) {
      connection_.InstallDecrypter(
          ENCRYPTION_FORWARD_SECURE,
          std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_FORWARD_SECURE));
    } else {
      connection_.SetAlternativeDecrypter(
          ENCRYPTION_FORWARD_SECURE,
          std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_FORWARD_SECURE),
          false);
    }
    peer_creator_.SetDefaultPeerAddress(kSelfAddress);
  }

  QuicConnectionTest(const QuicConnectionTest&) = delete;
  QuicConnectionTest& operator=(const QuicConnectionTest&) = delete;

  ParsedQuicVersion version() { return GetParam().version; }

  void SetClientConnectionId(const QuicConnectionId& client_connection_id) {
    connection_.set_client_connection_id(client_connection_id);
    writer_->framer()->framer()->SetExpectedClientConnectionIdLength(
        client_connection_id.length());
  }

  void SetDecrypter(EncryptionLevel level,
                    std::unique_ptr<QuicDecrypter> decrypter) {
    if (connection_.version().KnowsWhichDecrypterToUse()) {
      connection_.InstallDecrypter(level, std::move(decrypter));
    } else {
      connection_.SetAlternativeDecrypter(level, std::move(decrypter), false);
    }
  }

  void ProcessPacket(uint64_t number) {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacket(number);
    if (connection_.GetSendAlarm()->IsSet()) {
      connection_.GetSendAlarm()->Fire();
    }
  }

  void ProcessReceivedPacket(const QuicSocketAddress& self_address,
                             const QuicSocketAddress& peer_address,
                             const QuicReceivedPacket& packet) {
    connection_.ProcessUdpPacket(self_address, peer_address, packet);
    if (connection_.GetSendAlarm()->IsSet()) {
      connection_.GetSendAlarm()->Fire();
    }
  }

  QuicFrame MakeCryptoFrame() const {
    if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
      return QuicFrame(new QuicCryptoFrame(crypto_frame_));
    }
    return QuicFrame(QuicStreamFrame(
        QuicUtils::GetCryptoStreamId(connection_.transport_version()), false,
        0u, absl::string_view()));
  }

  void ProcessFramePacket(QuicFrame frame) {
    ProcessFramePacketWithAddresses(frame, kSelfAddress, kPeerAddress,
                                    ENCRYPTION_FORWARD_SECURE);
  }

  void ProcessFramePacketWithAddresses(QuicFrame frame,
                                       QuicSocketAddress self_address,
                                       QuicSocketAddress peer_address,
                                       EncryptionLevel level) {
    QuicFrames frames;
    frames.push_back(QuicFrame(frame));
    return ProcessFramesPacketWithAddresses(frames, self_address, peer_address,
                                            level);
  }

  std::unique_ptr<QuicReceivedPacket> ConstructPacket(QuicFrames frames,
                                                      EncryptionLevel level,
                                                      char* buffer,
                                                      size_t buffer_len) {
    QUICHE_DCHECK(peer_framer_.HasEncrypterOfEncryptionLevel(level));
    peer_creator_.set_encryption_level(level);
    QuicPacketCreatorPeer::SetSendVersionInPacket(
        &peer_creator_,
        level < ENCRYPTION_FORWARD_SECURE &&
            connection_.perspective() == Perspective::IS_SERVER);

    SerializedPacket serialized_packet =
        QuicPacketCreatorPeer::SerializeAllFrames(&peer_creator_, frames,
                                                  buffer, buffer_len);
    return std::make_unique<QuicReceivedPacket>(
        serialized_packet.encrypted_buffer, serialized_packet.encrypted_length,
        clock_.Now());
  }

  void ProcessFramesPacketWithAddresses(QuicFrames frames,
                                        QuicSocketAddress self_address,
                                        QuicSocketAddress peer_address,
                                        EncryptionLevel level) {
    char buffer[kMaxOutgoingPacketSize];
    connection_.ProcessUdpPacket(
        self_address, peer_address,
        *ConstructPacket(std::move(frames), level, buffer,
                         kMaxOutgoingPacketSize));
    if (connection_.GetSendAlarm()->IsSet()) {
      connection_.GetSendAlarm()->Fire();
    }
  }

  // Bypassing the packet creator is unrealistic, but allows us to process
  // packets the QuicPacketCreator won't allow us to create.
  void ForceProcessFramePacket(QuicFrame frame) {
    QuicFrames frames;
    frames.push_back(QuicFrame(frame));
    bool send_version = connection_.perspective() == Perspective::IS_SERVER;
    if (connection_.version().KnowsWhichDecrypterToUse()) {
      send_version = true;
    }
    QuicPacketCreatorPeer::SetSendVersionInPacket(&peer_creator_, send_version);
    QuicPacketHeader header;
    QuicPacketCreatorPeer::FillPacketHeader(&peer_creator_, &header);
    char encrypted_buffer[kMaxOutgoingPacketSize];
    size_t length = peer_framer_.BuildDataPacket(
        header, frames, encrypted_buffer, kMaxOutgoingPacketSize,
        ENCRYPTION_INITIAL);
    QUICHE_DCHECK_GT(length, 0u);

    const size_t encrypted_length = peer_framer_.EncryptInPlace(
        ENCRYPTION_INITIAL, header.packet_number,
        GetStartOfEncryptedData(peer_framer_.version().transport_version,
                                header),
        length, kMaxOutgoingPacketSize, encrypted_buffer);
    QUICHE_DCHECK_GT(encrypted_length, 0u);

    connection_.ProcessUdpPacket(
        kSelfAddress, kPeerAddress,
        QuicReceivedPacket(encrypted_buffer, encrypted_length, clock_.Now()));
  }

  size_t ProcessFramePacketAtLevel(uint64_t number, QuicFrame frame,
                                   EncryptionLevel level) {
    return ProcessFramePacketAtLevelWithEcn(number, frame, level, ECN_NOT_ECT);
  }

  size_t ProcessFramePacketAtLevelWithEcn(uint64_t number, QuicFrame frame,
                                          EncryptionLevel level,
                                          QuicEcnCodepoint ecn_codepoint) {
    QuicFrames frames;
    frames.push_back(frame);
    return ProcessFramesPacketAtLevelWithEcn(number, frames, level,
                                             ecn_codepoint);
  }

  size_t ProcessFramesPacketAtLevel(uint64_t number, QuicFrames frames,
                                    EncryptionLevel level) {
    return ProcessFramesPacketAtLevelWithEcn(number, frames, level,
                                             ECN_NOT_ECT);
  }

  size_t ProcessFramesPacketAtLevelWithEcn(uint64_t number,
                                           const QuicFrames& frames,
                                           EncryptionLevel level,
                                           QuicEcnCodepoint ecn_codepoint) {
    QuicPacketHeader header = ConstructPacketHeader(number, level);
    // Set the correct encryption level and encrypter on peer_creator and
    // peer_framer, respectively.
    peer_creator_.set_encryption_level(level);
    if (level > ENCRYPTION_INITIAL) {
      peer_framer_.SetEncrypter(level,
                                std::make_unique<TaggingEncrypter>(level));
      // Set the corresponding decrypter.
      if (connection_.version().KnowsWhichDecrypterToUse()) {
        connection_.InstallDecrypter(
            level, std::make_unique<StrictTaggingDecrypter>(level));
      } else {
        connection_.SetAlternativeDecrypter(
            level, std::make_unique<StrictTaggingDecrypter>(level), false);
      }
    }
    std::unique_ptr<QuicPacket> packet(ConstructPacket(header, frames));

    char buffer[kMaxOutgoingPacketSize];
    size_t encrypted_length =
        peer_framer_.EncryptPayload(level, QuicPacketNumber(number), *packet,
                                    buffer, kMaxOutgoingPacketSize);
    connection_.ProcessUdpPacket(
        kSelfAddress, kPeerAddress,
        QuicReceivedPacket(buffer, encrypted_length, clock_.Now(), false, 0,
                           true, nullptr, 0, false, ecn_codepoint));
    if (connection_.GetSendAlarm()->IsSet()) {
      connection_.GetSendAlarm()->Fire();
    }
    return encrypted_length;
  }

  struct PacketInfo {
    PacketInfo(uint64_t packet_number, QuicFrames frames, EncryptionLevel level)
        : packet_number(packet_number), frames(frames), level(level) {}

    uint64_t packet_number;
    QuicFrames frames;
    EncryptionLevel level;
  };

  size_t ProcessCoalescedPacket(std::vector<PacketInfo> packets) {
    return ProcessCoalescedPacket(packets, ECN_NOT_ECT);
  }

  size_t ProcessCoalescedPacket(std::vector<PacketInfo> packets,
                                QuicEcnCodepoint ecn_codepoint) {
    char coalesced_buffer[kMaxOutgoingPacketSize];
    size_t coalesced_size = 0;
    bool contains_initial = false;
    for (const auto& packet : packets) {
      QuicPacketHeader header =
          ConstructPacketHeader(packet.packet_number, packet.level);
      // Set the correct encryption level and encrypter on peer_creator and
      // peer_framer, respectively.
      peer_creator_.set_encryption_level(packet.level);
      if (packet.level == ENCRYPTION_INITIAL) {
        contains_initial = true;
      }
      EncryptionLevel level =
          QuicPacketCreatorPeer::GetEncryptionLevel(&peer_creator_);
      if (level > ENCRYPTION_INITIAL) {
        peer_framer_.SetEncrypter(level,
                                  std::make_unique<TaggingEncrypter>(level));
        // Set the corresponding decrypter.
        if (connection_.version().KnowsWhichDecrypterToUse()) {
          connection_.InstallDecrypter(
              level, std::make_unique<StrictTaggingDecrypter>(level));
        } else {
          connection_.SetDecrypter(
              level, std::make_unique<StrictTaggingDecrypter>(level));
        }
      }
      std::unique_ptr<QuicPacket> constructed_packet(
          ConstructPacket(header, packet.frames));

      char buffer[kMaxOutgoingPacketSize];
      size_t encrypted_length = peer_framer_.EncryptPayload(
          packet.level, QuicPacketNumber(packet.packet_number),
          *constructed_packet, buffer, kMaxOutgoingPacketSize);
      QUICHE_DCHECK_LE(coalesced_size + encrypted_length,
                       kMaxOutgoingPacketSize);
      memcpy(coalesced_buffer + coalesced_size, buffer, encrypted_length);
      coalesced_size += encrypted_length;
    }
    if (contains_initial) {
      // Padded coalesced packet to full if it contains initial packet.
      memset(coalesced_buffer + coalesced_size, '0',
             kMaxOutgoingPacketSize - coalesced_size);
    }
    connection_.ProcessUdpPacket(
        kSelfAddress, kPeerAddress,
        QuicReceivedPacket(coalesced_buffer, coalesced_size, clock_.Now(),
                           false, 0, true, nullptr, 0, false, ecn_codepoint));
    if (connection_.GetSendAlarm()->IsSet()) {
      connection_.GetSendAlarm()->Fire();
    }
    return coalesced_size;
  }

  size_t ProcessDataPacket(uint64_t number) {
    return ProcessDataPacketAtLevel(number, false, ENCRYPTION_FORWARD_SECURE);
  }

  size_t ProcessDataPacket(QuicPacketNumber packet_number) {
    return ProcessDataPacketAtLevel(packet_number, false,
                                    ENCRYPTION_FORWARD_SECURE);
  }

  size_t ProcessDataPacketAtLevel(QuicPacketNumber packet_number,
                                  bool has_stop_waiting,
                                  EncryptionLevel level) {
    return ProcessDataPacketAtLevel(packet_number.ToUint64(), has_stop_waiting,
                                    level);
  }

  size_t ProcessDataPacketAtLevel(uint64_t number, bool has_stop_waiting,
                                  EncryptionLevel level) {
    return ProcessDataPacketAtLevel(number, has_stop_waiting, level, 0);
  }

  size_t ProcessCryptoPacketAtLevel(uint64_t number, EncryptionLevel level) {
    QuicPacketHeader header = ConstructPacketHeader(number, level);
    QuicFrames frames;
    if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
      frames.push_back(QuicFrame(&crypto_frame_));
    } else {
      frames.push_back(QuicFrame(frame1_));
    }
    if (level == ENCRYPTION_INITIAL) {
      frames.push_back(QuicFrame(QuicPaddingFrame(-1)));
    }
    std::unique_ptr<QuicPacket> packet = ConstructPacket(header, frames);
    char buffer[kMaxOutgoingPacketSize];
    peer_creator_.set_encryption_level(level);
    size_t encrypted_length =
        peer_framer_.EncryptPayload(level, QuicPacketNumber(number), *packet,
                                    buffer, kMaxOutgoingPacketSize);
    connection_.ProcessUdpPacket(
        kSelfAddress, kPeerAddress,
        QuicReceivedPacket(buffer, encrypted_length, clock_.Now(), false));
    if (connection_.GetSendAlarm()->IsSet()) {
      connection_.GetSendAlarm()->Fire();
    }
    return encrypted_length;
  }

  size_t ProcessDataPacketAtLevel(uint64_t number, bool has_stop_waiting,
                                  EncryptionLevel level, uint32_t flow_label) {
    std::unique_ptr<QuicPacket> packet(
        ConstructDataPacket(number, has_stop_waiting, level));
    char buffer[kMaxOutgoingPacketSize];
    peer_creator_.set_encryption_level(level);
    size_t encrypted_length =
        peer_framer_.EncryptPayload(level, QuicPacketNumber(number), *packet,
                                    buffer, kMaxOutgoingPacketSize);
    connection_.ProcessUdpPacket(
        kSelfAddress, kPeerAddress,
        QuicReceivedPacket(buffer, encrypted_length, clock_.Now(), false,
                           0 /* ttl */, true /* ttl_valid */,
                           nullptr /* packet_headers */, 0 /* headers_length */,
                           false /* owns_header_buffer */, ECN_NOT_ECT,
                           /*tos=*/std::nullopt, flow_label));

    if (connection_.GetSendAlarm()->IsSet()) {
      connection_.GetSendAlarm()->Fire();
    }
    return encrypted_length;
  }

  void ProcessClosePacket(uint64_t number) {
    std::unique_ptr<QuicPacket> packet(ConstructClosePacket(number));
    char buffer[kMaxOutgoingPacketSize];
    size_t encrypted_length = peer_framer_.EncryptPayload(
        ENCRYPTION_FORWARD_SECURE, QuicPacketNumber(number), *packet, buffer,
        kMaxOutgoingPacketSize);
    connection_.ProcessUdpPacket(
        kSelfAddress, kPeerAddress,
        QuicReceivedPacket(buffer, encrypted_length, QuicTime::Zero(), false));
  }

  QuicByteCount SendStreamDataToPeer(QuicStreamId id, absl::string_view data,
                                     QuicStreamOffset offset,
                                     StreamSendingState state,
                                     QuicPacketNumber* last_packet) {
    QuicByteCount packet_size = 0;
    // Save the last packet's size.
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .Times(AnyNumber())
        .WillRepeatedly(SaveArg<3>(&packet_size));
    connection_.SendStreamDataWithString(id, data, offset, state);
    if (last_packet != nullptr) {
      *last_packet = creator_->packet_number();
    }
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .Times(AnyNumber());
    return packet_size;
  }

  void SendAckPacketToPeer() {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
    {
      QuicConnection::ScopedPacketFlusher flusher(&connection_);
      connection_.SendAck();
    }
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .Times(AnyNumber());
  }

  void SendRstStream(QuicStreamId id, QuicRstStreamErrorCode error,
                     QuicStreamOffset bytes_written) {
    notifier_.WriteOrBufferRstStream(id, error, bytes_written);
    connection_.OnStreamReset(id, error);
  }

  void SendPing() { notifier_.WriteOrBufferPing(); }

  MessageStatus SendMessage(absl::string_view message) {
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    quiche::QuicheMemSlice slice(quiche::QuicheBuffer::Copy(
        connection_.helper()->GetStreamSendBufferAllocator(), message));
    return connection_.SendMessage(1, absl::MakeSpan(&slice, 1), false);
  }

  void ProcessAckPacket(uint64_t packet_number, QuicAckFrame* frame) {
    if (packet_number > 1) {
      QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, packet_number - 1);
    } else {
      QuicPacketCreatorPeer::ClearPacketNumber(&peer_creator_);
    }
    ProcessFramePacket(QuicFrame(frame));
  }

  void ProcessAckPacket(QuicAckFrame* frame) {
    ProcessFramePacket(QuicFrame(frame));
  }

  void ProcessStopWaitingPacket(QuicStopWaitingFrame frame) {
    ProcessFramePacket(QuicFrame(frame));
  }

  size_t ProcessStopWaitingPacketAtLevel(uint64_t number,
                                         QuicStopWaitingFrame frame,
                                         EncryptionLevel /*level*/) {
    return ProcessFramePacketAtLevel(number, QuicFrame(frame),
                                     ENCRYPTION_ZERO_RTT);
  }

  void ProcessGoAwayPacket(QuicGoAwayFrame* frame) {
    ProcessFramePacket(QuicFrame(frame));
  }

  bool IsMissing(uint64_t number) {
    return IsAwaitingPacket(connection_.ack_frame(), QuicPacketNumber(number),
                            QuicPacketNumber());
  }

  std::unique_ptr<QuicPacket> ConstructPacket(const QuicPacketHeader& header,
                                              const QuicFrames& frames) {
    auto packet = BuildUnsizedDataPacket(&peer_framer_, header, frames);
    EXPECT_NE(nullptr, packet.get());
    return packet;
  }

  QuicPacketHeader ConstructPacketHeader(uint64_t number,
                                         EncryptionLevel level) {
    QuicPacketHeader header;
    if (level < ENCRYPTION_FORWARD_SECURE) {
      // Set long header type accordingly.
      header.version_flag = true;
      header.form = IETF_QUIC_LONG_HEADER_PACKET;
      header.long_packet_type = EncryptionlevelToLongHeaderType(level);
      if (QuicVersionHasLongHeaderLengths(
              peer_framer_.version().transport_version)) {
        header.length_length = quiche::VARIABLE_LENGTH_INTEGER_LENGTH_2;
        if (header.long_packet_type == INITIAL) {
          header.retry_token_length_length =
              quiche::VARIABLE_LENGTH_INTEGER_LENGTH_1;
        }
      }
    }
    // Set connection_id to peer's in memory representation as this data packet
    // is created by peer_framer.
    if (peer_framer_.perspective() == Perspective::IS_SERVER) {
      header.source_connection_id = connection_id_;
      header.source_connection_id_included = connection_id_included_;
      header.destination_connection_id_included = CONNECTION_ID_ABSENT;
    } else {
      header.destination_connection_id = connection_id_;
      header.destination_connection_id_included = connection_id_included_;
    }
    if (peer_framer_.perspective() == Perspective::IS_SERVER) {
      if (!connection_.client_connection_id().IsEmpty()) {
        header.destination_connection_id = connection_.client_connection_id();
        header.destination_connection_id_included = CONNECTION_ID_PRESENT;
      } else {
        header.destination_connection_id_included = CONNECTION_ID_ABSENT;
      }
      if (header.version_flag) {
        header.source_connection_id = connection_id_;
        header.source_connection_id_included = CONNECTION_ID_PRESENT;
        if (GetParam().version.handshake_protocol == PROTOCOL_QUIC_CRYPTO &&
            header.long_packet_type == ZERO_RTT_PROTECTED) {
          header.nonce = &kTestDiversificationNonce;
        }
      }
    }
    header.packet_number_length = packet_number_length_;
    header.packet_number = QuicPacketNumber(number);
    return header;
  }

  std::unique_ptr<QuicPacket> ConstructDataPacket(uint64_t number,
                                                  bool has_stop_waiting,
                                                  EncryptionLevel level) {
    QuicPacketHeader header = ConstructPacketHeader(number, level);
    QuicFrames frames;
    if (VersionHasIetfQuicFrames(version().transport_version) &&
        (level == ENCRYPTION_INITIAL || level == ENCRYPTION_HANDSHAKE)) {
      frames.push_back(QuicFrame(QuicPingFrame()));
      frames.push_back(QuicFrame(QuicPaddingFrame(100)));
    } else {
      frames.push_back(QuicFrame(frame1_));
      if (has_stop_waiting) {
        frames.push_back(QuicFrame(stop_waiting_));
      }
    }
    return ConstructPacket(header, frames);
  }

  std::unique_ptr<SerializedPacket> ConstructProbingPacket() {
    peer_creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
    if (VersionHasIetfQuicFrames(version().transport_version)) {
      QuicPathFrameBuffer payload = {
          {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xfe}};
      return QuicPacketCreatorPeer::
          SerializePathChallengeConnectivityProbingPacket(&peer_creator_,
                                                          payload);
    }
    QUICHE_DCHECK(!GetQuicReloadableFlag(quic_ignore_gquic_probing));
    return QuicPacketCreatorPeer::SerializeConnectivityProbingPacket(
        &peer_creator_);
  }

  std::unique_ptr<QuicPacket> ConstructClosePacket(uint64_t number) {
    peer_creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
    QuicPacketHeader header;
    // Set connection_id to peer's in memory representation as this connection
    // close packet is created by peer_framer.
    if (peer_framer_.perspective() == Perspective::IS_SERVER) {
      header.source_connection_id = connection_id_;
      header.destination_connection_id_included = CONNECTION_ID_ABSENT;
    } else {
      header.destination_connection_id = connection_id_;
      header.destination_connection_id_included = CONNECTION_ID_ABSENT;
    }

    header.packet_number = QuicPacketNumber(number);

    QuicErrorCode kQuicErrorCode = QUIC_PEER_GOING_AWAY;
    QuicConnectionCloseFrame qccf(peer_framer_.transport_version(),
                                  kQuicErrorCode, NO_IETF_QUIC_ERROR, "",
                                  /*transport_close_frame_type=*/0);
    QuicFrames frames;
    frames.push_back(QuicFrame(&qccf));
    return ConstructPacket(header, frames);
  }

  QuicTime::Delta DefaultRetransmissionTime() {
    return QuicTime::Delta::FromMilliseconds(kDefaultRetransmissionTimeMs);
  }

  QuicTime::Delta DefaultDelayedAckTime() {
    return QuicTime::Delta::FromMilliseconds(GetDefaultDelayedAckTimeMs());
  }

  const QuicStopWaitingFrame InitStopWaitingFrame(uint64_t least_unacked) {
    QuicStopWaitingFrame frame;
    frame.least_unacked = QuicPacketNumber(least_unacked);
    return frame;
  }

  // Construct a ack_frame that acks all packet numbers between 1 and
  // |largest_acked|, except |missing|.
  // REQUIRES: 1 <= |missing| < |largest_acked|
  QuicAckFrame ConstructAckFrame(uint64_t largest_acked, uint64_t missing) {
    return ConstructAckFrame(QuicPacketNumber(largest_acked),
                             QuicPacketNumber(missing));
  }

  QuicAckFrame ConstructAckFrame(QuicPacketNumber largest_acked,
                                 QuicPacketNumber missing) {
    if (missing == QuicPacketNumber(1)) {
      return InitAckFrame({{missing + 1, largest_acked + 1}});
    }
    return InitAckFrame(
        {{QuicPacketNumber(1), missing}, {missing + 1, largest_acked + 1}});
  }

  // Undo nacking a packet within the frame.
  void AckPacket(QuicPacketNumber arrived, QuicAckFrame* frame) {
    EXPECT_FALSE(frame->packets.Contains(arrived));
    frame->packets.Add(arrived);
  }

  void TriggerConnectionClose() {
    // Send an erroneous packet to close the connection.
    EXPECT_CALL(visitor_,
                OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
        .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));

    EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
    // Triggers a connection by receiving ACK of unsent packet.
    QuicAckFrame frame = InitAckFrame(10000);
    ProcessAckPacket(1, &frame);
    EXPECT_FALSE(QuicConnectionPeer::GetConnectionClosePacket(&connection_) ==
                 nullptr);
    EXPECT_EQ(1, connection_close_frame_count_);
    EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
                IsError(QUIC_INVALID_ACK_DATA));
  }

  void BlockOnNextWrite() {
    writer_->BlockOnNextWrite();
    EXPECT_CALL(visitor_, OnWriteBlocked()).Times(AtLeast(1));
  }

  void SimulateNextPacketTooLarge() { writer_->SimulateNextPacketTooLarge(); }

  void ExpectNextPacketUnprocessable() {
    writer_->ExpectNextPacketUnprocessable();
  }

  void AlwaysGetPacketTooLarge() { writer_->AlwaysGetPacketTooLarge(); }

  void SetWritePauseTimeDelta(QuicTime::Delta delta) {
    writer_->SetWritePauseTimeDelta(delta);
  }

  void CongestionBlockWrites() {
    EXPECT_CALL(*send_algorithm_, CanSend(_))
        .WillRepeatedly(testing::Return(false));
  }

  void CongestionUnblockWrites() {
    EXPECT_CALL(*send_algorithm_, CanSend(_))
        .WillRepeatedly(testing::Return(true));
  }

  void set_perspective(Perspective perspective) {
    connection_.set_perspective(perspective);
    if (perspective == Perspective::IS_SERVER) {
      connection_.set_can_truncate_connection_ids(true);
      QuicConnectionPeer::SetNegotiatedVersion(&connection_);
      connection_.OnSuccessfulVersionNegotiation();
    }
    QuicFramerPeer::SetPerspective(&peer_framer_,
                                   QuicUtils::InvertPerspective(perspective));
    peer_framer_.SetInitialObfuscators(TestConnectionId());
    for (EncryptionLevel level : {ENCRYPTION_ZERO_RTT, ENCRYPTION_HANDSHAKE,
                                  ENCRYPTION_FORWARD_SECURE}) {
      if (peer_framer_.HasEncrypterOfEncryptionLevel(level)) {
        peer_creator_.SetEncrypter(level,
                                   std::make_unique<TaggingEncrypter>(level));
      }
    }
  }

  void set_packets_between_probes_base(
      const QuicPacketCount packets_between_probes_base) {
    QuicConnectionPeer::ReInitializeMtuDiscoverer(
        &connection_, packets_between_probes_base,
        QuicPacketNumber(packets_between_probes_base));
  }

  bool IsDefaultTestConfiguration() {
    TestParams p = GetParam();
    return p.ack_response == AckResponse::kImmediate &&
           p.version == AllSupportedVersions()[0];
  }

  void TestConnectionCloseQuicErrorCode(QuicErrorCode expected_code) {
    // Not strictly needed for this test, but is commonly done.
    EXPECT_FALSE(QuicConnectionPeer::GetConnectionClosePacket(&connection_) ==
                 nullptr);
    const std::vector<QuicConnectionCloseFrame>& connection_close_frames =
        writer_->connection_close_frames();
    ASSERT_EQ(1u, connection_close_frames.size());

    EXPECT_THAT(connection_close_frames[0].quic_error_code,
                IsError(expected_code));

    if (!VersionHasIetfQuicFrames(version().transport_version)) {
      EXPECT_THAT(connection_close_frames[0].wire_error_code,
                  IsError(expected_code));
      EXPECT_EQ(GOOGLE_QUIC_CONNECTION_CLOSE,
                connection_close_frames[0].close_type);
      return;
    }

    QuicErrorCodeToIetfMapping mapping =
        QuicErrorCodeToTransportErrorCode(expected_code);

    if (mapping.is_transport_close) {
      // This Google QUIC Error Code maps to a transport close,
      EXPECT_EQ(IETF_QUIC_TRANSPORT_CONNECTION_CLOSE,
                connection_close_frames[0].close_type);
    } else {
      // This maps to an application close.
      EXPECT_EQ(IETF_QUIC_APPLICATION_CONNECTION_CLOSE,
                connection_close_frames[0].close_type);
    }
    EXPECT_EQ(mapping.error_code, connection_close_frames[0].wire_error_code);
  }

  void MtuDiscoveryTestInit() {
    set_perspective(Perspective::IS_SERVER);
    QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
    if (version().SupportsAntiAmplificationLimit()) {
      QuicConnectionPeer::SetAddressValidated(&connection_);
    }
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    peer_creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
    // Prevent packets from being coalesced.
    EXPECT_CALL(visitor_, GetHandshakeState())
        .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
    EXPECT_TRUE(connection_.connected());
  }

  void PathProbeTestInit(Perspective perspective,
                         bool receive_new_server_connection_id = true) {
    set_perspective(perspective);
    connection_.CreateConnectionIdManager();
    EXPECT_EQ(connection_.perspective(), perspective);
    if (perspective == Perspective::IS_SERVER) {
      QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
    }
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    peer_creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
    // Discard INITIAL key.
    connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
    connection_.NeuterUnencryptedPackets();
    // Prevent packets from being coalesced.
    EXPECT_CALL(visitor_, GetHandshakeState())
        .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
    if (version().SupportsAntiAmplificationLimit() &&
        perspective == Perspective::IS_SERVER) {
      QuicConnectionPeer::SetAddressValidated(&connection_);
    }
    // Clear direct_peer_address.
    QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
    // Clear effective_peer_address, it is the same as direct_peer_address for
    // this test.
    QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                                QuicSocketAddress());
    EXPECT_FALSE(connection_.effective_peer_address().IsInitialized());

    if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
      EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
    } else {
      EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
    }
    QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 2);
    ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress,
                                    kPeerAddress, ENCRYPTION_FORWARD_SECURE);
    EXPECT_EQ(kPeerAddress, connection_.peer_address());
    EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
    if (perspective == Perspective::IS_CLIENT &&
        receive_new_server_connection_id && version().HasIetfQuicFrames()) {
      QuicNewConnectionIdFrame frame;
      frame.connection_id = TestConnectionId(1234);
      ASSERT_NE(frame.connection_id, connection_.connection_id());
      frame.stateless_reset_token =
          QuicUtils::GenerateStatelessResetToken(frame.connection_id);
      frame.retire_prior_to = 0u;
      frame.sequence_number = 1u;
      connection_.OnNewConnectionIdFrame(frame);
    }
  }

  void ServerHandlePreferredAddressInit() {
    ASSERT_TRUE(GetParam().version.HasIetfQuicFrames());
    set_perspective(Perspective::IS_SERVER);
    connection_.CreateConnectionIdManager();
    QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
    SetQuicReloadableFlag(quic_use_received_client_addresses_cache, true);
    EXPECT_CALL(visitor_, AllowSelfAddressChange())
        .WillRepeatedly(Return(true));

    connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    peer_creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
    // Discard INITIAL key.
    connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
    connection_.NeuterUnencryptedPackets();
    // Prevent packets from being coalesced.
    EXPECT_CALL(visitor_, GetHandshakeState())
        .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
    if (version().SupportsAntiAmplificationLimit()) {
      QuicConnectionPeer::SetAddressValidated(&connection_);
    }
    // Clear direct_peer_address.
    QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
    // Clear effective_peer_address, it is the same as direct_peer_address for
    // this test.
    QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                                QuicSocketAddress());
    EXPECT_FALSE(connection_.effective_peer_address().IsInitialized());

    if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
      EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
    } else {
      EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
    }
    QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 2);
    ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress,
                                    kPeerAddress, ENCRYPTION_FORWARD_SECURE);
    EXPECT_EQ(kPeerAddress, connection_.peer_address());
    EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
    QuicConfig config;
    EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
    EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
    EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
    connection_.SetFromConfig(config);
    connection_.set_expected_server_preferred_address(kServerPreferredAddress);
  }

  // Receive server preferred address.
  void ServerPreferredAddressInit(QuicConfig& config) {
    ASSERT_EQ(Perspective::IS_CLIENT, connection_.perspective());
    ASSERT_TRUE(version().HasIetfQuicFrames());
    ASSERT_TRUE(connection_.self_address().host().IsIPv6());
    const QuicConnectionId connection_id = TestConnectionId(17);
    const StatelessResetToken reset_token =
        QuicUtils::GenerateStatelessResetToken(connection_id);

    connection_.CreateConnectionIdManager();

    connection_.SendCryptoStreamData();
    EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
    EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
    QuicAckFrame frame = InitAckFrame(1);
    // Received ACK for packet 1.
    ProcessFramePacketAtLevel(1, QuicFrame(&frame), ENCRYPTION_INITIAL);
    // Discard INITIAL key.
    connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);

    QuicConfigPeer::SetReceivedStatelessResetToken(&config,
                                                   kTestStatelessResetToken);
    QuicConfigPeer::SetReceivedAlternateServerAddress(&config,
                                                      kServerPreferredAddress);
    QuicConfigPeer::SetPreferredAddressConnectionIdAndToken(
        &config, connection_id, reset_token);
    EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
    EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
    EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
    connection_.SetFromConfig(config);

    ASSERT_TRUE(
        QuicConnectionPeer::GetReceivedServerPreferredAddress(&connection_)
            .IsInitialized());
    EXPECT_EQ(
        kServerPreferredAddress,
        QuicConnectionPeer::GetReceivedServerPreferredAddress(&connection_));
  }

  // If defer sending is enabled, tell |visitor_| to return true on the next
  // call to WillingAndAbleToWrite().
  // This function can be used before a call to ProcessXxxPacket, to allow the
  // process function to schedule and fire the send alarm at the end.
  void ForceWillingAndAbleToWriteOnceForDeferSending() {
    if (GetParam().ack_response == AckResponse::kDefer) {
      EXPECT_CALL(visitor_, WillingAndAbleToWrite())
          .WillOnce(Return(true))
          .RetiresOnSaturation();
    }
  }

  void TestClientRetryHandling(bool invalid_retry_tag,
                               bool missing_original_id_in_config,
                               bool wrong_original_id_in_config,
                               bool missing_retry_id_in_config,
                               bool wrong_retry_id_in_config);

  void TestReplaceConnectionIdFromInitial();

  QuicConnectionId connection_id_;
  QuicFramer framer_;

  MockSendAlgorithm* send_algorithm_;
  std::unique_ptr<MockLossAlgorithm> loss_algorithm_;
  MockClock clock_;
  MockRandom random_generator_;
  quiche::SimpleBufferAllocator buffer_allocator_;
  std::unique_ptr<TestConnectionHelper> helper_;
  std::unique_ptr<TestAlarmFactory> alarm_factory_;
  QuicFramer peer_framer_;
  QuicPacketCreator peer_creator_;
  std::unique_ptr<TestPacketWriter> writer_;
  TestConnection connection_;
  QuicPacketCreator* creator_;
  QuicSentPacketManager* manager_;
  StrictMock<MockQuicConnectionVisitor> visitor_;

  QuicStreamFrame frame1_;
  QuicStreamFrame frame2_;
  QuicCryptoFrame crypto_frame_;
  QuicAckFrame ack_;
  QuicStopWaitingFrame stop_waiting_;
  QuicPacketNumberLength packet_number_length_;
  QuicConnectionIdIncluded connection_id_included_;

  SimpleSessionNotifier notifier_;

  QuicConnectionCloseFrame saved_connection_close_frame_;
  int connection_close_frame_count_;
  MockConnectionIdGenerator connection_id_generator_;
};

// Run all end to end tests with all supported versions.
INSTANTIATE_TEST_SUITE_P(QuicConnectionTests, QuicConnectionTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

// Regression test for b/372756997.
TEST_P(QuicConnectionTest, NoNestedCloseConnection) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .WillRepeatedly(
          Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));
  EXPECT_CALL(connection_, OnSerializedPacket(_)).Times(AnyNumber());

  // Prepare the writer to fail to send the first connection close packet due
  // to the packet being too large.
  writer_->SetShouldWriteFail();
  writer_->SetWriteError(*writer_->MessageTooBigErrorCode());

  connection_.CloseConnection(
      QUIC_CRYPTO_TOO_MANY_ENTRIES, "Closed by test",
      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
  EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
              IsError(QUIC_CRYPTO_TOO_MANY_ENTRIES));
}

// These two tests ensure that the QuicErrorCode mapping works correctly.
// Both tests expect to see a Google QUIC close if not running IETF QUIC.
// If running IETF QUIC, the first will generate a transport connection
// close, the second an application connection close.
// The connection close codes for the two tests are manually chosen;
// they are expected to always map to transport- and application-
// closes, respectively. If that changes, new codes should be chosen.
TEST_P(QuicConnectionTest, CloseErrorCodeTestTransport) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
  connection_.CloseConnection(
      IETF_QUIC_PROTOCOL_VIOLATION, "Should be transport close",
      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
  EXPECT_FALSE(connection_.connected());
  TestConnectionCloseQuicErrorCode(IETF_QUIC_PROTOCOL_VIOLATION);
}

// Test that the IETF QUIC Error code mapping function works
// properly for application connection close codes.
TEST_P(QuicConnectionTest, CloseErrorCodeTestApplication) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
  connection_.CloseConnection(
      QUIC_HEADERS_STREAM_DATA_DECOMPRESS_FAILURE,
      "Should be application close",
      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
  EXPECT_FALSE(connection_.connected());
  TestConnectionCloseQuicErrorCode(QUIC_HEADERS_STREAM_DATA_DECOMPRESS_FAILURE);
}

TEST_P(QuicConnectionTest, SelfAddressChangeAtClient) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  EXPECT_EQ(Perspective::IS_CLIENT, connection_.perspective());
  EXPECT_TRUE(connection_.connected());

  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_));
  } else {
    EXPECT_CALL(visitor_, OnStreamFrame(_));
  }
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress, kPeerAddress,
                                  ENCRYPTION_INITIAL);
  // Cause change in self_address.
  QuicIpAddress host;
  host.FromString("1.1.1.1");
  QuicSocketAddress self_address(host, 123);
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_));
  } else {
    EXPECT_CALL(visitor_, OnStreamFrame(_));
  }
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), self_address, kPeerAddress,
                                  ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.connected());
  EXPECT_NE(connection_.self_address(), self_address);
}

TEST_P(QuicConnectionTest, SelfAddressChangeAtServer) {
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);

  EXPECT_EQ(Perspective::IS_SERVER, connection_.perspective());
  EXPECT_TRUE(connection_.connected());

  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_));
  } else {
    EXPECT_CALL(visitor_, OnStreamFrame(_));
  }
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress, kPeerAddress,
                                  ENCRYPTION_INITIAL);
  // Cause change in self_address.
  QuicIpAddress host;
  host.FromString("1.1.1.1");
  QuicSocketAddress self_address(host, 123);
  EXPECT_EQ(0u, connection_.GetStats().packets_dropped);
  EXPECT_CALL(visitor_, AllowSelfAddressChange()).WillOnce(Return(false));
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), self_address, kPeerAddress,
                                  ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.connected());
  EXPECT_EQ(1u, connection_.GetStats().packets_dropped);
}

TEST_P(QuicConnectionTest, AllowSelfAddressChangeToMappedIpv4AddressAtServer) {
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);

  EXPECT_EQ(Perspective::IS_SERVER, connection_.perspective());
  EXPECT_TRUE(connection_.connected());

  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(3);
  } else {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(3);
  }
  QuicIpAddress host;
  host.FromString("1.1.1.1");
  QuicSocketAddress self_address1(host, 443);
  connection_.SetSelfAddress(self_address1);
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), self_address1,
                                  kPeerAddress, ENCRYPTION_INITIAL);
  // Cause self_address change to mapped Ipv4 address.
  QuicIpAddress host2;
  host2.FromString(
      absl::StrCat("::ffff:", connection_.self_address().host().ToString()));
  QuicSocketAddress self_address2(host2, connection_.self_address().port());
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), self_address2,
                                  kPeerAddress, ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.connected());
  // self_address change back to Ipv4 address.
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), self_address1,
                                  kPeerAddress, ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.connected());
}

TEST_P(QuicConnectionTest, ClientAddressChangeAndPacketReordered) {
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is the same as direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());

  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  } else {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  }
  QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 5);
  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(),
                        /*port=*/23456);
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress,
                                  kNewPeerAddress, ENCRYPTION_INITIAL);
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());

  // Decrease packet number to simulate out-of-order packets.
  QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 4);
  // This is an old packet, do not migrate.
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress, kPeerAddress,
                                  ENCRYPTION_INITIAL);
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());
}

TEST_P(QuicConnectionTest, PeerPortChangeAtServer) {
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  EXPECT_EQ(Perspective::IS_SERVER, connection_.perspective());
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  // Prevent packets from being coalesced.
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  if (version().SupportsAntiAmplificationLimit()) {
    QuicConnectionPeer::SetAddressValidated(&connection_);
  }

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is the same as direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());
  EXPECT_FALSE(connection_.effective_peer_address().IsInitialized());

  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  QuicTime::Delta default_init_rtt = rtt_stats->initial_rtt();
  rtt_stats->set_initial_rtt(default_init_rtt * 2);
  EXPECT_EQ(2 * default_init_rtt, rtt_stats->initial_rtt());

  QuicSentPacketManagerPeer::SetConsecutivePtoCount(manager_, 1);
  EXPECT_EQ(1u, manager_->GetConsecutivePtoCount());

  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);
  EXPECT_CALL(visitor_, OnStreamFrame(_))
      .WillOnce(Invoke(
          [=, this]() { EXPECT_EQ(kPeerAddress, connection_.peer_address()); }))
      .WillOnce(Invoke([=, this]() {
        EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
      }));
  QuicFrames frames;
  frames.push_back(QuicFrame(frame1_));
  ProcessFramesPacketWithAddresses(frames, kSelfAddress, kPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  // Process another packet with a different peer address on server side will
  // start connection migration.
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(1);
  QuicFrames frames2;
  frames2.push_back(QuicFrame(frame2_));
  ProcessFramesPacketWithAddresses(frames2, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());
  // PORT_CHANGE shouldn't state change in sent packet manager.
  EXPECT_EQ(2 * default_init_rtt, rtt_stats->initial_rtt());
  EXPECT_EQ(1u, manager_->GetConsecutivePtoCount());
  EXPECT_EQ(manager_->GetSendAlgorithm(), send_algorithm_);
  if (version().HasIetfQuicFrames()) {
    EXPECT_EQ(NO_CHANGE, connection_.active_effective_peer_migration_type());
    EXPECT_EQ(1u, connection_.GetStats().num_validated_peer_migration);
    EXPECT_EQ(1u, connection_.num_linkable_client_migration());
  }
}

TEST_P(QuicConnectionTest, PeerIpAddressChangeAtServer) {
  set_perspective(Perspective::IS_SERVER);
  if (!version().SupportsAntiAmplificationLimit() ||
      GetQuicFlag(quic_enforce_strict_amplification_factor)) {
    return;
  }
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  EXPECT_EQ(Perspective::IS_SERVER, connection_.perspective());
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  // Discard INITIAL key.
  connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
  connection_.NeuterUnencryptedPackets();
  // Prevent packets from being coalesced.
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  QuicConnectionPeer::SetAddressValidated(&connection_);
  connection_.OnHandshakeComplete();

  // Enable 5 RTO
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(k5RTO);
  config.SetInitialReceivedConnectionOptions(connection_options);
  QuicConfigPeer::SetNegotiated(&config, true);
  QuicConfigPeer::SetReceivedOriginalConnectionId(&config,
                                                  connection_.connection_id());
  QuicConfigPeer::SetReceivedInitialSourceConnectionId(&config,
                                                       QuicConnectionId());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is the same as direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());
  EXPECT_FALSE(connection_.effective_peer_address().IsInitialized());

  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback4(), /*port=*/23456);
  EXPECT_CALL(visitor_, OnStreamFrame(_))
      .WillOnce(Invoke(
          [=, this]() { EXPECT_EQ(kPeerAddress, connection_.peer_address()); }))
      .WillOnce(Invoke([=, this]() {
        EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
      }));
  QuicFrames frames;
  frames.push_back(QuicFrame(frame1_));
  ProcessFramesPacketWithAddresses(frames, kSelfAddress, kPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  // Send some data to make connection has packets in flight.
  connection_.SendStreamData3();
  EXPECT_EQ(1u, writer_->packets_write_attempts());
  EXPECT_TRUE(connection_.BlackholeDetectionInProgress());
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());

  // Process another packet with a different peer address on server side will
  // start connection migration.
  EXPECT_CALL(visitor_, OnConnectionMigration(IPV6_TO_IPV4_CHANGE)).Times(1);
  // IETF QUIC send algorithm should be changed to a different object, so no
  // OnPacketSent() called on the old send algorithm.
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, _, _, _, NO_RETRANSMITTABLE_DATA))
      .Times(0);
  // Do not propagate OnCanWrite() to session notifier.
  EXPECT_CALL(visitor_, OnCanWrite()).Times(AnyNumber());

  QuicFrames frames2;
  frames2.push_back(QuicFrame(frame2_));
  ProcessFramesPacketWithAddresses(frames2, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());
  EXPECT_EQ(IPV6_TO_IPV4_CHANGE,
            connection_.active_effective_peer_migration_type());
  EXPECT_FALSE(connection_.BlackholeDetectionInProgress());
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  EXPECT_EQ(2u, writer_->packets_write_attempts());
  EXPECT_FALSE(writer_->path_challenge_frames().empty());
  QuicPathFrameBuffer payload =
      writer_->path_challenge_frames().front().data_buffer;
  EXPECT_NE(connection_.sent_packet_manager().GetSendAlgorithm(),
            send_algorithm_);
  // Switch to use the mock send algorithm.
  send_algorithm_ = new StrictMock<MockSendAlgorithm>();
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(kDefaultTCPMSS));
  EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, BandwidthEstimate())
      .Times(AnyNumber())
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, InSlowStart()).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, InRecovery()).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, PopulateConnectionStats(_)).Times(AnyNumber());
  connection_.SetSendAlgorithm(send_algorithm_);

  // PATH_CHALLENGE is expanded upto the max packet size which may exceeds the
  // anti-amplification limit.
  EXPECT_EQ(kNewPeerAddress, writer_->last_write_peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());
  EXPECT_EQ(1u,
            connection_.GetStats().num_reverse_path_validtion_upon_migration);

  // Verify server is throttled by anti-amplification limit.
  connection_.SendCryptoDataWithString("foo", 0);
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  // Receiving an ACK to the packet sent after changing peer address doesn't
  // finish migration validation.
  QuicAckFrame ack_frame = InitAckFrame(2);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _));
  ProcessFramePacketWithAddresses(QuicFrame(&ack_frame), kSelfAddress,
                                  kNewPeerAddress, ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());
  EXPECT_EQ(IPV6_TO_IPV4_CHANGE,
            connection_.active_effective_peer_migration_type());

  // Receiving PATH_RESPONSE should lift the anti-amplification limit.
  QuicFrames frames3;
  frames3.push_back(QuicFrame(QuicPathResponseFrame(99, payload)));
  EXPECT_CALL(visitor_, MaybeSendAddressToken());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(testing::AtLeast(1u));
  ProcessFramesPacketWithAddresses(frames3, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(NO_CHANGE, connection_.active_effective_peer_migration_type());

  // Verify the anti-amplification limit is lifted by sending a packet larger
  // than the anti-amplification limit.
  connection_.SendCryptoDataWithString(std::string(1200, 'a'), 0);
  EXPECT_EQ(1u, connection_.GetStats().num_validated_peer_migration);
  EXPECT_EQ(1u, connection_.num_linkable_client_migration());
}

TEST_P(QuicConnectionTest, PeerIpAddressChangeAtServerWithMissingConnectionId) {
  set_perspective(Perspective::IS_SERVER);
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  EXPECT_EQ(Perspective::IS_SERVER, connection_.perspective());

  QuicConnectionId client_cid0 = TestConnectionId(1);
  QuicConnectionId client_cid1 = TestConnectionId(3);
  QuicConnectionId server_cid1;
  SetClientConnectionId(client_cid0);
  connection_.CreateConnectionIdManager();
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  // Prevent packets from being coalesced.
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  QuicConnectionPeer::SetAddressValidated(&connection_);

  // Sends new server CID to client.
  if (!connection_.connection_id().IsEmpty()) {
    EXPECT_CALL(connection_id_generator_, GenerateNextConnectionId(_))
        .WillOnce(Return(TestConnectionId(456)));
  }
  EXPECT_CALL(visitor_, MaybeReserveConnectionId(_))
      .WillOnce(Invoke([&](const QuicConnectionId& cid) {
        server_cid1 = cid;
        return true;
      }));
  EXPECT_CALL(visitor_, SendNewConnectionId(_));
  connection_.OnHandshakeComplete();

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is the same as direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());
  EXPECT_FALSE(connection_.effective_peer_address().IsInitialized());

  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback4(), /*port=*/23456);
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(2);
  QuicFrames frames;
  frames.push_back(QuicFrame(frame1_));
  ProcessFramesPacketWithAddresses(frames, kSelfAddress, kPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  // Send some data to make connection has packets in flight.
  connection_.SendStreamData3();
  EXPECT_EQ(1u, writer_->packets_write_attempts());

  // Process another packet with a different peer address on server side will
  // start connection migration.
  peer_creator_.SetServerConnectionId(server_cid1);
  EXPECT_CALL(visitor_, OnConnectionMigration(IPV6_TO_IPV4_CHANGE)).Times(1);
  // Do not propagate OnCanWrite() to session notifier.
  EXPECT_CALL(visitor_, OnCanWrite()).Times(AnyNumber());

  QuicFrames frames2;
  frames2.push_back(QuicFrame(frame2_));
  if (GetQuicFlag(quic_enforce_strict_amplification_factor)) {
    frames2.push_back(QuicFrame(QuicPaddingFrame(-1)));
  }
  ProcessFramesPacketWithAddresses(frames2, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());

  // Writing path response & reverse path challenge is blocked due to missing
  // client connection ID, i.e., packets_write_attempts is unchanged.
  EXPECT_EQ(1u, writer_->packets_write_attempts());

  // Receives new client CID from client would unblock write.
  QuicNewConnectionIdFrame new_cid_frame;
  new_cid_frame.connection_id = client_cid1;
  new_cid_frame.sequence_number = 1u;
  new_cid_frame.retire_prior_to = 0u;
  connection_.OnNewConnectionIdFrame(new_cid_frame);
  connection_.SendStreamData3();

  EXPECT_EQ(2u, writer_->packets_write_attempts());
}

TEST_P(QuicConnectionTest, EffectivePeerAddressChangeAtServer) {
  if (GetQuicFlag(quic_enforce_strict_amplification_factor)) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  EXPECT_EQ(Perspective::IS_SERVER, connection_.perspective());
  if (version().SupportsAntiAmplificationLimit()) {
    QuicConnectionPeer::SetAddressValidated(&connection_);
  }
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  // Discard INITIAL key.
  connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
  connection_.NeuterUnencryptedPackets();
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is different from direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());
  const QuicSocketAddress kEffectivePeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/43210);
  connection_.ReturnEffectivePeerAddressForNextPacket(kEffectivePeerAddress);

  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  } else {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  }
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress, kPeerAddress,
                                  ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kEffectivePeerAddress, connection_.effective_peer_address());

  // Process another packet with the same direct peer address and different
  // effective peer address on server side will start connection migration.
  const QuicSocketAddress kNewEffectivePeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/54321);
  connection_.ReturnEffectivePeerAddressForNextPacket(kNewEffectivePeerAddress);
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(1);
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress, kPeerAddress,
                                  ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewEffectivePeerAddress, connection_.effective_peer_address());
  EXPECT_EQ(kPeerAddress, writer_->last_write_peer_address());
  if (GetParam().version.HasIetfQuicFrames()) {
    EXPECT_EQ(NO_CHANGE, connection_.active_effective_peer_migration_type());
    EXPECT_EQ(1u, connection_.GetStats().num_validated_peer_migration);
    EXPECT_EQ(1u, connection_.num_linkable_client_migration());
  }

  // Process another packet with a different direct peer address and the same
  // effective peer address on server side will not start connection migration.
  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);
  connection_.ReturnEffectivePeerAddressForNextPacket(kNewEffectivePeerAddress);
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);

  if (!GetParam().version.HasIetfQuicFrames()) {
    // ack_frame is used to complete the migration started by the last packet,
    // we need to make sure a new migration does not start after the previous
    // one is completed.
    QuicAckFrame ack_frame = InitAckFrame(1);
    EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _));
    ProcessFramePacketWithAddresses(QuicFrame(&ack_frame), kSelfAddress,
                                    kNewPeerAddress, ENCRYPTION_FORWARD_SECURE);
    EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
    EXPECT_EQ(kNewEffectivePeerAddress, connection_.effective_peer_address());
    EXPECT_EQ(NO_CHANGE, connection_.active_effective_peer_migration_type());
  }

  // Process another packet with different direct peer address and different
  // effective peer address on server side will start connection migration.
  const QuicSocketAddress kNewerEffectivePeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/65432);
  const QuicSocketAddress kFinalPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/34567);
  connection_.ReturnEffectivePeerAddressForNextPacket(
      kNewerEffectivePeerAddress);
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(1);
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress,
                                  kFinalPeerAddress, ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kFinalPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewerEffectivePeerAddress, connection_.effective_peer_address());
  if (GetParam().version.HasIetfQuicFrames()) {
    EXPECT_EQ(NO_CHANGE, connection_.active_effective_peer_migration_type());
    EXPECT_EQ(send_algorithm_,
              connection_.sent_packet_manager().GetSendAlgorithm());
    EXPECT_EQ(2u, connection_.GetStats().num_validated_peer_migration);
  }

  // While the previous migration is ongoing, process another packet with the
  // same direct peer address and different effective peer address on server
  // side will start a new connection migration.
  const QuicSocketAddress kNewestEffectivePeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback4(), /*port=*/65430);
  connection_.ReturnEffectivePeerAddressForNextPacket(
      kNewestEffectivePeerAddress);
  EXPECT_CALL(visitor_, OnConnectionMigration(IPV6_TO_IPV4_CHANGE)).Times(1);
  if (!GetParam().version.HasIetfQuicFrames()) {
    EXPECT_CALL(*send_algorithm_, OnConnectionMigration()).Times(1);
  }
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress,
                                  kFinalPeerAddress, ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kFinalPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewestEffectivePeerAddress, connection_.effective_peer_address());
  EXPECT_EQ(IPV6_TO_IPV4_CHANGE,
            connection_.active_effective_peer_migration_type());
  if (GetParam().version.HasIetfQuicFrames()) {
    EXPECT_NE(send_algorithm_,
              connection_.sent_packet_manager().GetSendAlgorithm());
    EXPECT_EQ(kFinalPeerAddress, writer_->last_write_peer_address());
    EXPECT_FALSE(writer_->path_challenge_frames().empty());
    EXPECT_EQ(0u, connection_.GetStats()
                      .num_peer_migration_while_validating_default_path);
    EXPECT_TRUE(connection_.HasPendingPathValidation());
  }
}

// Regression test for b/200020764.
TEST_P(QuicConnectionTest, ConnectionMigrationWithPendingPaddingBytes) {
  // TODO(haoyuewang) Move these test setup code to a common member function.
  set_perspective(Perspective::IS_SERVER);
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  connection_.CreateConnectionIdManager();
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  QuicConnectionPeer::SetPeerAddress(&connection_, kPeerAddress);
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_, kPeerAddress);
  QuicConnectionPeer::SetAddressValidated(&connection_);

  // Sends new server CID to client.
  QuicConnectionId new_cid;
  if (!connection_.connection_id().IsEmpty()) {
    EXPECT_CALL(connection_id_generator_, GenerateNextConnectionId(_))
        .WillOnce(Return(TestConnectionId(456)));
  }
  EXPECT_CALL(visitor_, MaybeReserveConnectionId(_))
      .WillOnce(Invoke([&](const QuicConnectionId& cid) {
        new_cid = cid;
        return true;
      }));
  EXPECT_CALL(visitor_, SendNewConnectionId(_));
  // Discard INITIAL key.
  connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
  connection_.NeuterUnencryptedPackets();
  connection_.OnHandshakeComplete();
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));

  auto* packet_creator = QuicConnectionPeer::GetPacketCreator(&connection_);
  packet_creator->FlushCurrentPacket();
  packet_creator->AddPendingPadding(50u);
  const QuicSocketAddress kPeerAddress3 =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/56789);
  auto ack_frame = InitAckFrame(1);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _));
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(1);
  ProcessFramesPacketWithAddresses({QuicFrame(&ack_frame)}, kSelfAddress,
                                   kPeerAddress3, ENCRYPTION_FORWARD_SECURE);
  // Any pending frames/padding should be flushed before default_path_ is
  // temporarily reset.
  ASSERT_EQ(connection_.self_address_on_default_path_while_sending_packet()
                .host()
                .address_family(),
            IpAddressFamily::IP_V6);
}

// Regression test for b/196208556.
TEST_P(QuicConnectionTest,
       ReversePathValidationResponseReceivedFromUnexpectedPeerAddress) {
  set_perspective(Perspective::IS_SERVER);
  if (!version().HasIetfQuicFrames() ||
      GetQuicFlag(quic_enforce_strict_amplification_factor)) {
    return;
  }
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  connection_.CreateConnectionIdManager();
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  QuicConnectionPeer::SetPeerAddress(&connection_, kPeerAddress);
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_, kPeerAddress);
  QuicConnectionPeer::SetAddressValidated(&connection_);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  // Sends new server CID to client.
  QuicConnectionId new_cid;
  if (!connection_.connection_id().IsEmpty()) {
    EXPECT_CALL(connection_id_generator_, GenerateNextConnectionId(_))
        .WillOnce(Return(TestConnectionId(456)));
  }
  EXPECT_CALL(visitor_, MaybeReserveConnectionId(_))
      .WillOnce(Invoke([&](const QuicConnectionId& cid) {
        new_cid = cid;
        return true;
      }));
  EXPECT_CALL(visitor_, SendNewConnectionId(_));
  // Discard INITIAL key.
  connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
  connection_.NeuterUnencryptedPackets();
  connection_.OnHandshakeComplete();
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));

  // Process a non-probing packet to migrate to path 2 and kick off reverse path
  // validation.
  EXPECT_CALL(visitor_, OnConnectionMigration(IPV6_TO_IPV4_CHANGE)).Times(1);
  const QuicSocketAddress kPeerAddress2 =
      QuicSocketAddress(QuicIpAddress::Loopback4(), /*port=*/23456);
  peer_creator_.SetServerConnectionId(new_cid);
  ProcessFramesPacketWithAddresses({QuicFrame(QuicPingFrame())}, kSelfAddress,
                                   kPeerAddress2, ENCRYPTION_FORWARD_SECURE);
  EXPECT_FALSE(writer_->path_challenge_frames().empty());
  QuicPathFrameBuffer reverse_path_challenge_payload =
      writer_->path_challenge_frames().front().data_buffer;

  // Receiveds a packet from path 3 with PATH_RESPONSE frame intended to
  // validate path 2 and a non-probing frame.
  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    const QuicSocketAddress kPeerAddress3 =
        QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/56789);
    auto ack_frame = InitAckFrame(1);
    EXPECT_CALL(visitor_, OnConnectionMigration(IPV4_TO_IPV6_CHANGE)).Times(1);
    EXPECT_CALL(visitor_, MaybeSendAddressToken()).WillOnce(Invoke([this]() {
      connection_.SendControlFrame(
          QuicFrame(new QuicNewTokenFrame(1, "new_token")));
      return true;
    }));
    ProcessFramesPacketWithAddresses(
        {QuicFrame(QuicPathResponseFrame(0, reverse_path_challenge_payload)),
         QuicFrame(&ack_frame)},
        kSelfAddress, kPeerAddress3, ENCRYPTION_FORWARD_SECURE);
  }
}

TEST_P(QuicConnectionTest, ReversePathValidationFailureAtServer) {
  set_perspective(Perspective::IS_SERVER);
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  EXPECT_EQ(Perspective::IS_SERVER, connection_.perspective());
  SetClientConnectionId(TestConnectionId(1));
  connection_.CreateConnectionIdManager();
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  // Discard INITIAL key.
  connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
  connection_.NeuterUnencryptedPackets();
  // Prevent packets from being coalesced.
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  QuicConnectionPeer::SetAddressValidated(&connection_);

  QuicConnectionId client_cid0 = connection_.client_connection_id();
  QuicConnectionId client_cid1 = TestConnectionId(2);
  QuicConnectionId server_cid0 = connection_.connection_id();
  QuicConnectionId server_cid1;
  // Sends new server CID to client.
  if (!connection_.connection_id().IsEmpty()) {
    EXPECT_CALL(connection_id_generator_, GenerateNextConnectionId(_))
        .WillOnce(Return(TestConnectionId(456)));
  }
  EXPECT_CALL(visitor_, MaybeReserveConnectionId(_))
      .WillOnce(Invoke([&](const QuicConnectionId& cid) {
        server_cid1 = cid;
        return true;
      }));
  EXPECT_CALL(visitor_, SendNewConnectionId(_));
  connection_.OnHandshakeComplete();
  // Receives new client CID from client.
  QuicNewConnectionIdFrame new_cid_frame;
  new_cid_frame.connection_id = client_cid1;
  new_cid_frame.sequence_number = 1u;
  new_cid_frame.retire_prior_to = 0u;
  connection_.OnNewConnectionIdFrame(new_cid_frame);
  auto* packet_creator = QuicConnectionPeer::GetPacketCreator(&connection_);
  ASSERT_EQ(packet_creator->GetDestinationConnectionId(), client_cid0);
  ASSERT_EQ(packet_creator->GetSourceConnectionId(), server_cid0);

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is the same as direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());
  EXPECT_FALSE(connection_.effective_peer_address().IsInitialized());

  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback4(), /*port=*/23456);
  EXPECT_CALL(visitor_, OnStreamFrame(_))
      .WillOnce(Invoke(
          [=, this]() { EXPECT_EQ(kPeerAddress, connection_.peer_address()); }))
      .WillOnce(Invoke([=, this]() {
        EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
      }));
  QuicFrames frames;
  frames.push_back(QuicFrame(frame1_));
  ProcessFramesPacketWithAddresses(frames, kSelfAddress, kPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  // Process another packet with a different peer address on server side will
  // start connection migration.
  EXPECT_CALL(visitor_, OnConnectionMigration(IPV6_TO_IPV4_CHANGE)).Times(1);
  // IETF QUIC send algorithm should be changed to a different object, so no
  // OnPacketSent() called on the old send algorithm.
  EXPECT_CALL(*send_algorithm_, OnConnectionMigration()).Times(0);

  QuicFrames frames2;
  frames2.push_back(QuicFrame(frame2_));
  QuicPaddingFrame padding;
  frames2.push_back(QuicFrame(padding));
  peer_creator_.SetServerConnectionId(server_cid1);
  ProcessFramesPacketWithAddresses(frames2, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());
  EXPECT_EQ(IPV6_TO_IPV4_CHANGE,
            connection_.active_effective_peer_migration_type());
  EXPECT_LT(0u, writer_->packets_write_attempts());
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  EXPECT_NE(connection_.sent_packet_manager().GetSendAlgorithm(),
            send_algorithm_);
  EXPECT_EQ(kNewPeerAddress, writer_->last_write_peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());
  const auto* default_path = QuicConnectionPeer::GetDefaultPath(&connection_);
  const auto* alternative_path =
      QuicConnectionPeer::GetAlternativePath(&connection_);
  EXPECT_EQ(default_path->client_connection_id, client_cid1);
  EXPECT_EQ(default_path->server_connection_id, server_cid1);
  EXPECT_EQ(alternative_path->client_connection_id, client_cid0);
  EXPECT_EQ(alternative_path->server_connection_id, server_cid0);
  EXPECT_EQ(packet_creator->GetDestinationConnectionId(), client_cid1);
  EXPECT_EQ(packet_creator->GetSourceConnectionId(), server_cid1);

  for (size_t i = 0; i < QuicPathValidator::kMaxRetryTimes; ++i) {
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(3 * kInitialRttMs));
    static_cast<TestAlarmFactory::TestAlarm*>(
        QuicPathValidatorPeer::retry_timer(
            QuicConnectionPeer::path_validator(&connection_)))
        ->Fire();
  }
  EXPECT_EQ(IPV6_TO_IPV4_CHANGE,
            connection_.active_effective_peer_migration_type());

  // Make sure anti-amplification limit is not reached.
  ProcessFramesPacketWithAddresses(
      {QuicFrame(QuicPingFrame()), QuicFrame(QuicPaddingFrame())}, kSelfAddress,
      kNewPeerAddress, ENCRYPTION_FORWARD_SECURE);
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, nullptr);
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());

  // Advance the time so that the reverse path validation times out.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(3 * kInitialRttMs));
  static_cast<TestAlarmFactory::TestAlarm*>(
      QuicPathValidatorPeer::retry_timer(
          QuicConnectionPeer::path_validator(&connection_)))
      ->Fire();
  EXPECT_EQ(NO_CHANGE, connection_.active_effective_peer_migration_type());
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
  EXPECT_EQ(connection_.sent_packet_manager().GetSendAlgorithm(),
            send_algorithm_);
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());

  // Verify that default_path_ is reverted and alternative_path_ is cleared.
  EXPECT_EQ(default_path->client_connection_id, client_cid0);
  EXPECT_EQ(default_path->server_connection_id, server_cid0);
  EXPECT_TRUE(alternative_path->server_connection_id.IsEmpty());
  EXPECT_FALSE(alternative_path->stateless_reset_token.has_value());
  auto* retire_peer_issued_cid_alarm =
      connection_.GetRetirePeerIssuedConnectionIdAlarm();
  ASSERT_TRUE(retire_peer_issued_cid_alarm->IsSet());
  EXPECT_CALL(visitor_, SendRetireConnectionId(/*sequence_number=*/1u));
  retire_peer_issued_cid_alarm->Fire();
  EXPECT_EQ(packet_creator->GetDestinationConnectionId(), client_cid0);
  EXPECT_EQ(packet_creator->GetSourceConnectionId(), server_cid0);
}

TEST_P(QuicConnectionTest, ReceivePathProbeWithNoAddressChangeAtServer) {
  if (!version().HasIetfQuicFrames() &&
      GetQuicReloadableFlag(quic_ignore_gquic_probing)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_SERVER);

  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  EXPECT_CALL(visitor_, OnPacketReceived(_, _, false)).Times(0);

  // Process a padded PING packet with no peer address change on server side
  // will be ignored. But a PATH CHALLENGE packet with no peer address change
  // will be considered as path probing.
  std::unique_ptr<SerializedPacket> probing_packet = ConstructProbingPacket();

  std::unique_ptr<QuicReceivedPacket> received(ConstructReceivedPacket(
      QuicEncryptedPacket(probing_packet->encrypted_buffer,
                          probing_packet->encrypted_length),
      clock_.Now()));

  uint64_t num_probing_received =
      connection_.GetStats().num_connectivity_probing_received;
  ProcessReceivedPacket(kSelfAddress, kPeerAddress, *received);

  EXPECT_EQ(
      num_probing_received + (GetParam().version.HasIetfQuicFrames() ? 1u : 0u),
      connection_.GetStats().num_connectivity_probing_received);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
}

// Regression test for b/150161358.
TEST_P(QuicConnectionTest, BufferedMtuPacketTooBig) {
  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(1);
  writer_->SetWriteBlocked();

  // Send a MTU packet while blocked. It should be buffered.
  connection_.SendMtuDiscoveryPacket(kMaxOutgoingPacketSize);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());
  EXPECT_TRUE(writer_->IsWriteBlocked());

  writer_->AlwaysGetPacketTooLarge();
  writer_->SetWritable();
  connection_.OnCanWrite();
}

TEST_P(QuicConnectionTest, WriteOutOfOrderQueuedPackets) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (!IsDefaultTestConfiguration()) {
    return;
  }

  set_perspective(Perspective::IS_CLIENT);

  BlockOnNextWrite();

  QuicStreamId stream_id = 2;
  connection_.SendStreamDataWithString(stream_id, "foo", 0, NO_FIN);

  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  writer_->SetWritable();
  connection_.SendConnectivityProbingPacket(writer_.get(),
                                            connection_.peer_address());
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _)).Times(0);
  connection_.OnCanWrite();
}

TEST_P(QuicConnectionTest, DiscardQueuedPacketsAfterConnectionClose) {
  // Regression test for b/74073386.
  {
    InSequence seq;
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .Times(AtLeast(1));
    EXPECT_CALL(visitor_, OnConnectionClosed(_, _)).Times(AtLeast(1));
  }

  set_perspective(Perspective::IS_CLIENT);

  writer_->SimulateNextPacketTooLarge();

  // This packet write should fail, which should cause the connection to close
  // after sending a connection close packet, then the failed packet should be
  // queued.
  connection_.SendStreamDataWithString(/*id=*/2, "foo", 0, NO_FIN);

  EXPECT_FALSE(connection_.connected());
  // No need to buffer packets.
  EXPECT_EQ(0u, connection_.NumQueuedPackets());

  EXPECT_EQ(0u, connection_.GetStats().packets_discarded);
  connection_.OnCanWrite();
  EXPECT_EQ(0u, connection_.GetStats().packets_discarded);
}

class TestQuicPathValidationContext : public QuicPathValidationContext {
 public:
  TestQuicPathValidationContext(const QuicSocketAddress& self_address,
                                const QuicSocketAddress& peer_address,

                                QuicPacketWriter* writer)
      : QuicPathValidationContext(self_address, peer_address),
        writer_(writer) {}

  QuicPacketWriter* WriterToUse() override { return writer_; }

 private:
  QuicPacketWriter* writer_;
};

class TestValidationResultDelegate : public QuicPathValidator::ResultDelegate {
 public:
  TestValidationResultDelegate(QuicConnection* connection,
                               const QuicSocketAddress& expected_self_address,
                               const QuicSocketAddress& expected_peer_address,
                               bool* success)
      : QuicPathValidator::ResultDelegate(),
        connection_(connection),
        expected_self_address_(expected_self_address),
        expected_peer_address_(expected_peer_address),
        success_(success) {}
  void OnPathValidationSuccess(
      std::unique_ptr<QuicPathValidationContext> context,
      QuicTime /*start_time*/) override {
    EXPECT_EQ(expected_self_address_, context->self_address());
    EXPECT_EQ(expected_peer_address_, context->peer_address());
    *success_ = true;
  }

  void OnPathValidationFailure(
      std::unique_ptr<QuicPathValidationContext> context) override {
    EXPECT_EQ(expected_self_address_, context->self_address());
    EXPECT_EQ(expected_peer_address_, context->peer_address());
    if (connection_->perspective() == Perspective::IS_CLIENT) {
      connection_->OnPathValidationFailureAtClient(/*is_multi_port=*/false,
                                                   *context);
    }
    *success_ = false;
  }

 private:
  QuicConnection* connection_;
  QuicSocketAddress expected_self_address_;
  QuicSocketAddress expected_peer_address_;
  bool* success_;
};

// A test implementation which migrates to server preferred address
// on path validation suceeds. Otherwise, client cleans up alternative path.
class ServerPreferredAddressTestResultDelegate
    : public QuicPathValidator::ResultDelegate {
 public:
  explicit ServerPreferredAddressTestResultDelegate(QuicConnection* connection)
      : connection_(connection) {}
  void OnPathValidationSuccess(
      std::unique_ptr<QuicPathValidationContext> context,
      QuicTime /*start_time*/) override {
    connection_->OnServerPreferredAddressValidated(*context, false);
  }

  void OnPathValidationFailure(
      std::unique_ptr<QuicPathValidationContext> context) override {
    connection_->OnPathValidationFailureAtClient(/*is_multi_port=*/false,
                                                 *context);
  }

 protected:
  QuicConnection* connection() { return connection_; }

 private:
  QuicConnection* connection_;
};

// Receive a path probe request at the server side, in IETF version: receive a
// packet contains PATH CHALLENGE with peer address change.
TEST_P(QuicConnectionTest, ReceivePathProbingFromNewPeerAddressAtServer) {
  if (!version().HasIetfQuicFrames() &&
      GetQuicReloadableFlag(quic_ignore_gquic_probing)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_SERVER);

  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  QuicPathFrameBuffer payload;
  if (!GetParam().version.HasIetfQuicFrames()) {
    EXPECT_CALL(visitor_,
                OnPacketReceived(_, _, /*is_connectivity_probe=*/true))
        .Times(1);
  } else {
    EXPECT_CALL(visitor_, OnPacketReceived(_, _, _)).Times(0);
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .Times(AtLeast(1u))
        .WillOnce(Invoke([&]() {
          EXPECT_EQ(1u, writer_->path_challenge_frames().size());
          EXPECT_EQ(1u, writer_->path_response_frames().size());
          payload = writer_->path_challenge_frames().front().data_buffer;
        }))
        .WillRepeatedly(DoDefault());
  }
  // Process a probing packet from a new peer address on server side
  // is effectively receiving a connectivity probing.
  const QuicSocketAddress kNewPeerAddress(QuicIpAddress::Loopback4(),
                                          /*port=*/23456);

  std::unique_ptr<SerializedPacket> probing_packet = ConstructProbingPacket();
  std::unique_ptr<QuicReceivedPacket> received(ConstructReceivedPacket(
      QuicEncryptedPacket(probing_packet->encrypted_buffer,
                          probing_packet->encrypted_length),
      clock_.Now()));
  uint64_t num_probing_received =
      connection_.GetStats().num_connectivity_probing_received;
  ProcessReceivedPacket(kSelfAddress, kNewPeerAddress, *received);

  EXPECT_EQ(num_probing_received + 1,
            connection_.GetStats().num_connectivity_probing_received);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
  if (GetParam().version.HasIetfQuicFrames()) {
    QuicByteCount bytes_sent =
        QuicConnectionPeer::BytesSentOnAlternativePath(&connection_);
    EXPECT_LT(0u, bytes_sent);
    EXPECT_EQ(received->length(),
              QuicConnectionPeer::BytesReceivedOnAlternativePath(&connection_));

    // Receiving one more probing packet should update the bytes count.
    probing_packet = ConstructProbingPacket();
    received.reset(ConstructReceivedPacket(
        QuicEncryptedPacket(probing_packet->encrypted_buffer,
                            probing_packet->encrypted_length),
        clock_.Now()));
    ProcessReceivedPacket(kSelfAddress, kNewPeerAddress, *received);

    EXPECT_EQ(num_probing_received + 2,
              connection_.GetStats().num_connectivity_probing_received);
    EXPECT_EQ(2 * bytes_sent,
              QuicConnectionPeer::BytesSentOnAlternativePath(&connection_));
    EXPECT_EQ(2 * received->length(),
              QuicConnectionPeer::BytesReceivedOnAlternativePath(&connection_));

    EXPECT_EQ(2 * bytes_sent,
              QuicConnectionPeer::BytesSentOnAlternativePath(&connection_));
    QuicFrames frames;
    frames.push_back(QuicFrame(QuicPathResponseFrame(99, payload)));
    ProcessFramesPacketWithAddresses(frames, connection_.self_address(),
                                     kNewPeerAddress,
                                     ENCRYPTION_FORWARD_SECURE);
    EXPECT_LT(2 * received->length(),
              QuicConnectionPeer::BytesReceivedOnAlternativePath(&connection_));
    EXPECT_TRUE(QuicConnectionPeer::IsAlternativePathValidated(&connection_));
    // Receiving another probing packet from a newer address with a different
    // port shouldn't trigger another reverse path validation.
    QuicSocketAddress kNewerPeerAddress(QuicIpAddress::Loopback4(),
                                        /*port=*/34567);
    probing_packet = ConstructProbingPacket();
    received.reset(ConstructReceivedPacket(
        QuicEncryptedPacket(probing_packet->encrypted_buffer,
                            probing_packet->encrypted_length),
        clock_.Now()));
    ProcessReceivedPacket(kSelfAddress, kNewerPeerAddress, *received);
    EXPECT_FALSE(connection_.HasPendingPathValidation());
    EXPECT_TRUE(QuicConnectionPeer::IsAlternativePathValidated(&connection_));
  }

  // Process another packet with the old peer address on server side will not
  // start peer migration.
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress, kPeerAddress,
                                  ENCRYPTION_INITIAL);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
}

// Receive a packet contains PATH CHALLENGE with self address change.
TEST_P(QuicConnectionTest, ReceivePathProbingToPreferredAddressAtServer) {
  if (!GetParam().version.HasIetfQuicFrames()) {
    return;
  }
  ServerHandlePreferredAddressInit();

  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  EXPECT_CALL(visitor_, OnPacketReceived(_, _, _)).Times(0);

  // Process a probing packet to the server preferred address.
  std::unique_ptr<SerializedPacket> probing_packet = ConstructProbingPacket();
  std::unique_ptr<QuicReceivedPacket> received(ConstructReceivedPacket(
      QuicEncryptedPacket(probing_packet->encrypted_buffer,
                          probing_packet->encrypted_length),
      clock_.Now()));
  uint64_t num_probing_received =
      connection_.GetStats().num_connectivity_probing_received;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(AtLeast(1u))
      .WillOnce(Invoke([&]() {
        EXPECT_EQ(1u, writer_->path_response_frames().size());
        // Verify that the PATH_RESPONSE is sent from the original self address.
        EXPECT_EQ(kSelfAddress.host(), writer_->last_write_source_address());
        EXPECT_EQ(kPeerAddress, writer_->last_write_peer_address());
      }));
  ProcessReceivedPacket(kServerPreferredAddress, kPeerAddress, *received);

  EXPECT_EQ(num_probing_received + 1,
            connection_.GetStats().num_connectivity_probing_received);
  EXPECT_FALSE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kServerPreferredAddress, kPeerAddress));
  EXPECT_NE(kServerPreferredAddress, connection_.self_address());

  // Receiving another probing packet from a new client address.
  const QuicSocketAddress kNewPeerAddress(QuicIpAddress::Loopback4(),
                                          /*port=*/34567);
  probing_packet = ConstructProbingPacket();
  received.reset(ConstructReceivedPacket(
      QuicEncryptedPacket(probing_packet->encrypted_buffer,
                          probing_packet->encrypted_length),
      clock_.Now()));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(AtLeast(1u))
      .WillOnce(Invoke([&]() {
        EXPECT_EQ(1u, writer_->path_response_frames().size());
        EXPECT_EQ(1u, writer_->path_challenge_frames().size());
        EXPECT_EQ(kServerPreferredAddress.host(),
                  writer_->last_write_source_address());
        // The responses should be sent from preferred address given server
        // has not received packet on original address from the new client
        // address.
        EXPECT_EQ(kNewPeerAddress, writer_->last_write_peer_address());
      }));
  ProcessReceivedPacket(kServerPreferredAddress, kNewPeerAddress, *received);

  EXPECT_EQ(num_probing_received + 2,
            connection_.GetStats().num_connectivity_probing_received);
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(&connection_, kSelfAddress,
                                                    kNewPeerAddress));
  EXPECT_LT(0u, QuicConnectionPeer::BytesSentOnAlternativePath(&connection_));
  EXPECT_EQ(received->length(),
            QuicConnectionPeer::BytesReceivedOnAlternativePath(&connection_));
}

// Receive a padded PING packet with a port change on server side.
TEST_P(QuicConnectionTest, ReceivePaddedPingWithPortChangeAtServer) {
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  EXPECT_EQ(Perspective::IS_SERVER, connection_.perspective());
  if (version().SupportsAntiAmplificationLimit()) {
    QuicConnectionPeer::SetAddressValidated(&connection_);
  }

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is the same as direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());
  EXPECT_FALSE(connection_.effective_peer_address().IsInitialized());

  if (GetParam().version.UsesCryptoFrames()) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  } else {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  }
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress, kPeerAddress,
                                  ENCRYPTION_INITIAL);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  if (GetParam().version.HasIetfQuicFrames() ||
      GetQuicReloadableFlag(quic_ignore_gquic_probing)) {
    // In IETF version, a padded PING packet with port change is not taken as
    // connectivity probe.
    EXPECT_CALL(visitor_, GetHandshakeState())
        .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
    EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(1);
    EXPECT_CALL(visitor_, OnPacketReceived(_, _, _)).Times(0);
  } else {
    // In non-IETF version, process a padded PING packet from a new peer
    // address on server side is effectively receiving a connectivity probing.
    EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
    EXPECT_CALL(visitor_,
                OnPacketReceived(_, _, /*is_connectivity_probe=*/true))
        .Times(1);
  }
  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);

  QuicFrames frames;
  // Write a PING frame, which has no data payload.
  QuicPingFrame ping_frame;
  frames.push_back(QuicFrame(ping_frame));

  // Add padding to the rest of the packet.
  QuicPaddingFrame padding_frame;
  frames.push_back(QuicFrame(padding_frame));

  uint64_t num_probing_received =
      connection_.GetStats().num_connectivity_probing_received;

  ProcessFramesPacketWithAddresses(frames, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_INITIAL);

  if (GetParam().version.HasIetfQuicFrames() ||
      GetQuicReloadableFlag(quic_ignore_gquic_probing)) {
    // Padded PING with port changen is not considered as connectivity probe but
    // a PORT CHANGE.
    EXPECT_EQ(num_probing_received,
              connection_.GetStats().num_connectivity_probing_received);
    EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
    EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());
  } else {
    EXPECT_EQ(num_probing_received + 1,
              connection_.GetStats().num_connectivity_probing_received);
    EXPECT_EQ(kPeerAddress, connection_.peer_address());
    EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
  }

  if (GetParam().version.HasIetfQuicFrames() ||
      GetQuicReloadableFlag(quic_ignore_gquic_probing)) {
    EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(1);
  }
  // Process another packet with the old peer address on server side.
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress, kPeerAddress,
                                  ENCRYPTION_INITIAL);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
}

TEST_P(QuicConnectionTest, ReceiveReorderedPathProbingAtServer) {
  if (!GetParam().version.HasIetfQuicFrames() &&
      GetQuicReloadableFlag(quic_ignore_gquic_probing)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_SERVER);

  // Decrease packet number to simulate out-of-order packets.
  QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 4);

  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  if (!GetParam().version.HasIetfQuicFrames()) {
    EXPECT_CALL(visitor_,
                OnPacketReceived(_, _, /*is_connectivity_probe=*/true))
        .Times(1);
  } else {
    EXPECT_CALL(visitor_, OnPacketReceived(_, _, _)).Times(0);
  }

  // Process a padded PING packet from a new peer address on server side
  // is effectively receiving a connectivity probing, even if a newer packet has
  // been received before this one.
  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);

  std::unique_ptr<SerializedPacket> probing_packet = ConstructProbingPacket();
  std::unique_ptr<QuicReceivedPacket> received(ConstructReceivedPacket(
      QuicEncryptedPacket(probing_packet->encrypted_buffer,
                          probing_packet->encrypted_length),
      clock_.Now()));

  uint64_t num_probing_received =
      connection_.GetStats().num_connectivity_probing_received;
  ProcessReceivedPacket(kSelfAddress, kNewPeerAddress, *received);

  EXPECT_EQ(num_probing_received +
                (!version().HasIetfQuicFrames() &&
                         GetQuicReloadableFlag(quic_ignore_gquic_probing)
                     ? 0u
                     : 1u),
            connection_.GetStats().num_connectivity_probing_received);
  EXPECT_EQ((!version().HasIetfQuicFrames() &&
                     GetQuicReloadableFlag(quic_ignore_gquic_probing)
                 ? kNewPeerAddress
                 : kPeerAddress),
            connection_.peer_address());
  EXPECT_EQ((!version().HasIetfQuicFrames() &&
                     GetQuicReloadableFlag(quic_ignore_gquic_probing)
                 ? kNewPeerAddress
                 : kPeerAddress),
            connection_.effective_peer_address());
}

TEST_P(QuicConnectionTest, MigrateAfterProbingAtServer) {
  if (!GetParam().version.HasIetfQuicFrames() &&
      GetQuicReloadableFlag(quic_ignore_gquic_probing)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_SERVER);

  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  if (!GetParam().version.HasIetfQuicFrames()) {
    EXPECT_CALL(visitor_,
                OnPacketReceived(_, _, /*is_connectivity_probe=*/true))
        .Times(1);
  } else {
    EXPECT_CALL(visitor_, OnPacketReceived(_, _, _)).Times(0);
  }

  // Process a padded PING packet from a new peer address on server side
  // is effectively receiving a connectivity probing.
  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);

  std::unique_ptr<SerializedPacket> probing_packet = ConstructProbingPacket();
  std::unique_ptr<QuicReceivedPacket> received(ConstructReceivedPacket(
      QuicEncryptedPacket(probing_packet->encrypted_buffer,
                          probing_packet->encrypted_length),
      clock_.Now()));
  ProcessReceivedPacket(kSelfAddress, kNewPeerAddress, *received);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  // Process another non-probing packet with the new peer address on server
  // side will start peer migration.
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(1);

  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress,
                                  kNewPeerAddress, ENCRYPTION_INITIAL);
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());
}

TEST_P(QuicConnectionTest, ReceiveConnectivityProbingPacketAtClient) {
  if (!version().HasIetfQuicFrames() &&
      GetQuicReloadableFlag(quic_ignore_gquic_probing)) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  PathProbeTestInit(Perspective::IS_CLIENT);

  // Client takes all padded PING packet as speculative connectivity
  // probing packet, and reports to visitor.
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);

  std::unique_ptr<SerializedPacket> probing_packet = ConstructProbingPacket();
  std::unique_ptr<QuicReceivedPacket> received(ConstructReceivedPacket(
      QuicEncryptedPacket(probing_packet->encrypted_buffer,
                          probing_packet->encrypted_length),
      clock_.Now()));
  uint64_t num_probing_received =
      connection_.GetStats().num_connectivity_probing_received;
  ProcessReceivedPacket(kSelfAddress, kPeerAddress, *received);

  EXPECT_EQ(
      num_probing_received + (GetParam().version.HasIetfQuicFrames() ? 1u : 0u),
      connection_.GetStats().num_connectivity_probing_received);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
}

TEST_P(QuicConnectionTest, ReceiveConnectivityProbingResponseAtClient) {
  if (GetParam().version.HasIetfQuicFrames() ||
      GetQuicReloadableFlag(quic_ignore_gquic_probing)) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  PathProbeTestInit(Perspective::IS_CLIENT);

  // Process a padded PING packet with a different self address on client side
  // is effectively receiving a connectivity probing.
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  if (!GetParam().version.HasIetfQuicFrames()) {
    EXPECT_CALL(visitor_,
                OnPacketReceived(_, _, /*is_connectivity_probe=*/true))
        .Times(1);
  } else {
    EXPECT_CALL(visitor_, OnPacketReceived(_, _, _)).Times(0);
  }

  const QuicSocketAddress kNewSelfAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);

  std::unique_ptr<SerializedPacket> probing_packet = ConstructProbingPacket();
  std::unique_ptr<QuicReceivedPacket> received(ConstructReceivedPacket(
      QuicEncryptedPacket(probing_packet->encrypted_buffer,
                          probing_packet->encrypted_length),
      clock_.Now()));
  uint64_t num_probing_received =
      connection_.GetStats().num_connectivity_probing_received;
  ProcessReceivedPacket(kNewSelfAddress, kPeerAddress, *received);

  EXPECT_EQ(num_probing_received + 1,
            connection_.GetStats().num_connectivity_probing_received);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
}

TEST_P(QuicConnectionTest, PeerAddressChangeAtClient) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  set_perspective(Perspective::IS_CLIENT);
  EXPECT_EQ(Perspective::IS_CLIENT, connection_.perspective());

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is the same as direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());
  EXPECT_FALSE(connection_.effective_peer_address().IsInitialized());

  if (connection_.version().HasIetfQuicFrames()) {
    // Verify the 2nd packet from unknown server address gets dropped.
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(1);
  } else if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(2);
  } else {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(2);
  }
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress, kPeerAddress,
                                  ENCRYPTION_INITIAL);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress,
                                  kNewPeerAddress, ENCRYPTION_INITIAL);
  if (connection_.version().HasIetfQuicFrames()) {
    // IETF QUIC disallows server initiated address change.
    EXPECT_EQ(kPeerAddress, connection_.peer_address());
    EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
  } else {
    EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
    EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());
  }
}

TEST_P(QuicConnectionTest, NoNormalizedPeerAddressChangeAtClient) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  QuicIpAddress peer_ip;
  peer_ip.FromString("1.1.1.1");

  QuicSocketAddress peer_addr = QuicSocketAddress(peer_ip, /*port=*/443);
  QuicSocketAddress dualstack_peer_addr =
      QuicSocketAddress(peer_addr.host().DualStacked(), peer_addr.port());

  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_)).Times(AnyNumber());
  set_perspective(Perspective::IS_CLIENT);
  EXPECT_EQ(Perspective::IS_CLIENT, connection_.perspective());

  QuicConnectionPeer::SetDirectPeerAddress(&connection_, dualstack_peer_addr);

  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress, peer_addr,
                                  ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.connected());

  if (GetQuicReloadableFlag(quic_test_peer_addr_change_after_normalize)) {
    EXPECT_EQ(0u, connection_.GetStats().packets_dropped);
  } else {
    EXPECT_EQ(1u, connection_.GetStats().packets_dropped);
  }
}

TEST_P(QuicConnectionTest, ServerAddressChangesToKnownAddress) {
  if (!connection_.version().HasIetfQuicFrames()) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  set_perspective(Perspective::IS_CLIENT);
  EXPECT_EQ(Perspective::IS_CLIENT, connection_.perspective());

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is the same as direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());
  EXPECT_FALSE(connection_.effective_peer_address().IsInitialized());

  // Verify all 3 packets get processed.
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(3);
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress, kPeerAddress,
                                  ENCRYPTION_INITIAL);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  // Process another packet with a different but known server address.
  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);
  connection_.AddKnownServerAddress(kNewPeerAddress);
  EXPECT_CALL(visitor_, OnConnectionMigration(_)).Times(0);
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress,
                                  kNewPeerAddress, ENCRYPTION_INITIAL);
  // Verify peer address does not change.
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  // Process 3rd packet from previous server address.
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress, kPeerAddress,
                                  ENCRYPTION_INITIAL);
  // Verify peer address does not change.
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
}

TEST_P(QuicConnectionTest,
       PeerAddressChangesToPreferredAddressBeforeClientInitiates) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  ASSERT_EQ(Perspective::IS_CLIENT, connection_.perspective());
  ASSERT_TRUE(connection_.self_address().host().IsIPv6());
  const QuicConnectionId connection_id = TestConnectionId(17);
  const StatelessResetToken reset_token =
      QuicUtils::GenerateStatelessResetToken(connection_id);

  connection_.CreateConnectionIdManager();

  connection_.SendCryptoStreamData();
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame = InitAckFrame(1);
  // Received ACK for packet 1.
  ProcessFramePacketAtLevel(1, QuicFrame(&frame), ENCRYPTION_INITIAL);
  // Discard INITIAL key.
  connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);

  QuicConfig config;
  QuicConfigPeer::SetReceivedStatelessResetToken(&config,
                                                 kTestStatelessResetToken);
  QuicConfigPeer::SetReceivedAlternateServerAddress(&config,
                                                    kServerPreferredAddress);
  QuicConfigPeer::SetPreferredAddressConnectionIdAndToken(
      &config, connection_id, reset_token);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
  ASSERT_TRUE(
      QuicConnectionPeer::GetReceivedServerPreferredAddress(&connection_)
          .IsInitialized());
  EXPECT_EQ(
      kServerPreferredAddress,
      QuicConnectionPeer::GetReceivedServerPreferredAddress(&connection_));

  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(0);
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress,
                                  kServerPreferredAddress, ENCRYPTION_INITIAL);
}

TEST_P(QuicConnectionTest, MaxPacketSize) {
  EXPECT_EQ(Perspective::IS_CLIENT, connection_.perspective());
  EXPECT_EQ(1250u, connection_.max_packet_length());
}

TEST_P(QuicConnectionTest, PeerLowersMaxPacketSize) {
  EXPECT_EQ(Perspective::IS_CLIENT, connection_.perspective());

  // SetFromConfig is always called after construction from InitializeSession.
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  constexpr uint32_t kTestMaxPacketSize = 1233u;
  QuicConfig config;
  QuicConfigPeer::SetReceivedMaxPacketSize(&config, kTestMaxPacketSize);
  connection_.SetFromConfig(config);

  EXPECT_EQ(kTestMaxPacketSize, connection_.max_packet_length());
}

TEST_P(QuicConnectionTest, PeerCannotRaiseMaxPacketSize) {
  EXPECT_EQ(Perspective::IS_CLIENT, connection_.perspective());

  // SetFromConfig is always called after construction from InitializeSession.
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  constexpr uint32_t kTestMaxPacketSize = 1450u;
  QuicConfig config;
  QuicConfigPeer::SetReceivedMaxPacketSize(&config, kTestMaxPacketSize);
  connection_.SetFromConfig(config);

  EXPECT_EQ(kDefaultMaxPacketSize, connection_.max_packet_length());
}

TEST_P(QuicConnectionTest, SmallerServerMaxPacketSize) {
  TestConnection connection(TestConnectionId(), kSelfAddress, kPeerAddress,
                            helper_.get(), alarm_factory_.get(), writer_.get(),
                            Perspective::IS_SERVER, version(),
                            connection_id_generator_);
  EXPECT_EQ(Perspective::IS_SERVER, connection.perspective());
  EXPECT_EQ(1000u, connection.max_packet_length());
}

TEST_P(QuicConnectionTest, LowerServerResponseMtuTest) {
  set_perspective(Perspective::IS_SERVER);
  connection_.SetMaxPacketLength(1000);
  EXPECT_EQ(1000u, connection_.max_packet_length());

  SetQuicFlag(quic_use_lower_server_response_mtu_for_test, true);
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(::testing::AtMost(1));
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(::testing::AtMost(1));
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_INITIAL);
  EXPECT_EQ(1250u, connection_.max_packet_length());
}

TEST_P(QuicConnectionTest, IncreaseServerMaxPacketSize) {
  set_perspective(Perspective::IS_SERVER);
  connection_.SetMaxPacketLength(1000);

  QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  header.version_flag = true;
  header.packet_number = QuicPacketNumber(12);

  if (QuicVersionHasLongHeaderLengths(
          peer_framer_.version().transport_version)) {
    header.long_packet_type = INITIAL;
    header.retry_token_length_length = quiche::VARIABLE_LENGTH_INTEGER_LENGTH_1;
    header.length_length = quiche::VARIABLE_LENGTH_INTEGER_LENGTH_2;
  }

  QuicFrames frames;
  QuicPaddingFrame padding;
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    frames.push_back(QuicFrame(&crypto_frame_));
  } else {
    frames.push_back(QuicFrame(frame1_));
  }
  frames.push_back(QuicFrame(padding));
  std::unique_ptr<QuicPacket> packet(ConstructPacket(header, frames));
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length =
      peer_framer_.EncryptPayload(ENCRYPTION_INITIAL, QuicPacketNumber(12),
                                  *packet, buffer, kMaxOutgoingPacketSize);
  EXPECT_EQ(kMaxOutgoingPacketSize,
            encrypted_length +
                (connection_.version().KnowsWhichDecrypterToUse() ? 0 : 4));

  framer_.set_version(version());
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(1);
  } else {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  }
  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(buffer, encrypted_length, clock_.ApproximateNow(),
                         false));

  EXPECT_EQ(kMaxOutgoingPacketSize,
            connection_.max_packet_length() +
                (connection_.version().KnowsWhichDecrypterToUse() ? 0 : 4));
}

TEST_P(QuicConnectionTest, IncreaseServerMaxPacketSizeWhileWriterLimited) {
  const QuicByteCount lower_max_packet_size = 1240;
  writer_->set_max_packet_size(lower_max_packet_size);
  set_perspective(Perspective::IS_SERVER);
  connection_.SetMaxPacketLength(1000);
  EXPECT_EQ(1000u, connection_.max_packet_length());

  QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  header.version_flag = true;
  header.packet_number = QuicPacketNumber(12);

  if (QuicVersionHasLongHeaderLengths(
          peer_framer_.version().transport_version)) {
    header.long_packet_type = INITIAL;
    header.retry_token_length_length = quiche::VARIABLE_LENGTH_INTEGER_LENGTH_1;
    header.length_length = quiche::VARIABLE_LENGTH_INTEGER_LENGTH_2;
  }

  QuicFrames frames;
  QuicPaddingFrame padding;
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    frames.push_back(QuicFrame(&crypto_frame_));
  } else {
    frames.push_back(QuicFrame(frame1_));
  }
  frames.push_back(QuicFrame(padding));
  std::unique_ptr<QuicPacket> packet(ConstructPacket(header, frames));
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length =
      peer_framer_.EncryptPayload(ENCRYPTION_INITIAL, QuicPacketNumber(12),
                                  *packet, buffer, kMaxOutgoingPacketSize);
  EXPECT_EQ(kMaxOutgoingPacketSize,
            encrypted_length +
                (connection_.version().KnowsWhichDecrypterToUse() ? 0 : 4));

  framer_.set_version(version());
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(1);
  } else {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  }
  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(buffer, encrypted_length, clock_.ApproximateNow(),
                         false));

  // Here, the limit imposed by the writer is lower than the size of the packet
  // received, so the writer max packet size is used.
  EXPECT_EQ(lower_max_packet_size, connection_.max_packet_length());
}

TEST_P(QuicConnectionTest, LimitMaxPacketSizeByWriter) {
  const QuicByteCount lower_max_packet_size = 1240;
  writer_->set_max_packet_size(lower_max_packet_size);

  static_assert(lower_max_packet_size < kDefaultMaxPacketSize,
                "Default maximum packet size is too low");
  connection_.SetMaxPacketLength(kDefaultMaxPacketSize);

  EXPECT_EQ(lower_max_packet_size, connection_.max_packet_length());
}

TEST_P(QuicConnectionTest, LimitMaxPacketSizeByWriterForNewConnection) {
  const QuicConnectionId connection_id = TestConnectionId(17);
  const QuicByteCount lower_max_packet_size = 1240;
  writer_->set_max_packet_size(lower_max_packet_size);
  TestConnection connection(connection_id, kSelfAddress, kPeerAddress,
                            helper_.get(), alarm_factory_.get(), writer_.get(),
                            Perspective::IS_CLIENT, version(),
                            connection_id_generator_);
  EXPECT_EQ(Perspective::IS_CLIENT, connection.perspective());
  EXPECT_EQ(lower_max_packet_size, connection.max_packet_length());
}

TEST_P(QuicConnectionTest, PacketsInOrder) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessPacket(1);
  EXPECT_EQ(QuicPacketNumber(1u), LargestAcked(connection_.ack_frame()));
  EXPECT_EQ(1u, connection_.ack_frame().packets.NumIntervals());

  ProcessPacket(2);
  EXPECT_EQ(QuicPacketNumber(2u), LargestAcked(connection_.ack_frame()));
  EXPECT_EQ(1u, connection_.ack_frame().packets.NumIntervals());

  ProcessPacket(3);
  EXPECT_EQ(QuicPacketNumber(3u), LargestAcked(connection_.ack_frame()));
  EXPECT_EQ(1u, connection_.ack_frame().packets.NumIntervals());
}

TEST_P(QuicConnectionTest, PacketsOutOfOrder) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessPacket(3);
  EXPECT_EQ(QuicPacketNumber(3u), LargestAcked(connection_.ack_frame()));
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(2);
  EXPECT_EQ(QuicPacketNumber(3u), LargestAcked(connection_.ack_frame()));
  EXPECT_FALSE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(1);
  EXPECT_EQ(QuicPacketNumber(3u), LargestAcked(connection_.ack_frame()));
  EXPECT_FALSE(IsMissing(2));
  EXPECT_FALSE(IsMissing(1));
}

TEST_P(QuicConnectionTest, DuplicatePacket) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessPacket(3);
  EXPECT_EQ(QuicPacketNumber(3u), LargestAcked(connection_.ack_frame()));
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  // Send packet 3 again, but do not set the expectation that
  // the visitor OnStreamFrame() will be called.
  ProcessDataPacket(3);
  EXPECT_EQ(QuicPacketNumber(3u), LargestAcked(connection_.ack_frame()));
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));
}

TEST_P(QuicConnectionTest, PacketsOutOfOrderWithAdditionsAndLeastAwaiting) {
  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessPacket(3);
  EXPECT_EQ(QuicPacketNumber(3u), LargestAcked(connection_.ack_frame()));
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(2);
  EXPECT_EQ(QuicPacketNumber(3u), LargestAcked(connection_.ack_frame()));
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(5);
  EXPECT_EQ(QuicPacketNumber(5u), LargestAcked(connection_.ack_frame()));
  EXPECT_TRUE(IsMissing(1));
  EXPECT_TRUE(IsMissing(4));

  // Pretend at this point the client has gotten acks for 2 and 3 and 1 is a
  // packet the peer will not retransmit.  It indicates this by sending 'least
  // awaiting' is 4.  The connection should then realize 1 will not be
  // retransmitted, and will remove it from the missing list.
  QuicAckFrame frame = InitAckFrame(1);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _));
  ProcessAckPacket(6, &frame);

  // Force an ack to be sent.
  SendAckPacketToPeer();
  EXPECT_TRUE(IsMissing(4));
}

TEST_P(QuicConnectionTest, RejectUnencryptedStreamData) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (!IsDefaultTestConfiguration() ||
      VersionHasIetfQuicFrames(version().transport_version)) {
    return;
  }

  // Process an unencrypted packet from the non-crypto stream.
  frame1_.stream_id = 3;
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));
  EXPECT_QUIC_PEER_BUG(ProcessDataPacketAtLevel(1, false, ENCRYPTION_INITIAL),
                       "");
  TestConnectionCloseQuicErrorCode(QUIC_UNENCRYPTED_STREAM_DATA);
}

TEST_P(QuicConnectionTest, OutOfOrderReceiptCausesAckSend) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessPacket(3);
  // Should not cause an ack.
  EXPECT_EQ(0u, writer_->packets_write_attempts());

  ProcessPacket(2);
  // Should ack immediately, since this fills the last hole.
  EXPECT_EQ(1u, writer_->packets_write_attempts());

  ProcessPacket(1);
  // Should ack immediately, since this fills the last hole.
  EXPECT_EQ(2u, writer_->packets_write_attempts());

  ProcessPacket(4);
  // Should not cause an ack.
  EXPECT_EQ(2u, writer_->packets_write_attempts());
}

TEST_P(QuicConnectionTest, OutOfOrderAckReceiptCausesNoAck) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  SendStreamDataToPeer(1, "foo", 0, NO_FIN, nullptr);
  SendStreamDataToPeer(1, "bar", 3, NO_FIN, nullptr);
  EXPECT_EQ(2u, writer_->packets_write_attempts());

  QuicAckFrame ack1 = InitAckFrame(1);
  QuicAckFrame ack2 = InitAckFrame(2);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    EXPECT_CALL(visitor_, OnOneRttPacketAcknowledged()).Times(1);
  }
  ProcessAckPacket(2, &ack2);
  // Should ack immediately since we have missing packets.
  EXPECT_EQ(2u, writer_->packets_write_attempts());

  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    EXPECT_CALL(visitor_, OnOneRttPacketAcknowledged()).Times(0);
  }
  ProcessAckPacket(1, &ack1);
  // Should not ack an ack filling a missing packet.
  EXPECT_EQ(2u, writer_->packets_write_attempts());
}

TEST_P(QuicConnectionTest, AckReceiptCausesAckSend) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  QuicPacketNumber original, second;

  QuicByteCount packet_size =
      SendStreamDataToPeer(3, "foo", 0, NO_FIN, &original);  // 1st packet.
  SendStreamDataToPeer(3, "bar", 3, NO_FIN, &second);        // 2nd packet.

  QuicAckFrame frame = InitAckFrame({{second, second + 1}});
  // First nack triggers early retransmit.
  LostPacketVector lost_packets;
  lost_packets.push_back(LostPacket(original, kMaxOutgoingPacketSize));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(lost_packets),
                      Return(LossDetectionInterface::DetectionStats())));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicPacketNumber retransmission;
  // Packet 1 is short header for IETF QUIC because the encryption level
  // switched to ENCRYPTION_FORWARD_SECURE in SendStreamDataToPeer.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, packet_size, _))
      .WillOnce(SaveArg<2>(&retransmission));

  ProcessAckPacket(&frame);

  QuicAckFrame frame2 = ConstructAckFrame(retransmission, original);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  ProcessAckPacket(&frame2);

  // Now if the peer sends an ack which still reports the retransmitted packet
  // as missing, that will bundle an ack with data after two acks in a row
  // indicate the high water mark needs to be raised.
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, _, _, _, HAS_RETRANSMITTABLE_DATA));
  connection_.SendStreamDataWithString(3, "foo", 6, NO_FIN);
  // No ack sent.
  size_t padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + 1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->stream_frames().size());

  // No more packet loss for the rest of the test.
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .Times(AnyNumber());
  ProcessAckPacket(&frame2);
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, _, _, _, HAS_RETRANSMITTABLE_DATA));
  connection_.SendStreamDataWithString(3, "foofoofoo", 9, NO_FIN);
  // Ack bundled.
  // Do not ACK acks.
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_TRUE(writer_->ack_frames().empty());

  // But an ack with no missing packets will not send an ack.
  AckPacket(original, &frame2);
  ProcessAckPacket(&frame2);
  ProcessAckPacket(&frame2);
}

TEST_P(QuicConnectionTest, AckFrequencyUpdatedFromAckFrequencyFrame) {
  if (!GetParam().version.HasIetfQuicFrames()) {
    return;
  }
  connection_.set_can_receive_ack_frequency_frame();

  // Expect 13 acks, every 3rd packet including the first packet with
  // AckFrequencyFrame.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(13);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  QuicAckFrequencyFrame ack_frequency_frame;
  ack_frequency_frame.packet_tolerance = 3;
  ProcessFramePacketAtLevel(1, QuicFrame(&ack_frequency_frame),
                            ENCRYPTION_FORWARD_SECURE);

  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(38);
  // Receives packets 2 - 39.
  for (size_t i = 2; i <= 39; ++i) {
    ProcessDataPacket(i);
  }
}

TEST_P(QuicConnectionTest, AckDecimationReducesAcks) {
  const size_t kMinRttMs = 40;
  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(kMinRttMs),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  EXPECT_CALL(visitor_, OnAckNeedsRetransmittableFrame()).Times(AnyNumber());

  // Start ack decimation from 10th packet.
  connection_.set_min_received_before_ack_decimation(10);

  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(30);

  // Expect 6 acks: 5 acks between packets 1-10, and ack at 20.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(6);
  // Receives packets 1 - 29.
  for (size_t i = 1; i <= 29; ++i) {
    ProcessDataPacket(i);
  }

  // We now receive the 30th packet, and so we send an ack.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  ProcessDataPacket(30);
}

TEST_P(QuicConnectionTest, AckNeedsRetransmittableFrames) {
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(99);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(19);
  // Receives packets 1 - 39.
  for (size_t i = 1; i <= 39; ++i) {
    ProcessDataPacket(i);
  }
  // Receiving Packet 40 causes 20th ack to send. Session is informed and adds
  // WINDOW_UPDATE.
  EXPECT_CALL(visitor_, OnAckNeedsRetransmittableFrame())
      .WillOnce(Invoke([this]() {
        connection_.SendControlFrame(QuicFrame(QuicWindowUpdateFrame(1, 0, 0)));
      }));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  EXPECT_EQ(0u, writer_->window_update_frames().size());
  ProcessDataPacket(40);
  EXPECT_EQ(1u, writer_->window_update_frames().size());

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(9);
  // Receives packets 41 - 59.
  for (size_t i = 41; i <= 59; ++i) {
    ProcessDataPacket(i);
  }
  // Send a packet containing stream frame.
  SendStreamDataToPeer(
      QuicUtils::GetFirstBidirectionalStreamId(
          connection_.version().transport_version, Perspective::IS_CLIENT),
      "bar", 0, NO_FIN, nullptr);

  // Session will not be informed until receiving another 20 packets.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(19);
  for (size_t i = 60; i <= 98; ++i) {
    ProcessDataPacket(i);
    EXPECT_EQ(0u, writer_->window_update_frames().size());
  }
  // Session does not add a retransmittable frame.
  EXPECT_CALL(visitor_, OnAckNeedsRetransmittableFrame())
      .WillOnce(Invoke([this]() {
        connection_.SendControlFrame(QuicFrame(QuicPingFrame(1)));
      }));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  EXPECT_EQ(0u, writer_->ping_frames().size());
  ProcessDataPacket(99);
  EXPECT_EQ(0u, writer_->window_update_frames().size());
  // A ping frame will be added.
  EXPECT_EQ(1u, writer_->ping_frames().size());
}

TEST_P(QuicConnectionTest, AckNeedsRetransmittableFramesAfterPto) {
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(kEACK);
  config.SetConnectionOptionsToSend(connection_options);
  connection_.SetFromConfig(config);

  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.OnHandshakeComplete();

  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(10);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(4);
  // Receive packets 1 - 9.
  for (size_t i = 1; i <= 9; ++i) {
    ProcessDataPacket(i);
  }

  // Send a ping and fire the retransmission alarm.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
  SendPing();
  QuicTime retransmission_time =
      connection_.GetRetransmissionAlarm()->deadline();
  clock_.AdvanceTime(retransmission_time - clock_.Now());
  connection_.GetRetransmissionAlarm()->Fire();
  ASSERT_LT(0u, manager_->GetConsecutivePtoCount());

  // Process a packet, which requests a retransmittable frame be bundled
  // with the ACK.
  EXPECT_CALL(visitor_, OnAckNeedsRetransmittableFrame())
      .WillOnce(Invoke([this]() {
        connection_.SendControlFrame(QuicFrame(QuicWindowUpdateFrame(1, 0, 0)));
      }));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  ProcessDataPacket(11);
  EXPECT_EQ(1u, writer_->window_update_frames().size());
}

TEST_P(QuicConnectionTest, TooManySentPackets) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  QuicPacketCount max_tracked_packets = 50;
  QuicConnectionPeer::SetMaxTrackedPackets(&connection_, max_tracked_packets);

  const int num_packets = max_tracked_packets + 5;

  for (int i = 0; i < num_packets; ++i) {
    SendStreamDataToPeer(1, "foo", 3 * i, NO_FIN, nullptr);
  }

  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));

  ProcessFramePacket(QuicFrame(QuicPingFrame()));

  TestConnectionCloseQuicErrorCode(QUIC_TOO_MANY_OUTSTANDING_SENT_PACKETS);
}

TEST_P(QuicConnectionTest, LargestObservedLower) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  SendStreamDataToPeer(1, "foo", 0, NO_FIN, nullptr);
  SendStreamDataToPeer(1, "bar", 3, NO_FIN, nullptr);
  SendStreamDataToPeer(1, "eep", 6, NO_FIN, nullptr);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));

  // Start out saying the largest observed is 2.
  QuicAckFrame frame1 = InitAckFrame(1);
  QuicAckFrame frame2 = InitAckFrame(2);
  ProcessAckPacket(&frame2);

  EXPECT_CALL(visitor_, OnCanWrite()).Times(AnyNumber());
  ProcessAckPacket(&frame1);
}

TEST_P(QuicConnectionTest, AckUnsentData) {
  // Ack a packet which has not been sent.
  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AtLeast(1));
  QuicAckFrame frame = InitAckFrame(1);
  EXPECT_CALL(visitor_, OnCanWrite()).Times(0);
  ProcessAckPacket(&frame);
  TestConnectionCloseQuicErrorCode(QUIC_INVALID_ACK_DATA);
}

TEST_P(QuicConnectionTest, BasicSending) {
  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  const QuicConnectionStats& stats = connection_.GetStats();
  EXPECT_FALSE(stats.first_decrypted_packet.IsInitialized());
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacket(1);
  EXPECT_EQ(QuicPacketNumber(1), stats.first_decrypted_packet);
  QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 2);
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, &last_packet);  // Packet 1
  EXPECT_EQ(QuicPacketNumber(1u), last_packet);
  SendAckPacketToPeer();  // Packet 2

  SendAckPacketToPeer();  // Packet 3

  SendStreamDataToPeer(1, "bar", 3, NO_FIN, &last_packet);  // Packet 4
  EXPECT_EQ(QuicPacketNumber(4u), last_packet);
  SendAckPacketToPeer();  // Packet 5

  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));

  // Peer acks up to packet 3.
  QuicAckFrame frame = InitAckFrame(3);
  ProcessAckPacket(&frame);
  SendAckPacketToPeer();  // Packet 6

  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));

  // Peer acks up to packet 4, the last packet.
  QuicAckFrame frame2 = InitAckFrame(6);
  ProcessAckPacket(&frame2);  // Acks don't instigate acks.

  // Verify that we did not send an ack.
  EXPECT_EQ(QuicPacketNumber(6u), writer_->header().packet_number);

  // If we force an ack, we shouldn't change our retransmit state.
  SendAckPacketToPeer();  // Packet 7

  // But if we send more data it should.
  SendStreamDataToPeer(1, "eep", 6, NO_FIN, &last_packet);  // Packet 8
  EXPECT_EQ(QuicPacketNumber(8u), last_packet);
  SendAckPacketToPeer();  // Packet 9
  EXPECT_EQ(QuicPacketNumber(1), stats.first_decrypted_packet);
}

// QuicConnection should record the packet sent-time prior to sending the
// packet.
TEST_P(QuicConnectionTest, RecordSentTimeBeforePacketSent) {
  // We're using a MockClock for the tests, so we have complete control over the
  // time.
  // Our recorded timestamp for the last packet sent time will be passed in to
  // the send_algorithm.  Make sure that it is set to the correct value.
  QuicTime actual_recorded_send_time = QuicTime::Zero();
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(SaveArg<0>(&actual_recorded_send_time));

  // First send without any pause and check the result.
  QuicTime expected_recorded_send_time = clock_.Now();
  connection_.SendStreamDataWithString(1, "foo", 0, NO_FIN);
  EXPECT_EQ(expected_recorded_send_time, actual_recorded_send_time)
      << "Expected time = " << expected_recorded_send_time.ToDebuggingValue()
      << ".  Actual time = " << actual_recorded_send_time.ToDebuggingValue();

  // Now pause during the write, and check the results.
  actual_recorded_send_time = QuicTime::Zero();
  const QuicTime::Delta write_pause_time_delta =
      QuicTime::Delta::FromMilliseconds(5000);
  SetWritePauseTimeDelta(write_pause_time_delta);
  expected_recorded_send_time = clock_.Now();

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(SaveArg<0>(&actual_recorded_send_time));
  connection_.SendStreamDataWithString(2, "baz", 0, NO_FIN);
  EXPECT_EQ(expected_recorded_send_time, actual_recorded_send_time)
      << "Expected time = " << expected_recorded_send_time.ToDebuggingValue()
      << ".  Actual time = " << actual_recorded_send_time.ToDebuggingValue();
}

TEST_P(QuicConnectionTest, ConnectionStatsRetransmission_WithRetransmissions) {
  // Send two stream frames in 1 packet by queueing them.
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);

  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    connection_.SaveAndSendStreamData(
        GetNthClientInitiatedStreamId(1, connection_.transport_version()),
        "helloworld", 0, NO_FIN, PTO_RETRANSMISSION);
    connection_.SaveAndSendStreamData(
        GetNthClientInitiatedStreamId(2, connection_.transport_version()),
        "helloworld", 0, NO_FIN, LOSS_RETRANSMISSION);
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  }

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  EXPECT_EQ(2u, writer_->frame_count());
  for (auto& frame : writer_->stream_frames()) {
    EXPECT_EQ(frame->data_length, 10u);
  }

  ASSERT_EQ(connection_.GetStats().packets_retransmitted, 1u);
  ASSERT_GE(connection_.GetStats().bytes_retransmitted, 20u);
}

TEST_P(QuicConnectionTest, ConnectionStatsRetransmission_WithMixedFrames) {
  // Send two stream frames in 1 packet by queueing them.
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);

  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    // First frame is retransmission. Second is NOT_RETRANSMISSION but the
    // packet retains the PTO_RETRANSMISSION type.
    connection_.SaveAndSendStreamData(
        GetNthClientInitiatedStreamId(1, connection_.transport_version()),
        "helloworld", 0, NO_FIN, PTO_RETRANSMISSION);
    connection_.SaveAndSendStreamData(
        GetNthClientInitiatedStreamId(2, connection_.transport_version()),
        "helloworld", 0, NO_FIN, NOT_RETRANSMISSION);
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  }

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  EXPECT_EQ(2u, writer_->frame_count());
  for (auto& frame : writer_->stream_frames()) {
    EXPECT_EQ(frame->data_length, 10u);
  }

  ASSERT_EQ(connection_.GetStats().packets_retransmitted, 1u);
  ASSERT_GE(connection_.GetStats().bytes_retransmitted, 10u);
}

TEST_P(QuicConnectionTest, ConnectionStatsRetransmission_NoRetransmission) {
  // Send two stream frames in 1 packet by queueing them.
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);

  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    // Both frames are NOT_RETRANSMISSION
    connection_.SaveAndSendStreamData(
        GetNthClientInitiatedStreamId(1, connection_.transport_version()),
        "helloworld", 0, NO_FIN, NOT_RETRANSMISSION);
    connection_.SaveAndSendStreamData(
        GetNthClientInitiatedStreamId(2, connection_.transport_version()),
        "helloworld", 0, NO_FIN, NOT_RETRANSMISSION);
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  }

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  EXPECT_EQ(2u, writer_->frame_count());
  ASSERT_EQ(connection_.GetStats().packets_retransmitted, 0u);
  ASSERT_EQ(connection_.GetStats().bytes_retransmitted, 0u);
}

TEST_P(QuicConnectionTest, FramePacking) {
  // Send two stream frames in 1 packet by queueing them.
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    connection_.SendStreamData3();
    connection_.SendStreamData5();
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  }
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure it's an ack and two stream frames from
  // two different streams.
  EXPECT_EQ(2u, writer_->frame_count());
  EXPECT_TRUE(writer_->stop_waiting_frames().empty());

  EXPECT_TRUE(writer_->ack_frames().empty());

  ASSERT_EQ(2u, writer_->stream_frames().size());
  EXPECT_EQ(GetNthClientInitiatedStreamId(1, connection_.transport_version()),
            writer_->stream_frames()[0]->stream_id);
  EXPECT_EQ(GetNthClientInitiatedStreamId(2, connection_.transport_version()),
            writer_->stream_frames()[1]->stream_id);
}

TEST_P(QuicConnectionTest, FramePackingNonCryptoThenCrypto) {
  // Send two stream frames (one non-crypto, then one crypto) in 2 packets by
  // queueing them.
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    connection_.SendStreamData3();
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
    // Set the crypters for INITIAL packets in the TestPacketWriter.
    if (!connection_.version().KnowsWhichDecrypterToUse()) {
      writer_->framer()->framer()->SetAlternativeDecrypter(
          ENCRYPTION_INITIAL,
          std::make_unique<NullDecrypter>(Perspective::IS_SERVER), false);
    }
    connection_.SendCryptoStreamData();
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  }
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure it contains a crypto stream frame.
  EXPECT_LE(2u, writer_->frame_count());
  ASSERT_LE(1u, writer_->padding_frames().size());
  if (!QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    ASSERT_EQ(1u, writer_->stream_frames().size());
    EXPECT_EQ(QuicUtils::GetCryptoStreamId(connection_.transport_version()),
              writer_->stream_frames()[0]->stream_id);
  } else {
    EXPECT_LE(1u, writer_->crypto_frames().size());
  }
}

TEST_P(QuicConnectionTest, FramePackingCryptoThenNonCrypto) {
  // Send two stream frames (one crypto, then one non-crypto) in 2 packets by
  // queueing them.
  {
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    connection_.SendCryptoStreamDataAtLevel(ENCRYPTION_FORWARD_SECURE);
    connection_.SendStreamData3();
  }
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure it's the stream frame from stream 3.
  size_t padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + 1u, writer_->frame_count());
  ASSERT_EQ(1u, writer_->stream_frames().size());
  EXPECT_EQ(GetNthClientInitiatedStreamId(1, connection_.transport_version()),
            writer_->stream_frames()[0]->stream_id);
}

TEST_P(QuicConnectionTest, FramePackingAckResponse) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  // Process a data packet to queue up a pending ack.
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(1);
  } else {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  }
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_INITIAL);

  QuicPacketNumber last_packet;
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    connection_.SendCryptoDataWithString("foo", 0);
  } else {
    SendStreamDataToPeer(
        QuicUtils::GetCryptoStreamId(connection_.transport_version()), "foo", 0,
        NO_FIN, &last_packet);
  }
  // Verify ack is bundled with outging packet.
  EXPECT_FALSE(writer_->ack_frames().empty());

  EXPECT_CALL(visitor_, OnCanWrite())
      .WillOnce(DoAll(IgnoreResult(InvokeWithoutArgs(
                          &connection_, &TestConnection::SendStreamData3)),
                      IgnoreResult(InvokeWithoutArgs(
                          &connection_, &TestConnection::SendStreamData5))));

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);

  // Process a data packet to cause the visitor's OnCanWrite to be invoked.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  peer_framer_.SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
  SetDecrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_FORWARD_SECURE));
  ForceWillingAndAbleToWriteOnceForDeferSending();
  ProcessDataPacket(2);

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure it's an ack and two stream frames from
  // two different streams.
  EXPECT_EQ(3u, writer_->frame_count());
  EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  EXPECT_FALSE(writer_->ack_frames().empty());
  ASSERT_EQ(2u, writer_->stream_frames().size());
  EXPECT_EQ(GetNthClientInitiatedStreamId(1, connection_.transport_version()),
            writer_->stream_frames()[0]->stream_id);
  EXPECT_EQ(GetNthClientInitiatedStreamId(2, connection_.transport_version()),
            writer_->stream_frames()[1]->stream_id);
}

TEST_P(QuicConnectionTest, FramePackingSendv) {
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));

  QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      connection_.transport_version(), Perspective::IS_CLIENT);
  connection_.SaveAndSendStreamData(stream_id, "ABCDEF", 0, NO_FIN);

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure multiple iovector blocks have
  // been packed into a single stream frame from one stream.
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_EQ(0u, writer_->padding_frames().size());
  QuicStreamFrame* frame = writer_->stream_frames()[0].get();
  EXPECT_EQ(stream_id, frame->stream_id);
  EXPECT_EQ("ABCDEF",
            absl::string_view(frame->data_buffer, frame->data_length));
}

TEST_P(QuicConnectionTest, FramePackingSendvQueued) {
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));

  BlockOnNextWrite();
  QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      connection_.transport_version(), Perspective::IS_CLIENT);
  connection_.SaveAndSendStreamData(stream_id, "ABCDEF", 0, NO_FIN);

  EXPECT_EQ(1u, connection_.NumQueuedPackets());
  EXPECT_TRUE(connection_.HasQueuedData());

  // Unblock the writes and actually send.
  writer_->SetWritable();
  connection_.OnCanWrite();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());

  // Parse the last packet and ensure it's one stream frame from one stream.
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_EQ(0u, writer_->padding_frames().size());
  QuicStreamFrame* frame = writer_->stream_frames()[0].get();
  EXPECT_EQ(stream_id, frame->stream_id);
  EXPECT_EQ("ABCDEF",
            absl::string_view(frame->data_buffer, frame->data_length));
}

TEST_P(QuicConnectionTest, SendingZeroBytes) {
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  // Send a zero byte write with a fin using writev.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  QuicStreamId stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      connection_.transport_version(), Perspective::IS_CLIENT);
  connection_.SaveAndSendStreamData(stream_id, {}, 0, FIN);

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Padding frames are added by v99 to ensure a minimum packet size.
  size_t extra_padding_frames = 0;
  if (GetParam().version.HasHeaderProtection()) {
    extra_padding_frames = 1;
  }

  // Parse the last packet and ensure it's one stream frame from one stream.
  EXPECT_EQ(1u + extra_padding_frames, writer_->frame_count());
  EXPECT_EQ(extra_padding_frames, writer_->padding_frames().size());
  ASSERT_EQ(1u, writer_->stream_frames().size());
  EXPECT_EQ(stream_id, writer_->stream_frames()[0]->stream_id);
  EXPECT_TRUE(writer_->stream_frames()[0]->fin);
}

TEST_P(QuicConnectionTest, LargeSendWithPendingAck) {
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  // Set the ack alarm by processing a ping frame.
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Processs a PING frame.
  ProcessFramePacket(QuicFrame(QuicPingFrame()));
  // Ensure that this has caused the ACK alarm to be set.
  EXPECT_TRUE(connection_.HasPendingAcks());

  // Send data and ensure the ack is bundled.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(9);
  const std::string data(10000, '?');
  QuicConsumedData consumed = connection_.SaveAndSendStreamData(
      GetNthClientInitiatedStreamId(0, connection_.transport_version()), data,
      0, FIN);
  EXPECT_EQ(data.length(), consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure it's one stream frame with a fin.
  EXPECT_EQ(1u, writer_->frame_count());
  ASSERT_EQ(1u, writer_->stream_frames().size());
  EXPECT_EQ(GetNthClientInitiatedStreamId(0, connection_.transport_version()),
            writer_->stream_frames()[0]->stream_id);
  EXPECT_TRUE(writer_->stream_frames()[0]->fin);
  // Ensure the ack alarm was cancelled when the ack was sent.
  EXPECT_FALSE(connection_.HasPendingAcks());
}

TEST_P(QuicConnectionTest, OnCanWrite) {
  // Visitor's OnCanWrite will send data, but will have more pending writes.
  EXPECT_CALL(visitor_, OnCanWrite())
      .WillOnce(DoAll(IgnoreResult(InvokeWithoutArgs(
                          &connection_, &TestConnection::SendStreamData3)),
                      IgnoreResult(InvokeWithoutArgs(
                          &connection_, &TestConnection::SendStreamData5))));
  {
    InSequence seq;
    EXPECT_CALL(visitor_, WillingAndAbleToWrite()).WillOnce(Return(true));
    EXPECT_CALL(visitor_, WillingAndAbleToWrite())
        .WillRepeatedly(Return(false));
  }

  EXPECT_CALL(*send_algorithm_, CanSend(_))
      .WillRepeatedly(testing::Return(true));

  connection_.OnCanWrite();

  // Parse the last packet and ensure it's the two stream frames from
  // two different streams.
  EXPECT_EQ(2u, writer_->frame_count());
  EXPECT_EQ(2u, writer_->stream_frames().size());
  EXPECT_EQ(GetNthClientInitiatedStreamId(1, connection_.transport_version()),
            writer_->stream_frames()[0]->stream_id);
  EXPECT_EQ(GetNthClientInitiatedStreamId(2, connection_.transport_version()),
            writer_->stream_frames()[1]->stream_id);
}

TEST_P(QuicConnectionTest, RetransmitOnNack) {
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(3, "foo", 0, NO_FIN, &last_packet);
  SendStreamDataToPeer(3, "foos", 3, NO_FIN, &last_packet);
  SendStreamDataToPeer(3, "fooos", 7, NO_FIN, &last_packet);

  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Don't lose a packet on an ack, and nothing is retransmitted.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame ack_one = InitAckFrame(1);
  ProcessAckPacket(&ack_one);

  // Lose a packet and ensure it triggers retransmission.
  QuicAckFrame nack_two = ConstructAckFrame(3, 2);
  LostPacketVector lost_packets;
  lost_packets.push_back(
      LostPacket(QuicPacketNumber(2), kMaxOutgoingPacketSize));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(lost_packets),
                      Return(LossDetectionInterface::DetectionStats())));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  EXPECT_FALSE(QuicPacketCreatorPeer::SendVersionInPacket(creator_));
  ProcessAckPacket(&nack_two);
}

TEST_P(QuicConnectionTest, DoNotSendQueuedPacketForResetStream) {
  // Block the connection to queue the packet.
  BlockOnNextWrite();

  QuicStreamId stream_id = 2;
  connection_.SendStreamDataWithString(stream_id, "foo", 0, NO_FIN);

  // Now that there is a queued packet, reset the stream.
  SendRstStream(stream_id, QUIC_ERROR_PROCESSING_STREAM, 3);

  // Unblock the connection and verify that only the RST_STREAM is sent.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  writer_->SetWritable();
  connection_.OnCanWrite();
  size_t padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + 1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->rst_stream_frames().size());
}

TEST_P(QuicConnectionTest, SendQueuedPacketForQuicRstStreamNoError) {
  // Block the connection to queue the packet.
  BlockOnNextWrite();

  QuicStreamId stream_id = 2;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendStreamDataWithString(stream_id, "foo", 0, NO_FIN);

  // Now that there is a queued packet, reset the stream.
  SendRstStream(stream_id, QUIC_STREAM_NO_ERROR, 3);

  // Unblock the connection and verify that the RST_STREAM is sent and the data
  // packet is sent.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AtLeast(1));
  writer_->SetWritable();
  connection_.OnCanWrite();
  size_t padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + 1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->rst_stream_frames().size());
}

TEST_P(QuicConnectionTest, DoNotRetransmitForResetStreamOnNack) {
  QuicStreamId stream_id = 2;
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(stream_id, "foo", 0, NO_FIN, &last_packet);
  SendStreamDataToPeer(stream_id, "foos", 3, NO_FIN, &last_packet);
  SendStreamDataToPeer(stream_id, "fooos", 7, NO_FIN, &last_packet);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  SendRstStream(stream_id, QUIC_ERROR_PROCESSING_STREAM, 12);

  // Lose a packet and ensure it does not trigger retransmission.
  QuicAckFrame nack_two = ConstructAckFrame(last_packet, last_packet - 1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  ProcessAckPacket(&nack_two);
}

TEST_P(QuicConnectionTest, RetransmitForQuicRstStreamNoErrorOnNack) {
  QuicStreamId stream_id = 2;
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(stream_id, "foo", 0, NO_FIN, &last_packet);
  SendStreamDataToPeer(stream_id, "foos", 3, NO_FIN, &last_packet);
  SendStreamDataToPeer(stream_id, "fooos", 7, NO_FIN, &last_packet);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  SendRstStream(stream_id, QUIC_STREAM_NO_ERROR, 12);

  // Lose a packet, ensure it triggers retransmission.
  QuicAckFrame nack_two = ConstructAckFrame(last_packet, last_packet - 1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  LostPacketVector lost_packets;
  lost_packets.push_back(LostPacket(last_packet - 1, kMaxOutgoingPacketSize));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(lost_packets),
                      Return(LossDetectionInterface::DetectionStats())));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AtLeast(1));
  ProcessAckPacket(&nack_two);
}

TEST_P(QuicConnectionTest, DoNotRetransmitForResetStreamOnRTO) {
  QuicStreamId stream_id = 2;
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(stream_id, "foo", 0, NO_FIN, &last_packet);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  SendRstStream(stream_id, QUIC_ERROR_PROCESSING_STREAM, 3);

  // Fire the RTO and verify that the RST_STREAM is resent, not stream data.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  clock_.AdvanceTime(DefaultRetransmissionTime());
  connection_.GetRetransmissionAlarm()->Fire();
  size_t padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + 1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->rst_stream_frames().size());
  EXPECT_EQ(stream_id, writer_->rst_stream_frames().front().stream_id);
}

// Ensure that if the only data in flight is non-retransmittable, the
// retransmission alarm is not set.
TEST_P(QuicConnectionTest, CancelRetransmissionAlarmAfterResetStream) {
  QuicStreamId stream_id = 2;
  QuicPacketNumber last_data_packet;
  SendStreamDataToPeer(stream_id, "foo", 0, NO_FIN, &last_data_packet);

  // Cancel the stream.
  const QuicPacketNumber rst_packet = last_data_packet + 1;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, rst_packet, _, _)).Times(1);
  SendRstStream(stream_id, QUIC_ERROR_PROCESSING_STREAM, 3);

  // Ack the RST_STREAM frame (since it's retransmittable), but not the data
  // packet, which is no longer retransmittable since the stream was cancelled.
  QuicAckFrame nack_stream_data =
      ConstructAckFrame(rst_packet, last_data_packet);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  ProcessAckPacket(&nack_stream_data);

  // Ensure that the data is still in flight, but the retransmission alarm is no
  // longer set.
  EXPECT_GT(manager_->GetBytesInFlight(), 0u);
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, RetransmitForQuicRstStreamNoErrorOnPTO) {
  QuicStreamId stream_id = 2;
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(stream_id, "foo", 0, NO_FIN, &last_packet);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  SendRstStream(stream_id, QUIC_STREAM_NO_ERROR, 3);

  // Fire the RTO and verify that the RST_STREAM is resent, the stream data
  // is sent.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AtLeast(1));
  clock_.AdvanceTime(DefaultRetransmissionTime());
  connection_.GetRetransmissionAlarm()->Fire();
  size_t padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + 1u, writer_->frame_count());
}

TEST_P(QuicConnectionTest, DoNotSendPendingRetransmissionForResetStream) {
  QuicStreamId stream_id = 2;
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(stream_id, "foo", 0, NO_FIN, &last_packet);
  SendStreamDataToPeer(stream_id, "foos", 3, NO_FIN, &last_packet);
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(stream_id, "fooos", 7, NO_FIN);

  // Lose a packet which will trigger a pending retransmission.
  QuicAckFrame ack = ConstructAckFrame(last_packet, last_packet - 1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  ProcessAckPacket(&ack);

  SendRstStream(stream_id, QUIC_ERROR_PROCESSING_STREAM, 12);

  // Unblock the connection and verify that the RST_STREAM is sent but not the
  // second data packet nor a retransmit.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  writer_->SetWritable();
  connection_.OnCanWrite();
  size_t padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + 1u, writer_->frame_count());
  ASSERT_EQ(1u, writer_->rst_stream_frames().size());
  EXPECT_EQ(stream_id, writer_->rst_stream_frames().front().stream_id);
}

TEST_P(QuicConnectionTest, SendPendingRetransmissionForQuicRstStreamNoError) {
  QuicStreamId stream_id = 2;
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(stream_id, "foo", 0, NO_FIN, &last_packet);
  SendStreamDataToPeer(stream_id, "foos", 3, NO_FIN, &last_packet);
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(stream_id, "fooos", 7, NO_FIN);

  // Lose a packet which will trigger a pending retransmission.
  QuicAckFrame ack = ConstructAckFrame(last_packet, last_packet - 1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  LostPacketVector lost_packets;
  lost_packets.push_back(LostPacket(last_packet - 1, kMaxOutgoingPacketSize));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(lost_packets),
                      Return(LossDetectionInterface::DetectionStats())));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  ProcessAckPacket(&ack);

  SendRstStream(stream_id, QUIC_STREAM_NO_ERROR, 12);

  // Unblock the connection and verify that the RST_STREAM is sent and the
  // second data packet or a retransmit is sent.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AtLeast(2));
  writer_->SetWritable();
  connection_.OnCanWrite();
  // The RST_STREAM_FRAME is sent after queued packets and pending
  // retransmission.
  connection_.SendControlFrame(QuicFrame(
      new QuicRstStreamFrame(1, stream_id, QUIC_STREAM_NO_ERROR, 14)));
  size_t padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + 1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->rst_stream_frames().size());
}

TEST_P(QuicConnectionTest, RetransmitAckedPacket) {
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, &last_packet);    // Packet 1
  SendStreamDataToPeer(1, "foos", 3, NO_FIN, &last_packet);   // Packet 2
  SendStreamDataToPeer(1, "fooos", 7, NO_FIN, &last_packet);  // Packet 3

  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Instigate a loss with an ack.
  QuicAckFrame nack_two = ConstructAckFrame(3, 2);
  // The first nack should trigger a fast retransmission, but we'll be
  // write blocked, so the packet will be queued.
  BlockOnNextWrite();

  LostPacketVector lost_packets;
  lost_packets.push_back(
      LostPacket(QuicPacketNumber(2), kMaxOutgoingPacketSize));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(lost_packets),
                      Return(LossDetectionInterface::DetectionStats())));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, QuicPacketNumber(4), _, _))
      .Times(1);
  ProcessAckPacket(&nack_two);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Now, ack the previous transmission.
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(false, _, _, _, _, _, _));
  QuicAckFrame ack_all = InitAckFrame(3);
  ProcessAckPacket(&ack_all);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, QuicPacketNumber(4), _, _))
      .Times(0);

  writer_->SetWritable();
  connection_.OnCanWrite();

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  // We do not store retransmittable frames of this retransmission.
  EXPECT_FALSE(QuicConnectionPeer::HasRetransmittableFrames(&connection_, 4));
}

TEST_P(QuicConnectionTest, RetransmitNackedLargestObserved) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  QuicPacketNumber original, second;

  QuicByteCount packet_size =
      SendStreamDataToPeer(3, "foo", 0, NO_FIN, &original);  // 1st packet.
  SendStreamDataToPeer(3, "bar", 3, NO_FIN, &second);        // 2nd packet.

  QuicAckFrame frame = InitAckFrame({{second, second + 1}});
  // The first nack should retransmit the largest observed packet.
  LostPacketVector lost_packets;
  lost_packets.push_back(LostPacket(original, kMaxOutgoingPacketSize));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(lost_packets),
                      Return(LossDetectionInterface::DetectionStats())));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  // Packet 1 is short header for IETF QUIC because the encryption level
  // switched to ENCRYPTION_FORWARD_SECURE in SendStreamDataToPeer.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, packet_size, _));
  ProcessAckPacket(&frame);
}

TEST_P(QuicConnectionTest, WriteBlockedBufferedThenSent) {
  BlockOnNextWrite();
  writer_->set_is_write_blocked_data_buffered(true);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendStreamDataWithString(1, "foo", 0, NO_FIN);
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());

  writer_->SetWritable();
  connection_.OnCanWrite();
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, WriteBlockedThenSent) {
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  BlockOnNextWrite();
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendStreamDataWithString(1, "foo", 0, NO_FIN);
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // The second packet should also be queued, in order to ensure packets are
  // never sent out of order.
  writer_->SetWritable();
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendStreamDataWithString(1, "foo", 0, NO_FIN);
  EXPECT_EQ(2u, connection_.NumQueuedPackets());

  // Now both are sent in order when we unblock.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.OnCanWrite();
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, RetransmitWriteBlockedAckedOriginalThenSent) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());

  BlockOnNextWrite();
  writer_->set_is_write_blocked_data_buffered(true);
  // Simulate the retransmission alarm firing.
  clock_.AdvanceTime(DefaultRetransmissionTime());
  connection_.GetRetransmissionAlarm()->Fire();

  // Ack the sent packet before the callback returns, which happens in
  // rare circumstances with write blocked sockets.
  QuicAckFrame ack = InitAckFrame(1);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  ProcessAckPacket(&ack);

  writer_->SetWritable();
  connection_.OnCanWrite();
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  EXPECT_FALSE(QuicConnectionPeer::HasRetransmittableFrames(&connection_, 3));
}

TEST_P(QuicConnectionTest, AlarmsWhenWriteBlocked) {
  // Block the connection.
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  EXPECT_EQ(1u, writer_->packets_write_attempts());
  EXPECT_TRUE(writer_->IsWriteBlocked());

  // Set the send alarm. Fire the alarm and ensure it doesn't attempt to write.
  connection_.GetSendAlarm()->Set(clock_.ApproximateNow());
  connection_.GetSendAlarm()->Fire();
  EXPECT_TRUE(writer_->IsWriteBlocked());
  EXPECT_EQ(1u, writer_->packets_write_attempts());
}

TEST_P(QuicConnectionTest, NoSendAlarmAfterProcessPacketWhenWriteBlocked) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Block the connection.
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  EXPECT_TRUE(writer_->IsWriteBlocked());
  EXPECT_EQ(1u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.GetSendAlarm()->IsSet());

  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  // Process packet number 1. Can not call ProcessPacket or ProcessDataPacket
  // here, because they will fire the alarm after QuicConnection::ProcessPacket
  // is returned.
  const uint64_t received_packet_num = 1;
  const bool has_stop_waiting = false;
  const EncryptionLevel level = ENCRYPTION_FORWARD_SECURE;
  std::unique_ptr<QuicPacket> packet(
      ConstructDataPacket(received_packet_num, has_stop_waiting, level));
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length =
      peer_framer_.EncryptPayload(level, QuicPacketNumber(received_packet_num),
                                  *packet, buffer, kMaxOutgoingPacketSize);
  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(buffer, encrypted_length, clock_.Now(), false));

  EXPECT_TRUE(writer_->IsWriteBlocked());
  EXPECT_FALSE(connection_.GetSendAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, SendAlarmNonZeroDelay) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  // Set a 10 ms send alarm delay. The send alarm after processing the packet
  // should fire after waiting 10ms, not immediately.
  connection_.set_defer_send_in_response_to_packets(true);
  connection_.sent_packet_manager().SetDeferredSendAlarmDelay(
      QuicTime::Delta::FromMilliseconds(10));
  EXPECT_FALSE(connection_.GetSendAlarm()->IsSet());

  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  // Process packet number 1. Can not call ProcessPacket or ProcessDataPacket
  // here, because they will fire the alarm after QuicConnection::ProcessPacket
  // is returned.
  const uint64_t received_packet_num = 1;
  const bool has_stop_waiting = false;
  const EncryptionLevel level = ENCRYPTION_FORWARD_SECURE;
  std::unique_ptr<QuicPacket> packet(
      ConstructDataPacket(received_packet_num, has_stop_waiting, level));
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length =
      peer_framer_.EncryptPayload(level, QuicPacketNumber(received_packet_num),
                                  *packet, buffer, kMaxOutgoingPacketSize);
  EXPECT_CALL(visitor_, WillingAndAbleToWrite()).WillRepeatedly(Return(true));
  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(buffer, encrypted_length, clock_.Now(), false));

  EXPECT_TRUE(connection_.GetSendAlarm()->IsSet());
  // It was set to be 10 ms in the future, so it should at the least be greater
  // than now + 5 ms.
  EXPECT_TRUE(connection_.GetSendAlarm()->deadline() >
              clock_.ApproximateNow() + QuicTime::Delta::FromMilliseconds(5));
}

TEST_P(QuicConnectionTest, AddToWriteBlockedListIfWriterBlockedWhenProcessing) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, nullptr);

  // Simulate the case where a shared writer gets blocked by another connection.
  writer_->SetWriteBlocked();

  // Process an ACK, make sure the connection calls visitor_->OnWriteBlocked().
  QuicAckFrame ack1 = InitAckFrame(1);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _));
  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(1);
  ProcessAckPacket(1, &ack1);
}

TEST_P(QuicConnectionTest, DoNotAddToWriteBlockedListAfterDisconnect) {
  writer_->SetBatchMode(true);
  EXPECT_TRUE(connection_.connected());
  // Have to explicitly grab the OnConnectionClosed frame and check
  // its parameters because this is a silent connection close and the
  // frame is not also transmitted to the peer.
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));

  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(0);

  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    connection_.CloseConnection(QUIC_PEER_GOING_AWAY, "no reason",
                                ConnectionCloseBehavior::SILENT_CLOSE);

    EXPECT_FALSE(connection_.connected());
    writer_->SetWriteBlocked();
  }
  EXPECT_EQ(1, connection_close_frame_count_);
  EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
              IsError(QUIC_PEER_GOING_AWAY));
}

TEST_P(QuicConnectionTest, AddToWriteBlockedListIfBlockedOnFlushPackets) {
  writer_->SetBatchMode(true);
  writer_->BlockOnNextFlush();

  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(1);
  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    // flusher's destructor will call connection_.FlushPackets, which should add
    // the connection to the write blocked list.
  }
}

TEST_P(QuicConnectionTest, NoLimitPacketsPerNack) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  int offset = 0;
  // Send packets 1 to 15.
  for (int i = 0; i < 15; ++i) {
    SendStreamDataToPeer(1, "foo", offset, NO_FIN, nullptr);
    offset += 3;
  }

  // Ack 15, nack 1-14.

  QuicAckFrame nack =
      InitAckFrame({{QuicPacketNumber(15), QuicPacketNumber(16)}});

  // 14 packets have been NACK'd and lost.
  LostPacketVector lost_packets;
  for (int i = 1; i < 15; ++i) {
    lost_packets.push_back(
        LostPacket(QuicPacketNumber(i), kMaxOutgoingPacketSize));
  }
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(lost_packets),
                      Return(LossDetectionInterface::DetectionStats())));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  ProcessAckPacket(&nack);
}

// Test sending multiple acks from the connection to the session.
TEST_P(QuicConnectionTest, MultipleAcks) {
  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacket(1);
  QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 2);
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, &last_packet);  // Packet 1
  EXPECT_EQ(QuicPacketNumber(1u), last_packet);
  SendStreamDataToPeer(3, "foo", 0, NO_FIN, &last_packet);  // Packet 2
  EXPECT_EQ(QuicPacketNumber(2u), last_packet);
  SendAckPacketToPeer();                                    // Packet 3
  SendStreamDataToPeer(5, "foo", 0, NO_FIN, &last_packet);  // Packet 4
  EXPECT_EQ(QuicPacketNumber(4u), last_packet);
  SendStreamDataToPeer(1, "foo", 3, NO_FIN, &last_packet);  // Packet 5
  EXPECT_EQ(QuicPacketNumber(5u), last_packet);
  SendStreamDataToPeer(3, "foo", 3, NO_FIN, &last_packet);  // Packet 6
  EXPECT_EQ(QuicPacketNumber(6u), last_packet);

  // Client will ack packets 1, 2, [!3], 4, 5.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame1 = ConstructAckFrame(5, 3);
  ProcessAckPacket(&frame1);

  // Now the client implicitly acks 3, and explicitly acks 6.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame2 = InitAckFrame(6);
  ProcessAckPacket(&frame2);
}

TEST_P(QuicConnectionTest, DontLatchUnackedPacket) {
  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacket(1);
  QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 2);
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, nullptr);  // Packet 1;
  // From now on, we send acks, so the send algorithm won't mark them pending.
  SendAckPacketToPeer();  // Packet 2

  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame = InitAckFrame(1);
  ProcessAckPacket(&frame);

  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  frame = InitAckFrame(2);
  ProcessAckPacket(&frame);

  // When we send an ack, we make sure our least-unacked makes sense.  In this
  // case since we're not waiting on an ack for 2 and all packets are acked, we
  // set it to 3.
  SendAckPacketToPeer();  // Packet 3

  // Ack the ack, which updates the rtt and raises the least unacked.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  frame = InitAckFrame(3);
  ProcessAckPacket(&frame);

  SendStreamDataToPeer(1, "bar", 3, NO_FIN, nullptr);  // Packet 4
  SendAckPacketToPeer();                               // Packet 5

  // Send two data packets at the end, and ensure if the last one is acked,
  // the least unacked is raised above the ack packets.
  SendStreamDataToPeer(1, "bar", 6, NO_FIN, nullptr);  // Packet 6
  SendStreamDataToPeer(1, "bar", 9, NO_FIN, nullptr);  // Packet 7

  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  frame = InitAckFrame({{QuicPacketNumber(1), QuicPacketNumber(5)},
                        {QuicPacketNumber(7), QuicPacketNumber(8)}});
  ProcessAckPacket(&frame);
}

TEST_P(QuicConnectionTest, SendHandshakeMessages) {
  // Attempt to send a handshake message and have the socket block.
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
  BlockOnNextWrite();
  connection_.SendCryptoDataWithString("foo", 0);
  // The packet should be serialized, but not queued.
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Switch to the new encrypter.
  connection_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);

  // Now become writeable and flush the packets.
  writer_->SetWritable();
  EXPECT_CALL(visitor_, OnCanWrite());
  connection_.OnCanWrite();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());

  // Verify that the handshake packet went out with Initial encryption.
  EXPECT_NE(0x02020202u, writer_->final_bytes_of_last_packet());
}

TEST_P(QuicConnectionTest, DropRetransmitsForInitialPacketAfterForwardSecure) {
  connection_.SendCryptoStreamData();
  // Simulate the retransmission alarm firing and the socket blocking.
  BlockOnNextWrite();
  clock_.AdvanceTime(DefaultRetransmissionTime());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Go forward secure.
  connection_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<TaggingEncrypter>(0x02));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  notifier_.NeuterUnencryptedData();
  connection_.NeuterUnencryptedPackets();
  connection_.OnHandshakeComplete();

  EXPECT_EQ(QuicTime::Zero(), connection_.GetRetransmissionAlarm()->deadline());
  // Unblock the socket and ensure that no packets are sent.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  writer_->SetWritable();
  connection_.OnCanWrite();
}

TEST_P(QuicConnectionTest, RetransmitPacketsWithInitialEncryption) {
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);

  connection_.SendCryptoDataWithString("foo", 0);

  connection_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);
  if (!connection_.version().KnowsWhichDecrypterToUse()) {
    writer_->framer()->framer()->SetAlternativeDecrypter(
        ENCRYPTION_ZERO_RTT,
        std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_ZERO_RTT), false);
  }

  SendStreamDataToPeer(2, "bar", 0, NO_FIN, nullptr);
  EXPECT_FALSE(notifier_.HasLostStreamData());
  connection_.MarkZeroRttPacketsForRetransmission(0);
  EXPECT_TRUE(notifier_.HasLostStreamData());
}

TEST_P(QuicConnectionTest, BufferNonDecryptablePackets) {
  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  // SetFromConfig is always called after construction from InitializeSession.
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  connection_.SetFromConfig(config);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  peer_framer_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
  if (!connection_.version().KnowsWhichDecrypterToUse()) {
    writer_->framer()->framer()->SetDecrypter(
        ENCRYPTION_ZERO_RTT, std::make_unique<TaggingDecrypter>());
  }

  // Process an encrypted packet which can not yet be decrypted which should
  // result in the packet being buffered.
  ProcessDataPacketAtLevel(1, !kHasStopWaiting, ENCRYPTION_ZERO_RTT);

  // Transition to the new encryption state and process another encrypted packet
  // which should result in the original packet being processed.
  SetDecrypter(ENCRYPTION_ZERO_RTT,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_ZERO_RTT));
  connection_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(2);
  ProcessDataPacketAtLevel(2, !kHasStopWaiting, ENCRYPTION_ZERO_RTT);

  // Finally, process a third packet and note that we do not reprocess the
  // buffered packet.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(3, !kHasStopWaiting, ENCRYPTION_ZERO_RTT);
}

TEST_P(QuicConnectionTest, Buffer100NonDecryptablePacketsThenKeyChange) {
  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  // SetFromConfig is always called after construction from InitializeSession.
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  config.set_max_undecryptable_packets(100);
  connection_.SetFromConfig(config);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  peer_framer_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));

  // Process an encrypted packet which can not yet be decrypted which should
  // result in the packet being buffered.
  for (uint64_t i = 1; i <= 100; ++i) {
    ProcessDataPacketAtLevel(i, !kHasStopWaiting, ENCRYPTION_ZERO_RTT);
  }

  // Transition to the new encryption state and process another encrypted packet
  // which should result in the original packets being processed.
  EXPECT_FALSE(connection_.GetProcessUndecryptablePacketsAlarm()->IsSet());
  SetDecrypter(ENCRYPTION_ZERO_RTT,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_ZERO_RTT));
  EXPECT_TRUE(connection_.GetProcessUndecryptablePacketsAlarm()->IsSet());
  connection_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);

  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(100);
  if (!connection_.version().KnowsWhichDecrypterToUse()) {
    writer_->framer()->framer()->SetDecrypter(
        ENCRYPTION_ZERO_RTT, std::make_unique<TaggingDecrypter>());
  }
  connection_.GetProcessUndecryptablePacketsAlarm()->Fire();

  // Finally, process a third packet and note that we do not reprocess the
  // buffered packet.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(102, !kHasStopWaiting, ENCRYPTION_ZERO_RTT);
}

TEST_P(QuicConnectionTest, SetRTOAfterWritingToSocket) {
  BlockOnNextWrite();
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendStreamDataWithString(1, "foo", 0, NO_FIN);
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());

  // Test that RTO is started once we write to the socket.
  writer_->SetWritable();
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.OnCanWrite();
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, TestQueued) {
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(1, "foo", 0, NO_FIN);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Unblock the writes and actually send.
  writer_->SetWritable();
  connection_.OnCanWrite();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, InitialTimeout) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AnyNumber());
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());

  // SetFromConfig sets the initial timeouts before negotiation.
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  connection_.SetFromConfig(config);
  // Subtract a second from the idle timeout on the client side.
  QuicTime default_timeout =
      clock_.ApproximateNow() +
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs - 1);
  EXPECT_EQ(default_timeout, connection_.GetTimeoutAlarm()->deadline());

  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));
  // Simulate the timeout alarm firing.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs - 1));
  connection_.GetTimeoutAlarm()->Fire();

  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());

  EXPECT_FALSE(connection_.HasPendingAcks());
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetSendAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetProcessUndecryptablePacketsAlarm()->IsSet());
  TestConnectionCloseQuicErrorCode(QUIC_NETWORK_IDLE_TIMEOUT);
}

TEST_P(QuicConnectionTest, IdleTimeoutAfterFirstSentPacket) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AnyNumber());
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());

  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  connection_.SetFromConfig(config);
  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  QuicTime initial_ddl =
      clock_.ApproximateNow() +
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs - 1);
  EXPECT_EQ(initial_ddl, connection_.GetTimeoutAlarm()->deadline());
  EXPECT_TRUE(connection_.connected());

  // Advance the time and send the first packet to the peer.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(20));
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, &last_packet);
  EXPECT_EQ(QuicPacketNumber(1u), last_packet);
  // This will be the updated deadline for the connection to idle time out.
  QuicTime new_ddl = clock_.ApproximateNow() +
                     QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs - 1);

  // Simulate the timeout alarm firing, the connection should not be closed as
  // a new packet has been sent.
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _)).Times(0);
  QuicTime::Delta delay = initial_ddl - clock_.ApproximateNow();
  clock_.AdvanceTime(delay);
  // Verify the timeout alarm deadline is updated.
  EXPECT_TRUE(connection_.connected());
  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_EQ(new_ddl, connection_.GetTimeoutAlarm()->deadline());

  // Simulate the timeout alarm firing again, the connection now should be
  // closed.
  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));
  clock_.AdvanceTime(new_ddl - clock_.ApproximateNow());
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());

  EXPECT_FALSE(connection_.HasPendingAcks());
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetSendAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  TestConnectionCloseQuicErrorCode(QUIC_NETWORK_IDLE_TIMEOUT);
}

TEST_P(QuicConnectionTest, IdleTimeoutAfterSendTwoPackets) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AnyNumber());
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());

  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  connection_.SetFromConfig(config);
  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  QuicTime initial_ddl =
      clock_.ApproximateNow() +
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs - 1);
  EXPECT_EQ(initial_ddl, connection_.GetTimeoutAlarm()->deadline());
  EXPECT_TRUE(connection_.connected());

  // Immediately send the first packet, this is a rare case but test code will
  // hit this issue often as MockClock used for tests doesn't move with code
  // execution until manually adjusted.
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, &last_packet);
  EXPECT_EQ(QuicPacketNumber(1u), last_packet);

  // Advance the time and send the second packet to the peer.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(20));
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, &last_packet);
  EXPECT_EQ(QuicPacketNumber(2u), last_packet);

  // Simulate the timeout alarm firing, the connection will be closed.
  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));
  clock_.AdvanceTime(initial_ddl - clock_.ApproximateNow());
  connection_.GetTimeoutAlarm()->Fire();

  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());

  EXPECT_FALSE(connection_.HasPendingAcks());
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetSendAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  TestConnectionCloseQuicErrorCode(QUIC_NETWORK_IDLE_TIMEOUT);
}

TEST_P(QuicConnectionTest, HandshakeTimeout) {
  // Use a shorter handshake timeout than idle timeout for this test.
  const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(5);
  connection_.SetNetworkTimeouts(timeout, timeout);
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AnyNumber());

  QuicTime handshake_timeout =
      clock_.ApproximateNow() + timeout - QuicTime::Delta::FromSeconds(1);
  EXPECT_EQ(handshake_timeout, connection_.GetTimeoutAlarm()->deadline());
  EXPECT_TRUE(connection_.connected());

  // Send and ack new data 3 seconds later to lengthen the idle timeout.
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(0, connection_.transport_version()),
      "GET /", 0, FIN, nullptr);
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(3));
  QuicAckFrame frame = InitAckFrame(1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  ProcessAckPacket(&frame);

  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_TRUE(connection_.connected());

  clock_.AdvanceTime(timeout - QuicTime::Delta::FromSeconds(2));

  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));
  // Simulate the timeout alarm firing.
  connection_.GetTimeoutAlarm()->Fire();

  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());

  EXPECT_FALSE(connection_.HasPendingAcks());
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetSendAlarm()->IsSet());
  TestConnectionCloseQuicErrorCode(QUIC_HANDSHAKE_TIMEOUT);
}

TEST_P(QuicConnectionTest, PingAfterSend) {
  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());

  // Advance to 5ms, and send a packet to the peer, which will set
  // the ping alarm.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(0, connection_.transport_version()),
      "GET /", 0, FIN, nullptr);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(15),
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  // Now recevie an ACK of the previous packet, which will move the
  // ping alarm forward.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  QuicAckFrame frame = InitAckFrame(1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  // The ping timer is set slightly less than 15 seconds in the future, because
  // of the 1s ping timer alarm granularity.
  EXPECT_EQ(
      QuicTime::Delta::FromSeconds(15) - QuicTime::Delta::FromMilliseconds(5),
      connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  writer_->Reset();
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(15));
  connection_.GetPingAlarm()->Fire();
  size_t padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + 1u, writer_->frame_count());
  ASSERT_EQ(1u, writer_->ping_frames().size());
  writer_->Reset();

  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(false));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  SendAckPacketToPeer();

  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, ReducedPingTimeout) {
  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());

  // Use a reduced ping timeout for this connection.
  connection_.set_keep_alive_ping_timeout(QuicTime::Delta::FromSeconds(10));

  // Advance to 5ms, and send a packet to the peer, which will set
  // the ping alarm.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(0, connection_.transport_version()),
      "GET /", 0, FIN, nullptr);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(10),
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  // Now recevie an ACK of the previous packet, which will move the
  // ping alarm forward.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  QuicAckFrame frame = InitAckFrame(1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  // The ping timer is set slightly less than 10 seconds in the future, because
  // of the 1s ping timer alarm granularity.
  EXPECT_EQ(
      QuicTime::Delta::FromSeconds(10) - QuicTime::Delta::FromMilliseconds(5),
      connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  writer_->Reset();
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(10));
  connection_.GetPingAlarm()->Fire();
  size_t padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + 1u, writer_->frame_count());
  ASSERT_EQ(1u, writer_->ping_frames().size());
  writer_->Reset();

  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(false));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  SendAckPacketToPeer();

  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
}

// Tests whether sending an MTU discovery packet to peer successfully causes the
// maximum packet size to increase.
TEST_P(QuicConnectionTest, SendMtuDiscoveryPacket) {
  MtuDiscoveryTestInit();

  // Send an MTU probe.
  const size_t new_mtu = kDefaultMaxPacketSize + 100;
  QuicByteCount mtu_probe_size;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(SaveArg<3>(&mtu_probe_size));
  connection_.SendMtuDiscoveryPacket(new_mtu);
  EXPECT_EQ(new_mtu, mtu_probe_size);
  EXPECT_EQ(QuicPacketNumber(1u), creator_->packet_number());

  // Send more than MTU worth of data.  No acknowledgement was received so far,
  // so the MTU should be at its old value.
  const std::string data(kDefaultMaxPacketSize + 1, '.');
  QuicByteCount size_before_mtu_change;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(2)
      .WillOnce(SaveArg<3>(&size_before_mtu_change))
      .WillOnce(Return());
  connection_.SendStreamDataWithString(3, data, 0, FIN);
  EXPECT_EQ(QuicPacketNumber(3u), creator_->packet_number());
  EXPECT_EQ(kDefaultMaxPacketSize, size_before_mtu_change);

  // Acknowledge all packets so far.
  QuicAckFrame probe_ack = InitAckFrame(3);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  ProcessAckPacket(&probe_ack);
  EXPECT_EQ(new_mtu, connection_.max_packet_length());

  // Send the same data again.  Check that it fits into a single packet now.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendStreamDataWithString(3, data, 0, FIN);
  EXPECT_EQ(QuicPacketNumber(4u), creator_->packet_number());
}

// Verifies that when a MTU probe packet is sent and buffered in a batch writer,
// the writer is flushed immediately.
TEST_P(QuicConnectionTest, BatchWriterFlushedAfterMtuDiscoveryPacket) {
  writer_->SetBatchMode(true);
  MtuDiscoveryTestInit();

  // Send an MTU probe.
  const size_t target_mtu = kDefaultMaxPacketSize + 100;
  QuicByteCount mtu_probe_size;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(SaveArg<3>(&mtu_probe_size));
  const uint32_t prior_flush_attempts = writer_->flush_attempts();
  connection_.SendMtuDiscoveryPacket(target_mtu);
  EXPECT_EQ(target_mtu, mtu_probe_size);
  EXPECT_EQ(writer_->flush_attempts(), prior_flush_attempts + 1);
}

// Tests whether MTU discovery does not happen when it is not explicitly enabled
// by the connection options.
TEST_P(QuicConnectionTest, MtuDiscoveryDisabled) {
  MtuDiscoveryTestInit();

  const QuicPacketCount packets_between_probes_base = 10;
  set_packets_between_probes_base(packets_between_probes_base);

  const QuicPacketCount number_of_packets = packets_between_probes_base * 2;
  for (QuicPacketCount i = 0; i < number_of_packets; i++) {
    SendStreamDataToPeer(3, ".", i, NO_FIN, nullptr);
    EXPECT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
    EXPECT_EQ(0u, connection_.mtu_probe_count());
  }
}

// Tests whether MTU discovery works when all probes are acknowledged on the
// first try.
TEST_P(QuicConnectionTest, MtuDiscoveryEnabled) {
  MtuDiscoveryTestInit();

  const QuicPacketCount packets_between_probes_base = 5;
  set_packets_between_probes_base(packets_between_probes_base);

  connection_.EnablePathMtuDiscovery(send_algorithm_);

  // Send enough packets so that the next one triggers path MTU discovery.
  for (QuicPacketCount i = 0; i < packets_between_probes_base - 1; i++) {
    SendStreamDataToPeer(3, ".", i, NO_FIN, nullptr);
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  // Trigger the probe.
  SendStreamDataToPeer(3, "!", packets_between_probes_base - 1, NO_FIN,
                       nullptr);
  ASSERT_TRUE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  QuicByteCount probe_size;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(SaveArg<3>(&probe_size));
  connection_.GetMtuDiscoveryAlarm()->Fire();

  EXPECT_THAT(probe_size, InRange(connection_.max_packet_length(),
                                  kMtuDiscoveryTargetPacketSizeHigh));

  const QuicPacketNumber probe_packet_number =
      FirstSendingPacketNumber() + packets_between_probes_base;
  ASSERT_EQ(probe_packet_number, creator_->packet_number());

  // Acknowledge all packets sent so far.
  {
    QuicAckFrame probe_ack = InitAckFrame(probe_packet_number);
    EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _))
        .Times(AnyNumber());
    ProcessAckPacket(&probe_ack);
    EXPECT_EQ(probe_size, connection_.max_packet_length());
    EXPECT_EQ(0u, connection_.GetBytesInFlight());

    EXPECT_EQ(1u, connection_.mtu_probe_count());
  }

  QuicStreamOffset stream_offset = packets_between_probes_base;
  QuicByteCount last_probe_size = 0;
  for (size_t num_probes = 1; num_probes < kMtuDiscoveryAttempts;
       ++num_probes) {
    // Send just enough packets without triggering the next probe.
    for (QuicPacketCount i = 0;
         i < (packets_between_probes_base << num_probes) - 1; ++i) {
      SendStreamDataToPeer(3, ".", stream_offset++, NO_FIN, nullptr);
      ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
    }

    // Trigger the next probe.
    SendStreamDataToPeer(3, "!", stream_offset++, NO_FIN, nullptr);
    ASSERT_TRUE(connection_.GetMtuDiscoveryAlarm()->IsSet());
    QuicByteCount new_probe_size;
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .WillOnce(SaveArg<3>(&new_probe_size));
    connection_.GetMtuDiscoveryAlarm()->Fire();
    EXPECT_THAT(new_probe_size,
                InRange(probe_size, kMtuDiscoveryTargetPacketSizeHigh));
    EXPECT_EQ(num_probes + 1, connection_.mtu_probe_count());

    // Acknowledge all packets sent so far.
    QuicAckFrame probe_ack = InitAckFrame(creator_->packet_number());
    ProcessAckPacket(&probe_ack);
    EXPECT_EQ(new_probe_size, connection_.max_packet_length());
    EXPECT_EQ(0u, connection_.GetBytesInFlight());

    last_probe_size = probe_size;
    probe_size = new_probe_size;
  }

  // The last probe size should be equal to the target.
  EXPECT_EQ(probe_size, kMtuDiscoveryTargetPacketSizeHigh);

  writer_->SetShouldWriteFail();

  // Ignore PACKET_WRITE_ERROR once.
  SendStreamDataToPeer(3, "(", stream_offset++, NO_FIN, nullptr);
  EXPECT_EQ(last_probe_size, connection_.max_packet_length());
  EXPECT_TRUE(connection_.connected());

  // Close connection on another PACKET_WRITE_ERROR.
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _))
      .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));
  SendStreamDataToPeer(3, ")", stream_offset++, NO_FIN, nullptr);
  EXPECT_EQ(last_probe_size, connection_.max_packet_length());
  EXPECT_FALSE(connection_.connected());
  EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
              IsError(QUIC_PACKET_WRITE_ERROR));
}

// After a successful MTU probe, one and only one write error should be ignored
// if it happened in QuicConnection::FlushPacket.
TEST_P(QuicConnectionTest,
       MtuDiscoveryIgnoreOneWriteErrorInFlushAfterSuccessfulProbes) {
  MtuDiscoveryTestInit();
  writer_->SetBatchMode(true);

  const QuicPacketCount packets_between_probes_base = 5;
  set_packets_between_probes_base(packets_between_probes_base);

  connection_.EnablePathMtuDiscovery(send_algorithm_);

  const QuicByteCount original_max_packet_length =
      connection_.max_packet_length();
  // Send enough packets so that the next one triggers path MTU discovery.
  for (QuicPacketCount i = 0; i < packets_between_probes_base - 1; i++) {
    SendStreamDataToPeer(3, ".", i, NO_FIN, nullptr);
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  // Trigger the probe.
  SendStreamDataToPeer(3, "!", packets_between_probes_base - 1, NO_FIN,
                       nullptr);
  ASSERT_TRUE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  QuicByteCount probe_size;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(SaveArg<3>(&probe_size));
  connection_.GetMtuDiscoveryAlarm()->Fire();

  EXPECT_THAT(probe_size, InRange(connection_.max_packet_length(),
                                  kMtuDiscoveryTargetPacketSizeHigh));

  const QuicPacketNumber probe_packet_number =
      FirstSendingPacketNumber() + packets_between_probes_base;
  ASSERT_EQ(probe_packet_number, creator_->packet_number());

  // Acknowledge all packets sent so far.
  {
    QuicAckFrame probe_ack = InitAckFrame(probe_packet_number);
    EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _))
        .Times(AnyNumber());
    ProcessAckPacket(&probe_ack);
    EXPECT_EQ(probe_size, connection_.max_packet_length());
    EXPECT_EQ(0u, connection_.GetBytesInFlight());
  }

  EXPECT_EQ(1u, connection_.mtu_probe_count());

  writer_->SetShouldWriteFail();

  // Ignore PACKET_WRITE_ERROR once.
  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    // flusher's destructor will call connection_.FlushPackets, which should
    // get a WRITE_STATUS_ERROR from the writer and ignore it.
  }
  EXPECT_EQ(original_max_packet_length, connection_.max_packet_length());
  EXPECT_TRUE(connection_.connected());

  // Close connection on another PACKET_WRITE_ERROR.
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _))
      .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));
  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    // flusher's destructor will call connection_.FlushPackets, which should
    // get a WRITE_STATUS_ERROR from the writer and ignore it.
  }
  EXPECT_EQ(original_max_packet_length, connection_.max_packet_length());
  EXPECT_FALSE(connection_.connected());
  EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
              IsError(QUIC_PACKET_WRITE_ERROR));
}

// Simulate the case where the first attempt to send a probe is write blocked,
// and after unblock, the second attempt returns a MSG_TOO_BIG error.
TEST_P(QuicConnectionTest, MtuDiscoveryWriteBlocked) {
  MtuDiscoveryTestInit();

  const QuicPacketCount packets_between_probes_base = 5;
  set_packets_between_probes_base(packets_between_probes_base);

  connection_.EnablePathMtuDiscovery(send_algorithm_);

  // Send enough packets so that the next one triggers path MTU discovery.
  for (QuicPacketCount i = 0; i < packets_between_probes_base - 1; i++) {
    SendStreamDataToPeer(3, ".", i, NO_FIN, nullptr);
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  QuicByteCount original_max_packet_length = connection_.max_packet_length();

  // Trigger the probe.
  SendStreamDataToPeer(3, "!", packets_between_probes_base - 1, NO_FIN,
                       nullptr);
  ASSERT_TRUE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  BlockOnNextWrite();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  connection_.GetMtuDiscoveryAlarm()->Fire();
  EXPECT_EQ(1u, connection_.mtu_probe_count());
  EXPECT_EQ(1u, connection_.NumQueuedPackets());
  ASSERT_TRUE(connection_.connected());

  writer_->SetWritable();
  SimulateNextPacketTooLarge();
  connection_.OnCanWrite();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_EQ(original_max_packet_length, connection_.max_packet_length());
  EXPECT_TRUE(connection_.connected());
}

// Tests whether MTU discovery works correctly when the probes never get
// acknowledged.
TEST_P(QuicConnectionTest, MtuDiscoveryFailed) {
  MtuDiscoveryTestInit();

  // Lower the number of probes between packets in order to make the test go
  // much faster.
  const QuicPacketCount packets_between_probes_base = 5;
  set_packets_between_probes_base(packets_between_probes_base);

  connection_.EnablePathMtuDiscovery(send_algorithm_);

  const QuicTime::Delta rtt = QuicTime::Delta::FromMilliseconds(100);

  EXPECT_EQ(packets_between_probes_base,
            QuicConnectionPeer::GetPacketsBetweenMtuProbes(&connection_));

  // This tests sends more packets than strictly necessary to make sure that if
  // the connection was to send more discovery packets than needed, those would
  // get caught as well.
  const QuicPacketCount number_of_packets =
      packets_between_probes_base * (1 << (kMtuDiscoveryAttempts + 1));
  std::vector<QuicPacketNumber> mtu_discovery_packets;
  // Called on many acks.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _))
      .Times(AnyNumber());
  for (QuicPacketCount i = 0; i < number_of_packets; i++) {
    SendStreamDataToPeer(3, "!", i, NO_FIN, nullptr);
    clock_.AdvanceTime(rtt);

    // Receive an ACK, which marks all data packets as received, and all MTU
    // discovery packets as missing.

    QuicAckFrame ack;

    if (!mtu_discovery_packets.empty()) {
      QuicPacketNumber min_packet = *min_element(mtu_discovery_packets.begin(),
                                                 mtu_discovery_packets.end());
      QuicPacketNumber max_packet = *max_element(mtu_discovery_packets.begin(),
                                                 mtu_discovery_packets.end());
      ack.packets.AddRange(QuicPacketNumber(1), min_packet);
      ack.packets.AddRange(QuicPacketNumber(max_packet + 1),
                           creator_->packet_number() + 1);
      ack.largest_acked = creator_->packet_number();

    } else {
      ack.packets.AddRange(QuicPacketNumber(1), creator_->packet_number() + 1);
      ack.largest_acked = creator_->packet_number();
    }

    ProcessAckPacket(&ack);

    // Trigger MTU probe if it would be scheduled now.
    if (!connection_.GetMtuDiscoveryAlarm()->IsSet()) {
      continue;
    }

    // Fire the alarm.  The alarm should cause a packet to be sent.
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
    connection_.GetMtuDiscoveryAlarm()->Fire();
    // Record the packet number of the MTU discovery packet in order to
    // mark it as NACK'd.
    mtu_discovery_packets.push_back(creator_->packet_number());
  }

  // Ensure the number of packets between probes grows exponentially by checking
  // it against the closed-form expression for the packet number.
  ASSERT_EQ(kMtuDiscoveryAttempts, mtu_discovery_packets.size());
  for (uint64_t i = 0; i < kMtuDiscoveryAttempts; i++) {
    // 2^0 + 2^1 + 2^2 + ... + 2^n = 2^(n + 1) - 1
    const QuicPacketCount packets_between_probes =
        packets_between_probes_base * ((1 << (i + 1)) - 1);
    EXPECT_EQ(QuicPacketNumber(packets_between_probes + (i + 1)),
              mtu_discovery_packets[i]);
  }

  EXPECT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  EXPECT_EQ(kDefaultMaxPacketSize, connection_.max_packet_length());
  EXPECT_EQ(kMtuDiscoveryAttempts, connection_.mtu_probe_count());
}

// Probe 3 times, the first one succeeds, then fails, then succeeds again.
TEST_P(QuicConnectionTest, MtuDiscoverySecondProbeFailed) {
  MtuDiscoveryTestInit();

  const QuicPacketCount packets_between_probes_base = 5;
  set_packets_between_probes_base(packets_between_probes_base);

  connection_.EnablePathMtuDiscovery(send_algorithm_);

  // Send enough packets so that the next one triggers path MTU discovery.
  QuicStreamOffset stream_offset = 0;
  for (QuicPacketCount i = 0; i < packets_between_probes_base - 1; i++) {
    SendStreamDataToPeer(3, ".", stream_offset++, NO_FIN, nullptr);
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  // Trigger the probe.
  SendStreamDataToPeer(3, "!", packets_between_probes_base - 1, NO_FIN,
                       nullptr);
  ASSERT_TRUE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  QuicByteCount probe_size;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(SaveArg<3>(&probe_size));
  connection_.GetMtuDiscoveryAlarm()->Fire();
  EXPECT_THAT(probe_size, InRange(connection_.max_packet_length(),
                                  kMtuDiscoveryTargetPacketSizeHigh));

  const QuicPacketNumber probe_packet_number =
      FirstSendingPacketNumber() + packets_between_probes_base;
  ASSERT_EQ(probe_packet_number, creator_->packet_number());

  // Acknowledge all packets sent so far.
  QuicAckFrame first_ack = InitAckFrame(probe_packet_number);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _))
      .Times(AnyNumber());
  ProcessAckPacket(&first_ack);
  EXPECT_EQ(probe_size, connection_.max_packet_length());
  EXPECT_EQ(0u, connection_.GetBytesInFlight());

  EXPECT_EQ(1u, connection_.mtu_probe_count());

  // Send just enough packets without triggering the second probe.
  for (QuicPacketCount i = 0; i < (packets_between_probes_base << 1) - 1; ++i) {
    SendStreamDataToPeer(3, ".", stream_offset++, NO_FIN, nullptr);
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  // Trigger the second probe.
  SendStreamDataToPeer(3, "!", stream_offset++, NO_FIN, nullptr);
  ASSERT_TRUE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  QuicByteCount second_probe_size;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(SaveArg<3>(&second_probe_size));
  connection_.GetMtuDiscoveryAlarm()->Fire();
  EXPECT_THAT(second_probe_size,
              InRange(probe_size, kMtuDiscoveryTargetPacketSizeHigh));
  EXPECT_EQ(2u, connection_.mtu_probe_count());

  // Acknowledge all packets sent so far, except the second probe.
  QuicPacketNumber second_probe_packet_number = creator_->packet_number();
  QuicAckFrame second_ack = InitAckFrame(second_probe_packet_number - 1);
  ProcessAckPacket(&first_ack);
  EXPECT_EQ(probe_size, connection_.max_packet_length());

  // Send just enough packets without triggering the third probe.
  for (QuicPacketCount i = 0; i < (packets_between_probes_base << 2) - 1; ++i) {
    SendStreamDataToPeer(3, "@", stream_offset++, NO_FIN, nullptr);
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  // Trigger the third probe.
  SendStreamDataToPeer(3, "#", stream_offset++, NO_FIN, nullptr);
  ASSERT_TRUE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  QuicByteCount third_probe_size;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(SaveArg<3>(&third_probe_size));
  connection_.GetMtuDiscoveryAlarm()->Fire();
  EXPECT_THAT(third_probe_size, InRange(probe_size, second_probe_size));
  EXPECT_EQ(3u, connection_.mtu_probe_count());

  // Acknowledge all packets sent so far, except the second probe.
  QuicAckFrame third_ack =
      ConstructAckFrame(creator_->packet_number(), second_probe_packet_number);
  ProcessAckPacket(&third_ack);
  EXPECT_EQ(third_probe_size, connection_.max_packet_length());

  SendStreamDataToPeer(3, "$", stream_offset++, NO_FIN, nullptr);
  EXPECT_TRUE(connection_.PathMtuReductionDetectionInProgress());

  if (connection_.PathDegradingDetectionInProgress() &&
      QuicConnectionPeer::GetPathDegradingDeadline(&connection_) <
          QuicConnectionPeer::GetPathMtuReductionDetectionDeadline(
              &connection_)) {
    // Fire path degrading alarm first.
    connection_.PathDegradingTimeout();
  }

  // Verify the max packet size has not reduced.
  EXPECT_EQ(third_probe_size, connection_.max_packet_length());

  // Fire alarm to get path mtu reduction callback called.
  EXPECT_TRUE(connection_.PathMtuReductionDetectionInProgress());
  connection_.GetBlackholeDetectorAlarm()->Fire();

  // Verify the max packet size has reduced to the previous value.
  EXPECT_EQ(probe_size, connection_.max_packet_length());
}

// Tests whether MTU discovery works when the writer has a limit on how large a
// packet can be.
TEST_P(QuicConnectionTest, MtuDiscoveryWriterLimited) {
  MtuDiscoveryTestInit();

  const QuicByteCount mtu_limit = kMtuDiscoveryTargetPacketSizeHigh - 1;
  writer_->set_max_packet_size(mtu_limit);

  const QuicPacketCount packets_between_probes_base = 5;
  set_packets_between_probes_base(packets_between_probes_base);

  connection_.EnablePathMtuDiscovery(send_algorithm_);

  // Send enough packets so that the next one triggers path MTU discovery.
  for (QuicPacketCount i = 0; i < packets_between_probes_base - 1; i++) {
    SendStreamDataToPeer(3, ".", i, NO_FIN, nullptr);
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  // Trigger the probe.
  SendStreamDataToPeer(3, "!", packets_between_probes_base - 1, NO_FIN,
                       nullptr);
  ASSERT_TRUE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  QuicByteCount probe_size;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(SaveArg<3>(&probe_size));
  connection_.GetMtuDiscoveryAlarm()->Fire();

  EXPECT_THAT(probe_size, InRange(connection_.max_packet_length(), mtu_limit));

  const QuicPacketNumber probe_sequence_number =
      FirstSendingPacketNumber() + packets_between_probes_base;
  ASSERT_EQ(probe_sequence_number, creator_->packet_number());

  // Acknowledge all packets sent so far.
  {
    QuicAckFrame probe_ack = InitAckFrame(probe_sequence_number);
    EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _))
        .Times(AnyNumber());
    ProcessAckPacket(&probe_ack);
    EXPECT_EQ(probe_size, connection_.max_packet_length());
    EXPECT_EQ(0u, connection_.GetBytesInFlight());
  }

  EXPECT_EQ(1u, connection_.mtu_probe_count());

  QuicStreamOffset stream_offset = packets_between_probes_base;
  for (size_t num_probes = 1; num_probes < kMtuDiscoveryAttempts;
       ++num_probes) {
    // Send just enough packets without triggering the next probe.
    for (QuicPacketCount i = 0;
         i < (packets_between_probes_base << num_probes) - 1; ++i) {
      SendStreamDataToPeer(3, ".", stream_offset++, NO_FIN, nullptr);
      ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
    }

    // Trigger the next probe.
    SendStreamDataToPeer(3, "!", stream_offset++, NO_FIN, nullptr);
    ASSERT_TRUE(connection_.GetMtuDiscoveryAlarm()->IsSet());
    QuicByteCount new_probe_size;
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .WillOnce(SaveArg<3>(&new_probe_size));
    connection_.GetMtuDiscoveryAlarm()->Fire();
    EXPECT_THAT(new_probe_size, InRange(probe_size, mtu_limit));
    EXPECT_EQ(num_probes + 1, connection_.mtu_probe_count());

    // Acknowledge all packets sent so far.
    QuicAckFrame probe_ack = InitAckFrame(creator_->packet_number());
    ProcessAckPacket(&probe_ack);
    EXPECT_EQ(new_probe_size, connection_.max_packet_length());
    EXPECT_EQ(0u, connection_.GetBytesInFlight());

    probe_size = new_probe_size;
  }

  // The last probe size should be equal to the target.
  EXPECT_EQ(probe_size, mtu_limit);
}

// Tests whether MTU discovery works when the writer returns an error despite
// advertising higher packet length.
TEST_P(QuicConnectionTest, MtuDiscoveryWriterFailed) {
  MtuDiscoveryTestInit();

  const QuicByteCount mtu_limit = kMtuDiscoveryTargetPacketSizeHigh - 1;
  const QuicByteCount initial_mtu = connection_.max_packet_length();
  EXPECT_LT(initial_mtu, mtu_limit);
  writer_->set_max_packet_size(mtu_limit);

  const QuicPacketCount packets_between_probes_base = 5;
  set_packets_between_probes_base(packets_between_probes_base);

  connection_.EnablePathMtuDiscovery(send_algorithm_);

  // Send enough packets so that the next one triggers path MTU discovery.
  for (QuicPacketCount i = 0; i < packets_between_probes_base - 1; i++) {
    SendStreamDataToPeer(3, ".", i, NO_FIN, nullptr);
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  // Trigger the probe.
  SendStreamDataToPeer(3, "!", packets_between_probes_base - 1, NO_FIN,
                       nullptr);
  ASSERT_TRUE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  writer_->SimulateNextPacketTooLarge();
  connection_.GetMtuDiscoveryAlarm()->Fire();
  ASSERT_TRUE(connection_.connected());

  // Send more data.
  QuicPacketNumber probe_number = creator_->packet_number();
  QuicPacketCount extra_packets = packets_between_probes_base * 3;
  for (QuicPacketCount i = 0; i < extra_packets; i++) {
    connection_.EnsureWritableAndSendStreamData5();
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  // Acknowledge all packets sent so far, except for the lost probe.
  QuicAckFrame probe_ack =
      ConstructAckFrame(creator_->packet_number(), probe_number);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  ProcessAckPacket(&probe_ack);
  EXPECT_EQ(initial_mtu, connection_.max_packet_length());

  // Send more packets, and ensure that none of them sets the alarm.
  for (QuicPacketCount i = 0; i < 4 * packets_between_probes_base; i++) {
    connection_.EnsureWritableAndSendStreamData5();
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  EXPECT_EQ(initial_mtu, connection_.max_packet_length());
  EXPECT_EQ(1u, connection_.mtu_probe_count());
}

TEST_P(QuicConnectionTest, NoMtuDiscoveryAfterConnectionClosed) {
  MtuDiscoveryTestInit();

  const QuicPacketCount packets_between_probes_base = 10;
  set_packets_between_probes_base(packets_between_probes_base);

  connection_.EnablePathMtuDiscovery(send_algorithm_);

  // Send enough packets so that the next one triggers path MTU discovery.
  for (QuicPacketCount i = 0; i < packets_between_probes_base - 1; i++) {
    SendStreamDataToPeer(3, ".", i, NO_FIN, nullptr);
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  SendStreamDataToPeer(3, "!", packets_between_probes_base - 1, NO_FIN,
                       nullptr);
  EXPECT_TRUE(connection_.GetMtuDiscoveryAlarm()->IsSet());

  EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
  connection_.CloseConnection(QUIC_PEER_GOING_AWAY, "no reason",
                              ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, TimeoutAfterSendDuringHandshake) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  connection_.SetFromConfig(config);

  const QuicTime::Delta initial_idle_timeout =
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs - 1);
  const QuicTime::Delta five_ms = QuicTime::Delta::FromMilliseconds(5);
  QuicTime default_timeout = clock_.ApproximateNow() + initial_idle_timeout;

  // When we send a packet, the timeout will change to 5ms +
  // kInitialIdleTimeoutSecs.
  clock_.AdvanceTime(five_ms);
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, FIN, nullptr);
  EXPECT_EQ(default_timeout + five_ms,
            connection_.GetTimeoutAlarm()->deadline());

  // Now send more data. This will not move the timeout because
  // no data has been received since the previous write.
  clock_.AdvanceTime(five_ms);
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      3, FIN, nullptr);
  EXPECT_EQ(default_timeout + five_ms,
            connection_.GetTimeoutAlarm()->deadline());

  // The original alarm will fire.  We should not time out because we had a
  // network event at t=5ms.  The alarm will reregister.
  clock_.AdvanceTime(initial_idle_timeout - five_ms - five_ms);
  EXPECT_EQ(default_timeout, clock_.ApproximateNow());
  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_TRUE(connection_.connected());
  EXPECT_EQ(default_timeout + five_ms,
            connection_.GetTimeoutAlarm()->deadline());

  // This time, we should time out.
  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AtLeast(1));
  clock_.AdvanceTime(five_ms);
  EXPECT_EQ(default_timeout + five_ms, clock_.ApproximateNow());
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());
  TestConnectionCloseQuicErrorCode(QUIC_NETWORK_IDLE_TIMEOUT);
}

TEST_P(QuicConnectionTest, TimeoutAfterSendAfterHandshake) {
  // When the idle timeout fires, verify that by default we do not send any
  // connection close packets.
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;

  // Create a handshake message that also enables silent close.
  CryptoHandshakeMessage msg;
  std::string error_details;
  QuicConfig client_config;
  client_config.SetInitialStreamFlowControlWindowToSend(
      kInitialStreamFlowControlWindowForTest);
  client_config.SetInitialSessionFlowControlWindowToSend(
      kInitialSessionFlowControlWindowForTest);
  client_config.SetIdleNetworkTimeout(
      QuicTime::Delta::FromSeconds(kMaximumIdleTimeoutSecs));
  client_config.ToHandshakeMessage(&msg, connection_.transport_version());
  const QuicErrorCode error =
      config.ProcessPeerHello(msg, CLIENT, &error_details);
  EXPECT_THAT(error, IsQuicNoError());

  if (connection_.version().UsesTls()) {
    QuicConfigPeer::SetReceivedOriginalConnectionId(
        &config, connection_.connection_id());
    QuicConfigPeer::SetReceivedInitialSourceConnectionId(
        &config, connection_.connection_id());
  }
  connection_.SetFromConfig(config);

  const QuicTime::Delta default_idle_timeout =
      QuicTime::Delta::FromSeconds(kMaximumIdleTimeoutSecs - 1);
  const QuicTime::Delta five_ms = QuicTime::Delta::FromMilliseconds(5);
  QuicTime default_timeout = clock_.ApproximateNow() + default_idle_timeout;

  // When we send a packet, the timeout will change to 5ms +
  // kInitialIdleTimeoutSecs.
  clock_.AdvanceTime(five_ms);
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, FIN, nullptr);
  EXPECT_EQ(default_timeout + five_ms,
            connection_.GetTimeoutAlarm()->deadline());

  // Now send more data. This will not move the timeout because
  // no data has been received since the previous write.
  clock_.AdvanceTime(five_ms);
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      3, FIN, nullptr);
  EXPECT_EQ(default_timeout + five_ms,
            connection_.GetTimeoutAlarm()->deadline());

  // The original alarm will fire.  We should not time out because we had a
  // network event at t=5ms.  The alarm will reregister.
  clock_.AdvanceTime(default_idle_timeout - five_ms - five_ms);
  EXPECT_EQ(default_timeout, clock_.ApproximateNow());
  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_TRUE(connection_.connected());
  EXPECT_EQ(default_timeout + five_ms,
            connection_.GetTimeoutAlarm()->deadline());

  // This time, we should time out.
  // This results in a SILENT_CLOSE, so the writer will not be invoked
  // and will not save the frame. Grab the frame from OnConnectionClosed
  // directly.
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));

  clock_.AdvanceTime(five_ms);
  EXPECT_EQ(default_timeout + five_ms, clock_.ApproximateNow());
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());
  EXPECT_EQ(1, connection_close_frame_count_);
  EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
              IsError(QUIC_NETWORK_IDLE_TIMEOUT));
}

TEST_P(QuicConnectionTest, TimeoutAfterSendSilentCloseWithOpenStreams) {
  // Same test as above, but having open streams causes a connection close
  // to be sent.
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;

  // Create a handshake message that also enables silent close.
  CryptoHandshakeMessage msg;
  std::string error_details;
  QuicConfig client_config;
  client_config.SetInitialStreamFlowControlWindowToSend(
      kInitialStreamFlowControlWindowForTest);
  client_config.SetInitialSessionFlowControlWindowToSend(
      kInitialSessionFlowControlWindowForTest);
  client_config.SetIdleNetworkTimeout(
      QuicTime::Delta::FromSeconds(kMaximumIdleTimeoutSecs));
  client_config.ToHandshakeMessage(&msg, connection_.transport_version());
  const QuicErrorCode error =
      config.ProcessPeerHello(msg, CLIENT, &error_details);
  EXPECT_THAT(error, IsQuicNoError());

  if (connection_.version().UsesTls()) {
    QuicConfigPeer::SetReceivedOriginalConnectionId(
        &config, connection_.connection_id());
    QuicConfigPeer::SetReceivedInitialSourceConnectionId(
        &config, connection_.connection_id());
  }
  connection_.SetFromConfig(config);

  const QuicTime::Delta default_idle_timeout =
      QuicTime::Delta::FromSeconds(kMaximumIdleTimeoutSecs - 1);
  const QuicTime::Delta five_ms = QuicTime::Delta::FromMilliseconds(5);
  QuicTime default_timeout = clock_.ApproximateNow() + default_idle_timeout;

  // When we send a packet, the timeout will change to 5ms +
  // kInitialIdleTimeoutSecs.
  clock_.AdvanceTime(five_ms);
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, FIN, nullptr);
  EXPECT_EQ(default_timeout + five_ms,
            connection_.GetTimeoutAlarm()->deadline());

  // Indicate streams are still open.
  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));
  if (GetQuicReloadableFlag(quic_add_stream_info_to_idle_close_detail)) {
    EXPECT_CALL(visitor_, GetStreamsInfoForLogging()).WillOnce(Return(""));
  }

  // This time, we should time out and send a connection close due to the TLP.
  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AtLeast(1));
  clock_.AdvanceTime(connection_.GetTimeoutAlarm()->deadline() -
                     clock_.ApproximateNow() + five_ms);
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());
  TestConnectionCloseQuicErrorCode(QUIC_NETWORK_IDLE_TIMEOUT);
}

TEST_P(QuicConnectionTest, TimeoutAfterReceive) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  connection_.SetFromConfig(config);

  const QuicTime::Delta initial_idle_timeout =
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs - 1);
  const QuicTime::Delta five_ms = QuicTime::Delta::FromMilliseconds(5);
  QuicTime default_timeout = clock_.ApproximateNow() + initial_idle_timeout;

  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, NO_FIN);
  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      3, NO_FIN);

  EXPECT_EQ(default_timeout, connection_.GetTimeoutAlarm()->deadline());
  clock_.AdvanceTime(five_ms);

  // When we receive a packet, the timeout will change to 5ms +
  // kInitialIdleTimeoutSecs.
  QuicAckFrame ack = InitAckFrame(2);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  ProcessAckPacket(&ack);

  // The original alarm will fire.  We should not time out because we had a
  // network event at t=5ms.  The alarm will reregister.
  clock_.AdvanceTime(initial_idle_timeout - five_ms);
  EXPECT_EQ(default_timeout, clock_.ApproximateNow());
  EXPECT_TRUE(connection_.connected());
  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_EQ(default_timeout + five_ms,
            connection_.GetTimeoutAlarm()->deadline());

  // This time, we should time out.
  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AtLeast(1));
  clock_.AdvanceTime(five_ms);
  EXPECT_EQ(default_timeout + five_ms, clock_.ApproximateNow());
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());
  TestConnectionCloseQuicErrorCode(QUIC_NETWORK_IDLE_TIMEOUT);
}

TEST_P(QuicConnectionTest, TimeoutAfterReceiveNotSendWhenUnacked) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  connection_.SetFromConfig(config);

  const QuicTime::Delta initial_idle_timeout =
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs - 1);
  connection_.SetNetworkTimeouts(
      QuicTime::Delta::Infinite(),
      initial_idle_timeout + QuicTime::Delta::FromSeconds(1));
  const QuicTime::Delta five_ms = QuicTime::Delta::FromMilliseconds(5);
  QuicTime default_timeout = clock_.ApproximateNow() + initial_idle_timeout;

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, NO_FIN);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      3, NO_FIN);

  EXPECT_EQ(default_timeout, connection_.GetTimeoutAlarm()->deadline());

  clock_.AdvanceTime(five_ms);

  // When we receive a packet, the timeout will change to 5ms +
  // kInitialIdleTimeoutSecs.
  QuicAckFrame ack = InitAckFrame(2);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  ProcessAckPacket(&ack);

  // The original alarm will fire.  We should not time out because we had a
  // network event at t=5ms.  The alarm will reregister.
  clock_.AdvanceTime(initial_idle_timeout - five_ms);
  EXPECT_EQ(default_timeout, clock_.ApproximateNow());
  EXPECT_TRUE(connection_.connected());
  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_EQ(default_timeout + five_ms,
            connection_.GetTimeoutAlarm()->deadline());

  // Now, send packets while advancing the time and verify that the connection
  // eventually times out.
  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AnyNumber());
  for (int i = 0; i < 100 && connection_.connected(); ++i) {
    QUIC_LOG(INFO) << "sending data packet";
    connection_.SendStreamDataWithString(
        GetNthClientInitiatedStreamId(1, connection_.transport_version()),
        "foo", 0, NO_FIN);
    connection_.GetTimeoutAlarm()->Fire();
    clock_.AdvanceTime(QuicTime::Delta::FromSeconds(1));
  }
  EXPECT_FALSE(connection_.connected());
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  TestConnectionCloseQuicErrorCode(QUIC_NETWORK_IDLE_TIMEOUT);
}

TEST_P(QuicConnectionTest, SendScheduler) {
  // Test that if we send a packet without delay, it is not queued.
  QuicFramerPeer::SetPerspective(&peer_framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> packet =
      ConstructDataPacket(1, !kHasStopWaiting, ENCRYPTION_INITIAL);
  QuicPacketCreatorPeer::SetPacketNumber(creator_, 1);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  connection_.SendPacket(ENCRYPTION_INITIAL, 1, std::move(packet),
                         HAS_RETRANSMITTABLE_DATA, false, false);
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, FailToSendFirstPacket) {
  // Test that the connection does not crash when it fails to send the first
  // packet at which point self_address_ might be uninitialized.
  QuicFramerPeer::SetPerspective(&peer_framer_, Perspective::IS_CLIENT);
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _)).Times(1);
  std::unique_ptr<QuicPacket> packet =
      ConstructDataPacket(1, !kHasStopWaiting, ENCRYPTION_INITIAL);
  QuicPacketCreatorPeer::SetPacketNumber(creator_, 1);
  writer_->SetShouldWriteFail();
  connection_.SendPacket(ENCRYPTION_INITIAL, 1, std::move(packet),
                         HAS_RETRANSMITTABLE_DATA, false, false);
}

TEST_P(QuicConnectionTest, SendSchedulerEAGAIN) {
  QuicFramerPeer::SetPerspective(&peer_framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> packet =
      ConstructDataPacket(1, !kHasStopWaiting, ENCRYPTION_INITIAL);
  QuicPacketCreatorPeer::SetPacketNumber(creator_, 1);
  BlockOnNextWrite();
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, QuicPacketNumber(2u), _, _))
      .Times(0);
  connection_.SendPacket(ENCRYPTION_INITIAL, 1, std::move(packet),
                         HAS_RETRANSMITTABLE_DATA, false, false);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, TestQueueLimitsOnSendStreamData) {
  // Queue the first packet.
  size_t payload_length = connection_.max_packet_length();
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillOnce(testing::Return(false));
  const std::string payload(payload_length, 'a');
  QuicStreamId first_bidi_stream_id(QuicUtils::GetFirstBidirectionalStreamId(
      connection_.version().transport_version, Perspective::IS_CLIENT));
  EXPECT_EQ(0u, connection_
                    .SendStreamDataWithString(first_bidi_stream_id, payload, 0,
                                              NO_FIN)
                    .bytes_consumed);
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, SendingThreePackets) {
  // Make the payload twice the size of the packet, so 3 packets are written.
  size_t total_payload_length = 2 * connection_.max_packet_length();
  const std::string payload(total_payload_length, 'a');
  QuicStreamId first_bidi_stream_id(QuicUtils::GetFirstBidirectionalStreamId(
      connection_.version().transport_version, Perspective::IS_CLIENT));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(3);
  EXPECT_EQ(payload.size(), connection_
                                .SendStreamDataWithString(first_bidi_stream_id,
                                                          payload, 0, NO_FIN)
                                .bytes_consumed);
}

TEST_P(QuicConnectionTest, LoopThroughSendingPacketsWithTruncation) {
  set_perspective(Perspective::IS_SERVER);
  // Set up a larger payload than will fit in one packet.
  const std::string payload(connection_.max_packet_length(), 'a');
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _)).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillRepeatedly(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillRepeatedly(Return(false));

  // Now send some packets with no truncation.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
  EXPECT_EQ(payload.size(),
            connection_.SendStreamDataWithString(3, payload, 0, NO_FIN)
                .bytes_consumed);
  // Track the size of the second packet here.  The overhead will be the largest
  // we see in this test, due to the non-truncated connection id.
  size_t non_truncated_packet_size = writer_->last_packet_size();

  // Change to a 0 byte connection id.
  QuicConfig config;
  QuicConfigPeer::SetReceivedBytesForConnectionId(&config, 0);
  connection_.SetFromConfig(config);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
  EXPECT_EQ(payload.size(),
            connection_.SendStreamDataWithString(3, payload, 1350, NO_FIN)
                .bytes_consumed);
  // Short header packets sent from server omit connection ID already, and
  // stream offset size increases from 0 to 2.
  EXPECT_EQ(non_truncated_packet_size, writer_->last_packet_size() - 2);
}

TEST_P(QuicConnectionTest, SendDelayedAck) {
  QuicTime ack_time = clock_.ApproximateNow() + DefaultDelayedAckTime();
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.HasPendingAcks());
  SetDecrypter(ENCRYPTION_ZERO_RTT,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_ZERO_RTT));
  peer_framer_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
  // Process a packet from the non-crypto stream.
  frame1_.stream_id = 3;

  // The same as ProcessPacket(1) except that ENCRYPTION_ZERO_RTT is used
  // instead of ENCRYPTION_INITIAL.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(1, !kHasStopWaiting, ENCRYPTION_ZERO_RTT);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.HasPendingAcks());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());
  // Simulate delayed ack alarm firing.
  clock_.AdvanceTime(DefaultDelayedAckTime());
  connection_.GetAckAlarm()->Fire();
  // Check that ack is sent and that delayed ack alarm is reset.
  size_t padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + 1u, writer_->frame_count());
  EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.HasPendingAcks());
}

TEST_P(QuicConnectionTest, SendDelayedAckDecimation) {
  EXPECT_CALL(visitor_, OnAckNeedsRetransmittableFrame()).Times(AnyNumber());

  const size_t kMinRttMs = 40;
  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(kMinRttMs),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  // The ack time should be based on min_rtt/4, since it's less than the
  // default delayed ack time.
  QuicTime ack_time = clock_.ApproximateNow() +
                      QuicTime::Delta::FromMilliseconds(kMinRttMs / 4);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.HasPendingAcks());
  SetDecrypter(ENCRYPTION_ZERO_RTT,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_ZERO_RTT));
  peer_framer_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
  // Process a packet from the non-crypto stream.
  frame1_.stream_id = 3;

  // Process all the initial packets in order so there aren't missing packets.
  uint64_t kFirstDecimatedPacket = 101;
  for (unsigned int i = 0; i < kFirstDecimatedPacket - 1; ++i) {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(1 + i, !kHasStopWaiting, ENCRYPTION_ZERO_RTT);
  }
  EXPECT_FALSE(connection_.HasPendingAcks());
  // The same as ProcessPacket(1) except that ENCRYPTION_ZERO_RTT is used
  // instead of ENCRYPTION_INITIAL.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(kFirstDecimatedPacket, !kHasStopWaiting,
                           ENCRYPTION_ZERO_RTT);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.HasPendingAcks());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

  // The 10th received packet causes an ack to be sent.
  for (int i = 0; i < 9; ++i) {
    EXPECT_TRUE(connection_.HasPendingAcks());
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(kFirstDecimatedPacket + 1 + i, !kHasStopWaiting,
                             ENCRYPTION_ZERO_RTT);
  }
  // Check that ack is sent and that delayed ack alarm is reset.
  size_t padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + 1u, writer_->frame_count());
  EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.HasPendingAcks());
}

TEST_P(QuicConnectionTest, SendDelayedAckDecimationUnlimitedAggregation) {
  EXPECT_CALL(visitor_, OnAckNeedsRetransmittableFrame()).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  QuicTagVector connection_options;
  // No limit on the number of packets received before sending an ack.
  connection_options.push_back(kAKDU);
  config.SetConnectionOptionsToSend(connection_options);
  connection_.SetFromConfig(config);

  const size_t kMinRttMs = 40;
  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(kMinRttMs),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  // The ack time should be based on min_rtt/4, since it's less than the
  // default delayed ack time.
  QuicTime ack_time = clock_.ApproximateNow() +
                      QuicTime::Delta::FromMilliseconds(kMinRttMs / 4);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.HasPendingAcks());
  SetDecrypter(ENCRYPTION_ZERO_RTT,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_ZERO_RTT));
  peer_framer_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
  // Process a packet from the non-crypto stream.
  frame1_.stream_id = 3;

  // Process all the initial packets in order so there aren't missing packets.
  uint64_t kFirstDecimatedPacket = 101;
  for (unsigned int i = 0; i < kFirstDecimatedPacket - 1; ++i) {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(1 + i, !kHasStopWaiting, ENCRYPTION_ZERO_RTT);
  }
  EXPECT_FALSE(connection_.HasPendingAcks());
  // The same as ProcessPacket(1) except that ENCRYPTION_ZERO_RTT is used
  // instead of ENCRYPTION_INITIAL.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(kFirstDecimatedPacket, !kHasStopWaiting,
                           ENCRYPTION_ZERO_RTT);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.HasPendingAcks());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

  // 18 packets will not cause an ack to be sent.  19 will because when
  // stop waiting frames are in use, we ack every 20 packets no matter what.
  for (int i = 0; i < 18; ++i) {
    EXPECT_TRUE(connection_.HasPendingAcks());
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(kFirstDecimatedPacket + 1 + i, !kHasStopWaiting,
                             ENCRYPTION_ZERO_RTT);
  }
  // The delayed ack timer should still be set to the expected deadline.
  EXPECT_TRUE(connection_.HasPendingAcks());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());
}

TEST_P(QuicConnectionTest, SendDelayedAckDecimationEighthRtt) {
  EXPECT_CALL(visitor_, OnAckNeedsRetransmittableFrame()).Times(AnyNumber());
  QuicConnectionPeer::SetAckDecimationDelay(&connection_, 0.125);

  const size_t kMinRttMs = 40;
  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(kMinRttMs),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  // The ack time should be based on min_rtt/8, since it's less than the
  // default delayed ack time.
  QuicTime ack_time = clock_.ApproximateNow() +
                      QuicTime::Delta::FromMilliseconds(kMinRttMs / 8);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.HasPendingAcks());
  SetDecrypter(ENCRYPTION_ZERO_RTT,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_ZERO_RTT));
  peer_framer_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
  // Process a packet from the non-crypto stream.
  frame1_.stream_id = 3;

  // Process all the initial packets in order so there aren't missing packets.
  uint64_t kFirstDecimatedPacket = 101;
  for (unsigned int i = 0; i < kFirstDecimatedPacket - 1; ++i) {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(1 + i, !kHasStopWaiting, ENCRYPTION_ZERO_RTT);
  }
  EXPECT_FALSE(connection_.HasPendingAcks());
  // The same as ProcessPacket(1) except that ENCRYPTION_ZERO_RTT is used
  // instead of ENCRYPTION_INITIAL.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(kFirstDecimatedPacket, !kHasStopWaiting,
                           ENCRYPTION_ZERO_RTT);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.HasPendingAcks());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

  // The 10th received packet causes an ack to be sent.
  for (int i = 0; i < 9; ++i) {
    EXPECT_TRUE(connection_.HasPendingAcks());
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(kFirstDecimatedPacket + 1 + i, !kHasStopWaiting,
                             ENCRYPTION_ZERO_RTT);
  }
  // Check that ack is sent and that delayed ack alarm is reset.
  size_t padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + 1u, writer_->frame_count());
  EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.HasPendingAcks());
}

TEST_P(QuicConnectionTest, SendDelayedAckOnHandshakeConfirmed) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessPacket(1);
  // Check that ack is sent and that delayed ack alarm is set.
  EXPECT_TRUE(connection_.HasPendingAcks());
  QuicTime ack_time = clock_.ApproximateNow() + DefaultDelayedAckTime();
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

  // Completing the handshake as the server does nothing.
  QuicConnectionPeer::SetPerspective(&connection_, Perspective::IS_SERVER);
  connection_.OnHandshakeComplete();
  EXPECT_TRUE(connection_.HasPendingAcks());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

  // Complete the handshake as the client decreases the delayed ack time to 0ms.
  QuicConnectionPeer::SetPerspective(&connection_, Perspective::IS_CLIENT);
  connection_.OnHandshakeComplete();
  EXPECT_TRUE(connection_.HasPendingAcks());
  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    EXPECT_EQ(clock_.ApproximateNow() + DefaultDelayedAckTime(),
              connection_.GetAckAlarm()->deadline());
  } else {
    EXPECT_EQ(clock_.ApproximateNow(), connection_.GetAckAlarm()->deadline());
  }
}

TEST_P(QuicConnectionTest, SendDelayedAckOnSecondPacket) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessPacket(1);
  ProcessPacket(2);
  // Check that ack is sent and that delayed ack alarm is reset.
  size_t padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + 1u, writer_->frame_count());
  EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.HasPendingAcks());
}

TEST_P(QuicConnectionTest, NoAckOnOldNacks) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  ProcessPacket(2);
  size_t frames_per_ack = 1;

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  ProcessPacket(3);
  size_t padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + frames_per_ack, writer_->frame_count());
  EXPECT_FALSE(writer_->ack_frames().empty());
  writer_->Reset();

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  ProcessPacket(4);
  EXPECT_EQ(0u, writer_->frame_count());

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  ProcessPacket(5);
  padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + frames_per_ack, writer_->frame_count());
  EXPECT_FALSE(writer_->ack_frames().empty());
  writer_->Reset();

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  // Now only set the timer on the 6th packet, instead of sending another ack.
  ProcessPacket(6);
  padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count, writer_->frame_count());
  EXPECT_TRUE(connection_.HasPendingAcks());
}

TEST_P(QuicConnectionTest, SendDelayedAckOnOutgoingPacket) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnStreamFrame(_));
  peer_framer_.SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
  SetDecrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_FORWARD_SECURE));
  ProcessDataPacket(1);
  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, NO_FIN);
  // Check that ack is bundled with outgoing data and that delayed ack
  // alarm is reset.
  EXPECT_EQ(2u, writer_->frame_count());
  EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.HasPendingAcks());
}

TEST_P(QuicConnectionTest, SendDelayedAckOnOutgoingCryptoPacket) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(1);
  } else {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  }
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_INITIAL);
  connection_.SendCryptoDataWithString("foo", 0);
  // Check that ack is bundled with outgoing crypto data.
  EXPECT_FALSE(writer_->ack_frames().empty());
  if (!QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_FALSE(writer_->stream_frames().empty());
  } else {
    EXPECT_FALSE(writer_->crypto_frames().empty());
  }
  EXPECT_FALSE(writer_->padding_frames().empty());
  EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  EXPECT_FALSE(connection_.HasPendingAcks());
}

TEST_P(QuicConnectionTest, BlockAndBufferOnFirstCHLOPacketOfTwo) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessPacket(1);
  BlockOnNextWrite();
  writer_->set_is_write_blocked_data_buffered(true);
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  } else {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
  }
  connection_.SendCryptoDataWithString("foo", 0);
  EXPECT_TRUE(writer_->IsWriteBlocked());
  EXPECT_FALSE(connection_.HasQueuedData());
  connection_.SendCryptoDataWithString("bar", 3);
  EXPECT_TRUE(writer_->IsWriteBlocked());
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    // CRYPTO frames are not flushed when writer is blocked.
    EXPECT_FALSE(connection_.HasQueuedData());
  } else {
    EXPECT_TRUE(connection_.HasQueuedData());
  }
}

TEST_P(QuicConnectionTest, BundleAckForSecondCHLO) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.HasPendingAcks());
  EXPECT_CALL(visitor_, OnCanWrite())
      .WillOnce(IgnoreResult(InvokeWithoutArgs(
          &connection_, &TestConnection::SendCryptoStreamData)));
  // Process a packet from the crypto stream, which is frame1_'s default.
  // Receiving the CHLO as packet 2 first will cause the connection to
  // immediately send an ack, due to the packet gap.
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(1);
  } else {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  }
  ForceWillingAndAbleToWriteOnceForDeferSending();
  ProcessCryptoPacketAtLevel(2, ENCRYPTION_INITIAL);
  // Check that ack is sent and that delayed ack alarm is reset.
  EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  if (!QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_FALSE(writer_->stream_frames().empty());
  } else {
    EXPECT_FALSE(writer_->crypto_frames().empty());
  }
  EXPECT_FALSE(writer_->padding_frames().empty());
  ASSERT_FALSE(writer_->ack_frames().empty());
  EXPECT_EQ(QuicPacketNumber(2u), LargestAcked(writer_->ack_frames().front()));
  EXPECT_FALSE(connection_.HasPendingAcks());
}

TEST_P(QuicConnectionTest, BundleAckForSecondCHLOTwoPacketReject) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.HasPendingAcks());

  // Process two packets from the crypto stream, which is frame1_'s default,
  // simulating a 2 packet reject.
  {
    if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
      EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(1);
    } else {
      EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    }
    ProcessCryptoPacketAtLevel(1, ENCRYPTION_INITIAL);
    // Send the new CHLO when the REJ is processed.
    if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
      EXPECT_CALL(visitor_, OnCryptoFrame(_))
          .WillOnce(IgnoreResult(InvokeWithoutArgs(
              &connection_, &TestConnection::SendCryptoStreamData)));
    } else {
      EXPECT_CALL(visitor_, OnStreamFrame(_))
          .WillOnce(IgnoreResult(InvokeWithoutArgs(
              &connection_, &TestConnection::SendCryptoStreamData)));
    }
    ForceWillingAndAbleToWriteOnceForDeferSending();
    ProcessCryptoPacketAtLevel(2, ENCRYPTION_INITIAL);
  }
  // Check that ack is sent and that delayed ack alarm is reset.
  EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  if (!QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_FALSE(writer_->stream_frames().empty());
  } else {
    EXPECT_FALSE(writer_->crypto_frames().empty());
  }
  EXPECT_FALSE(writer_->padding_frames().empty());
  ASSERT_FALSE(writer_->ack_frames().empty());
  EXPECT_EQ(QuicPacketNumber(2u), LargestAcked(writer_->ack_frames().front()));
  EXPECT_FALSE(connection_.HasPendingAcks());
}

TEST_P(QuicConnectionTest, BundleAckWithDataOnIncomingAck) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, NO_FIN);
  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      3, NO_FIN);
  // Ack the second packet, which will retransmit the first packet.
  QuicAckFrame ack = ConstructAckFrame(2, 1);
  LostPacketVector lost_packets;
  lost_packets.push_back(
      LostPacket(QuicPacketNumber(1), kMaxOutgoingPacketSize));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(lost_packets),
                      Return(LossDetectionInterface::DetectionStats())));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  ProcessAckPacket(&ack);
  size_t padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + 1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->stream_frames().size());
  writer_->Reset();

  // Now ack the retransmission, which will both raise the high water mark
  // and see if there is more data to send.
  ack = ConstructAckFrame(3, 1);
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  ProcessAckPacket(&ack);

  // Check that no packet is sent and the ack alarm isn't set.
  EXPECT_EQ(0u, writer_->frame_count());
  EXPECT_FALSE(connection_.HasPendingAcks());
  writer_->Reset();

  // Send the same ack, but send both data and an ack together.
  ack = ConstructAckFrame(3, 1);
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(visitor_, OnCanWrite())
      .WillOnce(IgnoreResult(InvokeWithoutArgs(
          &connection_, &TestConnection::EnsureWritableAndSendStreamData5)));
  ForceWillingAndAbleToWriteOnceForDeferSending();
  ProcessAckPacket(&ack);

  // Check that ack is bundled with outgoing data and the delayed ack
  // alarm is reset.
  // Do not ACK acks.
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_TRUE(writer_->ack_frames().empty());
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_FALSE(connection_.HasPendingAcks());
}

TEST_P(QuicConnectionTest, NoAckSentForClose) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessPacket(1);
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_PEER))
      .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  ProcessClosePacket(2);
  EXPECT_EQ(1, connection_close_frame_count_);
  EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
              IsError(QUIC_PEER_GOING_AWAY));
}

TEST_P(QuicConnectionTest, SendWhenDisconnected) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));
  connection_.CloseConnection(QUIC_PEER_GOING_AWAY, "no reason",
                              ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_FALSE(connection_.connected());
  EXPECT_FALSE(connection_.CanWrite(HAS_RETRANSMITTABLE_DATA));
  EXPECT_EQ(DISCARD, connection_.GetSerializedPacketFate(
                         /*is_mtu_discovery=*/false, ENCRYPTION_INITIAL));
}

TEST_P(QuicConnectionTest, SendConnectivityProbingWhenDisconnected) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (!IsDefaultTestConfiguration()) {
    return;
  }

  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));
  connection_.CloseConnection(QUIC_PEER_GOING_AWAY, "no reason",
                              ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_FALSE(connection_.connected());
  EXPECT_FALSE(connection_.CanWrite(HAS_RETRANSMITTABLE_DATA));

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, QuicPacketNumber(1), _, _))
      .Times(0);

  EXPECT_QUIC_BUG(connection_.SendConnectivityProbingPacket(
                      writer_.get(), connection_.peer_address()),
                  "Not sending connectivity probing packet as connection is "
                  "disconnected.");
  EXPECT_EQ(1, connection_close_frame_count_);
  EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
              IsError(QUIC_PEER_GOING_AWAY));
}

TEST_P(QuicConnectionTest, WriteBlockedAfterClientSendsConnectivityProbe) {
  PathProbeTestInit(Perspective::IS_CLIENT);
  TestPacketWriter probing_writer(version(), &clock_, Perspective::IS_CLIENT);
  // Block next write so that sending connectivity probe will encounter a
  // blocked write when send a connectivity probe to the peer.
  probing_writer.BlockOnNextWrite();
  // Connection will not be marked as write blocked as connectivity probe only
  // affects the probing_writer which is not the default.
  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(0);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, QuicPacketNumber(1), _, _))
      .Times(1);
  connection_.SendConnectivityProbingPacket(&probing_writer,
                                            connection_.peer_address());
}

TEST_P(QuicConnectionTest, WriterBlockedAfterServerSendsConnectivityProbe) {
  PathProbeTestInit(Perspective::IS_SERVER);
  if (version().SupportsAntiAmplificationLimit()) {
    QuicConnectionPeer::SetAddressValidated(&connection_);
  }

  // Block next write so that sending connectivity probe will encounter a
  // blocked write when send a connectivity probe to the peer.
  writer_->BlockOnNextWrite();
  // Connection will be marked as write blocked as server uses the default
  // writer to send connectivity probes.
  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(1);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, QuicPacketNumber(1), _, _))
      .Times(1);
  if (VersionHasIetfQuicFrames(GetParam().version.transport_version)) {
    QuicPathFrameBuffer payload{
        {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xfe}};
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    connection_.SendPathChallenge(
        payload, connection_.self_address(), connection_.peer_address(),
        connection_.effective_peer_address(), writer_.get());
  } else {
    connection_.SendConnectivityProbingPacket(writer_.get(),
                                              connection_.peer_address());
  }
}

TEST_P(QuicConnectionTest, WriterErrorWhenClientSendsConnectivityProbe) {
  PathProbeTestInit(Perspective::IS_CLIENT);
  TestPacketWriter probing_writer(version(), &clock_, Perspective::IS_CLIENT);
  probing_writer.SetShouldWriteFail();

  // Connection should not be closed if a connectivity probe is failed to be
  // sent.
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _)).Times(0);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, QuicPacketNumber(1), _, _))
      .Times(0);
  connection_.SendConnectivityProbingPacket(&probing_writer,
                                            connection_.peer_address());
}

TEST_P(QuicConnectionTest, WriterErrorWhenServerSendsConnectivityProbe) {
  PathProbeTestInit(Perspective::IS_SERVER);

  writer_->SetShouldWriteFail();
  // Connection should not be closed if a connectivity probe is failed to be
  // sent.
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _)).Times(0);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, QuicPacketNumber(1), _, _))
      .Times(0);
  connection_.SendConnectivityProbingPacket(writer_.get(),
                                            connection_.peer_address());
}

TEST_P(QuicConnectionTest, IetfStatelessReset) {
  QuicConfig config;
  QuicConfigPeer::SetReceivedStatelessResetToken(&config,
                                                 kTestStatelessResetToken);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  std::unique_ptr<QuicEncryptedPacket> packet(
      QuicFramer::BuildIetfStatelessResetPacket(connection_id_,
                                                /*received_packet_length=*/100,
                                                kTestStatelessResetToken));
  std::unique_ptr<QuicReceivedPacket> received(
      ConstructReceivedPacket(*packet, QuicTime::Zero()));
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_PEER))
      .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));
  connection_.ProcessUdpPacket(kSelfAddress, kPeerAddress, *received);
  EXPECT_EQ(1, connection_close_frame_count_);
  EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
              IsError(QUIC_PUBLIC_RESET));
}

TEST_P(QuicConnectionTest, GoAway) {
  if (VersionHasIetfQuicFrames(GetParam().version.transport_version)) {
    // GoAway is not available in version 99.
    return;
  }

  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  QuicGoAwayFrame* goaway = new QuicGoAwayFrame();
  goaway->last_good_stream_id = 1;
  goaway->error_code = QUIC_PEER_GOING_AWAY;
  goaway->reason_phrase = "Going away.";
  EXPECT_CALL(visitor_, OnGoAway(_));
  ProcessGoAwayPacket(goaway);
}

TEST_P(QuicConnectionTest, WindowUpdate) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  QuicWindowUpdateFrame window_update;
  window_update.stream_id = 3;
  window_update.max_data = 1234;
  EXPECT_CALL(visitor_, OnWindowUpdateFrame(_));
  ProcessFramePacket(QuicFrame(window_update));
}

TEST_P(QuicConnectionTest, Blocked) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  QuicBlockedFrame blocked;
  blocked.stream_id = 3;
  EXPECT_CALL(visitor_, OnBlockedFrame(_));
  ProcessFramePacket(QuicFrame(blocked));
  EXPECT_EQ(1u, connection_.GetStats().blocked_frames_received);
  EXPECT_EQ(0u, connection_.GetStats().blocked_frames_sent);
}

TEST_P(QuicConnectionTest, ZeroBytePacket) {
  // Don't close the connection for zero byte packets.
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _)).Times(0);
  QuicReceivedPacket encrypted(nullptr, 0, QuicTime::Zero());
  connection_.ProcessUdpPacket(kSelfAddress, kPeerAddress, encrypted);
}

TEST_P(QuicConnectionTest, ClientHandlesVersionNegotiation) {
  // All supported versions except the one the connection supports.
  ParsedQuicVersionVector versions;
  for (auto version : AllSupportedVersions()) {
    if (version != connection_.version()) {
      versions.push_back(version);
    }
  }

  // Send a version negotiation packet.
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      QuicFramer::BuildVersionNegotiationPacket(
          connection_id_, EmptyQuicConnectionId(), /*ietf_quic=*/true,
          connection_.version().HasLengthPrefixedConnectionIds(), versions));
  std::unique_ptr<QuicReceivedPacket> received(
      ConstructReceivedPacket(*encrypted, QuicTime::Zero()));
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));
  // Verify no connection close packet gets sent.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.ProcessUdpPacket(kSelfAddress, kPeerAddress, *received);
  EXPECT_FALSE(connection_.connected());
  EXPECT_EQ(1, connection_close_frame_count_);
  EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
              IsError(QUIC_INVALID_VERSION));
}

TEST_P(QuicConnectionTest, ClientHandlesVersionNegotiationWithConnectionClose) {
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(kINVC);
  config.SetClientConnectionOptions(connection_options);
  connection_.SetFromConfig(config);

  // All supported versions except the one the connection supports.
  ParsedQuicVersionVector versions;
  for (auto version : AllSupportedVersions()) {
    if (version != connection_.version()) {
      versions.push_back(version);
    }
  }

  // Send a version negotiation packet.
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      QuicFramer::BuildVersionNegotiationPacket(
          connection_id_, EmptyQuicConnectionId(), /*ietf_quic=*/true,
          connection_.version().HasLengthPrefixedConnectionIds(), versions));
  std::unique_ptr<QuicReceivedPacket> received(
      ConstructReceivedPacket(*encrypted, QuicTime::Zero()));
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));
  // Verify connection close packet gets sent.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AtLeast(1u));
  connection_.ProcessUdpPacket(kSelfAddress, kPeerAddress, *received);
  EXPECT_FALSE(connection_.connected());
  EXPECT_EQ(1, connection_close_frame_count_);
  EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
              IsError(QUIC_INVALID_VERSION));
}

TEST_P(QuicConnectionTest, BadVersionNegotiation) {
  // Send a version negotiation packet with the version the client started with.
  // It should be rejected.
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      QuicFramer::BuildVersionNegotiationPacket(
          connection_id_, EmptyQuicConnectionId(), /*ietf_quic=*/true,
          connection_.version().HasLengthPrefixedConnectionIds(),
          AllSupportedVersions()));
  std::unique_ptr<QuicReceivedPacket> received(
      ConstructReceivedPacket(*encrypted, QuicTime::Zero()));
  connection_.ProcessUdpPacket(kSelfAddress, kPeerAddress, *received);
  EXPECT_EQ(1, connection_close_frame_count_);
  EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
              IsError(QUIC_INVALID_VERSION_NEGOTIATION_PACKET));
}

TEST_P(QuicConnectionTest, ProcessFramesIfPacketClosedConnection) {
  // Construct a packet with stream frame and connection close frame.
  QuicPacketHeader header;
  if (peer_framer_.perspective() == Perspective::IS_SERVER) {
    header.source_connection_id = connection_id_;
    header.destination_connection_id_included = CONNECTION_ID_ABSENT;
  } else {
    header.destination_connection_id = connection_id_;
    header.destination_connection_id_included = CONNECTION_ID_ABSENT;
  }
  header.packet_number = QuicPacketNumber(1);
  header.version_flag = false;

  QuicErrorCode kQuicErrorCode = QUIC_PEER_GOING_AWAY;
  // This QuicConnectionCloseFrame will default to being for a Google QUIC
  // close. If doing IETF QUIC then set fields appropriately for CC/T or CC/A,
  // depending on the mapping.
  QuicConnectionCloseFrame qccf(peer_framer_.transport_version(),
                                kQuicErrorCode, NO_IETF_QUIC_ERROR, "",
                                /*transport_close_frame_type=*/0);
  QuicFrames frames;
  frames.push_back(QuicFrame(frame1_));
  frames.push_back(QuicFrame(&qccf));
  std::unique_ptr<QuicPacket> packet(ConstructPacket(header, frames));
  EXPECT_TRUE(nullptr != packet);
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length = peer_framer_.EncryptPayload(
      ENCRYPTION_FORWARD_SECURE, QuicPacketNumber(1), *packet, buffer,
      kMaxOutgoingPacketSize);

  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_PEER))
      .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(buffer, encrypted_length, QuicTime::Zero(), false));
  EXPECT_EQ(1, connection_close_frame_count_);
  EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
              IsError(QUIC_PEER_GOING_AWAY));
}

TEST_P(QuicConnectionTest, SelectMutualVersion) {
  connection_.SetSupportedVersions(AllSupportedVersions());
  // Set the connection to speak the lowest quic version.
  connection_.set_version(QuicVersionMin());
  EXPECT_EQ(QuicVersionMin(), connection_.version());

  // Pass in available versions which includes a higher mutually supported
  // version.  The higher mutually supported version should be selected.
  ParsedQuicVersionVector supported_versions = AllSupportedVersions();
  EXPECT_TRUE(connection_.SelectMutualVersion(supported_versions));
  EXPECT_EQ(QuicVersionMax(), connection_.version());

  // Expect that the lowest version is selected.
  // Ensure the lowest supported version is less than the max, unless they're
  // the same.
  ParsedQuicVersionVector lowest_version_vector;
  lowest_version_vector.push_back(QuicVersionMin());
  EXPECT_TRUE(connection_.SelectMutualVersion(lowest_version_vector));
  EXPECT_EQ(QuicVersionMin(), connection_.version());

  // Shouldn't be able to find a mutually supported version.
  ParsedQuicVersionVector unsupported_version;
  unsupported_version.push_back(UnsupportedQuicVersion());
  EXPECT_FALSE(connection_.SelectMutualVersion(unsupported_version));
}

TEST_P(QuicConnectionTest, ConnectionCloseWhenWritable) {
  EXPECT_FALSE(writer_->IsWriteBlocked());

  // Send a packet.
  connection_.SendStreamDataWithString(1, "foo", 0, NO_FIN);
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_EQ(1u, writer_->packets_write_attempts());

  TriggerConnectionClose();
  EXPECT_LE(2u, writer_->packets_write_attempts());
}

TEST_P(QuicConnectionTest, ConnectionCloseGettingWriteBlocked) {
  BlockOnNextWrite();
  TriggerConnectionClose();
  EXPECT_EQ(1u, writer_->packets_write_attempts());
  EXPECT_TRUE(writer_->IsWriteBlocked());
}

TEST_P(QuicConnectionTest, ConnectionCloseWhenWriteBlocked) {
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(1, "foo", 0, NO_FIN);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());
  EXPECT_EQ(1u, writer_->packets_write_attempts());
  EXPECT_TRUE(writer_->IsWriteBlocked());
  TriggerConnectionClose();
  EXPECT_EQ(1u, writer_->packets_write_attempts());
}

TEST_P(QuicConnectionTest, OnPacketSentDebugVisitor) {
  PathProbeTestInit(Perspective::IS_CLIENT);
  MockQuicConnectionDebugVisitor debug_visitor;
  connection_.set_debug_visitor(&debug_visitor);

  EXPECT_CALL(debug_visitor, OnPacketSent(_, _, _, _, _, _, _, _, _)).Times(1);
  connection_.SendStreamDataWithString(1, "foo", 0, NO_FIN);

  EXPECT_CALL(debug_visitor, OnPacketSent(_, _, _, _, _, _, _, _, _)).Times(1);
  connection_.SendConnectivityProbingPacket(writer_.get(),
                                            connection_.peer_address());
}

TEST_P(QuicConnectionTest, OnPacketHeaderDebugVisitor) {
  QuicPacketHeader header;
  header.packet_number = QuicPacketNumber(1);
  header.form = IETF_QUIC_LONG_HEADER_PACKET;

  MockQuicConnectionDebugVisitor debug_visitor;
  connection_.set_debug_visitor(&debug_visitor);
  EXPECT_CALL(debug_visitor, OnPacketHeader(Ref(header), _, _)).Times(1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_)).Times(1);
  EXPECT_CALL(debug_visitor, OnSuccessfulVersionNegotiation(_)).Times(1);
  connection_.OnPacketHeader(header);
}

TEST_P(QuicConnectionTest, OnPacketHeaderReturnValue) {
  QuicPacketHeader header;
  header.packet_number = QuicPacketNumber(1);
  header.form = IETF_QUIC_LONG_HEADER_PACKET;
  EXPECT_TRUE(connection_.OnPacketHeader(header));

  EXPECT_CALL(visitor_, OnConnectionClosed(_, _)).Times(1);
  connection_.CloseConnection(QUIC_NO_ERROR, "Closed by test",
                              ConnectionCloseBehavior::SILENT_CLOSE);

  header.packet_number = QuicPacketNumber(2);
  if (!GetQuicReloadableFlag(quic_on_packet_header_return_connected)) {
    EXPECT_QUICHE_DEBUG_DEATH(connection_.OnPacketHeader(header), ".*");
    return;
  }

  EXPECT_FALSE(connection_.OnPacketHeader(header));
}

TEST_P(QuicConnectionTest, Pacing) {
  TestConnection server(connection_id_, kPeerAddress, kSelfAddress,
                        helper_.get(), alarm_factory_.get(), writer_.get(),
                        Perspective::IS_SERVER, version(),
                        connection_id_generator_);
  TestConnection client(connection_id_, kSelfAddress, kPeerAddress,
                        helper_.get(), alarm_factory_.get(), writer_.get(),
                        Perspective::IS_CLIENT, version(),
                        connection_id_generator_);
  EXPECT_FALSE(QuicSentPacketManagerPeer::UsingPacing(
      static_cast<const QuicSentPacketManager*>(
          &client.sent_packet_manager())));
  EXPECT_FALSE(QuicSentPacketManagerPeer::UsingPacing(
      static_cast<const QuicSentPacketManager*>(
          &server.sent_packet_manager())));
}

TEST_P(QuicConnectionTest, WindowUpdateInstigateAcks) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Send a WINDOW_UPDATE frame.
  QuicWindowUpdateFrame window_update;
  window_update.stream_id = 3;
  window_update.max_data = 1234;
  EXPECT_CALL(visitor_, OnWindowUpdateFrame(_));
  ProcessFramePacket(QuicFrame(window_update));

  // Ensure that this has caused the ACK alarm to be set.
  EXPECT_TRUE(connection_.HasPendingAcks());
}

TEST_P(QuicConnectionTest, BlockedFrameInstigateAcks) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Send a BLOCKED frame.
  QuicBlockedFrame blocked;
  blocked.stream_id = 3;
  EXPECT_CALL(visitor_, OnBlockedFrame(_));
  ProcessFramePacket(QuicFrame(blocked));

  // Ensure that this has caused the ACK alarm to be set.
  EXPECT_TRUE(connection_.HasPendingAcks());
}

TEST_P(QuicConnectionTest, ReevaluateTimeUntilSendOnAck) {
  // Enable pacing.
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  connection_.SetFromConfig(config);

  // Send two packets.  One packet is not sufficient because if it gets acked,
  // there will be no packets in flight after that and the pacer will always
  // allow the next packet in that situation.
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, NO_FIN);
  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "bar",
      3, NO_FIN);
  connection_.OnCanWrite();

  // Schedule the next packet for a few milliseconds in future.
  QuicSentPacketManagerPeer::DisablePacerBursts(manager_);
  QuicTime scheduled_pacing_time =
      clock_.Now() + QuicTime::Delta::FromMilliseconds(5);
  QuicSentPacketManagerPeer::SetNextPacedPacketTime(manager_,
                                                    scheduled_pacing_time);

  // Send a packet and have it be blocked by congestion control.
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(false));
  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "baz",
      6, NO_FIN);
  EXPECT_FALSE(connection_.GetSendAlarm()->IsSet());

  // Process an ack and the send alarm will be set to the new 5ms delay.
  QuicAckFrame ack = InitAckFrame(1);
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
  ProcessAckPacket(&ack);
  size_t padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + 1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_TRUE(connection_.GetSendAlarm()->IsSet());
  EXPECT_EQ(scheduled_pacing_time, connection_.GetSendAlarm()->deadline());
  writer_->Reset();
}

TEST_P(QuicConnectionTest, SendAcksImmediately) {
  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacket(1);
  CongestionBlockWrites();
  SendAckPacketToPeer();
}

TEST_P(QuicConnectionTest, SendPingImmediately) {
  MockQuicConnectionDebugVisitor debug_visitor;
  connection_.set_debug_visitor(&debug_visitor);

  CongestionBlockWrites();
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  EXPECT_CALL(debug_visitor, OnPacketSent(_, _, _, _, _, _, _, _, _)).Times(1);
  EXPECT_CALL(debug_visitor, OnPingSent()).Times(1);
  connection_.SendControlFrame(QuicFrame(QuicPingFrame(1)));
  EXPECT_FALSE(connection_.HasQueuedData());
}

TEST_P(QuicConnectionTest, SendBlockedImmediately) {
  MockQuicConnectionDebugVisitor debug_visitor;
  connection_.set_debug_visitor(&debug_visitor);

  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  EXPECT_CALL(debug_visitor, OnPacketSent(_, _, _, _, _, _, _, _, _)).Times(1);
  EXPECT_EQ(0u, connection_.GetStats().blocked_frames_sent);
  connection_.SendControlFrame(QuicFrame(QuicBlockedFrame(1, 3, 0)));
  EXPECT_EQ(1u, connection_.GetStats().blocked_frames_sent);
  EXPECT_FALSE(connection_.HasQueuedData());
}

TEST_P(QuicConnectionTest, FailedToSendBlockedFrames) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  MockQuicConnectionDebugVisitor debug_visitor;
  connection_.set_debug_visitor(&debug_visitor);
  QuicBlockedFrame blocked(1, 3, 0);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  EXPECT_CALL(debug_visitor, OnPacketSent(_, _, _, _, _, _, _, _, _)).Times(0);
  EXPECT_EQ(0u, connection_.GetStats().blocked_frames_sent);
  connection_.SendControlFrame(QuicFrame(blocked));
  EXPECT_EQ(0u, connection_.GetStats().blocked_frames_sent);
  EXPECT_FALSE(connection_.HasQueuedData());
}

TEST_P(QuicConnectionTest, SendingUnencryptedStreamDataFails) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (!IsDefaultTestConfiguration()) {
    return;
  }

  EXPECT_QUIC_BUG(
      {
        EXPECT_CALL(visitor_,
                    OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
            .WillOnce(
                Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));
        connection_.SaveAndSendStreamData(3, {}, 0, FIN);
        EXPECT_FALSE(connection_.connected());
        EXPECT_EQ(1, connection_close_frame_count_);
        EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
                    IsError(QUIC_ATTEMPT_TO_SEND_UNENCRYPTED_STREAM_DATA));
      },
      "Cannot send stream data with level: ENCRYPTION_INITIAL");
}

TEST_P(QuicConnectionTest, SetRetransmissionAlarmForCryptoPacket) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendCryptoStreamData();

  // Verify retransmission timer is correctly set after crypto packet has been
  // sent.
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  QuicTime retransmission_time =
      QuicConnectionPeer::GetSentPacketManager(&connection_)
          ->GetRetransmissionTime();
  EXPECT_NE(retransmission_time, clock_.ApproximateNow());
  EXPECT_EQ(retransmission_time,
            connection_.GetRetransmissionAlarm()->deadline());

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.GetRetransmissionAlarm()->Fire();
}

// Includes regression test for b/69979024.
TEST_P(QuicConnectionTest, PathDegradingDetectionForNonCryptoPackets) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());
  EXPECT_FALSE(connection_.IsPathDegrading());
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  connection_.OnHandshakeComplete();

  const char data[] = "data";
  size_t data_size = strlen(data);
  QuicStreamOffset offset = 0;

  for (int i = 0; i < 2; ++i) {
    // Send a packet. Now there's a retransmittable packet on the wire, so the
    // path degrading detection should be set.
    connection_.SendStreamDataWithString(
        GetNthClientInitiatedStreamId(1, connection_.transport_version()), data,
        offset, NO_FIN);
    offset += data_size;
    EXPECT_TRUE(connection_.PathDegradingDetectionInProgress());
    // Check the deadline of the path degrading detection.
    QuicTime::Delta delay =
        QuicConnectionPeer::GetSentPacketManager(&connection_)
            ->GetPathDegradingDelay();
    EXPECT_EQ(delay, connection_.GetBlackholeDetectorAlarm()->deadline() -
                         clock_.ApproximateNow());

    // Send a second packet. The path degrading detection's deadline should
    // remain the same.
    // Regression test for b/69979024.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
    QuicTime prev_deadline =
        connection_.GetBlackholeDetectorAlarm()->deadline();
    connection_.SendStreamDataWithString(
        GetNthClientInitiatedStreamId(1, connection_.transport_version()), data,
        offset, NO_FIN);
    offset += data_size;
    EXPECT_TRUE(connection_.PathDegradingDetectionInProgress());
    EXPECT_EQ(prev_deadline,
              connection_.GetBlackholeDetectorAlarm()->deadline());

    // Now receive an ACK of the first packet. This should advance the path
    // degrading detection's deadline since forward progress has been made.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
    if (i == 0) {
      EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
    }
    EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
    QuicAckFrame frame = InitAckFrame(
        {{QuicPacketNumber(1u + 2u * i), QuicPacketNumber(2u + 2u * i)}});
    ProcessAckPacket(&frame);
    EXPECT_TRUE(connection_.PathDegradingDetectionInProgress());
    // Check the deadline of the path degrading detection.
    delay = QuicConnectionPeer::GetSentPacketManager(&connection_)
                ->GetPathDegradingDelay();
    EXPECT_EQ(delay, connection_.GetBlackholeDetectorAlarm()->deadline() -
                         clock_.ApproximateNow());

    if (i == 0) {
      // Now receive an ACK of the second packet. Since there are no more
      // retransmittable packets on the wire, this should cancel the path
      // degrading detection.
      clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
      EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
      frame = InitAckFrame({{QuicPacketNumber(2), QuicPacketNumber(3)}});
      ProcessAckPacket(&frame);
      EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());
    } else {
      // Advance time to the path degrading alarm's deadline and simulate
      // firing the alarm.
      clock_.AdvanceTime(delay);
      EXPECT_CALL(visitor_, OnPathDegrading());
      connection_.PathDegradingTimeout();
      EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());
    }
  }
  EXPECT_TRUE(connection_.IsPathDegrading());
}

TEST_P(QuicConnectionTest, RetransmittableOnWireSetsPingAlarm) {
  const QuicTime::Delta retransmittable_on_wire_timeout =
      QuicTime::Delta::FromMilliseconds(50);
  connection_.set_initial_retransmittable_on_wire_timeout(
      retransmittable_on_wire_timeout);

  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));

  EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());
  EXPECT_FALSE(connection_.IsPathDegrading());
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  connection_.OnHandshakeComplete();

  const char data[] = "data";
  size_t data_size = strlen(data);
  QuicStreamOffset offset = 0;

  // Send a packet.
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  // Now there's a retransmittable packet on the wire, so the path degrading
  // alarm should be set.
  // The retransmittable-on-wire alarm should not be set.
  EXPECT_TRUE(connection_.PathDegradingDetectionInProgress());
  QuicTime::Delta delay = QuicConnectionPeer::GetSentPacketManager(&connection_)
                              ->GetPathDegradingDelay();
  EXPECT_EQ(delay, connection_.GetBlackholeDetectorAlarm()->deadline() -
                       clock_.ApproximateNow());
  ASSERT_TRUE(connection_.sent_packet_manager().HasInFlightPackets());
  // The ping alarm is set for the ping timeout, not the shorter
  // retransmittable_on_wire_timeout.
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  QuicTime::Delta ping_delay = QuicTime::Delta::FromSeconds(kPingTimeoutSecs);
  EXPECT_EQ(ping_delay,
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  // Now receive an ACK of the packet.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame =
      InitAckFrame({{QuicPacketNumber(1), QuicPacketNumber(2)}});
  ProcessAckPacket(&frame);
  // No more retransmittable packets on the wire, so the path degrading alarm
  // should be cancelled, and the ping alarm should be set to the
  // retransmittable_on_wire_timeout.
  EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(retransmittable_on_wire_timeout,
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  // Simulate firing the ping alarm and sending a PING.
  clock_.AdvanceTime(retransmittable_on_wire_timeout);
  connection_.GetPingAlarm()->Fire();

  // Now there's a retransmittable packet (PING) on the wire, so the path
  // degrading alarm should be set.
  ASSERT_TRUE(connection_.PathDegradingDetectionInProgress());
  delay = QuicConnectionPeer::GetSentPacketManager(&connection_)
              ->GetPathDegradingDelay();
  EXPECT_EQ(delay, connection_.GetBlackholeDetectorAlarm()->deadline() -
                       clock_.ApproximateNow());
}

TEST_P(QuicConnectionTest, ServerRetransmittableOnWire) {
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  SetQuicReloadableFlag(quic_enable_server_on_wire_ping, true);

  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(kSRWP);
  config.SetInitialReceivedConnectionOptions(connection_options);
  connection_.SetFromConfig(config);

  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));

  ProcessPacket(1);

  ASSERT_TRUE(connection_.GetPingAlarm()->IsSet());
  QuicTime::Delta ping_delay = QuicTime::Delta::FromMilliseconds(200);
  EXPECT_EQ(ping_delay,
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  connection_.SendStreamDataWithString(2, "foo", 0, NO_FIN);
  // Verify PING alarm gets cancelled.
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());

  // Now receive an ACK of the packet.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(100));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame =
      InitAckFrame({{QuicPacketNumber(1), QuicPacketNumber(2)}});
  ProcessAckPacket(2, &frame);
  // Verify PING alarm gets scheduled.
  ASSERT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(ping_delay,
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());
}

TEST_P(QuicConnectionTest, RetransmittableOnWireSendFirstPacket) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  const QuicTime::Delta kRetransmittableOnWireTimeout =
      QuicTime::Delta::FromMilliseconds(200);
  const QuicTime::Delta kTestRtt = QuicTime::Delta::FromMilliseconds(100);

  connection_.set_initial_retransmittable_on_wire_timeout(
      kRetransmittableOnWireTimeout);

  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(kROWF);
  config.SetClientConnectionOptions(connection_options);
  connection_.SetFromConfig(config);

  // Send a request.
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  // Receive an ACK after 1-RTT.
  clock_.AdvanceTime(kTestRtt);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame =
      InitAckFrame({{QuicPacketNumber(1), QuicPacketNumber(2)}});
  ProcessAckPacket(&frame);
  ASSERT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(kRetransmittableOnWireTimeout,
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());
  EXPECT_EQ(1u, writer_->packets_write_attempts());

  // Fire retransmittable-on-wire alarm.
  clock_.AdvanceTime(kRetransmittableOnWireTimeout);
  connection_.GetPingAlarm()->Fire();
  EXPECT_EQ(2u, writer_->packets_write_attempts());
  // Verify alarm is set in keep-alive mode.
  ASSERT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kPingTimeoutSecs),
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());
}

TEST_P(QuicConnectionTest, RetransmittableOnWireSendRandomBytes) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  const QuicTime::Delta kRetransmittableOnWireTimeout =
      QuicTime::Delta::FromMilliseconds(200);
  const QuicTime::Delta kTestRtt = QuicTime::Delta::FromMilliseconds(100);

  connection_.set_initial_retransmittable_on_wire_timeout(
      kRetransmittableOnWireTimeout);

  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(kROWR);
  config.SetClientConnectionOptions(connection_options);
  connection_.SetFromConfig(config);

  // Send a request.
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  // Receive an ACK after 1-RTT.
  clock_.AdvanceTime(kTestRtt);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame =
      InitAckFrame({{QuicPacketNumber(1), QuicPacketNumber(2)}});
  ProcessAckPacket(&frame);
  ASSERT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(kRetransmittableOnWireTimeout,
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());
  EXPECT_EQ(1u, writer_->packets_write_attempts());

  // Fire retransmittable-on-wire alarm.
  clock_.AdvanceTime(kRetransmittableOnWireTimeout);
  // Next packet is not processable by the framer in the test writer.
  ExpectNextPacketUnprocessable();
  connection_.GetPingAlarm()->Fire();
  EXPECT_EQ(2u, writer_->packets_write_attempts());
  // Verify alarm is set in keep-alive mode.
  ASSERT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kPingTimeoutSecs),
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());
}

TEST_P(QuicConnectionTest,
       RetransmittableOnWireSendRandomBytesWithWriterBlocked) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);

  const QuicTime::Delta kRetransmittableOnWireTimeout =
      QuicTime::Delta::FromMilliseconds(200);
  const QuicTime::Delta kTestRtt = QuicTime::Delta::FromMilliseconds(100);

  connection_.set_initial_retransmittable_on_wire_timeout(
      kRetransmittableOnWireTimeout);

  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(kROWR);
  config.SetClientConnectionOptions(connection_options);
  connection_.SetFromConfig(config);

  // Send a request.
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  // Receive an ACK after 1-RTT.
  clock_.AdvanceTime(kTestRtt);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame =
      InitAckFrame({{QuicPacketNumber(1), QuicPacketNumber(2)}});
  ProcessAckPacket(&frame);
  ASSERT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(kRetransmittableOnWireTimeout,
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());
  EXPECT_EQ(1u, writer_->packets_write_attempts());
  // Receive an out of order data packet and block the ACK packet.
  BlockOnNextWrite();
  ProcessDataPacket(3);
  EXPECT_EQ(2u, writer_->packets_write_attempts());
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Fire retransmittable-on-wire alarm.
  clock_.AdvanceTime(kRetransmittableOnWireTimeout);
  connection_.GetPingAlarm()->Fire();
  // Verify the random bytes packet gets queued.
  EXPECT_EQ(2u, connection_.NumQueuedPackets());
}

// This test verifies that the connection marks path as degrading and does not
// spin timer to detect path degrading when a new packet is sent on the
// degraded path.
TEST_P(QuicConnectionTest, NoPathDegradingDetectionIfPathIsDegrading) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());
  EXPECT_FALSE(connection_.IsPathDegrading());
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  connection_.OnHandshakeComplete();

  const char data[] = "data";
  size_t data_size = strlen(data);
  QuicStreamOffset offset = 0;

  // Send the first packet. Now there's a retransmittable packet on the wire, so
  // the path degrading alarm should be set.
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_TRUE(connection_.PathDegradingDetectionInProgress());
  // Check the deadline of the path degrading detection.
  QuicTime::Delta delay = QuicConnectionPeer::GetSentPacketManager(&connection_)
                              ->GetPathDegradingDelay();
  EXPECT_EQ(delay, connection_.GetBlackholeDetectorAlarm()->deadline() -
                       clock_.ApproximateNow());

  // Send a second packet. The path degrading detection's deadline should remain
  // the same.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  QuicTime prev_deadline = connection_.GetBlackholeDetectorAlarm()->deadline();
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_TRUE(connection_.PathDegradingDetectionInProgress());
  EXPECT_EQ(prev_deadline, connection_.GetBlackholeDetectorAlarm()->deadline());

  // Now receive an ACK of the first packet. This should advance the path
  // degrading detection's deadline since forward progress has been made.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame =
      InitAckFrame({{QuicPacketNumber(1u), QuicPacketNumber(2u)}});
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.PathDegradingDetectionInProgress());
  // Check the deadline of the path degrading alarm.
  delay = QuicConnectionPeer::GetSentPacketManager(&connection_)
              ->GetPathDegradingDelay();
  EXPECT_EQ(delay, connection_.GetBlackholeDetectorAlarm()->deadline() -
                       clock_.ApproximateNow());

  // Advance time to the path degrading detection's deadline and simulate
  // firing the path degrading detection. This path will be considered as
  // degrading.
  clock_.AdvanceTime(delay);
  EXPECT_CALL(visitor_, OnPathDegrading()).Times(1);
  connection_.PathDegradingTimeout();
  EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());
  EXPECT_TRUE(connection_.IsPathDegrading());

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());
  // Send a third packet. The path degrading detection is no longer set but path
  // should still be marked as degrading.
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());
  EXPECT_TRUE(connection_.IsPathDegrading());
}

TEST_P(QuicConnectionTest, NoPathDegradingDetectionBeforeHandshakeConfirmed) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());
  EXPECT_FALSE(connection_.IsPathDegrading());
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_COMPLETE));

  connection_.SendStreamDataWithString(1, "data", 0, NO_FIN);
  if (GetQuicReloadableFlag(
          quic_no_path_degrading_before_handshake_confirmed) &&
      connection_.SupportsMultiplePacketNumberSpaces()) {
    EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());
  } else {
    EXPECT_TRUE(connection_.PathDegradingDetectionInProgress());
  }
}

// This test verifies that the connection unmarks path as degrarding and spins
// the timer to detect future path degrading when forward progress is made
// after path has been marked degrading.
TEST_P(QuicConnectionTest, UnmarkPathDegradingOnForwardProgress) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());
  EXPECT_FALSE(connection_.IsPathDegrading());
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  connection_.OnHandshakeComplete();

  const char data[] = "data";
  size_t data_size = strlen(data);
  QuicStreamOffset offset = 0;

  // Send the first packet. Now there's a retransmittable packet on the wire, so
  // the path degrading alarm should be set.
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_TRUE(connection_.PathDegradingDetectionInProgress());
  // Check the deadline of the path degrading alarm.
  QuicTime::Delta delay = QuicConnectionPeer::GetSentPacketManager(&connection_)
                              ->GetPathDegradingDelay();
  EXPECT_EQ(delay, connection_.GetBlackholeDetectorAlarm()->deadline() -
                       clock_.ApproximateNow());

  // Send a second packet. The path degrading alarm's deadline should remain
  // the same.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  QuicTime prev_deadline = connection_.GetBlackholeDetectorAlarm()->deadline();
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_TRUE(connection_.PathDegradingDetectionInProgress());
  EXPECT_EQ(prev_deadline, connection_.GetBlackholeDetectorAlarm()->deadline());

  // Now receive an ACK of the first packet. This should advance the path
  // degrading alarm's deadline since forward progress has been made.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame =
      InitAckFrame({{QuicPacketNumber(1u), QuicPacketNumber(2u)}});
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.PathDegradingDetectionInProgress());
  // Check the deadline of the path degrading alarm.
  delay = QuicConnectionPeer::GetSentPacketManager(&connection_)
              ->GetPathDegradingDelay();
  EXPECT_EQ(delay, connection_.GetBlackholeDetectorAlarm()->deadline() -
                       clock_.ApproximateNow());

  // Advance time to the path degrading alarm's deadline and simulate
  // firing the alarm.
  clock_.AdvanceTime(delay);
  EXPECT_CALL(visitor_, OnPathDegrading()).Times(1);
  connection_.PathDegradingTimeout();
  EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());
  EXPECT_TRUE(connection_.IsPathDegrading());

  // Send a third packet. The path degrading alarm is no longer set but path
  // should still be marked as degrading.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());
  EXPECT_TRUE(connection_.IsPathDegrading());

  // Now receive an ACK of the second packet. This should unmark the path as
  // degrading. And will set a timer to detect new path degrading.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  EXPECT_CALL(visitor_, OnForwardProgressMadeAfterPathDegrading()).Times(1);
  frame = InitAckFrame({{QuicPacketNumber(2), QuicPacketNumber(3)}});
  ProcessAckPacket(&frame);
  EXPECT_EQ(1,
            connection_.GetStats().num_forward_progress_after_path_degrading);
  EXPECT_FALSE(connection_.IsPathDegrading());
  EXPECT_TRUE(connection_.PathDegradingDetectionInProgress());
}

TEST_P(QuicConnectionTest, NoPathDegradingOnServer) {
  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);

  EXPECT_FALSE(connection_.IsPathDegrading());
  EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());

  // Send data.
  const char data[] = "data";
  connection_.SendStreamDataWithString(1, data, 0, NO_FIN);
  EXPECT_FALSE(connection_.IsPathDegrading());
  EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());

  // Ack data.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame =
      InitAckFrame({{QuicPacketNumber(1u), QuicPacketNumber(2u)}});
  ProcessAckPacket(&frame);
  EXPECT_FALSE(connection_.IsPathDegrading());
  EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());
}

TEST_P(QuicConnectionTest, NoPathDegradingAfterSendingAck) {
  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacket(1);
  SendAckPacketToPeer();
  EXPECT_FALSE(connection_.sent_packet_manager().unacked_packets().empty());
  EXPECT_FALSE(connection_.sent_packet_manager().HasInFlightPackets());
  EXPECT_FALSE(connection_.IsPathDegrading());
  EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());
}

TEST_P(QuicConnectionTest, MultipleCallsToCloseConnection) {
  // Verifies that multiple calls to CloseConnection do not
  // result in multiple attempts to close the connection - it will be marked as
  // disconnected after the first call.
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _)).Times(1);
  connection_.CloseConnection(QUIC_NO_ERROR, "no reason",
                              ConnectionCloseBehavior::SILENT_CLOSE);
  connection_.CloseConnection(QUIC_NO_ERROR, "no reason",
                              ConnectionCloseBehavior::SILENT_CLOSE);
}

TEST_P(QuicConnectionTest, ServerReceivesChloOnNonCryptoStream) {
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  QuicConnectionPeer::SetAddressValidated(&connection_);

  CryptoHandshakeMessage message;
  CryptoFramer framer;
  message.set_tag(kCHLO);
  std::unique_ptr<QuicData> data = framer.ConstructHandshakeMessage(message);
  frame1_.stream_id = 10;
  frame1_.data_buffer = data->data();
  frame1_.data_length = data->length();

  if (version().handshake_protocol == PROTOCOL_TLS1_3) {
    EXPECT_CALL(visitor_, BeforeConnectionCloseSent());
  }
  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));
  ForceProcessFramePacket(QuicFrame(frame1_));
  if (VersionHasIetfQuicFrames(version().transport_version)) {
    // INITIAL packet should not contain STREAM frame.
    TestConnectionCloseQuicErrorCode(IETF_QUIC_PROTOCOL_VIOLATION);
  } else {
    TestConnectionCloseQuicErrorCode(QUIC_MAYBE_CORRUPTED_MEMORY);
  }
}

TEST_P(QuicConnectionTest, ClientReceivesRejOnNonCryptoStream) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  CryptoHandshakeMessage message;
  CryptoFramer framer;
  message.set_tag(kREJ);
  std::unique_ptr<QuicData> data = framer.ConstructHandshakeMessage(message);
  frame1_.stream_id = 10;
  frame1_.data_buffer = data->data();
  frame1_.data_length = data->length();

  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));
  ForceProcessFramePacket(QuicFrame(frame1_));
  if (VersionHasIetfQuicFrames(version().transport_version)) {
    // INITIAL packet should not contain STREAM frame.
    TestConnectionCloseQuicErrorCode(IETF_QUIC_PROTOCOL_VIOLATION);
  } else {
    TestConnectionCloseQuicErrorCode(QUIC_MAYBE_CORRUPTED_MEMORY);
  }
}

TEST_P(QuicConnectionTest, CloseConnectionOnPacketTooLarge) {
  SimulateNextPacketTooLarge();
  // A connection close packet is sent
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .Times(1);
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  TestConnectionCloseQuicErrorCode(QUIC_PACKET_WRITE_ERROR);
}

TEST_P(QuicConnectionTest, AlwaysGetPacketTooLarge) {
  // Test even we always get packet too large, we do not infinitely try to send
  // close packet.
  AlwaysGetPacketTooLarge();
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .Times(1);
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  TestConnectionCloseQuicErrorCode(QUIC_PACKET_WRITE_ERROR);
}

TEST_P(QuicConnectionTest, CloseConnectionOnQueuedWriteError) {
  // Regression test for crbug.com/979507.
  //
  // If we get a write error when writing queued packets, we should attempt to
  // send a connection close packet, but if sending that fails, it shouldn't get
  // queued.

  // Queue a packet to write.
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Configure writer to always fail.
  AlwaysGetPacketTooLarge();

  // Expect that we attempt to close the connection exactly once.
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .Times(1);

  // Unblock the writes and actually send.
  writer_->SetWritable();
  connection_.OnCanWrite();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());

  TestConnectionCloseQuicErrorCode(QUIC_PACKET_WRITE_ERROR);
}

// Verify that if connection has no outstanding data, it notifies the send
// algorithm after the write.
TEST_P(QuicConnectionTest, SendDataAndBecomeApplicationLimited) {
  EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(1);
  {
    InSequence seq;
    EXPECT_CALL(visitor_, WillingAndAbleToWrite()).WillRepeatedly(Return(true));
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
    EXPECT_CALL(visitor_, WillingAndAbleToWrite())
        .WillRepeatedly(Return(false));
  }

  connection_.SendStreamData3();
}

// Verify that the connection does not become app-limited if there is
// outstanding data to send after the write.
TEST_P(QuicConnectionTest, NotBecomeApplicationLimitedIfMoreDataAvailable) {
  EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(0);
  {
    InSequence seq;
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
    EXPECT_CALL(visitor_, WillingAndAbleToWrite()).WillRepeatedly(Return(true));
  }

  connection_.SendStreamData3();
}

// Verify that the connection does not become app-limited after blocked write
// even if there is outstanding data to send after the write.
TEST_P(QuicConnectionTest, NotBecomeApplicationLimitedDueToWriteBlock) {
  EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(0);
  EXPECT_CALL(visitor_, WillingAndAbleToWrite()).WillRepeatedly(Return(true));
  BlockOnNextWrite();

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendStreamData3();

  // Now unblock the writer, become congestion control blocked,
  // and ensure we become app-limited after writing.
  writer_->SetWritable();
  CongestionBlockWrites();
  EXPECT_CALL(visitor_, WillingAndAbleToWrite()).WillRepeatedly(Return(false));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(1);
  connection_.OnCanWrite();
}

TEST_P(QuicConnectionTest, DoNotForceSendingAckOnPacketTooLarge) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  // Send an ack by simulating delayed ack alarm firing.
  ProcessPacket(1);
  EXPECT_TRUE(connection_.HasPendingAcks());
  connection_.GetAckAlarm()->Fire();
  // Simulate data packet causes write error.
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
  SimulateNextPacketTooLarge();
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  EXPECT_EQ(1u, writer_->connection_close_frames().size());
  // Ack frame is not bundled in connection close packet.
  EXPECT_TRUE(writer_->ack_frames().empty());
  if (writer_->padding_frames().empty()) {
    EXPECT_EQ(1u, writer_->frame_count());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
  }

  TestConnectionCloseQuicErrorCode(QUIC_PACKET_WRITE_ERROR);
}

TEST_P(QuicConnectionTest, CloseConnectionAllLevels) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }

  EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
  const QuicErrorCode kQuicErrorCode = QUIC_INTERNAL_ERROR;
  connection_.CloseConnection(
      kQuicErrorCode, "Some random error message",
      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);

  EXPECT_EQ(2u, QuicConnectionPeer::GetNumEncryptionLevels(&connection_));

  TestConnectionCloseQuicErrorCode(kQuicErrorCode);
  EXPECT_EQ(1u, writer_->connection_close_frames().size());

  if (!connection_.version().CanSendCoalescedPackets()) {
    // Each connection close packet should be sent in distinct UDP packets.
    EXPECT_EQ(QuicConnectionPeer::GetNumEncryptionLevels(&connection_),
              writer_->connection_close_packets());
    EXPECT_EQ(QuicConnectionPeer::GetNumEncryptionLevels(&connection_),
              writer_->packets_write_attempts());
    return;
  }

  // A single UDP packet should be sent with multiple connection close packets
  // coalesced together.
  EXPECT_EQ(1u, writer_->packets_write_attempts());

  // Only the first packet has been processed yet.
  EXPECT_EQ(1u, writer_->connection_close_packets());

  // ProcessPacket resets the visitor and frees the coalesced packet.
  ASSERT_TRUE(writer_->coalesced_packet() != nullptr);
  auto packet = writer_->coalesced_packet()->Clone();
  writer_->framer()->ProcessPacket(*packet);
  EXPECT_EQ(1u, writer_->connection_close_packets());
  ASSERT_TRUE(writer_->coalesced_packet() == nullptr);
}

TEST_P(QuicConnectionTest, CloseConnectionOneLevel) {
  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }

  EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
  const QuicErrorCode kQuicErrorCode = QUIC_INTERNAL_ERROR;
  connection_.CloseConnection(
      kQuicErrorCode, "Some random error message",
      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);

  EXPECT_EQ(2u, QuicConnectionPeer::GetNumEncryptionLevels(&connection_));

  TestConnectionCloseQuicErrorCode(kQuicErrorCode);
  EXPECT_EQ(1u, writer_->connection_close_frames().size());
  EXPECT_EQ(1u, writer_->connection_close_packets());
  EXPECT_EQ(1u, writer_->packets_write_attempts());
  ASSERT_TRUE(writer_->coalesced_packet() == nullptr);
}

TEST_P(QuicConnectionTest, DoNotPadServerInitialConnectionClose) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  // Receives packet 1000 in initial data.
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(1);
  ProcessCryptoPacketAtLevel(1000, ENCRYPTION_INITIAL);

  if (version().handshake_protocol == PROTOCOL_TLS1_3) {
    EXPECT_CALL(visitor_, BeforeConnectionCloseSent());
  }
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
  const QuicErrorCode kQuicErrorCode = QUIC_INTERNAL_ERROR;
  connection_.CloseConnection(
      kQuicErrorCode, "Some random error message",
      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);

  EXPECT_EQ(2u, QuicConnectionPeer::GetNumEncryptionLevels(&connection_));

  TestConnectionCloseQuicErrorCode(kQuicErrorCode);
  EXPECT_EQ(1u, writer_->connection_close_frames().size());
  EXPECT_TRUE(writer_->padding_frames().empty());
  EXPECT_EQ(ENCRYPTION_INITIAL, writer_->framer()->last_decrypted_level());
}

// Regression test for b/63620844.
TEST_P(QuicConnectionTest, FailedToWriteHandshakePacket) {
  SimulateNextPacketTooLarge();
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .Times(1);

  connection_.SendCryptoStreamData();
  TestConnectionCloseQuicErrorCode(QUIC_PACKET_WRITE_ERROR);
}

TEST_P(QuicConnectionTest, MaxPacingRate) {
  EXPECT_EQ(0, connection_.MaxPacingRate().ToBytesPerSecond());
  connection_.SetMaxPacingRate(QuicBandwidth::FromBytesPerSecond(100));
  EXPECT_EQ(100, connection_.MaxPacingRate().ToBytesPerSecond());
}

TEST_P(QuicConnectionTest, ClientAlwaysSendConnectionId) {
  EXPECT_EQ(Perspective::IS_CLIENT, connection_.perspective());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  EXPECT_EQ(CONNECTION_ID_PRESENT,
            writer_->last_packet_header().destination_connection_id_included);

  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  QuicConfigPeer::SetReceivedBytesForConnectionId(&config, 0);
  connection_.SetFromConfig(config);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendStreamDataWithString(3, "bar", 3, NO_FIN);
  // Verify connection id is still sent in the packet.
  EXPECT_EQ(CONNECTION_ID_PRESENT,
            writer_->last_packet_header().destination_connection_id_included);
}

TEST_P(QuicConnectionTest, PingAfterLastRetransmittablePacketAcked) {
  const QuicTime::Delta retransmittable_on_wire_timeout =
      QuicTime::Delta::FromMilliseconds(50);
  connection_.set_initial_retransmittable_on_wire_timeout(
      retransmittable_on_wire_timeout);

  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));

  const char data[] = "data";
  size_t data_size = strlen(data);
  QuicStreamOffset offset = 0;

  // Advance 5ms, send a retransmittable packet to the peer.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_TRUE(connection_.sent_packet_manager().HasInFlightPackets());
  // The ping alarm is set for the ping timeout, not the shorter
  // retransmittable_on_wire_timeout.
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  QuicTime::Delta ping_delay = QuicTime::Delta::FromSeconds(kPingTimeoutSecs);
  EXPECT_EQ(ping_delay,
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  // Advance 5ms, send a second retransmittable packet to the peer.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());

  // Now receive an ACK of the first packet. This should not set the
  // retransmittable-on-wire alarm since packet 2 is still on the wire.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame =
      InitAckFrame({{QuicPacketNumber(1), QuicPacketNumber(2)}});
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.sent_packet_manager().HasInFlightPackets());
  // The ping alarm is set for the ping timeout, not the shorter
  // retransmittable_on_wire_timeout.
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  // The ping alarm has a 1 second granularity, and the clock has been advanced
  // 10ms since it was originally set.
  EXPECT_EQ(ping_delay - QuicTime::Delta::FromMilliseconds(10),
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  // Now receive an ACK of the second packet. This should set the
  // retransmittable-on-wire alarm now that no retransmittable packets are on
  // the wire.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  frame = InitAckFrame({{QuicPacketNumber(2), QuicPacketNumber(3)}});
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(retransmittable_on_wire_timeout,
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  // Now receive a duplicate ACK of the second packet. This should not update
  // the ping alarm.
  QuicTime prev_deadline = connection_.GetPingAlarm()->deadline();
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  frame = InitAckFrame({{QuicPacketNumber(2), QuicPacketNumber(3)}});
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(prev_deadline, connection_.GetPingAlarm()->deadline());

  // Now receive a non-ACK packet.  This should not update the ping alarm.
  prev_deadline = connection_.GetPingAlarm()->deadline();
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  ProcessPacket(4);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(prev_deadline, connection_.GetPingAlarm()->deadline());

  // Simulate the alarm firing and check that a PING is sent.
  connection_.GetPingAlarm()->Fire();
  size_t padding_frame_count = writer_->padding_frames().size();
  EXPECT_EQ(padding_frame_count + 2u, writer_->frame_count());
  ASSERT_EQ(1u, writer_->ping_frames().size());
}

TEST_P(QuicConnectionTest, NoPingIfRetransmittablePacketSent) {
  const QuicTime::Delta retransmittable_on_wire_timeout =
      QuicTime::Delta::FromMilliseconds(50);
  connection_.set_initial_retransmittable_on_wire_timeout(
      retransmittable_on_wire_timeout);

  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));

  const char data[] = "data";
  size_t data_size = strlen(data);
  QuicStreamOffset offset = 0;

  // Advance 5ms, send a retransmittable packet to the peer.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_TRUE(connection_.sent_packet_manager().HasInFlightPackets());
  // The ping alarm is set for the ping timeout, not the shorter
  // retransmittable_on_wire_timeout.
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  QuicTime::Delta ping_delay = QuicTime::Delta::FromSeconds(kPingTimeoutSecs);
  EXPECT_EQ(ping_delay,
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  // Now receive an ACK of the first packet. This should set the
  // retransmittable-on-wire alarm now that no retransmittable packets are on
  // the wire.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame =
      InitAckFrame({{QuicPacketNumber(1), QuicPacketNumber(2)}});
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(retransmittable_on_wire_timeout,
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  // Before the alarm fires, send another retransmittable packet. This should
  // cancel the retransmittable-on-wire alarm since now there's a
  // retransmittable packet on the wire.
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());

  // Now receive an ACK of the second packet. This should set the
  // retransmittable-on-wire alarm now that no retransmittable packets are on
  // the wire.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  frame = InitAckFrame({{QuicPacketNumber(2), QuicPacketNumber(3)}});
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(retransmittable_on_wire_timeout,
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  // Simulate the alarm firing and check that a PING is sent.
  writer_->Reset();
  connection_.GetPingAlarm()->Fire();
  size_t padding_frame_count = writer_->padding_frames().size();
  // Do not ACK acks.
  EXPECT_EQ(padding_frame_count + 1u, writer_->frame_count());
  ASSERT_EQ(1u, writer_->ping_frames().size());
}

// When there is no stream data received but are open streams, send the
// first few consecutive pings with aggressive retransmittable-on-wire
// timeout. Exponentially back off the retransmittable-on-wire ping timeout
// afterwards until it exceeds the default ping timeout.
TEST_P(QuicConnectionTest, BackOffRetransmittableOnWireTimeout) {
  int max_aggressive_retransmittable_on_wire_ping_count = 5;
  SetQuicFlag(quic_max_aggressive_retransmittable_on_wire_ping_count,
              max_aggressive_retransmittable_on_wire_ping_count);
  const QuicTime::Delta initial_retransmittable_on_wire_timeout =
      QuicTime::Delta::FromMilliseconds(200);
  connection_.set_initial_retransmittable_on_wire_timeout(
      initial_retransmittable_on_wire_timeout);

  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));

  const char data[] = "data";
  // Advance 5ms, send a retransmittable data packet to the peer.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
  connection_.SendStreamDataWithString(1, data, 0, NO_FIN);
  EXPECT_TRUE(connection_.sent_packet_manager().HasInFlightPackets());
  // The ping alarm is set for the ping timeout, not the shorter
  // retransmittable_on_wire_timeout.
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kPingTimeoutSecs),
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_)).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _))
      .Times(AnyNumber());

  // Verify that the first few consecutive retransmittable on wire pings are
  // sent with aggressive timeout.
  for (int i = 0; i <= max_aggressive_retransmittable_on_wire_ping_count; i++) {
    // Receive an ACK of the previous packet. This should set the ping alarm
    // with the initial retransmittable-on-wire timeout.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
    QuicPacketNumber ack_num = creator_->packet_number();
    QuicAckFrame frame = InitAckFrame(
        {{QuicPacketNumber(ack_num), QuicPacketNumber(ack_num + 1)}});
    ProcessAckPacket(&frame);
    EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
    EXPECT_EQ(initial_retransmittable_on_wire_timeout,
              connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());
    // Simulate the alarm firing and check that a PING is sent.
    writer_->Reset();
    clock_.AdvanceTime(initial_retransmittable_on_wire_timeout);
    connection_.GetPingAlarm()->Fire();
  }

  QuicTime::Delta retransmittable_on_wire_timeout =
      initial_retransmittable_on_wire_timeout;

  // Verify subsequent pings are sent with timeout that is exponentially backed
  // off.
  while (retransmittable_on_wire_timeout * 2 <
         QuicTime::Delta::FromSeconds(kPingTimeoutSecs)) {
    // Receive an ACK for the previous PING. This should set the
    // ping alarm with backed off retransmittable-on-wire timeout.
    retransmittable_on_wire_timeout = retransmittable_on_wire_timeout * 2;
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
    QuicPacketNumber ack_num = creator_->packet_number();
    QuicAckFrame frame = InitAckFrame(
        {{QuicPacketNumber(ack_num), QuicPacketNumber(ack_num + 1)}});
    ProcessAckPacket(&frame);
    EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
    EXPECT_EQ(retransmittable_on_wire_timeout,
              connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

    // Simulate the alarm firing and check that a PING is sent.
    writer_->Reset();
    clock_.AdvanceTime(retransmittable_on_wire_timeout);
    connection_.GetPingAlarm()->Fire();
  }

  // The ping alarm is set with default ping timeout.
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kPingTimeoutSecs),
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  // Receive an ACK for the previous PING. The ping alarm is set with an
  // earlier deadline.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  QuicPacketNumber ack_num = creator_->packet_number();
  QuicAckFrame frame = InitAckFrame(
      {{QuicPacketNumber(ack_num), QuicPacketNumber(ack_num + 1)}});
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kPingTimeoutSecs) -
                QuicTime::Delta::FromMilliseconds(5),
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());
}

// This test verify that the count of consecutive aggressive pings is reset
// when new data is received. And it also verifies the connection resets
// the exponential back-off of the retransmittable-on-wire ping timeout
// after receiving new stream data.
TEST_P(QuicConnectionTest, ResetBackOffRetransmitableOnWireTimeout) {
  int max_aggressive_retransmittable_on_wire_ping_count = 3;
  SetQuicFlag(quic_max_aggressive_retransmittable_on_wire_ping_count, 3);
  const QuicTime::Delta initial_retransmittable_on_wire_timeout =
      QuicTime::Delta::FromMilliseconds(200);
  connection_.set_initial_retransmittable_on_wire_timeout(
      initial_retransmittable_on_wire_timeout);

  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_)).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _))
      .Times(AnyNumber());

  const char data[] = "data";
  // Advance 5ms, send a retransmittable data packet to the peer.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
  connection_.SendStreamDataWithString(1, data, 0, NO_FIN);
  EXPECT_TRUE(connection_.sent_packet_manager().HasInFlightPackets());
  // The ping alarm is set for the ping timeout, not the shorter
  // retransmittable_on_wire_timeout.
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kPingTimeoutSecs),
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  // Receive an ACK of the first packet. This should set the ping alarm with
  // initial retransmittable-on-wire timeout since there is no retransmittable
  // packet on the wire.
  {
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
    QuicAckFrame frame =
        InitAckFrame({{QuicPacketNumber(1), QuicPacketNumber(2)}});
    ProcessAckPacket(&frame);
    EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
    EXPECT_EQ(initial_retransmittable_on_wire_timeout,
              connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());
  }

  // Simulate the alarm firing and check that a PING is sent.
  writer_->Reset();
  clock_.AdvanceTime(initial_retransmittable_on_wire_timeout);
  connection_.GetPingAlarm()->Fire();

  // Receive an ACK for the previous PING. Ping alarm will be set with
  // aggressive timeout.
  {
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
    QuicPacketNumber ack_num = creator_->packet_number();
    QuicAckFrame frame = InitAckFrame(
        {{QuicPacketNumber(ack_num), QuicPacketNumber(ack_num + 1)}});
    ProcessAckPacket(&frame);
    EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
    EXPECT_EQ(initial_retransmittable_on_wire_timeout,
              connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());
  }

  // Process a data packet.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacket(peer_creator_.packet_number() + 1);
  QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_,
                                         peer_creator_.packet_number() + 1);
  EXPECT_EQ(initial_retransmittable_on_wire_timeout,
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());
  clock_.AdvanceTime(initial_retransmittable_on_wire_timeout);
  connection_.GetPingAlarm()->Fire();

  // Verify the count of consecutive aggressive pings is reset.
  for (int i = 0; i < max_aggressive_retransmittable_on_wire_ping_count; i++) {
    // Receive an ACK of the previous packet. This should set the ping alarm
    // with the initial retransmittable-on-wire timeout.
    const QuicPacketNumber ack_num = creator_->packet_number();
    QuicAckFrame frame = InitAckFrame(
        {{QuicPacketNumber(ack_num), QuicPacketNumber(ack_num + 1)}});
    ProcessAckPacket(&frame);
    EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
    EXPECT_EQ(initial_retransmittable_on_wire_timeout,
              connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());
    // Simulate the alarm firing and check that a PING is sent.
    writer_->Reset();
    clock_.AdvanceTime(initial_retransmittable_on_wire_timeout);
    connection_.GetPingAlarm()->Fire();
    // Advance 5ms to receive next packet.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  }

  // Receive another ACK for the previous PING. This should set the
  // ping alarm with backed off retransmittable-on-wire timeout.
  {
    const QuicPacketNumber ack_num = creator_->packet_number();
    QuicAckFrame frame = InitAckFrame(
        {{QuicPacketNumber(ack_num), QuicPacketNumber(ack_num + 1)}});
    ProcessAckPacket(&frame);
    EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
    EXPECT_EQ(initial_retransmittable_on_wire_timeout * 2,
              connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());
  }

  writer_->Reset();
  clock_.AdvanceTime(2 * initial_retransmittable_on_wire_timeout);
  connection_.GetPingAlarm()->Fire();

  // Process another data packet and a new ACK packet. The ping alarm is set
  // with aggressive ping timeout again.
  {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
    ProcessDataPacket(peer_creator_.packet_number() + 1);
    QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_,
                                           peer_creator_.packet_number() + 1);
    const QuicPacketNumber ack_num = creator_->packet_number();
    QuicAckFrame frame = InitAckFrame(
        {{QuicPacketNumber(ack_num), QuicPacketNumber(ack_num + 1)}});
    ProcessAckPacket(&frame);
    EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
    EXPECT_EQ(initial_retransmittable_on_wire_timeout,
              connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());
  }
}

// Make sure that we never send more retransmissible on the wire pings than
// the limit in FLAGS_quic_max_retransmittable_on_wire_ping_count.
TEST_P(QuicConnectionTest, RetransmittableOnWirePingLimit) {
  static constexpr int kMaxRetransmittableOnWirePingCount = 3;
  SetQuicFlag(quic_max_retransmittable_on_wire_ping_count,
              kMaxRetransmittableOnWirePingCount);
  static constexpr QuicTime::Delta initial_retransmittable_on_wire_timeout =
      QuicTime::Delta::FromMilliseconds(200);
  static constexpr QuicTime::Delta short_delay =
      QuicTime::Delta::FromMilliseconds(5);
  ASSERT_LT(short_delay * 10, initial_retransmittable_on_wire_timeout);
  connection_.set_initial_retransmittable_on_wire_timeout(
      initial_retransmittable_on_wire_timeout);

  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));

  const char data[] = "data";
  // Advance 5ms, send a retransmittable data packet to the peer.
  clock_.AdvanceTime(short_delay);
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
  connection_.SendStreamDataWithString(1, data, 0, NO_FIN);
  EXPECT_TRUE(connection_.sent_packet_manager().HasInFlightPackets());
  // The ping alarm is set for the ping timeout, not the shorter
  // retransmittable_on_wire_timeout.
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kPingTimeoutSecs),
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_)).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _))
      .Times(AnyNumber());

  // Verify that the first few consecutive retransmittable on wire pings are
  // sent with aggressive timeout.
  for (int i = 0; i <= kMaxRetransmittableOnWirePingCount; i++) {
    // Receive an ACK of the previous packet. This should set the ping alarm
    // with the initial retransmittable-on-wire timeout.
    clock_.AdvanceTime(short_delay);
    QuicPacketNumber ack_num = creator_->packet_number();
    QuicAckFrame frame = InitAckFrame(
        {{QuicPacketNumber(ack_num), QuicPacketNumber(ack_num + 1)}});
    ProcessAckPacket(&frame);
    EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
    EXPECT_EQ(initial_retransmittable_on_wire_timeout,
              connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());
    // Simulate the alarm firing and check that a PING is sent.
    writer_->Reset();
    clock_.AdvanceTime(initial_retransmittable_on_wire_timeout);
    connection_.GetPingAlarm()->Fire();
  }

  // Receive an ACK of the previous packet. This should set the ping alarm
  // but this time with the default ping timeout.
  QuicPacketNumber ack_num = creator_->packet_number();
  QuicAckFrame frame = InitAckFrame(
      {{QuicPacketNumber(ack_num), QuicPacketNumber(ack_num + 1)}});
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kPingTimeoutSecs),
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());
}

TEST_P(QuicConnectionTest, ValidStatelessResetToken) {
  const StatelessResetToken kTestToken{0, 1, 0, 1, 0, 1, 0, 1,
                                       0, 1, 0, 1, 0, 1, 0, 1};
  const StatelessResetToken kWrongTestToken{0, 1, 0, 1, 0, 1, 0, 1,
                                            0, 1, 0, 1, 0, 1, 0, 2};
  QuicConfig config;
  // No token has been received.
  EXPECT_FALSE(connection_.IsValidStatelessResetToken(kTestToken));

  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _)).Times(2);
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillRepeatedly(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillRepeatedly(Return(false));
  // Token is different from received token.
  QuicConfigPeer::SetReceivedStatelessResetToken(&config, kTestToken);
  connection_.SetFromConfig(config);
  EXPECT_FALSE(connection_.IsValidStatelessResetToken(kWrongTestToken));

  QuicConfigPeer::SetReceivedStatelessResetToken(&config, kTestToken);
  connection_.SetFromConfig(config);
  EXPECT_TRUE(connection_.IsValidStatelessResetToken(kTestToken));
}

TEST_P(QuicConnectionTest, WriteBlockedWithInvalidAck) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _)).Times(0);
  BlockOnNextWrite();
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendStreamDataWithString(5, "foo", 0, FIN);
  // This causes connection to be closed because packet 1 has not been sent yet.
  QuicAckFrame frame = InitAckFrame(1);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _));
  ProcessAckPacket(1, &frame);
  EXPECT_EQ(0, connection_close_frame_count_);
}

TEST_P(QuicConnectionTest, SendMessage) {
  if (connection_.version().UsesTls()) {
    QuicConfig config;
    QuicConfigPeer::SetReceivedMaxDatagramFrameSize(
        &config, kMaxAcceptedDatagramFrameSize);
    EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
    EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
    EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
    connection_.SetFromConfig(config);
  }
  std::string message(connection_.GetCurrentLargestMessagePayload() * 2, 'a');
  quiche::QuicheMemSlice slice;
  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    connection_.SendStreamData3();
    // Send a message which cannot fit into current open packet, and 2 packets
    // get sent, one contains stream frame, and the other only contains the
    // message frame.
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
    slice = MemSliceFromString(absl::string_view(
        message.data(), connection_.GetCurrentLargestMessagePayload()));
    EXPECT_EQ(MESSAGE_STATUS_SUCCESS,
              connection_.SendMessage(1, absl::MakeSpan(&slice, 1), false));
  }
  // Fail to send a message if connection is congestion control blocked.
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillOnce(Return(false));
  slice = MemSliceFromString("message");
  EXPECT_EQ(MESSAGE_STATUS_BLOCKED,
            connection_.SendMessage(2, absl::MakeSpan(&slice, 1), false));

  // Always fail to send a message which cannot fit into one packet.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  slice = MemSliceFromString(absl::string_view(
      message.data(), connection_.GetCurrentLargestMessagePayload() + 1));
  EXPECT_EQ(MESSAGE_STATUS_TOO_LARGE,
            connection_.SendMessage(3, absl::MakeSpan(&slice, 1), false));
}

TEST_P(QuicConnectionTest, GetCurrentLargestMessagePayload) {
  QuicPacketLength expected_largest_payload = 1215;
  if (connection_.version().SendsVariableLengthPacketNumberInLongHeader()) {
    expected_largest_payload += 3;
  }
  if (connection_.version().HasLongHeaderLengths()) {
    expected_largest_payload -= 2;
  }
  if (connection_.version().HasLengthPrefixedConnectionIds()) {
    expected_largest_payload -= 1;
  }
  if (connection_.version().UsesTls()) {
    // QUIC+TLS disallows DATAGRAM/MESSAGE frames before the handshake.
    EXPECT_EQ(connection_.GetCurrentLargestMessagePayload(), 0);
    QuicConfig config;
    QuicConfigPeer::SetReceivedMaxDatagramFrameSize(
        &config, kMaxAcceptedDatagramFrameSize);
    EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
    EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
    EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
    connection_.SetFromConfig(config);
    // Verify the value post-handshake.
    EXPECT_EQ(connection_.GetCurrentLargestMessagePayload(),
              expected_largest_payload);
  } else {
    EXPECT_EQ(connection_.GetCurrentLargestMessagePayload(),
              expected_largest_payload);
  }
}

TEST_P(QuicConnectionTest, GetGuaranteedLargestMessagePayload) {
  QuicPacketLength expected_largest_payload = 1215;
  if (connection_.version().HasLongHeaderLengths()) {
    expected_largest_payload -= 2;
  }
  if (connection_.version().HasLengthPrefixedConnectionIds()) {
    expected_largest_payload -= 1;
  }
  if (connection_.version().UsesTls()) {
    // QUIC+TLS disallows DATAGRAM/MESSAGE frames before the handshake.
    EXPECT_EQ(connection_.GetGuaranteedLargestMessagePayload(), 0);
    QuicConfig config;
    QuicConfigPeer::SetReceivedMaxDatagramFrameSize(
        &config, kMaxAcceptedDatagramFrameSize);
    EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
    EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
    EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
    connection_.SetFromConfig(config);
    // Verify the value post-handshake.
    EXPECT_EQ(connection_.GetGuaranteedLargestMessagePayload(),
              expected_largest_payload);
  } else {
    EXPECT_EQ(connection_.GetGuaranteedLargestMessagePayload(),
              expected_largest_payload);
  }
}

TEST_P(QuicConnectionTest, LimitedLargestMessagePayload) {
  if (!connection_.version().UsesTls()) {
    return;
  }
  constexpr QuicPacketLength kFrameSizeLimit = 1000;
  constexpr QuicPacketLength kPayloadSizeLimit =
      kFrameSizeLimit - kQuicFrameTypeSize;
  // QUIC+TLS disallows DATAGRAM/MESSAGE frames before the handshake.
  EXPECT_EQ(connection_.GetCurrentLargestMessagePayload(), 0);
  EXPECT_EQ(connection_.GetGuaranteedLargestMessagePayload(), 0);
  QuicConfig config;
  QuicConfigPeer::SetReceivedMaxDatagramFrameSize(&config, kFrameSizeLimit);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  // Verify the value post-handshake.
  EXPECT_EQ(connection_.GetCurrentLargestMessagePayload(), kPayloadSizeLimit);
  EXPECT_EQ(connection_.GetGuaranteedLargestMessagePayload(),
            kPayloadSizeLimit);
}

// Test to check that the path challenge/path response logic works
// correctly. This test is only for version-99
TEST_P(QuicConnectionTest, ServerResponseToPathChallenge) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_SERVER);
  QuicConnectionPeer::SetAddressValidated(&connection_);
  // First check if the server can send probing packet.
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);

  // Create and send the probe request (PATH_CHALLENGE frame).
  // SendConnectivityProbingPacket ends up calling
  // TestPacketWriter::WritePacket() which in turns receives and parses the
  // packet by calling framer_.ProcessPacket() -- which in turn calls
  // SimpleQuicFramer::OnPathChallengeFrame(). SimpleQuicFramer saves
  // the packet in writer_->path_challenge_frames()
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendConnectivityProbingPacket(writer_.get(),
                                            connection_.peer_address());
  // Save the random contents of the challenge for later comparison to the
  // response.
  ASSERT_GE(writer_->path_challenge_frames().size(), 1u);
  QuicPathFrameBuffer challenge_data =
      writer_->path_challenge_frames().front().data_buffer;

  // Normally, QuicConnection::OnPathChallengeFrame and OnPaddingFrame would be
  // called and it will perform actions to ensure that the rest of the protocol
  // is performed.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  EXPECT_TRUE(connection_.OnPathChallengeFrame(
      writer_->path_challenge_frames().front()));
  EXPECT_TRUE(connection_.OnPaddingFrame(writer_->padding_frames().front()));
  creator_->FlushCurrentPacket();

  // The final check is to ensure that the random data in the response matches
  // the random data from the challenge.
  EXPECT_EQ(1u, writer_->path_response_frames().size());
  EXPECT_EQ(0, memcmp(&challenge_data,
                      &(writer_->path_response_frames().front().data_buffer),
                      sizeof(challenge_data)));
}

TEST_P(QuicConnectionTest, ClientResponseToPathChallengeOnDefaulSocket) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_CLIENT);
  // First check if the client can send probing packet.
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);

  // Create and send the probe request (PATH_CHALLENGE frame).
  // SendConnectivityProbingPacket ends up calling
  // TestPacketWriter::WritePacket() which in turns receives and parses the
  // packet by calling framer_.ProcessPacket() -- which in turn calls
  // SimpleQuicFramer::OnPathChallengeFrame(). SimpleQuicFramer saves
  // the packet in writer_->path_challenge_frames()
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendConnectivityProbingPacket(writer_.get(),
                                            connection_.peer_address());
  // Save the random contents of the challenge for later validation against the
  // response.
  ASSERT_GE(writer_->path_challenge_frames().size(), 1u);
  QuicPathFrameBuffer challenge_data =
      writer_->path_challenge_frames().front().data_buffer;

  // Normally, QuicConnection::OnPathChallengeFrame would be
  // called and it will perform actions to ensure that the rest of the protocol
  // is performed.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  EXPECT_TRUE(connection_.OnPathChallengeFrame(
      writer_->path_challenge_frames().front()));
  EXPECT_TRUE(connection_.OnPaddingFrame(writer_->padding_frames().front()));
  creator_->FlushCurrentPacket();

  // The final check is to ensure that the random data in the response matches
  // the random data from the challenge.
  EXPECT_EQ(1u, writer_->path_response_frames().size());
  EXPECT_EQ(0, memcmp(&challenge_data,
                      &(writer_->path_response_frames().front().data_buffer),
                      sizeof(challenge_data)));
}

TEST_P(QuicConnectionTest, ClientResponseToPathChallengeOnAlternativeSocket) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_CLIENT);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);

  QuicSocketAddress kNewSelfAddress(QuicIpAddress::Loopback6(), /*port=*/23456);
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(AtLeast(1u))
      .WillOnce(Invoke([&]() {
        EXPECT_EQ(1u, new_writer.packets_write_attempts());
        EXPECT_EQ(1u, new_writer.path_challenge_frames().size());
        EXPECT_EQ(1u, new_writer.padding_frames().size());
        EXPECT_EQ(kNewSelfAddress.host(),
                  new_writer.last_write_source_address());
      }));
  bool success = false;
  connection_.ValidatePath(
      std::make_unique<TestQuicPathValidationContext>(
          kNewSelfAddress, connection_.peer_address(), &new_writer),
      std::make_unique<TestValidationResultDelegate>(
          &connection_, kNewSelfAddress, connection_.peer_address(), &success),
      PathValidationReason::kReasonUnknown);

  // Receiving a PATH_CHALLENGE on the alternative path. Response to this
  // PATH_CHALLENGE should be sent via the alternative writer.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(AtLeast(1u))
      .WillOnce(Invoke([&]() {
        EXPECT_EQ(2u, new_writer.packets_write_attempts());
        EXPECT_EQ(1u, new_writer.path_response_frames().size());
        EXPECT_EQ(1u, new_writer.padding_frames().size());
        EXPECT_EQ(kNewSelfAddress.host(),
                  new_writer.last_write_source_address());
      }))
      .WillRepeatedly(DoDefault());
  ;
  std::unique_ptr<SerializedPacket> probing_packet = ConstructProbingPacket();
  std::unique_ptr<QuicReceivedPacket> received(ConstructReceivedPacket(
      QuicEncryptedPacket(probing_packet->encrypted_buffer,
                          probing_packet->encrypted_length),
      clock_.Now()));
  ProcessReceivedPacket(kNewSelfAddress, kPeerAddress, *received);

  QuicSocketAddress kNewerSelfAddress(QuicIpAddress::Loopback6(),
                                      /*port=*/34567);
  // Receiving a PATH_CHALLENGE on an unknown socket should be ignored.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0u);
  ProcessReceivedPacket(kNewerSelfAddress, kPeerAddress, *received);
}

TEST_P(QuicConnectionTest,
       RestartPathDegradingDetectionAfterMigrationWithProbe) {
  if (!version().HasIetfQuicFrames() &&
      GetQuicReloadableFlag(quic_ignore_gquic_probing)) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  PathProbeTestInit(Perspective::IS_CLIENT);

  // Send data and verify the path degrading detection is set.
  const char data[] = "data";
  size_t data_size = strlen(data);
  QuicStreamOffset offset = 0;
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;

  // Verify the path degrading detection is in progress.
  EXPECT_TRUE(connection_.PathDegradingDetectionInProgress());
  EXPECT_FALSE(connection_.IsPathDegrading());
  QuicTime ddl = connection_.GetBlackholeDetectorAlarm()->deadline();

  // Simulate the firing of path degrading.
  clock_.AdvanceTime(ddl - clock_.ApproximateNow());
  EXPECT_CALL(visitor_, OnPathDegrading()).Times(1);
  connection_.PathDegradingTimeout();
  EXPECT_TRUE(connection_.IsPathDegrading());
  EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());

  if (!GetParam().version.HasIetfQuicFrames()) {
    // Simulate path degrading handling by sending a probe on an alternet path.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
    TestPacketWriter probing_writer(version(), &clock_, Perspective::IS_CLIENT);
    connection_.SendConnectivityProbingPacket(&probing_writer,
                                              connection_.peer_address());
    // Verify that path degrading detection is not reset.
    EXPECT_FALSE(connection_.PathDegradingDetectionInProgress());

    // Simulate successful path degrading handling by receiving probe response.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(20));

    EXPECT_CALL(visitor_,
                OnPacketReceived(_, _, /*is_connectivity_probe=*/true))
        .Times(1);
    const QuicSocketAddress kNewSelfAddress =
        QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);

    std::unique_ptr<SerializedPacket> probing_packet = ConstructProbingPacket();
    std::unique_ptr<QuicReceivedPacket> received(ConstructReceivedPacket(
        QuicEncryptedPacket(probing_packet->encrypted_buffer,
                            probing_packet->encrypted_length),
        clock_.Now()));
    uint64_t num_probing_received =
        connection_.GetStats().num_connectivity_probing_received;
    ProcessReceivedPacket(kNewSelfAddress, kPeerAddress, *received);

    EXPECT_EQ(num_probing_received +
                  (GetQuicReloadableFlag(quic_ignore_gquic_probing) ? 0u : 1u),
              connection_.GetStats().num_connectivity_probing_received);
    EXPECT_EQ(kPeerAddress, connection_.peer_address());
    EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
    EXPECT_TRUE(connection_.IsPathDegrading());
  }

  // Verify new path degrading detection is activated.
  EXPECT_CALL(visitor_, OnForwardProgressMadeAfterPathDegrading()).Times(1);
  connection_.OnSuccessfulMigration(/*is_port_change*/ true);
  EXPECT_FALSE(connection_.IsPathDegrading());
  EXPECT_TRUE(connection_.PathDegradingDetectionInProgress());
}

TEST_P(QuicConnectionTest, ClientsResetCwndAfterConnectionMigration) {
  if (!GetParam().version.HasIetfQuicFrames()) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  PathProbeTestInit(Perspective::IS_CLIENT);
  EXPECT_EQ(kSelfAddress, connection_.self_address());

  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  QuicTime::Delta default_init_rtt = rtt_stats->initial_rtt();
  rtt_stats->set_initial_rtt(default_init_rtt * 2);
  EXPECT_EQ(2 * default_init_rtt, rtt_stats->initial_rtt());

  QuicSentPacketManagerPeer::SetConsecutivePtoCount(manager_, 1);
  EXPECT_EQ(1u, manager_->GetConsecutivePtoCount());
  const SendAlgorithmInterface* send_algorithm = manager_->GetSendAlgorithm();

  // Migrate to a new address with different IP.
  const QuicSocketAddress kNewSelfAddress =
      QuicSocketAddress(QuicIpAddress::Loopback4(), /*port=*/23456);
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  connection_.MigratePath(kNewSelfAddress, connection_.peer_address(),
                          &new_writer, false);
  EXPECT_EQ(default_init_rtt, manager_->GetRttStats()->initial_rtt());
  EXPECT_EQ(0u, manager_->GetConsecutivePtoCount());
  EXPECT_NE(send_algorithm, manager_->GetSendAlgorithm());
}

// Regression test for b/110259444
TEST_P(QuicConnectionTest, DoNotScheduleSpuriousAckAlarm) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(AtLeast(1));
  writer_->SetWriteBlocked();

  ProcessPacket(1);
  // Verify ack alarm is set.
  EXPECT_TRUE(connection_.HasPendingAcks());
  // Fire the ack alarm, verify no packet is sent because the writer is blocked.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.GetAckAlarm()->Fire();

  writer_->SetWritable();
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  ProcessPacket(2);
  // Verify ack alarm is not set.
  EXPECT_FALSE(connection_.HasPendingAcks());
}

TEST_P(QuicConnectionTest, DisablePacingOffloadConnectionOptions) {
  EXPECT_FALSE(QuicConnectionPeer::SupportsReleaseTime(&connection_));
  writer_->set_supports_release_time(true);
  QuicConfig config;
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  EXPECT_TRUE(QuicConnectionPeer::SupportsReleaseTime(&connection_));

  QuicTagVector connection_options;
  connection_options.push_back(kNPCO);
  config.SetConnectionOptionsToSend(connection_options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  // Verify pacing offload is disabled.
  EXPECT_FALSE(QuicConnectionPeer::SupportsReleaseTime(&connection_));
}

// Regression test for b/110259444
// Get a path response without having issued a path challenge...
TEST_P(QuicConnectionTest, OrphanPathResponse) {
  QuicPathFrameBuffer data = {{0, 1, 2, 3, 4, 5, 6, 7}};

  QuicPathResponseFrame frame(99, data);
  EXPECT_TRUE(connection_.OnPathResponseFrame(frame));
  // If PATH_RESPONSE was accepted (payload matches the payload saved
  // in QuicConnection::transmitted_connectivity_probe_payload_) then
  // current_packet_content_ would be set to FIRST_FRAME_IS_PING.
  // Since this PATH_RESPONSE does not match, current_packet_content_
  // must not be FIRST_FRAME_IS_PING.
  EXPECT_NE(QuicConnection::FIRST_FRAME_IS_PING,
            QuicConnectionPeer::GetCurrentPacketContent(&connection_));
}

TEST_P(QuicConnectionTest, AcceptPacketNumberZero) {
  if (!VersionHasIetfQuicFrames(version().transport_version)) {
    return;
  }
  // Set first_sending_packet_number to be 0 to allow successfully processing
  // acks which ack packet number 0.
  QuicFramerPeer::SetFirstSendingPacketNumber(writer_->framer()->framer(), 0);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessPacket(0);
  EXPECT_EQ(QuicPacketNumber(0), LargestAcked(connection_.ack_frame()));
  EXPECT_EQ(1u, connection_.ack_frame().packets.NumIntervals());

  ProcessPacket(1);
  EXPECT_EQ(QuicPacketNumber(1), LargestAcked(connection_.ack_frame()));
  EXPECT_EQ(1u, connection_.ack_frame().packets.NumIntervals());

  ProcessPacket(2);
  EXPECT_EQ(QuicPacketNumber(2), LargestAcked(connection_.ack_frame()));
  EXPECT_EQ(1u, connection_.ack_frame().packets.NumIntervals());
}

TEST_P(QuicConnectionTest, MultiplePacketNumberSpacesBasicSending) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  connection_.SendCryptoStreamData();
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  QuicAckFrame frame1 = InitAckFrame(1);
  // Received ACK for packet 1.
  ProcessFramePacketAtLevel(30, QuicFrame(&frame1), ENCRYPTION_INITIAL);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(4);
  connection_.SendApplicationDataAtLevel(ENCRYPTION_ZERO_RTT, 5, "data", 0,
                                         NO_FIN);
  connection_.SendApplicationDataAtLevel(ENCRYPTION_ZERO_RTT, 5, "data", 4,
                                         NO_FIN);
  connection_.SendApplicationDataAtLevel(ENCRYPTION_FORWARD_SECURE, 5, "data",
                                         8, NO_FIN);
  connection_.SendApplicationDataAtLevel(ENCRYPTION_FORWARD_SECURE, 5, "data",
                                         12, FIN);
  // Received ACK for packets 2, 4, 5.
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  QuicAckFrame frame2 =
      InitAckFrame({{QuicPacketNumber(2), QuicPacketNumber(3)},
                    {QuicPacketNumber(4), QuicPacketNumber(6)}});
  // Make sure although the same packet number is used, but they are in
  // different packet number spaces.
  ProcessFramePacketAtLevel(30, QuicFrame(&frame2), ENCRYPTION_FORWARD_SECURE);
}

TEST_P(QuicConnectionTest, PeerAcksPacketsInWrongPacketNumberSpace) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  connection_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<TaggingEncrypter>(0x01));

  connection_.SendCryptoStreamData();
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  QuicAckFrame frame1 = InitAckFrame(1);
  // Received ACK for packet 1.
  ProcessFramePacketAtLevel(30, QuicFrame(&frame1), ENCRYPTION_INITIAL);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
  connection_.SendApplicationDataAtLevel(ENCRYPTION_ZERO_RTT, 5, "data", 0,
                                         NO_FIN);
  connection_.SendApplicationDataAtLevel(ENCRYPTION_ZERO_RTT, 5, "data", 4,
                                         NO_FIN);

  // Received ACK for packets 2 and 3 in wrong packet number space.
  QuicAckFrame invalid_ack =
      InitAckFrame({{QuicPacketNumber(2), QuicPacketNumber(4)}});
  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AtLeast(1));
  ProcessFramePacketAtLevel(300, QuicFrame(&invalid_ack), ENCRYPTION_INITIAL);
  TestConnectionCloseQuicErrorCode(QUIC_INVALID_ACK_DATA);
}

TEST_P(QuicConnectionTest, MultiplePacketNumberSpacesBasicReceiving) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  }
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  // Receives packet 1000 in initial data.
  ProcessCryptoPacketAtLevel(1000, ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.HasPendingAcks());
  peer_framer_.SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
  SetDecrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_FORWARD_SECURE));
  // Receives packet 1000 in application data.
  ProcessDataPacketAtLevel(1000, false, ENCRYPTION_FORWARD_SECURE);
  EXPECT_TRUE(connection_.HasPendingAcks());
  connection_.SendApplicationDataAtLevel(ENCRYPTION_FORWARD_SECURE, 5, "data",
                                         0, NO_FIN);
  // Verify application data ACK gets bundled with outgoing data.
  EXPECT_EQ(2u, writer_->frame_count());
  // Make sure ACK alarm is still set because initial data is not ACKed.
  EXPECT_TRUE(connection_.HasPendingAcks());
  // Receive packet 1001 in application data.
  ProcessDataPacketAtLevel(1001, false, ENCRYPTION_FORWARD_SECURE);
  clock_.AdvanceTime(DefaultRetransmissionTime());
  // Simulates ACK alarm fires and verify two ACKs are flushed.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
  connection_.SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
  connection_.GetAckAlarm()->Fire();
  EXPECT_FALSE(connection_.HasPendingAcks());
  // Receives more packets in application data.
  ProcessDataPacketAtLevel(1002, false, ENCRYPTION_FORWARD_SECURE);
  EXPECT_TRUE(connection_.HasPendingAcks());

  // Verify zero rtt and forward secure packets get acked in the same packet.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  ProcessDataPacket(1003);
  EXPECT_FALSE(connection_.HasPendingAcks());
}

TEST_P(QuicConnectionTest, CancelAckAlarmOnWriteBlocked) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  }
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  // Receives packet 1000 in initial data.
  ProcessCryptoPacketAtLevel(1000, ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.HasPendingAcks());
  peer_framer_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
  SetDecrypter(ENCRYPTION_ZERO_RTT,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_ZERO_RTT));
  // Receives packet 1000 in application data.
  ProcessDataPacketAtLevel(1000, false, ENCRYPTION_ZERO_RTT);
  EXPECT_TRUE(connection_.HasPendingAcks());

  writer_->SetWriteBlocked();
  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(AnyNumber());
  // Simulates ACK alarm fires and verify no ACK is flushed because of write
  // blocked.
  clock_.AdvanceTime(DefaultDelayedAckTime());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<TaggingEncrypter>(0x02));
  connection_.GetAckAlarm()->Fire();
  // Verify ACK alarm is not set.
  EXPECT_FALSE(connection_.HasPendingAcks());

  writer_->SetWritable();
  // Verify 2 ACKs are sent when connection gets unblocked.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
  connection_.OnCanWrite();
  EXPECT_FALSE(connection_.HasPendingAcks());
}

// Make sure a packet received with the right client connection ID is processed.
TEST_P(QuicConnectionTest, ValidClientConnectionId) {
  if (!framer_.version().SupportsClientConnectionIds()) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  SetClientConnectionId(TestConnectionId(0x33));
  QuicPacketHeader header = ConstructPacketHeader(1, ENCRYPTION_FORWARD_SECURE);
  header.destination_connection_id = TestConnectionId(0x33);
  header.destination_connection_id_included = CONNECTION_ID_PRESENT;
  header.source_connection_id_included = CONNECTION_ID_ABSENT;
  QuicFrames frames;
  QuicPingFrame ping_frame;
  QuicPaddingFrame padding_frame;
  frames.push_back(QuicFrame(ping_frame));
  frames.push_back(QuicFrame(padding_frame));
  std::unique_ptr<QuicPacket> packet =
      BuildUnsizedDataPacket(&peer_framer_, header, frames);
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length = peer_framer_.EncryptPayload(
      ENCRYPTION_FORWARD_SECURE, QuicPacketNumber(1), *packet, buffer,
      kMaxOutgoingPacketSize);
  QuicReceivedPacket received_packet(buffer, encrypted_length, clock_.Now(),
                                     false);
  EXPECT_EQ(0u, connection_.GetStats().packets_dropped);
  ProcessReceivedPacket(kSelfAddress, kPeerAddress, received_packet);
  EXPECT_EQ(0u, connection_.GetStats().packets_dropped);
}

// Make sure a packet received with a different client connection ID is dropped.
TEST_P(QuicConnectionTest, InvalidClientConnectionId) {
  if (!framer_.version().SupportsClientConnectionIds()) {
    return;
  }
  SetClientConnectionId(TestConnectionId(0x33));
  QuicPacketHeader header = ConstructPacketHeader(1, ENCRYPTION_FORWARD_SECURE);
  header.destination_connection_id = TestConnectionId(0xbad);
  header.destination_connection_id_included = CONNECTION_ID_PRESENT;
  header.source_connection_id_included = CONNECTION_ID_ABSENT;
  QuicFrames frames;
  QuicPingFrame ping_frame;
  QuicPaddingFrame padding_frame;
  frames.push_back(QuicFrame(ping_frame));
  frames.push_back(QuicFrame(padding_frame));
  std::unique_ptr<QuicPacket> packet =
      BuildUnsizedDataPacket(&peer_framer_, header, frames);
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length = peer_framer_.EncryptPayload(
      ENCRYPTION_FORWARD_SECURE, QuicPacketNumber(1), *packet, buffer,
      kMaxOutgoingPacketSize);
  QuicReceivedPacket received_packet(buffer, encrypted_length, clock_.Now(),
                                     false);
  EXPECT_EQ(0u, connection_.GetStats().packets_dropped);
  ProcessReceivedPacket(kSelfAddress, kPeerAddress, received_packet);
  EXPECT_EQ(1u, connection_.GetStats().packets_dropped);
}

// Make sure the first packet received with a different client connection ID on
// the server is processed and it changes the client connection ID.
TEST_P(QuicConnectionTest, UpdateClientConnectionIdFromFirstPacket) {
  if (!framer_.version().SupportsClientConnectionIds()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  QuicPacketHeader header = ConstructPacketHeader(1, ENCRYPTION_INITIAL);
  header.source_connection_id = TestConnectionId(0x33);
  header.source_connection_id_included = CONNECTION_ID_PRESENT;
  QuicFrames frames;
  QuicPingFrame ping_frame;
  QuicPaddingFrame padding_frame;
  frames.push_back(QuicFrame(ping_frame));
  frames.push_back(QuicFrame(padding_frame));
  std::unique_ptr<QuicPacket> packet =
      BuildUnsizedDataPacket(&peer_framer_, header, frames);
  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length =
      peer_framer_.EncryptPayload(ENCRYPTION_INITIAL, QuicPacketNumber(1),
                                  *packet, buffer, kMaxOutgoingPacketSize);
  QuicReceivedPacket received_packet(buffer, encrypted_length, clock_.Now(),
                                     false);
  EXPECT_EQ(0u, connection_.GetStats().packets_dropped);
  ProcessReceivedPacket(kSelfAddress, kPeerAddress, received_packet);
  EXPECT_EQ(0u, connection_.GetStats().packets_dropped);
  EXPECT_EQ(TestConnectionId(0x33), connection_.client_connection_id());
}
void QuicConnectionTest::TestReplaceConnectionIdFromInitial() {
  if (!framer_.version().AllowsVariableLengthConnectionIds()) {
    return;
  }
  // We start with a known connection ID.
  EXPECT_TRUE(connection_.connected());
  EXPECT_EQ(0u, connection_.GetStats().packets_dropped);
  EXPECT_NE(TestConnectionId(0x33), connection_.connection_id());
  // Receiving an initial can replace the connection ID once.
  {
    QuicPacketHeader header = ConstructPacketHeader(1, ENCRYPTION_INITIAL);
    header.source_connection_id = TestConnectionId(0x33);
    header.source_connection_id_included = CONNECTION_ID_PRESENT;
    QuicFrames frames;
    QuicPingFrame ping_frame;
    QuicPaddingFrame padding_frame;
    frames.push_back(QuicFrame(ping_frame));
    frames.push_back(QuicFrame(padding_frame));
    std::unique_ptr<QuicPacket> packet =
        BuildUnsizedDataPacket(&peer_framer_, header, frames);
    char buffer[kMaxOutgoingPacketSize];
    size_t encrypted_length =
        peer_framer_.EncryptPayload(ENCRYPTION_INITIAL, QuicPacketNumber(1),
                                    *packet, buffer, kMaxOutgoingPacketSize);
    QuicReceivedPacket received_packet(buffer, encrypted_length, clock_.Now(),
                                       false);
    ProcessReceivedPacket(kSelfAddress, kPeerAddress, received_packet);
  }
  EXPECT_TRUE(connection_.connected());
  EXPECT_EQ(0u, connection_.GetStats().packets_dropped);
  EXPECT_EQ(TestConnectionId(0x33), connection_.connection_id());
  // Trying to replace the connection ID a second time drops the packet.
  {
    QuicPacketHeader header = ConstructPacketHeader(2, ENCRYPTION_INITIAL);
    header.source_connection_id = TestConnectionId(0x66);
    header.source_connection_id_included = CONNECTION_ID_PRESENT;
    QuicFrames frames;
    QuicPingFrame ping_frame;
    QuicPaddingFrame padding_frame;
    frames.push_back(QuicFrame(ping_frame));
    frames.push_back(QuicFrame(padding_frame));
    std::unique_ptr<QuicPacket> packet =
        BuildUnsizedDataPacket(&peer_framer_, header, frames);
    char buffer[kMaxOutgoingPacketSize];
    size_t encrypted_length =
        peer_framer_.EncryptPayload(ENCRYPTION_INITIAL, QuicPacketNumber(2),
                                    *packet, buffer, kMaxOutgoingPacketSize);
    QuicReceivedPacket received_packet(buffer, encrypted_length, clock_.Now(),
                                       false);
    ProcessReceivedPacket(kSelfAddress, kPeerAddress, received_packet);
  }
  EXPECT_TRUE(connection_.connected());
  EXPECT_EQ(1u, connection_.GetStats().packets_dropped);
  EXPECT_EQ(TestConnectionId(0x33), connection_.connection_id());
}

TEST_P(QuicConnectionTest, ReplaceServerConnectionIdFromInitial) {
  TestReplaceConnectionIdFromInitial();
}

TEST_P(QuicConnectionTest, ReplaceServerConnectionIdFromRetryAndInitial) {
  // First make the connection process a RETRY and replace the server connection
  // ID a first time.
  TestClientRetryHandling(/*invalid_retry_tag=*/false,
                          /*missing_original_id_in_config=*/false,
                          /*wrong_original_id_in_config=*/false,
                          /*missing_retry_id_in_config=*/false,
                          /*wrong_retry_id_in_config=*/false);
  // Reset the test framer to use the right connection ID.
  peer_framer_.SetInitialObfuscators(connection_.connection_id());
  // Now process an INITIAL and replace the server connection ID a second time.
  TestReplaceConnectionIdFromInitial();
}

// Regression test for b/134416344.
TEST_P(QuicConnectionTest, CheckConnectedBeforeFlush) {
  // This test mimics a scenario where a connection processes 2 packets and the
  // 2nd packet contains connection close frame. When the 2nd flusher goes out
  // of scope, a delayed ACK is pending, and ACK alarm should not be scheduled
  // because connection is disconnected.
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
  EXPECT_EQ(Perspective::IS_CLIENT, connection_.perspective());
  const QuicErrorCode kErrorCode = QUIC_INTERNAL_ERROR;
  std::unique_ptr<QuicConnectionCloseFrame> connection_close_frame(
      new QuicConnectionCloseFrame(connection_.transport_version(), kErrorCode,
                                   NO_IETF_QUIC_ERROR, "",
                                   /*transport_close_frame_type=*/0));

  // Received 2 packets.
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  } else {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  }
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress, kPeerAddress,
                                  ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.HasPendingAcks());
  ProcessFramePacketWithAddresses(QuicFrame(connection_close_frame.release()),
                                  kSelfAddress, kPeerAddress,
                                  ENCRYPTION_INITIAL);
  // Verify ack alarm is not set.
  EXPECT_FALSE(connection_.HasPendingAcks());
}

// Verify that a packet containing three coalesced packets is parsed correctly.
TEST_P(QuicConnectionTest, CoalescedPacket) {
  if (!QuicVersionHasLongHeaderLengths(connection_.transport_version())) {
    // Coalesced packets can only be encoded using long header lengths.
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_TRUE(connection_.connected());
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(3);
  } else {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(3);
  }

  uint64_t packet_numbers[3] = {1, 2, 3};
  EncryptionLevel encryption_levels[3] = {
      ENCRYPTION_INITIAL, ENCRYPTION_INITIAL, ENCRYPTION_FORWARD_SECURE};
  char buffer[kMaxOutgoingPacketSize] = {};
  size_t total_encrypted_length = 0;
  for (int i = 0; i < 3; i++) {
    QuicPacketHeader header =
        ConstructPacketHeader(packet_numbers[i], encryption_levels[i]);
    QuicFrames frames;
    if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
      frames.push_back(QuicFrame(&crypto_frame_));
    } else {
      frames.push_back(QuicFrame(frame1_));
    }
    std::unique_ptr<QuicPacket> packet = ConstructPacket(header, frames);
    peer_creator_.set_encryption_level(encryption_levels[i]);
    size_t encrypted_length = peer_framer_.EncryptPayload(
        encryption_levels[i], QuicPacketNumber(packet_numbers[i]), *packet,
        buffer + total_encrypted_length,
        sizeof(buffer) - total_encrypted_length);
    EXPECT_GT(encrypted_length, 0u);
    total_encrypted_length += encrypted_length;
  }
  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(buffer, total_encrypted_length, clock_.Now(), false));
  if (connection_.GetSendAlarm()->IsSet()) {
    connection_.GetSendAlarm()->Fire();
  }

  EXPECT_TRUE(connection_.connected());
}

// Regression test for crbug.com/992831.
TEST_P(QuicConnectionTest, CoalescedPacketThatSavesFrames) {
  if (!QuicVersionHasLongHeaderLengths(connection_.transport_version())) {
    // Coalesced packets can only be encoded using long header lengths.
    return;
  }
  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    // TODO(b/129151114) Enable this test with multiple packet number spaces.
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_TRUE(connection_.connected());
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_))
        .Times(3)
        .WillRepeatedly([this](const QuicCryptoFrame& /*frame*/) {
          // QuicFrame takes ownership of the QuicBlockedFrame.
          connection_.SendControlFrame(QuicFrame(QuicBlockedFrame(1, 3, 0)));
        });
  } else {
    EXPECT_CALL(visitor_, OnStreamFrame(_))
        .Times(3)
        .WillRepeatedly([this](const QuicStreamFrame& /*frame*/) {
          // QuicFrame takes ownership of the QuicBlockedFrame.
          connection_.SendControlFrame(QuicFrame(QuicBlockedFrame(1, 3, 0)));
        });
  }

  uint64_t packet_numbers[3] = {1, 2, 3};
  EncryptionLevel encryption_levels[3] = {
      ENCRYPTION_INITIAL, ENCRYPTION_INITIAL, ENCRYPTION_FORWARD_SECURE};
  char buffer[kMaxOutgoingPacketSize] = {};
  size_t total_encrypted_length = 0;
  for (int i = 0; i < 3; i++) {
    QuicPacketHeader header =
        ConstructPacketHeader(packet_numbers[i], encryption_levels[i]);
    QuicFrames frames;
    if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
      frames.push_back(QuicFrame(&crypto_frame_));
    } else {
      frames.push_back(QuicFrame(frame1_));
    }
    std::unique_ptr<QuicPacket> packet = ConstructPacket(header, frames);
    peer_creator_.set_encryption_level(encryption_levels[i]);
    size_t encrypted_length = peer_framer_.EncryptPayload(
        encryption_levels[i], QuicPacketNumber(packet_numbers[i]), *packet,
        buffer + total_encrypted_length,
        sizeof(buffer) - total_encrypted_length);
    EXPECT_GT(encrypted_length, 0u);
    total_encrypted_length += encrypted_length;
  }
  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(buffer, total_encrypted_length, clock_.Now(), false));
  if (connection_.GetSendAlarm()->IsSet()) {
    connection_.GetSendAlarm()->Fire();
  }

  EXPECT_TRUE(connection_.connected());

  SendAckPacketToPeer();
}

// Regresstion test for b/138962304.
TEST_P(QuicConnectionTest, RtoAndWriteBlocked) {
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  QuicStreamId stream_id = 2;
  QuicPacketNumber last_data_packet;
  SendStreamDataToPeer(stream_id, "foo", 0, NO_FIN, &last_data_packet);
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());

  // Writer gets blocked.
  writer_->SetWriteBlocked();

  // Cancel the stream.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(AtLeast(1));
  EXPECT_CALL(visitor_, WillingAndAbleToWrite())
      .WillRepeatedly(
          Invoke(&notifier_, &SimpleSessionNotifier::WillingToWrite));
  SendRstStream(stream_id, QUIC_ERROR_PROCESSING_STREAM, 3);

  // Retransmission timer fires in RTO mode.
  connection_.GetRetransmissionAlarm()->Fire();
  // Verify no packets get flushed when writer is blocked.
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

// Regresstion test for b/138962304.
TEST_P(QuicConnectionTest, PtoAndWriteBlocked) {
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  QuicStreamId stream_id = 2;
  QuicPacketNumber last_data_packet;
  SendStreamDataToPeer(stream_id, "foo", 0, NO_FIN, &last_data_packet);
  SendStreamDataToPeer(4, "foo", 0, NO_FIN, &last_data_packet);
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());

  // Writer gets blocked.
  writer_->SetWriteBlocked();

  // Cancel stream 2.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(AtLeast(1));
  SendRstStream(stream_id, QUIC_ERROR_PROCESSING_STREAM, 3);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  // Retransmission timer fires in TLP mode.
  connection_.GetRetransmissionAlarm()->Fire();
  // Verify one packets is forced flushed when writer is blocked.
  EXPECT_EQ(1u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, ProbeTimeout) {
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(k2PTO);
  config.SetConnectionOptionsToSend(connection_options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  QuicStreamId stream_id = 2;
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(stream_id, "foooooo", 0, NO_FIN, &last_packet);
  SendStreamDataToPeer(stream_id, "foooooo", 7, NO_FIN, &last_packet);
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());

  // Reset stream.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  SendRstStream(stream_id, QUIC_ERROR_PROCESSING_STREAM, 3);

  // Fire the PTO and verify only the RST_STREAM is resent, not stream data.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_EQ(0u, writer_->stream_frames().size());
  EXPECT_EQ(1u, writer_->rst_stream_frames().size());
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, CloseConnectionAfter6ClientPTOs) {
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(k1PTO);
  connection_options.push_back(k6PTO);
  config.SetConnectionOptionsToSend(connection_options);
  QuicConfigPeer::SetNegotiated(&config, true);
  if (connection_.version().UsesTls()) {
    QuicConfigPeer::SetReceivedOriginalConnectionId(
        &config, connection_.connection_id());
    QuicConfigPeer::SetReceivedInitialSourceConnectionId(
        &config, connection_.connection_id());
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  if (GetQuicReloadableFlag(quic_default_enable_5rto_blackhole_detection2) ||
      GetQuicReloadableFlag(
          quic_no_path_degrading_before_handshake_confirmed)) {
    EXPECT_CALL(visitor_, GetHandshakeState())
        .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  }
  connection_.OnHandshakeComplete();
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  // Send stream data.
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, FIN, nullptr);

  // Fire the retransmission alarm 5 times.
  for (int i = 0; i < 5; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
    connection_.GetRetransmissionAlarm()->Fire();
    EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
    EXPECT_TRUE(connection_.connected());
  }
  EXPECT_CALL(visitor_, OnPathDegrading());
  connection_.PathDegradingTimeout();

  EXPECT_EQ(5u, connection_.sent_packet_manager().GetConsecutivePtoCount());
  // Closes connection on 6th PTO.
  // May send multiple connecction close packets with multiple PN spaces.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));
  ASSERT_TRUE(connection_.BlackholeDetectionInProgress());
  connection_.GetBlackholeDetectorAlarm()->Fire();
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());
  TestConnectionCloseQuicErrorCode(QUIC_TOO_MANY_RTOS);
}

TEST_P(QuicConnectionTest, CloseConnectionAfter7ClientPTOs) {
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(k2PTO);
  connection_options.push_back(k7PTO);
  config.SetConnectionOptionsToSend(connection_options);
  QuicConfigPeer::SetNegotiated(&config, true);
  if (connection_.version().UsesTls()) {
    QuicConfigPeer::SetReceivedOriginalConnectionId(
        &config, connection_.connection_id());
    QuicConfigPeer::SetReceivedInitialSourceConnectionId(
        &config, connection_.connection_id());
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  if (GetQuicReloadableFlag(quic_default_enable_5rto_blackhole_detection2) ||
      GetQuicReloadableFlag(
          quic_no_path_degrading_before_handshake_confirmed)) {
    EXPECT_CALL(visitor_, GetHandshakeState())
        .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  }
  connection_.OnHandshakeComplete();
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  // Send stream data.
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, FIN, nullptr);

  // Fire the retransmission alarm 6 times.
  for (int i = 0; i < 6; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
    connection_.GetRetransmissionAlarm()->Fire();
    EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
    EXPECT_TRUE(connection_.connected());
  }
  EXPECT_CALL(visitor_, OnPathDegrading());
  connection_.PathDegradingTimeout();

  EXPECT_EQ(6u, connection_.sent_packet_manager().GetConsecutivePtoCount());
  // Closes connection on 7th PTO.
  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AtLeast(1));
  ASSERT_TRUE(connection_.BlackholeDetectionInProgress());
  connection_.GetBlackholeDetectorAlarm()->Fire();
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());
  TestConnectionCloseQuicErrorCode(QUIC_TOO_MANY_RTOS);
}

TEST_P(QuicConnectionTest, CloseConnectionAfter8ClientPTOs) {
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(k2PTO);
  connection_options.push_back(k8PTO);
  QuicConfigPeer::SetNegotiated(&config, true);
  if (connection_.version().UsesTls()) {
    QuicConfigPeer::SetReceivedOriginalConnectionId(
        &config, connection_.connection_id());
    QuicConfigPeer::SetReceivedInitialSourceConnectionId(
        &config, connection_.connection_id());
  }
  config.SetConnectionOptionsToSend(connection_options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  if (GetQuicReloadableFlag(quic_default_enable_5rto_blackhole_detection2) ||
      GetQuicReloadableFlag(
          quic_no_path_degrading_before_handshake_confirmed)) {
    EXPECT_CALL(visitor_, GetHandshakeState())
        .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  }
  connection_.OnHandshakeComplete();
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  // Send stream data.
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, FIN, nullptr);

  // Fire the retransmission alarm 7 times.
  for (int i = 0; i < 7; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
    connection_.GetRetransmissionAlarm()->Fire();
    EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
    EXPECT_TRUE(connection_.connected());
  }
  EXPECT_CALL(visitor_, OnPathDegrading());
  connection_.PathDegradingTimeout();

  EXPECT_EQ(7u, connection_.sent_packet_manager().GetConsecutivePtoCount());
  // Closes connection on 8th PTO.
  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AtLeast(1));
  ASSERT_TRUE(connection_.BlackholeDetectionInProgress());
  connection_.GetBlackholeDetectorAlarm()->Fire();
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());
  TestConnectionCloseQuicErrorCode(QUIC_TOO_MANY_RTOS);
}

TEST_P(QuicConnectionTest, DeprecateHandshakeMode) {
  if (!connection_.version().SupportsAntiAmplificationLimit()) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  // Send CHLO.
  connection_.SendCryptoStreamData();
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());

  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  QuicAckFrame frame1 = InitAckFrame(1);
  // Received ACK for packet 1.
  ProcessFramePacketAtLevel(1, QuicFrame(&frame1), ENCRYPTION_INITIAL);

  // Verify retransmission alarm is still set because handshake is not
  // confirmed although there is nothing in flight.
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  EXPECT_EQ(0u, connection_.GetStats().pto_count);
  EXPECT_EQ(0u, connection_.GetStats().crypto_retransmit_count);

  // PTO fires, verify a PING packet gets sent because there is no data to send.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, QuicPacketNumber(3), _, _));
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_EQ(1u, connection_.GetStats().pto_count);
  EXPECT_EQ(1u, connection_.GetStats().crypto_retransmit_count);
  EXPECT_EQ(1u, writer_->ping_frames().size());
}

TEST_P(QuicConnectionTest, AntiAmplificationLimit) {
  if (!connection_.version().SupportsAntiAmplificationLimit() ||
      GetQuicFlag(quic_enforce_strict_amplification_factor)) {
    return;
  }
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());

  set_perspective(Perspective::IS_SERVER);
  // Verify no data can be sent at the beginning because bytes received is 0.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.SendCryptoDataWithString("foo", 0);
  EXPECT_FALSE(connection_.CanWrite(HAS_RETRANSMITTABLE_DATA));
  EXPECT_FALSE(connection_.CanWrite(NO_RETRANSMITTABLE_DATA));
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  // Receives packet 1.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  ForceWillingAndAbleToWriteOnceForDeferSending();
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_INITIAL);

  const size_t anti_amplification_factor =
      GetQuicFlag(quic_anti_amplification_factor);
  // Verify now packets can be sent.
  for (size_t i = 1; i < anti_amplification_factor; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
    connection_.SendCryptoDataWithString("foo", i * 3);
    // Verify retransmission alarm is not set if throttled by anti-amplification
    // limit.
    EXPECT_EQ(i != anti_amplification_factor - 1,
              connection_.GetRetransmissionAlarm()->IsSet());
  }
  // Verify server is throttled by anti-amplification limit.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.SendCryptoDataWithString("foo", anti_amplification_factor * 3);

  // Receives packet 2.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  ForceWillingAndAbleToWriteOnceForDeferSending();
  ProcessCryptoPacketAtLevel(2, ENCRYPTION_INITIAL);
  // Verify more packets can be sent.
  for (size_t i = anti_amplification_factor + 1;
       i < anti_amplification_factor * 2; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
    connection_.SendCryptoDataWithString("foo", i * 3);
  }
  // Verify server is throttled by anti-amplification limit.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.SendCryptoDataWithString("foo",
                                       2 * anti_amplification_factor * 3);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  ForceWillingAndAbleToWriteOnceForDeferSending();
  ProcessPacket(3);
  // Verify anti-amplification limit is gone after address validation.
  for (size_t i = 0; i < 100; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
    connection_.SendStreamDataWithString(3, "first", i * 0, NO_FIN);
  }
}

TEST_P(QuicConnectionTest, 3AntiAmplificationLimit) {
  if (!connection_.version().SupportsAntiAmplificationLimit() ||
      GetQuicFlag(quic_enforce_strict_amplification_factor)) {
    return;
  }
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());

  set_perspective(Perspective::IS_SERVER);
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(k3AFF);
  config.SetInitialReceivedConnectionOptions(connection_options);
  if (connection_.version().UsesTls()) {
    QuicConfigPeer::SetReceivedOriginalConnectionId(
        &config, connection_.connection_id());
    QuicConfigPeer::SetReceivedInitialSourceConnectionId(&config,
                                                         QuicConnectionId());
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);

  // Verify no data can be sent at the beginning because bytes received is 0.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.SendCryptoDataWithString("foo", 0);
  EXPECT_FALSE(connection_.CanWrite(HAS_RETRANSMITTABLE_DATA));
  EXPECT_FALSE(connection_.CanWrite(NO_RETRANSMITTABLE_DATA));
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  // Receives packet 1.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  ForceWillingAndAbleToWriteOnceForDeferSending();
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_INITIAL);

  const size_t anti_amplification_factor = 3;
  // Verify now packets can be sent.
  for (size_t i = 1; i < anti_amplification_factor; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
    connection_.SendCryptoDataWithString("foo", i * 3);
    // Verify retransmission alarm is not set if throttled by anti-amplification
    // limit.
    EXPECT_EQ(i != anti_amplification_factor - 1,
              connection_.GetRetransmissionAlarm()->IsSet());
  }
  // Verify server is throttled by anti-amplification limit.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.SendCryptoDataWithString("foo", anti_amplification_factor * 3);

  // Receives packet 2.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  ForceWillingAndAbleToWriteOnceForDeferSending();
  ProcessCryptoPacketAtLevel(2, ENCRYPTION_INITIAL);
  // Verify more packets can be sent.
  for (size_t i = anti_amplification_factor + 1;
       i < anti_amplification_factor * 2; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
    connection_.SendCryptoDataWithString("foo", i * 3);
  }
  // Verify server is throttled by anti-amplification limit.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.SendCryptoDataWithString("foo",
                                       2 * anti_amplification_factor * 3);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  ForceWillingAndAbleToWriteOnceForDeferSending();
  ProcessPacket(3);
  // Verify anti-amplification limit is gone after address validation.
  for (size_t i = 0; i < 100; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
    connection_.SendStreamDataWithString(3, "first", i * 0, NO_FIN);
  }
}

TEST_P(QuicConnectionTest, 10AntiAmplificationLimit) {
  if (!connection_.version().SupportsAntiAmplificationLimit() ||
      GetQuicFlag(quic_enforce_strict_amplification_factor)) {
    return;
  }
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());

  set_perspective(Perspective::IS_SERVER);
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(k10AF);
  config.SetInitialReceivedConnectionOptions(connection_options);
  if (connection_.version().UsesTls()) {
    QuicConfigPeer::SetReceivedOriginalConnectionId(
        &config, connection_.connection_id());
    QuicConfigPeer::SetReceivedInitialSourceConnectionId(&config,
                                                         QuicConnectionId());
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);

  // Verify no data can be sent at the beginning because bytes received is 0.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.SendCryptoDataWithString("foo", 0);
  EXPECT_FALSE(connection_.CanWrite(HAS_RETRANSMITTABLE_DATA));
  EXPECT_FALSE(connection_.CanWrite(NO_RETRANSMITTABLE_DATA));
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  // Receives packet 1.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  ForceWillingAndAbleToWriteOnceForDeferSending();
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_INITIAL);

  const size_t anti_amplification_factor = 10;
  // Verify now packets can be sent.
  for (size_t i = 1; i < anti_amplification_factor; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
    connection_.SendCryptoDataWithString("foo", i * 3);
    // Verify retransmission alarm is not set if throttled by anti-amplification
    // limit.
    EXPECT_EQ(i != anti_amplification_factor - 1,
              connection_.GetRetransmissionAlarm()->IsSet());
  }
  // Verify server is throttled by anti-amplification limit.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.SendCryptoDataWithString("foo", anti_amplification_factor * 3);

  // Receives packet 2.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  ForceWillingAndAbleToWriteOnceForDeferSending();
  ProcessCryptoPacketAtLevel(2, ENCRYPTION_INITIAL);
  // Verify more packets can be sent.
  for (size_t i = anti_amplification_factor + 1;
       i < anti_amplification_factor * 2; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
    connection_.SendCryptoDataWithString("foo", i * 3);
  }
  // Verify server is throttled by anti-amplification limit.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.SendCryptoDataWithString("foo",
                                       2 * anti_amplification_factor * 3);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  ForceWillingAndAbleToWriteOnceForDeferSending();
  ProcessPacket(3);
  // Verify anti-amplification limit is gone after address validation.
  for (size_t i = 0; i < 100; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
    connection_.SendStreamDataWithString(3, "first", i * 0, NO_FIN);
  }
}

TEST_P(QuicConnectionTest, AckPendingWithAmplificationLimited) {
  if (!connection_.version().SupportsAntiAmplificationLimit()) {
    return;
  }
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(AnyNumber());
  set_perspective(Perspective::IS_SERVER);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  // Receives packet 1.
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_INITIAL);
  connection_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
  EXPECT_TRUE(connection_.HasPendingAcks());
  // Send response in different encryption level and cause amplification factor
  // throttled.
  size_t i = 0;
  while (connection_.CanWrite(HAS_RETRANSMITTABLE_DATA)) {
    connection_.SendCryptoDataWithString(std::string(1024, 'a'), i * 1024,
                                         ENCRYPTION_HANDSHAKE);
    ++i;
  }
  // Verify ACK is still pending.
  EXPECT_TRUE(connection_.HasPendingAcks());

  // Fire ACK alarm and verify ACK cannot be sent due to amplification factor.
  clock_.AdvanceTime(connection_.GetAckAlarm()->deadline() - clock_.Now());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.GetAckAlarm()->Fire();
  // Verify ACK alarm is cancelled.
  EXPECT_FALSE(connection_.HasPendingAcks());

  // Receives packet 2 and verify ACK gets flushed.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  ProcessCryptoPacketAtLevel(2, ENCRYPTION_INITIAL);
  EXPECT_FALSE(writer_->ack_frames().empty());
}

TEST_P(QuicConnectionTest, ConnectionCloseFrameType) {
  if (!VersionHasIetfQuicFrames(version().transport_version)) {
    // Test relevent only for IETF QUIC.
    return;
  }
  const QuicErrorCode kQuicErrorCode = IETF_QUIC_PROTOCOL_VIOLATION;
  // Use the (unknown) frame type of 9999 to avoid triggering any logic
  // which might be associated with the processing of a known frame type.
  const uint64_t kTransportCloseFrameType = 9999u;
  QuicFramerPeer::set_current_received_frame_type(
      QuicConnectionPeer::GetFramer(&connection_), kTransportCloseFrameType);
  // Do a transport connection close
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
  connection_.CloseConnection(
      kQuicErrorCode, "Some random error message",
      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
  const std::vector<QuicConnectionCloseFrame>& connection_close_frames =
      writer_->connection_close_frames();
  ASSERT_EQ(1u, connection_close_frames.size());
  EXPECT_EQ(IETF_QUIC_TRANSPORT_CONNECTION_CLOSE,
            connection_close_frames[0].close_type);
  EXPECT_EQ(kQuicErrorCode, connection_close_frames[0].quic_error_code);
  EXPECT_EQ(kTransportCloseFrameType,
            connection_close_frames[0].transport_close_frame_type);
}

TEST_P(QuicConnectionTest, PtoSkipsPacketNumber) {
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(k1PTO);
  connection_options.push_back(kPTOS);
  config.SetConnectionOptionsToSend(connection_options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  QuicStreamId stream_id = 2;
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(stream_id, "foooooo", 0, NO_FIN, &last_packet);
  SendStreamDataToPeer(stream_id, "foooooo", 7, NO_FIN, &last_packet);
  EXPECT_EQ(QuicPacketNumber(2), last_packet);
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());

  // Fire PTO and verify the PTO retransmission skips one packet number.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_EQ(QuicPacketNumber(4), writer_->last_packet_header().packet_number);
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, PtoChangesFlowLabel) {
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(k1PTO);
  connection_options.push_back(kPTOS);
  config.SetConnectionOptionsToSend(connection_options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  EXPECT_EQ(0, connection_.outgoing_flow_label());
  connection_.EnableBlackholeAvoidanceViaFlowLabel();
  static_cast<test::MockRandom*>(helper_->GetRandomGenerator())->ChangeValue();
  const uint32_t flow_label = connection_.outgoing_flow_label();
  EXPECT_NE(0, flow_label);

  QuicStreamId stream_id = 2;
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(stream_id, "foooooo", 0, NO_FIN, &last_packet);
  SendStreamDataToPeer(stream_id, "foooooo", 7, NO_FIN, &last_packet);
  EXPECT_EQ(QuicPacketNumber(2), last_packet);
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());

  // Fire PTO and verify the flow label has changed.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_NE(flow_label, connection_.outgoing_flow_label());
  EXPECT_EQ(1, connection_.GetStats().num_flow_label_changes);

  EXPECT_CALL(visitor_, OnForwardProgressMadeAfterFlowLabelChange());
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _));
  QuicAckFrame frame = InitAckFrame(last_packet);
  ProcessAckPacket(1, &frame);
  EXPECT_EQ(
      1, connection_.GetStats().num_forward_progress_after_flow_label_change);
}

TEST_P(QuicConnectionTest, NewReceiveNewFlowLabelWithGapChangesFlowLabel) {
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(k1PTO);
  connection_options.push_back(kPTOS);
  config.SetConnectionOptionsToSend(connection_options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  EXPECT_EQ(0, connection_.outgoing_flow_label());
  connection_.EnableBlackholeAvoidanceViaFlowLabel();
  static_cast<test::MockRandom*>(helper_->GetRandomGenerator())->ChangeValue();
  const uint32_t flow_label = connection_.outgoing_flow_label();
  EXPECT_NE(0, flow_label);
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());

  // Receive the first packet to initialize the flow label.
  ProcessDataPacketAtLevel(1, !kHasStopWaiting, ENCRYPTION_INITIAL, 0);
  EXPECT_EQ(flow_label, connection_.outgoing_flow_label());

  // Receive the second packet with the same flow label
  ProcessDataPacketAtLevel(2, !kHasStopWaiting, ENCRYPTION_INITIAL, flow_label);
  EXPECT_EQ(flow_label, connection_.outgoing_flow_label());

  // Receive a packet with gap and a new flow label and verify the outgoing
  // flow label has changed.
  static_cast<test::MockRandom*>(helper_->GetRandomGenerator())->ChangeValue();
  ProcessDataPacketAtLevel(4, !kHasStopWaiting, ENCRYPTION_INITIAL,
                           flow_label + 1);
  EXPECT_NE(flow_label, connection_.outgoing_flow_label());
}

TEST_P(QuicConnectionTest,
       NewReceiveNewFlowLabelWithNoGapDoesNotChangeFlowLabel) {
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(k1PTO);
  connection_options.push_back(kPTOS);
  config.SetConnectionOptionsToSend(connection_options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  EXPECT_EQ(0, connection_.outgoing_flow_label());
  connection_.EnableBlackholeAvoidanceViaFlowLabel();
  static_cast<test::MockRandom*>(helper_->GetRandomGenerator())->ChangeValue();
  const uint32_t flow_label = connection_.outgoing_flow_label();
  EXPECT_NE(0, flow_label);
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());

  // Receive the first packet to initialize the flow label.
  ProcessDataPacketAtLevel(1, !kHasStopWaiting, ENCRYPTION_INITIAL, 0);
  EXPECT_EQ(flow_label, connection_.outgoing_flow_label());

  // Receive the second packet with the same flow label
  ProcessDataPacketAtLevel(2, !kHasStopWaiting, ENCRYPTION_INITIAL, flow_label);
  EXPECT_EQ(flow_label, connection_.outgoing_flow_label());

  // Receive a packet with no gap and a new flow label and verify the outgoing
  // flow label has not changed.
  ProcessDataPacketAtLevel(3, !kHasStopWaiting, ENCRYPTION_INITIAL, flow_label);
  EXPECT_EQ(flow_label, connection_.outgoing_flow_label());
}

TEST_P(QuicConnectionTest, SendCoalescedPackets) {
  if (!connection_.version().CanSendCoalescedPackets()) {
    return;
  }
  MockQuicConnectionDebugVisitor debug_visitor;
  connection_.set_debug_visitor(&debug_visitor);
  EXPECT_CALL(debug_visitor, OnPacketSent(_, _, _, _, _, _, _, _, _)).Times(3);
  EXPECT_CALL(debug_visitor, OnCoalescedPacketSent(_, _)).Times(1);
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(1);
  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
    connection_.SendCryptoDataWithString("foo", 0);
    // Verify this packet is on hold.
    EXPECT_EQ(0u, writer_->packets_write_attempts());

    connection_.SetEncrypter(ENCRYPTION_HANDSHAKE,
                             std::make_unique<TaggingEncrypter>(0x02));
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
    connection_.SendCryptoDataWithString("bar", 3);
    EXPECT_EQ(0u, writer_->packets_write_attempts());

    connection_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                             std::make_unique<TaggingEncrypter>(0x03));
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    SendStreamDataToPeer(2, "baz", 3, NO_FIN, nullptr);
  }
  // Verify all 3 packets are coalesced in the same UDP datagram.
  EXPECT_EQ(1u, writer_->packets_write_attempts());
  EXPECT_EQ(0x03030303u, writer_->final_bytes_of_last_packet());
  // Verify the packet is padded to full.
  EXPECT_EQ(connection_.max_packet_length(), writer_->last_packet_size());

  // Verify packet process.
  EXPECT_LE(1u, writer_->crypto_frames().size());
  EXPECT_EQ(0u, writer_->stream_frames().size());
  // Verify there is coalesced packet.
  EXPECT_NE(nullptr, writer_->coalesced_packet());
}

TEST_P(QuicConnectionTest, FailToCoalescePacket) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (!IsDefaultTestConfiguration() ||
      !connection_.version().CanSendCoalescedPackets() ||
      GetQuicFlag(quic_enforce_strict_amplification_factor)) {
    return;
  }

  set_perspective(Perspective::IS_SERVER);

  auto test_body = [&] {
    EXPECT_CALL(visitor_,
                OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
        .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));

    ProcessDataPacketAtLevel(1, !kHasStopWaiting, ENCRYPTION_INITIAL);

    {
      QuicConnection::ScopedPacketFlusher flusher(&connection_);
      connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
      connection_.SendCryptoDataWithString("foo", 0);
      // Verify this packet is on hold.
      EXPECT_EQ(0u, writer_->packets_write_attempts());

      connection_.SetEncrypter(ENCRYPTION_HANDSHAKE,
                               std::make_unique<TaggingEncrypter>(0x02));
      connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
      connection_.SendCryptoDataWithString("bar", 3);
      EXPECT_EQ(0u, writer_->packets_write_attempts());

      connection_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                               std::make_unique<TaggingEncrypter>(0x03));
      connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
      SendStreamDataToPeer(2, "baz", 3, NO_FIN, nullptr);

      creator_->Flush();

      auto& coalesced_packet =
          QuicConnectionPeer::GetCoalescedPacket(&connection_);
      QuicPacketLength coalesced_packet_max_length =
          coalesced_packet.max_packet_length();
      QuicCoalescedPacketPeer::SetMaxPacketLength(coalesced_packet,
                                                  coalesced_packet.length());

      // Make the coalescer's FORWARD_SECURE packet longer.
      *QuicCoalescedPacketPeer::GetMutableEncryptedBuffer(
          coalesced_packet, ENCRYPTION_FORWARD_SECURE) += "!!! TEST !!!";

      QUIC_LOG(INFO) << "Reduced coalesced_packet_max_length from "
                     << coalesced_packet_max_length << " to "
                     << coalesced_packet.max_packet_length()
                     << ", coalesced_packet.length:"
                     << coalesced_packet.length()
                     << ", coalesced_packet.packet_lengths:"
                     << absl::StrJoin(coalesced_packet.packet_lengths(), ":");
    }

    EXPECT_FALSE(connection_.connected());
    EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
                IsError(QUIC_FAILED_TO_SERIALIZE_PACKET));
    EXPECT_EQ(saved_connection_close_frame_.error_details,
              "Failed to serialize coalesced packet.");
  };

  EXPECT_QUIC_BUG(test_body(), "SerializeCoalescedPacket failed.");
}

TEST_P(QuicConnectionTest, ClientReceivedHandshakeDone) {
  if (!connection_.version().UsesTls()) {
    return;
  }
  EXPECT_CALL(visitor_, OnHandshakeDoneReceived());
  QuicFrames frames;
  frames.push_back(QuicFrame(QuicHandshakeDoneFrame()));
  frames.push_back(QuicFrame(QuicPaddingFrame(-1)));
  ProcessFramesPacketAtLevel(1, frames, ENCRYPTION_FORWARD_SECURE);
}

TEST_P(QuicConnectionTest, ServerReceivedHandshakeDone) {
  if (!connection_.version().UsesTls()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  EXPECT_CALL(visitor_, OnHandshakeDoneReceived()).Times(0);
  if (version().handshake_protocol == PROTOCOL_TLS1_3) {
    EXPECT_CALL(visitor_, BeforeConnectionCloseSent());
  }
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));
  QuicFrames frames;
  frames.push_back(QuicFrame(QuicHandshakeDoneFrame()));
  frames.push_back(QuicFrame(QuicPaddingFrame(-1)));
  ProcessFramesPacketAtLevel(1, frames, ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(1, connection_close_frame_count_);
  EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
              IsError(IETF_QUIC_PROTOCOL_VIOLATION));
}

TEST_P(QuicConnectionTest, MultiplePacketNumberSpacePto) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  // Send handshake packet.
  connection_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(1);
  connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_HANDSHAKE);
  EXPECT_EQ(0x01010101u, writer_->final_bytes_of_last_packet());

  // Send application data.
  connection_.SendApplicationDataAtLevel(ENCRYPTION_FORWARD_SECURE, 5, "data",
                                         0, NO_FIN);
  EXPECT_EQ(0x03030303u, writer_->final_bytes_of_last_packet());
  QuicTime retransmission_time =
      connection_.GetRetransmissionAlarm()->deadline();
  EXPECT_NE(QuicTime::Zero(), retransmission_time);

  // Retransmit handshake data.
  clock_.AdvanceTime(retransmission_time - clock_.Now());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, QuicPacketNumber(4), _, _));
  connection_.GetRetransmissionAlarm()->Fire();
  // Verify 1-RTT packet gets coalesced with handshake retransmission.
  EXPECT_EQ(0x03030303u, writer_->final_bytes_of_last_packet());

  // Send application data.
  connection_.SendApplicationDataAtLevel(ENCRYPTION_FORWARD_SECURE, 5, "data",
                                         4, NO_FIN);
  EXPECT_EQ(0x03030303u, writer_->final_bytes_of_last_packet());
  retransmission_time = connection_.GetRetransmissionAlarm()->deadline();
  EXPECT_NE(QuicTime::Zero(), retransmission_time);

  // Retransmit handshake data again.
  clock_.AdvanceTime(retransmission_time - clock_.Now());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, QuicPacketNumber(9), _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, QuicPacketNumber(8), _, _));
  connection_.GetRetransmissionAlarm()->Fire();
  // Verify 1-RTT packet gets coalesced with handshake retransmission.
  EXPECT_EQ(0x03030303u, writer_->final_bytes_of_last_packet());

  // Discard handshake key.
  connection_.OnHandshakeComplete();
  retransmission_time = connection_.GetRetransmissionAlarm()->deadline();
  EXPECT_NE(QuicTime::Zero(), retransmission_time);

  // Retransmit application data.
  clock_.AdvanceTime(retransmission_time - clock_.Now());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, QuicPacketNumber(11), _, _));
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_EQ(0x03030303u, writer_->final_bytes_of_last_packet());
}

void QuicConnectionTest::TestClientRetryHandling(
    bool invalid_retry_tag, bool missing_original_id_in_config,
    bool wrong_original_id_in_config, bool missing_retry_id_in_config,
    bool wrong_retry_id_in_config) {
  if (invalid_retry_tag) {
    ASSERT_FALSE(missing_original_id_in_config);
    ASSERT_FALSE(wrong_original_id_in_config);
    ASSERT_FALSE(missing_retry_id_in_config);
    ASSERT_FALSE(wrong_retry_id_in_config);
  } else {
    ASSERT_FALSE(missing_original_id_in_config && wrong_original_id_in_config);
    ASSERT_FALSE(missing_retry_id_in_config && wrong_retry_id_in_config);
  }
  if (!version().UsesTls()) {
    return;
  }

  // These values come from draft-ietf-quic-v2 Appendix A.4.
  uint8_t retry_packet_rfcv2[] = {
      0xcf, 0x6b, 0x33, 0x43, 0xcf, 0x00, 0x08, 0xf0, 0x67, 0xa5, 0x50, 0x2a,
      0x42, 0x62, 0xb5, 0x74, 0x6f, 0x6b, 0x65, 0x6e, 0xc8, 0x64, 0x6c, 0xe8,
      0xbf, 0xe3, 0x39, 0x52, 0xd9, 0x55, 0x54, 0x36, 0x65, 0xdc, 0xc7, 0xb6};
  // These values come from RFC9001 Appendix A.4.
  uint8_t retry_packet_rfcv1[] = {
      0xff, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0xf0, 0x67, 0xa5, 0x50, 0x2a,
      0x42, 0x62, 0xb5, 0x74, 0x6f, 0x6b, 0x65, 0x6e, 0x04, 0xa2, 0x65, 0xba,
      0x2e, 0xff, 0x4d, 0x82, 0x90, 0x58, 0xfb, 0x3f, 0x0f, 0x24, 0x96, 0xba};
  uint8_t retry_packet29[] = {
      0xff, 0xff, 0x00, 0x00, 0x1d, 0x00, 0x08, 0xf0, 0x67, 0xa5, 0x50, 0x2a,
      0x42, 0x62, 0xb5, 0x74, 0x6f, 0x6b, 0x65, 0x6e, 0xd1, 0x69, 0x26, 0xd8,
      0x1f, 0x6f, 0x9c, 0xa2, 0x95, 0x3a, 0x8a, 0xa4, 0x57, 0x5e, 0x1e, 0x49};

  uint8_t* retry_packet;
  size_t retry_packet_length;
  if (version() == ParsedQuicVersion::RFCv2()) {
    retry_packet = retry_packet_rfcv2;
    retry_packet_length = ABSL_ARRAYSIZE(retry_packet_rfcv2);
  } else if (version() == ParsedQuicVersion::RFCv1()) {
    retry_packet = retry_packet_rfcv1;
    retry_packet_length = ABSL_ARRAYSIZE(retry_packet_rfcv1);
  } else if (version() == ParsedQuicVersion::Draft29()) {
    retry_packet = retry_packet29;
    retry_packet_length = ABSL_ARRAYSIZE(retry_packet29);
  } else {
    // TODO(dschinazi) generate retry packets for all versions once we have
    // server-side support for generating these programmatically.
    return;
  }

  uint8_t original_connection_id_bytes[] = {0x83, 0x94, 0xc8, 0xf0,
                                            0x3e, 0x51, 0x57, 0x08};
  uint8_t new_connection_id_bytes[] = {0xf0, 0x67, 0xa5, 0x50,
                                       0x2a, 0x42, 0x62, 0xb5};
  uint8_t retry_token_bytes[] = {0x74, 0x6f, 0x6b, 0x65, 0x6e};

  QuicConnectionId original_connection_id(
      reinterpret_cast<char*>(original_connection_id_bytes),
      ABSL_ARRAYSIZE(original_connection_id_bytes));
  QuicConnectionId new_connection_id(
      reinterpret_cast<char*>(new_connection_id_bytes),
      ABSL_ARRAYSIZE(new_connection_id_bytes));

  std::string retry_token(reinterpret_cast<char*>(retry_token_bytes),
                          ABSL_ARRAYSIZE(retry_token_bytes));

  if (invalid_retry_tag) {
    // Flip the last bit of the retry packet to prevent the integrity tag
    // from validating correctly.
    retry_packet[retry_packet_length - 1] ^= 1;
  }

  QuicConnectionId config_original_connection_id = original_connection_id;
  if (wrong_original_id_in_config) {
    // Flip the first bit of the connection ID.
    ASSERT_FALSE(config_original_connection_id.IsEmpty());
    config_original_connection_id.mutable_data()[0] ^= 0x80;
  }
  QuicConnectionId config_retry_source_connection_id = new_connection_id;
  if (wrong_retry_id_in_config) {
    // Flip the first bit of the connection ID.
    ASSERT_FALSE(config_retry_source_connection_id.IsEmpty());
    config_retry_source_connection_id.mutable_data()[0] ^= 0x80;
  }

  // Make sure the connection uses the connection ID from the test vectors,
  QuicConnectionPeer::SetServerConnectionId(&connection_,
                                            original_connection_id);
  // Make sure our fake framer has the new post-retry INITIAL keys so that any
  // retransmission triggered by retry can be decrypted.
  writer_->framer()->framer()->SetInitialObfuscators(new_connection_id);

  // Process the RETRY packet.
  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(reinterpret_cast<char*>(retry_packet),
                         retry_packet_length, clock_.Now()));

  if (invalid_retry_tag) {
    // Make sure we refuse to process a RETRY with invalid tag.
    EXPECT_FALSE(connection_.GetStats().retry_packet_processed);
    EXPECT_EQ(connection_.connection_id(), original_connection_id);
    EXPECT_TRUE(QuicPacketCreatorPeer::GetRetryToken(
                    QuicConnectionPeer::GetPacketCreator(&connection_))
                    .empty());
    return;
  }

  // Make sure we correctly parsed the RETRY.
  EXPECT_TRUE(connection_.GetStats().retry_packet_processed);
  EXPECT_EQ(connection_.connection_id(), new_connection_id);
  EXPECT_EQ(QuicPacketCreatorPeer::GetRetryToken(
                QuicConnectionPeer::GetPacketCreator(&connection_)),
            retry_token);

  // Test validating the original_connection_id from the config.
  QuicConfig received_config;
  QuicConfigPeer::SetNegotiated(&received_config, true);
  if (connection_.version().UsesTls()) {
    QuicConfigPeer::SetReceivedInitialSourceConnectionId(
        &received_config, connection_.connection_id());
    if (!missing_retry_id_in_config) {
      QuicConfigPeer::SetReceivedRetrySourceConnectionId(
          &received_config, config_retry_source_connection_id);
    }
  }
  if (!missing_original_id_in_config) {
    QuicConfigPeer::SetReceivedOriginalConnectionId(
        &received_config, config_original_connection_id);
  }

  if (missing_original_id_in_config || wrong_original_id_in_config ||
      missing_retry_id_in_config || wrong_retry_id_in_config) {
    EXPECT_CALL(visitor_,
                OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
        .Times(1);
  } else {
    EXPECT_CALL(visitor_,
                OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
        .Times(0);
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _)).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillRepeatedly(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillRepeatedly(Return(false));
  connection_.SetFromConfig(received_config);
  if (missing_original_id_in_config || wrong_original_id_in_config ||
      missing_retry_id_in_config || wrong_retry_id_in_config) {
    ASSERT_FALSE(connection_.connected());
    TestConnectionCloseQuicErrorCode(IETF_QUIC_PROTOCOL_VIOLATION);
  } else {
    EXPECT_TRUE(connection_.connected());
  }
}

TEST_P(QuicConnectionTest, FixTimeoutsClient) {
  if (!connection_.version().UsesTls()) {
    return;
  }
  set_perspective(Perspective::IS_CLIENT);
  if (GetQuicReloadableFlag(quic_fix_timeouts)) {
    EXPECT_CALL(visitor_, GetHandshakeState())
        .WillRepeatedly(Return(HANDSHAKE_START));
  }
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(kFTOE);
  config.SetConnectionOptionsToSend(connection_options);
  QuicConfigPeer::SetNegotiated(&config, true);
  QuicConfigPeer::SetReceivedOriginalConnectionId(&config,
                                                  connection_.connection_id());
  QuicConfigPeer::SetReceivedInitialSourceConnectionId(
      &config, connection_.connection_id());

  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  QuicIdleNetworkDetector& idle_network_detector =
      QuicConnectionPeer::GetIdleNetworkDetector(&connection_);
  if (GetQuicReloadableFlag(quic_fix_timeouts)) {
    // Handshake timeout has not been removed yet.
    EXPECT_NE(idle_network_detector.handshake_timeout(),
              QuicTime::Delta::Infinite());
  } else {
    // Handshake timeout has been set to infinite.
    EXPECT_EQ(idle_network_detector.handshake_timeout(),
              QuicTime::Delta::Infinite());
  }
}

TEST_P(QuicConnectionTest, FixTimeoutsServer) {
  if (!connection_.version().UsesTls()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  if (GetQuicReloadableFlag(quic_fix_timeouts)) {
    EXPECT_CALL(visitor_, GetHandshakeState())
        .WillRepeatedly(Return(HANDSHAKE_START));
  }
  QuicConfig config;
  quic::QuicTagVector initial_received_options;
  initial_received_options.push_back(quic::kFTOE);
  ASSERT_TRUE(
      config.SetInitialReceivedConnectionOptions(initial_received_options));
  QuicConfigPeer::SetNegotiated(&config, true);
  QuicConfigPeer::SetReceivedOriginalConnectionId(&config,
                                                  connection_.connection_id());
  QuicConfigPeer::SetReceivedInitialSourceConnectionId(&config,
                                                       QuicConnectionId());

  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  QuicIdleNetworkDetector& idle_network_detector =
      QuicConnectionPeer::GetIdleNetworkDetector(&connection_);
  if (GetQuicReloadableFlag(quic_fix_timeouts)) {
    // Handshake timeout has not been removed yet.
    EXPECT_NE(idle_network_detector.handshake_timeout(),
              QuicTime::Delta::Infinite());
  } else {
    // Handshake timeout has been set to infinite.
    EXPECT_EQ(idle_network_detector.handshake_timeout(),
              QuicTime::Delta::Infinite());
  }
}

TEST_P(QuicConnectionTest, ClientParsesRetry) {
  TestClientRetryHandling(/*invalid_retry_tag=*/false,
                          /*missing_original_id_in_config=*/false,
                          /*wrong_original_id_in_config=*/false,
                          /*missing_retry_id_in_config=*/false,
                          /*wrong_retry_id_in_config=*/false);
}

TEST_P(QuicConnectionTest, ClientParsesRetryInvalidTag) {
  TestClientRetryHandling(/*invalid_retry_tag=*/true,
                          /*missing_original_id_in_config=*/false,
                          /*wrong_original_id_in_config=*/false,
                          /*missing_retry_id_in_config=*/false,
                          /*wrong_retry_id_in_config=*/false);
}

TEST_P(QuicConnectionTest, ClientParsesRetryMissingOriginalId) {
  TestClientRetryHandling(/*invalid_retry_tag=*/false,
                          /*missing_original_id_in_config=*/true,
                          /*wrong_original_id_in_config=*/false,
                          /*missing_retry_id_in_config=*/false,
                          /*wrong_retry_id_in_config=*/false);
}

TEST_P(QuicConnectionTest, ClientParsesRetryWrongOriginalId) {
  TestClientRetryHandling(/*invalid_retry_tag=*/false,
                          /*missing_original_id_in_config=*/false,
                          /*wrong_original_id_in_config=*/true,
                          /*missing_retry_id_in_config=*/false,
                          /*wrong_retry_id_in_config=*/false);
}

TEST_P(QuicConnectionTest, ClientParsesRetryMissingRetryId) {
  if (!connection_.version().UsesTls()) {
    // Versions that do not authenticate connection IDs never send the
    // retry_source_connection_id transport parameter.
    return;
  }
  TestClientRetryHandling(/*invalid_retry_tag=*/false,
                          /*missing_original_id_in_config=*/false,
                          /*wrong_original_id_in_config=*/false,
                          /*missing_retry_id_in_config=*/true,
                          /*wrong_retry_id_in_config=*/false);
}

TEST_P(QuicConnectionTest, ClientParsesRetryWrongRetryId) {
  if (!connection_.version().UsesTls()) {
    // Versions that do not authenticate connection IDs never send the
    // retry_source_connection_id transport parameter.
    return;
  }
  TestClientRetryHandling(/*invalid_retry_tag=*/false,
                          /*missing_original_id_in_config=*/false,
                          /*wrong_original_id_in_config=*/false,
                          /*missing_retry_id_in_config=*/false,
                          /*wrong_retry_id_in_config=*/true);
}

TEST_P(QuicConnectionTest, ClientRetransmitsInitialPacketsOnRetry) {
  if (!connection_.version().HasIetfQuicFrames()) {
    // TestClientRetryHandling() currently only supports IETF draft versions.
    return;
  }
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);

  connection_.SendCryptoStreamData();

  EXPECT_EQ(1u, writer_->packets_write_attempts());
  TestClientRetryHandling(/*invalid_retry_tag=*/false,
                          /*missing_original_id_in_config=*/false,
                          /*wrong_original_id_in_config=*/false,
                          /*missing_retry_id_in_config=*/false,
                          /*wrong_retry_id_in_config=*/false);

  // Verify that initial data is retransmitted immediately after receiving
  // RETRY.
  if (GetParam().ack_response == AckResponse::kImmediate) {
    EXPECT_EQ(2u, writer_->packets_write_attempts());
    EXPECT_LE(1u, writer_->framer()->crypto_frames().size());
  }
}

TEST_P(QuicConnectionTest, NoInitialPacketsRetransmissionOnInvalidRetry) {
  if (!connection_.version().HasIetfQuicFrames()) {
    return;
  }
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);

  connection_.SendCryptoStreamData();

  EXPECT_EQ(1u, writer_->packets_write_attempts());
  TestClientRetryHandling(/*invalid_retry_tag=*/true,
                          /*missing_original_id_in_config=*/false,
                          /*wrong_original_id_in_config=*/false,
                          /*missing_retry_id_in_config=*/false,
                          /*wrong_retry_id_in_config=*/false);

  EXPECT_EQ(1u, writer_->packets_write_attempts());
}

TEST_P(QuicConnectionTest, ClientReceivesOriginalConnectionIdWithoutRetry) {
  if (!connection_.version().UsesTls()) {
    // QUIC+TLS is required to transmit connection ID transport parameters.
    return;
  }
  if (connection_.version().UsesTls()) {
    // Versions that authenticate connection IDs always send the
    // original_destination_connection_id transport parameter.
    return;
  }
  // Make sure that receiving the original_destination_connection_id transport
  // parameter fails the handshake when no RETRY packet was received before it.
  QuicConfig received_config;
  QuicConfigPeer::SetNegotiated(&received_config, true);
  QuicConfigPeer::SetReceivedOriginalConnectionId(&received_config,
                                                  TestConnectionId(0x12345));
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _)).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillRepeatedly(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillRepeatedly(Return(false));
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .Times(1);
  connection_.SetFromConfig(received_config);
  EXPECT_FALSE(connection_.connected());
  TestConnectionCloseQuicErrorCode(IETF_QUIC_PROTOCOL_VIOLATION);
}

TEST_P(QuicConnectionTest, ClientReceivesRetrySourceConnectionIdWithoutRetry) {
  if (!connection_.version().UsesTls()) {
    // Versions that do not authenticate connection IDs never send the
    // retry_source_connection_id transport parameter.
    return;
  }
  // Make sure that receiving the retry_source_connection_id transport parameter
  // fails the handshake when no RETRY packet was received before it.
  QuicConfig received_config;
  QuicConfigPeer::SetNegotiated(&received_config, true);
  QuicConfigPeer::SetReceivedRetrySourceConnectionId(&received_config,
                                                     TestConnectionId(0x12345));
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _)).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillRepeatedly(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillRepeatedly(Return(false));
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .Times(1);
  connection_.SetFromConfig(received_config);
  EXPECT_FALSE(connection_.connected());
  TestConnectionCloseQuicErrorCode(IETF_QUIC_PROTOCOL_VIOLATION);
}

// Regression test for http://crbug/1047977
TEST_P(QuicConnectionTest, MaxStreamsFrameCausesConnectionClose) {
  if (!VersionHasIetfQuicFrames(connection_.transport_version())) {
    return;
  }
  // Received frame causes connection close.
  EXPECT_CALL(visitor_, OnMaxStreamsFrame(_))
      .WillOnce(InvokeWithoutArgs([this]() {
        EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
        connection_.CloseConnection(
            QUIC_TOO_MANY_BUFFERED_CONTROL_FRAMES, "error",
            ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
        return true;
      }));
  QuicFrames frames;
  frames.push_back(QuicFrame(QuicMaxStreamsFrame()));
  frames.push_back(QuicFrame(QuicPaddingFrame(-1)));
  ProcessFramesPacketAtLevel(1, frames, ENCRYPTION_FORWARD_SECURE);
}

TEST_P(QuicConnectionTest, StreamsBlockedFrameCausesConnectionClose) {
  if (!VersionHasIetfQuicFrames(connection_.transport_version())) {
    return;
  }
  // Received frame causes connection close.
  EXPECT_CALL(visitor_, OnStreamsBlockedFrame(_))
      .WillOnce(InvokeWithoutArgs([this]() {
        EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
        connection_.CloseConnection(
            QUIC_TOO_MANY_BUFFERED_CONTROL_FRAMES, "error",
            ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
        return true;
      }));
  QuicFrames frames;
  frames.push_back(
      QuicFrame(QuicStreamsBlockedFrame(kInvalidControlFrameId, 10, false)));
  frames.push_back(QuicFrame(QuicPaddingFrame(-1)));
  ProcessFramesPacketAtLevel(1, frames, ENCRYPTION_FORWARD_SECURE);
}

TEST_P(QuicConnectionTest,
       BundleAckWithConnectionCloseMultiplePacketNumberSpace) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  // Receives packet 1000 in initial data.
  ProcessCryptoPacketAtLevel(1000, ENCRYPTION_INITIAL);
  // Receives packet 2000 in application data.
  ProcessDataPacketAtLevel(2000, false, ENCRYPTION_FORWARD_SECURE);
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
  const QuicErrorCode kQuicErrorCode = QUIC_INTERNAL_ERROR;
  connection_.CloseConnection(
      kQuicErrorCode, "Some random error message",
      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);

  EXPECT_EQ(2u, QuicConnectionPeer::GetNumEncryptionLevels(&connection_));

  TestConnectionCloseQuicErrorCode(kQuicErrorCode);
  EXPECT_EQ(1u, writer_->connection_close_frames().size());
  // Verify ack is bundled.
  EXPECT_EQ(1u, writer_->ack_frames().size());

  if (!connection_.version().CanSendCoalescedPackets()) {
    // Each connection close packet should be sent in distinct UDP packets.
    EXPECT_EQ(QuicConnectionPeer::GetNumEncryptionLevels(&connection_),
              writer_->connection_close_packets());
    EXPECT_EQ(QuicConnectionPeer::GetNumEncryptionLevels(&connection_),
              writer_->packets_write_attempts());
    return;
  }

  // A single UDP packet should be sent with multiple connection close packets
  // coalesced together.
  EXPECT_EQ(1u, writer_->packets_write_attempts());

  // Only the first packet has been processed yet.
  EXPECT_EQ(1u, writer_->connection_close_packets());

  // ProcessPacket resets the visitor and frees the coalesced packet.
  ASSERT_TRUE(writer_->coalesced_packet() != nullptr);
  auto packet = writer_->coalesced_packet()->Clone();
  writer_->framer()->ProcessPacket(*packet);
  EXPECT_EQ(1u, writer_->connection_close_packets());
  EXPECT_EQ(1u, writer_->connection_close_frames().size());
  // Verify ack is bundled.
  EXPECT_EQ(1u, writer_->ack_frames().size());
  ASSERT_TRUE(writer_->coalesced_packet() == nullptr);
}

// Regression test for b/151220135.
TEST_P(QuicConnectionTest, SendPingWhenSkipPacketNumberForPto) {
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(kPTOS);
  connection_options.push_back(k1PTO);
  config.SetConnectionOptionsToSend(connection_options);
  if (connection_.version().UsesTls()) {
    QuicConfigPeer::SetReceivedMaxDatagramFrameSize(
        &config, kMaxAcceptedDatagramFrameSize);
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  connection_.OnHandshakeComplete();
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  EXPECT_EQ(MESSAGE_STATUS_SUCCESS, SendMessage("message"));
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());

  // PTO fires, verify a PING packet gets sent because there is no data to
  // send.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, QuicPacketNumber(3), _, _));
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_EQ(1u, connection_.GetStats().pto_count);
  EXPECT_EQ(0u, connection_.GetStats().crypto_retransmit_count);
  EXPECT_EQ(1u, writer_->ping_frames().size());
}

// Regression test for b/155757133
TEST_P(QuicConnectionTest, DonotChangeQueuedAcks) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  const size_t kMinRttMs = 40;
  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(kMinRttMs),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  // Discard INITIAL key.
  connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
  connection_.NeuterUnencryptedPackets();
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_COMPLETE));

  ProcessPacket(2);
  ProcessPacket(3);
  ProcessPacket(4);
  // Process a packet containing stream frame followed by ACK of packets 1.
  QuicFrames frames;
  frames.push_back(QuicFrame(QuicStreamFrame(
      QuicUtils::GetFirstBidirectionalStreamId(
          connection_.version().transport_version, Perspective::IS_CLIENT),
      false, 0u, absl::string_view())));
  QuicAckFrame ack_frame = InitAckFrame(1);
  frames.push_back(QuicFrame(&ack_frame));
  // Receiving stream frame causes something to send.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).WillOnce(Invoke([this]() {
    connection_.SendControlFrame(QuicFrame(QuicWindowUpdateFrame(1, 0, 0)));
    // Verify now the queued ACK contains packet number 2.
    EXPECT_TRUE(QuicPacketCreatorPeer::QueuedFrames(
                    QuicConnectionPeer::GetPacketCreator(&connection_))[0]
                    .ack_frame->packets.Contains(QuicPacketNumber(2)));
  }));
  ProcessFramesPacketAtLevel(9, frames, ENCRYPTION_FORWARD_SECURE);
  EXPECT_TRUE(writer_->ack_frames()[0].packets.Contains(QuicPacketNumber(2)));
}

TEST_P(QuicConnectionTest, DoNotExtendIdleTimeOnUndecryptablePackets) {
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  connection_.SetFromConfig(config);
  // Subtract a second from the idle timeout on the client side.
  QuicTime initial_deadline =
      clock_.ApproximateNow() +
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs - 1);
  EXPECT_EQ(initial_deadline, connection_.GetTimeoutAlarm()->deadline());

  // Received an undecryptable packet.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(1));
  peer_framer_.SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<quic::NullEncrypter>(Perspective::IS_CLIENT));
  ProcessDataPacketAtLevel(1, !kHasStopWaiting, ENCRYPTION_FORWARD_SECURE);
  // Verify deadline does not get extended.
  EXPECT_EQ(initial_deadline, connection_.GetTimeoutAlarm()->deadline());
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _)).Times(1);
  QuicTime::Delta delay = initial_deadline - clock_.ApproximateNow();
  clock_.AdvanceTime(delay);
  connection_.GetTimeoutAlarm()->Fire();
  // Verify connection gets closed.
  EXPECT_FALSE(connection_.connected());
}

TEST_P(QuicConnectionTest, BundleAckWithImmediateResponse) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);

  EXPECT_CALL(visitor_, OnStreamFrame(_)).WillOnce(Invoke([this]() {
    notifier_.WriteOrBufferWindowUpate(0, 0);
  }));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  ProcessDataPacket(1);
  // Verify ACK is bundled with WINDOW_UPDATE.
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.HasPendingAcks());
}

TEST_P(QuicConnectionTest, AckAlarmFiresEarly) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  }
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  // Receives packet 1000 in initial data.
  ProcessCryptoPacketAtLevel(1000, ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.HasPendingAcks());

  peer_framer_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
  SetDecrypter(ENCRYPTION_ZERO_RTT,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_ZERO_RTT));
  // Receives packet 1000 in application data.
  ProcessDataPacketAtLevel(1000, false, ENCRYPTION_ZERO_RTT);
  EXPECT_TRUE(connection_.HasPendingAcks());
  // Verify ACK deadline does not change.
  EXPECT_EQ(clock_.ApproximateNow() + kAlarmGranularity,
            connection_.GetAckAlarm()->deadline());

  // Ack alarm fires early.
  // Verify the earliest ACK is flushed.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.GetAckAlarm()->Fire();
  EXPECT_TRUE(connection_.HasPendingAcks());
  EXPECT_EQ(clock_.ApproximateNow() + DefaultDelayedAckTime(),
            connection_.GetAckAlarm()->deadline());
}

TEST_P(QuicConnectionTest, ClientOnlyBlackholeDetectionClient) {
  if (!GetQuicReloadableFlag(quic_default_enable_5rto_blackhole_detection2)) {
    return;
  }
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(kCBHD);
  config.SetConnectionOptionsToSend(connection_options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  connection_.OnHandshakeComplete();
  EXPECT_FALSE(connection_.GetBlackholeDetectorAlarm()->IsSet());
  // Send stream data.
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, FIN, nullptr);
  // Verify blackhole detection is in progress.
  EXPECT_TRUE(connection_.GetBlackholeDetectorAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, ClientOnlyBlackholeDetectionServer) {
  if (!GetQuicReloadableFlag(quic_default_enable_5rto_blackhole_detection2)) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  if (version().SupportsAntiAmplificationLimit()) {
    QuicConnectionPeer::SetAddressValidated(&connection_);
  }
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(kCBHD);
  config.SetInitialReceivedConnectionOptions(connection_options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_COMPLETE));
  EXPECT_FALSE(connection_.GetBlackholeDetectorAlarm()->IsSet());
  // Send stream data.
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, FIN, nullptr);
  // Verify blackhole detection is disabled.
  EXPECT_FALSE(connection_.GetBlackholeDetectorAlarm()->IsSet());
}

// Regresstion test for b/158491591.
TEST_P(QuicConnectionTest, MadeForwardProgressOnDiscardingKeys) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  // Send handshake packet.
  connection_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(1);
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(k5RTO);
  config.SetConnectionOptionsToSend(connection_options);
  QuicConfigPeer::SetNegotiated(&config, true);
  if (GetQuicReloadableFlag(quic_default_enable_5rto_blackhole_detection2) ||
      GetQuicReloadableFlag(
          quic_no_path_degrading_before_handshake_confirmed)) {
    EXPECT_CALL(visitor_, GetHandshakeState())
        .WillRepeatedly(Return(HANDSHAKE_COMPLETE));
  }
  if (connection_.version().UsesTls()) {
    QuicConfigPeer::SetReceivedOriginalConnectionId(
        &config, connection_.connection_id());
    QuicConfigPeer::SetReceivedInitialSourceConnectionId(
        &config, connection_.connection_id());
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);

  connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_HANDSHAKE);
  if (GetQuicReloadableFlag(
          quic_no_path_degrading_before_handshake_confirmed)) {
    // No blackhole detection before handshake confirmed.
    EXPECT_FALSE(connection_.BlackholeDetectionInProgress());
  } else {
    EXPECT_TRUE(connection_.BlackholeDetectionInProgress());
  }
  // Discard handshake keys.
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  connection_.OnHandshakeComplete();
  if (GetQuicReloadableFlag(quic_default_enable_5rto_blackhole_detection2) ||
      GetQuicReloadableFlag(
          quic_no_path_degrading_before_handshake_confirmed)) {
    // Verify blackhole detection stops.
    EXPECT_FALSE(connection_.BlackholeDetectionInProgress());
  } else {
    // Problematic: although there is nothing in flight, blackhole detection is
    // still in progress.
    EXPECT_TRUE(connection_.BlackholeDetectionInProgress());
  }
}

TEST_P(QuicConnectionTest, ProcessUndecryptablePacketsBasedOnEncryptionLevel) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  // SetFromConfig is always called after construction from InitializeSession.
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(AnyNumber());
  QuicConfig config;
  connection_.SetFromConfig(config);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  connection_.RemoveDecrypter(ENCRYPTION_FORWARD_SECURE);

  peer_framer_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  peer_framer_.SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));

  for (uint64_t i = 1; i <= 3; ++i) {
    ProcessDataPacketAtLevel(i, !kHasStopWaiting, ENCRYPTION_HANDSHAKE);
  }
  ProcessDataPacketAtLevel(4, !kHasStopWaiting, ENCRYPTION_FORWARD_SECURE);
  for (uint64_t j = 5; j <= 7; ++j) {
    ProcessDataPacketAtLevel(j, !kHasStopWaiting, ENCRYPTION_HANDSHAKE);
  }
  EXPECT_EQ(7u, QuicConnectionPeer::NumUndecryptablePackets(&connection_));
  EXPECT_FALSE(connection_.GetProcessUndecryptablePacketsAlarm()->IsSet());
  SetDecrypter(ENCRYPTION_HANDSHAKE,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_HANDSHAKE));
  EXPECT_TRUE(connection_.GetProcessUndecryptablePacketsAlarm()->IsSet());
  connection_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
  // Verify all ENCRYPTION_HANDSHAKE packets get processed.
  if (!VersionHasIetfQuicFrames(version().transport_version)) {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(6);
  }
  connection_.GetProcessUndecryptablePacketsAlarm()->Fire();
  EXPECT_EQ(1u, QuicConnectionPeer::NumUndecryptablePackets(&connection_));

  SetDecrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_FORWARD_SECURE));
  EXPECT_TRUE(connection_.GetProcessUndecryptablePacketsAlarm()->IsSet());
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
  // Verify the 1-RTT packet gets processed.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  connection_.GetProcessUndecryptablePacketsAlarm()->Fire();
  EXPECT_EQ(0u, QuicConnectionPeer::NumUndecryptablePackets(&connection_));
}

TEST_P(QuicConnectionTest, ServerBundlesInitialDataWithInitialAck) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  }
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  // Receives packet 1000 in initial data.
  ProcessCryptoPacketAtLevel(1000, ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.HasPendingAcks());

  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_INITIAL);
  QuicTime expected_pto_time =
      connection_.sent_packet_manager().GetRetransmissionTime();

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  connection_.SetEncrypter(ENCRYPTION_HANDSHAKE,
                           std::make_unique<TaggingEncrypter>(0x02));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(1);
  connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_HANDSHAKE);
  // Verify PTO time does not change.
  EXPECT_EQ(expected_pto_time,
            connection_.sent_packet_manager().GetRetransmissionTime());

  // Receives packet 1001 in initial data.
  ProcessCryptoPacketAtLevel(1001, ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.HasPendingAcks());
  // Receives packet 1002 in initial data.
  ProcessCryptoPacketAtLevel(1002, ENCRYPTION_INITIAL);
  EXPECT_FALSE(writer_->ack_frames().empty());
  // Verify CRYPTO frame is bundled with INITIAL ACK.
  EXPECT_FALSE(writer_->crypto_frames().empty());
  // Verify PTO time changes.
  EXPECT_NE(expected_pto_time,
            connection_.sent_packet_manager().GetRetransmissionTime());
}

TEST_P(QuicConnectionTest, ClientBundlesHandshakeDataWithHandshakeAck) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  EXPECT_EQ(Perspective::IS_CLIENT, connection_.perspective());
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  }
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  connection_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
  SetDecrypter(ENCRYPTION_HANDSHAKE,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_HANDSHAKE));
  peer_framer_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  // Receives packet 1000 in handshake data.
  ProcessCryptoPacketAtLevel(1000, ENCRYPTION_HANDSHAKE);
  EXPECT_TRUE(connection_.HasPendingAcks());
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(1);
  connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_HANDSHAKE);

  // Receives packet 1001 in handshake data.
  ProcessCryptoPacketAtLevel(1001, ENCRYPTION_HANDSHAKE);
  EXPECT_TRUE(connection_.HasPendingAcks());
  // Receives packet 1002 in handshake data.
  ProcessCryptoPacketAtLevel(1002, ENCRYPTION_HANDSHAKE);
  EXPECT_FALSE(writer_->ack_frames().empty());
  // Verify CRYPTO frame is bundled with HANDSHAKE ACK.
  EXPECT_FALSE(writer_->crypto_frames().empty());
}

// Regresstion test for b/156232673.
TEST_P(QuicConnectionTest, CoalescePacketOfLowerEncryptionLevel) {
  if (!connection_.version().CanSendCoalescedPackets()) {
    return;
  }
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(1);
  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    connection_.SetEncrypter(
        ENCRYPTION_HANDSHAKE,
        std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
    connection_.SetEncrypter(
        ENCRYPTION_FORWARD_SECURE,
        std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    SendStreamDataToPeer(2, std::string(1286, 'a'), 0, NO_FIN, nullptr);
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
    // Try to coalesce a HANDSHAKE packet after 1-RTT packet.
    // Verify soft max packet length gets resumed and handshake packet gets
    // successfully sent.
    connection_.SendCryptoDataWithString("a", 0, ENCRYPTION_HANDSHAKE);
  }
}

// Regression test for b/160790422.
TEST_P(QuicConnectionTest, ServerRetransmitsHandshakeDataEarly) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  }
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  // Receives packet 1000 in initial data.
  ProcessCryptoPacketAtLevel(1000, ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.HasPendingAcks());

  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  // Send INITIAL 1.
  connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_INITIAL);
  QuicTime expected_pto_time =
      connection_.sent_packet_manager().GetRetransmissionTime();

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  connection_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(1);
  // Send HANDSHAKE 2 and 3.
  connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_HANDSHAKE);
  connection_.SendCryptoDataWithString("bar", 3, ENCRYPTION_HANDSHAKE);
  // Verify PTO time does not change.
  EXPECT_EQ(expected_pto_time,
            connection_.sent_packet_manager().GetRetransmissionTime());

  // Receives ACK for HANDSHAKE 2.
  QuicFrames frames;
  auto ack_frame = InitAckFrame({{QuicPacketNumber(2), QuicPacketNumber(3)}});
  frames.push_back(QuicFrame(&ack_frame));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _));
  ProcessFramesPacketAtLevel(30, frames, ENCRYPTION_HANDSHAKE);
  // Discard INITIAL key.
  connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
  connection_.NeuterUnencryptedPackets();
  // Receives PING from peer.
  frames.clear();
  frames.push_back(QuicFrame(QuicPingFrame()));
  frames.push_back(QuicFrame(QuicPaddingFrame(3)));
  ProcessFramesPacketAtLevel(31, frames, ENCRYPTION_HANDSHAKE);
  EXPECT_EQ(clock_.Now() + kAlarmGranularity,
            connection_.GetAckAlarm()->deadline());
  // Fire ACK alarm.
  clock_.AdvanceTime(kAlarmGranularity);
  connection_.GetAckAlarm()->Fire();
  EXPECT_FALSE(writer_->ack_frames().empty());
  // Verify handshake data gets retransmitted early.
  EXPECT_FALSE(writer_->crypto_frames().empty());
}

// Regression test for b/161228202
TEST_P(QuicConnectionTest, InflatedRttSample) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  // 30ms RTT.
  const QuicTime::Delta kTestRTT = QuicTime::Delta::FromMilliseconds(30);
  set_perspective(Perspective::IS_SERVER);
  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  // Receives packet 1000 in initial data.
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  }
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  ProcessCryptoPacketAtLevel(1000, ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.HasPendingAcks());

  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  // Send INITIAL 1.
  std::string initial_crypto_data(512, 'a');
  connection_.SendCryptoDataWithString(initial_crypto_data, 0,
                                       ENCRYPTION_INITIAL);
  ASSERT_TRUE(connection_.sent_packet_manager()
                  .GetRetransmissionTime()
                  .IsInitialized());
  QuicTime::Delta pto_timeout =
      connection_.sent_packet_manager().GetRetransmissionTime() - clock_.Now();
  // Send Handshake 2.
  connection_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(1);
  std::string handshake_crypto_data(1024, 'a');
  connection_.SendCryptoDataWithString(handshake_crypto_data, 0,
                                       ENCRYPTION_HANDSHAKE);

  // INITIAL 1 gets lost and PTO fires.
  clock_.AdvanceTime(pto_timeout);
  connection_.GetRetransmissionAlarm()->Fire();

  clock_.AdvanceTime(kTestRTT);
  // Assume retransmitted INITIAL gets received.
  QuicFrames frames;
  auto ack_frame = InitAckFrame({{QuicPacketNumber(4), QuicPacketNumber(5)}});
  frames.push_back(QuicFrame(&ack_frame));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _))
      .Times(AnyNumber());
  ProcessFramesPacketAtLevel(1001, frames, ENCRYPTION_INITIAL);
  EXPECT_EQ(kTestRTT, rtt_stats->latest_rtt());
  // Because retransmitted INITIAL gets received so HANDSHAKE 2 gets processed.
  frames.clear();
  // HANDSHAKE 5 is also processed.
  QuicAckFrame ack_frame2 =
      InitAckFrame({{QuicPacketNumber(2), QuicPacketNumber(3)},
                    {QuicPacketNumber(5), QuicPacketNumber(6)}});
  ack_frame2.ack_delay_time = QuicTime::Delta::Zero();
  frames.push_back(QuicFrame(&ack_frame2));
  ProcessFramesPacketAtLevel(1, frames, ENCRYPTION_HANDSHAKE);
  // Verify RTT inflation gets mitigated.
  EXPECT_EQ(rtt_stats->latest_rtt(), kTestRTT);
}

// Regression test for b/161228202
TEST_P(QuicConnectionTest, CoalescingPacketCausesInfiniteLoop) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  // Receives packet 1000 in initial data.
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  }
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());

  // Set anti amplification factor to 2, such that RetransmitDataOfSpaceIfAny
  // makes no forward progress and causes infinite loop.
  SetQuicFlag(quic_anti_amplification_factor, 2);

  ProcessCryptoPacketAtLevel(1000, ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.HasPendingAcks());

  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  // Send INITIAL 1.
  std::string initial_crypto_data(512, 'a');
  connection_.SendCryptoDataWithString(initial_crypto_data, 0,
                                       ENCRYPTION_INITIAL);
  ASSERT_TRUE(connection_.sent_packet_manager()
                  .GetRetransmissionTime()
                  .IsInitialized());
  QuicTime::Delta pto_timeout =
      connection_.sent_packet_manager().GetRetransmissionTime() - clock_.Now();
  // Send Handshake 2.
  connection_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
  // Verify HANDSHAKE packet is coalesced with INITIAL retransmission.
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(1);
  std::string handshake_crypto_data(1024, 'a');
  connection_.SendCryptoDataWithString(handshake_crypto_data, 0,
                                       ENCRYPTION_HANDSHAKE);

  // INITIAL 1 gets lost and PTO fires.
  clock_.AdvanceTime(pto_timeout);
  connection_.GetRetransmissionAlarm()->Fire();
}

TEST_P(QuicConnectionTest, ClientAckDelayForAsyncPacketProcessing) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  // SetFromConfig is always called after construction from InitializeSession.
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).WillOnce(Invoke([this]() {
    connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
    connection_.NeuterUnencryptedPackets();
  }));
  QuicConfig config;
  connection_.SetFromConfig(config);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  peer_framer_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  EXPECT_EQ(0u, QuicConnectionPeer::NumUndecryptablePackets(&connection_));

  // Received undecryptable HANDSHAKE 2.
  ProcessDataPacketAtLevel(2, !kHasStopWaiting, ENCRYPTION_HANDSHAKE);
  ASSERT_EQ(1u, QuicConnectionPeer::NumUndecryptablePackets(&connection_));
  // Received INITIAL 4 (which is retransmission of INITIAL 1) after 100ms.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(100));
  ProcessDataPacketAtLevel(4, !kHasStopWaiting, ENCRYPTION_INITIAL);
  // Generate HANDSHAKE key.
  SetDecrypter(ENCRYPTION_HANDSHAKE,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_HANDSHAKE));
  EXPECT_TRUE(connection_.GetProcessUndecryptablePacketsAlarm()->IsSet());
  connection_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
  // Verify HANDSHAKE packet gets processed.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.GetProcessUndecryptablePacketsAlarm()->Fire();
  // Verify immediate ACK has been sent out when flush went out of scope.
  ASSERT_FALSE(connection_.HasPendingAcks());
  ASSERT_FALSE(writer_->ack_frames().empty());
  // Verify the ack_delay_time in the sent HANDSHAKE ACK frame is 100ms.
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(100),
            writer_->ack_frames()[0].ack_delay_time);
  ASSERT_TRUE(writer_->coalesced_packet() == nullptr);
}

TEST_P(QuicConnectionTest, TestingLiveness) {
  const size_t kMinRttMs = 40;
  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(kMinRttMs),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;

  CryptoHandshakeMessage msg;
  std::string error_details;
  QuicConfig client_config;
  client_config.SetInitialStreamFlowControlWindowToSend(
      kInitialStreamFlowControlWindowForTest);
  client_config.SetInitialSessionFlowControlWindowToSend(
      kInitialSessionFlowControlWindowForTest);
  client_config.SetIdleNetworkTimeout(QuicTime::Delta::FromSeconds(30));
  client_config.ToHandshakeMessage(&msg, connection_.transport_version());
  const QuicErrorCode error =
      config.ProcessPeerHello(msg, CLIENT, &error_details);
  EXPECT_THAT(error, IsQuicNoError());

  if (connection_.version().UsesTls()) {
    QuicConfigPeer::SetReceivedOriginalConnectionId(
        &config, connection_.connection_id());
    QuicConfigPeer::SetReceivedInitialSourceConnectionId(
        &config, connection_.connection_id());
  }

  connection_.SetFromConfig(config);
  connection_.OnHandshakeComplete();
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  ASSERT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.MaybeTestLiveness());

  QuicTime deadline = QuicConnectionPeer::GetIdleNetworkDeadline(&connection_);
  QuicTime::Delta timeout = deadline - clock_.ApproximateNow();
  // Advance time to near the idle timeout.
  clock_.AdvanceTime(timeout - QuicTime::Delta::FromMilliseconds(1));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  EXPECT_TRUE(connection_.MaybeTestLiveness());
  // Verify idle deadline does not change.
  EXPECT_EQ(deadline, QuicConnectionPeer::GetIdleNetworkDeadline(&connection_));
}

TEST_P(QuicConnectionTest, DisableLivenessTesting) {
  const size_t kMinRttMs = 40;
  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(kMinRttMs),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;

  CryptoHandshakeMessage msg;
  std::string error_details;
  QuicConfig client_config;
  client_config.SetInitialStreamFlowControlWindowToSend(
      kInitialStreamFlowControlWindowForTest);
  client_config.SetInitialSessionFlowControlWindowToSend(
      kInitialSessionFlowControlWindowForTest);
  client_config.SetIdleNetworkTimeout(QuicTime::Delta::FromSeconds(30));
  client_config.ToHandshakeMessage(&msg, connection_.transport_version());
  const QuicErrorCode error =
      config.ProcessPeerHello(msg, CLIENT, &error_details);
  EXPECT_THAT(error, IsQuicNoError());

  if (connection_.version().UsesTls()) {
    QuicConfigPeer::SetReceivedOriginalConnectionId(
        &config, connection_.connection_id());
    QuicConfigPeer::SetReceivedInitialSourceConnectionId(
        &config, connection_.connection_id());
  }

  connection_.SetFromConfig(config);
  connection_.OnHandshakeComplete();
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.DisableLivenessTesting();
  ASSERT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.MaybeTestLiveness());

  QuicTime deadline = QuicConnectionPeer::GetIdleNetworkDeadline(&connection_);
  QuicTime::Delta timeout = deadline - clock_.ApproximateNow();
  // Advance time to near the idle timeout.
  clock_.AdvanceTime(timeout - QuicTime::Delta::FromMilliseconds(1));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  EXPECT_FALSE(connection_.MaybeTestLiveness());
}

TEST_P(QuicConnectionTest, SilentIdleTimeout) {
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  if (version().SupportsAntiAmplificationLimit()) {
    QuicConnectionPeer::SetAddressValidated(&connection_);
  }

  QuicConfig config;
  QuicConfigPeer::SetNegotiated(&config, true);
  if (connection_.version().UsesTls()) {
    QuicConfigPeer::SetReceivedOriginalConnectionId(
        &config, connection_.connection_id());
    QuicConfigPeer::SetReceivedInitialSourceConnectionId(&config,
                                                         QuicConnectionId());
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);

  EXPECT_TRUE(connection_.connected());
  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());

  if (version().handshake_protocol == PROTOCOL_TLS1_3) {
    EXPECT_CALL(visitor_, BeforeConnectionCloseSent());
  }
  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.GetTimeoutAlarm()->Fire();
  // Verify the connection close packets get serialized and added to
  // termination packets list.
  EXPECT_NE(nullptr,
            QuicConnectionPeer::GetConnectionClosePacket(&connection_));
}

TEST_P(QuicConnectionTest, DoNotSendPing) {
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.OnHandshakeComplete();
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(0, connection_.transport_version()),
      "GET /", 0, FIN, nullptr);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(15),
            connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  // Now recevie an ACK and response of the previous packet, which will move the
  // ping alarm forward.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  QuicFrames frames;
  QuicAckFrame ack_frame = InitAckFrame(1);
  frames.push_back(QuicFrame(&ack_frame));
  frames.push_back(QuicFrame(QuicStreamFrame(
      GetNthClientInitiatedStreamId(0, connection_.transport_version()), true,
      0u, absl::string_view())));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessFramesPacketAtLevel(1, frames, ENCRYPTION_FORWARD_SECURE);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  // The ping timer is set slightly less than 15 seconds in the future, because
  // of the 1s ping timer alarm granularity.
  EXPECT_EQ(
      QuicTime::Delta::FromSeconds(15) - QuicTime::Delta::FromMilliseconds(5),
      connection_.GetPingAlarm()->deadline() - clock_.ApproximateNow());

  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(15));
  // Suppose now ShouldKeepConnectionAlive returns false.
  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(false));
  // Verify PING does not get sent.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.GetPingAlarm()->Fire();
}

// Regression test for b/159698337
TEST_P(QuicConnectionTest, DuplicateAckCausesLostPackets) {
  if (!GetQuicReloadableFlag(quic_default_enable_5rto_blackhole_detection2)) {
    return;
  }
  // Finish handshake.
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  notifier_.NeuterUnencryptedData();
  connection_.NeuterUnencryptedPackets();
  connection_.OnHandshakeComplete();
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));

  std::string data(1200, 'a');
  // Send data packets 1 - 5.
  for (size_t i = 0; i < 5; ++i) {
    SendStreamDataToPeer(
        GetNthClientInitiatedStreamId(1, connection_.transport_version()), data,
        i * 1200, i == 4 ? FIN : NO_FIN, nullptr);
  }
  ASSERT_TRUE(connection_.BlackholeDetectionInProgress());

  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _))
      .Times(3);

  // ACK packet 5 and 1 and 2 are detected lost.
  QuicAckFrame frame =
      InitAckFrame({{QuicPacketNumber(5), QuicPacketNumber(6)}});
  LostPacketVector lost_packets;
  lost_packets.push_back(
      LostPacket(QuicPacketNumber(1), kMaxOutgoingPacketSize));
  lost_packets.push_back(
      LostPacket(QuicPacketNumber(2), kMaxOutgoingPacketSize));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .Times(AnyNumber())
      .WillOnce(DoAll(SetArgPointee<5>(lost_packets),
                      Return(LossDetectionInterface::DetectionStats())))
      .WillRepeatedly(DoDefault());
  ;
  ProcessAckPacket(1, &frame);
  EXPECT_TRUE(connection_.BlackholeDetectionInProgress());
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());

  // ACK packet 1 - 5 and 7.
  QuicAckFrame frame2 =
      InitAckFrame({{QuicPacketNumber(1), QuicPacketNumber(6)},
                    {QuicPacketNumber(7), QuicPacketNumber(8)}});
  ProcessAckPacket(2, &frame2);
  EXPECT_TRUE(connection_.BlackholeDetectionInProgress());

  // ACK packet 7 again and assume packet 6 is detected lost.
  QuicAckFrame frame3 =
      InitAckFrame({{QuicPacketNumber(7), QuicPacketNumber(8)}});
  lost_packets.clear();
  lost_packets.push_back(
      LostPacket(QuicPacketNumber(6), kMaxOutgoingPacketSize));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .Times(AnyNumber())
      .WillOnce(DoAll(SetArgPointee<5>(lost_packets),
                      Return(LossDetectionInterface::DetectionStats())));
  ProcessAckPacket(3, &frame3);
  // Make sure loss detection is cancelled even there is no new acked packets.
  EXPECT_FALSE(connection_.BlackholeDetectionInProgress());
}

TEST_P(QuicConnectionTest, ShorterIdleTimeoutOnSentPackets) {
  EXPECT_TRUE(connection_.connected());
  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(100),
                       QuicTime::Delta::Zero(), QuicTime::Zero());

  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  config.SetClientConnectionOptions(QuicTagVector{kFIDT});
  QuicConfigPeer::SetNegotiated(&config, true);
  if (GetQuicReloadableFlag(quic_default_enable_5rto_blackhole_detection2)) {
    EXPECT_CALL(visitor_, GetHandshakeState())
        .WillRepeatedly(Return(HANDSHAKE_COMPLETE));
  }
  if (connection_.version().UsesTls()) {
    QuicConfigPeer::SetReceivedOriginalConnectionId(
        &config, connection_.connection_id());
    QuicConfigPeer::SetReceivedInitialSourceConnectionId(
        &config, connection_.connection_id());
  }
  connection_.SetFromConfig(config);

  ASSERT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  // Send a packet close to timeout.
  QuicTime::Delta timeout =
      connection_.GetTimeoutAlarm()->deadline() - clock_.Now();
  clock_.AdvanceTime(timeout - QuicTime::Delta::FromSeconds(1));
  // Send stream data.
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, FIN, nullptr);
  // Verify this sent packet does not extend idle timeout since 1s is > PTO
  // delay.
  ASSERT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(1),
            connection_.GetTimeoutAlarm()->deadline() - clock_.Now());

  // Received an ACK 100ms later.
  clock_.AdvanceTime(timeout - QuicTime::Delta::FromMilliseconds(100));
  QuicAckFrame ack = InitAckFrame(1);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  ProcessAckPacket(1, &ack);
  // Verify idle timeout gets extended.
  EXPECT_EQ(clock_.Now() + timeout, connection_.GetTimeoutAlarm()->deadline());
}

// Regression test for b/166255274
TEST_P(QuicConnectionTest,
       ReserializeInitialPacketInCoalescerAfterDiscardingInitialKey) {
  if (!connection_.version().CanSendCoalescedPackets()) {
    return;
  }
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(1);
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.HasPendingAcks());
  connection_.SetEncrypter(ENCRYPTION_HANDSHAKE,
                           std::make_unique<TaggingEncrypter>(0x02));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).WillOnce(Invoke([this]() {
    connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
    connection_.NeuterUnencryptedPackets();
  }));
  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_HANDSHAKE);
    // Verify the packet is on hold.
    EXPECT_EQ(0u, writer_->packets_write_attempts());
    // Flush pending ACKs.
    connection_.GetAckAlarm()->Fire();
  }
  EXPECT_FALSE(connection_.packet_creator().HasPendingFrames());
  // The ACK frame is deleted along with initial_packet_ in coalescer. Sending
  // connection close would cause this (released) ACK frame be serialized (and
  // crashes).
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(1000, false, ENCRYPTION_FORWARD_SECURE);
  EXPECT_TRUE(connection_.connected());
}

TEST_P(QuicConnectionTest, PathValidationOnNewSocketSuccess) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_CLIENT);
  const QuicSocketAddress kNewSelfAddress(QuicIpAddress::Any4(), 12345);
  EXPECT_NE(kNewSelfAddress, connection_.self_address());
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(AtLeast(1u))
      .WillOnce(Invoke([&]() {
        EXPECT_EQ(1u, new_writer.packets_write_attempts());
        EXPECT_EQ(1u, new_writer.path_challenge_frames().size());
        EXPECT_EQ(1u, new_writer.padding_frames().size());
        EXPECT_EQ(kNewSelfAddress.host(),
                  new_writer.last_write_source_address());
      }))
      .WillRepeatedly(DoDefault());
  ;
  bool success = false;
  connection_.ValidatePath(
      std::make_unique<TestQuicPathValidationContext>(
          kNewSelfAddress, connection_.peer_address(), &new_writer),
      std::make_unique<TestValidationResultDelegate>(
          &connection_, kNewSelfAddress, connection_.peer_address(), &success),
      PathValidationReason::kReasonUnknown);
  EXPECT_EQ(0u, writer_->packets_write_attempts());

  QuicFrames frames;
  frames.push_back(QuicFrame(QuicPathResponseFrame(
      99, new_writer.path_challenge_frames().front().data_buffer)));
  ProcessFramesPacketWithAddresses(frames, kNewSelfAddress, kPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_TRUE(success);
}

TEST_P(QuicConnectionTest, PathValidationOnNewSocketWriteBlocked) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_CLIENT);
  const QuicSocketAddress kNewSelfAddress(QuicIpAddress::Any4(), 12345);
  EXPECT_NE(kNewSelfAddress, connection_.self_address());
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  new_writer.SetWriteBlocked();
  bool success = false;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.ValidatePath(
      std::make_unique<TestQuicPathValidationContext>(
          kNewSelfAddress, connection_.peer_address(), &new_writer),
      std::make_unique<TestValidationResultDelegate>(
          &connection_, kNewSelfAddress, connection_.peer_address(), &success),
      PathValidationReason::kReasonUnknown);
  EXPECT_EQ(0u, new_writer.packets_write_attempts());
  EXPECT_TRUE(connection_.HasPendingPathValidation());

  new_writer.SetWritable();
  // Retry after time out.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(3 * kInitialRttMs));
  static_cast<test::MockRandom*>(helper_->GetRandomGenerator())->ChangeValue();
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(Invoke([&]() {
        EXPECT_EQ(1u, new_writer.packets_write_attempts());
        EXPECT_EQ(1u, new_writer.path_challenge_frames().size());
        EXPECT_EQ(1u, new_writer.padding_frames().size());
        EXPECT_EQ(kNewSelfAddress.host(),
                  new_writer.last_write_source_address());
      }));
  static_cast<TestAlarmFactory::TestAlarm*>(
      QuicPathValidatorPeer::retry_timer(
          QuicConnectionPeer::path_validator(&connection_)))
      ->Fire();
  EXPECT_EQ(1u, new_writer.packets_write_attempts());

  QuicFrames frames;
  QuicPathFrameBuffer path_frame_buffer{0, 1, 2, 3, 4, 5, 6, 7};
  frames.push_back(QuicFrame(QuicPathChallengeFrame(0, path_frame_buffer)));
  new_writer.SetWriteBlocked();
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillRepeatedly(Invoke([&] {
        // Packets other than PATH_RESPONSE may be sent over the default writer.
        EXPECT_EQ(1u, new_writer.packets_write_attempts());
        EXPECT_TRUE(new_writer.path_response_frames().empty());
        EXPECT_EQ(1u, writer_->packets_write_attempts());
      }));
  ProcessFramesPacketWithAddresses(frames, kNewSelfAddress,
                                   connection_.peer_address(),
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(1u, new_writer.packets_write_attempts());
}

TEST_P(QuicConnectionTest, NewPathValidationCancelsPreviousOne) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_CLIENT);
  const QuicSocketAddress kNewSelfAddress(QuicIpAddress::Any4(), 12345);
  EXPECT_NE(kNewSelfAddress, connection_.self_address());
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(AtLeast(1u))
      .WillOnce(Invoke([&]() {
        EXPECT_EQ(1u, new_writer.packets_write_attempts());
        EXPECT_EQ(1u, new_writer.path_challenge_frames().size());
        EXPECT_EQ(1u, new_writer.padding_frames().size());
        EXPECT_EQ(kNewSelfAddress.host(),
                  new_writer.last_write_source_address());
      }));
  bool success = true;
  connection_.ValidatePath(
      std::make_unique<TestQuicPathValidationContext>(
          kNewSelfAddress, connection_.peer_address(), &new_writer),
      std::make_unique<TestValidationResultDelegate>(
          &connection_, kNewSelfAddress, connection_.peer_address(), &success),
      PathValidationReason::kReasonUnknown);
  EXPECT_EQ(0u, writer_->packets_write_attempts());

  // Start another path validation request.
  const QuicSocketAddress kNewSelfAddress2(QuicIpAddress::Any4(), 12346);
  EXPECT_NE(kNewSelfAddress2, connection_.self_address());
  TestPacketWriter new_writer2(version(), &clock_, Perspective::IS_CLIENT);
  bool success2 = false;
  connection_.ValidatePath(
      std::make_unique<TestQuicPathValidationContext>(
          kNewSelfAddress2, connection_.peer_address(), &new_writer2),
      std::make_unique<TestValidationResultDelegate>(
          &connection_, kNewSelfAddress2, connection_.peer_address(),
          &success2),
      PathValidationReason::kReasonUnknown);
  EXPECT_FALSE(success);
  // There is no pening path validation as there is no available connection ID.
  EXPECT_FALSE(connection_.HasPendingPathValidation());
}

// Regression test for b/182571515.
TEST_P(QuicConnectionTest, PathValidationRetry) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_CLIENT);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(2u)
      .WillRepeatedly(Invoke([&]() {
        EXPECT_EQ(1u, writer_->path_challenge_frames().size());
        EXPECT_EQ(1u, writer_->padding_frames().size());
      }));
  bool success = true;
  connection_.ValidatePath(std::make_unique<TestQuicPathValidationContext>(
                               connection_.self_address(),
                               connection_.peer_address(), writer_.get()),
                           std::make_unique<TestValidationResultDelegate>(
                               &connection_, connection_.self_address(),
                               connection_.peer_address(), &success),
                           PathValidationReason::kReasonUnknown);
  EXPECT_EQ(1u, writer_->packets_write_attempts());
  EXPECT_TRUE(connection_.HasPendingPathValidation());

  // Retry after time out.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(3 * kInitialRttMs));
  static_cast<test::MockRandom*>(helper_->GetRandomGenerator())->ChangeValue();
  static_cast<TestAlarmFactory::TestAlarm*>(
      QuicPathValidatorPeer::retry_timer(
          QuicConnectionPeer::path_validator(&connection_)))
      ->Fire();
  EXPECT_EQ(2u, writer_->packets_write_attempts());
}

TEST_P(QuicConnectionTest, PathValidationReceivesStatelessReset) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_CLIENT);
  QuicConfig config;
  QuicConfigPeer::SetReceivedStatelessResetToken(&config,
                                                 kTestStatelessResetToken);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  const QuicSocketAddress kNewSelfAddress(QuicIpAddress::Any4(), 12345);
  EXPECT_NE(kNewSelfAddress, connection_.self_address());
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(AtLeast(1u))
      .WillOnce(Invoke([&]() {
        EXPECT_EQ(1u, new_writer.packets_write_attempts());
        EXPECT_EQ(1u, new_writer.path_challenge_frames().size());
        EXPECT_EQ(1u, new_writer.padding_frames().size());
        EXPECT_EQ(kNewSelfAddress.host(),
                  new_writer.last_write_source_address());
      }))
      .WillRepeatedly(DoDefault());
  ;
  bool success = true;
  connection_.ValidatePath(
      std::make_unique<TestQuicPathValidationContext>(
          kNewSelfAddress, connection_.peer_address(), &new_writer),
      std::make_unique<TestValidationResultDelegate>(
          &connection_, kNewSelfAddress, connection_.peer_address(), &success),
      PathValidationReason::kReasonUnknown);
  EXPECT_EQ(0u, writer_->packets_write_attempts());
  EXPECT_TRUE(connection_.HasPendingPathValidation());

  std::unique_ptr<QuicEncryptedPacket> packet(
      QuicFramer::BuildIetfStatelessResetPacket(connection_id_,
                                                /*received_packet_length=*/100,
                                                kTestStatelessResetToken));
  std::unique_ptr<QuicReceivedPacket> received(
      ConstructReceivedPacket(*packet, QuicTime::Zero()));
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _)).Times(0);
  connection_.ProcessUdpPacket(kNewSelfAddress, kPeerAddress, *received);
  EXPECT_FALSE(connection_.HasPendingPathValidation());
  EXPECT_FALSE(success);
}

// Tests that PATH_CHALLENGE is dropped if it is sent via a blocked alternative
// writer.
TEST_P(QuicConnectionTest, SendPathChallengeUsingBlockedNewSocket) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_CLIENT);
  const QuicSocketAddress kNewSelfAddress(QuicIpAddress::Any4(), 12345);
  EXPECT_NE(kNewSelfAddress, connection_.self_address());
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  new_writer.BlockOnNextWrite();
  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(0);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(AtLeast(1))
      .WillOnce(Invoke([&]() {
        // Even though the socket is blocked, the PATH_CHALLENGE should still be
        // treated as sent.
        EXPECT_EQ(1u, new_writer.packets_write_attempts());
        EXPECT_EQ(1u, new_writer.path_challenge_frames().size());
        EXPECT_EQ(1u, new_writer.padding_frames().size());
        EXPECT_EQ(kNewSelfAddress.host(),
                  new_writer.last_write_source_address());
      }))
      .WillRepeatedly(DoDefault());
  ;
  bool success = false;
  connection_.ValidatePath(
      std::make_unique<TestQuicPathValidationContext>(
          kNewSelfAddress, connection_.peer_address(), &new_writer),
      std::make_unique<TestValidationResultDelegate>(
          &connection_, kNewSelfAddress, connection_.peer_address(), &success),
      PathValidationReason::kReasonUnknown);
  EXPECT_EQ(0u, writer_->packets_write_attempts());

  new_writer.SetWritable();
  // Write event on the default socket shouldn't make any difference.
  connection_.OnCanWrite();
  // A NEW_CONNECTION_ID frame is received in PathProbeTestInit and OnCanWrite
  // will write a acking packet.
  EXPECT_EQ(1u, writer_->packets_write_attempts());
  EXPECT_EQ(1u, new_writer.packets_write_attempts());
}

//  Tests that PATH_CHALLENGE is dropped if it is sent via the default writer
//  and the writer is blocked.
TEST_P(QuicConnectionTest, SendPathChallengeUsingBlockedDefaultSocket) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_SERVER);
  const QuicSocketAddress kNewPeerAddress(QuicIpAddress::Any4(), 12345);
  writer_->BlockOnNextWrite();
  // 1st time is after writer returns WRITE_STATUS_BLOCKED. 2nd time is in
  // ShouldGeneratePacket().
  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(AtLeast(2));
  QuicPathFrameBuffer path_challenge_payload{0, 1, 2, 3, 4, 5, 6, 7};
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(AtLeast(1u))
      .WillOnce(Invoke([&]() {
        // This packet isn't sent actually, instead it is buffered in the
        // connection.
        EXPECT_EQ(1u, writer_->packets_write_attempts());
        EXPECT_EQ(1u, writer_->path_response_frames().size());
        EXPECT_EQ(0,
                  memcmp(&path_challenge_payload,
                         &writer_->path_response_frames().front().data_buffer,
                         sizeof(path_challenge_payload)));
        EXPECT_EQ(1u, writer_->path_challenge_frames().size());
        EXPECT_EQ(1u, writer_->padding_frames().size());
        EXPECT_EQ(kNewPeerAddress, writer_->last_write_peer_address());
      }))
      .WillRepeatedly(Invoke([&]() {
        // Only one PATH_CHALLENGE should be sent out.
        EXPECT_EQ(0u, writer_->path_challenge_frames().size());
      }));
  // Receiving a PATH_CHALLENGE from the new peer address should trigger address
  // validation.
  QuicFrames frames;
  frames.push_back(
      QuicFrame(QuicPathChallengeFrame(0, path_challenge_payload)));
  ProcessFramesPacketWithAddresses(frames, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(1u, writer_->packets_write_attempts());

  // Try again with the new socket blocked from the beginning. The 2nd
  // PATH_CHALLENGE shouldn't be serialized, but be dropped.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(3 * kInitialRttMs));
  static_cast<test::MockRandom*>(helper_->GetRandomGenerator())->ChangeValue();
  static_cast<TestAlarmFactory::TestAlarm*>(
      QuicPathValidatorPeer::retry_timer(
          QuicConnectionPeer::path_validator(&connection_)))
      ->Fire();

  // No more write attempt should be made.
  EXPECT_EQ(1u, writer_->packets_write_attempts());

  writer_->SetWritable();
  // OnCanWrite() should actually write out the 1st PATH_CHALLENGE packet
  // buffered earlier, thus incrementing the write counter. It may also send
  // ACKs to previously received packets.
  connection_.OnCanWrite();
  EXPECT_LE(2u, writer_->packets_write_attempts());
}

// Tests that write error on the alternate socket should be ignored.
TEST_P(QuicConnectionTest, SendPathChallengeFailOnNewSocket) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_CLIENT);
  const QuicSocketAddress kNewSelfAddress(QuicIpAddress::Any4(), 12345);
  EXPECT_NE(kNewSelfAddress, connection_.self_address());
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  new_writer.SetShouldWriteFail();
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .Times(0);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0u);

  bool success = false;
  connection_.ValidatePath(
      std::make_unique<TestQuicPathValidationContext>(
          kNewSelfAddress, connection_.peer_address(), &new_writer),
      std::make_unique<TestValidationResultDelegate>(
          &connection_, kNewSelfAddress, connection_.peer_address(), &success),
      PathValidationReason::kReasonUnknown);
  EXPECT_EQ(1u, new_writer.packets_write_attempts());
  EXPECT_EQ(1u, new_writer.path_challenge_frames().size());
  EXPECT_EQ(1u, new_writer.padding_frames().size());
  EXPECT_EQ(kNewSelfAddress.host(), new_writer.last_write_source_address());

  EXPECT_EQ(0u, writer_->packets_write_attempts());
  //  Regardless of the write error, the connection should still be connected.
  EXPECT_TRUE(connection_.connected());
}

// Tests that write error while sending PATH_CHALLANGE from the default socket
// should close the connection.
TEST_P(QuicConnectionTest, SendPathChallengeFailOnDefaultPath) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_CLIENT);

  writer_->SetShouldWriteFail();
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .WillOnce(
          Invoke([](QuicConnectionCloseFrame frame, ConnectionCloseSource) {
            EXPECT_EQ(QUIC_PACKET_WRITE_ERROR, frame.quic_error_code);
          }));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0u);
  {
    // Add a flusher to force flush, otherwise the frames will remain in the
    // packet creator.
    bool success = false;
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    connection_.ValidatePath(std::make_unique<TestQuicPathValidationContext>(
                                 connection_.self_address(),
                                 connection_.peer_address(), writer_.get()),
                             std::make_unique<TestValidationResultDelegate>(
                                 &connection_, connection_.self_address(),
                                 connection_.peer_address(), &success),
                             PathValidationReason::kReasonUnknown);
  }
  EXPECT_EQ(1u, writer_->packets_write_attempts());
  EXPECT_EQ(1u, writer_->path_challenge_frames().size());
  EXPECT_EQ(1u, writer_->padding_frames().size());
  EXPECT_EQ(connection_.peer_address(), writer_->last_write_peer_address());
  EXPECT_FALSE(connection_.connected());
  // Closing connection should abandon ongoing path validation.
  EXPECT_FALSE(connection_.HasPendingPathValidation());
}

TEST_P(QuicConnectionTest, SendPathChallengeFailOnAlternativePeerAddress) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_CLIENT);

  writer_->SetShouldWriteFail();
  const QuicSocketAddress kNewPeerAddress(QuicIpAddress::Any4(), 12345);
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .WillOnce(
          Invoke([](QuicConnectionCloseFrame frame, ConnectionCloseSource) {
            EXPECT_EQ(QUIC_PACKET_WRITE_ERROR, frame.quic_error_code);
          }));
  // Sending PATH_CHALLENGE to trigger a flush write which will fail and close
  // the connection.
  bool success = false;
  connection_.ValidatePath(
      std::make_unique<TestQuicPathValidationContext>(
          connection_.self_address(), kNewPeerAddress, writer_.get()),
      std::make_unique<TestValidationResultDelegate>(
          &connection_, connection_.self_address(), kNewPeerAddress, &success),
      PathValidationReason::kReasonUnknown);

  EXPECT_EQ(1u, writer_->packets_write_attempts());
  EXPECT_FALSE(connection_.HasPendingPathValidation());
  EXPECT_EQ(1u, writer_->path_challenge_frames().size());
  EXPECT_EQ(1u, writer_->padding_frames().size());
  EXPECT_EQ(kNewPeerAddress, writer_->last_write_peer_address());
  EXPECT_FALSE(connection_.connected());
}

TEST_P(QuicConnectionTest,
       SendPathChallengeFailPacketTooBigOnAlternativePeerAddress) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_CLIENT);
  // Make sure there is no outstanding ACK_FRAME to write.
  connection_.OnCanWrite();
  uint32_t num_packets_write_attempts = writer_->packets_write_attempts();

  writer_->SetShouldWriteFail();
  writer_->SetWriteError(*writer_->MessageTooBigErrorCode());
  const QuicSocketAddress kNewPeerAddress(QuicIpAddress::Any4(), 12345);
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .Times(0u);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0u);
  // Sending PATH_CHALLENGE to trigger a flush write which will fail with
  // MSG_TOO_BIG.
  bool success = false;
  connection_.ValidatePath(
      std::make_unique<TestQuicPathValidationContext>(
          connection_.self_address(), kNewPeerAddress, writer_.get()),
      std::make_unique<TestValidationResultDelegate>(
          &connection_, connection_.self_address(), kNewPeerAddress, &success),
      PathValidationReason::kReasonUnknown);
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  // Connection shouldn't be closed.
  EXPECT_TRUE(connection_.connected());
  EXPECT_EQ(++num_packets_write_attempts, writer_->packets_write_attempts());
  EXPECT_EQ(1u, writer_->path_challenge_frames().size());
  EXPECT_EQ(1u, writer_->padding_frames().size());
  EXPECT_EQ(kNewPeerAddress, writer_->last_write_peer_address());
}

// Check that if there are two PATH_CHALLENGE frames in the packet, the latter
// one is ignored.
TEST_P(QuicConnectionTest, ReceiveMultiplePathChallenge) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_SERVER);

  QuicPathFrameBuffer path_frame_buffer1{0, 1, 2, 3, 4, 5, 6, 7};
  QuicPathFrameBuffer path_frame_buffer2{8, 9, 10, 11, 12, 13, 14, 15};
  QuicFrames frames;
  frames.push_back(QuicFrame(QuicPathChallengeFrame(0, path_frame_buffer1)));
  frames.push_back(QuicFrame(QuicPathChallengeFrame(0, path_frame_buffer2)));
  const QuicSocketAddress kNewPeerAddress(QuicIpAddress::Loopback6(),
                                          /*port=*/23456);

  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);

  // Expect 2 packets to be sent: the first are padded PATH_RESPONSE(s) to the
  // alternative peer address. The 2nd is a ACK-only packet to the original
  // peer address.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(2)
      .WillOnce(Invoke([=, this]() {
        EXPECT_EQ(1u, writer_->path_response_frames().size());
        // The final check is to ensure that the random data in the response
        // matches the random data from the challenge.
        EXPECT_EQ(0,
                  memcmp(path_frame_buffer1.data(),
                         &(writer_->path_response_frames().front().data_buffer),
                         sizeof(path_frame_buffer1)));
        EXPECT_EQ(1u, writer_->padding_frames().size());
        EXPECT_EQ(kNewPeerAddress, writer_->last_write_peer_address());
      }))
      .WillOnce(Invoke([=, this]() {
        // The last write of ACK-only packet should still use the old peer
        // address.
        EXPECT_EQ(kPeerAddress, writer_->last_write_peer_address());
      }));
  ProcessFramesPacketWithAddresses(frames, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
}

TEST_P(QuicConnectionTest, ReceiveStreamFrameBeforePathChallenge) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_SERVER);

  QuicFrames frames;
  frames.push_back(QuicFrame(frame1_));
  QuicPathFrameBuffer path_frame_buffer{0, 1, 2, 3, 4, 5, 6, 7};
  frames.push_back(QuicFrame(QuicPathChallengeFrame(0, path_frame_buffer)));
  frames.push_back(QuicFrame(QuicPaddingFrame(-1)));
  const QuicSocketAddress kNewPeerAddress(QuicIpAddress::Loopback4(),
                                          /*port=*/23456);

  EXPECT_CALL(visitor_, OnConnectionMigration(IPV6_TO_IPV4_CHANGE));
  EXPECT_CALL(*send_algorithm_, OnConnectionMigration()).Times(0u);
  EXPECT_CALL(visitor_, OnStreamFrame(_))
      .WillOnce(Invoke([=, this](const QuicStreamFrame& frame) {
        // Send some data on the stream. The STREAM_FRAME should be built into
        // one packet together with the latter PATH_RESPONSE and PATH_CHALLENGE.
        const std::string data{"response body"};
        connection_.producer()->SaveStreamData(frame.stream_id, data);
        return notifier_.WriteOrBufferData(frame.stream_id, data.length(),
                                           NO_FIN);
      }));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0u);
  ProcessFramesPacketWithAddresses(frames, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);

  // Verify that this packet contains a STREAM_FRAME and a
  // PATH_RESPONSE_FRAME.
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_EQ(1u, writer_->path_response_frames().size());
  EXPECT_EQ(1u, writer_->path_challenge_frames().size());
  // The final check is to ensure that the random data in the response
  // matches the random data from the challenge.
  EXPECT_EQ(0, memcmp(path_frame_buffer.data(),
                      &(writer_->path_response_frames().front().data_buffer),
                      sizeof(path_frame_buffer)));
  EXPECT_EQ(1u, writer_->path_challenge_frames().size());
  EXPECT_EQ(1u, writer_->padding_frames().size());
  EXPECT_EQ(kNewPeerAddress, writer_->last_write_peer_address());
  EXPECT_TRUE(connection_.HasPendingPathValidation());
}

TEST_P(QuicConnectionTest, ReceiveStreamFrameFollowingPathChallenge) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_SERVER);

  QuicFrames frames;
  QuicPathFrameBuffer path_frame_buffer{0, 1, 2, 3, 4, 5, 6, 7};
  frames.push_back(QuicFrame(QuicPathChallengeFrame(0, path_frame_buffer)));
  // PATH_RESPONSE should be flushed out before the rest packet is parsed.
  frames.push_back(QuicFrame(frame1_));
  const QuicSocketAddress kNewPeerAddress(QuicIpAddress::Loopback4(),
                                          /*port=*/23456);
  QuicByteCount received_packet_size;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(AtLeast(1u))
      .WillOnce(Invoke([=, this, &received_packet_size]() {
        // Verify that this packet contains a PATH_RESPONSE_FRAME.
        EXPECT_EQ(0u, writer_->stream_frames().size());
        EXPECT_EQ(1u, writer_->path_response_frames().size());
        // The final check is to ensure that the random data in the response
        // matches the random data from the challenge.
        EXPECT_EQ(0,
                  memcmp(path_frame_buffer.data(),
                         &(writer_->path_response_frames().front().data_buffer),
                         sizeof(path_frame_buffer)));
        EXPECT_EQ(1u, writer_->path_challenge_frames().size());
        EXPECT_EQ(1u, writer_->padding_frames().size());
        EXPECT_EQ(kNewPeerAddress, writer_->last_write_peer_address());
        received_packet_size =
            QuicConnectionPeer::BytesReceivedOnAlternativePath(&connection_);
      }));
  EXPECT_CALL(visitor_, OnConnectionMigration(IPV6_TO_IPV4_CHANGE));
  EXPECT_CALL(*send_algorithm_, OnConnectionMigration()).Times(0u);
  EXPECT_CALL(visitor_, OnStreamFrame(_))
      .WillOnce(Invoke([=, this](const QuicStreamFrame& frame) {
        // Send some data on the stream. The STREAM_FRAME should be built into a
        // new packet but throttled by anti-amplifciation limit.
        const std::string data{"response body"};
        connection_.producer()->SaveStreamData(frame.stream_id, data);
        return notifier_.WriteOrBufferData(frame.stream_id, data.length(),
                                           NO_FIN);
      }));

  ProcessFramesPacketWithAddresses(frames, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  EXPECT_EQ(0u,
            QuicConnectionPeer::BytesReceivedOnAlternativePath(&connection_));
  EXPECT_EQ(
      received_packet_size,
      QuicConnectionPeer::BytesReceivedBeforeAddressValidation(&connection_));
}

// Tests that a PATH_CHALLENGE is received in between other frames in an out of
// order packet.
TEST_P(QuicConnectionTest, PathChallengeWithDataInOutOfOrderPacket) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_SERVER);

  QuicFrames frames;
  frames.push_back(QuicFrame(frame1_));
  QuicPathFrameBuffer path_frame_buffer{0, 1, 2, 3, 4, 5, 6, 7};
  frames.push_back(QuicFrame(QuicPathChallengeFrame(0, path_frame_buffer)));
  frames.push_back(QuicFrame(frame2_));
  const QuicSocketAddress kNewPeerAddress(QuicIpAddress::Loopback6(),
                                          /*port=*/23456);

  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0u);
  EXPECT_CALL(visitor_, OnStreamFrame(_))
      .Times(2)
      .WillRepeatedly(Invoke([=, this](const QuicStreamFrame& frame) {
        // Send some data on the stream. The STREAM_FRAME should be built into
        // one packet together with the latter PATH_RESPONSE.
        const std::string data{"response body"};
        connection_.producer()->SaveStreamData(frame.stream_id, data);
        return notifier_.WriteOrBufferData(frame.stream_id, data.length(),
                                           NO_FIN);
      }));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(Invoke([=, this]() {
        // Verify that this packet contains a STREAM_FRAME and is sent to the
        // original peer address.
        EXPECT_EQ(1u, writer_->stream_frames().size());
        // No connection migration should happen because the packet is received
        // out of order.
        EXPECT_EQ(kPeerAddress, writer_->last_write_peer_address());
      }))
      .WillOnce(Invoke([=, this]() {
        EXPECT_EQ(1u, writer_->path_response_frames().size());
        // The final check is to ensure that the random data in the response
        // matches the random data from the challenge.
        EXPECT_EQ(0,
                  memcmp(path_frame_buffer.data(),
                         &(writer_->path_response_frames().front().data_buffer),
                         sizeof(path_frame_buffer)));
        EXPECT_EQ(1u, writer_->padding_frames().size());
        // PATH_RESPONSE should be sent in another packet to a different peer
        // address.
        EXPECT_EQ(kNewPeerAddress, writer_->last_write_peer_address());
      }))
      .WillOnce(Invoke([=, this]() {
        // Verify that this packet contains a STREAM_FRAME and is sent to the
        // original peer address.
        EXPECT_EQ(1u, writer_->stream_frames().size());
        // No connection migration should happen because the packet is received
        // out of order.
        EXPECT_EQ(kPeerAddress, writer_->last_write_peer_address());
      }));
  // Lower the packet number so that receiving this packet shouldn't trigger
  // peer migration.
  QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 1);
  ProcessFramesPacketWithAddresses(frames, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
}

// Tests that a PATH_CHALLENGE is cached if its PATH_RESPONSE can't be sent.
TEST_P(QuicConnectionTest, FailToWritePathResponseAtServer) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_SERVER);

  QuicFrames frames;
  QuicPathFrameBuffer path_frame_buffer{0, 1, 2, 3, 4, 5, 6, 7};
  frames.push_back(QuicFrame(QuicPathChallengeFrame(0, path_frame_buffer)));
  const QuicSocketAddress kNewPeerAddress(QuicIpAddress::Loopback6(),
                                          /*port=*/23456);

  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0u);
  // Lower the packet number so that receiving this packet shouldn't trigger
  // peer migration.
  QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 1);
  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(AtLeast(1));
  writer_->SetWriteBlocked();
  ProcessFramesPacketWithAddresses(frames, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
}

// Regression test for b/168101557.
TEST_P(QuicConnectionTest, HandshakeDataDoesNotGetPtoed) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  }
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.HasPendingAcks());

  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  // Send INITIAL 1.
  connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_INITIAL);

  connection_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
  SetDecrypter(ENCRYPTION_HANDSHAKE,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_HANDSHAKE));
  // Send HANDSHAKE packets.
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(1);
  connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_HANDSHAKE);

  connection_.SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  // Send half RTT packet.
  connection_.SendStreamDataWithString(2, "foo", 0, NO_FIN);

  // Receives HANDSHAKE 1.
  peer_framer_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_HANDSHAKE);
  // Discard INITIAL key.
  connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
  connection_.NeuterUnencryptedPackets();
  // Verify there is pending ACK.
  ASSERT_TRUE(connection_.HasPendingAcks());
  // Set the send alarm.
  connection_.GetSendAlarm()->Set(clock_.ApproximateNow());

  // Fire ACK alarm.
  connection_.GetAckAlarm()->Fire();
  // Verify 1-RTT packet is coalesced with handshake packet.
  EXPECT_EQ(0x03030303u, writer_->final_bytes_of_last_packet());
  connection_.GetSendAlarm()->Fire();

  ASSERT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  connection_.GetRetransmissionAlarm()->Fire();
  // Verify a handshake packet gets PTOed and 1-RTT packet gets coalesced.
  EXPECT_EQ(0x03030303u, writer_->final_bytes_of_last_packet());
}

// Regression test for b/168294218.
TEST_P(QuicConnectionTest, CoalescerHandlesInitialKeyDiscard) {
  if (!connection_.version().CanSendCoalescedPackets()) {
    return;
  }
  SetQuicReloadableFlag(quic_discard_initial_packet_with_key_dropped, true);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).WillOnce(Invoke([this]() {
    connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
    connection_.NeuterUnencryptedPackets();
  }));
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());

  EXPECT_EQ(0u, connection_.GetStats().packets_discarded);
  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    ProcessCryptoPacketAtLevel(1000, ENCRYPTION_INITIAL);
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
    connection_.SetEncrypter(
        ENCRYPTION_HANDSHAKE,
        std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
    connection_.SendCryptoDataWithString(std::string(1200, 'a'), 0);
    // Verify this packet is on hold.
    EXPECT_EQ(0u, writer_->packets_write_attempts());
  }
  EXPECT_TRUE(connection_.connected());
}

// Regresstion test for b/168294218
TEST_P(QuicConnectionTest, ZeroRttRejectionAndMissingInitialKeys) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  // Not defer send in response to packet.
  connection_.set_defer_send_in_response_to_packets(false);
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).WillOnce(Invoke([this]() {
    connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
    connection_.NeuterUnencryptedPackets();
  }));
  EXPECT_CALL(visitor_, OnCryptoFrame(_))
      .WillRepeatedly(Invoke([=, this](const QuicCryptoFrame& frame) {
        if (frame.level == ENCRYPTION_HANDSHAKE) {
          // 0-RTT gets rejected.
          connection_.MarkZeroRttPacketsForRetransmission(0);
          // Send Crypto data.
          connection_.SetEncrypter(
              ENCRYPTION_HANDSHAKE,
              std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
          connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
          connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_HANDSHAKE);
          connection_.SetEncrypter(
              ENCRYPTION_FORWARD_SECURE,
              std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
          connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
          // Advance INITIAL ack delay to trigger initial ACK to be sent AFTER
          // the retransmission of rejected 0-RTT packets while the HANDSHAKE
          // packet is still in the coalescer, such that the INITIAL key gets
          // dropped between SendAllPendingAcks and actually send the ack frame,
          // bummer.
          clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
        }
      }));
  connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_INITIAL);
  // Send 0-RTT packet.
  connection_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);
  connection_.SendStreamDataWithString(2, "foo", 0, NO_FIN);

  QuicAckFrame frame1 = InitAckFrame(1);
  // Received ACK for packet 1.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _));
  ProcessFramePacketAtLevel(1, QuicFrame(&frame1), ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());

  // Fire retransmission alarm.
  connection_.GetRetransmissionAlarm()->Fire();

  QuicFrames frames1;
  frames1.push_back(QuicFrame(&crypto_frame_));
  QuicFrames frames2;
  QuicCryptoFrame crypto_frame(ENCRYPTION_HANDSHAKE, 0,
                               absl::string_view(data1));
  frames2.push_back(QuicFrame(&crypto_frame));
  ProcessCoalescedPacket(
      {{2, frames1, ENCRYPTION_INITIAL}, {3, frames2, ENCRYPTION_HANDSHAKE}});
}

TEST_P(QuicConnectionTest, OnZeroRttPacketAcked) {
  if (!connection_.version().UsesTls()) {
    return;
  }
  MockQuicConnectionDebugVisitor debug_visitor;
  connection_.set_debug_visitor(&debug_visitor);
  connection_.SendCryptoStreamData();
  // Send 0-RTT packet.
  connection_.SetEncrypter(ENCRYPTION_ZERO_RTT,
                           std::make_unique<TaggingEncrypter>(0x02));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);
  connection_.SendStreamDataWithString(2, "foo", 0, NO_FIN);
  connection_.SendStreamDataWithString(4, "bar", 0, NO_FIN);
  // Received ACK for packet 1, HANDSHAKE packet and 1-RTT ACK.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _))
      .Times(AnyNumber());
  QuicFrames frames1;
  QuicAckFrame ack_frame1 = InitAckFrame(1);
  frames1.push_back(QuicFrame(&ack_frame1));

  QuicFrames frames2;
  QuicCryptoFrame crypto_frame(ENCRYPTION_HANDSHAKE, 0,
                               absl::string_view(data1));
  frames2.push_back(QuicFrame(&crypto_frame));
  EXPECT_CALL(debug_visitor, OnZeroRttPacketAcked()).Times(0);
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(1);
  ProcessCoalescedPacket(
      {{1, frames1, ENCRYPTION_INITIAL}, {2, frames2, ENCRYPTION_HANDSHAKE}});

  QuicFrames frames3;
  QuicAckFrame ack_frame2 =
      InitAckFrame({{QuicPacketNumber(2), QuicPacketNumber(3)}});
  frames3.push_back(QuicFrame(&ack_frame2));
  EXPECT_CALL(debug_visitor, OnZeroRttPacketAcked()).Times(1);
  ProcessCoalescedPacket({{3, frames3, ENCRYPTION_FORWARD_SECURE}});

  QuicFrames frames4;
  QuicAckFrame ack_frame3 =
      InitAckFrame({{QuicPacketNumber(3), QuicPacketNumber(4)}});
  frames4.push_back(QuicFrame(&ack_frame3));
  EXPECT_CALL(debug_visitor, OnZeroRttPacketAcked()).Times(0);
  ProcessCoalescedPacket({{4, frames4, ENCRYPTION_FORWARD_SECURE}});
}

TEST_P(QuicConnectionTest, InitiateKeyUpdate) {
  if (!connection_.version().UsesTls()) {
    return;
  }

  TransportParameters params;
  QuicConfig config;
  std::string error_details;
  EXPECT_THAT(config.ProcessTransportParameters(
                  params, /* is_resumption = */ false, &error_details),
              IsQuicNoError());
  QuicConfigPeer::SetNegotiated(&config, true);
  if (connection_.version().UsesTls()) {
    QuicConfigPeer::SetReceivedOriginalConnectionId(
        &config, connection_.connection_id());
    QuicConfigPeer::SetReceivedInitialSourceConnectionId(
        &config, connection_.connection_id());
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);

  EXPECT_FALSE(connection_.IsKeyUpdateAllowed());

  MockFramerVisitor peer_framer_visitor_;
  peer_framer_.set_visitor(&peer_framer_visitor_);

  uint8_t correct_tag = ENCRYPTION_FORWARD_SECURE;
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<TaggingEncrypter>(correct_tag));
  SetDecrypter(ENCRYPTION_FORWARD_SECURE,
               std::make_unique<StrictTaggingDecrypter>(correct_tag));
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  connection_.OnHandshakeComplete();

  peer_framer_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                            std::make_unique<TaggingEncrypter>(correct_tag));

  // Key update should still not be allowed, since no packet has been acked
  // from the current key phase.
  EXPECT_FALSE(connection_.IsKeyUpdateAllowed());
  EXPECT_FALSE(connection_.HaveSentPacketsInCurrentKeyPhaseButNoneAcked());

  // Send packet 1.
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, &last_packet);
  EXPECT_EQ(QuicPacketNumber(1u), last_packet);

  // Key update should still not be allowed, even though a packet was sent in
  // the current key phase it hasn't been acked yet.
  EXPECT_FALSE(connection_.IsKeyUpdateAllowed());
  EXPECT_TRUE(connection_.HaveSentPacketsInCurrentKeyPhaseButNoneAcked());

  EXPECT_FALSE(connection_.GetDiscardPreviousOneRttKeysAlarm()->IsSet());
  // Receive ack for packet 1.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame1 = InitAckFrame(1);
  ProcessAckPacket(&frame1);

  // OnDecryptedFirstPacketInKeyPhase is called even on the first key phase,
  // so discard_previous_keys_alarm_ should be set now.
  EXPECT_TRUE(connection_.GetDiscardPreviousOneRttKeysAlarm()->IsSet());
  EXPECT_FALSE(connection_.HaveSentPacketsInCurrentKeyPhaseButNoneAcked());

  correct_tag++;
  // Key update should now be allowed.
  EXPECT_CALL(visitor_, AdvanceKeysAndCreateCurrentOneRttDecrypter())
      .WillOnce([&correct_tag]() {
        return std::make_unique<StrictTaggingDecrypter>(correct_tag);
      });
  EXPECT_CALL(visitor_, CreateCurrentOneRttEncrypter())
      .WillOnce([&correct_tag]() {
        return std::make_unique<TaggingEncrypter>(correct_tag);
      });
  EXPECT_CALL(visitor_, OnKeyUpdate(KeyUpdateReason::kLocalForTests));
  EXPECT_TRUE(connection_.InitiateKeyUpdate(KeyUpdateReason::kLocalForTests));
  // discard_previous_keys_alarm_ should not be set until a packet from the new
  // key phase has been received. (The alarm that was set above should be
  // cleared if it hasn't fired before the next key update happened.)
  EXPECT_FALSE(connection_.GetDiscardPreviousOneRttKeysAlarm()->IsSet());
  EXPECT_FALSE(connection_.HaveSentPacketsInCurrentKeyPhaseButNoneAcked());

  // Pretend that peer accepts the key update.
  EXPECT_CALL(peer_framer_visitor_,
              AdvanceKeysAndCreateCurrentOneRttDecrypter())
      .WillOnce([&correct_tag]() {
        return std::make_unique<StrictTaggingDecrypter>(correct_tag);
      });
  EXPECT_CALL(peer_framer_visitor_, CreateCurrentOneRttEncrypter())
      .WillOnce([&correct_tag]() {
        return std::make_unique<TaggingEncrypter>(correct_tag);
      });
  peer_framer_.SetKeyUpdateSupportForConnection(true);
  peer_framer_.DoKeyUpdate(KeyUpdateReason::kRemote);

  // Another key update should not be allowed yet.
  EXPECT_FALSE(connection_.IsKeyUpdateAllowed());

  // Send packet 2.
  SendStreamDataToPeer(2, "bar", 0, NO_FIN, &last_packet);
  EXPECT_EQ(QuicPacketNumber(2u), last_packet);
  EXPECT_TRUE(connection_.HaveSentPacketsInCurrentKeyPhaseButNoneAcked());
  // Receive ack for packet 2.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame2 = InitAckFrame(2);
  ProcessAckPacket(&frame2);
  EXPECT_TRUE(connection_.GetDiscardPreviousOneRttKeysAlarm()->IsSet());
  EXPECT_FALSE(connection_.HaveSentPacketsInCurrentKeyPhaseButNoneAcked());

  correct_tag++;
  // Key update should be allowed again now that a packet has been acked from
  // the current key phase.
  EXPECT_CALL(visitor_, AdvanceKeysAndCreateCurrentOneRttDecrypter())
      .WillOnce([&correct_tag]() {
        return std::make_unique<StrictTaggingDecrypter>(correct_tag);
      });
  EXPECT_CALL(visitor_, CreateCurrentOneRttEncrypter())
      .WillOnce([&correct_tag]() {
        return std::make_unique<TaggingEncrypter>(correct_tag);
      });
  EXPECT_CALL(visitor_, OnKeyUpdate(KeyUpdateReason::kLocalForTests));
  EXPECT_TRUE(connection_.InitiateKeyUpdate(KeyUpdateReason::kLocalForTests));

  // Pretend that peer accepts the key update.
  EXPECT_CALL(peer_framer_visitor_,
              AdvanceKeysAndCreateCurrentOneRttDecrypter())
      .WillOnce([&correct_tag]() {
        return std::make_unique<StrictTaggingDecrypter>(correct_tag);
      });
  EXPECT_CALL(peer_framer_visitor_, CreateCurrentOneRttEncrypter())
      .WillOnce([&correct_tag]() {
        return std::make_unique<TaggingEncrypter>(correct_tag);
      });
  peer_framer_.DoKeyUpdate(KeyUpdateReason::kRemote);

  // Another key update should not be allowed yet.
  EXPECT_FALSE(connection_.IsKeyUpdateAllowed());

  // Send packet 3.
  SendStreamDataToPeer(3, "baz", 0, NO_FIN, &last_packet);
  EXPECT_EQ(QuicPacketNumber(3u), last_packet);

  // Another key update should not be allowed yet.
  EXPECT_FALSE(connection_.IsKeyUpdateAllowed());
  EXPECT_TRUE(connection_.HaveSentPacketsInCurrentKeyPhaseButNoneAcked());

  // Receive ack for packet 3.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame3 = InitAckFrame(3);
  ProcessAckPacket(&frame3);
  EXPECT_TRUE(connection_.GetDiscardPreviousOneRttKeysAlarm()->IsSet());
  EXPECT_FALSE(connection_.HaveSentPacketsInCurrentKeyPhaseButNoneAcked());

  correct_tag++;
  // Key update should be allowed now.
  EXPECT_CALL(visitor_, AdvanceKeysAndCreateCurrentOneRttDecrypter())
      .WillOnce([&correct_tag]() {
        return std::make_unique<StrictTaggingDecrypter>(correct_tag);
      });
  EXPECT_CALL(visitor_, CreateCurrentOneRttEncrypter())
      .WillOnce([&correct_tag]() {
        return std::make_unique<TaggingEncrypter>(correct_tag);
      });
  EXPECT_CALL(visitor_, OnKeyUpdate(KeyUpdateReason::kLocalForTests));
  EXPECT_TRUE(connection_.InitiateKeyUpdate(KeyUpdateReason::kLocalForTests));
  EXPECT_FALSE(connection_.GetDiscardPreviousOneRttKeysAlarm()->IsSet());
  EXPECT_FALSE(connection_.HaveSentPacketsInCurrentKeyPhaseButNoneAcked());
}

TEST_P(QuicConnectionTest, InitiateKeyUpdateApproachingConfidentialityLimit) {
  if (!connection_.version().UsesTls()) {
    return;
  }

  SetQuicFlag(quic_key_update_confidentiality_limit, 3U);

  std::string error_details;
  TransportParameters params;
  // Key update is enabled.
  QuicConfig config;
  EXPECT_THAT(config.ProcessTransportParameters(
                  params, /* is_resumption = */ false, &error_details),
              IsQuicNoError());
  QuicConfigPeer::SetNegotiated(&config, true);
  if (connection_.version().UsesTls()) {
    QuicConfigPeer::SetReceivedOriginalConnectionId(
        &config, connection_.connection_id());
    QuicConfigPeer::SetReceivedInitialSourceConnectionId(
        &config, connection_.connection_id());
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);

  MockFramerVisitor peer_framer_visitor_;
  peer_framer_.set_visitor(&peer_framer_visitor_);

  uint8_t current_tag = ENCRYPTION_FORWARD_SECURE;

  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<TaggingEncrypter>(current_tag));
  SetDecrypter(ENCRYPTION_FORWARD_SECURE,
               std::make_unique<StrictTaggingDecrypter>(current_tag));
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  connection_.OnHandshakeComplete();

  peer_framer_.SetKeyUpdateSupportForConnection(true);
  peer_framer_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                            std::make_unique<TaggingEncrypter>(current_tag));

  const QuicConnectionStats& stats = connection_.GetStats();

  for (int packet_num = 1; packet_num <= 8; ++packet_num) {
    if (packet_num == 3 || packet_num == 6) {
      current_tag++;
      EXPECT_CALL(visitor_, AdvanceKeysAndCreateCurrentOneRttDecrypter())
          .WillOnce([current_tag]() {
            return std::make_unique<StrictTaggingDecrypter>(current_tag);
          });
      EXPECT_CALL(visitor_, CreateCurrentOneRttEncrypter())
          .WillOnce([current_tag]() {
            return std::make_unique<TaggingEncrypter>(current_tag);
          });
      EXPECT_CALL(visitor_,
                  OnKeyUpdate(KeyUpdateReason::kLocalKeyUpdateLimitOverride));
    }
    // Send packet.
    QuicPacketNumber last_packet;
    SendStreamDataToPeer(packet_num, "foo", 0, NO_FIN, &last_packet);
    EXPECT_EQ(QuicPacketNumber(packet_num), last_packet);
    if (packet_num >= 6) {
      EXPECT_EQ(2U, stats.key_update_count);
    } else if (packet_num >= 3) {
      EXPECT_EQ(1U, stats.key_update_count);
    } else {
      EXPECT_EQ(0U, stats.key_update_count);
    }

    if (packet_num == 4 || packet_num == 7) {
      // Pretend that peer accepts the key update.
      EXPECT_CALL(peer_framer_visitor_,
                  AdvanceKeysAndCreateCurrentOneRttDecrypter())
          .WillOnce([current_tag]() {
            return std::make_unique<StrictTaggingDecrypter>(current_tag);
          });
      EXPECT_CALL(peer_framer_visitor_, CreateCurrentOneRttEncrypter())
          .WillOnce([current_tag]() {
            return std::make_unique<TaggingEncrypter>(current_tag);
          });
      peer_framer_.DoKeyUpdate(KeyUpdateReason::kRemote);
    }
    // Receive ack for packet.
    EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
    QuicAckFrame frame1 = InitAckFrame(packet_num);
    ProcessAckPacket(&frame1);
  }
}

TEST_P(QuicConnectionTest,
       CloseConnectionOnConfidentialityLimitKeyUpdateNotAllowed) {
  if (!connection_.version().UsesTls()) {
    return;
  }

  // Set key update confidentiality limit to 1 packet.
  SetQuicFlag(quic_key_update_confidentiality_limit, 1U);
  // Use confidentiality limit for connection close of 3 packets.
  constexpr size_t kConfidentialityLimit = 3U;

  std::string error_details;
  TransportParameters params;
  // Key update is enabled.
  QuicConfig config;
  EXPECT_THAT(config.ProcessTransportParameters(
                  params, /* is_resumption = */ false, &error_details),
              IsQuicNoError());
  QuicConfigPeer::SetNegotiated(&config, true);
  if (connection_.version().UsesTls()) {
    QuicConfigPeer::SetReceivedOriginalConnectionId(
        &config, connection_.connection_id());
    QuicConfigPeer::SetReceivedInitialSourceConnectionId(
        &config, connection_.connection_id());
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);

  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<TaggingEncrypterWithConfidentialityLimit>(
          ENCRYPTION_FORWARD_SECURE, kConfidentialityLimit));
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  connection_.OnHandshakeComplete();

  QuicPacketNumber last_packet;
  // Send 3 packets without receiving acks for any of them. Key update will not
  // be allowed, so the confidentiality limit should be reached, forcing the
  // connection to be closed.
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, &last_packet);
  EXPECT_TRUE(connection_.connected());
  SendStreamDataToPeer(2, "foo", 0, NO_FIN, &last_packet);
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
  SendStreamDataToPeer(3, "foo", 0, NO_FIN, &last_packet);
  EXPECT_FALSE(connection_.connected());
  const QuicConnectionStats& stats = connection_.GetStats();
  EXPECT_EQ(0U, stats.key_update_count);
  TestConnectionCloseQuicErrorCode(QUIC_AEAD_LIMIT_REACHED);
}

TEST_P(QuicConnectionTest, CloseConnectionOnIntegrityLimitDuringHandshake) {
  if (!connection_.version().UsesTls()) {
    return;
  }

  constexpr uint8_t correct_tag = ENCRYPTION_HANDSHAKE;
  constexpr uint8_t wrong_tag = 0xFE;
  constexpr QuicPacketCount kIntegrityLimit = 3;

  SetDecrypter(ENCRYPTION_HANDSHAKE,
               std::make_unique<StrictTaggingDecrypterWithIntegrityLimit>(
                   correct_tag, kIntegrityLimit));
  connection_.SetEncrypter(ENCRYPTION_HANDSHAKE,
                           std::make_unique<TaggingEncrypter>(correct_tag));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
  peer_framer_.SetEncrypter(ENCRYPTION_HANDSHAKE,
                            std::make_unique<TaggingEncrypter>(wrong_tag));
  for (uint64_t i = 1; i <= kIntegrityLimit; ++i) {
    EXPECT_TRUE(connection_.connected());
    if (i == kIntegrityLimit) {
      EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
      EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(AnyNumber());
    }
    ProcessDataPacketAtLevel(i, !kHasStopWaiting, ENCRYPTION_HANDSHAKE);
    EXPECT_EQ(
        i, connection_.GetStats().num_failed_authentication_packets_received);
  }
  EXPECT_FALSE(connection_.connected());
  TestConnectionCloseQuicErrorCode(QUIC_AEAD_LIMIT_REACHED);
}

TEST_P(QuicConnectionTest, CloseConnectionOnIntegrityLimitAfterHandshake) {
  if (!connection_.version().UsesTls()) {
    return;
  }

  constexpr uint8_t correct_tag = ENCRYPTION_FORWARD_SECURE;
  constexpr uint8_t wrong_tag = 0xFE;
  constexpr QuicPacketCount kIntegrityLimit = 3;

  SetDecrypter(ENCRYPTION_FORWARD_SECURE,
               std::make_unique<StrictTaggingDecrypterWithIntegrityLimit>(
                   correct_tag, kIntegrityLimit));
  connection_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<TaggingEncrypter>(correct_tag));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  connection_.OnHandshakeComplete();
  connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
  peer_framer_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                            std::make_unique<TaggingEncrypter>(wrong_tag));
  for (uint64_t i = 1; i <= kIntegrityLimit; ++i) {
    EXPECT_TRUE(connection_.connected());
    if (i == kIntegrityLimit) {
      EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
    }
    ProcessDataPacketAtLevel(i, !kHasStopWaiting, ENCRYPTION_FORWARD_SECURE);
    EXPECT_EQ(
        i, connection_.GetStats().num_failed_authentication_packets_received);
  }
  EXPECT_FALSE(connection_.connected());
  TestConnectionCloseQuicErrorCode(QUIC_AEAD_LIMIT_REACHED);
}

TEST_P(QuicConnectionTest,
       CloseConnectionOnIntegrityLimitAcrossEncryptionLevels) {
  if (!connection_.version().UsesTls()) {
    return;
  }

  uint8_t correct_tag = ENCRYPTION_HANDSHAKE;
  constexpr uint8_t wrong_tag = 0xFE;
  constexpr QuicPacketCount kIntegrityLimit = 4;

  SetDecrypter(ENCRYPTION_HANDSHAKE,
               std::make_unique<StrictTaggingDecrypterWithIntegrityLimit>(
                   correct_tag, kIntegrityLimit));
  connection_.SetEncrypter(ENCRYPTION_HANDSHAKE,
                           std::make_unique<TaggingEncrypter>(correct_tag));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
  peer_framer_.SetEncrypter(ENCRYPTION_HANDSHAKE,
                            std::make_unique<TaggingEncrypter>(wrong_tag));
  for (uint64_t i = 1; i <= 2; ++i) {
    EXPECT_TRUE(connection_.connected());
    ProcessDataPacketAtLevel(i, !kHasStopWaiting, ENCRYPTION_HANDSHAKE);
    EXPECT_EQ(
        i, connection_.GetStats().num_failed_authentication_packets_received);
  }

  correct_tag = ENCRYPTION_FORWARD_SECURE;
  SetDecrypter(ENCRYPTION_FORWARD_SECURE,
               std::make_unique<StrictTaggingDecrypterWithIntegrityLimit>(
                   correct_tag, kIntegrityLimit));
  connection_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<TaggingEncrypter>(correct_tag));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  connection_.OnHandshakeComplete();
  connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
  connection_.RemoveEncrypter(ENCRYPTION_HANDSHAKE);
  peer_framer_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                            std::make_unique<TaggingEncrypter>(wrong_tag));
  for (uint64_t i = 3; i <= kIntegrityLimit; ++i) {
    EXPECT_TRUE(connection_.connected());
    if (i == kIntegrityLimit) {
      EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
    }
    ProcessDataPacketAtLevel(i, !kHasStopWaiting, ENCRYPTION_FORWARD_SECURE);
    EXPECT_EQ(
        i, connection_.GetStats().num_failed_authentication_packets_received);
  }
  EXPECT_FALSE(connection_.connected());
  TestConnectionCloseQuicErrorCode(QUIC_AEAD_LIMIT_REACHED);
}

TEST_P(QuicConnectionTest, IntegrityLimitDoesNotApplyWithoutDecryptionKey) {
  if (!connection_.version().UsesTls()) {
    return;
  }

  constexpr uint8_t correct_tag = ENCRYPTION_HANDSHAKE;
  constexpr uint8_t wrong_tag = 0xFE;
  constexpr QuicPacketCount kIntegrityLimit = 3;

  SetDecrypter(ENCRYPTION_HANDSHAKE,
               std::make_unique<StrictTaggingDecrypterWithIntegrityLimit>(
                   correct_tag, kIntegrityLimit));
  connection_.SetEncrypter(ENCRYPTION_HANDSHAKE,
                           std::make_unique<TaggingEncrypter>(correct_tag));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
  connection_.RemoveDecrypter(ENCRYPTION_FORWARD_SECURE);

  peer_framer_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                            std::make_unique<TaggingEncrypter>(wrong_tag));
  for (uint64_t i = 1; i <= kIntegrityLimit * 2; ++i) {
    EXPECT_TRUE(connection_.connected());
    ProcessDataPacketAtLevel(i, !kHasStopWaiting, ENCRYPTION_FORWARD_SECURE);
    EXPECT_EQ(
        0u, connection_.GetStats().num_failed_authentication_packets_received);
  }
  EXPECT_TRUE(connection_.connected());
}

TEST_P(QuicConnectionTest, CloseConnectionOnIntegrityLimitAcrossKeyPhases) {
  if (!connection_.version().UsesTls()) {
    return;
  }

  constexpr QuicPacketCount kIntegrityLimit = 4;

  TransportParameters params;
  QuicConfig config;
  std::string error_details;
  EXPECT_THAT(config.ProcessTransportParameters(
                  params, /* is_resumption = */ false, &error_details),
              IsQuicNoError());
  QuicConfigPeer::SetNegotiated(&config, true);
  if (connection_.version().UsesTls()) {
    QuicConfigPeer::SetReceivedOriginalConnectionId(
        &config, connection_.connection_id());
    QuicConfigPeer::SetReceivedInitialSourceConnectionId(
        &config, connection_.connection_id());
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);

  MockFramerVisitor peer_framer_visitor_;
  peer_framer_.set_visitor(&peer_framer_visitor_);

  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<TaggingEncrypter>(0x01));
  SetDecrypter(ENCRYPTION_FORWARD_SECURE,
               std::make_unique<StrictTaggingDecrypterWithIntegrityLimit>(
                   ENCRYPTION_FORWARD_SECURE, kIntegrityLimit));
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  connection_.OnHandshakeComplete();
  connection_.RemoveEncrypter(ENCRYPTION_INITIAL);

  peer_framer_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                            std::make_unique<TaggingEncrypter>(0xFF));
  for (uint64_t i = 1; i <= 2; ++i) {
    EXPECT_TRUE(connection_.connected());
    ProcessDataPacketAtLevel(i, !kHasStopWaiting, ENCRYPTION_FORWARD_SECURE);
    EXPECT_EQ(
        i, connection_.GetStats().num_failed_authentication_packets_received);
  }

  peer_framer_.SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
  // Send packet 1.
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, &last_packet);
  EXPECT_EQ(QuicPacketNumber(1u), last_packet);
  // Receive ack for packet 1.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame1 = InitAckFrame(1);
  ProcessAckPacket(&frame1);
  // Key update should now be allowed, initiate it.
  EXPECT_CALL(visitor_, AdvanceKeysAndCreateCurrentOneRttDecrypter())
      .WillOnce([kIntegrityLimit]() {
        return std::make_unique<StrictTaggingDecrypterWithIntegrityLimit>(
            0x02, kIntegrityLimit);
      });
  EXPECT_CALL(visitor_, CreateCurrentOneRttEncrypter()).WillOnce([]() {
    return std::make_unique<TaggingEncrypter>(0x02);
  });
  EXPECT_CALL(visitor_, OnKeyUpdate(KeyUpdateReason::kLocalForTests));
  EXPECT_TRUE(connection_.InitiateKeyUpdate(KeyUpdateReason::kLocalForTests));

  // Pretend that peer accepts the key update.
  EXPECT_CALL(peer_framer_visitor_,
              AdvanceKeysAndCreateCurrentOneRttDecrypter())
      .WillOnce(
          []() { return std::make_unique<StrictTaggingDecrypter>(0x02); });
  EXPECT_CALL(peer_framer_visitor_, CreateCurrentOneRttEncrypter())
      .WillOnce([]() { return std::make_unique<TaggingEncrypter>(0x02); });
  peer_framer_.SetKeyUpdateSupportForConnection(true);
  peer_framer_.DoKeyUpdate(KeyUpdateReason::kLocalForTests);

  // Send packet 2.
  SendStreamDataToPeer(2, "bar", 0, NO_FIN, &last_packet);
  EXPECT_EQ(QuicPacketNumber(2u), last_packet);
  // Receive ack for packet 2.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _, _, _));
  QuicAckFrame frame2 = InitAckFrame(2);
  ProcessAckPacket(&frame2);

  EXPECT_EQ(2u,
            connection_.GetStats().num_failed_authentication_packets_received);

  // Do two more undecryptable packets. Integrity limit should be reached.
  peer_framer_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                            std::make_unique<TaggingEncrypter>(0xFF));
  for (uint64_t i = 3; i <= kIntegrityLimit; ++i) {
    EXPECT_TRUE(connection_.connected());
    if (i == kIntegrityLimit) {
      EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
    }
    ProcessDataPacketAtLevel(i, !kHasStopWaiting, ENCRYPTION_FORWARD_SECURE);
    EXPECT_EQ(
        i, connection_.GetStats().num_failed_authentication_packets_received);
  }
  EXPECT_FALSE(connection_.connected());
  TestConnectionCloseQuicErrorCode(QUIC_AEAD_LIMIT_REACHED);
}

TEST_P(QuicConnectionTest, SendAckFrequencyFrame) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  SetQuicReloadableFlag(quic_can_send_ack_frequency, true);
  set_perspective(Perspective::IS_SERVER);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AnyNumber());

  QuicConfig config;
  QuicConfigPeer::SetReceivedMinAckDelayMs(&config, /*min_ack_delay_ms=*/1);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  QuicConnectionPeer::SetAddressValidated(&connection_);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  peer_creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);

  connection_.OnHandshakeComplete();

  writer_->SetWritable();
  QuicPacketCreatorPeer::SetPacketNumber(creator_, 99);
  // Send packet 100
  SendStreamDataToPeer(/*id=*/1, "foo", /*offset=*/0, NO_FIN, nullptr);

  QuicAckFrequencyFrame captured_frame;
  EXPECT_CALL(visitor_, SendAckFrequency(_))
      .WillOnce(Invoke([&captured_frame](const QuicAckFrequencyFrame& frame) {
        captured_frame = frame;
      }));
  // Send packet 101.
  SendStreamDataToPeer(/*id=*/1, "bar", /*offset=*/3, NO_FIN, nullptr);

  EXPECT_EQ(captured_frame.packet_tolerance, 10u);
  EXPECT_EQ(captured_frame.max_ack_delay,
            QuicTime::Delta::FromMilliseconds(GetDefaultDelayedAckTimeMs()));

  // Sending packet 102 does not trigger sending another AckFrequencyFrame.
  SendStreamDataToPeer(/*id=*/1, "baz", /*offset=*/6, NO_FIN, nullptr);
}

TEST_P(QuicConnectionTest, SendAckFrequencyFrameUponHandshakeCompletion) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  SetQuicReloadableFlag(quic_can_send_ack_frequency, true);
  set_perspective(Perspective::IS_SERVER);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AnyNumber());

  QuicConfig config;
  QuicConfigPeer::SetReceivedMinAckDelayMs(&config, /*min_ack_delay_ms=*/1);
  QuicTagVector quic_tag_vector;
  // Enable sending AckFrequency upon handshake completion.
  quic_tag_vector.push_back(kAFF2);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, quic_tag_vector);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  QuicConnectionPeer::SetAddressValidated(&connection_);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  peer_creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);

  QuicAckFrequencyFrame captured_frame;
  EXPECT_CALL(visitor_, SendAckFrequency(_))
      .WillOnce(Invoke([&captured_frame](const QuicAckFrequencyFrame& frame) {
        captured_frame = frame;
      }));

  connection_.OnHandshakeComplete();

  EXPECT_EQ(captured_frame.packet_tolerance, 2u);
  EXPECT_EQ(captured_frame.max_ack_delay,
            QuicTime::Delta::FromMilliseconds(GetDefaultDelayedAckTimeMs()));
}

TEST_P(QuicConnectionTest, FastRecoveryOfLostServerHello) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  connection_.SetFromConfig(config);

  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  connection_.SendCryptoStreamData();
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(20));

  // Assume ServerHello gets lost.
  peer_framer_.SetEncrypter(ENCRYPTION_HANDSHAKE,
                            std::make_unique<TaggingEncrypter>(0x02));
  ProcessCryptoPacketAtLevel(2, ENCRYPTION_HANDSHAKE);
  ASSERT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  // Shorten PTO for fast recovery from lost ServerHello.
  EXPECT_EQ(clock_.ApproximateNow() + kAlarmGranularity,
            connection_.GetRetransmissionAlarm()->deadline());
}

TEST_P(QuicConnectionTest, ServerHelloGetsReordered) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  connection_.SetFromConfig(config);
  EXPECT_CALL(visitor_, OnCryptoFrame(_))
      .WillRepeatedly(Invoke([=, this](const QuicCryptoFrame& frame) {
        if (frame.level == ENCRYPTION_INITIAL) {
          // Install handshake read keys.
          SetDecrypter(
              ENCRYPTION_HANDSHAKE,
              std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_HANDSHAKE));
          connection_.SetEncrypter(
              ENCRYPTION_HANDSHAKE,
              std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
          connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
        }
      }));

  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  connection_.SendCryptoStreamData();
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(20));

  // Assume ServerHello gets reordered.
  peer_framer_.SetEncrypter(ENCRYPTION_HANDSHAKE,
                            std::make_unique<TaggingEncrypter>(0x02));
  ProcessCryptoPacketAtLevel(2, ENCRYPTION_HANDSHAKE);
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_INITIAL);
  // Verify fast recovery is not enabled.
  EXPECT_EQ(connection_.sent_packet_manager().GetRetransmissionTime(),
            connection_.GetRetransmissionAlarm()->deadline());
}

TEST_P(QuicConnectionTest, MigratePath) {
  connection_.CreateConnectionIdManager();
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.OnHandshakeComplete();
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  EXPECT_CALL(visitor_, OnPathDegrading());
  connection_.OnPathDegradingDetected();
  const QuicSocketAddress kNewSelfAddress(QuicIpAddress::Any4(), 12345);
  EXPECT_NE(kNewSelfAddress, connection_.self_address());

  // Buffer a packet.
  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(1);
  writer_->SetWriteBlocked();
  connection_.SendMtuDiscoveryPacket(kMaxOutgoingPacketSize);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  if (version().HasIetfQuicFrames()) {
    QuicNewConnectionIdFrame frame;
    frame.connection_id = TestConnectionId(1234);
    ASSERT_NE(frame.connection_id, connection_.connection_id());
    frame.stateless_reset_token =
        QuicUtils::GenerateStatelessResetToken(frame.connection_id);
    frame.retire_prior_to = 0u;
    frame.sequence_number = 1u;
    connection_.OnNewConnectionIdFrame(frame);
  }

  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  EXPECT_CALL(visitor_, OnForwardProgressMadeAfterPathDegrading());
  EXPECT_TRUE(connection_.MigratePath(kNewSelfAddress,
                                      connection_.peer_address(), &new_writer,
                                      /*owns_writer=*/false));

  EXPECT_EQ(kNewSelfAddress, connection_.self_address());
  EXPECT_EQ(&new_writer, QuicConnectionPeer::GetWriter(&connection_));
  EXPECT_FALSE(connection_.IsPathDegrading());
  // Buffered packet on the old path should be discarded.
  if (version().HasIetfQuicFrames()) {
    EXPECT_EQ(0u, connection_.NumQueuedPackets());
  } else {
    EXPECT_EQ(1u, connection_.NumQueuedPackets());
  }
}

TEST_P(QuicConnectionTest, MigrateToNewPathDuringProbing) {
  if (!VersionHasIetfQuicFrames(connection_.version().transport_version)) {
    return;
  }
  PathProbeTestInit(Perspective::IS_CLIENT);
  const QuicSocketAddress kNewSelfAddress(QuicIpAddress::Any4(), 12345);
  EXPECT_NE(kNewSelfAddress, connection_.self_address());
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  bool success = false;
  connection_.ValidatePath(
      std::make_unique<TestQuicPathValidationContext>(
          kNewSelfAddress, connection_.peer_address(), &new_writer),
      std::make_unique<TestValidationResultDelegate>(
          &connection_, kNewSelfAddress, connection_.peer_address(), &success),
      PathValidationReason::kReasonUnknown);
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));

  connection_.MigratePath(kNewSelfAddress, connection_.peer_address(),
                          &new_writer, /*owns_writer=*/false);
  EXPECT_EQ(kNewSelfAddress, connection_.self_address());
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  EXPECT_FALSE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));
}

TEST_P(QuicConnectionTest, MultiPortConnection) {
  set_perspective(Perspective::IS_CLIENT);
  QuicConfig config;
  config.SetClientConnectionOptions(QuicTagVector{kMPQC});
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  connection_.CreateConnectionIdManager();
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.OnHandshakeComplete();

  EXPECT_CALL(visitor_, OnPathDegrading());
  connection_.OnPathDegradingDetected();

  auto self_address = connection_.self_address();
  const QuicSocketAddress kNewSelfAddress(self_address.host(),
                                          self_address.port() + 1);
  EXPECT_NE(kNewSelfAddress, self_address);
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);

  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive()).WillOnce(Return(false));
  QuicNewConnectionIdFrame frame;
  frame.connection_id = TestConnectionId(1234);
  ASSERT_NE(frame.connection_id, connection_.connection_id());
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 0u;
  frame.sequence_number = 1u;
  EXPECT_CALL(visitor_, CreateContextForMultiPortPath)
      .WillRepeatedly(testing::WithArgs<0>([&](auto&& observer) {
        observer->OnMultiPortPathContextAvailable(
            std::move(std::make_unique<TestQuicPathValidationContext>(
                kNewSelfAddress, connection_.peer_address(), &new_writer)));
      }));
  connection_.OnNewConnectionIdFrame(frame);
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));
  auto* alt_path = QuicConnectionPeer::GetAlternativePath(&connection_);
  EXPECT_FALSE(alt_path->validated);
  EXPECT_EQ(PathValidationReason::kMultiPort,
            QuicConnectionPeer::path_validator(&connection_)
                ->GetPathValidationReason());

  // Suppose the server retransmits the NEW_CID frame, the client will receive
  // the same frame again. It should be ignored.
  // Regression test of crbug.com/1406762
  connection_.OnNewConnectionIdFrame(frame);

  // 30ms RTT.
  const QuicTime::Delta kTestRTT = QuicTime::Delta::FromMilliseconds(30);
  // Fake a response delay.
  clock_.AdvanceTime(kTestRTT);

  QuicFrames frames;
  frames.push_back(QuicFrame(QuicPathResponseFrame(
      99, new_writer.path_challenge_frames().back().data_buffer)));
  ProcessFramesPacketWithAddresses(frames, kNewSelfAddress, kPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  // No migration should happen and the alternative path should still be alive.
  EXPECT_FALSE(connection_.HasPendingPathValidation());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));
  EXPECT_TRUE(alt_path->validated);
  auto stats = connection_.multi_port_stats();
  EXPECT_EQ(1, connection_.GetStats().num_path_degrading);
  EXPECT_EQ(1, stats->num_successful_probes);
  EXPECT_EQ(1, stats->num_client_probing_attempts);
  EXPECT_EQ(1, connection_.GetStats().num_client_probing_attempts);
  EXPECT_EQ(0, stats->num_multi_port_probe_failures_when_path_degrading);
  EXPECT_EQ(kTestRTT, stats->rtt_stats.latest_rtt());
  EXPECT_EQ(kTestRTT,
            stats->rtt_stats_when_default_path_degrading.latest_rtt());

  // Receiving the retransmitted NEW_CID frame now should still have no effect.
  EXPECT_CALL(visitor_, CreateContextForMultiPortPath).Times(0);
  connection_.OnNewConnectionIdFrame(frame);

  // When there's no active request, the probing shouldn't happen. But the
  // probing context should be saved.
  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive()).WillOnce(Return(false));
  connection_.GetMultiPortProbingAlarm()->Fire();
  EXPECT_FALSE(connection_.HasPendingPathValidation());
  EXPECT_FALSE(connection_.GetMultiPortProbingAlarm()->IsSet());

  // Simulate the situation where a new request stream is created.
  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));
  random_generator_.ChangeValue();
  connection_.MaybeProbeMultiPortPath();
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));
  EXPECT_TRUE(alt_path->validated);
  // Fake a response delay.
  clock_.AdvanceTime(kTestRTT);
  QuicFrames frames2;
  frames2.push_back(QuicFrame(QuicPathResponseFrame(
      99, new_writer.path_challenge_frames().back().data_buffer)));
  ProcessFramesPacketWithAddresses(frames2, kNewSelfAddress, kPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  // No migration should happen and the alternative path should still be alive.
  EXPECT_FALSE(connection_.HasPendingPathValidation());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));
  EXPECT_TRUE(alt_path->validated);
  EXPECT_EQ(1, connection_.GetStats().num_path_degrading);
  EXPECT_EQ(0, stats->num_multi_port_probe_failures_when_path_degrading);
  EXPECT_EQ(kTestRTT, stats->rtt_stats.latest_rtt());
  EXPECT_EQ(kTestRTT,
            stats->rtt_stats_when_default_path_degrading.latest_rtt());

  EXPECT_CALL(visitor_, OnForwardProgressMadeAfterPathDegrading());
  QuicConnectionPeer::OnForwardProgressMade(&connection_);

  EXPECT_TRUE(connection_.GetMultiPortProbingAlarm()->IsSet());
  // Since there's already a scheduled probing alarm, manual calls won't have
  // any effect.
  connection_.MaybeProbeMultiPortPath();
  EXPECT_FALSE(connection_.HasPendingPathValidation());

  // Since kMPQM is not set, migration shouldn't happen
  EXPECT_CALL(visitor_, OnPathDegrading());
  EXPECT_CALL(visitor_, MigrateToMultiPortPath(_)).Times(0);
  connection_.OnPathDegradingDetected();
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));

  // Simulate the case where the path validation fails after retries.
  connection_.GetMultiPortProbingAlarm()->Fire();
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));
  for (size_t i = 0; i < QuicPathValidator::kMaxRetryTimes + 1; ++i) {
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(3 * kInitialRttMs));
    static_cast<TestAlarmFactory::TestAlarm*>(
        QuicPathValidatorPeer::retry_timer(
            QuicConnectionPeer::path_validator(&connection_)))
        ->Fire();
  }

  EXPECT_FALSE(connection_.HasPendingPathValidation());
  EXPECT_FALSE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));
  EXPECT_EQ(2, connection_.GetStats().num_path_degrading);
  EXPECT_EQ(1, stats->num_multi_port_probe_failures_when_path_degrading);
  EXPECT_EQ(0, stats->num_multi_port_probe_failures_when_path_not_degrading);
  EXPECT_EQ(0, connection_.GetStats().num_stateless_resets_on_alternate_path);
}

TEST_P(QuicConnectionTest, TooManyMultiPortPathCreations) {
  set_perspective(Perspective::IS_CLIENT);
  QuicConfig config;
  config.SetClientConnectionOptions(QuicTagVector{kMPQC});
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  connection_.CreateConnectionIdManager();
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.OnHandshakeComplete();

  EXPECT_CALL(visitor_, OnPathDegrading());
  connection_.OnPathDegradingDetected();

  auto self_address = connection_.self_address();
  const QuicSocketAddress kNewSelfAddress(self_address.host(),
                                          self_address.port() + 1);
  EXPECT_NE(kNewSelfAddress, self_address);
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);

  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));

  {
    QuicNewConnectionIdFrame frame;
    frame.connection_id = TestConnectionId(1234);
    ASSERT_NE(frame.connection_id, connection_.connection_id());
    frame.stateless_reset_token =
        QuicUtils::GenerateStatelessResetToken(frame.connection_id);
    frame.retire_prior_to = 0u;
    frame.sequence_number = 1u;
    EXPECT_CALL(visitor_, CreateContextForMultiPortPath)
        .WillRepeatedly(testing::WithArgs<0>([&](auto&& observer) {
          observer->OnMultiPortPathContextAvailable(
              std::move(std::make_unique<TestQuicPathValidationContext>(
                  kNewSelfAddress, connection_.peer_address(), &new_writer)));
        }));
    EXPECT_TRUE(connection_.OnNewConnectionIdFrame(frame));
  }
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));
  auto* alt_path = QuicConnectionPeer::GetAlternativePath(&connection_);
  EXPECT_FALSE(alt_path->validated);

  EXPECT_TRUE(connection_.HasPendingPathValidation());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));
  for (size_t i = 0; i < QuicPathValidator::kMaxRetryTimes + 1; ++i) {
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(3 * kInitialRttMs));
    static_cast<TestAlarmFactory::TestAlarm*>(
        QuicPathValidatorPeer::retry_timer(
            QuicConnectionPeer::path_validator(&connection_)))
        ->Fire();
  }

  auto stats = connection_.multi_port_stats();
  EXPECT_FALSE(connection_.HasPendingPathValidation());
  EXPECT_FALSE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));
  EXPECT_EQ(1, connection_.GetStats().num_path_degrading);
  EXPECT_EQ(1, stats->num_multi_port_probe_failures_when_path_degrading);

  uint64_t connection_id = 1235;
  for (size_t i = 0; i < kMaxNumMultiPortPaths - 1; ++i) {
    QuicNewConnectionIdFrame frame;
    frame.connection_id = TestConnectionId(connection_id + i);
    ASSERT_NE(frame.connection_id, connection_.connection_id());
    frame.stateless_reset_token =
        QuicUtils::GenerateStatelessResetToken(frame.connection_id);
    frame.retire_prior_to = 0u;
    frame.sequence_number = i + 2;
    EXPECT_CALL(visitor_, CreateContextForMultiPortPath)
        .WillRepeatedly(testing::WithArgs<0>([&](auto&& observer) {
          observer->OnMultiPortPathContextAvailable(
              std::move(std::make_unique<TestQuicPathValidationContext>(
                  kNewSelfAddress, connection_.peer_address(), &new_writer)));
        }));
    EXPECT_TRUE(connection_.OnNewConnectionIdFrame(frame));
    EXPECT_TRUE(connection_.HasPendingPathValidation());
    EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
        &connection_, kNewSelfAddress, connection_.peer_address()));
    EXPECT_FALSE(alt_path->validated);

    for (size_t j = 0; j < QuicPathValidator::kMaxRetryTimes + 1; ++j) {
      clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(3 * kInitialRttMs));
      static_cast<TestAlarmFactory::TestAlarm*>(
          QuicPathValidatorPeer::retry_timer(
              QuicConnectionPeer::path_validator(&connection_)))
          ->Fire();
    }

    EXPECT_FALSE(connection_.HasPendingPathValidation());
    EXPECT_FALSE(QuicConnectionPeer::IsAlternativePath(
        &connection_, kNewSelfAddress, connection_.peer_address()));
    EXPECT_EQ(1, connection_.GetStats().num_path_degrading);
    EXPECT_EQ(i + 2, stats->num_multi_port_probe_failures_when_path_degrading);
  }

  // The 6th attemp should fail.
  QuicNewConnectionIdFrame frame2;
  frame2.connection_id = TestConnectionId(1239);
  ASSERT_NE(frame2.connection_id, connection_.connection_id());
  frame2.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame2.connection_id);
  frame2.retire_prior_to = 0u;
  frame2.sequence_number = 6u;
  EXPECT_TRUE(connection_.OnNewConnectionIdFrame(frame2));
  EXPECT_FALSE(connection_.HasPendingPathValidation());
  EXPECT_EQ(kMaxNumMultiPortPaths,
            stats->num_multi_port_probe_failures_when_path_degrading);
}

TEST_P(QuicConnectionTest, MultiPortPathReceivesStatelessReset) {
  set_perspective(Perspective::IS_CLIENT);
  QuicConfig config;
  QuicConfigPeer::SetReceivedStatelessResetToken(&config,
                                                 kTestStatelessResetToken);
  config.SetClientConnectionOptions(QuicTagVector{kMPQC});
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  connection_.CreateConnectionIdManager();
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.OnHandshakeComplete();

  EXPECT_CALL(visitor_, OnPathDegrading());
  connection_.OnPathDegradingDetected();

  auto self_address = connection_.self_address();
  const QuicSocketAddress kNewSelfAddress(self_address.host(),
                                          self_address.port() + 1);
  EXPECT_NE(kNewSelfAddress, self_address);
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);

  QuicNewConnectionIdFrame frame;
  frame.connection_id = TestConnectionId(1234);
  ASSERT_NE(frame.connection_id, connection_.connection_id());
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 0u;
  frame.sequence_number = 1u;
  EXPECT_CALL(visitor_, CreateContextForMultiPortPath)
      .WillRepeatedly(testing::WithArgs<0>([&](auto&& observer) {
        observer->OnMultiPortPathContextAvailable(
            std::move(std::make_unique<TestQuicPathValidationContext>(
                kNewSelfAddress, connection_.peer_address(), &new_writer)));
      }));
  connection_.OnNewConnectionIdFrame(frame);
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));
  auto* alt_path = QuicConnectionPeer::GetAlternativePath(&connection_);
  EXPECT_FALSE(alt_path->validated);
  EXPECT_EQ(PathValidationReason::kMultiPort,
            QuicConnectionPeer::path_validator(&connection_)
                ->GetPathValidationReason());

  std::unique_ptr<QuicEncryptedPacket> packet(
      QuicFramer::BuildIetfStatelessResetPacket(connection_id_,
                                                /*received_packet_length=*/100,
                                                kTestStatelessResetToken));
  std::unique_ptr<QuicReceivedPacket> received(
      ConstructReceivedPacket(*packet, QuicTime::Zero()));
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_PEER))
      .Times(0);
  connection_.ProcessUdpPacket(kNewSelfAddress, kPeerAddress, *received);
  EXPECT_EQ(connection_.GetStats().num_client_probing_attempts, 1);
  EXPECT_EQ(connection_.GetStats().num_stateless_resets_on_alternate_path, 1);
}

// Test that if the client's active migration is disabled, multi-port will not
// be attempted.
TEST_P(QuicConnectionTest, MultiPortPathRespectsActiveMigrationConfig) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  set_perspective(Perspective::IS_CLIENT);
  QuicConfig config;
  QuicConfigPeer::SetReceivedStatelessResetToken(&config,
                                                 kTestStatelessResetToken);
  QuicConfigPeer::SetReceivedDisableConnectionMigration(&config);
  config.SetClientConnectionOptions(QuicTagVector{kMPQC});
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  connection_.CreateConnectionIdManager();
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.OnHandshakeComplete();

  EXPECT_CALL(visitor_, OnPathDegrading());
  connection_.OnPathDegradingDetected();

  QuicNewConnectionIdFrame frame;
  frame.connection_id = TestConnectionId(1234);
  ASSERT_NE(frame.connection_id, connection_.connection_id());
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 0u;
  frame.sequence_number = 1u;
  EXPECT_CALL(visitor_, CreateContextForMultiPortPath).Times(0);
  connection_.OnNewConnectionIdFrame(frame);
  EXPECT_FALSE(connection_.HasPendingPathValidation());
}

// Verify that when multi-port is enabled and path degrading is triggered, if
// the alt-path is not ready, nothing happens.
TEST_P(QuicConnectionTest, PathDegradingWhenAltPathIsNotReady) {
  set_perspective(Perspective::IS_CLIENT);
  QuicConfig config;
  config.SetClientConnectionOptions(QuicTagVector{kMPQC});
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  connection_.CreateConnectionIdManager();
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.OnHandshakeComplete();

  auto self_address = connection_.self_address();
  const QuicSocketAddress kNewSelfAddress(self_address.host(),
                                          self_address.port() + 1);
  EXPECT_NE(kNewSelfAddress, self_address);
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);

  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));

  QuicNewConnectionIdFrame frame;
  frame.connection_id = TestConnectionId(1234);
  ASSERT_NE(frame.connection_id, connection_.connection_id());
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 0u;
  frame.sequence_number = 1u;
  EXPECT_CALL(visitor_, CreateContextForMultiPortPath)
      .WillRepeatedly(testing::WithArgs<0>([&](auto&& observer) {
        observer->OnMultiPortPathContextAvailable(
            std::move(std::make_unique<TestQuicPathValidationContext>(
                kNewSelfAddress, connection_.peer_address(), &new_writer)));
      }));
  EXPECT_TRUE(connection_.OnNewConnectionIdFrame(frame));
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));
  auto* alt_path = QuicConnectionPeer::GetAlternativePath(&connection_);
  EXPECT_FALSE(alt_path->validated);

  // The alt path is not ready, path degrading doesn't do anything.
  EXPECT_CALL(visitor_, OnPathDegrading());
  EXPECT_CALL(visitor_, MigrateToMultiPortPath(_)).Times(0);
  connection_.OnPathDegradingDetected();

  // 30ms RTT.
  const QuicTime::Delta kTestRTT = QuicTime::Delta::FromMilliseconds(30);
  // Fake a response delay.
  clock_.AdvanceTime(kTestRTT);

  // Even if the alt path is validated after path degrading, nothing should
  // happen.
  QuicFrames frames;
  frames.push_back(QuicFrame(QuicPathResponseFrame(
      99, new_writer.path_challenge_frames().back().data_buffer)));
  ProcessFramesPacketWithAddresses(frames, kNewSelfAddress, kPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  // No migration should happen and the alternative path should still be alive.
  EXPECT_FALSE(connection_.HasPendingPathValidation());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));
  EXPECT_TRUE(alt_path->validated);
}

// Verify that when multi-port is enabled and path degrading is triggered, if
// the alt-path is ready and not probing, it should be migrated.
TEST_P(QuicConnectionTest, PathDegradingWhenAltPathIsReadyAndNotProbing) {
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  set_perspective(Perspective::IS_CLIENT);
  QuicConfig config;
  config.SetClientConnectionOptions(QuicTagVector{kMPQC, kMPQM});
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  connection_.CreateConnectionIdManager();
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.OnHandshakeComplete();

  auto self_address = connection_.self_address();
  const QuicSocketAddress kNewSelfAddress(self_address.host(),
                                          self_address.port() + 1);
  EXPECT_NE(kNewSelfAddress, self_address);
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);

  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));

  QuicNewConnectionIdFrame frame;
  frame.connection_id = TestConnectionId(1234);
  ASSERT_NE(frame.connection_id, connection_.connection_id());
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 0u;
  frame.sequence_number = 1u;
  EXPECT_CALL(visitor_, CreateContextForMultiPortPath)
      .WillRepeatedly(testing::WithArgs<0>([&](auto&& observer) {
        observer->OnMultiPortPathContextAvailable(
            std::move(std::make_unique<TestQuicPathValidationContext>(
                kNewSelfAddress, connection_.peer_address(), &new_writer)));
      }));
  EXPECT_TRUE(connection_.OnNewConnectionIdFrame(frame));
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));
  auto* alt_path = QuicConnectionPeer::GetAlternativePath(&connection_);
  EXPECT_FALSE(alt_path->validated);

  // 30ms RTT.
  const QuicTime::Delta kTestRTT = QuicTime::Delta::FromMilliseconds(30);
  // Fake a response delay.
  clock_.AdvanceTime(kTestRTT);

  // Even if the alt path is validated after path degrading, nothing should
  // happen.
  QuicFrames frames;
  frames.push_back(QuicFrame(QuicPathResponseFrame(
      99, new_writer.path_challenge_frames().back().data_buffer)));
  ProcessFramesPacketWithAddresses(frames, kNewSelfAddress, kPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  // No migration should happen and the alternative path should still be alive.
  EXPECT_FALSE(connection_.HasPendingPathValidation());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));
  EXPECT_TRUE(alt_path->validated);

  // Trigger path degrading and the connection should attempt to migrate.
  EXPECT_CALL(visitor_, OnPathDegrading());
  EXPECT_CALL(visitor_, OnForwardProgressMadeAfterPathDegrading()).Times(0);
  EXPECT_CALL(visitor_, MigrateToMultiPortPath(_))
      .WillOnce(Invoke([&](std::unique_ptr<QuicPathValidationContext> context) {
        EXPECT_EQ(context->self_address(), kNewSelfAddress);
        connection_.MigratePath(context->self_address(),
                                context->peer_address(), context->WriterToUse(),
                                /*owns_writer=*/false);
      }));
  connection_.OnPathDegradingDetected();
}

// Verify that when multi-port is enabled and path degrading is triggered, if
// the alt-path is probing, the probing should be cancelled and the path should
// be migrated.
TEST_P(QuicConnectionTest, PathDegradingWhenAltPathIsReadyAndProbing) {
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  set_perspective(Perspective::IS_CLIENT);
  QuicConfig config;
  config.SetClientConnectionOptions(QuicTagVector{kMPQC, kMPQM});
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  connection_.CreateConnectionIdManager();
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.OnHandshakeComplete();

  auto self_address = connection_.self_address();
  const QuicSocketAddress kNewSelfAddress(self_address.host(),
                                          self_address.port() + 1);
  EXPECT_NE(kNewSelfAddress, self_address);
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);

  EXPECT_CALL(visitor_, ShouldKeepConnectionAlive())
      .WillRepeatedly(Return(true));

  QuicNewConnectionIdFrame frame;
  frame.connection_id = TestConnectionId(1234);
  ASSERT_NE(frame.connection_id, connection_.connection_id());
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 0u;
  frame.sequence_number = 1u;
  EXPECT_CALL(visitor_, CreateContextForMultiPortPath)
      .WillRepeatedly(testing::WithArgs<0>([&](auto&& observer) {
        observer->OnMultiPortPathContextAvailable(
            std::move(std::make_unique<TestQuicPathValidationContext>(
                kNewSelfAddress, connection_.peer_address(), &new_writer)));
      }));
  EXPECT_TRUE(connection_.OnNewConnectionIdFrame(frame));
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));
  auto* alt_path = QuicConnectionPeer::GetAlternativePath(&connection_);
  EXPECT_FALSE(alt_path->validated);

  // 30ms RTT.
  const QuicTime::Delta kTestRTT = QuicTime::Delta::FromMilliseconds(30);
  // Fake a response delay.
  clock_.AdvanceTime(kTestRTT);

  // Even if the alt path is validated after path degrading, nothing should
  // happen.
  QuicFrames frames;
  frames.push_back(QuicFrame(QuicPathResponseFrame(
      99, new_writer.path_challenge_frames().back().data_buffer)));
  ProcessFramesPacketWithAddresses(frames, kNewSelfAddress, kPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  // No migration should happen and the alternative path should still be alive.
  EXPECT_FALSE(connection_.HasPendingPathValidation());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, connection_.peer_address()));
  EXPECT_TRUE(alt_path->validated);

  random_generator_.ChangeValue();
  connection_.GetMultiPortProbingAlarm()->Fire();
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  EXPECT_FALSE(connection_.GetMultiPortProbingAlarm()->IsSet());

  // Trigger path degrading and the connection should attempt to migrate.
  EXPECT_CALL(visitor_, OnPathDegrading());
  EXPECT_CALL(visitor_, OnForwardProgressMadeAfterPathDegrading()).Times(0);
  EXPECT_CALL(visitor_, MigrateToMultiPortPath(_))
      .WillOnce(Invoke([&](std::unique_ptr<QuicPathValidationContext> context) {
        EXPECT_EQ(context->self_address(), kNewSelfAddress);
        connection_.MigratePath(context->self_address(),
                                context->peer_address(), context->WriterToUse(),
                                /*owns_writer=*/false);
      }));
  connection_.OnPathDegradingDetected();
  EXPECT_FALSE(connection_.HasPendingPathValidation());
  auto* path_validator = QuicConnectionPeer::path_validator(&connection_);
  EXPECT_FALSE(QuicPathValidatorPeer::retry_timer(path_validator)->IsSet());
}

TEST_P(QuicConnectionTest, SingleAckInPacket) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
  connection_.NeuterUnencryptedPackets();
  connection_.OnHandshakeComplete();
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_COMPLETE));

  EXPECT_CALL(visitor_, OnStreamFrame(_)).WillOnce(Invoke([=, this]() {
    connection_.SendStreamData3();
    connection_.CloseConnection(
        QUIC_INTERNAL_ERROR, "error",
        ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
  }));
  QuicFrames frames;
  frames.push_back(QuicFrame(frame1_));
  ProcessFramesPacketWithAddresses(frames, kSelfAddress, kPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  ASSERT_FALSE(writer_->ack_frames().empty());
  EXPECT_EQ(1u, writer_->ack_frames().size());
}

TEST_P(QuicConnectionTest,
       ServerReceivedZeroRttPacketAfterOneRttPacketWithRetainedKey) {
  if (!connection_.version().UsesTls()) {
    return;
  }

  set_perspective(Perspective::IS_SERVER);
  SetDecrypter(ENCRYPTION_ZERO_RTT,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_ZERO_RTT));

  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(1, !kHasStopWaiting, ENCRYPTION_ZERO_RTT);

  // Finish handshake.
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  notifier_.NeuterUnencryptedData();
  connection_.NeuterUnencryptedPackets();
  connection_.OnHandshakeComplete();
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_COMPLETE));

  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(4, !kHasStopWaiting, ENCRYPTION_FORWARD_SECURE);
  EXPECT_TRUE(connection_.GetDiscardZeroRttDecryptionKeysAlarm()->IsSet());

  // 0-RTT packet received out of order should be decoded since the decrypter
  // is temporarily retained.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(2, !kHasStopWaiting, ENCRYPTION_ZERO_RTT);
  EXPECT_EQ(
      0u,
      connection_.GetStats()
          .num_tls_server_zero_rtt_packets_received_after_discarding_decrypter);

  // Simulate the timeout for discarding 0-RTT keys passing.
  connection_.GetDiscardZeroRttDecryptionKeysAlarm()->Fire();

  // Another 0-RTT packet received now should not be decoded.
  EXPECT_FALSE(connection_.GetDiscardZeroRttDecryptionKeysAlarm()->IsSet());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(0);
  ProcessDataPacketAtLevel(3, !kHasStopWaiting, ENCRYPTION_ZERO_RTT);
  EXPECT_EQ(
      1u,
      connection_.GetStats()
          .num_tls_server_zero_rtt_packets_received_after_discarding_decrypter);

  // The |discard_zero_rtt_decryption_keys_alarm_| should only be set on the
  // first 1-RTT packet received.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(5, !kHasStopWaiting, ENCRYPTION_FORWARD_SECURE);
  EXPECT_FALSE(connection_.GetDiscardZeroRttDecryptionKeysAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, NewTokenFrameInstigateAcks) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  QuicNewTokenFrame* new_token = new QuicNewTokenFrame();
  EXPECT_CALL(visitor_, OnNewTokenReceived(_));
  ProcessFramePacket(QuicFrame(new_token));

  // Ensure that this has caused the ACK alarm to be set.
  EXPECT_TRUE(connection_.HasPendingAcks());
}

TEST_P(QuicConnectionTest, ServerClosesConnectionOnNewTokenFrame) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  QuicNewTokenFrame* new_token = new QuicNewTokenFrame();
  EXPECT_CALL(visitor_, OnNewTokenReceived(_)).Times(0);
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
  EXPECT_CALL(visitor_, BeforeConnectionCloseSent());
  ProcessFramePacket(QuicFrame(new_token));
  EXPECT_FALSE(connection_.connected());
}

TEST_P(QuicConnectionTest, OverrideRetryTokenWithRetryPacket) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  std::string address_token = "TestAddressToken";
  connection_.SetSourceAddressTokenToSend(address_token);
  EXPECT_EQ(QuicPacketCreatorPeer::GetRetryToken(
                QuicConnectionPeer::GetPacketCreator(&connection_)),
            address_token);
  // Passes valid retry and verify token gets overridden.
  TestClientRetryHandling(/*invalid_retry_tag=*/false,
                          /*missing_original_id_in_config=*/false,
                          /*wrong_original_id_in_config=*/false,
                          /*missing_retry_id_in_config=*/false,
                          /*wrong_retry_id_in_config=*/false);
}

TEST_P(QuicConnectionTest, DonotOverrideRetryTokenWithAddressToken) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  // Passes valid retry and verify token gets overridden.
  TestClientRetryHandling(/*invalid_retry_tag=*/false,
                          /*missing_original_id_in_config=*/false,
                          /*wrong_original_id_in_config=*/false,
                          /*missing_retry_id_in_config=*/false,
                          /*wrong_retry_id_in_config=*/false);
  std::string retry_token = QuicPacketCreatorPeer::GetRetryToken(
      QuicConnectionPeer::GetPacketCreator(&connection_));

  std::string address_token = "TestAddressToken";
  connection_.SetSourceAddressTokenToSend(address_token);
  EXPECT_EQ(QuicPacketCreatorPeer::GetRetryToken(
                QuicConnectionPeer::GetPacketCreator(&connection_)),
            retry_token);
}

TEST_P(QuicConnectionTest,
       ServerReceivedZeroRttWithHigherPacketNumberThanOneRtt) {
  if (!connection_.version().UsesTls()) {
    return;
  }

  // The code that checks for this error piggybacks on some book-keeping state
  // kept for key update, so enable key update for the test.
  std::string error_details;
  TransportParameters params;
  QuicConfig config;
  EXPECT_THAT(config.ProcessTransportParameters(
                  params, /* is_resumption = */ false, &error_details),
              IsQuicNoError());
  QuicConfigPeer::SetNegotiated(&config, true);
  QuicConfigPeer::SetReceivedOriginalConnectionId(&config,
                                                  connection_.connection_id());
  QuicConfigPeer::SetReceivedInitialSourceConnectionId(
      &config, connection_.connection_id());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);

  set_perspective(Perspective::IS_SERVER);
  SetDecrypter(ENCRYPTION_ZERO_RTT,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_ZERO_RTT));

  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(1, !kHasStopWaiting, ENCRYPTION_ZERO_RTT);

  // Finish handshake.
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  notifier_.NeuterUnencryptedData();
  connection_.NeuterUnencryptedPackets();
  connection_.OnHandshakeComplete();
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_COMPLETE));

  // Decrypt a 1-RTT packet.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(2, !kHasStopWaiting, ENCRYPTION_FORWARD_SECURE);
  EXPECT_TRUE(connection_.GetDiscardZeroRttDecryptionKeysAlarm()->IsSet());

  // 0-RTT packet with higher packet number than a 1-RTT packet is invalid and
  // should cause the connection to be closed.
  EXPECT_CALL(visitor_, BeforeConnectionCloseSent());
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
  ProcessDataPacketAtLevel(3, !kHasStopWaiting, ENCRYPTION_ZERO_RTT);
  EXPECT_FALSE(connection_.connected());
  TestConnectionCloseQuicErrorCode(
      QUIC_INVALID_0RTT_PACKET_NUMBER_OUT_OF_ORDER);
}

// Regression test for b/177312785
TEST_P(QuicConnectionTest, PeerMigrateBeforeHandshakeConfirm) {
  if (!VersionHasIetfQuicFrames(version().transport_version)) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  EXPECT_EQ(Perspective::IS_SERVER, connection_.perspective());
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_START));

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is the same as direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());
  EXPECT_FALSE(connection_.effective_peer_address().IsInitialized());

  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kSelfAddress, kPeerAddress,
                                  ENCRYPTION_INITIAL);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  // Process another packet with a different peer address on server side will
  // close connection.
  QuicAckFrame frame = InitAckFrame(1);
  EXPECT_CALL(visitor_, BeforeConnectionCloseSent());
  EXPECT_CALL(visitor_,
              OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0u);

  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _))
      .Times(0);
  ProcessFramePacketWithAddresses(QuicFrame(&frame), kSelfAddress,
                                  kNewPeerAddress, ENCRYPTION_INITIAL);
  EXPECT_FALSE(connection_.connected());
}

// Regresstion test for b/175685916
TEST_P(QuicConnectionTest, TryToFlushAckWithAckQueued) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  SetQuicReloadableFlag(quic_can_send_ack_frequency, true);
  set_perspective(Perspective::IS_SERVER);

  QuicConfig config;
  QuicConfigPeer::SetReceivedMinAckDelayMs(&config, /*min_ack_delay_ms=*/1);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.OnHandshakeComplete();
  QuicPacketCreatorPeer::SetPacketNumber(creator_, 200);

  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(1, !kHasStopWaiting, ENCRYPTION_FORWARD_SECURE);
  // Sending ACK_FREQUENCY bundles ACK. QuicConnectionPeer::SendPing
  // will try to bundle ACK but there is no pending ACK.
  EXPECT_CALL(visitor_, SendAckFrequency(_))
      .WillOnce(Invoke(&notifier_,
                       &SimpleSessionNotifier::WriteOrBufferAckFrequency));
  QuicConnectionPeer::SendPing(&connection_);
}

TEST_P(QuicConnectionTest, PathChallengeBeforePeerIpAddressChangeAtServer) {
  set_perspective(Perspective::IS_SERVER);
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  PathProbeTestInit(Perspective::IS_SERVER);
  SetClientConnectionId(TestConnectionId(1));
  connection_.CreateConnectionIdManager();

  QuicConnectionId server_cid0 = connection_.connection_id();
  QuicConnectionId client_cid0 = connection_.client_connection_id();
  QuicConnectionId client_cid1 = TestConnectionId(2);
  QuicConnectionId server_cid1;
  // Sends new server CID to client.
  if (!connection_.connection_id().IsEmpty()) {
    EXPECT_CALL(connection_id_generator_, GenerateNextConnectionId(_))
        .WillOnce(Return(TestConnectionId(456)));
  }
  EXPECT_CALL(visitor_, MaybeReserveConnectionId(_))
      .WillOnce(Invoke([&](const QuicConnectionId& cid) {
        server_cid1 = cid;
        return true;
      }));
  EXPECT_CALL(visitor_, SendNewConnectionId(_));
  connection_.MaybeSendConnectionIdToClient();
  // Receives new client CID from client.
  QuicNewConnectionIdFrame new_cid_frame;
  new_cid_frame.connection_id = client_cid1;
  new_cid_frame.sequence_number = 1u;
  new_cid_frame.retire_prior_to = 0u;
  connection_.OnNewConnectionIdFrame(new_cid_frame);
  auto* packet_creator = QuicConnectionPeer::GetPacketCreator(&connection_);
  ASSERT_EQ(packet_creator->GetDestinationConnectionId(), client_cid0);
  ASSERT_EQ(packet_creator->GetSourceConnectionId(), server_cid0);

  peer_creator_.SetServerConnectionId(server_cid1);
  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback4(), /*port=*/23456);
  QuicPathFrameBuffer path_challenge_payload{0, 1, 2, 3, 4, 5, 6, 7};
  QuicFrames frames1;
  frames1.push_back(
      QuicFrame(QuicPathChallengeFrame(0, path_challenge_payload)));
  QuicPathFrameBuffer payload;
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, _, _, _, NO_RETRANSMITTABLE_DATA))
      .Times(AtLeast(1))
      .WillOnce(Invoke([&]() {
        EXPECT_EQ(kNewPeerAddress, writer_->last_write_peer_address());
        EXPECT_EQ(kPeerAddress, connection_.peer_address());
        EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
        EXPECT_FALSE(writer_->path_response_frames().empty());
        EXPECT_FALSE(writer_->path_challenge_frames().empty());
        payload = writer_->path_challenge_frames().front().data_buffer;
      }))
      .WillRepeatedly(DoDefault());
  ;
  ProcessFramesPacketWithAddresses(frames1, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  const auto* default_path = QuicConnectionPeer::GetDefaultPath(&connection_);
  const auto* alternative_path =
      QuicConnectionPeer::GetAlternativePath(&connection_);
  EXPECT_EQ(default_path->client_connection_id, client_cid0);
  EXPECT_EQ(default_path->server_connection_id, server_cid0);
  EXPECT_EQ(alternative_path->client_connection_id, client_cid1);
  EXPECT_EQ(alternative_path->server_connection_id, server_cid1);
  EXPECT_EQ(packet_creator->GetDestinationConnectionId(), client_cid0);
  EXPECT_EQ(packet_creator->GetSourceConnectionId(), server_cid0);

  // Process another packet with a different peer address on server side will
  // start connection migration.
  EXPECT_CALL(visitor_, OnConnectionMigration(IPV6_TO_IPV4_CHANGE)).Times(1);
  EXPECT_CALL(visitor_, OnStreamFrame(_)).WillOnce(Invoke([=, this]() {
    EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  }));
  // IETF QUIC send algorithm should be changed to a different object, so no
  // OnPacketSent() called on the old send algorithm.
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, _, _, _, NO_RETRANSMITTABLE_DATA))
      .Times(0);
  QuicFrames frames2;
  frames2.push_back(QuicFrame(frame2_));
  ProcessFramesPacketWithAddresses(frames2, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());
  EXPECT_EQ(IPV6_TO_IPV4_CHANGE,
            connection_.active_effective_peer_migration_type());
  EXPECT_TRUE(writer_->path_challenge_frames().empty());
  EXPECT_NE(connection_.sent_packet_manager().GetSendAlgorithm(),
            send_algorithm_);
  // Switch to use the mock send algorithm.
  send_algorithm_ = new StrictMock<MockSendAlgorithm>();
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(kDefaultTCPMSS));
  EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, BandwidthEstimate())
      .Times(AnyNumber())
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, InSlowStart()).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, InRecovery()).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, PopulateConnectionStats(_)).Times(AnyNumber());
  connection_.SetSendAlgorithm(send_algorithm_);
  EXPECT_EQ(default_path->client_connection_id, client_cid1);
  EXPECT_EQ(default_path->server_connection_id, server_cid1);
  // The previous default path is kept as alternative path before reverse path
  // validation finishes.
  EXPECT_EQ(alternative_path->client_connection_id, client_cid0);
  EXPECT_EQ(alternative_path->server_connection_id, server_cid0);
  EXPECT_EQ(packet_creator->GetDestinationConnectionId(), client_cid1);
  EXPECT_EQ(packet_creator->GetSourceConnectionId(), server_cid1);

  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());
  EXPECT_EQ(IPV6_TO_IPV4_CHANGE,
            connection_.active_effective_peer_migration_type());
  EXPECT_EQ(1u, connection_.GetStats()
                    .num_peer_migration_to_proactively_validated_address);

  // The PATH_CHALLENGE and PATH_RESPONSE is expanded upto the max packet size
  // which may exceeds the anti-amplification limit. Verify server is throttled
  // by anti-amplification limit.
  connection_.SendCryptoDataWithString("foo", 0);
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  // Receiving PATH_RESPONSE should lift the anti-amplification limit.
  QuicFrames frames3;
  frames3.push_back(QuicFrame(QuicPathResponseFrame(99, payload)));
  EXPECT_CALL(visitor_, MaybeSendAddressToken());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(testing::AtLeast(1u));
  ProcessFramesPacketWithAddresses(frames3, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(NO_CHANGE, connection_.active_effective_peer_migration_type());
  // Verify that alternative_path_ is cleared and the peer CID is retired.
  EXPECT_TRUE(alternative_path->client_connection_id.IsEmpty());
  EXPECT_TRUE(alternative_path->server_connection_id.IsEmpty());
  EXPECT_FALSE(alternative_path->stateless_reset_token.has_value());
  auto* retire_peer_issued_cid_alarm =
      connection_.GetRetirePeerIssuedConnectionIdAlarm();
  ASSERT_TRUE(retire_peer_issued_cid_alarm->IsSet());
  EXPECT_CALL(visitor_, SendRetireConnectionId(/*sequence_number=*/0u));
  retire_peer_issued_cid_alarm->Fire();

  // Verify the anti-amplification limit is lifted by sending a packet larger
  // than the anti-amplification limit.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  connection_.SendCryptoDataWithString(std::string(1200, 'a'), 0);
  EXPECT_EQ(1u, connection_.GetStats().num_validated_peer_migration);
  EXPECT_EQ(1u, connection_.num_unlinkable_client_migration());
}

TEST_P(QuicConnectionTest,
       PathValidationSucceedsBeforePeerIpAddressChangeAtServer) {
  set_perspective(Perspective::IS_SERVER);
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  PathProbeTestInit(Perspective::IS_SERVER);
  connection_.CreateConnectionIdManager();

  QuicConnectionId server_cid0 = connection_.connection_id();
  QuicConnectionId server_cid1;
  // Sends new server CID to client.
  if (!connection_.connection_id().IsEmpty()) {
    EXPECT_CALL(connection_id_generator_, GenerateNextConnectionId(_))
        .WillOnce(Return(TestConnectionId(456)));
  }
  EXPECT_CALL(visitor_, MaybeReserveConnectionId(_))
      .WillOnce(Invoke([&](const QuicConnectionId& cid) {
        server_cid1 = cid;
        return true;
      }));
  EXPECT_CALL(visitor_, SendNewConnectionId(_));
  connection_.MaybeSendConnectionIdToClient();
  auto* packet_creator = QuicConnectionPeer::GetPacketCreator(&connection_);
  ASSERT_EQ(packet_creator->GetSourceConnectionId(), server_cid0);

  // Receive probing packet with new peer address.
  peer_creator_.SetServerConnectionId(server_cid1);
  const QuicSocketAddress kNewPeerAddress(QuicIpAddress::Loopback4(),
                                          /*port=*/23456);
  QuicPathFrameBuffer payload;
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, _, _, _, NO_RETRANSMITTABLE_DATA))
      .WillOnce(Invoke([&]() {
        EXPECT_EQ(kNewPeerAddress, writer_->last_write_peer_address());
        EXPECT_EQ(kPeerAddress, connection_.peer_address());
        EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
        EXPECT_FALSE(writer_->path_response_frames().empty());
        EXPECT_FALSE(writer_->path_challenge_frames().empty());
        payload = writer_->path_challenge_frames().front().data_buffer;
      }))
      .WillRepeatedly(Invoke([&]() {
        // Only start reverse path validation once.
        EXPECT_TRUE(writer_->path_challenge_frames().empty());
      }));
  QuicPathFrameBuffer path_challenge_payload{0, 1, 2, 3, 4, 5, 6, 7};
  QuicFrames frames1;
  frames1.push_back(
      QuicFrame(QuicPathChallengeFrame(0, path_challenge_payload)));
  ProcessFramesPacketWithAddresses(frames1, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  const auto* default_path = QuicConnectionPeer::GetDefaultPath(&connection_);
  const auto* alternative_path =
      QuicConnectionPeer::GetAlternativePath(&connection_);
  EXPECT_EQ(default_path->server_connection_id, server_cid0);
  EXPECT_EQ(alternative_path->server_connection_id, server_cid1);
  EXPECT_EQ(packet_creator->GetSourceConnectionId(), server_cid0);

  // Receive PATH_RESPONSE should mark the new peer address validated.
  QuicFrames frames3;
  frames3.push_back(QuicFrame(QuicPathResponseFrame(99, payload)));
  ProcessFramesPacketWithAddresses(frames3, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);

  // Process another packet with a newer peer address with the same port will
  // start connection migration.
  EXPECT_CALL(visitor_, OnConnectionMigration(IPV6_TO_IPV4_CHANGE)).Times(1);
  // IETF QUIC send algorithm should be changed to a different object, so no
  // OnPacketSent() called on the old send algorithm.
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, _, _, _, NO_RETRANSMITTABLE_DATA))
      .Times(0);
  const QuicSocketAddress kNewerPeerAddress(QuicIpAddress::Loopback4(),
                                            /*port=*/34567);
  EXPECT_CALL(visitor_, OnStreamFrame(_)).WillOnce(Invoke([=, this]() {
    EXPECT_EQ(kNewerPeerAddress, connection_.peer_address());
  }));
  EXPECT_CALL(visitor_, MaybeSendAddressToken());
  QuicFrames frames2;
  frames2.push_back(QuicFrame(frame2_));
  ProcessFramesPacketWithAddresses(frames2, kSelfAddress, kNewerPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kNewerPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewerPeerAddress, connection_.effective_peer_address());
  // Since the newer address has the same IP as the previously validated probing
  // address. The peer migration becomes validated immediately.
  EXPECT_EQ(NO_CHANGE, connection_.active_effective_peer_migration_type());
  EXPECT_EQ(kNewerPeerAddress, writer_->last_write_peer_address());
  EXPECT_EQ(1u, connection_.GetStats()
                    .num_peer_migration_to_proactively_validated_address);
  EXPECT_FALSE(connection_.HasPendingPathValidation());
  EXPECT_NE(connection_.sent_packet_manager().GetSendAlgorithm(),
            send_algorithm_);

  EXPECT_EQ(default_path->server_connection_id, server_cid1);
  EXPECT_EQ(packet_creator->GetSourceConnectionId(), server_cid1);
  // Verify that alternative_path_ is cleared.
  EXPECT_TRUE(alternative_path->server_connection_id.IsEmpty());
  EXPECT_FALSE(alternative_path->stateless_reset_token.has_value());

  // Switch to use the mock send algorithm.
  send_algorithm_ = new StrictMock<MockSendAlgorithm>();
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(kDefaultTCPMSS));
  EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, BandwidthEstimate())
      .Times(AnyNumber())
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, InSlowStart()).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, InRecovery()).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, PopulateConnectionStats(_)).Times(AnyNumber());
  connection_.SetSendAlgorithm(send_algorithm_);

  // Verify the server is not throttled by the anti-amplification limit by
  // sending a packet larger than the anti-amplification limit.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  connection_.SendCryptoDataWithString(std::string(1200, 'a'), 0);
  EXPECT_EQ(1u, connection_.GetStats().num_validated_peer_migration);
}

// Regression test of b/228645208.
TEST_P(QuicConnectionTest, NoNonProbingFrameOnAlternativePath) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }

  PathProbeTestInit(Perspective::IS_SERVER);
  SetClientConnectionId(TestConnectionId(1));
  connection_.CreateConnectionIdManager();

  QuicConnectionId server_cid0 = connection_.connection_id();
  QuicConnectionId client_cid0 = connection_.client_connection_id();
  QuicConnectionId client_cid1 = TestConnectionId(2);
  QuicConnectionId server_cid1;
  // Sends new server CID to client.
  if (!connection_.connection_id().IsEmpty()) {
    EXPECT_CALL(connection_id_generator_, GenerateNextConnectionId(_))
        .WillOnce(Return(TestConnectionId(456)));
  }
  EXPECT_CALL(visitor_, MaybeReserveConnectionId(_))
      .WillOnce(Invoke([&](const QuicConnectionId& cid) {
        server_cid1 = cid;
        return true;
      }));
  EXPECT_CALL(visitor_, SendNewConnectionId(_));
  connection_.MaybeSendConnectionIdToClient();
  // Receives new client CID from client.
  QuicNewConnectionIdFrame new_cid_frame;
  new_cid_frame.connection_id = client_cid1;
  new_cid_frame.sequence_number = 1u;
  new_cid_frame.retire_prior_to = 0u;
  connection_.OnNewConnectionIdFrame(new_cid_frame);
  auto* packet_creator = QuicConnectionPeer::GetPacketCreator(&connection_);
  ASSERT_EQ(packet_creator->GetDestinationConnectionId(), client_cid0);
  ASSERT_EQ(packet_creator->GetSourceConnectionId(), server_cid0);

  peer_creator_.SetServerConnectionId(server_cid1);
  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback4(), /*port=*/23456);
  QuicPathFrameBuffer path_challenge_payload{0, 1, 2, 3, 4, 5, 6, 7};
  QuicFrames frames1;
  frames1.push_back(
      QuicFrame(QuicPathChallengeFrame(0, path_challenge_payload)));
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, _, _, _, NO_RETRANSMITTABLE_DATA))
      .Times(AtLeast(1))
      .WillOnce(Invoke([&]() {
        EXPECT_EQ(kNewPeerAddress, writer_->last_write_peer_address());
        EXPECT_EQ(kPeerAddress, connection_.peer_address());
        EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
        EXPECT_FALSE(writer_->path_response_frames().empty());
        EXPECT_FALSE(writer_->path_challenge_frames().empty());
      }))
      .WillRepeatedly(DoDefault());
  ProcessFramesPacketWithAddresses(frames1, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  const auto* default_path = QuicConnectionPeer::GetDefaultPath(&connection_);
  const auto* alternative_path =
      QuicConnectionPeer::GetAlternativePath(&connection_);
  EXPECT_EQ(default_path->client_connection_id, client_cid0);
  EXPECT_EQ(default_path->server_connection_id, server_cid0);
  EXPECT_EQ(alternative_path->client_connection_id, client_cid1);
  EXPECT_EQ(alternative_path->server_connection_id, server_cid1);
  EXPECT_EQ(packet_creator->GetDestinationConnectionId(), client_cid0);
  EXPECT_EQ(packet_creator->GetSourceConnectionId(), server_cid0);

  // Process non-probing packets on the default path.
  peer_creator_.SetServerConnectionId(server_cid0);
  EXPECT_CALL(visitor_, OnStreamFrame(_)).WillRepeatedly(Invoke([=, this]() {
    EXPECT_EQ(kPeerAddress, connection_.peer_address());
  }));
  // Receives packets 3 - 39 to send 19 ACK-only packets, which will force the
  // connection to reach |kMaxConsecutiveNonRetransmittablePackets| while
  // sending the next ACK.
  for (size_t i = 3; i <= 39; ++i) {
    ProcessDataPacket(i);
  }
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  EXPECT_TRUE(connection_.HasPendingAcks());
  QuicTime ack_time = connection_.GetAckAlarm()->deadline();
  QuicTime path_validation_retry_time =
      connection_.GetRetryTimeout(kNewPeerAddress, writer_.get());
  // Advance time to simultaneously fire path validation retry and ACK alarms.
  clock_.AdvanceTime(std::max(ack_time, path_validation_retry_time) -
                     clock_.ApproximateNow());

  // The 20th ACK should bundle with a WINDOW_UPDATE frame.
  EXPECT_CALL(visitor_, OnAckNeedsRetransmittableFrame())
      .WillOnce(Invoke([this]() {
        connection_.SendControlFrame(QuicFrame(QuicWindowUpdateFrame(1, 0, 0)));
      }));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(Invoke([&]() {
        EXPECT_EQ(kNewPeerAddress, writer_->last_write_peer_address());
        EXPECT_FALSE(writer_->path_challenge_frames().empty());
        // Retry path validation shouldn't bundle ACK.
        EXPECT_TRUE(writer_->ack_frames().empty());
      }))
      .WillOnce(Invoke([&]() {
        EXPECT_EQ(kPeerAddress, writer_->last_write_peer_address());
        EXPECT_FALSE(writer_->ack_frames().empty());
        EXPECT_FALSE(writer_->window_update_frames().empty());
      }));
  static_cast<TestAlarmFactory::TestAlarm*>(
      QuicPathValidatorPeer::retry_timer(
          QuicConnectionPeer::path_validator(&connection_)))
      ->Fire();
}

TEST_P(QuicConnectionTest, DoNotIssueNewCidIfVisitorSaysNo) {
  set_perspective(Perspective::IS_SERVER);
  if (!version().HasIetfQuicFrames()) {
    return;
  }

  connection_.CreateConnectionIdManager();

  QuicConnectionId server_cid0 = connection_.connection_id();
  QuicConnectionId client_cid1 = TestConnectionId(2);
  QuicConnectionId server_cid1;
  // Sends new server CID to client.
  if (!connection_.connection_id().IsEmpty()) {
    EXPECT_CALL(connection_id_generator_, GenerateNextConnectionId(_))
        .WillOnce(Return(TestConnectionId(456)));
  }
  EXPECT_CALL(visitor_, MaybeReserveConnectionId(_)).WillOnce(Return(false));
  EXPECT_CALL(visitor_, SendNewConnectionId(_)).Times(0);
  connection_.MaybeSendConnectionIdToClient();
}

TEST_P(QuicConnectionTest,
       ProbedOnAnotherPathAfterPeerIpAddressChangeAtServer) {
  PathProbeTestInit(Perspective::IS_SERVER);
  if (!version().HasIetfQuicFrames()) {
    return;
  }

  const QuicSocketAddress kNewPeerAddress(QuicIpAddress::Loopback4(),
                                          /*port=*/23456);

  // Process a packet with a new peer address will start connection migration.
  EXPECT_CALL(visitor_, OnConnectionMigration(IPV6_TO_IPV4_CHANGE)).Times(1);
  // IETF QUIC send algorithm should be changed to a different object, so no
  // OnPacketSent() called on the old send algorithm.
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, _, _, _, NO_RETRANSMITTABLE_DATA))
      .Times(0);
  EXPECT_CALL(visitor_, OnStreamFrame(_)).WillOnce(Invoke([=, this]() {
    EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  }));
  QuicFrames frames2;
  frames2.push_back(QuicFrame(frame2_));
  ProcessFramesPacketWithAddresses(frames2, kSelfAddress, kNewPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePathValidated(&connection_));
  EXPECT_TRUE(connection_.HasPendingPathValidation());

  // Switch to use the mock send algorithm.
  send_algorithm_ = new StrictMock<MockSendAlgorithm>();
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(kDefaultTCPMSS));
  EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, BandwidthEstimate())
      .Times(AnyNumber())
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, InSlowStart()).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, InRecovery()).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, PopulateConnectionStats(_)).Times(AnyNumber());
  connection_.SetSendAlgorithm(send_algorithm_);

  // Receive probing packet with a newer peer address shouldn't override the
  // on-going path validation.
  const QuicSocketAddress kNewerPeerAddress(QuicIpAddress::Loopback4(),
                                            /*port=*/34567);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(Invoke([&]() {
        EXPECT_EQ(kNewerPeerAddress, writer_->last_write_peer_address());
        EXPECT_FALSE(writer_->path_response_frames().empty());
        EXPECT_TRUE(writer_->path_challenge_frames().empty());
      }));
  QuicPathFrameBuffer path_challenge_payload{0, 1, 2, 3, 4, 5, 6, 7};
  QuicFrames frames1;
  frames1.push_back(
      QuicFrame(QuicPathChallengeFrame(0, path_challenge_payload)));
  ProcessFramesPacketWithAddresses(frames1, kSelfAddress, kNewerPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePathValidated(&connection_));
  EXPECT_TRUE(connection_.HasPendingPathValidation());
}

TEST_P(QuicConnectionTest,
       PathValidationFailedOnClientDueToLackOfServerConnectionId) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  PathProbeTestInit(Perspective::IS_CLIENT,
                    /*receive_new_server_connection_id=*/false);

  const QuicSocketAddress kNewSelfAddress(QuicIpAddress::Loopback4(),
                                          /*port=*/34567);

  bool success;
  connection_.ValidatePath(
      std::make_unique<TestQuicPathValidationContext>(
          kNewSelfAddress, connection_.peer_address(), writer_.get()),
      std::make_unique<TestValidationResultDelegate>(
          &connection_, kNewSelfAddress, connection_.peer_address(), &success),
      PathValidationReason::kReasonUnknown);

  EXPECT_FALSE(success);
}

TEST_P(QuicConnectionTest,
       PathValidationFailedOnClientDueToLackOfClientConnectionIdTheSecondTime) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  PathProbeTestInit(Perspective::IS_CLIENT,
                    /*receive_new_server_connection_id=*/false);
  SetClientConnectionId(TestConnectionId(1));

  // Make sure server connection ID is available for the 1st validation.
  QuicConnectionId server_cid0 = connection_.connection_id();
  QuicConnectionId server_cid1 = TestConnectionId(2);
  QuicConnectionId server_cid2 = TestConnectionId(4);
  QuicConnectionId client_cid1;
  QuicNewConnectionIdFrame frame1;
  frame1.connection_id = server_cid1;
  frame1.sequence_number = 1u;
  frame1.retire_prior_to = 0u;
  frame1.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame1.connection_id);
  connection_.OnNewConnectionIdFrame(frame1);
  const auto* packet_creator =
      QuicConnectionPeer::GetPacketCreator(&connection_);
  ASSERT_EQ(packet_creator->GetDestinationConnectionId(), server_cid0);

  // Client will issue a new client connection ID to server.
  EXPECT_CALL(connection_id_generator_, GenerateNextConnectionId(_))
      .WillOnce(Return(TestConnectionId(456)));
  EXPECT_CALL(visitor_, SendNewConnectionId(_))
      .WillOnce(Invoke([&](const QuicNewConnectionIdFrame& frame) {
        client_cid1 = frame.connection_id;
      }));

  const QuicSocketAddress kSelfAddress1(QuicIpAddress::Any4(), 12345);
  ASSERT_NE(kSelfAddress1, connection_.self_address());
  bool success1;
  connection_.ValidatePath(
      std::make_unique<TestQuicPathValidationContext>(
          kSelfAddress1, connection_.peer_address(), writer_.get()),
      std::make_unique<TestValidationResultDelegate>(
          &connection_, kSelfAddress1, connection_.peer_address(), &success1),
      PathValidationReason::kReasonUnknown);

  // Migrate upon 1st validation success.
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  ASSERT_TRUE(connection_.MigratePath(kSelfAddress1, connection_.peer_address(),
                                      &new_writer, /*owns_writer=*/false));
  QuicConnectionPeer::RetirePeerIssuedConnectionIdsNoLongerOnPath(&connection_);
  const auto* default_path = QuicConnectionPeer::GetDefaultPath(&connection_);
  EXPECT_EQ(default_path->client_connection_id, client_cid1);
  EXPECT_EQ(default_path->server_connection_id, server_cid1);
  EXPECT_EQ(default_path->stateless_reset_token, frame1.stateless_reset_token);
  const auto* alternative_path =
      QuicConnectionPeer::GetAlternativePath(&connection_);
  EXPECT_TRUE(alternative_path->client_connection_id.IsEmpty());
  EXPECT_TRUE(alternative_path->server_connection_id.IsEmpty());
  EXPECT_FALSE(alternative_path->stateless_reset_token.has_value());
  ASSERT_EQ(packet_creator->GetDestinationConnectionId(), server_cid1);

  // Client will retire server connection ID on old default_path.
  auto* retire_peer_issued_cid_alarm =
      connection_.GetRetirePeerIssuedConnectionIdAlarm();
  ASSERT_TRUE(retire_peer_issued_cid_alarm->IsSet());
  EXPECT_CALL(visitor_, SendRetireConnectionId(/*sequence_number=*/0u));
  retire_peer_issued_cid_alarm->Fire();

  // Another server connection ID is available to client.
  QuicNewConnectionIdFrame frame2;
  frame2.connection_id = server_cid2;
  frame2.sequence_number = 2u;
  frame2.retire_prior_to = 1u;
  frame2.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame2.connection_id);
  connection_.OnNewConnectionIdFrame(frame2);

  const QuicSocketAddress kSelfAddress2(QuicIpAddress::Loopback4(),
                                        /*port=*/45678);
  bool success2;
  connection_.ValidatePath(
      std::make_unique<TestQuicPathValidationContext>(
          kSelfAddress2, connection_.peer_address(), writer_.get()),
      std::make_unique<TestValidationResultDelegate>(
          &connection_, kSelfAddress2, connection_.peer_address(), &success2),
      PathValidationReason::kReasonUnknown);
  // Since server does not retire any client connection ID yet, 2nd validation
  // would fail due to lack of client connection ID.
  EXPECT_FALSE(success2);
}

TEST_P(QuicConnectionTest, ServerConnectionIdRetiredUponPathValidationFailure) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  PathProbeTestInit(Perspective::IS_CLIENT);

  // Make sure server connection ID is available for validation.
  QuicNewConnectionIdFrame frame;
  frame.connection_id = TestConnectionId(2);
  frame.sequence_number = 1u;
  frame.retire_prior_to = 0u;
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  connection_.OnNewConnectionIdFrame(frame);

  const QuicSocketAddress kNewSelfAddress(QuicIpAddress::Loopback4(),
                                          /*port=*/34567);
  bool success;
  connection_.ValidatePath(
      std::make_unique<TestQuicPathValidationContext>(
          kNewSelfAddress, connection_.peer_address(), writer_.get()),
      std::make_unique<TestValidationResultDelegate>(
          &connection_, kNewSelfAddress, connection_.peer_address(), &success),
      PathValidationReason::kReasonUnknown);

  auto* path_validator = QuicConnectionPeer::path_validator(&connection_);
  path_validator->CancelPathValidation();
  QuicConnectionPeer::RetirePeerIssuedConnectionIdsNoLongerOnPath(&connection_);
  EXPECT_FALSE(success);
  const auto* alternative_path =
      QuicConnectionPeer::GetAlternativePath(&connection_);
  EXPECT_TRUE(alternative_path->client_connection_id.IsEmpty());
  EXPECT_TRUE(alternative_path->server_connection_id.IsEmpty());
  EXPECT_FALSE(alternative_path->stateless_reset_token.has_value());

  // Client will retire server connection ID on alternative_path.
  auto* retire_peer_issued_cid_alarm =
      connection_.GetRetirePeerIssuedConnectionIdAlarm();
  ASSERT_TRUE(retire_peer_issued_cid_alarm->IsSet());
  EXPECT_CALL(visitor_, SendRetireConnectionId(/*sequence_number=*/1u));
  retire_peer_issued_cid_alarm->Fire();
}

TEST_P(QuicConnectionTest,
       MigratePathDirectlyFailedDueToLackOfServerConnectionId) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  PathProbeTestInit(Perspective::IS_CLIENT,
                    /*receive_new_server_connection_id=*/false);
  const QuicSocketAddress kSelfAddress1(QuicIpAddress::Any4(), 12345);
  ASSERT_NE(kSelfAddress1, connection_.self_address());

  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  ASSERT_FALSE(connection_.MigratePath(kSelfAddress1,
                                       connection_.peer_address(), &new_writer,
                                       /*owns_writer=*/false));
}

TEST_P(QuicConnectionTest,
       MigratePathDirectlyFailedDueToLackOfClientConnectionIdTheSecondTime) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  PathProbeTestInit(Perspective::IS_CLIENT,
                    /*receive_new_server_connection_id=*/false);
  SetClientConnectionId(TestConnectionId(1));

  // Make sure server connection ID is available for the 1st migration.
  QuicNewConnectionIdFrame frame1;
  frame1.connection_id = TestConnectionId(2);
  frame1.sequence_number = 1u;
  frame1.retire_prior_to = 0u;
  frame1.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame1.connection_id);
  connection_.OnNewConnectionIdFrame(frame1);

  // Client will issue a new client connection ID to server.
  QuicConnectionId new_client_connection_id;
  EXPECT_CALL(connection_id_generator_, GenerateNextConnectionId(_))
      .WillOnce(Return(TestConnectionId(456)));
  EXPECT_CALL(visitor_, SendNewConnectionId(_))
      .WillOnce(Invoke([&](const QuicNewConnectionIdFrame& frame) {
        new_client_connection_id = frame.connection_id;
      }));

  // 1st migration is successful.
  const QuicSocketAddress kSelfAddress1(QuicIpAddress::Any4(), 12345);
  ASSERT_NE(kSelfAddress1, connection_.self_address());
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  ASSERT_TRUE(connection_.MigratePath(kSelfAddress1, connection_.peer_address(),
                                      &new_writer,
                                      /*owns_writer=*/false));
  QuicConnectionPeer::RetirePeerIssuedConnectionIdsNoLongerOnPath(&connection_);
  const auto* default_path = QuicConnectionPeer::GetDefaultPath(&connection_);
  EXPECT_EQ(default_path->client_connection_id, new_client_connection_id);
  EXPECT_EQ(default_path->server_connection_id, frame1.connection_id);
  EXPECT_EQ(default_path->stateless_reset_token, frame1.stateless_reset_token);

  // Client will retire server connection ID on old default_path.
  auto* retire_peer_issued_cid_alarm =
      connection_.GetRetirePeerIssuedConnectionIdAlarm();
  ASSERT_TRUE(retire_peer_issued_cid_alarm->IsSet());
  EXPECT_CALL(visitor_, SendRetireConnectionId(/*sequence_number=*/0u));
  retire_peer_issued_cid_alarm->Fire();

  // Another server connection ID is available to client.
  QuicNewConnectionIdFrame frame2;
  frame2.connection_id = TestConnectionId(4);
  frame2.sequence_number = 2u;
  frame2.retire_prior_to = 1u;
  frame2.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame2.connection_id);
  connection_.OnNewConnectionIdFrame(frame2);

  // Since server does not retire any client connection ID yet, 2nd migration
  // would fail due to lack of client connection ID.
  const QuicSocketAddress kSelfAddress2(QuicIpAddress::Loopback4(),
                                        /*port=*/45678);
  auto new_writer2 = std::make_unique<TestPacketWriter>(version(), &clock_,
                                                        Perspective::IS_CLIENT);
  ASSERT_FALSE(connection_.MigratePath(
      kSelfAddress2, connection_.peer_address(), new_writer2.release(),
      /*owns_writer=*/true));
}

TEST_P(QuicConnectionTest,
       CloseConnectionAfterReceiveNewConnectionIdFromPeerUsingEmptyCID) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  ASSERT_TRUE(connection_.client_connection_id().IsEmpty());

  EXPECT_CALL(visitor_, BeforeConnectionCloseSent());
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));
  QuicNewConnectionIdFrame frame;
  frame.sequence_number = 1u;
  frame.connection_id = TestConnectionId(1);
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 0u;

  EXPECT_FALSE(connection_.OnNewConnectionIdFrame(frame));

  EXPECT_FALSE(connection_.connected());
  EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
              IsError(IETF_QUIC_PROTOCOL_VIOLATION));
}

TEST_P(QuicConnectionTest, NewConnectionIdFrameResultsInError) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  connection_.CreateConnectionIdManager();
  ASSERT_FALSE(connection_.connection_id().IsEmpty());

  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));
  QuicNewConnectionIdFrame frame;
  frame.sequence_number = 1u;
  frame.connection_id = connection_id_;  // Reuses connection ID casuing error.
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 0u;

  EXPECT_FALSE(connection_.OnNewConnectionIdFrame(frame));

  EXPECT_FALSE(connection_.connected());
  EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
              IsError(IETF_QUIC_PROTOCOL_VIOLATION));
}

TEST_P(QuicConnectionTest,
       ClientRetirePeerIssuedConnectionIdTriggeredByNewConnectionIdFrame) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  connection_.CreateConnectionIdManager();

  QuicNewConnectionIdFrame frame;
  frame.sequence_number = 1u;
  frame.connection_id = TestConnectionId(1);
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 0u;

  EXPECT_TRUE(connection_.OnNewConnectionIdFrame(frame));
  auto* retire_peer_issued_cid_alarm =
      connection_.GetRetirePeerIssuedConnectionIdAlarm();
  ASSERT_FALSE(retire_peer_issued_cid_alarm->IsSet());

  frame.sequence_number = 2u;
  frame.connection_id = TestConnectionId(2);
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 1u;  // CID associated with #1 will be retired.

  EXPECT_TRUE(connection_.OnNewConnectionIdFrame(frame));
  ASSERT_TRUE(retire_peer_issued_cid_alarm->IsSet());
  EXPECT_EQ(connection_.connection_id(), connection_id_);

  EXPECT_CALL(visitor_, SendRetireConnectionId(/*sequence_number=*/0u));
  retire_peer_issued_cid_alarm->Fire();
  EXPECT_EQ(connection_.connection_id(), TestConnectionId(2));
  EXPECT_EQ(connection_.packet_creator().GetDestinationConnectionId(),
            TestConnectionId(2));
}

TEST_P(QuicConnectionTest,
       ServerRetirePeerIssuedConnectionIdTriggeredByNewConnectionIdFrame) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  SetClientConnectionId(TestConnectionId(0));

  QuicNewConnectionIdFrame frame;
  frame.sequence_number = 1u;
  frame.connection_id = TestConnectionId(1);
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 0u;

  EXPECT_TRUE(connection_.OnNewConnectionIdFrame(frame));
  auto* retire_peer_issued_cid_alarm =
      connection_.GetRetirePeerIssuedConnectionIdAlarm();
  ASSERT_FALSE(retire_peer_issued_cid_alarm->IsSet());

  frame.sequence_number = 2u;
  frame.connection_id = TestConnectionId(2);
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 1u;  // CID associated with #1 will be retired.

  EXPECT_TRUE(connection_.OnNewConnectionIdFrame(frame));
  ASSERT_TRUE(retire_peer_issued_cid_alarm->IsSet());
  EXPECT_EQ(connection_.client_connection_id(), TestConnectionId(0));

  EXPECT_CALL(visitor_, SendRetireConnectionId(/*sequence_number=*/0u));
  retire_peer_issued_cid_alarm->Fire();
  EXPECT_EQ(connection_.client_connection_id(), TestConnectionId(2));
  EXPECT_EQ(connection_.packet_creator().GetDestinationConnectionId(),
            TestConnectionId(2));
}

TEST_P(
    QuicConnectionTest,
    ReplacePeerIssuedConnectionIdOnBothPathsTriggeredByNewConnectionIdFrame) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  PathProbeTestInit(Perspective::IS_SERVER);
  SetClientConnectionId(TestConnectionId(0));

  // Populate alternative_path_ with probing packet.
  std::unique_ptr<SerializedPacket> probing_packet = ConstructProbingPacket();

  std::unique_ptr<QuicReceivedPacket> received(ConstructReceivedPacket(
      QuicEncryptedPacket(probing_packet->encrypted_buffer,
                          probing_packet->encrypted_length),
      clock_.Now()));
  QuicIpAddress new_host;
  new_host.FromString("1.1.1.1");
  ProcessReceivedPacket(kSelfAddress,
                        QuicSocketAddress(new_host, /*port=*/23456), *received);

  EXPECT_EQ(
      TestConnectionId(0),
      QuicConnectionPeer::GetClientConnectionIdOnAlternativePath(&connection_));

  QuicNewConnectionIdFrame frame;
  frame.sequence_number = 1u;
  frame.connection_id = TestConnectionId(1);
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 0u;

  EXPECT_TRUE(connection_.OnNewConnectionIdFrame(frame));
  auto* retire_peer_issued_cid_alarm =
      connection_.GetRetirePeerIssuedConnectionIdAlarm();
  ASSERT_FALSE(retire_peer_issued_cid_alarm->IsSet());

  frame.sequence_number = 2u;
  frame.connection_id = TestConnectionId(2);
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 1u;  // CID associated with #1 will be retired.

  EXPECT_TRUE(connection_.OnNewConnectionIdFrame(frame));
  ASSERT_TRUE(retire_peer_issued_cid_alarm->IsSet());
  EXPECT_EQ(connection_.client_connection_id(), TestConnectionId(0));

  EXPECT_CALL(visitor_, SendRetireConnectionId(/*sequence_number=*/0u));
  retire_peer_issued_cid_alarm->Fire();
  EXPECT_EQ(connection_.client_connection_id(), TestConnectionId(2));
  EXPECT_EQ(connection_.packet_creator().GetDestinationConnectionId(),
            TestConnectionId(2));
  // Clean up alternative path connection ID.
  EXPECT_EQ(
      TestConnectionId(2),
      QuicConnectionPeer::GetClientConnectionIdOnAlternativePath(&connection_));
}

TEST_P(QuicConnectionTest,
       CloseConnectionAfterReceiveRetireConnectionIdWhenNoCIDIssued) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);

  EXPECT_CALL(visitor_, BeforeConnectionCloseSent());
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));
  QuicRetireConnectionIdFrame frame;
  frame.sequence_number = 1u;

  EXPECT_FALSE(connection_.OnRetireConnectionIdFrame(frame));

  EXPECT_FALSE(connection_.connected());
  EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
              IsError(IETF_QUIC_PROTOCOL_VIOLATION));
}

TEST_P(QuicConnectionTest, RetireConnectionIdFrameResultsInError) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  connection_.CreateConnectionIdManager();

  if (!connection_.connection_id().IsEmpty()) {
    EXPECT_CALL(connection_id_generator_, GenerateNextConnectionId(_))
        .WillOnce(Return(TestConnectionId(456)));
  }
  EXPECT_CALL(visitor_, MaybeReserveConnectionId(_)).WillOnce(Return(true));
  EXPECT_CALL(visitor_, SendNewConnectionId(_));
  connection_.MaybeSendConnectionIdToClient();

  EXPECT_CALL(visitor_, BeforeConnectionCloseSent());
  EXPECT_CALL(visitor_, OnConnectionClosed(_, ConnectionCloseSource::FROM_SELF))
      .WillOnce(Invoke(this, &QuicConnectionTest::SaveConnectionCloseFrame));
  QuicRetireConnectionIdFrame frame;
  frame.sequence_number = 2u;  // The corresponding ID is never issued.

  EXPECT_FALSE(connection_.OnRetireConnectionIdFrame(frame));

  EXPECT_FALSE(connection_.connected());
  EXPECT_THAT(saved_connection_close_frame_.quic_error_code,
              IsError(IETF_QUIC_PROTOCOL_VIOLATION));
}

TEST_P(QuicConnectionTest,
       ServerRetireSelfIssuedConnectionIdWithoutSendingNewConnectionIdBefore) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  connection_.CreateConnectionIdManager();

  auto* retire_self_issued_cid_alarm =
      connection_.GetRetireSelfIssuedConnectionIdAlarm();
  ASSERT_FALSE(retire_self_issued_cid_alarm->IsSet());

  QuicConnectionId cid0 = connection_id_;
  QuicRetireConnectionIdFrame frame;
  frame.sequence_number = 0u;

  if (!connection_.connection_id().IsEmpty()) {
    EXPECT_CALL(connection_id_generator_, GenerateNextConnectionId(cid0))
        .WillOnce(Return(TestConnectionId(456)));
    EXPECT_CALL(connection_id_generator_,
                GenerateNextConnectionId(TestConnectionId(456)))
        .WillOnce(Return(TestConnectionId(789)));
  }
  EXPECT_CALL(visitor_, MaybeReserveConnectionId(_))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(visitor_, SendNewConnectionId(_)).Times(2);
  EXPECT_TRUE(connection_.OnRetireConnectionIdFrame(frame));
}

TEST_P(QuicConnectionTest, ServerRetireSelfIssuedConnectionId) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  connection_.CreateConnectionIdManager();
  QuicConnectionId recorded_cid;
  auto cid_recorder = [&recorded_cid](const QuicConnectionId& cid) -> bool {
    recorded_cid = cid;
    return true;
  };
  QuicConnectionId cid0 = connection_id_;
  QuicConnectionId cid1;
  QuicConnectionId cid2;
  EXPECT_EQ(connection_.connection_id(), cid0);
  EXPECT_EQ(connection_.GetOneActiveServerConnectionId(), cid0);

  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  if (!connection_.connection_id().IsEmpty()) {
    EXPECT_CALL(connection_id_generator_, GenerateNextConnectionId(_))
        .WillOnce(Return(TestConnectionId(456)));
  }
  EXPECT_CALL(visitor_, MaybeReserveConnectionId(_))
      .WillOnce(Invoke(cid_recorder));
  EXPECT_CALL(visitor_, SendNewConnectionId(_));
  connection_.MaybeSendConnectionIdToClient();
  cid1 = recorded_cid;

  auto* retire_self_issued_cid_alarm =
      connection_.GetRetireSelfIssuedConnectionIdAlarm();
  ASSERT_FALSE(retire_self_issued_cid_alarm->IsSet());

  // Generate three packets with different connection IDs that will arrive out
  // of order (2, 1, 3) later.
  char buffers[3][kMaxOutgoingPacketSize];
  // Destination connection ID of packet1 is cid0.
  auto packet1 =
      ConstructPacket({QuicFrame(QuicPingFrame())}, ENCRYPTION_FORWARD_SECURE,
                      buffers[0], kMaxOutgoingPacketSize);
  peer_creator_.SetServerConnectionId(cid1);
  auto retire_cid_frame = std::make_unique<QuicRetireConnectionIdFrame>();
  retire_cid_frame->sequence_number = 0u;
  // Destination connection ID of packet2 is cid1.
  auto packet2 = ConstructPacket({QuicFrame(retire_cid_frame.release())},
                                 ENCRYPTION_FORWARD_SECURE, buffers[1],
                                 kMaxOutgoingPacketSize);
  // Destination connection ID of packet3 is cid1.
  auto packet3 =
      ConstructPacket({QuicFrame(QuicPingFrame())}, ENCRYPTION_FORWARD_SECURE,
                      buffers[2], kMaxOutgoingPacketSize);

  // Packet2 with RetireConnectionId frame trigers sending NewConnectionId
  // immediately.
  if (!connection_.connection_id().IsEmpty()) {
    EXPECT_CALL(connection_id_generator_, GenerateNextConnectionId(_))
        .WillOnce(Return(TestConnectionId(456)));
  }
  EXPECT_CALL(visitor_, MaybeReserveConnectionId(_))
      .WillOnce(Invoke(cid_recorder));
  EXPECT_CALL(visitor_, SendNewConnectionId(_));
  peer_creator_.SetServerConnectionId(cid1);
  connection_.ProcessUdpPacket(kSelfAddress, kPeerAddress, *packet2);
  cid2 = recorded_cid;
  // cid0 is not retired immediately.
  EXPECT_THAT(connection_.GetActiveServerConnectionIds(),
              ElementsAre(cid0, cid1, cid2));
  ASSERT_TRUE(retire_self_issued_cid_alarm->IsSet());
  EXPECT_EQ(connection_.connection_id(), cid1);
  EXPECT_TRUE(connection_.GetOneActiveServerConnectionId() == cid0 ||
              connection_.GetOneActiveServerConnectionId() == cid1 ||
              connection_.GetOneActiveServerConnectionId() == cid2);

  // Packet1 updates the connection ID on the default path but not the active
  // connection ID.
  connection_.ProcessUdpPacket(kSelfAddress, kPeerAddress, *packet1);
  EXPECT_EQ(connection_.connection_id(), cid0);
  EXPECT_TRUE(connection_.GetOneActiveServerConnectionId() == cid0 ||
              connection_.GetOneActiveServerConnectionId() == cid1 ||
              connection_.GetOneActiveServerConnectionId() == cid2);

  // cid0 is retired when the retire CID alarm fires.
  EXPECT_CALL(visitor_, OnServerConnectionIdRetired(cid0));
  retire_self_issued_cid_alarm->Fire();
  EXPECT_THAT(connection_.GetActiveServerConnectionIds(),
              ElementsAre(cid1, cid2));
  EXPECT_TRUE(connection_.GetOneActiveServerConnectionId() == cid1 ||
              connection_.GetOneActiveServerConnectionId() == cid2);

  // Packet3 updates the connection ID on the default path.
  connection_.ProcessUdpPacket(kSelfAddress, kPeerAddress, *packet3);
  EXPECT_EQ(connection_.connection_id(), cid1);
  EXPECT_TRUE(connection_.GetOneActiveServerConnectionId() == cid1 ||
              connection_.GetOneActiveServerConnectionId() == cid2);
}

TEST_P(QuicConnectionTest, PatchMissingClientConnectionIdOntoAlternativePath) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  connection_.CreateConnectionIdManager();
  connection_.set_client_connection_id(TestConnectionId(1));

  // Set up the state after path probing.
  const auto* default_path = QuicConnectionPeer::GetDefaultPath(&connection_);
  auto* alternative_path = QuicConnectionPeer::GetAlternativePath(&connection_);
  QuicIpAddress new_host;
  new_host.FromString("12.12.12.12");
  alternative_path->self_address = default_path->self_address;
  alternative_path->peer_address = QuicSocketAddress(new_host, 12345);
  alternative_path->server_connection_id = TestConnectionId(3);
  ASSERT_TRUE(alternative_path->client_connection_id.IsEmpty());
  ASSERT_FALSE(alternative_path->stateless_reset_token.has_value());

  QuicNewConnectionIdFrame frame;
  frame.sequence_number = 1u;
  frame.connection_id = TestConnectionId(5);
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 0u;
  // New ID is patched onto the alternative path when the needed
  // NEW_CONNECTION_ID frame is received after PATH_CHALLENGE frame.
  connection_.OnNewConnectionIdFrame(frame);

  ASSERT_EQ(alternative_path->client_connection_id, frame.connection_id);
  ASSERT_EQ(alternative_path->stateless_reset_token,
            frame.stateless_reset_token);
}

TEST_P(QuicConnectionTest, PatchMissingClientConnectionIdOntoDefaultPath) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  connection_.CreateConnectionIdManager();
  connection_.set_client_connection_id(TestConnectionId(1));

  // Set up the state after peer migration without probing.
  auto* default_path = QuicConnectionPeer::GetDefaultPath(&connection_);
  auto* alternative_path = QuicConnectionPeer::GetAlternativePath(&connection_);
  auto* packet_creator = QuicConnectionPeer::GetPacketCreator(&connection_);
  *alternative_path = std::move(*default_path);
  QuicIpAddress new_host;
  new_host.FromString("12.12.12.12");
  default_path->self_address = default_path->self_address;
  default_path->peer_address = QuicSocketAddress(new_host, 12345);
  default_path->server_connection_id = TestConnectionId(3);
  packet_creator->SetDefaultPeerAddress(default_path->peer_address);
  packet_creator->SetServerConnectionId(default_path->server_connection_id);
  packet_creator->SetClientConnectionId(default_path->client_connection_id);

  ASSERT_FALSE(default_path->validated);
  ASSERT_TRUE(default_path->client_connection_id.IsEmpty());
  ASSERT_FALSE(default_path->stateless_reset_token.has_value());

  QuicNewConnectionIdFrame frame;
  frame.sequence_number = 1u;
  frame.connection_id = TestConnectionId(5);
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 0u;
  // New ID is patched onto the default path when the needed
  // NEW_CONNECTION_ID frame is received after PATH_CHALLENGE frame.
  connection_.OnNewConnectionIdFrame(frame);

  ASSERT_EQ(default_path->client_connection_id, frame.connection_id);
  ASSERT_EQ(default_path->stateless_reset_token, frame.stateless_reset_token);
  ASSERT_EQ(packet_creator->GetDestinationConnectionId(), frame.connection_id);
}

TEST_P(QuicConnectionTest, ShouldGeneratePacketBlockedByMissingConnectionId) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  connection_.set_client_connection_id(TestConnectionId(1));
  connection_.CreateConnectionIdManager();
  if (version().SupportsAntiAmplificationLimit()) {
    QuicConnectionPeer::SetAddressValidated(&connection_);
  }

  ASSERT_TRUE(
      connection_.ShouldGeneratePacket(NO_RETRANSMITTABLE_DATA, NOT_HANDSHAKE));

  QuicPacketCreator* packet_creator =
      QuicConnectionPeer::GetPacketCreator(&connection_);
  QuicIpAddress peer_host1;
  peer_host1.FromString("12.12.12.12");
  QuicSocketAddress peer_address1(peer_host1, 1235);

  {
    // No connection ID is available as context is created without any.
    QuicPacketCreator::ScopedPeerAddressContext context(
        packet_creator, peer_address1, EmptyQuicConnectionId(),
        EmptyQuicConnectionId());
    ASSERT_FALSE(connection_.ShouldGeneratePacket(NO_RETRANSMITTABLE_DATA,
                                                  NOT_HANDSHAKE));
  }
  ASSERT_TRUE(
      connection_.ShouldGeneratePacket(NO_RETRANSMITTABLE_DATA, NOT_HANDSHAKE));
}

// Regression test for b/182571515
TEST_P(QuicConnectionTest, LostDataThenGetAcknowledged) {
  set_perspective(Perspective::IS_SERVER);
  if (!version().SupportsAntiAmplificationLimit() ||
      GetQuicFlag(quic_enforce_strict_amplification_factor)) {
    return;
  }

  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  if (version().SupportsAntiAmplificationLimit()) {
    QuicConnectionPeer::SetAddressValidated(&connection_);
  }
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  // Discard INITIAL key.
  connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
  connection_.NeuterUnencryptedPackets();
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));

  QuicPacketNumber last_packet;
  // Send packets 1 to 4.
  SendStreamDataToPeer(3, "foo", 0, NO_FIN, &last_packet);  // Packet 1
  SendStreamDataToPeer(3, "foo", 3, NO_FIN, &last_packet);  // Packet 2
  SendStreamDataToPeer(3, "foo", 6, NO_FIN, &last_packet);  // Packet 3
  SendStreamDataToPeer(3, "foo", 9, NO_FIN, &last_packet);  // Packet 4

  // Process a PING packet to set peer address.
  ProcessFramePacket(QuicFrame(QuicPingFrame()));

  // Process a packet containing a STREAM_FRAME and an ACK with changed peer
  // address.
  QuicFrames frames;
  frames.push_back(QuicFrame(frame1_));
  QuicAckFrame ack = InitAckFrame({{QuicPacketNumber(1), QuicPacketNumber(5)}});
  frames.push_back(QuicFrame(&ack));

  // Invoke OnCanWrite.
  QuicIpAddress ip_address;
  ASSERT_TRUE(ip_address.FromString("127.0.52.223"));
  EXPECT_QUIC_BUG(
      {
        EXPECT_CALL(visitor_, OnConnectionMigration(_)).Times(1);
        EXPECT_CALL(visitor_, OnStreamFrame(_))
            .WillOnce(InvokeWithoutArgs(&notifier_,
                                        &SimpleSessionNotifier::OnCanWrite));
        ProcessFramesPacketWithAddresses(frames, kSelfAddress,
                                         QuicSocketAddress(ip_address, 1000),
                                         ENCRYPTION_FORWARD_SECURE);
        EXPECT_EQ(1u, writer_->path_challenge_frames().size());

        // Verify stream frame will not be retransmitted.
        EXPECT_TRUE(writer_->stream_frames().empty());
      },
      "Try to write mid packet processing");
}

TEST_P(QuicConnectionTest, PtoSendStreamData) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  if (QuicVersionUsesCryptoFrames(connection_.transport_version())) {
    EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  }
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.HasPendingAcks());

  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  // Send INITIAL 1.
  connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_INITIAL);

  connection_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
  SetDecrypter(ENCRYPTION_HANDSHAKE,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_HANDSHAKE));
  // Send HANDSHAKE packets.
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(1);
  connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_HANDSHAKE);

  connection_.SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);

  // Send half RTT packet with congestion control blocked.
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(false));
  connection_.SendStreamDataWithString(2, std::string(1500, 'a'), 0, NO_FIN);

  ASSERT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  connection_.GetRetransmissionAlarm()->Fire();
  // Verify INITIAL and HANDSHAKE get retransmitted.
  EXPECT_EQ(0x01010101u, writer_->final_bytes_of_last_packet());
}

TEST_P(QuicConnectionTest, SendingZeroRttPacketsDoesNotPostponePTO) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  // Send CHLO.
  connection_.SendCryptoStreamData();
  ASSERT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  // Install 0-RTT keys.
  connection_.SetEncrypter(ENCRYPTION_ZERO_RTT,
                           std::make_unique<TaggingEncrypter>(0x02));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);

  // CHLO gets acknowledged after 10ms.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  QuicAckFrame frame1 = InitAckFrame(1);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _));
  ProcessFramePacketAtLevel(1, QuicFrame(&frame1), ENCRYPTION_INITIAL);
  // Verify PTO is still armed since address validation is not finished yet.
  ASSERT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  QuicTime pto_deadline = connection_.GetRetransmissionAlarm()->deadline();

  // Send 0-RTT packet.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  connection_.SetEncrypter(ENCRYPTION_ZERO_RTT,
                           std::make_unique<TaggingEncrypter>(0x02));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);
  connection_.SendStreamDataWithString(2, "foo", 0, NO_FIN);
  ASSERT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  // PTO deadline should be unchanged.
  EXPECT_EQ(pto_deadline, connection_.GetRetransmissionAlarm()->deadline());
}

TEST_P(QuicConnectionTest, QueueingUndecryptablePacketsDoesntPostponePTO) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  config.set_max_undecryptable_packets(3);
  connection_.SetFromConfig(config);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  connection_.RemoveDecrypter(ENCRYPTION_FORWARD_SECURE);
  // Send CHLO.
  connection_.SendCryptoStreamData();

  // Send 0-RTT packet.
  connection_.SetEncrypter(ENCRYPTION_ZERO_RTT,
                           std::make_unique<TaggingEncrypter>(0x02));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);
  connection_.SendStreamDataWithString(2, "foo", 0, NO_FIN);

  // CHLO gets acknowledged after 10ms.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  QuicAckFrame frame1 = InitAckFrame(1);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _));
  ProcessFramePacketAtLevel(1, QuicFrame(&frame1), ENCRYPTION_INITIAL);
  // Verify PTO is still armed since address validation is not finished yet.
  ASSERT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  QuicTime pto_deadline = connection_.GetRetransmissionAlarm()->deadline();

  // Receive an undecryptable packets.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  peer_framer_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                            std::make_unique<TaggingEncrypter>(0xFF));
  ProcessDataPacketAtLevel(3, !kHasStopWaiting, ENCRYPTION_FORWARD_SECURE);
  // Verify PTO deadline is sooner.
  EXPECT_GT(pto_deadline, connection_.GetRetransmissionAlarm()->deadline());
  pto_deadline = connection_.GetRetransmissionAlarm()->deadline();

  // PTO fires.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  clock_.AdvanceTime(pto_deadline - clock_.ApproximateNow());
  connection_.GetRetransmissionAlarm()->Fire();
  // Verify PTO is still armed since address validation is not finished yet.
  ASSERT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  pto_deadline = connection_.GetRetransmissionAlarm()->deadline();

  // Verify PTO deadline does not change.
  ProcessDataPacketAtLevel(4, !kHasStopWaiting, ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(pto_deadline, connection_.GetRetransmissionAlarm()->deadline());
}

TEST_P(QuicConnectionTest, QueueUndecryptableHandshakePackets) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  config.set_max_undecryptable_packets(3);
  connection_.SetFromConfig(config);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  connection_.RemoveDecrypter(ENCRYPTION_HANDSHAKE);
  // Send CHLO.
  connection_.SendCryptoStreamData();

  // Send 0-RTT packet.
  connection_.SetEncrypter(ENCRYPTION_ZERO_RTT,
                           std::make_unique<TaggingEncrypter>(0x02));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);
  connection_.SendStreamDataWithString(2, "foo", 0, NO_FIN);
  EXPECT_EQ(0u, QuicConnectionPeer::NumUndecryptablePackets(&connection_));

  // Receive an undecryptable handshake packet.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  peer_framer_.SetEncrypter(ENCRYPTION_HANDSHAKE,
                            std::make_unique<TaggingEncrypter>(0xFF));
  ProcessDataPacketAtLevel(3, !kHasStopWaiting, ENCRYPTION_HANDSHAKE);
  // Verify this handshake packet gets queued.
  EXPECT_EQ(1u, QuicConnectionPeer::NumUndecryptablePackets(&connection_));
}

TEST_P(QuicConnectionTest, PingNotSentAt0RTTLevelWhenInitialAvailable) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  // Send CHLO.
  connection_.SendCryptoStreamData();
  // Send 0-RTT packet.
  connection_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);
  connection_.SendStreamDataWithString(2, "foo", 0, NO_FIN);

  // CHLO gets acknowledged after 10ms.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  QuicAckFrame frame1 = InitAckFrame(1);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _));
  ProcessFramePacketAtLevel(1, QuicFrame(&frame1), ENCRYPTION_INITIAL);
  // Verify PTO is still armed since address validation is not finished yet.
  ASSERT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  QuicTime pto_deadline = connection_.GetRetransmissionAlarm()->deadline();

  // PTO fires.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  clock_.AdvanceTime(pto_deadline - clock_.ApproximateNow());
  connection_.GetRetransmissionAlarm()->Fire();
  // Verify the PING gets sent in ENCRYPTION_INITIAL.
  EXPECT_NE(0x02020202u, writer_->final_bytes_of_last_packet());
}

TEST_P(QuicConnectionTest, AckElicitingFrames) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  config.SetReliableStreamReset(true);
  connection_.SetFromConfig(config);

  EXPECT_CALL(connection_id_generator_,
              GenerateNextConnectionId(TestConnectionId(12)))
      .WillOnce(Return(TestConnectionId(456)));
  EXPECT_CALL(connection_id_generator_,
              GenerateNextConnectionId(TestConnectionId(456)))
      .WillOnce(Return(TestConnectionId(789)));
  EXPECT_CALL(visitor_, SendNewConnectionId(_)).Times(2);
  EXPECT_CALL(visitor_, OnRstStream(_));
  EXPECT_CALL(visitor_, OnResetStreamAt(_));
  EXPECT_CALL(visitor_, OnWindowUpdateFrame(_));
  EXPECT_CALL(visitor_, OnBlockedFrame(_));
  EXPECT_CALL(visitor_, OnHandshakeDoneReceived());
  EXPECT_CALL(visitor_, OnStreamFrame(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _));
  EXPECT_CALL(visitor_, OnMaxStreamsFrame(_));
  EXPECT_CALL(visitor_, OnStreamsBlockedFrame(_));
  EXPECT_CALL(visitor_, OnStopSendingFrame(_));
  EXPECT_CALL(visitor_, OnMessageReceived(""));
  EXPECT_CALL(visitor_, OnNewTokenReceived(""));

  SetClientConnectionId(TestConnectionId(12));
  connection_.CreateConnectionIdManager();
  QuicConnectionPeer::GetSelfIssuedConnectionIdManager(&connection_)
      ->MaybeSendNewConnectionIds();
  connection_.set_can_receive_ack_frequency_frame();
  connection_.set_can_receive_ack_frequency_immediate_ack(true);

  QuicAckFrame ack_frame = InitAckFrame(1);
  QuicRstStreamFrame rst_stream_frame;
  QuicWindowUpdateFrame window_update_frame;
  QuicPathChallengeFrame path_challenge_frame;
  QuicNewConnectionIdFrame new_connection_id_frame;
  new_connection_id_frame.sequence_number = 1u;
  QuicRetireConnectionIdFrame retire_connection_id_frame;
  retire_connection_id_frame.sequence_number = 1u;
  QuicStopSendingFrame stop_sending_frame;
  QuicPathResponseFrame path_response_frame;
  QuicMessageFrame message_frame;
  QuicNewTokenFrame new_token_frame;
  QuicAckFrequencyFrame ack_frequency_frame;
  QuicResetStreamAtFrame reset_stream_at_frame;
  QuicBlockedFrame blocked_frame;
  size_t packet_number = 1;

  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  QuicFramer* framer = const_cast<QuicFramer*>(&connection_.framer());
  framer->set_process_reset_stream_at(true);
  peer_framer_.set_process_reset_stream_at(true);

  for (uint8_t i = 0; i < NUM_FRAME_TYPES; ++i) {
    QuicFrameType frame_type = static_cast<QuicFrameType>(i);
    bool skipped = false;
    QuicFrame frame;
    QuicFrames frames;
    // Add some padding to fullfill the min size requirement of header
    // protection.
    frames.push_back(QuicFrame(QuicPaddingFrame(10)));
    switch (frame_type) {
      case PADDING_FRAME:
        frame = QuicFrame(QuicPaddingFrame(10));
        break;
      case MTU_DISCOVERY_FRAME:
        frame = QuicFrame(QuicMtuDiscoveryFrame());
        break;
      case PING_FRAME:
        frame = QuicFrame(QuicPingFrame());
        break;
      case MAX_STREAMS_FRAME:
        frame = QuicFrame(QuicMaxStreamsFrame());
        break;
      case STOP_WAITING_FRAME:
        // Not supported.
        skipped = true;
        break;
      case STREAMS_BLOCKED_FRAME:
        frame = QuicFrame(QuicStreamsBlockedFrame());
        break;
      case STREAM_FRAME:
        frame = QuicFrame(QuicStreamFrame());
        break;
      case HANDSHAKE_DONE_FRAME:
        frame = QuicFrame(QuicHandshakeDoneFrame());
        break;
      case ACK_FRAME:
        frame = QuicFrame(&ack_frame);
        break;
      case RST_STREAM_FRAME:
        frame = QuicFrame(&rst_stream_frame);
        break;
      case CONNECTION_CLOSE_FRAME:
        // Do not test connection close.
        skipped = true;
        break;
      case GOAWAY_FRAME:
        // Does not exist in IETF QUIC.
        skipped = true;
        break;
      case BLOCKED_FRAME:
        frame = QuicFrame(blocked_frame);
        break;
      case WINDOW_UPDATE_FRAME:
        frame = QuicFrame(window_update_frame);
        break;
      case PATH_CHALLENGE_FRAME:
        frame = QuicFrame(path_challenge_frame);
        break;
      case STOP_SENDING_FRAME:
        frame = QuicFrame(stop_sending_frame);
        break;
      case NEW_CONNECTION_ID_FRAME:
        frame = QuicFrame(&new_connection_id_frame);
        break;
      case RETIRE_CONNECTION_ID_FRAME:
        frame = QuicFrame(&retire_connection_id_frame);
        break;
      case PATH_RESPONSE_FRAME:
        frame = QuicFrame(path_response_frame);
        break;
      case MESSAGE_FRAME:
        frame = QuicFrame(&message_frame);
        break;
      case CRYPTO_FRAME:
        // CRYPTO_FRAME is ack eliciting is covered by other tests.
        skipped = true;
        break;
      case NEW_TOKEN_FRAME:
        frame = QuicFrame(&new_token_frame);
        break;
      case ACK_FREQUENCY_FRAME:
        frame = QuicFrame(&ack_frequency_frame);
        break;
      case IMMEDIATE_ACK_FRAME:
        frame = QuicFrame(QuicImmediateAckFrame());
        break;
      case RESET_STREAM_AT_FRAME:
        frame = QuicFrame(&reset_stream_at_frame);
        break;
      case NUM_FRAME_TYPES:
        skipped = true;
        break;
    }
    if (skipped) {
      continue;
    }
    ASSERT_EQ(frame_type, frame.type);
    frames.push_back(frame);
    EXPECT_FALSE(connection_.HasPendingAcks());
    // Process frame.
    ProcessFramesPacketAtLevel(packet_number++, frames,
                               ENCRYPTION_FORWARD_SECURE);
    if (QuicUtils::IsAckElicitingFrame(frame_type)) {
      if (frame_type != IMMEDIATE_ACK_FRAME) {
        ASSERT_TRUE(connection_.HasPendingAcks()) << frame;
        // Flush ACK.
        clock_.AdvanceTime(DefaultDelayedAckTime());
        connection_.GetAckAlarm()->Fire();
      }
      EXPECT_FALSE(writer_->ack_frames().empty());
      writer_->framer()->Reset();  // Clear the visitor.
    }
    EXPECT_FALSE(connection_.HasPendingAcks());
    ASSERT_TRUE(connection_.connected());
  }
}

TEST_P(QuicConnectionTest, ImmediateAckOverridesOtherFrame) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  QuicFrames frames;
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  connection_.set_can_receive_ack_frequency_immediate_ack(true);
  // A PING frame will set the ack timer. IMMEDIATE_ACK should override it to
  // send an ACK immediately.
  frames.push_back(QuicFrame(QuicPingFrame()));
  frames.push_back(QuicFrame(QuicImmediateAckFrame()));
  writer_->framer()->Reset();  // Clear the visitor.
  EXPECT_TRUE(writer_->ack_frames().empty());
  ProcessFramesPacketAtLevel(1, frames, ENCRYPTION_FORWARD_SECURE);
  EXPECT_FALSE(writer_->ack_frames().empty());
}

TEST_P(QuicConnectionTest, ReceivedChloAndAck) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  QuicFrames frames;
  QuicAckFrame ack_frame = InitAckFrame(1);
  frames.push_back(MakeCryptoFrame());
  frames.push_back(QuicFrame(&ack_frame));

  EXPECT_CALL(visitor_, OnCryptoFrame(_))
      .WillOnce(IgnoreResult(InvokeWithoutArgs(
          &connection_, &TestConnection::SendCryptoStreamData)));
  EXPECT_CALL(visitor_, BeforeConnectionCloseSent());
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
  ProcessFramesPacketWithAddresses(frames, kSelfAddress, kPeerAddress,
                                   ENCRYPTION_INITIAL);
}

// Regression test for b/201643321.
TEST_P(QuicConnectionTest, FailedToRetransmitShlo) {
  if (!version().SupportsAntiAmplificationLimit() ||
      GetQuicFlag(quic_enforce_strict_amplification_factor)) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  // Received INITIAL 1.
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.HasPendingAcks());

  peer_framer_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));

  connection_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  SetDecrypter(ENCRYPTION_HANDSHAKE,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_HANDSHAKE));
  SetDecrypter(ENCRYPTION_ZERO_RTT,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_ZERO_RTT));
  connection_.SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
  // Received ENCRYPTION_ZERO_RTT 1.
  ProcessDataPacketAtLevel(1, !kHasStopWaiting, ENCRYPTION_ZERO_RTT);
  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    // Send INITIAL 1.
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
    connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_INITIAL);
    // Send HANDSHAKE 2.
    EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(1);
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
    connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_HANDSHAKE);
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    // Send half RTT data to exhaust amplification credit.
    connection_.SendStreamDataWithString(0, std::string(100 * 1024, 'a'), 0,
                                         NO_FIN);
  }
  // Received INITIAL 2.
  ProcessCryptoPacketAtLevel(2, ENCRYPTION_INITIAL);
  ASSERT_TRUE(connection_.HasPendingAcks());
  // Verify ACK delay is 1ms.
  EXPECT_EQ(clock_.Now() + kAlarmGranularity,
            connection_.GetAckAlarm()->deadline());
  // ACK is not throttled by amplification limit, and SHLO is bundled. Also
  // HANDSHAKE + 1RTT packets get coalesced.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(3);
  // ACK alarm fires.
  clock_.AdvanceTime(kAlarmGranularity);
  connection_.GetAckAlarm()->Fire();
  // Verify 1-RTT packet is coalesced.
  EXPECT_EQ(0x03030303u, writer_->final_bytes_of_last_packet());
  // Only the first packet in the coalesced packet has been processed,
  // verify SHLO is bundled with INITIAL ACK.
  EXPECT_EQ(1u, writer_->ack_frames().size());
  EXPECT_EQ(1u, writer_->crypto_frames().size());
  // Process the coalesced HANDSHAKE packet.
  ASSERT_TRUE(writer_->coalesced_packet() != nullptr);
  auto packet = writer_->coalesced_packet()->Clone();
  writer_->framer()->ProcessPacket(*packet);
  EXPECT_EQ(0u, writer_->ack_frames().size());
  EXPECT_EQ(1u, writer_->crypto_frames().size());
  // Process the coalesced 1-RTT packet.
  ASSERT_TRUE(writer_->coalesced_packet() != nullptr);
  packet = writer_->coalesced_packet()->Clone();
  writer_->framer()->ProcessPacket(*packet);
  EXPECT_EQ(0u, writer_->crypto_frames().size());
  EXPECT_EQ(1u, writer_->stream_frames().size());

  // Received INITIAL 3.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AnyNumber());
  ProcessCryptoPacketAtLevel(3, ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.HasPendingAcks());
}

// Regression test for b/216133388.
TEST_P(QuicConnectionTest, FailedToConsumeCryptoData) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  // Received INITIAL 1.
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_INITIAL);
  EXPECT_TRUE(connection_.HasPendingAcks());

  peer_framer_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
  connection_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  SetDecrypter(ENCRYPTION_HANDSHAKE,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_HANDSHAKE));
  SetDecrypter(ENCRYPTION_ZERO_RTT,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_ZERO_RTT));
  connection_.SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
  // Received ENCRYPTION_ZERO_RTT 1.
  ProcessDataPacketAtLevel(1, !kHasStopWaiting, ENCRYPTION_ZERO_RTT);
  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    // Send INITIAL 1.
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
    connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_INITIAL);
    // Send HANDSHAKE 2.
    EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(1);
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
    connection_.SendCryptoDataWithString(std::string(200, 'a'), 0,
                                         ENCRYPTION_HANDSHAKE);
    // Send 1-RTT 3.
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    connection_.SendStreamDataWithString(0, std::string(40, 'a'), 0, NO_FIN);
  }
  // Received HANDSHAKE Ping, hence discard INITIAL keys.
  peer_framer_.SetEncrypter(ENCRYPTION_HANDSHAKE,
                            std::make_unique<TaggingEncrypter>(0x03));
  connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
  connection_.NeuterUnencryptedPackets();
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_HANDSHAKE);
  clock_.AdvanceTime(kAlarmGranularity);
  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    // Sending this 1-RTT data would leave the coalescer only have space to
    // accommodate the HANDSHAKE ACK. The crypto data cannot be bundled with the
    // ACK.
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    connection_.SendStreamDataWithString(0, std::string(1395, 'a'), 40, NO_FIN);
  }
  // Verify retransmission alarm is armed.
  ASSERT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  const QuicTime retransmission_time =
      connection_.GetRetransmissionAlarm()->deadline();
  clock_.AdvanceTime(retransmission_time - clock_.Now());
  connection_.GetRetransmissionAlarm()->Fire();

  // Verify the retransmission is a coalesced packet with HANDSHAKE 2 and
  // 1-RTT 3.
  EXPECT_EQ(0x03030303u, writer_->final_bytes_of_last_packet());
  // Only the first packet in the coalesced packet has been processed.
  EXPECT_EQ(1u, writer_->crypto_frames().size());
  // Process the coalesced 1-RTT packet.
  ASSERT_TRUE(writer_->coalesced_packet() != nullptr);
  auto packet = writer_->coalesced_packet()->Clone();
  writer_->framer()->ProcessPacket(*packet);
  EXPECT_EQ(1u, writer_->stream_frames().size());
  ASSERT_TRUE(writer_->coalesced_packet() == nullptr);
  // Verify retransmission alarm is still armed.
  ASSERT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
}

TEST_P(QuicConnectionTest,
       RTTSampleDoesNotIncludeQueuingDelayWithPostponedAckProcessing) {
  // An endpoint might postpone the processing of ACK when the corresponding
  // decryption key is not available. This test makes sure the RTT sample does
  // not include the queuing delay.
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  config.set_max_undecryptable_packets(3);
  connection_.SetFromConfig(config);

  // 30ms RTT.
  const QuicTime::Delta kTestRTT = QuicTime::Delta::FromMilliseconds(30);
  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  rtt_stats->UpdateRtt(kTestRTT, QuicTime::Delta::Zero(), QuicTime::Zero());

  // Send 0-RTT packet.
  connection_.RemoveDecrypter(ENCRYPTION_FORWARD_SECURE);
  connection_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);
  connection_.SendStreamDataWithString(0, std::string(10, 'a'), 0, FIN);

  // Receives 1-RTT ACK for 0-RTT packet after RTT + ack_delay.
  clock_.AdvanceTime(kTestRTT + QuicTime::Delta::FromMilliseconds(
                                    GetDefaultDelayedAckTimeMs()));
  EXPECT_EQ(0u, QuicConnectionPeer::NumUndecryptablePackets(&connection_));
  peer_framer_.SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
  QuicAckFrame ack_frame = InitAckFrame(1);
  // Peer reported ACK delay.
  ack_frame.ack_delay_time =
      QuicTime::Delta::FromMilliseconds(GetDefaultDelayedAckTimeMs());
  QuicFrames frames;
  frames.push_back(QuicFrame(&ack_frame));
  QuicPacketHeader header =
      ConstructPacketHeader(30, ENCRYPTION_FORWARD_SECURE);
  std::unique_ptr<QuicPacket> packet(ConstructPacket(header, frames));

  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length = peer_framer_.EncryptPayload(
      ENCRYPTION_FORWARD_SECURE, QuicPacketNumber(30), *packet, buffer,
      kMaxOutgoingPacketSize);
  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(buffer, encrypted_length, clock_.Now(), false));
  if (connection_.GetSendAlarm()->IsSet()) {
    connection_.GetSendAlarm()->Fire();
  }
  ASSERT_EQ(1u, QuicConnectionPeer::NumUndecryptablePackets(&connection_));

  // Assume 1-RTT decrypter is available after 10ms.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  EXPECT_FALSE(connection_.GetProcessUndecryptablePacketsAlarm()->IsSet());
  SetDecrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_FORWARD_SECURE));
  ASSERT_TRUE(connection_.GetProcessUndecryptablePacketsAlarm()->IsSet());

  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _));
  connection_.GetProcessUndecryptablePacketsAlarm()->Fire();
  // Verify RTT sample does not include queueing delay.
  EXPECT_EQ(rtt_stats->latest_rtt(), kTestRTT);
}

// Regression test for b/112480134.
TEST_P(QuicConnectionTest, NoExtraPaddingInReserializedInitial) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (!IsDefaultTestConfiguration() ||
      !connection_.version().CanSendCoalescedPackets()) {
    return;
  }

  set_perspective(Perspective::IS_SERVER);
  MockQuicConnectionDebugVisitor debug_visitor;
  connection_.set_debug_visitor(&debug_visitor);

  uint64_t debug_visitor_sent_count = 0;
  EXPECT_CALL(debug_visitor, OnPacketSent(_, _, _, _, _, _, _, _, _))
      .WillRepeatedly([&]() { debug_visitor_sent_count++; });

  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());

  // Received INITIAL 1.
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_INITIAL);

  peer_framer_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
  connection_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  SetDecrypter(ENCRYPTION_HANDSHAKE,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_HANDSHAKE));
  SetDecrypter(ENCRYPTION_ZERO_RTT,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_ZERO_RTT));
  connection_.SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));

  // Received ENCRYPTION_ZERO_RTT 2.
  ProcessDataPacketAtLevel(2, !kHasStopWaiting, ENCRYPTION_ZERO_RTT);

  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    // Send INITIAL 1.
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
    connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_INITIAL);
    // Send HANDSHAKE 2.
    EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(1);
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
    connection_.SendCryptoDataWithString(std::string(200, 'a'), 0,
                                         ENCRYPTION_HANDSHAKE);
    // Send 1-RTT 3.
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    connection_.SendStreamDataWithString(0, std::string(400, 'b'), 0, NO_FIN);
  }

  // Arrange the stream data to be sent in response to ENCRYPTION_INITIAL 3.
  const std::string data4(1000, '4');  // Data to send in stream id 4
  const std::string data8(3000, '8');  // Data to send in stream id 8
  EXPECT_CALL(visitor_, OnCanWrite()).WillOnce([&]() {
    connection_.producer()->SaveStreamData(4, data4);
    connection_.producer()->SaveStreamData(8, data8);

    notifier_.WriteOrBufferData(4, data4.size(), FIN_AND_PADDING);

    // This should trigger FlushCoalescedPacket.
    notifier_.WriteOrBufferData(8, data8.size(), FIN);
  });

  QuicByteCount pending_padding_after_serialize_2nd_1rtt_packet = 0;
  QuicPacketCount num_1rtt_packets_serialized = 0;
  EXPECT_CALL(connection_, OnSerializedPacket(_))
      .WillRepeatedly([&](SerializedPacket packet) {
        if (packet.encryption_level == ENCRYPTION_FORWARD_SECURE) {
          num_1rtt_packets_serialized++;
          if (num_1rtt_packets_serialized == 2) {
            pending_padding_after_serialize_2nd_1rtt_packet =
                connection_.packet_creator().pending_padding_bytes();
          }
        }
        connection_.QuicConnection::OnSerializedPacket(std::move(packet));
      });

  // Server receives INITIAL 3, this will serialzie FS 7 (stream 4, stream 8),
  // which will trigger a flush of a coalesced packet consists of INITIAL 4,
  // HS 5 and FS 6 (stream 4).

  // Expect no QUIC_BUG.
  ProcessDataPacketAtLevel(3, !kHasStopWaiting, ENCRYPTION_INITIAL);
  EXPECT_EQ(
      debug_visitor_sent_count,
      connection_.sent_packet_manager().GetLargestSentPacket().ToUint64());

  // The error only happens if after serializing the second 1RTT packet(pkt #7),
  // the pending padding bytes is non zero.
  EXPECT_GT(pending_padding_after_serialize_2nd_1rtt_packet, 0u);
  EXPECT_TRUE(connection_.connected());
}

TEST_P(QuicConnectionTest, ReportedAckDelayIncludesQueuingDelay) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  config.set_max_undecryptable_packets(3);
  connection_.SetFromConfig(config);

  // Receive 1-RTT ack-eliciting packet while keys are not available.
  connection_.RemoveDecrypter(ENCRYPTION_FORWARD_SECURE);
  peer_framer_.SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
  QuicFrames frames;
  frames.push_back(QuicFrame(QuicPingFrame()));
  frames.push_back(QuicFrame(QuicPaddingFrame(100)));
  QuicPacketHeader header =
      ConstructPacketHeader(30, ENCRYPTION_FORWARD_SECURE);
  std::unique_ptr<QuicPacket> packet(ConstructPacket(header, frames));

  char buffer[kMaxOutgoingPacketSize];
  size_t encrypted_length = peer_framer_.EncryptPayload(
      ENCRYPTION_FORWARD_SECURE, QuicPacketNumber(30), *packet, buffer,
      kMaxOutgoingPacketSize);
  EXPECT_EQ(0u, QuicConnectionPeer::NumUndecryptablePackets(&connection_));
  const QuicTime packet_receipt_time = clock_.Now();
  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(buffer, encrypted_length, clock_.Now(), false));
  if (connection_.GetSendAlarm()->IsSet()) {
    connection_.GetSendAlarm()->Fire();
  }
  ASSERT_EQ(1u, QuicConnectionPeer::NumUndecryptablePackets(&connection_));
  // 1-RTT keys become available after 10ms.
  const QuicTime::Delta kQueuingDelay = QuicTime::Delta::FromMilliseconds(10);
  clock_.AdvanceTime(kQueuingDelay);
  EXPECT_FALSE(connection_.GetProcessUndecryptablePacketsAlarm()->IsSet());
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  SetDecrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_FORWARD_SECURE));
  ASSERT_TRUE(connection_.GetProcessUndecryptablePacketsAlarm()->IsSet());

  connection_.GetProcessUndecryptablePacketsAlarm()->Fire();
  ASSERT_TRUE(connection_.HasPendingAcks());
  EXPECT_EQ(packet_receipt_time + DefaultDelayedAckTime(),
            connection_.GetAckAlarm()->deadline());
  clock_.AdvanceTime(packet_receipt_time + DefaultDelayedAckTime() -
                     clock_.Now());
  // Fire ACK alarm.
  connection_.GetAckAlarm()->Fire();
  ASSERT_EQ(1u, writer_->ack_frames().size());
  // Verify ACK delay time does not include queuing delay.
  EXPECT_EQ(DefaultDelayedAckTime(), writer_->ack_frames()[0].ack_delay_time);
}

TEST_P(QuicConnectionTest, CoalesceOneRTTPacketWithInitialAndHandshakePackets) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());

  // Received INITIAL 1.
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_INITIAL);

  peer_framer_.SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));

  connection_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  SetDecrypter(ENCRYPTION_HANDSHAKE,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_HANDSHAKE));
  SetDecrypter(ENCRYPTION_ZERO_RTT,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_ZERO_RTT));
  connection_.SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));

  // Received ENCRYPTION_ZERO_RTT 2.
  ProcessDataPacketAtLevel(2, !kHasStopWaiting, ENCRYPTION_ZERO_RTT);

  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    // Send INITIAL 1.
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
    connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_INITIAL);
    // Send HANDSHAKE 2.
    EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(1);
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
    connection_.SendCryptoDataWithString(std::string(200, 'a'), 0,
                                         ENCRYPTION_HANDSHAKE);
    // Send 1-RTT data.
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    connection_.SendStreamDataWithString(0, std::string(2000, 'b'), 0, FIN);
  }
  // Verify coalesced packet [INITIAL 1 + HANDSHAKE 2 + part of 1-RTT data] +
  // rest of 1-RTT data get sent.
  EXPECT_EQ(2u, writer_->packets_write_attempts());

  // Received ENCRYPTION_INITIAL 3.
  ProcessDataPacketAtLevel(3, !kHasStopWaiting, ENCRYPTION_INITIAL);

  // Verify a coalesced packet gets sent.
  EXPECT_EQ(3u, writer_->packets_write_attempts());

  // Only the first INITIAL packet has been processed yet.
  EXPECT_EQ(1u, writer_->ack_frames().size());
  EXPECT_EQ(1u, writer_->crypto_frames().size());

  // Process HANDSHAKE packet.
  ASSERT_TRUE(writer_->coalesced_packet() != nullptr);
  auto packet = writer_->coalesced_packet()->Clone();
  writer_->framer()->ProcessPacket(*packet);
  EXPECT_EQ(1u, writer_->crypto_frames().size());
  // Process 1-RTT packet.
  ASSERT_TRUE(writer_->coalesced_packet() != nullptr);
  packet = writer_->coalesced_packet()->Clone();
  writer_->framer()->ProcessPacket(*packet);
  EXPECT_EQ(1u, writer_->stream_frames().size());
}

// Regression test for b/180103273
TEST_P(QuicConnectionTest, SendMultipleConnectionCloses) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (!IsDefaultTestConfiguration()) {
    return;
  }
  if (!version().HasIetfQuicFrames() ||
      !GetQuicReloadableFlag(quic_default_enable_5rto_blackhole_detection2)) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  // Finish handshake.
  QuicConnectionPeer::SetAddressValidated(&connection_);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  notifier_.NeuterUnencryptedData();
  connection_.NeuterUnencryptedPackets();
  connection_.OnHandshakeComplete();
  connection_.RemoveEncrypter(ENCRYPTION_INITIAL);
  connection_.RemoveEncrypter(ENCRYPTION_HANDSHAKE);
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));

  SendStreamDataToPeer(1, "foo", 0, NO_FIN, nullptr);
  ASSERT_TRUE(connection_.BlackholeDetectionInProgress());
  // Verify that BeforeConnectionCloseSent() gets called twice,
  // while OnConnectionClosed() is called only once.
  EXPECT_CALL(visitor_, BeforeConnectionCloseSent()).Times(2);
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
  // Send connection close w/o closing connection.
  QuicConnectionPeer::SendConnectionClosePacket(
      &connection_, INTERNAL_ERROR, QUIC_INTERNAL_ERROR, "internal error");
  // Fire blackhole detection alarm.  This will invoke
  // SendConnectionClosePacket() a second time.
  EXPECT_QUIC_BUG(connection_.GetBlackholeDetectorAlarm()->Fire(),
                  // 1=QUIC_INTERNAL_ERROR, 85=QUIC_TOO_MANY_RTOS.
                  "Initial error code: 1, new error code: 85");
}

// Regression test for b/157895910.
TEST_P(QuicConnectionTest, EarliestSentTimeNotInitializedWhenPtoFires) {
  if (!connection_.SupportsMultiplePacketNumberSpaces()) {
    return;
  }
  set_perspective(Perspective::IS_SERVER);
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(AnyNumber());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());

  // Received INITIAL 1.
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_INITIAL);
  connection_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  SetDecrypter(ENCRYPTION_HANDSHAKE,
               std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_HANDSHAKE));
  connection_.SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_);
    // Send INITIAL 1.
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
    connection_.SendCryptoDataWithString("foo", 0, ENCRYPTION_INITIAL);
    // Send HANDSHAKE 2.
    EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(1);
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
    connection_.SendCryptoDataWithString(std::string(200, 'a'), 0,
                                         ENCRYPTION_HANDSHAKE);
    // Send half RTT data.
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    connection_.SendStreamDataWithString(0, std::string(2000, 'b'), 0, FIN);
  }

  // Received ACKs for both INITIAL and HANDSHAKE packets.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _))
      .Times(AnyNumber());
  QuicFrames frames1;
  QuicAckFrame ack_frame1 = InitAckFrame(1);
  frames1.push_back(QuicFrame(&ack_frame1));

  QuicFrames frames2;
  QuicAckFrame ack_frame2 =
      InitAckFrame({{QuicPacketNumber(2), QuicPacketNumber(3)}});
  frames2.push_back(QuicFrame(&ack_frame2));
  ProcessCoalescedPacket(
      {{2, frames1, ENCRYPTION_INITIAL}, {3, frames2, ENCRYPTION_HANDSHAKE}});
  // Verify PTO is not armed given the only outstanding data is half RTT data.
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, CalculateNetworkBlackholeDelay) {
  if (!IsDefaultTestConfiguration()) {
    return;
  }

  const QuicTime::Delta kOneSec = QuicTime::Delta::FromSeconds(1);
  const QuicTime::Delta kTwoSec = QuicTime::Delta::FromSeconds(2);
  const QuicTime::Delta kFourSec = QuicTime::Delta::FromSeconds(4);

  // Normal case: blackhole_delay longer than path_degrading_delay +
  // 2*pto_delay.
  EXPECT_EQ(QuicConnection::CalculateNetworkBlackholeDelay(kFourSec, kOneSec,
                                                           kOneSec),
            kFourSec);

  EXPECT_EQ(QuicConnection::CalculateNetworkBlackholeDelay(kFourSec, kOneSec,
                                                           kTwoSec),
            QuicTime::Delta::FromSeconds(5));
}

TEST_P(QuicConnectionTest, FixBytesAccountingForBufferedCoalescedPackets) {
  if (!connection_.version().CanSendCoalescedPackets()) {
    return;
  }
  // Write is blocked.
  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(AnyNumber());
  writer_->SetWriteBlocked();
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  QuicConnectionPeer::SendPing(&connection_);
  const QuicConnectionStats& stats = connection_.GetStats();
  // Verify padding is accounted.
  EXPECT_EQ(stats.bytes_sent, connection_.max_packet_length());
}

TEST_P(QuicConnectionTest, StrictAntiAmplificationLimit) {
  if (!connection_.version().SupportsAntiAmplificationLimit()) {
    return;
  }
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(AnyNumber());
  set_perspective(Perspective::IS_SERVER);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  // Verify no data can be sent at the beginning because bytes received is 0.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  connection_.SendCryptoDataWithString("foo", 0);
  EXPECT_FALSE(connection_.CanWrite(HAS_RETRANSMITTABLE_DATA));
  EXPECT_FALSE(connection_.CanWrite(NO_RETRANSMITTABLE_DATA));
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  const size_t anti_amplification_factor =
      GetQuicFlag(quic_anti_amplification_factor);
  // Receives packet 1.
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(1);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(anti_amplification_factor);
  ForceWillingAndAbleToWriteOnceForDeferSending();
  ProcessCryptoPacketAtLevel(1, ENCRYPTION_INITIAL);
  connection_.SetEncrypter(ENCRYPTION_HANDSHAKE,
                           std::make_unique<TaggingEncrypter>(0x02));
  connection_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<TaggingEncrypter>(0x03));

  for (size_t i = 1; i < anti_amplification_factor - 1; ++i) {
    connection_.SendCryptoDataWithString("foo", i * 3);
  }
  // Send an addtion packet with max_packet_size - 1.
  connection_.SetMaxPacketLength(connection_.max_packet_length() - 1);
  connection_.SendCryptoDataWithString("bar",
                                       (anti_amplification_factor - 1) * 3);
  EXPECT_LT(writer_->total_bytes_written(),
            anti_amplification_factor *
                QuicConnectionPeer::BytesReceivedOnDefaultPath(&connection_));
  if (GetQuicFlag(quic_enforce_strict_amplification_factor)) {
    // 3 connection closes which will be buffered.
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(3);
    // Verify retransmission alarm is not set.
    EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  } else {
    // Crypto + 3 connection closes.
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(4);
    EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  }
  // Try to send another packet with max_packet_size.
  connection_.SetMaxPacketLength(connection_.max_packet_length() + 1);
  connection_.SendCryptoDataWithString("bar", anti_amplification_factor * 3);
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  // Close connection.
  EXPECT_CALL(visitor_, BeforeConnectionCloseSent());
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _));
  connection_.CloseConnection(
      QUIC_INTERNAL_ERROR, "error",
      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  if (GetQuicFlag(quic_enforce_strict_amplification_factor)) {
    EXPECT_LT(writer_->total_bytes_written(),
              anti_amplification_factor *
                  QuicConnectionPeer::BytesReceivedOnDefaultPath(&connection_));
  } else {
    EXPECT_LT(writer_->total_bytes_written(),
              (anti_amplification_factor + 2) *
                  QuicConnectionPeer::BytesReceivedOnDefaultPath(&connection_));
    EXPECT_GT(writer_->total_bytes_written(),
              (anti_amplification_factor + 1) *
                  QuicConnectionPeer::BytesReceivedOnDefaultPath(&connection_));
  }
}

TEST_P(QuicConnectionTest, OriginalConnectionId) {
  set_perspective(Perspective::IS_SERVER);
  EXPECT_FALSE(connection_.GetDiscardZeroRttDecryptionKeysAlarm()->IsSet());
  EXPECT_EQ(connection_.GetOriginalDestinationConnectionId(),
            connection_.connection_id());
  QuicConnectionId original({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
  connection_.SetOriginalDestinationConnectionId(original);
  EXPECT_EQ(original, connection_.GetOriginalDestinationConnectionId());
  // Send a 1-RTT packet to start the DiscardZeroRttDecryptionKeys timer.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(1, false, ENCRYPTION_FORWARD_SECURE);
  if (connection_.version().UsesTls()) {
    EXPECT_TRUE(connection_.GetDiscardZeroRttDecryptionKeysAlarm()->IsSet());
    EXPECT_CALL(visitor_, OnServerConnectionIdRetired(original));
    connection_.GetDiscardZeroRttDecryptionKeysAlarm()->Fire();
    EXPECT_EQ(connection_.GetOriginalDestinationConnectionId(),
              connection_.connection_id());
  } else {
    EXPECT_EQ(connection_.GetOriginalDestinationConnectionId(), original);
  }
}

ACTION_P2(InstallKeys, conn, level) {
  uint8_t crypto_input = (level == ENCRYPTION_FORWARD_SECURE) ? 0x03 : 0x02;
  conn->SetEncrypter(level, std::make_unique<TaggingEncrypter>(crypto_input));
  conn->InstallDecrypter(
      level, std::make_unique<StrictTaggingDecrypter>(crypto_input));
  conn->SetDefaultEncryptionLevel(level);
}

TEST_P(QuicConnectionTest, ServerConnectionIdChangeWithLateInitial) {
  if (!connection_.version().HasIetfQuicFrames()) {
    return;
  }
  // Call SetFromConfig so that the undecrypted packet buffer size is
  // initialized above zero.
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  connection_.SetFromConfig(config);
  connection_.RemoveEncrypter(ENCRYPTION_FORWARD_SECURE);
  connection_.RemoveDecrypter(ENCRYPTION_FORWARD_SECURE);

  // Send Client Initial.
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  connection_.SendCryptoStreamData();

  EXPECT_EQ(1u, writer_->packets_write_attempts());
  // Server Handshake packet with new connection ID is buffered.
  QuicConnectionId old_id = connection_id_;
  connection_id_ = TestConnectionId(2);
  peer_creator_.SetEncrypter(ENCRYPTION_HANDSHAKE,
                             std::make_unique<TaggingEncrypter>(0x02));
  ProcessCryptoPacketAtLevel(0, ENCRYPTION_HANDSHAKE);
  EXPECT_EQ(QuicConnectionPeer::NumUndecryptablePackets(&connection_), 1u);
  EXPECT_EQ(connection_.connection_id(), old_id);

  // Server 1-RTT Packet is buffered.
  peer_creator_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                             std::make_unique<TaggingEncrypter>(0x03));
  ProcessDataPacket(0);
  EXPECT_EQ(QuicConnectionPeer::NumUndecryptablePackets(&connection_), 2u);

  // Pretend the server Initial packet will yield the Handshake keys.
  EXPECT_CALL(visitor_, OnCryptoFrame(_))
      .Times(2)
      .WillOnce(InstallKeys(&connection_, ENCRYPTION_HANDSHAKE))
      .WillOnce(InstallKeys(&connection_, ENCRYPTION_FORWARD_SECURE));
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessCryptoPacketAtLevel(0, ENCRYPTION_INITIAL);
  // Two packets processed, connection ID changed.
  EXPECT_EQ(QuicConnectionPeer::NumUndecryptablePackets(&connection_), 0u);
  EXPECT_EQ(connection_.connection_id(), connection_id_);
}

TEST_P(QuicConnectionTest, ServerConnectionIdChangeTwiceWithLateInitial) {
  if (!connection_.version().HasIetfQuicFrames()) {
    return;
  }
  // Call SetFromConfig so that the undecrypted packet buffer size is
  // initialized above zero.
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  connection_.SetFromConfig(config);

  // Send Client Initial.
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  connection_.SendCryptoStreamData();

  EXPECT_EQ(1u, writer_->packets_write_attempts());
  // Server Handshake Packet Arrives with new connection ID.
  QuicConnectionId old_id = connection_id_;
  connection_id_ = TestConnectionId(2);
  peer_creator_.SetEncrypter(ENCRYPTION_HANDSHAKE,
                             std::make_unique<TaggingEncrypter>(0x02));
  ProcessCryptoPacketAtLevel(0, ENCRYPTION_HANDSHAKE);
  // Packet is buffered.
  EXPECT_EQ(QuicConnectionPeer::NumUndecryptablePackets(&connection_), 1u);
  EXPECT_EQ(connection_.connection_id(), old_id);

  // Pretend the server Initial packet will yield the Handshake keys.
  EXPECT_CALL(visitor_, OnCryptoFrame(_))
      .WillOnce(InstallKeys(&connection_, ENCRYPTION_HANDSHAKE));
  connection_id_ = TestConnectionId(1);
  ProcessCryptoPacketAtLevel(0, ENCRYPTION_INITIAL);
  // Handshake packet discarded because there's a different connection ID.
  EXPECT_EQ(QuicConnectionPeer::NumUndecryptablePackets(&connection_), 0u);
  EXPECT_EQ(connection_.connection_id(), connection_id_);
}

TEST_P(QuicConnectionTest, ClientValidatedServerPreferredAddress) {
  // Test the scenario where the client validates server preferred address by
  // receiving PATH_RESPONSE from server preferred address.
  if (!connection_.version().HasIetfQuicFrames()) {
    return;
  }
  QuicConfig config;
  ServerPreferredAddressInit(config);
  const QuicSocketAddress kNewSelfAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  const StatelessResetToken kNewStatelessResetToken =
      QuicUtils::GenerateStatelessResetToken(TestConnectionId(17));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  // Kick off path validation of server preferred address on handshake
  // confirmed.
  EXPECT_CALL(visitor_,
              OnServerPreferredAddressAvailable(kServerPreferredAddress))
      .WillOnce(Invoke([&]() {
        connection_.ValidatePath(
            std::make_unique<TestQuicPathValidationContext>(
                kNewSelfAddress, kServerPreferredAddress, &new_writer),
            std::make_unique<ServerPreferredAddressTestResultDelegate>(
                &connection_),
            PathValidationReason::kReasonUnknown);
      }));
  connection_.OnHandshakeComplete();
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, kServerPreferredAddress));
  EXPECT_EQ(TestConnectionId(17),
            new_writer.last_packet_header().destination_connection_id);
  EXPECT_EQ(kServerPreferredAddress, new_writer.last_write_peer_address());

  ASSERT_FALSE(new_writer.path_challenge_frames().empty());
  QuicPathFrameBuffer payload =
      new_writer.path_challenge_frames().front().data_buffer;
  // Send data packet while path validation is pending.
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  ASSERT_FALSE(writer_->stream_frames().empty());
  // While path validation is pending, packet is sent on default path.
  EXPECT_EQ(TestConnectionId(),
            writer_->last_packet_header().destination_connection_id);
  EXPECT_EQ(kPeerAddress, writer_->last_write_peer_address());
  EXPECT_TRUE(connection_.IsValidStatelessResetToken(kTestStatelessResetToken));
  EXPECT_FALSE(connection_.IsValidStatelessResetToken(kNewStatelessResetToken));

  // Receive path response from server preferred address.
  QuicFrames frames;
  frames.push_back(QuicFrame(QuicPathResponseFrame(99, payload)));
  // Verify send_algorithm gets reset after migration (new sent packet is not
  // updated to exsting send_algorithm_).
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  ProcessFramesPacketWithAddresses(frames, kNewSelfAddress,
                                   kServerPreferredAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  ASSERT_FALSE(connection_.HasPendingPathValidation());
  EXPECT_TRUE(QuicConnectionPeer::IsDefaultPath(&connection_, kNewSelfAddress,
                                                kServerPreferredAddress));
  ASSERT_FALSE(new_writer.stream_frames().empty());
  // Verify stream data is retransmitted on new path.
  EXPECT_EQ(TestConnectionId(17),
            new_writer.last_packet_header().destination_connection_id);
  EXPECT_EQ(kServerPreferredAddress, new_writer.last_write_peer_address());
  // Verify stateless reset token gets changed.
  EXPECT_FALSE(
      connection_.IsValidStatelessResetToken(kTestStatelessResetToken));
  EXPECT_TRUE(connection_.IsValidStatelessResetToken(kNewStatelessResetToken));

  auto* retire_peer_issued_cid_alarm =
      connection_.GetRetirePeerIssuedConnectionIdAlarm();
  ASSERT_TRUE(retire_peer_issued_cid_alarm->IsSet());
  // Verify client retires connection ID with sequence number 0.
  EXPECT_CALL(visitor_, SendRetireConnectionId(/*sequence_number=*/0u));
  retire_peer_issued_cid_alarm->Fire();
  EXPECT_TRUE(connection_.GetStats().server_preferred_address_validated);
  EXPECT_FALSE(
      connection_.GetStats().failed_to_validate_server_preferred_address);
}

TEST_P(QuicConnectionTest, ClientValidatedServerPreferredAddress2) {
  // Test the scenario where the client validates server preferred address by
  // receiving PATH_RESPONSE from original server address.
  if (!connection_.version().HasIetfQuicFrames()) {
    return;
  }
  QuicConfig config;
  ServerPreferredAddressInit(config);
  const QuicSocketAddress kNewSelfAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  // Kick off path validation of server preferred address on handshake
  // confirmed.
  EXPECT_CALL(visitor_,
              OnServerPreferredAddressAvailable(kServerPreferredAddress))
      .WillOnce(Invoke([&]() {
        connection_.ValidatePath(
            std::make_unique<TestQuicPathValidationContext>(
                kNewSelfAddress, kServerPreferredAddress, &new_writer),
            std::make_unique<ServerPreferredAddressTestResultDelegate>(
                &connection_),
            PathValidationReason::kReasonUnknown);
      }));
  connection_.OnHandshakeComplete();
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  ASSERT_FALSE(new_writer.path_challenge_frames().empty());
  QuicPathFrameBuffer payload =
      new_writer.path_challenge_frames().front().data_buffer;
  // Send data packet while path validation is pending.
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  ASSERT_FALSE(writer_->stream_frames().empty());
  EXPECT_EQ(TestConnectionId(),
            writer_->last_packet_header().destination_connection_id);
  EXPECT_EQ(kPeerAddress, writer_->last_write_peer_address());

  // Receive path response from original server address.
  QuicFrames frames;
  frames.push_back(QuicFrame(QuicPathResponseFrame(99, payload)));
  ProcessFramesPacketWithAddresses(frames, kNewSelfAddress, kPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  ASSERT_FALSE(connection_.HasPendingPathValidation());
  ASSERT_FALSE(new_writer.stream_frames().empty());
  // Verify stream data is retransmitted on new path.
  EXPECT_EQ(TestConnectionId(17),
            new_writer.last_packet_header().destination_connection_id);
  EXPECT_EQ(kServerPreferredAddress, new_writer.last_write_peer_address());

  auto* retire_peer_issued_cid_alarm =
      connection_.GetRetirePeerIssuedConnectionIdAlarm();
  ASSERT_TRUE(retire_peer_issued_cid_alarm->IsSet());
  // Verify client retires connection ID with sequence number 0.
  EXPECT_CALL(visitor_, SendRetireConnectionId(/*sequence_number=*/0u));
  retire_peer_issued_cid_alarm->Fire();

  // Verify another packet from original server address gets processed.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  frames.clear();
  frames.push_back(QuicFrame(frame1_));
  ProcessFramesPacketWithAddresses(frames, kSelfAddress, kPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_TRUE(connection_.GetStats().server_preferred_address_validated);
  EXPECT_FALSE(
      connection_.GetStats().failed_to_validate_server_preferred_address);
}

TEST_P(QuicConnectionTest, ClientFailedToValidateServerPreferredAddress) {
  // Test the scenario where the client fails to validate server preferred
  // address.
  if (!connection_.version().HasIetfQuicFrames()) {
    return;
  }
  QuicConfig config;
  ServerPreferredAddressInit(config);
  const QuicSocketAddress kNewSelfAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  // Kick off path validation of server preferred address on handshake
  // confirmed.
  EXPECT_CALL(visitor_,
              OnServerPreferredAddressAvailable(kServerPreferredAddress))
      .WillOnce(Invoke([&]() {
        connection_.ValidatePath(
            std::make_unique<TestQuicPathValidationContext>(
                kNewSelfAddress, kServerPreferredAddress, &new_writer),
            std::make_unique<ServerPreferredAddressTestResultDelegate>(
                &connection_),
            PathValidationReason::kReasonUnknown);
      }));
  connection_.OnHandshakeComplete();
  EXPECT_TRUE(connection_.IsValidatingServerPreferredAddress());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, kServerPreferredAddress));
  ASSERT_FALSE(new_writer.path_challenge_frames().empty());

  // Receive mismatched path challenge from original server address.
  QuicFrames frames;
  frames.push_back(
      QuicFrame(QuicPathResponseFrame(99, {0, 1, 2, 3, 4, 5, 6, 7})));
  ProcessFramesPacketWithAddresses(frames, kNewSelfAddress, kPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  ASSERT_TRUE(connection_.HasPendingPathValidation());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, kServerPreferredAddress));

  // Simluate path validation times out.
  for (size_t i = 0; i < QuicPathValidator::kMaxRetryTimes + 1; ++i) {
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(3 * kInitialRttMs));
    static_cast<TestAlarmFactory::TestAlarm*>(
        QuicPathValidatorPeer::retry_timer(
            QuicConnectionPeer::path_validator(&connection_)))
        ->Fire();
  }
  EXPECT_FALSE(connection_.HasPendingPathValidation());
  EXPECT_FALSE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress, kServerPreferredAddress));
  // Verify stream data is sent on the default path.
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  ASSERT_FALSE(writer_->stream_frames().empty());
  EXPECT_EQ(TestConnectionId(),
            writer_->last_packet_header().destination_connection_id);
  EXPECT_EQ(kPeerAddress, writer_->last_write_peer_address());

  auto* retire_peer_issued_cid_alarm =
      connection_.GetRetirePeerIssuedConnectionIdAlarm();
  ASSERT_TRUE(retire_peer_issued_cid_alarm->IsSet());
  // Verify client retires connection ID with sequence number 1.
  EXPECT_CALL(visitor_, SendRetireConnectionId(/*sequence_number=*/1u));
  retire_peer_issued_cid_alarm->Fire();
  EXPECT_TRUE(connection_.IsValidStatelessResetToken(kTestStatelessResetToken));
  EXPECT_FALSE(connection_.GetStats().server_preferred_address_validated);
  EXPECT_TRUE(
      connection_.GetStats().failed_to_validate_server_preferred_address);
}

TEST_P(QuicConnectionTest, OptimizedServerPreferredAddress) {
  if (!connection_.version().HasIetfQuicFrames()) {
    return;
  }
  const QuicSocketAddress kNewSelfAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  EXPECT_CALL(visitor_,
              OnServerPreferredAddressAvailable(kServerPreferredAddress))
      .WillOnce(Invoke([&]() {
        connection_.ValidatePath(
            std::make_unique<TestQuicPathValidationContext>(
                kNewSelfAddress, kServerPreferredAddress, &new_writer),
            std::make_unique<ServerPreferredAddressTestResultDelegate>(
                &connection_),
            PathValidationReason::kReasonUnknown);
      }));
  QuicConfig config;
  config.SetClientConnectionOptions(QuicTagVector{kSPA2});
  ServerPreferredAddressInit(config);
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  ASSERT_FALSE(new_writer.path_challenge_frames().empty());

  // Send data packet while path validation is pending.
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  // Verify the packet is sent on both paths.
  EXPECT_FALSE(writer_->stream_frames().empty());
  EXPECT_FALSE(new_writer.stream_frames().empty());

  // Verify packet duplication stops on handshake confirmed.
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  connection_.OnHandshakeComplete();
  SendPing();
  EXPECT_FALSE(writer_->ping_frames().empty());
  EXPECT_TRUE(new_writer.ping_frames().empty());
}

TEST_P(QuicConnectionTest, OptimizedServerPreferredAddress2) {
  if (!connection_.version().HasIetfQuicFrames()) {
    return;
  }
  const QuicSocketAddress kNewSelfAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  EXPECT_CALL(visitor_,
              OnServerPreferredAddressAvailable(kServerPreferredAddress))
      .WillOnce(Invoke([&]() {
        connection_.ValidatePath(
            std::make_unique<TestQuicPathValidationContext>(
                kNewSelfAddress, kServerPreferredAddress, &new_writer),
            std::make_unique<ServerPreferredAddressTestResultDelegate>(
                &connection_),
            PathValidationReason::kReasonUnknown);
      }));
  QuicConfig config;
  config.SetClientConnectionOptions(QuicTagVector{kSPA2});
  ServerPreferredAddressInit(config);
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  ASSERT_FALSE(new_writer.path_challenge_frames().empty());

  // Send data packet while path validation is pending.
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  // Verify the packet is sent on both paths.
  EXPECT_FALSE(writer_->stream_frames().empty());
  EXPECT_FALSE(new_writer.stream_frames().empty());

  // Simluate path validation times out.
  for (size_t i = 0; i < QuicPathValidator::kMaxRetryTimes + 1; ++i) {
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(3 * kInitialRttMs));
    static_cast<TestAlarmFactory::TestAlarm*>(
        QuicPathValidatorPeer::retry_timer(
            QuicConnectionPeer::path_validator(&connection_)))
        ->Fire();
  }
  EXPECT_FALSE(connection_.HasPendingPathValidation());
  // Verify packet duplication stops if there is no pending validation.
  SendPing();
  EXPECT_FALSE(writer_->ping_frames().empty());
  EXPECT_TRUE(new_writer.ping_frames().empty());
}

TEST_P(QuicConnectionTest, MaxDuplicatedPacketsSentToServerPreferredAddress) {
  if (!connection_.version().HasIetfQuicFrames()) {
    return;
  }
  const QuicSocketAddress kNewSelfAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  EXPECT_CALL(visitor_,
              OnServerPreferredAddressAvailable(kServerPreferredAddress))
      .WillOnce(Invoke([&]() {
        connection_.ValidatePath(
            std::make_unique<TestQuicPathValidationContext>(
                kNewSelfAddress, kServerPreferredAddress, &new_writer),
            std::make_unique<ServerPreferredAddressTestResultDelegate>(
                &connection_),
            PathValidationReason::kReasonUnknown);
      }));
  QuicConfig config;
  config.SetClientConnectionOptions(QuicTagVector{kSPA2});
  ServerPreferredAddressInit(config);
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  ASSERT_FALSE(new_writer.path_challenge_frames().empty());

  // Send data packet while path validation is pending.
  size_t write_limit = writer_->packets_write_attempts();
  size_t new_write_limit = new_writer.packets_write_attempts();
  for (size_t i = 0; i < kMaxDuplicatedPacketsSentToServerPreferredAddress;
       ++i) {
    connection_.SendStreamDataWithString(3, "foo", i * 3, NO_FIN);
    // Verify the packet is sent on both paths.
    ASSERT_EQ(write_limit + 1, writer_->packets_write_attempts());
    ASSERT_EQ(new_write_limit + 1, new_writer.packets_write_attempts());
    ++write_limit;
    ++new_write_limit;
    EXPECT_FALSE(writer_->stream_frames().empty());
    EXPECT_FALSE(new_writer.stream_frames().empty());
  }

  // Verify packet duplication stops if duplication limit is hit.
  SendPing();
  ASSERT_EQ(write_limit + 1, writer_->packets_write_attempts());
  ASSERT_EQ(new_write_limit, new_writer.packets_write_attempts());
  EXPECT_FALSE(writer_->ping_frames().empty());
  EXPECT_TRUE(new_writer.ping_frames().empty());
}

TEST_P(QuicConnectionTest, MultiPortCreationAfterServerMigration) {
  if (!GetParam().version.HasIetfQuicFrames()) {
    return;
  }
  QuicConfig config;
  config.SetClientConnectionOptions(QuicTagVector{kMPQC});
  ServerPreferredAddressInit(config);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  QuicConnectionId cid_for_preferred_address = TestConnectionId(17);
  const QuicSocketAddress kNewSelfAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  EXPECT_CALL(visitor_,
              OnServerPreferredAddressAvailable(kServerPreferredAddress))
      .WillOnce(Invoke([&]() {
        connection_.ValidatePath(
            std::make_unique<TestQuicPathValidationContext>(
                kNewSelfAddress, kServerPreferredAddress, &new_writer),
            std::make_unique<ServerPreferredAddressTestResultDelegate>(
                &connection_),
            PathValidationReason::kReasonUnknown);
      }));
  // The connection should start probing the preferred address after handshake
  // confirmed.
  QuicPathFrameBuffer payload;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(testing::AtLeast(1u))
      .WillOnce(Invoke([&]() {
        EXPECT_EQ(1u, new_writer.path_challenge_frames().size());
        payload = new_writer.path_challenge_frames().front().data_buffer;
        EXPECT_EQ(kServerPreferredAddress,
                  new_writer.last_write_peer_address());
      }));
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  connection_.OnHandshakeComplete();
  EXPECT_TRUE(connection_.IsValidatingServerPreferredAddress());

  // Receiving PATH_RESPONSE should cause the connection to migrate to the
  // preferred address.
  QuicFrames frames;
  frames.push_back(QuicFrame(QuicPathResponseFrame(99, payload)));
  ProcessFramesPacketWithAddresses(frames, kNewSelfAddress, kPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_FALSE(connection_.IsValidatingServerPreferredAddress());
  EXPECT_EQ(kServerPreferredAddress, connection_.effective_peer_address());
  EXPECT_EQ(kNewSelfAddress, connection_.self_address());
  EXPECT_EQ(connection_.connection_id(), cid_for_preferred_address);

  // As the default path changed, the server issued CID 1 should be retired.
  auto* retire_peer_issued_cid_alarm =
      connection_.GetRetirePeerIssuedConnectionIdAlarm();
  ASSERT_TRUE(retire_peer_issued_cid_alarm->IsSet());
  EXPECT_CALL(visitor_, SendRetireConnectionId(/*sequence_number=*/0u));
  retire_peer_issued_cid_alarm->Fire();

  const QuicSocketAddress kNewSelfAddress2(kNewSelfAddress.host(),
                                           kNewSelfAddress.port() + 1);
  EXPECT_NE(kNewSelfAddress2, kNewSelfAddress);
  TestPacketWriter new_writer2(version(), &clock_, Perspective::IS_CLIENT);
  QuicNewConnectionIdFrame frame;
  frame.connection_id = TestConnectionId(789);
  ASSERT_NE(frame.connection_id, connection_.connection_id());
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 0u;
  frame.sequence_number = 2u;
  EXPECT_CALL(visitor_, CreateContextForMultiPortPath)
      .WillOnce(testing::WithArgs<0>([&](auto&& observer) {
        observer->OnMultiPortPathContextAvailable(
            std::move(std::make_unique<TestQuicPathValidationContext>(
                kNewSelfAddress2, connection_.peer_address(), &new_writer2)));
      }));
  connection_.OnNewConnectionIdFrame(frame);
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  EXPECT_EQ(1u, new_writer.path_challenge_frames().size());
  payload = new_writer.path_challenge_frames().front().data_buffer;
  EXPECT_EQ(kServerPreferredAddress, new_writer.last_write_peer_address());
  EXPECT_EQ(kNewSelfAddress2.host(), new_writer.last_write_source_address());
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress2, connection_.peer_address()));
  auto* alt_path = QuicConnectionPeer::GetAlternativePath(&connection_);
  EXPECT_FALSE(alt_path->validated);
  QuicFrames frames2;
  frames2.push_back(QuicFrame(QuicPathResponseFrame(99, payload)));
  ProcessFramesPacketWithAddresses(frames2, kNewSelfAddress2, kPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_TRUE(alt_path->validated);
}

// Tests that after half-way server migration, the client should be able to
// respond to any reverse path validation from the original server address.
TEST_P(QuicConnectionTest, ClientReceivePathChallengeAfterServerMigration) {
  if (!GetParam().version.HasIetfQuicFrames()) {
    return;
  }
  QuicConfig config;
  ServerPreferredAddressInit(config);
  QuicConnectionId cid_for_preferred_address = TestConnectionId(17);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_CALL(visitor_,
              OnServerPreferredAddressAvailable(kServerPreferredAddress))
      .WillOnce(Invoke([&]() {
        connection_.AddKnownServerAddress(kServerPreferredAddress);
      }));
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  connection_.OnHandshakeComplete();

  const QuicSocketAddress kNewSelfAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), kTestPort + 1);
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  auto context = std::make_unique<TestQuicPathValidationContext>(
      kNewSelfAddress, kServerPreferredAddress, &new_writer);
  // Pretend that the validation already succeeded. And start to use the server
  // preferred address.
  connection_.OnServerPreferredAddressValidated(*context, false);
  EXPECT_EQ(kServerPreferredAddress, connection_.effective_peer_address());
  EXPECT_EQ(kServerPreferredAddress, connection_.peer_address());
  EXPECT_EQ(kNewSelfAddress, connection_.self_address());
  EXPECT_EQ(connection_.connection_id(), cid_for_preferred_address);
  EXPECT_NE(connection_.sent_packet_manager().GetSendAlgorithm(),
            send_algorithm_);
  // Switch to use a mock send algorithm.
  send_algorithm_ = new StrictMock<MockSendAlgorithm>();
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(kDefaultTCPMSS));
  EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, BandwidthEstimate())
      .Times(AnyNumber())
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, InSlowStart()).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, InRecovery()).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, PopulateConnectionStats(_)).Times(AnyNumber());
  connection_.SetSendAlgorithm(send_algorithm_);

  // As the default path changed, the server issued CID 123 should be retired.
  QuicConnectionPeer::RetirePeerIssuedConnectionIdsNoLongerOnPath(&connection_);
  auto* retire_peer_issued_cid_alarm =
      connection_.GetRetirePeerIssuedConnectionIdAlarm();
  ASSERT_TRUE(retire_peer_issued_cid_alarm->IsSet());
  EXPECT_CALL(visitor_, SendRetireConnectionId(/*sequence_number=*/0u));
  retire_peer_issued_cid_alarm->Fire();

  // Receive PATH_CHALLENGE from the original server
  // address. The client connection responds it on the default path.
  QuicPathFrameBuffer path_challenge_payload{0, 1, 2, 3, 4, 5, 6, 7};
  QuicFrames frames1;
  frames1.push_back(
      QuicFrame(QuicPathChallengeFrame(0, path_challenge_payload)));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(AtLeast(1))
      .WillOnce(Invoke([&]() {
        ASSERT_FALSE(new_writer.path_response_frames().empty());
        EXPECT_EQ(
            0, memcmp(&path_challenge_payload,
                      &(new_writer.path_response_frames().front().data_buffer),
                      sizeof(path_challenge_payload)));
        EXPECT_EQ(kServerPreferredAddress,
                  new_writer.last_write_peer_address());
        EXPECT_EQ(kNewSelfAddress.host(),
                  new_writer.last_write_source_address());
      }));
  ProcessFramesPacketWithAddresses(frames1, kNewSelfAddress, kPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
}

// Tests that after half-way server migration, the client should be able to
// probe with a different socket and respond to reverse path validation.
TEST_P(QuicConnectionTest, ClientProbesAfterServerMigration) {
  if (!GetParam().version.HasIetfQuicFrames()) {
    return;
  }
  QuicConfig config;
  ServerPreferredAddressInit(config);
  QuicConnectionId cid_for_preferred_address = TestConnectionId(17);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);

  // The connection should start probing the preferred address after handshake
  // confirmed.
  EXPECT_CALL(visitor_,
              OnServerPreferredAddressAvailable(kServerPreferredAddress))
      .WillOnce(Invoke([&]() {
        connection_.AddKnownServerAddress(kServerPreferredAddress);
      }));
  EXPECT_CALL(visitor_, GetHandshakeState())
      .WillRepeatedly(Return(HANDSHAKE_CONFIRMED));
  connection_.OnHandshakeComplete();

  const QuicSocketAddress kNewSelfAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), kTestPort + 1);
  TestPacketWriter new_writer(version(), &clock_, Perspective::IS_CLIENT);
  auto context = std::make_unique<TestQuicPathValidationContext>(
      kNewSelfAddress, kServerPreferredAddress, &new_writer);
  // Pretend that the validation already succeeded.
  connection_.OnServerPreferredAddressValidated(*context, false);
  EXPECT_EQ(kServerPreferredAddress, connection_.effective_peer_address());
  EXPECT_EQ(kServerPreferredAddress, connection_.peer_address());
  EXPECT_EQ(kNewSelfAddress, connection_.self_address());
  EXPECT_EQ(connection_.connection_id(), cid_for_preferred_address);
  EXPECT_NE(connection_.sent_packet_manager().GetSendAlgorithm(),
            send_algorithm_);
  // Switch to use a mock send algorithm.
  send_algorithm_ = new StrictMock<MockSendAlgorithm>();
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(kDefaultTCPMSS));
  EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, BandwidthEstimate())
      .Times(AnyNumber())
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, InSlowStart()).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, InRecovery()).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, PopulateConnectionStats(_)).Times(AnyNumber());
  connection_.SetSendAlgorithm(send_algorithm_);

  // Receiving data from the original server address should not change the peer
  // address.
  EXPECT_CALL(visitor_, OnCryptoFrame(_));
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kNewSelfAddress,
                                  kPeerAddress, ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kServerPreferredAddress, connection_.effective_peer_address());
  EXPECT_EQ(kServerPreferredAddress, connection_.peer_address());

  // As the default path changed, the server issued CID 123 should be retired.
  auto* retire_peer_issued_cid_alarm =
      connection_.GetRetirePeerIssuedConnectionIdAlarm();
  ASSERT_TRUE(retire_peer_issued_cid_alarm->IsSet());
  EXPECT_CALL(visitor_, SendRetireConnectionId(/*sequence_number=*/0u));
  retire_peer_issued_cid_alarm->Fire();

  // Receiving a new CID from the server.
  QuicNewConnectionIdFrame new_cid_frame1;
  new_cid_frame1.connection_id = TestConnectionId(456);
  ASSERT_NE(new_cid_frame1.connection_id, connection_.connection_id());
  new_cid_frame1.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(new_cid_frame1.connection_id);
  new_cid_frame1.retire_prior_to = 0u;
  new_cid_frame1.sequence_number = 2u;
  connection_.OnNewConnectionIdFrame(new_cid_frame1);

  // Probe from a new socket.
  const QuicSocketAddress kNewSelfAddress2 =
      QuicSocketAddress(QuicIpAddress::Loopback4(), kTestPort + 2);
  TestPacketWriter new_writer2(version(), &clock_, Perspective::IS_CLIENT);
  bool success;
  QuicPathFrameBuffer payload;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(testing::AtLeast(1u))
      .WillOnce(Invoke([&]() {
        EXPECT_EQ(1u, new_writer2.path_challenge_frames().size());
        payload = new_writer2.path_challenge_frames().front().data_buffer;
        EXPECT_EQ(kServerPreferredAddress,
                  new_writer2.last_write_peer_address());
        EXPECT_EQ(kNewSelfAddress2.host(),
                  new_writer2.last_write_source_address());
      }));
  connection_.ValidatePath(
      std::make_unique<TestQuicPathValidationContext>(
          kNewSelfAddress2, connection_.peer_address(), &new_writer2),
      std::make_unique<TestValidationResultDelegate>(
          &connection_, kNewSelfAddress2, connection_.peer_address(), &success),
      PathValidationReason::kServerPreferredAddressMigration);
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(
      &connection_, kNewSelfAddress2, kServerPreferredAddress));

  // Our server implementation will send PATH_CHALLENGE from the original server
  // address. The client connection send PATH_RESPONSE to the default peer
  // address.
  QuicPathFrameBuffer path_challenge_payload{0, 1, 2, 3, 4, 5, 6, 7};
  QuicFrames frames;
  frames.push_back(
      QuicFrame(QuicPathChallengeFrame(0, path_challenge_payload)));
  frames.push_back(QuicFrame(QuicPathResponseFrame(99, payload)));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(AtLeast(1))
      .WillOnce(Invoke([&]() {
        EXPECT_FALSE(new_writer2.path_response_frames().empty());
        EXPECT_EQ(
            0, memcmp(&path_challenge_payload,
                      &(new_writer2.path_response_frames().front().data_buffer),
                      sizeof(path_challenge_payload)));
        EXPECT_EQ(kServerPreferredAddress,
                  new_writer2.last_write_peer_address());
        EXPECT_EQ(kNewSelfAddress2.host(),
                  new_writer2.last_write_source_address());
      }));
  ProcessFramesPacketWithAddresses(frames, kNewSelfAddress2, kPeerAddress,
                                   ENCRYPTION_FORWARD_SECURE);
  EXPECT_TRUE(success);
}

TEST_P(QuicConnectionTest, EcnMarksCorrectlyRecorded) {
  set_perspective(Perspective::IS_SERVER);
  QuicFrames frames;
  frames.push_back(QuicFrame(QuicPingFrame()));
  frames.push_back(QuicFrame(QuicPaddingFrame(7)));
  QuicAckFrame ack_frame =
      connection_.SupportsMultiplePacketNumberSpaces()
          ? connection_.received_packet_manager().GetAckFrame(APPLICATION_DATA)
          : connection_.received_packet_manager().ack_frame();
  EXPECT_FALSE(ack_frame.ecn_counters.has_value());

  ProcessFramesPacketAtLevelWithEcn(1, frames, ENCRYPTION_FORWARD_SECURE,
                                    ECN_ECT0);
  ack_frame =
      connection_.SupportsMultiplePacketNumberSpaces()
          ? connection_.received_packet_manager().GetAckFrame(APPLICATION_DATA)
          : connection_.received_packet_manager().ack_frame();
  // Send two PINGs so that the ACK goes too. The second packet should not
  // include an ACK, which checks that the packet state is cleared properly.
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  if (connection_.version().HasIetfQuicFrames()) {
    QuicConnectionPeer::SendPing(&connection_);
    QuicConnectionPeer::SendPing(&connection_);
  }
  QuicConnectionStats stats = connection_.GetStats();
  ASSERT_TRUE(ack_frame.ecn_counters.has_value());
  EXPECT_EQ(ack_frame.ecn_counters->ect0, 1);
  EXPECT_EQ(stats.num_ack_frames_sent_with_ecn,
            connection_.version().HasIetfQuicFrames() ? 1 : 0);
  EXPECT_EQ(stats.num_ecn_marks_received.ect0, 1);
  EXPECT_EQ(stats.num_ecn_marks_received.ect1, 0);
  EXPECT_EQ(stats.num_ecn_marks_received.ce, 0);
}

TEST_P(QuicConnectionTest, EcnMarksCoalescedPacket) {
  if (!connection_.version().CanSendCoalescedPackets()) {
    return;
  }
  QuicCryptoFrame crypto_frame1{ENCRYPTION_HANDSHAKE, 0, "foo"};
  QuicFrames frames1;
  frames1.push_back(QuicFrame(&crypto_frame1));
  QuicFrames frames2;
  QuicCryptoFrame crypto_frame2{ENCRYPTION_FORWARD_SECURE, 0, "bar"};
  frames2.push_back(QuicFrame(&crypto_frame2));
  std::vector<PacketInfo> packets = {{2, frames1, ENCRYPTION_HANDSHAKE},
                                     {3, frames2, ENCRYPTION_FORWARD_SECURE}};
  QuicAckFrame ack_frame =
      connection_.SupportsMultiplePacketNumberSpaces()
          ? connection_.received_packet_manager().GetAckFrame(APPLICATION_DATA)
          : connection_.received_packet_manager().ack_frame();
  EXPECT_FALSE(ack_frame.ecn_counters.has_value());
  ack_frame =
      connection_.SupportsMultiplePacketNumberSpaces()
          ? connection_.received_packet_manager().GetAckFrame(HANDSHAKE_DATA)
          : connection_.received_packet_manager().ack_frame();
  EXPECT_FALSE(ack_frame.ecn_counters.has_value());
  // Deliver packets.
  connection_.SetEncrypter(
      ENCRYPTION_HANDSHAKE,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_HANDSHAKE));
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(2);
  ProcessCoalescedPacket(packets, ECN_ECT0);
  // Send two PINGs so that the ACKs go too.
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  if (connection_.version().HasIetfQuicFrames()) {
    EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(1);
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
    QuicConnectionPeer::SendPing(&connection_);
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    QuicConnectionPeer::SendPing(&connection_);
  }
  QuicConnectionStats stats = connection_.GetStats();
  ack_frame =
      connection_.SupportsMultiplePacketNumberSpaces()
          ? connection_.received_packet_manager().GetAckFrame(HANDSHAKE_DATA)
          : connection_.received_packet_manager().ack_frame();
  ASSERT_TRUE(ack_frame.ecn_counters.has_value());
  EXPECT_EQ(ack_frame.ecn_counters->ect0,
            connection_.SupportsMultiplePacketNumberSpaces() ? 1 : 2);
  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    ack_frame = connection_.SupportsMultiplePacketNumberSpaces()
                    ? connection_.received_packet_manager().GetAckFrame(
                          APPLICATION_DATA)
                    : connection_.received_packet_manager().ack_frame();
    EXPECT_TRUE(ack_frame.ecn_counters.has_value());
    EXPECT_EQ(ack_frame.ecn_counters->ect0, 1);
  }
  EXPECT_EQ(stats.num_ecn_marks_received.ect0, 2);
  EXPECT_EQ(stats.num_ack_frames_sent_with_ecn,
            connection_.version().HasIetfQuicFrames() ? 2 : 0);
  EXPECT_EQ(stats.num_ecn_marks_received.ect1, 0);
  EXPECT_EQ(stats.num_ecn_marks_received.ce, 0);
}

TEST_P(QuicConnectionTest, EcnMarksUndecryptableCoalescedPacket) {
  if (!connection_.version().CanSendCoalescedPackets()) {
    return;
  }
  // SetFromConfig is always called after construction from InitializeSession.
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  config.set_max_undecryptable_packets(100);
  connection_.SetFromConfig(config);
  QuicCryptoFrame crypto_frame1{ENCRYPTION_HANDSHAKE, 0, "foo"};
  QuicFrames frames1;
  frames1.push_back(QuicFrame(&crypto_frame1));
  QuicFrames frames2;
  QuicCryptoFrame crypto_frame2{ENCRYPTION_FORWARD_SECURE, 0, "bar"};
  frames2.push_back(QuicFrame(&crypto_frame2));
  std::vector<PacketInfo> packets = {{2, frames1, ENCRYPTION_HANDSHAKE},
                                     {3, frames2, ENCRYPTION_FORWARD_SECURE}};
  char coalesced_buffer[kMaxOutgoingPacketSize];
  size_t coalesced_size = 0;
  for (const auto& packet : packets) {
    QuicPacketHeader header =
        ConstructPacketHeader(packet.packet_number, packet.level);
    // Set the correct encryption level and encrypter on peer_creator and
    // peer_framer, respectively.
    peer_creator_.set_encryption_level(packet.level);
    peer_framer_.SetEncrypter(packet.level,
                              std::make_unique<TaggingEncrypter>(packet.level));
    // Set the corresponding decrypter.
    if (packet.level == ENCRYPTION_HANDSHAKE) {
      connection_.SetEncrypter(
          packet.level, std::make_unique<TaggingEncrypter>(packet.level));
      connection_.SetDefaultEncryptionLevel(packet.level);
      SetDecrypter(packet.level,
                   std::make_unique<StrictTaggingDecrypter>(packet.level));
    }
    // Forward Secure packet is undecryptable.
    std::unique_ptr<QuicPacket> constructed_packet(
        ConstructPacket(header, packet.frames));

    char buffer[kMaxOutgoingPacketSize];
    size_t encrypted_length = peer_framer_.EncryptPayload(
        packet.level, QuicPacketNumber(packet.packet_number),
        *constructed_packet, buffer, kMaxOutgoingPacketSize);
    QUICHE_DCHECK_LE(coalesced_size + encrypted_length, kMaxOutgoingPacketSize);
    memcpy(coalesced_buffer + coalesced_size, buffer, encrypted_length);
    coalesced_size += encrypted_length;
  }
  QuicAckFrame ack_frame =
      connection_.SupportsMultiplePacketNumberSpaces()
          ? connection_.received_packet_manager().GetAckFrame(APPLICATION_DATA)
          : connection_.received_packet_manager().ack_frame();
  EXPECT_FALSE(ack_frame.ecn_counters.has_value());
  ack_frame =
      connection_.SupportsMultiplePacketNumberSpaces()
          ? connection_.received_packet_manager().GetAckFrame(HANDSHAKE_DATA)
          : connection_.received_packet_manager().ack_frame();
  EXPECT_FALSE(ack_frame.ecn_counters.has_value());
  // Deliver packets, but first remove the Forward Secure decrypter so that
  // packet has to be buffered.
  connection_.RemoveDecrypter(ENCRYPTION_FORWARD_SECURE);
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(1);
  EXPECT_CALL(visitor_, OnHandshakePacketSent()).Times(1);
  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(coalesced_buffer, coalesced_size, clock_.Now(), false,
                         0, true, nullptr, 0, true, ECN_ECT0));
  if (connection_.GetSendAlarm()->IsSet()) {
    connection_.GetSendAlarm()->Fire();
  }
  ack_frame =
      connection_.SupportsMultiplePacketNumberSpaces()
          ? connection_.received_packet_manager().GetAckFrame(HANDSHAKE_DATA)
          : connection_.received_packet_manager().ack_frame();
  ASSERT_TRUE(ack_frame.ecn_counters.has_value());
  EXPECT_EQ(ack_frame.ecn_counters->ect0, 1);
  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    ack_frame = connection_.SupportsMultiplePacketNumberSpaces()
                    ? connection_.received_packet_manager().GetAckFrame(
                          APPLICATION_DATA)
                    : connection_.received_packet_manager().ack_frame();
    EXPECT_FALSE(ack_frame.ecn_counters.has_value());
  }
  // Send PING packet with ECN_CE, which will change the ECN codepoint in
  // last_received_packet_info_.
  ProcessFramePacketAtLevelWithEcn(4, QuicFrame(QuicPingFrame()),
                                   ENCRYPTION_HANDSHAKE, ECN_CE);
  ack_frame =
      connection_.SupportsMultiplePacketNumberSpaces()
          ? connection_.received_packet_manager().GetAckFrame(HANDSHAKE_DATA)
          : connection_.received_packet_manager().ack_frame();
  ASSERT_TRUE(ack_frame.ecn_counters.has_value());
  EXPECT_EQ(ack_frame.ecn_counters->ect0, 1);
  EXPECT_EQ(ack_frame.ecn_counters->ce, 1);
  if (connection_.SupportsMultiplePacketNumberSpaces()) {
    ack_frame = connection_.SupportsMultiplePacketNumberSpaces()
                    ? connection_.received_packet_manager().GetAckFrame(
                          APPLICATION_DATA)
                    : connection_.received_packet_manager().ack_frame();
    EXPECT_FALSE(ack_frame.ecn_counters.has_value());
  }
  // Install decrypter for ENCRYPTION_FORWARD_SECURE. Make sure the original
  // ECN codepoint is incremented.
  EXPECT_CALL(visitor_, OnCryptoFrame(_)).Times(1);
  SetDecrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_FORWARD_SECURE));
  connection_.GetProcessUndecryptablePacketsAlarm()->Fire();
  ack_frame =
      connection_.SupportsMultiplePacketNumberSpaces()
          ? connection_.received_packet_manager().GetAckFrame(APPLICATION_DATA)
          : connection_.received_packet_manager().ack_frame();
  ASSERT_TRUE(ack_frame.ecn_counters.has_value());
  // Should be recorded as ECT(0), not CE.
  EXPECT_EQ(ack_frame.ecn_counters->ect0,
            connection_.SupportsMultiplePacketNumberSpaces() ? 1 : 2);
  QuicConnectionStats stats = connection_.GetStats();
  EXPECT_EQ(stats.num_ecn_marks_received.ect0, 2);
  EXPECT_EQ(stats.num_ecn_marks_received.ect1, 0);
  EXPECT_EQ(stats.num_ecn_marks_received.ce, 1);
}

TEST_P(QuicConnectionTest, ReceivedPacketInfoDefaults) {
  EXPECT_TRUE(QuicConnectionPeer::TestLastReceivedPacketInfoDefaults());
}

TEST_P(QuicConnectionTest, DetectMigrationToPreferredAddress) {
  if (!GetParam().version.HasIetfQuicFrames()) {
    return;
  }
  ServerHandlePreferredAddressInit();

  // Issue a new server CID associated with the preferred address.
  QuicConnectionId server_issued_cid_for_preferred_address =
      TestConnectionId(17);
  EXPECT_CALL(connection_id_generator_,
              GenerateNextConnectionId(connection_id_))
      .WillOnce(Return(server_issued_cid_for_preferred_address));
  EXPECT_CALL(visitor_, MaybeReserveConnectionId(_)).WillOnce(Return(true));
  std::optional<QuicNewConnectionIdFrame> frame =
      connection_.MaybeIssueNewConnectionIdForPreferredAddress();
  ASSERT_TRUE(frame.has_value());

  auto* packet_creator = QuicConnectionPeer::GetPacketCreator(&connection_);
  ASSERT_EQ(packet_creator->GetDestinationConnectionId(),
            connection_.client_connection_id());
  ASSERT_EQ(packet_creator->GetSourceConnectionId(), connection_id_);

  // Process a packet received at the preferred Address.
  peer_creator_.SetServerConnectionId(server_issued_cid_for_preferred_address);
  EXPECT_CALL(visitor_, OnCryptoFrame(_));
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kServerPreferredAddress,
                                  kPeerAddress, ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  // The server migrates half-way with the default path unchanged, and
  // continuing with the client issued CID 1.
  EXPECT_EQ(kSelfAddress.host(), writer_->last_write_source_address());
  EXPECT_EQ(kSelfAddress, connection_.self_address());

  // The peer retires CID 123.
  QuicRetireConnectionIdFrame retire_cid_frame;
  retire_cid_frame.sequence_number = 0u;
  EXPECT_CALL(connection_id_generator_,
              GenerateNextConnectionId(server_issued_cid_for_preferred_address))
      .WillOnce(Return(TestConnectionId(456)));
  EXPECT_CALL(visitor_, MaybeReserveConnectionId(_)).WillOnce(Return(true));
  EXPECT_CALL(visitor_, SendNewConnectionId(_));
  EXPECT_TRUE(connection_.OnRetireConnectionIdFrame(retire_cid_frame));

  // Process another packet received at Preferred Address.
  EXPECT_CALL(visitor_, OnCryptoFrame(_));
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kServerPreferredAddress,
                                  kPeerAddress, ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kSelfAddress.host(), writer_->last_write_source_address());
  EXPECT_EQ(kSelfAddress, connection_.self_address());
}

TEST_P(QuicConnectionTest,
       DetectSimutanuousServerAndClientAddressChangeWithProbe) {
  if (!GetParam().version.HasIetfQuicFrames()) {
    return;
  }
  ServerHandlePreferredAddressInit();

  // Issue a new server CID associated with the preferred address.
  QuicConnectionId server_issued_cid_for_preferred_address =
      TestConnectionId(17);
  EXPECT_CALL(connection_id_generator_,
              GenerateNextConnectionId(connection_id_))
      .WillOnce(Return(server_issued_cid_for_preferred_address));
  EXPECT_CALL(visitor_, MaybeReserveConnectionId(_)).WillOnce(Return(true));
  std::optional<QuicNewConnectionIdFrame> frame =
      connection_.MaybeIssueNewConnectionIdForPreferredAddress();
  ASSERT_TRUE(frame.has_value());

  auto* packet_creator = QuicConnectionPeer::GetPacketCreator(&connection_);
  ASSERT_EQ(packet_creator->GetSourceConnectionId(), connection_id_);
  ASSERT_EQ(packet_creator->GetDestinationConnectionId(),
            connection_.client_connection_id());

  // Receiving a probing packet from a new client address to the preferred
  // address.
  peer_creator_.SetServerConnectionId(server_issued_cid_for_preferred_address);
  const QuicSocketAddress kNewPeerAddress(QuicIpAddress::Loopback4(),
                                          /*port=*/34567);
  std::unique_ptr<SerializedPacket> probing_packet = ConstructProbingPacket();
  std::unique_ptr<QuicReceivedPacket> received(ConstructReceivedPacket(
      QuicEncryptedPacket(probing_packet->encrypted_buffer,
                          probing_packet->encrypted_length),
      clock_.Now()));
  uint64_t num_probing_received =
      connection_.GetStats().num_connectivity_probing_received;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(AtLeast(1u))
      .WillOnce(Invoke([&]() {
        EXPECT_EQ(1u, writer_->path_response_frames().size());
        EXPECT_EQ(1u, writer_->path_challenge_frames().size());
        // The responses should be sent from preferred address given server
        // has not received packet on original address from the new client
        // address.
        EXPECT_EQ(kServerPreferredAddress.host(),
                  writer_->last_write_source_address());
        EXPECT_EQ(kNewPeerAddress, writer_->last_write_peer_address());
      }))
      .WillRepeatedly(DoDefault());
  ProcessReceivedPacket(kServerPreferredAddress, kNewPeerAddress, *received);
  EXPECT_EQ(num_probing_received + 1,
            connection_.GetStats().num_connectivity_probing_received);
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(&connection_, kSelfAddress,
                                                    kNewPeerAddress));
  EXPECT_LT(0u, QuicConnectionPeer::BytesSentOnAlternativePath(&connection_));
  EXPECT_EQ(received->length(),
            QuicConnectionPeer::BytesReceivedOnAlternativePath(&connection_));
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kSelfAddress, connection_.self_address());

  // Process a data packet received at the preferred Address from the new client
  // address.
  EXPECT_CALL(visitor_, OnConnectionMigration(IPV6_TO_IPV4_CHANGE));
  EXPECT_CALL(visitor_, OnCryptoFrame(_));
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kServerPreferredAddress,
                                  kNewPeerAddress, ENCRYPTION_FORWARD_SECURE);
  // The server migrates half-way with the new peer address but the same default
  // self address.
  EXPECT_EQ(kSelfAddress.host(), writer_->last_write_source_address());
  EXPECT_EQ(kSelfAddress, connection_.self_address());
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_TRUE(connection_.HasPendingPathValidation());
  EXPECT_FALSE(QuicConnectionPeer::GetDefaultPath(&connection_)->validated);
  EXPECT_TRUE(QuicConnectionPeer::IsAlternativePath(&connection_, kSelfAddress,
                                                    kPeerAddress));
  EXPECT_EQ(packet_creator->GetSourceConnectionId(),
            server_issued_cid_for_preferred_address);

  // Process another packet received at the preferred Address.
  EXPECT_CALL(visitor_, OnCryptoFrame(_));
  ProcessFramePacketWithAddresses(MakeCryptoFrame(), kServerPreferredAddress,
                                  kNewPeerAddress, ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kServerPreferredAddress.host(),
            writer_->last_write_source_address());
  EXPECT_EQ(kSelfAddress, connection_.self_address());
}

TEST_P(QuicConnectionTest, EcnCodepointsRejected) {
  for (QuicEcnCodepoint ecn : {ECN_NOT_ECT, ECN_ECT0, ECN_ECT1, ECN_CE}) {
    if (ecn == ECN_ECT0) {
      EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
    } else if (ecn == ECN_ECT1) {
      EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
    }
    if (ecn == ECN_NOT_ECT) {
      EXPECT_TRUE(connection_.set_ecn_codepoint(ecn));
    } else {
      EXPECT_FALSE(connection_.set_ecn_codepoint(ecn));
    }
    EXPECT_EQ(connection_.ecn_codepoint(), ECN_NOT_ECT);
    EXPECT_CALL(connection_, OnSerializedPacket(_));
    SendPing();
    EXPECT_EQ(writer_->last_ecn_sent(), ECN_NOT_ECT);
  }
}

TEST_P(QuicConnectionTest, EcnCodepointsAccepted) {
  for (QuicEcnCodepoint ecn : {ECN_NOT_ECT, ECN_ECT0, ECN_ECT1, ECN_CE}) {
    if (ecn == ECN_ECT0) {
      EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(true));
    } else if (ecn == ECN_ECT1) {
      EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(true));
    }
    if (ecn == ECN_CE) {
      EXPECT_FALSE(connection_.set_ecn_codepoint(ecn));
    } else {
      EXPECT_TRUE(connection_.set_ecn_codepoint(ecn));
    }
    EXPECT_CALL(connection_, OnSerializedPacket(_));
    SendPing();
    QuicEcnCodepoint expected_codepoint = ecn;
    if (ecn == ECN_CE) {
      expected_codepoint = ECN_ECT1;
    }
    EXPECT_EQ(connection_.ecn_codepoint(), expected_codepoint);
    EXPECT_EQ(writer_->last_ecn_sent(), expected_codepoint);
  }
}

TEST_P(QuicConnectionTest, EcnValidationDisabled) {
  QuicConnectionPeer::DisableEcnCodepointValidation(&connection_);
  for (QuicEcnCodepoint ecn : {ECN_NOT_ECT, ECN_ECT0, ECN_ECT1, ECN_CE}) {
    EXPECT_TRUE(connection_.set_ecn_codepoint(ecn));
    EXPECT_CALL(connection_, OnSerializedPacket(_));
    SendPing();
    EXPECT_EQ(connection_.ecn_codepoint(), ecn);
    EXPECT_EQ(writer_->last_ecn_sent(), ecn);
  }
}

TEST_P(QuicConnectionTest, RtoDisablesEcnMarking) {
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(true));
  EXPECT_TRUE(connection_.set_ecn_codepoint(ECN_ECT1));
  QuicPacketCreatorPeer::SetPacketNumber(
      QuicConnectionPeer::GetPacketCreator(&connection_), 1);
  SendPing();
  connection_.OnRetransmissionAlarm();
  EXPECT_EQ(writer_->last_ecn_sent(), ECN_NOT_ECT);
  EXPECT_EQ(connection_.ecn_codepoint(), ECN_ECT1);
  // On 2nd RTO, QUIC abandons ECN.
  connection_.OnRetransmissionAlarm();
  EXPECT_EQ(writer_->last_ecn_sent(), ECN_NOT_ECT);
  EXPECT_EQ(connection_.ecn_codepoint(), ECN_NOT_ECT);
}

TEST_P(QuicConnectionTest, RtoDoesntDisableEcnMarkingIfEcnAcked) {
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(true));
  EXPECT_TRUE(connection_.set_ecn_codepoint(ECN_ECT1));
  QuicPacketCreatorPeer::SetPacketNumber(
      QuicConnectionPeer::GetPacketCreator(&connection_), 1);
  connection_.OnInFlightEcnPacketAcked();
  SendPing();
  // Because an ECN packet was acked, PTOs have no effect on ECN settings.
  connection_.OnRetransmissionAlarm();
  QuicEcnCodepoint expected_codepoint = ECN_ECT1;
  EXPECT_EQ(writer_->last_ecn_sent(), expected_codepoint);
  EXPECT_EQ(connection_.ecn_codepoint(), expected_codepoint);
  connection_.OnRetransmissionAlarm();
  EXPECT_EQ(writer_->last_ecn_sent(), expected_codepoint);
  EXPECT_EQ(connection_.ecn_codepoint(), expected_codepoint);
}

TEST_P(QuicConnectionTest, InvalidFeedbackCancelsEcn) {
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(true));
  EXPECT_TRUE(connection_.set_ecn_codepoint(ECN_ECT1));
  EXPECT_EQ(connection_.ecn_codepoint(), ECN_ECT1);
  connection_.OnInvalidEcnFeedback();
  EXPECT_EQ(connection_.ecn_codepoint(), ECN_NOT_ECT);
}

TEST_P(QuicConnectionTest, StateMatchesSentEcn) {
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(true));
  EXPECT_TRUE(connection_.set_ecn_codepoint(ECN_ECT1));
  SendPing();
  QuicSentPacketManager* sent_packet_manager =
      QuicConnectionPeer::GetSentPacketManager(&connection_);
  EXPECT_EQ(writer_->last_ecn_sent(), ECN_ECT1);
  EXPECT_EQ(
      QuicSentPacketManagerPeer::GetEct1Sent(sent_packet_manager, INITIAL_DATA),
      1);
}

TEST_P(QuicConnectionTest, CoalescedPacketSplitsEcn) {
  if (!connection_.version().CanSendCoalescedPackets()) {
    return;
  }
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(true));
  EXPECT_TRUE(connection_.set_ecn_codepoint(ECN_ECT1));
  // All these steps are necessary to send an INITIAL ping and save it to be
  // coalesced, instead of just calling SendPing() and sending it immediately.
  char buffer[1000];
  creator_->set_encryption_level(ENCRYPTION_INITIAL);
  QuicFrames frames;
  QuicPingFrame ping;
  frames.emplace_back(QuicFrame(ping));
  SerializedPacket packet1 = QuicPacketCreatorPeer::SerializeAllFrames(
      creator_, frames, buffer, sizeof(buffer));
  connection_.SendOrQueuePacket(std::move(packet1));
  creator_->set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(true));
  // If not for the line below, these packets would coalesce.
  EXPECT_TRUE(connection_.set_ecn_codepoint(ECN_ECT0));
  EXPECT_EQ(writer_->packets_write_attempts(), 0);
  SendPing();
  EXPECT_EQ(writer_->packets_write_attempts(), 2);
  EXPECT_EQ(writer_->last_ecn_sent(), ECN_ECT0);
}

TEST_P(QuicConnectionTest, BufferedPacketRetainsOldEcn) {
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(true));
  EXPECT_TRUE(connection_.set_ecn_codepoint(ECN_ECT1));
  writer_->SetWriteBlocked();
  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(2);
  SendPing();
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(true));
  EXPECT_TRUE(connection_.set_ecn_codepoint(ECN_ECT0));
  writer_->SetWritable();
  connection_.OnCanWrite();
  EXPECT_EQ(writer_->last_ecn_sent(), ECN_ECT1);
}

TEST_P(QuicConnectionTest, RejectEcnIfWriterDoesNotSupport) {
  MockPacketWriter mock_writer;
  QuicConnectionPeer::SetWriter(&connection_, &mock_writer, false);
  EXPECT_CALL(mock_writer, SupportsEcn()).WillOnce(Return(false));
  EXPECT_FALSE(connection_.set_ecn_codepoint(ECN_ECT1));
  EXPECT_EQ(connection_.ecn_codepoint(), ECN_NOT_ECT);
}

TEST_P(QuicConnectionTest, RejectResetStreamAtIfNotNegotiated) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  config.SetReliableStreamReset(false);
  connection_.SetFromConfig(config);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);

  EXPECT_CALL(visitor_, OnConnectionClosed(_, _)).Times(1);
  connection_.OnResetStreamAtFrame(QuicResetStreamAtFrame());
}

TEST_P(QuicConnectionTest, ResetStreamAt) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  QuicConfig config;
  config.SetReliableStreamReset(true);
  connection_.SetFromConfig(config);
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);

  EXPECT_CALL(visitor_, OnResetStreamAt(QuicResetStreamAtFrame(
                            0, 0, QUIC_STREAM_NO_ERROR, 20, 10)))
      .Times(1);
  connection_.OnResetStreamAtFrame(QuicResetStreamAtFrame(0, 0, 0, 20, 10));
}

TEST_P(QuicConnectionTest, OnParsedClientHelloInfoWithDebugVisitor) {
  const ParsedClientHello parsed_chlo{.sni = "sni",
                                      .uaid = "uiad",
                                      .supported_groups = {1, 2, 3},
                                      .cert_compression_algos = {4, 5, 6},
                                      .alpns = {"h2", "http/1.1"},
                                      .retry_token = "retry_token"};
  MockQuicConnectionDebugVisitor debug_visitor;
  connection_.set_debug_visitor(&debug_visitor);
  EXPECT_CALL(debug_visitor, OnParsedClientHelloInfo(parsed_chlo)).Times(1);
  connection_.OnParsedClientHelloInfo(parsed_chlo);
}

TEST_P(QuicConnectionTest, ConfigEnablesAckFrequency) {
  QuicConfig config;
  EXPECT_FALSE(QuicConnectionPeer::CanReceiveAckFrequencyFrames(&connection_));
  config.SetMinAckDelayDraft10Ms(kDefaultMinAckDelayTimeMs);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, EnableECT1()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm_, EnableECT0()).WillOnce(Return(false));
  connection_.SetFromConfig(config);
  EXPECT_TRUE(QuicConnectionPeer::CanReceiveAckFrequencyFrames(&connection_));
}

}  // namespace
}  // namespace test
}  // namespace quic
