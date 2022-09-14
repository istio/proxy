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

#include "internal_ssl_forwarder.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "source/common/protobuf/protobuf.h"
#include "src/envoy/common/metadata_object.h"
#include "src/envoy/internal_ssl_forwarder/config/internal_ssl_forwarder.pb.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/ssl/mocks.h"
#include "test/mocks/stream_info/mocks.h"
#include "test/test_common/utility.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace InternalSslForwarder {

class FilterTest : public testing::Test {
 public:
  FilterTest() { ENVOY_LOG_MISC(info, "internal ssl forwarder test"); }

 public:
  void initializeFilter() {
    istio::telemetry::internal_ssl_forwarder::v1::Config config;
    config_ = std::make_shared<Config>(config);
    filter_ = std::make_shared<Filter>(config_);
    filter_->initializeReadFilterCallbacks(read_filter_callbacks_);
  }

  ConfigSharedPtr config_;
  std::shared_ptr<Filter> filter_;
  NiceMock<Network::MockReadFilterCallbacks> read_filter_callbacks_;
};

TEST_F(FilterTest, BasicSSLPassing) {
  initializeFilter();
  const std::string ver = "v1.2";
  auto baggage = "k8s.namespace.name=default,k8s.deployment.name=foo";
  auto conn_info = std::make_shared<NiceMock<Ssl::MockConnectionInfo>>();
  ON_CALL(*conn_info, tlsVersion()).WillByDefault(testing::ReturnRef(ver));
  auto obj = Common::WorkloadMetadataObject::fromBaggage(baggage, conn_info);

  read_filter_callbacks_.connection_.stream_info_.filter_state_->setData(
      Common::WorkloadMetadataObject::kSourceMetadataObjectKey, obj,
      StreamInfo::FilterState::StateType::ReadOnly,
      StreamInfo::FilterState::LifeSpan::Connection);

  EXPECT_EQ(Network::FilterStatus::Continue, filter_->onNewConnection());

  EXPECT_EQ(read_filter_callbacks_.connection_.stream_info_
                .downstream_connection_info_provider_->sslConnection()
                ->tlsVersion(),
            ver);
}

}  // namespace InternalSslForwarder
}  // namespace NetworkFilters
}  // namespace Extensions
}  // namespace Envoy
