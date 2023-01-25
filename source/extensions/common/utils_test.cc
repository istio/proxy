/* Copyright 2017 Istio Authors. All Rights Reserved.
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

#include "source/extensions/common/utils.h"

#include "gmock/gmock.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/ssl/mocks.h"
#include "test/mocks/stream_info/mocks.h"
#include "test/test_common/utility.h"

namespace {

using Envoy::Utils::ParseJsonMessage;
using testing::NiceMock;
using testing::Return;

class UtilsTest : public testing::TestWithParam<bool> {
public:
  void testGetPrincipal(const std::vector<std::string>& sans, const std::string& want,
                        bool success) {
    setMockSan(sans);
    std::string actual;
    if (success) {
      EXPECT_TRUE(Envoy::Utils::GetPrincipal(&connection_, peer_, &actual));
    } else {
      EXPECT_FALSE(Envoy::Utils::GetPrincipal(&connection_, peer_, &actual));
    }
    EXPECT_EQ(actual, want);
  }

  void testGetTrustDomain(const std::vector<std::string>& sans, const std::string& want,
                          bool success) {
    setMockSan(sans);
    std::string actual;
    if (success) {
      EXPECT_TRUE(Envoy::Utils::GetTrustDomain(&connection_, peer_, &actual));
    } else {
      EXPECT_FALSE(Envoy::Utils::GetTrustDomain(&connection_, peer_, &actual));
    }
    EXPECT_EQ(actual, want);
  }

  void SetUp() override { peer_ = GetParam(); }

protected:
  NiceMock<Envoy::Network::MockConnection> connection_{};
  bool peer_;

  void setMockSan(const std::vector<std::string>& sans) {
    auto ssl = std::make_shared<NiceMock<Envoy::Ssl::MockConnectionInfo>>();
    EXPECT_CALL(Const(connection_), ssl()).WillRepeatedly(Return(ssl));
    if (peer_) {
      ON_CALL(*ssl, uriSanPeerCertificate()).WillByDefault(Return(sans));
    } else {
      ON_CALL(*ssl, uriSanLocalCertificate()).WillByDefault(Return(sans));
    }
  }
};

TEST_P(UtilsTest, GetPrincipal) {
  std::vector<std::string> sans{"spiffe://foo/bar", "bad"};
  testGetPrincipal(sans, "foo/bar", true);
}

TEST_P(UtilsTest, GetPrincipalNoSpiffePrefix) {
  std::vector<std::string> sans{"spiffe:foo/bar", "bad"};
  testGetPrincipal(sans, "spiffe:foo/bar", true);
}

TEST_P(UtilsTest, GetPrincipalFromSpiffePrefixSAN) {
  std::vector<std::string> sans{"bad", "spiffe://foo/bar"};
  testGetPrincipal(sans, "foo/bar", true);
}

TEST_P(UtilsTest, GetPrincipalFromNonSpiffePrefixSAN) {
  std::vector<std::string> sans{"foobar", "xyz"};
  testGetPrincipal(sans, "foobar", true);
}

TEST_P(UtilsTest, GetPrincipalEmpty) {
  std::vector<std::string> sans;
  testGetPrincipal(sans, "", false);
}

TEST_P(UtilsTest, GetTrustDomain) {
  std::vector<std::string> sans{"spiffe://td/bar", "bad"};
  testGetTrustDomain(sans, "td", true);
}

TEST_P(UtilsTest, GetTrustDomainEmpty) {
  std::vector<std::string> sans;
  testGetTrustDomain(sans, "", false);
}

TEST_P(UtilsTest, GetTrustDomainNoSpiffePrefix) {
  std::vector<std::string> sans{"spiffe:td/bar", "bad"};
  testGetTrustDomain(sans, "", false);
}

TEST_P(UtilsTest, GetTrustDomainFromSpiffePrefixSAN) {
  std::vector<std::string> sans{"bad", "spiffe://td/bar", "xyz"};
  testGetTrustDomain(sans, "td", true);
}

TEST_P(UtilsTest, GetTrustDomainFromNonSpiffePrefixSAN) {
  std::vector<std::string> sans{"tdbar", "xyz"};
  testGetTrustDomain(sans, "", false);
}

TEST_P(UtilsTest, GetTrustDomainNoSlash) {
  std::vector<std::string> sans{"spiffe://td", "bad"};
  testGetTrustDomain(sans, "", false);
}

INSTANTIATE_TEST_SUITE_P(UtilsTestPrincipalAndTrustDomain, UtilsTest, testing::Values(true, false),
                         [](const testing::TestParamInfo<UtilsTest::ParamType>& info) {
                           return info.param ? "peer" : "local";
                         });

class NamespaceTest : public ::testing::Test {
protected:
  void checkFalse(const std::string& principal) {
    auto out = Envoy::Utils::GetNamespace(principal);
    EXPECT_FALSE(out.has_value());
  }

  void checkTrue(const std::string& principal, absl::string_view ns) {
    auto out = Envoy::Utils::GetNamespace(principal);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(ns, out.value());
  }
};

TEST_F(NamespaceTest, TestGetNamespace) {
  checkFalse("");
  checkFalse("cluster.local");
  checkFalse("cluster.local/");
  checkFalse("cluster.local/ns");
  checkFalse("cluster.local/sa/user");
  checkFalse("cluster.local/sa/user/ns");
  checkFalse("cluster.local/sa/user_ns/");
  checkFalse("cluster.local/sa/user_ns/abc/xyz");
  checkFalse("cluster.local/NS/abc");

  checkTrue("cluster.local/ns/", "");
  checkTrue("cluster.local/ns//", "");
  checkTrue("cluster.local/sa/user/ns/", "");
  checkTrue("cluster.local/ns//sa/user", "");
  checkTrue("cluster.local/ns//ns/ns", "");

  checkTrue("cluster.local/ns/ns/ns/ns", "ns");
  checkTrue("cluster.local/ns/abc_ns", "abc_ns");
  checkTrue("cluster.local/ns/abc_ns/", "abc_ns");
  checkTrue("cluster.local/ns/abc_ns/sa/user_ns", "abc_ns");
  checkTrue("cluster.local/ns/abc_ns/sa/user_ns/other/xyz", "abc_ns");
  checkTrue("cluster.local/sa/user_ns/ns/abc", "abc");
  checkTrue("cluster.local/sa/user_ns/ns/abc/", "abc");
  checkTrue("cluster.local/sa/user_ns/ns/abc_ns", "abc_ns");
  checkTrue("cluster.local/sa/user_ns/ns/abc_ns/", "abc_ns");
}

} // namespace
