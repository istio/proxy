#include "quiche/balsa/header_properties.h"

#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quiche::header_properties::test {
namespace {

TEST(HeaderPropertiesTest, IsMultivaluedHeaderIsCaseInsensitive) {
  EXPECT_TRUE(IsMultivaluedHeader("content-encoding"));
  EXPECT_TRUE(IsMultivaluedHeader("Content-Encoding"));
  EXPECT_TRUE(IsMultivaluedHeader("set-cookie"));
  EXPECT_TRUE(IsMultivaluedHeader("sEt-cOOkie"));
  EXPECT_TRUE(IsMultivaluedHeader("X-Goo" /**/ "gle-Cache-Control"));
  EXPECT_TRUE(IsMultivaluedHeader("access-control-expose-HEADERS"));

  EXPECT_FALSE(IsMultivaluedHeader("set-cook"));
  EXPECT_FALSE(IsMultivaluedHeader("content-length"));
  EXPECT_FALSE(IsMultivaluedHeader("Content-Length"));
}

TEST(HeaderPropertiesTest, IsInvalidHeaderKeyChar) {
  EXPECT_TRUE(IsInvalidHeaderKeyChar(0x00));
  EXPECT_TRUE(IsInvalidHeaderKeyChar(0x06));
  EXPECT_TRUE(IsInvalidHeaderKeyChar(0x09));
  EXPECT_TRUE(IsInvalidHeaderKeyChar(0x1F));
  EXPECT_TRUE(IsInvalidHeaderKeyChar(0x7F));
  EXPECT_TRUE(IsInvalidHeaderKeyChar(' '));
  EXPECT_TRUE(IsInvalidHeaderKeyChar('"'));
  EXPECT_TRUE(IsInvalidHeaderKeyChar('\t'));
  EXPECT_TRUE(IsInvalidHeaderKeyChar('\r'));
  EXPECT_TRUE(IsInvalidHeaderKeyChar('\n'));
  EXPECT_TRUE(IsInvalidHeaderKeyChar('}'));

  EXPECT_FALSE(IsInvalidHeaderKeyChar('a'));
  EXPECT_FALSE(IsInvalidHeaderKeyChar('B'));
  EXPECT_FALSE(IsInvalidHeaderKeyChar('7'));
  EXPECT_FALSE(IsInvalidHeaderKeyChar(0x42));
  EXPECT_FALSE(IsInvalidHeaderKeyChar(0x7C));
  EXPECT_FALSE(IsInvalidHeaderKeyChar(0x7E));
}

TEST(HeaderPropertiesTest, IsInvalidHeaderKeyCharAllowDoubleQuote) {
  EXPECT_TRUE(IsInvalidHeaderKeyCharAllowDoubleQuote(0x00));
  EXPECT_TRUE(IsInvalidHeaderKeyCharAllowDoubleQuote(0x06));
  EXPECT_TRUE(IsInvalidHeaderKeyCharAllowDoubleQuote(0x09));
  EXPECT_TRUE(IsInvalidHeaderKeyCharAllowDoubleQuote(0x1F));
  EXPECT_TRUE(IsInvalidHeaderKeyCharAllowDoubleQuote(0x7F));
  EXPECT_TRUE(IsInvalidHeaderKeyCharAllowDoubleQuote(' '));
  EXPECT_TRUE(IsInvalidHeaderKeyCharAllowDoubleQuote('\t'));
  EXPECT_TRUE(IsInvalidHeaderKeyCharAllowDoubleQuote('\r'));
  EXPECT_TRUE(IsInvalidHeaderKeyCharAllowDoubleQuote('\n'));
  EXPECT_TRUE(IsInvalidHeaderKeyCharAllowDoubleQuote('}'));

  EXPECT_FALSE(IsInvalidHeaderKeyCharAllowDoubleQuote('"'));
  EXPECT_FALSE(IsInvalidHeaderKeyCharAllowDoubleQuote('a'));
  EXPECT_FALSE(IsInvalidHeaderKeyCharAllowDoubleQuote('B'));
  EXPECT_FALSE(IsInvalidHeaderKeyCharAllowDoubleQuote('7'));
  EXPECT_FALSE(IsInvalidHeaderKeyCharAllowDoubleQuote(0x42));
  EXPECT_FALSE(IsInvalidHeaderKeyCharAllowDoubleQuote(0x7C));
  EXPECT_FALSE(IsInvalidHeaderKeyCharAllowDoubleQuote(0x7E));
}

TEST(HeaderPropertiesTest, IsInvalidHeaderChar) {
  EXPECT_TRUE(IsInvalidHeaderChar(0x00));
  EXPECT_TRUE(IsInvalidHeaderChar(0x06));
  EXPECT_TRUE(IsInvalidHeaderChar(0x1F));
  EXPECT_TRUE(IsInvalidHeaderChar(0x7F));

  EXPECT_FALSE(IsInvalidHeaderChar(0x09));
  EXPECT_FALSE(IsInvalidHeaderChar(' '));
  EXPECT_FALSE(IsInvalidHeaderChar('\t'));
  EXPECT_FALSE(IsInvalidHeaderChar('\r'));
  EXPECT_FALSE(IsInvalidHeaderChar('\n'));
  EXPECT_FALSE(IsInvalidHeaderChar('a'));
  EXPECT_FALSE(IsInvalidHeaderChar('B'));
  EXPECT_FALSE(IsInvalidHeaderChar('7'));
  EXPECT_FALSE(IsInvalidHeaderChar(0x42));
  EXPECT_FALSE(IsInvalidHeaderChar(0x7D));
}

TEST(HeaderPropertiesTest, KeyMoreRestrictiveThanValue) {
  for (int c = 0; c < 255; ++c) {
    if (IsInvalidHeaderChar(c)) {
      EXPECT_TRUE(IsInvalidHeaderKeyChar(c)) << c;
    }
  }
}

TEST(HeaderPropertiesTest, HasInvalidHeaderChars) {
  const char with_null[] = "Here's l\x00king at you, kid";
  EXPECT_TRUE(HasInvalidHeaderChars(std::string(with_null, sizeof(with_null))));
  EXPECT_TRUE(HasInvalidHeaderChars("Why's \x06 afraid of \x07? \x07\x08\x09"));
  EXPECT_TRUE(HasInvalidHeaderChars("\x1Flower power"));
  EXPECT_TRUE(HasInvalidHeaderChars("\x7Flowers more powers"));

  EXPECT_FALSE(HasInvalidHeaderChars("Plenty of space"));
  EXPECT_FALSE(HasInvalidHeaderChars("Keeping \tabs"));
  EXPECT_FALSE(HasInvalidHeaderChars("Al\right"));
  EXPECT_FALSE(HasInvalidHeaderChars("\new day"));
  EXPECT_FALSE(HasInvalidHeaderChars("\x42 is a nice character"));
}

TEST(HeaderPropertiesTest, HasInvalidPathChar) {
  EXPECT_FALSE(HasInvalidPathChar(""));
  EXPECT_FALSE(HasInvalidPathChar("/"));
  EXPECT_FALSE(HasInvalidPathChar("invalid_path/but/valid/chars"));
  EXPECT_FALSE(HasInvalidPathChar("/path/with?query;fragment"));
  EXPECT_FALSE(HasInvalidPathChar("/path2.fun/my_site-root/!&$=,+*()/wow"));
  // Surprise! []{}^| are seen in requests on the internet.
  EXPECT_FALSE(HasInvalidPathChar("/square[brackets]surprisingly/allowed"));
  EXPECT_FALSE(HasInvalidPathChar("/curly{braces}surprisingly/allowed"));
  EXPECT_FALSE(HasInvalidPathChar("/caret^pipe|surprisingly/allowed"));
  // Surprise! Chrome sends backslash in query params, sometimes.
  EXPECT_FALSE(HasInvalidPathChar("/path/with?backslash\\hooray"));

  EXPECT_TRUE(HasInvalidPathChar("/path with spaces"));
  EXPECT_TRUE(HasInvalidPathChar("/path\rwith\tother\nwhitespace"));
  EXPECT_TRUE(HasInvalidPathChar("/backtick`"));
  EXPECT_TRUE(HasInvalidPathChar("/angle<brackets>also/bad"));
}

TEST(HeaderPropertiesTest, HasInvalidQueryChar) {
  EXPECT_FALSE(HasInvalidQueryChar(""));
  EXPECT_FALSE(HasInvalidQueryChar("/"));
  EXPECT_FALSE(HasInvalidQueryChar("valid_query/chars"));
  EXPECT_FALSE(HasInvalidQueryChar("query;fragment"));
  EXPECT_FALSE(HasInvalidQueryChar("query2.fun/my_site-root/!&$=,+*()/wow"));
  // Surprise! []{}^| are seen in requests on the internet.
  EXPECT_FALSE(HasInvalidQueryChar("square[brackets]surprisingly/allowed"));
  EXPECT_FALSE(HasInvalidQueryChar("curly{braces}surprisingly/allowed"));
  EXPECT_FALSE(HasInvalidQueryChar("caret^pipe|surprisingly/allowed"));
  // Surprise! Chrome sends backslash in query params, sometimes.
  EXPECT_FALSE(HasInvalidQueryChar("query_with?backslash\\hooray"));
  // Query params sometimes contain backtick or double quote.
  EXPECT_FALSE(HasInvalidQueryChar("backtick`"));
  EXPECT_FALSE(HasInvalidQueryChar("double\"quote"));

  EXPECT_TRUE(HasInvalidQueryChar("query with spaces"));
  EXPECT_TRUE(HasInvalidQueryChar("query\rwith\tother\nwhitespace"));
  EXPECT_TRUE(HasInvalidQueryChar("query_with_angle<brackets>also_bad"));
}

TEST(HeaderPropertiesTest, IsValidTokenVsHasInvalidHeaderChars) {
  absl::flat_hash_set<unsigned char> mismatch = {':'};
  for (int c = 0; c < 128; ++c) {
    if (mismatch.contains(c)) {
      continue;
    }

    unsigned char u_c = static_cast<unsigned char>(c);
    std::string s(1, u_c);
    EXPECT_EQ(IsValidToken(s), !IsInvalidHeaderKeyChar(u_c))
        << "char: [" << u_c << "], int = [" << c << "]";
  }
}

TEST(HeaderPropertiesTest, IsValidTokenEmptyAndMultiChar) {
  EXPECT_TRUE(IsValidToken("a"));
  EXPECT_TRUE(IsValidToken("GET"));
  EXPECT_TRUE(IsValidToken("GET'"));
  EXPECT_TRUE(IsValidToken("a-b-c"));
  EXPECT_TRUE(IsValidToken("!#$%&'*+-.^_`|~"));
  EXPECT_TRUE(IsValidToken("abcefghijklmnopqrstuvwxyz0123456789"));
  EXPECT_TRUE(
      IsValidToken("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                   "abcdefghijklmnopqrstuvwxyz"
                   "0123456789"
                   "!#$%&'*+-.^_`|~"));

  EXPECT_FALSE(IsValidToken("G ET"));
  EXPECT_FALSE(IsValidToken("G,ET"));
  EXPECT_FALSE(IsValidToken("G\tET"));
  EXPECT_FALSE(IsValidToken(absl::string_view("G\0ET", 3)));
  EXPECT_FALSE(IsValidToken("GET\""));
  EXPECT_FALSE(IsValidToken("GET\x85"));
  EXPECT_FALSE(IsValidToken("GET("));
  EXPECT_FALSE(IsValidToken("GET)"));
  EXPECT_FALSE(IsValidToken("GET{"));
  EXPECT_FALSE(IsValidToken("GET}"));
  EXPECT_FALSE(IsValidToken("GET}"));
  EXPECT_FALSE(IsValidToken("GET@"));
  EXPECT_FALSE(IsValidToken("GET["));
  EXPECT_FALSE(IsValidToken("GET\\"));
  EXPECT_FALSE(IsValidToken("GET\""));
  EXPECT_FALSE(IsValidToken("GET]"));
  EXPECT_FALSE(IsValidToken("GET:"));
  EXPECT_FALSE(IsValidToken("GET;"));
  EXPECT_FALSE(IsValidToken("GET?"));
  EXPECT_FALSE(IsValidToken("GET="));
  EXPECT_FALSE(IsValidToken("GET/"));
  EXPECT_FALSE(IsValidToken("GET\""));
  EXPECT_FALSE(IsValidToken("GET<"));
  EXPECT_FALSE(IsValidToken("GET>"));
  EXPECT_FALSE(IsValidToken("GET,"));
  EXPECT_FALSE(IsValidToken("GET\x7F"));
  EXPECT_FALSE(IsValidToken(""));
}

TEST(HeaderPropertiesTest, IsValidChunkExtensionValChar) {
  EXPECT_TRUE(IsValidChunkExtension(""));
  EXPECT_TRUE(IsValidChunkExtension(";"));
  EXPECT_TRUE(IsValidChunkExtension(";a"));
  EXPECT_TRUE(IsValidChunkExtension("; a"));
  EXPECT_TRUE(IsValidChunkExtension("\""));
  EXPECT_TRUE(IsValidChunkExtension(";\t a"));
  EXPECT_TRUE(IsValidChunkExtension(";a="));
  EXPECT_TRUE(IsValidChunkExtension(";a=b"));
  EXPECT_TRUE(IsValidChunkExtension(";a=\"\""));
  EXPECT_TRUE(IsValidChunkExtension(";a=\"ba\"z\""));
  EXPECT_TRUE(IsValidChunkExtension(";a=foo-quote'"));
  EXPECT_TRUE(IsValidChunkExtension(";foo=bar;baz=qux"));
  EXPECT_TRUE(IsValidChunkExtension(";foo=bar;baz=\"qu\";x"));
  EXPECT_TRUE(IsValidChunkExtension(";foo=bar;baz=\"q \tu\";x"));
  EXPECT_TRUE(IsValidChunkExtension("!#$%&'*+-.^_`|~"));
  EXPECT_TRUE(IsValidChunkExtension("abcefghijklmnopqrstuvwxyz0123456789"));
  EXPECT_TRUE(
      IsValidChunkExtension("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                            "abcdefghijklmnopqrstuvwxyz"
                            "0123456789"
                            "!#$%&'*+-.^_`|~"));
  EXPECT_TRUE(IsValidChunkExtension("GET\xFF"));
  EXPECT_TRUE(IsValidChunkExtension("GET\x80"));

  EXPECT_FALSE(
      IsValidChunkExtension(absl::string_view(";chunky-nulls=\0", 15)));
  EXPECT_FALSE(IsValidChunkExtension(";\x1"));
  EXPECT_FALSE(IsValidChunkExtension(";\n"));
  EXPECT_FALSE(IsValidChunkExtension(";\r"));
}

TEST(HeaderPropertiesTest, IsValidChunkExtension) {
  for (int i = 0; i < 256; ++i) {
    unsigned char c = static_cast<unsigned char>(i);
    if (c == ' ' || c == '\t') {
      continue;
    }
    if (absl::ascii_iscntrl(c)) {
      EXPECT_FALSE(IsValidChunkExtensionValChar(c));
    } else if (absl::ascii_isprint(c)) {
      EXPECT_TRUE(IsValidChunkExtensionValChar(c));
    } else if (absl::ascii_isspace(c)) {
      EXPECT_TRUE(IsValidChunkExtensionValChar(c));
    } else if (i >= 128) {
      EXPECT_TRUE(IsValidChunkExtensionValChar(c));
    } else {
      FAIL() << "Unexpected character: [" << c << "], int = [" << i << "]";
    }
  }
}

}  // namespace
}  // namespace quiche::header_properties::test
