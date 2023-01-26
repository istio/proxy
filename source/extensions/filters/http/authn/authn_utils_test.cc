/* Copyright 2018 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "source/extensions/filters/http/authn/authn_utils.h"

#include "source/common/common/base64.h"
#include "source/common/common/utility.h"
#include "source/extensions/filters/http/authn/test_utils.h"
#include "test/test_common/utility.h"

using google::protobuf::util::MessageDifferencer;
using istio::authn::JwtPayload;

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {
namespace {

const std::string kSecIstioAuthUserinfoHeaderValue =
    R"(
     {
       "iss": "issuer@foo.com",
       "sub": "sub@foo.com",
       "aud": "aud1",
       "non-string-will-be-ignored": 1512754205,
       "some-other-string-claims": "some-claims-kept"
     }
   )";
const std::string kSecIstioAuthUserInfoHeaderWithAudValueList =
    R"(
       {
         "iss": "issuer@foo.com",
         "sub": "sub@foo.com",
         "aud": "aud1  aud2",
         "non-string-will-be-ignored": 1512754205,
         "some-other-string-claims": "some-claims-kept"
       }
     )";
const std::string kSecIstioAuthUserInfoHeaderWithAudValueArray =
    R"(
       {
         "iss": "issuer@foo.com",
         "sub": "sub@foo.com",
         "aud": ["aud1", "aud2"],
         "non-string-will-be-ignored": 1512754205,
         "some-other-string-claims": "some-claims-kept"
       }
     )";
const std::string kSecIstioAuthUserInfoHeaderWithNestedClaims =
    R"(
       {
         "iss": "issuer@foo.com",
         "sub": "sub@foo.com",
         "nested1": {
           "aud1": "aud1a  aud1b",
           "list1": ["list1a", "list1b"],
           "other1": "str1",
           "non-string-ignored": 111,
           "nested2": {
             "aud2": "aud2a  aud2b",
             "list2": ["list2a", "list2b"],
             "other2": "str2",
             "non-string-ignored": 222
           }
         },
         "non-string-will-be-ignored": 1512754205,
         "some-other-string-claims": "some-claims-kept"
       }
     )";

TEST(AuthnUtilsTest, GetJwtPayloadFromHeaderTest) {
  JwtPayload payload, expected_payload;
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
      R"(
      user: "issuer@foo.com/sub@foo.com"
      audiences: ["aud1"]
      claims: {
        fields: {
          key: "aud"
          value: {
            list_value: {
              values: {
                string_value: "aud1"
              }
            }
          }
        }
        fields: {
          key: "iss"
          value: {
            list_value: {
              values: {
                string_value: "issuer@foo.com"
              }
            }
          }
        }
        fields: {
          key: "sub"
          value: {
            list_value: {
              values: {
                string_value: "sub@foo.com"
              }
            }
          }
        }
        fields: {
          key: "some-other-string-claims"
          value: {
            list_value: {
              values: {
                string_value: "some-claims-kept"
              }
            }
          }
        }
      }
      raw_claims: ")" +
          StringUtil::escape(kSecIstioAuthUserinfoHeaderValue) + R"(")",
      &expected_payload));
  // The payload returned from ProcessJwtPayload() should be the same as
  // the expected.
  bool ret = AuthnUtils::ProcessJwtPayload(kSecIstioAuthUserinfoHeaderValue, &payload);
  EXPECT_TRUE(ret);
  EXPECT_TRUE(MessageDifferencer::Equals(expected_payload, payload));
}

TEST(AuthnUtilsTest, ProcessJwtPayloadWithAudListTest) {
  JwtPayload payload, expected_payload;
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
      R"(
      user: "issuer@foo.com/sub@foo.com"
      audiences: "aud1"
      audiences: "aud2"
      claims: {
        fields: {
          key: "iss"
          value: {
            list_value: {
              values: {
                string_value: "issuer@foo.com"
              }
            }
          }
        }
        fields: {
          key: "sub"
          value: {
            list_value: {
              values: {
                string_value: "sub@foo.com"
              }
            }
          }
        }
        fields: {
          key: "aud"
          value: {
            list_value: {
              values: {
                string_value: "aud1"
              }
              values: {
                string_value: "aud2"
              }
            }
          }
        }
        fields: {
          key: "some-other-string-claims"
          value: {
            list_value: {
              values: {
                string_value: "some-claims-kept"
              }
            }
          }
        }
      }
      raw_claims: ")" +
          StringUtil::escape(kSecIstioAuthUserInfoHeaderWithAudValueList) + R"(")",
      &expected_payload));
  // The payload returned from ProcessJwtPayload() should be the same as
  // the expected. When there is no aud,  the aud is not saved in the payload
  // and claims.
  bool ret = AuthnUtils::ProcessJwtPayload(kSecIstioAuthUserInfoHeaderWithAudValueList, &payload);
  EXPECT_TRUE(ret);
  EXPECT_TRUE(MessageDifferencer::Equals(expected_payload, payload));
}

TEST(AuthnUtilsTest, ProcessJwtPayloadWithAudArrayTest) {
  JwtPayload payload, expected_payload;
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
      R"(
      user: "issuer@foo.com/sub@foo.com"
      audiences: "aud1"
      audiences: "aud2"
      claims: {
        fields: {
          key: "aud"
          value: {
            list_value: {
              values: {
                string_value: "aud1"
              }
              values: {
                string_value: "aud2"
              }
            }
          }
        }
        fields: {
          key: "iss"
          value: {
            list_value: {
              values: {
                string_value: "issuer@foo.com"
              }
            }
          }
        }
        fields: {
          key: "sub"
          value: {
            list_value: {
              values: {
                string_value: "sub@foo.com"
              }
            }
          }
        }
        fields: {
          key: "some-other-string-claims"
          value: {
            list_value: {
              values: {
                string_value: "some-claims-kept"
              }
            }
          }
        }
      }
      raw_claims: ")" +
          StringUtil::escape(kSecIstioAuthUserInfoHeaderWithAudValueArray) + R"(")",
      &expected_payload));
  // The payload returned from ProcessJwtPayload() should be the same as
  // the expected. When the aud is a string array, the aud is not saved in the
  // claims.
  bool ret = AuthnUtils::ProcessJwtPayload(kSecIstioAuthUserInfoHeaderWithAudValueArray, &payload);

  EXPECT_TRUE(ret);
  EXPECT_TRUE(MessageDifferencer::Equals(expected_payload, payload));
}

TEST(AuthnUtilsTest, ProcessJwtPayloadWithNestedClaimsTest) {
  JwtPayload payload, expected_payload;
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
      R"(
      user: "issuer@foo.com/sub@foo.com"
      claims: {
        fields: {
          key: "iss"
          value: {
            list_value: {
              values: {
                string_value: "issuer@foo.com"
              }
            }
          }
        }
        fields: {
          key: "sub"
          value: {
            list_value: {
              values: {
                string_value: "sub@foo.com"
              }
            }
          }
        }
        fields: {
          key: "some-other-string-claims"
          value: {
            list_value: {
              values: {
                string_value: "some-claims-kept"
              }
            }
          }
        }
        fields: {
          key: "nested1"
          value: {
            struct_value: {
              fields: {
                key: "aud1"
                value: {
                  list_value: {
                    values: {
                      string_value: "aud1a"
                    }
                    values: {
                      string_value: "aud1b"
                    }
                  }
                }
              }
              fields: {
                key: "list1"
                value: {
                  list_value: {
                    values: {
                      string_value: "list1a"
                    }
                    values: {
                      string_value: "list1b"
                    }
                  }
                }
              }
              fields: {
                key: "other1"
                value: {
                  list_value: {
                    values: {
                      string_value: "str1"
                    }
                  }
                }
              }
              fields: {
                key: "nested2"
                value: {
                  struct_value: {
                    fields: {
                      key: "aud2"
                      value: {
                        list_value: {
                          values: {
                            string_value: "aud2a"
                          }
                          values: {
                            string_value: "aud2b"
                          }
                        }
                      }
                    }
                    fields: {
                      key: "list2"
                      value: {
                        list_value: {
                          values: {
                            string_value: "list2a"
                          }
                          values: {
                            string_value: "list2b"
                          }
                        }
                      }
                    }
                    fields: {
                      key: "other2"
                      value: {
                        list_value: {
                          values: {
                            string_value: "str2"
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      raw_claims: ")" +
          StringUtil::escape(kSecIstioAuthUserInfoHeaderWithNestedClaims) + R"(")",
      &expected_payload));

  EXPECT_TRUE(AuthnUtils::ProcessJwtPayload(kSecIstioAuthUserInfoHeaderWithNestedClaims, &payload));
  EXPECT_TRUE(MessageDifferencer::Equals(expected_payload, payload));
}

TEST(AuthnUtilsTest, MatchString) {
  iaapi::StringMatch match;
  EXPECT_FALSE(AuthnUtils::MatchString({}, match));
  EXPECT_FALSE(AuthnUtils::MatchString("", match));

  match.set_exact("exact");
  EXPECT_TRUE(AuthnUtils::MatchString("exact", match));
  EXPECT_FALSE(AuthnUtils::MatchString("exac", match));
  EXPECT_FALSE(AuthnUtils::MatchString("exacy", match));

  match.set_prefix("prefix");
  EXPECT_TRUE(AuthnUtils::MatchString("prefix-1", match));
  EXPECT_TRUE(AuthnUtils::MatchString("prefix", match));
  EXPECT_FALSE(AuthnUtils::MatchString("prefi", match));
  EXPECT_FALSE(AuthnUtils::MatchString("prefiy", match));

  match.set_suffix("suffix");
  EXPECT_TRUE(AuthnUtils::MatchString("1-suffix", match));
  EXPECT_TRUE(AuthnUtils::MatchString("suffix", match));
  EXPECT_FALSE(AuthnUtils::MatchString("suffi", match));
  EXPECT_FALSE(AuthnUtils::MatchString("suffiy", match));

  match.set_regex(".+abc.+");
  EXPECT_TRUE(AuthnUtils::MatchString("1-abc-1", match));
  EXPECT_FALSE(AuthnUtils::MatchString("1-abc", match));
  EXPECT_FALSE(AuthnUtils::MatchString("abc-1", match));
  EXPECT_FALSE(AuthnUtils::MatchString("1-ac-1", match));
}

TEST(AuthnUtilsTest, ShouldValidateJwtPerPathExcluded) {
  iaapi::Jwt jwt;

  // Create a rule that triggers on everything except /good-x and /allow-x.
  auto* rule = jwt.add_trigger_rules();
  rule->add_excluded_paths()->set_exact("/good-x");
  rule->add_excluded_paths()->set_exact("/allow-x");
  EXPECT_FALSE(AuthnUtils::ShouldValidateJwtPerPath("/good-x", jwt));
  EXPECT_FALSE(AuthnUtils::ShouldValidateJwtPerPath("/allow-x", jwt));
  EXPECT_TRUE(AuthnUtils::ShouldValidateJwtPerPath("/good-1", jwt));
  EXPECT_TRUE(AuthnUtils::ShouldValidateJwtPerPath("/allow-1", jwt));
  EXPECT_TRUE(AuthnUtils::ShouldValidateJwtPerPath("/other", jwt));

  // Change the rule to only triggers on prefix /good and /allow.
  rule->add_included_paths()->set_prefix("/good");
  rule->add_included_paths()->set_prefix("/allow");
  EXPECT_FALSE(AuthnUtils::ShouldValidateJwtPerPath("/good-x", jwt));
  EXPECT_FALSE(AuthnUtils::ShouldValidateJwtPerPath("/allow-x", jwt));
  EXPECT_TRUE(AuthnUtils::ShouldValidateJwtPerPath("/good-1", jwt));
  EXPECT_TRUE(AuthnUtils::ShouldValidateJwtPerPath("/allow-1", jwt));
  EXPECT_FALSE(AuthnUtils::ShouldValidateJwtPerPath("/other", jwt));
}

TEST(AuthnUtilsTest, ShouldValidateJwtPerPathIncluded) {
  iaapi::Jwt jwt;

  // Create a rule that triggers on everything with prefix /good and /allow.
  auto* rule = jwt.add_trigger_rules();
  rule->add_included_paths()->set_prefix("/good");
  rule->add_included_paths()->set_prefix("/allow");
  EXPECT_TRUE(AuthnUtils::ShouldValidateJwtPerPath("/good-x", jwt));
  EXPECT_TRUE(AuthnUtils::ShouldValidateJwtPerPath("/allow-x", jwt));
  EXPECT_TRUE(AuthnUtils::ShouldValidateJwtPerPath("/good-2", jwt));
  EXPECT_TRUE(AuthnUtils::ShouldValidateJwtPerPath("/allow-1", jwt));
  EXPECT_FALSE(AuthnUtils::ShouldValidateJwtPerPath("/other", jwt));

  // Change the rule to also exclude /allow-x and /good-x.
  rule->add_excluded_paths()->set_exact("/good-x");
  rule->add_excluded_paths()->set_exact("/allow-x");
  EXPECT_FALSE(AuthnUtils::ShouldValidateJwtPerPath("/good-x", jwt));
  EXPECT_FALSE(AuthnUtils::ShouldValidateJwtPerPath("/allow-x", jwt));
  EXPECT_TRUE(AuthnUtils::ShouldValidateJwtPerPath("/good-2", jwt));
  EXPECT_TRUE(AuthnUtils::ShouldValidateJwtPerPath("/allow-1", jwt));
  EXPECT_FALSE(AuthnUtils::ShouldValidateJwtPerPath("/other", jwt));
}

TEST(AuthnUtilsTest, ShouldValidateJwtPerPathDefault) {
  iaapi::Jwt jwt;

  // Always trigger when path is unavailable.
  EXPECT_TRUE(AuthnUtils::ShouldValidateJwtPerPath("", jwt));

  // Always trigger when there is no rules in jwt.
  EXPECT_TRUE(AuthnUtils::ShouldValidateJwtPerPath("/test", jwt));

  // Add a rule that triggers on everything except /hello.
  jwt.add_trigger_rules()->add_excluded_paths()->set_exact("/hello");
  EXPECT_FALSE(AuthnUtils::ShouldValidateJwtPerPath("/hello", jwt));
  EXPECT_TRUE(AuthnUtils::ShouldValidateJwtPerPath("/other", jwt));

  // Add another rule that triggers on path /hello.
  jwt.add_trigger_rules()->add_included_paths()->set_exact("/hello");
  EXPECT_TRUE(AuthnUtils::ShouldValidateJwtPerPath("/hello", jwt));
  EXPECT_TRUE(AuthnUtils::ShouldValidateJwtPerPath("/other", jwt));
}

} // namespace
} // namespace AuthN
} // namespace Istio
} // namespace Http
} // namespace Envoy
