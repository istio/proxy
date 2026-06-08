// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/moq_varint.h"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_fuzztest.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_data_reader.h"
#include "quiche/common/quiche_data_writer.h"

namespace quiche {
namespace {

using ::testing::ElementsAre;

using ReadFunction = std::optional<uint64_t> (*)(QuicheDataReader&);
template <ReadFunction reader>
std::optional<uint64_t> CompleteRead(absl::string_view hex_varint) {
  std::string raw_data;
  if (!absl::HexStringToBytes(hex_varint, &raw_data)) {
    return std::nullopt;
  }
  QuicheDataReader view(raw_data);
  std::optional<uint64_t> result = reader(view);
  if (!view.IsDoneReading()) {
    return std::nullopt;
  }
  return result;
}

TEST(MoqVarintTest, VarintParsing) {
  EXPECT_EQ(CompleteRead<ReadMoqVarint>(""), std::nullopt);
  EXPECT_EQ(CompleteRead<ReadMoqVarint>("00"), 0);
  EXPECT_EQ(CompleteRead<ReadEbmlVarint>("80"), 0);
  EXPECT_EQ(CompleteRead<ReadEbmlVarint>("000000000000000000"), std::nullopt);

  // Examples from RFC 8794, Section 4.4.
  EXPECT_EQ(CompleteRead<ReadEbmlVarint>("82"), 2);
  EXPECT_EQ(CompleteRead<ReadEbmlVarint>("4002"), 2);
  EXPECT_EQ(CompleteRead<ReadEbmlVarint>("200002"), 2);
  EXPECT_EQ(CompleteRead<ReadEbmlVarint>("10000002"), 2);

  // Examples from draft-ietf-moq-transport-17(bis), Section 1.4.1.
  EXPECT_EQ(CompleteRead<ReadMoqVarint>("25"), 37);
  EXPECT_EQ(CompleteRead<ReadMoqVarint>("8025"), 37);
  EXPECT_EQ(CompleteRead<ReadMoqVarint>("bbbd"), 15293);
  EXPECT_EQ(CompleteRead<ReadMoqVarint>("ed7f3e7d"), 226442877);
  EXPECT_EQ(CompleteRead<ReadMoqVarint>("faa1a0e403d8"), 2893212287960);
  EXPECT_EQ(CompleteRead<ReadMoqVarint>("fefa318fa8e3ca11"), 70423237261249041);
  EXPECT_EQ(CompleteRead<ReadMoqVarint>("ffffffffffffffffff"),
            18446744073709551615u);

  // The forbidden MOQ varint length.
  EXPECT_EQ(CompleteRead<ReadMoqVarint>("fcffffffffffff"), std::nullopt);
  EXPECT_EQ(CompleteRead<ReadEbmlVarint>("02ffffffffffff"), 0xffffffffffff);
}

void FuzzMoqVarintParser(absl::string_view s) {
  QuicheDataReader reader(s);
  ReadMoqVarint(reader);
}
void FuzzEbmlVarintParser(absl::string_view s) {
  QuicheDataReader reader(s);
  ReadEbmlVarint(reader);
}
FUZZ_TEST(MoqVarintFuzzTest, FuzzMoqVarintParser);
FUZZ_TEST(MoqVarintFuzzTest, FuzzEbmlVarintParser);

using WriteFunction = bool (*)(QuicheDataWriter&, uint64_t);
template <WriteFunction writer>
std::optional<std::string> Write(uint64_t value) {
  char buffer[9];
  QuicheDataWriter output(sizeof(buffer), buffer);
  bool success = writer(output, value);
  if (!success) {
    return std::nullopt;
  }
  return absl::BytesToHexString(
      absl::string_view(buffer, sizeof(buffer) - output.remaining()));
}

TEST(MoqVarintTest, VarintSerialization) {
  EXPECT_EQ(Write<WriteMoqVarint>(0), "00");
  EXPECT_EQ(Write<WriteMoqVarint>(std::numeric_limits<uint64_t>::max()),
            "ffffffffffffffffff");
  EXPECT_EQ(Write<WriteMoqVarint>(UINT64_C(0xffffffffffffff00)),
            "ffffffffffffffff00");
  EXPECT_EQ(Write<WriteEbmlVarint>(0), "80");
  EXPECT_EQ(Write<WriteEbmlVarint>(127), "407f");
  EXPECT_EQ(Write<WriteEbmlVarint>(UINT64_C(1) << 56), std::nullopt);
  EXPECT_EQ(Write<WriteEbmlVarint>(0xffffffffffff), "02ffffffffffff");
  EXPECT_EQ(Write<WriteMoqVarint>(0xffffffffffff), "fe00ffffffffffff");

  // Examples from draft-ietf-moq-transport-17(bis), Section 1.4.1.
  EXPECT_EQ(Write<WriteMoqVarint>(37), "25");
  EXPECT_EQ(Write<WriteMoqVarint>(15293), "bbbd");
  EXPECT_EQ(Write<WriteMoqVarint>(226442877), "ed7f3e7d");
  EXPECT_EQ(Write<WriteMoqVarint>(2893212287960), "faa1a0e403d8");
  EXPECT_EQ(Write<WriteMoqVarint>(70423237261249041), "fefa318fa8e3ca11");
  EXPECT_EQ(Write<WriteMoqVarint>(18446744073709551615u), "ffffffffffffffffff");
}

TEST(MoqVarintTest, WriteMoqVarintWithCustomLength) {
  std::array<char, 9> buffer;
  quiche::QuicheDataWriter writer(buffer.size(), buffer.data());
  ASSERT_TRUE(WriteMoqVarintWithCustomLength(writer, 0xabcd, 9));
  EXPECT_THAT(buffer, ElementsAre(0xff, 0, 0, 0, 0, 0, 0, 0xab, 0xcd));
}

TEST(MoqVarintTest, VarintSerializationInsufficientBuffer) {
  const uint64_t kValue = (UINT64_C(1) << 56) - 1;
  char buffer[9];
  for (int size = 0; size < 8; ++size) {
    QuicheDataWriter writer(size, buffer);
    EXPECT_FALSE(WriteMoqVarint(writer, kValue)) << size;
    EXPECT_FALSE(WriteEbmlVarint(writer, kValue)) << size;
  }
}

void MoqRoundTrip(uint64_t value) {
  std::optional<std::string> encoded = Write<WriteMoqVarint>(value);
  ASSERT_TRUE(encoded.has_value());
  EXPECT_EQ(encoded->size() / 2, GetMoqVarintLengthForValue(value));
  std::optional<uint64_t> decoded = CompleteRead<ReadMoqVarint>(*encoded);
  EXPECT_EQ(value, decoded) << *encoded;
}
void EbmlRoundTrip(uint64_t value) {
  std::optional<std::string> encoded = Write<WriteEbmlVarint>(value);
  if (!encoded.has_value()) {
    return;
  }
  EXPECT_EQ(encoded->size() / 2, GetEbmlVarintLengthForValue(value));
  std::optional<uint64_t> decoded = CompleteRead<ReadEbmlVarint>(*encoded);
  EXPECT_EQ(value, decoded);
}
FUZZ_TEST(MoqVarintFuzzTest, MoqRoundTrip);
FUZZ_TEST(MoqVarintFuzzTest, EbmlRoundTrip);

}  // namespace
}  // namespace quiche
