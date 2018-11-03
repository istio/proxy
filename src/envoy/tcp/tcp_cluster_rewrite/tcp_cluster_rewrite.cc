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

#include "src/envoy/tcp/tcp_cluster_rewrite/tcp_cluster_rewrite.h"

#include "envoy/network/connection.h"

#include "common/common/assert.h"
#include "common/tcp_proxy/tcp_proxy.h"

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
  absl::string_view sni = read_callbacks_->connection().requestedServerName();
  ENVOY_CONN_LOG(trace,
                 "tcp_cluster_rewrite: new connection with server name {}",
                 read_callbacks_->connection(), sni);

  if (!sni.empty()) {
    // Rewrite the SNI value prior to setting the tcp_proxy cluster name.
    std::string cluster_name(absl::StrCat(sni));
    if (config_->shouldRewriteCluster()) {
      cluster_name = std::regex_replace(cluster_name, config_->clusterPattern(),
                                        config_->clusterReplacement());
    }
    ENVOY_CONN_LOG(trace, "tcp_cluster_rewrite: tcp proxy cluster name {}",
                   read_callbacks_->connection(), cluster_name);

    // Set the tcp_proxy cluster to the same value as the (rewritten) SNI. The
    // data is mutable to allow other filters to change it.
    read_callbacks_->connection().streamInfo().filterState().setData(
        TcpProxy::PerConnectionCluster::Key,
        std::make_unique<TcpProxy::PerConnectionCluster>(cluster_name),
        StreamInfo::FilterState::StateType::Mutable);
  }

  return Network::FilterStatus::Continue;
}

}  // namespace TcpClusterRewrite
}  // namespace Tcp
}  // namespace Envoy
