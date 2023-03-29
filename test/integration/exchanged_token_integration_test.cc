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

// The integration tests in this file test the end-to-end behaviour of
// an exchanged token when going through the HTTP filter chains
// (jwt-authn + istio-authn). Filters pass on processing
// results next filters using the request info through dynamic metadata.

#include "fmt/printf.h"
#include "gmock/gmock.h"
#include "source/extensions/common/filter_names.h"
#include "source/extensions/filters/http/well_known_names.h"
#include "src/istio/utils/attribute_names.h"
#include "test/integration/http_protocol_integration.h"

using ::testing::Contains;
using ::testing::Not;

namespace Envoy {
namespace {

// An example exchanged token
constexpr char kExchangedToken[] =
    "eyJhbGciOiJSUzI1NiIsImtpZCI6IkRIRmJwb0lVcXJZOHQyenBBMnFYZkNtcjVWTzVaRXI0Un"
    "pIVV8tZW52dlEiLCJ0eXAiOiJKV1QifQ.eyJhdWQiOiJleGFtcGxlLWF1ZGllbmNlIiwiZW1ha"
    "WwiOiJmb29AZ29vZ2xlLmNvbSIsImV4cCI6NDY5ODM2MTUwOCwiaWF0IjoxNTQ0NzYxNTA4LCJ"
    "pc3MiOiJodHRwczovL2V4YW1wbGUudG9rZW5fc2VydmljZS5jb20iLCJpc3Rpb19hdHRyaWJ1d"
    "GVzIjpbeyJzb3VyY2UuaXAiOiIxMjcuMC4wLjEifV0sImtleTEiOlsidmFsMiIsInZhbDMiXSw"
    "ib3JpZ2luYWxfY2xhaW1zIjp7ImVtYWlsIjoidXNlckBleGFtcGxlLmNvbSIsImlzcyI6Imh0d"
    "HBzOi8vYWNjb3VudHMuZXhhbXBsZS5jb20iLCJzdWIiOiJleGFtcGxlLXN1YmplY3QifSwic3V"
    "iIjoiaHR0cHM6Ly9hY2NvdW50cy5leGFtcGxlLmNvbS8xMjM0NTU2Nzg5MCJ9.mLm9Gmcd748a"
    "nwybiPxGPEuYgJBChqoHkVOvRhQN-H9jMqVKyF-7ynud1CJp5n72VeMB1FzvKAV0ErzSyWQc0i"
    "ofQywG6whYXP6zL-Oc0igUrLDvzb6PuBDkbWOcZrvHkHM4tIYAkF4j880GqMWEP3gGrykziIEY"
    "9g4povquCFSdkLjjyol2-Ge_6MFdayYoeWLLOaMP7tHiPTm_ajioQ4jcz5whBWu3DZWx4IuU5U"
    "IBYlHG_miJZv5zmwwQ60T1_p_sW7zkABJgDhCvu6cHh6g-hZdQvZbATFwMfN8VDzttTjRG8wuL"
    "lkQ1TTOCx5PDv-_gHfQfRWt8Z94HrIJPuQ";

// An example token without original_claims
constexpr char kTokenWithoutOriginalClaims[] =
    "eyJhbGciOiJSUzI1NiIsImtpZCI6IkRIRmJwb0lVcXJZOHQyenBBMnFYZkNtcjVWTzVaRXI0Un"
    "pIVV8tZW52dlEiLCJ0eXAiOiJKV1QifQ.eyJhdWQiOiJleGFtcGxlLWF1ZGllbmNlIiwiZW1ha"
    "WwiOiJmb29AZ29vZ2xlLmNvbSIsImV4cCI6NDY5ODcyNzc2NiwiaWF0IjoxNTQ1MTI3NzY2LCJ"
    "pc3MiOiJodHRwczovL2V4YW1wbGUudG9rZW5fc2VydmljZS5jb20iLCJpc3Rpb19hdHRyaWJ1d"
    "GVzIjpbeyJzb3VyY2UuaXAiOiIxMjcuMC4wLjEifV0sImtleTEiOlsidmFsMiIsInZhbDMiXSw"
    "ic3ViIjoiaHR0cHM6Ly9hY2NvdW50cy5leGFtcGxlLmNvbS8xMjM0NTU2Nzg5MCJ9.FVskjGxS"
    "cTuNFtKGRnQvQgejgcdPbunCAbXlj_ZYMawrHIYnrMt_Ddw5nOojxQu2zfkwoB004196ozNjDR"
    "ED4jpJA0T6HP7hyTHGbrp6h6Z4dQ_PcmAxdR2_g8GEo-bcJ-CcbATEyBtrDqLtFcgP-ev_ctAo"
    "BQHGp7qMgdpkQIJ07BTT1n6mghPFFCnA__RYWjPUwMLGZs_bOtWxHYbd-bkDSwg4Kbtf5-9oPI"
    "nwJc6oMGMVzdjmJYMadg5GEor5XhgYz3TThPzLlEsxa0loD9eJDBGgdwjA1cLuAGgM7_HgRfg7"
    "8ameSmQgSCsNlFB4k3ODeC-YC62KYdZ5Jdrg2A";

constexpr char kExpectedPrincipal[] = "https://accounts.example.com/example-subject";
constexpr char kDestinationNamespace[] = "pod";
constexpr char kDestinationUID[] = "kubernetes://dest.pod";
const std::string kHeaderForExchangedToken = "ingress-authorization";

// Generates basic test request header.
Http::TestRequestHeaderMapImpl BaseRequestHeaders() {
  return Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                        {":path", "/"},
                                        {":scheme", "http"},
                                        {":authority", "sni.lyft.com"},
                                        {"x-forwarded-for", "10.0.0.1"}};
}

// Generates test request header with given token.
Http::TestRequestHeaderMapImpl HeadersWithToken(const std::string& header,
                                                const std::string& token) {
  auto headers = BaseRequestHeaders();
  headers.addCopy(header, token);
  return headers;
}

std::string MakeJwtFilterConfig() {
  constexpr char kJwtFilterTemplate[] = R"(
  name: %s
  typed_config:
    '@type': type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: "type.googleapis.com/envoy.extensions.filters.http.jwt_authn.v3.JwtAuthentication"
    value:
      providers:
        example:
          issuer: https://example.token_service.com
          from_headers:
            - name: ingress-authorization
          local_jwks:
            inline_string: "%s"
          payload_in_metadata: https://example.token_service.com
        testing-rbac:
          issuer: testing-rbac@secure.istio.io
          local_jwks:
            inline_string: "%s"
          payload_in_metadata: testing-rbac@secure.istio.io
      rules:
      - match:
          prefix: /
        requires:
          requires_any:
            requirements:
            - provider_name: example
            - provider_name: testing-rbac
            - allow_missing_or_failed:
  )";
  // From
  // https://github.com/istio/istio/blob/master/security/tools/jwt/samples/jwks.json
  constexpr char kJwksInline[] =
      "{ \"keys\":[ "
      "{\"e\":\"AQAB\",\"kid\":\"DHFbpoIUqrY8t2zpA2qXfCmr5VO5ZEr4RzHU_-envvQ\","
      "\"kty\":\"RSA\",\"n\":\"xAE7eB6qugXyCAG3yhh7pkDkT65pHymX-"
      "P7KfIupjf59vsdo91bSP9C8H07pSAGQO1MV"
      "_xFj9VswgsCg4R6otmg5PV2He95lZdHtOcU5DXIg_"
      "pbhLdKXbi66GlVeK6ABZOUW3WYtnNHD-91gVuoeJT_"
      "DwtGGcp4ignkgXfkiEm4sw-4sfb4qdt5oLbyVpmW6x9cfa7vs2WTfURiCrBoUqgBo_-"
      "4WTiULmmHSGZHOjzwa8WtrtOQGsAFjIbno85jp6MnGGGZPYZbDAa_b3y5u-"
      "YpW7ypZrvD8BgtKVjgtQgZhLAGezMt0ua3DRrWnKqTZ0BJ_EyxOGuHJrLsn00fnMQ\"}]}";

  return fmt::sprintf(kJwtFilterTemplate, Extensions::HttpFilters::HttpFilterNames::get().JwtAuthn,
                      StringUtil::escape(kJwksInline), StringUtil::escape(kJwksInline));
}

std::string MakeAuthFilterConfig() {
  constexpr char kAuthnFilterWithJwtTemplate[] = R"(
    name: %s
    typed_config:
      '@type': type.googleapis.com/udpa.type.v1.TypedStruct
      type_url: "type.googleapis.com/istio.authentication.v1alpha1.Policy"
      value:
        policy:
          origins:
          - jwt:
              issuer: https://example.token_service.com
              jwt_headers:
                - ingress-authorization
          principalBinding: USE_ORIGIN
)";
  return fmt::sprintf(kAuthnFilterWithJwtTemplate, Utils::IstioFilterName::kAuthentication);
}

std::string MakeRbacFilterConfig() {
  constexpr char kRbacFilterTemplate[] = R"(
  name: envoy.filters.http.rbac
  typed_config:
    '@type': type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: "type.googleapis.com/extensions.filters.http.rbac.v3.RBAC"
    value:
      rules:
        policies:
          "foo":
            permissions:
              - any: true
            principals:
              - metadata:
                  filter: %s
                  path:
                    - key: %s
                  value:
                    string_match:
                      exact: %s
)";
  return fmt::sprintf(kRbacFilterTemplate, Utils::IstioFilterName::kAuthentication,
                      istio::utils::AttributeName::kRequestAuthPrincipal, kExpectedPrincipal);
}

class ExchangedTokenIntegrationTest : public HttpProtocolIntegrationTest {
public:
  void SetUp() override {
    config_helper_.addConfigModifier(addNodeMetadata());

    config_helper_.addFilter(MakeRbacFilterConfig());
    config_helper_.addFilter(MakeAuthFilterConfig());
    config_helper_.addFilter(MakeJwtFilterConfig());

    HttpProtocolIntegrationTest::initialize();
  }

  void TearDown() override { cleanupConnection(fake_upstream_connection_); }

  ConfigHelper::ConfigModifierFunction addNodeMetadata() {
    return [](envoy::config::bootstrap::v3::Bootstrap& bootstrap) {
      ::google::protobuf::Struct meta;
      MessageUtil::loadFromJson(fmt::sprintf(R"({
        "ISTIO_VERSION": "1.0.1",
        "NODE_UID": "%s",
        "NODE_NAMESPACE": "%s"
      })",
                                             kDestinationUID, kDestinationNamespace),
                                meta);
      bootstrap.mutable_node()->mutable_metadata()->MergeFrom(meta);
    };
  }

  ConfigHelper::ConfigModifierFunction addCluster(const std::string& name) {
    return [name](envoy::config::bootstrap::v3::Bootstrap& bootstrap) {
      auto* cluster = bootstrap.mutable_static_resources()->add_clusters();
      cluster->MergeFrom(bootstrap.static_resources().clusters()[0]);
      cluster->mutable_http2_protocol_options();
      cluster->set_name(name);
    };
  }

  void cleanupConnection(FakeHttpConnectionPtr& connection) {
    if (connection != nullptr) {
      AssertionResult result = connection->close();
      RELEASE_ASSERT(result, result.message());
      result = connection->waitForDisconnect();
      RELEASE_ASSERT(result, result.message());
    }
  }
};

INSTANTIATE_TEST_SUITE_P(Protocols, ExchangedTokenIntegrationTest,
                         testing::ValuesIn(HttpProtocolIntegrationTest::getProtocolTestParams()),
                         HttpProtocolIntegrationTest::protocolTestParamsToString);

TEST_P(ExchangedTokenIntegrationTest, ValidExchangeToken) {
  codec_client_ = makeHttpConnection(makeClientConnection((lookupPort("http"))));

  // A valid exchanged token in the header for an exchanged token
  auto response = codec_client_->makeHeaderOnlyRequest(
      HeadersWithToken(kHeaderForExchangedToken, kExchangedToken));

  waitForNextUpstreamRequest(0);
  // Send backend response.
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, true);
  ASSERT_TRUE(response->waitForEndStream());

  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
}

TEST_P(ExchangedTokenIntegrationTest, ValidExchangeTokenAtWrongHeader) {
  codec_client_ = makeHttpConnection(makeClientConnection((lookupPort("http"))));

  // When a token is not in the header for an exchanged token,
  // it will not be regarded as an exchanged token.
  auto response =
      codec_client_->makeHeaderOnlyRequest(HeadersWithToken("wrong-header", kExchangedToken));

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("401", response->headers().Status()->value().getStringView());
}

TEST_P(ExchangedTokenIntegrationTest, TokenWithoutOriginalClaims) {
  codec_client_ = makeHttpConnection(makeClientConnection((lookupPort("http"))));

  // When a token does not contain original_claims,
  // it will be regarded as an invalid exchanged token.
  auto response = codec_client_->makeHeaderOnlyRequest(
      HeadersWithToken(kHeaderForExchangedToken, kTokenWithoutOriginalClaims));

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("401", response->headers().Status()->value().getStringView());
}

TEST_P(ExchangedTokenIntegrationTest, InvalidExchangeToken) {
  codec_client_ = makeHttpConnection(makeClientConnection((lookupPort("http"))));

  // When an invalid exchanged token is in the header for an exchanged token,
  // the request will be rejected.
  auto response = codec_client_->makeHeaderOnlyRequest(
      HeadersWithToken(kHeaderForExchangedToken, "invalid-token"));

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("401", response->headers().Status()->value().getStringView());
}

} // namespace
} // namespace Envoy
