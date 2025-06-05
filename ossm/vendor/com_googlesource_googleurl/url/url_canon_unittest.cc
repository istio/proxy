// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/url_canon.h"

#include <errno.h>
#include <stddef.h>

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon_internal.h"
#include "url/url_canon_stdstring.h"
#include "url/url_test_utils.h"

namespace url {

namespace {

struct ComponentCase {
  const char* input;
  const char* expected;
  Component expected_component;
  bool expected_success;
};

// ComponentCase but with dual 8-bit/16-bit input. Generally, the unit tests
// treat each input as optional, and will only try processing if non-NULL.
// The output is always 8-bit.
struct DualComponentCase {
  const char* input8;
  const wchar_t* input16;
  const char* expected;
  Component expected_component;
  bool expected_success;
};

// Test cases for CanonicalizeIPAddress(). The inputs are identical to
// DualComponentCase, but the output has extra CanonHostInfo fields.
struct IPAddressCase {
  const char* input8;
  const wchar_t* input16;
  const char* expected;
  Component expected_component;

  // CanonHostInfo fields, for verbose output.
  CanonHostInfo::Family expected_family;
  int expected_num_ipv4_components;
  const char* expected_address_hex;  // Two hex chars per IP address byte.
};

std::string BytesToHexString(unsigned char bytes[16], int length) {
  EXPECT_TRUE(length == 0 || length == 4 || length == 16)
      << "Bad IP address length: " << length;
  std::string result;
  for (int i = 0; i < length; ++i) {
    result.push_back(kHexCharLookup[(bytes[i] >> 4) & 0xf]);
    result.push_back(kHexCharLookup[bytes[i] & 0xf]);
  }
  return result;
}

struct ReplaceCase {
  const char* base;
  const char* scheme;
  const char* username;
  const char* password;
  const char* host;
  const char* port;
  const char* path;
  const char* query;
  const char* ref;
  const char* expected;
};

// Magic string used in the replacements code that tells SetupReplComp to
// call the clear function.
const char kDeleteComp[] = "|";

// Sets up a replacement for a single component. This is given pointers to
// the set and clear function for the component being replaced, and will
// either set the component (if it exists) or clear it (if the replacement
// string matches kDeleteComp).
//
// This template is currently used only for the 8-bit case, and the strlen
// causes it to fail in other cases. It is left a template in case we have
// tests for wide replacements.
template<typename CHAR>
void SetupReplComp(
    void (Replacements<CHAR>::*set)(const CHAR*, const Component&),
    void (Replacements<CHAR>::*clear)(),
    Replacements<CHAR>* rep,
    const CHAR* str) {
  if (str && str[0] == kDeleteComp[0]) {
    (rep->*clear)();
  } else if (str) {
    (rep->*set)(str, Component(0, static_cast<int>(strlen(str))));
  }
}

}  // namespace

TEST(URLCanonTest, DoAppendUTF8) {
  struct UTF8Case {
    unsigned input;
    const char* output;
  } utf_cases[] = {
    // Valid code points.
    {0x24, "\x24"},
    {0xA2, "\xC2\xA2"},
    {0x20AC, "\xE2\x82\xAC"},
    {0x24B62, "\xF0\xA4\xAD\xA2"},
    {0x10FFFF, "\xF4\x8F\xBF\xBF"},
  };
  std::string out_str;
  for (size_t i = 0; i < std::size(utf_cases); i++) {
    out_str.clear();
    StdStringCanonOutput output(&out_str);
    AppendUTF8Value(utf_cases[i].input, &output);
    output.Complete();
    EXPECT_EQ(utf_cases[i].output, out_str);
  }
}

TEST(URLCanonTest, DoAppendUTF8Invalid) {
  std::string out_str;
  StdStringCanonOutput output(&out_str);
  // Invalid code point (too large).
  EXPECT_DCHECK_DEATH({
    AppendUTF8Value(0x110000, &output);
    output.Complete();
  });
}

TEST(URLCanonTest, UTF) {
  // Low-level test that we handle reading, canonicalization, and writing
  // UTF-8/UTF-16 strings properly.
  struct UTFCase {
    const char* input8;
    const wchar_t* input16;
    bool expected_success;
    const char* output;
  } utf_cases[] = {
      // Valid canonical input should get passed through & escaped.
      {"\xe4\xbd\xa0\xe5\xa5\xbd", L"\x4f60\x597d", true, "%E4%BD%A0%E5%A5%BD"},
      // Test a character that takes > 16 bits (U+10300 = old italic letter A)
      {"\xF0\x90\x8C\x80", L"\xd800\xdf00", true, "%F0%90%8C%80"},
      // Non-shortest-form UTF-8 characters are invalid. The bad bytes should
      // each be replaced with the invalid character (EF BF DB in UTF-8).
      {"\xf0\x84\xbd\xa0\xe5\xa5\xbd", nullptr, false,
       "%EF%BF%BD%EF%BF%BD%EF%BF%BD%EF%BF%BD%E5%A5%BD"},
      // Invalid UTF-8 sequences should be marked as invalid (the first
      // sequence is truncated).
      {"\xe4\xa0\xe5\xa5\xbd", L"\xd800\x597d", false, "%EF%BF%BD%E5%A5%BD"},
      // Character going off the end.
      {"\xe4\xbd\xa0\xe5\xa5", L"\x4f60\xd800", false, "%E4%BD%A0%EF%BF%BD"},
      // ...same with low surrogates with no high surrogate.
      {nullptr, L"\xdc00", false, "%EF%BF%BD"},
      // Test a UTF-8 encoded surrogate value is marked as invalid.
      // ED A0 80 = U+D800
      {"\xed\xa0\x80", nullptr, false, "%EF%BF%BD%EF%BF%BD%EF%BF%BD"},
      // ...even when paired.
      {"\xed\xa0\x80\xed\xb0\x80", nullptr, false,
       "%EF%BF%BD%EF%BF%BD%EF%BF%BD%EF%BF%BD%EF%BF%BD%EF%BF%BD"},
  };

  std::string out_str;
  for (size_t i = 0; i < std::size(utf_cases); i++) {
    if (utf_cases[i].input8) {
      out_str.clear();
      StdStringCanonOutput output(&out_str);

      size_t input_len = strlen(utf_cases[i].input8);
      bool success = true;
      for (size_t ch = 0; ch < input_len; ch++) {
        success &= AppendUTF8EscapedChar(utf_cases[i].input8, &ch, input_len,
                                         &output);
      }
      output.Complete();
      EXPECT_EQ(utf_cases[i].expected_success, success);
      EXPECT_EQ(std::string(utf_cases[i].output), out_str);
    }
    if (utf_cases[i].input16) {
      out_str.clear();
      StdStringCanonOutput output(&out_str);

      std::u16string input_str(
          test_utils::TruncateWStringToUTF16(utf_cases[i].input16));
      size_t input_len = input_str.length();
      bool success = true;
      for (size_t ch = 0; ch < input_len; ch++) {
        success &= AppendUTF8EscapedChar(input_str.c_str(), &ch, input_len,
                                         &output);
      }
      output.Complete();
      EXPECT_EQ(utf_cases[i].expected_success, success);
      EXPECT_EQ(std::string(utf_cases[i].output), out_str);
    }

    if (utf_cases[i].input8 && utf_cases[i].input16 &&
        utf_cases[i].expected_success) {
      // Check that the UTF-8 and UTF-16 inputs are equivalent.

      // UTF-16 -> UTF-8
      std::string input8_str(utf_cases[i].input8);
      std::u16string input16_str(
          test_utils::TruncateWStringToUTF16(utf_cases[i].input16));
      EXPECT_EQ(input8_str, gurl_base::UTF16ToUTF8(input16_str));

      // UTF-8 -> UTF-16
      EXPECT_EQ(input16_str, gurl_base::UTF8ToUTF16(input8_str));
    }
  }
}

TEST(URLCanonTest, Scheme) {
  // Here, we're mostly testing that unusual characters are handled properly.
  // The canonicalizer doesn't do any parsing or whitespace detection. It will
  // also do its best on error, and will escape funny sequences (these won't be
  // valid schemes and it will return error).
  //
  // Note that the canonicalizer will append a colon to the output to separate
  // out the rest of the URL, which is not present in the input. We check,
  // however, that the output range includes everything but the colon.
  ComponentCase scheme_cases[] = {
    {"http", "http:", Component(0, 4), true},
    {"HTTP", "http:", Component(0, 4), true},
    {" HTTP ", "%20http%20:", Component(0, 10), false},
    {"htt: ", "htt%3A%20:", Component(0, 9), false},
    {"\xe4\xbd\xa0\xe5\xa5\xbdhttp", "%E4%BD%A0%E5%A5%BDhttp:", Component(0, 22), false},
      // Don't re-escape something already escaped. Note that it will
      // "canonicalize" the 'A' to 'a', but that's OK.
    {"ht%3Atp", "ht%3atp:", Component(0, 7), false},
    {"", ":", Component(0, 0), false},
  };

  std::string out_str;

  for (size_t i = 0; i < std::size(scheme_cases); i++) {
    int url_len = static_cast<int>(strlen(scheme_cases[i].input));
    Component in_comp(0, url_len);
    Component out_comp;

    out_str.clear();
    StdStringCanonOutput output1(&out_str);
    bool success = CanonicalizeScheme(scheme_cases[i].input, in_comp, &output1,
                                      &out_comp);
    output1.Complete();

    EXPECT_EQ(scheme_cases[i].expected_success, success);
    EXPECT_EQ(std::string(scheme_cases[i].expected), out_str);
    EXPECT_EQ(scheme_cases[i].expected_component.begin, out_comp.begin);
    EXPECT_EQ(scheme_cases[i].expected_component.len, out_comp.len);

    // Now try the wide version.
    out_str.clear();
    StdStringCanonOutput output2(&out_str);

    std::u16string wide_input(gurl_base::UTF8ToUTF16(scheme_cases[i].input));
    in_comp.len = static_cast<int>(wide_input.length());
    success = CanonicalizeScheme(wide_input.c_str(), in_comp, &output2,
                                 &out_comp);
    output2.Complete();

    EXPECT_EQ(scheme_cases[i].expected_success, success);
    EXPECT_EQ(std::string(scheme_cases[i].expected), out_str);
    EXPECT_EQ(scheme_cases[i].expected_component.begin, out_comp.begin);
    EXPECT_EQ(scheme_cases[i].expected_component.len, out_comp.len);
  }

  // Test the case where the scheme is declared nonexistent, it should be
  // converted into an empty scheme.
  Component out_comp;
  out_str.clear();
  StdStringCanonOutput output(&out_str);

  EXPECT_FALSE(CanonicalizeScheme("", Component(0, -1), &output, &out_comp));
  output.Complete();

  EXPECT_EQ(std::string(":"), out_str);
  EXPECT_EQ(0, out_comp.begin);
  EXPECT_EQ(0, out_comp.len);
}

TEST(URLCanonTest, Host) {
  IPAddressCase host_cases[] = {
       // Basic canonicalization, uppercase should be converted to lowercase.
    {"GoOgLe.CoM", L"GoOgLe.CoM", "google.com", Component(0, 10), CanonHostInfo::NEUTRAL, -1, ""},
      // Spaces and some other characters should be escaped.
    {"Goo%20 goo%7C|.com", L"Goo%20 goo%7C|.com", "goo%20%20goo%7C%7C.com", Component(0, 22), CanonHostInfo::NEUTRAL, -1, ""},
      // Exciting different types of spaces!
    {NULL, L"GOO\x00a0\x3000goo.com", "goo%20%20goo.com", Component(0, 16), CanonHostInfo::NEUTRAL, -1, ""},
      // Other types of space (no-break, zero-width, zero-width-no-break) are
      // name-prepped away to nothing.
    {NULL, L"GOO\x200b\x2060\xfeffgoo.com", "googoo.com", Component(0, 10), CanonHostInfo::NEUTRAL, -1, ""},
      // Ideographic full stop (full-width period for Chinese, etc.) should be
      // treated as a dot.
    {NULL, L"www.foo\x3002" L"bar.com", "www.foo.bar.com", Component(0, 15), CanonHostInfo::NEUTRAL, -1, ""},
      // Invalid unicode characters should fail...
      // ...In wide input, ICU will barf and we'll end up with the input as
      //    escaped UTF-8 (the invalid character should be replaced with the
      //    replacement character).
    {"\xef\xb7\x90zyx.com", L"\xfdd0zyx.com", "%EF%BF%BDzyx.com", Component(0, 16), CanonHostInfo::BROKEN, -1, ""},
      // ...This is the same as previous but with with escaped.
    {"%ef%b7%90zyx.com", L"%ef%b7%90zyx.com", "%EF%BF%BDzyx.com", Component(0, 16), CanonHostInfo::BROKEN, -1, ""},
      // Test name prepping, fullwidth input should be converted to ASCII and NOT
      // IDN-ized. This is "Go" in fullwidth UTF-8/UTF-16.
    {"\xef\xbc\xa7\xef\xbd\x8f.com", L"\xff27\xff4f.com", "go.com", Component(0, 6), CanonHostInfo::NEUTRAL, -1, ""},
      // Test that fullwidth escaped values are properly name-prepped,
      // then converted or rejected.
      // ...%41 in fullwidth = 'A' (also as escaped UTF-8 input)
    {"\xef\xbc\x85\xef\xbc\x94\xef\xbc\x91.com", L"\xff05\xff14\xff11.com", "a.com", Component(0, 5), CanonHostInfo::NEUTRAL, -1, ""},
    {"%ef%bc%85%ef%bc%94%ef%bc%91.com", L"%ef%bc%85%ef%bc%94%ef%bc%91.com", "a.com", Component(0, 5), CanonHostInfo::NEUTRAL, -1, ""},
      // ...%00 in fullwidth should fail (also as escaped UTF-8 input)
    {"\xef\xbc\x85\xef\xbc\x90\xef\xbc\x90.com", L"\xff05\xff10\xff10.com", "%00.com", Component(0, 7), CanonHostInfo::BROKEN, -1, ""},
    {"%ef%bc%85%ef%bc%90%ef%bc%90.com", L"%ef%bc%85%ef%bc%90%ef%bc%90.com", "%00.com", Component(0, 7), CanonHostInfo::BROKEN, -1, ""},
      // ICU will convert weird percents into ASCII percents, but not unescape
      // further. A weird percent is U+FE6A (EF B9 AA in UTF-8) which is a
      // "small percent". At this point we should be within our rights to mark
      // anything as invalid since the URL is corrupt or malicious. The code
      // happens to allow ASCII characters (%41 = "A" -> 'a') to be unescaped
      // and kept as valid, so we validate that behavior here, but this level
      // of fixing the input shouldn't be seen as required. "%81" is invalid.
    {"\xef\xb9\xaa" "41.com", L"\xfe6a" L"41.com", "a.com", Component(0, 5), CanonHostInfo::NEUTRAL, -1, ""},
    {"%ef%b9%aa" "41.com", L"\xfe6a" L"41.com", "a.com", Component(0, 5), CanonHostInfo::NEUTRAL, -1, ""},
    {"\xef\xb9\xaa" "81.com", L"\xfe6a" L"81.com", "%81.com", Component(0, 7), CanonHostInfo::BROKEN, -1, ""},
    {"%ef%b9%aa" "81.com", L"\xfe6a" L"81.com", "%81.com", Component(0, 7), CanonHostInfo::BROKEN, -1, ""},
      // Basic IDN support, UTF-8 and UTF-16 input should be converted to IDN
    {"\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xbd\xa0\xe5\xa5\xbd", L"\x4f60\x597d\x4f60\x597d", "xn--6qqa088eba", Component(0, 14), CanonHostInfo::NEUTRAL, -1, ""},
      // See http://unicode.org/cldr/utility/idna.jsp for other
      // examples/experiments and http://goo.gl/7yG11o
      // for the full list of characters handled differently by
      // IDNA 2003, UTS 46 (http://unicode.org/reports/tr46/ ) and IDNA 2008.

      // 4 Deviation characters are mapped/ignored in UTS 46 transitional
      // mechansm. UTS 46, table 4 row (g).
      // Sharp-s is mapped to 'ss' in UTS 46 and IDNA 2003.
      // Otherwise, it'd be "xn--fuball-cta.de".
    {"fu\xc3\x9f" "ball.de", L"fu\x00df" L"ball.de", "fussball.de",
      Component(0, 11), CanonHostInfo::NEUTRAL, -1, ""},
      // Final-sigma (U+03C3) is mapped to regular sigma (U+03C2).
      // Otherwise, it'd be "xn--wxaijb9b".
    {"\xcf\x83\xcf\x8c\xce\xbb\xce\xbf\xcf\x82", L"\x3c3\x3cc\x3bb\x3bf\x3c2",
      "xn--wxaikc6b", Component(0, 12),
      CanonHostInfo::NEUTRAL, -1, ""},
      // ZWNJ (U+200C) and ZWJ (U+200D) are mapped away in UTS 46 transitional
      // handling as well as in IDNA 2003.
    {"a\xe2\x80\x8c" "b\xe2\x80\x8d" "c", L"a\x200c" L"b\x200d" L"c", "abc",
      Component(0, 3), CanonHostInfo::NEUTRAL, -1, ""},
      // ZWJ between Devanagari characters is still mapped away in UTS 46
      // transitional handling. IDNA 2008 would give xn--11bo0mv54g.
    {"\xe0\xa4\x95\xe0\xa5\x8d\xe2\x80\x8d\xe0\xa4\x9c",
     L"\x915\x94d\x200d\x91c", "xn--11bo0m",
     Component(0, 10), CanonHostInfo::NEUTRAL, -1, ""},
      // Fullwidth exclamation mark is disallowed. UTS 46, table 4, row (b)
      // However, we do allow this at the moment because we don't use
      // STD3 rules and canonicalize full-width ASCII to ASCII.
    {"wow\xef\xbc\x81", L"wow\xff01", "wow%21",
      Component(0, 6), CanonHostInfo::NEUTRAL, -1, ""},
      // U+2132 (turned capital F) is disallowed. UTS 46, table 4, row (c)
      // Allowed in IDNA 2003, but the mapping changed after Unicode 3.2
    {"\xe2\x84\xb2oo", L"\x2132oo", "%E2%84%B2oo",
      Component(0, 11), CanonHostInfo::BROKEN, -1, ""},
      // U+2F868 (CJK Comp) is disallowed. UTS 46, table 4, row (d)
      // Allowed in IDNA 2003, but the mapping changed after Unicode 3.2
    {"\xf0\xaf\xa1\xa8\xe5\xa7\xbb.cn", L"\xd87e\xdc68\x59fb.cn",
      "%F0%AF%A1%A8%E5%A7%BB.cn",
      Component(0, 24), CanonHostInfo::BROKEN, -1, ""},
      // Maps uppercase letters to lower case letters. UTS 46 table 4 row (e)
    {"M\xc3\x9cNCHEN", L"M\xdcNCHEN", "xn--mnchen-3ya",
      Component(0, 14), CanonHostInfo::NEUTRAL, -1, ""},
      // An already-IDNA host is not modified.
    {"xn--mnchen-3ya", L"xn--mnchen-3ya", "xn--mnchen-3ya",
      Component(0, 14), CanonHostInfo::NEUTRAL, -1, ""},
      // Symbol/punctuations are allowed in IDNA 2003/UTS46.
      // Not allowed in IDNA 2008. UTS 46 table 4 row (f).
    {"\xe2\x99\xa5ny.us", L"\x2665ny.us", "xn--ny-s0x.us",
      Component(0, 13), CanonHostInfo::NEUTRAL, -1, ""},
      // U+11013 is new in Unicode 6.0 and is allowed. UTS 46 table 4, row (h)
      // We used to allow it because we passed through unassigned code points.
    {"\xf0\x91\x80\x93.com", L"\xd804\xdc13.com", "xn--n00d.com",
      Component(0, 12), CanonHostInfo::NEUTRAL, -1, ""},
      // U+0602 is disallowed in UTS46/IDNA 2008. UTS 46 table 4, row(i)
      // Used to be allowed in INDA 2003.
    {"\xd8\x82.eg", L"\x602.eg", "%D8%82.eg",
      Component(0, 9), CanonHostInfo::BROKEN, -1, ""},
      // U+20B7 is new in Unicode 5.2 (not a part of IDNA 2003 based
      // on Unicode 3.2). We did allow it in the past because we let unassigned
      // code point pass. We continue to allow it even though it's a
      // "punctuation and symbol" blocked in IDNA 2008.
      // UTS 46 table 4, row (j)
    {"\xe2\x82\xb7.com", L"\x20b7.com", "xn--wzg.com",
      Component(0, 11), CanonHostInfo::NEUTRAL, -1, ""},
      // Maps uppercase letters to lower case letters.
      // In IDNA 2003, it's allowed without case-folding
      // ( xn--bc-7cb.com ) because it's not defined in Unicode 3.2
      // (added in Unicode 4.1). UTS 46 table 4 row (k)
    {"bc\xc8\xba.com", L"bc\x23a.com", "xn--bc-is1a.com",
      Component(0, 15), CanonHostInfo::NEUTRAL, -1, ""},
      // Maps U+FF43 (Full Width Small Letter C) to 'c'.
    {"ab\xef\xbd\x83.xyz", L"ab\xff43.xyz", "abc.xyz",
      Component(0, 7), CanonHostInfo::NEUTRAL, -1, ""},
      // Maps U+1D68C (Math Monospace Small C) to 'c'.
      // U+1D68C = \xD835\xDE8C in UTF-16
    {"ab\xf0\x9d\x9a\x8c.xyz", L"ab\xd835\xde8c.xyz", "abc.xyz",
      Component(0, 7), CanonHostInfo::NEUTRAL, -1, ""},
      // BiDi check test
      // "Divehi" in Divehi (Thaana script) ends with BidiClass=NSM.
      // Disallowed in IDNA 2003 but now allowed in UTS 46/IDNA 2008.
    {"\xde\x8b\xde\xa8\xde\x88\xde\xac\xde\x80\xde\xa8",
     L"\x78b\x7a8\x788\x7ac\x780\x7a8", "xn--hqbpi0jcw",
     Component(0, 13), CanonHostInfo::NEUTRAL, -1, ""},
      // Disallowed in both IDNA 2003 and 2008 with BiDi check.
      // Labels starting with a RTL character cannot end with a LTR character.
    {"\xd8\xac\xd8\xa7\xd8\xb1xyz", L"\x62c\x627\x631xyz",
     "%D8%AC%D8%A7%D8%B1xyz", Component(0, 21),
     CanonHostInfo::BROKEN, -1, ""},
      // Labels starting with a RTL character can end with BC=EN (European
      // number). Disallowed in IDNA 2003 but now allowed.
    {"\xd8\xac\xd8\xa7\xd8\xb1" "2", L"\x62c\x627\x631" L"2",
     "xn--2-ymcov", Component(0, 11),
     CanonHostInfo::NEUTRAL, -1, ""},
      // Labels starting with a RTL character cannot have "L" characters
      // even if it ends with an BC=EN. Disallowed in both IDNA 2003/2008.
    {"\xd8\xac\xd8\xa7\xd8\xb1xy2", L"\x62c\x627\x631xy2",
     "%D8%AC%D8%A7%D8%B1xy2", Component(0, 21),
     CanonHostInfo::BROKEN, -1, ""},
      // Labels starting with a RTL character can end with BC=AN (Arabic number)
      // Disallowed in IDNA 2003, but now allowed.
    {"\xd8\xac\xd8\xa7\xd8\xb1\xd9\xa2", L"\x62c\x627\x631\x662",
     "xn--mgbjq0r", Component(0, 11),
     CanonHostInfo::NEUTRAL, -1, ""},
      // Labels starting with a RTL character cannot have "L" characters
      // even if it ends with an BC=AN (Arabic number).
      // Disallowed in both IDNA 2003/2008.
    {"\xd8\xac\xd8\xa7\xd8\xb1xy\xd9\xa2", L"\x62c\x627\x631xy\x662",
     "%D8%AC%D8%A7%D8%B1xy%D9%A2", Component(0, 26),
     CanonHostInfo::BROKEN, -1, ""},
      // Labels starting with a RTL character cannot mix BC=EN and BC=AN
    {"\xd8\xac\xd8\xa7\xd8\xb1xy2\xd9\xa2", L"\x62c\x627\x631xy2\x662",
     "%D8%AC%D8%A7%D8%B1xy2%D9%A2", Component(0, 27),
     CanonHostInfo::BROKEN, -1, ""},
      // As of Unicode 6.2, U+20CF is not assigned. We do not allow it.
    {"\xe2\x83\x8f.com", L"\x20cf.com", "%E2%83%8F.com",
      Component(0, 13), CanonHostInfo::BROKEN, -1, ""},
      // U+0080 is not allowed.
    {"\xc2\x80.com", L"\x80.com", "%C2%80.com",
      Component(0, 10), CanonHostInfo::BROKEN, -1, ""},
      // Mixed UTF-8 and escaped UTF-8 (narrow case) and UTF-16 and escaped
      // Mixed UTF-8 and escaped UTF-8 (narrow case) and UTF-16 and escaped
      // UTF-8 (wide case). The output should be equivalent to the true wide
      // character input above).
    {"%E4%BD%A0%E5%A5%BD\xe4\xbd\xa0\xe5\xa5\xbd",
      L"%E4%BD%A0%E5%A5%BD\x4f60\x597d", "xn--6qqa088eba",
      Component(0, 14), CanonHostInfo::NEUTRAL, -1, ""},
      // Invalid escaped characters should fail and the percents should be
      // escaped.
    {"%zz%66%a", L"%zz%66%a", "%25zzf%25a", Component(0, 10),
      CanonHostInfo::BROKEN, -1, ""},
      // If we get an invalid character that has been escaped.
    {"%25", L"%25", "%25", Component(0, 3),
      CanonHostInfo::BROKEN, -1, ""},
    {"hello%00", L"hello%00", "hello%00", Component(0, 8),
      CanonHostInfo::BROKEN, -1, ""},
      // Escaped numbers should be treated like IP addresses if they are.
    {"%30%78%63%30%2e%30%32%35%30.01", L"%30%78%63%30%2e%30%32%35%30.01",
      "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 3,
      "C0A80001"},
    {"%30%78%63%30%2e%30%32%35%30.01%2e", L"%30%78%63%30%2e%30%32%35%30.01%2e",
      "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 3,
      "C0A80001"},
      // Invalid escaping should trigger the regular host error handling.
    {"%3g%78%63%30%2e%30%32%35%30%2E.01", L"%3g%78%63%30%2e%30%32%35%30%2E.01", "%253gxc0.0250..01", Component(0, 17), CanonHostInfo::BROKEN, -1, ""},
      // Something that isn't exactly an IP should get treated as a host and
      // spaces escaped.
    {"192.168.0.1 hello", L"192.168.0.1 hello", "192.168.0.1%20hello", Component(0, 19), CanonHostInfo::NEUTRAL, -1, ""},
      // Fullwidth and escaped UTF-8 fullwidth should still be treated as IP.
      // These are "0Xc0.0250.01" in fullwidth.
    {"\xef\xbc\x90%Ef%bc\xb8%ef%Bd%83\xef\xbc\x90%EF%BC%8E\xef\xbc\x90\xef\xbc\x92\xef\xbc\x95\xef\xbc\x90\xef\xbc%8E\xef\xbc\x90\xef\xbc\x91", L"\xff10\xff38\xff43\xff10\xff0e\xff10\xff12\xff15\xff10\xff0e\xff10\xff11", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 3, "C0A80001"},
      // Broken IP addresses get marked as such.
    {"192.168.0.257", L"192.168.0.257", "192.168.0.257", Component(0, 13), CanonHostInfo::BROKEN, -1, ""},
    {"[google.com]", L"[google.com]", "[google.com]", Component(0, 12), CanonHostInfo::BROKEN, -1, ""},
      // Cyrillic letter followed by '(' should return punycode for '(' escaped
      // before punycode string was created. I.e.
      // if '(' is escaped after punycode is created we would get xn--%28-8tb
      // (incorrect).
    {"\xd1\x82(", L"\x0442(", "xn--%28-7ed", Component(0, 11),
      CanonHostInfo::NEUTRAL, -1, ""},
      // Address with all hexidecimal characters with leading number of 1<<32
      // or greater and should return NEUTRAL rather than BROKEN if not all
      // components are numbers.
    {"12345678912345.de", L"12345678912345.de", "12345678912345.de", Component(0, 17), CanonHostInfo::NEUTRAL, -1, ""},
    {"1.12345678912345.de", L"1.12345678912345.de", "1.12345678912345.de", Component(0, 19), CanonHostInfo::NEUTRAL, -1, ""},
    {"12345678912345.12345678912345.de", L"12345678912345.12345678912345.de", "12345678912345.12345678912345.de", Component(0, 32), CanonHostInfo::NEUTRAL, -1, ""},
    {"1.2.0xB3A73CE5B59.de", L"1.2.0xB3A73CE5B59.de", "1.2.0xb3a73ce5b59.de", Component(0, 20), CanonHostInfo::NEUTRAL, -1, ""},
    {"12345678912345.0xde", L"12345678912345.0xde", "12345678912345.0xde", Component(0, 19), CanonHostInfo::BROKEN, -1, ""},
    // A label that starts with "xn--" but contains non-ASCII characters should
    // be an error. Escape the invalid characters.
    {"xn--m\xc3\xbcnchen", L"xn--m\xfcnchen", "xn--m%C3%BCnchen", Component(0, 16), CanonHostInfo::BROKEN, -1, ""},
  };

  // CanonicalizeHost() non-verbose.
  std::string out_str;
  for (size_t i = 0; i < std::size(host_cases); i++) {
    // Narrow version.
    if (host_cases[i].input8) {
      int host_len = static_cast<int>(strlen(host_cases[i].input8));
      Component in_comp(0, host_len);
      Component out_comp;

      out_str.clear();
      StdStringCanonOutput output(&out_str);

      bool success = CanonicalizeHost(host_cases[i].input8, in_comp, &output,
                                      &out_comp);
      output.Complete();

      EXPECT_EQ(host_cases[i].expected_family != CanonHostInfo::BROKEN,
                success) << "for input: " << host_cases[i].input8;
      EXPECT_EQ(std::string(host_cases[i].expected), out_str) <<
                "for input: " << host_cases[i].input8;
      EXPECT_EQ(host_cases[i].expected_component.begin, out_comp.begin) <<
                "for input: " << host_cases[i].input8;
      EXPECT_EQ(host_cases[i].expected_component.len, out_comp.len) <<
                "for input: " << host_cases[i].input8;
    }

    // Wide version.
    if (host_cases[i].input16) {
      std::u16string input16(
          test_utils::TruncateWStringToUTF16(host_cases[i].input16));
      int host_len = static_cast<int>(input16.length());
      Component in_comp(0, host_len);
      Component out_comp;

      out_str.clear();
      StdStringCanonOutput output(&out_str);

      bool success = CanonicalizeHost(input16.c_str(), in_comp, &output,
                                      &out_comp);
      output.Complete();

      EXPECT_EQ(host_cases[i].expected_family != CanonHostInfo::BROKEN,
                success);
      EXPECT_EQ(std::string(host_cases[i].expected), out_str);
      EXPECT_EQ(host_cases[i].expected_component.begin, out_comp.begin);
      EXPECT_EQ(host_cases[i].expected_component.len, out_comp.len);
    }
  }

  // CanonicalizeHostVerbose()
  for (size_t i = 0; i < std::size(host_cases); i++) {
    // Narrow version.
    if (host_cases[i].input8) {
      int host_len = static_cast<int>(strlen(host_cases[i].input8));
      Component in_comp(0, host_len);

      out_str.clear();
      StdStringCanonOutput output(&out_str);
      CanonHostInfo host_info;

      CanonicalizeHostVerbose(host_cases[i].input8, in_comp, &output,
                              &host_info);
      output.Complete();

      EXPECT_EQ(host_cases[i].expected_family, host_info.family);
      EXPECT_EQ(std::string(host_cases[i].expected), out_str);
      EXPECT_EQ(host_cases[i].expected_component.begin,
                host_info.out_host.begin);
      EXPECT_EQ(host_cases[i].expected_component.len, host_info.out_host.len);
      EXPECT_EQ(std::string(host_cases[i].expected_address_hex),
                BytesToHexString(host_info.address, host_info.AddressLength()));
      if (host_cases[i].expected_family == CanonHostInfo::IPV4) {
        EXPECT_EQ(host_cases[i].expected_num_ipv4_components,
                  host_info.num_ipv4_components);
      }
    }

    // Wide version.
    if (host_cases[i].input16) {
      std::u16string input16(
          test_utils::TruncateWStringToUTF16(host_cases[i].input16));
      int host_len = static_cast<int>(input16.length());
      Component in_comp(0, host_len);

      out_str.clear();
      StdStringCanonOutput output(&out_str);
      CanonHostInfo host_info;

      CanonicalizeHostVerbose(input16.c_str(), in_comp, &output, &host_info);
      output.Complete();

      EXPECT_EQ(host_cases[i].expected_family, host_info.family);
      EXPECT_EQ(std::string(host_cases[i].expected), out_str);
      EXPECT_EQ(host_cases[i].expected_component.begin,
                host_info.out_host.begin);
      EXPECT_EQ(host_cases[i].expected_component.len, host_info.out_host.len);
      EXPECT_EQ(std::string(host_cases[i].expected_address_hex),
                BytesToHexString(host_info.address, host_info.AddressLength()));
      if (host_cases[i].expected_family == CanonHostInfo::IPV4) {
        EXPECT_EQ(host_cases[i].expected_num_ipv4_components,
                  host_info.num_ipv4_components);
      }
    }
  }
}

TEST(URLCanonTest, IPv4) {
  // clang-format off
  IPAddressCase cases[] = {
    // Empty is not an IP address.
    {"", L"", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {".", L".", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    // Regular IP addresses in different bases.
    {"192.168.0.1", L"192.168.0.1", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 4, "C0A80001"},
    {"0300.0250.00.01", L"0300.0250.00.01", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 4, "C0A80001"},
    {"0xC0.0Xa8.0x0.0x1", L"0xC0.0Xa8.0x0.0x1", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 4, "C0A80001"},
    // Non-IP addresses due to invalid characters.
    {"192.168.9.com", L"192.168.9.com", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    // Hostnames with a numeric final component but other components that don't
    // parse as numbers should be considered broken.
    {"19a.168.0.1", L"19a.168.0.1", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"19a.168.0.1.", L"19a.168.0.1.", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0308.0250.00.01", L"0308.0250.00.01", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0308.0250.00.01.", L"0308.0250.00.01.", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0xCG.0xA8.0x0.0x1", L"0xCG.0xA8.0x0.0x1", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0xCG.0xA8.0x0.0x1.", L"0xCG.0xA8.0x0.0x1.", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // Non-numeric terminal compeonent should be considered not IPv4 hostnames, but valid.
    {"19.168.0.1a", L"19.168.0.1a", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"0xC.0xA8.0x0.0x1G", L"0xC.0xA8.0x0.0x1G", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    // Hostnames that would be considered broken IPv4 hostnames should be considered valid non-IPv4 hostnames if they end with two dots instead of 0 or 1.
    {"19a.168.0.1..", L"19a.168.0.1..", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"0308.0250.00.01..", L"0308.0250.00.01..", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"0xCG.0xA8.0x0.0x1..", L"0xCG.0xA8.0x0.0x1..", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    // Hosts with components that aren't considered valid IPv4 numbers but are entirely numeric should be considered invalid.
    {"1.2.3.08", L"1.2.3.08", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"1.2.3.08.", L"1.2.3.08.", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // If there are not enough components, the last one should fill them out.
    {"192", L"192", "0.0.0.192", Component(0, 9), CanonHostInfo::IPV4, 1, "000000C0"},
    {"0xC0a80001", L"0xC0a80001", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 1, "C0A80001"},
    {"030052000001", L"030052000001", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 1, "C0A80001"},
    {"000030052000001", L"000030052000001", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 1, "C0A80001"},
    {"192.168", L"192.168", "192.0.0.168", Component(0, 11), CanonHostInfo::IPV4, 2, "C00000A8"},
    {"192.0x00A80001", L"192.0x000A80001", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 2, "C0A80001"},
    {"0xc0.052000001", L"0xc0.052000001", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 2, "C0A80001"},
    {"192.168.1", L"192.168.1", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 3, "C0A80001"},
    // Hostnames with too many components, but a numeric final numeric component are invalid.
    {"192.168.0.0.1", L"192.168.0.0.1", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // We allow a single trailing dot.
    {"192.168.0.1.", L"192.168.0.1.", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 4, "C0A80001"},
    {"192.168.0.1. hello", L"192.168.0.1. hello", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"192.168.0.1..", L"192.168.0.1..", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    // Hosts with two dots in a row with a final numeric component are considered invalid.
    {"192.168..1", L"192.168..1", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"192.168..1.", L"192.168..1.", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // Any numerical overflow should be marked as BROKEN.
    {"0x100.0", L"0x100.0", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0x100.0.0", L"0x100.0.0", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0x100.0.0.0", L"0x100.0.0.0", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0.0x100.0.0", L"0.0x100.0.0", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0.0.0x100.0", L"0.0.0x100.0", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0.0.0.0x100", L"0.0.0.0x100", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0.0.0x10000", L"0.0.0x10000", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0.0x1000000", L"0.0x1000000", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0x100000000", L"0x100000000", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // Repeat the previous tests, minus 1, to verify boundaries.
    {"0xFF.0", L"0xFF.0", "255.0.0.0", Component(0, 9), CanonHostInfo::IPV4, 2, "FF000000"},
    {"0xFF.0.0", L"0xFF.0.0", "255.0.0.0", Component(0, 9), CanonHostInfo::IPV4, 3, "FF000000"},
    {"0xFF.0.0.0", L"0xFF.0.0.0", "255.0.0.0", Component(0, 9), CanonHostInfo::IPV4, 4, "FF000000"},
    {"0.0xFF.0.0", L"0.0xFF.0.0", "0.255.0.0", Component(0, 9), CanonHostInfo::IPV4, 4, "00FF0000"},
    {"0.0.0xFF.0", L"0.0.0xFF.0", "0.0.255.0", Component(0, 9), CanonHostInfo::IPV4, 4, "0000FF00"},
    {"0.0.0.0xFF", L"0.0.0.0xFF", "0.0.0.255", Component(0, 9), CanonHostInfo::IPV4, 4, "000000FF"},
    {"0.0.0xFFFF", L"0.0.0xFFFF", "0.0.255.255", Component(0, 11), CanonHostInfo::IPV4, 3, "0000FFFF"},
    {"0.0xFFFFFF", L"0.0xFFFFFF", "0.255.255.255", Component(0, 13), CanonHostInfo::IPV4, 2, "00FFFFFF"},
    {"0xFFFFFFFF", L"0xFFFFFFFF", "255.255.255.255", Component(0, 15), CanonHostInfo::IPV4, 1, "FFFFFFFF"},
    // Old trunctations tests. They're all "BROKEN" now.
    {"276.256.0xf1a2.077777", L"276.256.0xf1a2.077777", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"192.168.0.257", L"192.168.0.257", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"192.168.0xa20001", L"192.168.0xa20001", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"192.015052000001", L"192.015052000001", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0X12C0a80001", L"0X12C0a80001", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"276.1.2", L"276.1.2", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // Too many components should be rejected, in valid ranges or not.
    {"255.255.255.255.255", L"255.255.255.255.255", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"256.256.256.256.256", L"256.256.256.256.256", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // Spaces should be rejected.
    {"192.168.0.1 hello", L"192.168.0.1 hello", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    // Very large numbers.
    {"0000000000000300.0x00000000000000fF.00000000000000001", L"0000000000000300.0x00000000000000fF.00000000000000001", "192.255.0.1", Component(0, 11), CanonHostInfo::IPV4, 3, "C0FF0001"},
    {"0000000000000300.0xffffffffFFFFFFFF.3022415481470977", L"0000000000000300.0xffffffffFFFFFFFF.3022415481470977", "", Component(0, 11), CanonHostInfo::BROKEN, -1, ""},
    // A number has no length limit, but long numbers can still overflow.
    {"00000000000000000001", L"00000000000000000001", "0.0.0.1", Component(0, 7), CanonHostInfo::IPV4, 1, "00000001"},
    {"0000000000000000100000000000000001", L"0000000000000000100000000000000001", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // If a long component is non-numeric, it's a hostname, *not* a broken IP.
    {"0.0.0.000000000000000000z", L"0.0.0.000000000000000000z", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"0.0.0.100000000000000000z", L"0.0.0.100000000000000000z", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    // Truncation of all zeros should still result in 0.
    {"0.00.0x.0x0", L"0.00.0x.0x0", "0.0.0.0", Component(0, 7), CanonHostInfo::IPV4, 4, "00000000"},
    // Non-ASCII characters in final component should return NEUTRAL.
    {"1.2.3.\xF0\x9F\x92\xA9", L"1.2.3.\xD83D\xDCA9", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"1.2.3.4\xF0\x9F\x92\xA9", L"1.2.3.4\xD83D\xDCA9", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"1.2.3.0x\xF0\x9F\x92\xA9", L"1.2.3.0x\xD83D\xDCA9", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"1.2.3.0\xF0\x9F\x92\xA9", L"1.2.3.0\xD83D\xDCA9", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    // Non-ASCII characters in other components should result in broken IPs when final component is numeric.
    {"1.2.\xF0\x9F\x92\xA9.4", L"1.2.\xD83D\xDCA9.4", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"1.2.3\xF0\x9F\x92\xA9.4", L"1.2.3\xD83D\xDCA9.4", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"1.2.0x\xF0\x9F\x92\xA9.4", L"1.2.0x\xD83D\xDCA9.4", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"1.2.0\xF0\x9F\x92\xA9.4", L"1.2.0\xD83D\xDCA9.4", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"\xF0\x9F\x92\xA9.2.3.4", L"\xD83D\xDCA9.2.3.4", "", Component(), CanonHostInfo::BROKEN, -1, ""},
  };
  // clang-format on

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.input8);

    // 8-bit version.
    Component component(0, static_cast<int>(strlen(test_case.input8)));

    std::string out_str1;
    StdStringCanonOutput output1(&out_str1);
    CanonHostInfo host_info;
    CanonicalizeIPAddress(test_case.input8, component, &output1, &host_info);
    output1.Complete();

    EXPECT_EQ(test_case.expected_family, host_info.family);
    EXPECT_EQ(std::string(test_case.expected_address_hex),
              BytesToHexString(host_info.address, host_info.AddressLength()));
    if (host_info.family == CanonHostInfo::IPV4) {
      EXPECT_STREQ(test_case.expected, out_str1.c_str());
      EXPECT_EQ(test_case.expected_component.begin, host_info.out_host.begin);
      EXPECT_EQ(test_case.expected_component.len, host_info.out_host.len);
      EXPECT_EQ(test_case.expected_num_ipv4_components,
                host_info.num_ipv4_components);
    }

    // 16-bit version.
    std::u16string input16(
        test_utils::TruncateWStringToUTF16(test_case.input16));
    component = Component(0, static_cast<int>(input16.length()));

    std::string out_str2;
    StdStringCanonOutput output2(&out_str2);
    CanonicalizeIPAddress(input16.c_str(), component, &output2, &host_info);
    output2.Complete();

    EXPECT_EQ(test_case.expected_family, host_info.family);
    EXPECT_EQ(std::string(test_case.expected_address_hex),
              BytesToHexString(host_info.address, host_info.AddressLength()));
    if (host_info.family == CanonHostInfo::IPV4) {
      EXPECT_STREQ(test_case.expected, out_str2.c_str());
      EXPECT_EQ(test_case.expected_component.begin, host_info.out_host.begin);
      EXPECT_EQ(test_case.expected_component.len, host_info.out_host.len);
      EXPECT_EQ(test_case.expected_num_ipv4_components,
                host_info.num_ipv4_components);
    }
  }
}

TEST(URLCanonTest, IPv6) {
  IPAddressCase cases[] = {
      // Empty is not an IP address.
    {"", L"", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
      // Non-IPs with [:] characters are marked BROKEN.
    {":", L":", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[", L"[", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[:", L"[:", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"]", L"]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {":]", L":]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[]", L"[]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[:]", L"[:]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
      // Regular IP address is invalid without bounding '[' and ']'.
    {"2001:db8::1", L"2001:db8::1", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[2001:db8::1", L"[2001:db8::1", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"2001:db8::1]", L"2001:db8::1]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
      // Regular IP addresses.
    {"[::]", L"[::]", "[::]", Component(0,4), CanonHostInfo::IPV6, -1, "00000000000000000000000000000000"},
    {"[::1]", L"[::1]", "[::1]", Component(0,5), CanonHostInfo::IPV6, -1, "00000000000000000000000000000001"},
    {"[1::]", L"[1::]", "[1::]", Component(0,5), CanonHostInfo::IPV6, -1, "00010000000000000000000000000000"},

    // Leading zeros should be stripped.
    {"[000:01:02:003:004:5:6:007]", L"[000:01:02:003:004:5:6:007]", "[0:1:2:3:4:5:6:7]", Component(0,17), CanonHostInfo::IPV6, -1, "00000001000200030004000500060007"},

    // Upper case letters should be lowercased.
    {"[A:b:c:DE:fF:0:1:aC]", L"[A:b:c:DE:fF:0:1:aC]", "[a:b:c:de:ff:0:1:ac]", Component(0,20), CanonHostInfo::IPV6, -1, "000A000B000C00DE00FF0000000100AC"},

    // The same address can be written with different contractions, but should
    // get canonicalized to the same thing.
    {"[1:0:0:2::3:0]", L"[1:0:0:2::3:0]", "[1::2:0:0:3:0]", Component(0,14), CanonHostInfo::IPV6, -1, "00010000000000020000000000030000"},
    {"[1::2:0:0:3:0]", L"[1::2:0:0:3:0]", "[1::2:0:0:3:0]", Component(0,14), CanonHostInfo::IPV6, -1, "00010000000000020000000000030000"},

    // Addresses with embedded IPv4.
    {"[::192.168.0.1]", L"[::192.168.0.1]", "[::c0a8:1]", Component(0,10), CanonHostInfo::IPV6, -1, "000000000000000000000000C0A80001"},
    {"[::ffff:192.168.0.1]", L"[::ffff:192.168.0.1]", "[::ffff:c0a8:1]", Component(0,15), CanonHostInfo::IPV6, -1, "00000000000000000000FFFFC0A80001"},
    {"[::eeee:192.168.0.1]", L"[::eeee:192.168.0.1]", "[::eeee:c0a8:1]", Component(0, 15), CanonHostInfo::IPV6, -1, "00000000000000000000EEEEC0A80001"},
    {"[2001::192.168.0.1]", L"[2001::192.168.0.1]", "[2001::c0a8:1]", Component(0, 14), CanonHostInfo::IPV6, -1, "200100000000000000000000C0A80001"},
    {"[1:2:192.168.0.1:5:6]", L"[1:2:192.168.0.1:5:6]", "", Component(), CanonHostInfo::BROKEN, -1, ""},

    // IPv4 with last component missing.
    {"[::ffff:192.1.2]", L"[::ffff:192.1.2]", "[::ffff:c001:2]", Component(0,15), CanonHostInfo::IPV6, -1, "00000000000000000000FFFFC0010002"},

    // IPv4 using hex.
    // TODO(eroman): Should this format be disallowed?
    {"[::ffff:0xC0.0Xa8.0x0.0x1]", L"[::ffff:0xC0.0Xa8.0x0.0x1]", "[::ffff:c0a8:1]", Component(0,15), CanonHostInfo::IPV6, -1, "00000000000000000000FFFFC0A80001"},

    // There may be zeros surrounding the "::" contraction.
    {"[0:0::0:0:8]", L"[0:0::0:0:8]", "[::8]", Component(0,5), CanonHostInfo::IPV6, -1, "00000000000000000000000000000008"},

    {"[2001:db8::1]", L"[2001:db8::1]", "[2001:db8::1]", Component(0,13), CanonHostInfo::IPV6, -1, "20010DB8000000000000000000000001"},

    // Can only have one "::" contraction in an IPv6 string literal.
    {"[2001::db8::1]", L"[2001::db8::1]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // No more than 2 consecutive ':'s.
    {"[2001:db8:::1]", L"[2001:db8:::1]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[:::]", L"[:::]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // Non-IP addresses due to invalid characters.
    {"[2001::.com]", L"[2001::.com]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // If there are not enough components, the last one should fill them out.
    // ... omitted at this time ...
    // Too many components means not an IP address. Similarly, with too few
    // if using IPv4 compat or mapped addresses.
    {"[::192.168.0.0.1]", L"[::192.168.0.0.1]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[::ffff:192.168.0.0.1]", L"[::ffff:192.168.0.0.1]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[1:2:3:4:5:6:7:8:9]", L"[1:2:3:4:5:6:7:8:9]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // Too many bits (even though 8 comonents, the last one holds 32 bits).
    {"[0:0:0:0:0:0:0:192.168.0.1]", L"[0:0:0:0:0:0:0:192.168.0.1]", "", Component(), CanonHostInfo::BROKEN, -1, ""},

    // Too many bits specified -- the contraction would have to be zero-length
    // to not exceed 128 bits.
    {"[1:2:3:4:5:6::192.168.0.1]", L"[1:2:3:4:5:6::192.168.0.1]", "", Component(), CanonHostInfo::BROKEN, -1, ""},

    // The contraction is for 16 bits of zero.
    {"[1:2:3:4:5:6::8]", L"[1:2:3:4:5:6::8]", "[1:2:3:4:5:6:0:8]", Component(0,17), CanonHostInfo::IPV6, -1, "00010002000300040005000600000008"},

    // Cannot have a trailing colon.
    {"[1:2:3:4:5:6:7:8:]", L"[1:2:3:4:5:6:7:8:]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[1:2:3:4:5:6:192.168.0.1:]", L"[1:2:3:4:5:6:192.168.0.1:]", "", Component(), CanonHostInfo::BROKEN, -1, ""},

    // Cannot have negative numbers.
    {"[-1:2:3:4:5:6:7:8]", L"[-1:2:3:4:5:6:7:8]", "", Component(), CanonHostInfo::BROKEN, -1, ""},

    // Scope ID -- the URL may contain an optional ["%" <scope_id>] section.
    // The scope_id should be included in the canonicalized URL, and is an
    // unsigned decimal number.

    // Invalid because no ID was given after the percent.

    // Don't allow scope-id
    {"[1::%1]", L"[1::%1]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[1::%eth0]", L"[1::%eth0]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[1::%]", L"[1::%]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[%]", L"[%]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[::%:]", L"[::%:]", "", Component(), CanonHostInfo::BROKEN, -1, ""},

    // Don't allow leading or trailing colons.
    {"[:0:0::0:0:8]", L"[:0:0::0:0:8]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[0:0::0:0:8:]", L"[0:0::0:0:8:]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[:0:0::0:0:8:]", L"[:0:0::0:0:8:]", "", Component(), CanonHostInfo::BROKEN, -1, ""},

      // We allow a single trailing dot.
    // ... omitted at this time ...
      // Two dots in a row means not an IP address.
    {"[::192.168..1]", L"[::192.168..1]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
      // Any non-first components get truncated to one byte.
    // ... omitted at this time ...
      // Spaces should be rejected.
    {"[::1 hello]", L"[::1 hello]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
  };

  for (size_t i = 0; i < std::size(cases); i++) {
    // 8-bit version.
    Component component(0, static_cast<int>(strlen(cases[i].input8)));

    std::string out_str1;
    StdStringCanonOutput output1(&out_str1);
    CanonHostInfo host_info;
    CanonicalizeIPAddress(cases[i].input8, component, &output1, &host_info);
    output1.Complete();

    EXPECT_EQ(cases[i].expected_family, host_info.family);
    EXPECT_EQ(std::string(cases[i].expected_address_hex),
              BytesToHexString(host_info.address, host_info.AddressLength())) << "iter " << i << " host " << cases[i].input8;
    if (host_info.family == CanonHostInfo::IPV6) {
      EXPECT_STREQ(cases[i].expected, out_str1.c_str());
      EXPECT_EQ(cases[i].expected_component.begin,
                host_info.out_host.begin);
      EXPECT_EQ(cases[i].expected_component.len, host_info.out_host.len);
    }

    // 16-bit version.
    std::u16string input16(
        test_utils::TruncateWStringToUTF16(cases[i].input16));
    component = Component(0, static_cast<int>(input16.length()));

    std::string out_str2;
    StdStringCanonOutput output2(&out_str2);
    CanonicalizeIPAddress(input16.c_str(), component, &output2, &host_info);
    output2.Complete();

    EXPECT_EQ(cases[i].expected_family, host_info.family);
    EXPECT_EQ(std::string(cases[i].expected_address_hex),
              BytesToHexString(host_info.address, host_info.AddressLength()));
    if (host_info.family == CanonHostInfo::IPV6) {
      EXPECT_STREQ(cases[i].expected, out_str2.c_str());
      EXPECT_EQ(cases[i].expected_component.begin, host_info.out_host.begin);
      EXPECT_EQ(cases[i].expected_component.len, host_info.out_host.len);
    }
  }
}

TEST(URLCanonTest, IPEmpty) {
  std::string out_str1;
  StdStringCanonOutput output1(&out_str1);
  CanonHostInfo host_info;

  // This tests tests.
  const char spec[] = "192.168.0.1";
  CanonicalizeIPAddress(spec, Component(), &output1, &host_info);
  EXPECT_FALSE(host_info.IsIPAddress());

  CanonicalizeIPAddress(spec, Component(0, 0), &output1, &host_info);
  EXPECT_FALSE(host_info.IsIPAddress());
}

// Verifies that CanonicalizeHostSubstring produces the expected output and
// does not "fix" IP addresses. Because this code is a subset of
// CanonicalizeHost, the shared functionality is not tested.
TEST(URLCanonTest, CanonicalizeHostSubstring) {
  // Basic sanity check.
  {
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    EXPECT_TRUE(CanonicalizeHostSubstring("M\xc3\x9cNCHEN.com",
                                          Component(0, 12), &output));
    output.Complete();
    EXPECT_EQ("xn--mnchen-3ya.com", out_str);
  }

  // Failure case.
  {
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    EXPECT_FALSE(CanonicalizeHostSubstring(
        test_utils::TruncateWStringToUTF16(L"\xfdd0zyx.com").c_str(),
        Component(0, 8), &output));
    output.Complete();
    EXPECT_EQ("%EF%BF%BDzyx.com", out_str);
  }

  // Should return true for empty input strings.
  {
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    EXPECT_TRUE(CanonicalizeHostSubstring("", Component(0, 0), &output));
    output.Complete();
    EXPECT_EQ(std::string(), out_str);
  }

  // Numbers that look like IP addresses should not be changed.
  {
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    EXPECT_TRUE(
        CanonicalizeHostSubstring("01.02.03.04", Component(0, 11), &output));
    output.Complete();
    EXPECT_EQ("01.02.03.04", out_str);
  }
}

TEST(URLCanonTest, UserInfo) {
  // Note that the canonicalizer should escape and treat empty components as
  // not being there.

  // We actually parse a full input URL so we can get the initial components.
  struct UserComponentCase {
    const char* input;
    const char* expected;
    Component expected_username;
    Component expected_password;
    bool expected_success;
  } user_info_cases[] = {
    {"http://user:pass@host.com/", "user:pass@", Component(0, 4), Component(5, 4), true},
    {"http://@host.com/", "", Component(0, -1), Component(0, -1), true},
    {"http://:@host.com/", "", Component(0, -1), Component(0, -1), true},
    {"http://foo:@host.com/", "foo@", Component(0, 3), Component(0, -1), true},
    {"http://:foo@host.com/", ":foo@", Component(0, 0), Component(1, 3), true},
    {"http://^ :$\t@host.com/", "%5E%20:$%09@", Component(0, 6), Component(7, 4), true},
    {"http://user:pass@/", "user:pass@", Component(0, 4), Component(5, 4), true},
    {"http://%2540:bar@domain.com/", "%2540:bar@", Component(0, 5), Component(6, 3), true },

      // IE7 compatibility: old versions allowed backslashes in usernames, but
      // IE7 does not. We disallow it as well.
    {"ftp://me\\mydomain:pass@foo.com/", "", Component(0, -1), Component(0, -1), true},
  };

  for (size_t i = 0; i < std::size(user_info_cases); i++) {
    int url_len = static_cast<int>(strlen(user_info_cases[i].input));
    Parsed parsed;
    ParseStandardURL(user_info_cases[i].input, url_len, &parsed);
    Component out_user, out_pass;
    std::string out_str;
    StdStringCanonOutput output1(&out_str);

    bool success = CanonicalizeUserInfo(user_info_cases[i].input,
                                        parsed.username,
                                        user_info_cases[i].input,
                                        parsed.password,
                                        &output1,
                                        &out_user,
                                        &out_pass);
    output1.Complete();

    EXPECT_EQ(user_info_cases[i].expected_success, success);
    EXPECT_EQ(std::string(user_info_cases[i].expected), out_str);
    EXPECT_EQ(user_info_cases[i].expected_username.begin, out_user.begin);
    EXPECT_EQ(user_info_cases[i].expected_username.len, out_user.len);
    EXPECT_EQ(user_info_cases[i].expected_password.begin, out_pass.begin);
    EXPECT_EQ(user_info_cases[i].expected_password.len, out_pass.len);

    // Now try the wide version
    out_str.clear();
    StdStringCanonOutput output2(&out_str);
    std::u16string wide_input(gurl_base::UTF8ToUTF16(user_info_cases[i].input));
    success = CanonicalizeUserInfo(wide_input.c_str(),
                                   parsed.username,
                                   wide_input.c_str(),
                                   parsed.password,
                                   &output2,
                                   &out_user,
                                   &out_pass);
    output2.Complete();

    EXPECT_EQ(user_info_cases[i].expected_success, success);
    EXPECT_EQ(std::string(user_info_cases[i].expected), out_str);
    EXPECT_EQ(user_info_cases[i].expected_username.begin, out_user.begin);
    EXPECT_EQ(user_info_cases[i].expected_username.len, out_user.len);
    EXPECT_EQ(user_info_cases[i].expected_password.begin, out_pass.begin);
    EXPECT_EQ(user_info_cases[i].expected_password.len, out_pass.len);
  }
}

TEST(URLCanonTest, Port) {
  // We only need to test that the number gets properly put into the output
  // buffer. The parser unit tests will test scanning the number correctly.
  //
  // Note that the CanonicalizePort will always prepend a colon to the output
  // to separate it from the colon that it assumes precedes it.
  struct PortCase {
    const char* input;
    int default_port;
    const char* expected;
    Component expected_component;
    bool expected_success;
  } port_cases[] = {
      // Invalid input should be copied w/ failure.
    {"as df", 80, ":as%20df", Component(1, 7), false},
    {"-2", 80, ":-2", Component(1, 2), false},
      // Default port should be omitted.
    {"80", 80, "", Component(0, -1), true},
    {"8080", 80, ":8080", Component(1, 4), true},
      // PORT_UNSPECIFIED should mean always keep the port.
    {"80", PORT_UNSPECIFIED, ":80", Component(1, 2), true},
  };

  for (size_t i = 0; i < std::size(port_cases); i++) {
    int url_len = static_cast<int>(strlen(port_cases[i].input));
    Component in_comp(0, url_len);
    Component out_comp;
    std::string out_str;
    StdStringCanonOutput output1(&out_str);
    bool success = CanonicalizePort(port_cases[i].input,
                                    in_comp,
                                    port_cases[i].default_port,
                                    &output1,
                                    &out_comp);
    output1.Complete();

    EXPECT_EQ(port_cases[i].expected_success, success);
    EXPECT_EQ(std::string(port_cases[i].expected), out_str);
    EXPECT_EQ(port_cases[i].expected_component.begin, out_comp.begin);
    EXPECT_EQ(port_cases[i].expected_component.len, out_comp.len);

    // Now try the wide version
    out_str.clear();
    StdStringCanonOutput output2(&out_str);
    std::u16string wide_input(gurl_base::UTF8ToUTF16(port_cases[i].input));
    success = CanonicalizePort(wide_input.c_str(),
                               in_comp,
                               port_cases[i].default_port,
                               &output2,
                               &out_comp);
    output2.Complete();

    EXPECT_EQ(port_cases[i].expected_success, success);
    EXPECT_EQ(std::string(port_cases[i].expected), out_str);
    EXPECT_EQ(port_cases[i].expected_component.begin, out_comp.begin);
    EXPECT_EQ(port_cases[i].expected_component.len, out_comp.len);
  }
}

DualComponentCase kCommonPathCases[] = {
    // ----- path collapsing tests -----
    {"/././foo", L"/././foo", "/foo", Component(0, 4), true},
    {"/./.foo", L"/./.foo", "/.foo", Component(0, 5), true},
    {"/foo/.", L"/foo/.", "/foo/", Component(0, 5), true},
    {"/foo/./", L"/foo/./", "/foo/", Component(0, 5), true},
    // double dots followed by a slash or the end of the string count
    {"/foo/bar/..", L"/foo/bar/..", "/foo/", Component(0, 5), true},
    {"/foo/bar/../", L"/foo/bar/../", "/foo/", Component(0, 5), true},
    // don't count double dots when they aren't followed by a slash
    {"/foo/..bar", L"/foo/..bar", "/foo/..bar", Component(0, 10), true},
    // some in the middle
    {"/foo/bar/../ton", L"/foo/bar/../ton", "/foo/ton", Component(0, 8), true},
    {"/foo/bar/../ton/../../a", L"/foo/bar/../ton/../../a", "/a",
     Component(0, 2), true},
    // we should not be able to go above the root
    {"/foo/../../..", L"/foo/../../..", "/", Component(0, 1), true},
    {"/foo/../../../ton", L"/foo/../../../ton", "/ton", Component(0, 4), true},
    // escaped dots should be unescaped and treated the same as dots
    {"/foo/%2e", L"/foo/%2e", "/foo/", Component(0, 5), true},
    {"/foo/%2e%2", L"/foo/%2e%2", "/foo/.%2", Component(0, 8), true},
    {"/foo/%2e./%2e%2e/.%2e/%2e.bar", L"/foo/%2e./%2e%2e/.%2e/%2e.bar",
     "/..bar", Component(0, 6), true},
    // Multiple slashes in a row should be preserved and treated like empty
    // directory names.
    {"////../..", L"////../..", "//", Component(0, 2), true},

    // ----- escaping tests -----
    {"/foo", L"/foo", "/foo", Component(0, 4), true},
    // Valid escape sequence
    {"/%20foo", L"/%20foo", "/%20foo", Component(0, 7), true},
    // Invalid escape sequence we should pass through unchanged.
    {"/foo%", L"/foo%", "/foo%", Component(0, 5), true},
    {"/foo%2", L"/foo%2", "/foo%2", Component(0, 6), true},
    // Invalid escape sequence: bad characters should be treated the same as
    // the surrounding text, not as escaped (in this case, UTF-8).
    {"/foo%2zbar", L"/foo%2zbar", "/foo%2zbar", Component(0, 10), true},
    {"/foo%2\xc2\xa9zbar", nullptr, "/foo%2%C2%A9zbar", Component(0, 16), true},
    {nullptr, L"/foo%2\xc2\xa9zbar", "/foo%2%C3%82%C2%A9zbar", Component(0, 22),
     true},
    // Regular characters that are escaped should be unescaped
    {"/foo%41%7a", L"/foo%41%7a", "/fooAz", Component(0, 6), true},
    // Funny characters that are unescaped should be escaped
    {"/foo\x09\x91%91", nullptr, "/foo%09%91%91", Component(0, 13), true},
    {nullptr, L"/foo\x09\x91%91", "/foo%09%C2%91%91", Component(0, 16), true},
    // Invalid characters that are escaped should cause a failure.
    {"/foo%00%51", L"/foo%00%51", "/foo%00Q", Component(0, 8), false},
    // Some characters should be passed through unchanged regardless of esc.
    {"/(%28:%3A%29)", L"/(%28:%3A%29)", "/(%28:%3A%29)", Component(0, 13),
     true},
    // Characters that are properly escaped should not have the case changed
    // of hex letters.
    {"/%3A%3a%3C%3c", L"/%3A%3a%3C%3c", "/%3A%3a%3C%3c", Component(0, 13),
     true},
    // Funny characters that are unescaped should be escaped
    {"/foo\tbar", L"/foo\tbar", "/foo%09bar", Component(0, 10), true},
    // Backslashes should get converted to forward slashes
    {"\\foo\\bar", L"\\foo\\bar", "/foo/bar", Component(0, 8), true},
    // Hashes found in paths (possibly only when the caller explicitly sets
    // the path on an already-parsed URL) should be escaped.
    {"/foo#bar", L"/foo#bar", "/foo%23bar", Component(0, 10), true},
    // %7f should be allowed and %3D should not be unescaped (these were wrong
    // in a previous version).
    {"/%7Ffp3%3Eju%3Dduvgw%3Dd", L"/%7Ffp3%3Eju%3Dduvgw%3Dd",
     "/%7Ffp3%3Eju%3Dduvgw%3Dd", Component(0, 24), true},
    // @ should be passed through unchanged (escaped or unescaped).
    {"/@asdf%40", L"/@asdf%40", "/@asdf%40", Component(0, 9), true},
    // Nested escape sequences should result in escaping the leading '%' if
    // unescaping would result in a new escape sequence.
    {"/%A%42", L"/%A%42", "/%25AB", Component(0, 6), true},
    {"/%%41B", L"/%%41B", "/%25AB", Component(0, 6), true},
    {"/%%41%42", L"/%%41%42", "/%25AB", Component(0, 6), true},
    // Make sure truncated "nested" escapes don't result in reading off the
    // string end.
    {"/%%41", L"/%%41", "/%A", Component(0, 3), true},
    // Don't unescape the leading '%' if unescaping doesn't result in a valid
    // new escape sequence.
    {"/%%470", L"/%%470", "/%G0", Component(0, 4), true},
    {"/%%2D%41", L"/%%2D%41", "/%-A", Component(0, 4), true},
    // Don't erroneously downcast a UTF-16 character in a way that makes it
    // look like part of an escape sequence.
    {nullptr, L"/%%41\x0130", "/%A%C4%B0", Component(0, 9), true},

    // ----- encoding tests -----
    // Basic conversions
    {"/\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xbd\xa0\xe5\xa5\xbd",
     L"/\x4f60\x597d\x4f60\x597d", "/%E4%BD%A0%E5%A5%BD%E4%BD%A0%E5%A5%BD",
     Component(0, 37), true},
    // Invalid unicode characters should fail. We only do validation on
    // UTF-16 input, so this doesn't happen on 8-bit.
    {"/\xef\xb7\x90zyx", nullptr, "/%EF%B7%90zyx", Component(0, 13), true},
    {nullptr, L"/\xfdd0zyx", "/%EF%BF%BDzyx", Component(0, 13), false},
};

typedef bool (*CanonFunc8Bit)(const char*,
                              const Component&,
                              CanonOutput*,
                              Component*);
typedef bool (*CanonFunc16Bit)(const char16_t*,
                               const Component&,
                               CanonOutput*,
                               Component*);

void DoPathTest(const DualComponentCase* path_cases,
                size_t num_cases,
                CanonFunc8Bit canon_func_8,
                CanonFunc16Bit canon_func_16) {
  for (size_t i = 0; i < num_cases; i++) {
    testing::Message scope_message;
    scope_message << path_cases[i].input8 << "," << path_cases[i].input16;
    SCOPED_TRACE(scope_message);
    if (path_cases[i].input8) {
      int len = static_cast<int>(strlen(path_cases[i].input8));
      Component in_comp(0, len);
      Component out_comp;
      std::string out_str;
      StdStringCanonOutput output(&out_str);
      bool success =
          canon_func_8(path_cases[i].input8, in_comp, &output, &out_comp);
      output.Complete();

      EXPECT_EQ(path_cases[i].expected_success, success);
      EXPECT_EQ(path_cases[i].expected_component.begin, out_comp.begin);
      EXPECT_EQ(path_cases[i].expected_component.len, out_comp.len);
      EXPECT_EQ(path_cases[i].expected, out_str);
    }

    if (path_cases[i].input16) {
      std::u16string input16(
          test_utils::TruncateWStringToUTF16(path_cases[i].input16));
      int len = static_cast<int>(input16.length());
      Component in_comp(0, len);
      Component out_comp;
      std::string out_str;
      StdStringCanonOutput output(&out_str);

      bool success =
          canon_func_16(input16.c_str(), in_comp, &output, &out_comp);
      output.Complete();

      EXPECT_EQ(path_cases[i].expected_success, success);
      EXPECT_EQ(path_cases[i].expected_component.begin, out_comp.begin);
      EXPECT_EQ(path_cases[i].expected_component.len, out_comp.len);
      EXPECT_EQ(path_cases[i].expected, out_str);
    }
  }
}

TEST(URLCanonTest, Path) {
  DoPathTest(kCommonPathCases, std::size(kCommonPathCases), CanonicalizePath,
             CanonicalizePath);

  // Manual test: embedded NULLs should be escaped and the URL should be marked
  // as invalid.
  const char path_with_null[] = "/ab\0c";
  Component in_comp(0, 5);
  Component out_comp;

  std::string out_str;
  StdStringCanonOutput output(&out_str);
  bool success = CanonicalizePath(path_with_null, in_comp, &output, &out_comp);
  output.Complete();
  EXPECT_FALSE(success);
  EXPECT_EQ("/ab%00c", out_str);
}

TEST(URLCanonTest, PartialPath) {
  DualComponentCase partial_path_cases[] = {
      {".html", L".html", ".html", Component(0, 5), true},
      {"", L"", "", Component(0, 0), true},
  };

  DoPathTest(kCommonPathCases, std::size(kCommonPathCases),
             CanonicalizePartialPath, CanonicalizePartialPath);
  DoPathTest(partial_path_cases, std::size(partial_path_cases),
             CanonicalizePartialPath, CanonicalizePartialPath);
}

TEST(URLCanonTest, Query) {
  struct QueryCase {
    const char* input8;
    const wchar_t* input16;
    const char* expected;
  } query_cases[] = {
      // Regular ASCII case.
    {"foo=bar", L"foo=bar", "?foo=bar"},
      // Allow question marks in the query without escaping
    {"as?df", L"as?df", "?as?df"},
      // Always escape '#' since it would mark the ref.
    {"as#df", L"as#df", "?as%23df"},
      // Escape some questionable 8-bit characters, but never unescape.
    {"\x02hello\x7f bye", L"\x02hello\x7f bye", "?%02hello%7F%20bye"},
    {"%40%41123", L"%40%41123", "?%40%41123"},
      // Chinese input/output
    {"q=\xe4\xbd\xa0\xe5\xa5\xbd", L"q=\x4f60\x597d", "?q=%E4%BD%A0%E5%A5%BD"},
      // Invalid UTF-8/16 input should be replaced with invalid characters.
    {"q=\xed\xed", L"q=\xd800\xd800", "?q=%EF%BF%BD%EF%BF%BD"},
      // Don't allow < or > because sometimes they are used for XSS if the
      // URL is echoed in content. Firefox does this, IE doesn't.
    {"q=<asdf>", L"q=<asdf>", "?q=%3Casdf%3E"},
      // Escape double quotemarks in the query.
    {"q=\"asdf\"", L"q=\"asdf\"", "?q=%22asdf%22"},
  };

  for (size_t i = 0; i < std::size(query_cases); i++) {
    Component out_comp;

    if (query_cases[i].input8) {
      int len = static_cast<int>(strlen(query_cases[i].input8));
      Component in_comp(0, len);
      std::string out_str;

      StdStringCanonOutput output(&out_str);
      CanonicalizeQuery(query_cases[i].input8, in_comp, NULL, &output,
                        &out_comp);
      output.Complete();

      EXPECT_EQ(query_cases[i].expected, out_str);
    }

    if (query_cases[i].input16) {
      std::u16string input16(
          test_utils::TruncateWStringToUTF16(query_cases[i].input16));
      int len = static_cast<int>(input16.length());
      Component in_comp(0, len);
      std::string out_str;

      StdStringCanonOutput output(&out_str);
      CanonicalizeQuery(input16.c_str(), in_comp, NULL, &output, &out_comp);
      output.Complete();

      EXPECT_EQ(query_cases[i].expected, out_str);
    }
  }

  // Extra test for input with embedded NULL;
  std::string out_str;
  StdStringCanonOutput output(&out_str);
  Component out_comp;
  CanonicalizeQuery("a \x00z\x01", Component(0, 5), NULL, &output, &out_comp);
  output.Complete();
  EXPECT_EQ("?a%20%00z%01", out_str);
}

TEST(URLCanonTest, Ref) {
  // Refs are trivial, it just checks the encoding.
  DualComponentCase ref_cases[] = {
      {"hello!", L"hello!", "#hello!", Component(1, 6), true},
      // We should escape spaces, double-quotes, angled braces, and backtics.
      {"hello, world", L"hello, world", "#hello,%20world", Component(1, 14),
       true},
      {"hello,\"world", L"hello,\"world", "#hello,%22world", Component(1, 14),
       true},
      {"hello,<world", L"hello,<world", "#hello,%3Cworld", Component(1, 14),
       true},
      {"hello,>world", L"hello,>world", "#hello,%3Eworld", Component(1, 14),
       true},
      {"hello,`world", L"hello,`world", "#hello,%60world", Component(1, 14),
       true},
      // UTF-8/wide input should be preserved
      {"\xc2\xa9", L"\xa9", "#%C2%A9", Component(1, 6), true},
      // Test a characer that takes > 16 bits (U+10300 = old italic letter A)
      {"\xF0\x90\x8C\x80ss", L"\xd800\xdf00ss", "#%F0%90%8C%80ss",
       Component(1, 14), true},
      // Escaping should be preserved unchanged, even invalid ones
      {"%41%a", L"%41%a", "#%41%a", Component(1, 5), true},
      // Invalid UTF-8/16 input should be flagged and the input made valid
      {"\xc2", nullptr, "#%EF%BF%BD", Component(1, 9), true},
      {nullptr, L"\xd800\x597d", "#%EF%BF%BD%E5%A5%BD", Component(1, 18), true},
      // Test a Unicode invalid character.
      {"a\xef\xb7\x90", L"a\xfdd0", "#a%EF%BF%BD", Component(1, 10), true},
      // Refs can have # signs and we should preserve them.
      {"asdf#qwer", L"asdf#qwer", "#asdf#qwer", Component(1, 9), true},
      {"#asdf", L"#asdf", "##asdf", Component(1, 5), true},
  };

  for (size_t i = 0; i < std::size(ref_cases); i++) {
    // 8-bit input
    if (ref_cases[i].input8) {
      int len = static_cast<int>(strlen(ref_cases[i].input8));
      Component in_comp(0, len);
      Component out_comp;

      std::string out_str;
      StdStringCanonOutput output(&out_str);
      CanonicalizeRef(ref_cases[i].input8, in_comp, &output, &out_comp);
      output.Complete();

      EXPECT_EQ(ref_cases[i].expected_component.begin, out_comp.begin);
      EXPECT_EQ(ref_cases[i].expected_component.len, out_comp.len);
      EXPECT_EQ(ref_cases[i].expected, out_str);
    }

    // 16-bit input
    if (ref_cases[i].input16) {
      std::u16string input16(
          test_utils::TruncateWStringToUTF16(ref_cases[i].input16));
      int len = static_cast<int>(input16.length());
      Component in_comp(0, len);
      Component out_comp;

      std::string out_str;
      StdStringCanonOutput output(&out_str);
      CanonicalizeRef(input16.c_str(), in_comp, &output, &out_comp);
      output.Complete();

      EXPECT_EQ(ref_cases[i].expected_component.begin, out_comp.begin);
      EXPECT_EQ(ref_cases[i].expected_component.len, out_comp.len);
      EXPECT_EQ(ref_cases[i].expected, out_str);
    }
  }

  // Try one with an embedded NULL. It should be stripped.
  const char null_input[5] = "ab\x00z";
  Component null_input_component(0, 4);
  Component out_comp;

  std::string out_str;
  StdStringCanonOutput output(&out_str);
  CanonicalizeRef(null_input, null_input_component, &output, &out_comp);
  output.Complete();

  EXPECT_EQ(1, out_comp.begin);
  EXPECT_EQ(6, out_comp.len);
  EXPECT_EQ("#ab%00z", out_str);
}

TEST(URLCanonTest, CanonicalizeStandardURL) {
  // The individual component canonicalize tests should have caught the cases
  // for each of those components. Here, we just need to test that the various
  // parts are included or excluded properly, and have the correct separators.
  struct URLCase {
    const char* input;
    const char* expected;
    bool expected_success;
  } cases[] = {
      {"http://www.google.com/foo?bar=baz#",
       "http://www.google.com/foo?bar=baz#", true},
      {"http://[www.google.com]/", "http://[www.google.com]/", false},
      {"ht\ttp:@www.google.com:80/;p?#", "ht%09tp://www.google.com:80/;p?#",
       false},
      {"http:////////user:@google.com:99?foo", "http://user@google.com:99/?foo",
       true},
      {"www.google.com", ":www.google.com/", false},
      {"http://192.0x00A80001", "http://192.168.0.1/", true},
      {"http://www/foo%2Ehtml", "http://www/foo.html", true},
      {"http://user:pass@/", "http://user:pass@/", false},
      {"http://%25DOMAIN:foobar@foodomain.com/",
       "http://%25DOMAIN:foobar@foodomain.com/", true},

      // Backslashes should get converted to forward slashes.
      {"http:\\\\www.google.com\\foo", "http://www.google.com/foo", true},

      // Busted refs shouldn't make the whole thing fail.
      {"http://www.google.com/asdf#\xc2",
       "http://www.google.com/asdf#%EF%BF%BD", true},

      // Basic port tests.
      {"http://foo:80/", "http://foo/", true},
      {"http://foo:81/", "http://foo:81/", true},
      {"httpa://foo:80/", "httpa://foo:80/", true},
      {"http://foo:-80/", "http://foo:-80/", false},

      {"https://foo:443/", "https://foo/", true},
      {"https://foo:80/", "https://foo:80/", true},
      {"ftp://foo:21/", "ftp://foo/", true},
      {"ftp://foo:80/", "ftp://foo:80/", true},
      {"gopher://foo:70/", "gopher://foo:70/", true},
      {"gopher://foo:443/", "gopher://foo:443/", true},
      {"ws://foo:80/", "ws://foo/", true},
      {"ws://foo:81/", "ws://foo:81/", true},
      {"ws://foo:443/", "ws://foo:443/", true},
      {"ws://foo:815/", "ws://foo:815/", true},
      {"wss://foo:80/", "wss://foo:80/", true},
      {"wss://foo:81/", "wss://foo:81/", true},
      {"wss://foo:443/", "wss://foo/", true},
      {"wss://foo:815/", "wss://foo:815/", true},

      // This particular code path ends up "backing up" to replace an invalid
      // host ICU generated with an escaped version. Test that in the context
      // of a full URL to make sure the backing up doesn't mess up the non-host
      // parts of the URL. "EF B9 AA" is U+FE6A which is a type of percent that
      // ICU will convert to an ASCII one, generating "%81".
      {"ws:)W\x1eW\xef\xb9\xaa"
       "81:80/",
       "ws://%29w%1ew%81/", false},
      // Regression test for the last_invalid_percent_index bug described in
      // https://crbug.com/1080890#c10.
      {R"(HTTP:S/5%\../>%41)", "http://s/%3EA", true},
  };

  for (size_t i = 0; i < std::size(cases); i++) {
    int url_len = static_cast<int>(strlen(cases[i].input));
    Parsed parsed;
    ParseStandardURL(cases[i].input, url_len, &parsed);

    Parsed out_parsed;
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    bool success = CanonicalizeStandardURL(
        cases[i].input, url_len, parsed,
        SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, NULL, &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(cases[i].expected_success, success);
    EXPECT_EQ(cases[i].expected, out_str);
  }
}

// The codepath here is the same as for regular canonicalization, so we just
// need to test that things are replaced or not correctly.
TEST(URLCanonTest, ReplaceStandardURL) {
  ReplaceCase replace_cases[] = {
      // Common case of truncating the path.
      {"http://www.google.com/foo?bar=baz#ref", nullptr, nullptr, nullptr,
       nullptr, nullptr, "/", kDeleteComp, kDeleteComp,
       "http://www.google.com/"},
      // Replace everything
      {"http://a:b@google.com:22/foo;bar?baz@cat", "https", "me", "pw",
       "host.com", "99", "/path", "query", "ref",
       "https://me:pw@host.com:99/path?query#ref"},
      // Replace nothing
      {"http://a:b@google.com:22/foo?baz@cat", nullptr, nullptr, nullptr,
       nullptr, nullptr, nullptr, nullptr, nullptr,
       "http://a:b@google.com:22/foo?baz@cat"},
      // Replace scheme with filesystem. The result is garbage, but you asked
      // for it.
      {"http://a:b@google.com:22/foo?baz@cat", "filesystem", nullptr, nullptr,
       nullptr, nullptr, nullptr, nullptr, nullptr,
       "filesystem://a:b@google.com:22/foo?baz@cat"},
  };

  for (size_t i = 0; i < std::size(replace_cases); i++) {
    const ReplaceCase& cur = replace_cases[i];
    int base_len = static_cast<int>(strlen(cur.base));
    Parsed parsed;
    ParseStandardURL(cur.base, base_len, &parsed);

    Replacements<char> r;
    typedef Replacements<char> R;  // Clean up syntax.

    // Note that for the scheme we pass in a different clear function since
    // there is no function to clear the scheme.
    SetupReplComp(&R::SetScheme, &R::ClearRef, &r, cur.scheme);
    SetupReplComp(&R::SetUsername, &R::ClearUsername, &r, cur.username);
    SetupReplComp(&R::SetPassword, &R::ClearPassword, &r, cur.password);
    SetupReplComp(&R::SetHost, &R::ClearHost, &r, cur.host);
    SetupReplComp(&R::SetPort, &R::ClearPort, &r, cur.port);
    SetupReplComp(&R::SetPath, &R::ClearPath, &r, cur.path);
    SetupReplComp(&R::SetQuery, &R::ClearQuery, &r, cur.query);
    SetupReplComp(&R::SetRef, &R::ClearRef, &r, cur.ref);

    std::string out_str;
    StdStringCanonOutput output(&out_str);
    Parsed out_parsed;
    ReplaceStandardURL(replace_cases[i].base, parsed, r,
                       SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, NULL,
                       &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(replace_cases[i].expected, out_str);
  }

  // The path pointer should be ignored if the address is invalid.
  {
    const char src[] = "http://www.google.com/here_is_the_path";
    int src_len = static_cast<int>(strlen(src));

    Parsed parsed;
    ParseStandardURL(src, src_len, &parsed);

    // Replace the path to 0 length string. By using 1 as the string address,
    // the test should get an access violation if it tries to dereference it.
    Replacements<char> r;
    r.SetPath(reinterpret_cast<char*>(0x00000001), Component(0, 0));
    std::string out_str1;
    StdStringCanonOutput output1(&out_str1);
    Parsed new_parsed;
    ReplaceStandardURL(src, parsed, r,
                       SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, NULL,
                       &output1, &new_parsed);
    output1.Complete();
    EXPECT_STREQ("http://www.google.com/", out_str1.c_str());

    // Same with an "invalid" path.
    r.SetPath(reinterpret_cast<char*>(0x00000001), Component());
    std::string out_str2;
    StdStringCanonOutput output2(&out_str2);
    ReplaceStandardURL(src, parsed, r,
                       SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, NULL,
                       &output2, &new_parsed);
    output2.Complete();
    EXPECT_STREQ("http://www.google.com/", out_str2.c_str());
  }
}

TEST(URLCanonTest, ReplaceFileURL) {
  ReplaceCase replace_cases[] = {
      // Replace everything
      {"file:///C:/gaba?query#ref", nullptr, nullptr, nullptr, "filer", nullptr,
       "/foo", "b", "c", "file://filer/foo?b#c"},
      // Replace nothing
      {"file:///C:/gaba?query#ref", nullptr, nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, nullptr, "file:///C:/gaba?query#ref"},
      {"file:///Y:", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, "file:///Y:"},
      {"file:///Y:/", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, "file:///Y:/"},
      {"file:///./Y", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, "file:///Y"},
      {"file:///./Y:", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, "file:///Y:"},
      // Clear non-path components (common)
      {"file:///C:/gaba?query#ref", nullptr, nullptr, nullptr, nullptr, nullptr,
       nullptr, kDeleteComp, kDeleteComp, "file:///C:/gaba"},
      // Replace path with something that doesn't begin with a slash and make
      // sure it gets added properly.
      {"file:///C:/gaba", nullptr, nullptr, nullptr, nullptr, nullptr,
       "interesting/", nullptr, nullptr, "file:///interesting/"},
      {"file:///home/gaba?query#ref", nullptr, nullptr, nullptr, "filer",
       nullptr, "/foo", "b", "c", "file://filer/foo?b#c"},
      {"file:///home/gaba?query#ref", nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, nullptr, nullptr, "file:///home/gaba?query#ref"},
      {"file:///home/gaba?query#ref", nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, kDeleteComp, kDeleteComp, "file:///home/gaba"},
      {"file:///home/gaba", nullptr, nullptr, nullptr, nullptr, nullptr,
       "interesting/", nullptr, nullptr, "file:///interesting/"},
      // Replace scheme -- shouldn't do anything.
      {"file:///C:/gaba?query#ref", "http", nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, nullptr, "file:///C:/gaba?query#ref"},
  };

  for (size_t i = 0; i < std::size(replace_cases); i++) {
    const ReplaceCase& cur = replace_cases[i];
    SCOPED_TRACE(cur.base);
    int base_len = static_cast<int>(strlen(cur.base));
    Parsed parsed;
    ParseFileURL(cur.base, base_len, &parsed);

    Replacements<char> r;
    typedef Replacements<char> R;  // Clean up syntax.
    SetupReplComp(&R::SetScheme, &R::ClearRef, &r, cur.scheme);
    SetupReplComp(&R::SetUsername, &R::ClearUsername, &r, cur.username);
    SetupReplComp(&R::SetPassword, &R::ClearPassword, &r, cur.password);
    SetupReplComp(&R::SetHost, &R::ClearHost, &r, cur.host);
    SetupReplComp(&R::SetPort, &R::ClearPort, &r, cur.port);
    SetupReplComp(&R::SetPath, &R::ClearPath, &r, cur.path);
    SetupReplComp(&R::SetQuery, &R::ClearQuery, &r, cur.query);
    SetupReplComp(&R::SetRef, &R::ClearRef, &r, cur.ref);

    std::string out_str;
    StdStringCanonOutput output(&out_str);
    Parsed out_parsed;
    ReplaceFileURL(cur.base, parsed, r, NULL, &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(replace_cases[i].expected, out_str);
  }
}

TEST(URLCanonTest, ReplaceFileSystemURL) {
  ReplaceCase replace_cases[] = {
      // Replace everything in the outer URL.
      {"filesystem:file:///temporary/gaba?query#ref", nullptr, nullptr, nullptr,
       nullptr, nullptr, "/foo", "b", "c",
       "filesystem:file:///temporary/foo?b#c"},
      // Replace nothing
      {"filesystem:file:///temporary/gaba?query#ref", nullptr, nullptr, nullptr,
       nullptr, nullptr, nullptr, nullptr, nullptr,
       "filesystem:file:///temporary/gaba?query#ref"},
      // Clear non-path components (common)
      {"filesystem:file:///temporary/gaba?query#ref", nullptr, nullptr, nullptr,
       nullptr, nullptr, nullptr, kDeleteComp, kDeleteComp,
       "filesystem:file:///temporary/gaba"},
      // Replace path with something that doesn't begin with a slash and make
      // sure it gets added properly.
      {"filesystem:file:///temporary/gaba?query#ref", nullptr, nullptr, nullptr,
       nullptr, nullptr, "interesting/", nullptr, nullptr,
       "filesystem:file:///temporary/interesting/?query#ref"},
      // Replace scheme -- shouldn't do anything except canonicalize.
      {"filesystem:http://u:p@bar.com/t/gaba?query#ref", "http", nullptr,
       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
       "filesystem:http://bar.com/t/gaba?query#ref"},
      // Replace username -- shouldn't do anything except canonicalize.
      {"filesystem:http://u:p@bar.com/t/gaba?query#ref", nullptr, "u2", nullptr,
       nullptr, nullptr, nullptr, nullptr, nullptr,
       "filesystem:http://bar.com/t/gaba?query#ref"},
      // Replace password -- shouldn't do anything except canonicalize.
      {"filesystem:http://u:p@bar.com/t/gaba?query#ref", nullptr, nullptr,
       "pw2", nullptr, nullptr, nullptr, nullptr, nullptr,
       "filesystem:http://bar.com/t/gaba?query#ref"},
      // Replace host -- shouldn't do anything except canonicalize.
      {"filesystem:http://u:p@bar.com:80/t/gaba?query#ref", nullptr, nullptr,
       nullptr, "foo.com", nullptr, nullptr, nullptr, nullptr,
       "filesystem:http://bar.com/t/gaba?query#ref"},
      // Replace port -- shouldn't do anything except canonicalize.
      {"filesystem:http://u:p@bar.com:40/t/gaba?query#ref", nullptr, nullptr,
       nullptr, nullptr, "41", nullptr, nullptr, nullptr,
       "filesystem:http://bar.com:40/t/gaba?query#ref"},
  };

  for (size_t i = 0; i < std::size(replace_cases); i++) {
    const ReplaceCase& cur = replace_cases[i];
    int base_len = static_cast<int>(strlen(cur.base));
    Parsed parsed;
    ParseFileSystemURL(cur.base, base_len, &parsed);

    Replacements<char> r;
    typedef Replacements<char> R;  // Clean up syntax.
    SetupReplComp(&R::SetScheme, &R::ClearRef, &r, cur.scheme);
    SetupReplComp(&R::SetUsername, &R::ClearUsername, &r, cur.username);
    SetupReplComp(&R::SetPassword, &R::ClearPassword, &r, cur.password);
    SetupReplComp(&R::SetHost, &R::ClearHost, &r, cur.host);
    SetupReplComp(&R::SetPort, &R::ClearPort, &r, cur.port);
    SetupReplComp(&R::SetPath, &R::ClearPath, &r, cur.path);
    SetupReplComp(&R::SetQuery, &R::ClearQuery, &r, cur.query);
    SetupReplComp(&R::SetRef, &R::ClearRef, &r, cur.ref);

    std::string out_str;
    StdStringCanonOutput output(&out_str);
    Parsed out_parsed;
    ReplaceFileSystemURL(cur.base, parsed, r, NULL, &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(replace_cases[i].expected, out_str);
  }
}

TEST(URLCanonTest, ReplacePathURL) {
  ReplaceCase replace_cases[] = {
      // Replace everything
      {"data:foo", "javascript", nullptr, nullptr, nullptr, nullptr,
       "alert('foo?');", nullptr, nullptr, "javascript:alert('foo?');"},
      // Replace nothing
      {"data:foo", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, "data:foo"},
      // Replace one or the other
      {"data:foo", "javascript", nullptr, nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, "javascript:foo"},
      {"data:foo", nullptr, nullptr, nullptr, nullptr, nullptr, "bar", nullptr,
       nullptr, "data:bar"},
      {"data:foo", nullptr, nullptr, nullptr, nullptr, nullptr, kDeleteComp,
       nullptr, nullptr, "data:"},
  };

  for (size_t i = 0; i < std::size(replace_cases); i++) {
    const ReplaceCase& cur = replace_cases[i];
    int base_len = static_cast<int>(strlen(cur.base));
    Parsed parsed;
    ParsePathURL(cur.base, base_len, false, &parsed);

    Replacements<char> r;
    typedef Replacements<char> R;  // Clean up syntax.
    SetupReplComp(&R::SetScheme, &R::ClearRef, &r, cur.scheme);
    SetupReplComp(&R::SetUsername, &R::ClearUsername, &r, cur.username);
    SetupReplComp(&R::SetPassword, &R::ClearPassword, &r, cur.password);
    SetupReplComp(&R::SetHost, &R::ClearHost, &r, cur.host);
    SetupReplComp(&R::SetPort, &R::ClearPort, &r, cur.port);
    SetupReplComp(&R::SetPath, &R::ClearPath, &r, cur.path);
    SetupReplComp(&R::SetQuery, &R::ClearQuery, &r, cur.query);
    SetupReplComp(&R::SetRef, &R::ClearRef, &r, cur.ref);

    std::string out_str;
    StdStringCanonOutput output(&out_str);
    Parsed out_parsed;
    ReplacePathURL(cur.base, parsed, r, &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(replace_cases[i].expected, out_str);
  }
}

TEST(URLCanonTest, ReplaceMailtoURL) {
  ReplaceCase replace_cases[] = {
      // Replace everything
    {"mailto:jon@foo.com?body=sup", "mailto", NULL, NULL, NULL, NULL, "addr1", "to=tony", NULL, "mailto:addr1?to=tony"},
      // Replace nothing
    {"mailto:jon@foo.com?body=sup", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "mailto:jon@foo.com?body=sup"},
      // Replace the path
    {"mailto:jon@foo.com?body=sup", NULL, NULL, NULL, NULL, NULL, "jason", NULL, NULL, "mailto:jason?body=sup"},
      // Replace the query
    {"mailto:jon@foo.com?body=sup", NULL, NULL, NULL, NULL, NULL, NULL, "custom=1", NULL, "mailto:jon@foo.com?custom=1"},
      // Replace the path and query
    {"mailto:jon@foo.com?body=sup", NULL, NULL, NULL, NULL, NULL, "jason", "custom=1", NULL, "mailto:jason?custom=1"},
      // Set the query to empty (should leave trailing question mark)
    {"mailto:jon@foo.com?body=sup", NULL, NULL, NULL, NULL, NULL, NULL, "", NULL, "mailto:jon@foo.com?"},
      // Clear the query
    {"mailto:jon@foo.com?body=sup", NULL, NULL, NULL, NULL, NULL, NULL, "|", NULL, "mailto:jon@foo.com"},
      // Clear the path
    {"mailto:jon@foo.com?body=sup", NULL, NULL, NULL, NULL, NULL, "|", NULL, NULL, "mailto:?body=sup"},
      // Clear the path + query
    {"mailto:", NULL, NULL, NULL, NULL, NULL, "|", "|", NULL, "mailto:"},
      // Setting the ref should have no effect
    {"mailto:addr1", NULL, NULL, NULL, NULL, NULL, NULL, NULL, "BLAH", "mailto:addr1"},
  };

  for (size_t i = 0; i < std::size(replace_cases); i++) {
    const ReplaceCase& cur = replace_cases[i];
    int base_len = static_cast<int>(strlen(cur.base));
    Parsed parsed;
    ParseMailtoURL(cur.base, base_len, &parsed);

    Replacements<char> r;
    typedef Replacements<char> R;
    SetupReplComp(&R::SetScheme, &R::ClearRef, &r, cur.scheme);
    SetupReplComp(&R::SetUsername, &R::ClearUsername, &r, cur.username);
    SetupReplComp(&R::SetPassword, &R::ClearPassword, &r, cur.password);
    SetupReplComp(&R::SetHost, &R::ClearHost, &r, cur.host);
    SetupReplComp(&R::SetPort, &R::ClearPort, &r, cur.port);
    SetupReplComp(&R::SetPath, &R::ClearPath, &r, cur.path);
    SetupReplComp(&R::SetQuery, &R::ClearQuery, &r, cur.query);
    SetupReplComp(&R::SetRef, &R::ClearRef, &r, cur.ref);

    std::string out_str;
    StdStringCanonOutput output(&out_str);
    Parsed out_parsed;
    ReplaceMailtoURL(cur.base, parsed, r, &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(replace_cases[i].expected, out_str);
  }
}

TEST(URLCanonTest, CanonicalizeFileURL) {
  struct URLCase {
    const char* input;
    const char* expected;
    bool expected_success;
    Component expected_host;
    Component expected_path;
  } cases[] = {
#ifdef _WIN32
      // Windows-style paths
      {"file:c:\\foo\\bar.html", "file:///C:/foo/bar.html", true, Component(),
       Component(7, 16)},
      {"  File:c|////foo\\bar.html", "file:///C:////foo/bar.html", true,
       Component(), Component(7, 19)},
      {"file:", "file:///", true, Component(), Component(7, 1)},
      {"file:UNChost/path", "file://unchost/path", true, Component(7, 7),
       Component(14, 5)},
      // CanonicalizeFileURL supports absolute Windows style paths for IE
      // compatibility. Note that the caller must decide that this is a file
      // URL itself so it can call the file canonicalizer. This is usually
      // done automatically as part of relative URL resolving.
      {"c:\\foo\\bar", "file:///C:/foo/bar", true, Component(),
       Component(7, 11)},
      {"C|/foo/bar", "file:///C:/foo/bar", true, Component(), Component(7, 11)},
      {"/C|\\foo\\bar", "file:///C:/foo/bar", true, Component(),
       Component(7, 11)},
      {"//C|/foo/bar", "file:///C:/foo/bar", true, Component(),
       Component(7, 11)},
      {"//server/file", "file://server/file", true, Component(7, 6),
       Component(13, 5)},
      {"\\\\server\\file", "file://server/file", true, Component(7, 6),
       Component(13, 5)},
      {"/\\server/file", "file://server/file", true, Component(7, 6),
       Component(13, 5)},
      // We should preserve the number of slashes after the colon for IE
      // compatibility, except when there is none, in which case we should
      // add one.
      {"file:c:foo/bar.html", "file:///C:/foo/bar.html", true, Component(),
       Component(7, 16)},
      {"file:/\\/\\C:\\\\//foo\\bar.html", "file:///C:////foo/bar.html", true,
       Component(), Component(7, 19)},
      // Three slashes should be non-UNC, even if there is no drive spec (IE
      // does this, which makes the resulting request invalid).
      {"file:///foo/bar.txt", "file:///foo/bar.txt", true, Component(),
       Component(7, 12)},
      // TODO(brettw) we should probably fail for invalid host names, which
      // would change the expected result on this test. We also currently allow
      // colon even though it's probably invalid, because its currently the
      // "natural" result of the way the canonicalizer is written. There doesn't
      // seem to be a strong argument for why allowing it here would be bad, so
      // we just tolerate it and the load will fail later.
      {"FILE:/\\/\\7:\\\\//foo\\bar.html", "file://7:////foo/bar.html", false,
       Component(7, 2), Component(9, 16)},
      {"file:filer/home\\me", "file://filer/home/me", true, Component(7, 5),
       Component(12, 8)},
      // Make sure relative paths can't go above the "C:"
      {"file:///C:/foo/../../../bar.html", "file:///C:/bar.html", true,
       Component(), Component(7, 12)},
      // Busted refs shouldn't make the whole thing fail.
      {"file:///C:/asdf#\xc2", "file:///C:/asdf#%EF%BF%BD", true, Component(),
       Component(7, 8)},
      {"file:///./s:", "file:///S:", true, Component(), Component(7, 3)},
#else
      // Unix-style paths
      {"file:///home/me", "file:///home/me", true, Component(),
       Component(7, 8)},
      // Windowsy ones should get still treated as Unix-style.
      {"file:c:\\foo\\bar.html", "file:///c:/foo/bar.html", true, Component(),
       Component(7, 16)},
      {"file:c|//foo\\bar.html", "file:///c%7C//foo/bar.html", true,
       Component(), Component(7, 19)},
      {"file:///./s:", "file:///s:", true, Component(), Component(7, 3)},
      // file: tests from WebKit (LayoutTests/fast/loader/url-parse-1.html)
      {"//", "file:///", true, Component(), Component(7, 1)},
      {"///", "file:///", true, Component(), Component(7, 1)},
      {"///test", "file:///test", true, Component(), Component(7, 5)},
      {"file://test", "file://test/", true, Component(7, 4), Component(11, 1)},
      {"file://localhost", "file://localhost/", true, Component(7, 9),
       Component(16, 1)},
      {"file://localhost/", "file://localhost/", true, Component(7, 9),
       Component(16, 1)},
      {"file://localhost/test", "file://localhost/test", true, Component(7, 9),
       Component(16, 5)},
#endif  // _WIN32
  };

  for (size_t i = 0; i < std::size(cases); i++) {
    int url_len = static_cast<int>(strlen(cases[i].input));
    Parsed parsed;
    ParseFileURL(cases[i].input, url_len, &parsed);

    Parsed out_parsed;
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    bool success = CanonicalizeFileURL(cases[i].input, url_len, parsed, NULL,
                                       &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(cases[i].expected_success, success);
    EXPECT_EQ(cases[i].expected, out_str);

    // Make sure the spec was properly identified, the file canonicalizer has
    // different code for writing the spec.
    EXPECT_EQ(0, out_parsed.scheme.begin);
    EXPECT_EQ(4, out_parsed.scheme.len);

    EXPECT_EQ(cases[i].expected_host.begin, out_parsed.host.begin);
    EXPECT_EQ(cases[i].expected_host.len, out_parsed.host.len);

    EXPECT_EQ(cases[i].expected_path.begin, out_parsed.path.begin);
    EXPECT_EQ(cases[i].expected_path.len, out_parsed.path.len);
  }
}

TEST(URLCanonTest, CanonicalizeFileSystemURL) {
  struct URLCase {
    const char* input;
    const char* expected;
    bool expected_success;
  } cases[] = {
      {"Filesystem:htTp://www.Foo.com:80/tempoRary",
       "filesystem:http://www.foo.com/tempoRary/", true},
      {"filesystem:httpS://www.foo.com/temporary/",
       "filesystem:https://www.foo.com/temporary/", true},
      {"filesystem:http://www.foo.com//", "filesystem:http://www.foo.com//",
       false},
      {"filesystem:http://www.foo.com/persistent/bob?query#ref",
       "filesystem:http://www.foo.com/persistent/bob?query#ref", true},
      {"filesystem:fIle://\\temporary/", "filesystem:file:///temporary/", true},
      {"filesystem:fiLe:///temporary", "filesystem:file:///temporary/", true},
      {"filesystem:File:///temporary/Bob?qUery#reF",
       "filesystem:file:///temporary/Bob?qUery#reF", true},
      {"FilEsysteM:htTp:E=/.", "filesystem:http://e%3D//", false},
  };

  for (size_t i = 0; i < std::size(cases); i++) {
    int url_len = static_cast<int>(strlen(cases[i].input));
    Parsed parsed;
    ParseFileSystemURL(cases[i].input, url_len, &parsed);

    Parsed out_parsed;
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    bool success = CanonicalizeFileSystemURL(cases[i].input, url_len, parsed,
                                             NULL, &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(cases[i].expected_success, success);
    EXPECT_EQ(cases[i].expected, out_str);

    // Make sure the spec was properly identified, the filesystem canonicalizer
    // has different code for writing the spec.
    EXPECT_EQ(0, out_parsed.scheme.begin);
    EXPECT_EQ(10, out_parsed.scheme.len);
    if (success)
      EXPECT_GT(out_parsed.path.len, 0);
  }
}

TEST(URLCanonTest, CanonicalizePathURL) {
  // Path URLs should get canonicalized schemes but nothing else.
  struct PathCase {
    const char* input;
    const char* expected;
  } path_cases[] = {
      {"javascript:", "javascript:"},
      {"JavaScript:Foo", "javascript:Foo"},
      {"Foo:\":This /is interesting;?#", "foo:\":This /is interesting;?#"},

      // Validation errors should not cause failure. See
      // https://crbug.com/925614.
      {"javascript:\uFFFF", "javascript:%EF%BF%BD"},
  };

  for (size_t i = 0; i < std::size(path_cases); i++) {
    int url_len = static_cast<int>(strlen(path_cases[i].input));
    Parsed parsed;
    ParsePathURL(path_cases[i].input, url_len, true, &parsed);

    Parsed out_parsed;
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    bool success = CanonicalizePathURL(path_cases[i].input, url_len, parsed,
                                       &output, &out_parsed);
    output.Complete();

    EXPECT_TRUE(success);
    EXPECT_EQ(path_cases[i].expected, out_str);

    EXPECT_EQ(0, out_parsed.host.begin);
    EXPECT_EQ(-1, out_parsed.host.len);

    // When we end with a colon at the end, there should be no path.
    if (path_cases[i].input[url_len - 1] == ':') {
      EXPECT_EQ(0, out_parsed.GetContent().begin);
      EXPECT_EQ(-1, out_parsed.GetContent().len);
    }
  }
}

TEST(URLCanonTest, CanonicalizePathURLPath) {
  struct PathCase {
    std::string input;
    std::wstring input16;
    std::string expected;
  } path_cases[] = {
      {"Foo", L"Foo", "Foo"},
      {"\":This /is interesting;?#", L"\":This /is interesting;?#",
       "\":This /is interesting;?#"},
      {"\uFFFF", L"\uFFFF", "%EF%BF%BD"},
  };

  for (size_t i = 0; i < std::size(path_cases); i++) {
    // 8-bit string input
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    url::Component out_component;
    CanonicalizePathURLPath(path_cases[i].input.data(),
                            Component(0, path_cases[i].input.size()), &output,
                            &out_component);
    output.Complete();

    EXPECT_EQ(path_cases[i].expected, out_str);

    EXPECT_EQ(0, out_component.begin);
    EXPECT_EQ(path_cases[i].expected.size(),
              static_cast<size_t>(out_component.len));

    // 16-bit string input
    std::string out_str16;
    StdStringCanonOutput output16(&out_str16);
    url::Component out_component16;
    std::u16string input16(
        test_utils::TruncateWStringToUTF16(path_cases[i].input16.data()));
    CanonicalizePathURLPath(input16.c_str(),
                            Component(0, path_cases[i].input16.size()),
                            &output16, &out_component16);
    output16.Complete();

    EXPECT_EQ(path_cases[i].expected, out_str16);

    EXPECT_EQ(0, out_component16.begin);
    EXPECT_EQ(path_cases[i].expected.size(),
              static_cast<size_t>(out_component16.len));
  }
}

TEST(URLCanonTest, CanonicalizeMailtoURL) {
  struct URLCase {
    const char* input;
    const char* expected;
    bool expected_success;
    Component expected_path;
    Component expected_query;
  } cases[] = {
    // Null character should be escaped to %00.
    // Keep this test first in the list as it is handled specially below.
    {"mailto:addr1\0addr2?foo",
     "mailto:addr1%00addr2?foo",
     true, Component(7, 13), Component(21, 3)},
    {"mailto:addr1",
     "mailto:addr1",
     true, Component(7, 5), Component()},
    {"mailto:addr1@foo.com",
     "mailto:addr1@foo.com",
     true, Component(7, 13), Component()},
    // Trailing whitespace is stripped.
    {"MaIlTo:addr1 \t ",
     "mailto:addr1",
     true, Component(7, 5), Component()},
    {"MaIlTo:addr1?to=jon",
     "mailto:addr1?to=jon",
     true, Component(7, 5), Component(13,6)},
    {"mailto:addr1,addr2",
     "mailto:addr1,addr2",
     true, Component(7, 11), Component()},
    // Embedded spaces must be encoded.
    {"mailto:addr1, addr2",
     "mailto:addr1,%20addr2",
     true, Component(7, 14), Component()},
    {"mailto:addr1, addr2?subject=one two ",
     "mailto:addr1,%20addr2?subject=one%20two",
     true, Component(7, 14), Component(22, 17)},
    {"mailto:addr1%2caddr2",
     "mailto:addr1%2caddr2",
     true, Component(7, 13), Component()},
    {"mailto:\xF0\x90\x8C\x80",
     "mailto:%F0%90%8C%80",
     true, Component(7, 12), Component()},
    // Invalid -- UTF-8 encoded surrogate value.
    {"mailto:\xed\xa0\x80",
     "mailto:%EF%BF%BD%EF%BF%BD%EF%BF%BD",
     false, Component(7, 27), Component()},
    {"mailto:addr1?",
     "mailto:addr1?",
     true, Component(7, 5), Component(13, 0)},
    // Certain characters have special meanings and must be encoded.
    {"mailto:! \x22$&()+,-./09:;<=>@AZ[\\]&_`az{|}~\x7f?Query! \x22$&()+,-./09:;<=>@AZ[\\]&_`az{|}~",
     "mailto:!%20%22$&()+,-./09:;%3C=%3E@AZ[\\]&_%60az%7B%7C%7D~%7F?Query!%20%22$&()+,-./09:;%3C=%3E@AZ[\\]&_`az{|}~",
     true, Component(7, 53), Component(61, 47)},
  };

  // Define outside of loop to catch bugs where components aren't reset
  Parsed parsed;
  Parsed out_parsed;

  for (size_t i = 0; i < std::size(cases); i++) {
    int url_len = static_cast<int>(strlen(cases[i].input));
    if (i == 0) {
      // The first test case purposely has a '\0' in it -- don't count it
      // as the string terminator.
      url_len = 22;
    }
    ParseMailtoURL(cases[i].input, url_len, &parsed);

    std::string out_str;
    StdStringCanonOutput output(&out_str);
    bool success = CanonicalizeMailtoURL(cases[i].input, url_len, parsed,
                                         &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(cases[i].expected_success, success);
    EXPECT_EQ(cases[i].expected, out_str);

    // Make sure the spec was properly identified
    EXPECT_EQ(0, out_parsed.scheme.begin);
    EXPECT_EQ(6, out_parsed.scheme.len);

    EXPECT_EQ(cases[i].expected_path.begin, out_parsed.path.begin);
    EXPECT_EQ(cases[i].expected_path.len, out_parsed.path.len);

    EXPECT_EQ(cases[i].expected_query.begin, out_parsed.query.begin);
    EXPECT_EQ(cases[i].expected_query.len, out_parsed.query.len);
  }
}

#ifndef WIN32

TEST(URLCanonTest, _itoa_s) {
  // We fill the buffer with 0xff to ensure that it's getting properly
  // null-terminated. We also allocate one byte more than what we tell
  // _itoa_s about, and ensure that the extra byte is untouched.
  char buf[6];
  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(0, _itoa_s(12, buf, sizeof(buf) - 1, 10));
  EXPECT_STREQ("12", buf);
  EXPECT_EQ('\xFF', buf[3]);

  // Test the edge cases - exactly the buffer size and one over
  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(0, _itoa_s(1234, buf, sizeof(buf) - 1, 10));
  EXPECT_STREQ("1234", buf);
  EXPECT_EQ('\xFF', buf[5]);

  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(EINVAL, _itoa_s(12345, buf, sizeof(buf) - 1, 10));
  EXPECT_EQ('\xFF', buf[5]);  // should never write to this location

  // Test the template overload (note that this will see the full buffer)
  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(0, _itoa_s(12, buf, 10));
  EXPECT_STREQ("12", buf);
  EXPECT_EQ('\xFF', buf[3]);

  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(0, _itoa_s(12345, buf, 10));
  EXPECT_STREQ("12345", buf);

  EXPECT_EQ(EINVAL, _itoa_s(123456, buf, 10));

  // Test that radix 16 is supported.
  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(0, _itoa_s(1234, buf, sizeof(buf) - 1, 16));
  EXPECT_STREQ("4d2", buf);
  EXPECT_EQ('\xFF', buf[5]);
}

TEST(URLCanonTest, _itow_s) {
  // We fill the buffer with 0xff to ensure that it's getting properly
  // null-terminated. We also allocate one byte more than what we tell
  // _itoa_s about, and ensure that the extra byte is untouched.
  char16_t buf[6];
  const char fill_mem = 0xff;
  const char16_t fill_char = 0xffff;
  memset(buf, fill_mem, sizeof(buf));
  EXPECT_EQ(0, _itow_s(12, buf, sizeof(buf) / 2 - 1, 10));
  EXPECT_EQ(u"12", std::u16string(buf));
  EXPECT_EQ(fill_char, buf[3]);

  // Test the edge cases - exactly the buffer size and one over
  EXPECT_EQ(0, _itow_s(1234, buf, sizeof(buf) / 2 - 1, 10));
  EXPECT_EQ(u"1234", std::u16string(buf));
  EXPECT_EQ(fill_char, buf[5]);

  memset(buf, fill_mem, sizeof(buf));
  EXPECT_EQ(EINVAL, _itow_s(12345, buf, sizeof(buf) / 2 - 1, 10));
  EXPECT_EQ(fill_char, buf[5]);  // should never write to this location

  // Test the template overload (note that this will see the full buffer)
  memset(buf, fill_mem, sizeof(buf));
  EXPECT_EQ(0, _itow_s(12, buf, 10));
  EXPECT_EQ(u"12", std::u16string(buf));
  EXPECT_EQ(fill_char, buf[3]);

  memset(buf, fill_mem, sizeof(buf));
  EXPECT_EQ(0, _itow_s(12345, buf, 10));
  EXPECT_EQ(u"12345", std::u16string(buf));

  EXPECT_EQ(EINVAL, _itow_s(123456, buf, 10));
}

#endif  // !WIN32

// Returns true if the given two structures are the same.
static bool ParsedIsEqual(const Parsed& a, const Parsed& b) {
  return a.scheme.begin == b.scheme.begin && a.scheme.len == b.scheme.len &&
         a.username.begin == b.username.begin && a.username.len == b.username.len &&
         a.password.begin == b.password.begin && a.password.len == b.password.len &&
         a.host.begin == b.host.begin && a.host.len == b.host.len &&
         a.port.begin == b.port.begin && a.port.len == b.port.len &&
         a.path.begin == b.path.begin && a.path.len == b.path.len &&
         a.query.begin == b.query.begin && a.query.len == b.query.len &&
         a.ref.begin == b.ref.begin && a.ref.len == b.ref.len;
}

TEST(URLCanonTest, ResolveRelativeURL) {
  struct RelativeCase {
    const char* base;      // Input base URL: MUST BE CANONICAL
    bool is_base_hier;     // Is the base URL hierarchical
    bool is_base_file;     // Tells us if the base is a file URL.
    const char* test;      // Input URL to test against.
    bool succeed_relative; // Whether we expect IsRelativeURL to succeed
    bool is_rel;           // Whether we expect |test| to be relative or not.
    bool succeed_resolve;  // Whether we expect ResolveRelativeURL to succeed.
    const char* resolved;  // What we expect in the result when resolving.
  } rel_cases[] = {
      // Basic absolute input.
    {"http://host/a", true, false, "http://another/", true, false, false, NULL},
    {"http://host/a", true, false, "http:////another/", true, false, false, NULL},
      // Empty relative URLs should only remove the ref part of the URL,
      // leaving the rest unchanged.
    {"http://foo/bar", true, false, "", true, true, true, "http://foo/bar"},
    {"http://foo/bar#ref", true, false, "", true, true, true, "http://foo/bar"},
    {"http://foo/bar#", true, false, "", true, true, true, "http://foo/bar"},
      // Spaces at the ends of the relative path should be ignored.
    {"http://foo/bar", true, false, "  another  ", true, true, true, "http://foo/another"},
    {"http://foo/bar", true, false, "  .  ", true, true, true, "http://foo/"},
    {"http://foo/bar", true, false, " \t ", true, true, true, "http://foo/bar"},
      // Matching schemes without two slashes are treated as relative.
    {"http://host/a", true, false, "http:path", true, true, true, "http://host/path"},
    {"http://host/a/", true, false, "http:path", true, true, true, "http://host/a/path"},
    {"http://host/a", true, false, "http:/path", true, true, true, "http://host/path"},
    {"http://host/a", true, false, "HTTP:/path", true, true, true, "http://host/path"},
      // Nonmatching schemes are absolute.
    {"http://host/a", true, false, "https:host2", true, false, false, NULL},
    {"http://host/a", true, false, "htto:/host2", true, false, false, NULL},
      // Absolute path input
    {"http://host/a", true, false, "/b/c/d", true, true, true, "http://host/b/c/d"},
    {"http://host/a", true, false, "\\b\\c\\d", true, true, true, "http://host/b/c/d"},
    {"http://host/a", true, false, "/b/../c", true, true, true, "http://host/c"},
    {"http://host/a?b#c", true, false, "/b/../c", true, true, true, "http://host/c"},
    {"http://host/a", true, false, "\\b/../c?x#y", true, true, true, "http://host/c?x#y"},
    {"http://host/a?b#c", true, false, "/b/../c?x#y", true, true, true, "http://host/c?x#y"},
      // Relative path input
    {"http://host/a", true, false, "b", true, true, true, "http://host/b"},
    {"http://host/a", true, false, "bc/de", true, true, true, "http://host/bc/de"},
    {"http://host/a/", true, false, "bc/de?query#ref", true, true, true, "http://host/a/bc/de?query#ref"},
    {"http://host/a/", true, false, ".", true, true, true, "http://host/a/"},
    {"http://host/a/", true, false, "..", true, true, true, "http://host/"},
    {"http://host/a/", true, false, "./..", true, true, true, "http://host/"},
    {"http://host/a/", true, false, "../.", true, true, true, "http://host/"},
    {"http://host/a/", true, false, "././.", true, true, true, "http://host/a/"},
    {"http://host/a?query#ref", true, false, "../../../foo", true, true, true, "http://host/foo"},
      // Query input
    {"http://host/a", true, false, "?foo=bar", true, true, true, "http://host/a?foo=bar"},
    {"http://host/a?x=y#z", true, false, "?", true, true, true, "http://host/a?"},
    {"http://host/a?x=y#z", true, false, "?foo=bar#com", true, true, true, "http://host/a?foo=bar#com"},
      // Ref input
    {"http://host/a", true, false, "#ref", true, true, true, "http://host/a#ref"},
    {"http://host/a#b", true, false, "#", true, true, true, "http://host/a#"},
    {"http://host/a?foo=bar#hello", true, false, "#bye", true, true, true, "http://host/a?foo=bar#bye"},
      // Non-hierarchical base: no relative handling. Relative input should
      // error, and if a scheme is present, it should be treated as absolute.
    {"data:foobar", false, false, "baz.html", false, false, false, NULL},
    {"data:foobar", false, false, "data:baz", true, false, false, NULL},
    {"data:foobar", false, false, "data:/base", true, false, false, NULL},
      // Non-hierarchical base: absolute input should succeed.
    {"data:foobar", false, false, "http://host/", true, false, false, NULL},
    {"data:foobar", false, false, "http:host", true, false, false, NULL},
      // Non-hierarchical base: empty URL should give error.
    {"data:foobar", false, false, "", false, false, false, NULL},
      // Invalid schemes should be treated as relative.
    {"http://foo/bar", true, false, "./asd:fgh", true, true, true, "http://foo/asd:fgh"},
    {"http://foo/bar", true, false, ":foo", true, true, true, "http://foo/:foo"},
    {"http://foo/bar", true, false, " hello world", true, true, true, "http://foo/hello%20world"},
    {"data:asdf", false, false, ":foo", false, false, false, NULL},
    {"data:asdf", false, false, "bad(':foo')", false, false, false, NULL},
      // We should treat semicolons like any other character in URL resolving
    {"http://host/a", true, false, ";foo", true, true, true, "http://host/;foo"},
    {"http://host/a;", true, false, ";foo", true, true, true, "http://host/;foo"},
    {"http://host/a", true, false, ";/../bar", true, true, true, "http://host/bar"},
      // Relative URLs can also be written as "//foo/bar" which is relative to
      // the scheme. In this case, it would take the old scheme, so for http
      // the example would resolve to "http://foo/bar".
    {"http://host/a", true, false, "//another", true, true, true, "http://another/"},
    {"http://host/a", true, false, "//another/path?query#ref", true, true, true, "http://another/path?query#ref"},
    {"http://host/a", true, false, "///another/path", true, true, true, "http://another/path"},
    {"http://host/a", true, false, "//Another\\path", true, true, true, "http://another/path"},
    {"http://host/a", true, false, "//", true, true, false, "http:"},
      // IE will also allow one or the other to be a backslash to get the same
      // behavior.
    {"http://host/a", true, false, "\\/another/path", true, true, true, "http://another/path"},
    {"http://host/a", true, false, "/\\Another\\path", true, true, true, "http://another/path"},
#ifdef WIN32
      // Resolving against Windows file base URLs.
    {"file:///C:/foo", true, true, "http://host/", true, false, false, NULL},
    {"file:///C:/foo", true, true, "bar", true, true, true, "file:///C:/bar"},
    {"file:///C:/foo", true, true, "../../../bar.html", true, true, true, "file:///C:/bar.html"},
    {"file:///C:/foo", true, true, "/../bar.html", true, true, true, "file:///C:/bar.html"},
      // But two backslashes on Windows should be UNC so should be treated
      // as absolute.
    {"http://host/a", true, false, "\\\\another\\path", true, false, false, NULL},
      // IE doesn't support drive specs starting with two slashes. It fails
      // immediately and doesn't even try to load. We fix it up to either
      // an absolute path or UNC depending on what it looks like.
    {"file:///C:/something", true, true, "//c:/foo", true, true, true, "file:///C:/foo"},
    {"file:///C:/something", true, true, "//localhost/c:/foo", true, true, true, "file:///C:/foo"},
      // Windows drive specs should be allowed and treated as absolute.
    {"file:///C:/foo", true, true, "c:", true, false, false, NULL},
    {"file:///C:/foo", true, true, "c:/foo", true, false, false, NULL},
    {"http://host/a", true, false, "c:\\foo", true, false, false, NULL},
      // Relative paths with drive letters should be allowed when the base is
      // also a file.
    {"file:///C:/foo", true, true, "/z:/bar", true, true, true, "file:///Z:/bar"},
      // Treat absolute paths as being off of the drive.
    {"file:///C:/foo", true, true, "/bar", true, true, true, "file:///C:/bar"},
    {"file://localhost/C:/foo", true, true, "/bar", true, true, true, "file://localhost/C:/bar"},
    {"file:///C:/foo/com/", true, true, "/bar", true, true, true, "file:///C:/bar"},
      // On Windows, two slashes without a drive letter when the base is a file
      // means that the path is UNC.
    {"file:///C:/something", true, true, "//somehost/path", true, true, true, "file://somehost/path"},
    {"file:///C:/something", true, true, "/\\//somehost/path", true, true, true, "file://somehost/path"},
#else
      // On Unix we fall back to relative behavior since there's nothing else
      // reasonable to do.
    {"http://host/a", true, false, "\\\\Another\\path", true, true, true, "http://another/path"},
#endif
      // Even on Windows, we don't allow relative drive specs when the base
      // is not file.
    {"http://host/a", true, false, "/c:\\foo", true, true, true, "http://host/c:/foo"},
    {"http://host/a", true, false, "//c:\\foo", true, true, true, "http://c/foo"},
      // Cross-platform relative file: resolution behavior.
    {"file://host/a", true, true, "/", true, true, true, "file://host/"},
    {"file://host/a", true, true, "//", true, true, true, "file:///"},
    {"file://host/a", true, true, "/b", true, true, true, "file://host/b"},
    {"file://host/a", true, true, "//b", true, true, true, "file://b/"},
      // Ensure that ports aren't allowed for hosts relative to a file url.
      // Although the result string shows a host:port portion, the call to
      // resolve the relative URL returns false, indicating parse failure,
      // which is what is required.
    {"file:///foo.txt", true, true, "//host:80/bar.txt", true, true, false, "file://host:80/bar.txt"},
      // Filesystem URL tests; filesystem URLs are only valid and relative if
      // they have no scheme, e.g. "./index.html". There's no valid equivalent
      // to http:index.html.
    {"filesystem:http://host/t/path", true, false, "filesystem:http://host/t/path2", true, false, false, NULL},
    {"filesystem:http://host/t/path", true, false, "filesystem:https://host/t/path2", true, false, false, NULL},
    {"filesystem:http://host/t/path", true, false, "http://host/t/path2", true, false, false, NULL},
    {"http://host/t/path", true, false, "filesystem:http://host/t/path2", true, false, false, NULL},
    {"filesystem:http://host/t/path", true, false, "./path2", true, true, true, "filesystem:http://host/t/path2"},
    {"filesystem:http://host/t/path/", true, false, "path2", true, true, true, "filesystem:http://host/t/path/path2"},
    {"filesystem:http://host/t/path", true, false, "filesystem:http:path2", true, false, false, NULL},
      // Absolute URLs are still not relative to a non-standard base URL.
    {"about:blank", false, false, "http://X/A", true, false, true, ""},
    {"about:blank", false, false, "content://content.Provider/", true, false, true, ""},
  };

  for (size_t i = 0; i < std::size(rel_cases); i++) {
    const RelativeCase& cur_case = rel_cases[i];

    Parsed parsed;
    int base_len = static_cast<int>(strlen(cur_case.base));
    if (cur_case.is_base_file)
      ParseFileURL(cur_case.base, base_len, &parsed);
    else if (cur_case.is_base_hier)
      ParseStandardURL(cur_case.base, base_len, &parsed);
    else
      ParsePathURL(cur_case.base, base_len, false, &parsed);

    // First see if it is relative.
    int test_len = static_cast<int>(strlen(cur_case.test));
    bool is_relative;
    Component relative_component;
    bool succeed_is_rel = IsRelativeURL(
        cur_case.base, parsed, cur_case.test, test_len, cur_case.is_base_hier,
        &is_relative, &relative_component);

    EXPECT_EQ(cur_case.succeed_relative, succeed_is_rel) <<
        "succeed is rel failure on " << cur_case.test;
    EXPECT_EQ(cur_case.is_rel, is_relative) <<
        "is rel failure on " << cur_case.test;
    // Now resolve it.
    if (succeed_is_rel && is_relative && cur_case.is_rel) {
      std::string resolved;
      StdStringCanonOutput output(&resolved);
      Parsed resolved_parsed;

      bool succeed_resolve = ResolveRelativeURL(
          cur_case.base, parsed, cur_case.is_base_file, cur_case.test,
          relative_component, NULL, &output, &resolved_parsed);
      output.Complete();

      EXPECT_EQ(cur_case.succeed_resolve, succeed_resolve);
      EXPECT_EQ(cur_case.resolved, resolved) << " on " << cur_case.test;

      // Verify that the output parsed structure is the same as parsing a
      // the URL freshly.
      Parsed ref_parsed;
      int resolved_len = static_cast<int>(resolved.size());
      if (cur_case.is_base_file) {
        ParseFileURL(resolved.c_str(), resolved_len, &ref_parsed);
      } else if (cur_case.is_base_hier) {
        ParseStandardURL(resolved.c_str(), resolved_len, &ref_parsed);
      } else {
        ParsePathURL(resolved.c_str(), resolved_len, false, &ref_parsed);
      }
      EXPECT_TRUE(ParsedIsEqual(ref_parsed, resolved_parsed));
    }
  }
}

// It used to be the case that when we did a replacement with a long buffer of
// UTF-16 characters, we would get invalid data in the URL. This is because the
// buffer that it used to hold the UTF-8 data was resized, while some pointers
// were still kept to the old buffer that was removed.
TEST(URLCanonTest, ReplacementOverflow) {
  const char src[] = "file:///C:/foo/bar";
  int src_len = static_cast<int>(strlen(src));
  Parsed parsed;
  ParseFileURL(src, src_len, &parsed);

  // Override two components, the path with something short, and the query with
  // something long enough to trigger the bug.
  Replacements<char16_t> repl;
  std::u16string new_query;
  for (int i = 0; i < 4800; i++)
    new_query.push_back('a');

  std::u16string new_path(test_utils::TruncateWStringToUTF16(L"/foo"));
  repl.SetPath(new_path.c_str(), Component(0, 4));
  repl.SetQuery(new_query.c_str(),
                Component(0, static_cast<int>(new_query.length())));

  // Call ReplaceComponents on the string. It doesn't matter if we call it for
  // standard URLs, file URLs, etc, since they will go to the same replacement
  // function that was buggy.
  Parsed repl_parsed;
  std::string repl_str;
  StdStringCanonOutput repl_output(&repl_str);
  ReplaceFileURL(src, parsed, repl, NULL, &repl_output, &repl_parsed);
  repl_output.Complete();

  // Generate the expected string and check.
  std::string expected("file:///foo?");
  for (size_t i = 0; i < new_query.length(); i++)
    expected.push_back('a');
  EXPECT_TRUE(expected == repl_str);
}

TEST(URLCanonTest, DefaultPortForScheme) {
  struct TestCases {
    const char* scheme;
    const int expected_port;
  } cases[]{
      {"http", 80},
      {"https", 443},
      {"ftp", 21},
      {"ws", 80},
      {"wss", 443},
      {"fake-scheme", PORT_UNSPECIFIED},
      {"HTTP", PORT_UNSPECIFIED},
      {"HTTPS", PORT_UNSPECIFIED},
      {"FTP", PORT_UNSPECIFIED},
      {"WS", PORT_UNSPECIFIED},
      {"WSS", PORT_UNSPECIFIED},
  };

  for (auto& test_case : cases) {
    SCOPED_TRACE(test_case.scheme);
    EXPECT_EQ(test_case.expected_port,
              DefaultPortForScheme(test_case.scheme, strlen(test_case.scheme)));
  }
}

TEST(URLCanonTest, FindWindowsDriveLetter) {
  struct TestCase {
    gurl_base::StringPiece spec;
    int begin;
    int end;  // -1 for end of spec
    int expected_drive_letter_pos;
  } cases[] = {
      {"/", 0, -1, -1},

      {"c:/foo", 0, -1, 0},
      {"/c:/foo", 0, -1, 1},
      {"//c:/foo", 0, -1, -1},  // "//" does not canonicalize to "/"
      {"\\C|\\foo", 0, -1, 1},
      {"/cd:/foo", 0, -1, -1},  // "/c" does not canonicalize to "/"
      {"/./c:/foo", 0, -1, 3},
      {"/.//c:/foo", 0, -1, -1},  // "/.//" does not canonicalize to "/"
      {"/././c:/foo", 0, -1, 5},
      {"/abc/c:/foo", 0, -1, -1},  // "/abc/" does not canonicalize to "/"
      {"/abc/./../c:/foo", 0, -1, 10},

      {"/c:/c:/foo", 3, -1, 4},  // actual input is "/c:/foo"
      {"/c:/foo", 3, -1, -1},    // actual input is "/foo"
      {"/c:/foo", 0, 1, -1},     // actual input is "/"
  };

  for (const auto& c : cases) {
    int end = c.end;
    if (end == -1)
      end = c.spec.size();

    EXPECT_EQ(c.expected_drive_letter_pos,
              FindWindowsDriveLetter(c.spec.data(), c.begin, end))
        << "for " << c.spec << "[" << c.begin << ":" << end << "] (UTF-8)";

    std::u16string spec16 = gurl_base::ASCIIToUTF16(c.spec);
    EXPECT_EQ(c.expected_drive_letter_pos,
              FindWindowsDriveLetter(spec16.data(), c.begin, end))
        << "for " << c.spec << "[" << c.begin << ":" << end << "] (UTF-16)";
  }
}

TEST(URLCanonTest, IDNToASCII) {
  RawCanonOutputW<1024> output;

  // Basic ASCII test.
  std::u16string str = u"hello";
  EXPECT_TRUE(IDNToASCII(str.data(), str.length(), &output));
  EXPECT_EQ(u"hello", std::u16string(output.data()));
  output.set_length(0);

  // Mixed ASCII/non-ASCII.
  str = u"hellö";
  EXPECT_TRUE(IDNToASCII(str.data(), str.length(), &output));
  EXPECT_EQ(u"xn--hell-8qa", std::u16string(output.data()));
  output.set_length(0);

  // All non-ASCII.
  str = u"你好";
  EXPECT_TRUE(IDNToASCII(str.data(), str.length(), &output));
  EXPECT_EQ(u"xn--6qq79v", std::u16string(output.data()));
  output.set_length(0);

  // Characters that need mapping (the resulting Punycode is the encoding for
  // "1⁄4").
  str = u"¼";
  EXPECT_TRUE(IDNToASCII(str.data(), str.length(), &output));
  EXPECT_EQ(u"xn--14-c6t", std::u16string(output.data()));
  output.set_length(0);

  // String to encode already starts with "xn--", and all ASCII. Should not
  // modify the string.
  str = u"xn--hell-8qa";
  EXPECT_TRUE(IDNToASCII(str.data(), str.length(), &output));
  EXPECT_EQ(u"xn--hell-8qa", std::u16string(output.data()));
  output.set_length(0);

  // String to encode already starts with "xn--", and mixed ASCII/non-ASCII.
  // Should fail, due to a special case: if the label starts with "xn--", it
  // should be parsed as Punycode, which must be all ASCII.
  str = u"xn--hellö";
  EXPECT_FALSE(IDNToASCII(str.data(), str.length(), &output));
  output.set_length(0);

  // String to encode already starts with "xn--", and mixed ASCII/non-ASCII.
  // This tests that there is still an error for the character '⁄' (U+2044),
  // which would be a valid ASCII character, U+0044, if the high byte were
  // ignored.
  str = u"xn--1⁄4";
  EXPECT_FALSE(IDNToASCII(str.data(), str.length(), &output));
  output.set_length(0);
}

}  // namespace url
