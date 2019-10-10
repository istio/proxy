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

#include "common/network/application_protocol.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "src/envoy/http/alpn/alpn_filter.h"
#include "test/mocks/http/mocks.h"

using istio::envoy::config::filter::http::alpn::v2alpha1::FilterConfig;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace Envoy {
namespace Http {
namespace Alpn {
namespace {

class AlpnFilterTest : public testing::Test {
 public:
  std::unique_ptr<AlpnFilter> makeDefaultFilter() {
    auto default_config = std::make_shared<AlpnFilterConfig>();
    auto filter = std::make_unique<AlpnFilter>(default_config);
    filter->setDecoderFilterCallbacks(callbacks_);
    return filter;
  }

  std::unique_ptr<AlpnFilter> makeAlpnOverrideFilter(
      const std::vector<std::string> &alpn) {
    FilterConfig proto_config;
    for (const auto &protocol : alpn) {
      proto_config.add_alpn_override(protocol);
    }
    auto config = std::make_shared<AlpnFilterConfig>(proto_config);
    auto filter = std::make_unique<AlpnFilter>(config);
    filter->setDecoderFilterCallbacks(callbacks_);
    return filter;
  }

 protected:
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks_;
  Http::TestHeaderMapImpl headers_;
};

TEST_F(AlpnFilterTest, NoAlpnOverride) {
  NiceMock<StreamInfo::MockStreamInfo> stream_info;
  ON_CALL(callbacks_, streamInfo()).WillByDefault(ReturnRef(stream_info));
  auto filter = makeDefaultFilter();
  EXPECT_CALL(stream_info, filterState()).Times(0);
  EXPECT_EQ(filter->decodeHeaders(headers_, false),
            Http::FilterHeadersStatus::Continue);
}

TEST_F(AlpnFilterTest, OverrideAlpn) {
  NiceMock<StreamInfo::MockStreamInfo> stream_info;
  ON_CALL(callbacks_, streamInfo()).WillByDefault(ReturnRef(stream_info));
  std::vector<std::string> alpn{"foo", "bar", "baz"};
  auto filter = makeAlpnOverrideFilter(alpn);
  Envoy::StreamInfo::FilterStateImpl filter_state;
  EXPECT_CALL(stream_info, filterState()).WillOnce(ReturnRef(filter_state));
  EXPECT_EQ(filter->decodeHeaders(headers_, false),
            Http::FilterHeadersStatus::Continue);
  EXPECT_TRUE(filter_state.hasData<Network::ApplicationProtocols>(
      Network::ApplicationProtocols::key()));
  auto alpn_override = filter_state
                           .getDataReadOnly<Network::ApplicationProtocols>(
                               Network::ApplicationProtocols::key())
                           .value();
  EXPECT_EQ(alpn_override, alpn);
}

}  // namespace
}  // namespace Alpn
}  // namespace Http
}  // namespace Envoy
