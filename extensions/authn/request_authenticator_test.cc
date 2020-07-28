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

#include "extensions/authn/request_authenticator.h"

#include "common/protobuf/protobuf.h"
#include "envoy/config/core/v3/base.pb.h"
#include "extensions/authn/test_utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "security/v1beta1/request_authentication.pb.h"
#include "test/mocks/http/mocks.h"
#include "test/test_common/utility.h"

using google::protobuf::util::MessageDifferencer;
using istio::authn::JwtPayload;
using istio::authn::Result;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

namespace proxy_wasm {
namespace null_plugin {
namespace AuthN {
namespace {

static constexpr absl::string_view kExchangedTokenHeaderName =
    "ingress-authorization";

static constexpr absl::string_view kExchangedTokenOriginalPayload =
    "original_claims";

static constexpr absl::string_view kSecIstioAuthUserinfoHeaderValue =
    R"(
     {
       "iss": "issuer@foo.com",
       "sub": "sub@foo.com",
       "aud": ["aud1", "aud2"],
       "non-string-will-be-ignored": 1512754205,
       "some-other-string-claims": "some-claims-kept"
     }
   )";

static constexpr absl::string_view kExchangedTokenPayload =
    R"(
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

static constexpr absl::string_view kExchangedTokenPayloadNoOriginalClaims =
    R"(
     {
       "iss": "token-service",
       "sub": "subject",
       "aud": ["aud1", "aud2"]
     }
   )";

class ValidateJwtTest : public testing::Test {
 public:
  virtual ~ValidateJwtTest() {}

  void addJwtRule(istio::security::v1beta1::JWTRule& rule) {
    request_authentication_policy_.mutable_jwt_rules()->Add(std::move(rule));
  }

  void createAuthenticator() {
    authenticator_.reset();
    authenticator_ = std::make_unique<RequestAuthenticator>(
        filter_context_, request_authentication_policy_);
  }

  void createFilterContext() {
    filter_context_.reset();
    filter_context_ = std::make_unique<FilterContext>(
        dynamic_metadata_, header_, nullptr,
        istio::envoy::config::filter::http::authn::v2alpha2::FilterConfig::
            default_instance());
  }

  void addEnvoyFilterMetadata(Envoy::ProtobufWkt::Struct& message) {
    (*dynamic_metadata_.mutable_filter_metadata())
        [Envoy::Extensions::HttpFilters::HttpFilterNames::get().JwtAuthn]
            .MergeFrom(message);
  }

  void checkResultPayload() {
    // Only to check result_payload_.raw_claims, which should be the same to
    // passed JWT payload, which is like kSecIstioAuthUserinfoHeaderValue.
    Envoy::ProtobufWkt::Struct result_payload_raw_claims;
    JsonStringToMessage(result_payload_.raw_claims(),
                        &result_payload_raw_claims,
                        google::protobuf::util::JsonParseOptions{});

    auto jwt_payload_fields = jwt_payload_.fields();
    if (expect_token_exchanged_ &&
        jwt_payload_fields.find(kExchangedTokenOriginalPayload.data()) !=
            jwt_payload_fields.end()) {
      EXPECT_TRUE(MessageDifferencer::Equals(
          result_payload_raw_claims,
          jwt_payload_fields.at(kExchangedTokenOriginalPayload.data())
              .struct_value()));
    } else {
      EXPECT_TRUE(
          MessageDifferencer::Equals(result_payload_raw_claims, jwt_payload_));
    }

    // Next, check fields which except raw_claims that already checked.
    // Because expected_payload_ is not expected to have raw_claims, it cuts
    // raw_claims from result_payload.
    ASSERT(expected_payload_.raw_claims().empty());
    result_payload_.clear_raw_claims();
    EXPECT_TRUE(MessageDifferencer::Equals(result_payload_, expected_payload_));
  }

  void initialize() {
    createFilterContext();
    createAuthenticator();
  }

 protected:
  std::unique_ptr<RequestAuthenticator> authenticator_;
  istio::security::v1beta1::RequestAuthentication
      request_authentication_policy_;
  Envoy::ProtobufWkt::Struct jwt_payload_;
  istio::authn::JwtPayload result_payload_;
  istio::authn::JwtPayload expected_payload_;
  envoy::config::core::v3::Metadata dynamic_metadata_;
  Envoy::Http::TestRequestHeaderMapImpl header_;
  FilterContextPtr filter_context_;
  bool expect_token_exchanged_{false};
};

TEST_F(ValidateJwtTest, NoIstioAuthnConfig) {
  istio::security::v1beta1::JWTRule jwt_rule;
  jwt_rule.set_issuer("issuer@foo.com");
  addJwtRule(jwt_rule);
  initialize();

  // authenticator_ has empty Istio authn config
  // When there is empty Istio authn config, validateJwt() should return
  // nullptr and failure.
  EXPECT_FALSE(authenticator_->validateJwt(&result_payload_));
  EXPECT_TRUE(MessageDifferencer::Equals(result_payload_, expected_payload_));
}

TEST_F(ValidateJwtTest, NoIssuer) {
  // no issuer in jwt
  initialize();

  // When there is no issuer in the JWT config, validateJwt() should return
  // nullptr and failure.
  EXPECT_FALSE(authenticator_->validateJwt(&result_payload_));
  EXPECT_TRUE(MessageDifferencer::Equals(result_payload_, expected_payload_));
}

TEST_F(ValidateJwtTest, HasJwtPayloadOutputButNoDataForIssuer) {
  istio::security::v1beta1::JWTRule jwt_rule;
  jwt_rule.set_issuer("issuer@foo.com");
  addJwtRule(jwt_rule);
  auto filter_metadata = Envoy::MessageUtil::keyValueStruct("foo", "bar");
  addEnvoyFilterMetadata(filter_metadata);
  initialize();

  // When there is no JWT payload for given issuer in request info dynamic
  // metadata, validateJwt() should return nullptr and failure.
  EXPECT_FALSE(authenticator_->validateJwt(&result_payload_));
  EXPECT_TRUE(MessageDifferencer::Equals(result_payload_, expected_payload_));
}

TEST_F(ValidateJwtTest, HasJwtPayloadOutPutButWithInvalidData) {
  istio::security::v1beta1::JWTRule jwt_rule;
  jwt_rule.set_issuer("issuer@foo.com");
  addJwtRule(jwt_rule);
  auto filter_metadata =
      Envoy::MessageUtil::keyValueStruct("issuer@foo.com", "bar");
  addEnvoyFilterMetadata(filter_metadata);
  initialize();

  EXPECT_FALSE(authenticator_->validateJwt(&result_payload_));
  EXPECT_TRUE(MessageDifferencer::Equals(result_payload_, expected_payload_));
}

TEST_F(ValidateJwtTest, MultipleJwtRulesWithValidJwt) {
  std::array<std::string, 3> issuer{"issuer2@foo.com", "issuer1@foo.com", "issuer@foo.com"};
  for (auto&& i : issuer) {
    istio::security::v1beta1::JWTRule jwt_rule;
    jwt_rule.set_issuer(i);
    addJwtRule(jwt_rule);
  }
  JsonStringToMessage(kSecIstioAuthUserinfoHeaderValue.data(), &jwt_payload_,
                      google::protobuf::util::JsonParseOptions{});
  Envoy::ProtobufWkt::Struct payload_to_pass;
  (*payload_to_pass.mutable_fields())["issuer@foo.com"]
      .mutable_struct_value()
      ->CopyFrom(jwt_payload_);
  addEnvoyFilterMetadata(payload_to_pass);
  initialize();

  EXPECT_TRUE(authenticator_->validateJwt(&result_payload_));
}

TEST_F(ValidateJwtTest, MultipleJwtRulesWithInvalidJwt) {
  std::array<std::string, 3> issuer{"issuer2@foo.com", "issuer1@foo.com", "issuer@foo.com"};
  for (auto&& i : issuer) {
    istio::security::v1beta1::JWTRule jwt_rule;
    jwt_rule.set_issuer(i);
    addJwtRule(jwt_rule);
  }
  JsonStringToMessage(kSecIstioAuthUserinfoHeaderValue.data(), &jwt_payload_,
                      google::protobuf::util::JsonParseOptions{});
  Envoy::ProtobufWkt::Struct payload_to_pass;
  (*payload_to_pass.mutable_fields())["dummy@foo.com"]
      .mutable_struct_value()
      ->CopyFrom(jwt_payload_);
  addEnvoyFilterMetadata(payload_to_pass);
  initialize();

  EXPECT_FALSE(authenticator_->validateJwt(&result_payload_));
}

TEST_F(ValidateJwtTest, HasJwtPayloadOutput) {
  JsonStringToMessage(kSecIstioAuthUserinfoHeaderValue.data(), &jwt_payload_,
                      google::protobuf::util::JsonParseOptions{});
  Envoy::ProtobufWkt::Struct payload_to_pass;
  (*payload_to_pass.mutable_fields())["issuer@foo.com"]
      .mutable_struct_value()
      ->CopyFrom(jwt_payload_);

  istio::security::v1beta1::JWTRule jwt_rule;
  jwt_rule.set_issuer("issuer@foo.com");
  addJwtRule(jwt_rule);
  addEnvoyFilterMetadata(payload_to_pass);

  JsonStringToMessage(
      R"(
  {
    "user": "issuer@foo.com/sub@foo.com",
    "audiences": ["aud1", "aud2"],
    "presenter": "",
    "claims": {
      "aud": ["aud1", "aud2"],
      "iss": ["issuer@foo.com"],
      "some-other-string-claims": ["some-claims-kept"],
      "sub": ["sub@foo.com"],
    }
  }
)",
      &expected_payload_, google::protobuf::util::JsonParseOptions{});
  initialize();

  EXPECT_TRUE(authenticator_->validateJwt(&result_payload_));
  checkResultPayload();
}

TEST_F(ValidateJwtTest, HasJwtPayloadOutputWithTokenExchanges) {
  JsonStringToMessage(kExchangedTokenPayload.data(), &jwt_payload_,
                      google::protobuf::util::JsonParseOptions{});
  Envoy::ProtobufWkt::Struct payload_to_pass;
  (*payload_to_pass.mutable_fields())["token-service"]
      .mutable_struct_value()
      ->CopyFrom(jwt_payload_);

  istio::security::v1beta1::JWTRule jwt_rule;
  jwt_rule.set_issuer("token-service");
  istio::security::v1beta1::JWTHeader jwt_header;
  jwt_header.set_name(kExchangedTokenHeaderName.data());
  jwt_header.set_prefix("Bearer ");
  *jwt_rule.add_from_headers() = jwt_header;
  addJwtRule(jwt_rule);
  addEnvoyFilterMetadata(payload_to_pass);
  expect_token_exchanged_ = true;

  JsonStringToMessage(
      R"(
  {
    "user": "https://accounts.example.com/example-subject",
    "claims": {
      "iss": ["https://accounts.example.com"],
      "sub": ["example-subject"],
      "email": ["user@example.com"]
    }
  }
)",
      &expected_payload_, google::protobuf::util::JsonParseOptions{});
  initialize();

  EXPECT_TRUE(authenticator_->validateJwt(&result_payload_));
  checkResultPayload();
}

TEST_F(ValidateJwtTest, HasJwtPayloadOutputWithoutTokenExchanges) {
  JsonStringToMessage(kExchangedTokenPayloadNoOriginalClaims.data(),
                      &jwt_payload_,
                      google::protobuf::util::JsonParseOptions{});
  Envoy::ProtobufWkt::Struct payload_to_pass;
  (*payload_to_pass.mutable_fields())["token-service"]
      .mutable_struct_value()
      ->CopyFrom(jwt_payload_);

  istio::security::v1beta1::JWTRule jwt_rule;
  jwt_rule.set_issuer("token-service");
  istio::security::v1beta1::JWTHeader jwt_header;
  jwt_header.set_name(kExchangedTokenHeaderName.data());
  jwt_header.set_prefix("Bearer ");
  *jwt_rule.add_from_headers() = jwt_header;
  addJwtRule(jwt_rule);
  addEnvoyFilterMetadata(payload_to_pass);
  initialize();

  EXPECT_FALSE(authenticator_->validateJwt(&result_payload_));
}

TEST_F(ValidateJwtTest,
       HasJwtPayloadOutputWithTokenExchangesAndNoExchangedTokenHeaderName) {
  JsonStringToMessage(kExchangedTokenPayload.data(), &jwt_payload_,
                      google::protobuf::util::JsonParseOptions{});
  Envoy::ProtobufWkt::Struct payload_to_pass;
  (*payload_to_pass.mutable_fields())["token-service"]
      .mutable_struct_value()
      ->CopyFrom(jwt_payload_);

  istio::security::v1beta1::JWTRule jwt_rule;
  jwt_rule.set_issuer("token-service");
  addJwtRule(jwt_rule);
  addEnvoyFilterMetadata(payload_to_pass);

  JsonStringToMessage(
      R"(
  {
    "user": "token-service/subject",
    "audiences": ["aud1", "aud2"],
    "claims": {
      "iss": ["token-service"],
      "sub": ["subject"],
      "aud": ["aud1", "aud2"]
    }
  }
)",
      &expected_payload_, google::protobuf::util::JsonParseOptions{});
  initialize();

  EXPECT_TRUE(authenticator_->validateJwt(&result_payload_));
  checkResultPayload();
}

static constexpr absl::string_view kSingleOriginMethodPolicy = R"(
  jwt_rules {
    issuer: "istio.io"
  }
)";

class MockRequestAuthenticator : public RequestAuthenticator {
public:
  MockRequestAuthenticator(
      FilterContextPtr filter_context,
      const istio::security::v1beta1::RequestAuthentication& policy);
  MOCK_METHOD(bool, validateJwt, (istio::authn::JwtPayload*));
};

MockRequestAuthenticator::MockRequestAuthenticator(
      FilterContextPtr filter_context,
      const istio::security::v1beta1::RequestAuthentication& policy) : RequestAuthenticator(filter_context, policy) {}

class RequestAuthenticatorTest : public testing::Test {
 public:
  RequestAuthenticatorTest() {}
  virtual ~RequestAuthenticatorTest() {}

  void createAuthenticator() {
    authenticator_.reset();
    authenticator_ =
        std::make_unique<MockRequestAuthenticator>(filter_context_, request_authentication_policy_);
  }

 protected:
  std::unique_ptr<MockRequestAuthenticator> authenticator_;
  envoy::config::core::v3::Metadata metadata_;
  Envoy::Http::TestRequestHeaderMapImpl header_{};
  FilterContextPtr filter_context_{std::make_unique<FilterContext>(
      envoy::config::core::v3::Metadata::default_instance(), header_, nullptr,
      istio::envoy::config::filter::http::authn::v2alpha2::FilterConfig::
          default_instance())};
  istio::security::v1beta1::RequestAuthentication request_authentication_policy_;
  istio::authn::Payload jwt_payload_;
  Result expected_result_;
};

TEST_F(RequestAuthenticatorTest, Empty) {
  createAuthenticator();

  EXPECT_FALSE(authenticator_->run(&jwt_payload_));
  EXPECT_TRUE(
          MessageDifferencer::Equals(expected_result_, filter_context_->authenticationResult()));
}

TEST_F(RequestAuthenticatorTest, Pass) {
  ASSERT_TRUE(Envoy::Protobuf::TextFormat::ParseFromString(
      kSingleOriginMethodPolicy.data(), &request_authentication_policy_));
  jwt_payload_ = TestUtilities::CreateJwtPayload("foo", "istio.io");
  createAuthenticator();
  
  EXPECT_CALL(*authenticator_, validateJwt(_)).WillOnce(Return(true));
  EXPECT_TRUE(authenticator_->run(&jwt_payload_));
}

TEST_F(RequestAuthenticatorTest, CORSPreflight) {
  ASSERT_TRUE(Envoy::Protobuf::TextFormat::ParseFromString(
      kSingleOriginMethodPolicy.data(), &request_authentication_policy_));
  jwt_payload_ = TestUtilities::CreateJwtPayload("foo", "istio.io");
  createAuthenticator();

  header_.addCopy(":method", "OPTIONS");
  header_.addCopy("origin", "example.com");
  header_.addCopy("access-control-request-method", "GET");

  EXPECT_TRUE(authenticator_->run(&jwt_payload_));
}

}  // namespace
}  // namespace AuthN
}  // namespace null_plugin
}  // namespace proxy_wasm
