// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_connection_id.h"

#include <cstdint>
#include <cstring>

#include "absl/types/span.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

namespace quic::test {

namespace {

class QuicConnectionIdTest : public QuicTest {};

TEST_F(QuicConnectionIdTest, Empty) {
  QuicConnectionId connection_id_empty = EmptyQuicConnectionId();
  EXPECT_TRUE(connection_id_empty.IsEmpty());
}

TEST_F(QuicConnectionIdTest, DefaultIsEmpty) {
  QuicConnectionId connection_id_empty = QuicConnectionId();
  EXPECT_TRUE(connection_id_empty.IsEmpty());
}

TEST_F(QuicConnectionIdTest, NotEmpty) {
  QuicConnectionId connection_id = test::TestConnectionId(1);
  EXPECT_FALSE(connection_id.IsEmpty());
}

TEST_F(QuicConnectionIdTest, ZeroIsNotEmpty) {
  QuicConnectionId connection_id = test::TestConnectionId(0);
  EXPECT_FALSE(connection_id.IsEmpty());
}

TEST_F(QuicConnectionIdTest, Data) {
  char connection_id_data[kQuicDefaultConnectionIdLength];
  memset(connection_id_data, 0x42, sizeof(connection_id_data));
  QuicConnectionId connection_id1 =
      QuicConnectionId(connection_id_data, sizeof(connection_id_data));
  QuicConnectionId connection_id2 =
      QuicConnectionId(connection_id_data, sizeof(connection_id_data));
  EXPECT_EQ(connection_id1, connection_id2);
  EXPECT_EQ(connection_id1.length(), kQuicDefaultConnectionIdLength);
  EXPECT_EQ(connection_id1.data(), connection_id1.mutable_data());
  EXPECT_EQ(0, memcmp(connection_id1.data(), connection_id2.data(),
                      sizeof(connection_id_data)));
  EXPECT_EQ(0, memcmp(connection_id1.data(), connection_id_data,
                      sizeof(connection_id_data)));
  connection_id2.mutable_data()[0] = 0x33;
  EXPECT_NE(connection_id1, connection_id2);
  static const uint8_t kNewLength = 4;
  connection_id2.set_length(kNewLength);
  EXPECT_EQ(kNewLength, connection_id2.length());
}

TEST_F(QuicConnectionIdTest, SpanData) {
  QuicConnectionId connection_id = QuicConnectionId({0x01, 0x02, 0x03});
  EXPECT_EQ(connection_id.length(), 3);
  QuicConnectionId empty_connection_id =
      QuicConnectionId(absl::Span<uint8_t>());
  EXPECT_EQ(empty_connection_id.length(), 0);
  QuicConnectionId connection_id2 = QuicConnectionId({
      0x01,
      0x02,
      0x03,
      0x04,
      0x05,
      0x06,
      0x07,
      0x08,
      0x09,
      0x0a,
      0x0b,
      0x0c,
      0x0d,
      0x0e,
      0x0f,
      0x10,
  });
  EXPECT_EQ(connection_id2.length(), 16);
}

TEST_F(QuicConnectionIdTest, StringData) {
  QuicConnectionId connection_id = QuicConnectionId("foobar");
  EXPECT_EQ(connection_id.length(), 6);
  EXPECT_EQ(connection_id.ToString(), "666f6f626172");
  EXPECT_EQ(connection_id.ToStringView(), "foobar");
  absl::string_view null_sv(nullptr, 0);
  QuicConnectionId null_connection_id(null_sv);
  EXPECT_EQ(null_connection_id.length(), 0);
  absl::string_view empty_sv = "";
  QuicConnectionId empty_connection_id(empty_sv);
  EXPECT_EQ(empty_connection_id.length(), 0);
}

TEST_F(QuicConnectionIdTest, DoubleConvert) {
  QuicConnectionId connection_id64_1 = test::TestConnectionId(1);
  QuicConnectionId connection_id64_2 = test::TestConnectionId(42);
  QuicConnectionId connection_id64_3 =
      test::TestConnectionId(UINT64_C(0xfedcba9876543210));
  EXPECT_EQ(connection_id64_1,
            test::TestConnectionId(
                test::TestConnectionIdToUInt64(connection_id64_1)));
  EXPECT_EQ(connection_id64_2,
            test::TestConnectionId(
                test::TestConnectionIdToUInt64(connection_id64_2)));
  EXPECT_EQ(connection_id64_3,
            test::TestConnectionId(
                test::TestConnectionIdToUInt64(connection_id64_3)));
  EXPECT_NE(connection_id64_1, connection_id64_2);
  EXPECT_NE(connection_id64_1, connection_id64_3);
  EXPECT_NE(connection_id64_2, connection_id64_3);
}

TEST_F(QuicConnectionIdTest, Hash) {
  QuicConnectionId connection_id64_1 = test::TestConnectionId(1);
  QuicConnectionId connection_id64_1b = test::TestConnectionId(1);
  QuicConnectionId connection_id64_2 = test::TestConnectionId(42);
  QuicConnectionId connection_id64_3 =
      test::TestConnectionId(UINT64_C(0xfedcba9876543210));
  EXPECT_EQ(connection_id64_1.Hash(), connection_id64_1b.Hash());
  EXPECT_NE(connection_id64_1.Hash(), connection_id64_2.Hash());
  EXPECT_NE(connection_id64_1.Hash(), connection_id64_3.Hash());
  EXPECT_NE(connection_id64_2.Hash(), connection_id64_3.Hash());

  // Verify that any two all-zero connection IDs of different lengths never
  // have the same hash.
  const char connection_id_bytes[255] = {};
  for (uint8_t i = 0; i < sizeof(connection_id_bytes) - 1; ++i) {
    QuicConnectionId connection_id_i(connection_id_bytes, i);
    for (uint8_t j = i + 1; j < sizeof(connection_id_bytes); ++j) {
      QuicConnectionId connection_id_j(connection_id_bytes, j);
      EXPECT_NE(connection_id_i.Hash(), connection_id_j.Hash());
    }
  }
}

TEST_F(QuicConnectionIdTest, AssignAndCopy) {
  QuicConnectionId connection_id = test::TestConnectionId(1);
  QuicConnectionId connection_id2 = test::TestConnectionId(2);
  connection_id = connection_id2;
  EXPECT_EQ(connection_id, test::TestConnectionId(2));
  EXPECT_NE(connection_id, test::TestConnectionId(1));
  connection_id = QuicConnectionId(test::TestConnectionId(1));
  EXPECT_EQ(connection_id, test::TestConnectionId(1));
  EXPECT_NE(connection_id, test::TestConnectionId(2));
}

TEST_F(QuicConnectionIdTest, ChangeLength) {
  QuicConnectionId connection_id64_1 = test::TestConnectionId(1);
  QuicConnectionId connection_id64_2 = test::TestConnectionId(2);
  QuicConnectionId connection_id200_2 = test::TestConnectionId(2);
  connection_id200_2.set_length(25);
  memset(connection_id200_2.mutable_data() + 8, 0, 17);
  char connection_id200_2_bytes[25] = {0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  QuicConnectionId connection_id200_2b(connection_id200_2_bytes,
                                       sizeof(connection_id200_2_bytes));
  EXPECT_EQ(connection_id200_2, connection_id200_2b);
  QuicConnectionId connection_id = connection_id64_1;
  connection_id.set_length(25);
  EXPECT_NE(connection_id64_1, connection_id);
  // Check resizing big to small.
  connection_id.set_length(8);
  EXPECT_EQ(connection_id64_1, connection_id);
  // Check resizing small to big.
  connection_id.set_length(25);
  memset(connection_id.mutable_data(), 0, connection_id.length());
  memcpy(connection_id.mutable_data(), connection_id64_2.data(),
         connection_id64_2.length());
  EXPECT_EQ(connection_id200_2, connection_id);
  EXPECT_EQ(connection_id200_2b, connection_id);
  QuicConnectionId connection_id192(connection_id200_2_bytes, 24);
  connection_id.set_length(24);
  EXPECT_EQ(connection_id192, connection_id);
  // Check resizing big to big.
  QuicConnectionId connection_id2 = connection_id192;
  connection_id2.set_length(25);
  connection_id2.mutable_data()[24] = 0;
  EXPECT_EQ(connection_id200_2, connection_id2);
  EXPECT_EQ(connection_id200_2b, connection_id2);
}

TEST_F(QuicConnectionIdTest, MaximumLength) {
  char bytes[255];
  memset(bytes, 0xa, sizeof(bytes));
  QuicConnectionId max_length(bytes, sizeof(bytes));
  EXPECT_EQ(memcmp(max_length.data(), bytes, sizeof(bytes)), 0);
}

}  // namespace

}  // namespace quic::test
