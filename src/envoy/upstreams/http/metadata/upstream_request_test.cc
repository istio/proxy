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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/common/http/common.h"
#include "test/mocks/http/stream_encoder.h"
#include "test/mocks/router/mocks.h"
#include "test/test_common/utility.h"

namespace Envoy {
namespace Upstreams {
namespace Http {
namespace Metadata {

class MetadataUpstreamTest : public ::testing::Test {
 protected:
  Router::MockUpstreamToDownstream upstream_to_downstream_;
  ::testing::NiceMock<Envoy::Http::MockRequestEncoder> encoder_;
};

TEST_F(MetadataUpstreamTest, Basic) {
  Envoy::Http::TestRequestHeaderMapImpl headers;
  HttpTestUtility::addDefaultHeaders(headers);
  auto upstream =
      std::make_unique<MetadataUpstream>(upstream_to_downstream_, &encoder_);
  EXPECT_TRUE(upstream->encodeHeaders(headers, false).ok());
}

}  // namespace Metadata
}  // namespace Http
}  // namespace Upstreams
}  // namespace Envoy
