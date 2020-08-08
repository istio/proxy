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

#include "extensions/authn/peer_authenticator.h"

#include "extensions/authn/test_utils.h"
#include "gtest/gtest.h"
#include "test/test_common/utility.h"

using ::google::protobuf::util::MessageDifferencer;
using testing::Invoke;
using testing::Return;

namespace Extensions {
namespace AuthN {
namespace {

namespace {
using istio::security::v1beta1::PeerAuthentication;

static PeerAuthentication::MutualTLS::Mode disable =
    PeerAuthentication::MutualTLS::DISABLE;
static PeerAuthentication::MutualTLS::Mode strict =
    PeerAuthentication::MutualTLS::STRICT;
static PeerAuthentication::MutualTLS::Mode permissive =
    PeerAuthentication::MutualTLS::PERMISSIVE;
}  // namespace

class MockConnectionContext : public ConnectionContext {
 public:
  MOCK_METHOD(absl::optional<std::string>, trustDomain, (bool));
  MOCK_METHOD(absl::optional<std::string>, principalDomain, (bool));
  MOCK_METHOD(bool, isMutualTls, (), (const));
  MOCK_METHOD(absl::optional<uint32_t>, port, (), (const));
};

class ValidateX509Test : public testing::Test {
 public:
  void setMtlsMode(
      istio::security::v1beta1::PeerAuthentication::MutualTLS::Mode mode) {
    peer_authentication_policy_.mutable_mtls()->set_mode(mode);
  }

  void initialize() {
    filter_context_.reset();
    filter_context_ = std::make_unique<FilterContext>(
        envoy::config::core::v3::Metadata::default_instance(),
        Envoy::Http::TestRequestHeaderMapImpl(), connection_context_,
        istio::envoy::config::filter::http::authn::v2alpha2::FilterConfig::
            default_instance());

    authenticator_.reset();
    authenticator_ = std::make_unique<PeerAuthenticatorImpl>(
        filter_context_, peer_authentication_policy_);
  }

 protected:
  std::unique_ptr<PeerAuthenticatorImpl> authenticator_;
  FilterContextPtr filter_context_;
  istio::security::v1beta1::PeerAuthentication peer_authentication_policy_;
  istio::authn::X509Payload result_payload_;
  std::shared_ptr<MockConnectionContext> connection_context_{
      std::make_shared<MockConnectionContext>()};
};

TEST_F(ValidateX509Test, EmptyPolicy) {
  initialize();

  // When there is no specified policy. It will be choiced UNSET as MutualTLS
  // policy. It will behave as if PERMISSIVE was specified.
  EXPECT_CALL(*connection_context_, principalDomain(true))
      .WillOnce(Return(absl::nullopt));
  EXPECT_CALL(*connection_context_, isMutualTls()).WillOnce(Return(true));
  EXPECT_TRUE(authenticator_->validateX509(&result_payload_,
                                           peer_authentication_policy_.mtls()));
}

TEST_F(ValidateX509Test, DisabledMutualTls) {
  setMtlsMode(disable);
  initialize();
  EXPECT_TRUE(authenticator_->validateX509(&result_payload_,
                                           peer_authentication_policy_.mtls()));
}

TEST_F(ValidateX509Test, NoUserStrictMutualTls) {
  setMtlsMode(strict);
  initialize();

  EXPECT_CALL(*connection_context_, principalDomain(true))
      .WillOnce(Return(absl::nullopt));
  EXPECT_CALL(*connection_context_, isMutualTls()).WillOnce(Return(true));
  EXPECT_FALSE(authenticator_->validateX509(
      &result_payload_, peer_authentication_policy_.mtls()));
}

TEST_F(ValidateX509Test, MutualTlsWithPeerUser) {
  setMtlsMode(strict);
  initialize();

  EXPECT_CALL(*connection_context_, principalDomain(true))
      .WillOnce(Return("istio.io"));
  EXPECT_CALL(*connection_context_, isMutualTls()).WillOnce(Return(true));

  // Has same trust domain between peer and local.
  EXPECT_CALL(*connection_context_, trustDomain(true))
      .WillOnce(Return("istio2.io"));
  EXPECT_CALL(*connection_context_, trustDomain(false))
      .WillOnce(Return("istio2.io"));

  EXPECT_TRUE(authenticator_->validateX509(&result_payload_,
                                           peer_authentication_policy_.mtls()));
  EXPECT_EQ("istio.io", result_payload_.user());

  // Permissive mode with peer user
  setMtlsMode(permissive);
  initialize();

  EXPECT_CALL(*connection_context_, principalDomain(true))
      .WillOnce(Return("istio.io"));
  EXPECT_CALL(*connection_context_, isMutualTls()).WillOnce(Return(true));

  // Has different trust domain between peer and local.
  EXPECT_CALL(*connection_context_, trustDomain(true))
      .WillOnce(Return("istio2.io"));
  EXPECT_CALL(*connection_context_, trustDomain(false))
      .WillOnce(Return("istio3.io"));

  EXPECT_FALSE(authenticator_->validateX509(
      &result_payload_, peer_authentication_policy_.mtls()));
  EXPECT_EQ("istio.io", result_payload_.user());
}

TEST_F(ValidateX509Test, NoUserPermissiveMutualTls) {
  setMtlsMode(permissive);
  initialize();

  EXPECT_CALL(*connection_context_, principalDomain(true))
      .WillOnce(Return(absl::nullopt));
  EXPECT_CALL(*connection_context_, isMutualTls()).WillOnce(Return(true));
  EXPECT_TRUE(authenticator_->validateX509(&result_payload_,
                                           peer_authentication_policy_.mtls()));
}

class MockPeerAuthenticator : public PeerAuthenticatorImpl {
 public:
  MockPeerAuthenticator(
      FilterContextPtr filter_context,
      const istio::security::v1beta1::PeerAuthentication& policy)
      : PeerAuthenticatorImpl(filter_context, policy) {}

  MOCK_METHOD(bool, validateX509,
              (istio::authn::X509Payload * payload,
               const istio::security::v1beta1::PeerAuthentication::MutualTLS&
                   mtls_policy));
};

class PeerAuthenticatorTest : public testing::Test {
 public:
  void initialize() {
    filter_context_.reset();
    filter_context_ = std::make_unique<FilterContext>(
        envoy::config::core::v3::Metadata::default_instance(),
        Envoy::Http::TestRequestHeaderMapImpl(), connection_context_,
        istio::envoy::config::filter::http::authn::v2alpha2::FilterConfig::
            default_instance());

    authenticator_.reset();
    authenticator_ = std::make_unique<MockPeerAuthenticator>(
        filter_context_, peer_authentication_policy_);
  }

  void setMtlsMode(
      istio::security::v1beta1::PeerAuthentication::MutualTLS::Mode mode) {
    peer_authentication_policy_.mutable_mtls()->set_mode(mode);
  }

  void setPortLevalMtls(
      uint32_t port,
      istio::security::v1beta1::PeerAuthentication::MutualTLS::Mode mode) {
    istio::security::v1beta1::PeerAuthentication::MutualTLS mtls_config;
    mtls_config.set_mode(mode);
    (*peer_authentication_policy_.mutable_port_level_mtls())[port] =
        mtls_config;
  }

 protected:
  std::unique_ptr<MockPeerAuthenticator> authenticator_;
  istio::authn::Payload result_payload_;

  FilterContextPtr filter_context_;
  istio::security::v1beta1::PeerAuthentication peer_authentication_policy_;
  std::shared_ptr<MockConnectionContext> connection_context_{
      std::make_shared<MockConnectionContext>()};
};

TEST_F(PeerAuthenticatorTest, EmptyPolicy) {
  initialize();
  EXPECT_CALL(*connection_context_, port()).WillOnce(Return(5000));
  EXPECT_CALL(*authenticator_, validateX509(_, _)).WillOnce(Return(false));

  EXPECT_FALSE(authenticator_->run(&result_payload_));
}

TEST_F(PeerAuthenticatorTest, NoPortLevelPolicy) {
  initialize();

  EXPECT_CALL(*connection_context_, port()).WillOnce(Return(5000));
  EXPECT_CALL(*authenticator_, validateX509(result_payload_.mutable_x509(), _))
      .WillOnce(Invoke(
          [](istio::authn::X509Payload* payload,
             const istio::security::v1beta1::PeerAuthentication::MutualTLS&) {
            payload->set_user("foo");
            return true;
          }));

  EXPECT_TRUE(authenticator_->run(&result_payload_));
  EXPECT_EQ("foo", filter_context_->authenticationResult().peer_user());
}

MATCHER_P(MtlsPolicyEq, rhs, "") { return arg.mode() == rhs.mode(); }

TEST_F(PeerAuthenticatorTest, BasicPortLevelPolicyTest) {
  setPortLevalMtls(5000, strict);
  initialize();

  EXPECT_CALL(*connection_context_, port()).WillOnce(Return(5000));
  EXPECT_CALL(
      *authenticator_,
      validateX509(
          result_payload_.mutable_x509(),
          MtlsPolicyEq(peer_authentication_policy_.port_level_mtls().at(5000))))
      .WillOnce(Invoke(
          [](istio::authn::X509Payload* payload,
             const istio::security::v1beta1::PeerAuthentication::MutualTLS&) {
            payload->set_user("foo");
            return true;
          }));

  EXPECT_TRUE(authenticator_->run(&result_payload_));
  EXPECT_EQ("foo", filter_context_->authenticationResult().peer_user());
}

TEST_F(PeerAuthenticatorTest, PortLevelPeerAuthenticationFailed) {
  setPortLevalMtls(5000, strict);
  initialize();

  EXPECT_CALL(*connection_context_, port()).WillOnce(Return(5000));
  EXPECT_CALL(
      *authenticator_,
      validateX509(
          result_payload_.mutable_x509(),
          MtlsPolicyEq(peer_authentication_policy_.port_level_mtls().at(5000))))
      .WillOnce(Return(false));

  EXPECT_FALSE(authenticator_->run(&result_payload_));
}

TEST_F(PeerAuthenticatorTest, PortLevelPeerAuthenticationNotFound) {
  setPortLevalMtls(8000, strict);
  initialize();

  EXPECT_CALL(*connection_context_, port()).WillOnce(Return(5000));
  EXPECT_CALL(*authenticator_,
              validateX509(result_payload_.mutable_x509(),
                           MtlsPolicyEq(peer_authentication_policy_.mtls())))
      .WillOnce(Invoke(
          [](istio::authn::X509Payload* payload,
             const istio::security::v1beta1::PeerAuthentication::MutualTLS&) {
            payload->set_user("foo");
            return true;
          }));

  EXPECT_TRUE(authenticator_->run(&result_payload_));
  EXPECT_EQ("foo", filter_context_->authenticationResult().peer_user());
}

}  // namespace
}  // namespace AuthN
}  // namespace Extensions