// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "openssl/hpke.h"
#include "openssl/ssl.h"
#include "quiche/quic/core/crypto/quic_decrypter.h"
#include "quiche/quic/core/crypto/quic_encrypter.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/quic/core/quic_packets.h"
#include "quiche/quic/core/quic_server_id.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/crypto_test_utils.h"
#include "quiche/quic/test_tools/quic_connection_peer.h"
#include "quiche/quic/test_tools/quic_framer_peer.h"
#include "quiche/quic/test_tools/quic_session_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/quic/test_tools/simple_session_cache.h"
#include "quiche/quic/tools/fake_proof_verifier.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

using testing::_;
using testing::HasSubstr;

namespace quic {
namespace test {
namespace {

constexpr char kServerHostname[] = "test.example.com";
constexpr uint16_t kServerPort = 443;

// TestProofVerifier wraps ProofVerifierForTesting, except for VerifyCertChain
// which, if TestProofVerifier is active, always returns QUIC_PENDING. (If this
// test proof verifier is not active, it delegates VerifyCertChain to the
// ProofVerifierForTesting.) The pending VerifyCertChain operation can be
// completed by calling InvokePendingCallback. This allows for testing
// asynchronous VerifyCertChain operations.
class TestProofVerifier : public ProofVerifier {
 public:
  TestProofVerifier()
      : verifier_(crypto_test_utils::ProofVerifierForTesting()) {}

  QuicAsyncStatus VerifyProof(
      const std::string& hostname, const uint16_t port,
      const std::string& server_config, QuicTransportVersion quic_version,
      absl::string_view chlo_hash, const std::vector<std::string>& certs,
      const std::string& cert_sct, const std::string& signature,
      const ProofVerifyContext* context, std::string* error_details,
      std::unique_ptr<ProofVerifyDetails>* details,
      std::unique_ptr<ProofVerifierCallback> callback) override {
    return verifier_->VerifyProof(
        hostname, port, server_config, quic_version, chlo_hash, certs, cert_sct,
        signature, context, error_details, details, std::move(callback));
  }

  QuicAsyncStatus VerifyCertChain(
      const std::string& hostname, const uint16_t port,
      const std::vector<std::string>& certs, const std::string& ocsp_response,
      const std::string& cert_sct, const ProofVerifyContext* context,
      std::string* error_details, std::unique_ptr<ProofVerifyDetails>* details,
      uint8_t* out_alert,
      std::unique_ptr<ProofVerifierCallback> callback) override {
    if (!active_) {
      return verifier_->VerifyCertChain(
          hostname, port, certs, ocsp_response, cert_sct, context,
          error_details, details, out_alert, std::move(callback));
    }
    pending_ops_.push_back(std::make_unique<VerifyChainPendingOp>(
        hostname, port, certs, ocsp_response, cert_sct, context, error_details,
        details, out_alert, std::move(callback), verifier_.get()));
    return QUIC_PENDING;
  }

  std::unique_ptr<ProofVerifyContext> CreateDefaultContext() override {
    return nullptr;
  }

  void Activate() { active_ = true; }

  size_t NumPendingCallbacks() const { return pending_ops_.size(); }

  void InvokePendingCallback(size_t n) {
    ASSERT_GT(NumPendingCallbacks(), n);
    pending_ops_[n]->Run();
    auto it = pending_ops_.begin() + n;
    pending_ops_.erase(it);
  }

 private:
  // Implementation of ProofVerifierCallback that fails if the callback is ever
  // run.
  class FailingProofVerifierCallback : public ProofVerifierCallback {
   public:
    void Run(bool /*ok*/, const std::string& /*error_details*/,
             std::unique_ptr<ProofVerifyDetails>* /*details*/) override {
      FAIL();
    }
  };

  class VerifyChainPendingOp {
   public:
    VerifyChainPendingOp(const std::string& hostname, const uint16_t port,
                         const std::vector<std::string>& certs,
                         const std::string& ocsp_response,
                         const std::string& cert_sct,
                         const ProofVerifyContext* context,
                         std::string* error_details,
                         std::unique_ptr<ProofVerifyDetails>* details,
                         uint8_t* out_alert,
                         std::unique_ptr<ProofVerifierCallback> callback,
                         ProofVerifier* delegate)
        : hostname_(hostname),
          port_(port),
          certs_(certs),
          ocsp_response_(ocsp_response),
          cert_sct_(cert_sct),
          context_(context),
          error_details_(error_details),
          details_(details),
          out_alert_(out_alert),
          callback_(std::move(callback)),
          delegate_(delegate) {}

    void Run() {
      // TestProofVerifier depends on crypto_test_utils::ProofVerifierForTesting
      // running synchronously. It passes a FailingProofVerifierCallback and
      // runs the original callback after asserting that the verification ran
      // synchronously.
      QuicAsyncStatus status = delegate_->VerifyCertChain(
          hostname_, port_, certs_, ocsp_response_, cert_sct_, context_,
          error_details_, details_, out_alert_,
          std::make_unique<FailingProofVerifierCallback>());
      ASSERT_NE(status, QUIC_PENDING);
      callback_->Run(status == QUIC_SUCCESS, *error_details_, details_);
    }

   private:
    std::string hostname_;
    const uint16_t port_;
    std::vector<std::string> certs_;
    std::string ocsp_response_;
    std::string cert_sct_;
    const ProofVerifyContext* context_;
    std::string* error_details_;
    std::unique_ptr<ProofVerifyDetails>* details_;
    uint8_t* out_alert_;
    std::unique_ptr<ProofVerifierCallback> callback_;
    ProofVerifier* delegate_;
  };

  std::unique_ptr<ProofVerifier> verifier_;
  bool active_ = false;
  std::vector<std::unique_ptr<VerifyChainPendingOp>> pending_ops_;
};

class TlsClientHandshakerTest : public QuicTestWithParam<ParsedQuicVersion> {
 public:
  TlsClientHandshakerTest()
      : supported_versions_({GetParam()}),
        server_id_(kServerHostname, kServerPort),
        server_compressed_certs_cache_(
            QuicCompressedCertsCache::kQuicCompressedCertsCacheSize) {
    crypto_config_ = std::make_unique<QuicCryptoClientConfig>(
        std::make_unique<TestProofVerifier>(),
        std::make_unique<test::SimpleSessionCache>());
    server_crypto_config_ = crypto_test_utils::CryptoServerConfigForTesting();
    CreateConnection();
  }

  void CreateSession() {
    session_ = std::make_unique<TestQuicSpdyClientSession>(
        connection_, DefaultQuicConfig(), supported_versions_, server_id_,
        crypto_config_.get(), ssl_config_);
    EXPECT_CALL(*session_, GetAlpnsToOffer())
        .WillRepeatedly(testing::Return(std::vector<std::string>(
            {AlpnForVersion(connection_->version())})));
  }

  void CreateConnection() {
    connection_ =
        new PacketSavingConnection(&client_helper_, &alarm_factory_,
                                   Perspective::IS_CLIENT, supported_versions_);
    // Advance the time, because timers do not like uninitialized times.
    connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
    CreateSession();
  }

  void CompleteCryptoHandshake() {
    CompleteCryptoHandshakeWithServerALPN(
        AlpnForVersion(connection_->version()));
  }

  void CompleteCryptoHandshakeWithServerALPN(const std::string& alpn) {
    EXPECT_CALL(*connection_, SendCryptoData(_, _, _))
        .Times(testing::AnyNumber());
    stream()->CryptoConnect();
    QuicConfig config;
    crypto_test_utils::HandshakeWithFakeServer(
        &config, server_crypto_config_.get(), &server_helper_, &alarm_factory_,
        connection_, stream(), alpn);
  }

  QuicCryptoClientStream* stream() {
    return session_->GetMutableCryptoStream();
  }

  QuicCryptoServerStreamBase* server_stream() {
    return server_session_->GetMutableCryptoStream();
  }

  // Initializes a fake server, and all its associated state, for testing.
  void InitializeFakeServer(const std::string& trust_anchor_id = "") {
    TestQuicSpdyServerSession* server_session = nullptr;
    server_crypto_config_ =
        crypto_test_utils::CryptoServerConfigForTesting(trust_anchor_id);
    CreateServerSessionForTest(
        server_id_, QuicTime::Delta::FromSeconds(100000), supported_versions_,
        &server_helper_, &alarm_factory_, server_crypto_config_.get(),
        &server_compressed_certs_cache_, &server_connection_, &server_session);
    server_session_.reset(server_session);
    std::string alpn = AlpnForVersion(connection_->version());
    EXPECT_CALL(*server_session_, SelectAlpn(_))
        .WillRepeatedly([alpn](const std::vector<absl::string_view>& alpns) {
          return std::find(alpns.cbegin(), alpns.cend(), alpn);
        });
  }

  static bssl::UniquePtr<SSL_ECH_KEYS> MakeTestEchKeys(
      const char* public_name, size_t max_name_len,
      std::string* ech_config_list) {
    bssl::ScopedEVP_HPKE_KEY key;
    if (!EVP_HPKE_KEY_generate(key.get(), EVP_hpke_x25519_hkdf_sha256())) {
      return nullptr;
    }

    uint8_t* ech_config;
    size_t ech_config_len;
    if (!SSL_marshal_ech_config(&ech_config, &ech_config_len,
                                /*config_id=*/1, key.get(), public_name,
                                max_name_len)) {
      return nullptr;
    }
    bssl::UniquePtr<uint8_t> scoped_ech_config(ech_config);

    uint8_t* ech_config_list_raw;
    size_t ech_config_list_len;
    bssl::UniquePtr<SSL_ECH_KEYS> keys(SSL_ECH_KEYS_new());
    if (!keys ||
        !SSL_ECH_KEYS_add(keys.get(), /*is_retry_config=*/1, ech_config,
                          ech_config_len, key.get()) ||
        !SSL_ECH_KEYS_marshal_retry_configs(keys.get(), &ech_config_list_raw,
                                            &ech_config_list_len)) {
      return nullptr;
    }
    bssl::UniquePtr<uint8_t> scoped_ech_config_list(ech_config_list_raw);

    ech_config_list->assign(ech_config_list_raw,
                            ech_config_list_raw + ech_config_list_len);
    return keys;
  }

  MockQuicConnectionHelper server_helper_;
  MockQuicConnectionHelper client_helper_;
  MockAlarmFactory alarm_factory_;
  PacketSavingConnection* connection_;
  ParsedQuicVersionVector supported_versions_;
  std::unique_ptr<TestQuicSpdyClientSession> session_;
  QuicServerId server_id_;
  CryptoHandshakeMessage message_;
  std::unique_ptr<QuicCryptoClientConfig> crypto_config_;
  std::optional<QuicSSLConfig> ssl_config_;

  // Server state.
  std::unique_ptr<QuicCryptoServerConfig> server_crypto_config_;
  PacketSavingConnection* server_connection_;
  std::unique_ptr<TestQuicSpdyServerSession> server_session_;
  QuicCompressedCertsCache server_compressed_certs_cache_;
};

INSTANTIATE_TEST_SUITE_P(TlsHandshakerTests, TlsClientHandshakerTest,
                         ::testing::ValuesIn(AllSupportedVersionsWithTls()),
                         ::testing::PrintToStringParamName());

TEST_P(TlsClientHandshakerTest, NotInitiallyConnected) {
  EXPECT_FALSE(stream()->encryption_established());
  EXPECT_FALSE(stream()->one_rtt_keys_available());
}

TEST_P(TlsClientHandshakerTest, ConnectedAfterHandshake) {
  CompleteCryptoHandshake();
  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_FALSE(stream()->MatchedTrustAnchorIdForTesting());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  EXPECT_FALSE(stream()->IsResumption());
}

TEST_P(TlsClientHandshakerTest, ConnectionClosedOnTlsError) {
  // Have client send ClientHello.
  stream()->CryptoConnect();
  EXPECT_CALL(*connection_, CloseConnection(QUIC_HANDSHAKE_FAILED, _, _, _));

  // Send a zero-length ServerHello from server to client.
  char bogus_handshake_message[] = {
      // Handshake struct (RFC 8446 appendix B.3)
      2,        // HandshakeType server_hello
      0, 0, 0,  // uint24 length
  };
  stream()->crypto_message_parser()->ProcessInput(
      absl::string_view(bogus_handshake_message,
                        ABSL_ARRAYSIZE(bogus_handshake_message)),
      ENCRYPTION_INITIAL);

  EXPECT_FALSE(stream()->one_rtt_keys_available());
}

TEST_P(TlsClientHandshakerTest, ProofVerifyDetailsAvailableAfterHandshake) {
  EXPECT_CALL(*session_, OnProofVerifyDetailsAvailable(testing::_));
  stream()->CryptoConnect();
  QuicConfig config;
  crypto_test_utils::HandshakeWithFakeServer(
      &config, server_crypto_config_.get(), &server_helper_, &alarm_factory_,
      connection_, stream(), AlpnForVersion(connection_->version()));
  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
}

TEST_P(TlsClientHandshakerTest, HandshakeWithAsyncProofVerifier) {
  InitializeFakeServer();

  // Enable TestProofVerifier to capture call to VerifyCertChain and run it
  // asynchronously.
  TestProofVerifier* proof_verifier =
      static_cast<TestProofVerifier*>(crypto_config_->proof_verifier());
  proof_verifier->Activate();

  stream()->CryptoConnect();
  // Exchange handshake messages.
  std::pair<size_t, size_t> moved_message_counts =
      crypto_test_utils::AdvanceHandshake(
          connection_, stream(), 0, server_connection_, server_stream(), 0);

  ASSERT_EQ(proof_verifier->NumPendingCallbacks(), 1u);
  proof_verifier->InvokePendingCallback(0);

  // Exchange more handshake messages.
  crypto_test_utils::AdvanceHandshake(
      connection_, stream(), moved_message_counts.first, server_connection_,
      server_stream(), moved_message_counts.second);

  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
}

TEST_P(TlsClientHandshakerTest, HandshakeWithTrustAnchorIds) {
  SetQuicReloadableFlag(enable_tls_trust_anchor_ids, true);
  const std::string kTestTrustAnchorId = {0x03, 0x01, 0x02, 0x03};
  const std::string kTestServerTrustAnchorId = {0x01, 0x02, 0x03};
  InitializeFakeServer(kTestServerTrustAnchorId);
  ssl_config_.emplace();
  ssl_config_->trust_anchor_ids = kTestTrustAnchorId;
  CreateConnection();
  CompleteCryptoHandshake();
  ASSERT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->MatchedTrustAnchorIdForTesting());
}

// Tests that the client can complete a handshake in which it sends multiple
// Trust Anchor IDs, one which matches the server's credential and one which
// doesn't.
TEST_P(TlsClientHandshakerTest, HandshakeWithMultipleTrustAnchorIds) {
  SetQuicReloadableFlag(enable_tls_trust_anchor_ids, true);
  // The client sends two trust anchor IDs, the first of which doesn't match the
  // server's credential and the second does.
  const std::string kTestTrustAnchorIds = {0x04, 0x00, 0x01, 0x02, 0x03,
                                           0x03, 0x01, 0x02, 0x03};
  const std::string kTestServerTrustAnchorId = {0x01, 0x02, 0x03};
  InitializeFakeServer(kTestServerTrustAnchorId);
  ssl_config_.emplace();
  ssl_config_->trust_anchor_ids = kTestTrustAnchorIds;
  CreateConnection();
  CompleteCryptoHandshake();
  ASSERT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->MatchedTrustAnchorIdForTesting());
}

// Tests that the client can complete a handshake in which it sends no Trust
// Anchor IDs.
TEST_P(TlsClientHandshakerTest, HandshakeWithEmptyTrustAnchorIdList) {
  SetQuicReloadableFlag(enable_tls_trust_anchor_ids, true);
  InitializeFakeServer("");
  ssl_config_.emplace();
  ssl_config_->trust_anchor_ids.emplace();
  CreateConnection();

  // Add a DoS callback on the server, to test that the client sent an empty
  // extension. This is a bit of a hack. TlsServerHandshaker already configures
  // the certificate selection callback, but does not usefully expose any way
  // for tests to inspect the ClientHello. So, instead, we register a different
  // callback that also gets the ClientHello.
  static bool callback_ran;
  callback_ran = false;
  SSL_CTX_set_dos_protection_cb(
      server_crypto_config_->ssl_ctx(),
      [](const SSL_CLIENT_HELLO* client_hello) -> int {
        const uint8_t* data;
        size_t len;
        EXPECT_TRUE(SSL_early_callback_ctx_extension_get(
            client_hello, TLSEXT_TYPE_trust_anchors, &data, &len));
        // The extension should contain an empty list, i.e. a two-byte encoding
        // of a zero length.
        EXPECT_EQ(len, 2u);
        EXPECT_EQ(data[0], 0x00);
        EXPECT_EQ(data[1], 0x00);
        callback_ran = true;
        return 1;
      });

  CompleteCryptoHandshake();
  ASSERT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(callback_ran);
}

TEST_P(TlsClientHandshakerTest, Resumption) {
  // Disable 0-RTT on the server so that we're only testing 1-RTT resumption:
  SSL_CTX_set_early_data_enabled(server_crypto_config_->ssl_ctx(), false);
  // Finish establishing the first connection:
  CompleteCryptoHandshake();

  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  EXPECT_FALSE(stream()->ResumptionAttempted());
  EXPECT_FALSE(stream()->IsResumption());

  // Create a second connection
  CreateConnection();
  CompleteCryptoHandshake();

  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  EXPECT_TRUE(stream()->ResumptionAttempted());
  EXPECT_TRUE(stream()->IsResumption());
}

TEST_P(TlsClientHandshakerTest, ResumptionRejection) {
  // Disable 0-RTT on the server before the first connection so the client
  // doesn't attempt a 0-RTT resumption, only a 1-RTT resumption.
  SSL_CTX_set_early_data_enabled(server_crypto_config_->ssl_ctx(), false);
  // Finish establishing the first connection:
  CompleteCryptoHandshake();

  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  EXPECT_FALSE(stream()->ResumptionAttempted());
  EXPECT_FALSE(stream()->IsResumption());

  // Create a second connection, but disable resumption on the server.
  SSL_CTX_set_options(server_crypto_config_->ssl_ctx(), SSL_OP_NO_TICKET);
  CreateConnection();
  CompleteCryptoHandshake();

  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  EXPECT_TRUE(stream()->ResumptionAttempted());
  EXPECT_FALSE(stream()->IsResumption());
  EXPECT_FALSE(stream()->EarlyDataAccepted());
  EXPECT_EQ(stream()->EarlyDataReason(),
            ssl_early_data_unsupported_for_session);
}

TEST_P(TlsClientHandshakerTest, ZeroRttResumption) {
  // Finish establishing the first connection:
  CompleteCryptoHandshake();

  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  EXPECT_FALSE(stream()->IsResumption());

  // Create a second connection
  CreateConnection();
  // OnConfigNegotiated should be called twice - once when processing saved
  // 0-RTT transport parameters, and then again when receiving transport
  // parameters from the server.
  EXPECT_CALL(*session_, OnConfigNegotiated()).Times(2);
  EXPECT_CALL(*connection_, SendCryptoData(_, _, _))
      .Times(testing::AnyNumber());
  // Start the second handshake and confirm we have keys before receiving any
  // messages from the server.
  stream()->CryptoConnect();
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_NE(stream()->crypto_negotiated_params().cipher_suite, 0);
  EXPECT_NE(stream()->crypto_negotiated_params().key_exchange_group, 0);
  EXPECT_NE(stream()->crypto_negotiated_params().peer_signature_algorithm, 0);
  // Finish the handshake with the server.
  QuicConfig config;
  crypto_test_utils::HandshakeWithFakeServer(
      &config, server_crypto_config_.get(), &server_helper_, &alarm_factory_,
      connection_, stream(), AlpnForVersion(connection_->version()));

  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  EXPECT_TRUE(stream()->IsResumption());
  EXPECT_TRUE(stream()->EarlyDataAccepted());
  EXPECT_EQ(stream()->EarlyDataReason(), ssl_early_data_accepted);
}

// Regression test for b/186438140.
TEST_P(TlsClientHandshakerTest, ZeroRttResumptionWithAyncProofVerifier) {
  // Finish establishing the first connection, so the second connection can
  // resume.
  CompleteCryptoHandshake();

  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  EXPECT_FALSE(stream()->IsResumption());

  // Create a second connection.
  CreateConnection();
  InitializeFakeServer();
  EXPECT_CALL(*session_, OnConfigNegotiated());
  EXPECT_CALL(*connection_, SendCryptoData(_, _, _))
      .Times(testing::AnyNumber());
  // Enable TestProofVerifier to capture the call to VerifyCertChain and run it
  // asynchronously.
  TestProofVerifier* proof_verifier =
      static_cast<TestProofVerifier*>(crypto_config_->proof_verifier());
  proof_verifier->Activate();
  // Start the second handshake.
  stream()->CryptoConnect();

  ASSERT_EQ(proof_verifier->NumPendingCallbacks(), 1u);

  // Advance the handshake with the server. Since cert verification has not
  // finished yet, client cannot derive HANDSHAKE and 1-RTT keys.
  crypto_test_utils::AdvanceHandshake(connection_, stream(), 0,
                                      server_connection_, server_stream(), 0);

  EXPECT_FALSE(stream()->one_rtt_keys_available());
  EXPECT_FALSE(server_stream()->one_rtt_keys_available());

  // Finish cert verification after receiving packets from server.
  proof_verifier->InvokePendingCallback(0);

  QuicFramer* framer = QuicConnectionPeer::GetFramer(connection_);
  // Verify client has derived HANDSHAKE key.
  EXPECT_NE(nullptr,
            QuicFramerPeer::GetEncrypter(framer, ENCRYPTION_HANDSHAKE));

  // Ideally, we should also verify that the process_undecryptable_packets_alarm
  // is set and processing the undecryptable packets can advance the handshake
  // to completion. Unfortunately, the test facilities used in this test does
  // not support queuing and processing undecryptable packets.
}

TEST_P(TlsClientHandshakerTest, ZeroRttRejection) {
  // Finish establishing the first connection:
  CompleteCryptoHandshake();

  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  EXPECT_FALSE(stream()->IsResumption());

  // Create a second connection, but disable 0-RTT on the server.
  SSL_CTX_set_early_data_enabled(server_crypto_config_->ssl_ctx(), false);
  CreateConnection();

  // OnConfigNegotiated should be called twice - once when processing saved
  // 0-RTT transport parameters, and then again when receiving transport
  // parameters from the server.
  EXPECT_CALL(*session_, OnConfigNegotiated()).Times(2);

  // 4 packets will be sent in this connection: initial handshake packet, 0-RTT
  // packet containing SETTINGS, handshake packet upon 0-RTT rejection, 0-RTT
  // packet retransmission.
  EXPECT_CALL(*connection_,
              OnPacketSent(ENCRYPTION_INITIAL, NOT_RETRANSMISSION));
  if (VersionUsesHttp3(session_->transport_version())) {
    EXPECT_CALL(*connection_,
                OnPacketSent(ENCRYPTION_ZERO_RTT, NOT_RETRANSMISSION));
  }
  EXPECT_CALL(*connection_,
              OnPacketSent(ENCRYPTION_HANDSHAKE, NOT_RETRANSMISSION));
  if (VersionUsesHttp3(session_->transport_version())) {
    // TODO(b/158027651): change transmission type to
    // ALL_ZERO_RTT_RETRANSMISSION.
    EXPECT_CALL(*connection_,
                OnPacketSent(ENCRYPTION_FORWARD_SECURE, LOSS_RETRANSMISSION));
  }

  CompleteCryptoHandshake();

  QuicFramer* framer = QuicConnectionPeer::GetFramer(connection_);
  EXPECT_EQ(nullptr, QuicFramerPeer::GetEncrypter(framer, ENCRYPTION_ZERO_RTT));

  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  EXPECT_TRUE(stream()->IsResumption());
  EXPECT_FALSE(stream()->EarlyDataAccepted());
  EXPECT_EQ(stream()->EarlyDataReason(), ssl_early_data_peer_declined);
}

TEST_P(TlsClientHandshakerTest, ZeroRttAndResumptionRejection) {
  // Finish establishing the first connection:
  CompleteCryptoHandshake();

  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  EXPECT_FALSE(stream()->IsResumption());

  // Create a second connection, but disable resumption on the server.
  SSL_CTX_set_options(server_crypto_config_->ssl_ctx(), SSL_OP_NO_TICKET);
  CreateConnection();

  // OnConfigNegotiated should be called twice - once when processing saved
  // 0-RTT transport parameters, and then again when receiving transport
  // parameters from the server.
  EXPECT_CALL(*session_, OnConfigNegotiated()).Times(2);

  // 4 packets will be sent in this connection: initial handshake packet, 0-RTT
  // packet containing SETTINGS, handshake packet upon 0-RTT rejection, 0-RTT
  // packet retransmission.
  EXPECT_CALL(*connection_,
              OnPacketSent(ENCRYPTION_INITIAL, NOT_RETRANSMISSION));
  if (VersionUsesHttp3(session_->transport_version())) {
    EXPECT_CALL(*connection_,
                OnPacketSent(ENCRYPTION_ZERO_RTT, NOT_RETRANSMISSION));
  }
  EXPECT_CALL(*connection_,
              OnPacketSent(ENCRYPTION_HANDSHAKE, NOT_RETRANSMISSION));
  if (VersionUsesHttp3(session_->transport_version())) {
    // TODO(b/158027651): change transmission type to
    // ALL_ZERO_RTT_RETRANSMISSION.
    EXPECT_CALL(*connection_,
                OnPacketSent(ENCRYPTION_FORWARD_SECURE, LOSS_RETRANSMISSION));
  }

  CompleteCryptoHandshake();

  QuicFramer* framer = QuicConnectionPeer::GetFramer(connection_);
  EXPECT_EQ(nullptr, QuicFramerPeer::GetEncrypter(framer, ENCRYPTION_ZERO_RTT));

  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  EXPECT_FALSE(stream()->IsResumption());
  EXPECT_FALSE(stream()->EarlyDataAccepted());
  EXPECT_EQ(stream()->EarlyDataReason(), ssl_early_data_session_not_resumed);
}

TEST_P(TlsClientHandshakerTest, ClientSendsNoSNI) {
  // Reconfigure client to sent an empty server hostname. The crypto config also
  // needs to be recreated to use a FakeProofVerifier since the server's cert
  // won't match the empty hostname.
  server_id_ = QuicServerId("", 443);
  crypto_config_.reset(new QuicCryptoClientConfig(
      std::make_unique<FakeProofVerifier>(), nullptr));
  CreateConnection();
  InitializeFakeServer();

  stream()->CryptoConnect();
  crypto_test_utils::CommunicateHandshakeMessages(
      connection_, stream(), server_connection_, server_stream());

  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());

  EXPECT_EQ(server_stream()->crypto_negotiated_params().sni, "");
}

TEST_P(TlsClientHandshakerTest, ClientSendingTooManyALPNs) {
  std::string long_alpn(250, 'A');
  EXPECT_QUIC_BUG(
      {
        EXPECT_CALL(*session_, GetAlpnsToOffer())
            .WillOnce(testing::Return(std::vector<std::string>({
                long_alpn + "1",
                long_alpn + "2",
                long_alpn + "3",
                long_alpn + "4",
                long_alpn + "5",
                long_alpn + "6",
                long_alpn + "7",
                long_alpn + "8",
            })));
        stream()->CryptoConnect();
      },
      "Failed to set ALPN");
}

TEST_P(TlsClientHandshakerTest, ServerRequiresCustomALPN) {
  InitializeFakeServer();
  const std::string kTestAlpn = "An ALPN That Client Did Not Offer";
  EXPECT_CALL(*server_session_, SelectAlpn(_))
      .WillOnce([kTestAlpn](const std::vector<absl::string_view>& alpns) {
        return std::find(alpns.cbegin(), alpns.cend(), kTestAlpn);
      });

  EXPECT_CALL(
      *server_connection_,
      CloseConnection(
          QUIC_HANDSHAKE_FAILED,
          static_cast<QuicIetfTransportErrorCodes>(CRYPTO_ERROR_FIRST + 120),
          HasSubstr("TLS handshake failure (ENCRYPTION_INITIAL) 120: "
                    "no application protocol"),
          _));

  stream()->CryptoConnect();
  crypto_test_utils::AdvanceHandshake(connection_, stream(), 0,
                                      server_connection_, server_stream(), 0);

  EXPECT_FALSE(stream()->one_rtt_keys_available());
  EXPECT_FALSE(server_stream()->one_rtt_keys_available());
  EXPECT_FALSE(stream()->encryption_established());
  EXPECT_FALSE(server_stream()->encryption_established());
}

TEST_P(TlsClientHandshakerTest, ZeroRTTNotAttemptedOnALPNChange) {
  // Finish establishing the first connection:
  CompleteCryptoHandshake();

  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  EXPECT_FALSE(stream()->IsResumption());

  // Create a second connection
  CreateConnection();
  // Override the ALPN to send on the second connection.
  const std::string kTestAlpn = "Test ALPN";
  EXPECT_CALL(*session_, GetAlpnsToOffer())
      .WillRepeatedly(testing::Return(std::vector<std::string>({kTestAlpn})));
  // OnConfigNegotiated should only be called once: when transport parameters
  // are received from the server.
  EXPECT_CALL(*session_, OnConfigNegotiated()).Times(1);

  CompleteCryptoHandshakeWithServerALPN(kTestAlpn);
  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  EXPECT_FALSE(stream()->EarlyDataAccepted());
  EXPECT_EQ(stream()->EarlyDataReason(), ssl_early_data_alpn_mismatch);
}

TEST_P(TlsClientHandshakerTest, InvalidSNI) {
  // Test that a client will skip sending SNI if configured to send an invalid
  // hostname. In this case, the inclusion of '!' is invalid.
  server_id_ = QuicServerId("invalid!.example.com", 443);
  crypto_config_.reset(new QuicCryptoClientConfig(
      std::make_unique<FakeProofVerifier>(), nullptr));
  CreateConnection();
  InitializeFakeServer();

  stream()->CryptoConnect();
  crypto_test_utils::CommunicateHandshakeMessages(
      connection_, stream(), server_connection_, server_stream());

  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());

  EXPECT_EQ(server_stream()->crypto_negotiated_params().sni, "");
}

TEST_P(TlsClientHandshakerTest, BadTransportParams) {
  if (!connection_->version().UsesHttp3()) {
    return;
  }
  // Finish establishing the first connection:
  CompleteCryptoHandshake();

  // Create a second connection
  CreateConnection();

  stream()->CryptoConnect();
  auto* id_manager = QuicSessionPeer::ietf_streamid_manager(session_.get());
  EXPECT_EQ(kDefaultMaxStreamsPerConnection,
            id_manager->max_outgoing_bidirectional_streams());
  QuicConfig config;
  config.SetMaxBidirectionalStreamsToSend(
      config.GetMaxBidirectionalStreamsToSend() - 1);

  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_ZERO_RTT_REJECTION_LIMIT_REDUCED, _, _))
      .WillOnce(testing::Invoke(connection_,
                                &MockQuicConnection::ReallyCloseConnection));
  // Close connection will be called again in the handshaker, but this will be
  // no-op as the connection is already closed.
  EXPECT_CALL(*connection_, CloseConnection(QUIC_HANDSHAKE_FAILED, _, _));

  crypto_test_utils::HandshakeWithFakeServer(
      &config, server_crypto_config_.get(), &server_helper_, &alarm_factory_,
      connection_, stream(), AlpnForVersion(connection_->version()));
}

TEST_P(TlsClientHandshakerTest, ECH) {
  ssl_config_.emplace();
  bssl::UniquePtr<SSL_ECH_KEYS> ech_keys =
      MakeTestEchKeys("public-name.example", /*max_name_len=*/64,
                      &ssl_config_->ech_config_list);
  ASSERT_TRUE(ech_keys);

  // Configure the server to use the test ECH keys.
  ASSERT_TRUE(
      SSL_CTX_set1_ech_keys(server_crypto_config_->ssl_ctx(), ech_keys.get()));

  // Recreate the client to pick up the new `ssl_config_`.
  CreateConnection();

  // The handshake should complete and negotiate ECH.
  CompleteCryptoHandshake();
  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  EXPECT_TRUE(stream()->crypto_negotiated_params().encrypted_client_hello);
}

TEST_P(TlsClientHandshakerTest, ECHWithConfigAndGREASE) {
  ssl_config_.emplace();
  bssl::UniquePtr<SSL_ECH_KEYS> ech_keys =
      MakeTestEchKeys("public-name.example", /*max_name_len=*/64,
                      &ssl_config_->ech_config_list);
  ASSERT_TRUE(ech_keys);
  ssl_config_->ech_grease_enabled = true;

  // Configure the server to use the test ECH keys.
  ASSERT_TRUE(
      SSL_CTX_set1_ech_keys(server_crypto_config_->ssl_ctx(), ech_keys.get()));

  // Recreate the client to pick up the new `ssl_config_`.
  CreateConnection();

  // When both ECH and ECH GREASE are enabled, ECH should take precedence.
  // The handshake should complete and negotiate ECH.
  CompleteCryptoHandshake();
  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  EXPECT_TRUE(stream()->crypto_negotiated_params().encrypted_client_hello);
}

TEST_P(TlsClientHandshakerTest, ECHInvalidConfig) {
  // An invalid ECHConfigList should fail before sending a ClientHello.
  ssl_config_.emplace();
  ssl_config_->ech_config_list = "invalid config";
  CreateConnection();
  EXPECT_CALL(*connection_, CloseConnection(QUIC_HANDSHAKE_FAILED, _, _));
  stream()->CryptoConnect();
}

TEST_P(TlsClientHandshakerTest, ECHWrongKeys) {
  ssl_config_.emplace();
  bssl::UniquePtr<SSL_ECH_KEYS> ech_keys1 =
      MakeTestEchKeys("public-name.example", /*max_name_len=*/64,
                      &ssl_config_->ech_config_list);
  ASSERT_TRUE(ech_keys1);

  std::string ech_config_list2;
  bssl::UniquePtr<SSL_ECH_KEYS> ech_keys2 = MakeTestEchKeys(
      "public-name.example", /*max_name_len=*/64, &ech_config_list2);
  ASSERT_TRUE(ech_keys2);

  // Configure the server to use different keys from what the client has.
  ASSERT_TRUE(
      SSL_CTX_set1_ech_keys(server_crypto_config_->ssl_ctx(), ech_keys2.get()));

  // Recreate the client to pick up the new `ssl_config_`.
  CreateConnection();

  // TODO(crbug.com/1287248): This should instead output sufficient information
  // to run the recovery flow.
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_HANDSHAKE_FAILED,
                              static_cast<QuicIetfTransportErrorCodes>(
                                  CRYPTO_ERROR_FIRST + SSL_AD_ECH_REQUIRED),
                              _, _))
      .WillOnce(testing::Invoke(connection_,
                                &MockQuicConnection::ReallyCloseConnection4));

  // The handshake should complete and negotiate ECH.
  CompleteCryptoHandshake();
}

// Test that ECH GREASE can be configured.
TEST_P(TlsClientHandshakerTest, ECHGrease) {
  ssl_config_.emplace();
  ssl_config_->ech_grease_enabled = true;
  CreateConnection();

  // Add a DoS callback on the server, to test that the client sent a GREASE
  // message. This is a bit of a hack. TlsServerHandshaker already configures
  // the certificate selection callback, but does not usefully expose any way
  // for tests to inspect the ClientHello. So, instead, we register a different
  // callback that also gets the ClientHello.
  static bool callback_ran;
  callback_ran = false;
  SSL_CTX_set_dos_protection_cb(
      server_crypto_config_->ssl_ctx(),
      [](const SSL_CLIENT_HELLO* client_hello) -> int {
        const uint8_t* data;
        size_t len;
        EXPECT_TRUE(SSL_early_callback_ctx_extension_get(
            client_hello, TLSEXT_TYPE_encrypted_client_hello, &data, &len));
        callback_ran = true;
        return 1;
      });

  CompleteCryptoHandshake();
  EXPECT_TRUE(callback_ran);

  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  // Sending an ignored ECH GREASE extension does not count as negotiating ECH.
  EXPECT_FALSE(stream()->crypto_negotiated_params().encrypted_client_hello);
}

TEST_P(TlsClientHandshakerTest, EnableMLKEM) {
  crypto_config_->set_preferred_groups({SSL_GROUP_X25519_MLKEM768});
  server_crypto_config_->set_preferred_groups(
      {SSL_GROUP_X25519_MLKEM768, SSL_GROUP_X25519, SSL_GROUP_SECP256R1,
       SSL_GROUP_SECP384R1});
  CreateConnection();

  CompleteCryptoHandshake();
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  EXPECT_EQ(SSL_GROUP_X25519_MLKEM768, SSL_get_group_id(stream()->GetSsl()));
}

TEST_P(TlsClientHandshakerTest, EnableClientAlpsUseNewCodepoint) {
  // The intent of this test is to demonstrate the handshake should complete
  // successfully.
  SCOPED_TRACE("Test allows alps new codepoint.");
  crypto_config_->set_alps_use_new_codepoint(true);
  CreateConnection();

  // Add a DoS callback on the server, to test that the client sent the new
  // ALPS codepoint.
  static bool callback_ran;
  callback_ran = false;
  SSL_CTX_set_dos_protection_cb(
      server_crypto_config_->ssl_ctx(),
      [](const SSL_CLIENT_HELLO* client_hello) -> int {
        const uint8_t* data;
        size_t len;
        EXPECT_TRUE(SSL_early_callback_ctx_extension_get(
            client_hello, TLSEXT_TYPE_application_settings, &data, &len));
        callback_ran = true;
        return 1;
      });

  CompleteCryptoHandshake();
  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(callback_ran);
}

}  // namespace
}  // namespace test
}  // namespace quic
