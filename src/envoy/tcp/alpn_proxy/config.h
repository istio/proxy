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

#pragma once

#include "extensions/filters/network/common/factory_base.h"
#include "src/envoy/tcp/alpn_proxy/config/alpn_proxy.pb.h"

namespace Envoy {
namespace Tcp {
namespace AlpnProxy {

/**
 * Config registration for the Alpn Proxy filter. @see
 *  NamedNetworkFilterConfigFactory.
 */
class AlpnProxyConfigFactory
    : public Server::Configuration::NamedNetworkFilterConfigFactory {
 public:
  Network::FilterFactoryCb createFilterFactory(
      const Json::Object&, Server::Configuration::FactoryContext&) override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }

  Network::FilterFactoryCb createFilterFactoryFromProto(
      const Protobuf::Message&,
      Server::Configuration::FactoryContext&) override;

  ProtobufTypes::MessagePtr createEmptyConfigProto() override;

  std::string name() override { return "envoy.filters.network.alpn_proxy"; }

 private:
  Network::FilterFactoryCb createFilterFactory(
      const envoy::tcp::alpnproxy::config::AlpnProxy& proto_config,
      Server::Configuration::FactoryContext& context);
};

/**
 * Config registration for the Alpn Proxy Upstream filter. @see
 *  NamedUpstreamNetworkFilterConfigFactory.
 */
class AlpnProxyUpstreamConfigFactory
    : public Server::Configuration::NamedUpstreamNetworkFilterConfigFactory {
 public:
  Network::FilterFactoryCb createFilterFactoryFromProto(
      const Protobuf::Message&,
      Server::Configuration::CommonFactoryContext&) override;

  ProtobufTypes::MessagePtr createEmptyConfigProto() override;

  std::string name() override {
    return "envoy.filters.network.upstream.alpn_proxy";
  }

 private:
  Network::FilterFactoryCb createFilterFactory(
      const envoy::tcp::alpnproxy::config::AlpnProxy& proto_config,
      Server::Configuration::CommonFactoryContext& context);
};

}  // namespace AlpnProxy
}  // namespace Tcp
}  // namespace Envoy
