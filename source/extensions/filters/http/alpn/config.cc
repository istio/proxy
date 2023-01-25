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

#include "source/extensions/filters/http/alpn/config.h"

#include "source/common/protobuf/message_validator_impl.h"
#include "source/extensions/common/filter_names.h"
#include "source/extensions/filters/http/alpn/alpn_filter.h"

using istio::envoy::config::filter::http::alpn::v2alpha1::FilterConfig;

namespace Envoy {
namespace Http {
namespace Alpn {
Http::FilterFactoryCb
AlpnConfigFactory::createFilterFactoryFromProto(const Protobuf::Message& config, const std::string&,
                                                Server::Configuration::FactoryContext& context) {
  return createFilterFactory(dynamic_cast<const FilterConfig&>(config), context.clusterManager());
}

ProtobufTypes::MessagePtr AlpnConfigFactory::createEmptyConfigProto() {
  return ProtobufTypes::MessagePtr{new FilterConfig};
}

std::string AlpnConfigFactory::name() const { return Utils::IstioFilterName::kAlpn; }

Http::FilterFactoryCb
AlpnConfigFactory::createFilterFactory(const FilterConfig& proto_config,
                                       Upstream::ClusterManager& cluster_manager) {
  AlpnFilterConfigSharedPtr filter_config{
      std::make_shared<AlpnFilterConfig>(proto_config, cluster_manager)};
  return [filter_config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamDecoderFilter(std::make_unique<AlpnFilter>(filter_config));
  };
}

/**
 * Static registration for the alpn override filter. @see RegisterFactory.
 */
REGISTER_FACTORY(AlpnConfigFactory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace Alpn
} // namespace Http
} // namespace Envoy
