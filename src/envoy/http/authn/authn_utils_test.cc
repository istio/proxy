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

#include "src/envoy/http/authn/authn_utils.h"
#include "common/common/base64.h"
#include "src/envoy/http/authn/authenticator_base.h"
#include "src/envoy/http/authn/test_utils.h"
#include "test/test_common/utility.h"

using google::protobuf::util::MessageDifferencer;
using istio::authn::JwtPayload;

namespace iaapi = istio::authentication::v1alpha1;

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {
namespace {

const std::string kSecIstioAuthUserInfoHeaderKey = "sec-istio-auth-userinfo";
const std::string kSecIstioAuthUserinfoHeaderValue =
    R"(
     {
       "iss": "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com",
       "sub": "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com",
       "aud": "bookstore-esp-echo.cloudendpointsapis.com",
       "iat": 1512754205,
       "exp": 5112754205
     }
   )";
const std::string kSecIstioAuthUserInfoHeaderWithNoAudValue =
    R"(
       {
         "iss": "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com",
         "sub": "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com",
         "iat": 1512754205,
         "exp": 5112754205
       }
     )";
const std::string kSecIstioAuthUserInfoHeaderWithTwoAudValue =
    R"(
       {
         "iss": "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com",
         "sub": "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com",
         "aud": ["bookstore-esp-echo.cloudendpointsapis.com", "bookstore-esp-echo2.cloudendpointsapis.com"],
         "iat": 1512754205,
         "exp": 5112754205
       }
     )";

TEST(AuthnUtilsTest, GetJwtPayloadFromHeaderTest) {
  JwtPayload payload, expected_payload;
  std::string value_base64 =
      Base64::encode(kSecIstioAuthUserinfoHeaderValue.c_str(),
                     kSecIstioAuthUserinfoHeaderValue.size());
  Http::TestHeaderMapImpl request_headers_with_jwt{
      {kSecIstioAuthUserInfoHeaderKey, value_base64}};
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
      R"(
      user: "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com/628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com"
      audiences: ["bookstore-esp-echo.cloudendpointsapis.com"]
      claims {
        key: "aud"
        value: "bookstore-esp-echo.cloudendpointsapis.com"
      }
      claims {
        key: "iss"
        value: "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com"
      }
      claims {
        key: "sub"
        value: "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com"
      }
    )",
      &expected_payload));
  // The payload returned from GetJWTPayloadFromHeaders() should be the same as
  // the expected.
  bool ret = AuthnUtils::GetJWTPayloadFromHeaders(
      request_headers_with_jwt, LowerCaseString(kSecIstioAuthUserInfoHeaderKey),
      &payload);
  EXPECT_TRUE(ret);
  EXPECT_TRUE(MessageDifferencer::Equals(expected_payload, payload));
}

TEST(AuthnUtilsTest, GetJwtPayloadFromHeaderWithNoAudTest) {
  JwtPayload payload, expected_payload;
  std::string value_base64 =
      Base64::encode(kSecIstioAuthUserInfoHeaderWithNoAudValue.c_str(),
                     kSecIstioAuthUserInfoHeaderWithNoAudValue.size());
  Http::TestHeaderMapImpl request_headers_with_jwt{
      {kSecIstioAuthUserInfoHeaderKey, value_base64}};
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
      R"(
      user: "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com/628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com"
      claims {
        key: "iss"
        value: "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com"
      }
      claims {
        key: "sub"
        value: "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com"
      }
    )",
      &expected_payload));
  // The payload returned from GetJWTPayloadFromHeaders() should be the same as
  // the expected. When there is no aud,  the aud is not saved in the payload
  // and claims.
  bool ret = AuthnUtils::GetJWTPayloadFromHeaders(
      request_headers_with_jwt, LowerCaseString(kSecIstioAuthUserInfoHeaderKey),
      &payload);
  EXPECT_TRUE(ret);
  EXPECT_TRUE(MessageDifferencer::Equals(expected_payload, payload));
}

TEST(AuthnUtilsTest, GetJwtPayloadFromHeaderWithTwoAudTest) {
  JwtPayload payload, expected_payload;
  std::string value_base64 =
      Base64::encode(kSecIstioAuthUserInfoHeaderWithTwoAudValue.c_str(),
                     kSecIstioAuthUserInfoHeaderWithTwoAudValue.size());
  Http::TestHeaderMapImpl request_headers_with_jwt{
      {kSecIstioAuthUserInfoHeaderKey, value_base64}};
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
      R"(
      user: "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com/628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com"
      audiences: "bookstore-esp-echo.cloudendpointsapis.com"
      audiences: "bookstore-esp-echo2.cloudendpointsapis.com"
      claims {
        key: "iss"
        value: "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com"
      }
      claims {
        key: "sub"
        value: "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com"
      }
    )",
      &expected_payload));
  // The payload returned from GetJWTPayloadFromHeaders() should be the same as
  // the expected. When the aud is a string array, the aud is not saved in the
  // claims.
  bool ret = AuthnUtils::GetJWTPayloadFromHeaders(
      request_headers_with_jwt, LowerCaseString(kSecIstioAuthUserInfoHeaderKey),
      &payload);
  EXPECT_TRUE(ret);
  EXPECT_TRUE(MessageDifferencer::Equals(expected_payload, payload));
}

}  // namespace
}  // namespace AuthN
}  // namespace Istio
}  // namespace Http
}  // namespace Envoy
