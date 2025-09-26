#include "quiche/http2/adapter/noop_header_validator.h"

#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace http2 {
namespace adapter {
namespace test {

using Header = std::pair<absl::string_view, absl::string_view>;
constexpr Header kSampleRequestPseudoheaders[] = {{":authority", "www.foo.com"},
                                                  {":method", "GET"},
                                                  {":path", "/foo"},
                                                  {":scheme", "https"}};

TEST(NoopHeaderValidatorTest, HeaderNameEmpty) {
  NoopHeaderValidator v;
  NoopHeaderValidator::HeaderStatus status =
      v.ValidateSingleHeader("", "value");
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK, status);
}

TEST(NoopHeaderValidatorTest, HeaderValueEmpty) {
  NoopHeaderValidator v;
  NoopHeaderValidator::HeaderStatus status = v.ValidateSingleHeader("name", "");
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK, status);
}

TEST(NoopHeaderValidatorTest, ExceedsMaxSize) {
  NoopHeaderValidator v;
  v.SetMaxFieldSize(64u);
  NoopHeaderValidator::HeaderStatus status =
      v.ValidateSingleHeader("name", "value");
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK, status);
  status = v.ValidateSingleHeader(
      "name2",
      "Antidisestablishmentariansism is supercalifragilisticexpialodocious.");
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK, status);
}

TEST(NoopHeaderValidatorTest, AnyNameCharIsValid) {
  NoopHeaderValidator v;
  char pseudo_name[] = ":met hod";
  char name[] = "na me";
  for (int i = std::numeric_limits<char>::min();
       i < std::numeric_limits<char>::max(); ++i) {
    char c = static_cast<char>(i);
    // Test a pseudo-header name with this char.
    pseudo_name[3] = c;
    auto sv = absl::string_view(pseudo_name, 8);
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(sv, "value"));
    // Test a regular header name with this char.
    name[2] = c;
    sv = absl::string_view(name, 5);
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(sv, "value"));
  }
}

TEST(NoopHeaderValidatorTest, AnyValueCharIsValid) {
  NoopHeaderValidator v;
  char value[] = "val ue";
  for (int i = std::numeric_limits<char>::min();
       i < std::numeric_limits<char>::max(); ++i) {
    char c = static_cast<char>(i);
    value[3] = c;
    auto sv = absl::string_view(value, 6);
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader("name", sv));
  }
}

TEST(NoopHeaderValidatorTest, AnyStatusIsValid) {
  NoopHeaderValidator v;

  for (HeaderType type : {HeaderType::RESPONSE, HeaderType::RESPONSE_100}) {
    v.StartHeaderBlock();
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":status", "bar"));
    EXPECT_TRUE(v.FinishHeaderBlock(type));

    v.StartHeaderBlock();
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":status", "10"));
    EXPECT_TRUE(v.FinishHeaderBlock(type));

    v.StartHeaderBlock();
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":status", "9000"));
    EXPECT_TRUE(v.FinishHeaderBlock(type));

    v.StartHeaderBlock();
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":status", "400"));
    EXPECT_TRUE(v.FinishHeaderBlock(type));
  }
}

TEST(NoopHeaderValidatorTest, AnyAuthorityCharIsValid) {
  char value[] = "ho st.example.com";
  for (int i = std::numeric_limits<char>::min();
       i < std::numeric_limits<char>::max(); ++i) {
    char c = static_cast<char>(i);
    value[2] = c;
    auto sv = absl::string_view(value, 17);
    for (absl::string_view key : {":authority", "host"}) {
      NoopHeaderValidator v;
      v.StartHeaderBlock();
      EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(key, sv));
    }
  }
}

TEST(NoopHeaderValidatorTest, RequestHostAndAuthority) {
  NoopHeaderValidator v;
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(to_add.first, to_add.second));
  }
  // If both "host" and ":authority" have the same value, validation succeeds.
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("host", "www.foo.com"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));

  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(to_add.first, to_add.second));
  }
  // If "host" and ":authority" have different values, validation still
  // succeeds.
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("host", "www.bar.com"));
}

TEST(NoopHeaderValidatorTest, RequestPseudoHeaders) {
  NoopHeaderValidator v;
  for (Header to_skip : kSampleRequestPseudoheaders) {
    v.StartHeaderBlock();
    for (Header to_add : kSampleRequestPseudoheaders) {
      if (to_add != to_skip) {
        EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
                  v.ValidateSingleHeader(to_add.first, to_add.second));
      }
    }
    // Even if a pseudo-header is missing, final validation will succeed.
    EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));
  }

  // When all pseudo-headers are present, final validation will succeed.
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(to_add.first, to_add.second));
  }
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));

  // When an extra pseudo-header is present, final validation will still
  // succeed.
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(to_add.first, to_add.second));
  }
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":extra", "blah"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));

  // When a required pseudo-header is repeated, final validation will succeed.
  for (Header to_repeat : kSampleRequestPseudoheaders) {
    v.StartHeaderBlock();
    for (Header to_add : kSampleRequestPseudoheaders) {
      EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
      if (to_add == to_repeat) {
        EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
                  v.ValidateSingleHeader(to_add.first, to_add.second));
      }
    }
    EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));
  }
}

TEST(NoopHeaderValidatorTest, WebsocketPseudoHeaders) {
  NoopHeaderValidator v;
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(to_add.first, to_add.second));
  }
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":protocol", "websocket"));
  // Validation always succeeds.
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));

  // This is a no-op for NoopHeaderValidator.
  v.SetAllowExtendedConnect();

  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(to_add.first, to_add.second));
  }
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":protocol", "websocket"));
  // The validator does not check for a CONNECT request.
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));

  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    if (to_add.first == ":method") {
      EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, "CONNECT"));
    } else {
      EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
    }
  }
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":protocol", "websocket"));
  // After allowing the method, `:protocol` is acepted for CONNECT requests.
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));
}

TEST(NoopHeaderValidatorTest, AsteriskPathPseudoHeader) {
  NoopHeaderValidator v;

  // The validator does not perform any path validation.
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    if (to_add.first == ":path") {
      EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, "*"));
    } else {
      EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
    }
  }
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));

  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    if (to_add.first == ":path") {
      EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, "*"));
    } else if (to_add.first == ":method") {
      EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, "OPTIONS"));
    } else {
      EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
    }
  }
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));
}

TEST(NoopHeaderValidatorTest, InvalidPathPseudoHeader) {
  NoopHeaderValidator v;

  // An empty path is allowed.
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    if (to_add.first == ":path") {
      EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, ""));
    } else {
      EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
    }
  }
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));

  // A path that does not start with a slash is allowed.
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    if (to_add.first == ":path") {
      EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, "shawarma"));
    } else {
      EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
    }
  }
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));
}

TEST(NoopHeaderValidatorTest, ResponsePseudoHeaders) {
  NoopHeaderValidator v;

  for (HeaderType type : {HeaderType::RESPONSE, HeaderType::RESPONSE_100}) {
    // When `:status` is missing, validation succeeds.
    v.StartHeaderBlock();
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader("foo", "bar"));
    EXPECT_TRUE(v.FinishHeaderBlock(type));

    // When all pseudo-headers are present, final validation succeeds.
    v.StartHeaderBlock();
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":status", "199"));
    EXPECT_TRUE(v.FinishHeaderBlock(type));
    EXPECT_EQ("199", v.status_header());

    // When `:status` is repeated, validation succeeds.
    v.StartHeaderBlock();
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":status", "199"));
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":status", "299"));
    EXPECT_TRUE(v.FinishHeaderBlock(type));

    // When an extra pseudo-header is present, final validation succeeds.
    v.StartHeaderBlock();
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":status", "199"));
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":extra", "blorp"));
    EXPECT_TRUE(v.FinishHeaderBlock(type));
  }
}

TEST(NoopHeaderValidatorTest, ResponseWithHost) {
  NoopHeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "200"));
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("host", "myserver.com"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::RESPONSE));
}

TEST(NoopHeaderValidatorTest, Response204) {
  NoopHeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "204"));
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("x-content", "is not present"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::RESPONSE));
}

TEST(NoopHeaderValidatorTest, ResponseWithMultipleIdenticalContentLength) {
  NoopHeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "200"));
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "13"));
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "13"));
}

TEST(NoopHeaderValidatorTest, ResponseWithMultipleDifferingContentLength) {
  NoopHeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "200"));
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "13"));
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "17"));
}

TEST(NoopHeaderValidatorTest, Response204WithContentLengthZero) {
  NoopHeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "204"));
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("x-content", "is not present"));
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "0"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::RESPONSE));
}

TEST(NoopHeaderValidatorTest, Response204WithContentLength) {
  NoopHeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "204"));
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("x-content", "is not present"));
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "1"));
}

TEST(NoopHeaderValidatorTest, Response100) {
  NoopHeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "100"));
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("x-content", "is not present"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::RESPONSE));
}

TEST(NoopHeaderValidatorTest, Response100WithContentLengthZero) {
  NoopHeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "100"));
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("x-content", "is not present"));
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "0"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::RESPONSE));
}

TEST(NoopHeaderValidatorTest, Response100WithContentLength) {
  NoopHeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "100"));
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("x-content", "is not present"));
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "1"));
}

TEST(NoopHeaderValidatorTest, ResponseTrailerPseudoHeaders) {
  NoopHeaderValidator v;

  // When no pseudo-headers are present, validation will succeed.
  v.StartHeaderBlock();
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("foo", "bar"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::RESPONSE_TRAILER));

  // When a pseudo-header is present, validation will succeed.
  v.StartHeaderBlock();
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "200"));
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("foo", "bar"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::RESPONSE_TRAILER));
}

TEST(NoopHeaderValidatorTest, ValidContentLength) {
  NoopHeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(v.content_length(), std::nullopt);
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "41"));
  EXPECT_EQ(v.content_length(), std::nullopt);

  v.StartHeaderBlock();
  EXPECT_EQ(v.content_length(), std::nullopt);
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "42"));
  EXPECT_EQ(v.content_length(), std::nullopt);
}

TEST(NoopHeaderValidatorTest, InvalidContentLength) {
  NoopHeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(v.content_length(), std::nullopt);
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", ""));
  EXPECT_EQ(v.content_length(), std::nullopt);
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "nan"));
  EXPECT_EQ(v.content_length(), std::nullopt);
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "-42"));
  EXPECT_EQ(v.content_length(), std::nullopt);
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "42"));
  EXPECT_EQ(v.content_length(), std::nullopt);
}

TEST(NoopHeaderValidatorTest, TeHeader) {
  NoopHeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("te", "trailers"));

  v.StartHeaderBlock();
  EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("te", "trailers, deflate"));
}

TEST(NoopHeaderValidatorTest, ConnectionSpecificHeaders) {
  const std::vector<Header> connection_headers = {
      {"connection", "keep-alive"}, {"proxy-connection", "keep-alive"},
      {"keep-alive", "timeout=42"}, {"transfer-encoding", "chunked"},
      {"upgrade", "h2c"},
  };
  for (const auto& [connection_key, connection_value] : connection_headers) {
    NoopHeaderValidator v;
    v.StartHeaderBlock();
    for (const auto& [sample_key, sample_value] : kSampleRequestPseudoheaders) {
      EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(sample_key, sample_value));
    }
    EXPECT_EQ(NoopHeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(connection_key, connection_value));
  }
}

}  // namespace test
}  // namespace adapter
}  // namespace http2
