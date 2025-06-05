// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_server_id.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "quiche/quic/platform/api/quic_test.h"

namespace quic::test {

namespace {

using ::testing::Optional;
using ::testing::Property;

class QuicServerIdTest : public QuicTest {};

TEST_F(QuicServerIdTest, Constructor) {
  QuicServerId google_server_id("google.com", 10);
  EXPECT_EQ("google.com", google_server_id.host());
  EXPECT_EQ(10, google_server_id.port());

  QuicServerId private_server_id("mail.google.com", 12);
  EXPECT_EQ("mail.google.com", private_server_id.host());
  EXPECT_EQ(12, private_server_id.port());
}

TEST_F(QuicServerIdTest, LessThan) {
  QuicServerId a_10_https("a.com", 10);
  QuicServerId a_11_https("a.com", 11);
  QuicServerId b_10_https("b.com", 10);
  QuicServerId b_11_https("b.com", 11);

  // Test combinations of host and port being same on left and right side of
  // less than.
  EXPECT_FALSE(a_10_https < a_10_https);
  EXPECT_TRUE(a_10_https < a_11_https);

  // Test with either host, port or https being different on left and right side
  // of less than.
  EXPECT_TRUE(a_10_https < a_11_https);
  EXPECT_TRUE(a_10_https < b_10_https);
  EXPECT_TRUE(a_10_https < b_11_https);
  EXPECT_FALSE(a_11_https < a_10_https);
  EXPECT_FALSE(a_11_https < b_10_https);
  EXPECT_TRUE(a_11_https < b_11_https);
  EXPECT_FALSE(b_10_https < a_10_https);
  EXPECT_TRUE(b_10_https < a_11_https);
  EXPECT_TRUE(b_10_https < b_11_https);
  EXPECT_FALSE(b_11_https < a_10_https);
  EXPECT_FALSE(b_11_https < a_11_https);
  EXPECT_FALSE(b_11_https < b_10_https);
}

TEST_F(QuicServerIdTest, Equals) {
  QuicServerId a_10_https("a.com", 10);
  QuicServerId a_11_https("a.com", 11);
  QuicServerId b_10_https("b.com", 10);
  QuicServerId b_11_https("b.com", 11);

  EXPECT_NE(a_10_https.cache_key(), a_11_https.cache_key());
  EXPECT_NE(a_10_https.cache_key(), b_10_https.cache_key());
  EXPECT_NE(a_10_https.cache_key(), b_11_https.cache_key());
  EXPECT_NE(a_10_https, a_11_https);
  EXPECT_NE(a_10_https, b_10_https);
  EXPECT_NE(a_10_https, b_11_https);

  QuicServerId new_a_10_https("a.com", 10);
  QuicServerId new_a_11_https("a.com", 11);
  QuicServerId new_b_10_https("b.com", 10);
  QuicServerId new_b_11_https("b.com", 11);

  EXPECT_EQ(new_a_10_https.cache_key(), a_10_https.cache_key());
  EXPECT_EQ(new_a_11_https.cache_key(), a_11_https.cache_key());
  EXPECT_EQ(new_b_10_https.cache_key(), b_10_https.cache_key());
  EXPECT_EQ(new_b_11_https.cache_key(), b_11_https.cache_key());
  EXPECT_EQ(new_a_10_https, a_10_https);
  EXPECT_EQ(new_a_11_https, a_11_https);
  EXPECT_EQ(new_b_10_https, b_10_https);
  EXPECT_EQ(new_b_11_https, b_11_https);
}

TEST_F(QuicServerIdTest, Parse) {
  std::optional<QuicServerId> server_id =
      QuicServerId::ParseFromHostPortString("host.test:500");

  EXPECT_THAT(server_id, Optional(Property(&QuicServerId::host, "host.test")));
  EXPECT_THAT(server_id, Optional(Property(&QuicServerId::port, 500)));
}

TEST_F(QuicServerIdTest, CannotParseMissingPort) {
  std::optional<QuicServerId> server_id =
      QuicServerId::ParseFromHostPortString("host.test");

  EXPECT_EQ(server_id, std::nullopt);
}

TEST_F(QuicServerIdTest, CannotParseEmptyPort) {
  std::optional<QuicServerId> server_id =
      QuicServerId::ParseFromHostPortString("host.test:");

  EXPECT_EQ(server_id, std::nullopt);
}

TEST_F(QuicServerIdTest, CannotParseEmptyHost) {
  std::optional<QuicServerId> server_id =
      QuicServerId::ParseFromHostPortString(":500");

  EXPECT_EQ(server_id, std::nullopt);
}

TEST_F(QuicServerIdTest, CannotParseUserInfo) {
  std::optional<QuicServerId> server_id =
      QuicServerId::ParseFromHostPortString("userinfo@host.test:500");

  EXPECT_EQ(server_id, std::nullopt);
}

TEST_F(QuicServerIdTest, ParseIpv6Literal) {
  std::optional<QuicServerId> server_id =
      QuicServerId::ParseFromHostPortString("[::1]:400");

  EXPECT_THAT(server_id, Optional(Property(&QuicServerId::host, "[::1]")));
  EXPECT_THAT(server_id, Optional(Property(&QuicServerId::port, 400)));
}

TEST_F(QuicServerIdTest, ParseUnbracketedIpv6Literal) {
  std::optional<QuicServerId> server_id =
      QuicServerId::ParseFromHostPortString("::1:400");

  EXPECT_THAT(server_id, Optional(Property(&QuicServerId::host, "::1")));
  EXPECT_THAT(server_id, Optional(Property(&QuicServerId::port, 400)));
}

TEST_F(QuicServerIdTest, AddBracketsToIpv6) {
  QuicServerId server_id("::1", 100);

  EXPECT_EQ(server_id.GetHostWithIpv6Brackets(), "[::1]");
  EXPECT_EQ(server_id.ToHostPortString(), "[::1]:100");
}

TEST_F(QuicServerIdTest, AddBracketsAlreadyIncluded) {
  QuicServerId server_id("[::1]", 100);

  EXPECT_EQ(server_id.GetHostWithIpv6Brackets(), "[::1]");
  EXPECT_EQ(server_id.ToHostPortString(), "[::1]:100");
}

TEST_F(QuicServerIdTest, AddBracketsNotAddedToNonIpv6) {
  QuicServerId server_id("host.test", 100);

  EXPECT_EQ(server_id.GetHostWithIpv6Brackets(), "host.test");
  EXPECT_EQ(server_id.ToHostPortString(), "host.test:100");
}

TEST_F(QuicServerIdTest, RemoveBracketsFromIpv6) {
  QuicServerId server_id("[::1]", 100);

  EXPECT_EQ(server_id.GetHostWithoutIpv6Brackets(), "::1");
}

TEST_F(QuicServerIdTest, RemoveBracketsNotIncluded) {
  QuicServerId server_id("::1", 100);

  EXPECT_EQ(server_id.GetHostWithoutIpv6Brackets(), "::1");
}

TEST_F(QuicServerIdTest, RemoveBracketsFromNonIpv6) {
  QuicServerId server_id("host.test", 100);

  EXPECT_EQ(server_id.GetHostWithoutIpv6Brackets(), "host.test");
}

TEST_F(QuicServerIdTest, CacheKeyEquals) {
  QuicServerId a_10_https("a.com", 10, "key1");
  QuicServerId a_11_https("a.com", 11, "key1");
  QuicServerId b_10_https("b.com", 10, "key2");
  QuicServerId b_11_https("b.com", 11, "key2");

  EXPECT_EQ(a_10_https.cache_key(), a_10_https.cache_key());
  EXPECT_EQ(a_10_https.cache_key(), a_11_https.cache_key());
  EXPECT_EQ(b_10_https.cache_key(), b_11_https.cache_key());
  EXPECT_NE(a_10_https.cache_key(), b_10_https.cache_key());
  EXPECT_NE(a_10_https.cache_key(), b_11_https.cache_key());
  EXPECT_NE(a_11_https.cache_key(), b_10_https.cache_key());
  EXPECT_NE(a_11_https.cache_key(), b_11_https.cache_key());

  EXPECT_EQ(a_10_https, a_10_https);
  EXPECT_NE(a_10_https, a_11_https);
  EXPECT_NE(b_10_https, b_11_https);
  EXPECT_NE(a_10_https, b_10_https);
  EXPECT_NE(a_10_https, b_11_https);
  EXPECT_NE(a_11_https, b_10_https);
  EXPECT_NE(a_11_https, b_11_https);

  EXPECT_EQ(QuicServerIdHash()(a_10_https), QuicServerIdHash()(a_10_https));
  EXPECT_NE(QuicServerIdHash()(a_10_https), QuicServerIdHash()(a_11_https));
  EXPECT_NE(QuicServerIdHash()(b_10_https), QuicServerIdHash()(b_11_https));
}

TEST_F(QuicServerIdTest, CacheKeyLessThan) {
  std::vector<QuicServerId> server_ids;
  for (uint16_t port : {10, 11}) {
    for (std::string host : {"a.com", "b.com"}) {
      for (std::string cache_key : {"key1", "key2"}) {
        server_ids.push_back(QuicServerId(host, port, cache_key));
      }
    }
  }
  for (size_t i = 0; i < server_ids.size() - 1; ++i) {
    EXPECT_LT(server_ids[i], server_ids[i + 1]);
  }
}

}  // namespace

}  // namespace quic::test
