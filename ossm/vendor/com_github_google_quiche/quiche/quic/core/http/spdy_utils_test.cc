// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/http/spdy_utils.h"

#include <memory>
#include <string>

#include "absl/base/macros.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/platform/api/quic_test.h"

using quiche::HttpHeaderBlock;
using testing::Pair;
using testing::UnorderedElementsAre;

namespace quic {
namespace test {
namespace {

const bool kExpectFinalByteOffset = true;
const bool kDoNotExpectFinalByteOffset = false;

static std::unique_ptr<QuicHeaderList> FromList(
    const QuicHeaderList::ListType& src) {
  auto headers = std::make_unique<QuicHeaderList>();
  for (const auto& p : src) {
    headers->OnHeader(p.first, p.second);
  }
  headers->OnHeaderBlockEnd(0, 0);
  return headers;
}

}  // anonymous namespace

using CopyAndValidateHeaders = QuicTest;

TEST_F(CopyAndValidateHeaders, NormalUsage) {
  auto headers = FromList({// All cookie crumbs are joined.
                           {"cookie", " part 1"},
                           {"cookie", "part 2 "},
                           {"cookie", "part3"},

                           // Already-delimited headers are passed through.
                           {"passed-through", std::string("foo\0baz", 7)},

                           // Other headers are joined on \0.
                           {"joined", "value 1"},
                           {"joined", "value 2"},

                           // Empty headers remain empty.
                           {"empty", ""},

                           // Joined empty headers work as expected.
                           {"empty-joined", ""},
                           {"empty-joined", "foo"},
                           {"empty-joined", ""},
                           {"empty-joined", ""},

                           // Non-continguous cookie crumb.
                           {"cookie", " fin!"}});

  int64_t content_length = -1;
  HttpHeaderBlock block;
  ASSERT_TRUE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
  EXPECT_THAT(block,
              UnorderedElementsAre(
                  Pair("cookie", " part 1; part 2 ; part3;  fin!"),
                  Pair("passed-through", absl::string_view("foo\0baz", 7)),
                  Pair("joined", absl::string_view("value 1\0value 2", 15)),
                  Pair("empty", ""),
                  Pair("empty-joined", absl::string_view("\0foo\0\0", 6))));
  EXPECT_EQ(-1, content_length);
}

TEST_F(CopyAndValidateHeaders, EmptyName) {
  auto headers = FromList({{"foo", "foovalue"}, {"", "barvalue"}, {"baz", ""}});
  int64_t content_length = -1;
  HttpHeaderBlock block;
  ASSERT_FALSE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
}

TEST_F(CopyAndValidateHeaders, UpperCaseName) {
  auto headers =
      FromList({{"foo", "foovalue"}, {"bar", "barvalue"}, {"bAz", ""}});
  int64_t content_length = -1;
  HttpHeaderBlock block;
  ASSERT_FALSE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
}

TEST_F(CopyAndValidateHeaders, MultipleContentLengths) {
  auto headers = FromList({{"content-length", "9"},
                           {"foo", "foovalue"},
                           {"content-length", "9"},
                           {"bar", "barvalue"},
                           {"baz", ""}});
  int64_t content_length = -1;
  HttpHeaderBlock block;
  ASSERT_TRUE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
  EXPECT_THAT(block, UnorderedElementsAre(
                         Pair("foo", "foovalue"), Pair("bar", "barvalue"),
                         Pair("content-length", absl::string_view("9\09", 3)),
                         Pair("baz", "")));
  EXPECT_EQ(9, content_length);
}

TEST_F(CopyAndValidateHeaders, InconsistentContentLengths) {
  auto headers = FromList({{"content-length", "9"},
                           {"foo", "foovalue"},
                           {"content-length", "8"},
                           {"bar", "barvalue"},
                           {"baz", ""}});
  int64_t content_length = -1;
  HttpHeaderBlock block;
  ASSERT_FALSE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
}

TEST_F(CopyAndValidateHeaders, LargeContentLength) {
  auto headers = FromList({{"content-length", "9000000000"},
                           {"foo", "foovalue"},
                           {"bar", "barvalue"},
                           {"baz", ""}});
  int64_t content_length = -1;
  HttpHeaderBlock block;
  ASSERT_TRUE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
  EXPECT_THAT(block,
              UnorderedElementsAre(
                  Pair("foo", "foovalue"), Pair("bar", "barvalue"),
                  Pair("content-length", absl::string_view("9000000000")),
                  Pair("baz", "")));
  EXPECT_EQ(9000000000, content_length);
}

TEST_F(CopyAndValidateHeaders, NonDigitContentLength) {
  // Section 3.3.2 of RFC 7230 defines content-length as being only digits.
  // Number parsers might accept symbols like a leading plus; test that this
  // fails to parse.
  auto headers = FromList({{"content-length", "+123"},
                           {"foo", "foovalue"},
                           {"bar", "barvalue"},
                           {"baz", ""}});
  int64_t content_length = -1;
  HttpHeaderBlock block;
  EXPECT_FALSE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
}

TEST_F(CopyAndValidateHeaders, MultipleValues) {
  auto headers = FromList({{"foo", "foovalue"},
                           {"bar", "barvalue"},
                           {"baz", ""},
                           {"foo", "boo"},
                           {"baz", "buzz"}});
  int64_t content_length = -1;
  HttpHeaderBlock block;
  ASSERT_TRUE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
  EXPECT_THAT(block, UnorderedElementsAre(
                         Pair("foo", absl::string_view("foovalue\0boo", 12)),
                         Pair("bar", "barvalue"),
                         Pair("baz", absl::string_view("\0buzz", 5))));
  EXPECT_EQ(-1, content_length);
}

TEST_F(CopyAndValidateHeaders, MoreThanTwoValues) {
  auto headers = FromList({{"set-cookie", "value1"},
                           {"set-cookie", "value2"},
                           {"set-cookie", "value3"}});
  int64_t content_length = -1;
  HttpHeaderBlock block;
  ASSERT_TRUE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
  EXPECT_THAT(block, UnorderedElementsAre(Pair(
                         "set-cookie",
                         absl::string_view("value1\0value2\0value3", 20))));
  EXPECT_EQ(-1, content_length);
}

TEST_F(CopyAndValidateHeaders, Cookie) {
  auto headers = FromList({{"foo", "foovalue"},
                           {"bar", "barvalue"},
                           {"cookie", "value1"},
                           {"baz", ""}});
  int64_t content_length = -1;
  HttpHeaderBlock block;
  ASSERT_TRUE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
  EXPECT_THAT(block, UnorderedElementsAre(
                         Pair("foo", "foovalue"), Pair("bar", "barvalue"),
                         Pair("cookie", "value1"), Pair("baz", "")));
  EXPECT_EQ(-1, content_length);
}

TEST_F(CopyAndValidateHeaders, MultipleCookies) {
  auto headers = FromList({{"foo", "foovalue"},
                           {"bar", "barvalue"},
                           {"cookie", "value1"},
                           {"baz", ""},
                           {"cookie", "value2"}});
  int64_t content_length = -1;
  HttpHeaderBlock block;
  ASSERT_TRUE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
  EXPECT_THAT(block, UnorderedElementsAre(
                         Pair("foo", "foovalue"), Pair("bar", "barvalue"),
                         Pair("cookie", "value1; value2"), Pair("baz", "")));
  EXPECT_EQ(-1, content_length);
}

using CopyAndValidateTrailers = QuicTest;

TEST_F(CopyAndValidateTrailers, SimplestValidList) {
  // Verify that the simplest trailers are valid: just a final byte offset that
  // gets parsed successfully.
  auto trailers = FromList({{kFinalOffsetHeaderKey, "1234"}});
  size_t final_byte_offset = 0;
  HttpHeaderBlock block;
  EXPECT_TRUE(SpdyUtils::CopyAndValidateTrailers(
      *trailers, kExpectFinalByteOffset, &final_byte_offset, &block));
  EXPECT_EQ(1234u, final_byte_offset);
}

TEST_F(CopyAndValidateTrailers, EmptyTrailerListWithFinalByteOffsetExpected) {
  // An empty trailer list will fail as expected key kFinalOffsetHeaderKey is
  // not present.
  QuicHeaderList trailers;
  size_t final_byte_offset = 0;
  HttpHeaderBlock block;
  EXPECT_FALSE(SpdyUtils::CopyAndValidateTrailers(
      trailers, kExpectFinalByteOffset, &final_byte_offset, &block));
}

TEST_F(CopyAndValidateTrailers,
       EmptyTrailerListWithFinalByteOffsetNotExpected) {
  // An empty trailer list will pass successfully if kFinalOffsetHeaderKey is
  // not expected.
  QuicHeaderList trailers;
  size_t final_byte_offset = 0;
  HttpHeaderBlock block;
  EXPECT_TRUE(SpdyUtils::CopyAndValidateTrailers(
      trailers, kDoNotExpectFinalByteOffset, &final_byte_offset, &block));
  EXPECT_TRUE(block.empty());
}

TEST_F(CopyAndValidateTrailers, FinalByteOffsetExpectedButNotPresent) {
  // Validation fails if expected kFinalOffsetHeaderKey is not present, even if
  // the rest of the header block is valid.
  auto trailers = FromList({{"key", "value"}});
  size_t final_byte_offset = 0;
  HttpHeaderBlock block;
  EXPECT_FALSE(SpdyUtils::CopyAndValidateTrailers(
      *trailers, kExpectFinalByteOffset, &final_byte_offset, &block));
}

TEST_F(CopyAndValidateTrailers, FinalByteOffsetNotExpectedButPresent) {
  // Validation fails if kFinalOffsetHeaderKey is present but should not be,
  // even if the rest of the header block is valid.
  auto trailers = FromList({{"key", "value"}, {kFinalOffsetHeaderKey, "1234"}});
  size_t final_byte_offset = 0;
  HttpHeaderBlock block;
  EXPECT_FALSE(SpdyUtils::CopyAndValidateTrailers(
      *trailers, kDoNotExpectFinalByteOffset, &final_byte_offset, &block));
}

TEST_F(CopyAndValidateTrailers, FinalByteOffsetNotExpectedAndNotPresent) {
  // Validation succeeds if kFinalOffsetHeaderKey is not expected and not
  // present.
  auto trailers = FromList({{"key", "value"}});
  size_t final_byte_offset = 0;
  HttpHeaderBlock block;
  EXPECT_TRUE(SpdyUtils::CopyAndValidateTrailers(
      *trailers, kDoNotExpectFinalByteOffset, &final_byte_offset, &block));
  EXPECT_THAT(block, UnorderedElementsAre(Pair("key", "value")));
}

TEST_F(CopyAndValidateTrailers, EmptyName) {
  // Trailer validation will fail with an empty header key, in an otherwise
  // valid block of trailers.
  auto trailers = FromList({{"", "value"}, {kFinalOffsetHeaderKey, "1234"}});
  size_t final_byte_offset = 0;
  HttpHeaderBlock block;
  EXPECT_FALSE(SpdyUtils::CopyAndValidateTrailers(
      *trailers, kExpectFinalByteOffset, &final_byte_offset, &block));
}

TEST_F(CopyAndValidateTrailers, PseudoHeaderInTrailers) {
  // Pseudo headers are illegal in trailers.
  auto trailers =
      FromList({{":pseudo_key", "value"}, {kFinalOffsetHeaderKey, "1234"}});
  size_t final_byte_offset = 0;
  HttpHeaderBlock block;
  EXPECT_FALSE(SpdyUtils::CopyAndValidateTrailers(
      *trailers, kExpectFinalByteOffset, &final_byte_offset, &block));
}

TEST_F(CopyAndValidateTrailers, DuplicateTrailers) {
  // Duplicate trailers are allowed, and their values are concatenated into a
  // single string delimted with '\0'. Some of the duplicate headers
  // deliberately have an empty value.
  auto trailers = FromList({{"key", "value0"},
                            {"key", "value1"},
                            {"key", ""},
                            {"key", ""},
                            {"key", "value2"},
                            {"key", ""},
                            {kFinalOffsetHeaderKey, "1234"},
                            {"other_key", "value"},
                            {"key", "non_contiguous_duplicate"}});
  size_t final_byte_offset = 0;
  HttpHeaderBlock block;
  EXPECT_TRUE(SpdyUtils::CopyAndValidateTrailers(
      *trailers, kExpectFinalByteOffset, &final_byte_offset, &block));
  EXPECT_THAT(
      block,
      UnorderedElementsAre(
          Pair("key",
               absl::string_view(
                   "value0\0value1\0\0\0value2\0\0non_contiguous_duplicate",
                   48)),
          Pair("other_key", "value")));
}

TEST_F(CopyAndValidateTrailers, DuplicateCookies) {
  // Duplicate cookie headers in trailers should be concatenated into a single
  //  "; " delimted string.
  auto headers = FromList({{"cookie", " part 1"},
                           {"cookie", "part 2 "},
                           {"cookie", "part3"},
                           {"key", "value"},
                           {kFinalOffsetHeaderKey, "1234"},
                           {"cookie", " non_contiguous_cookie!"}});

  size_t final_byte_offset = 0;
  HttpHeaderBlock block;
  EXPECT_TRUE(SpdyUtils::CopyAndValidateTrailers(
      *headers, kExpectFinalByteOffset, &final_byte_offset, &block));
  EXPECT_THAT(
      block,
      UnorderedElementsAre(
          Pair("cookie", " part 1; part 2 ; part3;  non_contiguous_cookie!"),
          Pair("key", "value")));
}

using PopulateHeaderBlockFromUrl = QuicTest;

TEST_F(PopulateHeaderBlockFromUrl, NormalUsage) {
  std::string url = "https://www.google.com/index.html";
  HttpHeaderBlock headers;
  EXPECT_TRUE(SpdyUtils::PopulateHeaderBlockFromUrl(url, &headers));
  EXPECT_EQ("https", headers[":scheme"].as_string());
  EXPECT_EQ("www.google.com", headers[":authority"].as_string());
  EXPECT_EQ("/index.html", headers[":path"].as_string());
}

TEST_F(PopulateHeaderBlockFromUrl, UrlWithNoPath) {
  std::string url = "https://www.google.com";
  HttpHeaderBlock headers;
  EXPECT_TRUE(SpdyUtils::PopulateHeaderBlockFromUrl(url, &headers));
  EXPECT_EQ("https", headers[":scheme"].as_string());
  EXPECT_EQ("www.google.com", headers[":authority"].as_string());
  EXPECT_EQ("/", headers[":path"].as_string());
}

TEST_F(PopulateHeaderBlockFromUrl, Failure) {
  HttpHeaderBlock headers;
  EXPECT_FALSE(SpdyUtils::PopulateHeaderBlockFromUrl("/", &headers));
  EXPECT_FALSE(SpdyUtils::PopulateHeaderBlockFromUrl("/index.html", &headers));
  EXPECT_FALSE(
      SpdyUtils::PopulateHeaderBlockFromUrl("www.google.com/", &headers));
}

using ExtractQuicVersionFromAltSvcEntry = QuicTest;

TEST_F(ExtractQuicVersionFromAltSvcEntry, SupportedVersion) {
  ParsedQuicVersionVector supported_versions = AllSupportedVersions();
  spdy::SpdyAltSvcWireFormat::AlternativeService entry;
  for (const ParsedQuicVersion& version : supported_versions) {
    entry.protocol_id = AlpnForVersion(version);
    ParsedQuicVersion expected_version = version;
    // Versions with share an ALPN with v1 are currently unable to be
    // advertised with Alt-Svc.
    if (entry.protocol_id == AlpnForVersion(ParsedQuicVersion::RFCv1()) &&
        version != ParsedQuicVersion::RFCv1()) {
      expected_version = ParsedQuicVersion::RFCv1();
    }
    EXPECT_EQ(expected_version, SpdyUtils::ExtractQuicVersionFromAltSvcEntry(
                                    entry, supported_versions))
        << "version: " << version;
  }
}

TEST_F(ExtractQuicVersionFromAltSvcEntry, UnsupportedVersion) {
  spdy::SpdyAltSvcWireFormat::AlternativeService entry;
  entry.protocol_id = "quic";
  EXPECT_EQ(ParsedQuicVersion::Unsupported(),
            SpdyUtils::ExtractQuicVersionFromAltSvcEntry(
                entry, AllSupportedVersions()));
}

}  // namespace test
}  // namespace quic
