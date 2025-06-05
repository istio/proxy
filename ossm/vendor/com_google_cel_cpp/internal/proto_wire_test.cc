// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "internal/proto_wire.h"

#include <limits>

#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "internal/testing.h"

namespace cel::internal {

template <typename T>
inline constexpr bool operator==(const VarintDecodeResult<T>& lhs,
                                 const VarintDecodeResult<T>& rhs) {
  return lhs.value == rhs.value && lhs.size_bytes == rhs.size_bytes;
}

inline constexpr bool operator==(const ProtoWireTag& lhs,
                                 const ProtoWireTag& rhs) {
  return lhs.field_number() == rhs.field_number() && lhs.type() == rhs.type();
}

namespace {

using ::absl_testing::IsOkAndHolds;
using ::testing::Eq;
using ::testing::Optional;

TEST(Varint, Size) {
  EXPECT_EQ(VarintSize(int32_t{-1}),
            VarintSize(std::numeric_limits<uint64_t>::max()));
  EXPECT_EQ(VarintSize(int64_t{-1}),
            VarintSize(std::numeric_limits<uint64_t>::max()));
}

TEST(Varint, MaxSize) {
  EXPECT_EQ(kMaxVarintSize<bool>, 1);
  EXPECT_EQ(kMaxVarintSize<int32_t>, 10);
  EXPECT_EQ(kMaxVarintSize<int64_t>, 10);
  EXPECT_EQ(kMaxVarintSize<uint32_t>, 5);
  EXPECT_EQ(kMaxVarintSize<uint64_t>, 10);
}

namespace {

template <typename T>
absl::Cord VarintEncode(T value) {
  absl::Cord cord;
  internal::VarintEncode(value, cord);
  return cord;
}

}  // namespace

TEST(Varint, Encode) {
  EXPECT_EQ(VarintEncode(true), "\x01");
  EXPECT_EQ(VarintEncode(int32_t{1}), "\x01");
  EXPECT_EQ(VarintEncode(int64_t{1}), "\x01");
  EXPECT_EQ(VarintEncode(uint32_t{1}), "\x01");
  EXPECT_EQ(VarintEncode(uint64_t{1}), "\x01");
  EXPECT_EQ(VarintEncode(int32_t{-1}),
            VarintEncode(std::numeric_limits<uint64_t>::max()));
  EXPECT_EQ(VarintEncode(int64_t{-1}),
            VarintEncode(std::numeric_limits<uint64_t>::max()));
  EXPECT_EQ(VarintEncode(std::numeric_limits<uint32_t>::max()),
            "\xff\xff\xff\xff\x0f");
  EXPECT_EQ(VarintEncode(std::numeric_limits<uint64_t>::max()),
            "\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01");
}

TEST(Varint, Decode) {
  EXPECT_THAT(VarintDecode<bool>(absl::Cord("\x01")),
              Optional(Eq(VarintDecodeResult<bool>{true, 1})));
  EXPECT_THAT(VarintDecode<int32_t>(absl::Cord("\x01")),
              Optional(Eq(VarintDecodeResult<int32_t>{1, 1})));
  EXPECT_THAT(VarintDecode<int64_t>(absl::Cord("\x01")),
              Optional(Eq(VarintDecodeResult<int64_t>{1, 1})));
  EXPECT_THAT(VarintDecode<uint32_t>(absl::Cord("\x01")),
              Optional(Eq(VarintDecodeResult<uint32_t>{1, 1})));
  EXPECT_THAT(VarintDecode<uint64_t>(absl::Cord("\x01")),
              Optional(Eq(VarintDecodeResult<uint64_t>{1, 1})));
  EXPECT_THAT(VarintDecode<uint32_t>(absl::Cord("\xff\xff\xff\xff\x0f")),
              Optional(Eq(VarintDecodeResult<uint32_t>{
                  std::numeric_limits<uint32_t>::max(), 5})));
  EXPECT_THAT(VarintDecode<int64_t>(
                  absl::Cord("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01")),
              Optional(Eq(VarintDecodeResult<int64_t>{int64_t{-1}, 10})));
  EXPECT_THAT(VarintDecode<uint64_t>(
                  absl::Cord("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01")),
              Optional(Eq(VarintDecodeResult<uint64_t>{
                  std::numeric_limits<uint64_t>::max(), 10})));
}

namespace {

template <typename T>
absl::Cord Fixed64Encode(T value) {
  absl::Cord cord;
  internal::Fixed64Encode(value, cord);
  return cord;
}

template <typename T>
absl::Cord Fixed32Encode(T value) {
  absl::Cord cord;
  internal::Fixed32Encode(value, cord);
  return cord;
}

}  // namespace

TEST(Fixed64, Encode) {
  EXPECT_EQ(Fixed64Encode(0.0), Fixed64Encode(uint64_t{0}));
}

TEST(Fixed64, Decode) {
  EXPECT_THAT(Fixed64Decode<double>(Fixed64Encode(0.0)), Optional(Eq(0.0)));
}

TEST(Fixed32, Encode) {
  EXPECT_EQ(Fixed32Encode(0.0f), Fixed32Encode(uint32_t{0}));
}

TEST(Fixed32, Decode) {
  EXPECT_THAT(Fixed32Decode<float>(
                  absl::Cord(absl::string_view("\x00\x00\x00\x00", 4))),
              Optional(Eq(0.0)));
}

TEST(DecodeProtoWireTag, Uint64TooLarge) {
  EXPECT_THAT(DecodeProtoWireTag(uint64_t{1} << 32), Eq(absl::nullopt));
}

TEST(DecodeProtoWireTag, Uint64ZeroFieldNumber) {
  EXPECT_THAT(DecodeProtoWireTag(uint64_t{0}), Eq(absl::nullopt));
}

TEST(DecodeProtoWireTag, Uint32ZeroFieldNumber) {
  EXPECT_THAT(DecodeProtoWireTag(uint32_t{0}), Eq(absl::nullopt));
}

TEST(DecodeProtoWireTag, Success) {
  EXPECT_THAT(DecodeProtoWireTag(uint64_t{1} << 3),
              Optional(Eq(ProtoWireTag(1, ProtoWireType::kVarint))));
  EXPECT_THAT(DecodeProtoWireTag(uint32_t{1} << 3),
              Optional(Eq(ProtoWireTag(1, ProtoWireType::kVarint))));
}

void TestSkipLengthValueSuccess(absl::Cord data, ProtoWireType type,
                                size_t skipped) {
  size_t before = data.size();
  EXPECT_TRUE(SkipLengthValue(data, type));
  EXPECT_EQ(before - skipped, data.size());
}

void TestSkipLengthValueFailure(absl::Cord data, ProtoWireType type) {
  EXPECT_FALSE(SkipLengthValue(data, type));
}

TEST(SkipLengthValue, Varint) {
  TestSkipLengthValueSuccess(
      absl::Cord("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01"),
      ProtoWireType::kVarint, 10);
  TestSkipLengthValueSuccess(absl::Cord("\x01"), ProtoWireType::kVarint, 1);
  TestSkipLengthValueFailure(
      absl::Cord("\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01"),
      ProtoWireType::kVarint);
}

TEST(SkipLengthValue, Fixed64) {
  TestSkipLengthValueSuccess(
      absl::Cord(
          absl::string_view("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 8)),
      ProtoWireType::kFixed64, 8);
  TestSkipLengthValueFailure(absl::Cord(absl::string_view("\x00", 1)),
                             ProtoWireType::kFixed64);
}

TEST(SkipLengthValue, LengthDelimited) {
  TestSkipLengthValueSuccess(absl::Cord(absl::string_view("\x00", 1)),
                             ProtoWireType::kLengthDelimited, 1);
  TestSkipLengthValueSuccess(absl::Cord(absl::string_view("\x01\x00", 2)),
                             ProtoWireType::kLengthDelimited, 2);
  TestSkipLengthValueFailure(absl::Cord("\x01"),
                             ProtoWireType::kLengthDelimited);
}

TEST(SkipLengthValue, Fixed32) {
  TestSkipLengthValueSuccess(
      absl::Cord(absl::string_view("\x00\x00\x00\x00", 4)),
      ProtoWireType::kFixed32, 4);
  TestSkipLengthValueFailure(absl::Cord(absl::string_view("\x00", 1)),
                             ProtoWireType::kFixed32);
}

TEST(SkipLengthValue, Decoder) {
  {
    ProtoWireDecoder decoder("", absl::Cord(absl::string_view("\x0a\x00", 2)));
    ASSERT_TRUE(decoder.HasNext());
    EXPECT_THAT(
        decoder.ReadTag(),
        IsOkAndHolds(Eq(ProtoWireTag(1, ProtoWireType::kLengthDelimited))));
    EXPECT_OK(decoder.SkipLengthValue());
    ASSERT_FALSE(decoder.HasNext());
  }
}

TEST(ProtoWireEncoder, BadTag) {
  absl::Cord data;
  ProtoWireEncoder encoder("foo.Bar", data);
  EXPECT_TRUE(encoder.empty());
  EXPECT_EQ(encoder.size(), 0);
  EXPECT_OK(encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kVarint)));
  EXPECT_OK(encoder.WriteVarint(1));
  encoder.EnsureFullyEncoded();
  EXPECT_FALSE(encoder.empty());
  EXPECT_EQ(encoder.size(), 2);
  EXPECT_EQ(data, "\x08\x01");
}

TEST(ProtoWireEncoder, Varint) {
  absl::Cord data;
  ProtoWireEncoder encoder("foo.Bar", data);
  EXPECT_TRUE(encoder.empty());
  EXPECT_EQ(encoder.size(), 0);
  EXPECT_OK(encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kVarint)));
  EXPECT_OK(encoder.WriteVarint(1));
  encoder.EnsureFullyEncoded();
  EXPECT_FALSE(encoder.empty());
  EXPECT_EQ(encoder.size(), 2);
  EXPECT_EQ(data, "\x08\x01");
}

TEST(ProtoWireEncoder, Fixed32) {
  absl::Cord data;
  ProtoWireEncoder encoder("foo.Bar", data);
  EXPECT_TRUE(encoder.empty());
  EXPECT_EQ(encoder.size(), 0);
  EXPECT_OK(encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kFixed32)));
  EXPECT_OK(encoder.WriteFixed32(0.0f));
  encoder.EnsureFullyEncoded();
  EXPECT_FALSE(encoder.empty());
  EXPECT_EQ(encoder.size(), 5);
  EXPECT_EQ(data, absl::string_view("\x0d\x00\x00\x00\x00", 5));
}

TEST(ProtoWireEncoder, Fixed64) {
  absl::Cord data;
  ProtoWireEncoder encoder("foo.Bar", data);
  EXPECT_TRUE(encoder.empty());
  EXPECT_EQ(encoder.size(), 0);
  EXPECT_OK(encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kFixed64)));
  EXPECT_OK(encoder.WriteFixed64(0.0));
  encoder.EnsureFullyEncoded();
  EXPECT_FALSE(encoder.empty());
  EXPECT_EQ(encoder.size(), 9);
  EXPECT_EQ(data, absl::string_view("\x09\x00\x00\x00\x00\x00\x00\x00\x00", 9));
}

TEST(ProtoWireEncoder, LengthDelimited) {
  absl::Cord data;
  ProtoWireEncoder encoder("foo.Bar", data);
  EXPECT_TRUE(encoder.empty());
  EXPECT_EQ(encoder.size(), 0);
  EXPECT_OK(encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kLengthDelimited)));
  EXPECT_OK(encoder.WriteLengthDelimited(absl::Cord("foo")));
  encoder.EnsureFullyEncoded();
  EXPECT_FALSE(encoder.empty());
  EXPECT_EQ(encoder.size(), 5);
  EXPECT_EQ(data,
            "\x0a\x03"
            "foo");
}

}  // namespace

}  // namespace cel::internal
