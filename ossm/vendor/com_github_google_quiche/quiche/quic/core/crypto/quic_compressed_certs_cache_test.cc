// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/crypto/quic_compressed_certs_cache.h"

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "quiche/quic/core/crypto/cert_compressor.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/crypto_test_utils.h"

namespace quic {

namespace test {

namespace {

class QuicCompressedCertsCacheTest : public QuicTest {
 public:
  QuicCompressedCertsCacheTest()
      : certs_cache_(QuicCompressedCertsCache::kQuicCompressedCertsCacheSize) {}

 protected:
  QuicCompressedCertsCache certs_cache_;
};

TEST_F(QuicCompressedCertsCacheTest, CacheHit) {
  std::vector<std::string> certs = {"leaf cert", "intermediate cert",
                                    "root cert"};
  quiche::QuicheReferenceCountedPointer<ProofSource::Chain> chain(
      new ProofSource::Chain(certs));
  std::string cached_certs = "cached certs";
  std::string compressed = "compressed cert";

  certs_cache_.Insert(chain, cached_certs, compressed);

  const std::string* cached_value =
      certs_cache_.GetCompressedCert(chain, cached_certs);
  ASSERT_NE(nullptr, cached_value);
  EXPECT_EQ(*cached_value, compressed);
}

TEST_F(QuicCompressedCertsCacheTest, CacheMiss) {
  std::vector<std::string> certs = {"leaf cert", "intermediate cert",
                                    "root cert"};
  quiche::QuicheReferenceCountedPointer<ProofSource::Chain> chain(
      new ProofSource::Chain(certs));

  std::string cached_certs = "cached certs";
  std::string compressed = "compressed cert";

  certs_cache_.Insert(chain, cached_certs, compressed);

  EXPECT_EQ(nullptr,
            certs_cache_.GetCompressedCert(chain, "mismatched cached certs"));

  // A different chain though with equivalent certs should get a cache miss.
  quiche::QuicheReferenceCountedPointer<ProofSource::Chain> chain2(
      new ProofSource::Chain(certs));
  EXPECT_EQ(nullptr, certs_cache_.GetCompressedCert(chain2, cached_certs));
}

TEST_F(QuicCompressedCertsCacheTest, CacheMissDueToEviction) {
  // Test cache returns a miss when a queried uncompressed certs was cached but
  // then evicted.
  std::vector<std::string> certs = {"leaf cert", "intermediate cert",
                                    "root cert"};
  quiche::QuicheReferenceCountedPointer<ProofSource::Chain> chain(
      new ProofSource::Chain(certs));

  std::string cached_certs = "cached certs";
  std::string compressed = "compressed cert";
  certs_cache_.Insert(chain, cached_certs, compressed);

  // Insert another kQuicCompressedCertsCacheSize certs to evict the first
  // cached cert.
  for (unsigned int i = 0;
       i < QuicCompressedCertsCache::kQuicCompressedCertsCacheSize; i++) {
    EXPECT_EQ(certs_cache_.Size(), i + 1);
    certs_cache_.Insert(chain, absl::StrCat(i), absl::StrCat(i));
  }
  EXPECT_EQ(certs_cache_.MaxSize(), certs_cache_.Size());

  EXPECT_EQ(nullptr, certs_cache_.GetCompressedCert(chain, cached_certs));
}

}  // namespace
}  // namespace test
}  // namespace quic
