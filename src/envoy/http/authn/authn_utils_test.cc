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
    "eyJpc3MiOiI2Mjg2NDU3NDE4ODEtbm9hYml1MjNmNWE4bThvdmQ4dWN2Njk4bGo3OH"
    "Z2MGxAZGV2ZWxvcGVyLmdzZXJ2aWNlYWNjb3VudC5jb20iLCJzdWIiOiI2Mjg2NDU3"
    "NDE4ODEtbm9hYml1MjNmNWE4bThvdmQ4dWN2Njk4bGo3OHZ2MGxAZGV2ZWxvcGVyLm"
    "dzZXJ2aWNlYWNjb3VudC5jb20iLCJhdWQiOiJib29rc3RvcmUtZXNwLWVjaG8uY2xv"
    "dWRlbmRwb2ludHNhcGlzLmNvbSIsImlhdCI6MTUxMjc1NDIwNSwiZXhwIjo1MTEyNz"
    "U0MjA1fQ==";

TEST(AuthnUtilsTest, GetJwtPayloadFromHeaderTest) {
  JwtPayload payload, expected_payload;
  Http::TestHeaderMapImpl request_headers_with_jwt{
      {kSecIstioAuthUserInfoHeaderKey, kSecIstioAuthUserinfoHeaderValue}};
  google::protobuf::util::JsonParseOptions options;
  JsonStringToMessage(
      R"({
           "user": "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com/628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com",
           "audiences": "bookstore-esp-echo.cloudendpointsapis.com",
           "claims": {
             "aud": "bookstore-esp-echo.cloudendpointsapis.com",
             "iss": "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com",
             "sub": "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com"
           }
         }
        )",
      &expected_payload, options);
  // The payload returned from GetJWTPayloadFromHeaders() should be the same as
  // the expected.
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
