// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/crypto/certificate_util.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "openssl/ssl.h"
#include "quiche/quic/core/crypto/certificate_view.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/platform/api/quic_test_output.h"

namespace quic {
namespace test {
namespace {

TEST(CertificateUtilTest, CreateSelfSignedCertificate) {
  bssl::UniquePtr<EVP_PKEY> key = MakeKeyPairForSelfSignedCertificate();
  ASSERT_NE(key, nullptr);

  CertificatePrivateKey cert_key(std::move(key));

  CertificateOptions options;
  options.subject = "CN=subject";
  options.serial_number = 0x12345678;
  options.validity_start = {2020, 1, 1, 0, 0, 0};
  options.validity_end = {2049, 12, 31, 0, 0, 0};
  std::string der_cert =
      CreateSelfSignedCertificate(*cert_key.private_key(), options);
  ASSERT_FALSE(der_cert.empty());

  QuicSaveTestOutput("CertificateUtilTest_CreateSelfSignedCert.crt", der_cert);

  std::unique_ptr<CertificateView> cert_view =
      CertificateView::ParseSingleCertificate(der_cert);
  ASSERT_NE(cert_view, nullptr);
  EXPECT_EQ(cert_view->public_key_type(), PublicKeyType::kP256);

  std::optional<std::string> subject = cert_view->GetHumanReadableSubject();
  ASSERT_TRUE(subject.has_value());
  EXPECT_EQ(*subject, options.subject);

  EXPECT_TRUE(
      cert_key.ValidForSignatureAlgorithm(SSL_SIGN_ECDSA_SECP256R1_SHA256));
  EXPECT_TRUE(cert_key.MatchesPublicKey(*cert_view));
}

}  // namespace
}  // namespace test
}  // namespace quic
