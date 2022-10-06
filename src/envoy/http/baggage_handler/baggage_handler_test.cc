// Copyright Istio Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "baggage_handler.h"

#include "extensions/common/metadata_object.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "source/common/http/header_map_impl.h"
#include "source/common/protobuf/protobuf.h"
#include "src/envoy/http/baggage_handler/config/baggage_handler.pb.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/network/mocks.h"
// #include "test/mocks/ssl/mocks.h"
#include "test/mocks/stream_info/mocks.h"
#include "test/test_common/utility.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace Envoy {
namespace Http {
namespace BaggageHandler {

class BaggageHandlerFilterTest : public testing::Test {
 public:
  BaggageHandlerFilterTest() { ENVOY_LOG_MISC(info, "test"); }

 public:
  void initializeFilter() {
    istio::telemetry::baggagehandler::v1::Config config;
    config_ = std::make_shared<Config>(config);
    filter_ = std::make_shared<BaggageHandlerFilter>(config_);

    filter_state_ = std::make_shared<StreamInfo::FilterStateImpl>(
        StreamInfo::FilterState::LifeSpan::Request);

    ON_CALL(decoder_callbacks_.stream_info_, filterState())
        .WillByDefault(::testing::ReturnRef(filter_state_));

    auto connRef =
        OptRef<NiceMock<Envoy::Network::MockConnection>>{connection_};
    ON_CALL(decoder_callbacks_, connection())
        .WillByDefault(
            ::testing::Return(OptRef<const Network::Connection>{connection_}));

    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
  }

  ConfigSharedPtr config_;
  std::shared_ptr<BaggageHandlerFilter> filter_;
  std::shared_ptr<StreamInfo::FilterState> filter_state_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info_;
  NiceMock<Envoy::Network::MockConnection> connection_;
};

TEST_F(BaggageHandlerFilterTest, BasicBaggageTest) {
  initializeFilter();

  auto baggage = absl::StrCat(
      "k8s.namespace.name=default,k8s.deployment.name=foo,service.name=",
      "foo-service,service.version=v1alpha3");
  Http::TestRequestHeaderMapImpl incoming_headers{{"baggage", baggage}};

  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(incoming_headers, false));

  EXPECT_TRUE(
      filter_state_->hasDataWithName(Istio::Common::kSourceMetadataObjectKey));
  auto found =
      filter_state_->getDataReadOnly<Istio::Common::WorkloadMetadataObject>(
          Istio::Common::kSourceMetadataObjectKey);

  EXPECT_EQ(found->canonical_name_, "foo-service");

  filter_->onDestroy();
}

}  // namespace BaggageHandler
}  // namespace Http
}  // namespace Envoy
