// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/load_balancer/load_balancer_decoder.h"

#include <cstdint>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/quic/core/quic_connection_id.h"
#include "quiche/quic/load_balancer/load_balancer_config.h"
#include "quiche/quic/load_balancer/load_balancer_server_id.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_test.h"

namespace quic {

namespace test {

namespace {

class LoadBalancerDecoderTest : public QuicTest {};

// Convenience function to shorten the code. Does not check if |array| is long
// enough or |length| is valid for a server ID.
inline LoadBalancerServerId MakeServerId(const uint8_t array[],
                                         const uint8_t length) {
  return LoadBalancerServerId(absl::Span<const uint8_t>(array, length));
}

constexpr char kRawKey[] = {0x8f, 0x95, 0xf0, 0x92, 0x45, 0x76, 0x5f, 0x80,
                            0x25, 0x69, 0x34, 0xe5, 0x0c, 0x66, 0x20, 0x7f};
constexpr absl::string_view kKey(kRawKey, kLoadBalancerKeyLen);
constexpr uint8_t kServerId[] = {0xed, 0x79, 0x3a, 0x51, 0xd4, 0x9b, 0x8f, 0x5f,
                                 0xab, 0x65, 0xba, 0x04, 0xc3, 0x33, 0x0a};

struct LoadBalancerDecoderTestCase {
  LoadBalancerConfig config;
  QuicConnectionId connection_id;
  LoadBalancerServerId server_id;
};

TEST_F(LoadBalancerDecoderTest, UnencryptedConnectionIdTestVectors) {
  const struct LoadBalancerDecoderTestCase test_vectors[2] = {
      {
          *LoadBalancerConfig::CreateUnencrypted(0, 3, 4),
          QuicConnectionId({0x07, 0xed, 0x79, 0x3a, 0x80, 0x49, 0x71, 0x8a}),
          MakeServerId(kServerId, 3),
      },
      {
          *LoadBalancerConfig::CreateUnencrypted(1, 8, 5),
          QuicConnectionId({0x2d, 0xed, 0x79, 0x3a, 0x51, 0xd4, 0x9b, 0x8f,
                            0x5f, 0xee, 0x15, 0xda, 0x27, 0xc4}),
          MakeServerId(kServerId, 8),
      }};
  for (const auto& test : test_vectors) {
    LoadBalancerDecoder decoder;
    LoadBalancerServerId answer;
    EXPECT_TRUE(decoder.AddConfig(test.config));
    EXPECT_TRUE(decoder.GetServerId(test.connection_id, answer));
    EXPECT_EQ(answer, test.server_id);
  }
}

// Compare test vectors from Appendix B of draft-ietf-quic-load-balancers-19.
TEST_F(LoadBalancerDecoderTest, DecoderTestVectors) {
  // Try (1) the "standard" CID length of 8
  // (2) server_id_len > nonce_len, so there is a fourth decryption pass
  // (3) the single-pass encryption case
  // (4) An even total length.
  const struct LoadBalancerDecoderTestCase test_vectors[4] = {
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
  for (const auto& test : test_vectors) {
    LoadBalancerDecoder decoder;
    EXPECT_TRUE(decoder.AddConfig(test.config));
    LoadBalancerServerId answer;
    EXPECT_TRUE(decoder.GetServerId(test.connection_id, answer));
    EXPECT_EQ(answer, test.server_id);
  }
}

TEST_F(LoadBalancerDecoderTest, InvalidConfigId) {
  LoadBalancerServerId server_id({0x01, 0x02, 0x03});
  EXPECT_TRUE(server_id.IsValid());
  LoadBalancerDecoder decoder;
  EXPECT_TRUE(
      decoder.AddConfig(*LoadBalancerConfig::CreateUnencrypted(1, 3, 4)));
  QuicConnectionId wrong_config_id(
      {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07});
  LoadBalancerServerId answer;
  EXPECT_FALSE(decoder.GetServerId(
      QuicConnectionId({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}),
      answer));
}

TEST_F(LoadBalancerDecoderTest, UnroutableCodepoint) {
  LoadBalancerServerId server_id({0x01, 0x02, 0x03});
  EXPECT_TRUE(server_id.IsValid());
  LoadBalancerDecoder decoder;
  EXPECT_TRUE(
      decoder.AddConfig(*LoadBalancerConfig::CreateUnencrypted(1, 3, 4)));
  LoadBalancerServerId answer;
  EXPECT_FALSE(decoder.GetServerId(
      QuicConnectionId({0xe0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}),
      answer));
}

TEST_F(LoadBalancerDecoderTest, UnroutableCodepointAnyLength) {
  LoadBalancerServerId server_id({0x01, 0x02, 0x03});
  EXPECT_TRUE(server_id.IsValid());
  LoadBalancerDecoder decoder;
  EXPECT_TRUE(
      decoder.AddConfig(*LoadBalancerConfig::CreateUnencrypted(1, 3, 4)));
  LoadBalancerServerId answer;
  EXPECT_FALSE(decoder.GetServerId(QuicConnectionId({0xff}), answer));
}

TEST_F(LoadBalancerDecoderTest, ConnectionIdTooShort) {
  LoadBalancerServerId server_id({0x01, 0x02, 0x03});
  EXPECT_TRUE(server_id.IsValid());
  LoadBalancerDecoder decoder;
  EXPECT_TRUE(
      decoder.AddConfig(*LoadBalancerConfig::CreateUnencrypted(0, 3, 4)));
  LoadBalancerServerId answer;
  EXPECT_FALSE(decoder.GetServerId(
      QuicConnectionId({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06}), answer));
}

TEST_F(LoadBalancerDecoderTest, ConnectionIdTooLongIsOK) {
  LoadBalancerServerId server_id({0x01, 0x02, 0x03});
  LoadBalancerDecoder decoder;
  EXPECT_TRUE(
      decoder.AddConfig(*LoadBalancerConfig::CreateUnencrypted(0, 3, 4)));
  LoadBalancerServerId answer;
  EXPECT_TRUE(decoder.GetServerId(
      QuicConnectionId({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}),
      answer));
  EXPECT_EQ(answer, server_id);
}

TEST_F(LoadBalancerDecoderTest, DeleteConfigBadId) {
  LoadBalancerDecoder decoder;
  decoder.AddConfig(*LoadBalancerConfig::CreateUnencrypted(2, 3, 4));
  decoder.DeleteConfig(0);
  EXPECT_QUIC_BUG(decoder.DeleteConfig(7),
                  "Decoder deleting config with invalid config_id 7");
  LoadBalancerServerId answer;
  EXPECT_TRUE(decoder.GetServerId(
      QuicConnectionId({0x40, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}),
      answer));
}

TEST_F(LoadBalancerDecoderTest, DeleteConfigGoodId) {
  LoadBalancerDecoder decoder;
  decoder.AddConfig(*LoadBalancerConfig::CreateUnencrypted(2, 3, 4));
  decoder.DeleteConfig(2);
  LoadBalancerServerId answer;
  EXPECT_FALSE(decoder.GetServerId(
      QuicConnectionId({0x40, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}),
      answer));
}

// Create two server IDs and make sure the decoder decodes the correct one.
TEST_F(LoadBalancerDecoderTest, TwoServerIds) {
  LoadBalancerServerId server_id1({0x01, 0x02, 0x03});
  EXPECT_TRUE(server_id1.IsValid());
  LoadBalancerServerId server_id2({0x04, 0x05, 0x06});
  LoadBalancerDecoder decoder;
  EXPECT_TRUE(
      decoder.AddConfig(*LoadBalancerConfig::CreateUnencrypted(0, 3, 4)));
  LoadBalancerServerId answer;
  EXPECT_TRUE(decoder.GetServerId(
      QuicConnectionId({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}),
      answer));
  EXPECT_EQ(answer, server_id1);
  EXPECT_TRUE(decoder.GetServerId(
      QuicConnectionId({0x00, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a}),
      answer));
  EXPECT_EQ(answer, server_id2);
}

TEST_F(LoadBalancerDecoderTest, GetConfigId) {
  EXPECT_FALSE(
      LoadBalancerDecoder::GetConfigId(QuicConnectionId()).has_value());
  for (uint8_t i = 0; i < kNumLoadBalancerConfigs; i++) {
    const QuicConnectionId connection_id(
        {static_cast<unsigned char>(i << kConnectionIdLengthBits)});
    auto config_id = LoadBalancerDecoder::GetConfigId(connection_id);
    EXPECT_EQ(config_id,
              LoadBalancerDecoder::GetConfigId(connection_id.data()[0]));
    EXPECT_TRUE(config_id.has_value());
    EXPECT_EQ(*config_id, i);
  }
  EXPECT_FALSE(
      LoadBalancerDecoder::GetConfigId(QuicConnectionId({0xe0})).has_value());
}

TEST_F(LoadBalancerDecoderTest, GetConfig) {
  LoadBalancerDecoder decoder;
  decoder.AddConfig(*LoadBalancerConfig::CreateUnencrypted(2, 3, 4));

  EXPECT_EQ(decoder.GetConfig(0), nullptr);
  EXPECT_EQ(decoder.GetConfig(1), nullptr);
  EXPECT_EQ(decoder.GetConfig(3), nullptr);
  EXPECT_EQ(decoder.GetConfig(4), nullptr);

  const LoadBalancerConfig* config = decoder.GetConfig(2);
  ASSERT_NE(config, nullptr);
  EXPECT_EQ(config->server_id_len(), 3);
  EXPECT_EQ(config->nonce_len(), 4);
  EXPECT_FALSE(config->IsEncrypted());
}

TEST_F(LoadBalancerDecoderTest, OnePassIgnoreAdditionalBytes) {
  uint8_t ptext[] = {0x00, 0xed, 0x79, 0x3a, 0x51, 0xd4, 0x9b, 0x8f, 0x5f, 0xee,
                     0x08, 0x0d, 0xbf, 0x48, 0xc0, 0xd1, 0xe5, 0xda, 0x41};
  uint8_t ctext[] = {0x00, 0x4d, 0xd2, 0xd0, 0x5a, 0x7b, 0x0d, 0xe9, 0xb2, 0xb9,
                     0x90, 0x7a, 0xfb, 0x5e, 0xcf, 0x8c, 0xc3, 0xda, 0x41};
  LoadBalancerDecoder decoder;
  decoder.AddConfig(
      *LoadBalancerConfig::Create(0, 8, 8, absl::string_view(kRawKey, 16)));
  LoadBalancerServerId original_server_id(absl::Span<uint8_t>(&ptext[1], 8));
  QuicConnectionId cid(absl::Span<uint8_t>(ctext, sizeof(ctext)));
  LoadBalancerServerId answer;
  EXPECT_TRUE(decoder.GetServerId(cid, answer));
  EXPECT_EQ(answer, original_server_id);
}

}  // namespace

}  // namespace test

}  // namespace quic
