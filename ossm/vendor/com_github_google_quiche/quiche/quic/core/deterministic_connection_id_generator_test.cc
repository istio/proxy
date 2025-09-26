// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/deterministic_connection_id_generator.h"

#include <optional>
#include <ostream>
#include <vector>

#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

namespace quic {
namespace test {
namespace {

struct TestParams {
  TestParams(int connection_id_length)
      : connection_id_length_(connection_id_length) {}
  TestParams() : TestParams(kQuicDefaultConnectionIdLength) {}

  friend std::ostream& operator<<(std::ostream& os, const TestParams& p) {
    os << "{ connection ID length: " << p.connection_id_length_ << " }";
    return os;
  }

  int connection_id_length_;
};

// Constructs various test permutations.
std::vector<struct TestParams> GetTestParams() {
  std::vector<struct TestParams> params;
  std::vector<int> connection_id_lengths{7, 8, 9, 16, 20};
  for (int connection_id_length : connection_id_lengths) {
    params.push_back(TestParams(connection_id_length));
  }
  return params;
}

class DeterministicConnectionIdGeneratorTest
    : public QuicTestWithParam<TestParams> {
 public:
  DeterministicConnectionIdGeneratorTest()
      : connection_id_length_(GetParam().connection_id_length_),
        generator_(DeterministicConnectionIdGenerator(connection_id_length_)),
        version_(ParsedQuicVersion::RFCv1()) {}

 protected:
  int connection_id_length_;
  DeterministicConnectionIdGenerator generator_;
  ParsedQuicVersion version_;
};

INSTANTIATE_TEST_SUITE_P(DeterministicConnectionIdGeneratorTests,
                         DeterministicConnectionIdGeneratorTest,
                         ::testing::ValuesIn(GetTestParams()));

TEST_P(DeterministicConnectionIdGeneratorTest,
       NextConnectionIdIsDeterministic) {
  // Verify that two equal connection IDs get the same replacement.
  QuicConnectionId connection_id64a = TestConnectionId(33);
  QuicConnectionId connection_id64b = TestConnectionId(33);
  EXPECT_EQ(connection_id64a, connection_id64b);
  EXPECT_EQ(*generator_.GenerateNextConnectionId(connection_id64a),
            *generator_.GenerateNextConnectionId(connection_id64b));
  QuicConnectionId connection_id72a = TestConnectionIdNineBytesLong(42);
  QuicConnectionId connection_id72b = TestConnectionIdNineBytesLong(42);
  EXPECT_EQ(connection_id72a, connection_id72b);
  EXPECT_EQ(*generator_.GenerateNextConnectionId(connection_id72a),
            *generator_.GenerateNextConnectionId(connection_id72b));
}

TEST_P(DeterministicConnectionIdGeneratorTest,
       NextConnectionIdLengthIsCorrect) {
  // Verify that all generated IDs are of the correct length.
  const char connection_id_bytes[255] = {};
  for (uint8_t i = 0; i < sizeof(connection_id_bytes) - 1; ++i) {
    QuicConnectionId connection_id(connection_id_bytes, i);
    std::optional<QuicConnectionId> replacement_connection_id =
        generator_.GenerateNextConnectionId(connection_id);
    ASSERT_TRUE(replacement_connection_id.has_value());
    EXPECT_EQ(connection_id_length_, replacement_connection_id->length());
  }
}

TEST_P(DeterministicConnectionIdGeneratorTest, NextConnectionIdHasEntropy) {
  // Make sure all these test connection IDs have different replacements.
  for (uint64_t i = 0; i < 256; ++i) {
    QuicConnectionId connection_id_i = TestConnectionId(i);
    std::optional<QuicConnectionId> new_i =
        generator_.GenerateNextConnectionId(connection_id_i);
    ASSERT_TRUE(new_i.has_value());
    EXPECT_NE(connection_id_i, *new_i);
    for (uint64_t j = i + 1; j <= 256; ++j) {
      QuicConnectionId connection_id_j = TestConnectionId(j);
      EXPECT_NE(connection_id_i, connection_id_j);
      std::optional<QuicConnectionId> new_j =
          generator_.GenerateNextConnectionId(connection_id_j);
      ASSERT_TRUE(new_j.has_value());
      EXPECT_NE(*new_i, *new_j);
    }
  }
}

TEST_P(DeterministicConnectionIdGeneratorTest,
       OnlyReplaceConnectionIdWithWrongLength) {
  const char connection_id_input[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                      0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
                                      0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14};
  for (int i = 0; i < kQuicMaxConnectionIdWithLengthPrefixLength; i++) {
    QuicConnectionId input = QuicConnectionId(connection_id_input, i);
    std::optional<QuicConnectionId> output =
        generator_.MaybeReplaceConnectionId(input, version_);
    if (i == connection_id_length_) {
      EXPECT_FALSE(output.has_value());
    } else {
      ASSERT_TRUE(output.has_value());
      EXPECT_EQ(*output, generator_.GenerateNextConnectionId(input));
    }
  }
}

TEST_P(DeterministicConnectionIdGeneratorTest, ReturnLength) {
  EXPECT_EQ(generator_.ConnectionIdLength(0x01), connection_id_length_);
}

}  // namespace
}  // namespace test
}  // namespace quic
