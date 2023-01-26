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

#include "source/extensions/filters/network/sni_verifier/config.h"

#include "envoy/registry/registry.h"
#include "source/extensions/filters/network/sni_verifier/config.pb.h"
#include "source/extensions/filters/network/sni_verifier/sni_verifier.h"

namespace Envoy {
namespace Tcp {
namespace SniVerifier {

Network::FilterFactoryCb SniVerifierConfigFactory::createFilterFactoryFromProto(
    const Protobuf::Message&, Server::Configuration::FactoryContext& context) {
  return createFilterFactoryFromContext(context);
}

ProtobufTypes::MessagePtr SniVerifierConfigFactory::createEmptyConfigProto() {
  return std::make_unique<io::istio::tcp::sni_verifier::v1::Config>();
}

Network::FilterFactoryCb SniVerifierConfigFactory::createFilterFactoryFromContext(
    Server::Configuration::FactoryContext& context) {
  ConfigSharedPtr filter_config(new Config(context.scope()));
  return [filter_config](Network::FilterManager& filter_manager) -> void {
    filter_manager.addReadFilter(std::make_shared<Filter>(filter_config));
  };
}

/**
 * Static registration for the echo filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<SniVerifierConfigFactory,
                                 Server::Configuration::NamedNetworkFilterConfigFactory>
    registered_;

} // namespace SniVerifier
} // namespace Tcp
} // namespace Envoy
