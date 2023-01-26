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

#include "envoy/config/filter/network/tcp_cluster_rewrite/v2alpha1/config.pb.h"
#include "envoy/network/connection.h"
#include "envoy/network/filter.h"
#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"

using namespace ::istio::envoy::config::filter::network::tcp_cluster_rewrite;

namespace Envoy {
namespace Tcp {
namespace TcpClusterRewrite {

/**
 * Config registration for the TCP cluster rewrite filter. @see
 * NamedNetworkFilterConfigFactory.
 */
class TcpClusterRewriteFilterConfigFactory
    : public Server::Configuration::NamedNetworkFilterConfigFactory {
public:
  Network::FilterFactoryCb
  createFilterFactoryFromProto(const Protobuf::Message&,
                               Server::Configuration::FactoryContext&) override;

  ProtobufTypes::MessagePtr createEmptyConfigProto() override;

  std::string name() const override { return "envoy.filters.network.tcp_cluster_rewrite"; }

private:
  Network::FilterFactoryCb createFilterFactory(const v2alpha1::TcpClusterRewrite& config_pb);
};

} // namespace TcpClusterRewrite
} // namespace Tcp
} // namespace Envoy
