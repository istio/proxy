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

#include "envoy/network/connection.h"
#include "source/common/common/assert.h"
#include "source/common/tcp_proxy/tcp_proxy.h"

using namespace ::istio::envoy::config::filter::network::tcp_cluster_rewrite;

namespace Envoy {
namespace Tcp {
namespace TcpClusterRewrite {

TcpClusterRewriteFilterConfig::TcpClusterRewriteFilterConfig(
    const v2alpha1::TcpClusterRewrite& proto_config) {
  if (!proto_config.cluster_pattern().empty()) {
    should_rewrite_cluster_ = true;
    cluster_pattern_ = std::regex(proto_config.cluster_pattern());
    cluster_replacement_ = proto_config.cluster_replacement();
  } else {
    should_rewrite_cluster_ = false;
  }
}

Network::FilterStatus TcpClusterRewriteFilter::onNewConnection() {
  if (config_->shouldRewriteCluster() &&
      read_callbacks_->connection()
          .streamInfo()
          .filterState()
          ->hasData<TcpProxy::PerConnectionCluster>(TcpProxy::PerConnectionCluster::key())) {
    absl::string_view cluster_name =
        read_callbacks_->connection()
            .streamInfo()
            .filterState()
            ->getDataReadOnly<TcpProxy::PerConnectionCluster>(TcpProxy::PerConnectionCluster::key())
            ->value();
    ENVOY_CONN_LOG(trace, "tcp_cluster_rewrite: new connection with server name {}",
                   read_callbacks_->connection(), cluster_name);

    // Rewrite the cluster name prior to setting the tcp_proxy cluster name.
    std::string final_cluster_name(absl::StrCat(cluster_name));
    final_cluster_name = std::regex_replace(final_cluster_name, config_->clusterPattern(),
                                            config_->clusterReplacement());
    ENVOY_CONN_LOG(trace, "tcp_cluster_rewrite: final tcp proxy cluster name {}",
                   read_callbacks_->connection(), final_cluster_name);

    try {
      // The data is mutable to allow other filters to change it.
      read_callbacks_->connection().streamInfo().filterState()->setData(
          TcpProxy::PerConnectionCluster::key(),
          std::make_unique<TcpProxy::PerConnectionCluster>(final_cluster_name),
          StreamInfo::FilterState::StateType::Mutable,
          StreamInfo::FilterState::LifeSpan::Connection);
    } catch (const EnvoyException& e) {
      ENVOY_CONN_LOG(critical, "tcp_cluster_rewrite: error setting data: {}",
                     read_callbacks_->connection(), e.what());
      throw;
    } catch (...) {
      ENVOY_LOG(critical, "tcp_cluster_rewrite: error setting data due to unknown exception");
      throw;
    }
  }

  return Network::FilterStatus::Continue;
}

} // namespace TcpClusterRewrite
} // namespace Tcp
} // namespace Envoy
