// Copyright 2021 Google LLC
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

#include "internal/strings.h"

#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/cord.h"
#include "absl/strings/cord_test_helpers.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "internal/testing.h"
#include "internal/utf8.h"

namespace cel::internal {
namespace {

using ::absl_testing::StatusIs;

constexpr char kUnicodeNotAllowedInBytes1[] =
    "Unicode escape sequence \\u cannot be used in bytes literals";
constexpr char kUnicodeNotAllowedInBytes2[] =
    "Unicode escape sequence \\U cannot be used in bytes literals";

// <quoted> takes a string literal of the form '...', r'...', "..." or r"...".
// <unquoted> is the expected parsed form of <quoted>.
void TestQuotedString(const std::string& unquoted, const std::string& quoted) {
  auto status_or_unquoted = ParseStringLiteral(quoted);
  EXPECT_OK(status_or_unquoted) << unquoted;
  EXPECT_EQ(unquoted, status_or_unquoted.value()) << quoted;
}

void TestString(const std::string& unquoted) {
  TestQuotedString(unquoted, FormatStringLiteral(unquoted));
  TestQuotedString(unquoted, FormatStringLiteral(absl::Cord(unquoted)));
  if (unquoted.size() > 1) {
    const size_t mid = unquoted.size() / 2;
    TestQuotedString(unquoted, FormatStringLiteral(absl::MakeFragmentedCord(
                                   {absl::string_view(unquoted).substr(0, mid),
                                    absl::string_view(unquoted).substr(mid)})));
  }
  TestQuotedString(unquoted,
                   absl::StrCat("'''", EscapeString(unquoted), "'''"));
  TestQuotedString(unquoted,
                   absl::StrCat("\"\"\"", EscapeString(unquoted), "\"\"\""));
}

void TestRawString(const std::string& unquoted) {
  const std::string quote = (!absl::StrContains(unquoted, "'")) ? "'" : "\"";
  TestQuotedString(unquoted, absl::StrCat("r", quote, unquoted, quote));
  TestQuotedString(unquoted, absl::StrCat("r\"", unquoted, "\""));
  TestQuotedString(unquoted, absl::StrCat("r'''", unquoted, "'''"));
  TestQuotedString(unquoted, absl::StrCat("r\"\"\"", unquoted, "\"\"\""));
}

// <quoted> is the quoted version of <unquoted> and represents the original
// string mentioned in the test case.
// This method compares the unescaped <unquoted> against its round trip version
// i.e. after carrying out escaping followed by unescaping on it.
void TestBytesEscaping(const std::string& unquoted, const std::string& quoted) {
  ASSERT_OK_AND_ASSIGN(auto unescaped, UnescapeBytes(unquoted));
  const std::string escaped = EscapeBytes(unescaped);
  ASSERT_OK_AND_ASSIGN(auto unescaped2, UnescapeBytes(escaped));
  EXPECT_EQ(unescaped, unescaped2);
  std::string escaped2 = EscapeBytes(unescaped, true);
  ASSERT_OK_AND_ASSIGN(auto unescaped3, UnescapeBytes(escaped2));
  EXPECT_EQ(unescaped, unescaped3);
}

// <quoted> takes a byte literal of the form b'...', b'''...'''
void TestBytesLiteral(const std::string& quoted) {
  // Parse the literal.
  ASSERT_OK_AND_ASSIGN(auto unquoted, ParseBytesLiteral(quoted));

  // Take the parsed literal and turn it back to a literal.
  std::string requoted = FormatBytesLiteral(unquoted);
  // Parse it again.
  ASSERT_OK_AND_ASSIGN(auto unquoted2, ParseBytesLiteral(requoted));
  // Test the parsed literal forms for equality, not the unparsed forms.
  // This is because the unparsed forms can have different representations for
  // the same data, i.e., \000 and \x00.
  EXPECT_EQ(unquoted, unquoted2)
      << "unquoted : " << unquoted << "\nunquoted2: " << unquoted2;

  TestBytesEscaping(unquoted, quoted);
}

// <quoted> takes a raw byte literal of the form rb'...', br'...', rb'''...'''
// or br'''...'''. <unquoted> is the expected parsed form of <quoted>.
void TestQuotedRawBytesLiteral(const std::string& unquoted,
                               const std::string& quoted) {
  ASSERT_OK_AND_ASSIGN(auto actual_unquoted, ParseBytesLiteral(quoted));
  EXPECT_EQ(unquoted, actual_unquoted) << "quoted: " << quoted;
}

// <unquoted> takes a string of not escaped unquoted bytes.
void TestUnescapedBytes(const std::string& unquoted) {
  TestBytesLiteral(FormatBytesLiteral(unquoted));
}

void TestRawBytes(const std::string& unquoted) {
  const std::string quote = (!absl::StrContains(unquoted, "'")) ? "'" : "\"";
  TestQuotedRawBytesLiteral(unquoted,
                            absl::StrCat("rb", quote, unquoted, quote));
  TestQuotedRawBytesLiteral(unquoted,
                            absl::StrCat("br", quote, unquoted, quote));
  TestQuotedRawBytesLiteral(unquoted, absl::StrCat("rb'''", unquoted, "'''"));
  TestQuotedRawBytesLiteral(unquoted, absl::StrCat("br'''", unquoted, "'''"));
  TestQuotedRawBytesLiteral(unquoted,
                            absl::StrCat("rb\"\"\"", unquoted, "\"\"\""));
  TestQuotedRawBytesLiteral(unquoted,
                            absl::StrCat("br\"\"\"", unquoted, "\"\"\""));
}

void TestParseString(const std::string& orig) {
  EXPECT_OK(ParseStringLiteral(orig)) << orig;
}

void TestParseBytes(const std::string& orig) {
  EXPECT_OK(ParseBytesLiteral(orig)) << orig;
}

void TestStringEscaping(const std::string& orig) {
  const std::string escaped = EscapeString(orig);
  ASSERT_OK_AND_ASSIGN(auto unescaped, UnescapeString(escaped));
  EXPECT_EQ(orig, unescaped) << "escaped: " << escaped;
}

void TestValue(const std::string& orig) {
  TestStringEscaping(orig);
  TestString(orig);
}

// Test that <str> is treated as invalid, with error offset
// <expected_error_offset> and an error that contains substring
// <expected_error_substr>. The last arguments are optional because most
// flat-out bad inputs are rejected without further information.
void TestInvalidString(const std::string& str,
                       const std::string& expected_error_substr = "") {
  auto status_or_string = ParseStringLiteral(str);
  EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_TRUE(absl::StrContains(status_or_string.status().message(),
                                expected_error_substr));
}

// Test that <str> is treated as invalid, with error offset
// <expected_error_offset> and an error that contains substring
// <expected_error_substr>. The last arguments are optional because most
// flat-out bad inputs are rejected without further information.
void TestInvalidBytes(const std::string& str,
                      const std::string& expected_error_substr = "") {
  auto status_or_string = ParseBytesLiteral(str);
  EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_TRUE(absl::StrContains(status_or_string.status().message(),
                                expected_error_substr));
}

TEST(StringsTest, TestParsingOfAllEscapeCharacters) {
  // All the valid escapes.
  const std::set<char> valid_escapes = {'a', 'b',  'f', 'n', 'r',  't',
                                        'v', '\\', '?', '"', '\'', '`',
                                        'u', 'U',  'x', 'X'};
  for (int escape_char_int = 0; escape_char_int < 255; ++escape_char_int) {
    char escape_char = static_cast<char>(escape_char_int);
    absl::string_view escape_piece(&escape_char, 1);
    if (valid_escapes.find(escape_char) != valid_escapes.end()) {
      if (escape_char == '\'') {
        TestParseString(absl::StrCat("\"a\\", escape_piece, "0010ffff\""));
      }
      TestParseString(absl::StrCat("'a\\", escape_piece, "0010ffff'"));
      TestParseString(absl::StrCat("'''a\\", escape_piece, "0010ffff'''"));
    } else if (absl::ascii_isdigit(escape_char)) {
      // Can also escape 0-3.
      const std::string test_string =
          absl::StrCat("'a\\", escape_piece, "00b'");
      const std::string test_triple_quoted_string =
          absl::StrCat("'''a\\", escape_piece, "00b'''");
      if (escape_char <= '3') {
        TestParseString(test_string);
        TestParseString(test_triple_quoted_string);
      } else {
        TestInvalidString(test_string, "Illegal escape sequence: ");
        TestInvalidString(test_triple_quoted_string,
                          "Illegal escape sequence: ");
      }
    } else {
      if (Utf8IsValid(escape_piece)) {
        const std::string expected_error =
            ((escape_char == '\n' || escape_char == '\r')
                 ? "Illegal escaped newline"
                 : "Illegal escape sequence: ");
        TestInvalidString(absl::StrCat("'a\\", escape_piece, "b'"),
                          expected_error);
        TestInvalidString(absl::StrCat("'''a\\", escape_piece, "b'''"),
                          expected_error);
      } else {
        TestInvalidString(absl::StrCat("'a\\", escape_piece, "b'"),
                          "Structurally invalid UTF8"  //
                          " string");
        TestInvalidString(absl::StrCat("'''a\\", escape_piece, "b'''"),
                          "Structurally invalid UTF8"  //
                          " string");
      }
    }
  }
}

TEST(StringsTest, TestParsingOfOctalEscapes) {
  for (int idx = 0; idx < 256; ++idx) {
    const char end_char = (idx % 8) + '0';
    const char mid_char = ((idx / 8) % 8) + '0';
    const char lead_char = (idx / 64) + '0';
    absl::string_view lead_piece(&lead_char, 1);
    absl::string_view mid_piece(&mid_char, 1);
    absl::string_view end_piece(&end_char, 1);
    const std::string test_string =
        absl::StrCat(lead_piece, mid_piece, end_piece);
    TestParseString(absl::StrCat("'\\", test_string, "'"));
    TestParseString(absl::StrCat("'''\\", test_string, "'''"));
    TestParseBytes(absl::StrCat("b'\\", test_string, "'"));
  }
  TestInvalidString("'\\'", "String must end with '");
  TestInvalidString("'abc\\'", "String must end with '");
  TestInvalidString("'''\\'''", "String must end with '''");
  TestInvalidString("'''abc\\'''", "String must end with '''");
  TestInvalidString(
      "'\\0'", "Octal escape must be followed by 3 octal digits but saw: \\0");
  TestInvalidString(
      "'''abc\\0'''",
      "Octal escape must be followed by 3 octal digits but saw: \\0");
  TestInvalidString(
      "'\\00'",
      "Octal escape must be followed by 3 octal digits but saw: \\00");
  TestInvalidString(
      "'''ab\\00'''",
      "Octal escape must be followed by 3 octal digits but saw: \\00");
  TestInvalidString(
      "'a\\008'",
      "Octal escape must be followed by 3 octal digits but saw: \\008");
  TestInvalidString(
      "'''\\008'''",
      "Octal escape must be followed by 3 octal digits but saw: \\008");
  TestInvalidString("'\\400'", "Illegal escape sequence: \\4");
  TestInvalidString("'''\\400'''", "Illegal escape sequence: \\4");
  TestInvalidString("'\\777'", "Illegal escape sequence: \\7");
  TestInvalidString("'''\\777'''", "Illegal escape sequence: \\7");
}

TEST(StringsTest, TestParsingOfHexEscapes) {
  for (int idx = 0; idx < 256; ++idx) {
    char lead_char = absl::StrFormat("%X", idx / 16)[0];
    char end_char = absl::StrFormat("%x", idx % 16)[0];
    absl::string_view lead_piece(&lead_char, 1);
    absl::string_view end_piece(&end_char, 1);
    TestParseString(absl::StrCat("'\\x", lead_piece, end_piece, "'"));
    TestParseString(absl::StrCat("'''\\x", lead_piece, end_piece, "'''"));
    TestParseString(absl::StrCat("'\\X", lead_piece, end_piece, "'"));
    TestParseString(absl::StrCat("'''\\X", lead_piece, end_piece, "'''"));
    TestParseBytes(absl::StrCat("b'\\X", lead_piece, end_piece, "'"));
  }
  TestInvalidString("'\\x'");
  TestInvalidString("'''\\x'''");
  TestInvalidString("'\\x0'");
  TestInvalidString("'''\\x0'''");
  TestInvalidString("'\\x0G'");
  TestInvalidString("'''\\x0G'''");
}

TEST(StringsTest, RoundTrip) {
  // Empty string is valid as a string but not an identifier.
  TestStringEscaping("");
  TestString("");

  TestValue("abc");
  TestValue("abc123");
  TestValue("123abc");
  TestValue("_abc123");
  TestValue("_123");
  TestValue("abc def");
  TestValue("a`b");
  TestValue("a77b");
  TestValue("\"abc\"");
  TestValue("'abc'");
  TestValue("`abc`");
  TestValue("aaa'bbb\"ccc`ddd");
  TestValue("\n");
  TestValue("\\");
  TestValue("\\n");
  TestValue("\x12");
  TestValue("a,g  8q483 *(YG(*$(&*98fg\\r\\n\\t\x12gb");

  // Value with an embedded zero char.
  std::string s = "abc";
  s[1] = 0;
  TestValue(s);

  // Reserved SQL keyword, which must be quoted as an identifier.
  TestValue("select");
  TestValue("SELECT");
  TestValue("SElecT");
  // Non-reserved SQL keyword, which shouldn't be quoted.
  TestValue("options");

  // Note that control characters and other odd byte values such as \0 are
  // allowed in string literals as long as they are utf8 structurally valid.
  TestValue("\x01\x31");
  TestValue("abc\xb\x42\141bc");
  TestValue("123\1\x31\x32\x33");
  TestValue("\\\"\xe8\xb0\xb7\xe6\xad\x8c\\\" is Google\\\'s Chinese name");
}

TEST(StringsTest, InvalidString) {
  const std::string kInvalidStringLiteral = "Invalid string literal";

  TestInvalidString("A", kInvalidStringLiteral);    // No quote at all
  TestInvalidString("'", kInvalidStringLiteral);    // No closing quote
  TestInvalidString("\"", kInvalidStringLiteral);   // No closing quote
  TestInvalidString("a'", kInvalidStringLiteral);   // No opening quote
  TestInvalidString("a\"", kInvalidStringLiteral);  // No opening quote
  TestInvalidString("'''", "String cannot contain unescaped '");
  TestInvalidString("\"\"\"", "String cannot contain unescaped \"");
  TestInvalidString("''''", "String cannot contain unescaped '");
  TestInvalidString("\"\"\"\"", "String cannot contain unescaped \"");
  TestInvalidString("'''''", "String cannot contain unescaped '");
  TestInvalidString("\"\"\"\"\"", "String cannot contain unescaped \"");
  TestInvalidString("'''''''", "String cannot contain unescaped '''");
  TestInvalidString("\"\"\"\"\"\"\"", "String cannot contain unescaped \"\"\"");
  TestInvalidString("'''''''''", "String cannot contain unescaped '''");
  TestInvalidString("\"\"\"\"\"\"\"\"\"",
                    "String cannot contain unescaped \"\"\"");

  TestInvalidString("abc");

  TestInvalidString("'abc'def'", "String cannot contain unescaped '");
  TestInvalidString("'abc''def'", "String cannot contain unescaped '");
  TestInvalidString("\"abc\"\"def\"", "String cannot contain unescaped \"");
  TestInvalidString("'''abc'''def'''", "String cannot contain unescaped '''");
  TestInvalidString("\"\"\"abc\"\"\"def\"\"\"",
                    "String cannot contain unescaped \"\"\"");

  TestInvalidString("'abc");
  TestInvalidString("\"abc");
  TestInvalidString("'''abc");
  TestInvalidString("\"\"\"abc");

  TestInvalidString("abc'");
  TestInvalidString("abc\"");
  TestInvalidString("abc'''");
  TestInvalidString("abc\"\"\"");

  TestInvalidString("\"abc'");
  TestInvalidString("'abc\"");
  TestInvalidString("'''abc'", "String cannot contain unescaped '");
  TestInvalidString("'''abc\"");

  TestInvalidString("'''a'", "String cannot contain unescaped '");
  TestInvalidString("\"\"\"a\"", "String cannot contain unescaped \"");
  TestInvalidString("'''a''", "String cannot contain unescaped '");
  TestInvalidString("\"\"\"a\"\"", "String cannot contain unescaped \"");
  TestInvalidString("'''a''''", "String cannot contain unescaped '''");
  TestInvalidString("\"\"\"a\"\"\"\"",
                    "String cannot contain unescaped \"\"\"");

  TestInvalidString("'''abc\"\"\"");
  TestInvalidString("\"\"\"abc'");
  TestInvalidString("\"\"\"abc\"", "String cannot contain unescaped \"");
  TestInvalidString("\"\"\"abc'''");
  TestInvalidString("'''\\\''''''", "String cannot contain unescaped '''");
  TestInvalidString("\"\"\"\\\"\"\"\"\"\"",
                    "String cannot contain unescaped \"\"\"");
  TestInvalidString("''''\\\'''''", "String cannot contain unescaped '''");
  TestInvalidString("\"\"\"\"\\\"\"\"\"\"",
                    "String cannot contain unescaped \"\"\"");
  TestInvalidString("\"\"\"'a' \"b\"\"\"\"",
                    "String cannot contain unescaped \"\"\"");

  TestInvalidString("`abc`");

  TestInvalidString("'abc\\'", "String must end with '");
  TestInvalidString("\"abc\\\"", "String must end with \"");
  TestInvalidString("'''abc\\'''", "String must end with '''");
  TestInvalidString("\"\"\"abc\\\"\"\"", "String must end with \"\"\"");

  TestInvalidString("'\\U12345678'",
                    "Value of \\U12345678 exceeds Unicode limit (0x0010FFFF)");

  // All trailing escapes.
  TestInvalidString("'\\");
  TestInvalidString("\"\\");
  TestInvalidString("''''''\\");
  TestInvalidString("\"\"\"\"\"\"\\");
  TestInvalidString("''\\\\");
  TestInvalidString("\"\"\\\\");
  TestInvalidString("''''''\\\\");
  TestInvalidString("\"\"\"\"\"\"\\\\");

  // String with an unescaped 0 byte.
  std::string s = "abc";
  s[1] = 0;
  TestInvalidString(s);
  // Note: These are C-escapes to define the invalid strings.
  TestInvalidString("'\xc1'", "Structurally invalid UTF8 string");
  TestInvalidString("'\xca'", "Structurally invalid UTF8 string");
  TestInvalidString("'\xcc'", "Structurally invalid UTF8 string");
  TestInvalidString("'\xFA'", "Structurally invalid UTF8 string");
  TestInvalidString("'\xc1\xca\x1b\x62\x19o\xcc\x04'",
                    "Structurally invalid UTF8 string");

  TestInvalidString("'\xc2\xc0'",
                    "Structurally invalid UTF8 string");  // First byte ok utf8,
                                                          // invalid together.
  TestValue("\xc2\xbf");  // Same first byte, good sequence.

  // These are all valid prefixes for utf8 characters, but the characters
  // are not complete.
  TestInvalidString(
      "'\xc2'",
      "Structurally invalid UTF8 string");  // Should be 2 byte utf8 character.
  TestInvalidString(
      "'\xc3'",
      "Structurally invalid UTF8 string");  // Should be 2 byte utf8 character.
  TestInvalidString(
      "'\xe0'",
      "Structurally invalid UTF8 string");  // Should be 3 byte utf8 character.
  TestInvalidString(
      "'\xe0\xac'",
      "Structurally invalid UTF8 string");  // Should be 3 byte utf8 character.
  TestInvalidString(
      "'\xf0'",
      "Structurally invalid UTF8 string");  // Should be 4 byte utf8 character.
  TestInvalidString(
      "'\xf0\x90'",
      "Structurally invalid UTF8 string");  // Should be 4 byte utf8 character.
  TestInvalidString(
      "'\xf0\x90\x80'",
      "Structurally invalid UTF8 string");  // Should be 4 byte utf8 character.
}

TEST(BytesTest, RoundTrip) {
  TestBytesLiteral("b\"\"");
  TestBytesLiteral("b\"\"\"\"\"\"");
  TestUnescapedBytes("");

  TestBytesLiteral("b'\\000\\x00AAA\\xfF\\377'");
  TestBytesLiteral("b'''\\000\\x00AAA\\xfF\\377'''");
  TestBytesLiteral("b'\\a\\b\\f\\n\\r\\t\\v\\\\\\?\\\"\\'\\`\\x00\\Xff'");
  TestBytesLiteral("b'''\\a\\b\\f\\n\\r\\t\\v\\\\\\?\\\"\\'\\`\\x00\\Xff'''");

  TestBytesLiteral("b'\\n\\012\\x0A'");  // Different newline representations.
  TestBytesLiteral("b'''\\n\\012\\x0A'''");

  // Note the C-escaping to define the bytes.  These are invalid strings for
  // various reasons, but are valid as bytes.
  TestUnescapedBytes("\xc1");
  TestUnescapedBytes("\xca");
  TestUnescapedBytes("\xcc");
  TestUnescapedBytes("\xFA");
  TestUnescapedBytes("\xc1\xca\x1b\x62\x19o\xcc\x04");
}

TEST(BytesTest, ToBytesLiteralTests) {
  // ToBytesLiteral will choose to quote with ' if it will avoid escaping.
  // Non-printable bytes are escaped as hex.  For printable bytes, only the
  // escape character and quote character are escaped.
  EXPECT_EQ("b\"\"", FormatBytesLiteral(""));
  EXPECT_EQ("b\"abc\"", FormatBytesLiteral("abc"));
  EXPECT_EQ("b\"abc'def\"", FormatBytesLiteral("abc'def"));
  EXPECT_EQ("b'abc\"def'", FormatBytesLiteral("abc\"def"));
  EXPECT_EQ("b\"abc`def\"", FormatBytesLiteral("abc`def"));
  EXPECT_EQ("b\"abc'\\\"`def\"", FormatBytesLiteral("abc'\"`def"));

  // Override the quoting style to use single quotes.
  EXPECT_EQ("b''", FormatSingleQuotedBytesLiteral(""));
  EXPECT_EQ("b'abc'", FormatSingleQuotedBytesLiteral("abc"));
  EXPECT_EQ("b'abc\\'def'", FormatSingleQuotedBytesLiteral("abc'def"));
  EXPECT_EQ("b'abc\"def'", FormatSingleQuotedBytesLiteral("abc\"def"));
  EXPECT_EQ("b'abc`def'", FormatSingleQuotedBytesLiteral("abc`def"));
  EXPECT_EQ("b'abc\\'\"`def'", FormatSingleQuotedBytesLiteral("abc'\"`def"));

  // Override the quoting style to use double quotes.
  EXPECT_EQ("b\"\"", FormatDoubleQuotedBytesLiteral(""));
  EXPECT_EQ("b\"abc\"", FormatDoubleQuotedBytesLiteral("abc"));
  EXPECT_EQ("b\"abc'def\"", FormatDoubleQuotedBytesLiteral("abc'def"));
  EXPECT_EQ("b\"abc\\\"def\"", FormatDoubleQuotedBytesLiteral("abc\"def"));
  EXPECT_EQ("b\"abc`def\"", FormatDoubleQuotedBytesLiteral("abc`def"));
  EXPECT_EQ("b\"abc'\\\"`def\"", FormatDoubleQuotedBytesLiteral("abc'\"`def"));

  EXPECT_EQ("b\"\\x07-\\x08-\\x0c-\\x0a-\\x0d-\\x09-\\x0b-\\\\-?-\\\"-'-`\"",
            FormatBytesLiteral("\a-\b-\f-\n-\r-\t-\v-\\-?-\"-'-`"));

  EXPECT_EQ("b\"\\x0a\"", FormatBytesLiteral("\n"));

  ASSERT_OK_AND_ASSIGN(auto unquoted,
                       ParseBytesLiteral("b'\\n\\012\\x0a\\x0A'"));
  EXPECT_EQ("b\"\\x0a\\x0a\\x0a\\x0a\"", FormatBytesLiteral(unquoted));
}

TEST(ByesTest, InvalidBytes) {
  TestInvalidBytes("A", "Invalid bytes literal");    // No quotes
  TestInvalidBytes("b'A", "Invalid bytes literal");  // No ending quote
  TestInvalidBytes("'A'", "Invalid bytes literal");  // No ending quote
  TestInvalidBytes("'A'", "Invalid bytes literal");  // No 'b' prefix.
  TestInvalidBytes("'''A'''");
  TestInvalidBytes("b'k\\u0030'", kUnicodeNotAllowedInBytes1);
  TestInvalidBytes("b'''\\u0030'''", kUnicodeNotAllowedInBytes1);
  TestInvalidBytes("b'\\U00000030'", kUnicodeNotAllowedInBytes2);
  TestInvalidBytes("b'''qwerty\\U00000030'''", kUnicodeNotAllowedInBytes2);
  EXPECT_FALSE(UnescapeBytes("abc\\u0030").ok());
  EXPECT_FALSE(UnescapeBytes("abc\\U00000030").ok());
  EXPECT_FALSE(UnescapeBytes("abc\\U00000030").ok());
}

TEST(RawStringsTest, ValidCases) {
  TestRawString("");
  TestRawString("1");
  TestRawString("\\x53");
  TestRawString("\\x123");
  TestRawString("\\001");
  TestRawString("a\\44'A");
  TestRawString("a\\e");
  TestRawString("\\ea");
  TestRawString("\\U1234");
  TestRawString("\\u");
  TestRawString("\\xc2\\\\");
  TestRawString("f\\(abc',(.*),def\\?");
  TestRawString("a\\\"b");
}

TEST(RawStringsTest, InvalidRawStrings) {
  TestInvalidString("r\"\\\"", "String must end with \"");
  TestInvalidString("r\"\\\\\\\"", "String must end with \"");
  TestInvalidString("r\"");
  TestInvalidString("r");
  TestInvalidString("rb\"\"");
  TestInvalidString("b\"\"");
  TestInvalidString("r'''", "String cannot contain unescaped '");
}

TEST(RawBytesTest, ValidCases) {
  TestRawBytes("");
  TestRawBytes("1");
  TestRawBytes("\\x53");
  TestRawBytes("\\x123");
  TestRawBytes("\\001");
  TestRawBytes("a\\44'A");
  TestRawBytes("a\\e");
  TestRawBytes("\\ea");
  TestRawBytes("\\U1234");
  TestRawBytes("\\u");
  TestRawBytes("\\xc2\\\\");
  TestRawBytes("f\\(abc',(.*),def\\?");
}

TEST(RawBytesTest, InvalidRawBytes) {
  TestInvalidBytes("r''");
  TestInvalidBytes("r''''''");
  TestInvalidBytes("rrb''");
  TestInvalidBytes("brb''");
  TestInvalidBytes("rb'a\\e");
  TestInvalidBytes("rb\"\\\"", "String must end with \"");
  TestInvalidBytes("br\"\\\\\\\"", "String must end with \"");
  TestInvalidBytes("rb");
  TestInvalidBytes("br");
  TestInvalidBytes("rb\"");
  TestInvalidBytes("rb\"\"\"", "String cannot contain unescaped \"");
  TestInvalidBytes("rb\"xyz\"\"", "String cannot contain unescaped \"");
}

TEST(StringsTest, QuotedForms) {
  // EscapeString escapes all quote characters.
  EXPECT_EQ("", EscapeString(""));
  EXPECT_EQ("abc", EscapeString("abc"));
  EXPECT_EQ("abc\\'def", EscapeString("abc'def"));
  EXPECT_EQ("abc\\\"def", EscapeString("abc\"def"));
  EXPECT_EQ("abc\\`def", EscapeString("abc`def"));

  // ToStringLiteral will choose to quote with ' if it will avoid escaping.
  // Other quoted characters will not be escaped.
  EXPECT_EQ("\"\"", FormatStringLiteral(""));
  EXPECT_EQ("\"abc\"", FormatStringLiteral("abc"));
  EXPECT_EQ("\"abc'def\"", FormatStringLiteral("abc'def"));
  EXPECT_EQ("'abc\"def'", FormatStringLiteral("abc\"def"));
  EXPECT_EQ("\"abc`def\"", FormatStringLiteral("abc`def"));
  EXPECT_EQ("\"abc'\\\"`def\"", FormatStringLiteral("abc'\"`def"));

  // Override the quoting style to use single quotes.
  EXPECT_EQ("''", FormatSingleQuotedStringLiteral(""));
  EXPECT_EQ("'abc'", FormatSingleQuotedStringLiteral("abc"));
  EXPECT_EQ("'abc\\'def'", FormatSingleQuotedStringLiteral("abc'def"));
  EXPECT_EQ("'abc\"def'", FormatSingleQuotedStringLiteral("abc\"def"));
  EXPECT_EQ("'abc`def'", FormatSingleQuotedStringLiteral("abc`def"));
  EXPECT_EQ("'abc\\'\"`def'", FormatSingleQuotedStringLiteral("abc'\"`def"));

  // Override the quoting style to use double quotes.
  EXPECT_EQ("\"\"", FormatDoubleQuotedStringLiteral(""));
  EXPECT_EQ("\"abc\"", FormatDoubleQuotedStringLiteral("abc"));
  EXPECT_EQ("\"abc'def\"", FormatDoubleQuotedStringLiteral("abc'def"));
  EXPECT_EQ("\"abc\\\"def\"", FormatDoubleQuotedStringLiteral("abc\"def"));
  EXPECT_EQ("\"abc`def\"", FormatDoubleQuotedStringLiteral("abc`def"));
  EXPECT_EQ("\"abc'\\\"`def\"", FormatDoubleQuotedStringLiteral("abc'\"`def"));
}

void ExpectParsedString(const std::string& expected,
                        const std::vector<std::string>& quoted_strings) {
  for (const std::string& quoted : quoted_strings) {
    ASSERT_OK_AND_ASSIGN(auto parsed, ParseStringLiteral(quoted));
    EXPECT_EQ(expected, parsed);
  }
}

void ExpectParsedBytes(const std::string& expected,
                       const std::vector<std::string>& quoted_strings) {
  for (const std::string& quoted : quoted_strings) {
    ASSERT_OK_AND_ASSIGN(auto parsed, ParseBytesLiteral(quoted));
    EXPECT_EQ(expected, parsed);
  }
}

TEST(StringsTest, Parse) {
  ExpectParsedString("abc",
                     {"'abc'", "\"abc\"", "'''abc'''", "\"\"\"abc\"\"\""});
  ExpectParsedString(
      "abc\ndef\x12ghi",
      {"'abc\\ndef\\x12ghi'", "\"abc\\ndef\\x12ghi\"",
       "'''abc\\ndef\\x12ghi'''", "\"\"\"abc\\ndef\\x12ghi\"\"\""});
  ExpectParsedString("\xF4\x8F\xBF\xBD",
                     {"'\\U0010FFFD'", "\"\\U0010FFFD\"", "'''\\U0010FFFD'''",
                      "\"\"\"\\U0010FFFD\"\"\""});

  // Some more test cases for triple quoted content.
  ExpectParsedString("", {"''''''", "\"\"\"\"\"\""});
  ExpectParsedString("'\"", {"''''\"'''"});
  ExpectParsedString("''''''", {"'''''\\'''\\''''"});
  ExpectParsedString("'", {"'''\\''''"});
  ExpectParsedString("''", {"'''\\'\\''''"});
  ExpectParsedString("'\"", {"''''\"'''"});
  ExpectParsedString("'a", {"''''a'''"});
  ExpectParsedString("\"a", {"\"\"\"\"a\"\"\""});
  ExpectParsedString("''a", {"'''''a'''"});
  ExpectParsedString("\"\"a", {"\"\"\"\"\"a\"\"\""});
}

TEST(StringsTest, TestNewlines) {
  ExpectParsedString("a\nb", {"'''a\rb'''", "'''a\nb'''", "'''a\r\nb'''"});
  ExpectParsedString("a\n\nb", {"'''a\n\rb'''", "'''a\r\n\r\nb'''"});
  // Escaped newlines.
  ExpectParsedString("a\nb", {"'''a\\nb'''"});
  ExpectParsedString("a\rb", {"'''a\\rb'''"});
  ExpectParsedString("a\r\nb", {"'''a\\r\\nb'''"});
}

TEST(RawStringsTest, CompareRawAndRegularStringParsing) {
  ExpectParsedString("\\n",
                     {"r'\\n'", "r\"\\n\"", "r'''\\n'''", "r\"\"\"\\n\"\"\""});
  ExpectParsedString("\n",
                     {"'\\n'", "\"\\n\"", "'''\\n'''", "\"\"\"\\n\"\"\""});

  ExpectParsedString("\\e",
                     {"r'\\e'", "r\"\\e\"", "r'''\\e'''", "r\"\"\"\\e\"\"\""});
  TestInvalidString("'\\e'", "Illegal escape sequence: \\e");
  TestInvalidString("\"\\e\"", "Illegal escape sequence: \\e");
  TestInvalidString("'''\\e'''", "Illegal escape sequence: \\e");
  TestInvalidString("\"\"\"\\e\"\"\"", "Illegal escape sequence: \\e");

  ExpectParsedString(
      "\\x0", {"r'\\x0'", "r\"\\x0\"", "r'''\\x0'''", "r\"\"\"\\x0\"\"\""});
  constexpr char kHexError[] =
      "Hex escape must be followed by 2 hex digits but saw: \\x0";
  TestInvalidString("'\\x0'", kHexError);
  TestInvalidString("\"\\x0\"", kHexError);
  TestInvalidString("'''\\x0'''", kHexError);
  TestInvalidString("\"\"\"\\x0\"\"\"", kHexError);

  ExpectParsedString("\\'", {"r'\\\''"});
  ExpectParsedString("'", {"'\\\''"});
  ExpectParsedString("\\\"", {"r\"\\\"\""});
  ExpectParsedString("\"", {"\"\\\"\""});
  ExpectParsedString("''\\'", {"r'''\'\'\\\''''"});
  ExpectParsedString("'''", {"'''\'\'\\\''''"});
  ExpectParsedString("\"\"\\\"", {"r\"\"\"\"\"\\\"\"\"\""});
  ExpectParsedString("\"\"\"", {"\"\"\"\"\"\\\"\"\"\""});
}

TEST(RawBytesTest, CompareRawAndRegularBytesParsing) {
  ExpectParsedBytes("\\n", {"rb'\\n'", "br'\\n'", "rb\"\\n\"", "br\"\\n\""});
  ExpectParsedBytes("\n", {"b'\\n'", "b\"\\n\""});

  ExpectParsedBytes("\\u0030", {"rb'\\u0030'", "br'\\u0030'", "rb\"\\u0030\"",
                                "br\"\\u0030\""});
  TestInvalidBytes("b'\\u0030'", kUnicodeNotAllowedInBytes1);
  TestInvalidBytes("b\"\\u0030\"", kUnicodeNotAllowedInBytes1);
  TestInvalidBytes("b\"abc\\u0030\"", kUnicodeNotAllowedInBytes1);

  ExpectParsedBytes("\\U00000030", {"rb'\\U00000030'", "br'\\U00000030'",
                                    "rb\"\\U00000030\"", "br\"\\U00000030\""});
  TestInvalidBytes("b'\\U00000030'", kUnicodeNotAllowedInBytes2);
  TestInvalidBytes("b\"\\U00000030\"", kUnicodeNotAllowedInBytes2);
  TestInvalidBytes("b\"abc\\U00000030\"", kUnicodeNotAllowedInBytes2);

  ExpectParsedBytes("\\e", {"rb'\\e'", "br'\\e'", "rb\"\\e\"", "br\"\\e\""});
  TestInvalidBytes("b'\\e'", "Illegal escape sequence: \\e");
  TestInvalidBytes("b\"\\e\"", "Illegal escape sequence: \\e");
  TestInvalidBytes("b\"abcd\\e\"", "Illegal escape sequence: \\e");

  ExpectParsedBytes("\\'", {"rb'\\\''", "br'\\\''"});
  ExpectParsedBytes("'", {"b'\\\''"});
  ExpectParsedBytes("\\\"", {"rb\"\\\"\"", "br\"\\\"\""});
  ExpectParsedBytes("\"", {"b\"\\\"\""});
  ExpectParsedBytes("''\\'", {"rb'''\'\'\\\''''", "br'''\'\'\\\''''"});
  ExpectParsedBytes("'''", {"b'''\'\'\\\''''"});
  ExpectParsedBytes("\"\"\\\"",
                    {"rb\"\"\"\"\"\\\"\"\"\"", "br\"\"\"\"\"\\\"\"\"\""});
  ExpectParsedBytes("\"\"\"", {"b\"\"\"\"\"\\\"\"\"\""});
}

struct epair {
  std::string escaped;
  std::string unescaped;
};

// Copied from strings/escaping_test.cc, CEscape::BasicEscaping.
TEST(StringsTest, UTF8Escape) {
  epair utf8_hex_values[] = {
      {"\x20\xe4\xbd\xa0\\t\xe5\xa5\xbd,\\r!\\n",
       "\x20\xe4\xbd\xa0\t\xe5\xa5\xbd,\r!\n"},
      {"\xe8\xa9\xa6\xe9\xa8\x93\\\' means \\\"test\\\"",
       "\xe8\xa9\xa6\xe9\xa8\x93\' means \"test\""},
      {"\\\\\xe6\x88\x91\\\\:\\\\\xe6\x9d\xa8\xe6\xac\xa2\\\\",
       "\\\xe6\x88\x91\\:\\\xe6\x9d\xa8\xe6\xac\xa2\\"},
      {"\xed\x81\xac\xeb\xa1\xac\\x08\\t\\n\\x0b\\x0c\\r",
       "\xed\x81\xac\xeb\xa1\xac\010\011\012\013\014\015"}};

  for (int i = 0; i < ABSL_ARRAYSIZE(utf8_hex_values); ++i) {
    std::string escaped = EscapeString(utf8_hex_values[i].unescaped);
    EXPECT_EQ(escaped, utf8_hex_values[i].escaped);
  }
}

// Originally copied from strings/escaping_test.cc, Unescape::BasicFunction,
// but changes for '\\xABCD' which only parses 2 hex digits after the escape.
TEST(StringsTest, UTF8Unescape) {
  epair tests[] = {{"\\u0030", "0"},
                   {"\\u00A3", "\xC2\xA3"},
                   {"\\u22FD", "\xE2\x8B\xBD"},
                   {"\\ud7FF", "\xED\x9F\xBF"},
                   {"\\u22FD", "\xE2\x8B\xBD"},
                   {"\\U00010000", "\xF0\x90\x80\x80"},
                   {"\\U0000E000", "\xEE\x80\x80"},
                   {"\\U0001DFFF", "\xF0\x9D\xBF\xBF"},
                   {"\\U0010FFFD", "\xF4\x8F\xBF\xBD"},
                   {"\\xAbCD",
                    "\xc2\xab"
                    "CD"},
                   {"\\253CD",
                    "\xc2\xab"
                    "CD"},
                   {"\\x4141", "A41"}};
  for (int i = 0; i < ABSL_ARRAYSIZE(tests); ++i) {
    const std::string& e = tests[i].escaped;
    const std::string& u = tests[i].unescaped;
    ASSERT_OK_AND_ASSIGN(auto out, UnescapeString(e));
    EXPECT_EQ(u, out) << "original escaped: '" << e << "'\nunescaped: '" << out
                      << "'\nexpected unescaped: '" << u << "'";
  }
  std::string bad[] = {"\\u1",                 // too short
                       "\\U1",                 // too short
                       "\\Uffffff", "\\777"};  // exceeds 0xff
  for (int i = 0; i < ABSL_ARRAYSIZE(bad); ++i) {
    const std::string& e = bad[i];
    auto status_or_string = UnescapeString(e);
    EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kInvalidArgument));
    EXPECT_TRUE(absl::StrContains(status_or_string.status().message(),
                                  "Invalid escaped string"));
  }
}

TEST(StringsTest, TestUnescapeErrorMessages) {
  std::string error_string;
  std::string out;

  auto status_or_string = UnescapeString("\\2");
  EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_TRUE(absl::StrContains(
      status_or_string.status().message(),
      "Illegal escape sequence: Octal escape must be followed by 3 octal "
      "digits but saw: \\2"));

  status_or_string = UnescapeString("\\22X0");
  EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_TRUE(absl::StrContains(
      status_or_string.status().message(),
      "Illegal escape sequence: Octal escape must be followed by 3 octal "
      "digits but saw: \\22X"));

  status_or_string = UnescapeString("\\X0");
  EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_TRUE(absl::StrContains(
      status_or_string.status().message(),
      "Illegal escape sequence: Hex escape must be followed by 2 hex digits "
      "but saw: \\X0"));

  status_or_string = UnescapeString("\\x0G0");
  EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_TRUE(absl::StrContains(
      status_or_string.status().message(),
      "Illegal escape sequence: Hex escape must be followed by 2 hex digits "
      "but saw: \\x0G"));

  status_or_string = UnescapeString("\\u00");
  EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_TRUE(absl::StrContains(
      status_or_string.status().message(),
      "Illegal escape sequence: \\u must be followed by 4 hex digits but saw: "
      "\\u00"));

  status_or_string = UnescapeString("\\ude8c");
  EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_TRUE(absl::StrContains(
      status_or_string.status().message(),
      "Illegal escape sequence: Unicode value \\ude8c is invalid"));

  status_or_string = UnescapeString("\\u000G0");
  EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_TRUE(absl::StrContains(
      status_or_string.status().message(),
      "Illegal escape sequence: \\u must be followed by 4 hex digits but saw: "
      "\\u000G"));

  status_or_string = UnescapeString("\\U00");
  EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_TRUE(absl::StrContains(
      status_or_string.status().message(),
      "Illegal escape sequence: \\U must be followed by 8 hex digits but saw: "
      "\\U00"));

  status_or_string = UnescapeString("\\U000000G00");
  EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_TRUE(absl::StrContains(
      status_or_string.status().message(),
      "Illegal escape sequence: \\U must be followed by 8 hex digits but saw: "
      "\\U000000G0"));

  status_or_string = UnescapeString("\\U0000D83D");
  EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_TRUE(absl::StrContains(
      status_or_string.status().message(),
      "Illegal escape sequence: Unicode value \\U0000D83D is invalid"));

  status_or_string = UnescapeString("\\UFFFFFFFF0");
  EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_TRUE(absl::StrContains(
      status_or_string.status().message(),
      "Illegal escape sequence: Value of \\UFFFFFFFF exceeds Unicode limit "
      "(0x0010FFFF)"));
}

}  // namespace
}  // namespace cel::internal
