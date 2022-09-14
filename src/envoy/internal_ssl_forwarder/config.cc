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

#include "src/envoy/internal_ssl_forwarder/config.h"

#include "envoy/network/connection.h"
#include "envoy/registry/registry.h"
#include "source/common/config/utility.h"
#include "src/envoy/internal_ssl_forwarder/config/internal_ssl_forwarder.pb.h"
#include "src/envoy/internal_ssl_forwarder/internal_ssl_forwarder.h"

using namespace istio::telemetry::internal_ssl_forwarder;

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace InternalSslForwarder {

Network::FilterFactoryCb
InternalSslForwarderConfigFactory::createFilterFactoryFromProto(
    const Protobuf::Message& message, Server::Configuration::FactoryContext&) {
  const auto& typed_config = dynamic_cast<const v1::Config&>(message);

  ConfigSharedPtr config = std::make_shared<Config>(typed_config);

  return [config](Network::FilterManager& filter_manager) -> void {
    filter_manager.addReadFilter(std::make_shared<Filter>(config));
  };
};

/**
 * Static registration for the internal ssl forwarder filter. @see
 * RegisterFactory.
 */
REGISTER_FACTORY(InternalSslForwarderConfigFactory,
                 Server::Configuration::NamedNetworkFilterConfigFactory);

}  // namespace InternalSslForwarder
}  // namespace NetworkFilters
}  // namespace Extensions
}  // namespace Envoy
