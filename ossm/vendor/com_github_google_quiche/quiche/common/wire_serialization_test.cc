// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/wire_serialization.h"

#include <array>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_buffer_allocator.h"
#include "quiche/common/quiche_endian.h"
#include "quiche/common/quiche_status_utils.h"
#include "quiche/common/simple_buffer_allocator.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace quiche::test {
namespace {

using ::testing::ElementsAre;

constexpr uint64_t kInvalidVarInt = std::numeric_limits<uint64_t>::max();

template <typename... Ts>
absl::StatusOr<quiche::QuicheBuffer> SerializeIntoSimpleBuffer(Ts... data) {
  return SerializeIntoBuffer(quiche::SimpleBufferAllocator::Get(), data...);
}

template <typename... Ts>
void ExpectEncoding(const std::string& description, absl::string_view expected,
                    Ts... data) {
  absl::StatusOr<quiche::QuicheBuffer> actual =
      SerializeIntoSimpleBuffer(data...);
  QUICHE_ASSERT_OK(actual);
  quiche::test::CompareCharArraysWithHexError(description, actual->data(),
                                              actual->size(), expected.data(),
                                              expected.size());
}

template <typename... Ts>
void ExpectEncodingHex(const std::string& description,
                       absl::string_view expected_hex, Ts... data) {
  std::string expected;
  ASSERT_TRUE(absl::HexStringToBytes(expected_hex, &expected));
  ExpectEncoding(description, expected, data...);
}

TEST(SerializationTest, SerializeStrings) {
  absl::StatusOr<quiche::QuicheBuffer> one_string =
      SerializeIntoSimpleBuffer(WireBytes("test"));
  QUICHE_ASSERT_OK(one_string);
  EXPECT_EQ(one_string->AsStringView(), "test");

  absl::StatusOr<quiche::QuicheBuffer> two_strings =
      SerializeIntoSimpleBuffer(WireBytes("Hello"), WireBytes("World"));
  QUICHE_ASSERT_OK(two_strings);
  EXPECT_EQ(two_strings->AsStringView(), "HelloWorld");
}

TEST(SerializationTest, SerializeIntegers) {
  ExpectEncodingHex("one uint8_t value", "42", WireUint8(0x42));
  ExpectEncodingHex("two uint8_t values", "ab01", WireUint8(0xab),
                    WireUint8(0x01));
  ExpectEncodingHex("one uint16_t value", "1234", WireUint16(0x1234));
  ExpectEncodingHex("one uint32_t value", "12345678", WireUint32(0x12345678));
  ExpectEncodingHex("one uint64_t value", "123456789abcdef0",
                    WireUint64(UINT64_C(0x123456789abcdef0)));
  ExpectEncodingHex("mix of values", "aabbcc000000dd", WireUint8(0xaa),
                    WireUint16(0xbbcc), WireUint32(0xdd));
}

TEST(SerializationTest, SerializeLittleEndian) {
  char buffer[4];
  QuicheDataWriter writer(sizeof(buffer), buffer,
                          quiche::Endianness::HOST_BYTE_ORDER);
  QUICHE_ASSERT_OK(
      SerializeIntoWriter(writer, WireUint16(0x1234), WireUint16(0xabcd)));
  absl::string_view actual(writer.data(), writer.length());
  std::string expected;
  ASSERT_TRUE(absl::HexStringToBytes("3412cdab", &expected));
  EXPECT_EQ(actual, expected);
}

TEST(SerializationTest, SerializeVarInt62) {
  // Test cases from RFC 9000, Appendix A.1
  ExpectEncodingHex("1-byte varint", "25", WireVarInt62(37));
  ExpectEncodingHex("2-byte varint", "7bbd", WireVarInt62(15293));
  ExpectEncodingHex("4-byte varint", "9d7f3e7d", WireVarInt62(494878333));
  ExpectEncodingHex("8-byte varint", "c2197c5eff14e88c",
                    WireVarInt62(UINT64_C(151288809941952652)));
}

TEST(SerializationTest, SerializeStringWithVarInt62Length) {
  ExpectEncodingHex("short string", "0474657374",
                    WireStringWithVarInt62Length("test"));
  const std::string long_string(15293, 'a');
  ExpectEncoding("long string", absl::StrCat("\x7b\xbd", long_string),
                 WireStringWithVarInt62Length(long_string));
  ExpectEncodingHex("empty string", "00", WireStringWithVarInt62Length(""));
}

TEST(SerializationTest, SerializeOptionalValues) {
  std::optional<uint8_t> has_no_value;
  std::optional<uint8_t> has_value = 0x42;
  ExpectEncodingHex("optional without value", "00", WireUint8(0),
                    WireOptional<WireUint8>(has_no_value));
  ExpectEncodingHex("optional with value", "0142", WireUint8(1),
                    WireOptional<WireUint8>(has_value));
  ExpectEncodingHex("empty data", "", WireOptional<WireUint8>(has_no_value));

  std::optional<std::string> has_no_string;
  std::optional<std::string> has_string = "\x42";
  ExpectEncodingHex("optional no string", "",
                    WireOptional<WireStringWithVarInt62Length>(has_no_string));
  ExpectEncodingHex("optional string", "0142",
                    WireOptional<WireStringWithVarInt62Length>(has_string));
}

enum class TestEnum {
  kValue1 = 0x17,
  kValue2 = 0x19,
};

TEST(SerializationTest, SerializeEnumValue) {
  ExpectEncodingHex("enum value", "17", WireVarInt62(TestEnum::kValue1));
}

TEST(SerializationTest, SerializeLotsOfValues) {
  ExpectEncodingHex("ten values", "00010203040506070809", WireUint8(0),
                    WireUint8(1), WireUint8(2), WireUint8(3), WireUint8(4),
                    WireUint8(5), WireUint8(6), WireUint8(7), WireUint8(8),
                    WireUint8(9));
}

TEST(SerializationTest, FailDueToLackOfSpace) {
  char buffer[4];
  QuicheDataWriter writer(sizeof(buffer), buffer);
  QUICHE_EXPECT_OK(SerializeIntoWriter(writer, WireUint32(0)));
  ASSERT_EQ(writer.remaining(), 0u);
  EXPECT_THAT(
      SerializeIntoWriter(writer, WireUint32(0)),
      StatusIs(absl::StatusCode::kInternal, "Failed to serialize field #0"));
  EXPECT_THAT(
      SerializeIntoWriter(writer, WireStringWithVarInt62Length("test")),
      StatusIs(
          absl::StatusCode::kInternal,
          "Failed to serialize the length prefix while serializing field #0"));
}

TEST(SerializationTest, FailDueToInvalidValue) {
  EXPECT_QUICHE_BUG(
      ExpectEncoding("invalid varint", "", WireVarInt62(kInvalidVarInt)),
      "too big for VarInt62");
}

TEST(SerializationTest, InvalidValueCausesPartialWrite) {
  char buffer[3] = {'\0'};
  QuicheDataWriter writer(sizeof(buffer), buffer);
  QUICHE_EXPECT_OK(SerializeIntoWriter(writer, WireBytes("a")));
  EXPECT_THAT(
      SerializeIntoWriter(writer, WireBytes("b"),
                          WireBytes("A considerably long string, writing which "
                                    "will most likely cause ASAN to crash"),
                          WireBytes("c")),
      StatusIs(absl::StatusCode::kInternal, "Failed to serialize field #1"));
  EXPECT_THAT(buffer, ElementsAre('a', 'b', '\0'));

  QUICHE_EXPECT_OK(SerializeIntoWriter(writer, WireBytes("z")));
  EXPECT_EQ(buffer[2], 'z');
}

TEST(SerializationTest, SerializeVector) {
  std::vector<absl::string_view> strs = {"foo", "test", "bar"};
  absl::StatusOr<quiche::QuicheBuffer> serialized =
      SerializeIntoSimpleBuffer(WireSpan<WireBytes>(absl::MakeSpan(strs)));
  QUICHE_ASSERT_OK(serialized);
  EXPECT_EQ(serialized->AsStringView(), "footestbar");
}

struct AwesomeStruct {
  uint64_t awesome_number;
  std::string awesome_text;
};

class WireAwesomeStruct {
 public:
  using DataType = AwesomeStruct;

  WireAwesomeStruct(const AwesomeStruct& awesome) : awesome_(awesome) {}

  size_t GetLengthOnWire() {
    return quiche::ComputeLengthOnWire(WireUint16(awesome_.awesome_number),
                                       WireBytes(awesome_.awesome_text));
  }
  absl::Status SerializeIntoWriter(QuicheDataWriter& writer) {
    return AppendToStatus(::quiche::SerializeIntoWriter(
                              writer, WireUint16(awesome_.awesome_number),
                              WireBytes(awesome_.awesome_text)),
                          " while serializing AwesomeStruct");
  }

 private:
  const AwesomeStruct& awesome_;
};

TEST(SerializationTest, CustomStruct) {
  AwesomeStruct awesome;
  awesome.awesome_number = 0xabcd;
  awesome.awesome_text = "test";
  ExpectEncodingHex("struct", "abcd74657374", WireAwesomeStruct(awesome));
}

TEST(SerializationTest, CustomStructSpan) {
  std::array<AwesomeStruct, 2> awesome;
  awesome[0].awesome_number = 0xabcd;
  awesome[0].awesome_text = "test";
  awesome[1].awesome_number = 0x1234;
  awesome[1].awesome_text = std::string(3, '\0');
  ExpectEncodingHex("struct", "abcd746573741234000000",
                    WireSpan<WireAwesomeStruct>(absl::MakeSpan(awesome)));
}

class WireFormatterThatWritesTooLittle {
 public:
  using DataType = absl::string_view;

  explicit WireFormatterThatWritesTooLittle(absl::string_view s) : s_(s) {}

  size_t GetLengthOnWire() const { return s_.size(); }
  bool SerializeIntoWriter(QuicheDataWriter& writer) {
    return writer.WriteStringPiece(s_.substr(0, s_.size() - 1));
  }

 private:
  absl::string_view s_;
};

TEST(SerializationTest, CustomStructWritesTooLittle) {
  absl::Status status;
#if defined(NDEBUG)
  constexpr absl::string_view kStr = "\xaa\xbb\xcc\xdd";
  status = SerializeIntoSimpleBuffer(WireFormatterThatWritesTooLittle(kStr))
               .status();
  EXPECT_THAT(status, StatusIs(absl::StatusCode::kInternal,
                               ::testing::HasSubstr("Excess 1 bytes")));
#elif GTEST_HAS_DEATH_TEST
  constexpr absl::string_view kStr = "\xaa\xbb\xcc\xdd";
  EXPECT_QUICHE_DEBUG_DEATH(
      status = SerializeIntoSimpleBuffer(WireFormatterThatWritesTooLittle(kStr))
                   .status(),
      "while serializing field #0");
  EXPECT_THAT(status, StatusIs(absl::StatusCode::kOk));
#endif
}

TEST(SerializationTest, Empty) { ExpectEncodingHex("nothing", ""); }

}  // namespace
}  // namespace quiche::test
