// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/test_tools/crypto_test_utils.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/proto/crypto_server_config_proto.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/mock_clock.h"

namespace quic {
namespace test {

class ShloVerifier {
 public:
  ShloVerifier(QuicCryptoServerConfig* crypto_config,
               QuicSocketAddress server_addr, QuicSocketAddress client_addr,
               const QuicClock* clock,
               quiche::QuicheReferenceCountedPointer<QuicSignedServerConfig>
                   signed_config,
               QuicCompressedCertsCache* compressed_certs_cache,
               ParsedQuicVersion version)
      : crypto_config_(crypto_config),
        server_addr_(server_addr),
        client_addr_(client_addr),
        clock_(clock),
        signed_config_(signed_config),
        compressed_certs_cache_(compressed_certs_cache),
        params_(new QuicCryptoNegotiatedParameters),
        version_(version) {}

  class ValidateClientHelloCallback : public ValidateClientHelloResultCallback {
   public:
    explicit ValidateClientHelloCallback(ShloVerifier* shlo_verifier)
        : shlo_verifier_(shlo_verifier) {}
    void Run(quiche::QuicheReferenceCountedPointer<
                 ValidateClientHelloResultCallback::Result>
                 result,
             std::unique_ptr<ProofSource::Details> /* details */) override {
      shlo_verifier_->ValidateClientHelloDone(result);
    }

   private:
    ShloVerifier* shlo_verifier_;
  };

  std::unique_ptr<ValidateClientHelloCallback>
  GetValidateClientHelloCallback() {
    return std::make_unique<ValidateClientHelloCallback>(this);
  }

  absl::string_view server_nonce() { return server_nonce_; }
  bool chlo_accepted() const { return chlo_accepted_; }

 private:
  void ValidateClientHelloDone(
      const quiche::QuicheReferenceCountedPointer<
          ValidateClientHelloResultCallback::Result>& result) {
    result_ = result;
    crypto_config_->ProcessClientHello(
        result_, /*reject_only=*/false,
        /*connection_id=*/TestConnectionId(1), server_addr_, client_addr_,
        version_, AllSupportedVersions(), clock_, QuicRandom::GetInstance(),
        compressed_certs_cache_, params_, signed_config_,
        /*total_framing_overhead=*/50, kDefaultMaxPacketSize,
        GetProcessClientHelloCallback());
  }

  class ProcessClientHelloCallback : public ProcessClientHelloResultCallback {
   public:
    explicit ProcessClientHelloCallback(ShloVerifier* shlo_verifier)
        : shlo_verifier_(shlo_verifier) {}
    void Run(QuicErrorCode /*error*/, const std::string& /*error_details*/,
             std::unique_ptr<CryptoHandshakeMessage> message,
             std::unique_ptr<DiversificationNonce> /*diversification_nonce*/,
             std::unique_ptr<ProofSource::Details> /*proof_source_details*/)
        override {
      shlo_verifier_->ProcessClientHelloDone(std::move(message));
    }

   private:
    ShloVerifier* shlo_verifier_;
  };

  std::unique_ptr<ProcessClientHelloCallback> GetProcessClientHelloCallback() {
    return std::make_unique<ProcessClientHelloCallback>(this);
  }

  void ProcessClientHelloDone(std::unique_ptr<CryptoHandshakeMessage> message) {
    if (message->tag() == kSHLO) {
      chlo_accepted_ = true;
    } else {
      QUIC_LOG(INFO) << "Fail to pass validation. Get "
                     << message->DebugString();
      chlo_accepted_ = false;
      EXPECT_EQ(1u, result_->info.reject_reasons.size());
      EXPECT_EQ(SERVER_NONCE_REQUIRED_FAILURE, result_->info.reject_reasons[0]);
      server_nonce_ = result_->info.server_nonce;
    }
  }

  QuicCryptoServerConfig* crypto_config_;
  QuicSocketAddress server_addr_;
  QuicSocketAddress client_addr_;
  const QuicClock* clock_;
  quiche::QuicheReferenceCountedPointer<QuicSignedServerConfig> signed_config_;
  QuicCompressedCertsCache* compressed_certs_cache_;

  quiche::QuicheReferenceCountedPointer<QuicCryptoNegotiatedParameters> params_;
  quiche::QuicheReferenceCountedPointer<
      ValidateClientHelloResultCallback::Result>
      result_;

  const ParsedQuicVersion version_;
  bool chlo_accepted_ = false;
  absl::string_view server_nonce_;
};

class CryptoTestUtilsTest : public QuicTest {};

TEST_F(CryptoTestUtilsTest, TestGenerateFullCHLO) {
  MockClock clock;
  QuicCryptoServerConfig crypto_config(
      QuicCryptoServerConfig::TESTING, QuicRandom::GetInstance(),
      crypto_test_utils::ProofSourceForTesting(), KeyExchangeSource::Default());
  QuicSocketAddress server_addr(QuicIpAddress::Any4(), 5);
  QuicSocketAddress client_addr(QuicIpAddress::Loopback4(), 1);
  quiche::QuicheReferenceCountedPointer<QuicSignedServerConfig> signed_config(
      new QuicSignedServerConfig);
  QuicCompressedCertsCache compressed_certs_cache(
      QuicCompressedCertsCache::kQuicCompressedCertsCacheSize);
  CryptoHandshakeMessage full_chlo;

  QuicCryptoServerConfig::ConfigOptions old_config_options;
  old_config_options.id = "old-config-id";
  crypto_config.AddDefaultConfig(QuicRandom::GetInstance(), &clock,
                                 old_config_options);
  QuicCryptoServerConfig::ConfigOptions new_config_options;
  QuicServerConfigProtobuf primary_config = crypto_config.GenerateConfig(
      QuicRandom::GetInstance(), &clock, new_config_options);
  primary_config.set_primary_time(clock.WallNow().ToUNIXSeconds());
  std::unique_ptr<CryptoHandshakeMessage> msg =
      crypto_config.AddConfig(primary_config, clock.WallNow());
  absl::string_view orbit;
  ASSERT_TRUE(msg->GetStringPiece(kOBIT, &orbit));
  std::string nonce;
  CryptoUtils::GenerateNonce(clock.WallNow(), QuicRandom::GetInstance(), orbit,
                             &nonce);
  std::string nonce_hex = "#" + absl::BytesToHexString(nonce);

  char public_value[32];
  memset(public_value, 42, sizeof(public_value));
  std::string pub_hex = "#" + absl::BytesToHexString(absl::string_view(
                                  public_value, sizeof(public_value)));

  // The methods below use a PROTOCOL_QUIC_CRYPTO version so we pick the
  // first one from the list of supported versions.
  QuicTransportVersion transport_version = QUIC_VERSION_UNSUPPORTED;
  for (const ParsedQuicVersion& version : AllSupportedVersions()) {
    if (version.handshake_protocol == PROTOCOL_QUIC_CRYPTO) {
      transport_version = version.transport_version;
      break;
    }
  }
  ASSERT_NE(QUIC_VERSION_UNSUPPORTED, transport_version);

  CryptoHandshakeMessage inchoate_chlo = crypto_test_utils::CreateCHLO(
      {{"PDMD", "X509"},
       {"AEAD", "AESG"},
       {"KEXS", "C255"},
       {"COPT", "SREJ"},
       {"PUBS", pub_hex},
       {"NONC", nonce_hex},
       {"VER\0",
        QuicVersionLabelToString(CreateQuicVersionLabel(
            ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, transport_version)))}},
      kClientHelloMinimumSize);

  crypto_test_utils::GenerateFullCHLO(inchoate_chlo, &crypto_config,
                                      server_addr, client_addr,
                                      transport_version, &clock, signed_config,
                                      &compressed_certs_cache, &full_chlo);
  // Verify that full_chlo can pass crypto_config's verification.
  ShloVerifier shlo_verifier(
      &crypto_config, server_addr, client_addr, &clock, signed_config,
      &compressed_certs_cache,
      ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, transport_version));
  crypto_config.ValidateClientHello(
      full_chlo, client_addr, server_addr, transport_version, &clock,
      signed_config, shlo_verifier.GetValidateClientHelloCallback());
  ASSERT_EQ(shlo_verifier.chlo_accepted(),
            !GetQuicReloadableFlag(quic_require_handshake_confirmation));
  if (!shlo_verifier.chlo_accepted()) {
    ShloVerifier shlo_verifier2(
        &crypto_config, server_addr, client_addr, &clock, signed_config,
        &compressed_certs_cache,
        ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, transport_version));
    full_chlo.SetStringPiece(
        kServerNonceTag,
        "#" + absl::BytesToHexString(shlo_verifier.server_nonce()));
    crypto_config.ValidateClientHello(
        full_chlo, client_addr, server_addr, transport_version, &clock,
        signed_config, shlo_verifier2.GetValidateClientHelloCallback());
    EXPECT_TRUE(shlo_verifier2.chlo_accepted()) << full_chlo.DebugString();
  }
}

}  // namespace test
}  // namespace quic
