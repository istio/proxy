// Copyright Istio Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"
#include "src/envoy/internal_ssl_forwarder/config/internal_ssl_forwarder.pb.h"

constexpr char kFactoryName[] = "istio.filters.network.internal_ssl_forwarder";

using namespace istio::telemetry::internal_ssl_forwarder;

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace InternalSslForwarder {

/**
 * Config registration for the internal ssl forwarder filter. @see
 * NamedNetworkFilterConfigFactory.
 */
class InternalSslForwarderConfigFactory
    : public Server::Configuration::NamedNetworkFilterConfigFactory {
 public:
  InternalSslForwarderConfigFactory() {}

  Network::FilterFactoryCb createFilterFactoryFromProto(
      const Protobuf::Message& config,
      Server::Configuration::FactoryContext& filter_chain_factory_context)
      override;

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<v1::Config>();
  }

  std::string name() const override { return kFactoryName; }
};

}  // namespace InternalSslForwarder
}  // namespace NetworkFilters
}  // namespace Extensions
}  // namespace Envoy
