// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/http2/hpack/huffman/hpack_huffman_encoder.h"

#include <cstddef>
#include <string>

#include "absl/base/macros.h"
#include "absl/strings/escaping.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace http2 {
namespace {

TEST(HuffmanEncoderTest, Empty) {
  std::string empty("");
  size_t encoded_size = HuffmanSize(empty);
  EXPECT_EQ(0u, encoded_size);

  std::string buffer;
  HuffmanEncode(empty, encoded_size, &buffer);
  EXPECT_EQ("", buffer);
}

TEST(HuffmanEncoderTest, SpecRequestExamples) {
  std::string test_table[] = {
      "f1e3c2e5f23a6ba0ab90f4ff",
      "www.example.com",

      "a8eb10649cbf",
      "no-cache",

      "25a849e95ba97d7f",
      "custom-key",

      "25a849e95bb8e8b4bf",
      "custom-value",
  };
  for (size_t i = 0; i != ABSL_ARRAYSIZE(test_table); i += 2) {
    std::string huffman_encoded;
    ASSERT_TRUE(absl::HexStringToBytes(test_table[i], &huffman_encoded));
    const std::string& plain_string(test_table[i + 1]);
    size_t encoded_size = HuffmanSize(plain_string);
    EXPECT_EQ(huffman_encoded.size(), encoded_size);
    std::string buffer;
    buffer.reserve(huffman_encoded.size());
    HuffmanEncode(plain_string, encoded_size, &buffer);
    EXPECT_EQ(buffer, huffman_encoded) << "Error encoding " << plain_string;
  }
}

TEST(HuffmanEncoderTest, SpecResponseExamples) {
  std::string test_table[] = {
      "6402",
      "302",

      "aec3771a4b",
      "private",

      "d07abe941054d444a8200595040b8166e082a62d1bff",
      "Mon, 21 Oct 2013 20:13:21 GMT",

      "9d29ad171863c78f0b97c8e9ae82ae43d3",
      "https://www.example.com",

      "94e7821dd7f2e6c7b335dfdfcd5b3960d5af27087f3672c1ab270fb5291f9587316065c0"
      "03ed4ee5b1063d5007",
      "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1",
  };
  for (size_t i = 0; i != ABSL_ARRAYSIZE(test_table); i += 2) {
    std::string huffman_encoded;
    ASSERT_TRUE(absl::HexStringToBytes(test_table[i], &huffman_encoded));
    const std::string& plain_string(test_table[i + 1]);
    size_t encoded_size = HuffmanSize(plain_string);
    EXPECT_EQ(huffman_encoded.size(), encoded_size);
    std::string buffer;
    HuffmanEncode(plain_string, encoded_size, &buffer);
    EXPECT_EQ(buffer, huffman_encoded) << "Error encoding " << plain_string;
  }
}

TEST(HuffmanEncoderTest, EncodedSizeAgreesWithEncodeString) {
  std::string test_table[] = {
      "",
      "Mon, 21 Oct 2013 20:13:21 GMT",
      "https://www.example.com",
      "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1",
      std::string(1, '\0'),
      std::string("foo\0bar", 7),
      std::string(256, '\0'),
  };
  // Modify last |test_table| entry to cover all codes.
  for (size_t i = 0; i != 256; ++i) {
    test_table[ABSL_ARRAYSIZE(test_table) - 1][i] = static_cast<char>(i);
  }

  for (size_t i = 0; i != ABSL_ARRAYSIZE(test_table); ++i) {
    const std::string& plain_string = test_table[i];
    size_t encoded_size = HuffmanSize(plain_string);
    std::string huffman_encoded;
    HuffmanEncode(plain_string, encoded_size, &huffman_encoded);
    EXPECT_EQ(encoded_size, huffman_encoded.size());
  }
}

// Test that encoding appends to output without overwriting it.
TEST(HuffmanEncoderTest, AppendToOutput) {
  size_t encoded_size = HuffmanSize("foo");
  std::string buffer;
  HuffmanEncode("foo", encoded_size, &buffer);
  std::string expected_encoding;
  ASSERT_TRUE(absl::HexStringToBytes("94e7", &expected_encoding));
  EXPECT_EQ(expected_encoding, buffer);

  encoded_size = HuffmanSize("bar");
  HuffmanEncode("bar", encoded_size, &buffer);
  ASSERT_TRUE(absl::HexStringToBytes("94e78c767f", &expected_encoding));
  EXPECT_EQ(expected_encoding, buffer);
}

}  // namespace
}  // namespace http2
