// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/quiche_data_writer.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "absl/base/macros.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_data_reader.h"
#include "quiche/common/quiche_endian.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace quiche {
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

class QuicheDataWriterTest : public QuicheTestWithParam<TestParams> {};

INSTANTIATE_TEST_SUITE_P(QuicheDataWriterTests, QuicheDataWriterTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicheDataWriterTest, Write16BitUnsignedIntegers) {
  char little_endian16[] = {0x22, 0x11};
  char big_endian16[] = {0x11, 0x22};
  char buffer16[2];
  {
    uint16_t in_memory16 = 0x1122;
    QuicheDataWriter writer(2, buffer16, GetParam().endianness);
    writer.WriteUInt16(in_memory16);
    test::CompareCharArraysWithHexError(
        "uint16_t", buffer16, 2,
        GetParam().endianness == quiche::NETWORK_BYTE_ORDER ? big_endian16
                                                            : little_endian16,
        2);

    uint16_t read_number16;
    QuicheDataReader reader(buffer16, 2, GetParam().endianness);
    reader.ReadUInt16(&read_number16);
    EXPECT_EQ(in_memory16, read_number16);
  }

  {
    uint64_t in_memory16 = 0x0000000000001122;
    QuicheDataWriter writer(2, buffer16, GetParam().endianness);
    writer.WriteBytesToUInt64(2, in_memory16);
    test::CompareCharArraysWithHexError(
        "uint16_t", buffer16, 2,
        GetParam().endianness == quiche::NETWORK_BYTE_ORDER ? big_endian16
                                                            : little_endian16,
        2);

    uint64_t read_number16;
    QuicheDataReader reader(buffer16, 2, GetParam().endianness);
    reader.ReadBytesToUInt64(2, &read_number16);
    EXPECT_EQ(in_memory16, read_number16);
  }
}

TEST_P(QuicheDataWriterTest, Write24BitUnsignedIntegers) {
  char little_endian24[] = {0x33, 0x22, 0x11};
  char big_endian24[] = {0x11, 0x22, 0x33};
  char buffer24[3];
  uint64_t in_memory24 = 0x0000000000112233;
  QuicheDataWriter writer(3, buffer24, GetParam().endianness);
  writer.WriteBytesToUInt64(3, in_memory24);
  test::CompareCharArraysWithHexError(
      "uint24", buffer24, 3,
      GetParam().endianness == quiche::NETWORK_BYTE_ORDER ? big_endian24
                                                          : little_endian24,
      3);

  uint64_t read_number24;
  QuicheDataReader reader(buffer24, 3, GetParam().endianness);
  reader.ReadBytesToUInt64(3, &read_number24);
  EXPECT_EQ(in_memory24, read_number24);
}

TEST_P(QuicheDataWriterTest, Write32BitUnsignedIntegers) {
  char little_endian32[] = {0x44, 0x33, 0x22, 0x11};
  char big_endian32[] = {0x11, 0x22, 0x33, 0x44};
  char buffer32[4];
  {
    uint32_t in_memory32 = 0x11223344;
    QuicheDataWriter writer(4, buffer32, GetParam().endianness);
    writer.WriteUInt32(in_memory32);
    test::CompareCharArraysWithHexError(
        "uint32_t", buffer32, 4,
        GetParam().endianness == quiche::NETWORK_BYTE_ORDER ? big_endian32
                                                            : little_endian32,
        4);

    uint32_t read_number32;
    QuicheDataReader reader(buffer32, 4, GetParam().endianness);
    reader.ReadUInt32(&read_number32);
    EXPECT_EQ(in_memory32, read_number32);
  }

  {
    uint64_t in_memory32 = 0x11223344;
    QuicheDataWriter writer(4, buffer32, GetParam().endianness);
    writer.WriteBytesToUInt64(4, in_memory32);
    test::CompareCharArraysWithHexError(
        "uint32_t", buffer32, 4,
        GetParam().endianness == quiche::NETWORK_BYTE_ORDER ? big_endian32
                                                            : little_endian32,
        4);

    uint64_t read_number32;
    QuicheDataReader reader(buffer32, 4, GetParam().endianness);
    reader.ReadBytesToUInt64(4, &read_number32);
    EXPECT_EQ(in_memory32, read_number32);
  }
}

TEST_P(QuicheDataWriterTest, Write40BitUnsignedIntegers) {
  uint64_t in_memory40 = 0x0000001122334455;
  char little_endian40[] = {0x55, 0x44, 0x33, 0x22, 0x11};
  char big_endian40[] = {0x11, 0x22, 0x33, 0x44, 0x55};
  char buffer40[5];
  QuicheDataWriter writer(5, buffer40, GetParam().endianness);
  writer.WriteBytesToUInt64(5, in_memory40);
  test::CompareCharArraysWithHexError(
      "uint40", buffer40, 5,
      GetParam().endianness == quiche::NETWORK_BYTE_ORDER ? big_endian40
                                                          : little_endian40,
      5);

  uint64_t read_number40;
  QuicheDataReader reader(buffer40, 5, GetParam().endianness);
  reader.ReadBytesToUInt64(5, &read_number40);
  EXPECT_EQ(in_memory40, read_number40);
}

TEST_P(QuicheDataWriterTest, Write48BitUnsignedIntegers) {
  uint64_t in_memory48 = 0x0000112233445566;
  char little_endian48[] = {0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
  char big_endian48[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  char buffer48[6];
  QuicheDataWriter writer(6, buffer48, GetParam().endianness);
  writer.WriteBytesToUInt64(6, in_memory48);
  test::CompareCharArraysWithHexError(
      "uint48", buffer48, 6,
      GetParam().endianness == quiche::NETWORK_BYTE_ORDER ? big_endian48
                                                          : little_endian48,
      6);

  uint64_t read_number48;
  QuicheDataReader reader(buffer48, 6, GetParam().endianness);
  reader.ReadBytesToUInt64(6., &read_number48);
  EXPECT_EQ(in_memory48, read_number48);
}

TEST_P(QuicheDataWriterTest, Write56BitUnsignedIntegers) {
  uint64_t in_memory56 = 0x0011223344556677;
  char little_endian56[] = {0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
  char big_endian56[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
  char buffer56[7];
  QuicheDataWriter writer(7, buffer56, GetParam().endianness);
  writer.WriteBytesToUInt64(7, in_memory56);
  test::CompareCharArraysWithHexError(
      "uint56", buffer56, 7,
      GetParam().endianness == quiche::NETWORK_BYTE_ORDER ? big_endian56
                                                          : little_endian56,
      7);

  uint64_t read_number56;
  QuicheDataReader reader(buffer56, 7, GetParam().endianness);
  reader.ReadBytesToUInt64(7, &read_number56);
  EXPECT_EQ(in_memory56, read_number56);
}

TEST_P(QuicheDataWriterTest, Write64BitUnsignedIntegers) {
  uint64_t in_memory64 = 0x1122334455667788;
  unsigned char little_endian64[] = {0x88, 0x77, 0x66, 0x55,
                                     0x44, 0x33, 0x22, 0x11};
  unsigned char big_endian64[] = {0x11, 0x22, 0x33, 0x44,
                                  0x55, 0x66, 0x77, 0x88};
  char buffer64[8];
  QuicheDataWriter writer(8, buffer64, GetParam().endianness);
  writer.WriteBytesToUInt64(8, in_memory64);
  test::CompareCharArraysWithHexError(
      "uint64_t", buffer64, 8,
      GetParam().endianness == quiche::NETWORK_BYTE_ORDER
          ? AsChars(big_endian64)
          : AsChars(little_endian64),
      8);

  uint64_t read_number64;
  QuicheDataReader reader(buffer64, 8, GetParam().endianness);
  reader.ReadBytesToUInt64(8, &read_number64);
  EXPECT_EQ(in_memory64, read_number64);

  QuicheDataWriter writer2(8, buffer64, GetParam().endianness);
  writer2.WriteUInt64(in_memory64);
  test::CompareCharArraysWithHexError(
      "uint64_t", buffer64, 8,
      GetParam().endianness == quiche::NETWORK_BYTE_ORDER
          ? AsChars(big_endian64)
          : AsChars(little_endian64),
      8);
  read_number64 = 0u;
  QuicheDataReader reader2(buffer64, 8, GetParam().endianness);
  reader2.ReadUInt64(&read_number64);
  EXPECT_EQ(in_memory64, read_number64);
}

TEST_P(QuicheDataWriterTest, WriteIntegers) {
  char buf[43];
  uint8_t i8 = 0x01;
  uint16_t i16 = 0x0123;
  uint32_t i32 = 0x01234567;
  uint64_t i64 = 0x0123456789ABCDEF;
  QuicheDataWriter writer(46, buf, GetParam().endianness);
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

  QuicheDataReader reader(buf, 46, GetParam().endianness);
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

TEST_P(QuicheDataWriterTest, WriteBytes) {
  char bytes[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  char buf[ABSL_ARRAYSIZE(bytes)];
  QuicheDataWriter writer(ABSL_ARRAYSIZE(buf), buf, GetParam().endianness);
  EXPECT_TRUE(writer.WriteBytes(bytes, ABSL_ARRAYSIZE(bytes)));
  for (unsigned int i = 0; i < ABSL_ARRAYSIZE(bytes); ++i) {
    EXPECT_EQ(bytes[i], buf[i]);
  }
}

const int kVarIntBufferLength = 1024;

// Encodes and then decodes a specified value, checks that the
// value that was encoded is the same as the decoded value, the length
// is correct, and that after decoding, all data in the buffer has
// been consumed..
// Returns true if everything works, false if not.
bool EncodeDecodeValue(uint64_t value_in, char* buffer, size_t size_of_buffer) {
  // Init the buffer to all 0, just for cleanliness. Makes for better
  // output if, in debugging, we need to dump out the buffer.
  memset(buffer, 0, size_of_buffer);
  // make a writer. Note that for IETF encoding
  // we do not care about endianness... It's always big-endian,
  // but the c'tor expects to be told what endianness is in force...
  QuicheDataWriter writer(size_of_buffer, buffer,
                          quiche::Endianness::NETWORK_BYTE_ORDER);

  // Try to write the value.
  if (writer.WriteVarInt62(value_in) != true) {
    return false;
  }
  // Look at the value we encoded. Determine how much should have been
  // used based on the value, and then check the state of the writer
  // to see that it matches.
  size_t expected_length = 0;
  if (value_in <= 0x3f) {
    expected_length = 1;
  } else if (value_in <= 0x3fff) {
    expected_length = 2;
  } else if (value_in <= 0x3fffffff) {
    expected_length = 4;
  } else {
    expected_length = 8;
  }
  if (writer.length() != expected_length) {
    return false;
  }

  // set up a reader, just the length we've used, no more, no less.
  QuicheDataReader reader(buffer, expected_length,
                          quiche::Endianness::NETWORK_BYTE_ORDER);
  uint64_t value_out;

  if (reader.ReadVarInt62(&value_out) == false) {
    return false;
  }
  if (value_in != value_out) {
    return false;
  }
  // We only write one value so there had better be nothing left to read
  return reader.IsDoneReading();
}

// Test that 8-byte-encoded Variable Length Integers are properly laid
// out in the buffer.
TEST_P(QuicheDataWriterTest, VarInt8Layout) {
  char buffer[1024];

  // Check that the layout of bytes in the buffer is correct. Bytes
  // are always encoded big endian...
  memset(buffer, 0, sizeof(buffer));
  QuicheDataWriter writer(sizeof(buffer), static_cast<char*>(buffer),
                          quiche::Endianness::NETWORK_BYTE_ORDER);
  EXPECT_TRUE(writer.WriteVarInt62(UINT64_C(0x3142f3e4d5c6b7a8)));
  EXPECT_EQ(static_cast<unsigned char>(*(writer.data() + 0)),
            (0x31 + 0xc0));  // 0xc0 for encoding
  EXPECT_EQ(static_cast<unsigned char>(*(writer.data() + 1)), 0x42);
  EXPECT_EQ(static_cast<unsigned char>(*(writer.data() + 2)), 0xf3);
  EXPECT_EQ(static_cast<unsigned char>(*(writer.data() + 3)), 0xe4);
  EXPECT_EQ(static_cast<unsigned char>(*(writer.data() + 4)), 0xd5);
  EXPECT_EQ(static_cast<unsigned char>(*(writer.data() + 5)), 0xc6);
  EXPECT_EQ(static_cast<unsigned char>(*(writer.data() + 6)), 0xb7);
  EXPECT_EQ(static_cast<unsigned char>(*(writer.data() + 7)), 0xa8);
}

// Test that 4-byte-encoded Variable Length Integers are properly laid
// out in the buffer.
TEST_P(QuicheDataWriterTest, VarInt4Layout) {
  char buffer[1024];

  // Check that the layout of bytes in the buffer is correct. Bytes
  // are always encoded big endian...
  memset(buffer, 0, sizeof(buffer));
  QuicheDataWriter writer(sizeof(buffer), static_cast<char*>(buffer),
                          quiche::Endianness::NETWORK_BYTE_ORDER);
  EXPECT_TRUE(writer.WriteVarInt62(0x3243f4e5));
  EXPECT_EQ(static_cast<unsigned char>(*(writer.data() + 0)),
            (0x32 + 0x80));  // 0x80 for encoding
  EXPECT_EQ(static_cast<unsigned char>(*(writer.data() + 1)), 0x43);
  EXPECT_EQ(static_cast<unsigned char>(*(writer.data() + 2)), 0xf4);
  EXPECT_EQ(static_cast<unsigned char>(*(writer.data() + 3)), 0xe5);
}

// Test that 2-byte-encoded Variable Length Integers are properly laid
// out in the buffer.
TEST_P(QuicheDataWriterTest, VarInt2Layout) {
  char buffer[1024];

  // Check that the layout of bytes in the buffer is correct. Bytes
  // are always encoded big endian...
  memset(buffer, 0, sizeof(buffer));
  QuicheDataWriter writer(sizeof(buffer), static_cast<char*>(buffer),
                          quiche::Endianness::NETWORK_BYTE_ORDER);
  EXPECT_TRUE(writer.WriteVarInt62(0x3647));
  EXPECT_EQ(static_cast<unsigned char>(*(writer.data() + 0)),
            (0x36 + 0x40));  // 0x40 for encoding
  EXPECT_EQ(static_cast<unsigned char>(*(writer.data() + 1)), 0x47);
}

// Test that 1-byte-encoded Variable Length Integers are properly laid
// out in the buffer.
TEST_P(QuicheDataWriterTest, VarInt1Layout) {
  char buffer[1024];

  // Check that the layout of bytes in the buffer
  // is correct. Bytes are always encoded big endian...
  memset(buffer, 0, sizeof(buffer));
  QuicheDataWriter writer(sizeof(buffer), static_cast<char*>(buffer),
                          quiche::Endianness::NETWORK_BYTE_ORDER);
  EXPECT_TRUE(writer.WriteVarInt62(0x3f));
  EXPECT_EQ(static_cast<unsigned char>(*(writer.data() + 0)), 0x3f);
}

// Test certain, targeted, values that are expected to succeed:
// 0, 1,
// 0x3e, 0x3f, 0x40, 0x41 (around the 1-2 byte transitions)
// 0x3ffe, 0x3fff, 0x4000, 0x4001 (the 2-4 byte transition)
// 0x3ffffffe, 0x3fffffff, 0x40000000, 0x40000001 (the 4-8 byte
//                          transition)
// 0x3ffffffffffffffe, 0x3fffffffffffffff,  (the highest valid values)
// 0xfe, 0xff, 0x100, 0x101,
// 0xfffe, 0xffff, 0x10000, 0x10001,
// 0xfffffe, 0xffffff, 0x1000000, 0x1000001,
// 0xfffffffe, 0xffffffff, 0x100000000, 0x100000001,
// 0xfffffffffe, 0xffffffffff, 0x10000000000, 0x10000000001,
// 0xfffffffffffe, 0xffffffffffff, 0x1000000000000, 0x1000000000001,
// 0xfffffffffffffe, 0xffffffffffffff, 0x100000000000000, 0x100000000000001,
TEST_P(QuicheDataWriterTest, VarIntGoodTargetedValues) {
  char buffer[kVarIntBufferLength];
  uint64_t passing_values[] = {
      0,
      1,
      0x3e,
      0x3f,
      0x40,
      0x41,
      0x3ffe,
      0x3fff,
      0x4000,
      0x4001,
      0x3ffffffe,
      0x3fffffff,
      0x40000000,
      0x40000001,
      0x3ffffffffffffffe,
      0x3fffffffffffffff,
      0xfe,
      0xff,
      0x100,
      0x101,
      0xfffe,
      0xffff,
      0x10000,
      0x10001,
      0xfffffe,
      0xffffff,
      0x1000000,
      0x1000001,
      0xfffffffe,
      0xffffffff,
      0x100000000,
      0x100000001,
      0xfffffffffe,
      0xffffffffff,
      0x10000000000,
      0x10000000001,
      0xfffffffffffe,
      0xffffffffffff,
      0x1000000000000,
      0x1000000000001,
      0xfffffffffffffe,
      0xffffffffffffff,
      0x100000000000000,
      0x100000000000001,
  };
  for (uint64_t test_val : passing_values) {
    EXPECT_TRUE(
        EncodeDecodeValue(test_val, static_cast<char*>(buffer), sizeof(buffer)))
        << " encode/decode of " << test_val << " failed";
  }
}
//
// Test certain, targeted, values where failure is expected (the
// values are invalid w.r.t. IETF VarInt encoding):
// 0x4000000000000000, 0x4000000000000001,  ( Just above max allowed value)
// 0xfffffffffffffffe, 0xffffffffffffffff,  (should fail)
TEST_P(QuicheDataWriterTest, VarIntBadTargetedValues) {
  char buffer[kVarIntBufferLength];
  uint64_t failing_values[] = {
      0x4000000000000000,
      0x4000000000000001,
      0xfffffffffffffffe,
      0xffffffffffffffff,
  };
  for (uint64_t test_val : failing_values) {
    EXPECT_FALSE(
        EncodeDecodeValue(test_val, static_cast<char*>(buffer), sizeof(buffer)))
        << " encode/decode of " << test_val << " succeeded, but was an "
        << "invalid value";
  }
}
// Test writing varints with a forced length.
TEST_P(QuicheDataWriterTest, WriteVarInt62WithForcedLength) {
  char buffer[90];
  memset(buffer, 0, sizeof(buffer));
  QuicheDataWriter writer(sizeof(buffer), static_cast<char*>(buffer));

  writer.WriteVarInt62WithForcedLength(1, VARIABLE_LENGTH_INTEGER_LENGTH_1);
  writer.WriteVarInt62WithForcedLength(1, VARIABLE_LENGTH_INTEGER_LENGTH_2);
  writer.WriteVarInt62WithForcedLength(1, VARIABLE_LENGTH_INTEGER_LENGTH_4);
  writer.WriteVarInt62WithForcedLength(1, VARIABLE_LENGTH_INTEGER_LENGTH_8);

  writer.WriteVarInt62WithForcedLength(63, VARIABLE_LENGTH_INTEGER_LENGTH_1);
  writer.WriteVarInt62WithForcedLength(63, VARIABLE_LENGTH_INTEGER_LENGTH_2);
  writer.WriteVarInt62WithForcedLength(63, VARIABLE_LENGTH_INTEGER_LENGTH_4);
  writer.WriteVarInt62WithForcedLength(63, VARIABLE_LENGTH_INTEGER_LENGTH_8);

  writer.WriteVarInt62WithForcedLength(64, VARIABLE_LENGTH_INTEGER_LENGTH_2);
  writer.WriteVarInt62WithForcedLength(64, VARIABLE_LENGTH_INTEGER_LENGTH_4);
  writer.WriteVarInt62WithForcedLength(64, VARIABLE_LENGTH_INTEGER_LENGTH_8);

  writer.WriteVarInt62WithForcedLength(16383, VARIABLE_LENGTH_INTEGER_LENGTH_2);
  writer.WriteVarInt62WithForcedLength(16383, VARIABLE_LENGTH_INTEGER_LENGTH_4);
  writer.WriteVarInt62WithForcedLength(16383, VARIABLE_LENGTH_INTEGER_LENGTH_8);

  writer.WriteVarInt62WithForcedLength(16384, VARIABLE_LENGTH_INTEGER_LENGTH_4);
  writer.WriteVarInt62WithForcedLength(16384, VARIABLE_LENGTH_INTEGER_LENGTH_8);

  writer.WriteVarInt62WithForcedLength(1073741823,
                                       VARIABLE_LENGTH_INTEGER_LENGTH_4);
  writer.WriteVarInt62WithForcedLength(1073741823,
                                       VARIABLE_LENGTH_INTEGER_LENGTH_8);

  writer.WriteVarInt62WithForcedLength(1073741824,
                                       VARIABLE_LENGTH_INTEGER_LENGTH_8);

  QuicheDataReader reader(buffer, sizeof(buffer));

  uint64_t test_val = 0;
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(reader.ReadVarInt62(&test_val));
    EXPECT_EQ(test_val, 1u);
  }
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(reader.ReadVarInt62(&test_val));
    EXPECT_EQ(test_val, 63u);
  }

  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(reader.ReadVarInt62(&test_val));
    EXPECT_EQ(test_val, 64u);
  }
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(reader.ReadVarInt62(&test_val));
    EXPECT_EQ(test_val, 16383u);
  }

  for (int i = 0; i < 2; ++i) {
    EXPECT_TRUE(reader.ReadVarInt62(&test_val));
    EXPECT_EQ(test_val, 16384u);
  }
  for (int i = 0; i < 2; ++i) {
    EXPECT_TRUE(reader.ReadVarInt62(&test_val));
    EXPECT_EQ(test_val, 1073741823u);
  }

  EXPECT_TRUE(reader.ReadVarInt62(&test_val));
  EXPECT_EQ(test_val, 1073741824u);

  // We are at the end of the buffer so this should fail.
  EXPECT_FALSE(reader.ReadVarInt62(&test_val));
}

// Following tests all try to fill the buffer with multiple values,
// go one value more than the buffer can accommodate, then read
// the successfully encoded values, and try to read the unsuccessfully
// encoded value. The following is the number of values to encode.
const int kMultiVarCount = 1000;

// Test writing & reading multiple 8-byte-encoded varints
TEST_P(QuicheDataWriterTest, MultiVarInt8) {
  uint64_t test_val;
  char buffer[8 * kMultiVarCount];
  memset(buffer, 0, sizeof(buffer));
  QuicheDataWriter writer(sizeof(buffer), static_cast<char*>(buffer),
                          quiche::Endianness::NETWORK_BYTE_ORDER);
  // Put N values into the buffer. Adding i to the value ensures that
  // each value is different so we can detect if we overwrite values,
  // or read the same value over and over.
  for (int i = 0; i < kMultiVarCount; i++) {
    EXPECT_TRUE(writer.WriteVarInt62(UINT64_C(0x3142f3e4d5c6b7a8) + i));
  }
  EXPECT_EQ(writer.length(), 8u * kMultiVarCount);

  // N+1st should fail, the buffer is full.
  EXPECT_FALSE(writer.WriteVarInt62(UINT64_C(0x3142f3e4d5c6b7a8)));

  // Now we should be able to read out the N values that were
  // successfully encoded.
  QuicheDataReader reader(buffer, sizeof(buffer),
                          quiche::Endianness::NETWORK_BYTE_ORDER);
  for (int i = 0; i < kMultiVarCount; i++) {
    EXPECT_TRUE(reader.ReadVarInt62(&test_val));
    EXPECT_EQ(test_val, (UINT64_C(0x3142f3e4d5c6b7a8) + i));
  }
  // And the N+1st should fail.
  EXPECT_FALSE(reader.ReadVarInt62(&test_val));
}

// Test writing & reading multiple 4-byte-encoded varints
TEST_P(QuicheDataWriterTest, MultiVarInt4) {
  uint64_t test_val;
  char buffer[4 * kMultiVarCount];
  memset(buffer, 0, sizeof(buffer));
  QuicheDataWriter writer(sizeof(buffer), static_cast<char*>(buffer),
                          quiche::Endianness::NETWORK_BYTE_ORDER);
  // Put N values into the buffer. Adding i to the value ensures that
  // each value is different so we can detect if we overwrite values,
  // or read the same value over and over.
  for (int i = 0; i < kMultiVarCount; i++) {
    EXPECT_TRUE(writer.WriteVarInt62(UINT64_C(0x3142f3e4) + i));
  }
  EXPECT_EQ(writer.length(), 4u * kMultiVarCount);

  // N+1st should fail, the buffer is full.
  EXPECT_FALSE(writer.WriteVarInt62(UINT64_C(0x3142f3e4)));

  // Now we should be able to read out the N values that were
  // successfully encoded.
  QuicheDataReader reader(buffer, sizeof(buffer),
                          quiche::Endianness::NETWORK_BYTE_ORDER);
  for (int i = 0; i < kMultiVarCount; i++) {
    EXPECT_TRUE(reader.ReadVarInt62(&test_val));
    EXPECT_EQ(test_val, (UINT64_C(0x3142f3e4) + i));
  }
  // And the N+1st should fail.
  EXPECT_FALSE(reader.ReadVarInt62(&test_val));
}

// Test writing & reading multiple 2-byte-encoded varints
TEST_P(QuicheDataWriterTest, MultiVarInt2) {
  uint64_t test_val;
  char buffer[2 * kMultiVarCount];
  memset(buffer, 0, sizeof(buffer));
  QuicheDataWriter writer(sizeof(buffer), static_cast<char*>(buffer),
                          quiche::Endianness::NETWORK_BYTE_ORDER);
  // Put N values into the buffer. Adding i to the value ensures that
  // each value is different so we can detect if we overwrite values,
  // or read the same value over and over.
  for (int i = 0; i < kMultiVarCount; i++) {
    EXPECT_TRUE(writer.WriteVarInt62(UINT64_C(0x3142) + i));
  }
  EXPECT_EQ(writer.length(), 2u * kMultiVarCount);

  // N+1st should fail, the buffer is full.
  EXPECT_FALSE(writer.WriteVarInt62(UINT64_C(0x3142)));

  // Now we should be able to read out the N values that were
  // successfully encoded.
  QuicheDataReader reader(buffer, sizeof(buffer),
                          quiche::Endianness::NETWORK_BYTE_ORDER);
  for (int i = 0; i < kMultiVarCount; i++) {
    EXPECT_TRUE(reader.ReadVarInt62(&test_val));
    EXPECT_EQ(test_val, (UINT64_C(0x3142) + i));
  }
  // And the N+1st should fail.
  EXPECT_FALSE(reader.ReadVarInt62(&test_val));
}

// Test writing & reading multiple 1-byte-encoded varints
TEST_P(QuicheDataWriterTest, MultiVarInt1) {
  uint64_t test_val;
  char buffer[1 * kMultiVarCount];
  memset(buffer, 0, sizeof(buffer));
  QuicheDataWriter writer(sizeof(buffer), static_cast<char*>(buffer),
                          quiche::Endianness::NETWORK_BYTE_ORDER);
  // Put N values into the buffer. Adding i to the value ensures that
  // each value is different so we can detect if we overwrite values,
  // or read the same value over and over. &0xf ensures we do not
  // overflow the max value for single-byte encoding.
  for (int i = 0; i < kMultiVarCount; i++) {
    EXPECT_TRUE(writer.WriteVarInt62(UINT64_C(0x30) + (i & 0xf)));
  }
  EXPECT_EQ(writer.length(), 1u * kMultiVarCount);

  // N+1st should fail, the buffer is full.
  EXPECT_FALSE(writer.WriteVarInt62(UINT64_C(0x31)));

  // Now we should be able to read out the N values that were
  // successfully encoded.
  QuicheDataReader reader(buffer, sizeof(buffer),
                          quiche::Endianness::NETWORK_BYTE_ORDER);
  for (int i = 0; i < kMultiVarCount; i++) {
    EXPECT_TRUE(reader.ReadVarInt62(&test_val));
    EXPECT_EQ(test_val, (UINT64_C(0x30) + (i & 0xf)));
  }
  // And the N+1st should fail.
  EXPECT_FALSE(reader.ReadVarInt62(&test_val));
}

TEST_P(QuicheDataWriterTest, Seek) {
  char buffer[3] = {};
  QuicheDataWriter writer(ABSL_ARRAYSIZE(buffer), buffer,
                          GetParam().endianness);
  EXPECT_TRUE(writer.WriteUInt8(42));
  EXPECT_TRUE(writer.Seek(1));
  EXPECT_TRUE(writer.WriteUInt8(3));

  char expected[] = {42, 0, 3};
  for (size_t i = 0; i < ABSL_ARRAYSIZE(expected); ++i) {
    EXPECT_EQ(buffer[i], expected[i]);
  }
}

TEST_P(QuicheDataWriterTest, SeekTooFarFails) {
  char buffer[20];

  // Check that one can seek to the end of the writer, but not past.
  {
    QuicheDataWriter writer(ABSL_ARRAYSIZE(buffer), buffer,
                            GetParam().endianness);
    EXPECT_TRUE(writer.Seek(20));
    EXPECT_FALSE(writer.Seek(1));
  }

  // Seeking several bytes past the end fails.
  {
    QuicheDataWriter writer(ABSL_ARRAYSIZE(buffer), buffer,
                            GetParam().endianness);
    EXPECT_FALSE(writer.Seek(100));
  }

  // Seeking so far that arithmetic overflow could occur also fails.
  {
    QuicheDataWriter writer(ABSL_ARRAYSIZE(buffer), buffer,
                            GetParam().endianness);
    EXPECT_TRUE(writer.Seek(10));
    EXPECT_FALSE(writer.Seek(std::numeric_limits<size_t>::max()));
  }
}

TEST_P(QuicheDataWriterTest, PayloadReads) {
  char buffer[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  char expected_first_read[4] = {1, 2, 3, 4};
  char expected_remaining[12] = {5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  QuicheDataReader reader(buffer, sizeof(buffer));
  absl::string_view previously_read_payload1 = reader.PreviouslyReadPayload();
  EXPECT_TRUE(previously_read_payload1.empty());
  char first_read_buffer[4] = {};
  EXPECT_TRUE(reader.ReadBytes(first_read_buffer, sizeof(first_read_buffer)));
  test::CompareCharArraysWithHexError(
      "first read", first_read_buffer, sizeof(first_read_buffer),
      expected_first_read, sizeof(expected_first_read));
  absl::string_view peeked_remaining_payload = reader.PeekRemainingPayload();
  test::CompareCharArraysWithHexError(
      "peeked_remaining_payload", peeked_remaining_payload.data(),
      peeked_remaining_payload.length(), expected_remaining,
      sizeof(expected_remaining));
  absl::string_view full_payload = reader.FullPayload();
  test::CompareCharArraysWithHexError("full_payload", full_payload.data(),
                                      full_payload.length(), buffer,
                                      sizeof(buffer));
  absl::string_view previously_read_payload2 = reader.PreviouslyReadPayload();
  test::CompareCharArraysWithHexError(
      "previously_read_payload2", previously_read_payload2.data(),
      previously_read_payload2.length(), first_read_buffer,
      sizeof(first_read_buffer));
  absl::string_view read_remaining_payload = reader.ReadRemainingPayload();
  test::CompareCharArraysWithHexError(
      "read_remaining_payload", read_remaining_payload.data(),
      read_remaining_payload.length(), expected_remaining,
      sizeof(expected_remaining));
  EXPECT_TRUE(reader.IsDoneReading());
  absl::string_view full_payload2 = reader.FullPayload();
  test::CompareCharArraysWithHexError("full_payload2", full_payload2.data(),
                                      full_payload2.length(), buffer,
                                      sizeof(buffer));
  absl::string_view previously_read_payload3 = reader.PreviouslyReadPayload();
  test::CompareCharArraysWithHexError(
      "previously_read_payload3", previously_read_payload3.data(),
      previously_read_payload3.length(), buffer, sizeof(buffer));
}

}  // namespace
}  // namespace test
}  // namespace quiche
