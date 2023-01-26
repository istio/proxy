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

#pragma once

#include <regex>

#include "envoy/config/filter/network/tcp_cluster_rewrite/v2alpha1/config.pb.h"
#include "envoy/network/filter.h"
#include "source/common/common/logger.h"

using namespace ::istio::envoy::config::filter::network::tcp_cluster_rewrite;

namespace Envoy {
namespace Tcp {
namespace TcpClusterRewrite {

/**
 * Configuration for the TCP cluster rewrite filter.
 */
class TcpClusterRewriteFilterConfig {
public:
  TcpClusterRewriteFilterConfig(const v2alpha1::TcpClusterRewrite& proto_config);

  bool shouldRewriteCluster() const { return should_rewrite_cluster_; }
  std::regex clusterPattern() const { return cluster_pattern_; }
  std::string clusterReplacement() const { return cluster_replacement_; }

private:
  bool should_rewrite_cluster_;
  std::regex cluster_pattern_;
  std::string cluster_replacement_;
};

typedef std::shared_ptr<TcpClusterRewriteFilterConfig> TcpClusterRewriteFilterConfigSharedPtr;

/**
 * Implementation of the TCP cluster rewrite filter that sets the upstream
 * cluster name from the SNI field in the TLS connection.
 */
class TcpClusterRewriteFilter : public Network::ReadFilter, Logger::Loggable<Logger::Id::filter> {
public:
  TcpClusterRewriteFilter(TcpClusterRewriteFilterConfigSharedPtr config) : config_(config) {}

  // Network::ReadFilter
  Network::FilterStatus onData(Buffer::Instance&, bool) override {
    return Network::FilterStatus::Continue;
  }

  Network::FilterStatus onNewConnection() override;

  void initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) override {
    read_callbacks_ = &callbacks;
  }

private:
  TcpClusterRewriteFilterConfigSharedPtr config_;
  Network::ReadFilterCallbacks* read_callbacks_{};
};

} // namespace TcpClusterRewrite
} // namespace Tcp
} // namespace Envoy
