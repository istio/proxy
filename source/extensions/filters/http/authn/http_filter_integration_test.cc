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

#include "fmt/printf.h"
#include "source/common/common/base64.h"
#include "source/common/common/utility.h"
#include "source/extensions/common/filter_names.h"
#include "source/extensions/filters/http/well_known_names.h"
#include "src/istio/authn/context.pb.h"
#include "test/integration/http_protocol_integration.h"

using google::protobuf::util::MessageDifferencer;
using istio::authn::Payload;
using istio::authn::Result;

namespace Envoy {
namespace {

static const Envoy::Http::LowerCaseString kSecIstioAuthnPayloadHeaderKey("sec-istio-authn-payload");

// Default request for testing.
Http::TestRequestHeaderMapImpl SimpleRequestHeaders() {
  return Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                        {":path", "/"},
                                        {":scheme", "http"},
                                        {":authority", "sni.lyft.com"},
                                        {"x-forwarded-for", "10.0.0.1"}};
}

// Keep the same as issuer in the policy below.
static const char kJwtIssuer[] = "some@issuer";

static const char kAuthnFilterWithJwt[] = R"(
    name: istio_authn
    typed_config:
      '@type': type.googleapis.com/udpa.type.v1.TypedStruct
      type_url: "type.googleapis.com/istio.authentication.v1alpha1.Policy"
      value:
        policy:
          origins:
          - jwt:
              issuer: some@issuer
              jwks_uri: http://localhost:8081/)";

// Payload data to inject. Note the iss claim intentionally set different from
// kJwtIssuer.
static const char kMockJwtPayload[] = "{\"iss\":\"https://example.com\","
                                      "\"sub\":\"test@example.com\",\"exp\":2001001001,"
                                      "\"aud\":\"example_service\"}";
// Returns a simple header-to-metadata filter config that can be used to inject
// data into request info dynamic metadata for testing.
std::string MakeHeaderToMetadataConfig() {
  return fmt::sprintf(
      R"(
    name: %s
    typed_config:
      '@type': type.googleapis.com/udpa.type.v1.TypedStruct
      type_url: type.googleapis.com/envoy.extensions.filters.http.header_to_metadata.v3.Config
      value:
        request_rules:
        - header: x-mock-metadata-injection
          on_header_missing:
            metadata_namespace: %s
            key: %s
            value: "%s"
            type: STRING)",
      Extensions::HttpFilters::HttpFilterNames::get().HeaderToMetadata,
      Utils::IstioFilterName::kJwt, kJwtIssuer, StringUtil::escape(kMockJwtPayload));
}

typedef HttpProtocolIntegrationTest AuthenticationFilterIntegrationTest;

INSTANTIATE_TEST_SUITE_P(Protocols, AuthenticationFilterIntegrationTest,
                         testing::ValuesIn(HttpProtocolIntegrationTest::getProtocolTestParams()),
                         HttpProtocolIntegrationTest::protocolTestParamsToString);

TEST_P(AuthenticationFilterIntegrationTest, EmptyPolicy) {
  config_helper_.addFilter("name: istio_authn");
  initialize();
  codec_client_ = makeHttpConnection(makeClientConnection((lookupPort("http"))));
  auto response = codec_client_->makeHeaderOnlyRequest(SimpleRequestHeaders());
  // Wait for request to upstream (backend)
  waitForNextUpstreamRequest();

  // Send backend response.
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, true);

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
}

TEST_P(AuthenticationFilterIntegrationTest, SourceMTlsFail) {
  config_helper_.addFilter(R"(
    name: istio_authn
    typed_config:
      '@type': type.googleapis.com/udpa.type.v1.TypedStruct
      type_url: "type.googleapis.com/istio.authentication.v1alpha1.Policy"
      value:
        policy:
          peers:
          - mtls: {})");
  initialize();

  // AuthN filter use MTls, but request doesn't have certificate, request
  // would be rejected.
  codec_client_ = makeHttpConnection(makeClientConnection((lookupPort("http"))));
  auto response = codec_client_->makeHeaderOnlyRequest(SimpleRequestHeaders());

  // Request is rejected, there will be no upstream request (thus no
  // waitForNextUpstreamRequest).
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("401", response->headers().Status()->value().getStringView());
}

// TODO (diemtvu/lei-tang): add test for MTls success.

TEST_P(AuthenticationFilterIntegrationTest, OriginJwtRequiredHeaderNoJwtFail) {
  config_helper_.addFilter(kAuthnFilterWithJwt);
  initialize();

  // The AuthN filter requires JWT, but request doesn't have JWT, request
  // would be rejected.
  codec_client_ = makeHttpConnection(makeClientConnection((lookupPort("http"))));
  auto response = codec_client_->makeHeaderOnlyRequest(SimpleRequestHeaders());

  // Request is rejected, there will be no upstream request (thus no
  // waitForNextUpstreamRequest).
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("401", response->headers().Status()->value().getStringView());
}

TEST_P(AuthenticationFilterIntegrationTest, CheckValidJwtPassAuthentication) {
  config_helper_.addFilter(kAuthnFilterWithJwt);
  config_helper_.addFilter(MakeHeaderToMetadataConfig());
  initialize();

  // The AuthN filter requires JWT. The http request contains validated JWT and
  // the authentication should succeed.
  codec_client_ = makeHttpConnection(makeClientConnection((lookupPort("http"))));
  auto response = codec_client_->makeHeaderOnlyRequest(SimpleRequestHeaders());

  // Wait for request to upstream (backend)
  waitForNextUpstreamRequest();
  // Send backend response.
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, true);

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
}

TEST_P(AuthenticationFilterIntegrationTest, CORSPreflight) {
  config_helper_.addFilter(kAuthnFilterWithJwt);
  initialize();

  // The AuthN filter requires JWT but should bypass CORS preflight request even
  // it doesn't have JWT token.
  codec_client_ = makeHttpConnection(makeClientConnection((lookupPort("http"))));
  auto headers = Http::TestRequestHeaderMapImpl{
      {":method", "OPTIONS"},
      {":path", "/"},
      {":scheme", "http"},
      {":authority", "sni.lyft.com"},
      {"x-forwarded-for", "10.0.0.1"},
      {"access-control-request-method", "GET"},
      {"origin", "example.com"},
  };
  auto response = codec_client_->makeHeaderOnlyRequest(headers);

  // Wait for request to upstream (backend)
  waitForNextUpstreamRequest();
  // Send backend response.
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, true);

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
}

} // namespace
} // namespace Envoy
