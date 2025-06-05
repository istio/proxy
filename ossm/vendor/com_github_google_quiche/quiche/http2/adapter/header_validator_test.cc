#include "quiche/http2/adapter/header_validator.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace http2 {
namespace adapter {
namespace test {

using ::testing::Optional;

using Header = std::pair<absl::string_view, absl::string_view>;
constexpr Header kSampleRequestPseudoheaders[] = {{":authority", "www.foo.com"},
                                                  {":method", "GET"},
                                                  {":path", "/foo"},
                                                  {":scheme", "https"}};

TEST(HeaderValidatorTest, HeaderNameEmpty) {
  HeaderValidator v;
  HeaderValidator::HeaderStatus status = v.ValidateSingleHeader("", "value");
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID, status);
}

TEST(HeaderValidatorTest, HeaderValueEmpty) {
  HeaderValidator v;
  HeaderValidator::HeaderStatus status = v.ValidateSingleHeader("name", "");
  EXPECT_EQ(HeaderValidator::HEADER_OK, status);
}

TEST(HeaderValidatorTest, ExceedsMaxSize) {
  HeaderValidator v;
  v.SetMaxFieldSize(64u);
  HeaderValidator::HeaderStatus status =
      v.ValidateSingleHeader("name", "value");
  EXPECT_EQ(HeaderValidator::HEADER_OK, status);
  status = v.ValidateSingleHeader(
      "name2",
      "Antidisestablishmentariansism is supercalifragilisticexpialodocious.");
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_TOO_LONG, status);
}

TEST(HeaderValidatorTest, NameHasInvalidChar) {
  HeaderValidator v;
  for (const bool is_pseudo_header : {true, false}) {
    // These characters should be allowed. (Not exhaustive.)
    for (const char* c : {"!", "3", "a", "_", "|", "~"}) {
      const std::string name = is_pseudo_header ? absl::StrCat(":met", c, "hod")
                                                : absl::StrCat("na", c, "me");
      HeaderValidator::HeaderStatus status =
          v.ValidateSingleHeader(name, "value");
      EXPECT_EQ(HeaderValidator::HEADER_OK, status);
    }
    // These should not. (Not exhaustive.)
    for (const char* c : {"\\", "<", ";", "[", "=", " ", "\r", "\n", ",", "\"",
                          "\x1F", "\x91"}) {
      const std::string name = is_pseudo_header ? absl::StrCat(":met", c, "hod")
                                                : absl::StrCat("na", c, "me");
      HeaderValidator::HeaderStatus status =
          v.ValidateSingleHeader(name, "value");
      EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID, status)
          << "with name [" << name << "]";
    }
    // Test nul separately.
    {
      const absl::string_view name = is_pseudo_header
                                         ? absl::string_view(":met\0hod", 8)
                                         : absl::string_view("na\0me", 5);
      HeaderValidator::HeaderStatus status =
          v.ValidateSingleHeader(name, "value");
      EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID, status);
    }
    // Uppercase characters in header names should not be allowed.
    const std::string uc_name = is_pseudo_header ? ":Method" : "Name";
    HeaderValidator::HeaderStatus status =
        v.ValidateSingleHeader(uc_name, "value");
    EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID, status);
  }
}

TEST(HeaderValidatorTest, ValueHasInvalidChar) {
  HeaderValidator v;
  // These characters should be allowed. (Not exhaustive.)
  for (const char* c :
       {"!", "3", "a", "_", "|", "~", "\\", "<", ";", "[", "=", "A", "\t"}) {
    const std::string value = absl::StrCat("val", c, "ue");
    EXPECT_TRUE(
        HeaderValidator::IsValidHeaderValue(value, ObsTextOption::kDisallow));
    HeaderValidator::HeaderStatus status =
        v.ValidateSingleHeader("name", value);
    EXPECT_EQ(HeaderValidator::HEADER_OK, status);
  }
  // These should not.
  for (const char* c : {"\r", "\n"}) {
    const std::string value = absl::StrCat("val", c, "ue");
    EXPECT_FALSE(
        HeaderValidator::IsValidHeaderValue(value, ObsTextOption::kDisallow));
    HeaderValidator::HeaderStatus status =
        v.ValidateSingleHeader("name", value);
    EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID, status);
  }
  // Test nul separately.
  {
    const std::string value("val\0ue", 6);
    EXPECT_FALSE(
        HeaderValidator::IsValidHeaderValue(value, ObsTextOption::kDisallow));
    HeaderValidator::HeaderStatus status =
        v.ValidateSingleHeader("name", value);
    EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID, status);
  }
  {
    const std::string obs_text_value = "val\xa9ue";
    // Test that obs-text is disallowed by default.
    EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
              v.ValidateSingleHeader("name", obs_text_value));
    // Test that obs-text is disallowed when configured.
    v.SetObsTextOption(ObsTextOption::kDisallow);
    EXPECT_FALSE(HeaderValidator::IsValidHeaderValue(obs_text_value,
                                                     ObsTextOption::kDisallow));
    EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
              v.ValidateSingleHeader("name", obs_text_value));
    // Test that obs-text is allowed when configured.
    v.SetObsTextOption(ObsTextOption::kAllow);
    EXPECT_TRUE(HeaderValidator::IsValidHeaderValue(obs_text_value,
                                                    ObsTextOption::kAllow));
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader("name", obs_text_value));
  }
}

TEST(HeaderValidatorTest, StatusHasInvalidChar) {
  HeaderValidator v;

  for (HeaderType type : {HeaderType::RESPONSE, HeaderType::RESPONSE_100}) {
    // When `:status` has a non-digit value, validation will fail.
    v.StartHeaderBlock();
    EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
              v.ValidateSingleHeader(":status", "bar"));
    EXPECT_FALSE(v.FinishHeaderBlock(type));

    // When `:status` is too short, validation will fail.
    v.StartHeaderBlock();
    EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
              v.ValidateSingleHeader(":status", "10"));
    EXPECT_FALSE(v.FinishHeaderBlock(type));

    // When `:status` is too long, validation will fail.
    v.StartHeaderBlock();
    EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
              v.ValidateSingleHeader(":status", "9000"));
    EXPECT_FALSE(v.FinishHeaderBlock(type));

    // When `:status` is just right, validation will succeed.
    v.StartHeaderBlock();
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":status", "400"));
    EXPECT_TRUE(v.FinishHeaderBlock(type));
  }
}

TEST(HeaderValidatorTest, AuthorityHasInvalidChar) {
  for (absl::string_view key : {":authority", "host"}) {
    // These characters should be allowed. (Not exhaustive.)
    for (const absl::string_view c : {"1", "-", "!", ":", "+", "=", ","}) {
      const std::string value = absl::StrCat("ho", c, "st.example.com");
      EXPECT_TRUE(HeaderValidator::IsValidAuthority(value));

      HeaderValidator v;
      v.StartHeaderBlock();
      HeaderValidator::HeaderStatus status = v.ValidateSingleHeader(key, value);
      EXPECT_EQ(HeaderValidator::HEADER_OK, status)
          << " with name [" << key << "] and value [" << value << "]";
    }
    // These should not.
    for (const absl::string_view c : {"\r", "\n", "|", "\\", "`"}) {
      const std::string value = absl::StrCat("ho", c, "st.example.com");
      EXPECT_FALSE(HeaderValidator::IsValidAuthority(value));

      HeaderValidator v;
      v.StartHeaderBlock();
      HeaderValidator::HeaderStatus status = v.ValidateSingleHeader(key, value);
      EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID, status);
    }

    {
      // IPv4 example
      const std::string value = "123.45.67.89";
      EXPECT_TRUE(HeaderValidator::IsValidAuthority(value));

      HeaderValidator v;
      v.StartHeaderBlock();
      HeaderValidator::HeaderStatus status = v.ValidateSingleHeader(key, value);
      EXPECT_EQ(HeaderValidator::HEADER_OK, status);
    }

    {
      // IPv6 examples
      const std::string value1 = "2001:0db8:85a3:0000:0000:8a2e:0370:7334";
      EXPECT_TRUE(HeaderValidator::IsValidAuthority(value1));

      HeaderValidator v;
      v.StartHeaderBlock();
      HeaderValidator::HeaderStatus status =
          v.ValidateSingleHeader(key, value1);
      EXPECT_EQ(HeaderValidator::HEADER_OK, status);

      const std::string value2 = "[::1]:80";
      EXPECT_TRUE(HeaderValidator::IsValidAuthority(value2));
      HeaderValidator v2;
      v2.StartHeaderBlock();
      status = v2.ValidateSingleHeader(key, value2);
      EXPECT_EQ(HeaderValidator::HEADER_OK, status);
    }

    {
      // Empty field
      EXPECT_TRUE(HeaderValidator::IsValidAuthority(""));

      HeaderValidator v;
      v.StartHeaderBlock();
      HeaderValidator::HeaderStatus status = v.ValidateSingleHeader(key, "");
      EXPECT_EQ(HeaderValidator::HEADER_OK, status);
    }
  }
}

TEST(HeaderValidatorTest, RequestHostAndAuthority) {
  HeaderValidator v;
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(to_add.first, to_add.second));
  }
  // If both "host" and ":authority" have the same value, validation succeeds.
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("host", "www.foo.com"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));

  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(to_add.first, to_add.second));
  }
  // If "host" and ":authority" have different values, validation fails.
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
            v.ValidateSingleHeader("host", "www.bar.com"));
}

TEST(HeaderValidatorTest, RequestHostAndAuthorityLax) {
  HeaderValidator v;
  v.SetAllowDifferentHostAndAuthority();
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(to_add.first, to_add.second));
  }
  // Since the option is set, validation succeeds even if "host" and
  // ":authority" have different values.
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("host", "www.bar.com"));
}

TEST(HeaderValidatorTest, MethodHasInvalidChar) {
  HeaderValidator v;
  v.StartHeaderBlock();

  std::vector<absl::string_view> bad_methods = {
      "In[]valid{}",   "co,mma", "spac e",     "a@t",    "equals=",
      "question?mark", "co:lon", "semi;colon", "sla/sh", "back\\slash",
  };

  std::vector<absl::string_view> good_methods = {
      "lowercase",   "MiXeDcAsE", "NONCANONICAL", "HASH#",
      "under_score", "PI|PE",     "Tilde~",       "quote'",
  };

  for (absl::string_view value : bad_methods) {
    v.StartHeaderBlock();
    EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
              v.ValidateSingleHeader(":method", value));
  }

  for (absl::string_view value : good_methods) {
    v.StartHeaderBlock();
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":method", value));
    for (Header to_add : kSampleRequestPseudoheaders) {
      if (to_add.first == ":method") {
        continue;
      }
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
    }
    EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));
  }
}

TEST(HeaderValidatorTest, RequestPseudoHeaders) {
  HeaderValidator v;
  for (Header to_skip : kSampleRequestPseudoheaders) {
    v.StartHeaderBlock();
    for (Header to_add : kSampleRequestPseudoheaders) {
      if (to_add != to_skip) {
        EXPECT_EQ(HeaderValidator::HEADER_OK,
                  v.ValidateSingleHeader(to_add.first, to_add.second));
      }
    }
    // When any pseudo-header is missing, final validation will fail.
    EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));
  }

  // When all pseudo-headers are present, final validation will succeed.
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(to_add.first, to_add.second));
  }
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));

  // When an extra pseudo-header is present, final validation will fail.
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(to_add.first, to_add.second));
  }
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":extra", "blah"));
  EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));

  // When a required pseudo-header is repeated, final validation will fail.
  for (Header to_repeat : kSampleRequestPseudoheaders) {
    v.StartHeaderBlock();
    for (Header to_add : kSampleRequestPseudoheaders) {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
      if (to_add == to_repeat) {
        EXPECT_EQ(HeaderValidator::HEADER_OK,
                  v.ValidateSingleHeader(to_add.first, to_add.second));
      }
    }
    EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));
  }
}

TEST(HeaderValidatorTest, ConnectHeaders) {
  // Too few headers.
  HeaderValidator v;
  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":authority", "athena.dialup.mit.edu:23"));
  EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":method", "CONNECT"));
  EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));

  // Too many headers.
  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":authority", "athena.dialup.mit.edu:23"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":method", "CONNECT"));
  EXPECT_EQ(HeaderValidator::HEADER_OK, v.ValidateSingleHeader(":path", "/"));
  EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));

  // Empty :authority
  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":authority", ""));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":method", "CONNECT"));
  EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));

  // Just right.
  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":authority", "athena.dialup.mit.edu:23"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":method", "CONNECT"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));

  v.SetAllowExtendedConnect();
  // "Classic" CONNECT headers should still be accepted.
  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":authority", "athena.dialup.mit.edu:23"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":method", "CONNECT"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));
}

TEST(HeaderValidatorTest, WebsocketPseudoHeaders) {
  HeaderValidator v;
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(to_add.first, to_add.second));
  }
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":protocol", "websocket"));
  // At this point, `:protocol` is treated as an extra pseudo-header.
  EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));

  // Future header blocks may send the `:protocol` pseudo-header for CONNECT
  // requests.
  v.SetAllowExtendedConnect();

  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(to_add.first, to_add.second));
  }
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":protocol", "websocket"));
  // The method is not "CONNECT", so `:protocol` is still treated as an extra
  // pseudo-header.
  EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));

  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    if (to_add.first == ":method") {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, "CONNECT"));
    } else {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
    }
  }
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":protocol", "websocket"));
  // After allowing the method, `:protocol` is acepted for CONNECT requests.
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));
}

TEST(HeaderValidatorTest, AsteriskPathPseudoHeader) {
  HeaderValidator v;

  // An asterisk :path should not be allowed for non-OPTIONS requests.
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    if (to_add.first == ":path") {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, "*"));
    } else {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
    }
  }
  EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));

  // An asterisk :path should be allowed for OPTIONS requests.
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    if (to_add.first == ":path") {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, "*"));
    } else if (to_add.first == ":method") {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, "OPTIONS"));
    } else {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
    }
  }
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));
}

TEST(HeaderValidatorTest, InvalidPathPseudoHeader) {
  HeaderValidator v;

  // An empty path should fail on single header validation and finish.
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    if (to_add.first == ":path") {
      EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
                v.ValidateSingleHeader(to_add.first, ""));
    } else {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
    }
  }
  EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));

  // A path that does not start with a slash should fail on finish.
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    if (to_add.first == ":path") {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, "shawarma"));
    } else {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
    }
  }
  EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));

  // Various valid path characters.
  for (const absl::string_view c : {"/", "?", "_", "'", "9", "&", "(", "@", ":",
                                    "<", ">", "\\", "[", "}", "`", "\\", "#"}) {
    const std::string value = absl::StrCat("/shawa", c, "rma");

    HeaderValidator validator;
    validator.StartHeaderBlock();
    for (Header to_add : kSampleRequestPseudoheaders) {
      if (to_add.first == ":path") {
        EXPECT_EQ(HeaderValidator::HEADER_OK,
                  validator.ValidateSingleHeader(to_add.first, value))
            << "Problematic char: [" << c << "]";
      } else {
        EXPECT_EQ(HeaderValidator::HEADER_OK,
                  validator.ValidateSingleHeader(to_add.first, to_add.second));
      }
    }
    EXPECT_TRUE(validator.FinishHeaderBlock(HeaderType::REQUEST));
  }

  // Various invalid path characters.
  for (const absl::string_view c : {"\n", "\r", " ", "\t"}) {
    SCOPED_TRACE(absl::StrCat("char: ", absl::CEscape(c)));

    const std::string value = absl::StrCat("/shawa", c, "rma");

    HeaderValidator validator;
    validator.StartHeaderBlock();
    for (Header to_add : kSampleRequestPseudoheaders) {
      if (to_add.first == ":path") {
        EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
                  validator.ValidateSingleHeader(to_add.first, value));
      } else {
        EXPECT_EQ(HeaderValidator::HEADER_OK,
                  validator.ValidateSingleHeader(to_add.first, to_add.second));
      }
    }
    validator.FinishHeaderBlock(HeaderType::REQUEST);
  }
}

TEST(HeaderValidatorTest, PathStrictValidation) {
  // Various invalid path characters.
  for (const absl::string_view c : {"[", "<", "}", "`", "\\", " ", "\t", "#"}) {
    const std::string value = absl::StrCat("/shawa", c, "rma");

    HeaderValidator validator;

    // Required for strict path validation.
    validator.SetValidatePath();

    validator.StartHeaderBlock();
    for (Header to_add : kSampleRequestPseudoheaders) {
      if (to_add.first == ":path") {
        EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
                  validator.ValidateSingleHeader(to_add.first, value));
      } else {
        EXPECT_EQ(HeaderValidator::HEADER_OK,
                  validator.ValidateSingleHeader(to_add.first, to_add.second));
      }
    }
    EXPECT_FALSE(validator.FinishHeaderBlock(HeaderType::REQUEST));
  }

  // The fragment initial character can be explicitly allowed.
  {
    HeaderValidator validator;

    // Required for strict path validation.
    validator.SetValidatePath();

    validator.SetAllowFragmentInPath();
    validator.StartHeaderBlock();
    for (Header to_add : kSampleRequestPseudoheaders) {
      if (to_add.first == ":path") {
        EXPECT_EQ(HeaderValidator::HEADER_OK,
                  validator.ValidateSingleHeader(to_add.first, "/shawa#rma"));
      } else {
        EXPECT_EQ(HeaderValidator::HEADER_OK,
                  validator.ValidateSingleHeader(to_add.first, to_add.second));
      }
    }
    EXPECT_TRUE(validator.FinishHeaderBlock(HeaderType::REQUEST));
  }
}

TEST(HeaderValidatorTest, ResponsePseudoHeaders) {
  HeaderValidator v;

  for (HeaderType type : {HeaderType::RESPONSE, HeaderType::RESPONSE_100}) {
    // When `:status` is missing, validation will fail.
    v.StartHeaderBlock();
    EXPECT_EQ(HeaderValidator::HEADER_OK, v.ValidateSingleHeader("foo", "bar"));
    EXPECT_FALSE(v.FinishHeaderBlock(type));

    // When all pseudo-headers are present, final validation will succeed.
    v.StartHeaderBlock();
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":status", "199"));
    EXPECT_TRUE(v.FinishHeaderBlock(type));
    EXPECT_EQ("199", v.status_header());

    // When `:status` is repeated, validation will fail.
    v.StartHeaderBlock();
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":status", "199"));
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":status", "299"));
    EXPECT_FALSE(v.FinishHeaderBlock(type));

    // When an extra pseudo-header is present, final validation will fail.
    v.StartHeaderBlock();
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":status", "199"));
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":extra", "blorp"));
    EXPECT_FALSE(v.FinishHeaderBlock(type));
  }
}

TEST(HeaderValidatorTest, ResponseWithHost) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "200"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("host", "myserver.com"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::RESPONSE));
}

TEST(HeaderValidatorTest, Response204) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "204"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("x-content", "is not present"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::RESPONSE));
}

TEST(HeaderValidatorTest, ResponseWithMultipleIdenticalContentLength) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "200"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "13"));
  EXPECT_EQ(HeaderValidator::HEADER_SKIP,
            v.ValidateSingleHeader("content-length", "13"));
}

TEST(HeaderValidatorTest, ResponseWithMultipleDifferingContentLength) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "200"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "13"));
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
            v.ValidateSingleHeader("content-length", "17"));
}

TEST(HeaderValidatorTest, Response204WithContentLengthZero) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "204"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("x-content", "is not present"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "0"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::RESPONSE));
}

TEST(HeaderValidatorTest, Response204WithContentLength) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "204"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("x-content", "is not present"));
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
            v.ValidateSingleHeader("content-length", "1"));
}

TEST(HeaderValidatorTest, Response100) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "100"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("x-content", "is not present"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::RESPONSE));
}

TEST(HeaderValidatorTest, Response100WithContentLengthZero) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "100"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("x-content", "is not present"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "0"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::RESPONSE));
}

TEST(HeaderValidatorTest, Response100WithContentLength) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "100"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("x-content", "is not present"));
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
            v.ValidateSingleHeader("content-length", "1"));
}

TEST(HeaderValidatorTest, ResponseTrailerPseudoHeaders) {
  HeaderValidator v;

  // When no pseudo-headers are present, validation will succeed.
  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK, v.ValidateSingleHeader("foo", "bar"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::RESPONSE_TRAILER));

  // When any pseudo-header is present, final validation will fail.
  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "200"));
  EXPECT_EQ(HeaderValidator::HEADER_OK, v.ValidateSingleHeader("foo", "bar"));
  EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::RESPONSE_TRAILER));
}

TEST(HeaderValidatorTest, ValidContentLength) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(v.content_length(), std::nullopt);
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "41"));
  EXPECT_THAT(v.content_length(), Optional(41));

  v.StartHeaderBlock();
  EXPECT_EQ(v.content_length(), std::nullopt);
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "42"));
  EXPECT_THAT(v.content_length(), Optional(42));
}

TEST(HeaderValidatorTest, InvalidContentLength) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(v.content_length(), std::nullopt);
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
            v.ValidateSingleHeader("content-length", ""));
  EXPECT_EQ(v.content_length(), std::nullopt);
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
            v.ValidateSingleHeader("content-length", "nan"));
  EXPECT_EQ(v.content_length(), std::nullopt);
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
            v.ValidateSingleHeader("content-length", "-42"));
  EXPECT_EQ(v.content_length(), std::nullopt);
  // End on a positive note.
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "42"));
  EXPECT_THAT(v.content_length(), Optional(42));
}

TEST(HeaderValidatorTest, TeHeader) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("te", "trailers"));

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
            v.ValidateSingleHeader("te", "trailers, deflate"));
}

TEST(HeaderValidatorTest, ConnectionSpecificHeaders) {
  const std::vector<Header> connection_headers = {
      {"connection", "keep-alive"}, {"proxy-connection", "keep-alive"},
      {"keep-alive", "timeout=42"}, {"transfer-encoding", "chunked"},
      {"upgrade", "h2c"},
  };
  for (const auto& [connection_key, connection_value] : connection_headers) {
    HeaderValidator v;
    v.StartHeaderBlock();
    for (const auto& [sample_key, sample_value] : kSampleRequestPseudoheaders) {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(sample_key, sample_value));
    }
    EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
              v.ValidateSingleHeader(connection_key, connection_value));
  }
}

TEST(HeaderValidatorTest, MixedCaseHeaderName) {
  HeaderValidator v;
  v.SetAllowUppercaseInHeaderNames();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("MixedCaseName", "value"));
}

// SetAllowUppercaseInHeaderNames() only applies to non-pseudo-headers.
TEST(HeaderValidatorTest, MixedCasePseudoHeader) {
  HeaderValidator v;
  v.SetAllowUppercaseInHeaderNames();
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
            v.ValidateSingleHeader(":PATH", "/"));
}

// Matching `host` is case-insensitive.
TEST(HeaderValidatorTest, MixedCaseHost) {
  HeaderValidator v;
  v.SetAllowUppercaseInHeaderNames();
  for (Header to_add : kSampleRequestPseudoheaders) {
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(to_add.first, to_add.second));
  }
  // Validation fails, because "host" and ":authority" have different values.
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
            v.ValidateSingleHeader("Host", "www.bar.com"));
}

// Matching `content-length` is case-insensitive.
TEST(HeaderValidatorTest, MixedCaseContentLength) {
  HeaderValidator v;
  v.SetAllowUppercaseInHeaderNames();
  EXPECT_EQ(v.content_length(), std::nullopt);
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("Content-Length", "42"));
  EXPECT_THAT(v.content_length(), Optional(42));
}

}  // namespace test
}  // namespace adapter
}  // namespace http2
