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

#include "src/envoy/upstreams/http/metadata/upstream_request.h"

#include <memory>

#include "envoy/config/core/v3/base.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/common/http/common.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/http/stream_encoder.h"
#include "test/mocks/router/mocks.h"
#include "test/mocks/upstream/cluster_info.h"
#include "test/mocks/upstream/host.h"
#include "test/test_common/utility.h"

using testing::NiceMock;
using testing::ReturnRef;

namespace Envoy {
namespace Upstreams {
namespace Http {
namespace Metadata {

class MetadataUpstreamTest : public ::testing::Test {
 public:
  MetadataUpstreamTest() {
    cluster_metadata_ = std::make_shared<envoy::config::core::v3::Metadata>(
        TestUtility::parseYaml<envoy::config::core::v3::Metadata>(
            R"EOF(
            filter_metadata:
              istio:
                default_original_port: "8080"
            )EOF"));
  }

 protected:
  Router::MockUpstreamToDownstream upstream_to_downstream_;
  NiceMock<Envoy::Http::MockRequestEncoder> encoder_;
  std::shared_ptr<Upstream::MockHost> host_{new NiceMock<Upstream::MockHost>()};
  std::shared_ptr<Upstream::MockClusterInfo> info_{
      new NiceMock<Upstream::MockClusterInfo>()};
  std::shared_ptr<envoy::config::core::v3::Metadata> cluster_metadata_;
};

TEST_F(MetadataUpstreamTest, Basic) {
  Envoy::Http::TestRequestHeaderMapImpl headers;
  HttpTestUtility::addDefaultHeaders(headers);
  auto upstream = std::make_unique<MetadataUpstream>(upstream_to_downstream_,
                                                     &encoder_, host_);
  EXPECT_TRUE(upstream->encodeHeaders(headers, false).ok());
}

TEST_F(MetadataUpstreamTest, TestAddClusterInfo) {
  ON_CALL(*host_, cluster()).WillByDefault(ReturnRef(*info_));
  ON_CALL(*info_, metadata()).WillByDefault(ReturnRef(*cluster_metadata_));
  auto upstream = std::make_unique<MetadataUpstream>(upstream_to_downstream_,
                                                     &encoder_, host_);
  Envoy::Http::TestRequestHeaderMapImpl headers;
  HttpTestUtility::addDefaultHeaders(headers);
  EXPECT_CALL(
      encoder_,
      encodeHeaders(HeaderHasValueRef("x-istio-original-port", "8080"), false));
  EXPECT_TRUE(upstream->encodeHeaders(headers, false).ok());
}

}  // namespace Metadata
}  // namespace Http
}  // namespace Upstreams
}  // namespace Envoy
