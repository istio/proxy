// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_utils.h"

#include <string>
#include <vector>

#include "absl/base/macros.h"
#include "absl/numeric/int128.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_connection_id.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

namespace quic {
namespace test {
namespace {

class QuicUtilsTest : public QuicTest {};

TEST_F(QuicUtilsTest, DetermineAddressChangeType) {
  const std::string kIPv4String1 = "1.2.3.4";
  const std::string kIPv4String2 = "1.2.3.5";
  const std::string kIPv4String3 = "1.1.3.5";
  const std::string kIPv6String1 = "2001:700:300:1800::f";
  const std::string kIPv6String2 = "2001:700:300:1800:1:1:1:f";
  QuicSocketAddress old_address;
  QuicSocketAddress new_address;
  QuicIpAddress address;

  EXPECT_EQ(NO_CHANGE,
            QuicUtils::DetermineAddressChangeType(old_address, new_address));
  ASSERT_TRUE(address.FromString(kIPv4String1));
  old_address = QuicSocketAddress(address, 1234);
  EXPECT_EQ(NO_CHANGE,
            QuicUtils::DetermineAddressChangeType(old_address, new_address));
  new_address = QuicSocketAddress(address, 1234);
  EXPECT_EQ(NO_CHANGE,
            QuicUtils::DetermineAddressChangeType(old_address, new_address));

  new_address = QuicSocketAddress(address, 5678);
  EXPECT_EQ(PORT_CHANGE,
            QuicUtils::DetermineAddressChangeType(old_address, new_address));
  ASSERT_TRUE(address.FromString(kIPv6String1));
  old_address = QuicSocketAddress(address, 1234);
  new_address = QuicSocketAddress(address, 5678);
  EXPECT_EQ(PORT_CHANGE,
            QuicUtils::DetermineAddressChangeType(old_address, new_address));

  ASSERT_TRUE(address.FromString(kIPv4String1));
  old_address = QuicSocketAddress(address, 1234);
  ASSERT_TRUE(address.FromString(kIPv6String1));
  new_address = QuicSocketAddress(address, 1234);
  EXPECT_EQ(IPV4_TO_IPV6_CHANGE,
            QuicUtils::DetermineAddressChangeType(old_address, new_address));

  old_address = QuicSocketAddress(address, 1234);
  ASSERT_TRUE(address.FromString(kIPv4String1));
  new_address = QuicSocketAddress(address, 1234);
  EXPECT_EQ(IPV6_TO_IPV4_CHANGE,
            QuicUtils::DetermineAddressChangeType(old_address, new_address));

  ASSERT_TRUE(address.FromString(kIPv6String2));
  new_address = QuicSocketAddress(address, 1234);
  EXPECT_EQ(IPV6_TO_IPV6_CHANGE,
            QuicUtils::DetermineAddressChangeType(old_address, new_address));

  ASSERT_TRUE(address.FromString(kIPv4String1));
  old_address = QuicSocketAddress(address, 1234);
  ASSERT_TRUE(address.FromString(kIPv4String2));
  new_address = QuicSocketAddress(address, 1234);
  EXPECT_EQ(IPV4_SUBNET_CHANGE,
            QuicUtils::DetermineAddressChangeType(old_address, new_address));
  ASSERT_TRUE(address.FromString(kIPv4String3));
  new_address = QuicSocketAddress(address, 1234);
  EXPECT_EQ(IPV4_TO_IPV4_CHANGE,
            QuicUtils::DetermineAddressChangeType(old_address, new_address));
}

absl::uint128 IncrementalHashReference(const void* data, size_t len) {
  // The two constants are defined as part of the hash algorithm.
  // see http://www.isthe.com/chongo/tech/comp/fnv/
  // hash = 144066263297769815596495629667062367629
  absl::uint128 hash = absl::MakeUint128(UINT64_C(7809847782465536322),
                                         UINT64_C(7113472399480571277));
  // kPrime = 309485009821345068724781371
  const absl::uint128 kPrime = absl::MakeUint128(16777216, 315);
  const uint8_t* octets = reinterpret_cast<const uint8_t*>(data);
  for (size_t i = 0; i < len; ++i) {
    hash = hash ^ absl::MakeUint128(0, octets[i]);
    hash = hash * kPrime;
  }
  return hash;
}

TEST_F(QuicUtilsTest, ReferenceTest) {
  std::vector<uint8_t> data(32);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = i % 255;
  }
  EXPECT_EQ(IncrementalHashReference(data.data(), data.size()),
            QuicUtils::FNV1a_128_Hash(absl::string_view(
                reinterpret_cast<const char*>(data.data()), data.size())));
}

TEST_F(QuicUtilsTest, IsUnackable) {
  for (size_t i = FIRST_PACKET_STATE; i <= LAST_PACKET_STATE; ++i) {
    if (i == NEVER_SENT || i == ACKED || i == UNACKABLE) {
      EXPECT_FALSE(QuicUtils::IsAckable(static_cast<SentPacketState>(i)));
    } else {
      EXPECT_TRUE(QuicUtils::IsAckable(static_cast<SentPacketState>(i)));
    }
  }
}

TEST_F(QuicUtilsTest, RetransmissionTypeToPacketState) {
  for (size_t i = FIRST_TRANSMISSION_TYPE; i <= LAST_TRANSMISSION_TYPE; ++i) {
    if (i == NOT_RETRANSMISSION) {
      continue;
    }
    SentPacketState state = QuicUtils::RetransmissionTypeToPacketState(
        static_cast<TransmissionType>(i));
    if (i == HANDSHAKE_RETRANSMISSION) {
      EXPECT_EQ(HANDSHAKE_RETRANSMITTED, state);
    } else if (i == LOSS_RETRANSMISSION) {
      EXPECT_EQ(LOST, state);
    } else if (i == ALL_ZERO_RTT_RETRANSMISSION) {
      EXPECT_EQ(UNACKABLE, state);
    } else if (i == PTO_RETRANSMISSION) {
      EXPECT_EQ(PTO_RETRANSMITTED, state);
    } else if (i == PATH_RETRANSMISSION) {
      EXPECT_EQ(NOT_CONTRIBUTING_RTT, state);
    } else if (i == ALL_INITIAL_RETRANSMISSION) {
      EXPECT_EQ(UNACKABLE, state);
    } else {
      QUICHE_DCHECK(false)
          << "No corresponding packet state according to transmission type: "
          << i;
    }
  }
}

TEST_F(QuicUtilsTest, IsIetfPacketHeader) {
  // IETF QUIC short header
  uint8_t first_byte = 0;
  EXPECT_TRUE(QuicUtils::IsIetfPacketHeader(first_byte));
  EXPECT_TRUE(QuicUtils::IsIetfPacketShortHeader(first_byte));

  // IETF QUIC long header
  first_byte |= (FLAGS_LONG_HEADER | FLAGS_DEMULTIPLEXING_BIT);
  EXPECT_TRUE(QuicUtils::IsIetfPacketHeader(first_byte));
  EXPECT_FALSE(QuicUtils::IsIetfPacketShortHeader(first_byte));

  // IETF QUIC long header, version negotiation.
  first_byte = 0;
  first_byte |= FLAGS_LONG_HEADER;
  EXPECT_TRUE(QuicUtils::IsIetfPacketHeader(first_byte));
  EXPECT_FALSE(QuicUtils::IsIetfPacketShortHeader(first_byte));

  // GQUIC
  first_byte = 0;
  first_byte |= PACKET_PUBLIC_FLAGS_8BYTE_CONNECTION_ID;
  EXPECT_FALSE(QuicUtils::IsIetfPacketHeader(first_byte));
  EXPECT_FALSE(QuicUtils::IsIetfPacketShortHeader(first_byte));
}

TEST_F(QuicUtilsTest, RandomConnectionId) {
  MockRandom random(33);
  QuicConnectionId connection_id = QuicUtils::CreateRandomConnectionId(&random);
  EXPECT_EQ(connection_id.length(), sizeof(uint64_t));
  char connection_id_bytes[sizeof(uint64_t)];
  random.RandBytes(connection_id_bytes, ABSL_ARRAYSIZE(connection_id_bytes));
  EXPECT_EQ(connection_id,
            QuicConnectionId(static_cast<char*>(connection_id_bytes),
                             ABSL_ARRAYSIZE(connection_id_bytes)));
  EXPECT_NE(connection_id, EmptyQuicConnectionId());
  EXPECT_NE(connection_id, TestConnectionId());
  EXPECT_NE(connection_id, TestConnectionId(1));
  EXPECT_NE(connection_id, TestConnectionIdNineBytesLong(1));
  EXPECT_EQ(QuicUtils::CreateRandomConnectionId().length(),
            kQuicDefaultConnectionIdLength);
}

TEST_F(QuicUtilsTest, RandomConnectionIdVariableLength) {
  MockRandom random(1337);
  const uint8_t connection_id_length = 9;
  QuicConnectionId connection_id =
      QuicUtils::CreateRandomConnectionId(connection_id_length, &random);
  EXPECT_EQ(connection_id.length(), connection_id_length);
  char connection_id_bytes[connection_id_length];
  random.RandBytes(connection_id_bytes, ABSL_ARRAYSIZE(connection_id_bytes));
  EXPECT_EQ(connection_id,
            QuicConnectionId(static_cast<char*>(connection_id_bytes),
                             ABSL_ARRAYSIZE(connection_id_bytes)));
  EXPECT_NE(connection_id, EmptyQuicConnectionId());
  EXPECT_NE(connection_id, TestConnectionId());
  EXPECT_NE(connection_id, TestConnectionId(1));
  EXPECT_NE(connection_id, TestConnectionIdNineBytesLong(1));
  EXPECT_EQ(QuicUtils::CreateRandomConnectionId(connection_id_length).length(),
            connection_id_length);
}

TEST_F(QuicUtilsTest, VariableLengthConnectionId) {
  EXPECT_FALSE(VersionAllowsVariableLengthConnectionIds(QUIC_VERSION_46));
  EXPECT_TRUE(QuicUtils::IsConnectionIdValidForVersion(
      QuicUtils::CreateZeroConnectionId(QUIC_VERSION_46), QUIC_VERSION_46));
  EXPECT_NE(QuicUtils::CreateZeroConnectionId(QUIC_VERSION_46),
            EmptyQuicConnectionId());
  EXPECT_FALSE(QuicUtils::IsConnectionIdValidForVersion(EmptyQuicConnectionId(),
                                                        QUIC_VERSION_46));
}

TEST_F(QuicUtilsTest, StatelessResetToken) {
  QuicConnectionId connection_id1a = test::TestConnectionId(1);
  QuicConnectionId connection_id1b = test::TestConnectionId(1);
  QuicConnectionId connection_id2 = test::TestConnectionId(2);
  StatelessResetToken token1a =
      QuicUtils::GenerateStatelessResetToken(connection_id1a);
  StatelessResetToken token1b =
      QuicUtils::GenerateStatelessResetToken(connection_id1b);
  StatelessResetToken token2 =
      QuicUtils::GenerateStatelessResetToken(connection_id2);
  EXPECT_EQ(token1a, token1b);
  EXPECT_NE(token1a, token2);
  EXPECT_TRUE(QuicUtils::AreStatelessResetTokensEqual(token1a, token1b));
  EXPECT_FALSE(QuicUtils::AreStatelessResetTokensEqual(token1a, token2));
}

TEST_F(QuicUtilsTest, EcnCodepointToString) {
  EXPECT_EQ(EcnCodepointToString(ECN_NOT_ECT), "Not-ECT");
  EXPECT_EQ(EcnCodepointToString(ECN_ECT0), "ECT(0)");
  EXPECT_EQ(EcnCodepointToString(ECN_ECT1), "ECT(1)");
  EXPECT_EQ(EcnCodepointToString(ECN_CE), "CE");
}

TEST_F(QuicUtilsTest, PosixBasename) {
  EXPECT_EQ("", PosixBasename("/hello/"));
  EXPECT_EQ("hello", PosixBasename("/hello"));
  EXPECT_EQ("world", PosixBasename("hello/world"));
  EXPECT_EQ("", PosixBasename("hello/"));
  EXPECT_EQ("world", PosixBasename("world"));
  EXPECT_EQ("", PosixBasename("/"));
  EXPECT_EQ("", PosixBasename(""));
  // "\\" is not treated as a path separator.
  EXPECT_EQ("C:\\hello", PosixBasename("C:\\hello"));
  EXPECT_EQ("world", PosixBasename("C:\\hello/world"));
}

enum class TestEnumClassBit : uint8_t {
  BIT_ZERO = 0,
  BIT_ONE,
  BIT_TWO,
};

enum TestEnumBit {
  TEST_BIT_0 = 0,
  TEST_BIT_1,
  TEST_BIT_2,
};

TEST(QuicBitMaskTest, EnumClass) {
  BitMask<TestEnumClassBit> mask(
      {TestEnumClassBit::BIT_ZERO, TestEnumClassBit::BIT_TWO});
  EXPECT_TRUE(mask.IsSet(TestEnumClassBit::BIT_ZERO));
  EXPECT_FALSE(mask.IsSet(TestEnumClassBit::BIT_ONE));
  EXPECT_TRUE(mask.IsSet(TestEnumClassBit::BIT_TWO));

  mask.ClearAll();
  EXPECT_FALSE(mask.IsSet(TestEnumClassBit::BIT_ZERO));
  EXPECT_FALSE(mask.IsSet(TestEnumClassBit::BIT_ONE));
  EXPECT_FALSE(mask.IsSet(TestEnumClassBit::BIT_TWO));
}

TEST(QuicBitMaskTest, Enum) {
  BitMask<TestEnumBit> mask({TEST_BIT_1, TEST_BIT_2});
  EXPECT_FALSE(mask.IsSet(TEST_BIT_0));
  EXPECT_TRUE(mask.IsSet(TEST_BIT_1));
  EXPECT_TRUE(mask.IsSet(TEST_BIT_2));

  mask.ClearAll();
  EXPECT_FALSE(mask.IsSet(TEST_BIT_0));
  EXPECT_FALSE(mask.IsSet(TEST_BIT_1));
  EXPECT_FALSE(mask.IsSet(TEST_BIT_2));
}

TEST(QuicBitMaskTest, Integer) {
  BitMask<int> mask({1, 3});
  EXPECT_EQ(mask.Max(), 3);
  mask.Set(3);
  mask.Set({5, 7, 9});
  EXPECT_EQ(mask.Max(), 9);
  EXPECT_FALSE(mask.IsSet(0));
  EXPECT_TRUE(mask.IsSet(1));
  EXPECT_FALSE(mask.IsSet(2));
  EXPECT_TRUE(mask.IsSet(3));
  EXPECT_FALSE(mask.IsSet(4));
  EXPECT_TRUE(mask.IsSet(5));
  EXPECT_FALSE(mask.IsSet(6));
  EXPECT_TRUE(mask.IsSet(7));
  EXPECT_FALSE(mask.IsSet(8));
  EXPECT_TRUE(mask.IsSet(9));
}

TEST(QuicBitMaskTest, NumBits) {
  EXPECT_EQ(64u, BitMask<int>::NumBits());
  EXPECT_EQ(32u, (BitMask<int, uint32_t>::NumBits()));
}

TEST(QuicBitMaskTest, Constructor) {
  BitMask<int> empty_mask;
  for (size_t bit = 0; bit < empty_mask.NumBits(); ++bit) {
    EXPECT_FALSE(empty_mask.IsSet(bit));
  }

  BitMask<int> mask({1, 3});
  BitMask<int> mask2 = mask;
  BitMask<int> mask3(mask2);

  for (size_t bit = 0; bit < mask.NumBits(); ++bit) {
    EXPECT_EQ(mask.IsSet(bit), mask2.IsSet(bit));
    EXPECT_EQ(mask.IsSet(bit), mask3.IsSet(bit));
  }

  EXPECT_TRUE(std::is_trivially_copyable<BitMask<int>>::value);
}

TEST(QuicBitMaskTest, Any) {
  BitMask<int> mask;
  EXPECT_FALSE(mask.Any());
  mask.Set(3);
  EXPECT_TRUE(mask.Any());
  mask.Set(2);
  EXPECT_TRUE(mask.Any());
  mask.ClearAll();
  EXPECT_FALSE(mask.Any());
}

TEST(QuicBitMaskTest, And) {
  using Mask = BitMask<int>;
  EXPECT_EQ(Mask({1, 3, 6}) & Mask({3, 5, 6}), Mask({3, 6}));
  EXPECT_EQ(Mask({1, 2, 4}) & Mask({3, 5}), Mask({}));
  EXPECT_EQ(Mask({1, 2, 3, 4, 5}) & Mask({}), Mask({}));
}

}  // namespace
}  // namespace test
}  // namespace quic
