// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/crypto/client_proof_source.h"

#include <string>

#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/test_certificates.h"

namespace quic {
namespace test {

quiche::QuicheReferenceCountedPointer<ClientProofSource::Chain>
TestCertChain() {
  return quiche::QuicheReferenceCountedPointer<ClientProofSource::Chain>(
      new ClientProofSource::Chain({std::string(kTestCertificate)}));
}

CertificatePrivateKey TestPrivateKey() {
  CBS private_key_cbs;
  CBS_init(&private_key_cbs,
           reinterpret_cast<const uint8_t*>(kTestCertificatePrivateKey.data()),
           kTestCertificatePrivateKey.size());

  return CertificatePrivateKey(
      bssl::UniquePtr<EVP_PKEY>(EVP_parse_private_key(&private_key_cbs)));
}

const ClientProofSource::CertAndKey* TestCertAndKey() {
  static const ClientProofSource::CertAndKey cert_and_key(TestCertChain(),
                                                          TestPrivateKey());
  return &cert_and_key;
}

quiche::QuicheReferenceCountedPointer<ClientProofSource::Chain>
NullCertChain() {
  return quiche::QuicheReferenceCountedPointer<ClientProofSource::Chain>();
}

quiche::QuicheReferenceCountedPointer<ClientProofSource::Chain>
EmptyCertChain() {
  return quiche::QuicheReferenceCountedPointer<ClientProofSource::Chain>(
      new ClientProofSource::Chain(std::vector<std::string>()));
}

quiche::QuicheReferenceCountedPointer<ClientProofSource::Chain> BadCertChain() {
  return quiche::QuicheReferenceCountedPointer<ClientProofSource::Chain>(
      new ClientProofSource::Chain({"This is the content of a bad cert."}));
}

CertificatePrivateKey EmptyPrivateKey() {
  return CertificatePrivateKey(bssl::UniquePtr<EVP_PKEY>(EVP_PKEY_new()));
}

#define VERIFY_CERT_AND_KEY_MATCHES(lhs, rhs) \
  do {                                        \
    SCOPED_TRACE(testing::Message());         \
    VerifyCertAndKeyMatches(lhs.get(), rhs);  \
  } while (0)

void VerifyCertAndKeyMatches(const ClientProofSource::CertAndKey* lhs,
                             const ClientProofSource::CertAndKey* rhs) {
  if (lhs == rhs) {
    return;
  }

  if (lhs == nullptr) {
    ADD_FAILURE() << "lhs is nullptr, but rhs is not";
    return;
  }

  if (rhs == nullptr) {
    ADD_FAILURE() << "rhs is nullptr, but lhs is not";
    return;
  }

  if (1 != EVP_PKEY_cmp(lhs->private_key.private_key(),
                        rhs->private_key.private_key())) {
    ADD_FAILURE() << "Private keys mismatch";
    return;
  }

  const ClientProofSource::Chain* lhs_chain = lhs->chain.get();
  const ClientProofSource::Chain* rhs_chain = rhs->chain.get();

  if (lhs_chain == rhs_chain) {
    return;
  }

  if (lhs_chain == nullptr) {
    ADD_FAILURE() << "lhs->chain is nullptr, but rhs->chain is not";
    return;
  }

  if (rhs_chain == nullptr) {
    ADD_FAILURE() << "rhs->chain is nullptr, but lhs->chain is not";
    return;
  }

  if (lhs_chain->certs.size() != rhs_chain->certs.size()) {
    ADD_FAILURE() << "Cert chain length differ. lhs:" << lhs_chain->certs.size()
                  << ", rhs:" << rhs_chain->certs.size();
    return;
  }

  for (size_t i = 0; i < lhs_chain->certs.size(); ++i) {
    if (lhs_chain->certs[i] != rhs_chain->certs[i]) {
      ADD_FAILURE() << "The " << i << "-th certs differ.";
      return;
    }
  }

  // All good.
}

TEST(DefaultClientProofSource, FullDomain) {
  DefaultClientProofSource proof_source;
  ASSERT_TRUE(proof_source.AddCertAndKey({"www.google.com"}, TestCertChain(),
                                         TestPrivateKey()));
  VERIFY_CERT_AND_KEY_MATCHES(proof_source.GetCertAndKey("www.google.com"),
                              TestCertAndKey());
  EXPECT_EQ(proof_source.GetCertAndKey("*.google.com"), nullptr);
  EXPECT_EQ(proof_source.GetCertAndKey("*"), nullptr);
}

TEST(DefaultClientProofSource, WildcardDomain) {
  DefaultClientProofSource proof_source;
  ASSERT_TRUE(proof_source.AddCertAndKey({"*.google.com"}, TestCertChain(),
                                         TestPrivateKey()));
  VERIFY_CERT_AND_KEY_MATCHES(proof_source.GetCertAndKey("www.google.com"),
                              TestCertAndKey());
  VERIFY_CERT_AND_KEY_MATCHES(proof_source.GetCertAndKey("*.google.com"),
                              TestCertAndKey());
  EXPECT_EQ(proof_source.GetCertAndKey("*"), nullptr);
}

TEST(DefaultClientProofSource, DefaultDomain) {
  DefaultClientProofSource proof_source;
  ASSERT_TRUE(
      proof_source.AddCertAndKey({"*"}, TestCertChain(), TestPrivateKey()));
  VERIFY_CERT_AND_KEY_MATCHES(proof_source.GetCertAndKey("www.google.com"),
                              TestCertAndKey());
  VERIFY_CERT_AND_KEY_MATCHES(proof_source.GetCertAndKey("*.google.com"),
                              TestCertAndKey());
  VERIFY_CERT_AND_KEY_MATCHES(proof_source.GetCertAndKey("*"),
                              TestCertAndKey());
}

TEST(DefaultClientProofSource, FullAndWildcard) {
  DefaultClientProofSource proof_source;
  ASSERT_TRUE(proof_source.AddCertAndKey({"www.google.com", "*.google.com"},
                                         TestCertChain(), TestPrivateKey()));
  VERIFY_CERT_AND_KEY_MATCHES(proof_source.GetCertAndKey("www.google.com"),
                              TestCertAndKey());
  VERIFY_CERT_AND_KEY_MATCHES(proof_source.GetCertAndKey("foo.google.com"),
                              TestCertAndKey());
  EXPECT_EQ(proof_source.GetCertAndKey("www.example.com"), nullptr);
  EXPECT_EQ(proof_source.GetCertAndKey("*"), nullptr);
}

TEST(DefaultClientProofSource, FullWildcardAndDefault) {
  DefaultClientProofSource proof_source;
  ASSERT_TRUE(
      proof_source.AddCertAndKey({"www.google.com", "*.google.com", "*"},
                                 TestCertChain(), TestPrivateKey()));
  VERIFY_CERT_AND_KEY_MATCHES(proof_source.GetCertAndKey("www.google.com"),
                              TestCertAndKey());
  VERIFY_CERT_AND_KEY_MATCHES(proof_source.GetCertAndKey("foo.google.com"),
                              TestCertAndKey());
  VERIFY_CERT_AND_KEY_MATCHES(proof_source.GetCertAndKey("www.example.com"),
                              TestCertAndKey());
  VERIFY_CERT_AND_KEY_MATCHES(proof_source.GetCertAndKey("*.google.com"),
                              TestCertAndKey());
  VERIFY_CERT_AND_KEY_MATCHES(proof_source.GetCertAndKey("*"),
                              TestCertAndKey());
}

TEST(DefaultClientProofSource, EmptyCerts) {
  DefaultClientProofSource proof_source;
  EXPECT_QUIC_BUG(ASSERT_FALSE(proof_source.AddCertAndKey(
                      {"*"}, NullCertChain(), TestPrivateKey())),
                  "Certificate chain is empty");

  EXPECT_QUIC_BUG(ASSERT_FALSE(proof_source.AddCertAndKey(
                      {"*"}, EmptyCertChain(), TestPrivateKey())),
                  "Certificate chain is empty");
  EXPECT_EQ(proof_source.GetCertAndKey("*"), nullptr);
}

TEST(DefaultClientProofSource, BadCerts) {
  DefaultClientProofSource proof_source;
  EXPECT_QUIC_BUG(ASSERT_FALSE(proof_source.AddCertAndKey({"*"}, BadCertChain(),
                                                          TestPrivateKey())),
                  "Unabled to parse leaf certificate");
  EXPECT_EQ(proof_source.GetCertAndKey("*"), nullptr);
}

TEST(DefaultClientProofSource, KeyMismatch) {
  DefaultClientProofSource proof_source;
  EXPECT_QUIC_BUG(ASSERT_FALSE(proof_source.AddCertAndKey(
                      {"www.google.com"}, TestCertChain(), EmptyPrivateKey())),
                  "Private key does not match the leaf certificate");
  EXPECT_EQ(proof_source.GetCertAndKey("*"), nullptr);
}

}  // namespace test
}  // namespace quic
