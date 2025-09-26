// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/tls_server_handshaker.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "openssl/ssl.h"
#include "quiche/quic/core/crypto/certificate_util.h"
#include "quiche/quic/core/crypto/certificate_view.h"
#include "quiche/quic/core/crypto/client_proof_source.h"
#include "quiche/quic/core/crypto/proof_source.h"
#include "quiche/quic/core/crypto/proof_verifier.h"
#include "quiche/quic/core/crypto/quic_compressed_certs_cache.h"
#include "quiche/quic/core/crypto/quic_crypto_client_config.h"
#include "quiche/quic/core/crypto/quic_crypto_server_config.h"
#include "quiche/quic/core/crypto/quic_random.h"
#include "quiche/quic/core/crypto/transport_parameters.h"
#include "quiche/quic/core/quic_crypto_client_stream.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/quic/core/quic_session.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/core/tls_handshaker.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/crypto_test_utils.h"
#include "quiche/quic/test_tools/failing_proof_source.h"
#include "quiche/quic/test_tools/fake_proof_source.h"
#include "quiche/quic/test_tools/fake_proof_source_handle.h"
#include "quiche/quic/test_tools/quic_config_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/quic/test_tools/simple_session_cache.h"
#include "quiche/quic/test_tools/test_ticket_crypter.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/platform/api/quiche_reference_counted.h"

namespace quic {
class QuicConnection;
class QuicStream;
}  // namespace quic

using testing::_;
using testing::HasSubstr;
using testing::NiceMock;
using testing::Return;

namespace quic {
namespace test {

namespace {

const char kServerHostname[] = "test.example.com";
const uint16_t kServerPort = 443;

struct TestParams {
  ParsedQuicVersion version;
  bool disable_resumption;
};

ABSL_ATTRIBUTE_UNUSED  // Used by ::testing::PrintToStringParamName().
    std::string PrintToString(const TestParams& p) {
  return absl::StrCat(
      ParsedQuicVersionToString(p.version), "_",
      (p.disable_resumption ? "ResumptionDisabled" : "ResumptionEnabled"));
}

// Constructs test permutations.
std::vector<TestParams> GetTestParams() {
  std::vector<TestParams> params;
  for (const auto& version : AllSupportedVersionsWithTls()) {
    for (bool disable_resumption : {false, true}) {
      params.push_back(TestParams{version, disable_resumption});
    }
  }
  return params;
}

class MockProofVerifier : public ProofVerifier {
 public:
  MOCK_METHOD(QuicAsyncStatus, VerifyProof,
              (const std::string& hostname, uint16_t port,
               const std::string& server_config,
               QuicTransportVersion transport_version,
               absl::string_view chlo_hash,
               const std::vector<std::string>& certs,
               const std::string& cert_sct, const std::string& signature,
               const ProofVerifyContext* context, std::string* error_details,
               std::unique_ptr<ProofVerifyDetails>* details,
               std::unique_ptr<ProofVerifierCallback> callback),
              (override));

  MOCK_METHOD(QuicAsyncStatus, VerifyCertChain,
              (const std::string& hostname, uint16_t port,
               const std::vector<std::string>& certs,
               const std::string& ocsp_response, const std::string& cert_sct,
               const ProofVerifyContext* context, std::string* error_details,
               std::unique_ptr<ProofVerifyDetails>* details, uint8_t* out_alert,
               std::unique_ptr<ProofVerifierCallback> callback),
              (override));

  MOCK_METHOD(std::unique_ptr<ProofVerifyContext>, CreateDefaultContext, (),
              (override));
};

class TestTlsServerHandshaker : public TlsServerHandshaker {
 public:
  static constexpr TransportParameters::TransportParameterId
      kFailHandshakeParam{0xFFEACA};

  TestTlsServerHandshaker(QuicSession* session,
                          const QuicCryptoServerConfig* crypto_config)
      : TlsServerHandshaker(session, crypto_config),
        proof_source_(crypto_config->proof_source()) {
    ON_CALL(*this, MaybeCreateProofSourceHandle())
        .WillByDefault(testing::Invoke(
            this, &TestTlsServerHandshaker::RealMaybeCreateProofSourceHandle));

    ON_CALL(*this, OverrideQuicConfigDefaults(_))
        .WillByDefault(testing::Invoke(
            this, &TestTlsServerHandshaker::RealOverrideQuicConfigDefaults));
  }

  MOCK_METHOD(std::unique_ptr<ProofSourceHandle>, MaybeCreateProofSourceHandle,
              (), (override));

  MOCK_METHOD(void, OverrideQuicConfigDefaults, (QuicConfig * config),
              (override));

  void SetupProofSourceHandle(
      FakeProofSourceHandle::Action select_cert_action,
      FakeProofSourceHandle::Action compute_signature_action,
      QuicDelayedSSLConfig dealyed_ssl_config = QuicDelayedSSLConfig()) {
    EXPECT_CALL(*this, MaybeCreateProofSourceHandle())
        .WillOnce(
            [this, select_cert_action, compute_signature_action,
             dealyed_ssl_config]() {
              auto handle = std::make_unique<FakeProofSourceHandle>(
                  proof_source_, this, select_cert_action,
                  compute_signature_action, dealyed_ssl_config);
              fake_proof_source_handle_ = handle.get();
              return handle;
            });
  }

  FakeProofSourceHandle* fake_proof_source_handle() {
    return fake_proof_source_handle_;
  }

  bool received_client_cert() const { return received_client_cert_; }
  QuicAsyncStatus verify_cert_chain_status() const {
    return verify_cert_chain_status_;
  }

  using TlsServerHandshaker::AdvanceHandshake;
  using TlsServerHandshaker::expected_ssl_error;

 protected:
  QuicAsyncStatus VerifyCertChain(
      const std::vector<std::string>& certs, std::string* error_details,
      std::unique_ptr<ProofVerifyDetails>* details, uint8_t* out_alert,
      std::unique_ptr<ProofVerifierCallback> callback) override {
    received_client_cert_ = true;
    verify_cert_chain_status_ = TlsServerHandshaker::VerifyCertChain(
        certs, error_details, details, out_alert, std::move(callback));
    return verify_cert_chain_status_;
  }

  bool ProcessAdditionalTransportParameters(
      const TransportParameters& params) override {
    return !params.custom_parameters.contains(kFailHandshakeParam);
  }

 private:
  std::unique_ptr<ProofSourceHandle> RealMaybeCreateProofSourceHandle() {
    return TlsServerHandshaker::MaybeCreateProofSourceHandle();
  }

  void RealOverrideQuicConfigDefaults(QuicConfig* config) {
    return TlsServerHandshaker::OverrideQuicConfigDefaults(config);
  }

  // Owned by TlsServerHandshaker.
  FakeProofSourceHandle* fake_proof_source_handle_ = nullptr;
  ProofSource* proof_source_ = nullptr;
  QuicAsyncStatus verify_cert_chain_status_;
  bool received_client_cert_ = false;
};

class TlsServerHandshakerTestSession : public TestQuicSpdyServerSession {
 public:
  using TestQuicSpdyServerSession::TestQuicSpdyServerSession;

  std::unique_ptr<QuicCryptoServerStreamBase> CreateQuicCryptoServerStream(
      const QuicCryptoServerConfig* crypto_config,
      QuicCompressedCertsCache* /*compressed_certs_cache*/) override {
    if (connection()->version().handshake_protocol == PROTOCOL_TLS1_3) {
      return std::make_unique<NiceMock<TestTlsServerHandshaker>>(this,
                                                                 crypto_config);
    }

    QUICHE_CHECK(false) << "Unsupported handshake protocol: "
                        << connection()->version().handshake_protocol;
    return nullptr;
  }
};

class TlsServerHandshakerTest : public QuicTestWithParam<TestParams> {
 public:
  TlsServerHandshakerTest()
      : server_compressed_certs_cache_(
            QuicCompressedCertsCache::kQuicCompressedCertsCacheSize),
        server_id_(kServerHostname, kServerPort),
        supported_versions_({GetParam().version}) {
    SetQuicFlag(quic_disable_server_tls_resumption,
                GetParam().disable_resumption);
    client_crypto_config_ = std::make_unique<QuicCryptoClientConfig>(
        crypto_test_utils::ProofVerifierForTesting(),
        std::make_unique<test::SimpleSessionCache>());
    InitializeServerConfig();
    InitializeServer();
    InitializeFakeClient();
  }

  ~TlsServerHandshakerTest() override {
    // Ensure that anything that might reference |helpers_| is destroyed before
    // |helpers_| is destroyed.
    server_session_.reset();
    client_session_.reset();
    helpers_.clear();
    alarm_factories_.clear();
  }

  std::unique_ptr<FakeProofSource> InitializeProofSource() {
    auto ticket_crypter = std::make_unique<TestTicketCrypter>();
    ticket_crypter_ = ticket_crypter.get();
    auto proof_source = std::make_unique<FakeProofSource>();
    proof_source_ = proof_source.get();
    proof_source_->SetTicketCrypter(std::move(ticket_crypter));
    return proof_source;
  }

  void InitializeServerConfig() {
    server_crypto_config_ = std::make_unique<QuicCryptoServerConfig>(
        QuicCryptoServerConfig::TESTING, QuicRandom::GetInstance(),
        InitializeProofSource(), KeyExchangeSource::Default());
  }

  void InitializeServerConfigWithFailingProofSource() {
    server_crypto_config_ = std::make_unique<QuicCryptoServerConfig>(
        QuicCryptoServerConfig::TESTING, QuicRandom::GetInstance(),
        std::make_unique<FailingProofSource>(), KeyExchangeSource::Default());
  }

  void InitializeServerConfigWithProofSourceAndVerifier(
      std::unique_ptr<ProofSource> proof_source,
      std::unique_ptr<ProofVerifier> proof_verifier) {
    server_crypto_config_ = std::make_unique<QuicCryptoServerConfig>(
        QuicCryptoServerConfig::TESTING, QuicRandom::GetInstance(),
        std::move(proof_source), KeyExchangeSource::Default(),
        std::move(proof_verifier));
  }

  void CreateTlsServerHandshakerTestSession(MockQuicConnectionHelper* helper,
                                            MockAlarmFactory* alarm_factory) {
    server_connection_ = new PacketSavingConnection(
        helper, alarm_factory, Perspective::IS_SERVER,
        ParsedVersionOfIndex(supported_versions_, 0));

    TlsServerHandshakerTestSession* server_session =
        new TlsServerHandshakerTestSession(
            server_connection_, DefaultQuicConfig(), supported_versions_,
            server_crypto_config_.get(), &server_compressed_certs_cache_);
    server_session->set_client_cert_mode(initial_client_cert_mode_);
    server_session->Initialize();

    // We advance the clock initially because the default time is zero and the
    // strike register worries that we've just overflowed a uint32_t time.
    server_connection_->AdvanceTime(QuicTime::Delta::FromSeconds(100000));

    QUICHE_CHECK(server_session);
    server_session_.reset(server_session);
  }

  void InitializeServerWithFakeProofSourceHandle() {
    helpers_.push_back(std::make_unique<NiceMock<MockQuicConnectionHelper>>());
    alarm_factories_.push_back(std::make_unique<MockAlarmFactory>());
    CreateTlsServerHandshakerTestSession(helpers_.back().get(),
                                         alarm_factories_.back().get());
    server_handshaker_ = static_cast<NiceMock<TestTlsServerHandshaker>*>(
        server_session_->GetMutableCryptoStream());
    EXPECT_CALL(*server_session_->helper(), CanAcceptClientHello(_, _, _, _, _))
        .Times(testing::AnyNumber());
    EXPECT_CALL(*server_session_, SelectAlpn(_))
        .WillRepeatedly([this](const std::vector<absl::string_view>& alpns) {
          return std::find(
              alpns.cbegin(), alpns.cend(),
              AlpnForVersion(server_session_->connection()->version()));
        });
    crypto_test_utils::SetupCryptoServerConfigForTest(
        server_connection_->clock(), server_connection_->random_generator(),
        server_crypto_config_.get());
  }

  // Initializes the crypto server stream state for testing.  May be
  // called multiple times.
  void InitializeServer() {
    TestQuicSpdyServerSession* server_session = nullptr;
    helpers_.push_back(std::make_unique<NiceMock<MockQuicConnectionHelper>>());
    alarm_factories_.push_back(std::make_unique<MockAlarmFactory>());
    CreateServerSessionForTest(
        server_id_, QuicTime::Delta::FromSeconds(100000), supported_versions_,
        helpers_.back().get(), alarm_factories_.back().get(),
        server_crypto_config_.get(), &server_compressed_certs_cache_,
        &server_connection_, &server_session);
    QUICHE_CHECK(server_session);
    server_session_.reset(server_session);
    server_handshaker_ = nullptr;
    EXPECT_CALL(*server_session_->helper(), CanAcceptClientHello(_, _, _, _, _))
        .Times(testing::AnyNumber());
    EXPECT_CALL(*server_session_, SelectAlpn(_))
        .WillRepeatedly([this](const std::vector<absl::string_view>& alpns) {
          return std::find(
              alpns.cbegin(), alpns.cend(),
              AlpnForVersion(server_session_->connection()->version()));
        });
    crypto_test_utils::SetupCryptoServerConfigForTest(
        server_connection_->clock(), server_connection_->random_generator(),
        server_crypto_config_.get());
  }

  QuicCryptoServerStreamBase* server_stream() {
    return server_session_->GetMutableCryptoStream();
  }

  QuicCryptoClientStream* client_stream() {
    return client_session_->GetMutableCryptoStream();
  }

  // Initializes a fake client, and all its associated state, for
  // testing.  May be called multiple times.
  void InitializeFakeClient() {
    TestQuicSpdyClientSession* client_session = nullptr;
    helpers_.push_back(std::make_unique<NiceMock<MockQuicConnectionHelper>>());
    alarm_factories_.push_back(std::make_unique<MockAlarmFactory>());
    CreateClientSessionForTest(
        server_id_, QuicTime::Delta::FromSeconds(100000), supported_versions_,
        helpers_.back().get(), alarm_factories_.back().get(),
        client_crypto_config_.get(), &client_connection_, &client_session);
    const std::string default_alpn =
        AlpnForVersion(client_connection_->version());
    ON_CALL(*client_session, GetAlpnsToOffer())
        .WillByDefault(Return(std::vector<std::string>({default_alpn})));
    QUICHE_CHECK(client_session);
    client_session_.reset(client_session);
    moved_messages_counts_ = {0, 0};
  }

  void CompleteCryptoHandshake() {
    while (!client_stream()->one_rtt_keys_available() ||
           !server_stream()->one_rtt_keys_available()) {
      auto previous_moved_messages_counts = moved_messages_counts_;
      AdvanceHandshakeWithFakeClient();
      // Check that the handshake has made forward progress
      ASSERT_NE(previous_moved_messages_counts, moved_messages_counts_);
    }
  }

  // Performs a single round of handshake message-exchange between the
  // client and server.
  void AdvanceHandshakeWithFakeClient() {
    QUICHE_CHECK(server_connection_);
    QUICHE_CHECK(client_session_ != nullptr);

    EXPECT_CALL(*client_session_, OnProofValid(_)).Times(testing::AnyNumber());
    EXPECT_CALL(*client_session_, OnProofVerifyDetailsAvailable(_))
        .Times(testing::AnyNumber());
    EXPECT_CALL(*client_connection_, OnCanWrite()).Times(testing::AnyNumber());
    EXPECT_CALL(*server_connection_, OnCanWrite()).Times(testing::AnyNumber());
    // Call CryptoConnect if we haven't moved any client messages yet.
    if (moved_messages_counts_.first == 0) {
      client_stream()->CryptoConnect();
    }
    moved_messages_counts_ = crypto_test_utils::AdvanceHandshake(
        client_connection_, client_stream(), moved_messages_counts_.first,
        server_connection_, server_stream(), moved_messages_counts_.second);
  }

  void ExpectHandshakeSuccessful() {
    EXPECT_TRUE(client_stream()->one_rtt_keys_available());
    EXPECT_TRUE(client_stream()->encryption_established());
    EXPECT_TRUE(server_stream()->one_rtt_keys_available());
    EXPECT_TRUE(server_stream()->encryption_established());
    EXPECT_EQ(HANDSHAKE_COMPLETE, client_stream()->GetHandshakeState());
    EXPECT_EQ(HANDSHAKE_CONFIRMED, server_stream()->GetHandshakeState());

    const auto& client_crypto_params =
        client_stream()->crypto_negotiated_params();
    const auto& server_crypto_params =
        server_stream()->crypto_negotiated_params();
    // The TLS params should be filled in on the client.
    EXPECT_NE(0, client_crypto_params.cipher_suite);
    EXPECT_NE(0, client_crypto_params.key_exchange_group);
    EXPECT_NE(0, client_crypto_params.peer_signature_algorithm);

    // The cipher suite and key exchange group should match on the client and
    // server.
    EXPECT_EQ(client_crypto_params.cipher_suite,
              server_crypto_params.cipher_suite);
    EXPECT_EQ(client_crypto_params.key_exchange_group,
              server_crypto_params.key_exchange_group);
    // We don't support client certs on the server (yet), so the server
    // shouldn't have a peer signature algorithm to report.
    EXPECT_EQ(0, server_crypto_params.peer_signature_algorithm);
  }

  // Should only be called when using FakeProofSourceHandle.
  FakeProofSourceHandle::SelectCertArgs last_select_cert_args() const {
    QUICHE_CHECK(server_handshaker_ &&
                 server_handshaker_->fake_proof_source_handle());
    QUICHE_CHECK(!server_handshaker_->fake_proof_source_handle()
                      ->all_select_cert_args()
                      .empty());
    return server_handshaker_->fake_proof_source_handle()
        ->all_select_cert_args()
        .back();
  }

  // Should only be called when using FakeProofSourceHandle.
  FakeProofSourceHandle::ComputeSignatureArgs last_compute_signature_args()
      const {
    QUICHE_CHECK(server_handshaker_ &&
                 server_handshaker_->fake_proof_source_handle());
    QUICHE_CHECK(!server_handshaker_->fake_proof_source_handle()
                      ->all_compute_signature_args()
                      .empty());
    return server_handshaker_->fake_proof_source_handle()
        ->all_compute_signature_args()
        .back();
  }

 protected:
  // Setup the client to send a (self-signed) client cert to the server, if
  // requested. InitializeFakeClient() must be called after this to take effect.
  bool SetupClientCert() {
    auto client_proof_source = std::make_unique<DefaultClientProofSource>();

    CertificatePrivateKey client_cert_key(
        MakeKeyPairForSelfSignedCertificate());

    CertificateOptions options;
    options.subject = "CN=subject";
    options.serial_number = 0x12345678;
    options.validity_start = {2020, 1, 1, 0, 0, 0};
    options.validity_end = {2049, 12, 31, 0, 0, 0};
    std::string der_cert =
        CreateSelfSignedCertificate(*client_cert_key.private_key(), options);

    quiche::QuicheReferenceCountedPointer<ClientProofSource::Chain>
        client_cert_chain(new ClientProofSource::Chain({der_cert}));

    if (!client_proof_source->AddCertAndKey({"*"}, client_cert_chain,
                                            std::move(client_cert_key))) {
      return false;
    }

    client_crypto_config_->set_proof_source(std::move(client_proof_source));
    return true;
  }

  // Every connection gets its own MockQuicConnectionHelper and
  // MockAlarmFactory, tracked separately from the server and client state so
  // their lifetimes persist through the whole test.
  std::vector<std::unique_ptr<MockQuicConnectionHelper>> helpers_;
  std::vector<std::unique_ptr<MockAlarmFactory>> alarm_factories_;

  // Server state.
  PacketSavingConnection* server_connection_;
  std::unique_ptr<TestQuicSpdyServerSession> server_session_;
  // Only set when initialized with InitializeServerWithFakeProofSourceHandle.
  NiceMock<TestTlsServerHandshaker>* server_handshaker_ = nullptr;
  TestTicketCrypter* ticket_crypter_;  // owned by proof_source_
  FakeProofSource* proof_source_;      // owned by server_crypto_config_
  std::unique_ptr<QuicCryptoServerConfig> server_crypto_config_;
  QuicCompressedCertsCache server_compressed_certs_cache_;
  QuicServerId server_id_;
  ClientCertMode initial_client_cert_mode_ = ClientCertMode::kNone;

  // Client state.
  PacketSavingConnection* client_connection_;
  std::unique_ptr<QuicCryptoClientConfig> client_crypto_config_;
  std::unique_ptr<TestQuicSpdyClientSession> client_session_;

  crypto_test_utils::FakeClientOptions client_options_;
  // How many handshake messages have been moved from client to server and
  // server to client.
  std::pair<size_t, size_t> moved_messages_counts_ = {0, 0};

  // Which QUIC versions the client and server support.
  ParsedQuicVersionVector supported_versions_;
};

INSTANTIATE_TEST_SUITE_P(TlsServerHandshakerTests, TlsServerHandshakerTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

TEST_P(TlsServerHandshakerTest, NotInitiallyConected) {
  EXPECT_FALSE(server_stream()->encryption_established());
  EXPECT_FALSE(server_stream()->one_rtt_keys_available());
}

TEST_P(TlsServerHandshakerTest, ConnectedAfterTlsHandshake) {
  CompleteCryptoHandshake();
  EXPECT_EQ(PROTOCOL_TLS1_3, server_stream()->handshake_protocol());
  ExpectHandshakeSuccessful();
}

TEST_P(TlsServerHandshakerTest, HandshakeWithAsyncSelectCertSuccess) {
  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_ASYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::
          DELEGATE_SYNC);

  EXPECT_CALL(*client_connection_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*server_connection_, CloseConnection(_, _, _)).Times(0);

  // Start handshake.
  AdvanceHandshakeWithFakeClient();

  ASSERT_TRUE(
      server_handshaker_->fake_proof_source_handle()->HasPendingOperation());
  server_handshaker_->fake_proof_source_handle()->CompletePendingOperation();

  CompleteCryptoHandshake();

  ExpectHandshakeSuccessful();
}

TEST_P(TlsServerHandshakerTest, HandshakeWithAsyncSelectCertFailure) {
  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::FAIL_ASYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::
          DELEGATE_SYNC);

  // Start handshake.
  AdvanceHandshakeWithFakeClient();

  ASSERT_TRUE(
      server_handshaker_->fake_proof_source_handle()->HasPendingOperation());
  server_handshaker_->fake_proof_source_handle()->CompletePendingOperation();

  // Check that the server didn't send any handshake messages, because it failed
  // to handshake.
  EXPECT_EQ(moved_messages_counts_.second, 0u);
  EXPECT_EQ(server_handshaker_->extra_error_details(),
            "select_cert_error: proof_source_handle async failure");
}

TEST_P(TlsServerHandshakerTest, HandshakeWithAsyncSelectCertAndSignature) {
  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_ASYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::
          DELEGATE_ASYNC);

  EXPECT_CALL(*client_connection_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*server_connection_, CloseConnection(_, _, _)).Times(0);

  // Start handshake.
  AdvanceHandshakeWithFakeClient();

  // A select cert operation is now pending.
  ASSERT_TRUE(
      server_handshaker_->fake_proof_source_handle()->HasPendingOperation());
  EXPECT_EQ(server_handshaker_->expected_ssl_error(),
            SSL_ERROR_PENDING_CERTIFICATE);

  // Complete the pending select cert. It should advance the handshake to
  // compute a signature, which will also be saved as a pending operation.
  server_handshaker_->fake_proof_source_handle()->CompletePendingOperation();

  // A compute signature operation is now pending.
  ASSERT_TRUE(
      server_handshaker_->fake_proof_source_handle()->HasPendingOperation());
  EXPECT_EQ(server_handshaker_->expected_ssl_error(),
            SSL_ERROR_WANT_PRIVATE_KEY_OPERATION);

  server_handshaker_->fake_proof_source_handle()->CompletePendingOperation();

  CompleteCryptoHandshake();

  ExpectHandshakeSuccessful();
}

TEST_P(TlsServerHandshakerTest, HandshakeWithAsyncSignature) {
  EXPECT_CALL(*client_connection_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*server_connection_, CloseConnection(_, _, _)).Times(0);
  // Enable FakeProofSource to capture call to ComputeTlsSignature and run it
  // asynchronously.
  proof_source_->Activate();

  // Start handshake.
  AdvanceHandshakeWithFakeClient();

  ASSERT_EQ(proof_source_->NumPendingCallbacks(), 1);
  proof_source_->InvokePendingCallback(0);

  CompleteCryptoHandshake();

  ExpectHandshakeSuccessful();
}

TEST_P(TlsServerHandshakerTest, CancelPendingSelectCert) {
  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_ASYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::
          DELEGATE_SYNC);

  EXPECT_CALL(*client_connection_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*server_connection_, CloseConnection(_, _, _)).Times(0);

  // Start handshake.
  AdvanceHandshakeWithFakeClient();

  ASSERT_TRUE(
      server_handshaker_->fake_proof_source_handle()->HasPendingOperation());
  server_handshaker_->CancelOutstandingCallbacks();
  ASSERT_FALSE(
      server_handshaker_->fake_proof_source_handle()->HasPendingOperation());
  // CompletePendingOperation should be noop.
  server_handshaker_->fake_proof_source_handle()->CompletePendingOperation();
}

TEST_P(TlsServerHandshakerTest, CancelPendingSignature) {
  EXPECT_CALL(*client_connection_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*server_connection_, CloseConnection(_, _, _)).Times(0);
  // Enable FakeProofSource to capture call to ComputeTlsSignature and run it
  // asynchronously.
  proof_source_->Activate();

  // Start handshake.
  AdvanceHandshakeWithFakeClient();

  ASSERT_EQ(proof_source_->NumPendingCallbacks(), 1);
  server_session_ = nullptr;

  proof_source_->InvokePendingCallback(0);
}

TEST_P(TlsServerHandshakerTest, ExtractSNI) {
  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();

  EXPECT_EQ(server_stream()->crypto_negotiated_params().sni,
            "test.example.com");
}

TEST_P(TlsServerHandshakerTest, ServerConnectionIdPassedToSelectCert) {
  InitializeServerWithFakeProofSourceHandle();

  // Disable early data.
  server_session_->set_early_data_enabled(false);

  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_SYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::
          DELEGATE_SYNC);
  InitializeFakeClient();
  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();

  EXPECT_EQ(last_select_cert_args().original_connection_id, TestConnectionId());
}

TEST_P(TlsServerHandshakerTest, HostnameForCertSelectionAndComputeSignature) {
  // Client uses upper case letters in hostname. It is considered valid by
  // QuicHostnameUtils::IsValidSNI, but it should be normalized for cert
  // selection.
  server_id_ = QuicServerId("tEsT.EXAMPLE.CoM", kServerPort);
  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_SYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::
          DELEGATE_SYNC);
  InitializeFakeClient();
  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();

  EXPECT_EQ(server_stream()->crypto_negotiated_params().sni,
            "test.example.com");

  EXPECT_EQ(last_select_cert_args().hostname, "test.example.com");
  EXPECT_EQ(last_compute_signature_args().hostname, "test.example.com");
}

TEST_P(TlsServerHandshakerTest, SSLConfigForCertSelection) {
  InitializeServerWithFakeProofSourceHandle();

  // Disable early data.
  server_session_->set_early_data_enabled(false);

  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_SYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::
          DELEGATE_SYNC);
  InitializeFakeClient();
  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();

  EXPECT_FALSE(last_select_cert_args().ssl_config.early_data_enabled);
}

TEST_P(TlsServerHandshakerTest, ConnectionClosedOnTlsError) {
  EXPECT_CALL(*server_connection_,
              CloseConnection(QUIC_HANDSHAKE_FAILED, _, _, _));

  // Send a zero-length ClientHello from client to server.
  char bogus_handshake_message[] = {
      // Handshake struct (RFC 8446 appendix B.3)
      1,        // HandshakeType client_hello
      0, 0, 0,  // uint24 length
  };

  // Install a packet flusher such that the packets generated by
  // |server_connection_| in response to this handshake message are more likely
  // to be coalesced and/or batched in the writer.
  //
  // This is required by TlsServerHandshaker because without the flusher, it
  // tends to generate many small, uncoalesced packets, one per
  // TlsHandshaker::WriteMessage.
  QuicConnection::ScopedPacketFlusher flusher(server_connection_);
  server_stream()->crypto_message_parser()->ProcessInput(
      absl::string_view(bogus_handshake_message,
                        ABSL_ARRAYSIZE(bogus_handshake_message)),
      ENCRYPTION_INITIAL);

  EXPECT_FALSE(server_stream()->one_rtt_keys_available());
}

TEST_P(TlsServerHandshakerTest, ClientSendingBadALPN) {
  const std::string kTestBadClientAlpn = "bad-client-alpn";
  EXPECT_CALL(*client_session_, GetAlpnsToOffer())
      .WillOnce(Return(std::vector<std::string>({kTestBadClientAlpn})));

  EXPECT_CALL(
      *server_connection_,
      CloseConnection(
          QUIC_HANDSHAKE_FAILED,
          static_cast<QuicIetfTransportErrorCodes>(CRYPTO_ERROR_FIRST + 120),
          HasSubstr("TLS handshake failure (ENCRYPTION_INITIAL) 120: "
                    "no application protocol"),
          _));

  AdvanceHandshakeWithFakeClient();

  EXPECT_FALSE(client_stream()->one_rtt_keys_available());
  EXPECT_FALSE(client_stream()->encryption_established());
  EXPECT_FALSE(server_stream()->one_rtt_keys_available());
  EXPECT_FALSE(server_stream()->encryption_established());
}

TEST_P(TlsServerHandshakerTest, CustomALPNNegotiation) {
  EXPECT_CALL(*client_connection_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*server_connection_, CloseConnection(_, _, _)).Times(0);

  const std::string kTestAlpn = "A Custom ALPN Value";
  const std::vector<std::string> kTestAlpns(
      {"foo", "bar", kTestAlpn, "something else"});
  EXPECT_CALL(*client_session_, GetAlpnsToOffer())
      .WillRepeatedly(Return(kTestAlpns));
  EXPECT_CALL(*server_session_, SelectAlpn(_))
      .WillOnce(
          [kTestAlpn, kTestAlpns](const std::vector<absl::string_view>& alpns) {
            EXPECT_THAT(alpns, testing::ElementsAreArray(kTestAlpns));
            return std::find(alpns.cbegin(), alpns.cend(), kTestAlpn);
          });
  EXPECT_CALL(*client_session_, OnAlpnSelected(absl::string_view(kTestAlpn)));
  EXPECT_CALL(*server_session_, OnAlpnSelected(absl::string_view(kTestAlpn)));

  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
}

TEST_P(TlsServerHandshakerTest, RejectInvalidSNI) {
  SetQuicFlag(quic_client_allow_invalid_sni_for_test, true);
  server_id_ = QuicServerId("invalid!.example.com", kServerPort);
  InitializeFakeClient();

  // Run the handshake and expect it to fail.
  AdvanceHandshakeWithFakeClient();
  EXPECT_FALSE(server_stream()->encryption_established());
  EXPECT_FALSE(server_stream()->one_rtt_keys_available());
}

TEST_P(TlsServerHandshakerTest, Resumption) {
  // Do the first handshake
  InitializeFakeClient();
  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_FALSE(client_stream()->IsResumption());
  EXPECT_FALSE(server_stream()->IsResumption());
  EXPECT_FALSE(server_stream()->ResumptionAttempted());

  // Now do another handshake
  InitializeServer();
  InitializeFakeClient();
  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_NE(client_stream()->IsResumption(), GetParam().disable_resumption);
  EXPECT_NE(server_stream()->IsResumption(), GetParam().disable_resumption);
  EXPECT_NE(server_stream()->ResumptionAttempted(),
            GetParam().disable_resumption);
}

TEST_P(TlsServerHandshakerTest, ResumptionWithAsyncDecryptCallback) {
  // Do the first handshake
  InitializeFakeClient();
  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();

  ticket_crypter_->SetRunCallbacksAsync(true);
  // Now do another handshake
  InitializeServer();
  InitializeFakeClient();

  AdvanceHandshakeWithFakeClient();
  if (GetParam().disable_resumption) {
    ASSERT_EQ(ticket_crypter_->NumPendingCallbacks(), 0u);
    return;
  }
  // Test that the DecryptCallback will be run asynchronously, and then run it.
  ASSERT_EQ(ticket_crypter_->NumPendingCallbacks(), 1u);
  ticket_crypter_->RunPendingCallback(0);

  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_TRUE(client_stream()->IsResumption());
  EXPECT_TRUE(server_stream()->IsResumption());
  EXPECT_TRUE(server_stream()->ResumptionAttempted());
}

TEST_P(TlsServerHandshakerTest, ResumptionWithPlaceholderTicket) {
  // Do the first handshake
  InitializeFakeClient();

  ticket_crypter_->set_fail_encrypt(true);
  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_FALSE(client_stream()->IsResumption());
  EXPECT_FALSE(server_stream()->IsResumption());
  EXPECT_FALSE(server_stream()->ResumptionAttempted());

  // Now do another handshake. It should end up with a full handshake. When the
  // placeholder ticket is enabled, it will be undecryptable. When it is
  // disabled, newer BoringSSL servers will skip sending a ticket altogether, so
  // the client will not even attempt resumption.
  InitializeServer();
  InitializeFakeClient();
  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_FALSE(client_stream()->IsResumption());
  EXPECT_FALSE(server_stream()->IsResumption());
}

TEST_P(TlsServerHandshakerTest, AdvanceHandshakeDuringAsyncDecryptCallback) {
  if (GetParam().disable_resumption) {
    return;
  }

  // Do the first handshake
  InitializeFakeClient();
  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();

  ticket_crypter_->SetRunCallbacksAsync(true);
  // Now do another handshake
  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_SYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::
          DELEGATE_SYNC);
  InitializeFakeClient();

  AdvanceHandshakeWithFakeClient();

  // Ensure an async DecryptCallback is now pending.
  ASSERT_EQ(ticket_crypter_->NumPendingCallbacks(), 1u);

  {
    QuicConnection::ScopedPacketFlusher flusher(server_connection_);
    server_handshaker_->AdvanceHandshake();
  }

  // This will delete |server_handshaker_|.
  server_session_ = nullptr;

  ticket_crypter_->RunPendingCallback(0);  // Should not crash.
}

TEST_P(TlsServerHandshakerTest, ResumptionWithFailingDecryptCallback) {
  if (GetParam().disable_resumption) {
    return;
  }

  // Do the first handshake
  InitializeFakeClient();
  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();

  ticket_crypter_->set_fail_decrypt(true);
  // Now do another handshake
  InitializeServer();
  InitializeFakeClient();
  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_FALSE(client_stream()->IsResumption());
  EXPECT_FALSE(server_stream()->IsResumption());
  EXPECT_TRUE(server_stream()->ResumptionAttempted());
}

TEST_P(TlsServerHandshakerTest, ResumptionWithFailingAsyncDecryptCallback) {
  if (GetParam().disable_resumption) {
    return;
  }

  // Do the first handshake
  InitializeFakeClient();
  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();

  ticket_crypter_->set_fail_decrypt(true);
  ticket_crypter_->SetRunCallbacksAsync(true);
  // Now do another handshake
  InitializeServer();
  InitializeFakeClient();

  AdvanceHandshakeWithFakeClient();
  // Test that the DecryptCallback will be run asynchronously, and then run it.
  ASSERT_EQ(ticket_crypter_->NumPendingCallbacks(), 1u);
  ticket_crypter_->RunPendingCallback(0);

  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_FALSE(client_stream()->IsResumption());
  EXPECT_FALSE(server_stream()->IsResumption());
  EXPECT_TRUE(server_stream()->ResumptionAttempted());
}

TEST_P(TlsServerHandshakerTest, HandshakeFailsWithFailingProofSource) {
  InitializeServerConfigWithFailingProofSource();
  InitializeServer();
  InitializeFakeClient();

  // Attempt handshake.
  AdvanceHandshakeWithFakeClient();
  // Check that the server didn't send any handshake messages, because it failed
  // to handshake.
  EXPECT_EQ(moved_messages_counts_.second, 0u);
}

TEST_P(TlsServerHandshakerTest, ZeroRttResumption) {
  std::vector<uint8_t> application_state = {0, 1, 2, 3};

  // Do the first handshake
  server_stream()->SetServerApplicationStateForResumption(
      std::make_unique<ApplicationState>(application_state));
  InitializeFakeClient();
  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_FALSE(client_stream()->IsResumption());
  EXPECT_FALSE(server_stream()->IsZeroRtt());

  // Now do another handshake
  InitializeServer();
  server_stream()->SetServerApplicationStateForResumption(
      std::make_unique<ApplicationState>(application_state));
  InitializeFakeClient();
  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_NE(client_stream()->IsResumption(), GetParam().disable_resumption);
  EXPECT_NE(server_stream()->IsZeroRtt(), GetParam().disable_resumption);
}

TEST_P(TlsServerHandshakerTest, ZeroRttRejectOnApplicationStateChange) {
  std::vector<uint8_t> original_application_state = {1, 2};
  std::vector<uint8_t> new_application_state = {3, 4};

  // Do the first handshake
  server_stream()->SetServerApplicationStateForResumption(
      std::make_unique<ApplicationState>(original_application_state));
  InitializeFakeClient();
  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_FALSE(client_stream()->IsResumption());
  EXPECT_FALSE(server_stream()->IsZeroRtt());

  // Do another handshake, but change the application state
  InitializeServer();
  server_stream()->SetServerApplicationStateForResumption(
      std::make_unique<ApplicationState>(new_application_state));
  InitializeFakeClient();
  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_NE(client_stream()->IsResumption(), GetParam().disable_resumption);
  EXPECT_FALSE(server_stream()->IsZeroRtt());
}

TEST_P(TlsServerHandshakerTest, RequestClientCert) {
  ASSERT_TRUE(SetupClientCert());
  InitializeFakeClient();

  initial_client_cert_mode_ = ClientCertMode::kRequest;
  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_SYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::
          DELEGATE_SYNC);

  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_TRUE(server_handshaker_->received_client_cert());
}

TEST_P(TlsServerHandshakerTest, RequestClientCertAndVerify) {
  ASSERT_TRUE(SetupClientCert());
  InitializeFakeClient();

  initial_client_cert_mode_ = ClientCertMode::kRequest;
  auto proof_source = InitializeProofSource();
  auto proof_verifier = std::make_unique<NiceMock<MockProofVerifier>>();
  EXPECT_CALL(*proof_verifier, VerifyCertChain).WillOnce(Return(QUIC_SUCCESS));
  InitializeServerConfigWithProofSourceAndVerifier(std::move(proof_source),
                                                   std::move(proof_verifier));
  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_SYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::
          DELEGATE_SYNC);

  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_TRUE(server_handshaker_->received_client_cert());
  EXPECT_EQ(QuicAsyncStatus::QUIC_SUCCESS,
            server_handshaker_->verify_cert_chain_status());
}

TEST_P(TlsServerHandshakerTest, RequestClientCertAndFailVerification) {
  ASSERT_TRUE(SetupClientCert());
  InitializeFakeClient();

  initial_client_cert_mode_ = ClientCertMode::kRequest;
  auto proof_source = InitializeProofSource();
  auto proof_verifier = std::make_unique<NiceMock<MockProofVerifier>>();
  EXPECT_CALL(*proof_verifier, VerifyCertChain).WillOnce(Return(QUIC_FAILURE));
  InitializeServerConfigWithProofSourceAndVerifier(std::move(proof_source),
                                                   std::move(proof_verifier));
  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_SYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::
          DELEGATE_SYNC);

  // Advance handshake until it can't make any more progress due to the
  // failing proof verifier.
  while (!client_stream()->one_rtt_keys_available() ||
         !server_stream()->one_rtt_keys_available()) {
    auto previous_moved_messages_counts = moved_messages_counts_;
    AdvanceHandshakeWithFakeClient();
    // Break if the handshake has stopped making progress.
    if (previous_moved_messages_counts == moved_messages_counts_) {
      break;
    }
  }

  // Check that handshake did not finish successfully from the server side.
  EXPECT_FALSE(server_stream()->one_rtt_keys_available());
  // Handshake is not complete and not confirmed.
  EXPECT_EQ(HANDSHAKE_PROCESSED, server_stream()->GetHandshakeState());
  EXPECT_EQ(QuicAsyncStatus::QUIC_FAILURE,
            server_handshaker_->verify_cert_chain_status());
}

TEST_P(TlsServerHandshakerTest,
       SetInvalidServerTransportParamsByDelayedSslConfig) {
  ASSERT_TRUE(SetupClientCert());
  InitializeFakeClient();

  QuicDelayedSSLConfig delayed_ssl_config;
  delayed_ssl_config.quic_transport_parameters = {1, 2, 3};
  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_ASYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::DELEGATE_SYNC,
      delayed_ssl_config);

  AdvanceHandshakeWithFakeClient();
  ASSERT_TRUE(
      server_handshaker_->fake_proof_source_handle()->HasPendingOperation());
  server_handshaker_->fake_proof_source_handle()->CompletePendingOperation();

  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_FALSE(server_handshaker_->fake_proof_source_handle()
                   ->all_compute_signature_args()
                   .empty());
}

TEST_P(TlsServerHandshakerTest,
       SetValidServerTransportParamsByDelayedSslConfig) {
  ParsedQuicVersion version = GetParam().version;

  TransportParameters server_params;
  std::string error_details;
  server_params.perspective = quic::Perspective::IS_SERVER;
  server_params.legacy_version_information =
      TransportParameters::LegacyVersionInformation();
  server_params.legacy_version_information.value().supported_versions =
      quic::CreateQuicVersionLabelVector(
          quic::ParsedQuicVersionVector{version});
  server_params.legacy_version_information.value().version =
      quic::CreateQuicVersionLabel(version);
  server_params.version_information = TransportParameters::VersionInformation();
  server_params.version_information.value().chosen_version =
      quic::CreateQuicVersionLabel(version);
  server_params.version_information.value().other_versions =
      quic::CreateQuicVersionLabelVector(
          quic::ParsedQuicVersionVector{version});

  ASSERT_TRUE(server_params.AreValid(&error_details)) << error_details;

  std::vector<uint8_t> server_params_bytes;
  ASSERT_TRUE(
      SerializeTransportParameters(server_params, &server_params_bytes));

  ASSERT_TRUE(SetupClientCert());
  InitializeFakeClient();

  QuicDelayedSSLConfig delayed_ssl_config;
  delayed_ssl_config.quic_transport_parameters = server_params_bytes;
  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_ASYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::DELEGATE_SYNC,
      delayed_ssl_config);

  AdvanceHandshakeWithFakeClient();
  ASSERT_TRUE(
      server_handshaker_->fake_proof_source_handle()->HasPendingOperation());
  server_handshaker_->fake_proof_source_handle()->CompletePendingOperation();

  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_FALSE(server_handshaker_->fake_proof_source_handle()
                   ->all_compute_signature_args()
                   .empty());
}

TEST_P(TlsServerHandshakerTest, RequestClientCertByDelayedSslConfig) {
  ASSERT_TRUE(SetupClientCert());
  InitializeFakeClient();

  QuicDelayedSSLConfig delayed_ssl_config;
  delayed_ssl_config.client_cert_mode = ClientCertMode::kRequest;
  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_ASYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::DELEGATE_SYNC,
      delayed_ssl_config);

  AdvanceHandshakeWithFakeClient();
  ASSERT_TRUE(
      server_handshaker_->fake_proof_source_handle()->HasPendingOperation());
  server_handshaker_->fake_proof_source_handle()->CompletePendingOperation();

  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_TRUE(server_handshaker_->received_client_cert());
}

TEST_P(TlsServerHandshakerTest, RequestClientCert_NoCert) {
  initial_client_cert_mode_ = ClientCertMode::kRequest;
  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_SYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::
          DELEGATE_SYNC);

  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_FALSE(server_handshaker_->received_client_cert());
}

TEST_P(TlsServerHandshakerTest, RequestAndRequireClientCert) {
  ASSERT_TRUE(SetupClientCert());
  InitializeFakeClient();

  initial_client_cert_mode_ = ClientCertMode::kRequire;
  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_SYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::
          DELEGATE_SYNC);

  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_TRUE(server_handshaker_->received_client_cert());
}

TEST_P(TlsServerHandshakerTest, RequestAndRequireClientCertByDelayedSslConfig) {
  ASSERT_TRUE(SetupClientCert());
  InitializeFakeClient();

  QuicDelayedSSLConfig delayed_ssl_config;
  delayed_ssl_config.client_cert_mode = ClientCertMode::kRequire;
  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_ASYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::DELEGATE_SYNC,
      delayed_ssl_config);

  AdvanceHandshakeWithFakeClient();
  ASSERT_TRUE(
      server_handshaker_->fake_proof_source_handle()->HasPendingOperation());
  server_handshaker_->fake_proof_source_handle()->CompletePendingOperation();

  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_TRUE(server_handshaker_->received_client_cert());
}

TEST_P(TlsServerHandshakerTest, RequestAndRequireClientCert_NoCert) {
  initial_client_cert_mode_ = ClientCertMode::kRequire;
  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_SYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::
          DELEGATE_SYNC);

  EXPECT_CALL(*server_connection_,
              CloseConnection(QUIC_TLS_CERTIFICATE_REQUIRED, _, _, _));

  AdvanceHandshakeWithFakeClient();
  AdvanceHandshakeWithFakeClient();
  EXPECT_FALSE(server_handshaker_->received_client_cert());
}

TEST_P(TlsServerHandshakerTest, CloseConnectionBeforeSelectCert) {
  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::
          FAIL_SYNC_DO_NOT_CHECK_CLOSED,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::
          FAIL_SYNC_DO_NOT_CHECK_CLOSED);

  EXPECT_CALL(*server_handshaker_, OverrideQuicConfigDefaults(_))
      .WillOnce([](QuicConfig* config) {
        QuicConfigPeer::SetReceivedMaxUnidirectionalStreams(config,
                                                            /*max_streams=*/0);
      });

  EXPECT_CALL(*server_connection_,
              CloseConnection(QUIC_ZERO_RTT_RESUMPTION_LIMIT_REDUCED, _, _))
      .WillOnce([this](QuicErrorCode error, const std::string& details,
                       ConnectionCloseBehavior connection_close_behavior) {
        server_connection_->ReallyCloseConnection(error, details,
                                                  connection_close_behavior);
        ASSERT_FALSE(server_connection_->connected());
      });

  AdvanceHandshakeWithFakeClient();

  EXPECT_TRUE(server_handshaker_->fake_proof_source_handle()
                  ->all_select_cert_args()
                  .empty());
}

TEST_P(TlsServerHandshakerTest, FailUponCustomTranportParam) {
  client_session_->config()->custom_transport_parameters_to_send().emplace(
      TestTlsServerHandshaker::kFailHandshakeParam,
      "Fail handshake upon seeing this.");

  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_ASYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::
          DELEGATE_SYNC);
  EXPECT_CALL(
      *server_connection_,
      CloseConnection(QUIC_HANDSHAKE_FAILED,
                      "Failed to process additional transport parameters", _));

  // Start handshake.
  AdvanceHandshakeWithFakeClient();
}

TEST_P(TlsServerHandshakerTest, SuccessWithCustomTranportParam) {
  client_session_->config()->custom_transport_parameters_to_send().emplace(
      TransportParameters::TransportParameterId{0xFFEADD},
      "Continue upon seeing this.");

  InitializeServerWithFakeProofSourceHandle();
  server_handshaker_->SetupProofSourceHandle(
      /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_ASYNC,
      /*compute_signature_action=*/FakeProofSourceHandle::Action::
          DELEGATE_SYNC);
  EXPECT_CALL(*server_connection_, CloseConnection(_, _, _)).Times(0);

  // Start handshake.
  AdvanceHandshakeWithFakeClient();
  ASSERT_TRUE(
      server_handshaker_->fake_proof_source_handle()->HasPendingOperation());
  server_handshaker_->fake_proof_source_handle()->CompletePendingOperation();

  CompleteCryptoHandshake();

  ExpectHandshakeSuccessful();
}

TEST_P(TlsServerHandshakerTest, EnableMLKEM) {
  server_crypto_config_->set_preferred_groups({SSL_GROUP_X25519_MLKEM768});
  client_crypto_config_->set_preferred_groups(
      {SSL_GROUP_X25519_MLKEM768, SSL_GROUP_X25519, SSL_GROUP_SECP256R1,
       SSL_GROUP_SECP384R1});

  InitializeServer();
  InitializeFakeClient();
  CompleteCryptoHandshake();
  ExpectHandshakeSuccessful();
  EXPECT_EQ(PROTOCOL_TLS1_3, server_stream()->handshake_protocol());
  EXPECT_EQ(SSL_GROUP_X25519_MLKEM768,
            SSL_get_group_id(server_stream()->GetSsl()));
}

TEST_P(TlsServerHandshakerTest, AlpsUseNewCodepoint) {
  const struct {
    bool client_use_alps_new_codepoint;
  } tests[] = {
      // The intent of this test is to demonstrate different combinations of
      // ALPS codepoint settings works well.
      {true},
      {false},
  };
  for (size_t i = 0; i < ABSL_ARRAYSIZE(tests); i++) {
    SCOPED_TRACE(absl::StrCat("Test #", i));
    const auto& test = tests[i];
    client_crypto_config_->set_alps_use_new_codepoint(
        test.client_use_alps_new_codepoint);

    ASSERT_TRUE(SetupClientCert());
    InitializeFakeClient();

    InitializeServerWithFakeProofSourceHandle();
    server_handshaker_->SetupProofSourceHandle(
        /*select_cert_action=*/FakeProofSourceHandle::Action::DELEGATE_SYNC,
        /*compute_signature_action=*/FakeProofSourceHandle::Action::
            DELEGATE_SYNC);

    // Start handshake.
    AdvanceHandshakeWithFakeClient();
    EXPECT_EQ(test.client_use_alps_new_codepoint,
              server_handshaker_->UseAlpsNewCodepoint());

    CompleteCryptoHandshake();
    ExpectHandshakeSuccessful();
    EXPECT_EQ(PROTOCOL_TLS1_3, server_stream()->handshake_protocol());
  }
}

}  // namespace
}  // namespace test
}  // namespace quic
