/* Copyright 2019 Istio Authors. All Rights Reserved.
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

#include "src/envoy/tcp/alpn_proxy/config.h"
#include "envoy/network/connection.h"
#include "envoy/registry/registry.h"
#include "src/envoy/tcp/alpn_proxy/alpn_proxy.h"
#include "src/envoy/utils/config.h"

namespace Envoy {
namespace Tcp {
namespace AlpnProxy {
namespace {

static constexpr char StatPrefix[] = "alpn_proxy.";

Network::FilterFactoryCb createFilterFactoryHelper(
    const envoy::tcp::alpnproxy::config::AlpnProxy& proto_config,
    Server::Configuration::CommonFactoryContext& context,
    FilterDirection filter_direction) {
  ASSERT(!proto_config.protocol().empty());

  AlpnProxyConfigSharedPtr filter_config(std::make_shared<AlpnProxyConfig>(
      StatPrefix, proto_config.protocol(), proto_config.node_metadata_id(),
      filter_direction, context.scope()));
  return [filter_config,
          &context](Network::FilterManager& filter_manager) -> void {
    filter_manager.addFilter(
        std::make_shared<AlpnProxyFilter>(filter_config, context.localInfo(),
                                          context.messageValidationVisitor()));
  };
}
}  // namespace

Network::FilterFactoryCb AlpnProxyConfigFactory::createFilterFactoryFromProto(
    const Protobuf::Message& config,
    Server::Configuration::FactoryContext& context) {
  return createFilterFactory(
      dynamic_cast<const envoy::tcp::alpnproxy::config::AlpnProxy&>(config),
      context);
}

ProtobufTypes::MessagePtr AlpnProxyConfigFactory::createEmptyConfigProto() {
  return ProtobufTypes::MessagePtr{
      new envoy::tcp::alpnproxy::config::AlpnProxy};
}

Network::FilterFactoryCb AlpnProxyConfigFactory::createFilterFactory(
    const envoy::tcp::alpnproxy::config::AlpnProxy& proto_config,
    Server::Configuration::FactoryContext& context) {
  return createFilterFactoryHelper(proto_config, context,
                                   FilterDirection::Downstream);
}

Network::FilterFactoryCb
AlpnProxyUpstreamConfigFactory::createFilterFactoryFromProto(
    const Protobuf::Message& config,
    Server::Configuration::CommonFactoryContext& context) {
  return createFilterFactory(
      dynamic_cast<const envoy::tcp::alpnproxy::config::AlpnProxy&>(config),
      context);
}

ProtobufTypes::MessagePtr
AlpnProxyUpstreamConfigFactory::createEmptyConfigProto() {
  return ProtobufTypes::MessagePtr{
      new envoy::tcp::alpnproxy::config::AlpnProxy};
}

Network::FilterFactoryCb AlpnProxyUpstreamConfigFactory::createFilterFactory(
    const envoy::tcp::alpnproxy::config::AlpnProxy& proto_config,
    Server::Configuration::CommonFactoryContext& context) {
  return createFilterFactoryHelper(proto_config, context,
                                   FilterDirection::Upstream);
}

/**
 * Static registration for the Alpn Proxy Downstream filter. @see
 * RegisterFactory.
 */
static Registry::RegisterFactory<
    AlpnProxyConfigFactory,
    Server::Configuration::NamedNetworkFilterConfigFactory>
    registered_;

/**
 * Static registration for the Alpn Proxy Upstream filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<
    AlpnProxyUpstreamConfigFactory,
    Server::Configuration::NamedUpstreamNetworkFilterConfigFactory>
    registered_upstream_;

}  // namespace AlpnProxy
}  // namespace Tcp
}  // namespace Envoy
