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

#include "source/extensions/filters/http/authn/authenticator_base.h"

#include "envoy/config/core/v3/base.pb.h"
#include "envoy/config/filter/http/authn/v2alpha1/config.pb.h"
#include "gmock/gmock.h"
#include "source/common/common/base64.h"
#include "source/common/protobuf/protobuf.h"
#include "source/extensions/common/filter_names.h"
#include "source/extensions/filters/http/authn/test_utils.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/ssl/mocks.h"
#include "test/test_common/status_utility.h"

using google::protobuf::util::MessageDifferencer;
using istio::authn::Payload;
using istio::envoy::config::filter::http::authn::v2alpha1::FilterConfig;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

namespace iaapi = istio::authentication::v1alpha1;

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
       "aud": ["aud1", "aud2"],
       "non-string-will-be-ignored": 1512754205,
       "some-other-string-claims": "some-claims-kept"
     }
   )";

const std::string kExchangedTokenHeaderName = "ingress-authorization";

const std::string kExchangedTokenPayload =
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

const std::string kExchangedTokenPayloadNoOriginalClaims =
    R"(
     {
       "iss": "token-service",
       "sub": "subject",
       "aud": ["aud1", "aud2"]
     }
   )";

class MockAuthenticatorBase : public AuthenticatorBase {
public:
  MockAuthenticatorBase(FilterContext* filter_context) : AuthenticatorBase(filter_context) {}
  MOCK_METHOD1(run, bool(Payload*));
};

class ValidateX509Test : public testing::TestWithParam<iaapi::MutualTls::Mode>,
                         public Logger::Loggable<Logger::Id::filter> {
public:
  virtual ~ValidateX509Test() {}

  NiceMock<Envoy::Network::MockConnection> connection_{};
  Http::RequestHeaderMapPtr header_ = Envoy::Http::RequestHeaderMapImpl::create();
  FilterConfig filter_config_{};
  FilterContext filter_context_{envoy::config::core::v3::Metadata::default_instance(), *header_,
                                &connection_, filter_config_};

  MockAuthenticatorBase authenticator_{&filter_context_};

  void SetUp() override {
    mtls_params_.set_mode(ValidateX509Test::GetParam());
    payload_ = new Payload();
  }

  void TearDown() override { delete (payload_); }

protected:
  iaapi::MutualTls mtls_params_;
  iaapi::Jwt jwt_;
  Payload* payload_;
  Payload default_payload_;
};

TEST_P(ValidateX509Test, PlaintextConnection) {
  // Should return false except mode is PERMISSIVE (accept plaintext)
  if (ValidateX509Test::GetParam() == iaapi::MutualTls::PERMISSIVE) {
    EXPECT_TRUE(authenticator_.validateX509(mtls_params_, payload_));
  } else {
    EXPECT_FALSE(authenticator_.validateX509(mtls_params_, payload_));
  }
  EXPECT_TRUE(MessageDifferencer::Equals(*payload_, default_payload_));
}

TEST_P(ValidateX509Test, SslConnectionWithNoPeerCert) {
  auto ssl = std::make_shared<NiceMock<Ssl::MockConnectionInfo>>();
  ON_CALL(*ssl, peerCertificatePresented()).WillByDefault(Return(false));
  EXPECT_CALL(Const(connection_), ssl()).WillRepeatedly(Return(ssl));

  // Should return false except mode is PERMISSIVE (accept plaintext).
  if (ValidateX509Test::GetParam() == iaapi::MutualTls::PERMISSIVE) {
    EXPECT_TRUE(authenticator_.validateX509(mtls_params_, payload_));
  } else {
    EXPECT_FALSE(authenticator_.validateX509(mtls_params_, payload_));
  }
  EXPECT_TRUE(MessageDifferencer::Equals(*payload_, default_payload_));
}

TEST_P(ValidateX509Test, SslConnectionWithPeerCert) {
  auto ssl = std::make_shared<NiceMock<Ssl::MockConnectionInfo>>();
  ON_CALL(*ssl, peerCertificatePresented()).WillByDefault(Return(true));
  ON_CALL(*ssl, uriSanPeerCertificate()).WillByDefault(Return(std::vector<std::string>{"foo"}));
  EXPECT_CALL(Const(connection_), ssl()).WillRepeatedly(Return(ssl));

  // Should return false due to unable to extract trust domain from principal.
  EXPECT_FALSE(authenticator_.validateX509(mtls_params_, payload_));
  // When client certificate is present on mTLS, authenticated attribute should
  // be extracted.
  EXPECT_EQ(payload_->x509().user(), "foo");
}

TEST_P(ValidateX509Test, SslConnectionWithCertsSkipTrustDomainValidation) {
  // skip trust domain validation.
  google::protobuf::util::JsonParseOptions options;
  ASSERT_OK(JsonStringToMessage("{ skip_validate_trust_domain: true }", &filter_config_, options));

  auto ssl = std::make_shared<NiceMock<Ssl::MockConnectionInfo>>();
  ON_CALL(*ssl, peerCertificatePresented()).WillByDefault(Return(true));
  ON_CALL(*ssl, uriSanPeerCertificate()).WillByDefault(Return(std::vector<std::string>{"foo"}));
  EXPECT_CALL(Const(connection_), ssl()).WillRepeatedly(Return(ssl));

  // Should return true due to trust domain validation skipped.
  EXPECT_TRUE(authenticator_.validateX509(mtls_params_, payload_));
  EXPECT_EQ(payload_->x509().user(), "foo");
}

TEST_P(ValidateX509Test, SslConnectionWithSpiffeCertsSameTrustDomain) {
  auto ssl = std::make_shared<NiceMock<Ssl::MockConnectionInfo>>();
  ON_CALL(*ssl, peerCertificatePresented()).WillByDefault(Return(true));
  ON_CALL(*ssl, uriSanPeerCertificate())
      .WillByDefault(Return(std::vector<std::string>{"spiffe://td/foo"}));
  ON_CALL(*ssl, uriSanLocalCertificate())
      .WillByDefault(Return(std::vector<std::string>{"spiffe://td/bar"}));
  EXPECT_CALL(Const(connection_), ssl()).WillRepeatedly(Return(ssl));

  EXPECT_TRUE(authenticator_.validateX509(mtls_params_, payload_));
  // When client certificate is present on mTLS, authenticated attribute should
  // be extracted.
  EXPECT_EQ(payload_->x509().user(), "td/foo");
}

TEST_P(ValidateX509Test, SslConnectionWithSpiffeCertsDifferentTrustDomain) {
  auto ssl = std::make_shared<NiceMock<Ssl::MockConnectionInfo>>();
  ON_CALL(*ssl, peerCertificatePresented()).WillByDefault(Return(true));
  ON_CALL(*ssl, uriSanPeerCertificate())
      .WillByDefault(Return(std::vector<std::string>{"spiffe://td-1/foo"}));
  ON_CALL(*ssl, uriSanLocalCertificate())
      .WillByDefault(Return(std::vector<std::string>{"spiffe://td-2/bar"}));
  EXPECT_CALL(Const(connection_), ssl()).WillRepeatedly(Return(ssl));

  // Should return false due to trust domain validation failed.
  EXPECT_FALSE(authenticator_.validateX509(mtls_params_, payload_));
  // When client certificate is present on mTLS, authenticated attribute should
  // be extracted.
  EXPECT_EQ(payload_->x509().user(), "td-1/foo");
}

TEST_P(ValidateX509Test, SslConnectionWithPeerMalformedSpiffeCert) {
  // skip trust domain validation.
  google::protobuf::util::JsonParseOptions options;
  ASSERT_OK(JsonStringToMessage("{ skip_validate_trust_domain: true }", &filter_config_, options));

  auto ssl = std::make_shared<NiceMock<Ssl::MockConnectionInfo>>();
  ON_CALL(*ssl, peerCertificatePresented()).WillByDefault(Return(true));
  ON_CALL(*ssl, uriSanPeerCertificate())
      .WillByDefault(Return(std::vector<std::string>{"spiffe:foo"}));
  ON_CALL(*ssl, uriSanLocalCertificate())
      .WillByDefault(Return(std::vector<std::string>{"spiffe://td-2/bar"}));
  EXPECT_CALL(Const(connection_), ssl()).WillRepeatedly(Return(ssl));

  EXPECT_TRUE(authenticator_.validateX509(mtls_params_, payload_));
  // When client certificate is present on mTLS and the spiffe subject format is
  // wrong
  // ("spiffe:foo" instead of "spiffe://foo"), the user attribute should be
  // extracted.
  EXPECT_EQ(payload_->x509().user(), "spiffe:foo");
}

INSTANTIATE_TEST_SUITE_P(ValidateX509Tests, ValidateX509Test,
                         testing::Values(iaapi::MutualTls::STRICT, iaapi::MutualTls::PERMISSIVE));

class ValidateJwtTest : public testing::Test, public Logger::Loggable<Logger::Id::filter> {
public:
  virtual ~ValidateJwtTest() {}

  // StrictMock<Envoy::RequestInfo::MockRequestInfo> request_info_{};
  envoy::config::core::v3::Metadata dynamic_metadata_;
  NiceMock<Envoy::Network::MockConnection> connection_{};
  Http::RequestHeaderMapPtr header_ = Envoy::Http::RequestHeaderMapImpl::create();
  FilterConfig filter_config_{};
  FilterContext filter_context_{dynamic_metadata_, *header_, &connection_, filter_config_};
  MockAuthenticatorBase authenticator_{&filter_context_};

  void SetUp() override { payload_ = new Payload(); }

  void TearDown() override { delete (payload_); }

protected:
  iaapi::MutualTls mtls_params_;
  iaapi::Jwt jwt_;
  Payload* payload_;
  Payload default_payload_;
};

TEST_F(ValidateJwtTest, NoIstioAuthnConfig) {
  jwt_.set_issuer("issuer@foo.com");
  // authenticator_ has empty Istio authn config
  // When there is empty Istio authn config, validateJwt() should return
  // nullptr and failure.
  EXPECT_FALSE(authenticator_.validateJwt(jwt_, payload_));
  EXPECT_TRUE(MessageDifferencer::Equals(*payload_, default_payload_));
}

TEST_F(ValidateJwtTest, NoIssuer) {
  // no issuer in jwt
  google::protobuf::util::JsonParseOptions options;
  ASSERT_OK(JsonStringToMessage(
      R"({
              "jwt_output_payload_locations":
              {
                "issuer@foo.com": "sec-istio-auth-userinfo"
              }
           }
        )",
      &filter_config_, options));

  // When there is no issuer in the JWT config, validateJwt() should return
  // nullptr and failure.
  EXPECT_FALSE(authenticator_.validateJwt(jwt_, payload_));
  EXPECT_TRUE(MessageDifferencer::Equals(*payload_, default_payload_));
}

TEST_F(ValidateJwtTest, OutputPayloadLocationNotDefine) {
  jwt_.set_issuer("issuer@foo.com");
  google::protobuf::util::JsonParseOptions options;
  ASSERT_OK(JsonStringToMessage(
      R"({
              "jwt_output_payload_locations":
              {
              }
           }
        )",
      &filter_config_, options));

  // authenticator has empty jwt_output_payload_locations in Istio authn config
  // When there is no matching jwt_output_payload_locations for the issuer in
  // the Istio authn config, validateJwt() should return nullptr and failure.
  EXPECT_FALSE(authenticator_.validateJwt(jwt_, payload_));
  EXPECT_TRUE(MessageDifferencer::Equals(*payload_, default_payload_));
}

TEST_F(ValidateJwtTest, NoJwtPayloadOutput) {
  jwt_.set_issuer("issuer@foo.com");

  // When there is no JWT in request info dynamic metadata, validateJwt() should
  // return nullptr and failure.
  EXPECT_FALSE(authenticator_.validateJwt(jwt_, payload_));
  EXPECT_TRUE(MessageDifferencer::Equals(*payload_, default_payload_));
}

TEST_F(ValidateJwtTest, HasJwtPayloadOutputButNoDataForKey) {
  jwt_.set_issuer("issuer@foo.com");

  (*dynamic_metadata_
        .mutable_filter_metadata())[Extensions::HttpFilters::HttpFilterNames::get().JwtAuthn]
      .MergeFrom(MessageUtil::keyValueStruct("foo", "bar"));

  // When there is no JWT payload for given issuer in request info dynamic
  // metadata, validateJwt() should return nullptr and failure.
  EXPECT_FALSE(authenticator_.validateJwt(jwt_, payload_));
  EXPECT_TRUE(MessageDifferencer::Equals(*payload_, default_payload_));
}

TEST_F(ValidateJwtTest, JwtPayloadAvailableWithBadData) {
  jwt_.set_issuer("issuer@foo.com");
  (*dynamic_metadata_
        .mutable_filter_metadata())[Extensions::HttpFilters::HttpFilterNames::get().JwtAuthn]
      .MergeFrom(MessageUtil::keyValueStruct("issuer@foo.com", "bad-data"));
  // EXPECT_CALL(request_info_, dynamicMetadata());

  EXPECT_FALSE(authenticator_.validateJwt(jwt_, payload_));
  EXPECT_TRUE(MessageDifferencer::Equivalent(*payload_, default_payload_));
}

TEST_F(ValidateJwtTest, JwtPayloadAvailable) {
  jwt_.set_issuer("issuer@foo.com");
  google::protobuf::Struct header_payload;
  ASSERT_OK(JsonStringToMessage(kSecIstioAuthUserinfoHeaderValue, &header_payload,
                                google::protobuf::util::JsonParseOptions{}));
  google::protobuf::Struct payload;
  (*payload.mutable_fields())["issuer@foo.com"].mutable_struct_value()->CopyFrom(header_payload);
  (*dynamic_metadata_
        .mutable_filter_metadata())[Extensions::HttpFilters::HttpFilterNames::get().JwtAuthn]
      .MergeFrom(payload);

  Payload expected_payload;
  ASSERT_OK(JsonStringToMessage(
      R"({
             "jwt": {
               "user": "issuer@foo.com/sub@foo.com",
               "audiences": ["aud1", "aud2"],
               "presenter": "",
               "claims": {
                 "aud": ["aud1", "aud2"],
                 "iss": ["issuer@foo.com"],
                 "some-other-string-claims": ["some-claims-kept"],
                 "sub": ["sub@foo.com"],
               },
               "raw_claims": "\n     {\n       \"iss\": \"issuer@foo.com\",\n       \"sub\": \"sub@foo.com\",\n       \"aud\": [\"aud1\", \"aud2\"],\n       \"non-string-will-be-ignored\": 1512754205,\n       \"some-other-string-claims\": \"some-claims-kept\"\n     }\n   ",
             }
           }
        )",
      &expected_payload, google::protobuf::util::JsonParseOptions{}));

  EXPECT_TRUE(authenticator_.validateJwt(jwt_, payload_));
  MessageDifferencer diff;
  const google::protobuf::FieldDescriptor* field =
      expected_payload.jwt().GetDescriptor()->FindFieldByName("raw_claims");
  diff.IgnoreField(field);
  EXPECT_TRUE(diff.Compare(expected_payload, *payload_));
}

TEST_F(ValidateJwtTest, OriginalPayloadOfExchangedToken) {
  jwt_.set_issuer("token-service");
  jwt_.add_jwt_headers(kExchangedTokenHeaderName);

  google::protobuf::Struct exchange_token_payload;
  ASSERT_OK(JsonStringToMessage(kExchangedTokenPayload, &exchange_token_payload,
                                google::protobuf::util::JsonParseOptions{}));
  google::protobuf::Struct payload;
  (*payload.mutable_fields())["token-service"].mutable_struct_value()->CopyFrom(
      exchange_token_payload);
  (*dynamic_metadata_
        .mutable_filter_metadata())[Extensions::HttpFilters::HttpFilterNames::get().JwtAuthn]
      .MergeFrom(payload);

  Payload expected_payload;
  ASSERT_OK(JsonStringToMessage(
      R"({
             "jwt": {
               "user": "https://accounts.example.com/example-subject",
               "claims": {
                 "iss": ["https://accounts.example.com"],
                 "sub": ["example-subject"],
                 "email": ["user@example.com"]
               },
               "raw_claims": "{\"email\":\"user@example.com\",\"iss\":\"https://accounts.example.com\",\"sub\":\"example-subject\"}"
             }
           }
        )",
      &expected_payload, google::protobuf::util::JsonParseOptions{}));

  EXPECT_TRUE(authenticator_.validateJwt(jwt_, payload_));
  // On different platforms, the order of fields in raw_claims may be
  // different. E.g., on MacOs, the raw_claims in the payload_ can be:
  // raw_claims:
  // "{\"email\":\"user@example.com\",\"sub\":\"example-subject\",\"iss\":\"https://accounts.example.com\"}"
  // Therefore, raw_claims is skipped to avoid a flaky test.
  MessageDifferencer diff;
  const google::protobuf::FieldDescriptor* field =
      expected_payload.jwt().GetDescriptor()->FindFieldByName("raw_claims");
  diff.IgnoreField(field);
  EXPECT_TRUE(diff.Compare(expected_payload, *payload_));
}

TEST_F(ValidateJwtTest, OriginalPayloadOfExchangedTokenMissing) {
  jwt_.set_issuer("token-service");
  jwt_.add_jwt_headers(kExchangedTokenHeaderName);

  google::protobuf::Struct exchange_token_payload;
  ASSERT_OK(JsonStringToMessage(kExchangedTokenPayloadNoOriginalClaims, &exchange_token_payload,
                                google::protobuf::util::JsonParseOptions{}));
  google::protobuf::Struct payload;
  (*payload.mutable_fields())["token-service"].mutable_struct_value()->CopyFrom(
      exchange_token_payload);
  (*dynamic_metadata_
        .mutable_filter_metadata())[Extensions::HttpFilters::HttpFilterNames::get().JwtAuthn]
      .MergeFrom(payload);

  // When no original_claims in an exchanged token, the token
  // is treated as invalid.
  EXPECT_FALSE(authenticator_.validateJwt(jwt_, payload_));
}

TEST_F(ValidateJwtTest, OriginalPayloadOfExchangedTokenNotInIntendedHeader) {
  jwt_.set_issuer("token-service");

  google::protobuf::Struct exchange_token_payload;
  ASSERT_OK(JsonStringToMessage(kExchangedTokenPayload, &exchange_token_payload,
                                google::protobuf::util::JsonParseOptions{}));
  google::protobuf::Struct payload;
  (*payload.mutable_fields())["token-service"].mutable_struct_value()->CopyFrom(
      exchange_token_payload);
  (*dynamic_metadata_
        .mutable_filter_metadata())[Extensions::HttpFilters::HttpFilterNames::get().JwtAuthn]
      .MergeFrom(payload);

  Payload expected_payload;
  ASSERT_OK(JsonStringToMessage(
      R"({
             "jwt": {
               "user": "token-service/subject",
               "audiences": ["aud1", "aud2"],
               "claims": {
                 "iss": ["token-service"],
                 "sub": ["subject"],
                 "aud": ["aud1", "aud2"],
                 "original_claims": {
                   "iss": ["https://accounts.example.com"],
                   "sub": ["example-subject"],
                   "email": ["user@example.com"]
                 }
               },
               "raw_claims":"\n     {\n       \"iss\": \"token-service\",\n       \"sub\": \"subject\",\n       \"aud\": [\"aud1\", \"aud2\"],\n       \"original_claims\": {\n         \"iss\": \"https://accounts.example.com\",\n         \"sub\": \"example-subject\",\n         \"email\": \"user@example.com\"\n       }\n     }\n   "
             }
           }
        )",
      &expected_payload, google::protobuf::util::JsonParseOptions{}));

  // When an exchanged token is not in the intended header, the token
  // is treated as a normal token with its claims extracted.
  EXPECT_TRUE(authenticator_.validateJwt(jwt_, payload_));
  MessageDifferencer diff;
  const google::protobuf::FieldDescriptor* field =
      expected_payload.jwt().GetDescriptor()->FindFieldByName("raw_claims");
  diff.IgnoreField(field);
  EXPECT_TRUE(diff.Compare(expected_payload, *payload_));
}

} // namespace
} // namespace AuthN
} // namespace Istio
} // namespace Http
} // namespace Envoy
