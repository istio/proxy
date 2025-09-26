// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/load_balancer/load_balancer_config.h"

#include <array>
#include <cstdint>
#include <cstring>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/quic/core/quic_connection_id.h"
#include "quiche/quic/load_balancer/load_balancer_server_id.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_test.h"

namespace quic {

namespace test {

class LoadBalancerConfigPeer {
 public:
  static bool InitializeFourPass(LoadBalancerConfig& config,
                                 const uint8_t* input, uint8_t* left,
                                 uint8_t* right, uint8_t* half_len) {
    return config.InitializeFourPass(input, left, right, half_len);
  }

  static void EncryptionPass(LoadBalancerConfig& config, uint8_t index,
                             uint8_t half_len, bool is_length_odd,
                             uint8_t* left, uint8_t* right) {
    config.EncryptionPass(index, half_len, is_length_odd, left, right);
  }
};

namespace {

constexpr char raw_key[] = {
    0xfd, 0xf7, 0x26, 0xa9, 0x89, 0x3e, 0xc0, 0x5c,
    0x06, 0x32, 0xd3, 0x95, 0x66, 0x80, 0xba, 0xf0,
};

class LoadBalancerConfigTest : public QuicTest {};

TEST_F(LoadBalancerConfigTest, InvalidParams) {
  // Bogus config_id.
  EXPECT_QUIC_BUG(
      EXPECT_FALSE(LoadBalancerConfig::CreateUnencrypted(7, 4, 10).has_value()),
      "Invalid LoadBalancerConfig Config ID 7 Server ID Length 4 "
      "Nonce Length 10");
  // Bad Server ID lengths.
  EXPECT_QUIC_BUG(EXPECT_FALSE(LoadBalancerConfig::Create(
                                   2, 0, 10, absl::string_view(raw_key, 16))
                                   .has_value()),
                  "Invalid LoadBalancerConfig Config ID 2 Server ID Length 0 "
                  "Nonce Length 10");
  EXPECT_QUIC_BUG(
      EXPECT_FALSE(LoadBalancerConfig::CreateUnencrypted(6, 16, 4).has_value()),
      "Invalid LoadBalancerConfig Config ID 6 Server ID Length 16 "
      "Nonce Length 4");
  // Bad Nonce lengths.
  EXPECT_QUIC_BUG(
      EXPECT_FALSE(LoadBalancerConfig::CreateUnencrypted(6, 4, 2).has_value()),
      "Invalid LoadBalancerConfig Config ID 6 Server ID Length 4 "
      "Nonce Length 2");
  EXPECT_QUIC_BUG(
      EXPECT_FALSE(LoadBalancerConfig::CreateUnencrypted(6, 1, 17).has_value()),
      "Invalid LoadBalancerConfig Config ID 6 Server ID Length 1 "
      "Nonce Length 17");
  // Bad key lengths.
  EXPECT_QUIC_BUG(
      EXPECT_FALSE(LoadBalancerConfig::Create(2, 3, 4, "").has_value()),
      "Invalid LoadBalancerConfig Key Length: 0");
  EXPECT_QUIC_BUG(EXPECT_FALSE(LoadBalancerConfig::Create(
                                   2, 3, 4, absl::string_view(raw_key, 10))
                                   .has_value()),
                  "Invalid LoadBalancerConfig Key Length: 10");
  EXPECT_QUIC_BUG(EXPECT_FALSE(LoadBalancerConfig::Create(
                                   0, 3, 4, absl::string_view(raw_key, 17))
                                   .has_value()),
                  "Invalid LoadBalancerConfig Key Length: 17");
}

TEST_F(LoadBalancerConfigTest, ValidParams) {
  // Test valid configurations and accessors
  auto config = LoadBalancerConfig::CreateUnencrypted(0, 3, 4);
  EXPECT_TRUE(config.has_value());
  EXPECT_EQ(config->config_id(), 0);
  EXPECT_EQ(config->server_id_len(), 3);
  EXPECT_EQ(config->nonce_len(), 4);
  EXPECT_EQ(config->plaintext_len(), 7);
  EXPECT_EQ(config->total_len(), 8);
  EXPECT_FALSE(config->IsEncrypted());
  auto config2 =
      LoadBalancerConfig::Create(2, 6, 7, absl::string_view(raw_key, 16));
  EXPECT_TRUE(config.has_value());
  EXPECT_EQ(config2->config_id(), 2);
  EXPECT_EQ(config2->server_id_len(), 6);
  EXPECT_EQ(config2->nonce_len(), 7);
  EXPECT_EQ(config2->plaintext_len(), 13);
  EXPECT_EQ(config2->total_len(), 14);
  EXPECT_TRUE(config2->IsEncrypted());
}

// Compare EncryptionPass() results to the example in
// draft-ietf-quic-load-balancers-19, Section 4.3.2.
TEST_F(LoadBalancerConfigTest, TestEncryptionPassExample) {
  auto config =
      LoadBalancerConfig::Create(0, 3, 4, absl::string_view(raw_key, 16));
  EXPECT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsEncrypted());
  uint8_t input[] = {0x07, 0x31, 0x44, 0x1a, 0x9c, 0x69, 0xc2, 0x75};
  std::array<uint8_t, kLoadBalancerBlockSize> left, right;
  uint8_t half_len;

  bool is_length_odd = LoadBalancerConfigPeer::InitializeFourPass(
      *config, input + 1, left.data(), right.data(), &half_len);
  EXPECT_TRUE(is_length_odd);
  std::array<std::array<uint8_t, kLoadBalancerBlockSize>,
             kNumLoadBalancerCryptoPasses + 1>
      expected_left = {{
          {0x31, 0x44, 0x1a, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x07, 0x00},
          {0x31, 0x44, 0x1a, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x07, 0x01},
          {0xd4, 0xa0, 0x48, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x07, 0x01},
          {0xd4, 0xa0, 0x48, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x07, 0x03},
          {0x67, 0x94, 0x7d, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x07, 0x03},
      }};
  std::array<std::array<uint8_t, kLoadBalancerBlockSize>,
             kNumLoadBalancerCryptoPasses + 1>
      expected_right = {{
          {0x0c, 0x69, 0xc2, 0x75, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x07, 0x00},
          {0x0e, 0x3c, 0x1f, 0xf9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x07, 0x00},
          {0x0e, 0x3c, 0x1f, 0xf9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x07, 0x02},
          {0x09, 0xbe, 0x05, 0x4a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x07, 0x02},
          {0x09, 0xbe, 0x05, 0x4a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x07, 0x04},
      }};

  EXPECT_EQ(left, expected_left[0]);
  EXPECT_EQ(right, expected_right[0]);
  for (int i = 1; i <= kNumLoadBalancerCryptoPasses; ++i) {
    LoadBalancerConfigPeer::EncryptionPass(*config, i, half_len, is_length_odd,
                                           left.data(), right.data());
    EXPECT_EQ(left, expected_left[i]);
    EXPECT_EQ(right, expected_right[i]);
  }
}

// Check that the encryption pass code can decode its own ciphertext. Various
// pointer errors could cause the code to overwrite bits that contain
// important information.
TEST_F(LoadBalancerConfigTest, EncryptionPassesAreReversible) {
  auto config =
      LoadBalancerConfig::Create(0, 3, 4, absl::string_view(raw_key, 16));
  std::array<uint8_t, kLoadBalancerBlockSize> start_left = {
      0x31, 0x44, 0x1a, 0x90, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00,
  };
  std::array<uint8_t, kLoadBalancerBlockSize> start_right = {
      0x0c, 0x69, 0xc2, 0x75, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00,
  };
  std::array<uint8_t, kLoadBalancerBlockSize> left = start_left,
                                              right = start_right;
  // Work left->right and right->left passes.
  LoadBalancerConfigPeer::EncryptionPass(*config, 1, 4, true, left.data(),
                                         right.data());
  LoadBalancerConfigPeer::EncryptionPass(*config, 2, 4, true, left.data(),
                                         right.data());
  LoadBalancerConfigPeer::EncryptionPass(*config, 2, 4, true, left.data(),
                                         right.data());
  LoadBalancerConfigPeer::EncryptionPass(*config, 1, 4, true, left.data(),
                                         right.data());
  // Since index is manually written into the second byte only on input, it is
  // not reversible.
  left[15] = 0;
  right[15] = 0;
  EXPECT_EQ(left, start_left);
  EXPECT_EQ(right, start_right);
}

// Tests for Encrypt() and Decrypt() are in LoadBalancerEncoderTest and
// LoadBalancerDecoderTest, respectively.

TEST_F(LoadBalancerConfigTest, InvalidBlockEncryption) {
  uint8_t pt[kLoadBalancerBlockSize + 1], ct[kLoadBalancerBlockSize];
  auto pt_config = LoadBalancerConfig::CreateUnencrypted(0, 8, 8);
  ASSERT_TRUE(pt_config.has_value());
  EXPECT_FALSE(pt_config->BlockEncrypt(pt, ct));
  EXPECT_FALSE(pt_config->BlockDecrypt(ct, pt));
  EXPECT_TRUE(pt_config->FourPassEncrypt(absl::Span<uint8_t>(pt, sizeof(pt)))
                  .IsEmpty());
  LoadBalancerServerId answer;
  EXPECT_FALSE(pt_config->FourPassDecrypt(
      absl::Span<uint8_t>(pt, sizeof(pt) - 1), answer));
  auto small_cid_config =
      LoadBalancerConfig::Create(0, 3, 4, absl::string_view(raw_key, 16));
  ASSERT_TRUE(small_cid_config.has_value());
  EXPECT_TRUE(small_cid_config->BlockEncrypt(pt, ct));
  EXPECT_FALSE(small_cid_config->BlockDecrypt(ct, pt));
  auto block_config =
      LoadBalancerConfig::Create(0, 8, 8, absl::string_view(raw_key, 16));
  ASSERT_TRUE(block_config.has_value());
  EXPECT_TRUE(block_config->BlockEncrypt(pt, ct));
  EXPECT_TRUE(block_config->BlockDecrypt(ct, pt));
}

// Block decrypt test from the Test Vector in
// draft-ietf-quic-load-balancers-19, Appendix B.
TEST_F(LoadBalancerConfigTest, BlockEncryptionExample) {
  const uint8_t ptext[] = {0xed, 0x79, 0x3a, 0x51, 0xd4, 0x9b, 0x8f, 0x5f,
                           0xee, 0x08, 0x0d, 0xbf, 0x48, 0xc0, 0xd1, 0xe5};
  const uint8_t ctext[] = {0x4d, 0xd2, 0xd0, 0x5a, 0x7b, 0x0d, 0xe9, 0xb2,
                           0xb9, 0x90, 0x7a, 0xfb, 0x5e, 0xcf, 0x8c, 0xc3};
  const char key[] = {0x8f, 0x95, 0xf0, 0x92, 0x45, 0x76, 0x5f, 0x80,
                      0x25, 0x69, 0x34, 0xe5, 0x0c, 0x66, 0x20, 0x7f};
  uint8_t result[sizeof(ptext)];
  auto config = LoadBalancerConfig::Create(0, 8, 8, absl::string_view(key, 16));
  EXPECT_TRUE(config->BlockEncrypt(ptext, result));
  EXPECT_EQ(memcmp(result, ctext, sizeof(ctext)), 0);
  EXPECT_TRUE(config->BlockDecrypt(ctext, result));
  EXPECT_EQ(memcmp(result, ptext, sizeof(ptext)), 0);
}

TEST_F(LoadBalancerConfigTest, ConfigIsCopyable) {
  const uint8_t ptext[] = {0xed, 0x79, 0x3a, 0x51, 0xd4, 0x9b, 0x8f, 0x5f,
                           0xee, 0x08, 0x0d, 0xbf, 0x48, 0xc0, 0xd1, 0xe5};
  const uint8_t ctext[] = {0x4d, 0xd2, 0xd0, 0x5a, 0x7b, 0x0d, 0xe9, 0xb2,
                           0xb9, 0x90, 0x7a, 0xfb, 0x5e, 0xcf, 0x8c, 0xc3};
  const char key[] = {0x8f, 0x95, 0xf0, 0x92, 0x45, 0x76, 0x5f, 0x80,
                      0x25, 0x69, 0x34, 0xe5, 0x0c, 0x66, 0x20, 0x7f};
  uint8_t result[sizeof(ptext)];
  auto config = LoadBalancerConfig::Create(0, 8, 8, absl::string_view(key, 16));
  auto config2 = config;
  EXPECT_TRUE(config->BlockEncrypt(ptext, result));
  EXPECT_EQ(memcmp(result, ctext, sizeof(ctext)), 0);
  EXPECT_TRUE(config2->BlockEncrypt(ptext, result));
  EXPECT_EQ(memcmp(result, ctext, sizeof(ctext)), 0);
}

TEST_F(LoadBalancerConfigTest, FourPassInputTooShort) {
  auto config =
      LoadBalancerConfig::Create(0, 3, 4, absl::string_view(raw_key, 16));
  uint8_t input[] = {0x0d, 0xd2, 0xd0, 0x5a, 0x7b, 0x0d, 0xe9};
  LoadBalancerServerId answer;
  bool decrypt_result;
  EXPECT_QUIC_BUG(
      decrypt_result = config->FourPassDecrypt(
          absl::Span<const uint8_t>(input, sizeof(input) - 1), answer),
      "Called FourPassDecrypt with a short Connection ID");
  EXPECT_FALSE(decrypt_result);
  QuicConnectionId encrypt_result;
  EXPECT_QUIC_BUG(encrypt_result = config->FourPassEncrypt(
                      absl::Span<uint8_t>(input, sizeof(input))),
                  "Called FourPassEncrypt with a short Connection ID");
  EXPECT_TRUE(encrypt_result.IsEmpty());
}

}  // namespace

}  // namespace test

}  // namespace quic
