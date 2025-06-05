// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "quiche/quic/core/io/quic_default_event_loop.h"
#include "quiche/quic/core/io/quic_event_loop.h"
#include "quiche/quic/core/proto/crypto_server_config_proto.h"
#include "quiche/quic/core/quic_alarm_factory.h"
#include "quiche/quic/core/quic_default_clock.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/platform/api/quic_test_loopback.h"
#include "quiche/quic/qbone/platform/icmp_packet.h"
#include "quiche/quic/qbone/qbone_client_session.h"
#include "quiche/quic/qbone/qbone_constants.h"
#include "quiche/quic/qbone/qbone_control_placeholder.pb.h"
#include "quiche/quic/qbone/qbone_packet_processor_test_tools.h"
#include "quiche/quic/qbone/qbone_server_session.h"
#include "quiche/quic/test_tools/crypto_test_utils.h"
#include "quiche/quic/test_tools/mock_clock.h"
#include "quiche/quic/test_tools/mock_connection_id_generator.h"
#include "quiche/quic/test_tools/quic_connection_peer.h"
#include "quiche/quic/test_tools/quic_session_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/quiche_callbacks.h"

namespace quic {
namespace test {
namespace {

using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Not;

std::string TestPacketIn(const std::string& body) {
  return PrependIPv6HeaderForTest(body, 5);
}

std::string TestPacketOut(const std::string& body) {
  return PrependIPv6HeaderForTest(body, 4);
}

ParsedQuicVersionVector GetTestParams() {
  SetQuicReloadableFlag(quic_disable_version_q046, false);
  return CurrentSupportedVersionsWithQuicCrypto();
}

// Used by QuicCryptoServerConfig to provide server credentials, passes
// everything through to ProofSourceForTesting if success is true,
// and fails otherwise.
class IndirectionProofSource : public ProofSource {
 public:
  explicit IndirectionProofSource(bool success) {
    if (success) {
      proof_source_ = crypto_test_utils::ProofSourceForTesting();
    }
  }

  // ProofSource override.
  void GetProof(const QuicSocketAddress& server_address,
                const QuicSocketAddress& client_address,
                const std::string& hostname, const std::string& server_config,
                QuicTransportVersion transport_version,
                absl::string_view chlo_hash,
                std::unique_ptr<Callback> callback) override {
    if (!proof_source_) {
      QuicCryptoProof proof;
      quiche::QuicheReferenceCountedPointer<ProofSource::Chain> chain =
          GetCertChain(server_address, client_address, hostname,
                       &proof.cert_matched_sni);
      callback->Run(/*ok=*/false, chain, proof, /*details=*/nullptr);
      return;
    }
    proof_source_->GetProof(server_address, client_address, hostname,
                            server_config, transport_version, chlo_hash,
                            std::move(callback));
  }

  quiche::QuicheReferenceCountedPointer<Chain> GetCertChain(
      const QuicSocketAddress& server_address,
      const QuicSocketAddress& client_address, const std::string& hostname,
      bool* cert_matched_sni) override {
    if (!proof_source_) {
      return quiche::QuicheReferenceCountedPointer<Chain>();
    }
    return proof_source_->GetCertChain(server_address, client_address, hostname,
                                       cert_matched_sni);
  }

  void ComputeTlsSignature(
      const QuicSocketAddress& server_address,
      const QuicSocketAddress& client_address, const std::string& hostname,
      uint16_t signature_algorithm, absl::string_view in,
      std::unique_ptr<SignatureCallback> callback) override {
    if (!proof_source_) {
      callback->Run(/*ok=*/true, "Signature", /*details=*/nullptr);
      return;
    }
    proof_source_->ComputeTlsSignature(server_address, client_address, hostname,
                                       signature_algorithm, in,
                                       std::move(callback));
  }

  QuicSignatureAlgorithmVector SupportedTlsSignatureAlgorithms()
      const override {
    if (!proof_source_) {
      return {};
    }
    return proof_source_->SupportedTlsSignatureAlgorithms();
  }

  TicketCrypter* GetTicketCrypter() override { return nullptr; }

 private:
  std::unique_ptr<ProofSource> proof_source_;
};

// Used by QuicCryptoClientConfig to verify server credentials, passes
// everything through to ProofVerifierForTesting is success is true,
// otherwise returns a canned response of QUIC_FAILURE.
class IndirectionProofVerifier : public ProofVerifier {
 public:
  explicit IndirectionProofVerifier(bool success) {
    if (success) {
      proof_verifier_ = crypto_test_utils::ProofVerifierForTesting();
    }
  }

  // ProofVerifier override
  QuicAsyncStatus VerifyProof(
      const std::string& hostname, const uint16_t port,
      const std::string& server_config, QuicTransportVersion transport_version,
      absl::string_view chlo_hash, const std::vector<std::string>& certs,
      const std::string& cert_sct, const std::string& signature,
      const ProofVerifyContext* context, std::string* error_details,
      std::unique_ptr<ProofVerifyDetails>* verify_details,
      std::unique_ptr<ProofVerifierCallback> callback) override {
    if (!proof_verifier_) {
      return QUIC_FAILURE;
    }
    return proof_verifier_->VerifyProof(
        hostname, port, server_config, transport_version, chlo_hash, certs,
        cert_sct, signature, context, error_details, verify_details,
        std::move(callback));
  }

  QuicAsyncStatus VerifyCertChain(
      const std::string& hostname, const uint16_t port,
      const std::vector<std::string>& certs, const std::string& ocsp_response,
      const std::string& cert_sct, const ProofVerifyContext* context,
      std::string* error_details, std::unique_ptr<ProofVerifyDetails>* details,
      uint8_t* out_alert,
      std::unique_ptr<ProofVerifierCallback> callback) override {
    if (!proof_verifier_) {
      return QUIC_FAILURE;
    }
    return proof_verifier_->VerifyCertChain(
        hostname, port, certs, ocsp_response, cert_sct, context, error_details,
        details, out_alert, std::move(callback));
  }

  std::unique_ptr<ProofVerifyContext> CreateDefaultContext() override {
    if (!proof_verifier_) {
      return nullptr;
    }
    return proof_verifier_->CreateDefaultContext();
  }

 private:
  std::unique_ptr<ProofVerifier> proof_verifier_;
};

class DataSavingQbonePacketWriter : public QbonePacketWriter {
 public:
  void WritePacketToNetwork(const char* packet, size_t size) override {
    data_.push_back(std::string(packet, size));
  }

  const std::vector<std::string>& data() { return data_; }

 private:
  std::vector<std::string> data_;
};

template <class T>
class DataSavingQboneControlHandler : public QboneControlHandler<T> {
 public:
  void OnControlRequest(const T& request) override { data_.push_back(request); }

  void OnControlError() override { error_ = true; }

  const std::vector<T>& data() { return data_; }
  bool error() { return error_; }

 private:
  std::vector<T> data_;
  bool error_ = false;
};

// Single-threaded scheduled task runner based on a MockClock.
//
// Simulates asynchronous execution on a single thread by holding scheduled
// tasks until Run() is called. Performs no synchronization, assumes that
// Schedule() and Run() are called on the same thread.
class FakeTaskRunner {
 public:
  explicit FakeTaskRunner(MockQuicConnectionHelper* helper)
      : tasks_([](const TaskType& l, const TaskType& r) {
          // Items at a later time should run after items at an earlier time.
          // Priority queue comparisons should return true if l appears after r.
          return l->time() > r->time();
        }),
        helper_(helper) {}

  // Runs all tasks in time order.  Executes tasks scheduled at
  // the same in an arbitrary order.
  void Run() {
    while (!tasks_.empty()) {
      tasks_.top()->Run();
      tasks_.pop();
    }
  }

 private:
  class InnerTask {
   public:
    InnerTask(std::function<void()> task, QuicTime time)
        : task_(std::move(task)), time_(time) {}

    void Cancel() { cancelled_ = true; }

    void Run() {
      if (!cancelled_) {
        std::move(task_)();
      }
    }

    QuicTime time() const { return time_; }

   private:
    bool cancelled_ = false;
    quiche::SingleUseCallback<void()> task_;
    QuicTime time_;
  };

 public:
  // Schedules a function to run immediately and advances the time.
  void Schedule(std::function<void()> task) {
    tasks_.push(std::shared_ptr<InnerTask>(
        new InnerTask(std::move(task), helper_->GetClock()->Now())));
    helper_->AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
  }

 private:
  using TaskType = std::shared_ptr<InnerTask>;
  std::priority_queue<
      TaskType, std::vector<TaskType>,
      quiche::UnretainedCallback<bool(const TaskType&, const TaskType&)>>
      tasks_;
  MockQuicConnectionHelper* helper_;
};

class QboneSessionTest : public QuicTestWithParam<ParsedQuicVersion> {
 public:
  QboneSessionTest()
      : supported_versions_({GetParam()}),
        runner_(&helper_),
        compressed_certs_cache_(100) {}

  ~QboneSessionTest() override {
    delete client_connection_;
    delete server_connection_;
  }

  const MockClock* GetClock() const {
    return static_cast<const MockClock*>(helper_.GetClock());
  }

  // The parameters are used to control whether the handshake will success or
  // not.
  void CreateClientAndServerSessions(bool client_handshake_success = true,
                                     bool server_handshake_success = true,
                                     bool send_qbone_alpn = true) {
    // Quic crashes if packets are sent at time 0, and the clock defaults to 0.
    helper_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1000));
    event_loop_ = GetDefaultEventLoop()->Create(QuicDefaultClock::Get());
    alarm_factory_ = event_loop_->CreateAlarmFactory();
    client_writer_ = std::make_unique<DataSavingQbonePacketWriter>();
    server_writer_ = std::make_unique<DataSavingQbonePacketWriter>();
    client_handler_ =
        std::make_unique<DataSavingQboneControlHandler<QboneClientRequest>>();
    server_handler_ =
        std::make_unique<DataSavingQboneControlHandler<QboneServerRequest>>();
    QuicSocketAddress server_address(TestLoopback(), 0);
    QuicSocketAddress client_address;
    if (server_address.host().address_family() == IpAddressFamily::IP_V4) {
      client_address = QuicSocketAddress(QuicIpAddress::Any4(), 0);
    } else {
      client_address = QuicSocketAddress(QuicIpAddress::Any6(), 0);
    }

    {
      client_connection_ = new QuicConnection(
          TestConnectionId(), client_address, server_address, &helper_,
          alarm_factory_.get(), new NiceMock<MockPacketWriter>(), true,
          Perspective::IS_CLIENT, supported_versions_,
          connection_id_generator_);
      client_connection_->SetSelfAddress(client_address);
      QuicConfig config;
      client_crypto_config_ = std::make_unique<QuicCryptoClientConfig>(
          std::make_unique<IndirectionProofVerifier>(client_handshake_success));
      if (send_qbone_alpn) {
        client_crypto_config_->set_alpn("qbone");
      }
      client_peer_ = std::make_unique<QboneClientSession>(
          client_connection_, client_crypto_config_.get(),
          /*owner=*/nullptr, config, supported_versions_,
          QuicServerId("test.example.com", 1234), client_writer_.get(),
          client_handler_.get());
    }

    {
      server_connection_ = new QuicConnection(
          TestConnectionId(), server_address, client_address, &helper_,
          alarm_factory_.get(), new NiceMock<MockPacketWriter>(), true,
          Perspective::IS_SERVER, supported_versions_,
          connection_id_generator_);
      server_connection_->SetSelfAddress(server_address);
      QuicConfig config;
      server_crypto_config_ = std::make_unique<QuicCryptoServerConfig>(
          QuicCryptoServerConfig::TESTING, QuicRandom::GetInstance(),
          std::make_unique<IndirectionProofSource>(server_handshake_success),
          KeyExchangeSource::Default());
      QuicCryptoServerConfig::ConfigOptions options;
      QuicServerConfigProtobuf primary_config =
          server_crypto_config_->GenerateConfig(QuicRandom::GetInstance(),
                                                GetClock(), options);
      std::unique_ptr<CryptoHandshakeMessage> message(
          server_crypto_config_->AddConfig(primary_config,
                                           GetClock()->WallNow()));

      server_peer_ = std::make_unique<QboneServerSession>(
          supported_versions_, server_connection_, nullptr, config,
          server_crypto_config_.get(), &compressed_certs_cache_,
          server_writer_.get(), TestLoopback6(), TestLoopback6(), 64,
          server_handler_.get());
    }

    // Hook everything up!
    MockPacketWriter* client_writer = static_cast<MockPacketWriter*>(
        QuicConnectionPeer::GetWriter(client_peer_->connection()));
    ON_CALL(*client_writer, WritePacket(_, _, _, _, _, _))
        .WillByDefault(Invoke([this](const char* buffer, size_t buf_len,
                                     const QuicIpAddress& self_address,
                                     const QuicSocketAddress& peer_address,
                                     PerPacketOptions* option,
                                     const QuicPacketWriterParams& params) {
          char* copy = new char[1024 * 1024];
          memcpy(copy, buffer, buf_len);
          runner_.Schedule([this, copy, buf_len] {
            QuicReceivedPacket packet(copy, buf_len, GetClock()->Now());
            server_peer_->ProcessUdpPacket(server_connection_->self_address(),
                                           client_connection_->self_address(),
                                           packet);
            delete[] copy;
          });
          return WriteResult(WRITE_STATUS_OK, buf_len);
        }));
    MockPacketWriter* server_writer = static_cast<MockPacketWriter*>(
        QuicConnectionPeer::GetWriter(server_peer_->connection()));
    ON_CALL(*server_writer, WritePacket(_, _, _, _, _, _))
        .WillByDefault(Invoke([this](const char* buffer, size_t buf_len,
                                     const QuicIpAddress& self_address,
                                     const QuicSocketAddress& peer_address,
                                     PerPacketOptions* options,
                                     const QuicPacketWriterParams& params) {
          char* copy = new char[1024 * 1024];
          memcpy(copy, buffer, buf_len);
          runner_.Schedule([this, copy, buf_len] {
            QuicReceivedPacket packet(copy, buf_len, GetClock()->Now());
            client_peer_->ProcessUdpPacket(client_connection_->self_address(),
                                           server_connection_->self_address(),
                                           packet);
            delete[] copy;
          });
          return WriteResult(WRITE_STATUS_OK, buf_len);
        }));
  }

  void StartHandshake() {
    server_peer_->Initialize();
    client_peer_->Initialize();
    runner_.Run();
  }

  void ExpectICMPTooBigResponse(const std::vector<std::string>& written_packets,
                                const int mtu, const std::string& packet) {
    auto* header = reinterpret_cast<const ip6_hdr*>(packet.data());
    icmp6_hdr icmp_header{};
    icmp_header.icmp6_type = ICMP6_PACKET_TOO_BIG;
    icmp_header.icmp6_mtu = mtu;

    std::string expected;
    CreateIcmpPacket(header->ip6_dst, header->ip6_src, icmp_header, packet,
                     [&expected](absl::string_view icmp_packet) {
                       expected = std::string(icmp_packet);
                     });

    EXPECT_THAT(written_packets, Contains(expected));
  }

  // Test handshake establishment and sending/receiving of data for two
  // directions.
  void TestStreamConnection(bool use_messages) {
    ASSERT_TRUE(server_peer_->OneRttKeysAvailable());
    ASSERT_TRUE(client_peer_->OneRttKeysAvailable());
    ASSERT_TRUE(server_peer_->IsEncryptionEstablished());
    ASSERT_TRUE(client_peer_->IsEncryptionEstablished());

    // Create an outgoing stream from the client and say hello.
    QUIC_LOG(INFO) << "Sending client -> server";
    client_peer_->ProcessPacketFromNetwork(TestPacketIn("hello"));
    client_peer_->ProcessPacketFromNetwork(TestPacketIn("world"));
    runner_.Run();
    // The server should see the data, the client hasn't received
    // anything yet.
    EXPECT_THAT(server_writer_->data(),
                ElementsAre(TestPacketOut("hello"), TestPacketOut("world")));
    EXPECT_TRUE(client_writer_->data().empty());
    EXPECT_EQ(0u, server_peer_->GetNumActiveStreams());
    EXPECT_EQ(0u, client_peer_->GetNumActiveStreams());

    // Let's pretend some service responds.
    QUIC_LOG(INFO) << "Sending server -> client";
    server_peer_->ProcessPacketFromNetwork(TestPacketIn("Hello Again"));
    server_peer_->ProcessPacketFromNetwork(TestPacketIn("Again"));
    runner_.Run();
    EXPECT_THAT(server_writer_->data(),
                ElementsAre(TestPacketOut("hello"), TestPacketOut("world")));
    EXPECT_THAT(
        client_writer_->data(),
        ElementsAre(TestPacketOut("Hello Again"), TestPacketOut("Again")));
    EXPECT_EQ(0u, server_peer_->GetNumActiveStreams());
    EXPECT_EQ(0u, client_peer_->GetNumActiveStreams());

    // Try to send long payloads that are larger than the QUIC MTU but
    // smaller than the QBONE max size.
    // This should trigger the non-ephemeral stream code path.
    std::string long_data(
        QboneConstants::kMaxQbonePacketBytes - sizeof(ip6_hdr) - 1, 'A');
    QUIC_LOG(INFO) << "Sending server -> client long data";
    server_peer_->ProcessPacketFromNetwork(TestPacketIn(long_data));
    runner_.Run();
    if (use_messages) {
      ExpectICMPTooBigResponse(
          server_writer_->data(),
          server_peer_->connection()->GetGuaranteedLargestMessagePayload(),
          TestPacketOut(long_data));
    } else {
      EXPECT_THAT(client_writer_->data(), Contains(TestPacketOut(long_data)));
    }
    EXPECT_THAT(server_writer_->data(),
                Not(Contains(TestPacketOut(long_data))));
    EXPECT_EQ(0u, server_peer_->GetNumActiveStreams());
    EXPECT_EQ(0u, client_peer_->GetNumActiveStreams());

    QUIC_LOG(INFO) << "Sending client -> server long data";
    client_peer_->ProcessPacketFromNetwork(TestPacketIn(long_data));
    runner_.Run();
    if (use_messages) {
      ExpectICMPTooBigResponse(
          client_writer_->data(),
          client_peer_->connection()->GetGuaranteedLargestMessagePayload(),
          TestPacketIn(long_data));
    } else {
      EXPECT_THAT(server_writer_->data(), Contains(TestPacketOut(long_data)));
    }
    EXPECT_FALSE(client_peer_->EarlyDataAccepted());
    EXPECT_FALSE(client_peer_->ReceivedInchoateReject());
    EXPECT_THAT(client_peer_->GetNumReceivedServerConfigUpdates(), Eq(0));

    if (!use_messages) {
      EXPECT_THAT(client_peer_->GetNumStreamedPackets(), Eq(1));
      EXPECT_THAT(server_peer_->GetNumStreamedPackets(), Eq(1));
    }

    if (use_messages) {
      EXPECT_THAT(client_peer_->GetNumEphemeralPackets(), Eq(0));
      EXPECT_THAT(server_peer_->GetNumEphemeralPackets(), Eq(0));
      EXPECT_THAT(client_peer_->GetNumMessagePackets(), Eq(2));
      EXPECT_THAT(server_peer_->GetNumMessagePackets(), Eq(2));
    } else {
      EXPECT_THAT(client_peer_->GetNumEphemeralPackets(), Eq(2));
      EXPECT_THAT(server_peer_->GetNumEphemeralPackets(), Eq(2));
      EXPECT_THAT(client_peer_->GetNumMessagePackets(), Eq(0));
      EXPECT_THAT(server_peer_->GetNumMessagePackets(), Eq(0));
    }

    // All streams are ephemeral and should be gone.
    EXPECT_EQ(0u, server_peer_->GetNumActiveStreams());
    EXPECT_EQ(0u, client_peer_->GetNumActiveStreams());
  }

  // Test that client and server are not connected after handshake failure.
  void TestDisconnectAfterFailedHandshake() {
    EXPECT_FALSE(client_peer_->IsEncryptionEstablished());
    EXPECT_FALSE(client_peer_->OneRttKeysAvailable());

    EXPECT_FALSE(server_peer_->IsEncryptionEstablished());
    EXPECT_FALSE(server_peer_->OneRttKeysAvailable());
  }

 protected:
  const ParsedQuicVersionVector supported_versions_;
  std::unique_ptr<QuicEventLoop> event_loop_;
  std::unique_ptr<QuicAlarmFactory> alarm_factory_;
  FakeTaskRunner runner_;
  MockQuicConnectionHelper helper_;
  QuicConnection* client_connection_;
  QuicConnection* server_connection_;
  QuicCompressedCertsCache compressed_certs_cache_;

  std::unique_ptr<QuicCryptoClientConfig> client_crypto_config_;
  std::unique_ptr<QuicCryptoServerConfig> server_crypto_config_;
  std::unique_ptr<DataSavingQbonePacketWriter> client_writer_;
  std::unique_ptr<DataSavingQbonePacketWriter> server_writer_;
  std::unique_ptr<DataSavingQboneControlHandler<QboneClientRequest>>
      client_handler_;
  std::unique_ptr<DataSavingQboneControlHandler<QboneServerRequest>>
      server_handler_;

  std::unique_ptr<QboneServerSession> server_peer_;
  std::unique_ptr<QboneClientSession> client_peer_;
  MockConnectionIdGenerator connection_id_generator_;
};

INSTANTIATE_TEST_SUITE_P(Tests, QboneSessionTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

TEST_P(QboneSessionTest, StreamConnection) {
  CreateClientAndServerSessions();
  client_peer_->set_send_packets_as_messages(false);
  server_peer_->set_send_packets_as_messages(false);
  StartHandshake();
  TestStreamConnection(false);
}

TEST_P(QboneSessionTest, Messages) {
  CreateClientAndServerSessions();
  client_peer_->set_send_packets_as_messages(true);
  server_peer_->set_send_packets_as_messages(true);
  StartHandshake();
  TestStreamConnection(true);
}

TEST_P(QboneSessionTest, ClientRejection) {
  CreateClientAndServerSessions(false /*client_handshake_success*/,
                                true /*server_handshake_success*/,
                                true /*send_qbone_alpn*/);
  StartHandshake();
  TestDisconnectAfterFailedHandshake();
}

TEST_P(QboneSessionTest, BadAlpn) {
  CreateClientAndServerSessions(true /*client_handshake_success*/,
                                true /*server_handshake_success*/,
                                false /*send_qbone_alpn*/);
  StartHandshake();
  TestDisconnectAfterFailedHandshake();
}

TEST_P(QboneSessionTest, ServerRejection) {
  CreateClientAndServerSessions(true /*client_handshake_success*/,
                                false /*server_handshake_success*/,
                                true /*send_qbone_alpn*/);
  StartHandshake();
  TestDisconnectAfterFailedHandshake();
}

// Test that data streams are not created before handshake.
TEST_P(QboneSessionTest, CannotCreateDataStreamBeforeHandshake) {
  CreateClientAndServerSessions();
  EXPECT_QUIC_BUG(client_peer_->ProcessPacketFromNetwork(TestPacketIn("hello")),
                  "Attempting to send packet before encryption established");
  EXPECT_QUIC_BUG(server_peer_->ProcessPacketFromNetwork(TestPacketIn("hello")),
                  "Attempting to send packet before encryption established");
  EXPECT_EQ(0u, server_peer_->GetNumActiveStreams());
  EXPECT_EQ(0u, client_peer_->GetNumActiveStreams());
}

TEST_P(QboneSessionTest, ControlRequests) {
  CreateClientAndServerSessions();
  StartHandshake();
  EXPECT_TRUE(client_handler_->data().empty());
  EXPECT_FALSE(client_handler_->error());
  EXPECT_TRUE(server_handler_->data().empty());
  EXPECT_FALSE(server_handler_->error());

  QboneClientRequest client_request;
  client_request.SetExtension(client_placeholder, "hello from the server");
  EXPECT_TRUE(server_peer_->SendClientRequest(client_request));
  runner_.Run();
  ASSERT_FALSE(client_handler_->data().empty());
  EXPECT_THAT(client_handler_->data()[0].GetExtension(client_placeholder),
              Eq("hello from the server"));
  EXPECT_FALSE(client_handler_->error());

  QboneServerRequest server_request;
  server_request.SetExtension(server_placeholder, "hello from the client");
  EXPECT_TRUE(client_peer_->SendServerRequest(server_request));
  runner_.Run();
  ASSERT_FALSE(server_handler_->data().empty());
  EXPECT_THAT(server_handler_->data()[0].GetExtension(server_placeholder),
              Eq("hello from the client"));
  EXPECT_FALSE(server_handler_->error());
}

}  // namespace
}  // namespace test
}  // namespace quic
