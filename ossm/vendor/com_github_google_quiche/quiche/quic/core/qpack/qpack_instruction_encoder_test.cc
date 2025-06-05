// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/qpack/qpack_instruction_encoder.h"

#include <string>

#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/platform/api/quic_logging.h"
#include "quiche/quic/platform/api/quic_test.h"

namespace quic {
namespace test {

class QpackInstructionWithValuesPeer {
 public:
  static QpackInstructionWithValues CreateQpackInstructionWithValues(
      const QpackInstruction* instruction) {
    QpackInstructionWithValues instruction_with_values;
    instruction_with_values.instruction_ = instruction;
    return instruction_with_values;
  }

  static void set_s_bit(QpackInstructionWithValues* instruction_with_values,
                        bool s_bit) {
    instruction_with_values->s_bit_ = s_bit;
  }

  static void set_varint(QpackInstructionWithValues* instruction_with_values,
                         uint64_t varint) {
    instruction_with_values->varint_ = varint;
  }

  static void set_varint2(QpackInstructionWithValues* instruction_with_values,
                          uint64_t varint2) {
    instruction_with_values->varint2_ = varint2;
  }

  static void set_name(QpackInstructionWithValues* instruction_with_values,
                       absl::string_view name) {
    instruction_with_values->name_ = name;
  }

  static void set_value(QpackInstructionWithValues* instruction_with_values,
                        absl::string_view value) {
    instruction_with_values->value_ = value;
  }
};

namespace {

class QpackInstructionEncoderTest : public QuicTestWithParam<bool> {
 protected:
  QpackInstructionEncoderTest()
      : encoder_(GetHuffmanEncoding()), verified_position_(0) {}
  ~QpackInstructionEncoderTest() override = default;

  bool DisableHuffmanEncoding() { return GetParam(); }
  HuffmanEncoding GetHuffmanEncoding() {
    return DisableHuffmanEncoding() ? HuffmanEncoding::kDisabled
                                    : HuffmanEncoding::kEnabled;
  }

  // Append encoded |instruction| to |output_|.
  void EncodeInstruction(
      const QpackInstructionWithValues& instruction_with_values) {
    encoder_.Encode(instruction_with_values, &output_);
  }

  // Compare substring appended to |output_| since last EncodedSegmentMatches()
  // call against hex-encoded argument.
  bool EncodedSegmentMatches(absl::string_view hex_encoded_expected_substring) {
    auto recently_encoded =
        absl::string_view(output_).substr(verified_position_);
    std::string expected;
    EXPECT_TRUE(
        absl::HexStringToBytes(hex_encoded_expected_substring, &expected));
    verified_position_ = output_.size();
    return recently_encoded == expected;
  }

 private:
  QpackInstructionEncoder encoder_;
  std::string output_;
  std::string::size_type verified_position_;
};

INSTANTIATE_TEST_SUITE_P(DisableHuffmanEncoding, QpackInstructionEncoderTest,
                         testing::Values(false, true));

TEST_P(QpackInstructionEncoderTest, Varint) {
  const QpackInstruction instruction{QpackInstructionOpcode{0x00, 0x80},
                                     {{QpackInstructionFieldType::kVarint, 7}}};

  auto instruction_with_values =
      QpackInstructionWithValuesPeer::CreateQpackInstructionWithValues(
          &instruction);
  QpackInstructionWithValuesPeer::set_varint(&instruction_with_values, 5);
  EncodeInstruction(instruction_with_values);
  EXPECT_TRUE(EncodedSegmentMatches("05"));

  QpackInstructionWithValuesPeer::set_varint(&instruction_with_values, 127);
  EncodeInstruction(instruction_with_values);
  EXPECT_TRUE(EncodedSegmentMatches("7f00"));
}

TEST_P(QpackInstructionEncoderTest, SBitAndTwoVarint2) {
  const QpackInstruction instruction{
      QpackInstructionOpcode{0x80, 0xc0},
      {{QpackInstructionFieldType::kSbit, 0x20},
       {QpackInstructionFieldType::kVarint, 5},
       {QpackInstructionFieldType::kVarint2, 8}}};

  auto instruction_with_values =
      QpackInstructionWithValuesPeer::CreateQpackInstructionWithValues(
          &instruction);
  QpackInstructionWithValuesPeer::set_s_bit(&instruction_with_values, true);
  QpackInstructionWithValuesPeer::set_varint(&instruction_with_values, 5);
  QpackInstructionWithValuesPeer::set_varint2(&instruction_with_values, 200);
  EncodeInstruction(instruction_with_values);
  EXPECT_TRUE(EncodedSegmentMatches("a5c8"));

  QpackInstructionWithValuesPeer::set_s_bit(&instruction_with_values, false);
  QpackInstructionWithValuesPeer::set_varint(&instruction_with_values, 31);
  QpackInstructionWithValuesPeer::set_varint2(&instruction_with_values, 356);
  EncodeInstruction(instruction_with_values);
  EXPECT_TRUE(EncodedSegmentMatches("9f00ff65"));
}

TEST_P(QpackInstructionEncoderTest, SBitAndVarintAndValue) {
  const QpackInstruction instruction{QpackInstructionOpcode{0xc0, 0xc0},
                                     {{QpackInstructionFieldType::kSbit, 0x20},
                                      {QpackInstructionFieldType::kVarint, 5},
                                      {QpackInstructionFieldType::kValue, 7}}};

  auto instruction_with_values =
      QpackInstructionWithValuesPeer::CreateQpackInstructionWithValues(
          &instruction);
  QpackInstructionWithValuesPeer::set_s_bit(&instruction_with_values, true);
  QpackInstructionWithValuesPeer::set_varint(&instruction_with_values, 100);
  QpackInstructionWithValuesPeer::set_value(&instruction_with_values, "foo");
  EncodeInstruction(instruction_with_values);
  if (DisableHuffmanEncoding()) {
    EXPECT_TRUE(EncodedSegmentMatches("ff4503666f6f"));
  } else {
    EXPECT_TRUE(EncodedSegmentMatches("ff458294e7"));
  }

  QpackInstructionWithValuesPeer::set_s_bit(&instruction_with_values, false);
  QpackInstructionWithValuesPeer::set_varint(&instruction_with_values, 3);
  QpackInstructionWithValuesPeer::set_value(&instruction_with_values, "bar");
  EncodeInstruction(instruction_with_values);
  EXPECT_TRUE(EncodedSegmentMatches("c303626172"));
}

TEST_P(QpackInstructionEncoderTest, Name) {
  const QpackInstruction instruction{QpackInstructionOpcode{0xe0, 0xe0},
                                     {{QpackInstructionFieldType::kName, 4}}};

  auto instruction_with_values =
      QpackInstructionWithValuesPeer::CreateQpackInstructionWithValues(
          &instruction);
  QpackInstructionWithValuesPeer::set_name(&instruction_with_values, "");
  EncodeInstruction(instruction_with_values);
  EXPECT_TRUE(EncodedSegmentMatches("e0"));

  QpackInstructionWithValuesPeer::set_name(&instruction_with_values, "foo");
  EncodeInstruction(instruction_with_values);
  if (DisableHuffmanEncoding()) {
    EXPECT_TRUE(EncodedSegmentMatches("e3666f6f"));
  } else {
    EXPECT_TRUE(EncodedSegmentMatches("f294e7"));
  }

  QpackInstructionWithValuesPeer::set_name(&instruction_with_values, "bar");
  EncodeInstruction(instruction_with_values);
  EXPECT_TRUE(EncodedSegmentMatches("e3626172"));
}

TEST_P(QpackInstructionEncoderTest, Value) {
  const QpackInstruction instruction{QpackInstructionOpcode{0xf0, 0xf0},
                                     {{QpackInstructionFieldType::kValue, 3}}};

  auto instruction_with_values =
      QpackInstructionWithValuesPeer::CreateQpackInstructionWithValues(
          &instruction);
  QpackInstructionWithValuesPeer::set_value(&instruction_with_values, "");
  EncodeInstruction(instruction_with_values);
  EXPECT_TRUE(EncodedSegmentMatches("f0"));
  QpackInstructionWithValuesPeer::set_value(&instruction_with_values, "foo");
  EncodeInstruction(instruction_with_values);
  if (DisableHuffmanEncoding()) {
    EXPECT_TRUE(EncodedSegmentMatches("f3666f6f"));
  } else {
    EXPECT_TRUE(EncodedSegmentMatches("fa94e7"));
  }

  QpackInstructionWithValuesPeer::set_value(&instruction_with_values, "bar");
  EncodeInstruction(instruction_with_values);
  EXPECT_TRUE(EncodedSegmentMatches("f3626172"));
}

TEST_P(QpackInstructionEncoderTest, SBitAndNameAndValue) {
  const QpackInstruction instruction{QpackInstructionOpcode{0xf0, 0xf0},
                                     {{QpackInstructionFieldType::kSbit, 0x08},
                                      {QpackInstructionFieldType::kName, 2},
                                      {QpackInstructionFieldType::kValue, 7}}};

  auto instruction_with_values =
      QpackInstructionWithValuesPeer::CreateQpackInstructionWithValues(
          &instruction);
  QpackInstructionWithValuesPeer::set_s_bit(&instruction_with_values, false);
  QpackInstructionWithValuesPeer::set_name(&instruction_with_values, "");
  QpackInstructionWithValuesPeer::set_value(&instruction_with_values, "");
  EncodeInstruction(instruction_with_values);
  EXPECT_TRUE(EncodedSegmentMatches("f000"));

  QpackInstructionWithValuesPeer::set_s_bit(&instruction_with_values, true);
  QpackInstructionWithValuesPeer::set_name(&instruction_with_values, "foo");
  QpackInstructionWithValuesPeer::set_value(&instruction_with_values, "bar");
  EncodeInstruction(instruction_with_values);
  if (DisableHuffmanEncoding()) {
    EXPECT_TRUE(EncodedSegmentMatches("fb00666f6f03626172"));
  } else {
    EXPECT_TRUE(EncodedSegmentMatches("fe94e703626172"));
  }
}

}  // namespace
}  // namespace test
}  // namespace quic
