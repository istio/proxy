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

#include "src/envoy/http/baggage_handler/config.h"

#include "source/common/protobuf/message_validator_impl.h"
#include "src/envoy/http/baggage_handler/baggage_handler.h"
#include "src/envoy/http/baggage_handler/config/baggage_handler.pb.h"

namespace Envoy {
namespace Http {
namespace BaggageHandler {

Http::FilterFactoryCb BaggageHandlerConfigFactory::createFilterFactoryFromProto(
    const Protobuf::Message &config, const std::string &,
    Server::Configuration::FactoryContext &) {
  return createFilterFactory(
      dynamic_cast<const istio::telemetry::baggagehandler::v1::Config &>(
          config));
}

ProtobufTypes::MessagePtr
BaggageHandlerConfigFactory::createEmptyConfigProto() {
  return ProtobufTypes::MessagePtr{
      new istio::telemetry::baggagehandler::v1::Config};
}

std::string BaggageHandlerConfigFactory::name() const {
  return "istio.filters.http.baggage_handler";
}

Http::FilterFactoryCb BaggageHandlerConfigFactory::createFilterFactory(
    const istio::telemetry::baggagehandler::v1::Config &proto_config) {
  ConfigSharedPtr filter_config(std::make_shared<Config>(proto_config));
  return [filter_config](Http::FilterChainFactoryCallbacks &callbacks) -> void {
    callbacks.addStreamFilter(
        Http::StreamFilterSharedPtr{new BaggageHandlerFilter(filter_config)});
  };
}

/**
 * Static registration for the baggage handler filter. @see RegisterFactory.
 */
REGISTER_FACTORY(BaggageHandlerConfigFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory);

}  // namespace BaggageHandler
}  // namespace Http
}  // namespace Envoy
