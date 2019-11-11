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

#include "src/envoy/utils/utils.h"

#include "gmock/gmock.h"
#include "mixer/v1/config/client/client_config.pb.h"
#include "src/istio/mixerclient/check_context.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/ssl/mocks.h"
#include "test/mocks/stream_info/mocks.h"
#include "test/test_common/utility.h"

namespace {

using Envoy::Utils::CheckResponseInfoToStreamInfo;
using Envoy::Utils::ParseJsonMessage;
using testing::NiceMock;
using testing::Return;

class UtilsTest : public testing::TestWithParam<bool> {
 public:
  void testGetPrincipal(const std::vector<std::string>& sans,
                        const std::string& want, bool success) {
    setMockSan(sans);
    std::string actual;
    if (success) {
      EXPECT_TRUE(Envoy::Utils::GetPrincipal(&connection_, peer_, &actual));
    } else {
      EXPECT_FALSE(Envoy::Utils::GetPrincipal(&connection_, peer_, &actual));
    }
    EXPECT_EQ(actual, want);
  }

  void testGetTrustDomain(const std::vector<std::string>& sans,
                          const std::string& want, bool success) {
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

TEST(UtilsTest, ParseNormalMessage) {
  std::string config_str = R"({
        "default_destination_service": "service.svc.cluster.local",
      })";
  ::istio::mixer::v1::config::client::HttpClientConfig http_config;

  auto status = ParseJsonMessage(config_str, &http_config);
  EXPECT_OK(status) << status;
  EXPECT_EQ(http_config.default_destination_service(),
            "service.svc.cluster.local");
}

TEST(UtilsTest, ParseMessageWithUnknownField) {
  std::string config_str = R"({
        "default_destination_service": "service.svc.cluster.local",
        "unknown_field": "xxx",
      })";
  ::istio::mixer::v1::config::client::HttpClientConfig http_config;

  EXPECT_OK(ParseJsonMessage(config_str, &http_config));
  EXPECT_EQ(http_config.default_destination_service(),
            "service.svc.cluster.local");
}

TEST(UtilsTest, CheckResponseInfoToStreamInfo) {
  auto attributes = std::make_shared<::istio::mixerclient::SharedAttributes>();
  ::istio::mixerclient::CheckContext check_response(
      0U, false /* fail_open */, attributes);  // by default status is unknown
  Envoy::StreamInfo::MockStreamInfo mock_stream_info;

  EXPECT_CALL(
      mock_stream_info,
      setResponseFlag(
          Envoy::StreamInfo::ResponseFlag::UnauthorizedExternalService));
  EXPECT_CALL(mock_stream_info, setDynamicMetadata(_, _))
      .WillOnce(Invoke(
          [](const std::string& key, const Envoy::ProtobufWkt::Struct& value) {
            EXPECT_EQ("istio.mixer", key);
            EXPECT_EQ(1, value.fields().count("status"));
            EXPECT_EQ("UNKNOWN", value.fields().at("status").string_value());
          }));

  CheckResponseInfoToStreamInfo(check_response, mock_stream_info);
}

TEST_P(UtilsTest, GetPrincipal) {
  std::vector<std::string> sans{"spiffe://foo/bar", "bad"};
  testGetPrincipal(sans, "foo/bar", true);
}

TEST_P(UtilsTest, GetPrincipalNoSpiffePrefix) {
  std::vector<std::string> sans{"spiffe:foo/bar", "bad"};
  testGetPrincipal(sans, "spiffe:foo/bar", true);
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

TEST_P(UtilsTest, GetTrustDomainNoSlash) {
  std::vector<std::string> sans{"spiffe://td", "bad"};
  testGetTrustDomain(sans, "", false);
}

INSTANTIATE_TEST_SUITE_P(
    UtilsTestPrincipalAndTrustDomain, UtilsTest, testing::Values(true, false),
    [](const testing::TestParamInfo<UtilsTest::ParamType>& info) {
      return info.param ? "peer" : "local";
    });

}  // namespace
