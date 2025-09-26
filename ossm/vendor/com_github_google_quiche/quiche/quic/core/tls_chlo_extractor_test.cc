// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/tls_chlo_extractor.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "openssl/ssl.h"
#include "quiche/quic/core/http/quic_spdy_client_session.h"
#include "quiche/quic/core/quic_connection.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/crypto_test_utils.h"
#include "quiche/quic/test_tools/first_flight.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/quic/test_tools/simple_session_cache.h"
#include "quiche/common/print_elements.h"

namespace quic {
namespace test {
namespace {

static int DummyCompressFunc(SSL* /*ssl*/, CBB* /*out*/, const uint8_t* /*in*/,
                             size_t /*in_len*/) {
  return 1;
}

static int DummyDecompressFunc(SSL* /*ssl*/, CRYPTO_BUFFER** /*out*/,
                               size_t /*uncompressed_len*/,
                               const uint8_t* /*in*/, size_t /*in_len*/) {
  return 1;
}

using testing::_;
using testing::AnyNumber;

class TlsChloExtractorTest : public QuicTestWithParam<ParsedQuicVersion> {
 protected:
  TlsChloExtractorTest() : version_(GetParam()), server_id_(TestServerId()) {}

  void Initialize() {
    tls_chlo_extractor_ = std::make_unique<TlsChloExtractor>();
    AnnotatedPackets packets =
        GetAnnotatedFirstFlightOfPackets(version_, config_);
    packets_ = std::move(packets.packets);
    crypto_stream_size_ = packets.crypto_stream_size;
    QUIC_DLOG(INFO) << "Initialized with " << packets_.size()
                    << " packets with crypto_stream_size:"
                    << crypto_stream_size_;
  }

  void Initialize(std::unique_ptr<QuicCryptoClientConfig> crypto_config) {
    tls_chlo_extractor_ = std::make_unique<TlsChloExtractor>();
    AnnotatedPackets packets = GetAnnotatedFirstFlightOfPackets(
        version_, config_, TestConnectionId(), EmptyQuicConnectionId(),
        std::move(crypto_config));
    packets_ = std::move(packets.packets);
    crypto_stream_size_ = packets.crypto_stream_size;
    QUIC_DLOG(INFO) << "Initialized with " << packets_.size()
                    << " packets with crypto_stream_size:"
                    << crypto_stream_size_;
  }

  // Perform a full handshake in order to insert a SSL_SESSION into
  // crypto_config->session_cache(), which can be used by a TLS resumption.
  void PerformFullHandshake(QuicCryptoClientConfig* crypto_config) const {
    ASSERT_NE(crypto_config->session_cache(), nullptr);
    MockQuicConnectionHelper client_helper, server_helper;
    MockAlarmFactory alarm_factory;
    ParsedQuicVersionVector supported_versions = {version_};
    PacketSavingConnection* client_connection =
        new PacketSavingConnection(&client_helper, &alarm_factory,
                                   Perspective::IS_CLIENT, supported_versions);
    // Advance the time, because timers do not like uninitialized times.
    client_connection->AdvanceTime(QuicTime::Delta::FromSeconds(1));
    QuicSpdyClientSession client_session(config_, supported_versions,
                                         client_connection, server_id_,
                                         crypto_config);
    client_session.Initialize();

    std::unique_ptr<QuicCryptoServerConfig> server_crypto_config =
        crypto_test_utils::CryptoServerConfigForTesting();
    QuicConfig server_config;

    EXPECT_CALL(*client_connection, SendCryptoData(_, _, _)).Times(AnyNumber());
    client_session.GetMutableCryptoStream()->CryptoConnect();

    crypto_test_utils::HandshakeWithFakeServer(
        &server_config, server_crypto_config.get(), &server_helper,
        &alarm_factory, client_connection,
        client_session.GetMutableCryptoStream(),
        AlpnForVersion(client_connection->version()));

    // For some reason, the test client can not receive the server settings and
    // the SSL_SESSION will not be inserted to client's session_cache. We create
    // a dummy settings and call SetServerApplicationStateForResumption manually
    // to ensure the SSL_SESSION is cached.
    // TODO(wub): Fix crypto_test_utils::HandshakeWithFakeServer to make sure a
    // SSL_SESSION is cached at the client, and remove the rest of the function.
    SettingsFrame server_settings;
    server_settings.values[SETTINGS_QPACK_MAX_TABLE_CAPACITY] =
        kDefaultQpackMaxDynamicTableCapacity;
    std::string settings_frame =
        HttpEncoder::SerializeSettingsFrame(server_settings);
    client_session.GetMutableCryptoStream()
        ->SetServerApplicationStateForResumption(
            std::make_unique<ApplicationState>(
                settings_frame.data(),
                settings_frame.data() + settings_frame.length()));
  }

  void IngestPackets() {
    for (const std::unique_ptr<QuicReceivedPacket>& packet : packets_) {
      ReceivedPacketInfo packet_info(
          QuicSocketAddress(TestPeerIPAddress(), kTestPort),
          QuicSocketAddress(TestPeerIPAddress(), kTestPort), *packet);
      std::string detailed_error;
      std::optional<absl::string_view> retry_token;
      absl::string_view destination_connection_id, source_connection_id;
      const QuicErrorCode error = QuicFramer::ParsePublicHeaderDispatcher(
          *packet, /*expected_destination_connection_id_length=*/0,
          &packet_info.form, &packet_info.long_packet_type,
          &packet_info.version_flag, &packet_info.use_length_prefix,
          &packet_info.version_label, &packet_info.version,
          &destination_connection_id, &source_connection_id, &retry_token,
          &detailed_error);
      ASSERT_THAT(error, IsQuicNoError()) << detailed_error;
      tls_chlo_extractor_->IngestPacket(packet_info.version,
                                        packet_info.packet);
    }
    packets_.clear();
  }

  void ValidateChloDetails(const TlsChloExtractor* extractor = nullptr) const {
    if (extractor == nullptr) {
      extractor = tls_chlo_extractor_.get();
    }

    EXPECT_TRUE(extractor->HasParsedFullChlo());
    std::vector<std::string> alpns = extractor->alpns();
    ASSERT_EQ(alpns.size(), 1u);
    EXPECT_EQ(alpns[0], AlpnForVersion(version_));
    EXPECT_EQ(extractor->server_name(), TestHostname());
    // Crypto stream has one frame in the following format:
    // CRYPTO Frame {
    //  Type (i) = 0x06,
    //  Offset (i),
    //  Length (i),
    //  Crypto Data (..),
    // }
    //
    // Type is 1 byte long, Offset is zero and also 1 byte long, and
    // all generated ClientHello messages have 2 byte length. So
    // the header is 4 bytes total.
    EXPECT_EQ(extractor->client_hello_bytes().size(), crypto_stream_size_ - 4);
  }

  void IncreaseSizeOfChlo() {
    // Add a 2000-byte custom parameter to increase the length of the CHLO.
    constexpr auto kCustomParameterId =
        static_cast<TransportParameters::TransportParameterId>(0xff33);
    std::string kCustomParameterValue(2000, '-');
    config_.custom_transport_parameters_to_send()[kCustomParameterId] =
        kCustomParameterValue;
  }

  ParsedQuicVersion version_;
  QuicServerId server_id_;
  std::unique_ptr<TlsChloExtractor> tls_chlo_extractor_;
  QuicConfig config_;
  std::vector<std::unique_ptr<QuicReceivedPacket>> packets_;
  uint64_t crypto_stream_size_;
};

INSTANTIATE_TEST_SUITE_P(TlsChloExtractorTests, TlsChloExtractorTest,
                         ::testing::ValuesIn(AllSupportedVersionsWithTls()),
                         ::testing::PrintToStringParamName());

TEST_P(TlsChloExtractorTest, Simple) {
  Initialize();
  EXPECT_EQ(packets_.size(), 1u);
  IngestPackets();
  ValidateChloDetails();
  EXPECT_EQ(tls_chlo_extractor_->state(),
            TlsChloExtractor::State::kParsedFullSinglePacketChlo);
  EXPECT_FALSE(tls_chlo_extractor_->resumption_attempted());
  EXPECT_FALSE(tls_chlo_extractor_->early_data_attempted());
}

TEST_P(TlsChloExtractorTest, TlsExtensionInfo_ResumptionOnly) {
  auto crypto_client_config = std::make_unique<QuicCryptoClientConfig>(
      crypto_test_utils::ProofVerifierForTesting(),
      std::make_unique<SimpleSessionCache>());
  PerformFullHandshake(crypto_client_config.get());

  SSL_CTX_set_early_data_enabled(crypto_client_config->ssl_ctx(), 0);
  Initialize(std::move(crypto_client_config));
  EXPECT_GE(packets_.size(), 1u);
  IngestPackets();
  ValidateChloDetails();
  EXPECT_EQ(tls_chlo_extractor_->state(),
            TlsChloExtractor::State::kParsedFullSinglePacketChlo);
  EXPECT_TRUE(tls_chlo_extractor_->resumption_attempted());
  EXPECT_FALSE(tls_chlo_extractor_->early_data_attempted());
}

TEST_P(TlsChloExtractorTest, TlsExtensionInfo_ZeroRtt) {
  auto crypto_client_config = std::make_unique<QuicCryptoClientConfig>(
      crypto_test_utils::ProofVerifierForTesting(),
      std::make_unique<SimpleSessionCache>());
  PerformFullHandshake(crypto_client_config.get());

  IncreaseSizeOfChlo();
  Initialize(std::move(crypto_client_config));
  EXPECT_GE(packets_.size(), 1u);
  IngestPackets();
  ValidateChloDetails();
  EXPECT_EQ(tls_chlo_extractor_->state(),
            TlsChloExtractor::State::kParsedFullMultiPacketChlo);
  EXPECT_TRUE(tls_chlo_extractor_->resumption_attempted());
  EXPECT_TRUE(tls_chlo_extractor_->early_data_attempted());
}

TEST_P(TlsChloExtractorTest, TlsExtensionInfo_SupportedGroups) {
  const std::vector<std::vector<uint16_t>> preferred_groups_to_test = {
      // Only one group
      {SSL_GROUP_X25519},
      // Two groups
      {SSL_GROUP_X25519_MLKEM768, SSL_GROUP_X25519},
  };
  for (const std::vector<uint16_t>& preferred_groups :
       preferred_groups_to_test) {
    auto crypto_client_config = std::make_unique<QuicCryptoClientConfig>(
        crypto_test_utils::ProofVerifierForTesting());
    crypto_client_config->set_preferred_groups(preferred_groups);

    Initialize(std::move(crypto_client_config));
    IngestPackets();
    ValidateChloDetails();
    EXPECT_EQ(tls_chlo_extractor_->supported_groups(), preferred_groups);
  }
}

TEST_P(TlsChloExtractorTest, TlsExtensionInfo_CertCompressionAlgos) {
  const std::vector<std::vector<uint16_t>> supported_groups_to_test = {
      // No cert compression algos
      {},
      // One cert compression algo
      {1},
      // Two cert compression algos
      {1, 2},
      // Three cert compression algos
      {1, 2, 3},
      // Four cert compression algos
      {1, 2, 3, 65535},
  };
  for (const std::vector<uint16_t>& supported_cert_compression_algos :
       supported_groups_to_test) {
    auto crypto_client_config = std::make_unique<QuicCryptoClientConfig>(
        crypto_test_utils::ProofVerifierForTesting());
    for (uint16_t cert_compression_algo : supported_cert_compression_algos) {
      ASSERT_TRUE(SSL_CTX_add_cert_compression_alg(
          crypto_client_config->ssl_ctx(), cert_compression_algo,
          DummyCompressFunc, DummyDecompressFunc));
    }

    Initialize(std::move(crypto_client_config));
    IngestPackets();
    ValidateChloDetails();
    if (GetQuicReloadableFlag(quic_parse_cert_compression_algos_from_chlo)) {
      EXPECT_EQ(tls_chlo_extractor_->cert_compression_algos(),
                supported_cert_compression_algos)
          << quiche::PrintElements(
                 tls_chlo_extractor_->cert_compression_algos());
    } else {
      EXPECT_TRUE(tls_chlo_extractor_->cert_compression_algos().empty());
    }
  }
}

TEST_P(TlsChloExtractorTest, TlsExtensionInfo_QuicTransportParameters) {
  Initialize();
  EXPECT_EQ(packets_.size(), 1u);
  IngestPackets();
  ValidateChloDetails();

  // RFC QUIC has transport parameters, drafts doesn't.
  if (version_ == ParsedQuicVersion::RFCv1() ||
      version_ == ParsedQuicVersion::RFCv2()) {
    EXPECT_FALSE(tls_chlo_extractor_->transport_params().empty());
  } else {
    EXPECT_TRUE(tls_chlo_extractor_->transport_params().empty());
  }
}

TEST_P(TlsChloExtractorTest, MultiPacket) {
  IncreaseSizeOfChlo();
  Initialize();
  EXPECT_EQ(packets_.size(), 2u);
  IngestPackets();
  ValidateChloDetails();
  EXPECT_EQ(tls_chlo_extractor_->state(),
            TlsChloExtractor::State::kParsedFullMultiPacketChlo);
}

TEST_P(TlsChloExtractorTest, MultiPacketReordered) {
  IncreaseSizeOfChlo();
  Initialize();
  ASSERT_EQ(packets_.size(), 2u);
  // Artificially reorder both packets.
  std::swap(packets_[0], packets_[1]);
  IngestPackets();
  ValidateChloDetails();
  EXPECT_EQ(tls_chlo_extractor_->state(),
            TlsChloExtractor::State::kParsedFullMultiPacketChlo);
}

TEST_P(TlsChloExtractorTest, MoveAssignment) {
  Initialize();
  EXPECT_EQ(packets_.size(), 1u);
  TlsChloExtractor other_extractor;
  *tls_chlo_extractor_ = std::move(other_extractor);
  IngestPackets();
  ValidateChloDetails();
  EXPECT_EQ(tls_chlo_extractor_->state(),
            TlsChloExtractor::State::kParsedFullSinglePacketChlo);
}

TEST_P(TlsChloExtractorTest, MoveAssignmentAfterExtraction) {
  Initialize();
  EXPECT_EQ(packets_.size(), 1u);
  IngestPackets();
  ValidateChloDetails();
  EXPECT_EQ(tls_chlo_extractor_->state(),
            TlsChloExtractor::State::kParsedFullSinglePacketChlo);

  TlsChloExtractor other_extractor = std::move(*tls_chlo_extractor_);

  EXPECT_EQ(other_extractor.state(),
            TlsChloExtractor::State::kParsedFullSinglePacketChlo);
  ValidateChloDetails(&other_extractor);
}

TEST_P(TlsChloExtractorTest, MoveAssignmentBetweenPackets) {
  IncreaseSizeOfChlo();
  Initialize();
  ASSERT_EQ(packets_.size(), 2u);
  TlsChloExtractor other_extractor;

  // Have |other_extractor| parse the first packet.
  ReceivedPacketInfo packet_info(
      QuicSocketAddress(TestPeerIPAddress(), kTestPort),
      QuicSocketAddress(TestPeerIPAddress(), kTestPort), *packets_[0]);
  std::string detailed_error;
  std::optional<absl::string_view> retry_token;
  absl::string_view destination_connection_id, source_connection_id;
  const QuicErrorCode error = QuicFramer::ParsePublicHeaderDispatcher(
      *packets_[0], /*expected_destination_connection_id_length=*/0,
      &packet_info.form, &packet_info.long_packet_type,
      &packet_info.version_flag, &packet_info.use_length_prefix,
      &packet_info.version_label, &packet_info.version,
      &destination_connection_id, &source_connection_id, &retry_token,
      &detailed_error);
  ASSERT_THAT(error, IsQuicNoError()) << detailed_error;
  other_extractor.IngestPacket(packet_info.version, packet_info.packet);
  // Remove the first packet from the list.
  packets_.erase(packets_.begin());
  EXPECT_EQ(packets_.size(), 1u);

  // Move the extractor.
  *tls_chlo_extractor_ = std::move(other_extractor);

  // Have |tls_chlo_extractor_| parse the second packet.
  IngestPackets();

  ValidateChloDetails();
  EXPECT_EQ(tls_chlo_extractor_->state(),
            TlsChloExtractor::State::kParsedFullMultiPacketChlo);
}

}  // namespace
}  // namespace test
}  // namespace quic
