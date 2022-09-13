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

#pragma once

#include "source/extensions/filters/http/common/factory_base.h"
#include "src/envoy/http/baggage_handler/config/baggage_handler.pb.h"

namespace Envoy {
namespace Http {
namespace BaggageHandler {

/**
 * Config registration for the baggage handler filter.
 */
class BaggageHandlerConfigFactory
    : public Server::Configuration::NamedHttpFilterConfigFactory {
 public:
  // Server::Configuration::NamedHttpFilterConfigFactory
  Http::FilterFactoryCb createFilterFactoryFromProto(
      const Protobuf::Message &config, const std::string &stat_prefix,
      Server::Configuration::FactoryContext &context) override;

  ProtobufTypes::MessagePtr createEmptyConfigProto() override;

  std::string name() const override;

 private:
  Http::FilterFactoryCb createFilterFactory(
      const istio::telemetry::baggagehandler::v1::Config &config_pb);
};

}  // namespace BaggageHandler
}  // namespace Http
}  // namespace Envoy
