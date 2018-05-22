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

#include "src/envoy/http/jwt_auth/http_filter.h"
#include "src/envoy/utils/jwt_authenticator.h"
#include "common/http/header_map_impl.h"
#include "test/test_common/utility.h"
#include "test/mocks/http/mocks.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::Invoke;
using ::testing::_;

namespace Envoy {
namespace Http {
namespace {
  // Payload:
  // {"iss":"https://example.com","sub":"test@example.com","aud":"example_service","exp":2001001001}
  const std::string kJwt =
    "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJodHRwczovL2V4YW1wbGUu"
    "Y29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIsImV4cCI6MjAwMTAwMTAwMSwiY"
    "XVkIjoiZXhhbXBsZV9zZXJ2aWNlIn0.cuui_Syud76B0tqvjESE8IZbX7vzG6xA-M"
    "Daof1qEFNIoCFT_YQPkseLSUSR2Od3TJcNKk-dKjvUEL1JW3kGnyC1dBx4f3-Xxro"
    "yL23UbR2eS8TuxO9ZcNCGkjfvH5O4mDb6cVkFHRDEolGhA7XwNiuVgkGJ5Wkrvshi"
    "h6nqKXcPNaRx9lOaRWg2PkE6ySNoyju7rNfunXYtVxPuUIkl0KMq3WXWRb_cb8a_Z"
    "EprqSZUzi_ZzzYzqBNVhIJujcNWij7JRra2sXXiSAfKjtxHQoxrX8n4V1ySWJ3_1T"
    "H_cJcdfS_RKP7YgXRWC0L16PNF5K7iqRqmjKALNe83ZFnFIw";

  class JwtAuthenticatorMock : public Utils::Jwt::JwtAuthenticator {
    public:
      MOCK_METHOD0(onDestroy, void());
      MOCK_METHOD2(Verify, void(std::unique_ptr<Utils::Jwt::JwtTokenExtractor::Token> &token, Utils::Jwt::JwtAuthenticator::Callbacks* callback));
      MOCK_METHOD2(Verify, void(Http::HeaderMap& headers, Utils::Jwt::JwtAuthenticator::Callbacks* callback));
    private:
      void onSuccess(Http::MessagePtr&& ) override {};
      void onFailure(Http::AsyncClient::FailureReason) override {};
  };

  class JwtVerificationFilterTest : public testing::Test {
    public:
      JwtVerificationFilterTest() {}

      virtual ~JwtVerificationFilterTest() {}

      void SetUp() override {
      }

      void TearDown() override { }
    private:
      std::unique_ptr<JwtVerificationFilter> filter_;
      JwtAuthenticatorMock jwt_authenticator_mock_;
  };

  /* Test to verify that when onDestroy() is called the internal jwt_authenticator
   * is also destroyed. */
  TEST_F(JwtVerificationFilterTest, DestroysAuthenticator) {
    // Setup
    ::envoy::config::filter::http::jwt_authn::v2alpha::JwtAuthentication config;

    JwtAuthenticatorMock authenticator_mock;
    std::shared_ptr<Utils::Jwt::JwtAuthenticator> authenticator(&authenticator_mock,
                                                                [](Utils::Jwt::JwtAuthenticator *) {});
    JwtVerificationFilter filter(authenticator, config);

    EXPECT_CALL(authenticator_mock, onDestroy()).Times(1);

    // Act
    filter.onDestroy();
  }

  /* Test that when configured valid tokens are forwarded to the next filter in the chain. */
  TEST_F(JwtVerificationFilterTest, VerifiesSuccessfullyForwardingOriginalToken) {
    // Setup
    Utils::Jwt::Jwt jwt(kJwt);
    auto headers = Http::TestHeaderMapImpl{{"Authorization", "token"}};
    ::envoy::config::filter::http::common::v1alpha::JwtRule rule;
    rule.mutable_forwarder()->set_forward_payload_header("x-some-header");
    rule.mutable_forwarder()->set_forward(true);
    ::envoy::config::filter::http::jwt_authn::v2alpha::JwtAuthentication config;
    auto rules = config.mutable_rules();
    (*rules)[std::string("https://example.com")] = rule;

    JwtAuthenticatorMock authenticator_mock;
    std::shared_ptr<Utils::Jwt::JwtAuthenticator> authenticator(&authenticator_mock,
                                                                [](Utils::Jwt::JwtAuthenticator *) {});
    JwtVerificationFilter filter(authenticator, config);

    EXPECT_CALL(authenticator_mock, Verify(testing::Matcher<Http::HeaderMap&>(_),_)).WillOnce(
      Invoke([&jwt](Http::HeaderMap &, Utils::Jwt::JwtAuthenticator::Callbacks* callback) {
        Http::LowerCaseString header("authorization");
        callback->onSuccess(&jwt, &header);
      })
    );

    // Act
    FilterHeadersStatus status = filter.decodeHeaders(headers, false);

    // Assert
    EXPECT_EQ(status, FilterHeadersStatus::Continue);
    EXPECT_TRUE(headers.get(Http::LowerCaseString("x-some-header")));
    EXPECT_EQ(std::string("token"),
      headers.get(Http::LowerCaseString("authorization"))->value().c_str());
    EXPECT_EQ(jwt.PayloadStrBase64Url(),
      headers.get(Http::LowerCaseString("sec-istio-auth-userinfo"))->value().c_str());
  }

  /* Test that when configured valid tokens are not forwarded to the next filter in the chain. */
  TEST_F(JwtVerificationFilterTest, VerifiesSuccessfullyWithoutForwardingOriginalToken) {
    // Setup
    Utils::Jwt::Jwt jwt(kJwt);
    auto headers = Http::TestHeaderMapImpl{
      {"Authorization", "Bearer " + kJwt}, {"sec-istio-auth-userinfo", "original"}};
    ::envoy::config::filter::http::common::v1alpha::JwtRule rule;
    rule.mutable_forwarder()->set_forward_payload_header("x-some-header");
    rule.mutable_forwarder()->set_forward(false);
    ::envoy::config::filter::http::jwt_authn::v2alpha::JwtAuthentication config;
    auto rules = config.mutable_rules();
    (*rules)[std::string("https://example.com")] = rule;

    JwtAuthenticatorMock authenticator_mock;
    std::shared_ptr<Utils::Jwt::JwtAuthenticator> authenticator(&authenticator_mock,
                                                                [](Utils::Jwt::JwtAuthenticator *) {});
    JwtVerificationFilter filter(authenticator, config);

    EXPECT_CALL(authenticator_mock, Verify(testing::Matcher<Http::HeaderMap&>(_),_)).WillOnce(
      Invoke([&jwt](Http::HeaderMap &, Utils::Jwt::JwtAuthenticator::Callbacks* callback) {
        Http::LowerCaseString header("authorization");
        callback->onSuccess(&jwt, &header);
      })
    );

    // Act
    FilterHeadersStatus status = filter.decodeHeaders(headers, false);

    // Assert
    EXPECT_EQ(status, FilterHeadersStatus::Continue);
    EXPECT_EQ(jwt.PayloadStrBase64Url(), headers.get(Http::LowerCaseString("x-some-header"))->value().c_str());
    EXPECT_FALSE(headers.Authorization());
    EXPECT_EQ(jwt.PayloadStrBase64Url(),
      headers.get(Http::LowerCaseString("sec-istio-auth-userinfo"))->value().c_str());
  }

  /* Test that when token verification fails an HTTP 401 Unauthorized failure is returned. */
  TEST_F(JwtVerificationFilterTest, Fails401) {
    // Setup
    auto headers = Http::TestHeaderMapImpl{
      {"Authorization", "Bearer " + kJwt}, {"sec-istio-auth-userinfo", "original"}};
    ::envoy::config::filter::http::jwt_authn::v2alpha::JwtAuthentication config;
    config.set_allow_missing_or_failed(false);

    JwtAuthenticatorMock authenticator_mock;
    std::shared_ptr<Utils::Jwt::JwtAuthenticator> authenticator(&authenticator_mock,
                                                                [](Utils::Jwt::JwtAuthenticator *) {});
    Http::MockStreamDecoderFilterCallbacks callbacks;
    EXPECT_CALL(callbacks, encodeHeaders_(_, _)).WillOnce(
      Invoke([](Http::HeaderMap &headers, bool) {
        EXPECT_EQ(headers.Status()->value(), "401");
      })
    );
    EXPECT_CALL(callbacks, encodeData(_, _)).WillOnce(
      Invoke([](Buffer::Instance &data, bool) {
        std::string body(static_cast<char*>(data.linearize(data.length())), data.length());
        EXPECT_EQ(body, "Unauthorized");
      })
    );

    JwtVerificationFilter filter(authenticator, config);
    filter.setDecoderFilterCallbacks(callbacks);

    EXPECT_CALL(authenticator_mock, Verify(testing::Matcher<Http::HeaderMap&>(_),_)).WillOnce(
      Invoke([](Http::HeaderMap &, Utils::Jwt::JwtAuthenticator::Callbacks* callback) {
        callback->onError(Utils::Jwt::Status::JWT_MISSED);
      })
    );

    // Act
    FilterHeadersStatus status = filter.decodeHeaders(headers, false);

    // Assert
    EXPECT_EQ(status, FilterHeadersStatus::StopIteration);
  }

  /* Test that when token verification fails and bypass-on-failure enabled, continue decoding. */
  TEST_F(JwtVerificationFilterTest, ContinueOnVerificationErrorWhenBypassEnabled) {
    // Setup
    auto headers = Http::TestHeaderMapImpl{};
    ::envoy::config::filter::http::jwt_authn::v2alpha::JwtAuthentication config;
    config.set_allow_missing_or_failed(true);

    JwtAuthenticatorMock authenticator_mock;
    std::shared_ptr<Utils::Jwt::JwtAuthenticator> authenticator(&authenticator_mock,
                                                                [](Utils::Jwt::JwtAuthenticator *) {});
    Http::MockStreamDecoderFilterCallbacks callbacks;

    JwtVerificationFilter filter(authenticator, config);
    filter.setDecoderFilterCallbacks(callbacks);

    EXPECT_CALL(authenticator_mock, Verify(testing::Matcher<Http::HeaderMap&>(_),_)).WillOnce(
      Invoke([](Http::HeaderMap &, Utils::Jwt::JwtAuthenticator::Callbacks* callback) {
        callback->onError(Utils::Jwt::Status::JWT_MISSED);
      })
    );

    // Act
    FilterHeadersStatus status = filter.decodeHeaders(headers, false);

    // Assert
    EXPECT_EQ(status, FilterHeadersStatus::Continue);
  }
}
} // namespace Http
} // namespace Envoy

