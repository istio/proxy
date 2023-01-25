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

#include "source/extensions/filters/network/metadata_exchange/config.h"

#include "envoy/network/connection.h"
#include "envoy/registry/registry.h"
#include "source/extensions/filters/network/metadata_exchange/metadata_exchange.h"

namespace Envoy {
namespace Tcp {
namespace MetadataExchange {
namespace {

static constexpr char StatPrefix[] = "metadata_exchange.";

Network::FilterFactoryCb createFilterFactoryHelper(
    const envoy::tcp::metadataexchange::config::MetadataExchange& proto_config,
    Server::Configuration::CommonFactoryContext& context, FilterDirection filter_direction) {
  ASSERT(!proto_config.protocol().empty());

  MetadataExchangeConfigSharedPtr filter_config(std::make_shared<MetadataExchangeConfig>(
      StatPrefix, proto_config.protocol(), filter_direction, context.scope()));
  return [filter_config, &context](Network::FilterManager& filter_manager) -> void {
    filter_manager.addFilter(
        std::make_shared<MetadataExchangeFilter>(filter_config, context.localInfo()));
  };
}
} // namespace

Network::FilterFactoryCb MetadataExchangeConfigFactory::createFilterFactoryFromProto(
    const Protobuf::Message& config, Server::Configuration::FactoryContext& context) {
  return createFilterFactory(
      dynamic_cast<const envoy::tcp::metadataexchange::config::MetadataExchange&>(config), context);
}

ProtobufTypes::MessagePtr MetadataExchangeConfigFactory::createEmptyConfigProto() {
  return std::make_unique<envoy::tcp::metadataexchange::config::MetadataExchange>();
}

Network::FilterFactoryCb MetadataExchangeConfigFactory::createFilterFactory(
    const envoy::tcp::metadataexchange::config::MetadataExchange& proto_config,
    Server::Configuration::FactoryContext& context) {
  return createFilterFactoryHelper(proto_config, context, FilterDirection::Downstream);
}

Network::FilterFactoryCb MetadataExchangeUpstreamConfigFactory::createFilterFactoryFromProto(
    const Protobuf::Message& config, Server::Configuration::CommonFactoryContext& context) {
  return createFilterFactory(
      dynamic_cast<const envoy::tcp::metadataexchange::config::MetadataExchange&>(config), context);
}

ProtobufTypes::MessagePtr MetadataExchangeUpstreamConfigFactory::createEmptyConfigProto() {
  return std::make_unique<envoy::tcp::metadataexchange::config::MetadataExchange>();
}

Network::FilterFactoryCb MetadataExchangeUpstreamConfigFactory::createFilterFactory(
    const envoy::tcp::metadataexchange::config::MetadataExchange& proto_config,
    Server::Configuration::CommonFactoryContext& context) {
  return createFilterFactoryHelper(proto_config, context, FilterDirection::Upstream);
}

/**
 * Static registration for the MetadataExchange Downstream filter. @see
 * RegisterFactory.
 */
static Registry::RegisterFactory<MetadataExchangeConfigFactory,
                                 Server::Configuration::NamedNetworkFilterConfigFactory>
    registered_;

/**
 * Static registration for the MetadataExchange Upstream filter. @see
 * RegisterFactory.
 */
static Registry::RegisterFactory<MetadataExchangeUpstreamConfigFactory,
                                 Server::Configuration::NamedUpstreamNetworkFilterConfigFactory>
    registered_upstream_;

} // namespace MetadataExchange
} // namespace Tcp
} // namespace Envoy
