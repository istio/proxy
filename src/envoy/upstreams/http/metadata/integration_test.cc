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

#include "envoy/config/bootstrap/v3/bootstrap.pb.h"
#include "envoy/network/address.h"
#include "envoy/router/router.h"
#include "gtest/gtest.h"
#include "source/common/http/codec_client.h"
#include "src/envoy/upstreams/http/metadata/config.h"
#include "test/integration/http_integration.h"
#include "test/test_common/environment.h"
#include "test/test_common/registry.h"
#include "test/test_common/utility.h"

namespace Envoy {
namespace {

class MetadataIntegrationTest
    : public testing::TestWithParam<Network::Address::IpVersion>,
      public HttpIntegrationTest {
 public:
  MetadataIntegrationTest()
      : HttpIntegrationTest(Http::CodecClient::Type::HTTP2, GetParam()) {}

  void initialize() override {
    config_helper_.addConfigModifier(
        [](envoy::config::bootstrap::v3::Bootstrap& bootstrap) {
          auto* cluster =
              bootstrap.mutable_static_resources()->mutable_clusters(0);
          cluster->mutable_upstream_config()->set_name(
              "istio.filters.connection_pools.http.metadata");
          cluster->mutable_upstream_config()->mutable_typed_config();
        });
    HttpIntegrationTest::initialize();
  }

  Upstreams::Http::Metadata::MetadataGenericConnPoolFactory factory_;
};

INSTANTIATE_TEST_SUITE_P(
    IpVersions, MetadataIntegrationTest,
    testing::ValuesIn(TestEnvironment::getIpVersionsForTest()),
    TestUtility::ipTestParamsToString);

TEST_P(MetadataIntegrationTest, Basic) {
  initialize();
  Registry::InjectFactory<Router::GenericConnPoolFactory> registration(
      factory_);

  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = sendRequestAndWaitForResponse(
      default_request_headers_, 0,
      Http::TestResponseHeaderMapImpl{{":status", "200"}}, 0);
  EXPECT_TRUE(upstream_request_->complete());

  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
}

}  // namespace
}  // namespace Envoy
