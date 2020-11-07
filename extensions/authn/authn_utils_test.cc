/* Copyright 2020 Istio Authors. All Rights Reserved.
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
#include "extensions/authn/authn_utils.h"

#include "common/common/base64.h"
#include "common/common/utility.h"
#include "extensions/authn/test_utils.h"
#include "test/test_common/utility.h"

using google::protobuf::util::MessageDifferencer;
using istio::authn::JwtPayload;

namespace Extensions {
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

TEST(AuthnUtilsTest, GetJwtPayloadFromHeaderTest) {
  JwtPayload payload, expected_payload;
  ASSERT_TRUE(Envoy::Protobuf::TextFormat::ParseFromString(
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
          Envoy::StringUtil::escape(kSecIstioAuthUserinfoHeaderValue) + R"(")",
      &expected_payload));
  // The payload returned from ProcessJwtPayload() should be the same as
  // the expected.
  bool ret =
      AuthnUtils::ProcessJwtPayload(kSecIstioAuthUserinfoHeaderValue, &payload);
  EXPECT_TRUE(ret);
  EXPECT_TRUE(MessageDifferencer::Equals(expected_payload, payload));
}

TEST(AuthnUtilsTest, ProcessJwtPayloadWithAudListTest) {
  JwtPayload payload, expected_payload;
  ASSERT_TRUE(Envoy::Protobuf::TextFormat::ParseFromString(
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
          Envoy::StringUtil::escape(
              kSecIstioAuthUserInfoHeaderWithAudValueList) +
          R"(")",
      &expected_payload));
  // The payload returned from ProcessJwtPayload() should be the same as
  // the expected. When there is no aud,  the aud is not saved in the payload
  // and claims.
  bool ret = AuthnUtils::ProcessJwtPayload(
      kSecIstioAuthUserInfoHeaderWithAudValueList, &payload);
  EXPECT_TRUE(ret);
  EXPECT_TRUE(MessageDifferencer::Equals(expected_payload, payload));
}

TEST(AuthnUtilsTest, ProcessJwtPayloadWithAudArrayTest) {
  JwtPayload payload, expected_payload;
  ASSERT_TRUE(Envoy::Protobuf::TextFormat::ParseFromString(
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
          Envoy::StringUtil::escape(
              kSecIstioAuthUserInfoHeaderWithAudValueArray) +
          R"(")",
      &expected_payload));
  // The payload returned from ProcessJwtPayload() should be the same as
  // the expected. When the aud is a string array, the aud is not saved in the
  // claims.
  bool ret = AuthnUtils::ProcessJwtPayload(
      kSecIstioAuthUserInfoHeaderWithAudValueArray, &payload);

  EXPECT_TRUE(ret);
  EXPECT_TRUE(MessageDifferencer::Equals(expected_payload, payload));
}

TEST(AuthnUtilsTest, ExtractOriginalPayloadTest) {
  std::string payload_str;
  std::string token = R"(
     {
       "iss": "token-service",
       "sub": "subject",
       "aud": ["aud1", "aud2"],
       "original_claims": {
         "iss": "https://accounts.example.com",
         "sub": "example-subject",
         "email": "user@example.com"
       }
     }
   )";
  EXPECT_TRUE(AuthnUtils::ExtractOriginalPayload(token, &payload_str));

  std::string token2 = "{}";
  EXPECT_FALSE(AuthnUtils::ExtractOriginalPayload(token2, &payload_str));
}

}  // namespace
}  // namespace AuthN
}  // namespace Extensions
