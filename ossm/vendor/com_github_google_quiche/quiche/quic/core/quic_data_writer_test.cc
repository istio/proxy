// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_data_writer.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "absl/base/macros.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_connection_id.h"
#include "quiche/quic/core/quic_data_reader.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/quiche_endian.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace quic {
namespace test {
namespace {

char* AsChars(unsigned char* data) { return reinterpret_cast<char*>(data); }

struct TestParams {
  explicit TestParams(quiche::Endianness endianness) : endianness(endianness) {}

  quiche::Endianness endianness;
};

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const TestParams& p) {
  return absl::StrCat(
      (p.endianness == quiche::NETWORK_BYTE_ORDER ? "Network" : "Host"),
      "ByteOrder");
}

std::vector<TestParams> GetTestParams() {
  std::vector<TestParams> params;
  for (quiche::Endianness endianness :
       {quiche::NETWORK_BYTE_ORDER, quiche::HOST_BYTE_ORDER}) {
    params.push_back(TestParams(endianness));
  }
  return params;
}

class QuicDataWriterTest : public QuicTestWithParam<TestParams> {};

INSTANTIATE_TEST_SUITE_P(QuicDataWriterTests, QuicDataWriterTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicDataWriterTest, SanityCheckUFloat16Consts) {
  // Check the arithmetic on the constants - otherwise the values below make
  // no sense.
  EXPECT_EQ(30, kUFloat16MaxExponent);
  EXPECT_EQ(11, kUFloat16MantissaBits);
  EXPECT_EQ(12, kUFloat16MantissaEffectiveBits);
  EXPECT_EQ(UINT64_C(0x3FFC0000000), kUFloat16MaxValue);
}

TEST_P(QuicDataWriterTest, WriteUFloat16) {
  struct TestCase {
    uint64_t decoded;
    uint16_t encoded;
  };
  TestCase test_cases[] = {
      // Small numbers represent themselves.
      {0, 0},
      {1, 1},
      {2, 2},
      {3, 3},
      {4, 4},
      {5, 5},
      {6, 6},
      {7, 7},
      {15, 15},
      {31, 31},
      {42, 42},
      {123, 123},
      {1234, 1234},
      // Check transition through 2^11.
      {2046, 2046},
      {2047, 2047},
      {2048, 2048},
      {2049, 2049},
      // Running out of mantissa at 2^12.
      {4094, 4094},
      {4095, 4095},
      {4096, 4096},
      {4097, 4096},
      {4098, 4097},
      {4099, 4097},
      {4100, 4098},
      {4101, 4098},
      // Check transition through 2^13.
      {8190, 6143},
      {8191, 6143},
      {8192, 6144},
      {8193, 6144},
      {8194, 6144},
      {8195, 6144},
      {8196, 6145},
      {8197, 6145},
      // Half-way through the exponents.
      {0x7FF8000, 0x87FF},
      {0x7FFFFFF, 0x87FF},
      {0x8000000, 0x8800},
      {0xFFF0000, 0x8FFF},
      {0xFFFFFFF, 0x8FFF},
      {0x10000000, 0x9000},
      // Transition into the largest exponent.
      {0x1FFFFFFFFFE, 0xF7FF},
      {0x1FFFFFFFFFF, 0xF7FF},
      {0x20000000000, 0xF800},
      {0x20000000001, 0xF800},
      {0x2003FFFFFFE, 0xF800},
      {0x2003FFFFFFF, 0xF800},
      {0x20040000000, 0xF801},
      {0x20040000001, 0xF801},
      // Transition into the max value and clamping.
      {0x3FF80000000, 0xFFFE},
      {0x3FFBFFFFFFF, 0xFFFE},
      {0x3FFC0000000, 0xFFFF},
      {0x3FFC0000001, 0xFFFF},
      {0x3FFFFFFFFFF, 0xFFFF},
      {0x40000000000, 0xFFFF},
      {0xFFFFFFFFFFFFFFFF, 0xFFFF},
  };
  int num_test_cases = sizeof(test_cases) / sizeof(test_cases[0]);

  for (int i = 0; i < num_test_cases; ++i) {
    char buffer[2];
    QuicDataWriter writer(2, buffer, GetParam().endianness);
    EXPECT_TRUE(writer.WriteUFloat16(test_cases[i].decoded));
    uint16_t result = *reinterpret_cast<uint16_t*>(writer.data());
    if (GetParam().endianness == quiche::NETWORK_BYTE_ORDER) {
      result = quiche::QuicheEndian::HostToNet16(result);
    }
    EXPECT_EQ(test_cases[i].encoded, result);
  }
}

TEST_P(QuicDataWriterTest, ReadUFloat16) {
  struct TestCase {
    uint64_t decoded;
    uint16_t encoded;
  };
  TestCase test_cases[] = {
      // There are fewer decoding test cases because encoding truncates, and
      // decoding returns the smallest expansion.
      // Small numbers represent themselves.
      {0, 0},
      {1, 1},
      {2, 2},
      {3, 3},
      {4, 4},
      {5, 5},
      {6, 6},
      {7, 7},
      {15, 15},
      {31, 31},
      {42, 42},
      {123, 123},
      {1234, 1234},
      // Check transition through 2^11.
      {2046, 2046},
      {2047, 2047},
      {2048, 2048},
      {2049, 2049},
      // Running out of mantissa at 2^12.
      {4094, 4094},
      {4095, 4095},
      {4096, 4096},
      {4098, 4097},
      {4100, 4098},
      // Check transition through 2^13.
      {8190, 6143},
      {8192, 6144},
      {8196, 6145},
      // Half-way through the exponents.
      {0x7FF8000, 0x87FF},
      {0x8000000, 0x8800},
      {0xFFF0000, 0x8FFF},
      {0x10000000, 0x9000},
      // Transition into the largest exponent.
      {0x1FFE0000000, 0xF7FF},
      {0x20000000000, 0xF800},
      {0x20040000000, 0xF801},
      // Transition into the max value.
      {0x3FF80000000, 0xFFFE},
      {0x3FFC0000000, 0xFFFF},
  };
  int num_test_cases = sizeof(test_cases) / sizeof(test_cases[0]);

  for (int i = 0; i < num_test_cases; ++i) {
    uint16_t encoded_ufloat = test_cases[i].encoded;
    if (GetParam().endianness == quiche::NETWORK_BYTE_ORDER) {
      encoded_ufloat = quiche::QuicheEndian::HostToNet16(encoded_ufloat);
    }
    QuicDataReader reader(reinterpret_cast<char*>(&encoded_ufloat), 2,
                          GetParam().endianness);
    uint64_t value;
    EXPECT_TRUE(reader.ReadUFloat16(&value));
    EXPECT_EQ(test_cases[i].decoded, value);
  }
}

TEST_P(QuicDataWriterTest, RoundTripUFloat16) {
  // Just test all 16-bit encoded values. 0 and max already tested above.
  uint64_t previous_value = 0;
  for (uint16_t i = 1; i < 0xFFFF; ++i) {
    // Read the two bytes.
    uint16_t read_number = i;
    if (GetParam().endianness == quiche::NETWORK_BYTE_ORDER) {
      read_number = quiche::QuicheEndian::HostToNet16(read_number);
    }
    QuicDataReader reader(reinterpret_cast<char*>(&read_number), 2,
                          GetParam().endianness);
    uint64_t value;
    // All values must be decodable.
    EXPECT_TRUE(reader.ReadUFloat16(&value));
    // Check that small numbers represent themselves
    if (i < 4097) {
      EXPECT_EQ(i, value);
    }
    // Check there's monotonic growth.
    EXPECT_LT(previous_value, value);
    // Check that precision is within 0.5% away from the denormals.
    if (i > 2000) {
      EXPECT_GT(previous_value * 1005, value * 1000);
    }
    // Check we're always within the promised range.
    EXPECT_LT(value, UINT64_C(0x3FFC0000000));
    previous_value = value;
    char buffer[6];
    QuicDataWriter writer(6, buffer, GetParam().endianness);
    EXPECT_TRUE(writer.WriteUFloat16(value - 1));
    EXPECT_TRUE(writer.WriteUFloat16(value));
    EXPECT_TRUE(writer.WriteUFloat16(value + 1));
    // Check minimal decoding (previous decoding has previous encoding).
    uint16_t encoded1 = *reinterpret_cast<uint16_t*>(writer.data());
    uint16_t encoded2 = *reinterpret_cast<uint16_t*>(writer.data() + 2);
    uint16_t encoded3 = *reinterpret_cast<uint16_t*>(writer.data() + 4);
    if (GetParam().endianness == quiche::NETWORK_BYTE_ORDER) {
      encoded1 = quiche::QuicheEndian::NetToHost16(encoded1);
      encoded2 = quiche::QuicheEndian::NetToHost16(encoded2);
      encoded3 = quiche::QuicheEndian::NetToHost16(encoded3);
    }
    EXPECT_EQ(i - 1, encoded1);
    // Check roundtrip.
    EXPECT_EQ(i, encoded2);
    // Check next decoding.
    EXPECT_EQ(i < 4096 ? i + 1 : i, encoded3);
  }
}

TEST_P(QuicDataWriterTest, WriteConnectionId) {
  QuicConnectionId connection_id =
      TestConnectionId(UINT64_C(0x0011223344556677));
  char big_endian[] = {
      0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
  };
  EXPECT_EQ(connection_id.length(), ABSL_ARRAYSIZE(big_endian));
  ASSERT_LE(connection_id.length(), 255);
  char buffer[255];
  QuicDataWriter writer(connection_id.length(), buffer, GetParam().endianness);
  EXPECT_TRUE(writer.WriteConnectionId(connection_id));
  quiche::test::CompareCharArraysWithHexError(
      "connection_id", buffer, connection_id.length(), big_endian,
      connection_id.length());

  QuicConnectionId read_connection_id;
  QuicDataReader reader(buffer, connection_id.length(), GetParam().endianness);
  EXPECT_TRUE(
      reader.ReadConnectionId(&read_connection_id, ABSL_ARRAYSIZE(big_endian)));
  EXPECT_EQ(connection_id, read_connection_id);
}

TEST_P(QuicDataWriterTest, LengthPrefixedConnectionId) {
  QuicConnectionId connection_id =
      TestConnectionId(UINT64_C(0x0011223344556677));
  char length_prefixed_connection_id[] = {
      0x08, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
  };
  EXPECT_EQ(ABSL_ARRAYSIZE(length_prefixed_connection_id),
            kConnectionIdLengthSize + connection_id.length());
  char buffer[kConnectionIdLengthSize + 255] = {};
  QuicDataWriter writer(ABSL_ARRAYSIZE(buffer), buffer);
  EXPECT_TRUE(writer.WriteLengthPrefixedConnectionId(connection_id));
  quiche::test::CompareCharArraysWithHexError(
      "WriteLengthPrefixedConnectionId", buffer, writer.length(),
      length_prefixed_connection_id,
      ABSL_ARRAYSIZE(length_prefixed_connection_id));

  // Verify that writing length then connection ID produces the same output.
  memset(buffer, 0, ABSL_ARRAYSIZE(buffer));
  QuicDataWriter writer2(ABSL_ARRAYSIZE(buffer), buffer);
  EXPECT_TRUE(writer2.WriteUInt8(connection_id.length()));
  EXPECT_TRUE(writer2.WriteConnectionId(connection_id));
  quiche::test::CompareCharArraysWithHexError(
      "Write length then ConnectionId", buffer, writer2.length(),
      length_prefixed_connection_id,
      ABSL_ARRAYSIZE(length_prefixed_connection_id));

  QuicConnectionId read_connection_id;
  QuicDataReader reader(buffer, ABSL_ARRAYSIZE(buffer));
  EXPECT_TRUE(reader.ReadLengthPrefixedConnectionId(&read_connection_id));
  EXPECT_EQ(connection_id, read_connection_id);

  // Verify that reading length then connection ID produces the same output.
  uint8_t read_connection_id_length2 = 33;
  QuicConnectionId read_connection_id2;
  QuicDataReader reader2(buffer, ABSL_ARRAYSIZE(buffer));
  ASSERT_TRUE(reader2.ReadUInt8(&read_connection_id_length2));
  EXPECT_EQ(connection_id.length(), read_connection_id_length2);
  EXPECT_TRUE(reader2.ReadConnectionId(&read_connection_id2,
                                       read_connection_id_length2));
  EXPECT_EQ(connection_id, read_connection_id2);
}

TEST_P(QuicDataWriterTest, EmptyConnectionIds) {
  QuicConnectionId empty_connection_id = EmptyQuicConnectionId();
  char buffer[2];
  QuicDataWriter writer(ABSL_ARRAYSIZE(buffer), buffer, GetParam().endianness);
  EXPECT_TRUE(writer.WriteConnectionId(empty_connection_id));
  EXPECT_TRUE(writer.WriteUInt8(1));
  EXPECT_TRUE(writer.WriteConnectionId(empty_connection_id));
  EXPECT_TRUE(writer.WriteUInt8(2));
  EXPECT_TRUE(writer.WriteConnectionId(empty_connection_id));
  EXPECT_FALSE(writer.WriteUInt8(3));

  EXPECT_EQ(buffer[0], 1);
  EXPECT_EQ(buffer[1], 2);

  QuicConnectionId read_connection_id = TestConnectionId();
  uint8_t read_byte;
  QuicDataReader reader(buffer, ABSL_ARRAYSIZE(buffer), GetParam().endianness);
  EXPECT_TRUE(reader.ReadConnectionId(&read_connection_id, 0));
  EXPECT_EQ(read_connection_id, empty_connection_id);
  EXPECT_TRUE(reader.ReadUInt8(&read_byte));
  EXPECT_EQ(read_byte, 1);
  // Reset read_connection_id to something else to verify that
  // ReadConnectionId properly sets it back to empty.
  read_connection_id = TestConnectionId();
  EXPECT_TRUE(reader.ReadConnectionId(&read_connection_id, 0));
  EXPECT_EQ(read_connection_id, empty_connection_id);
  EXPECT_TRUE(reader.ReadUInt8(&read_byte));
  EXPECT_EQ(read_byte, 2);
  read_connection_id = TestConnectionId();
  EXPECT_TRUE(reader.ReadConnectionId(&read_connection_id, 0));
  EXPECT_EQ(read_connection_id, empty_connection_id);
  EXPECT_FALSE(reader.ReadUInt8(&read_byte));
}

TEST_P(QuicDataWriterTest, WriteTag) {
  char CHLO[] = {
      'C',
      'H',
      'L',
      'O',
  };
  const int kBufferLength = sizeof(QuicTag);
  char buffer[kBufferLength];
  QuicDataWriter writer(kBufferLength, buffer, GetParam().endianness);
  writer.WriteTag(kCHLO);
  quiche::test::CompareCharArraysWithHexError("CHLO", buffer, kBufferLength,
                                              CHLO, kBufferLength);

  QuicTag read_chlo;
  QuicDataReader reader(buffer, kBufferLength, GetParam().endianness);
  reader.ReadTag(&read_chlo);
  EXPECT_EQ(kCHLO, read_chlo);
}

TEST_P(QuicDataWriterTest, Write16BitUnsignedIntegers) {
  char little_endian16[] = {0x22, 0x11};
  char big_endian16[] = {0x11, 0x22};
  char buffer16[2];
  {
    uint16_t in_memory16 = 0x1122;
    QuicDataWriter writer(2, buffer16, GetParam().endianness);
    writer.WriteUInt16(in_memory16);
    quiche::test::CompareCharArraysWithHexError(
        "uint16_t", buffer16, 2,
        GetParam().endianness == quiche::NETWORK_BYTE_ORDER ? big_endian16
                                                            : little_endian16,
        2);

    uint16_t read_number16;
    QuicDataReader reader(buffer16, 2, GetParam().endianness);
    reader.ReadUInt16(&read_number16);
    EXPECT_EQ(in_memory16, read_number16);
  }

  {
    uint64_t in_memory16 = 0x0000000000001122;
    QuicDataWriter writer(2, buffer16, GetParam().endianness);
    writer.WriteBytesToUInt64(2, in_memory16);
    quiche::test::CompareCharArraysWithHexError(
        "uint16_t", buffer16, 2,
        GetParam().endianness == quiche::NETWORK_BYTE_ORDER ? big_endian16
                                                            : little_endian16,
        2);

    uint64_t read_number16;
    QuicDataReader reader(buffer16, 2, GetParam().endianness);
    reader.ReadBytesToUInt64(2, &read_number16);
    EXPECT_EQ(in_memory16, read_number16);
  }
}

TEST_P(QuicDataWriterTest, Write24BitUnsignedIntegers) {
  char little_endian24[] = {0x33, 0x22, 0x11};
  char big_endian24[] = {0x11, 0x22, 0x33};
  char buffer24[3];
  uint64_t in_memory24 = 0x0000000000112233;
  QuicDataWriter writer(3, buffer24, GetParam().endianness);
  writer.WriteBytesToUInt64(3, in_memory24);
  quiche::test::CompareCharArraysWithHexError(
      "uint24", buffer24, 3,
      GetParam().endianness == quiche::NETWORK_BYTE_ORDER ? big_endian24
                                                          : little_endian24,
      3);

  uint64_t read_number24;
  QuicDataReader reader(buffer24, 3, GetParam().endianness);
  reader.ReadBytesToUInt64(3, &read_number24);
  EXPECT_EQ(in_memory24, read_number24);
}

TEST_P(QuicDataWriterTest, Write32BitUnsignedIntegers) {
  char little_endian32[] = {0x44, 0x33, 0x22, 0x11};
  char big_endian32[] = {0x11, 0x22, 0x33, 0x44};
  char buffer32[4];
  {
    uint32_t in_memory32 = 0x11223344;
    QuicDataWriter writer(4, buffer32, GetParam().endianness);
    writer.WriteUInt32(in_memory32);
    quiche::test::CompareCharArraysWithHexError(
        "uint32_t", buffer32, 4,
        GetParam().endianness == quiche::NETWORK_BYTE_ORDER ? big_endian32
                                                            : little_endian32,
        4);

    uint32_t read_number32;
    QuicDataReader reader(buffer32, 4, GetParam().endianness);
    reader.ReadUInt32(&read_number32);
    EXPECT_EQ(in_memory32, read_number32);
  }

  {
    uint64_t in_memory32 = 0x11223344;
    QuicDataWriter writer(4, buffer32, GetParam().endianness);
    writer.WriteBytesToUInt64(4, in_memory32);
    quiche::test::CompareCharArraysWithHexError(
        "uint32_t", buffer32, 4,
        GetParam().endianness == quiche::NETWORK_BYTE_ORDER ? big_endian32
                                                            : little_endian32,
        4);

    uint64_t read_number32;
    QuicDataReader reader(buffer32, 4, GetParam().endianness);
    reader.ReadBytesToUInt64(4, &read_number32);
    EXPECT_EQ(in_memory32, read_number32);
  }
}

TEST_P(QuicDataWriterTest, Write40BitUnsignedIntegers) {
  uint64_t in_memory40 = 0x0000001122334455;
  char little_endian40[] = {0x55, 0x44, 0x33, 0x22, 0x11};
  char big_endian40[] = {0x11, 0x22, 0x33, 0x44, 0x55};
  char buffer40[5];
  QuicDataWriter writer(5, buffer40, GetParam().endianness);
  writer.WriteBytesToUInt64(5, in_memory40);
  quiche::test::CompareCharArraysWithHexError(
      "uint40", buffer40, 5,
      GetParam().endianness == quiche::NETWORK_BYTE_ORDER ? big_endian40
                                                          : little_endian40,
      5);

  uint64_t read_number40;
  QuicDataReader reader(buffer40, 5, GetParam().endianness);
  reader.ReadBytesToUInt64(5, &read_number40);
  EXPECT_EQ(in_memory40, read_number40);
}

TEST_P(QuicDataWriterTest, Write48BitUnsignedIntegers) {
  uint64_t in_memory48 = 0x0000112233445566;
  char little_endian48[] = {0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
  char big_endian48[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  char buffer48[6];
  QuicDataWriter writer(6, buffer48, GetParam().endianness);
  writer.WriteBytesToUInt64(6, in_memory48);
  quiche::test::CompareCharArraysWithHexError(
      "uint48", buffer48, 6,
      GetParam().endianness == quiche::NETWORK_BYTE_ORDER ? big_endian48
                                                          : little_endian48,
      6);

  uint64_t read_number48;
  QuicDataReader reader(buffer48, 6, GetParam().endianness);
  reader.ReadBytesToUInt64(6., &read_number48);
  EXPECT_EQ(in_memory48, read_number48);
}

TEST_P(QuicDataWriterTest, Write56BitUnsignedIntegers) {
  uint64_t in_memory56 = 0x0011223344556677;
  char little_endian56[] = {0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
  char big_endian56[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
  char buffer56[7];
  QuicDataWriter writer(7, buffer56, GetParam().endianness);
  writer.WriteBytesToUInt64(7, in_memory56);
  quiche::test::CompareCharArraysWithHexError(
      "uint56", buffer56, 7,
      GetParam().endianness == quiche::NETWORK_BYTE_ORDER ? big_endian56
                                                          : little_endian56,
      7);

  uint64_t read_number56;
  QuicDataReader reader(buffer56, 7, GetParam().endianness);
  reader.ReadBytesToUInt64(7, &read_number56);
  EXPECT_EQ(in_memory56, read_number56);
}

TEST_P(QuicDataWriterTest, Write64BitUnsignedIntegers) {
  uint64_t in_memory64 = 0x1122334455667788;
  unsigned char little_endian64[] = {0x88, 0x77, 0x66, 0x55,
                                     0x44, 0x33, 0x22, 0x11};
  unsigned char big_endian64[] = {0x11, 0x22, 0x33, 0x44,
                                  0x55, 0x66, 0x77, 0x88};
  char buffer64[8];
  QuicDataWriter writer(8, buffer64, GetParam().endianness);
  writer.WriteBytesToUInt64(8, in_memory64);
  quiche::test::CompareCharArraysWithHexError(
      "uint64_t", buffer64, 8,
      GetParam().endianness == quiche::NETWORK_BYTE_ORDER
          ? AsChars(big_endian64)
          : AsChars(little_endian64),
      8);

  uint64_t read_number64;
  QuicDataReader reader(buffer64, 8, GetParam().endianness);
  reader.ReadBytesToUInt64(8, &read_number64);
  EXPECT_EQ(in_memory64, read_number64);

  QuicDataWriter writer2(8, buffer64, GetParam().endianness);
  writer2.WriteUInt64(in_memory64);
  quiche::test::CompareCharArraysWithHexError(
      "uint64_t", buffer64, 8,
      GetParam().endianness == quiche::NETWORK_BYTE_ORDER
          ? AsChars(big_endian64)
          : AsChars(little_endian64),
      8);
  read_number64 = 0u;
  QuicDataReader reader2(buffer64, 8, GetParam().endianness);
  reader2.ReadUInt64(&read_number64);
  EXPECT_EQ(in_memory64, read_number64);
}

TEST_P(QuicDataWriterTest, WriteIntegers) {
  char buf[43];
  uint8_t i8 = 0x01;
  uint16_t i16 = 0x0123;
  uint32_t i32 = 0x01234567;
  uint64_t i64 = 0x0123456789ABCDEF;
  QuicDataWriter writer(46, buf, GetParam().endianness);
  for (size_t i = 0; i < 10; ++i) {
    switch (i) {
      case 0u:
        EXPECT_TRUE(writer.WriteBytesToUInt64(i, i64));
        break;
      case 1u:
        EXPECT_TRUE(writer.WriteUInt8(i8));
        EXPECT_TRUE(writer.WriteBytesToUInt64(i, i64));
        break;
      case 2u:
        EXPECT_TRUE(writer.WriteUInt16(i16));
        EXPECT_TRUE(writer.WriteBytesToUInt64(i, i64));
        break;
      case 3u:
        EXPECT_TRUE(writer.WriteBytesToUInt64(i, i64));
        break;
      case 4u:
        EXPECT_TRUE(writer.WriteUInt32(i32));
        EXPECT_TRUE(writer.WriteBytesToUInt64(i, i64));
        break;
      case 5u:
      case 6u:
      case 7u:
      case 8u:
        EXPECT_TRUE(writer.WriteBytesToUInt64(i, i64));
        break;
      default:
        EXPECT_FALSE(writer.WriteBytesToUInt64(i, i64));
    }
  }

  QuicDataReader reader(buf, 46, GetParam().endianness);
  for (size_t i = 0; i < 10; ++i) {
    uint8_t read8;
    uint16_t read16;
    uint32_t read32;
    uint64_t read64;
    switch (i) {
      case 0u:
        EXPECT_TRUE(reader.ReadBytesToUInt64(i, &read64));
        EXPECT_EQ(0u, read64);
        break;
      case 1u:
        EXPECT_TRUE(reader.ReadUInt8(&read8));
        EXPECT_TRUE(reader.ReadBytesToUInt64(i, &read64));
        EXPECT_EQ(i8, read8);
        EXPECT_EQ(0xEFu, read64);
        break;
      case 2u:
        EXPECT_TRUE(reader.ReadUInt16(&read16));
        EXPECT_TRUE(reader.ReadBytesToUInt64(i, &read64));
        EXPECT_EQ(i16, read16);
        EXPECT_EQ(0xCDEFu, read64);
        break;
      case 3u:
        EXPECT_TRUE(reader.ReadBytesToUInt64(i, &read64));
        EXPECT_EQ(0xABCDEFu, read64);
        break;
      case 4u:
        EXPECT_TRUE(reader.ReadUInt32(&read32));
        EXPECT_TRUE(reader.ReadBytesToUInt64(i, &read64));
        EXPECT_EQ(i32, read32);
        EXPECT_EQ(0x89ABCDEFu, read64);
        break;
      case 5u:
        EXPECT_TRUE(reader.ReadBytesToUInt64(i, &read64));
        EXPECT_EQ(0x6789ABCDEFu, read64);
        break;
      case 6u:
        EXPECT_TRUE(reader.ReadBytesToUInt64(i, &read64));
        EXPECT_EQ(0x456789ABCDEFu, read64);
        break;
      case 7u:
        EXPECT_TRUE(reader.ReadBytesToUInt64(i, &read64));
        EXPECT_EQ(0x23456789ABCDEFu, read64);
        break;
      case 8u:
        EXPECT_TRUE(reader.ReadBytesToUInt64(i, &read64));
        EXPECT_EQ(0x0123456789ABCDEFu, read64);
        break;
      default:
        EXPECT_FALSE(reader.ReadBytesToUInt64(i, &read64));
    }
  }
}

TEST_P(QuicDataWriterTest, WriteBytes) {
  char bytes[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  char buf[ABSL_ARRAYSIZE(bytes)];
  QuicDataWriter writer(ABSL_ARRAYSIZE(buf), buf, GetParam().endianness);
  EXPECT_TRUE(writer.WriteBytes(bytes, ABSL_ARRAYSIZE(bytes)));
  for (unsigned int i = 0; i < ABSL_ARRAYSIZE(bytes); ++i) {
    EXPECT_EQ(bytes[i], buf[i]);
  }
}

// Following tests all try to fill the buffer with multiple values,
// go one value more than the buffer can accommodate, then read
// the successfully encoded values, and try to read the unsuccessfully
// encoded value. The following is the number of values to encode.
const int kMultiVarCount = 1000;

// Test encoding/decoding stream-id values.
void EncodeDecodeStreamId(uint64_t value_in) {
  char buffer[1 * kMultiVarCount];
  memset(buffer, 0, sizeof(buffer));

  // Encode the given Stream ID.
  QuicDataWriter writer(sizeof(buffer), static_cast<char*>(buffer),
                        quiche::Endianness::NETWORK_BYTE_ORDER);
  EXPECT_TRUE(writer.WriteVarInt62(value_in));

  QuicDataReader reader(buffer, sizeof(buffer),
                        quiche::Endianness::NETWORK_BYTE_ORDER);
  QuicStreamId received_stream_id;
  uint64_t temp;
  EXPECT_TRUE(reader.ReadVarInt62(&temp));
  received_stream_id = static_cast<QuicStreamId>(temp);
  EXPECT_EQ(value_in, received_stream_id);
}

// Test writing & reading stream-ids of various value.
TEST_P(QuicDataWriterTest, StreamId1) {
  // Check a 1-byte QuicStreamId, should work
  EncodeDecodeStreamId(UINT64_C(0x15));

  // Check a 2-byte QuicStream ID. It should work.
  EncodeDecodeStreamId(UINT64_C(0x1567));

  // Check a QuicStreamId that requires 4 bytes of encoding
  // This should work.
  EncodeDecodeStreamId(UINT64_C(0x34567890));

  // Check a QuicStreamId that requires 8 bytes of encoding
  // but whose value is in the acceptable range.
  // This should work.
  EncodeDecodeStreamId(UINT64_C(0xf4567890));
}

TEST_P(QuicDataWriterTest, WriteRandomBytes) {
  char buffer[20];
  char expected[20];
  for (size_t i = 0; i < 20; ++i) {
    expected[i] = 'r';
  }
  MockRandom random;
  QuicDataWriter writer(20, buffer, GetParam().endianness);
  EXPECT_FALSE(writer.WriteRandomBytes(&random, 30));

  EXPECT_TRUE(writer.WriteRandomBytes(&random, 20));
  quiche::test::CompareCharArraysWithHexError("random", buffer, 20, expected,
                                              20);
}

TEST_P(QuicDataWriterTest, WriteInsecureRandomBytes) {
  char buffer[20];
  char expected[20];
  for (size_t i = 0; i < 20; ++i) {
    expected[i] = 'r';
  }
  MockRandom random;
  QuicDataWriter writer(20, buffer, GetParam().endianness);
  EXPECT_FALSE(writer.WriteInsecureRandomBytes(&random, 30));

  EXPECT_TRUE(writer.WriteInsecureRandomBytes(&random, 20));
  quiche::test::CompareCharArraysWithHexError("random", buffer, 20, expected,
                                              20);
}

TEST_P(QuicDataWriterTest, PeekVarInt62Length) {
  // In range [0, 63], variable length should be 1 byte.
  char buffer[20];
  QuicDataWriter writer(20, buffer, quiche::NETWORK_BYTE_ORDER);
  EXPECT_TRUE(writer.WriteVarInt62(50));
  QuicDataReader reader(buffer, 20, quiche::NETWORK_BYTE_ORDER);
  EXPECT_EQ(1, reader.PeekVarInt62Length());
  // In range (63-16383], variable length should be 2 byte2.
  char buffer2[20];
  QuicDataWriter writer2(20, buffer2, quiche::NETWORK_BYTE_ORDER);
  EXPECT_TRUE(writer2.WriteVarInt62(100));
  QuicDataReader reader2(buffer2, 20, quiche::NETWORK_BYTE_ORDER);
  EXPECT_EQ(2, reader2.PeekVarInt62Length());
  // In range (16383, 1073741823], variable length should be 4 bytes.
  char buffer3[20];
  QuicDataWriter writer3(20, buffer3, quiche::NETWORK_BYTE_ORDER);
  EXPECT_TRUE(writer3.WriteVarInt62(20000));
  QuicDataReader reader3(buffer3, 20, quiche::NETWORK_BYTE_ORDER);
  EXPECT_EQ(4, reader3.PeekVarInt62Length());
  // In range (1073741823, 4611686018427387903], variable length should be 8
  // bytes.
  char buffer4[20];
  QuicDataWriter writer4(20, buffer4, quiche::NETWORK_BYTE_ORDER);
  EXPECT_TRUE(writer4.WriteVarInt62(2000000000));
  QuicDataReader reader4(buffer4, 20, quiche::NETWORK_BYTE_ORDER);
  EXPECT_EQ(8, reader4.PeekVarInt62Length());
}

TEST_P(QuicDataWriterTest, ValidStreamCount) {
  char buffer[1024];
  memset(buffer, 0, sizeof(buffer));
  QuicDataWriter writer(sizeof(buffer), static_cast<char*>(buffer),
                        quiche::Endianness::NETWORK_BYTE_ORDER);
  QuicDataReader reader(buffer, sizeof(buffer));
  const QuicStreamCount write_stream_count = 0xffeeddcc;
  EXPECT_TRUE(writer.WriteVarInt62(write_stream_count));
  QuicStreamCount read_stream_count;
  uint64_t temp;
  EXPECT_TRUE(reader.ReadVarInt62(&temp));
  read_stream_count = static_cast<QuicStreamId>(temp);
  EXPECT_EQ(write_stream_count, read_stream_count);
}

TEST_P(QuicDataWriterTest, Seek) {
  char buffer[3] = {};
  QuicDataWriter writer(ABSL_ARRAYSIZE(buffer), buffer, GetParam().endianness);
  EXPECT_TRUE(writer.WriteUInt8(42));
  EXPECT_TRUE(writer.Seek(1));
  EXPECT_TRUE(writer.WriteUInt8(3));

  char expected[] = {42, 0, 3};
  for (size_t i = 0; i < ABSL_ARRAYSIZE(expected); ++i) {
    EXPECT_EQ(buffer[i], expected[i]);
  }
}

TEST_P(QuicDataWriterTest, SeekTooFarFails) {
  char buffer[20];

  // Check that one can seek to the end of the writer, but not past.
  {
    QuicDataWriter writer(ABSL_ARRAYSIZE(buffer), buffer,
                          GetParam().endianness);
    EXPECT_TRUE(writer.Seek(20));
    EXPECT_FALSE(writer.Seek(1));
  }

  // Seeking several bytes past the end fails.
  {
    QuicDataWriter writer(ABSL_ARRAYSIZE(buffer), buffer,
                          GetParam().endianness);
    EXPECT_FALSE(writer.Seek(100));
  }

  // Seeking so far that arithmetic overflow could occur also fails.
  {
    QuicDataWriter writer(ABSL_ARRAYSIZE(buffer), buffer,
                          GetParam().endianness);
    EXPECT_TRUE(writer.Seek(10));
    EXPECT_FALSE(writer.Seek(std::numeric_limits<size_t>::max()));
  }
}

TEST_P(QuicDataWriterTest, PayloadReads) {
  char buffer[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  char expected_first_read[4] = {1, 2, 3, 4};
  char expected_remaining[12] = {5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  QuicDataReader reader(buffer, sizeof(buffer));
  char first_read_buffer[4] = {};
  EXPECT_TRUE(reader.ReadBytes(first_read_buffer, sizeof(first_read_buffer)));
  quiche::test::CompareCharArraysWithHexError(
      "first read", first_read_buffer, sizeof(first_read_buffer),
      expected_first_read, sizeof(expected_first_read));
  absl::string_view peeked_remaining_payload = reader.PeekRemainingPayload();
  quiche::test::CompareCharArraysWithHexError(
      "peeked_remaining_payload", peeked_remaining_payload.data(),
      peeked_remaining_payload.length(), expected_remaining,
      sizeof(expected_remaining));
  absl::string_view full_payload = reader.FullPayload();
  quiche::test::CompareCharArraysWithHexError(
      "full_payload", full_payload.data(), full_payload.length(), buffer,
      sizeof(buffer));
  absl::string_view read_remaining_payload = reader.ReadRemainingPayload();
  quiche::test::CompareCharArraysWithHexError(
      "read_remaining_payload", read_remaining_payload.data(),
      read_remaining_payload.length(), expected_remaining,
      sizeof(expected_remaining));
  EXPECT_TRUE(reader.IsDoneReading());
  absl::string_view full_payload2 = reader.FullPayload();
  quiche::test::CompareCharArraysWithHexError(
      "full_payload2", full_payload2.data(), full_payload2.length(), buffer,
      sizeof(buffer));
}

TEST_P(QuicDataWriterTest, StringPieceVarInt62) {
  char inner_buffer[16] = {1, 2,  3,  4,  5,  6,  7,  8,
                           9, 10, 11, 12, 13, 14, 15, 16};
  absl::string_view inner_payload_write(inner_buffer, sizeof(inner_buffer));
  char buffer[sizeof(inner_buffer) + sizeof(uint8_t)] = {};
  QuicDataWriter writer(sizeof(buffer), buffer);
  EXPECT_TRUE(writer.WriteStringPieceVarInt62(inner_payload_write));
  EXPECT_EQ(0u, writer.remaining());
  QuicDataReader reader(buffer, sizeof(buffer));
  absl::string_view inner_payload_read;
  EXPECT_TRUE(reader.ReadStringPieceVarInt62(&inner_payload_read));
  quiche::test::CompareCharArraysWithHexError(
      "inner_payload", inner_payload_write.data(), inner_payload_write.length(),
      inner_payload_read.data(), inner_payload_read.length());
}

}  // namespace
}  // namespace test
}  // namespace quic
