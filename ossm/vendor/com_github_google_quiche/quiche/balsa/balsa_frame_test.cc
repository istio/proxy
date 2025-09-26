// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/balsa/balsa_frame.h"

#include <stdlib.h>

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/escaping.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "quiche/balsa/balsa_enums.h"
#include "quiche/balsa/balsa_headers.h"
#include "quiche/balsa/balsa_visitor_interface.h"
#include "quiche/balsa/http_validation_policy.h"
#include "quiche/balsa/noop_balsa_visitor.h"
#include "quiche/balsa/simple_buffer.h"
#include "quiche/common/platform/api/quiche_command_line_flags.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/platform/api/quiche_flags.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/platform/api/quiche_test.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Range;
using ::testing::StrEq;
using ::testing::StrictMock;

DEFINE_QUICHE_COMMAND_LINE_FLAG(
    std::string, randseed, "",
    "This is the seed for Pseudo-random number"
    " generator used when generating random messages for unittests");

namespace quiche::test {

// This random engine from the standard library supports initialization with a
// seed, which is helpful for reproducing any unit test failures that are due to
// random sequence variation.
using RandomEngine = std::mt19937;

class BalsaFrameTestPeer {
 public:
  static int32_t HeaderFramingFound(BalsaFrame* balsa_frame, char c) {
    return balsa_frame->HeaderFramingFound(c);
  }

  static void FindColonsAndParseIntoKeyValue(BalsaFrame* balsa_frame,
                                             const BalsaFrame::Lines& lines,
                                             bool is_trailer,
                                             BalsaHeaders* headers) {
    bool has_continuation_lines = false;
    balsa_frame->FindColonsAndParseIntoKeyValue(lines, is_trailer, headers,
                                                &has_continuation_lines);
  }
};

class BalsaHeadersTestPeer {
 public:
  static void WriteFromFramer(BalsaHeaders* headers, const char* ptr,
                              size_t size) {
    headers->WriteFromFramer(ptr, size);
  }
};

namespace {

// This class encapsulates the policy of seed selection. If user supplies a
// valid use via the --randseed flag, GetSeed will only return the user
// supplied seed value. This is useful in reproducing bugs reported by the
// test. If an invalid seed value is supplied (likely due to bad numeric
// format), the test will abort (since this mode tend to be used for debugging,
// it is better to die early so the user knows a bad value is supplied). If no
// seed is supplied, the value supplied by ACMRandom::HostnamePidTimeSeed() is
// used. This class is supposed to be a singleton, but there is no ill-effect if
// multiple instances are created (although that tends not to be what the user
// wants).
class TestSeed {
 public:
  TestSeed() : test_seed_(0), user_supplied_seed_(false) {}

  void Initialize(const std::string& seed_flag) {
    if (!seed_flag.empty()) {
      ASSERT_TRUE(absl::SimpleAtoi(seed_flag, &test_seed_));
      user_supplied_seed_ = true;
    }
  }

  int GetSeed() const {
    int seed =
        (user_supplied_seed_ ? test_seed_
                             : testing::UnitTest::GetInstance()->random_seed());
    QUICHE_LOG(INFO) << "**** The current seed is " << seed << " ****";
    return seed;
  }

 private:
  int test_seed_;
  bool user_supplied_seed_;
};

static bool RandomBool(RandomEngine& rng) { return rng() % 2 != 0; }

std::string EscapeString(absl::string_view message) {
  return absl::StrReplaceAll(
      message, {{"\n", "\\\\n\n"}, {"\\r", "\\\\r"}, {"\\t", "\\\\t"}});
}

char random_lws(RandomEngine& rng) {
  if (RandomBool(rng)) {
    return '\t';
  }
  return ' ';
}

const char* random_line_term(RandomEngine& rng) {
  if (RandomBool(rng)) {
    return "\r\n";
  }
  return "\n";
}

void AppendRandomWhitespace(RandomEngine& rng, std::stringstream* s) {
  // Appending a random amount of whitespace to the unparsed value. There is a
  // max of 1000 pieces of whitespace that will be attached, however, it is
  // extremely unlikely (1 in 2^1000) that we'll hit this limit, as we have a
  // 50% probability of exiting the loop at any point in time.
  for (int i = 0; i < 1000 && RandomBool(rng); ++i) {
    *s << random_lws(rng);
  }
}

// Creates an HTTP message firstline from the given inputs.
//
// tokens - The list of nonwhitespace tokens (which should later be parsed out
//          from the firstline).
// whitespace - the whitespace that occurs before, between, and
//              after the tokens. Note that the last whitespace
//              character should -not- include any '\n'.
// line_ending - one of "\n" or "\r\n"
//
// whitespace[0] occurs before the first token.
// whitespace[1] occurs between the first and second token
// whitespace[2] occurs between the second and third token
// whitespace[3] occurs between the third token and the line_ending.
//
// This code:
//   const char tokens[3] = {"GET", "/", "HTTP/1.0"};
//   const char whitespace[4] = { "\n\n", " ", "\t", "\t"};
//   const char line_ending = "\r\n";
//   CreateFirstLine(tokens, whitespace, line_ending) ->
// Would yield the following string:
//   string(
//     "\n"
//     "\n"
//     "GET /\tHTTP/1.0\t\r\n"
//   );
//
std::string CreateFirstLine(const char* tokens[3], const char* whitespace[4],
                            const char* line_ending) {
  QUICHE_CHECK(tokens != nullptr);
  QUICHE_CHECK(whitespace != nullptr);
  QUICHE_CHECK(line_ending != nullptr);
  QUICHE_CHECK(std::string(line_ending) == "\n" ||
               std::string(line_ending) == "\r\n")
      << "line_ending: " << EscapeString(line_ending);
  SimpleBuffer firstline_buffer;
  firstline_buffer.WriteString(whitespace[0]);
  for (int i = 0; i < 3; ++i) {
    firstline_buffer.WriteString(tokens[i]);
    firstline_buffer.WriteString(whitespace[i + 1]);
  }
  firstline_buffer.WriteString(line_ending);
  return std::string(firstline_buffer.GetReadableRegion());
}

// Creates a string (ostensibly an entire HTTP message) from the given input
// arguments.
//
// firstline - the first line of the request or response.
//     The firstline should already have a line-ending on it.  If you use the
//     CreateFirstLine function, you'll get a valid firstline string for this
//     function.  This may include 'extraneous' whitespace before the first
//     nonwhitespace character, including '\n's
// headers - a list of the -interpreted- key, value pairs.
//     In other words, the value should be what you expect to get out of the
//     headers after framing has occurred (and should include no whitespace
//     before or after the first and list nonwhitespace characters,
//     respectively).  While this function will succeed if you don't follow
//     these guidelines, the VerifyHeaderLines function will likely not agree
//     with that input.
// headers_len - the number of key value pairs
// colon - the string that exists between the key and value pairs.
//     It MUST include EXACTLY one colon, and may include any amount of either
//     ' ' or '\t'. Note that for certain key strings, this value will be
//     modified to exclude any leading whitespace. See the body of the function
//     for more details.
// line_ending - one of "\r\n", or "\n\n"
// body - the appropriate body.
//     The CreateMessage function does not do any checking that the headers
//     agree with the present of any body, so the input must be correct given
//     the set of headers.
std::string CreateMessage(const char* firstline,
                          const std::pair<std::string, std::string>* headers,
                          size_t headers_len, const char* colon,
                          const char* line_ending, const char* body) {
  SimpleBuffer request_buffer;
  request_buffer.WriteString(firstline);
  if (headers_len > 0) {
    QUICHE_CHECK(headers != nullptr);
    QUICHE_CHECK(colon != nullptr);
  }
  QUICHE_CHECK(line_ending != nullptr);
  QUICHE_CHECK(std::string(line_ending) == "\n" ||
               std::string(line_ending) == "\r\n")
      << "line_ending: " << EscapeString(line_ending);
  QUICHE_CHECK(body != nullptr);
  for (size_t i = 0; i < headers_len; ++i) {
    bool only_whitespace_in_key = true;
    {
      // If the 'key' part includes no non-whitespace characters, then we need
      // to be sure that the 'colon' part includes no whitespace before the
      // ':'. If it did, then the line would be (correctly!) interpreted as a
      // continuation, and the test would not work properly.
      const char* tmp_key = headers[i].first.c_str();
      while (*tmp_key != '\0') {
        if (*tmp_key > ' ') {
          only_whitespace_in_key = false;
          break;
        }
        ++tmp_key;
      }
    }
    const char* tmp_colon = colon;
    if (only_whitespace_in_key) {
      while (*tmp_colon != ':') {
        ++tmp_colon;
      }
    }
    request_buffer.WriteString(headers[i].first);
    request_buffer.WriteString(tmp_colon);
    request_buffer.WriteString(headers[i].second);
    request_buffer.WriteString(line_ending);
  }
  request_buffer.WriteString(line_ending);
  request_buffer.WriteString(body);
  return std::string(request_buffer.GetReadableRegion());
}

void VerifyRequestFirstLine(const char* tokens[3],
                            const BalsaHeaders& headers) {
  EXPECT_EQ(tokens[0], headers.request_method());
  EXPECT_EQ(tokens[1], headers.request_uri());
  EXPECT_EQ(0u, headers.parsed_response_code());
  EXPECT_EQ(tokens[2], headers.request_version());
}

void VerifyResponseFirstLine(const char* tokens[3],
                             size_t expected_response_code,
                             const BalsaHeaders& headers) {
  EXPECT_EQ(tokens[0], headers.response_version());
  EXPECT_EQ(tokens[1], headers.response_code());
  EXPECT_EQ(expected_response_code, headers.parsed_response_code());
  EXPECT_EQ(tokens[2], headers.response_reason_phrase());
}

// This function verifies that the expected_headers key and values
// are exactly equal to that returned by an iterator to a BalsaHeader
// object.
//
// expected_headers - key, value pairs, in the order in which they're
//                    expected to be returned from the iterator.
// headers_len - as expected, the number of expected key-value pairs.
// headers - the BalsaHeaders from which we'll examine the actual
//           headers.
void VerifyHeaderLines(
    const std::pair<std::string, std::string>* expected_headers,
    size_t headers_len, const BalsaHeaders& headers) {
  BalsaHeaders::const_header_lines_iterator it = headers.lines().begin();
  for (size_t i = 0; it != headers.lines().end(); ++it, ++i) {
    ASSERT_GT(headers_len, i);
    std::string actual_key;
    std::string actual_value;
    if (!it->first.empty()) {
      actual_key = std::string(it->first);
    }
    if (!it->second.empty()) {
      actual_value = std::string(it->second);
    }
    EXPECT_THAT(actual_key, StrEq(expected_headers[i].first));
    EXPECT_THAT(actual_value, StrEq(expected_headers[i].second));
  }
  EXPECT_TRUE(headers.lines().end() == it);
}

void FirstLineParsedCorrectlyHelper(const char* tokens[3],
                                    size_t expected_response_code,
                                    bool is_request, const char* whitespace) {
  BalsaHeaders headers;
  BalsaFrame framer;
  framer.set_is_request(is_request);
  framer.set_balsa_headers(&headers);
  const char* tmp_tokens[3] = {tokens[0], tokens[1], tokens[2]};
  const char* tmp_whitespace[4] = {"", whitespace, whitespace, ""};
  for (int j = 2; j >= 0; --j) {
    framer.Reset();
    std::string firstline = CreateFirstLine(tmp_tokens, tmp_whitespace, "\n");
    std::string message =
        CreateMessage(firstline.c_str(), nullptr, 0, nullptr, "\n", "");
    SCOPED_TRACE(absl::StrFormat("input: \n%s", EscapeString(message)));
    EXPECT_GE(message.size(),
              framer.ProcessInput(message.data(), message.size()));
    // If this is a request then we don't expect a framer error (as we'll be
    // getting back warnings that fields are missing). If, however, this is
    // a response, and it is missing anything other than the reason phrase,
    // the framer will signal an error instead.
    if (is_request || j >= 1) {
      EXPECT_FALSE(framer.Error());
      if (is_request) {
        EXPECT_TRUE(framer.MessageFullyRead());
      }
      if (j == 0) {
        expected_response_code = 0;
      }
      if (is_request) {
        VerifyRequestFirstLine(tmp_tokens, *framer.headers());
      } else {
        VerifyResponseFirstLine(tmp_tokens, expected_response_code,
                                *framer.headers());
      }
    } else {
      EXPECT_TRUE(framer.Error());
    }
    tmp_tokens[j] = "";
    tmp_whitespace[j] = "";
  }
}

TEST(HTTPBalsaFrame, ParseStateToString) {
  EXPECT_STREQ("ERROR",
               BalsaFrameEnums::ParseStateToString(BalsaFrameEnums::ERROR));
  EXPECT_STREQ("READING_HEADER_AND_FIRSTLINE",
               BalsaFrameEnums::ParseStateToString(
                   BalsaFrameEnums::READING_HEADER_AND_FIRSTLINE));
  EXPECT_STREQ("READING_CHUNK_LENGTH",
               BalsaFrameEnums::ParseStateToString(
                   BalsaFrameEnums::READING_CHUNK_LENGTH));
  EXPECT_STREQ("READING_CHUNK_EXTENSION",
               BalsaFrameEnums::ParseStateToString(
                   BalsaFrameEnums::READING_CHUNK_EXTENSION));
  EXPECT_STREQ("READING_CHUNK_DATA", BalsaFrameEnums::ParseStateToString(
                                         BalsaFrameEnums::READING_CHUNK_DATA));
  EXPECT_STREQ("READING_CHUNK_TERM", BalsaFrameEnums::ParseStateToString(
                                         BalsaFrameEnums::READING_CHUNK_TERM));
  EXPECT_STREQ("READING_LAST_CHUNK_TERM",
               BalsaFrameEnums::ParseStateToString(
                   BalsaFrameEnums::READING_LAST_CHUNK_TERM));
  EXPECT_STREQ("READING_TRAILER", BalsaFrameEnums::ParseStateToString(
                                      BalsaFrameEnums::READING_TRAILER));
  EXPECT_STREQ("READING_UNTIL_CLOSE",
               BalsaFrameEnums::ParseStateToString(
                   BalsaFrameEnums::READING_UNTIL_CLOSE));
  EXPECT_STREQ("READING_CONTENT", BalsaFrameEnums::ParseStateToString(
                                      BalsaFrameEnums::READING_CONTENT));
  EXPECT_STREQ("MESSAGE_FULLY_READ", BalsaFrameEnums::ParseStateToString(
                                         BalsaFrameEnums::MESSAGE_FULLY_READ));

  EXPECT_STREQ("UNKNOWN_STATE", BalsaFrameEnums::ParseStateToString(
                                    BalsaFrameEnums::NUM_STATES));
  EXPECT_STREQ("UNKNOWN_STATE",
               BalsaFrameEnums::ParseStateToString(
                   static_cast<BalsaFrameEnums::ParseState>(-1)));

  for (int i = 0; i < BalsaFrameEnums::NUM_STATES; ++i) {
    EXPECT_STRNE("UNKNOWN_STATE",
                 BalsaFrameEnums::ParseStateToString(
                     static_cast<BalsaFrameEnums::ParseState>(i)));
  }
}

TEST(HTTPBalsaFrame, ErrorCodeToString) {
  EXPECT_STREQ("NO_STATUS_LINE_IN_RESPONSE",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::NO_STATUS_LINE_IN_RESPONSE));
  EXPECT_STREQ("NO_REQUEST_LINE_IN_REQUEST",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::NO_REQUEST_LINE_IN_REQUEST));
  EXPECT_STREQ("FAILED_TO_FIND_WS_AFTER_RESPONSE_VERSION",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::FAILED_TO_FIND_WS_AFTER_RESPONSE_VERSION));
  EXPECT_STREQ("FAILED_TO_FIND_WS_AFTER_REQUEST_METHOD",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::FAILED_TO_FIND_WS_AFTER_REQUEST_METHOD));
  EXPECT_STREQ(
      "FAILED_TO_FIND_WS_AFTER_RESPONSE_STATUSCODE",
      BalsaFrameEnums::ErrorCodeToString(
          BalsaFrameEnums::FAILED_TO_FIND_WS_AFTER_RESPONSE_STATUSCODE));
  EXPECT_STREQ(
      "FAILED_TO_FIND_WS_AFTER_REQUEST_REQUEST_URI",
      BalsaFrameEnums::ErrorCodeToString(
          BalsaFrameEnums::FAILED_TO_FIND_WS_AFTER_REQUEST_REQUEST_URI));
  EXPECT_STREQ(
      "FAILED_TO_FIND_NL_AFTER_RESPONSE_REASON_PHRASE",
      BalsaFrameEnums::ErrorCodeToString(
          BalsaFrameEnums::FAILED_TO_FIND_NL_AFTER_RESPONSE_REASON_PHRASE));
  EXPECT_STREQ(
      "FAILED_TO_FIND_NL_AFTER_REQUEST_HTTP_VERSION",
      BalsaFrameEnums::ErrorCodeToString(
          BalsaFrameEnums::FAILED_TO_FIND_NL_AFTER_REQUEST_HTTP_VERSION));
  EXPECT_STREQ("FAILED_CONVERTING_STATUS_CODE_TO_INT",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::FAILED_CONVERTING_STATUS_CODE_TO_INT));
  EXPECT_STREQ("HEADERS_TOO_LONG", BalsaFrameEnums::ErrorCodeToString(
                                       BalsaFrameEnums::HEADERS_TOO_LONG));
  EXPECT_STREQ("UNPARSABLE_CONTENT_LENGTH",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::UNPARSABLE_CONTENT_LENGTH));
  EXPECT_STREQ("MAYBE_BODY_BUT_NO_CONTENT_LENGTH",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::MAYBE_BODY_BUT_NO_CONTENT_LENGTH));
  EXPECT_STREQ("HEADER_MISSING_COLON",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::HEADER_MISSING_COLON));
  EXPECT_STREQ("INVALID_CHUNK_LENGTH",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::INVALID_CHUNK_LENGTH));
  EXPECT_STREQ("CHUNK_LENGTH_OVERFLOW",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::CHUNK_LENGTH_OVERFLOW));
  EXPECT_STREQ("CALLED_BYTES_SPLICED_WHEN_UNSAFE_TO_DO_SO",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::CALLED_BYTES_SPLICED_WHEN_UNSAFE_TO_DO_SO));
  EXPECT_STREQ("CALLED_BYTES_SPLICED_AND_EXCEEDED_SAFE_SPLICE_AMOUNT",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::
                       CALLED_BYTES_SPLICED_AND_EXCEEDED_SAFE_SPLICE_AMOUNT));
  EXPECT_STREQ("MULTIPLE_CONTENT_LENGTH_KEYS",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::MULTIPLE_CONTENT_LENGTH_KEYS));
  EXPECT_STREQ("MULTIPLE_TRANSFER_ENCODING_KEYS",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::MULTIPLE_TRANSFER_ENCODING_KEYS));
  EXPECT_STREQ("INVALID_HEADER_FORMAT",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::INVALID_HEADER_FORMAT));
  EXPECT_STREQ("INVALID_TRAILER_FORMAT",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::INVALID_TRAILER_FORMAT));
  EXPECT_STREQ("TRAILER_TOO_LONG", BalsaFrameEnums::ErrorCodeToString(
                                       BalsaFrameEnums::TRAILER_TOO_LONG));
  EXPECT_STREQ("TRAILER_MISSING_COLON",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::TRAILER_MISSING_COLON));
  EXPECT_STREQ("INTERNAL_LOGIC_ERROR",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::INTERNAL_LOGIC_ERROR));
  EXPECT_STREQ("INVALID_HEADER_CHARACTER",
               BalsaFrameEnums::ErrorCodeToString(
                   BalsaFrameEnums::INVALID_HEADER_CHARACTER));

  EXPECT_STREQ("UNKNOWN_ERROR", BalsaFrameEnums::ErrorCodeToString(
                                    BalsaFrameEnums::NUM_ERROR_CODES));
  EXPECT_STREQ("UNKNOWN_ERROR",
               BalsaFrameEnums::ErrorCodeToString(
                   static_cast<BalsaFrameEnums::ErrorCode>(-1)));

  for (int i = 0; i < BalsaFrameEnums::NUM_ERROR_CODES; ++i) {
    EXPECT_STRNE("UNKNOWN_ERROR",
                 BalsaFrameEnums::ErrorCodeToString(
                     static_cast<BalsaFrameEnums::ErrorCode>(i)));
  }
}

class FakeHeaders {
 public:
  struct KeyValuePair {
    KeyValuePair(const std::string& key, const std::string& value)
        : key(key), value(value) {}
    KeyValuePair() {}

    std::string key;
    std::string value;
  };
  typedef std::vector<KeyValuePair> KeyValuePairs;
  KeyValuePairs key_value_pairs_;

  bool operator==(const FakeHeaders& other) const {
    if (key_value_pairs_.size() != other.key_value_pairs_.size()) {
      return false;
    }
    for (KeyValuePairs::size_type i = 0; i < key_value_pairs_.size(); ++i) {
      if (key_value_pairs_[i].key != other.key_value_pairs_[i].key) {
        return false;
      }
      if (key_value_pairs_[i].value != other.key_value_pairs_[i].value) {
        return false;
      }
    }
    return true;
  }

  void AddKeyValue(const std::string& key, const std::string& value) {
    key_value_pairs_.push_back(KeyValuePair(key, value));
  }
};

class BalsaVisitorMock : public BalsaVisitorInterface {
 public:
  ~BalsaVisitorMock() override = default;

  void ProcessHeaders(const BalsaHeaders& headers) override {
    FakeHeaders fake_headers;
    GenerateFakeHeaders(headers, &fake_headers);
    ProcessHeaders(fake_headers);
  }
  void OnTrailers(std::unique_ptr<BalsaHeaders> trailers) override {
    FakeHeaders fake_trailers;
    GenerateFakeHeaders(*trailers, &fake_trailers);
    OnTrailers(fake_trailers);
  }

  MOCK_METHOD(void, OnRawBodyInput, (absl::string_view input), (override));
  MOCK_METHOD(void, OnBodyChunkInput, (absl::string_view input), (override));
  MOCK_METHOD(void, OnHeaderInput, (absl::string_view input), (override));
  MOCK_METHOD(void, OnTrailerInput, (absl::string_view input), (override));
  MOCK_METHOD(void, ProcessHeaders, (const FakeHeaders& headers));
  MOCK_METHOD(void, OnTrailers, (const FakeHeaders& trailers));
  MOCK_METHOD(void, OnRequestFirstLineInput,
              (absl::string_view line_input, absl::string_view method_input,
               absl::string_view request_uri, absl::string_view version_input),
              (override));
  MOCK_METHOD(void, OnResponseFirstLineInput,
              (absl::string_view line_input, absl::string_view version_input,
               absl::string_view status_input, absl::string_view reason_input),
              (override));
  MOCK_METHOD(void, OnChunkLength, (size_t length), (override));
  MOCK_METHOD(void, OnChunkExtensionInput, (absl::string_view input),
              (override));
  MOCK_METHOD(void, OnInterimHeaders, (std::unique_ptr<BalsaHeaders> headers),
              (override));
  MOCK_METHOD(void, ContinueHeaderDone, (), (override));
  MOCK_METHOD(void, HeaderDone, (), (override));
  MOCK_METHOD(void, MessageDone, (), (override));
  MOCK_METHOD(void, HandleError, (BalsaFrameEnums::ErrorCode error_code),
              (override));
  MOCK_METHOD(void, HandleWarning, (BalsaFrameEnums::ErrorCode error_code),
              (override));

 private:
  static void GenerateFakeHeaders(const BalsaHeaders& headers,
                                  FakeHeaders* fake_headers) {
    for (const auto& line : headers.lines()) {
      fake_headers->AddKeyValue(std::string(line.first),
                                std::string(line.second));
    }
  }
};

class HTTPBalsaFrameTest : public QuicheTest {
 protected:
  void SetUp() override {
    balsa_frame_.set_balsa_headers(&headers_);
    balsa_frame_.set_balsa_visitor(&visitor_mock_);
    balsa_frame_.set_is_request(true);
    balsa_frame_.EnableTrailers();
  }

  void VerifyFirstLineParsing(const std::string& firstline,
                              BalsaFrameEnums::ErrorCode error_code) {
    balsa_frame_.ProcessInput(firstline.data(), firstline.size());
    EXPECT_EQ(error_code, balsa_frame_.ErrorCode());
  }

  BalsaHeaders headers_;
  BalsaFrame balsa_frame_;
  NiceMock<BalsaVisitorMock> visitor_mock_;
};

// Test correct return value for HeaderFramingFound.
TEST_F(HTTPBalsaFrameTest, TestHeaderFramingFound) {
  // Pattern \r\n\r\n should match kValidTerm1.
  EXPECT_EQ(0, BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, ' '));
  EXPECT_EQ(0, BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, '\r'));
  EXPECT_EQ(0, BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, '\n'));
  EXPECT_EQ(0, BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, '\r'));
  EXPECT_EQ(BalsaFrame::kValidTerm1,
            BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, '\n'));

  // Pattern \n\r\n should match kValidTerm1.
  EXPECT_EQ(0, BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, '\t'));
  EXPECT_EQ(0, BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, '\n'));
  EXPECT_EQ(0, BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, '\r'));
  EXPECT_EQ(BalsaFrame::kValidTerm1,
            BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, '\n'));

  // Pattern \r\n\n should match kValidTerm2.
  EXPECT_EQ(0, BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, 'a'));
  EXPECT_EQ(0, BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, '\r'));
  EXPECT_EQ(0, BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, '\n'));
  EXPECT_EQ(BalsaFrame::kValidTerm2,
            BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, '\n'));

  // Pattern \n\n should match kValidTerm2.
  EXPECT_EQ(0, BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, '1'));
  EXPECT_EQ(0, BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, '\n'));
  EXPECT_EQ(BalsaFrame::kValidTerm2,
            BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, '\n'));

  // Other patterns should not match.
  EXPECT_EQ(0, BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, ':'));
  EXPECT_EQ(0, BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, '\r'));
  EXPECT_EQ(0, BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, '\r'));
  EXPECT_EQ(0, BalsaFrameTestPeer::HeaderFramingFound(&balsa_frame_, '\n'));
}

TEST_F(HTTPBalsaFrameTest, MissingColonInTrailer) {
  const absl::string_view trailer = "kv\r\n\r\n";

  BalsaFrame::Lines lines;
  lines.push_back({0, 4});
  lines.push_back({4, trailer.length()});
  BalsaHeaders trailers;
  BalsaHeadersTestPeer::WriteFromFramer(&trailers, trailer.data(),
                                        trailer.length());
  BalsaFrameTestPeer::FindColonsAndParseIntoKeyValue(
      &balsa_frame_, lines, true /*is_trailer*/, &trailers);
  // Note missing colon is not an error, just a warning.
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::TRAILER_MISSING_COLON, balsa_frame_.ErrorCode());
}

// Correctness of FindColonsAndParseIntoKeyValue is already verified for
// headers, so trailer related test is light.
TEST_F(HTTPBalsaFrameTest, FindColonsAndParseIntoKeyValueInTrailer) {
  const absl::string_view trailer_line1 = "Fraction: 0.23\r\n";
  const absl::string_view trailer_line2 = "Some:junk \r\n";
  const absl::string_view trailer_line3 = "\r\n";
  const std::string trailer =
      absl::StrCat(trailer_line1, trailer_line2, trailer_line3);

  BalsaFrame::Lines lines;
  lines.push_back({0, trailer_line1.length()});
  lines.push_back({trailer_line1.length(),
                   trailer_line1.length() + trailer_line2.length()});
  lines.push_back(
      {trailer_line1.length() + trailer_line2.length(), trailer.length()});
  BalsaHeaders trailers;
  BalsaHeadersTestPeer::WriteFromFramer(&trailers, trailer.data(),
                                        trailer.length());
  BalsaFrameTestPeer::FindColonsAndParseIntoKeyValue(
      &balsa_frame_, lines, true /*is_trailer*/, &trailers);
  EXPECT_FALSE(balsa_frame_.Error());
  absl::string_view fraction = trailers.GetHeader("Fraction");
  EXPECT_EQ("0.23", fraction);
  absl::string_view some = trailers.GetHeader("Some");
  EXPECT_EQ("junk", some);
}

TEST_F(HTTPBalsaFrameTest, InvalidTrailer) {
  const absl::string_view trailer_line1 = "Fraction : 0.23\r\n";
  const absl::string_view trailer_line2 = "Some\t  :junk \r\n";
  const absl::string_view trailer_line3 = "\r\n";
  const std::string trailer =
      absl::StrCat(trailer_line1, trailer_line2, trailer_line3);

  BalsaFrame::Lines lines;
  lines.push_back({0, trailer_line1.length()});
  lines.push_back({trailer_line1.length(),
                   trailer_line1.length() + trailer_line2.length()});
  lines.push_back(
      {trailer_line1.length() + trailer_line2.length(), trailer.length()});
  BalsaHeaders trailers;
  BalsaHeadersTestPeer::WriteFromFramer(&trailers, trailer.data(),
                                        trailer.length());
  BalsaFrameTestPeer::FindColonsAndParseIntoKeyValue(
      &balsa_frame_, lines, true /*is_trailer*/, &trailers);
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::INVALID_TRAILER_NAME_CHARACTER,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, OneCharacterFirstLineParsedAsExpected) {
  VerifyFirstLineParsing(
      "a\r\n\r\n", BalsaFrameEnums::FAILED_TO_FIND_WS_AFTER_REQUEST_METHOD);
}

TEST_F(HTTPBalsaFrameTest,
       OneCharacterFirstLineWithWhitespaceParsedAsExpected) {
  VerifyFirstLineParsing(
      "a   \r\n\r\n", BalsaFrameEnums::FAILED_TO_FIND_WS_AFTER_REQUEST_METHOD);
}

TEST_F(HTTPBalsaFrameTest, WhitespaceOnlyFirstLineIsNotACompleteHeader) {
  VerifyFirstLineParsing(" \n\n", BalsaFrameEnums::NO_REQUEST_LINE_IN_REQUEST);
}

TEST(HTTPBalsaFrame, RequestFirstLineParsedCorrectly) {
  const char* request_tokens[3] = {"GET", "/jjsdjrqk", "HTTP/1.0"};
  FirstLineParsedCorrectlyHelper(request_tokens, 0, true, " ");
  FirstLineParsedCorrectlyHelper(request_tokens, 0, true, "\t");
  FirstLineParsedCorrectlyHelper(request_tokens, 0, true, "\t    ");
  FirstLineParsedCorrectlyHelper(request_tokens, 0, true, "   \t");
  FirstLineParsedCorrectlyHelper(request_tokens, 0, true, "   \t \t  ");
}

TEST(HTTPBalsaFrame, RequestLineSanitizedProperly) {
  SCOPED_TRACE("Testing that the request line is properly sanitized.");
  using FirstLineValidationOption =
      HttpValidationPolicy::FirstLineValidationOption;

  struct TestCase {
    const absl::string_view input;     // Input to the parser.
    const absl::string_view parsed;    // Expected output.
    FirstLineValidationOption option;  // Whether to sanitize/reject.
    BalsaFrameEnums::ErrorCode expected_error;
  };
  const std::vector<TestCase> cases = {
      // No invalid whitespace.
      {"GET / HTTP/1.1\r\n", "GET / HTTP/1.1", FirstLineValidationOption::NONE,
       BalsaFrameEnums::BALSA_NO_ERROR},
      {"GET / HTTP/1.1\r\n", "GET / HTTP/1.1",
       FirstLineValidationOption::SANITIZE, BalsaFrameEnums::BALSA_NO_ERROR},
      {"GET / HTTP/1.1\r\n", "GET / HTTP/1.1",
       FirstLineValidationOption::REJECT, BalsaFrameEnums::BALSA_NO_ERROR},

      // Illegal CR in the request-line.
      {"GET /\rHTTP/1.1\r\n", "GET /\rHTTP/1.1",
       FirstLineValidationOption::NONE, BalsaFrameEnums::BALSA_NO_ERROR},
      {"GET /\rHTTP/1.1\r\n", "GET / HTTP/1.1",
       FirstLineValidationOption::SANITIZE, BalsaFrameEnums::BALSA_NO_ERROR},
      {"GET /\rHTTP/1.1\r\n", "", FirstLineValidationOption::REJECT,
       BalsaFrameEnums::INVALID_WS_IN_REQUEST_LINE},

      // Invalid tab in the request-line.
      {"GET \t/ HTTP/1.1\r\n", "GET \t/ HTTP/1.1",
       FirstLineValidationOption::NONE, BalsaFrameEnums::BALSA_NO_ERROR},
      {"GET \t/ HTTP/1.1\r\n", "GET  / HTTP/1.1",
       FirstLineValidationOption::SANITIZE, BalsaFrameEnums::BALSA_NO_ERROR},
      {"GET \t/ HTTP/1.1\r\n", "", FirstLineValidationOption::REJECT,
       BalsaFrameEnums::INVALID_WS_IN_REQUEST_LINE},

      // Both CR and tab in the request-line.
      {"GET \t/\rHTTP/1.1 \r\n", "GET \t/\rHTTP/1.1",
       FirstLineValidationOption::NONE, BalsaFrameEnums::BALSA_NO_ERROR},
      {"GET \t/\rHTTP/1.1 \r\n", "GET  / HTTP/1.1",
       FirstLineValidationOption::SANITIZE, BalsaFrameEnums::BALSA_NO_ERROR},
      {"GET \t/\rHTTP/1.1 \r\n", "", FirstLineValidationOption::REJECT,
       BalsaFrameEnums::INVALID_WS_IN_REQUEST_LINE},
  };
  const absl::string_view kHeaderLineAndEnding = "Foo: bar\r\n\r\n";
  for (auto& [firstline, parsed, ws_option, expected_error] : cases) {
    SCOPED_TRACE(
        absl::StrCat("Input: ", absl::CEscape(firstline),
                     " Expected output: ", absl::CEscape(parsed),
                     " whitespace option: ", static_cast<int>(ws_option)));
    const std::string input = absl::StrCat(firstline, kHeaderLineAndEnding);

    BalsaHeaders headers;
    BalsaFrame framer;
    HttpValidationPolicy policy;
    policy.sanitize_cr_tab_in_first_line = ws_option;
    framer.set_http_validation_policy(policy);
    framer.set_is_request(true);
    framer.set_balsa_headers(&headers);
    framer.ProcessInput(input.data(), input.size());
    EXPECT_EQ(headers.first_line(), parsed);
    EXPECT_EQ(framer.ErrorCode(), expected_error);
  }
}

TEST_F(HTTPBalsaFrameTest, NonnumericResponseCode) {
  balsa_frame_.set_is_request(false);

  VerifyFirstLineParsing("HTTP/1.1 0x3 Digits only\r\n\r\n",
                         BalsaFrameEnums::FAILED_CONVERTING_STATUS_CODE_TO_INT);

  EXPECT_EQ("HTTP/1.1 0x3 Digits only", headers_.first_line());
}

TEST_F(HTTPBalsaFrameTest, NegativeResponseCode) {
  balsa_frame_.set_is_request(false);

  VerifyFirstLineParsing("HTTP/1.1 -11 No sign allowed\r\n\r\n",
                         BalsaFrameEnums::FAILED_CONVERTING_STATUS_CODE_TO_INT);

  EXPECT_EQ("HTTP/1.1 -11 No sign allowed", headers_.first_line());
}

TEST_F(HTTPBalsaFrameTest, WithoutTrailingWhitespace) {
  balsa_frame_.set_is_request(false);

  VerifyFirstLineParsing(
      "HTTP/1.1 101\r\n\r\n",
      BalsaFrameEnums::FAILED_TO_FIND_WS_AFTER_RESPONSE_STATUSCODE);

  EXPECT_EQ("HTTP/1.1 101", headers_.first_line());
}

TEST_F(HTTPBalsaFrameTest, TrailingWhitespace) {
  balsa_frame_.set_is_request(false);

  // b/69446061
  std::string firstline = "HTTP/1.1 101 \r\n\r\n";
  balsa_frame_.ProcessInput(firstline.data(), firstline.size());

  EXPECT_EQ("HTTP/1.1 101 ", headers_.first_line());
}

TEST(HTTPBalsaFrame, ResponseFirstLineParsedCorrectly) {
  const char* response_tokens[3] = {"HTTP/1.1", "200", "A reason\tphrase"};
  FirstLineParsedCorrectlyHelper(response_tokens, 200, false, " ");
  FirstLineParsedCorrectlyHelper(response_tokens, 200, false, "\t");
  FirstLineParsedCorrectlyHelper(response_tokens, 200, false, "\t    ");
  FirstLineParsedCorrectlyHelper(response_tokens, 200, false, "   \t");
  FirstLineParsedCorrectlyHelper(response_tokens, 200, false, "   \t \t  ");

  response_tokens[1] = "312";
  FirstLineParsedCorrectlyHelper(response_tokens, 312, false, " ");
  FirstLineParsedCorrectlyHelper(response_tokens, 312, false, "\t");
  FirstLineParsedCorrectlyHelper(response_tokens, 312, false, "\t    ");
  FirstLineParsedCorrectlyHelper(response_tokens, 312, false, "   \t");
  FirstLineParsedCorrectlyHelper(response_tokens, 312, false, "   \t \t  ");

  // Who knows what the future may hold w.r.t. response codes?!
  response_tokens[1] = "4242";
  FirstLineParsedCorrectlyHelper(response_tokens, 4242, false, " ");
  FirstLineParsedCorrectlyHelper(response_tokens, 4242, false, "\t");
  FirstLineParsedCorrectlyHelper(response_tokens, 4242, false, "\t    ");
  FirstLineParsedCorrectlyHelper(response_tokens, 4242, false, "   \t");
  FirstLineParsedCorrectlyHelper(response_tokens, 4242, false, "   \t \t  ");
}

TEST(HTTPBalsaFrame, StatusLineSanitizedProperly) {
  SCOPED_TRACE("Testing that the status line is properly sanitized.");
  using FirstLineValidationOption =
      HttpValidationPolicy::FirstLineValidationOption;

  struct TestCase {
    const absl::string_view input;     // Input to the parser.
    const absl::string_view parsed;    // Expected output.
    FirstLineValidationOption option;  // Whether to sanitize/reject.
    BalsaFrameEnums::ErrorCode expected_error;
  };
  const std::vector<TestCase> cases = {
      // No invalid whitespace.
      {"HTTP/1.1 200 OK\r\n", "HTTP/1.1 200 OK",
       FirstLineValidationOption::NONE, BalsaFrameEnums::BALSA_NO_ERROR},
      {"HTTP/1.1 200 OK\r\n", "HTTP/1.1 200 OK",
       FirstLineValidationOption::SANITIZE, BalsaFrameEnums::BALSA_NO_ERROR},
      {"HTTP/1.1 200 OK\r\n", "HTTP/1.1 200 OK",
       FirstLineValidationOption::REJECT, BalsaFrameEnums::BALSA_NO_ERROR},

      // Illegal CR in the status-line.
      {"HTTP/1.1 200\rOK\r\n", "HTTP/1.1 200\rOK",
       FirstLineValidationOption::NONE, BalsaFrameEnums::BALSA_NO_ERROR},
      {"HTTP/1.1 200\rOK\r\n", "HTTP/1.1 200 OK",
       FirstLineValidationOption::SANITIZE, BalsaFrameEnums::BALSA_NO_ERROR},
      {"HTTP/1.1 200\rOK\r\n", "", FirstLineValidationOption::REJECT,
       BalsaFrameEnums::INVALID_WS_IN_STATUS_LINE},

      // Invalid tab in the status-line.
      {"HTTP/1.1 \t200 OK\r\n", "HTTP/1.1 \t200 OK",
       FirstLineValidationOption::NONE, BalsaFrameEnums::BALSA_NO_ERROR},
      {"HTTP/1.1 \t200 OK\r\n", "HTTP/1.1  200 OK",
       FirstLineValidationOption::SANITIZE, BalsaFrameEnums::BALSA_NO_ERROR},
      {"HTTP/1.1 \t200 OK\r\n", "", FirstLineValidationOption::REJECT,
       BalsaFrameEnums::INVALID_WS_IN_STATUS_LINE},

      // Both CR and tab in the request-line.
      {"HTTP/1.1 \t200\rOK \r\n", "HTTP/1.1 \t200\rOK",
       FirstLineValidationOption::NONE, BalsaFrameEnums::BALSA_NO_ERROR},
      {"HTTP/1.1 \t200\rOK \r\n", "HTTP/1.1  200 OK",
       FirstLineValidationOption::SANITIZE, BalsaFrameEnums::BALSA_NO_ERROR},
      {"HTTP/1.1 \t200\rOK \r\n", "", FirstLineValidationOption::REJECT,
       BalsaFrameEnums::INVALID_WS_IN_STATUS_LINE},
  };
  const absl::string_view kHeaderLineAndEnding =
      "Foo: bar\r\nContent-Length: 0\r\n\r\n";
  for (auto& [firstline, parsed, ws_option, expected_error] : cases) {
    SCOPED_TRACE(
        absl::StrCat("Input: ", absl::CEscape(firstline),
                     " Expected output: ", absl::CEscape(parsed),
                     " whitespace option: ", static_cast<int>(ws_option)));
    const std::string input = absl::StrCat(firstline, kHeaderLineAndEnding);

    BalsaHeaders headers;
    BalsaFrame framer;
    HttpValidationPolicy policy;
    policy.sanitize_cr_tab_in_first_line = ws_option;
    framer.set_http_validation_policy(policy);
    framer.set_is_request(false);
    framer.set_balsa_headers(&headers);
    framer.ProcessInput(input.data(), input.size());
    EXPECT_EQ(headers.first_line(), parsed);
    EXPECT_EQ(framer.ErrorCode(), expected_error);
  }
}

void HeaderLineTestHelper(const char* firstline, bool is_request,
                          const std::pair<std::string, std::string>* headers,
                          size_t headers_len, const char* colon,
                          const char* line_ending) {
  BalsaHeaders balsa_headers;
  BalsaFrame framer;
  framer.set_is_request(is_request);
  framer.set_balsa_headers(&balsa_headers);
  std::string message =
      CreateMessage(firstline, headers, headers_len, colon, line_ending, "");
  SCOPED_TRACE(EscapeString(message));
  size_t bytes_consumed = framer.ProcessInput(message.data(), message.size());
  EXPECT_EQ(message.size(), bytes_consumed);
  VerifyHeaderLines(headers, headers_len, *framer.headers());
}

TEST(HTTPBalsaFrame, RequestLinesParsedProperly) {
  SCOPED_TRACE("Testing that lines are properly parsed.");
  const char firstline[] = "GET / HTTP/1.1\r\n";
  const std::pair<std::string, std::string> headers[] = {
      std::pair<std::string, std::string>("foo", "bar"),
      std::pair<std::string, std::string>("duck", "water"),
      std::pair<std::string, std::string>("goose", "neck"),
      std::pair<std::string, std::string>("key_is_fine",
                                          "value:includes:colons"),
      std::pair<std::string, std::string>("trucks",
                                          "along\rvalue\rincluding\rslash\rrs"),
      std::pair<std::string, std::string>("monster", "truck"),
      std::pair<std::string, std::string>("another_key", ":colons in value"),
      std::pair<std::string, std::string>("another_key", "colons in value:"),
      std::pair<std::string, std::string>("another_key",
                                          "value includes\r\n continuation"),
      std::pair<std::string, std::string>("key_without_continuations",
                                          "multiple\n in\r\n the\n value"),
      std::pair<std::string, std::string>("key_without_value",
                                          ""),  // empty value
      std::pair<std::string, std::string>("",
                                          "value without key"),  // empty key
      std::pair<std::string, std::string>("", ""),  // both key and value empty
      std::pair<std::string, std::string>("normal_key", "normal_value"),
  };
  const size_t headers_len = ABSL_ARRAYSIZE(headers);
  HeaderLineTestHelper(firstline, true, headers, headers_len, ":", "\n");
  HeaderLineTestHelper(firstline, true, headers, headers_len, ": ", "\n");
  HeaderLineTestHelper(firstline, true, headers, headers_len, ": ", "\r\n");
  HeaderLineTestHelper(firstline, true, headers, headers_len, ":\t", "\n");
  HeaderLineTestHelper(firstline, true, headers, headers_len, ":\t", "\r\n");
  HeaderLineTestHelper(firstline, true, headers, headers_len, ":\t ", "\n");
  HeaderLineTestHelper(firstline, true, headers, headers_len, ":\t ", "\r\n");
  HeaderLineTestHelper(firstline, true, headers, headers_len, ":\t\t", "\n");
  HeaderLineTestHelper(firstline, true, headers, headers_len, ":\t\t", "\r\n");
  HeaderLineTestHelper(firstline, true, headers, headers_len, ":\t \t", "\n");
  HeaderLineTestHelper(firstline, true, headers, headers_len, ":\t \t", "\r\n");
}

TEST(HTTPBalsaFrame, CarriageReturnIllegalInHeaders) {
  HttpValidationPolicy policy{.disallow_lone_cr_in_request_headers = true};
  BalsaHeaders balsa_headers;
  BalsaFrame framer;
  framer.set_is_request(true);
  framer.set_balsa_headers(&balsa_headers);
  framer.set_http_validation_policy(policy);
  framer.set_invalid_chars_level(BalsaFrame::InvalidCharsLevel::kError);
  const std::pair<std::string, std::string> headers[] = {
      std::pair<std::string, std::string>("foo", "bar"),
      std::pair<std::string, std::string>("trucks", "value-has-solo-\r-in it"),
  };
  std::string message =
      CreateMessage("GET / \rHTTP/1.1\r\n", headers, 2, ":", "\r\n", "");
  framer.ProcessInput(message.data(), message.size());
  EXPECT_EQ(framer.ErrorCode(), BalsaFrameEnums::INVALID_HEADER_CHARACTER);
}

// Test that lone '\r' detection works correctly in the firstline
// even if it is the last character of fractional input.
TEST(HTTPBalsaFrame, CarriageReturnIllegalInFirstLineOnInputBoundary) {
  HttpValidationPolicy policy{.disallow_lone_cr_in_request_headers = true};
  BalsaHeaders balsa_headers;
  BalsaFrame framer;
  framer.set_is_request(true);
  framer.set_balsa_headers(&balsa_headers);
  framer.set_http_validation_policy(policy);
  framer.set_invalid_chars_level(BalsaFrame::InvalidCharsLevel::kError);
  constexpr absl::string_view message1("GET / \r");
  constexpr absl::string_view message2("HTTP/1.1\r\n\r\n");
  EXPECT_EQ(message1.size(),
            framer.ProcessInput(message1.data(), message1.size()));
  EXPECT_EQ(message2.size(),
            framer.ProcessInput(message2.data(), message2.size()));
  EXPECT_EQ(framer.ErrorCode(), BalsaFrameEnums::INVALID_HEADER_CHARACTER);
}

// Test that lone '\r' detection works correctly in header values
// even if it is the last character of fractional input.
TEST(HTTPBalsaFrame, CarriageReturnIllegalInHeaderValueOnInputBoundary) {
  HttpValidationPolicy policy{.disallow_lone_cr_in_request_headers = true};
  BalsaHeaders balsa_headers;
  BalsaFrame framer;
  framer.set_is_request(true);
  framer.set_balsa_headers(&balsa_headers);
  framer.set_http_validation_policy(policy);
  framer.set_invalid_chars_level(BalsaFrame::InvalidCharsLevel::kError);
  constexpr absl::string_view message1("GET / HTTP/1.1\r\nfoo: b\r");
  constexpr absl::string_view message2("ar\r\n\r\n");
  EXPECT_EQ(message1.size(),
            framer.ProcessInput(message1.data(), message1.size()));
  EXPECT_EQ(message2.size(),
            framer.ProcessInput(message2.data(), message2.size()));
  EXPECT_EQ(framer.ErrorCode(), BalsaFrameEnums::INVALID_HEADER_CHARACTER);
}

TEST(HTTPBalsaFrame, CarriageReturnIllegalInHeaderKey) {
  BalsaHeaders balsa_headers;
  BalsaFrame framer;
  framer.set_is_request(true);
  framer.set_balsa_headers(&balsa_headers);
  framer.set_invalid_chars_level(BalsaFrame::InvalidCharsLevel::kError);
  const std::pair<std::string, std::string> headers[] = {
      std::pair<std::string, std::string>("tru\rcks", "along"),
  };
  std::string message =
      CreateMessage("GET / HTTP/1.1\r\n", headers, 1, ":", "\r\n", "");
  framer.ProcessInput(message.data(), message.size());
  EXPECT_EQ(framer.ErrorCode(), BalsaFrameEnums::INVALID_HEADER_NAME_CHARACTER);
}

TEST(HTTPBalsaFrame, ResponseLinesParsedProperly) {
  SCOPED_TRACE("ResponseLineParsedProperly");
  const char firstline[] = "HTTP/1.0 200 A reason\tphrase\r\n";
  const std::pair<std::string, std::string> headers[] = {
      std::pair<std::string, std::string>("foo", "bar"),
      std::pair<std::string, std::string>("duck", "water"),
      std::pair<std::string, std::string>("goose", "neck"),
      std::pair<std::string, std::string>("key_is_fine",
                                          "value:includes:colons"),
      std::pair<std::string, std::string>("trucks",
                                          "along\rvalue\rincluding\rslash\rrs"),
      std::pair<std::string, std::string>("monster", "truck"),
      std::pair<std::string, std::string>("another_key", ":colons in value"),
      std::pair<std::string, std::string>("another_key", "colons in value:"),
      std::pair<std::string, std::string>("another_key",
                                          "value includes\r\n continuation"),
      std::pair<std::string, std::string>("key_includes_no_continuations",
                                          "multiple\n in\r\n the\n value"),
      std::pair<std::string, std::string>("key_without_value",
                                          ""),  // empty value
      std::pair<std::string, std::string>("",
                                          "value without key"),  // empty key
      std::pair<std::string, std::string>("", ""),  // both key and value empty
      std::pair<std::string, std::string>("normal_key", "normal_value"),
  };
  const size_t headers_len = ABSL_ARRAYSIZE(headers);
  HeaderLineTestHelper(firstline, false, headers, headers_len, ":", "\n");
  HeaderLineTestHelper(firstline, false, headers, headers_len, ": ", "\n");
  HeaderLineTestHelper(firstline, false, headers, headers_len, ": ", "\r\n");
  HeaderLineTestHelper(firstline, false, headers, headers_len, ":\t", "\n");
  HeaderLineTestHelper(firstline, false, headers, headers_len, ":\t", "\r\n");
  HeaderLineTestHelper(firstline, false, headers, headers_len, ":\t ", "\n");
  HeaderLineTestHelper(firstline, false, headers, headers_len, ":\t ", "\r\n");
  HeaderLineTestHelper(firstline, false, headers, headers_len, ":\t\t", "\n");
  HeaderLineTestHelper(firstline, false, headers, headers_len, ":\t\t", "\r\n");
  HeaderLineTestHelper(firstline, false, headers, headers_len, ":\t \t", "\n");
  HeaderLineTestHelper(firstline, false, headers, headers_len, ":\t \t",
                       "\r\n");
}

void WhitespaceHeaderTestHelper(
    const std::string& message, bool is_request,
    BalsaFrameEnums::ErrorCode expected_error_code) {
  BalsaHeaders balsa_headers;
  BalsaFrame framer;
  framer.set_is_request(is_request);
  framer.set_balsa_headers(&balsa_headers);
  SCOPED_TRACE(EscapeString(message));
  size_t bytes_consumed = framer.ProcessInput(message.data(), message.size());
  EXPECT_EQ(message.size(), bytes_consumed);
  if (expected_error_code == BalsaFrameEnums::BALSA_NO_ERROR ||
      // Obs fold currently is not treated as an error, but only a warning.
      expected_error_code == BalsaFrameEnums::OBS_FOLD_IN_HEADERS ||
      expected_error_code == BalsaFrameEnums::OBS_FOLD_IN_TRAILERS) {
    EXPECT_EQ(false, framer.Error());
  } else {
    EXPECT_EQ(true, framer.Error());
  }
  EXPECT_EQ(expected_error_code, framer.ErrorCode());
}

TEST(HTTPBalsaFrame, WhitespaceInRequestsProcessedProperly) {
  SCOPED_TRACE(
      "Test that a request header with a line with spaces and no "
      "data generates an error.");
  WhitespaceHeaderTestHelper(
      "GET / HTTP/1.1\r\n"
      " \r\n"
      "\r\n",
      true, BalsaFrameEnums::INVALID_HEADER_NAME_CHARACTER);
  WhitespaceHeaderTestHelper(
      "GET / HTTP/1.1\r\n"
      "   \r\n"
      "test: test\r\n"
      "\r\n",
      true, BalsaFrameEnums::INVALID_HEADER_NAME_CHARACTER);

  SCOPED_TRACE("Test proper handling for line continuation in requests.");
  WhitespaceHeaderTestHelper(
      "GET / HTTP/1.1\r\n"
      "test: test\r\n"
      " continued\r\n"
      "\r\n",
      true, BalsaFrameEnums::OBS_FOLD_IN_HEADERS);
  WhitespaceHeaderTestHelper(
      "GET / HTTP/1.1\r\n"
      "test: test\r\n"
      " \r\n"
      "\r\n",
      true, BalsaFrameEnums::OBS_FOLD_IN_HEADERS);
  SCOPED_TRACE(
      "Test a confusing and ambiguous case: is it a line continuation or a new "
      "header field?");
  WhitespaceHeaderTestHelper(
      "GET / HTTP/1.1\r\n"
      "test: test\r\n"
      "  confusing:continued\r\n"
      "\r\n",
      true, BalsaFrameEnums::OBS_FOLD_IN_HEADERS);
}

TEST(HTTPBalsaFrame, WhitespaceInResponsesProcessedProperly) {
  SCOPED_TRACE(
      "Test that a response header with a line with spaces and no "
      "data generates an error.");
  WhitespaceHeaderTestHelper(
      "HTTP/1.0 200 Reason\r\n"
      "  \r\nContent-Length: 0\r\n"
      "\r\n",
      false, BalsaFrameEnums::INVALID_HEADER_NAME_CHARACTER);

  SCOPED_TRACE("Test proper handling for line continuation in responses.");
  WhitespaceHeaderTestHelper(
      "HTTP/1.0 200 Reason\r\n"
      "test: test\r\n"
      " continued\r\n"
      "Content-Length: 0\r\n"
      "\r\n",
      false, BalsaFrameEnums::OBS_FOLD_IN_HEADERS);
  WhitespaceHeaderTestHelper(
      "HTTP/1.0 200 Reason\r\n"
      "test: test\r\n"
      " \r\n"
      "Content-Length: 0\r\n"
      "\r\n",
      false, BalsaFrameEnums::OBS_FOLD_IN_HEADERS);
  SCOPED_TRACE(
      "Test a confusing and ambiguous case: is it a line continuation or a new "
      "header field?");
  WhitespaceHeaderTestHelper(
      "HTTP/1.0 200 Reason\r\n"
      "test: test\r\n"
      "   confusing:continued\r\n"
      "Content-Length: 0\r\n"
      "\r\n",
      false, BalsaFrameEnums::OBS_FOLD_IN_HEADERS);
}

TEST_F(HTTPBalsaFrameTest, VisitorInvokedProperlyForTrivialRequest) {
  std::string message = "GET /foobar HTTP/1.0\r\n\n";

  FakeHeaders fake_headers;

  {
    InSequence s;

    EXPECT_CALL(visitor_mock_,
                OnRequestFirstLineInput("GET /foobar HTTP/1.0", "GET",
                                        "/foobar", "HTTP/1.0"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(message));

  ASSERT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
}

TEST_F(HTTPBalsaFrameTest, VisitorInvokedProperlyForRequestWithBlankLines) {
  std::string message = "\n\n\r\n\nGET /foobar HTTP/1.0\r\n\n";

  FakeHeaders fake_headers;

  {
    InSequence s1;
    // Yes, that is correct-- the framer 'eats' the blank-lines at the beginning
    // and never notifies the visitor.

    EXPECT_CALL(visitor_mock_,
                OnRequestFirstLineInput("GET /foobar HTTP/1.0", "GET",
                                        "/foobar", "HTTP/1.0"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput("GET /foobar HTTP/1.0\r\n\n"));

  ASSERT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
}

TEST_F(HTTPBalsaFrameTest,
       VisitorInvokedProperlyForRequestWithSplitBlankLines) {
  std::string blanks =
      "\n"
      "\n"
      "\r\n"
      "\n";
  std::string header_input = "GET /foobar HTTP/1.0\r\n\n";

  FakeHeaders fake_headers;

  {
    InSequence s1;
    // Yes, that is correct-- the framer 'eats' the blank-lines at the beginning
    // and never notifies the visitor.

    EXPECT_CALL(visitor_mock_,
                OnRequestFirstLineInput("GET /foobar HTTP/1.0", "GET",
                                        "/foobar", "HTTP/1.0"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput("GET /foobar HTTP/1.0\r\n\n"));

  ASSERT_EQ(blanks.size(),
            balsa_frame_.ProcessInput(blanks.data(), blanks.size()));
  ASSERT_EQ(header_input.size(), balsa_frame_.ProcessInput(
                                     header_input.data(), header_input.size()));
}

TEST_F(HTTPBalsaFrameTest,
       VisitorInvokedProperlyForRequestWithZeroContentLength) {
  std::string message =
      "PUT /search?q=fo HTTP/1.1\n"
      "content-length:      0  \n"
      "\n";

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("content-length", "0");

  {
    InSequence s1;

    EXPECT_CALL(visitor_mock_,
                OnRequestFirstLineInput("PUT /search?q=fo HTTP/1.1", "PUT",
                                        "/search?q=fo", "HTTP/1.1"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(message));

  ASSERT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
}

TEST_F(HTTPBalsaFrameTest,
       VisitorInvokedProperlyForRequestWithMissingContentLength) {
  std::string message =
      "PUT /search?q=fo HTTP/1.1\n"
      "\n";

  auto error_code =
      BalsaFrameEnums::BalsaFrameEnums::REQUIRED_BODY_BUT_NO_CONTENT_LENGTH;
  EXPECT_CALL(visitor_mock_, HandleError(error_code));

  balsa_frame_.ProcessInput(message.data(), message.size());
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(error_code, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, ContentLengthNotRequired) {
  HttpValidationPolicy http_validation_policy;
  http_validation_policy.require_content_length_if_body_required = false;
  balsa_frame_.set_http_validation_policy(http_validation_policy);

  std::string message =
      "PUT /search?q=fo HTTP/1.1\n"
      "\n";

  balsa_frame_.ProcessInput(message.data(), message.size());
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
}

TEST_F(HTTPBalsaFrameTest,
       VisitorInvokedProperlyForPermittedMissingContentLength) {
  std::string message =
      "PUT /search?q=fo HTTP/1.1\n"
      "\n";

  FakeHeaders fake_headers;

  {
    InSequence s1;

    EXPECT_CALL(visitor_mock_,
                OnRequestFirstLineInput("PUT /search?q=fo HTTP/1.1", "PUT",
                                        "/search?q=fo", "HTTP/1.1"));
  }
  ASSERT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
}

TEST_F(HTTPBalsaFrameTest, NothingBadHappensWhenNothingInConnectionLine) {
  // This is similar to the test above, but we use different whitespace
  // throughout.
  std::string message =
      "PUT \t /search?q=fo \t HTTP/1.1 \t \r\n"
      "Connection:\r\n"
      "content-length: 0\r\n"
      "\r\n";

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("Connection", "");
  fake_headers.AddKeyValue("content-length", "0");

  {
    InSequence s1;

    EXPECT_CALL(visitor_mock_,
                OnRequestFirstLineInput("PUT \t /search?q=fo \t HTTP/1.1",
                                        "PUT", "/search?q=fo", "HTTP/1.1"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(message));

  ASSERT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
}

TEST_F(HTTPBalsaFrameTest, NothingBadHappensWhenOnlyCommentsInConnectionLine) {
  // This is similar to the test above, but we use different whitespace
  // throughout.
  std::string message =
      "PUT \t /search?q=fo \t HTTP/1.1 \t \r\n"
      "Connection: ,,,,,,,,\r\n"
      "content-length: 0\r\n"
      "\r\n";

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("Connection", ",,,,,,,,");
  fake_headers.AddKeyValue("content-length", "0");

  {
    InSequence s1;

    EXPECT_CALL(visitor_mock_,
                OnRequestFirstLineInput("PUT \t /search?q=fo \t HTTP/1.1",
                                        "PUT", "/search?q=fo", "HTTP/1.1"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(message));

  ASSERT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
}

TEST_F(HTTPBalsaFrameTest,
       VisitorInvokedProperlyForRequestWithZeroContentLengthMk2) {
  // This is similar to the test above, but we use different whitespace
  // throughout.
  std::string message =
      "PUT \t /search?q=fo \t HTTP/1.1 \t \r\n"
      "Connection:      \t close      \t\r\n"
      "content-length:  \t\t   0 \t\t  \r\n"
      "\r\n";

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("Connection", "close");
  fake_headers.AddKeyValue("content-length", "0");

  {
    InSequence s1;

    EXPECT_CALL(visitor_mock_,
                OnRequestFirstLineInput("PUT \t /search?q=fo \t HTTP/1.1",
                                        "PUT", "/search?q=fo", "HTTP/1.1"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(message));

  ASSERT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
}

TEST_F(HTTPBalsaFrameTest, NothingBadHappensWhenNoVisitorIsAssigned) {
  std::string headers =
      "GET / HTTP/1.1\r\n"
      "Connection: close\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";

  std::string chunks =
      "3\r\n"
      "123\r\n"
      "0\r\n";
  std::string trailer =
      "crass: monkeys\r\n"
      "funky: monkeys\r\n"
      "\r\n";

  balsa_frame_.set_balsa_visitor(nullptr);
  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));
  ASSERT_EQ(chunks.size(),
            balsa_frame_.ProcessInput(chunks.data(), chunks.size()));
  EXPECT_EQ(trailer.size(),
            balsa_frame_.ProcessInput(trailer.data(), trailer.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, RequestWithTrailers) {
  std::string headers =
      "GET / HTTP/1.1\r\n"
      "Connection: close\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";

  std::string chunks =
      "3\r\n"
      "123\r\n"
      "0\r\n";
  std::string trailer =
      "crass: monkeys\r\n"
      "funky: monkeys\r\n"
      "\r\n";

  InSequence s;

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("Connection", "close");
  fake_headers.AddKeyValue("transfer-encoding", "chunked");
  EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));
  testing::Mock::VerifyAndClearExpectations(&visitor_mock_);

  ASSERT_EQ(chunks.size(),
            balsa_frame_.ProcessInput(chunks.data(), chunks.size()));

  FakeHeaders fake_trailers;
  fake_trailers.AddKeyValue("crass", "monkeys");
  fake_trailers.AddKeyValue("funky", "monkeys");
  EXPECT_CALL(visitor_mock_, OnTrailers(fake_trailers));

  EXPECT_CALL(visitor_mock_, OnTrailerInput(_)).Times(AtLeast(1));

  EXPECT_EQ(trailer.size(),
            balsa_frame_.ProcessInput(trailer.data(), trailer.size()));

  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, NothingBadHappensWhenNoVisitorIsAssignedInResponse) {
  std::string headers =
      "HTTP/1.1 502 Bad Gateway\r\n"
      "Connection: close\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";

  std::string chunks =
      "3\r\n"
      "123\r\n"
      "0\r\n";
  std::string trailer =
      "crass: monkeys\r\n"
      "funky: monkeys\r\n"
      "\r\n";
  balsa_frame_.set_is_request(false);
  balsa_frame_.set_balsa_visitor(nullptr);

  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));
  ASSERT_EQ(chunks.size(),
            balsa_frame_.ProcessInput(chunks.data(), chunks.size()));
  EXPECT_EQ(trailer.size(),
            balsa_frame_.ProcessInput(trailer.data(), trailer.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, TransferEncodingIdentityIsIgnored) {
  std::string headers =
      "GET / HTTP/1.1\r\n"
      "Connection: close\r\n"
      "transfer-encoding: identity\r\n"
      "content-length: 10\r\n"
      "\r\n";

  std::string body = "1234567890";
  std::string message = (headers + body);

  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  ASSERT_EQ(body.size(), balsa_frame_.ProcessInput(body.data(), body.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest,
       NothingBadHappensWhenAVisitorIsChangedToNULLInMidParsing) {
  std::string headers =
      "GET / HTTP/1.1\r\n"
      "Connection: close\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";

  std::string chunks =
      "3\r\n"
      "123\r\n"
      "0\r\n";
  std::string trailer =
      "crass: monkeys\r\n"
      "funky: monkeys\r\n"
      "\n";

  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));
  balsa_frame_.set_balsa_visitor(nullptr);
  ASSERT_EQ(chunks.size(),
            balsa_frame_.ProcessInput(chunks.data(), chunks.size()));
  ASSERT_EQ(trailer.size(),
            balsa_frame_.ProcessInput(trailer.data(), trailer.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest,
       NothingBadHappensWhenAVisitorIsChangedToNULLInMidParsingInTrailer) {
  std::string headers =
      "HTTP/1.1 503 Server Not Available\r\n"
      "Connection: close\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";

  std::string chunks =
      "3\r\n"
      "123\r\n"
      "0\r\n";
  std::string trailer =
      "crass: monkeys\r\n"
      "funky: monkeys\r\n"
      "\n";

  balsa_frame_.set_is_request(false);

  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));
  balsa_frame_.set_balsa_visitor(nullptr);
  ASSERT_EQ(chunks.size(),
            balsa_frame_.ProcessInput(chunks.data(), chunks.size()));
  ASSERT_EQ(trailer.size(),
            balsa_frame_.ProcessInput(trailer.data(), trailer.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest,
       NothingBadHappensWhenNoVisitorAssignedAndChunkingErrorOccurs) {
  std::string headers =
      "GET / HTTP/1.1\r\n"
      "Connection: close\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";

  std::string chunks =
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\r\n"  // should overflow
      "0\r\n";
  std::string trailer =
      "crass: monkeys\r\n"
      "funky: monkeys\r\n"
      "\n";

  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));
  balsa_frame_.set_balsa_visitor(nullptr);
  EXPECT_GE(chunks.size(),
            balsa_frame_.ProcessInput(chunks.data(), chunks.size()));
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::CHUNK_LENGTH_OVERFLOW, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, FramerRecognizesSemicolonAsChunkSizeDelimiter) {
  std::string headers =
      "GET / HTTP/1.1\r\n"
      "Connection: close\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";

  std::string chunks =
      "8; foo=bar\r\n"
      "deadbeef\r\n"
      "0\r\n"
      "\r\n";

  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));

  balsa_frame_.set_balsa_visitor(&visitor_mock_);
  EXPECT_CALL(visitor_mock_, OnChunkLength(8));
  EXPECT_CALL(visitor_mock_, OnChunkLength(0));
  EXPECT_CALL(visitor_mock_, OnChunkExtensionInput("; foo=bar"));
  EXPECT_CALL(visitor_mock_, OnChunkExtensionInput(""));

  EXPECT_EQ(chunks.size(),
            balsa_frame_.ProcessInput(chunks.data(), chunks.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
}

TEST_F(HTTPBalsaFrameTest, NonAsciiCharacterInChunkLength) {
  std::string headers =
      "GET / HTTP/1.1\r\n"
      "Connection: close\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";

  std::string chunks =
      "555\xAB\r\n"  // Character overflowing 7 bits, see b/20238315
      "0\r\n";
  std::string trailer =
      "crass: monkeys\r\n"
      "funky: monkeys\r\n"
      "\n";

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("Connection", "close");
  fake_headers.AddKeyValue("transfer-encoding", "chunked");

  auto error_code = BalsaFrameEnums::INVALID_CHUNK_LENGTH;
  {
    InSequence s1;
    EXPECT_CALL(visitor_mock_, OnRequestFirstLineInput("GET / HTTP/1.1", "GET",
                                                       "/", "HTTP/1.1"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, OnRawBodyInput("555\xAB"));
    EXPECT_CALL(visitor_mock_, HandleError(error_code));
  }

  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));
  EXPECT_EQ(strlen("555\xAB"),
            balsa_frame_.ProcessInput(chunks.data(), chunks.size()));
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::INVALID_CHUNK_LENGTH, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, VisitorCalledAsExpectedWhenChunkingOverflowOccurs) {
  std::string headers =
      "GET / HTTP/1.1\r\n"
      "Connection: close\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";

  std::string chunks =
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\r\n"  // should overflow
      "0\r\n";
  std::string trailer =
      "crass: monkeys\r\n"
      "funky: monkeys\r\n"
      "\n";

  const char* chunk_read_before_overflow = "FFFFFFFFFFFFFFFFF";

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("Connection", "close");
  fake_headers.AddKeyValue("transfer-encoding", "chunked");

  auto error_code = BalsaFrameEnums::CHUNK_LENGTH_OVERFLOW;
  {
    InSequence s1;
    EXPECT_CALL(visitor_mock_, OnRequestFirstLineInput("GET / HTTP/1.1", "GET",
                                                       "/", "HTTP/1.1"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, OnRawBodyInput(chunk_read_before_overflow));
    EXPECT_CALL(visitor_mock_, HandleError(error_code));
  }

  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));
  EXPECT_EQ(strlen(chunk_read_before_overflow),
            balsa_frame_.ProcessInput(chunks.data(), chunks.size()));
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::CHUNK_LENGTH_OVERFLOW, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest,
       VisitorCalledAsExpectedWhenInvalidChunkLengthOccurs) {
  std::string headers =
      "GET / HTTP/1.1\r\n"
      "Connection: close\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";

  std::string chunks =
      "12z123 \r\n"  // invalid chunk length
      "0\r\n";
  std::string trailer =
      "crass: monkeys\r\n"
      "funky: monkeys\r\n"
      "\n";

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("Connection", "close");
  fake_headers.AddKeyValue("transfer-encoding", "chunked");

  auto error_code = BalsaFrameEnums::INVALID_CHUNK_LENGTH;
  {
    InSequence s1;
    EXPECT_CALL(visitor_mock_, OnRequestFirstLineInput("GET / HTTP/1.1", "GET",
                                                       "/", "HTTP/1.1"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, OnRawBodyInput("12z"));
    EXPECT_CALL(visitor_mock_, HandleError(error_code));
  }

  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));
  EXPECT_EQ(3u, balsa_frame_.ProcessInput(chunks.data(), chunks.size()));
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::INVALID_CHUNK_LENGTH, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, VisitorInvokedProperlyForRequestWithContentLength) {
  std::string message_headers =
      "PUT \t /search?q=fo \t HTTP/1.1 \t \r\n"
      "content-length:  \t\t   20 \t\t  \r\n"
      "\r\n";
  std::string message_body = "12345678901234567890";
  std::string message =
      std::string(message_headers) + std::string(message_body);

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("content-length", "20");

  {
    InSequence s1;
    EXPECT_CALL(visitor_mock_,
                OnRequestFirstLineInput("PUT \t /search?q=fo \t HTTP/1.1",
                                        "PUT", "/search?q=fo", "HTTP/1.1"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, OnRawBodyInput(message_body));
    EXPECT_CALL(visitor_mock_, OnBodyChunkInput(message_body));
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(message_headers));

  ASSERT_EQ(message_headers.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  ASSERT_EQ(message_body.size(),
            balsa_frame_.ProcessInput(message.data() + message_headers.size(),
                                      message.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest,
       VisitorInvokedProperlyForRequestWithOneCharContentLength) {
  std::string message_headers =
      "PUT \t /search?q=fo \t HTTP/1.1 \t \r\n"
      "content-length:  \t\t   2 \t\t  \r\n"
      "\r\n";
  std::string message_body = "12";
  std::string message =
      std::string(message_headers) + std::string(message_body);

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("content-length", "2");

  {
    InSequence s1;
    EXPECT_CALL(visitor_mock_,
                OnRequestFirstLineInput("PUT \t /search?q=fo \t HTTP/1.1",
                                        "PUT", "/search?q=fo", "HTTP/1.1"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, OnRawBodyInput(message_body));
    EXPECT_CALL(visitor_mock_, OnBodyChunkInput(message_body));
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(message_headers));

  ASSERT_EQ(message_headers.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  ASSERT_EQ(message_body.size(),
            balsa_frame_.ProcessInput(message.data() + message_headers.size(),
                                      message.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, InvalidChunkExtensionWithCarriageReturn) {
  balsa_frame_.set_http_validation_policy(
      HttpValidationPolicy{.disallow_lone_cr_in_chunk_extension = true});
  std::string message_headers =
      "POST /potato?salad=withmayo HTTP/1.1\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";
  std::string message_body =
      "9; bad\rextension\r\n"
      "012345678\r\n"
      "0\r\n"
      "\r\n";
  std::string message =
      std::string(message_headers) + std::string(message_body);

  EXPECT_CALL(visitor_mock_,
              HandleError(BalsaFrameEnums::INVALID_CHUNK_EXTENSION));
  ASSERT_EQ(message_headers.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  balsa_frame_.ProcessInput(message.data() + message_headers.size(),
                            message.size());
}

// Regression test for b/347710034: `disallow_lone_cr_in_chunk_extension` should
// not trigger false positive when "\r\n" terminating chunk length is separated
// into multiple calls to ProcessInput().
TEST_F(HTTPBalsaFrameTest, ChunkExtensionCarriageReturnLineFeedAtBoundary) {
  balsa_frame_.set_http_validation_policy(
      HttpValidationPolicy{.disallow_lone_cr_in_chunk_extension = true});
  EXPECT_CALL(visitor_mock_, ProcessHeaders(_));
  EXPECT_CALL(visitor_mock_, HeaderDone());
  constexpr absl::string_view headers(
      "POST / HTTP/1.1\r\n"
      "transfer-encoding: chunked\r\n\r\n");
  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));

  constexpr absl::string_view body1("3\r");
  ASSERT_EQ(body1.size(),
            balsa_frame_.ProcessInput(body1.data(), body1.size()));
  ASSERT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());

  constexpr absl::string_view body2(
      "\nfoo\r\n"
      "0\r\n\r\n");

  EXPECT_CALL(visitor_mock_, OnBodyChunkInput("foo"));
  EXPECT_CALL(visitor_mock_, MessageDone());
  ASSERT_EQ(body2.size(),
            balsa_frame_.ProcessInput(body2.data(), body2.size()));

  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
}

// A CR character followed by a non-LF character is detected even if separated
// into multiple calls to ProcessInput().
TEST_F(HTTPBalsaFrameTest, ChunkExtensionLoneCarriageReturnAtBoundary) {
  balsa_frame_.set_http_validation_policy(
      HttpValidationPolicy{.disallow_lone_cr_in_chunk_extension = true});
  EXPECT_CALL(visitor_mock_, ProcessHeaders(_));
  EXPECT_CALL(visitor_mock_, HeaderDone());
  constexpr absl::string_view headers(
      "POST / HTTP/1.1\r\n"
      "transfer-encoding: chunked\r\n\r\n");
  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));

  constexpr absl::string_view body1("3\r");
  ASSERT_EQ(body1.size(),
            balsa_frame_.ProcessInput(body1.data(), body1.size()));
  ASSERT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());

  constexpr absl::string_view body2("a");
  EXPECT_EQ(0, balsa_frame_.ProcessInput(body2.data(), body2.size()));
  EXPECT_EQ(BalsaFrameEnums::INVALID_CHUNK_EXTENSION, balsa_frame_.ErrorCode());
}

// Regression test for chunk extension sanitization.
TEST_F(HTTPBalsaFrameTest, InvalidChunkExtensionWithLineFeedAllowed) {
  balsa_frame_.set_http_validation_policy(
      HttpValidationPolicy{.disallow_lone_lf_in_chunk_extension = false});
  std::string message_headers =
      "POST /potato?salad=withmayo HTTP/1.1\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";
  std::string message_body =
      "9; bad\nextension\r\n"
      "012345678\r\n"
      "0\r\n"
      "\r\n";
  std::string message =
      std::string(message_headers) + std::string(message_body);

  EXPECT_CALL(visitor_mock_,
              HandleError(BalsaFrameEnums::INVALID_CHUNK_EXTENSION))
      .Times(0);  // Bug! b/380075645
  ASSERT_EQ(message_headers.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  balsa_frame_.ProcessInput(message.data() + message_headers.size(),
                            message.size());
}

// A LF character preceded by CR is allowed even if separated into multiple
// calls to ProcessInput().
TEST_F(HTTPBalsaFrameTest,
       ChunkExtensionLoneCarriageReturnAtBoundaryWithLineFeed) {
  balsa_frame_.set_http_validation_policy(
      HttpValidationPolicy{.disallow_lone_lf_in_chunk_extension = true});
  EXPECT_CALL(visitor_mock_, ProcessHeaders(_));
  EXPECT_CALL(visitor_mock_, HeaderDone());
  constexpr absl::string_view headers(
      "POST / HTTP/1.1\r\n"
      "transfer-encoding: chunked\r\n\r\n");
  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));

  constexpr absl::string_view body1("3\r");
  ASSERT_EQ(body1.size(),
            balsa_frame_.ProcessInput(body1.data(), body1.size()));
  ASSERT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());

  constexpr absl::string_view body2("\n");
  EXPECT_EQ(body2.size(),
            balsa_frame_.ProcessInput(body2.data(), body2.size()));
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, InvalidChunkExtensionWithLineFeedRejected) {
  balsa_frame_.set_http_validation_policy(
      HttpValidationPolicy{.disallow_lone_lf_in_chunk_extension = true});
  std::string message_headers =
      "POST /potato?salad=withmayo HTTP/1.1\r\n"
      "transfer-encoding: chunked\r\n\r\n";
  std::string message_body =
      "9; bad\nextension\r\n"
      "012345678\r\n"
      "0\r\n"
      "\r\n";
  const std::string message = absl::StrCat(message_headers, message_body);

  EXPECT_CALL(visitor_mock_,
              HandleError(BalsaFrameEnums::INVALID_CHUNK_EXTENSION))
      .Times(1);
  ASSERT_EQ(message_headers.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  balsa_frame_.ProcessInput(message.data() + message_headers.size(),
                            message.size());
}

TEST_F(HTTPBalsaFrameTest,
       VisitorInvokedProperlyForRequestWithTransferEncoding) {
  std::string message_headers =
      "DELETE /search?q=fo \t HTTP/1.1 \t \r\n"
      "trAnsfer-eNcoding:  chunked\r\n"
      "\r\n";
  std::string message_body =
      "A            chunkjed extension  \r\n"
      "01234567890            more crud including numbers 123123\r\n"
      "3f\r\n"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
      "0 last one\r\n"
      "\r\n";
  std::string message_body_data =
      "0123456789"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

  std::string message =
      std::string(message_headers) + std::string(message_body);

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("trAnsfer-eNcoding", "chunked");

  {
    InSequence s1;
    EXPECT_CALL(visitor_mock_,
                OnRequestFirstLineInput("DELETE /search?q=fo \t HTTP/1.1",
                                        "DELETE", "/search?q=fo", "HTTP/1.1"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, OnChunkLength(10));
    EXPECT_CALL(visitor_mock_,
                OnChunkExtensionInput("            chunkjed extension  "));
    EXPECT_CALL(visitor_mock_, OnChunkLength(63));
    EXPECT_CALL(visitor_mock_, OnChunkExtensionInput(""));
    EXPECT_CALL(visitor_mock_, OnChunkLength(0));
    EXPECT_CALL(visitor_mock_, OnChunkExtensionInput(" last one"));
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(message_headers));
  std::string body_input;
  EXPECT_CALL(visitor_mock_, OnRawBodyInput(_))
      .WillRepeatedly([&body_input](absl::string_view input) {
        absl::StrAppend(&body_input, input);
      });
  std::string body_data;
  EXPECT_CALL(visitor_mock_, OnBodyChunkInput(_))
      .WillRepeatedly([&body_data](absl::string_view input) {
        absl::StrAppend(&body_data, input);
      });
  EXPECT_CALL(visitor_mock_, OnTrailerInput(_)).Times(0);

  ASSERT_EQ(message_headers.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_EQ(message_body.size(),
            balsa_frame_.ProcessInput(message.data() + message_headers.size(),
                                      message.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());

  EXPECT_EQ(message_body, body_input);
  EXPECT_EQ(message_body_data, body_data);
}

// Validates that chunked requests terminated by \r\n\n are accepted.
// Note that this does not comply with
// https://datatracker.ietf.org/doc/html/rfc9112#name-chunked-transfer-coding
// which states that both `last-chunk` _and_ `chunked_body` must end with CR_LF.
TEST_F(HTTPBalsaFrameTest,
       TransferEncodingChunkedFramesMessagesEndingWithCR_LF_LF) {
  EXPECT_CALL(visitor_mock_, ProcessHeaders(_));
  EXPECT_CALL(visitor_mock_, HeaderDone());
  const std::string message1 =
      "POST / HTTP/1.1\r\n"
      "Host: 1.1.1.1\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n";
  EXPECT_EQ(message1.size(),
            balsa_frame_.ProcessInput(message1.data(), message1.size()));
  EXPECT_FALSE(balsa_frame_.Error())
      << BalsaFrameEnums::ErrorCodeToString(balsa_frame_.ErrorCode());

  const std::string chunk_size = "2\r\n";
  EXPECT_EQ(chunk_size.size(),
            balsa_frame_.ProcessInput(chunk_size.data(), chunk_size.size()));
  EXPECT_FALSE(balsa_frame_.Error())
      << BalsaFrameEnums::ErrorCodeToString(balsa_frame_.ErrorCode());

  EXPECT_CALL(visitor_mock_, OnBodyChunkInput("AA"));
  EXPECT_CALL(visitor_mock_, MessageDone());
  const std::string chunks =
      "AA\r\n"
      "0\r\n"
      "\n";
  ASSERT_EQ(chunks.size(),
            balsa_frame_.ProcessInput(chunks.data(), chunks.size()));

  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
  // According to the RFC, this should be false!
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
}

TEST_F(
    HTTPBalsaFrameTest,
    TransferEncodingChunkedFramesMessagesEndingWithCR_LF_LFFailsWhenPolicySet) {
  HttpValidationPolicy http_validation_policy{
      .require_chunked_body_end_with_crlf_crlf = true};
  balsa_frame_.set_http_validation_policy(http_validation_policy);
  EXPECT_CALL(visitor_mock_, ProcessHeaders(_));
  EXPECT_CALL(visitor_mock_, HeaderDone());
  const std::string message1 =
      "POST / HTTP/1.1\r\n"
      "Host: 1.1.1.1\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n";
  EXPECT_EQ(message1.size(),
            balsa_frame_.ProcessInput(message1.data(), message1.size()));
  EXPECT_FALSE(balsa_frame_.Error())
      << BalsaFrameEnums::ErrorCodeToString(balsa_frame_.ErrorCode());

  const std::string chunk_size = "2\r\n";
  EXPECT_EQ(chunk_size.size(),
            balsa_frame_.ProcessInput(chunk_size.data(), chunk_size.size()));
  EXPECT_FALSE(balsa_frame_.Error())
      << BalsaFrameEnums::ErrorCodeToString(balsa_frame_.ErrorCode());

  EXPECT_CALL(visitor_mock_, OnBodyChunkInput("AA"));
  EXPECT_CALL(visitor_mock_, MessageDone()).Times(0);
  const std::string chunks =
      "AA\r\n"
      "0\r\n"
      "\n";
  ASSERT_EQ(chunks.size(),
            balsa_frame_.ProcessInput(chunks.data(), chunks.size()));

  EXPECT_EQ(BalsaFrameEnums::INVALID_CHUNK_FRAMING, balsa_frame_.ErrorCode());
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
}

TEST_F(HTTPBalsaFrameTest, FirstlinesWithMultipleSpacesAllowed) {
  HttpValidationPolicy http_validation_policy{
      .sanitize_firstline_spaces =
          HttpValidationPolicy::FirstLineValidationOption::NONE};
  balsa_frame_.set_http_validation_policy(http_validation_policy);
  EXPECT_CALL(visitor_mock_, ProcessHeaders(_));
  EXPECT_CALL(visitor_mock_, HeaderDone());
  const std::string message1 =
      "GET  / HTTP/1.1\r\n"
      "Host: 1.1.1.1\r\n"
      "\r\n";
  EXPECT_EQ(message1.size(),
            balsa_frame_.ProcessInput(message1.data(), message1.size()));
  EXPECT_FALSE(balsa_frame_.Error())
      << BalsaFrameEnums::ErrorCodeToString(balsa_frame_.ErrorCode());

  EXPECT_EQ("GET  / HTTP/1.1", headers_.first_line());
}

TEST_F(HTTPBalsaFrameTest, FirstlinesWithMultipleSpacesRejected) {
  HttpValidationPolicy http_validation_policy{
      .sanitize_firstline_spaces =
          HttpValidationPolicy::FirstLineValidationOption::REJECT};
  balsa_frame_.set_http_validation_policy(http_validation_policy);
  EXPECT_CALL(visitor_mock_, ProcessHeaders(_)).Times(0);
  EXPECT_CALL(visitor_mock_, HeaderDone()).Times(0);
  const std::string message1 =
      "GET  / HTTP/1.1\r\n"
      "Host: 1.1.1.1\r\n"
      "\r\n";
  EXPECT_GT(message1.size(),
            balsa_frame_.ProcessInput(message1.data(), message1.size()));
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::MULTIPLE_SPACES_IN_REQUEST_LINE,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, FirstlinesWithMultipleSpacesSanitized) {
  HttpValidationPolicy http_validation_policy{
      .sanitize_firstline_spaces =
          HttpValidationPolicy::FirstLineValidationOption::SANITIZE};
  balsa_frame_.set_http_validation_policy(http_validation_policy);
  EXPECT_CALL(visitor_mock_, ProcessHeaders(_));
  EXPECT_CALL(visitor_mock_, HeaderDone());
  const std::string message1 =
      "GET  / HTTP/1.1\r\n"
      "Host: 1.1.1.1\r\n"
      "\r\n";
  EXPECT_EQ(message1.size(),
            balsa_frame_.ProcessInput(message1.data(), message1.size()));
  EXPECT_FALSE(balsa_frame_.Error())
      << BalsaFrameEnums::ErrorCodeToString(balsa_frame_.ErrorCode());

  EXPECT_EQ("GET / HTTP/1.1", headers_.first_line());
}

TEST_F(HTTPBalsaFrameTest, ResponseFirstlinesWithMultipleSpacesAllowed) {
  HttpValidationPolicy http_validation_policy{
      .sanitize_firstline_spaces =
          HttpValidationPolicy::FirstLineValidationOption::NONE};
  balsa_frame_.set_http_validation_policy(http_validation_policy);
  EXPECT_CALL(visitor_mock_, ProcessHeaders(_));
  EXPECT_CALL(visitor_mock_, HeaderDone());
  const std::string message1 =
      "HTTP/1.1 200  OK\r\n"
      "Content-Type: text/html\r\n"
      "\r\n";
  EXPECT_EQ(message1.size(),
            balsa_frame_.ProcessInput(message1.data(), message1.size()));
  EXPECT_FALSE(balsa_frame_.Error())
      << BalsaFrameEnums::ErrorCodeToString(balsa_frame_.ErrorCode());

  EXPECT_EQ("HTTP/1.1 200  OK", headers_.first_line());
}

TEST_F(HTTPBalsaFrameTest, ResponseFirstlinesWithMultipleSpacesRejected) {
  HttpValidationPolicy http_validation_policy{
      .sanitize_firstline_spaces =
          HttpValidationPolicy::FirstLineValidationOption::REJECT};
  balsa_frame_.set_http_validation_policy(http_validation_policy);
  EXPECT_CALL(visitor_mock_, ProcessHeaders(_)).Times(0);
  EXPECT_CALL(visitor_mock_, HeaderDone()).Times(0);
  const std::string message1 =
      "HTTP/1.1 200  OK\r\n"
      "Content-Type: text/html\r\n"
      "\r\n";
  EXPECT_GT(message1.size(),
            balsa_frame_.ProcessInput(message1.data(), message1.size()));
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::MULTIPLE_SPACES_IN_REQUEST_LINE,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, ResponseFirstlinesWithMultipleSpacesSanitized) {
  HttpValidationPolicy http_validation_policy{
      .sanitize_firstline_spaces =
          HttpValidationPolicy::FirstLineValidationOption::SANITIZE};
  balsa_frame_.set_http_validation_policy(http_validation_policy);
  EXPECT_CALL(visitor_mock_, ProcessHeaders(_));
  EXPECT_CALL(visitor_mock_, HeaderDone());
  const std::string message1 =
      "HTTP/1.1 200  OK\r\n"
      "Content-Type: text/html\r\n"
      "\r\n";
  EXPECT_EQ(message1.size(),
            balsa_frame_.ProcessInput(message1.data(), message1.size()));
  EXPECT_FALSE(balsa_frame_.Error())
      << BalsaFrameEnums::ErrorCodeToString(balsa_frame_.ErrorCode());

  EXPECT_EQ("HTTP/1.1 200 OK", headers_.first_line());
}

TEST_F(HTTPBalsaFrameTest,
       VisitorInvokedProperlyForRequestWithTransferEncodingAndTrailers) {
  std::string message_headers =
      "DELETE /search?q=fo \t HTTP/1.1 \t \r\n"
      "trAnsfer-eNcoding:  chunked\r\n"
      "another_random_header:  \r\n"
      "  \t \n"
      "  \t includes a continuation\n"
      "\r\n";
  std::string message_body =
      "A            chunkjed extension  \r\n"
      "01234567890            more crud including numbers 123123\r\n"
      "3f\r\n"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
      "1  \r\n"
      "x   \r\n"
      "0 last one\r\n";
  std::string trailer_data =
      "a_trailer_key: and a trailer value\r\n"
      "\r\n";
  std::string message_body_data =
      "0123456789"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

  std::string message = (std::string(message_headers) +
                         std::string(message_body) + std::string(trailer_data));

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("trAnsfer-eNcoding", "chunked");
  fake_headers.AddKeyValue("another_random_header", "includes a continuation");

  FakeHeaders fake_trailers;
  fake_trailers.AddKeyValue("a_trailer_key", "and a trailer value");

  {
    InSequence s1;

    EXPECT_CALL(visitor_mock_,
                OnRequestFirstLineInput("DELETE /search?q=fo \t HTTP/1.1",
                                        "DELETE", "/search?q=fo", "HTTP/1.1"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, OnChunkLength(10));
    EXPECT_CALL(visitor_mock_, OnChunkLength(63));
    EXPECT_CALL(visitor_mock_, OnChunkLength(1));
    EXPECT_CALL(visitor_mock_, OnChunkLength(0));
    EXPECT_CALL(visitor_mock_, OnTrailers(fake_trailers));
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(message_headers));
  std::string body_input;
  EXPECT_CALL(visitor_mock_, OnRawBodyInput(_))
      .WillRepeatedly([&body_input](absl::string_view input) {
        absl::StrAppend(&body_input, input);
      });
  std::string body_data;
  EXPECT_CALL(visitor_mock_, OnBodyChunkInput(_))
      .WillRepeatedly([&body_data](absl::string_view input) {
        absl::StrAppend(&body_data, input);
      });
  EXPECT_CALL(visitor_mock_, OnTrailerInput(trailer_data));
  EXPECT_CALL(visitor_mock_, OnChunkExtensionInput(_)).Times(AnyNumber());

  ASSERT_EQ(message_headers.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_EQ(message_body.size() + trailer_data.size(),
            balsa_frame_.ProcessInput(message.data() + message_headers.size(),
                                      message.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::OBS_FOLD_IN_HEADERS, balsa_frame_.ErrorCode());

  EXPECT_EQ(message_body, body_input);
  EXPECT_EQ(message_body_data, body_data);
}

TEST_F(HTTPBalsaFrameTest,
       VisitorInvokedProperlyWithRequestFirstLineWarningWithOnlyMethod) {
  std::string message = "GET\n";

  FakeHeaders fake_headers;

  auto error_code = BalsaFrameEnums::FAILED_TO_FIND_WS_AFTER_REQUEST_METHOD;
  {
    InSequence s;
    EXPECT_CALL(visitor_mock_, HandleWarning(error_code));
    EXPECT_CALL(visitor_mock_, OnRequestFirstLineInput("GET", "GET", "", ""));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(message));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::FAILED_TO_FIND_WS_AFTER_REQUEST_METHOD,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest,
       VisitorInvokedProperlyWithRequestFirstLineWarningWithOnlyMethodAndWS) {
  std::string message = "GET  \n";

  FakeHeaders fake_headers;

  auto error_code = BalsaFrameEnums::FAILED_TO_FIND_WS_AFTER_REQUEST_METHOD;
  {
    InSequence s;
    EXPECT_CALL(visitor_mock_, HandleWarning(error_code));
    // The flag setting here intentionally alters the framer's behavior with
    // trailing whitespace.
    EXPECT_CALL(visitor_mock_, OnRequestFirstLineInput("GET  ", "GET", "", ""));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(message));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::FAILED_TO_FIND_WS_AFTER_REQUEST_METHOD,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, AbsoluteFormTargetUri) {
  std::string message =
      "GET http://www.google.com/index.html HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "\r\n";
  balsa_frame_.set_is_request(true);

  EXPECT_CALL(visitor_mock_, OnHeaderInput(message));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
  EXPECT_EQ("http://www.google.com/index.html",
            balsa_frame_.headers()->request_uri());
  EXPECT_EQ("example.com", balsa_frame_.headers()->GetHeader("host"));
}

TEST_F(HTTPBalsaFrameTest, InvalidAbsoluteFormTargetUri) {
  std::string message =
      "GET -pwn/index.html HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "\r\n";
  balsa_frame_.set_is_request(true);

  EXPECT_CALL(visitor_mock_, OnHeaderInput(message));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.is_valid_target_uri());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
  EXPECT_EQ("-pwn/index.html", balsa_frame_.headers()->request_uri());
  EXPECT_EQ("example.com", balsa_frame_.headers()->GetHeader("host"));
}

TEST_F(HTTPBalsaFrameTest, RejectInvalidAbsoluteFormTargetUri) {
  HttpValidationPolicy http_validation_policy{.disallow_invalid_target_uris =
                                                  true};
  balsa_frame_.set_http_validation_policy(http_validation_policy);
  std::string message =
      "GET -pwn/index.html HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "\r\n";
  balsa_frame_.set_is_request(true);

  const size_t end_of_first_line = message.find_first_of("\r\n") + 1;
  EXPECT_EQ(end_of_first_line,
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::INVALID_TARGET_URI, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, RejectStarForNonOptions) {
  HttpValidationPolicy http_validation_policy{.disallow_invalid_target_uris =
                                                  true};
  balsa_frame_.set_http_validation_policy(http_validation_policy);
  std::string message =
      "GET * HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "\r\n";
  balsa_frame_.set_is_request(true);

  const size_t end_of_first_line = message.find_first_of("\r\n") + 1;
  EXPECT_EQ(end_of_first_line,
            balsa_frame_.ProcessInput(message.data(), message.size()));

  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::INVALID_TARGET_URI, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, AllowStarForOptions) {
  HttpValidationPolicy http_validation_policy{.disallow_invalid_target_uris =
                                                  true};
  balsa_frame_.set_http_validation_policy(http_validation_policy);
  std::string message =
      "OPTIONS * HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "\r\n";
  balsa_frame_.set_is_request(true);

  EXPECT_CALL(visitor_mock_, OnHeaderInput(message));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
}

TEST_F(HTTPBalsaFrameTest, RejectConnectWithNoPort) {
  HttpValidationPolicy http_validation_policy{.disallow_invalid_target_uris =
                                                  true};
  balsa_frame_.set_http_validation_policy(http_validation_policy);
  std::string message =
      "CONNECT example.com HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "\r\n";
  balsa_frame_.set_is_request(true);

  const size_t end_of_first_line = message.find_first_of("\r\n") + 1;
  EXPECT_EQ(end_of_first_line,
            balsa_frame_.ProcessInput(message.data(), message.size()));

  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::INVALID_TARGET_URI, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, RejectConnectWithInvalidPort) {
  HttpValidationPolicy http_validation_policy{.disallow_invalid_target_uris =
                                                  true};
  balsa_frame_.set_http_validation_policy(http_validation_policy);
  std::string message =
      "CONNECT example.com:443z HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "\r\n";
  balsa_frame_.set_is_request(true);

  const size_t end_of_first_line = message.find_first_of("\r\n") + 1;
  EXPECT_EQ(end_of_first_line,
            balsa_frame_.ProcessInput(message.data(), message.size()));

  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::INVALID_TARGET_URI, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, AllowConnectWithValidPort) {
  HttpValidationPolicy http_validation_policy{.disallow_invalid_target_uris =
                                                  true};
  balsa_frame_.set_http_validation_policy(http_validation_policy);
  std::string message =
      "CONNECT example.com:443 HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "\r\n";
  balsa_frame_.set_is_request(true);

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
}

TEST_F(HTTPBalsaFrameTest,
       VisitorInvokedProperlyWithRequestFirstLineWarningWithMethodAndURI) {
  std::string message = "GET /uri\n";

  FakeHeaders fake_headers;

  auto error_code =
      BalsaFrameEnums::FAILED_TO_FIND_WS_AFTER_REQUEST_REQUEST_URI;
  {
    InSequence s;
    EXPECT_CALL(visitor_mock_, HandleWarning(error_code));
    EXPECT_CALL(visitor_mock_,
                OnRequestFirstLineInput("GET /uri", "GET", "/uri", ""));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(message));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::FAILED_TO_FIND_WS_AFTER_REQUEST_REQUEST_URI,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, VisitorInvokedProperlyWithResponseFirstLineError) {
  std::string message = "HTTP/1.1\n\n";

  FakeHeaders fake_headers;

  balsa_frame_.set_is_request(false);
  auto error_code = BalsaFrameEnums::FAILED_TO_FIND_WS_AFTER_RESPONSE_VERSION;
  {
    InSequence s;
    EXPECT_CALL(visitor_mock_, HandleError(error_code));
    // The function returns before any of the following is called.
    EXPECT_CALL(visitor_mock_, OnRequestFirstLineInput).Times(0);
    EXPECT_CALL(visitor_mock_, ProcessHeaders(_)).Times(0);
    EXPECT_CALL(visitor_mock_, HeaderDone()).Times(0);
    EXPECT_CALL(visitor_mock_, MessageDone()).Times(0);
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(_)).Times(0);

  EXPECT_GE(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::FAILED_TO_FIND_WS_AFTER_RESPONSE_VERSION,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, FlagsErrorWithContentLengthOverflow) {
  std::string message =
      "HTTP/1.0 200 OK\r\n"
      "content-length: 9999999999999999999999999999999999999999\n"
      "\n";

  balsa_frame_.set_is_request(false);
  auto error_code = BalsaFrameEnums::UNPARSABLE_CONTENT_LENGTH;
  EXPECT_CALL(visitor_mock_, HandleError(error_code));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::UNPARSABLE_CONTENT_LENGTH,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, FlagsErrorWithInvalidResponseCode) {
  std::string message =
      "HTTP/1.0 x OK\r\n"
      "\n";

  balsa_frame_.set_is_request(false);
  auto error_code = BalsaFrameEnums::FAILED_CONVERTING_STATUS_CODE_TO_INT;
  EXPECT_CALL(visitor_mock_, HandleError(error_code));

  EXPECT_GE(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::FAILED_CONVERTING_STATUS_CODE_TO_INT,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, FlagsErrorWithOverflowingResponseCode) {
  std::string message =
      "HTTP/1.0 999999999999999999999999999999999999999 OK\r\n"
      "\n";

  balsa_frame_.set_is_request(false);
  auto error_code = BalsaFrameEnums::FAILED_CONVERTING_STATUS_CODE_TO_INT;
  EXPECT_CALL(visitor_mock_, HandleError(error_code));

  EXPECT_GE(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::FAILED_CONVERTING_STATUS_CODE_TO_INT,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, FlagsErrorWithInvalidContentLength) {
  std::string message =
      "HTTP/1.0 200 OK\r\n"
      "content-length: xxx\n"
      "\n";

  balsa_frame_.set_is_request(false);
  auto error_code = BalsaFrameEnums::UNPARSABLE_CONTENT_LENGTH;
  EXPECT_CALL(visitor_mock_, HandleError(error_code));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::UNPARSABLE_CONTENT_LENGTH,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, FlagsErrorWithNegativeContentLengthValue) {
  std::string message =
      "HTTP/1.0 200 OK\r\n"
      "content-length: -20\n"
      "\n";

  balsa_frame_.set_is_request(false);
  auto error_code = BalsaFrameEnums::UNPARSABLE_CONTENT_LENGTH;
  EXPECT_CALL(visitor_mock_, HandleError(error_code));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::UNPARSABLE_CONTENT_LENGTH,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, FlagsErrorWithEmptyContentLengthValue) {
  std::string message =
      "HTTP/1.0 200 OK\r\n"
      "content-length: \n"
      "\n";

  balsa_frame_.set_is_request(false);
  auto error_code = BalsaFrameEnums::UNPARSABLE_CONTENT_LENGTH;
  EXPECT_CALL(visitor_mock_, HandleError(error_code));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::UNPARSABLE_CONTENT_LENGTH,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, VisitorInvokedProperlyForTrivialResponse) {
  std::string message =
      "HTTP/1.0 200 OK\r\n"
      "content-length: 0\n"
      "\n";

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("content-length", "0");

  balsa_frame_.set_is_request(false);
  {
    InSequence s;
    EXPECT_CALL(visitor_mock_, OnResponseFirstLineInput(
                                   "HTTP/1.0 200 OK", "HTTP/1.0", "200", "OK"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(message));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest,
       VisitorInvokedProperlyForResponseWithSplitBlankLines) {
  std::string blanks =
      "\n"
      "\r\n"
      "\r\n";
  std::string header_input =
      "HTTP/1.0 200 OK\r\n"
      "content-length: 0\n"
      "\n";
  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("content-length", "0");

  balsa_frame_.set_is_request(false);
  {
    InSequence s;
    EXPECT_CALL(visitor_mock_, OnResponseFirstLineInput(
                                   "HTTP/1.0 200 OK", "HTTP/1.0", "200", "OK"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(header_input));

  EXPECT_EQ(blanks.size(),
            balsa_frame_.ProcessInput(blanks.data(), blanks.size()));
  EXPECT_EQ(header_input.size(), balsa_frame_.ProcessInput(
                                     header_input.data(), header_input.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, VisitorInvokedProperlyForResponseWithBlankLines) {
  std::string blanks =
      "\n"
      "\r\n"
      "\n"
      "\n"
      "\r\n"
      "\r\n";
  std::string header_input =
      "HTTP/1.0 200 OK\r\n"
      "content-length: 0\n"
      "\n";
  std::string message = blanks + header_input;

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("content-length", "0");

  balsa_frame_.set_is_request(false);
  {
    InSequence s;
    EXPECT_CALL(visitor_mock_, OnResponseFirstLineInput(
                                   "HTTP/1.0 200 OK", "HTTP/1.0", "200", "OK"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(header_input));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, VisitorInvokedProperlyForResponseWithContentLength) {
  std::string message_headers =
      "HTTP/1.1  \t 200 Ok all is well\r\n"
      "content-length:  \t\t   20 \t\t  \r\n"
      "\r\n";
  std::string message_body = "12345678901234567890";
  std::string message =
      std::string(message_headers) + std::string(message_body);

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("content-length", "20");

  balsa_frame_.set_is_request(false);
  {
    InSequence s1;
    EXPECT_CALL(visitor_mock_,
                OnResponseFirstLineInput("HTTP/1.1  \t 200 Ok all is well",
                                         "HTTP/1.1", "200", "Ok all is well"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, OnRawBodyInput(message_body));
    EXPECT_CALL(visitor_mock_, OnBodyChunkInput(message_body));
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(message_headers));

  ASSERT_EQ(message_headers.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_EQ(message_body.size(),
            balsa_frame_.ProcessInput(message.data() + message_headers.size(),
                                      message.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest,
       VisitorInvokedProperlyForResponseWithTransferEncoding) {
  std::string message_headers =
      "HTTP/1.1  \t 200 Ok all is well\r\n"
      "trAnsfer-eNcoding:  chunked\r\n"
      "\r\n";
  std::string message_body =
      "A            chunkjed extension  \r\n"
      "01234567890            more crud including numbers 123123\r\n"
      "3f\r\n"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
      "0 last one\r\n"
      "\r\n";
  std::string message_body_data =
      "0123456789"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

  std::string message =
      std::string(message_headers) + std::string(message_body);

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("trAnsfer-eNcoding", "chunked");

  balsa_frame_.set_is_request(false);
  {
    InSequence s1;
    EXPECT_CALL(visitor_mock_,
                OnResponseFirstLineInput("HTTP/1.1  \t 200 Ok all is well",
                                         "HTTP/1.1", "200", "Ok all is well"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, OnChunkLength(10));
    EXPECT_CALL(visitor_mock_, OnChunkLength(63));
    EXPECT_CALL(visitor_mock_, OnChunkLength(0));
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(message_headers));
  std::string body_input;
  EXPECT_CALL(visitor_mock_, OnRawBodyInput(_))
      .WillRepeatedly([&body_input](absl::string_view input) {
        absl::StrAppend(&body_input, input);
      });
  std::string body_data;
  EXPECT_CALL(visitor_mock_, OnBodyChunkInput(_))
      .WillRepeatedly([&body_data](absl::string_view input) {
        absl::StrAppend(&body_data, input);
      });
  EXPECT_CALL(visitor_mock_, OnTrailerInput(_)).Times(0);

  ASSERT_EQ(message_headers.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_EQ(message_body.size(),
            balsa_frame_.ProcessInput(message.data() + message_headers.size(),
                                      message.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());

  EXPECT_EQ(message_body, body_input);
  EXPECT_EQ(message_body_data, body_data);
}

TEST_F(HTTPBalsaFrameTest,
       VisitorInvokedProperlyForResponseWithTransferEncodingAndTrailers) {
  std::string message_headers =
      "HTTP/1.1  \t 200 Ok all is well\r\n"
      "trAnsfer-eNcoding:  chunked\r\n"
      "\r\n";
  std::string message_body =
      "A            chunkjed extension  \r\n"
      "01234567890            more crud including numbers 123123\r\n"
      "3f\r\n"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
      "0 last one\r\n";
  std::string trailer_data =
      "a_trailer_key: and a trailer value\r\n"
      "\r\n";
  std::string message_body_data =
      "0123456789"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

  std::string message = (std::string(message_headers) +
                         std::string(message_body) + std::string(trailer_data));

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("trAnsfer-eNcoding", "chunked");

  FakeHeaders fake_headers_in_trailer;
  fake_headers_in_trailer.AddKeyValue("a_trailer_key", "and a trailer value");

  balsa_frame_.set_is_request(false);

  {
    InSequence s1;
    EXPECT_CALL(visitor_mock_,
                OnResponseFirstLineInput("HTTP/1.1  \t 200 Ok all is well",
                                         "HTTP/1.1", "200", "Ok all is well"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, OnChunkLength(10));
    EXPECT_CALL(visitor_mock_, OnChunkLength(63));
    EXPECT_CALL(visitor_mock_, OnChunkLength(0));
    EXPECT_CALL(visitor_mock_, OnTrailers(fake_headers_in_trailer));
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(message_headers));
  std::string body_input;
  EXPECT_CALL(visitor_mock_, OnRawBodyInput(_))
      .WillRepeatedly([&body_input](absl::string_view input) {
        absl::StrAppend(&body_input, input);
      });
  std::string body_data;
  EXPECT_CALL(visitor_mock_, OnBodyChunkInput(_))
      .WillRepeatedly([&body_data](absl::string_view input) {
        absl::StrAppend(&body_data, input);
      });
  EXPECT_CALL(visitor_mock_, OnTrailerInput(trailer_data));

  ASSERT_EQ(message_headers.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_EQ(message_body.size() + trailer_data.size(),
            balsa_frame_.ProcessInput(message.data() + message_headers.size(),
                                      message.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());

  EXPECT_EQ(message_body, body_input);
  EXPECT_EQ(message_body_data, body_data);
}

TEST_F(
    HTTPBalsaFrameTest,
    VisitorInvokedProperlyForResponseWithTransferEncodingAndTrailersBytePer) {
  std::string message_headers =
      "HTTP/1.1  \t 200 Ok all is well\r\n"
      "trAnsfer-eNcoding:  chunked\r\n"
      "\r\n";
  std::string message_body =
      "A            chunkjed extension  \r\n"
      "01234567890            more crud including numbers 123123\r\n"
      "3f\r\n"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
      "0 last one\r\n";
  std::string trailer_data =
      "a_trailer_key: and a trailer value\r\n"
      "\r\n";
  std::string message_body_data =
      "0123456789"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

  std::string message = (std::string(message_headers) +
                         std::string(message_body) + std::string(trailer_data));

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("trAnsfer-eNcoding", "chunked");
  FakeHeaders fake_headers_in_trailer;
  fake_headers_in_trailer.AddKeyValue("a_trailer_key", "and a trailer value");

  balsa_frame_.set_is_request(false);

  {
    InSequence s1;
    EXPECT_CALL(visitor_mock_,
                OnResponseFirstLineInput("HTTP/1.1  \t 200 Ok all is well",
                                         "HTTP/1.1", "200", "Ok all is well"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, OnChunkLength(10));
    EXPECT_CALL(visitor_mock_, OnChunkLength(63));
    EXPECT_CALL(visitor_mock_, OnChunkLength(0));
    EXPECT_CALL(visitor_mock_, OnTrailers(fake_headers_in_trailer));
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(message_headers));
  std::string body_input;
  EXPECT_CALL(visitor_mock_, OnRawBodyInput(_))
      .WillRepeatedly([&body_input](absl::string_view input) {
        absl::StrAppend(&body_input, input);
      });
  std::string body_data;
  EXPECT_CALL(visitor_mock_, OnBodyChunkInput(_))
      .WillRepeatedly([&body_data](absl::string_view input) {
        absl::StrAppend(&body_data, input);
      });
  std::string trailer_input;
  EXPECT_CALL(visitor_mock_, OnTrailerInput(_))
      .WillRepeatedly([&trailer_input](absl::string_view input) {
        absl::StrAppend(&trailer_input, input);
      });

  for (size_t i = 0; i < message.size(); ++i) {
    ASSERT_EQ(1u, balsa_frame_.ProcessInput(message.data() + i, 1));
  }
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());

  EXPECT_EQ(message_body, body_input);
  EXPECT_EQ(message_body_data, body_data);
  EXPECT_EQ(trailer_data, trailer_input);
}

TEST(HTTPBalsaFrame,
     VisitorInvokedProperlyForResponseWithTransferEncodingAndTrailersRandom) {
  TestSeed seed;
  seed.Initialize(GetQuicheCommandLineFlag(FLAGS_randseed));
  RandomEngine rng;
  rng.seed(seed.GetSeed());
  for (int i = 0; i < 1000; ++i) {
    std::string message_headers =
        "HTTP/1.1  \t 200 Ok all is well\r\n"
        "trAnsfer-eNcoding:  chunked\r\n"
        "\r\n";
    std::string message_body =
        "A            chunkjed extension  \r\n"
        "01234567890            more crud including numbers 123123\r\n"
        "3f\r\n"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
        "0 last one\r\n";
    std::string trailer_data =
        "a_trailer_key: and a trailer value\r\n"
        "\r\n";
    std::string message_body_data =
        "0123456789"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

    std::string message =
        (std::string(message_headers) + std::string(message_body) +
         std::string(trailer_data));

    FakeHeaders fake_headers;
    fake_headers.AddKeyValue("trAnsfer-eNcoding", "chunked");
    FakeHeaders fake_headers_in_trailer;
    fake_headers_in_trailer.AddKeyValue("a_trailer_key", "and a trailer value");

    StrictMock<BalsaVisitorMock> visitor_mock;

    BalsaHeaders headers;
    BalsaFrame balsa_frame;
    balsa_frame.set_is_request(false);
    balsa_frame.set_balsa_headers(&headers);
    balsa_frame.EnableTrailers();
    balsa_frame.set_balsa_visitor(&visitor_mock);

    {
      InSequence s1;
      EXPECT_CALL(visitor_mock, OnResponseFirstLineInput(
                                    "HTTP/1.1  \t 200 Ok all is well",
                                    "HTTP/1.1", "200", "Ok all is well"));
      EXPECT_CALL(visitor_mock, ProcessHeaders(fake_headers));
      EXPECT_CALL(visitor_mock, HeaderDone());
      EXPECT_CALL(visitor_mock, OnTrailers(fake_headers_in_trailer));
      EXPECT_CALL(visitor_mock, MessageDone());
    }
    EXPECT_CALL(visitor_mock, OnHeaderInput(message_headers));
    std::string body_input;
    EXPECT_CALL(visitor_mock, OnRawBodyInput(_))
        .WillRepeatedly([&body_input](absl::string_view input) {
          absl::StrAppend(&body_input, input);
        });
    std::string body_data;
    EXPECT_CALL(visitor_mock, OnBodyChunkInput(_))
        .WillRepeatedly([&body_data](absl::string_view input) {
          absl::StrAppend(&body_data, input);
        });
    std::string trailer_input;
    EXPECT_CALL(visitor_mock, OnTrailerInput(_))
        .WillRepeatedly([&trailer_input](absl::string_view input) {
          absl::StrAppend(&trailer_input, input);
        });
    EXPECT_CALL(visitor_mock, OnChunkLength(_)).Times(AtLeast(1));
    EXPECT_CALL(visitor_mock, OnChunkExtensionInput(_)).Times(AtLeast(1));

    size_t count = 0;
    size_t total_processed = 0;
    for (size_t j = 0; j < message.size();) {
      auto dist = std::uniform_int_distribution<>(0, message.size() - j + 1);
      count = dist(rng);
      size_t processed = balsa_frame.ProcessInput(message.data() + j, count);
      ASSERT_GE(count, processed);
      total_processed += processed;
      j += processed;
    }
    EXPECT_EQ(message.size(), total_processed);
    EXPECT_TRUE(balsa_frame.MessageFullyRead());
    EXPECT_FALSE(balsa_frame.Error());
    EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame.ErrorCode());

    EXPECT_EQ(message_body, body_input);
    EXPECT_EQ(message_body_data, body_data);
    EXPECT_EQ(trailer_data, trailer_input);
  }
}

TEST_F(HTTPBalsaFrameTest,
       AppropriateActionTakenWhenHeadersTooLongWithTooMuchInput) {
  const absl::string_view message =
      "GET /asflkasfdhjsafdkljhasfdlkjhasdflkjhsafdlkjhh HTTP/1.1";
  const size_t kAmountLessThanHeaderLen = 10;
  ASSERT_LE(kAmountLessThanHeaderLen, message.size());

  auto error_code = BalsaFrameEnums::HEADERS_TOO_LONG;
  EXPECT_CALL(visitor_mock_, HandleError(error_code));

  balsa_frame_.set_max_header_length(message.size() - kAmountLessThanHeaderLen);

  ASSERT_EQ(balsa_frame_.max_header_length(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::HEADERS_TOO_LONG, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, AppropriateActionTakenWhenHeadersTooLongWithBody) {
  std::string message =
      "PUT /foo HTTP/1.1\r\n"
      "Content-Length: 4\r\n"
      "header: xxxxxxxxx\r\n\r\n"
      "B";  // body begin

  auto error_code = BalsaFrameEnums::HEADERS_TOO_LONG;
  EXPECT_CALL(visitor_mock_, HandleError(error_code));

  // -2 because we have 1 byte of body, and we want to refuse
  // this.
  balsa_frame_.set_max_header_length(message.size() - 2);

  ASSERT_EQ(balsa_frame_.max_header_length(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::HEADERS_TOO_LONG, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, AppropriateActionTakenWhenHeadersTooLongWhenReset) {
  std::string message =
      "GET /asflkasfdhjsafdkljhasfdlkjhasdflkjhsafdlkjhh HTTP/1.1\r\n"
      "\r\n";
  const size_t kAmountLessThanHeaderLen = 10;
  ASSERT_LE(kAmountLessThanHeaderLen, message.size());

  auto error_code = BalsaFrameEnums::HEADERS_TOO_LONG;

  ASSERT_EQ(message.size() - 2,
            balsa_frame_.ProcessInput(message.data(), message.size() - 2));

  // Now set max header length to something smaller.
  balsa_frame_.set_max_header_length(message.size() - kAmountLessThanHeaderLen);
  EXPECT_CALL(visitor_mock_, HandleError(error_code));

  ASSERT_EQ(0u,
            balsa_frame_.ProcessInput(message.data() + message.size() - 2, 2));
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::HEADERS_TOO_LONG, balsa_frame_.ErrorCode());
}

class BalsaFrameParsingTest : public QuicheTest {
 protected:
  void SetUp() override {
    balsa_frame_.set_is_request(true);
    balsa_frame_.set_balsa_headers(&headers_);
    balsa_frame_.set_balsa_visitor(&visitor_mock_);
  }

  void TestEmptyHeaderKeyHelper(const std::string& message) {
    InSequence s;
    EXPECT_CALL(visitor_mock_, OnRequestFirstLineInput("GET / HTTP/1.1", "GET",
                                                       "/", "HTTP/1.1"));
    EXPECT_CALL(visitor_mock_, OnHeaderInput(_));
    EXPECT_CALL(visitor_mock_,
                HandleError(BalsaFrameEnums::INVALID_HEADER_FORMAT));

    ASSERT_EQ(message.size(),
              balsa_frame_.ProcessInput(message.data(), message.size()));
    EXPECT_TRUE(balsa_frame_.Error());
    Mock::VerifyAndClearExpectations(&visitor_mock_);
  }

  void TestInvalidTrailerFormat(const std::string& trailer,
                                bool invalid_name_char) {
    balsa_frame_.set_is_request(false);
    balsa_frame_.EnableTrailers();

    std::string headers =
        "HTTP/1.0 200 ok\r\n"
        "transfer-encoding: chunked\r\n"
        "\r\n";

    std::string chunks =
        "3\r\n"
        "123\r\n"
        "0\r\n";

    InSequence s;

    EXPECT_CALL(visitor_mock_, OnResponseFirstLineInput);
    EXPECT_CALL(visitor_mock_, OnHeaderInput);
    EXPECT_CALL(visitor_mock_, ProcessHeaders);
    EXPECT_CALL(visitor_mock_, HeaderDone);
    EXPECT_CALL(visitor_mock_, OnChunkLength(3));
    EXPECT_CALL(visitor_mock_, OnChunkExtensionInput);
    EXPECT_CALL(visitor_mock_, OnRawBodyInput);
    EXPECT_CALL(visitor_mock_, OnBodyChunkInput);
    EXPECT_CALL(visitor_mock_, OnChunkLength(0));
    EXPECT_CALL(visitor_mock_, OnChunkExtensionInput);
    EXPECT_CALL(visitor_mock_, OnRawBodyInput);
    EXPECT_CALL(visitor_mock_, OnRawBodyInput);
    const auto expected_error =
        invalid_name_char ? BalsaFrameEnums::INVALID_TRAILER_NAME_CHARACTER
                          : BalsaFrameEnums::INVALID_TRAILER_FORMAT;
    EXPECT_CALL(visitor_mock_, HandleError(expected_error)).Times(1);

    EXPECT_CALL(visitor_mock_, OnTrailers(_)).Times(0);
    EXPECT_CALL(visitor_mock_, MessageDone()).Times(0);

    ASSERT_EQ(headers.size(),
              balsa_frame_.ProcessInput(headers.data(), headers.size()));
    ASSERT_EQ(chunks.size(),
              balsa_frame_.ProcessInput(chunks.data(), chunks.size()));
    EXPECT_EQ(trailer.size(),
              balsa_frame_.ProcessInput(trailer.data(), trailer.size()));
    EXPECT_FALSE(balsa_frame_.MessageFullyRead());
    EXPECT_TRUE(balsa_frame_.Error());
    EXPECT_EQ(expected_error, balsa_frame_.ErrorCode());

    Mock::VerifyAndClearExpectations(&visitor_mock_);
  }

  BalsaHeaders headers_;
  BalsaFrame balsa_frame_;
  StrictMock<BalsaVisitorMock> visitor_mock_;
};

TEST_F(BalsaFrameParsingTest, AppropriateActionTakenWhenHeaderColonsAreFunny) {
  std::string message =
      "GET / HTTP/1.1\r\n"
      "a\r\n"
      "b\r\n"
      "c\r\n"
      "d\r\n"
      "e\r\n"
      "f\r\n"
      "g\r\n"
      "h\r\n"
      "i:\r\n"
      "j\r\n"
      "k\r\n"
      "l\r\n"
      "m\r\n"
      "n\r\n"
      "o\r\n"
      "p\r\n"
      "q\r\n"
      "r\r\n"
      "s\r\n"
      "t\r\n"
      "u\r\n"
      "v\r\n"
      "w\r\n"
      "x\r\n"
      "y\r\n"
      "z\r\n"
      "A\r\n"
      "B\r\n"
      ": val\r\n"
      "\r\n";

  EXPECT_CALL(visitor_mock_, OnRequestFirstLineInput("GET / HTTP/1.1", "GET",
                                                     "/", "HTTP/1.1"));
  EXPECT_CALL(visitor_mock_, OnHeaderInput(_));
  EXPECT_CALL(visitor_mock_,
              HandleWarning(BalsaFrameEnums::HEADER_MISSING_COLON))
      .Times(27);
  EXPECT_CALL(visitor_mock_,
              HandleError(BalsaFrameEnums::INVALID_HEADER_FORMAT));

  ASSERT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));

  EXPECT_TRUE(balsa_frame_.Error());
}

TEST_F(BalsaFrameParsingTest, ErrorWhenHeaderKeyIsEmpty) {
  std::string firstKeyIsEmpty =
      "GET / HTTP/1.1\r\n"
      ": \r\n"
      "a:b\r\n"
      "c:d\r\n"
      "\r\n";
  TestEmptyHeaderKeyHelper(firstKeyIsEmpty);

  balsa_frame_.Reset();

  std::string laterKeyIsEmpty =
      "GET / HTTP/1.1\r\n"
      "a:b\r\n"
      ": \r\n"
      "c:d\r\n"
      "\r\n";
  TestEmptyHeaderKeyHelper(laterKeyIsEmpty);
}

TEST_F(BalsaFrameParsingTest, InvalidTrailerFormat) {
  std::string trailer =
      ":monkeys\n"
      "\r\n";
  TestInvalidTrailerFormat(trailer, /*invalid_name_char=*/false);

  balsa_frame_.Reset();

  std::string trailer2 =
      "   \r\n"
      "test: test\r\n"
      "\r\n";
  TestInvalidTrailerFormat(trailer2, /*invalid_name_char=*/true);

  balsa_frame_.Reset();

  std::string trailer3 =
      "a: b\r\n"
      ": test\r\n"
      "\r\n";
  TestInvalidTrailerFormat(trailer3, /*invalid_name_char=*/false);
}

TEST_F(HTTPBalsaFrameTest,
       EnsureHeaderFramingFoundWithVariousCombinationsOfRN_RN) {
  const std::string message =
      "GET / HTTP/1.1\r\n"
      "content-length: 0\r\n"
      "a\r\n"
      "b\r\n"
      "c\r\n"
      "d\r\n"
      "e\r\n"
      "f\r\n"
      "g\r\n"
      "h\r\n"
      "i\r\n"
      "\r\n";
  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.Error())
      << BalsaFrameEnums::ErrorCodeToString(balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest,
       EnsureHeaderFramingFoundWithVariousCombinationsOfRN_N) {
  const std::string message =
      "GET / HTTP/1.1\n"
      "content-length: 0\n"
      "a\n"
      "b\n"
      "c\n"
      "d\n"
      "e\n"
      "f\n"
      "g\n"
      "h\n"
      "i\n"
      "\n";
  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.Error())
      << BalsaFrameEnums::ErrorCodeToString(balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest,
       EnsureHeaderFramingFoundWithVariousCombinationsOfRN_RN_N) {
  const std::string message =
      "GET / HTTP/1.1\n"
      "content-length: 0\r\n"
      "a\r\n"
      "b\n"
      "c\r\n"
      "d\n"
      "e\r\n"
      "f\n"
      "g\r\n"
      "h\n"
      "i\r\n"
      "\n";
  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.Error())
      << BalsaFrameEnums::ErrorCodeToString(balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest,
       EnsureHeaderFramingFoundWithVariousCombinationsOfRN_N_RN) {
  const std::string message =
      "GET / HTTP/1.1\n"
      "content-length: 0\r\n"
      "a\n"
      "b\r\n"
      "c\n"
      "d\r\n"
      "e\n"
      "f\r\n"
      "g\n"
      "h\r\n"
      "i\n"
      "\r\n";
  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.Error())
      << BalsaFrameEnums::ErrorCodeToString(balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, ReadUntilCloseStateEnteredAsExpectedAndNotExited) {
  std::string message =
      "HTTP/1.1 200 OK\r\n"
      "\r\n";
  balsa_frame_.set_is_request(false);
  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.Error())
      << BalsaFrameEnums::ErrorCodeToString(balsa_frame_.ErrorCode());
  EXPECT_EQ(BalsaFrameEnums::READING_UNTIL_CLOSE, balsa_frame_.ParseState());

  std::string gobldygook = "-198324-9182-43981-23498-98342-jasldfn-1294hj";
  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(gobldygook.size(),
              balsa_frame_.ProcessInput(gobldygook.data(), gobldygook.size()));
    EXPECT_FALSE(balsa_frame_.Error())
        << BalsaFrameEnums::ErrorCodeToString(balsa_frame_.ErrorCode());
    EXPECT_EQ(BalsaFrameEnums::READING_UNTIL_CLOSE, balsa_frame_.ParseState());
  }
}

TEST_F(HTTPBalsaFrameTest,
       BytesSafeToSpliceAndBytesSplicedWorksWithContentLength) {
  std::string header =
      "HTTP/1.1 200 OK\r\n"
      "content-length: 1000\r\n"
      "\r\n";
  balsa_frame_.set_is_request(false);
  size_t bytes_safe_to_splice = 1000;
  EXPECT_EQ(0u, balsa_frame_.BytesSafeToSplice());
  EXPECT_EQ(header.size(),
            balsa_frame_.ProcessInput(header.data(), header.size()));
  EXPECT_EQ(bytes_safe_to_splice, balsa_frame_.BytesSafeToSplice());
  while (bytes_safe_to_splice > 0) {
    balsa_frame_.BytesSpliced(1);
    bytes_safe_to_splice -= 1;
    ASSERT_FALSE(balsa_frame_.Error())
        << BalsaFrameEnums::ParseStateToString(balsa_frame_.ParseState()) << " "
        << BalsaFrameEnums::ErrorCodeToString(balsa_frame_.ErrorCode())
        << " with bytes_safe_to_splice: " << bytes_safe_to_splice
        << " and BytesSafeToSplice(): " << balsa_frame_.BytesSafeToSplice();
  }
  EXPECT_EQ(0u, balsa_frame_.BytesSafeToSplice());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
}

TEST_F(HTTPBalsaFrameTest, BytesSplicedFlagsErrorsWhenNotInProperState) {
  balsa_frame_.set_is_request(false);
  balsa_frame_.BytesSpliced(1);
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::CALLED_BYTES_SPLICED_WHEN_UNSAFE_TO_DO_SO,
            balsa_frame_.ErrorCode());
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
}

TEST_F(HTTPBalsaFrameTest,
       BytesSplicedFlagsErrorsWhenTooMuchSplicedForContentLen) {
  std::string header =
      "HTTP/1.1 200 OK\r\n"
      "content-length: 1000\r\n"
      "\r\n";
  balsa_frame_.set_is_request(false);
  EXPECT_EQ(0u, balsa_frame_.BytesSafeToSplice());
  EXPECT_EQ(header.size(),
            balsa_frame_.ProcessInput(header.data(), header.size()));
  EXPECT_EQ(1000u, balsa_frame_.BytesSafeToSplice());
  balsa_frame_.BytesSpliced(1001);
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(
      BalsaFrameEnums::CALLED_BYTES_SPLICED_AND_EXCEEDED_SAFE_SPLICE_AMOUNT,
      balsa_frame_.ErrorCode());
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
}

TEST_F(HTTPBalsaFrameTest, BytesSplicedWorksAsExpectedForReadUntilClose) {
  std::string header =
      "HTTP/1.1 200 OK\r\n"
      "\r\n";
  balsa_frame_.set_is_request(false);
  EXPECT_EQ(0u, balsa_frame_.BytesSafeToSplice());
  EXPECT_EQ(header.size(),
            balsa_frame_.ProcessInput(header.data(), header.size()));
  EXPECT_EQ(BalsaFrameEnums::READING_UNTIL_CLOSE, balsa_frame_.ParseState());
  EXPECT_EQ(std::numeric_limits<size_t>::max(),
            balsa_frame_.BytesSafeToSplice());
  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(std::numeric_limits<size_t>::max(),
              balsa_frame_.BytesSafeToSplice());
    balsa_frame_.BytesSpliced(12312312);
    EXPECT_FALSE(balsa_frame_.Error());
    EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  }
  EXPECT_EQ(std::numeric_limits<size_t>::max(),
            balsa_frame_.BytesSafeToSplice());
}

TEST_F(HTTPBalsaFrameTest,
       BytesSplicedFlagsErrorsWhenTooMuchSplicedForChunked) {
  std::string header =
      "HTTP/1.1 200 OK\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";
  std::string body_fragment = "a\r\n";
  balsa_frame_.set_is_request(false);
  EXPECT_EQ(0u, balsa_frame_.BytesSafeToSplice());
  EXPECT_EQ(header.size(),
            balsa_frame_.ProcessInput(header.data(), header.size()));
  EXPECT_EQ(0u, balsa_frame_.BytesSafeToSplice());
  EXPECT_EQ(
      body_fragment.size(),
      balsa_frame_.ProcessInput(body_fragment.data(), body_fragment.size()));
  EXPECT_EQ(10u, balsa_frame_.BytesSafeToSplice());
  balsa_frame_.BytesSpliced(11);
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(
      BalsaFrameEnums::CALLED_BYTES_SPLICED_AND_EXCEEDED_SAFE_SPLICE_AMOUNT,
      balsa_frame_.ErrorCode());
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
}

TEST_F(HTTPBalsaFrameTest, BytesSafeToSpliceAndBytesSplicedWorksWithChunks) {
  std::string header =
      "HTTP/1.1 200 OK\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";
  balsa_frame_.set_is_request(false);
  EXPECT_EQ(0u, balsa_frame_.BytesSafeToSplice());
  EXPECT_EQ(header.size(),
            balsa_frame_.ProcessInput(header.data(), header.size()));

  {
    std::string body_fragment = "3e8\r\n";
    EXPECT_FALSE(balsa_frame_.MessageFullyRead());
    size_t bytes_safe_to_splice = 1000;
    EXPECT_EQ(0u, balsa_frame_.BytesSafeToSplice());
    EXPECT_EQ(
        body_fragment.size(),
        balsa_frame_.ProcessInput(body_fragment.data(), body_fragment.size()));
    EXPECT_EQ(bytes_safe_to_splice, balsa_frame_.BytesSafeToSplice());
    while (bytes_safe_to_splice > 0) {
      balsa_frame_.BytesSpliced(1);
      bytes_safe_to_splice -= 1;
      ASSERT_FALSE(balsa_frame_.Error());
    }
    EXPECT_EQ(0u, balsa_frame_.BytesSafeToSplice());
    EXPECT_FALSE(balsa_frame_.Error());
  }
  {
    std::string body_fragment = "\r\n7d0\r\n";
    EXPECT_FALSE(balsa_frame_.MessageFullyRead());
    size_t bytes_safe_to_splice = 2000;
    EXPECT_EQ(0u, balsa_frame_.BytesSafeToSplice());
    EXPECT_EQ(
        body_fragment.size(),
        balsa_frame_.ProcessInput(body_fragment.data(), body_fragment.size()));
    EXPECT_EQ(bytes_safe_to_splice, balsa_frame_.BytesSafeToSplice());
    while (bytes_safe_to_splice > 0) {
      balsa_frame_.BytesSpliced(1);
      bytes_safe_to_splice -= 1;
      ASSERT_FALSE(balsa_frame_.Error());
    }
    EXPECT_EQ(0u, balsa_frame_.BytesSafeToSplice());
    EXPECT_FALSE(balsa_frame_.Error());
  }
  {
    std::string body_fragment = "\r\n1\r\n";
    EXPECT_FALSE(balsa_frame_.MessageFullyRead());
    size_t bytes_safe_to_splice = 1;
    EXPECT_EQ(0u, balsa_frame_.BytesSafeToSplice());
    EXPECT_EQ(
        body_fragment.size(),
        balsa_frame_.ProcessInput(body_fragment.data(), body_fragment.size()));
    EXPECT_EQ(bytes_safe_to_splice, balsa_frame_.BytesSafeToSplice());
    while (bytes_safe_to_splice > 0) {
      balsa_frame_.BytesSpliced(1);
      bytes_safe_to_splice -= 1;
      ASSERT_FALSE(balsa_frame_.Error());
    }
    EXPECT_EQ(0u, balsa_frame_.BytesSafeToSplice());
    EXPECT_FALSE(balsa_frame_.Error());
  }
  {
    std::string body_fragment = "\r\n0\r\n\r\n";
    EXPECT_FALSE(balsa_frame_.MessageFullyRead());
    EXPECT_EQ(0u, balsa_frame_.BytesSafeToSplice());
    EXPECT_EQ(
        body_fragment.size(),
        balsa_frame_.ProcessInput(body_fragment.data(), body_fragment.size()));
    EXPECT_EQ(0u, balsa_frame_.BytesSafeToSplice());
    EXPECT_FALSE(balsa_frame_.Error());
  }
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
}

TEST_F(HTTPBalsaFrameTest, TwoDifferentContentLengthHeadersIsAnError) {
  std::string header =
      "HTTP/1.1 200 OK\r\n"
      "content-length: 12\r\n"
      "content-length: 14\r\n"
      "\r\n";
  balsa_frame_.set_is_request(false);
  balsa_frame_.ProcessInput(header.data(), header.size());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::MULTIPLE_CONTENT_LENGTH_KEYS,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, TwoSameContentLengthHeadersIsNotAnError) {
  std::string header =
      "POST / HTTP/1.1\r\n"
      "content-length: 1\r\n"
      "content-length: 1\r\n"
      "\r\n"
      "1";
  balsa_frame_.ProcessInput(header.data(), header.size());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
  EXPECT_FALSE(balsa_frame_.Error());
  balsa_frame_.ProcessInput(header.data(), header.size());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
}

TEST_F(HTTPBalsaFrameTest, TwoSameContentLengthHeadersIsAnError) {
  HttpValidationPolicy http_validation_policy;
  http_validation_policy.disallow_multiple_content_length = true;
  balsa_frame_.set_http_validation_policy(http_validation_policy);

  std::string header =
      "POST / HTTP/1.1\r\n"
      "content-length: 1\r\n"
      "content-length: 1\r\n"
      "\r\n"
      "1";
  balsa_frame_.ProcessInput(header.data(), header.size());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::MULTIPLE_CONTENT_LENGTH_KEYS,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, ChunkedTransferEncodingWithContentLength) {
  std::string header =
      "POST / HTTP/1.1\r\n"
      "transfer-encoding: chunked\r\n"
      "content-length: 3\r\n"
      "\r\n";
  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("transfer-encoding", "chunked");
  fake_headers.AddKeyValue("content-length", "3");
  EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));

  balsa_frame_.ProcessInput(header.data(), header.size());

  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(headers_.content_length_status(),
            BalsaHeadersEnums::VALID_CONTENT_LENGTH);
  EXPECT_EQ(headers_.content_length(), 3);
  EXPECT_TRUE(headers_.transfer_encoding_is_chunked());
}

TEST_F(HTTPBalsaFrameTest, TwoTransferEncodingHeadersIsAnError) {
  std::string header =
      "HTTP/1.1 200 OK\r\n"
      "transfer-encoding: chunked\r\n"
      "transfer-encoding: identity\r\n"
      "content-length: 3\r\n"
      "\r\n";
  balsa_frame_.set_is_request(false);
  balsa_frame_.ProcessInput(header.data(), header.size());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::MULTIPLE_TRANSFER_ENCODING_KEYS,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, AcceptTwoTransferEncodingHeaders) {
  HttpValidationPolicy http_validation_policy;
  http_validation_policy.validate_transfer_encoding = false;
  balsa_frame_.set_http_validation_policy(http_validation_policy);

  std::string header =
      "HTTP/1.1 200 OK\r\n"
      "transfer-encoding: chunked\r\n"
      "transfer-encoding: identity\r\n"
      "content-length: 3\r\n"
      "\r\n";
  balsa_frame_.set_is_request(false);
  balsa_frame_.ProcessInput(header.data(), header.size());

  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, TwoTransferEncodingTokensIsAnError) {
  std::string header =
      "HTTP/1.1 200 OK\r\n"
      "transfer-encoding: chunked, identity\r\n"
      "content-length: 3\r\n"
      "\r\n";
  balsa_frame_.set_is_request(false);
  balsa_frame_.ProcessInput(header.data(), header.size());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::UNKNOWN_TRANSFER_ENCODING,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, AcceptTwoTransferEncodingTokens) {
  HttpValidationPolicy http_validation_policy;
  http_validation_policy.validate_transfer_encoding = false;
  balsa_frame_.set_http_validation_policy(http_validation_policy);

  std::string header =
      "HTTP/1.1 200 OK\r\n"
      "transfer-encoding: chunked, identity\r\n"
      "content-length: 3\r\n"
      "\r\n";
  balsa_frame_.set_is_request(false);
  balsa_frame_.ProcessInput(header.data(), header.size());

  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, UnknownTransferEncodingTokenIsAnError) {
  std::string header =
      "HTTP/1.1 200 OK\r\n"
      "transfer-encoding: chunked-identity\r\n"
      "content-length: 3\r\n"
      "\r\n";
  balsa_frame_.set_is_request(false);
  balsa_frame_.ProcessInput(header.data(), header.size());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::UNKNOWN_TRANSFER_ENCODING,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, AcceptUnknownTransferEncodingToken) {
  HttpValidationPolicy http_validation_policy;
  http_validation_policy.validate_transfer_encoding = false;
  balsa_frame_.set_http_validation_policy(http_validation_policy);

  std::string header =
      "HTTP/1.1 200 OK\r\n"
      "transfer-encoding: chunked-identity\r\n"
      "content-length: 3\r\n"
      "\r\n";
  balsa_frame_.set_is_request(false);
  balsa_frame_.ProcessInput(header.data(), header.size());

  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, MissingContentLength) {
  std::string header = "HTTP/1.1 200 OK\r\n\r\n";
  balsa_frame_.set_is_request(false);
  balsa_frame_.ProcessInput(header.data(), header.size());

  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::MAYBE_BODY_BUT_NO_CONTENT_LENGTH,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, MultipleTransferEncodingsWithMissingContentLength) {
  HttpValidationPolicy http_validation_policy;
  http_validation_policy.validate_transfer_encoding = false;
  balsa_frame_.set_http_validation_policy(http_validation_policy);

  std::string header =
      "HTTP/1.1 200 OK\r\n"
      "transfer-encoding: chunked\r\n"
      "transfer-encoding: identity\r\n"
      "\r\n";
  balsa_frame_.set_is_request(false);
  balsa_frame_.ProcessInput(header.data(), header.size());

  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::MAYBE_BODY_BUT_NO_CONTENT_LENGTH,
            balsa_frame_.ErrorCode());
}

class DetachOnDoneFramer : public NoOpBalsaVisitor {
 public:
  DetachOnDoneFramer() {
    framer_.set_balsa_headers(&headers_);
    framer_.set_balsa_visitor(this);
  }

  void MessageDone() override { framer_.set_balsa_headers(nullptr); }

  BalsaFrame* framer() { return &framer_; }

 protected:
  BalsaFrame framer_;
  BalsaHeaders headers_;
};

TEST(HTTPBalsaFrame, TestDetachOnDone) {
  DetachOnDoneFramer framer;
  const char* message = "GET HTTP/1.1\r\n\r\n";
  // Frame the whole message.  The framer will call MessageDone which will set
  // the headers to nullptr.
  framer.framer()->ProcessInput(message, strlen(message));
  EXPECT_TRUE(framer.framer()->MessageFullyRead());
  EXPECT_FALSE(framer.framer()->Error());
}

// We simply extend DetachOnDoneFramer so that we do not have
// to provide trivial implementation for various functions.
class ModifyMaxHeaderLengthFramerInFirstLine : public DetachOnDoneFramer {
 public:
  void MessageDone() override {}
  // This sets to max_header_length to a low number and
  // this would cause us to reject the query. Even though
  // our original headers length was acceptable.
  void OnRequestFirstLineInput(absl::string_view /*line_input*/,
                               absl::string_view /*method_input*/,
                               absl::string_view /*request_uri*/,
                               absl::string_view /*version_input*/
                               ) override {
    framer_.set_max_header_length(1);
  }
};

// In this case we have already processed the headers and called on
// the visitor HeadersDone and hence its too late to reduce the
// max_header_length here.
class ModifyMaxHeaderLengthFramerInHeaderDone : public DetachOnDoneFramer {
 public:
  void MessageDone() override {}
  void HeaderDone() override { framer_.set_max_header_length(1); }
};

TEST(HTTPBalsaFrame, ChangeMaxHeadersLengthOnFirstLine) {
  std::string message =
      "PUT /foo HTTP/1.1\r\n"
      "Content-Length: 2\r\n"
      "header: xxxxxxxxx\r\n\r\n"
      "B";  // body begin

  ModifyMaxHeaderLengthFramerInFirstLine balsa_frame;
  balsa_frame.framer()->set_is_request(true);
  balsa_frame.framer()->set_max_header_length(message.size() - 1);

  balsa_frame.framer()->ProcessInput(message.data(), message.size());
  EXPECT_EQ(BalsaFrameEnums::HEADERS_TOO_LONG,
            balsa_frame.framer()->ErrorCode());
}

TEST(HTTPBalsaFrame, ChangeMaxHeadersLengthOnHeaderDone) {
  std::string message =
      "PUT /foo HTTP/1.1\r\n"
      "Content-Length: 2\r\n"
      "header: xxxxxxxxx\r\n\r\n"
      "B";  // body begin

  ModifyMaxHeaderLengthFramerInHeaderDone balsa_frame;
  balsa_frame.framer()->set_is_request(true);
  balsa_frame.framer()->set_max_header_length(message.size() - 1);

  balsa_frame.framer()->ProcessInput(message.data(), message.size());
  EXPECT_EQ(0, balsa_frame.framer()->ErrorCode());
}

// This is a simple test to ensure the simple case that we accept
// a query which has headers size same as the max_header_length.
// (i.e., there is no off by one error).
TEST(HTTPBalsaFrame, HeadersSizeSameAsMaxLengthIsAccepted) {
  std::string message =
      "GET /foo HTTP/1.1\r\n"
      "header: xxxxxxxxx\r\n\r\n";

  ModifyMaxHeaderLengthFramerInHeaderDone balsa_frame;
  balsa_frame.framer()->set_is_request(true);
  balsa_frame.framer()->set_max_header_length(message.size());
  balsa_frame.framer()->ProcessInput(message.data(), message.size());
  EXPECT_EQ(0, balsa_frame.framer()->ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, KeyHasSpaces) {
  const std::string message =
      "GET / HTTP/1.1\r\n"
      "key has spaces: lock\r\n"
      "\r\n";
  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::INVALID_HEADER_NAME_CHARACTER,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, SpaceBeforeColon) {
  const std::string message =
      "GET / HTTP/1.1\r\n"
      "key : lock\r\n"
      "\r\n";
  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::INVALID_HEADER_NAME_CHARACTER,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, SpaceBeforeColonNotAfter) {
  const std::string message =
      "GET / HTTP/1.1\r\n"
      "key :lock\r\n"
      "\r\n";
  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::INVALID_HEADER_NAME_CHARACTER,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, KeyHasTabs) {
  const std::string message =
      "GET / HTTP/1.1\r\n"
      "key\thas\ttabs: lock\r\n"
      "\r\n";
  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::INVALID_HEADER_NAME_CHARACTER,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, TabBeforeColon) {
  const std::string message =
      "GET / HTTP/1.1\r\n"
      "key\t: lock\r\n"
      "\r\n";
  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::INVALID_HEADER_NAME_CHARACTER,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, KeyHasContinuation) {
  const std::string message =
      "GET / HTTP/1.1\r\n"
      "key\n includes continuation: but not value\r\n"
      "\r\n";
  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::INVALID_HEADER_NAME_CHARACTER,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, KeyHasMultipleContinuations) {
  const std::string message =
      "GET / HTTP/1.1\r\n"
      "key\n includes\r\n multiple\n continuations: but not value\r\n"
      "\r\n";
  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::INVALID_HEADER_NAME_CHARACTER,
            balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, KeyHasDoubleQuote) {
  const std::string message =
      "GET / HTTP/1.1\r\n"
      "key\"hasquote: lock\r\n"
      "\r\n";
  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
  EXPECT_TRUE(headers_.HasHeader("key\"hasquote"));
}

TEST_F(HTTPBalsaFrameTest, KeyHasDisallowedDoubleQuote) {
  HttpValidationPolicy http_validation_policy;
  http_validation_policy.disallow_double_quote_in_header_name = true;
  balsa_frame_.set_http_validation_policy(http_validation_policy);

  const std::string message =
      "GET / HTTP/1.1\r\n"
      "key\"hasquote: lock\r\n"
      "\r\n";
  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::INVALID_HEADER_NAME_CHARACTER,
            balsa_frame_.ErrorCode());
}

// Missing colon is a warning, not an error.
TEST_F(HTTPBalsaFrameTest, TrailerMissingColon) {
  std::string headers =
      "HTTP/1.0 302 Redirect\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";

  std::string chunks =
      "3\r\n"
      "123\r\n"
      "0\r\n";
  std::string trailer =
      "crass_monkeys\n"
      "\r\n";

  balsa_frame_.set_is_request(false);
  EXPECT_CALL(visitor_mock_,
              HandleWarning(BalsaFrameEnums::TRAILER_MISSING_COLON));
  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));
  ASSERT_EQ(chunks.size(),
            balsa_frame_.ProcessInput(chunks.data(), chunks.size()));

  FakeHeaders fake_trailers;
  fake_trailers.AddKeyValue("crass_monkeys", "");
  EXPECT_CALL(visitor_mock_, OnTrailers(fake_trailers));
  EXPECT_EQ(trailer.size(),
            balsa_frame_.ProcessInput(trailer.data(), trailer.size()));

  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::TRAILER_MISSING_COLON, balsa_frame_.ErrorCode());
}

// This tests multiple headers in trailer. We currently do not and have no plan
// to support Trailer field in headers to limit valid field-name in trailer.
// Test that we aren't confused by the non-alphanumeric characters in the
// trailer, especially ':'.
TEST_F(HTTPBalsaFrameTest, MultipleHeadersInTrailer) {
  std::string headers =
      "HTTP/1.1 200 OK\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";

  std::string chunks =
      "3\r\n"
      "123\n"
      "0\r\n";
  std::map<std::string, std::string> trailer;
  trailer["X-Trace"] =
      "http://trace.example.com/trace?host="
      "foobar.example.com&start=2012-06-03_15:59:06&rpc_duration=0.243349";
  trailer["Date"] = "Sun, 03 Jun 2012 22:59:06 GMT";
  trailer["Content-Type"] = "text/html";
  trailer["X-Backends"] = "127.0.0.1_0,foo.example.com:39359";
  trailer["X-Request-Trace"] =
      "foo.example.com:39359,127.0.0.1_1,"
      "foo.example.com:39359,127.0.0.1_0,"
      "foo.example.com:39359";
  trailer["X-Service-Trace"] = "default";
  trailer["X-Service"] = "default";

  std::map<std::string, std::string>::const_iterator iter;
  std::string trailer_data;
  TestSeed seed;
  seed.Initialize(GetQuicheCommandLineFlag(FLAGS_randseed));
  RandomEngine rng;
  rng.seed(seed.GetSeed());
  FakeHeaders fake_headers_in_trailer;
  for (iter = trailer.begin(); iter != trailer.end(); ++iter) {
    trailer_data += iter->first;
    trailer_data += ":";
    std::stringstream leading_whitespace_for_value;
    AppendRandomWhitespace(rng, &leading_whitespace_for_value);
    trailer_data += leading_whitespace_for_value.str();
    trailer_data += iter->second;
    std::stringstream trailing_whitespace_for_value;
    AppendRandomWhitespace(rng, &trailing_whitespace_for_value);
    trailer_data += trailing_whitespace_for_value.str();
    trailer_data += random_line_term(rng);
    fake_headers_in_trailer.AddKeyValue(iter->first, iter->second);
  }
  trailer_data += random_line_term(rng);

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("transfer-encoding", "chunked");

  {
    InSequence s1;
    EXPECT_CALL(visitor_mock_, OnResponseFirstLineInput(
                                   "HTTP/1.1 200 OK", "HTTP/1.1", "200", "OK"));
    EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, OnChunkLength(3));
    EXPECT_CALL(visitor_mock_, OnChunkLength(0));
    EXPECT_CALL(visitor_mock_, OnTrailers(fake_headers_in_trailer));
    EXPECT_CALL(visitor_mock_, OnTrailerInput(trailer_data));
    EXPECT_CALL(visitor_mock_, MessageDone());
  }
  EXPECT_CALL(visitor_mock_, OnHeaderInput(headers));
  std::string body_input;
  EXPECT_CALL(visitor_mock_, OnRawBodyInput(_))
      .WillRepeatedly([&body_input](absl::string_view input) {
        absl::StrAppend(&body_input, input);
      });
  EXPECT_CALL(visitor_mock_, OnBodyChunkInput("123"));

  balsa_frame_.set_is_request(false);

  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));
  ASSERT_EQ(chunks.size(),
            balsa_frame_.ProcessInput(chunks.data(), chunks.size()));
  EXPECT_EQ(trailer_data.size(), balsa_frame_.ProcessInput(
                                     trailer_data.data(), trailer_data.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());

  EXPECT_EQ(chunks, body_input);
}

// Test if trailer is not set (the common case), everything will be fine.
TEST_F(HTTPBalsaFrameTest, NothingBadHappensWithNULLTrailer) {
  std::string headers =
      "HTTP/1.1 200 OK\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";

  std::string chunks =
      "3\r\n"
      "123\r\n"
      "0\r\n";
  std::string trailer =
      "crass: monkeys\r\n"
      "funky: monkeys\r\n"
      "\n";

  // Use another BalsaFrame, which does not have the trailers option set.
  BalsaFrame balsa_frame;
  balsa_frame.set_balsa_headers(&headers_);
  balsa_frame.set_is_request(false);
  balsa_frame.set_balsa_visitor(nullptr);

  ASSERT_EQ(headers.size(),
            balsa_frame.ProcessInput(headers.data(), headers.size()));
  ASSERT_EQ(chunks.size(),
            balsa_frame.ProcessInput(chunks.data(), chunks.size()));
  ASSERT_EQ(trailer.size(),
            balsa_frame.ProcessInput(trailer.data(), trailer.size()));
  EXPECT_TRUE(balsa_frame.MessageFullyRead());
  EXPECT_FALSE(balsa_frame.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame.ErrorCode());
}

// Test Reset() correctly resets trailer related states.
TEST_F(HTTPBalsaFrameTest, FrameAndResetAndFrameAgain) {
  std::string headers =
      "HTTP/1.1 200 OK\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";

  std::string chunks =
      "3\r\n"
      "123\r\n"
      "0\r\n";
  std::string trailer =
      "k: v\n"
      "\n";

  balsa_frame_.set_is_request(false);

  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));
  ASSERT_EQ(chunks.size(),
            balsa_frame_.ProcessInput(chunks.data(), chunks.size()));
  {
    FakeHeaders fake_trailers;
    fake_trailers.AddKeyValue("k", "v");
    EXPECT_CALL(visitor_mock_, OnTrailers(fake_trailers));
  }
  ASSERT_EQ(trailer.size(),
            balsa_frame_.ProcessInput(trailer.data(), trailer.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());

  balsa_frame_.Reset();

  headers =
      "HTTP/1.1 404 Error\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";

  chunks =
      "4\r\n"
      "1234\r\n"
      "0\r\n";
  trailer =
      "nk: nv\n"
      "\n";

  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));
  ASSERT_EQ(chunks.size(),
            balsa_frame_.ProcessInput(chunks.data(), chunks.size()));
  {
    FakeHeaders fake_trailers;
    fake_trailers.AddKeyValue("nk", "nv");
    EXPECT_CALL(visitor_mock_, OnTrailers(fake_trailers));
  }
  ASSERT_EQ(trailer.size(),
            balsa_frame_.ProcessInput(trailer.data(), trailer.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

// valid chars are 9 (tab), 10 (LF), 13(CR), and 32-255
TEST_F(HTTPBalsaFrameTest, InvalidCharsInHeaderValueError) {
  balsa_frame_.set_invalid_chars_level(BalsaFrame::InvalidCharsLevel::kError);
  // nulls are double escaped since otherwise this initialized wrong
  const std::string kEscapedInvalid1 =
      "GET /foo HTTP/1.1\r\n"
      "Bogus-Head: val\\x00\r\n"
      "More-Invalid: \\x00\x01\x02\x03\x04\x05\x06\x07\x08\x0B\x0C\x0E\x0F\r\n"
      "And-More: \x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D"
      "\x1E\x1F\r\n\r\n";
  std::string message;
  // now we convert to real embedded nulls
  absl::CUnescape(kEscapedInvalid1, &message);

  EXPECT_CALL(visitor_mock_,
              HandleError(BalsaFrameEnums::INVALID_HEADER_CHARACTER));

  balsa_frame_.ProcessInput(message.data(), message.size());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
}

// Header names reject invalid chars even when the InvalidCharsLevel is kOff.
TEST_F(HTTPBalsaFrameTest, InvalidCharsInHeaderNameError) {
  balsa_frame_.set_invalid_chars_level(BalsaFrame::InvalidCharsLevel::kOff);
  // nulls are double escaped since otherwise this initialized wrong
  const std::string kEscapedInvalid1 =
      "GET /foo HTTP/1.1\r\n"
      "Bogus\\x00-Head: val\r\n\r\n";
  std::string message;
  // now we convert to real embedded nulls
  absl::CUnescape(kEscapedInvalid1, &message);

  EXPECT_CALL(visitor_mock_,
              HandleError(BalsaFrameEnums::INVALID_HEADER_NAME_CHARACTER));

  balsa_frame_.ProcessInput(message.data(), message.size());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
}

TEST_F(HTTPBalsaFrameTest, InvalidCharsInRequestHeaderError) {
  balsa_frame_.set_invalid_chars_level(BalsaFrame::InvalidCharsLevel::kError);
  const std::string kEscapedInvalid =
      "GET /foo HTTP/1.1\r\n"
      "Smuggle-Me: \\x00GET /bar HTTP/1.1\r\n"
      "Another-Header: value\r\n\r\n";
  std::string message;
  absl::CUnescape(kEscapedInvalid, &message);

  EXPECT_CALL(visitor_mock_,
              HandleError(BalsaFrameEnums::INVALID_HEADER_CHARACTER));

  balsa_frame_.ProcessInput(message.data(), message.size());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
}

TEST_F(HTTPBalsaFrameTest, InvalidCharsInResponseHeaderAllowed) {
  balsa_frame_.set_is_request(false);
  balsa_frame_.set_invalid_chars_level(BalsaFrame::InvalidCharsLevel::kOff);

  const absl::string_view headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 5\r\n"
      "foo: a\022b\r\n"
      "\r\n";
  EXPECT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));

  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, InvalidCharsInResponseHeaderError) {
  balsa_frame_.set_is_request(false);
  balsa_frame_.set_invalid_chars_level(BalsaFrame::InvalidCharsLevel::kError);

  const absl::string_view headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 5\r\n"
      "foo: a\022b\r\n"
      "\r\n";
  EXPECT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));

  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::INVALID_HEADER_CHARACTER,
            balsa_frame_.ErrorCode());
}

class HTTPBalsaFrameTestOneChar : public HTTPBalsaFrameTest,
                                  public testing::WithParamInterface<char> {
 public:
  char GetCharUnderTest() { return GetParam(); }
};

TEST_P(HTTPBalsaFrameTestOneChar, InvalidCharsErrorSet) {
  balsa_frame_.set_invalid_chars_level(BalsaFrame::InvalidCharsLevel::kError);
  const std::string kRequest =
      "GET /foo HTTP/1.1\r\n"
      "Bogus-Char-Goes-Here: ";
  const std::string kEnding = "\r\n\r\n";
  std::string message = kRequest;
  const char c = GetCharUnderTest();
  message.append(1, c);
  message.append(kEnding);
  if (c == 9 || c == 10 || c == 13) {
    // valid char
    EXPECT_CALL(visitor_mock_,
                HandleError(BalsaFrameEnums::INVALID_HEADER_CHARACTER))
        .Times(0);
    balsa_frame_.ProcessInput(message.data(), message.size());
    EXPECT_FALSE(balsa_frame_.Error());
    EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  } else {
    // invalid char
    EXPECT_CALL(visitor_mock_,
                HandleError(BalsaFrameEnums::INVALID_HEADER_CHARACTER));
    balsa_frame_.ProcessInput(message.data(), message.size());
    EXPECT_TRUE(balsa_frame_.Error());
    EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  }
}

INSTANTIATE_TEST_SUITE_P(TestInvalidCharSet, HTTPBalsaFrameTestOneChar,
                         Range<char>(0, 32));

TEST_F(HTTPBalsaFrameTest, InvalidCharEndOfLine) {
  balsa_frame_.set_invalid_chars_level(BalsaFrame::InvalidCharsLevel::kError);
  const std::string kInvalid1 =
      "GET /foo HTTP/1.1\r\n"
      "Header-Key: headervalue\\x00\r\n"
      "Legit-Header: legitvalue\r\n\r\n";
  std::string message;
  absl::CUnescape(kInvalid1, &message);

  EXPECT_CALL(visitor_mock_,
              HandleError(BalsaFrameEnums::INVALID_HEADER_CHARACTER));
  balsa_frame_.ProcessInput(message.data(), message.size());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
}

TEST_F(HTTPBalsaFrameTest, InvalidCharInFirstLine) {
  balsa_frame_.set_invalid_chars_level(BalsaFrame::InvalidCharsLevel::kError);
  const std::string kInvalid1 =
      "GET /foo \\x00HTTP/1.1\r\n"
      "Legit-Header: legitvalue\r\n\r\n";
  std::string message;
  absl::CUnescape(kInvalid1, &message);

  EXPECT_CALL(visitor_mock_,
              HandleError(BalsaFrameEnums::INVALID_HEADER_CHARACTER));
  balsa_frame_.ProcessInput(message.data(), message.size());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
}

// Test gibberish in headers and trailer. GFE does not crash but garbage in
// garbage out.
TEST_F(HTTPBalsaFrameTest, GibberishInHeadersAndTrailer) {
  // Use static_cast<char> for values exceeding SCHAR_MAX to make sure this
  // compiles on platforms where char is signed.
  const char kGibberish1[] = {static_cast<char>(138), static_cast<char>(175),
                              static_cast<char>(233), 0};
  const char kGibberish2[] = {'?',
                              '?',
                              static_cast<char>(128),
                              static_cast<char>(255),
                              static_cast<char>(129),
                              static_cast<char>(254),
                              0};
  const char kGibberish3[] = "foo: bar : eeep : baz";

  std::string gibberish_headers =
      absl::StrCat(kGibberish1, ":", kGibberish2, "\r\n", kGibberish3, "\r\n");

  std::string headers = absl::StrCat(
      "HTTP/1.1 200 OK\r\n"
      "transfer-encoding: chunked\r\n",
      gibberish_headers, "\r\n");

  std::string chunks =
      "3\r\n"
      "123\r\n"
      "0\r\n";

  std::string trailer = absl::StrCat("k: v\n", gibberish_headers, "\n");

  balsa_frame_.set_is_request(false);

  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));
  ASSERT_EQ(chunks.size(),
            balsa_frame_.ProcessInput(chunks.data(), chunks.size()));

  FakeHeaders fake_trailers;
  fake_trailers.AddKeyValue("k", "v");
  fake_trailers.AddKeyValue(kGibberish1, kGibberish2);
  fake_trailers.AddKeyValue("foo", "bar : eeep : baz");
  EXPECT_CALL(visitor_mock_, OnTrailers(fake_trailers));
  ASSERT_EQ(trailer.size(),
            balsa_frame_.ProcessInput(trailer.data(), trailer.size()));

  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());

  // Transfer-encoding can be multi-valued, so GetHeader does not work.
  EXPECT_TRUE(headers_.transfer_encoding_is_chunked());
  absl::string_view field_value = headers_.GetHeader(kGibberish1);
  EXPECT_EQ(kGibberish2, field_value);
  field_value = headers_.GetHeader("foo");
  EXPECT_EQ("bar : eeep : baz", field_value);
}

// Note we reuse the header length limit because trailer is just multiple
// headers.
TEST_F(HTTPBalsaFrameTest, TrailerTooLong) {
  std::string headers =
      "HTTP/1.0 200 ok\r\n"
      "transfer-encoding: chunked\r\n"
      "\r\n";

  std::string chunks =
      "3\r\n"
      "123\r\n"
      "0\r\n";
  std::string trailer =
      "very : long trailer\n"
      "should:cause\r\n"
      "trailer :too long error\n"
      "\r\n";

  balsa_frame_.set_is_request(false);
  ASSERT_LT(headers.size(), trailer.size());
  balsa_frame_.set_max_header_length(headers.size());

  EXPECT_CALL(visitor_mock_, HandleError(BalsaFrameEnums::TRAILER_TOO_LONG));
  EXPECT_CALL(visitor_mock_, OnTrailers(_)).Times(0);
  EXPECT_CALL(visitor_mock_, MessageDone()).Times(0);
  ASSERT_EQ(headers.size(),
            balsa_frame_.ProcessInput(headers.data(), headers.size()));
  ASSERT_EQ(chunks.size(),
            balsa_frame_.ProcessInput(chunks.data(), chunks.size()));
  EXPECT_EQ(balsa_frame_.max_header_length(),
            balsa_frame_.ProcessInput(trailer.data(), trailer.size()));
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::TRAILER_TOO_LONG, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, Parse100ContinueNoContinueHeadersNoCallback) {
  std::string continue_headers =
      "HTTP/1.1 100 Continue\r\n"
      "\r\n";

  // Do not set continue headers (or use interim callbacks). Then the parsed
  // continue headers are treated as final headers.
  balsa_frame_.set_is_request(false);
  balsa_frame_.set_use_interim_headers_callback(false);

  InSequence s;
  EXPECT_CALL(visitor_mock_, HeaderDone());
  EXPECT_CALL(visitor_mock_, MessageDone());

  ASSERT_EQ(balsa_frame_.ProcessInput(continue_headers.data(),
                                      continue_headers.size()),
            continue_headers.size())
      << balsa_frame_.ErrorCode();
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(headers_.parsed_response_code(), 100);
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
}

TEST_F(HTTPBalsaFrameTest, Parse100Continue) {
  std::string continue_headers =
      "HTTP/1.1 100 Continue\r\n"
      "\r\n";

  // The parsed continue headers are delivered as interim headers.
  balsa_frame_.set_is_request(false);
  balsa_frame_.set_use_interim_headers_callback(true);

  InSequence s;
  EXPECT_CALL(visitor_mock_, OnInterimHeaders(Pointee(Property(
                                 &BalsaHeaders::parsed_response_code, 100))));
  EXPECT_CALL(visitor_mock_, HeaderDone()).Times(0);
  EXPECT_CALL(visitor_mock_, MessageDone()).Times(0);

  ASSERT_EQ(balsa_frame_.ProcessInput(continue_headers.data(),
                                      continue_headers.size()),
            continue_headers.size())
      << balsa_frame_.ErrorCode();
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(headers_.parsed_response_code(), 0u);
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
}

// Handle two sets of headers when set up properly and the first is 100
// Continue.
TEST_F(HTTPBalsaFrameTest, Support100ContinueNoCallback) {
  std::string initial_headers =
      "HTTP/1.1 100 Continue\r\n"
      "\r\n";
  std::string real_headers =
      "HTTP/1.1 200 OK\r\n"
      "content-length: 3\r\n"
      "\r\n";
  std::string body = "foo";

  balsa_frame_.set_is_request(false);
  BalsaHeaders continue_headers;
  balsa_frame_.set_continue_headers(&continue_headers);
  balsa_frame_.set_use_interim_headers_callback(false);

  ASSERT_EQ(initial_headers.size(),
            balsa_frame_.ProcessInput(initial_headers.data(),
                                      initial_headers.size()));
  ASSERT_EQ(real_headers.size(),
            balsa_frame_.ProcessInput(real_headers.data(), real_headers.size()))
      << balsa_frame_.ErrorCode();
  ASSERT_EQ(body.size(), balsa_frame_.ProcessInput(body.data(), body.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

// Handle two sets of headers when set up properly and the first is 100
// Continue.
TEST_F(HTTPBalsaFrameTest, Support100Continue) {
  std::string initial_headers =
      "HTTP/1.1 100 Continue\r\n"
      "\r\n";
  std::string real_headers =
      "HTTP/1.1 200 OK\r\n"
      "content-length: 3\r\n"
      "\r\n";
  std::string body = "foo";

  balsa_frame_.set_is_request(false);
  balsa_frame_.set_use_interim_headers_callback(true);

  InSequence s;
  EXPECT_CALL(visitor_mock_, OnInterimHeaders(Pointee(Property(
                                 &BalsaHeaders::parsed_response_code, 100))));
  ASSERT_EQ(
      balsa_frame_.ProcessInput(initial_headers.data(), initial_headers.size()),
      initial_headers.size());
  ASSERT_FALSE(balsa_frame_.Error());

  EXPECT_CALL(visitor_mock_, HeaderDone());
  ASSERT_EQ(balsa_frame_.ProcessInput(real_headers.data(), real_headers.size()),
            real_headers.size())
      << balsa_frame_.ErrorCode();
  EXPECT_EQ(headers_.parsed_response_code(), 200);

  EXPECT_CALL(visitor_mock_, MessageDone());
  ASSERT_EQ(balsa_frame_.ProcessInput(body.data(), body.size()), body.size());

  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(balsa_frame_.ErrorCode(), BalsaFrameEnums::BALSA_NO_ERROR);
}

// If both the interim headers callback and continue headers are set, only the
// former should be used.
TEST_F(HTTPBalsaFrameTest, InterimHeadersCallbackTakesPrecedence) {
  std::string initial_headers =
      "HTTP/1.1 100 Continue\r\n"
      "\r\n";
  std::string real_headers =
      "HTTP/1.1 200 OK\r\n"
      "content-length: 3\r\n"
      "\r\n";
  std::string body = "foo";

  balsa_frame_.set_is_request(false);
  BalsaHeaders continue_headers;
  balsa_frame_.set_continue_headers(&continue_headers);
  balsa_frame_.set_use_interim_headers_callback(true);

  InSequence s;
  EXPECT_CALL(visitor_mock_, OnInterimHeaders(Pointee(Property(
                                 &BalsaHeaders::parsed_response_code, 100))));
  EXPECT_CALL(visitor_mock_, ContinueHeaderDone).Times(0);
  ASSERT_EQ(
      balsa_frame_.ProcessInput(initial_headers.data(), initial_headers.size()),
      initial_headers.size());
  EXPECT_EQ(continue_headers.parsed_response_code(), 0u);
  ASSERT_FALSE(balsa_frame_.Error());

  EXPECT_CALL(visitor_mock_, HeaderDone());
  ASSERT_EQ(balsa_frame_.ProcessInput(real_headers.data(), real_headers.size()),
            real_headers.size())
      << balsa_frame_.ErrorCode();
  EXPECT_EQ(headers_.parsed_response_code(), 200);

  EXPECT_CALL(visitor_mock_, MessageDone());
  ASSERT_EQ(balsa_frame_.ProcessInput(body.data(), body.size()), body.size());

  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(balsa_frame_.ErrorCode(), BalsaFrameEnums::BALSA_NO_ERROR);
}

// Handle two sets of headers when set up properly and the first is 100
// Continue and it meets the conditions for b/62408297.
TEST_F(HTTPBalsaFrameTest, Support100Continue401UnauthorizedNoCallback) {
  std::string initial_headers =
      "HTTP/1.1 100 Continue\r\n"
      "\r\n";
  std::string real_headers =
      "HTTP/1.1 401 Unauthorized\r\n"
      "content-length: 3\r\n"
      "\r\n";
  std::string body = "foo";

  balsa_frame_.set_is_request(false);
  BalsaHeaders continue_headers;
  balsa_frame_.set_continue_headers(&continue_headers);
  balsa_frame_.set_use_interim_headers_callback(false);

  ASSERT_EQ(initial_headers.size(),
            balsa_frame_.ProcessInput(initial_headers.data(),
                                      initial_headers.size()));
  ASSERT_EQ(real_headers.size(),
            balsa_frame_.ProcessInput(real_headers.data(), real_headers.size()))
      << balsa_frame_.ErrorCode();
  ASSERT_EQ(body.size(), balsa_frame_.ProcessInput(body.data(), body.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

// Handle two sets of headers when set up properly and the first is 100
// Continue and it meets the conditions for b/62408297.
TEST_F(HTTPBalsaFrameTest, Support100Continue401Unauthorized) {
  std::string initial_headers =
      "HTTP/1.1 100 Continue\r\n"
      "\r\n";
  std::string real_headers =
      "HTTP/1.1 401 Unauthorized\r\n"
      "content-length: 3\r\n"
      "\r\n";
  std::string body = "foo";

  balsa_frame_.set_is_request(false);
  balsa_frame_.set_use_interim_headers_callback(true);

  InSequence s;
  EXPECT_CALL(visitor_mock_, OnInterimHeaders(Pointee(Property(
                                 &BalsaHeaders::parsed_response_code, 100))));
  ASSERT_EQ(
      balsa_frame_.ProcessInput(initial_headers.data(), initial_headers.size()),
      initial_headers.size());
  ASSERT_FALSE(balsa_frame_.Error());

  EXPECT_CALL(visitor_mock_, HeaderDone());
  ASSERT_EQ(balsa_frame_.ProcessInput(real_headers.data(), real_headers.size()),
            real_headers.size())
      << balsa_frame_.ErrorCode();
  EXPECT_EQ(headers_.parsed_response_code(), 401);

  EXPECT_CALL(visitor_mock_, MessageDone());
  ASSERT_EQ(balsa_frame_.ProcessInput(body.data(), body.size()), body.size());

  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(balsa_frame_.ErrorCode(), BalsaFrameEnums::BALSA_NO_ERROR);
}

TEST_F(HTTPBalsaFrameTest, Support100ContinueRunTogetherNoCallback) {
  std::string both_headers =
      "HTTP/1.1 100 Continue\r\n"
      "\r\n"
      "HTTP/1.1 200 OK\r\n"
      "content-length: 3\r\n"
      "\r\n";
  std::string body = "foo";

  {
    InSequence s;
    EXPECT_CALL(visitor_mock_, ContinueHeaderDone());
    EXPECT_CALL(visitor_mock_, HeaderDone());
    EXPECT_CALL(visitor_mock_, MessageDone());
  }

  balsa_frame_.set_is_request(false);
  BalsaHeaders continue_headers;
  balsa_frame_.set_continue_headers(&continue_headers);
  balsa_frame_.set_use_interim_headers_callback(false);

  ASSERT_EQ(both_headers.size(),
            balsa_frame_.ProcessInput(both_headers.data(), both_headers.size()))
      << balsa_frame_.ErrorCode();
  ASSERT_EQ(body.size(), balsa_frame_.ProcessInput(body.data(), body.size()));
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

TEST_F(HTTPBalsaFrameTest, Support100ContinueRunTogether) {
  std::string both_headers =
      "HTTP/1.1 100 Continue\r\n"
      "\r\n"
      "HTTP/1.1 200 OK\r\n"
      "content-length: 3\r\n"
      "\r\n";
  std::string body = "foo";

  balsa_frame_.set_is_request(false);
  balsa_frame_.set_use_interim_headers_callback(true);

  InSequence s;
  EXPECT_CALL(visitor_mock_, OnInterimHeaders(Pointee(Property(
                                 &BalsaHeaders::parsed_response_code, 100))));
  EXPECT_CALL(visitor_mock_, HeaderDone());

  ASSERT_EQ(balsa_frame_.ProcessInput(both_headers.data(), both_headers.size()),
            both_headers.size())
      << balsa_frame_.ErrorCode();
  ASSERT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(headers_.parsed_response_code(), 200);

  EXPECT_CALL(visitor_mock_, MessageDone());
  ASSERT_EQ(balsa_frame_.ProcessInput(body.data(), body.size()), body.size());
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(balsa_frame_.ErrorCode(), BalsaFrameEnums::BALSA_NO_ERROR);
}

TEST_F(HTTPBalsaFrameTest, MultipleInterimHeaders) {
  std::string all_headers =
      "HTTP/1.1 100 Continue\r\n"
      "\r\n"
      "HTTP/1.1 103 Early Hints\r\n"
      "\r\n"
      "HTTP/1.1 200 OK\r\n"
      "content-length: 3\r\n"
      "\r\n";
  std::string body = "foo";

  balsa_frame_.set_is_request(false);
  balsa_frame_.set_use_interim_headers_callback(true);

  InSequence s;
  EXPECT_CALL(visitor_mock_, OnInterimHeaders(Pointee(Property(
                                 &BalsaHeaders::parsed_response_code, 100))));
  EXPECT_CALL(visitor_mock_, OnInterimHeaders(Pointee(Property(
                                 &BalsaHeaders::parsed_response_code, 103))));
  EXPECT_CALL(visitor_mock_, HeaderDone());

  ASSERT_EQ(balsa_frame_.ProcessInput(all_headers.data(), all_headers.size()),
            all_headers.size())
      << balsa_frame_.ErrorCode();
  ASSERT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(headers_.parsed_response_code(), 200);

  EXPECT_CALL(visitor_mock_, MessageDone());
  ASSERT_EQ(balsa_frame_.ProcessInput(body.data(), body.size()), body.size());
  EXPECT_TRUE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(balsa_frame_.ErrorCode(), BalsaFrameEnums::BALSA_NO_ERROR);
}

TEST_F(HTTPBalsaFrameTest, SwitchingProtocols) {
  const std::string headers =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "\r\n";
  const std::string body = "Bytes for the new protocol";
  const std::string message = absl::StrCat(headers, body);

  // Even with the interim headers callback set, the 101 response is delivered
  // as final response headers.
  balsa_frame_.set_is_request(false);
  balsa_frame_.set_use_interim_headers_callback(true);

  InSequence s;
  EXPECT_CALL(visitor_mock_, ProcessHeaders);
  EXPECT_CALL(visitor_mock_, HeaderDone());

  ASSERT_EQ(balsa_frame_.ProcessInput(message.data(), message.size()),
            headers.size())
      << balsa_frame_.ErrorCode();
  ASSERT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(headers_.parsed_response_code(), 101);

  balsa_frame_.AllowArbitraryBody();

  EXPECT_CALL(visitor_mock_, OnRawBodyInput("Bytes for the new protocol"));
  EXPECT_CALL(visitor_mock_, OnBodyChunkInput("Bytes for the new protocol"));
  EXPECT_CALL(visitor_mock_, MessageDone()).Times(0);

  ASSERT_EQ(balsa_frame_.ProcessInput(body.data(), body.size()), body.size());
  EXPECT_FALSE(balsa_frame_.MessageFullyRead());
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(balsa_frame_.ErrorCode(), BalsaFrameEnums::BALSA_NO_ERROR);
}

TEST_F(HTTPBalsaFrameTest, Http09) {
  constexpr absl::string_view request = "GET /\r\n";

  InSequence s;
  StrictMock<BalsaVisitorMock> visitor_mock;
  balsa_frame_.set_balsa_visitor(&visitor_mock);

  EXPECT_CALL(
      visitor_mock,
      HandleWarning(
          BalsaFrameEnums::FAILED_TO_FIND_WS_AFTER_REQUEST_REQUEST_URI));
  EXPECT_CALL(visitor_mock, OnRequestFirstLineInput("GET /", "GET", "/", ""));
  EXPECT_CALL(visitor_mock, OnHeaderInput(request));
  EXPECT_CALL(visitor_mock, ProcessHeaders(FakeHeaders{}));
  EXPECT_CALL(visitor_mock, HeaderDone());
  EXPECT_CALL(visitor_mock, MessageDone());

  EXPECT_EQ(request.size(),
            balsa_frame_.ProcessInput(request.data(), request.size()));

  // HTTP/0.9 request is parsed with a warning.
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::FAILED_TO_FIND_WS_AFTER_REQUEST_REQUEST_URI,
            balsa_frame_.ErrorCode());
}

// A.k.a., ObsFoldAllowed.
TEST_F(HTTPBalsaFrameTest, ContinuationAllowed) {
  // See RFC7230 Section 3.2 for the definition of obs-fold:
  // https://httpwg.org/specs/rfc7230.html#header.fields.
  const std::string message =
      "GET / HTTP/1.1\r\n"
      "key1: \n value starts with obs-fold\r\n"
      "key2: value\n includes obs-fold\r\n"
      "key3: value ends in obs-fold \n \r\n"
      "\r\n";

  // TODO(b/314138604): RFC9110 Section 5.5 requires received CR, LF and NUL
  // characters to be replaced with SP, see
  // https://www.rfc-editor.org/rfc/rfc9110.html#name-field-values.
  // BalsaFrame currently strips (instead of replacing) CR and LF if the value
  // starts or ends with obs-fold, and keeps them if they occur in the middle.
  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("key1", "value starts with obs-fold");
  fake_headers.AddKeyValue("key2", "value\n includes obs-fold");
  fake_headers.AddKeyValue("key3", "value ends in obs-fold");
  EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
  EXPECT_CALL(visitor_mock_,
              HandleWarning(BalsaFrameEnums::OBS_FOLD_IN_HEADERS))
      .Times(3);

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.Error());
}

// A.k.a., ObsFoldDisallowed.
TEST_F(HTTPBalsaFrameTest, ContinuationDisallowed) {
  HttpValidationPolicy http_validation_policy;
  http_validation_policy.disallow_header_continuation_lines = true;
  balsa_frame_.set_http_validation_policy(http_validation_policy);

  const std::string message =
      "GET / HTTP/1.1\r\n"
      "key: value\n includes obs-fold\r\n"
      "\r\n";
  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::INVALID_HEADER_FORMAT, balsa_frame_.ErrorCode());
}

// Tests that continuation lines are sanitized according to RFC7230 Section
// 3.2.4 when the sanitize_obs_fold_in_header_values policy is enabled.
TEST_F(HTTPBalsaFrameTest, ContinuationLinesSanitized) {
  HttpValidationPolicy http_validation_policy;
  http_validation_policy.sanitize_obs_fold_in_header_values = true;
  balsa_frame_.set_http_validation_policy(http_validation_policy);

  const std::string message =
      "GET / HTTP/1.1\r\n"
      "key1: obs-\n fold\r\n"
      "key2: obs-\r\n fold\r\n"
      "key3: obs-\n\tfold\r\n"
      "key4: obs-\r\n\tfold\r\n"
      "key5: obs-\n   fold\r\n"
      "key6: obs-\r\n   fold\r\n"
      "key7: obs-\n \tfold\r\n"
      "key8: obs-\r\n \tfold\r\n"
      "\r\n";

  FakeHeaders fake_headers;
  // The number of spaces in the header value is not important, but is equal to
  // the number of whitespace characters in the original header value.
  fake_headers.AddKeyValue("key1", "obs-  fold");
  fake_headers.AddKeyValue("key2", "obs-   fold");
  fake_headers.AddKeyValue("key3", "obs-  fold");
  fake_headers.AddKeyValue("key4", "obs-   fold");
  fake_headers.AddKeyValue("key5", "obs-    fold");
  fake_headers.AddKeyValue("key6", "obs-     fold");
  fake_headers.AddKeyValue("key7", "obs-   fold");
  fake_headers.AddKeyValue("key8", "obs-    fold");
  EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));
  EXPECT_CALL(visitor_mock_,
              HandleWarning(BalsaFrameEnums::OBS_FOLD_IN_HEADERS))
      .Times(8);

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
}

TEST_F(HTTPBalsaFrameTest, NullAtBeginningOrEndOfValue) {
  balsa_frame_.set_invalid_chars_level(BalsaFrame::InvalidCharsLevel::kError);

  constexpr absl::string_view null_string("\0", 1);
  const std::string message =
      absl::StrCat("GET / HTTP/1.1\r\n",                                 //
                   "key1: ", null_string, "value starts with null\r\n",  //
                   "key2: value ends in null", null_string, "\r\n",      //
                   "\r\n");

  // TODO(b/314138604): RFC9110 Section 5.5 requires received CR, LF and NUL
  // characters to be replaced with SP if the message is not rejected, see
  // https://www.rfc-editor.org/rfc/rfc9110.html#name-field-values.
  // BalsaFrame currently strips (instead of replacing) NUL at the beginning or
  // end of the header value.
  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("key1", "value starts with null");
  fake_headers.AddKeyValue("key2", "value ends in null");
  EXPECT_CALL(visitor_mock_,
              HandleError(BalsaFrameEnums::INVALID_HEADER_CHARACTER));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.Error());
}

TEST_F(HTTPBalsaFrameTest, NullInMiddleOfValue) {
  balsa_frame_.set_invalid_chars_level(BalsaFrame::InvalidCharsLevel::kError);

  constexpr absl::string_view null_string("\0", 1);
  const std::string message =
      absl::StrCat("GET / HTTP/1.1\r\n",                             //
                   "key: value ", null_string, "includes null\r\n",  //
                   "\r\n");

  // TODO(b/314138604): RFC9110 Section 5.5 requires received CR, LF and NUL
  // characters to be replaced with SP if the message is not rejected, see
  // https://www.rfc-editor.org/rfc/rfc9110.html#name-field-values.
  // BalsaFrame currently keeps the NUL character if it occurs in the middle.
  FakeHeaders fake_headers;
  fake_headers.AddKeyValue(
      "key", absl::StrCat("value ", null_string, "includes null"));
  EXPECT_CALL(visitor_mock_,
              HandleError(BalsaFrameEnums::INVALID_HEADER_CHARACTER));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.Error());
}

TEST_F(HTTPBalsaFrameTest, ObsTextNotFoundIfNotPresent) {
  HttpValidationPolicy http_validation_policy;
  http_validation_policy.disallow_obs_text_in_field_names = true;
  balsa_frame_.set_http_validation_policy(http_validation_policy);

  const std::string message =
      absl::StrCat("GET / HTTP/1.1\r\n",                       //
                   "key1: key does not contain obs-text\r\n",  //
                   "\r\n");

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("key1", "key does not contain obs-text");
  EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.Error());
}

TEST_F(HTTPBalsaFrameTest, HeaderFieldNameWithObsTextButPolicyDisabled) {
  HttpValidationPolicy http_validation_policy;
  http_validation_policy.disallow_obs_text_in_field_names = false;
  balsa_frame_.set_http_validation_policy(http_validation_policy);

  // The InvalidCharsLevel does not affect whether obs-text is rejected.
  balsa_frame_.set_invalid_chars_level(BalsaFrame::InvalidCharsLevel::kError);

  const std::string message =
      absl::StrCat("GET / HTTP/1.1\r\n",                      //
                   "\x80key1: key starts with obs-text\r\n",  //
                   "\r\n");

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("\x80key1", "key starts with obs-text");
  EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.Error());
}

TEST_F(HTTPBalsaFrameTest, HeaderFieldNameWithObsTextAndPolicyEnabled) {
  HttpValidationPolicy http_validation_policy;
  http_validation_policy.disallow_obs_text_in_field_names = true;
  balsa_frame_.set_http_validation_policy(http_validation_policy);

  // The InvalidCharsLevel does not affect whether obs-text is rejected.
  balsa_frame_.set_invalid_chars_level(BalsaFrame::InvalidCharsLevel::kOff);

  const std::string message =
      absl::StrCat("GET / HTTP/1.1\r\n",                      //
                   "\x80key1: key starts with obs-text\r\n",  //
                   "\r\n");

  EXPECT_CALL(visitor_mock_,
              HandleError(BalsaFrameEnums::INVALID_HEADER_NAME_CHARACTER));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.Error());
}

TEST_F(HTTPBalsaFrameTest, HeaderFieldNameWithObsTextAtEndRejected) {
  HttpValidationPolicy http_validation_policy;
  http_validation_policy.disallow_obs_text_in_field_names = true;
  balsa_frame_.set_http_validation_policy(http_validation_policy);

  const std::string message =
      absl::StrCat("GET / HTTP/1.1\r\n",                    //
                   "key1\x93: key ends with obs-text\r\n",  //
                   "\r\n");

  EXPECT_CALL(visitor_mock_,
              HandleError(BalsaFrameEnums::INVALID_HEADER_NAME_CHARACTER));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.Error());
}

TEST_F(HTTPBalsaFrameTest, HeaderFieldNameWithObsTextInMiddleRejected) {
  HttpValidationPolicy http_validation_policy;
  http_validation_policy.disallow_obs_text_in_field_names = true;
  balsa_frame_.set_http_validation_policy(http_validation_policy);

  const std::string message =
      absl::StrCat("GET / HTTP/1.1\r\n",                             //
                   "ke\xffy1: key contains obs-text in middle\r\n",  //
                   "\r\n");

  EXPECT_CALL(visitor_mock_,
              HandleError(BalsaFrameEnums::INVALID_HEADER_NAME_CHARACTER));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_TRUE(balsa_frame_.Error());
}

// This case is specifically allowed by RFC 9112 Section 4.
TEST_F(HTTPBalsaFrameTest, ObsTextInReasonPhraseAllowed) {
  HttpValidationPolicy http_validation_policy;
  http_validation_policy.disallow_obs_text_in_field_names = true;
  balsa_frame_.set_http_validation_policy(http_validation_policy);

  balsa_frame_.set_invalid_chars_level(BalsaFrame::InvalidCharsLevel::kError);

  balsa_frame_.set_is_request(false);

  const std::string message =
      absl::StrCat("HTTP/1.1 200 O\x90K\r\n",                            //
                   "surprising: obs-text allowed in reason phrase\r\n",  //
                   "content-length: 0\r\n"                               //
                   "\r\n");

  FakeHeaders fake_headers;
  fake_headers.AddKeyValue("surprising", "obs-text allowed in reason phrase");
  fake_headers.AddKeyValue("content-length", "0");
  EXPECT_CALL(visitor_mock_, ProcessHeaders(fake_headers));

  EXPECT_EQ(message.size(),
            balsa_frame_.ProcessInput(message.data(), message.size()));
  EXPECT_FALSE(balsa_frame_.Error());
  EXPECT_EQ(BalsaFrameEnums::BALSA_NO_ERROR, balsa_frame_.ErrorCode());
}

}  // namespace
}  // namespace quiche::test
