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

#include "src/envoy/http/authnv2/filter.h"

#include "common/common/base64.h"
#include "common/http/header_map_impl.h"
#include "common/stream_info/stream_info_impl.h"
#include "extensions/filters/http/well_known_names.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "src/envoy/http/authnv2/test_utils.h"
#include "src/envoy/utils/authn.h"
#include "src/envoy/utils/filter_names.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/ssl/mocks.h"
#include "test/test_common/simulated_time_system.h"
#include "test/test_common/test_time.h"
#include "test/test_common/utility.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::StrictMock;

namespace iaapi = istio::authentication::v1alpha1;

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {
namespace {

// Payload data to inject. Note the iss claim intentionally set different from
// kJwtIssuer.
static const std::string kMockJwtPayload = R"EOF(
{
    "iss":"https://example.com",
    "sub":"test@example.com",
    "exp":2001001001,
    "aud": "example_service"
}
)EOF";

class AuthenticationFilterTest : public testing::Test {
 public:
  AuthenticationFilterTest()
      : request_headers_{{":method", "GET"}, {":path", "/"}},
        ssl_(std::make_shared<NiceMock<Ssl::MockConnectionInfo>>()),
        test_time_(),
        stream_info_(Http::Protocol::Http2, test_time_.timeSystem()) {}

  void initialize(const std::vector<std::string>& jwts) {
    // Jwt metadata filter input setup.
    const std::string jwt_name =
        Extensions::HttpFilters::HttpFilterNames::get().JwtAuthn;
    auto& metadata = stream_info_.dynamicMetadata();
    for (const auto& jwt : jwts) {
      ProtobufWkt::Value value;
      Protobuf::util::JsonStringToMessage(jwt, value.mutable_struct_value());
      auto* jwt_issuers_map =
          (*metadata.mutable_filter_metadata())[jwt_name].mutable_fields();
      (*jwt_issuers_map)["https://example.com"] = value;
    }

    // Test mock setup.
    EXPECT_CALL(decoder_callbacks_, streamInfo())
        .WillRepeatedly(ReturnRef(stream_info_));
    ON_CALL(*ssl_, peerCertificatePresented()).WillByDefault(Return(true));
    ON_CALL(*ssl_, uriSanPeerCertificate())
        .WillByDefault(Return(
            std::vector<std::string>{"spiffe://cluster.local/ns/foo/sa/bar"}));
    EXPECT_CALL(Const(connection_), ssl()).WillRepeatedly(Return(ssl_));
    EXPECT_CALL(decoder_callbacks_, connection())
        .WillRepeatedly(Return(&connection_));

    filter_.setDecoderFilterCallbacks(decoder_callbacks_);
  }

  // validate the authn filter metadata is as expected as input specified.
  void validate(const std::string& expected_authn_yaml) {
    ProtobufWkt::Struct expected_authn_data;
    TestUtility::loadFromYaml(expected_authn_yaml, expected_authn_data);
    ProtobufWkt::Struct authn_data =
        stream_info_.dynamicMetadata().filter_metadata().at(
            Utils::IstioFilterName::kAuthentication);
    std::string authn_raw_claim =
        (*authn_data.mutable_fields())["request.auth.raw_claims"]
            .string_value();
    authn_data.mutable_fields()->erase("request.auth.raw_claims");
    EXPECT_EQ(authn_data.DebugString(), expected_authn_data.DebugString());
  }

 protected:
  Http::TestHeaderMapImpl request_headers_;
  AuthenticationFilter filter_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Envoy::Network::MockConnection> connection_{};
  std::shared_ptr<NiceMock<Ssl::MockConnectionInfo>> ssl_;
  ::Envoy::Event::SimulatedTimeSystem test_time_;
  StreamInfo::StreamInfoImpl stream_info_;
};

TEST_F(AuthenticationFilterTest, BasicAllAttributes) {
  initialize({kMockJwtPayload});
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_.decodeHeaders(request_headers_, true));
  validate(R"EOF(
request.auth.claims:
  aud:
    - example_service
  sub:
    - test@example.com
  iss:
    - https://example.com
request.auth.principal: https://example.com/test@example.com
source.principal: cluster.local/ns/foo/sa/bar
request.auth.audiences: example_service
)EOF");
}

TEST_F(AuthenticationFilterTest, MultiJwtsSelectByIssuerLexicalOrder) {
  initialize({
      R"EOF({
    "iss":"ab",
    "sub":"test@example.com",
    "aud": "ab-sub"
})EOF",
      R"EOF({
    "iss":"aa",
    "sub":"aa-sub",
})EOF",
  });
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_.decodeHeaders(request_headers_, true));
}

// This test case ensures the filter does not reject the request if the
// attributes extraction fails.
TEST_F(AuthenticationFilterTest, AlwaysContinueState) {}

}  // namespace
}  // namespace AuthN
}  // namespace Istio
}  // namespace Http
}  // namespace Envoy
