/* Copyright 2021 Istio Authors. All Rights Reserved.
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

#include "src/envoy/upstreams/tcp_proxy/upstream.h"

#include <memory>

#include "common/http/header_map_impl.h"
#include "common/http/headers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/http/stream_encoder.h"
#include "test/mocks/tcp/mocks.h"

namespace Envoy {
namespace Upstreams {
namespace TcpProxy {
namespace {
using envoy::extensions::filters::network::tcp_proxy::v3::
    TcpProxy_TunnelingConfig;

class MetadataUpstreamRequestEncoderTest : public testing::Test {
 public:
  MetadataUpstreamRequestEncoderTest() {
    EXPECT_CALL(encoder_, getStream()).Times(::testing::AnyNumber());
    config_.set_hostname("default.host.com:443");
  }

  void setupUpstream() {
    upstream_ = std::make_unique<MetadataUpstream>(callbacks_, config_);
  }

  Http::MockRequestEncoder encoder_;
  ::testing::NiceMock<Tcp::ConnectionPool::MockUpstreamCallbacks> callbacks_;
  std::unique_ptr<MetadataUpstream> upstream_;
  TcpProxy_TunnelingConfig config_;
};

TEST_F(MetadataUpstreamRequestEncoderTest, RequestEncoder) {
  setupUpstream();
  std::unique_ptr<Http::RequestHeaderMapImpl> expected_headers;
  expected_headers = Http::createHeaderMap<Http::RequestHeaderMapImpl>({
      {Http::Headers::get().Method, "CONNECT"},
      {Http::Headers::get().Host, config_.hostname()},
      {Http::Headers::get().Path, "/"},
      {Http::Headers::get().Scheme, Http::Headers::get().SchemeValues.Http},
      {Http::Headers::get().Protocol,
       Http::Headers::get().ProtocolValues.Bytestream},
  });

  EXPECT_CALL(encoder_,
              encodeHeaders(HeaderMapEqualRef(expected_headers.get()), false));
  upstream_->setRequestEncoder(encoder_, false);
}

}  // namespace
}  // namespace TcpProxy
}  // namespace Upstreams
}  // namespace Envoy
