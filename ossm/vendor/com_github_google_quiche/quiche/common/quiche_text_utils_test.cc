// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/quiche_text_utils.h"

#include <string>

#include "absl/strings/escaping.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quiche {
namespace test {

TEST(QuicheTestUtilsTest, StringPieceCaseHash) {
  const auto hasher = StringPieceCaseHash();
  EXPECT_EQ(hasher("content-length"), hasher("Content-Length"));
  EXPECT_EQ(hasher("Content-Length"), hasher("CONTENT-LENGTH"));
  EXPECT_EQ(hasher("CoNteNT-lEngTH"), hasher("content-length"));
  EXPECT_NE(hasher("content-length"), hasher("content_length"));
  // Case insensitivity is ASCII-only.
  EXPECT_NE(hasher("Türkiye"), hasher("TÜRKİYE"));
  EXPECT_EQ(
      hasher("This is a string that is too long for inlining and requires a "
             "heap allocation. Apparently PowerPC has 128 byte cache lines. "
             "Since our inline array is sized according to a cache line, we "
             "need this string to be longer than 128 bytes."),
      hasher("This Is A String That Is Too Long For Inlining And Requires A "
             "Heap Allocation. Apparently PowerPC Has 128 Byte Cache Lines. "
             "Since Our Inline Array Is Sized According To A Cache Line, We "
             "Need This String To Be Longer Than 128 Bytes."));
}

TEST(QuicheTextUtilsTest, ToLower) {
  EXPECT_EQ("lower", quiche::QuicheTextUtils::ToLower("LOWER"));
  EXPECT_EQ("lower", quiche::QuicheTextUtils::ToLower("lower"));
  EXPECT_EQ("lower", quiche::QuicheTextUtils::ToLower("lOwEr"));
  EXPECT_EQ("123", quiche::QuicheTextUtils::ToLower("123"));
  EXPECT_EQ("", quiche::QuicheTextUtils::ToLower(""));
}

TEST(QuicheTextUtilsTest, RemoveLeadingAndTrailingWhitespace) {
  for (auto* const input : {"text", " text", "  text", "text ", "text  ",
                            " text ", "  text  ", "\r\n\ttext", "text\n\r\t"}) {
    absl::string_view piece(input);
    quiche::QuicheTextUtils::RemoveLeadingAndTrailingWhitespace(&piece);
    EXPECT_EQ("text", piece);
  }
}

TEST(QuicheTextUtilsTest, HexDump) {
  // Verify output for empty input.
  std::string empty;
  ASSERT_TRUE(absl::HexStringToBytes("", &empty));
  EXPECT_EQ("", quiche::QuicheTextUtils::HexDump(empty));
  // Verify output of the HexDump method is as expected.
  char packet[] = {
      0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2c, 0x20, 0x51, 0x55, 0x49, 0x43, 0x21,
      0x20, 0x54, 0x68, 0x69, 0x73, 0x20, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,
      0x20, 0x73, 0x68, 0x6f, 0x75, 0x6c, 0x64, 0x20, 0x62, 0x65, 0x20, 0x6c,
      0x6f, 0x6e, 0x67, 0x20, 0x65, 0x6e, 0x6f, 0x75, 0x67, 0x68, 0x20, 0x74,
      0x6f, 0x20, 0x73, 0x70, 0x61, 0x6e, 0x20, 0x6d, 0x75, 0x6c, 0x74, 0x69,
      0x70, 0x6c, 0x65, 0x20, 0x6c, 0x69, 0x6e, 0x65, 0x73, 0x20, 0x6f, 0x66,
      0x20, 0x6f, 0x75, 0x74, 0x70, 0x75, 0x74, 0x2e, 0x01, 0x02, 0x03, 0x00,
  };
  EXPECT_EQ(
      quiche::QuicheTextUtils::HexDump(packet),
      "0x0000:  4865 6c6c 6f2c 2051 5549 4321 2054 6869  Hello,.QUIC!.Thi\n"
      "0x0010:  7320 7374 7269 6e67 2073 686f 756c 6420  s.string.should.\n"
      "0x0020:  6265 206c 6f6e 6720 656e 6f75 6768 2074  be.long.enough.t\n"
      "0x0030:  6f20 7370 616e 206d 756c 7469 706c 6520  o.span.multiple.\n"
      "0x0040:  6c69 6e65 7320 6f66 206f 7574 7075 742e  lines.of.output.\n"
      "0x0050:  0102 03                                  ...\n");
  // Verify that 0x21 and 0x7e are printable, 0x20 and 0x7f are not.
  std::string printable_and_unprintable_chars;
  ASSERT_TRUE(
      absl::HexStringToBytes("20217e7f", &printable_and_unprintable_chars));
  EXPECT_EQ("0x0000:  2021 7e7f                                .!~.\n",
            quiche::QuicheTextUtils::HexDump(printable_and_unprintable_chars));
  // Verify that values above numeric_limits<unsigned char>::max() are formatted
  // properly on platforms where char is unsigned.
  std::string large_chars;
  ASSERT_TRUE(absl::HexStringToBytes("90aaff", &large_chars));
  EXPECT_EQ("0x0000:  90aa ff                                  ...\n",
            quiche::QuicheTextUtils::HexDump(large_chars));
}

TEST(QuicheTextUtilsTest, Base64Encode) {
  std::string output;
  std::string input = "Hello";
  quiche::QuicheTextUtils::Base64Encode(
      reinterpret_cast<const uint8_t*>(input.data()), input.length(), &output);
  EXPECT_EQ("SGVsbG8", output);

  input =
      "Hello, QUIC! This string should be long enough to span"
      "multiple lines of output\n";
  quiche::QuicheTextUtils::Base64Encode(
      reinterpret_cast<const uint8_t*>(input.data()), input.length(), &output);
  EXPECT_EQ(
      "SGVsbG8sIFFVSUMhIFRoaXMgc3RyaW5nIHNob3VsZCBiZSBsb25n"
      "IGVub3VnaCB0byBzcGFubXVsdGlwbGUgbGluZXMgb2Ygb3V0cHV0Cg",
      output);
}

TEST(QuicheTextUtilsTest, ContainsUpperCase) {
  EXPECT_FALSE(quiche::QuicheTextUtils::ContainsUpperCase("abc"));
  EXPECT_FALSE(quiche::QuicheTextUtils::ContainsUpperCase(""));
  EXPECT_FALSE(quiche::QuicheTextUtils::ContainsUpperCase("123"));
  EXPECT_TRUE(quiche::QuicheTextUtils::ContainsUpperCase("ABC"));
  EXPECT_TRUE(quiche::QuicheTextUtils::ContainsUpperCase("aBc"));
}

}  // namespace test
}  // namespace quiche
