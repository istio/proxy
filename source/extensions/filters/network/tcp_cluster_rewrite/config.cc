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

#include "source/extensions/filters/network/tcp_cluster_rewrite/config.h"

#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"
#include "source/extensions/filters/network/tcp_cluster_rewrite/tcp_cluster_rewrite.h"

using namespace ::istio::envoy::config::filter::network::tcp_cluster_rewrite;

namespace Envoy {
namespace Tcp {
namespace TcpClusterRewrite {

Network::FilterFactoryCb TcpClusterRewriteFilterConfigFactory::createFilterFactoryFromProto(
    const Protobuf::Message& config, Server::Configuration::FactoryContext&) {
  return createFilterFactory(dynamic_cast<const v2alpha1::TcpClusterRewrite&>(config));
}

ProtobufTypes::MessagePtr TcpClusterRewriteFilterConfigFactory::createEmptyConfigProto() {
  return ProtobufTypes::MessagePtr{new v2alpha1::TcpClusterRewrite};
}

Network::FilterFactoryCb TcpClusterRewriteFilterConfigFactory::createFilterFactory(
    const v2alpha1::TcpClusterRewrite& config_pb) {
  TcpClusterRewriteFilterConfigSharedPtr config(
      std::make_shared<TcpClusterRewriteFilterConfig>(config_pb));
  return [config](Network::FilterManager& filter_manager) -> void {
    filter_manager.addReadFilter(std::make_shared<TcpClusterRewriteFilter>(config));
  };
}

/**
 * Static registration for the TCP cluster rewrite filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<TcpClusterRewriteFilterConfigFactory,
                                 Server::Configuration::NamedNetworkFilterConfigFactory>
    registered_;

} // namespace TcpClusterRewrite
} // namespace Tcp
} // namespace Envoy
