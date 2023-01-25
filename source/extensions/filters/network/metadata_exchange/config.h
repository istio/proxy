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

#include "envoy/server/filter_config.h"
#include "source/extensions/filters/network/metadata_exchange/config/metadata_exchange.pb.h"

namespace Envoy {
namespace Tcp {
namespace MetadataExchange {

/**
 * Config registration for the MetadataExchange filter. @see
 *  NamedNetworkFilterConfigFactory.
 */
class MetadataExchangeConfigFactory
    : public Server::Configuration::NamedNetworkFilterConfigFactory {
public:
  Network::FilterFactoryCb
  createFilterFactoryFromProto(const Protobuf::Message&,
                               Server::Configuration::FactoryContext&) override;

  ProtobufTypes::MessagePtr createEmptyConfigProto() override;

  std::string name() const override { return "envoy.filters.network.metadata_exchange"; }

private:
  Network::FilterFactoryCb
  createFilterFactory(const envoy::tcp::metadataexchange::config::MetadataExchange& proto_config,
                      Server::Configuration::FactoryContext& context);
};

/**
 * Config registration for the MetadataExchange Upstream filter. @see
 *  NamedUpstreamNetworkFilterConfigFactory.
 */
class MetadataExchangeUpstreamConfigFactory
    : public Server::Configuration::NamedUpstreamNetworkFilterConfigFactory {
public:
  Network::FilterFactoryCb
  createFilterFactoryFromProto(const Protobuf::Message&,
                               Server::Configuration::CommonFactoryContext&) override;

  ProtobufTypes::MessagePtr createEmptyConfigProto() override;

  std::string name() const override { return "envoy.filters.network.upstream.metadata_exchange"; }

private:
  Network::FilterFactoryCb
  createFilterFactory(const envoy::tcp::metadataexchange::config::MetadataExchange& proto_config,
                      Server::Configuration::CommonFactoryContext& context);
};

} // namespace MetadataExchange
} // namespace Tcp
} // namespace Envoy
