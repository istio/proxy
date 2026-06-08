// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/load_balancer/load_balancer_encoder.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <queue>

#include "absl/numeric/int128.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/quic/core/crypto/quic_random.h"
#include "quiche/quic/core/quic_connection_id.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/load_balancer/load_balancer_config.h"
#include "quiche/quic/load_balancer/load_balancer_server_id.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

namespace quic {

namespace test {

class LoadBalancerEncoderPeer {
 public:
  static void SetNumNoncesLeft(LoadBalancerEncoder &encoder,
                               uint64_t nonces_remaining) {
    encoder.num_nonces_left_ = absl::uint128(nonces_remaining);
  }
};

namespace {

class TestLoadBalancerEncoderVisitor
    : public LoadBalancerEncoderVisitorInterface {
 public:
  ~TestLoadBalancerEncoderVisitor() override {}

  void OnConfigAdded(const uint8_t config_id) override {
    num_adds_++;
    current_config_id_ = config_id;
  }

  void OnConfigChanged(const uint8_t old_config_id,
                       const uint8_t new_config_id) override {
    num_adds_++;
    num_deletes_++;
    EXPECT_EQ(old_config_id, current_config_id_);
    current_config_id_ = new_config_id;
  }

  void OnConfigDeleted(const uint8_t config_id) override {
    EXPECT_EQ(config_id, current_config_id_);
    current_config_id_.reset();
    num_deletes_++;
  }

  uint32_t num_adds() const { return num_adds_; }
  uint32_t num_deletes() const { return num_deletes_; }

 private:
  uint32_t num_adds_ = 0, num_deletes_ = 0;
  std::optional<uint8_t> current_config_id_ = std::optional<uint8_t>();
};

// Allows the caller to specify the exact results in 64-bit chunks.
class TestRandom : public QuicRandom {
 public:
  uint64_t RandUint64() override {
    if (next_values_.empty()) {
      return base_;
    }
    uint64_t value = next_values_.front();
    next_values_.pop();
    return value;
  }

  void RandBytes(void *data, size_t len) override {
    size_t written = 0;
    uint8_t *ptr = static_cast<uint8_t *>(data);
    while (written < len) {
      uint64_t result = RandUint64();
      size_t to_write = (len - written > sizeof(uint64_t)) ? sizeof(uint64_t)
                                                           : (len - written);
      memcpy(ptr + written, &result, to_write);
      written += to_write;
    }
  }

  void InsecureRandBytes(void *data, size_t len) override {
    RandBytes(data, len);
  }

  uint64_t InsecureRandUint64() override { return RandUint64(); }

  void AddNextValues(uint64_t hi, uint64_t lo) {
    next_values_.push(hi);
    next_values_.push(lo);
  }

 private:
  std::queue<uint64_t> next_values_;
  uint64_t base_ = 0xDEADBEEFDEADBEEF;
};

class LoadBalancerEncoderTest : public QuicTest {
 public:
  TestRandom random_;
};

// Convenience function to shorten the code. Does not check if |array| is long
// enough or |length| is valid for a server ID.
LoadBalancerServerId MakeServerId(const uint8_t array[], const uint8_t length) {
  return LoadBalancerServerId(absl::Span<const uint8_t>(array, length));
}

constexpr char kRawKey[] = {0x8f, 0x95, 0xf0, 0x92, 0x45, 0x76, 0x5f, 0x80,
                            0x25, 0x69, 0x34, 0xe5, 0x0c, 0x66, 0x20, 0x7f};
constexpr absl::string_view kKey(kRawKey, kLoadBalancerKeyLen);
constexpr uint64_t kNonceLow = 0xe5d1c048bf0d08ee;
constexpr uint64_t kNonceHigh = 0x9321e7e34dde525d;
constexpr uint8_t kServerId[] = {0xed, 0x79, 0x3a, 0x51, 0xd4, 0x9b, 0x8f, 0x5f,
                                 0xab, 0x65, 0xba, 0x04, 0xc3, 0x33, 0x0a};

TEST_F(LoadBalancerEncoderTest, BadUnroutableLength) {
  EXPECT_QUIC_BUG(
      EXPECT_FALSE(
          LoadBalancerEncoder::Create(random_, nullptr, false, 0).has_value()),
      "Invalid unroutable_connection_id_len = 0");
  EXPECT_QUIC_BUG(
      EXPECT_FALSE(
          LoadBalancerEncoder::Create(random_, nullptr, false, 21).has_value()),
      "Invalid unroutable_connection_id_len = 21");
}

TEST_F(LoadBalancerEncoderTest, BadServerIdLength) {
  auto encoder = LoadBalancerEncoder::Create(random_, nullptr, true);
  ASSERT_TRUE(encoder.has_value());
  // Expects a 3 byte server ID and got 4.
  auto config = LoadBalancerConfig::CreateUnencrypted(1, 3, 4);
  ASSERT_TRUE(config.has_value());
  EXPECT_QUIC_BUG(
      EXPECT_FALSE(encoder->UpdateConfig(*config, MakeServerId(kServerId, 4))),
      "Server ID length 4 does not match configured value of 3");
  EXPECT_FALSE(encoder->IsEncoding());
}

TEST_F(LoadBalancerEncoderTest, FailToUpdateConfigWithSameId) {
  TestLoadBalancerEncoderVisitor visitor;
  auto encoder = LoadBalancerEncoder::Create(random_, &visitor, true);
  ASSERT_TRUE(encoder.has_value());
  auto config = LoadBalancerConfig::CreateUnencrypted(1, 3, 4);
  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(encoder->UpdateConfig(*config, MakeServerId(kServerId, 3)));
  EXPECT_EQ(visitor.num_adds(), 1u);
  EXPECT_QUIC_BUG(
      EXPECT_FALSE(encoder->UpdateConfig(*config, MakeServerId(kServerId, 3))),
      "Attempting to change config with same ID");
  EXPECT_EQ(visitor.num_adds(), 1u);
}

struct LoadBalancerEncoderTestCase {
  LoadBalancerConfig config;
  QuicConnectionId connection_id;
  LoadBalancerServerId server_id;
};

TEST_F(LoadBalancerEncoderTest, UnencryptedConnectionIdTestVectors) {
  const struct LoadBalancerEncoderTestCase test_vectors[2] = {
      {
          *LoadBalancerConfig::CreateUnencrypted(0, 3, 4),
          QuicConnectionId({0x07, 0xed, 0x79, 0x3a, 0x80, 0x49, 0x71, 0x8a}),
          MakeServerId(kServerId, 3),
      },
      {
          *LoadBalancerConfig::CreateUnencrypted(1, 8, 5),
          QuicConnectionId({0x2d, 0xed, 0x79, 0x3a, 0x51, 0xd4, 0x9b, 0x8f,
                            0x5f, 0x8e, 0x98, 0x53, 0xfe, 0x93}),
          MakeServerId(kServerId, 8),
      },
  };
  for (const auto &test : test_vectors) {
    random_.AddNextValues(kNonceHigh, kNonceLow);
    auto encoder = LoadBalancerEncoder::Create(random_, nullptr, true, 8);
    EXPECT_TRUE(encoder->UpdateConfig(test.config, test.server_id));
    absl::uint128 nonces_left = encoder->num_nonces_left();
    EXPECT_EQ(encoder->GenerateConnectionId(), test.connection_id);
    EXPECT_EQ(encoder->num_nonces_left(), nonces_left - 1);
  }
}

// Follow example in draft-ietf-quic-load-balancers-19.
TEST_F(LoadBalancerEncoderTest, FollowSpecExample) {
  const uint8_t config_id = 0, server_id_len = 3, nonce_len = 4;
  const uint8_t raw_server_id[] = {
      0x31,
      0x44,
      0x1a,
  };
  const char raw_key[] = {
      0xfd, 0xf7, 0x26, 0xa9, 0x89, 0x3e, 0xc0, 0x5c,
      0x06, 0x32, 0xd3, 0x95, 0x66, 0x80, 0xba, 0xf0,
  };
  random_.AddNextValues(0, 0x75c2699c);
  auto encoder = LoadBalancerEncoder::Create(random_, nullptr, true, 8);
  ASSERT_TRUE(encoder.has_value());
  auto config = LoadBalancerConfig::Create(config_id, server_id_len, nonce_len,
                                           absl::string_view(raw_key));
  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(
      encoder->UpdateConfig(*config, LoadBalancerServerId(raw_server_id)));
  EXPECT_TRUE(encoder->IsEncoding());
  const char raw_connection_id[] = {0x07, 0x67, 0x94, 0x7d,
                                    0x29, 0xbe, 0x05, 0x4a};
  auto expected =
      QuicConnectionId(raw_connection_id, 1 + server_id_len + nonce_len);
  EXPECT_EQ(encoder->GenerateConnectionId(), expected);
}

// Compare test vectors from Appendix B of draft-ietf-quic-load-balancers-19.
TEST_F(LoadBalancerEncoderTest, EncoderTestVectors) {
  // Try (1) the "standard" ConnectionId length of 8
  // (2) server_id_len > nonce_len, so there is a fourth decryption pass
  // (3) the single-pass encryption case
  // (4) An even total length.
  const LoadBalancerEncoderTestCase test_vectors[4] = {
      {
          *LoadBalancerConfig::Create(0, 3, 4, kKey),
          QuicConnectionId({0x07, 0x20, 0xb1, 0xd0, 0x7b, 0x35, 0x9d, 0x3c}),
          MakeServerId(kServerId, 3),
      },
      {
          *LoadBalancerConfig::Create(1, 10, 5, kKey),
          QuicConnectionId({0x2f, 0xcc, 0x38, 0x1b, 0xc7, 0x4c, 0xb4, 0xfb,
                            0xad, 0x28, 0x23, 0xa3, 0xd1, 0xf8, 0xfe, 0xd2}),
          MakeServerId(kServerId, 10),
      },
      {
          *LoadBalancerConfig::Create(2, 8, 8, kKey),
          QuicConnectionId({0x50, 0x4d, 0xd2, 0xd0, 0x5a, 0x7b, 0x0d, 0xe9,
                            0xb2, 0xb9, 0x90, 0x7a, 0xfb, 0x5e, 0xcf, 0x8c,
                            0xc3}),
          MakeServerId(kServerId, 8),
      },
      {
          *LoadBalancerConfig::Create(0, 9, 9, kKey),
          QuicConnectionId({0x12, 0x57, 0x79, 0xc9, 0xcc, 0x86, 0xbe, 0xb3,
                            0xa3, 0xa4, 0xa3, 0xca, 0x96, 0xfc, 0xe4, 0xbf,
                            0xe0, 0xcd, 0xbc}),
          MakeServerId(kServerId, 9),
      },
  };
  for (const auto &test : test_vectors) {
    auto encoder = LoadBalancerEncoder::Create(random_, nullptr, true, 8);
    ASSERT_TRUE(encoder.has_value());
    random_.AddNextValues(kNonceHigh, kNonceLow);
    EXPECT_TRUE(encoder->UpdateConfig(test.config, test.server_id));
    EXPECT_EQ(encoder->GenerateConnectionId(), test.connection_id);
  }
}

TEST_F(LoadBalancerEncoderTest, RunOutOfNonces) {
  const uint8_t server_id_len = 3;
  TestLoadBalancerEncoderVisitor visitor;
  auto encoder = LoadBalancerEncoder::Create(random_, &visitor, true, 8);
  ASSERT_TRUE(encoder.has_value());
  auto config = LoadBalancerConfig::Create(0, server_id_len, 4, kKey);
  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(
      encoder->UpdateConfig(*config, MakeServerId(kServerId, server_id_len)));
  EXPECT_EQ(visitor.num_adds(), 1u);
  LoadBalancerEncoderPeer::SetNumNoncesLeft(*encoder, 2);
  EXPECT_EQ(encoder->num_nonces_left(), 2);
  EXPECT_EQ(encoder->GenerateConnectionId(),
            QuicConnectionId({0x07, 0x29, 0xd8, 0xc2, 0x17, 0xce, 0x2d, 0x92}));
  EXPECT_EQ(encoder->num_nonces_left(), 1);
  encoder->GenerateConnectionId();
  EXPECT_EQ(encoder->IsEncoding(), false);
  // No retire_calls except for the initial UpdateConfig.
  EXPECT_EQ(visitor.num_deletes(), 1u);
}

TEST_F(LoadBalancerEncoderTest, UnroutableConnectionId) {
  random_.AddNextValues(0x83, kNonceHigh);
  auto encoder = LoadBalancerEncoder::Create(random_, nullptr, false);
  ASSERT_TRUE(encoder.has_value());
  EXPECT_EQ(encoder->num_nonces_left(), 0);
  auto connection_id = encoder->GenerateConnectionId();
  // The first byte is the config_id (0xe0) xored with (0x83 & 0x1f).
  // The remaining bytes are random, and therefore match kNonceHigh.
  QuicConnectionId expected({0xe3, 0x5d, 0x52, 0xde, 0x4d, 0xe3, 0xe7, 0x21});
  EXPECT_EQ(expected, connection_id);
}

TEST_F(LoadBalancerEncoderTest, NonDefaultUnroutableConnectionIdLength) {
  auto encoder = LoadBalancerEncoder::Create(random_, nullptr, true, 9);
  ASSERT_TRUE(encoder.has_value());
  QuicConnectionId connection_id = encoder->GenerateConnectionId();
  EXPECT_EQ(connection_id.length(), 9);
}

TEST_F(LoadBalancerEncoderTest, DeleteConfigWhenNoConfigExists) {
  TestLoadBalancerEncoderVisitor visitor;
  auto encoder = LoadBalancerEncoder::Create(random_, &visitor, true);
  ASSERT_TRUE(encoder.has_value());
  encoder->DeleteConfig();
  EXPECT_EQ(visitor.num_deletes(), 0u);
}

TEST_F(LoadBalancerEncoderTest, AddConfig) {
  auto config = LoadBalancerConfig::CreateUnencrypted(0, 3, 4);
  ASSERT_TRUE(config.has_value());
  TestLoadBalancerEncoderVisitor visitor;
  auto encoder = LoadBalancerEncoder::Create(random_, &visitor, true);
  EXPECT_TRUE(encoder->UpdateConfig(*config, MakeServerId(kServerId, 3)));
  EXPECT_EQ(visitor.num_adds(), 1u);
  absl::uint128 left = encoder->num_nonces_left();
  EXPECT_EQ(left, (0x1ull << 32));
  EXPECT_TRUE(encoder->IsEncoding());
  EXPECT_FALSE(encoder->IsEncrypted());
  encoder->GenerateConnectionId();
  EXPECT_EQ(encoder->num_nonces_left(), left - 1);
  EXPECT_EQ(visitor.num_deletes(), 0u);
}

TEST_F(LoadBalancerEncoderTest, UpdateConfig) {
  auto config = LoadBalancerConfig::CreateUnencrypted(0, 3, 4);
  ASSERT_TRUE(config.has_value());
  TestLoadBalancerEncoderVisitor visitor;
  auto encoder = LoadBalancerEncoder::Create(random_, &visitor, true);
  EXPECT_TRUE(encoder->UpdateConfig(*config, MakeServerId(kServerId, 3)));
  config = LoadBalancerConfig::Create(1, 4, 4, kKey);
  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(encoder->UpdateConfig(*config, MakeServerId(kServerId, 4)));
  EXPECT_EQ(visitor.num_adds(), 2u);
  EXPECT_EQ(visitor.num_deletes(), 1u);
  EXPECT_TRUE(encoder->IsEncoding());
  EXPECT_TRUE(encoder->IsEncrypted());
}

TEST_F(LoadBalancerEncoderTest, DeleteConfig) {
  auto config = LoadBalancerConfig::CreateUnencrypted(0, 3, 4);
  ASSERT_TRUE(config.has_value());
  TestLoadBalancerEncoderVisitor visitor;
  auto encoder = LoadBalancerEncoder::Create(random_, &visitor, true);
  EXPECT_TRUE(encoder->UpdateConfig(*config, MakeServerId(kServerId, 3)));
  encoder->DeleteConfig();
  EXPECT_EQ(visitor.num_adds(), 1u);
  EXPECT_EQ(visitor.num_deletes(), 1u);
  EXPECT_FALSE(encoder->IsEncoding());
  EXPECT_FALSE(encoder->IsEncrypted());
  EXPECT_EQ(encoder->num_nonces_left(), 0);
}

TEST_F(LoadBalancerEncoderTest, DeleteConfigNoVisitor) {
  auto config = LoadBalancerConfig::CreateUnencrypted(0, 3, 4);
  ASSERT_TRUE(config.has_value());
  auto encoder = LoadBalancerEncoder::Create(random_, nullptr, true);
  EXPECT_TRUE(encoder->UpdateConfig(*config, MakeServerId(kServerId, 3)));
  encoder->DeleteConfig();
  EXPECT_FALSE(encoder->IsEncoding());
  EXPECT_FALSE(encoder->IsEncrypted());
  EXPECT_EQ(encoder->num_nonces_left(), 0);
}

TEST_F(LoadBalancerEncoderTest, MaybeReplaceConnectionIdReturnsNoChange) {
  auto encoder = LoadBalancerEncoder::Create(random_, nullptr, false);
  ASSERT_TRUE(encoder.has_value());
  EXPECT_EQ(encoder->MaybeReplaceConnectionId(TestConnectionId(1),
                                              ParsedQuicVersion::Q046()),
            std::nullopt);
}

TEST_F(LoadBalancerEncoderTest, MaybeReplaceConnectionIdReturnsChange) {
  random_.AddNextValues(0x83, kNonceHigh);
  auto encoder = LoadBalancerEncoder::Create(random_, nullptr, false);
  ASSERT_TRUE(encoder.has_value());
  // The first byte is the config_id (0xc0) xored with (0x83 & 0x3f).
  // The remaining bytes are random, and therefore match kNonceHigh.
  QuicConnectionId expected({0xe3, 0x5d, 0x52, 0xde, 0x4d, 0xe3, 0xe7, 0x21});
  EXPECT_EQ(*encoder->MaybeReplaceConnectionId(TestConnectionId(1),
                                               ParsedQuicVersion::RFCv1()),
            expected);
}

TEST_F(LoadBalancerEncoderTest, GenerateNextConnectionIdReturnsNoChange) {
  auto config = LoadBalancerConfig::CreateUnencrypted(0, 3, 4);
  ASSERT_TRUE(config.has_value());
  auto encoder = LoadBalancerEncoder::Create(random_, nullptr, true);
  EXPECT_TRUE(encoder->UpdateConfig(*config, MakeServerId(kServerId, 3)));
  EXPECT_EQ(encoder->GenerateNextConnectionId(TestConnectionId(1)),
            std::nullopt);
}

TEST_F(LoadBalancerEncoderTest, GenerateNextConnectionIdReturnsChange) {
  random_.AddNextValues(0x83, kNonceHigh);
  auto encoder = LoadBalancerEncoder::Create(random_, nullptr, false);
  ASSERT_TRUE(encoder.has_value());
  // The first byte is the config_id (0xc0) xored with (0x83 & 0x3f).
  // The remaining bytes are random, and therefore match kNonceHigh.
  QuicConnectionId expected({0xe3, 0x5d, 0x52, 0xde, 0x4d, 0xe3, 0xe7, 0x21});
  EXPECT_EQ(*encoder->GenerateNextConnectionId(TestConnectionId(1)), expected);
}

TEST_F(LoadBalancerEncoderTest, ConnectionIdLengthsEncoded) {
  // The first byte literally encodes the length.
  auto len_encoder = LoadBalancerEncoder::Create(random_, nullptr, true);
  ASSERT_TRUE(len_encoder.has_value());
  EXPECT_EQ(len_encoder->ConnectionIdLength(0xe8), 9);
  EXPECT_EQ(len_encoder->ConnectionIdLength(0x4a), 11);
  EXPECT_EQ(len_encoder->ConnectionIdLength(0x09), 10);
  // The length is not self-encoded anymore.
  auto encoder = LoadBalancerEncoder::Create(random_, nullptr, false);
  ASSERT_TRUE(encoder.has_value());
  EXPECT_EQ(encoder->ConnectionIdLength(0xe8), kQuicDefaultConnectionIdLength);
  EXPECT_EQ(encoder->ConnectionIdLength(0x4a), kQuicDefaultConnectionIdLength);
  EXPECT_EQ(encoder->ConnectionIdLength(0x09), kQuicDefaultConnectionIdLength);
  // Add config ID 0, so that ID now returns a different length.
  uint8_t config_id = 0;
  uint8_t server_id_len = 3;
  uint8_t nonce_len = 6;
  uint8_t config_0_len = server_id_len + nonce_len + 1;
  auto config0 = LoadBalancerConfig::CreateUnencrypted(config_id, server_id_len,
                                                       nonce_len);
  ASSERT_TRUE(config0.has_value());
  EXPECT_TRUE(
      encoder->UpdateConfig(*config0, MakeServerId(kServerId, server_id_len)));
  EXPECT_EQ(encoder->ConnectionIdLength(0xe8), kQuicDefaultConnectionIdLength);
  EXPECT_EQ(encoder->ConnectionIdLength(0x4a), kQuicDefaultConnectionIdLength);
  EXPECT_EQ(encoder->ConnectionIdLength(0x09), config_0_len);
  // Replace config ID 0 with 1. There are probably still packets with config
  // ID 0 arriving, so keep that length in memory.
  config_id = 1;
  nonce_len++;
  uint8_t config_1_len = server_id_len + nonce_len + 1;
  auto config1 = LoadBalancerConfig::CreateUnencrypted(config_id, server_id_len,
                                                       nonce_len);
  ASSERT_TRUE(config1.has_value());
  // Old config length still there after replacement
  EXPECT_TRUE(
      encoder->UpdateConfig(*config1, MakeServerId(kServerId, server_id_len)));
  EXPECT_EQ(encoder->ConnectionIdLength(0xe8), kQuicDefaultConnectionIdLength);
  EXPECT_EQ(encoder->ConnectionIdLength(0x2a), config_1_len);
  EXPECT_EQ(encoder->ConnectionIdLength(0x09), config_0_len);
  // Old config length still there after delete
  encoder->DeleteConfig();
  EXPECT_EQ(encoder->ConnectionIdLength(0xe8), kQuicDefaultConnectionIdLength);
  EXPECT_EQ(encoder->ConnectionIdLength(0x2a), config_1_len);
  EXPECT_EQ(encoder->ConnectionIdLength(0x09), config_0_len);
}

}  // namespace

}  // namespace test

}  // namespace quic
