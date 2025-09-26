// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/load_balancer/load_balancer_server_id.h"

#include <cstdint>
#include <cstring>

#include "absl/hash/hash_testing.h"
#include "absl/types/span.h"

#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_test.h"

namespace quic {

namespace test {

namespace {

class LoadBalancerServerIdTest : public QuicTest {};

constexpr uint8_t kRawServerId[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                    0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                                    0x0c, 0x0d, 0x0e, 0x0f};

TEST_F(LoadBalancerServerIdTest, CreateReturnsNullIfTooLong) {
  EXPECT_QUIC_BUG(EXPECT_FALSE(LoadBalancerServerId(
                                   absl::Span<const uint8_t>(kRawServerId, 16))
                                   .IsValid()),
                  "Attempted to create LoadBalancerServerId with length 16");
  EXPECT_QUIC_BUG(
      EXPECT_FALSE(LoadBalancerServerId(absl::Span<const uint8_t>()).IsValid()),
      "Attempted to create LoadBalancerServerId with length 0");
}

TEST_F(LoadBalancerServerIdTest, CompareIdenticalExceptLength) {
  LoadBalancerServerId server_id(absl::Span<const uint8_t>(kRawServerId, 15));
  ASSERT_TRUE(server_id.IsValid());
  EXPECT_EQ(server_id.length(), 15);
  LoadBalancerServerId shorter_server_id(
      absl::Span<const uint8_t>(kRawServerId, 5));
  ASSERT_TRUE(shorter_server_id.IsValid());
  EXPECT_EQ(shorter_server_id.length(), 5);
  // Shorter comes before longer if all bits match
  EXPECT_TRUE(shorter_server_id < server_id);
  EXPECT_FALSE(server_id < shorter_server_id);
  // Different lengths are never equal.
  EXPECT_FALSE(shorter_server_id == server_id);
}

TEST_F(LoadBalancerServerIdTest, AccessorFunctions) {
  LoadBalancerServerId server_id(absl::Span<const uint8_t>(kRawServerId, 5));
  EXPECT_TRUE(server_id.IsValid());
  EXPECT_EQ(server_id.length(), 5);
  EXPECT_EQ(memcmp(server_id.data().data(), kRawServerId, 5), 0);
  EXPECT_EQ(server_id.ToString(), "0001020304");
}

TEST_F(LoadBalancerServerIdTest, CompareDifferentServerIds) {
  LoadBalancerServerId server_id(absl::Span<const uint8_t>(kRawServerId, 5));
  ASSERT_TRUE(server_id.IsValid());
  LoadBalancerServerId reverse({0x0f, 0x0e, 0x0d, 0x0c, 0x0b});
  ASSERT_TRUE(reverse.IsValid());
  EXPECT_TRUE(server_id < reverse);
  LoadBalancerServerId long_server_id(
      absl::Span<const uint8_t>(kRawServerId, 15));
  EXPECT_TRUE(long_server_id < reverse);
}

TEST_F(LoadBalancerServerIdTest, EqualityOperators) {
  LoadBalancerServerId server_id(absl::Span<const uint8_t>(kRawServerId, 15));
  ASSERT_TRUE(server_id.IsValid());
  LoadBalancerServerId shorter_server_id(
      absl::Span<const uint8_t>(kRawServerId, 5));
  ASSERT_TRUE(shorter_server_id.IsValid());
  EXPECT_FALSE(server_id == shorter_server_id);
  LoadBalancerServerId server_id2 = server_id;
  EXPECT_TRUE(server_id == server_id2);
}

TEST_F(LoadBalancerServerIdTest, SupportsHash) {
  LoadBalancerServerId server_id(absl::Span<const uint8_t>(kRawServerId, 15));
  ASSERT_TRUE(server_id.IsValid());
  LoadBalancerServerId shorter_server_id(
      absl::Span<const uint8_t>(kRawServerId, 5));
  ASSERT_TRUE(shorter_server_id.IsValid());
  LoadBalancerServerId different_server_id({0x0f, 0x0e, 0x0d, 0x0c, 0x0b});
  ASSERT_TRUE(different_server_id.IsValid());
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      server_id,
      shorter_server_id,
      different_server_id,
  }));
}

TEST_F(LoadBalancerServerIdTest, SetLengthInvalid) {
  LoadBalancerServerId server_id;
  EXPECT_QUIC_BUG(server_id.set_length(16),
                  "Attempted to set LoadBalancerServerId length to 16");
  EXPECT_QUIC_BUG(server_id.set_length(0),
                  "Attempted to set LoadBalancerServerId length to 0");
  server_id.set_length(1);
  EXPECT_EQ(server_id.length(), 1);
  server_id.set_length(15);
  EXPECT_EQ(server_id.length(), 15);
}

}  // namespace

}  // namespace test

}  // namespace quic
