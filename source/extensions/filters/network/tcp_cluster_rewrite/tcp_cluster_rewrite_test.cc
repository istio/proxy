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

#include "source/extensions/filters/network/tcp_cluster_rewrite/tcp_cluster_rewrite.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "source/common/tcp_proxy/tcp_proxy.h"
#include "source/extensions/filters/network/tcp_cluster_rewrite/config.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/stream_info/mocks.h"

using namespace ::istio::envoy::config::filter::network::tcp_cluster_rewrite;
using testing::_;
using testing::Matcher;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace Envoy {
namespace Tcp {
namespace TcpClusterRewrite {

class TcpClusterRewriteFilterTest : public testing::Test {
public:
  TcpClusterRewriteFilterTest() {
    ON_CALL(filter_callbacks_.connection_, streamInfo()).WillByDefault(ReturnRef(stream_info_));
    ON_CALL(Const(filter_callbacks_.connection_), streamInfo())
        .WillByDefault(ReturnRef(stream_info_));
    configure(v2alpha1::TcpClusterRewrite());
  }

  void configure(v2alpha1::TcpClusterRewrite proto_config) {
    config_ = std::make_unique<TcpClusterRewriteFilterConfig>(proto_config);
    filter_ = std::make_unique<TcpClusterRewriteFilter>(config_);
    filter_->initializeReadFilterCallbacks(filter_callbacks_);
  }

  NiceMock<Network::MockReadFilterCallbacks> filter_callbacks_;
  NiceMock<StreamInfo::MockStreamInfo> stream_info_;
  TcpClusterRewriteFilterConfigSharedPtr config_;
  std::unique_ptr<TcpClusterRewriteFilter> filter_;
};

TEST_F(TcpClusterRewriteFilterTest, ClusterRewrite) {
  // no rewrite
  {
    stream_info_.filterState()->setData(
        TcpProxy::PerConnectionCluster::key(),
        std::make_unique<TcpProxy::PerConnectionCluster>("hello.ns1.svc.cluster.local"),
        StreamInfo::FilterState::StateType::Mutable, StreamInfo::FilterState::LifeSpan::Connection);
    filter_->onNewConnection();

    EXPECT_TRUE(stream_info_.filterState()->hasData<TcpProxy::PerConnectionCluster>(
        TcpProxy::PerConnectionCluster::key()));

    auto per_connection_cluster =
        stream_info_.filterState()->getDataReadOnly<TcpProxy::PerConnectionCluster>(
            TcpProxy::PerConnectionCluster::key());
    EXPECT_EQ(per_connection_cluster->value(), "hello.ns1.svc.cluster.local");
  }

  // with simple rewrite
  {
    v2alpha1::TcpClusterRewrite proto_config;
    proto_config.set_cluster_pattern("\\.global$");
    proto_config.set_cluster_replacement(".svc.cluster.local");
    configure(proto_config);

    stream_info_.filterState()->setData(
        TcpProxy::PerConnectionCluster::key(),
        std::make_unique<TcpProxy::PerConnectionCluster>("hello.ns1.global"),
        StreamInfo::FilterState::StateType::Mutable, StreamInfo::FilterState::LifeSpan::Connection);
    filter_->onNewConnection();

    EXPECT_TRUE(stream_info_.filterState()->hasData<TcpProxy::PerConnectionCluster>(
        TcpProxy::PerConnectionCluster::key()));

    auto per_connection_cluster =
        stream_info_.filterState()->getDataReadOnly<TcpProxy::PerConnectionCluster>(
            TcpProxy::PerConnectionCluster::key());
    EXPECT_EQ(per_connection_cluster->value(), "hello.ns1.svc.cluster.local");
  }

  // with regex rewrite
  {
    v2alpha1::TcpClusterRewrite proto_config;
    proto_config.set_cluster_pattern("^.*$");
    proto_config.set_cluster_replacement("another.svc.cluster.local");
    configure(proto_config);

    stream_info_.filterState()->setData(
        TcpProxy::PerConnectionCluster::key(),
        std::make_unique<TcpProxy::PerConnectionCluster>("hello.ns1.global"),
        StreamInfo::FilterState::StateType::Mutable, StreamInfo::FilterState::LifeSpan::Connection);
    filter_->onNewConnection();

    EXPECT_TRUE(stream_info_.filterState()->hasData<TcpProxy::PerConnectionCluster>(
        TcpProxy::PerConnectionCluster::key()));

    auto per_connection_cluster =
        stream_info_.filterState()->getDataReadOnly<TcpProxy::PerConnectionCluster>(
            TcpProxy::PerConnectionCluster::key());
    EXPECT_EQ(per_connection_cluster->value(), "another.svc.cluster.local");
  }
}

} // namespace TcpClusterRewrite
} // namespace Tcp
} // namespace Envoy
