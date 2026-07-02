/* Copyright Istio Authors. All Rights Reserved.
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

#include "source/extensions/filters/http/misdirected_request/config.h"

#include "source/extensions/filters/http/misdirected_request/misdirected_filter.h"

using istio::envoy::config::filter::http::misdirected_request::v1alpha1::FilterConfig;

namespace Envoy {
namespace Http {
namespace MisdirectedRequest {

absl::StatusOr<Http::FilterFactoryCb>
MisdirectedRequestConfigFactory::createFilterFactoryFromProto(
    const Protobuf::Message& config, const std::string&,
    Server::Configuration::FactoryContext& context) {
  const auto& proto_config = dynamic_cast<const FilterConfig&>(config);
  // Read the shared per-port hostname set once, from listener metadata, at config
  // construction time (once per listener) rather than per request.
  MisdirectedFilterConfigSharedPtr filter_config{std::make_shared<MisdirectedFilterConfig>(
      proto_config, context.listenerInfo().metadata())};
  return [filter_config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamDecoderFilter(std::make_unique<MisdirectedFilter>(filter_config));
  };
}

ProtobufTypes::MessagePtr MisdirectedRequestConfigFactory::createEmptyConfigProto() {
  return ProtobufTypes::MessagePtr{new FilterConfig};
}

std::string MisdirectedRequestConfigFactory::name() const { return "istio.misdirected_request"; }

/**
 * Static registration for the misdirected request filter. @see RegisterFactory.
 */
REGISTER_FACTORY(MisdirectedRequestConfigFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace MisdirectedRequest
} // namespace Http
} // namespace Envoy
