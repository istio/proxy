/* Copyright 2019 Istio Authors. All Rights Reserved.
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

#include "source/extensions/filters/http/alpn/config.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "source/extensions/filters/http/alpn/alpn_filter.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/stream_info/mocks.h"
#include "test/test_common/utility.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

using istio::envoy::config::filter::http::alpn::v2alpha1::FilterConfig;

namespace Envoy {
namespace Http {
namespace Alpn {
namespace {

TEST(AlpnFilterConfigTest, OverrideAlpn) {
  const std::string yaml = R"EOF(
    alpn_override:
    - upstream_protocol: HTTP10
      alpn_override: ["foo", "bar"]    
    - upstream_protocol: HTTP11
      alpn_override: ["baz"]
    - upstream_protocol: HTTP2
      alpn_override: ["qux"]
    )EOF";

  FilterConfig proto_config;
  TestUtility::loadFromYaml(yaml, proto_config);
  AlpnConfigFactory factory;
  NiceMock<Server::Configuration::MockFactoryContext> context;
  Http::FilterFactoryCb cb = factory.createFilterFactoryFromProto(proto_config, "stats", context);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  Http::StreamDecoderFilterSharedPtr added_filter;
  EXPECT_CALL(filter_callback, addStreamDecoderFilter(_))
      .WillOnce(Invoke([&added_filter](Http::StreamDecoderFilterSharedPtr filter) {
        added_filter = std::move(filter);
      }));

  cb(filter_callback);
  EXPECT_NE(dynamic_cast<AlpnFilter*>(added_filter.get()), nullptr);
}

} // namespace
} // namespace Alpn
} // namespace Http
} // namespace Envoy
