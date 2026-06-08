// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/crypto/proof_source_x509.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "openssl/ssl.h"
#include "quiche/quic/core/crypto/certificate_view.h"
#include "quiche/quic/core/crypto/proof_source.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/test_certificates.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/platform/api/quiche_reference_counted.h"

namespace quic {
namespace test {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Pointee;

MATCHER_P2(ReferenceCountedChainIs, certs, trust_anchor_id, "") {
  return ExplainMatchResult(
      Pointee(
          AllOf(Field("certs", &ProofSource::Chain::certs, certs),
                Field("trust_anchor_id", &ProofSource::Chain::trust_anchor_id,
                      trust_anchor_id))),
      arg, result_listener);
}

quiche::QuicheReferenceCountedPointer<ProofSource::Chain> MakeChain(
    absl::string_view cert) {
  return quiche::QuicheReferenceCountedPointer<ProofSource::Chain>(
      new ProofSource::Chain(std::vector<std::string>{std::string(cert)}));
}

class ProofSourceX509Test : public QuicTest {
 public:
  ProofSourceX509Test()
      : test_chain_(MakeChain(kTestCertificate)),
        wildcard_chain_(MakeChain(kWildcardCertificate)),
        test_key_(
            CertificatePrivateKey::LoadFromDer(kTestCertificatePrivateKey)),
        wildcard_key_(CertificatePrivateKey::LoadFromDer(
            kWildcardCertificatePrivateKey)) {
    QUICHE_CHECK(test_key_ != nullptr);
    QUICHE_CHECK(wildcard_key_ != nullptr);
  }

 protected:
  quiche::QuicheReferenceCountedPointer<ProofSource::Chain> test_chain_,
      wildcard_chain_;
  std::unique_ptr<CertificatePrivateKey> test_key_, wildcard_key_;
};

TEST_F(ProofSourceX509Test, AddCertificates) {
  std::unique_ptr<ProofSourceX509> proof_source =
      ProofSourceX509::Create(test_chain_, std::move(*test_key_));
  ASSERT_TRUE(proof_source != nullptr);
  EXPECT_TRUE(proof_source->AddCertificateChain(wildcard_chain_,
                                                std::move(*wildcard_key_)));
}

TEST_F(ProofSourceX509Test, AddCertificateKeyMismatch) {
  std::unique_ptr<ProofSourceX509> proof_source =
      ProofSourceX509::Create(test_chain_, std::move(*test_key_));
  ASSERT_TRUE(proof_source != nullptr);
  test_key_ = CertificatePrivateKey::LoadFromDer(kTestCertificatePrivateKey);
  EXPECT_QUIC_BUG((void)proof_source->AddCertificateChain(
                      wildcard_chain_, std::move(*test_key_)),
                  "Private key does not match");
}

TEST_F(ProofSourceX509Test, TlsSignature) {
  class Callback : public ProofSource::SignatureCallback {
   public:
    void Run(bool ok, std::string signature,
             std::unique_ptr<ProofSource::Details> /*details*/) override {
      ASSERT_TRUE(ok);
      std::unique_ptr<CertificateView> view =
          CertificateView::ParseSingleCertificate(kTestCertificate);
      EXPECT_TRUE(view->VerifySignature("Test data", signature,
                                        SSL_SIGN_RSA_PSS_RSAE_SHA256));
    }
  };

  std::unique_ptr<ProofSourceX509> proof_source =
      ProofSourceX509::Create(test_chain_, std::move(*test_key_));
  ASSERT_TRUE(proof_source != nullptr);

  proof_source->ComputeTlsSignature(QuicSocketAddress(), QuicSocketAddress(),
                                    "example.com", SSL_SIGN_RSA_PSS_RSAE_SHA256,
                                    "Test data", std::make_unique<Callback>());
}

class ProofSourceX509CertificateSelectionTest : public ProofSourceX509Test {
 protected:
  void SetUp() override {
    proof_source_ = ProofSourceX509::Create(test_chain_, std::move(*test_key_));
    ASSERT_TRUE(proof_source_);
    ASSERT_TRUE(proof_source_->AddCertificateChain(wildcard_chain_,
                                                   std::move(*wildcard_key_)));
  }

  std::unique_ptr<ProofSourceX509> proof_source_;
};

TEST_F(ProofSourceX509CertificateSelectionTest, DefaultCertificate) {
  bool cert_matched_sni;
  EXPECT_THAT(proof_source_
                  ->GetCertChain(QuicSocketAddress(), QuicSocketAddress(),
                                 "unknown.test", &cert_matched_sni)
                  ->certs,
              ::testing::ElementsAre(kTestCertificate));
  EXPECT_FALSE(cert_matched_sni);

  EXPECT_THAT(proof_source_->GetCertChains(QuicSocketAddress(),
                                           QuicSocketAddress(), "unknown.test"),
              FieldsAre(
                  /*chains_match_sni=*/IsFalse(),
                  /*chains=*/
                  ElementsAre(ReferenceCountedChainIs(
                      /*certs*/ ElementsAre(kTestCertificate),
                      /*trust_anchor_id*/ IsEmpty())),
                  /*ssl_compliance_policy=*/std::nullopt));
}

// mail.example.org is explicitly a SubjectAltName in `kTestCertificate`.
TEST_F(ProofSourceX509CertificateSelectionTest, SubjectAltName) {
  bool cert_matched_sni;
  EXPECT_THAT(proof_source_
                  ->GetCertChain(QuicSocketAddress(), QuicSocketAddress(),
                                 "mail.example.org", &cert_matched_sni)
                  ->certs,
              ::testing::ElementsAre(kTestCertificate));
  EXPECT_TRUE(cert_matched_sni);

  EXPECT_THAT(proof_source_->GetCertChains(
                  QuicSocketAddress(), QuicSocketAddress(), "mail.example.org"),
              FieldsAre(
                  /*chains_match_sni=*/IsTrue(),
                  /*chains=*/
                  ElementsAre(ReferenceCountedChainIs(
                      /*certs*/ ElementsAre(kTestCertificate),
                      /*trust_anchor_id*/ IsEmpty())),
                  /*ssl_compliance_policy=*/std::nullopt));
}

// www.foo.test is in `kWildcardCertificate`.
TEST_F(ProofSourceX509CertificateSelectionTest, DomainInWildcardCertificate) {
  bool cert_matched_sni;
  EXPECT_THAT(proof_source_
                  ->GetCertChain(QuicSocketAddress(), QuicSocketAddress(),
                                 "www.foo.test", &cert_matched_sni)
                  ->certs,
              ::testing::ElementsAre(kWildcardCertificate));
  EXPECT_TRUE(cert_matched_sni);

  EXPECT_THAT(proof_source_->GetCertChains(QuicSocketAddress(),
                                           QuicSocketAddress(), "www.foo.test"),
              FieldsAre(
                  /*chains_match_sni=*/IsTrue(),
                  /*chains=*/
                  ElementsAre(ReferenceCountedChainIs(
                      /*certs*/ ElementsAre(kWildcardCertificate),
                      /*trust_anchor_id*/ IsEmpty())),
                  /*ssl_compliance_policy=*/std::nullopt));
}

// *.wildcard.test is in `kWildcardCertificate`.
TEST_F(ProofSourceX509CertificateSelectionTest,
       SubdomainInWildcardCertificate) {
  bool cert_matched_sni;
  EXPECT_THAT(proof_source_
                  ->GetCertChain(QuicSocketAddress(), QuicSocketAddress(),
                                 "www.wildcard.test", &cert_matched_sni)
                  ->certs,
              ::testing::ElementsAre(kWildcardCertificate));
  EXPECT_TRUE(cert_matched_sni);

  EXPECT_THAT(
      proof_source_->GetCertChains(QuicSocketAddress(), QuicSocketAddress(),
                                   "www.wildcard.test"),
      FieldsAre(
          /*chains_match_sni=*/IsTrue(),
          /*chains=*/
          ElementsAre(ReferenceCountedChainIs(
              /*certs*/ ElementsAre(kWildcardCertificate),
              /*trust_anchor_id*/ IsEmpty())),
          /*ssl_compliance_policy=*/std::nullopt));

  EXPECT_THAT(proof_source_
                  ->GetCertChain(QuicSocketAddress(), QuicSocketAddress(),
                                 "etc.wildcard.test", &cert_matched_sni)
                  ->certs,
              ::testing::ElementsAre(kWildcardCertificate));
  EXPECT_TRUE(cert_matched_sni);

  EXPECT_THAT(
      proof_source_->GetCertChains(QuicSocketAddress(), QuicSocketAddress(),
                                   "etc.wildcard.test"),
      FieldsAre(
          /*chains_match_sni=*/IsTrue(),
          /*chains=*/
          ElementsAre(ReferenceCountedChainIs(
              /*certs*/ ElementsAre(kWildcardCertificate),
              /*trust_anchor_id*/ IsEmpty())),
          /*ssl_compliance_policy=*/std::nullopt));
}

// wildcard.test itself is not in `kWildcardCertificate`.
TEST_F(ProofSourceX509CertificateSelectionTest, NotInWildcardCertificate) {
  bool cert_matched_sni;
  EXPECT_THAT(proof_source_
                  ->GetCertChain(QuicSocketAddress(), QuicSocketAddress(),
                                 "wildcard.test", &cert_matched_sni)
                  ->certs,
              ::testing::ElementsAre(kTestCertificate));
  EXPECT_FALSE(cert_matched_sni);

  EXPECT_THAT(proof_source_->GetCertChains(
                  QuicSocketAddress(), QuicSocketAddress(), "wildcard.test"),
              FieldsAre(
                  /*chains_match_sni=*/IsFalse(),
                  /*chains=*/
                  ElementsAre(ReferenceCountedChainIs(
                      /*certs*/ ElementsAre(kTestCertificate),
                      /*trust_anchor_id*/ IsEmpty())),
                  /*ssl_compliance_policy=*/std::nullopt));
}

}  // namespace
}  // namespace test
}  // namespace quic
