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

#include "envoy/server/filter_config.h"

namespace Envoy {
namespace Tcp {
namespace ForwardDownstreamSni {

/**
 * Config registration for the forward_downstream_sni filter. @see
 * NamedNetworkFilterConfigFactory.
 */
class ForwardDownstreamSniNetworkFilterConfigFactory
    : public Server::Configuration::NamedNetworkFilterConfigFactory {
public:
  // NamedNetworkFilterConfigFactory
  Network::FilterFactoryCb
  createFilterFactoryFromProto(const Protobuf::Message&,
                               Server::Configuration::FactoryContext&) override;
  ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  std::string name() const override { return "forward_downstream_sni"; }
};

} // namespace ForwardDownstreamSni
} // namespace Tcp
} // namespace Envoy
