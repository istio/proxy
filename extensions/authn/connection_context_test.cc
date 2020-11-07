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

#include "extensions/authn/connection_context.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/ssl/mocks.h"

using testing::Return;
using testing::ReturnRef;

namespace Extensions {
namespace AuthN {
namespace {

class ConnectionContextTest : public testing::Test {
 public:
  Envoy::Network::MockConnection connection_;
  std::shared_ptr<Envoy::Ssl::MockConnectionInfo> ssl_conn_info_{
      std::make_shared<Envoy::Ssl::MockConnectionInfo>()};
  ConnectionContextImpl conn_context_{&connection_};
};

TEST_F(ConnectionContextTest, IsMutualTlsTest) {
  EXPECT_CALL(connection_, ssl())
      .Times(2)
      .WillRepeatedly(Return(ssl_conn_info_));
  EXPECT_CALL(*ssl_conn_info_, peerCertificatePresented())
      .WillOnce(Return(true));
  EXPECT_TRUE(conn_context_.isMutualTls());
}

TEST_F(ConnectionContextTest, TrustDomainTestWithoutSpiffePrefix) {
  EXPECT_CALL(connection_, ssl()).WillOnce(Return(ssl_conn_info_));
  EXPECT_CALL(*ssl_conn_info_, uriSanPeerCertificate())
      .WillOnce(Return(std::vector<std::string>{"istio.io", "istio2.io"}));
  EXPECT_FALSE(conn_context_.trustDomain(true).has_value());

  EXPECT_CALL(connection_, ssl()).WillOnce(Return(ssl_conn_info_));
  EXPECT_CALL(*ssl_conn_info_, uriSanLocalCertificate())
      .WillOnce(Return(std::vector<std::string>{"istio.io", "istio2.io"}));
  EXPECT_FALSE(conn_context_.trustDomain(false).has_value());
}

TEST_F(ConnectionContextTest, TrustDomainTestWithSpiffePrefix) {
  EXPECT_CALL(connection_, ssl()).WillOnce(Return(ssl_conn_info_));
  EXPECT_CALL(*ssl_conn_info_, uriSanPeerCertificate())
      .WillOnce(
          Return(std::vector<std::string>{"istio.io", "spiffe://istio2.io/"}));
  EXPECT_EQ(conn_context_.trustDomain(true).value(), "istio2.io");

  EXPECT_CALL(connection_, ssl()).WillOnce(Return(ssl_conn_info_));
  EXPECT_CALL(*ssl_conn_info_, uriSanLocalCertificate())
      .WillOnce(
          Return(std::vector<std::string>{"istio.io", "spiffe://istio2.io/"}));
  EXPECT_EQ(conn_context_.trustDomain(false).value(), "istio2.io");
}

TEST_F(ConnectionContextTest, TrustDomainTestWithInvalidSpiffePrefix) {
  EXPECT_CALL(connection_, ssl()).WillOnce(Return(ssl_conn_info_));
  EXPECT_CALL(*ssl_conn_info_, uriSanPeerCertificate())
      .WillOnce(
          Return(std::vector<std::string>{"istio.io", "spiffe:/istio2.io"}));
  EXPECT_FALSE(conn_context_.trustDomain(true).has_value());

  EXPECT_CALL(connection_, ssl()).WillOnce(Return(ssl_conn_info_));
  EXPECT_CALL(*ssl_conn_info_, uriSanLocalCertificate())
      .WillOnce(
          Return(std::vector<std::string>{"istio.io", "spiffe:/istio2.io"}));
  EXPECT_FALSE(conn_context_.trustDomain(false).has_value());
}

TEST_F(ConnectionContextTest, TrustDomainTestWithInvalidSpiffePrefixOnly) {
  EXPECT_CALL(connection_, ssl()).WillOnce(Return(ssl_conn_info_));
  EXPECT_CALL(*ssl_conn_info_, uriSanPeerCertificate())
      .WillOnce(Return(std::vector<std::string>{"spiffe:/istio2.io"}));
  EXPECT_FALSE(conn_context_.trustDomain(true).has_value());

  EXPECT_CALL(connection_, ssl()).WillOnce(Return(ssl_conn_info_));
  EXPECT_CALL(*ssl_conn_info_, uriSanLocalCertificate())
      .WillOnce(Return(std::vector<std::string>{"spiffe:/istio2.io"}));
  EXPECT_FALSE(conn_context_.trustDomain(false).has_value());
}

TEST_F(ConnectionContextTest, PrincipalDomainTestWithoutSpiffePrefix) {
  EXPECT_CALL(connection_, ssl()).WillOnce(Return(ssl_conn_info_));
  EXPECT_CALL(*ssl_conn_info_, uriSanPeerCertificate())
      .WillOnce(Return(std::vector<std::string>{"istio.io", "istio2.io"}));
  EXPECT_EQ(conn_context_.principalDomain(true).value(), "istio.io");

  EXPECT_CALL(connection_, ssl()).WillOnce(Return(ssl_conn_info_));
  EXPECT_CALL(*ssl_conn_info_, uriSanLocalCertificate())
      .WillOnce(Return(std::vector<std::string>{"istio.io", "istio2.io"}));
  EXPECT_EQ(conn_context_.principalDomain(false).value(), "istio.io");
}

TEST_F(ConnectionContextTest, PrincipalDomainTestWithSpiffePrefix) {
  EXPECT_CALL(connection_, ssl()).WillOnce(Return(ssl_conn_info_));
  EXPECT_CALL(*ssl_conn_info_, uriSanPeerCertificate())
      .WillOnce(
          Return(std::vector<std::string>{"istio.io", "spiffe://istio2.io/"}));
  EXPECT_EQ(conn_context_.principalDomain(true).value(), "istio2.io/");

  EXPECT_CALL(connection_, ssl()).WillOnce(Return(ssl_conn_info_));
  EXPECT_CALL(*ssl_conn_info_, uriSanLocalCertificate())
      .WillOnce(
          Return(std::vector<std::string>{"istio.io", "spiffe://istio2.io/"}));
  EXPECT_EQ(conn_context_.principalDomain(false).value(), "istio2.io/");
}

TEST_F(ConnectionContextTest, PrincipalDomainTestWithInvalidSpiffePrefix) {
  EXPECT_CALL(connection_, ssl()).WillOnce(Return(ssl_conn_info_));
  EXPECT_CALL(*ssl_conn_info_, uriSanPeerCertificate())
      .WillOnce(
          Return(std::vector<std::string>{"istio.io", "spiffe:/istio2.io"}));
  EXPECT_EQ(conn_context_.principalDomain(true).value(), "istio.io");

  EXPECT_CALL(connection_, ssl()).WillOnce(Return(ssl_conn_info_));
  EXPECT_CALL(*ssl_conn_info_, uriSanLocalCertificate())
      .WillOnce(
          Return(std::vector<std::string>{"istio.io", "spiffe:/istio2.io"}));
  EXPECT_EQ(conn_context_.principalDomain(false).value(), "istio.io");
}

TEST_F(ConnectionContextTest, PrincipalDomainTestWithInvalidSpiffePrefixOnly) {
  EXPECT_CALL(connection_, ssl()).WillOnce(Return(ssl_conn_info_));
  EXPECT_CALL(*ssl_conn_info_, uriSanPeerCertificate())
      .WillOnce(Return(std::vector<std::string>{"spiffe:/istio2.io"}));
  EXPECT_EQ(conn_context_.principalDomain(true).value(), "spiffe:/istio2.io");

  EXPECT_CALL(connection_, ssl()).WillOnce(Return(ssl_conn_info_));
  EXPECT_CALL(*ssl_conn_info_, uriSanLocalCertificate())
      .WillOnce(Return(std::vector<std::string>{"spiffe:/istio2.io"}));
  EXPECT_EQ(conn_context_.principalDomain(false).value(), "spiffe:/istio2.io");
}

}  // namespace
}  // namespace AuthN
}  // namespace Extensions