// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_piece.h"
#include "base/test/perf_time_logger.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"

namespace {

TEST(URLParse, FullURL) {
  constexpr gurl_base::StringPiece kUrl =
      "http://me:pass@host/foo/bar.html;param?query=yes#ref";

  url::Parsed parsed;
  gurl_base::PerfTimeLogger timer("Full_URL_Parse_AMillion");

  for (int i = 0; i < 1000000; i++)
    url::ParseStandardURL(kUrl.data(), kUrl.size(), &parsed);
  timer.Done();
}

constexpr gurl_base::StringPiece kTypicalUrl1 =
    "http://www.google.com/"
    "search?q=url+parsing&ie=utf-8&oe=utf-8&aq=t&rls=org.mozilla:en-US:"
    "official&client=firefox-a";

constexpr gurl_base::StringPiece kTypicalUrl2 =
    "http://www.amazon.com/Stephen-King-Thrillers-Horror-People/dp/0766012336/"
    "ref=sr_1_2/133-4144931-4505264?ie=UTF8&s=books&qid=2144880915&sr=8-2";

constexpr gurl_base::StringPiece kTypicalUrl3 =
    "http://store.apple.com/1-800-MY-APPLE/WebObjects/AppleStore.woa/wa/"
    "RSLID?nnmm=browse&mco=578E9744&node=home/desktop/mac_pro";

TEST(URLParse, TypicalURLParse) {
  url::Parsed parsed1;
  url::Parsed parsed2;
  url::Parsed parsed3;

  // Do this 1/3 of a million times since we do 3 different URLs.
  gurl_base::PerfTimeLogger parse_timer("Typical_URL_Parse_AMillion");
  for (int i = 0; i < 333333; i++) {
    url::ParseStandardURL(kTypicalUrl1.data(), kTypicalUrl1.size(), &parsed1);
    url::ParseStandardURL(kTypicalUrl2.data(), kTypicalUrl2.size(), &parsed2);
    url::ParseStandardURL(kTypicalUrl3.data(), kTypicalUrl3.size(), &parsed3);
  }
  parse_timer.Done();
}

// Includes both parsing and canonicalization with no mallocs.
TEST(URLParse, TypicalURLParseCanon) {
  url::Parsed parsed1;
  url::Parsed parsed2;
  url::Parsed parsed3;

  gurl_base::PerfTimeLogger canon_timer("Typical_Parse_Canon_AMillion");
  url::Parsed out_parsed;
  url::RawCanonOutput<1024> output;
  for (int i = 0; i < 333333; i++) {  // divide by 3 so we get 1M
    url::ParseStandardURL(kTypicalUrl1.data(), kTypicalUrl1.size(), &parsed1);
    output.set_length(0);
    url::CanonicalizeStandardURL(
        kTypicalUrl1.data(), kTypicalUrl1.size(), parsed1,
        url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, nullptr, &output,
        &out_parsed);

    url::ParseStandardURL(kTypicalUrl2.data(), kTypicalUrl2.size(), &parsed2);
    output.set_length(0);
    url::CanonicalizeStandardURL(
        kTypicalUrl2.data(), kTypicalUrl2.size(), parsed2,
        url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, nullptr, &output,
        &out_parsed);

    url::ParseStandardURL(kTypicalUrl3.data(), kTypicalUrl3.size(), &parsed3);
    output.set_length(0);
    url::CanonicalizeStandardURL(
        kTypicalUrl3.data(), kTypicalUrl3.size(), parsed3,
        url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, nullptr, &output,
        &out_parsed);
  }
  canon_timer.Done();
}

// Includes both parsing and canonicalization, and mallocs for the output.
TEST(URLParse, TypicalURLParseCanonStdString) {
  url::Parsed parsed1;
  url::Parsed parsed2;
  url::Parsed parsed3;

  gurl_base::PerfTimeLogger canon_timer("Typical_Parse_Canon_AMillion");
  url::Parsed out_parsed;
  for (int i = 0; i < 333333; i++) {  // divide by 3 so we get 1M
    url::ParseStandardURL(kTypicalUrl1.data(), kTypicalUrl1.size(), &parsed1);
    std::string out1;
    url::StdStringCanonOutput output1(&out1);
    url::CanonicalizeStandardURL(
        kTypicalUrl1.data(), kTypicalUrl1.size(), parsed1,
        url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, nullptr, &output1,
        &out_parsed);

    url::ParseStandardURL(kTypicalUrl2.data(), kTypicalUrl2.size(), &parsed2);
    std::string out2;
    url::StdStringCanonOutput output2(&out2);
    url::CanonicalizeStandardURL(
        kTypicalUrl2.data(), kTypicalUrl2.size(), parsed2,
        url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, nullptr, &output2,
        &out_parsed);

    url::ParseStandardURL(kTypicalUrl3.data(), kTypicalUrl3.size(), &parsed3);
    std::string out3;
    url::StdStringCanonOutput output3(&out3);
    url::CanonicalizeStandardURL(
        kTypicalUrl3.data(), kTypicalUrl3.size(), parsed3,
        url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, nullptr, &output3,
        &out_parsed);
  }
  canon_timer.Done();
}

TEST(URLParse, GURL) {
  gurl_base::PerfTimeLogger gurl_timer("Typical_GURL_AMillion");
  for (int i = 0; i < 333333; i++) {  // divide by 3 so we get 1M
    GURL gurl1(kTypicalUrl1);
    GURL gurl2(kTypicalUrl2);
    GURL gurl3(kTypicalUrl3);
  }
  gurl_timer.Done();
}

}  // namespace
