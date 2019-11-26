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

#include "src/envoy/http/authnv2/http_filter.h"

#include "common/common/base64.h"
#include "common/http/header_map_impl.h"
#include "common/stream_info/stream_info_impl.h"
#include "extensions/filters/http/well_known_names.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "src/envoy/http/authnv2/test_utils.h"
#include "src/envoy/utils/authn.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/ssl/mocks.h"
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
static const char kMockJwtPayload[] =
    "{\"iss\":\"https://example.com\","
    "\"sub\":\"test@example.com\",\"exp\":2001001001,"
    "\"aud\":\"example_service\"}";

class AuthenticationFilterTest : public testing::Test {
 public:
  AuthenticationFilterTest()
      : request_headers_{{":method", "GET"}, {":path", "/"}},
        test_time_(),
        stream_info_(Http::Protocol::Http2, test_time_.timeSystem()) {
    const std::string jwt_name =
        Extensions::HttpFilters::HttpFilterNames::get().JwtAuthn;
    auto& metadata = stream_info_.dynamicMetadata();
    ProtobufWkt::Struct value;
    Protobuf::util::JsonStringToMessage(kMockJwtPayload, &value);
    (*metadata.mutable_filter_metadata())[jwt_name].MergeFrom(value);
  }

  ~AuthenticationFilterTest() {}

  void SetUp() override {
    filter_.setDecoderFilterCallbacks(decoder_callbacks_);
  }

 protected:
  Http::TestHeaderMapImpl request_headers_;
  AuthenticationFilter filter_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Envoy::Network::MockConnection> connection_{};
  DangerousDeprecatedTestTime test_time_;
  StreamInfo::StreamInfoImpl stream_info_;
};

TEST_F(AuthenticationFilterTest, BasicPeerPrincipal) {
  AuthenticationFilter filter;
  filter.setDecoderFilterCallbacks(decoder_callbacks_);
  auto ssl = std::make_shared<NiceMock<Ssl::MockConnectionInfo>>();
  ON_CALL(*ssl, peerCertificatePresented()).WillByDefault(Return(true));
  ON_CALL(*ssl, uriSanPeerCertificate())
      .WillByDefault(Return(
          std::vector<std::string>{"spiffe://cluster.local/ns/foo/sa/bar"}));
  EXPECT_CALL(Const(connection_), ssl()).WillRepeatedly(Return(ssl));
  EXPECT_CALL(decoder_callbacks_, connection())
      .WillRepeatedly(Return(&connection_));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter.decodeHeaders(request_headers_, true));
}

TEST_F(AuthenticationFilterTest, BasicJwt) {}

TEST_F(AuthenticationFilterTest, MultiJwt) {}

TEST_F(AuthenticationFilterTest, AlwaysContinueState) {}

}  // namespace
}  // namespace AuthN
}  // namespace Istio
}  // namespace Http
}  // namespace Envoy
