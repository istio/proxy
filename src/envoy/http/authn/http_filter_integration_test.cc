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

#include "common/common/base64.h"
#include "src/istio/authn/context.pb.h"
#include "test/integration/http_protocol_integration.h"
#include "include/istio/utils/filter_names.h"
#include "fmt/printf.h"
#include "extensions/filters/http/well_known_names.h"
#include "common/common/utility.h"

using google::protobuf::util::MessageDifferencer;
using istio::authn::Payload;
using istio::authn::Result;

namespace Envoy {
namespace {
const std::string kSecIstioAuthUserInfoHeaderKey = "sec-istio-auth-userinfo";
const std::string kSecIstioAuthUserinfoHeaderValue =
    "eyJpc3MiOiI2Mjg2NDU3NDE4ODEtbm9hYml1MjNmNWE4bThvdmQ4dWN2Njk4bGo3OH"
    "Z2MGxAZGV2ZWxvcGVyLmdzZXJ2aWNlYWNjb3VudC5jb20iLCJzdWIiOiI2Mjg2NDU3"
    "NDE4ODEtbm9hYml1MjNmNWE4bThvdmQ4dWN2Njk4bGo3OHZ2MGxAZGV2ZWxvcGVyLm"
    "dzZXJ2aWNlYWNjb3VudC5jb20iLCJhdWQiOiJib29rc3RvcmUtZXNwLWVjaG8uY2xv"
    "dWRlbmRwb2ludHNhcGlzLmNvbSIsImlhdCI6MTUxMjc1NDIwNSwiZXhwIjo1MTEyNz"
    "U0MjA1fQ==";
const Envoy::Http::LowerCaseString kSecIstioAuthnPayloadHeaderKey(
    "sec-istio-authn-payload");
typedef HttpProtocolIntegrationTest AuthenticationFilterIntegrationTest;

const Http::TestHeaderMapImpl kSimpleRequestHeader{
  {
            {":method", "GET"},
            {":path", "/"},
            {":scheme", "http"},
            {":authority", "host"},
            {"x-forwarded-for", "10.0.0.1"},
  }
};

INSTANTIATE_TEST_CASE_P(Protocols, AuthenticationFilterIntegrationTest,
                        testing::ValuesIn(HttpProtocolIntegrationTest::getProtocolTestParams()),
                        HttpProtocolIntegrationTest::protocolTestParamsToString);

TEST_P(AuthenticationFilterIntegrationTest, EmptyPolicy) {
  config_helper_.addFilter("name: istio_authn");
  initialize();
  // setUpServer("src/envoy/http/authn/testdata/envoy_empty.conf";
  codec_client_ =
      makeHttpConnection(makeClientConnection((lookupPort("http"))));
  auto response =
      codec_client_->makeHeaderOnlyRequest(kSimpleRequestHeader);
  // Wait for request to upstream[0] (backend)
  waitForNextUpstreamRequest();

  // Send backend response.
  upstream_request_->encodeHeaders(Http::TestHeaderMapImpl{{":status", "200"}},
                                   true);

  response->waitForEndStream();
  EXPECT_TRUE(response->complete());
  EXPECT_STREQ("200", response->headers().Status()->value().c_str());
}

TEST_P(AuthenticationFilterIntegrationTest, SourceMTlsFail) {
  config_helper_.addFilter(R"(
    name: istio_authn
    config:
      policy:
        peers:
        - mtls: {})");
  initialize();

  // AuthN filter use MTls, but request doesn't have certificate, request
  // would be rejected.
  codec_client_ =
      makeHttpConnection(makeClientConnection((lookupPort("http"))));
  auto response =
      codec_client_->makeHeaderOnlyRequest(kSimpleRequestHeader);

  // Request is rejected, there will be no upstream request (thus no
  // waitForNextUpstreamRequest).
  response->waitForEndStream();
  EXPECT_TRUE(response->complete());
  EXPECT_STREQ("401", response->headers().Status()->value().c_str());
}

// TODO (diemtvu/lei-tang): add test for MTls success.

TEST_P(AuthenticationFilterIntegrationTest, OriginJwtRequiredHeaderNoJwtFail) {
  config_helper_.addFilter(R"(
    name: istio_authn
    config:
      policy:
        origins:
        - jwt:
            issuer: 628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com
            jwks_uri: http://localhost:8081/
        jwt_output_payload_locations: [
          628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com: sec-istio-auth-userinfo
        ])");
  initialize();

  // The AuthN filter requires JWT, but request doesn't have JWT, request
  // would be rejected.
  codec_client_ =
      makeHttpConnection(makeClientConnection((lookupPort("http"))));
  auto response =
      codec_client_->makeHeaderOnlyRequest(kSimpleRequestHeader);

  // Request is rejected, there will be no upstream request (thus no
  // waitForNextUpstreamRequest).
  response->waitForEndStream();
  EXPECT_TRUE(response->complete());
  EXPECT_STREQ("401", response->headers().Status()->value().c_str());
}

std::string MakeHeaderToMetadataConfig() {
  const std::string payload =
      "{\"iss\":\"https://"
      "example.com\",\"sub\":\"test@example.com\",\"exp\":2001001001,"
      "\"aud\":\"example_service\"}";
/*
        const char payload[] = R"({
          "iss": "https://example.com",
          "sub": "test@example.com",
          "exp": 2001001001,
          "aud": "example_service"
        })";
*/
        //ProtobufWkt::Struct pfc = MessageUtil::keyValueStruct("sec-istio-auth-userinfo", payload);
  return fmt::sprintf(R"(
    name: %s
    config:
      request_rules:
      - header: x-sec-istio-auth-userinfo
        on_header_missing:
          metadata_namespace: %s
          key: sec-istio-auth-userinfo
          value: "%s"
          type: STRING
  )", Extensions::HttpFilters::HttpFilterNames::get().HeaderToMetadata, istio::utils::FilterName::kJwt, StringUtil::escape(payload));
}
TEST_P(AuthenticationFilterIntegrationTest, CheckValidJwtPassAuthentication) {
  config_helper_.addFilter(R"(
    name: istio_authn
    config:
      jwt_output_payload_locations: {
        628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com: sec-istio-auth-userinfo
      }
      policy:
        origins:
        - jwt:
            issuer: 628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com
            jwks_uri: http://localhost:8081/
        )");
  config_helper_.addFilter(MakeHeaderToMetadataConfig());
  initialize();

  // The AuthN filter requires JWT. The http request contains validated JWT and
  // the authentication should succeed.
  codec_client_ =
      makeHttpConnection(makeClientConnection((lookupPort("http"))));
  auto response =
      codec_client_->makeRequestWithBody(kSimpleRequestHeader, 1024);
      // codec_client_->makeHeaderOnlyRequest(kSimpleRequestHeader);

  // Wait for request to upstream[0] (backend)
   waitForNextUpstreamRequest();
  // Send backend response.
  upstream_request_->encodeHeaders(Http::TestHeaderMapImpl{{":status", "200"}},
                                   true);

  response->waitForEndStream();
  EXPECT_TRUE(response->complete());
  EXPECT_STREQ("200", response->headers().Status()->value().c_str());
}

/*
TEST_P(AuthenticationFilterIntegrationTest, CheckConsumedJwtHeadersAreRemoved) {
  const Envoy::Http::LowerCaseString header_location(
      "location-to-read-jwt-result");
  const std::string jwt_header =
      R"(
     {
       "iss": "issuer@foo.com",
       "sub": "sub@foo.com",
       "aud": "aud1",
       "non-string-will-be-ignored": 1512754205,
       "some-other-string-claims": "some-claims-kept"
     }
   )";
  std::string jwt_header_base64 =
      Base64::encode(jwt_header.c_str(), jwt_header.size());
  Http::TestHeaderMapImpl request_headers_with_jwt_at_specified_location{
      {":method", "GET"},
      {":path", "/"},
      {":authority", "host"},
      {"location-to-read-jwt-result", jwt_header_base64}};
  // In this config, the JWT verification result for "issuer@foo.com" is in the
  // header "location-to-read-jwt-result"
  setUpServer(
      "src/envoy/http/authn/testdata/"
      "envoy_jwt_with_output_header_location.conf");
  // The AuthN filter requires JWT and the http request contains validated JWT.
  // In this case, the authentication should succeed and an authn result
  // should be generated.
  codec_client_ =
      makeHttpConnection(makeClientConnection((lookupPort("http"))));
  auto response = codec_client_->makeHeaderOnlyRequest(
      request_headers_with_jwt_at_specified_location);

  // Wait for request to upstream[0] (backend)
  waitForNextUpstreamRequest(0);
  response->waitForEndStream();

  // After Istio authn, the JWT headers consumed by Istio authn should have
  // been removed.
  EXPECT_TRUE(nullptr == upstream_request_->headers().get(header_location));
}

TEST_P(AuthenticationFilterIntegrationTest, CheckAuthnResultIsExpected) {
  setUpServer("src/envoy/http/authn/testdata/envoy_origin_jwt_authn_only.conf");

  // The AuthN filter requires JWT and the http request contains validated JWT.
  // In this case, the authentication should succeed and an authn result
  // should be generated.
  codec_client_ =
      makeHttpConnection(makeClientConnection((lookupPort("http"))));
  auto response =
      codec_client_->makeHeaderOnlyRequest(request_headers_with_jwt_);

  // Wait for request to upstream[0] (backend)
  waitForNextUpstreamRequest(0);
  response->waitForEndStream();

  // Authn result should be as expected
  const Envoy::Http::HeaderString &header_value =
      upstream_request_->headers().get(kSecIstioAuthnPayloadHeaderKey)->value();
  std::string value_base64(header_value.c_str(), header_value.size());
  const std::string value = Base64::decode(value_base64);
  Result result;
  google::protobuf::util::JsonParseOptions options;
  Result expected_result;

  bool parse_ret = result.ParseFromString(value);
  EXPECT_TRUE(parse_ret);
  JsonStringToMessage(
      R"(
          {
            "origin": {
              "user": "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com/628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com",
              "audiences": [
               "bookstore-esp-echo.cloudendpointsapis.com"
              ],
              "presenter": "",
              "claims": {
               "aud": "bookstore-esp-echo.cloudendpointsapis.com",
               "iss": "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com",
               "sub": "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com"
              },
              raw_claims: "{\"iss\":\"628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com\",\"sub\":\"628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com\",\"aud\":\"bookstore-esp-echo.cloudendpointsapis.com\",\"iat\":1512754205,\"exp\":5112754205}"
            }
          }
      )",
      &expected_result, options);
  // Note: TestUtility::protoEqual() uses SerializeAsString() and the output
  // is non-deterministic. Thus, MessageDifferencer::Equals() is used.
  EXPECT_TRUE(MessageDifferencer::Equals(expected_result, result));
}
*/
}  // namespace
}  // namespace Envoy
