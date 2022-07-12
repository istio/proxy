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

#include "src/envoy/set_internal_dst_address/filter.h"
#include "src/envoy/set_internal_dst_address/config.pb.h"

#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"

#include "source/common/network/utility.h"

namespace Istio {
namespace SetInternalDstAddress {

Envoy::Network::FilterStatus Filter::onAccept(Envoy::Network::ListenerFilterCallbacks& cb) {
  auto& socket = cb.socket();
  auto iter = cb.dynamicMetadata().filter_metadata().find("tunnel");
  if (iter != cb.dynamicMetadata().filter_metadata().end()) {
      auto address_it = iter->second.fields().find("tunnel_address");
      if (address_it != iter->second.fields().end() && address_it->second.has_string_value()) {
          auto local_address = Envoy::Network::Utility::parseInternetAddressAndPortNoThrow(
                  address_it->second.string_value(), /*v6only=*/false);
          if (local_address) {
            ENVOY_LOG_MISC(trace, "Set as original dst {}", local_address->asString());
            socket.connectionInfoProvider().setLocalAddress(local_address);
            socket.connectionInfoProvider().restoreLocalAddress(local_address);
          } else {
            ENVOY_LOG_MISC(trace, "Failed to parse address: {}",
                         address_it->second.string_value());
          }
          return Envoy::Network::FilterStatus::Continue;
      }
  }
  ENVOY_LOG_MISC(trace, "Cannot find original address metadata");
  return Envoy::Network::FilterStatus::Continue;
}

class FilterFactory : public Envoy::Server::Configuration::NamedListenerFilterConfigFactory {
public:
  // NamedListenerFilterConfigFactory
  Envoy::Network::ListenerFilterFactoryCb createListenerFilterFactoryFromProto(
      const Envoy::Protobuf::Message&,
      const Envoy::Network::ListenerFilterMatcherSharedPtr& listener_filter_matcher,
      Envoy::Server::Configuration::ListenerFactoryContext&) override {
    return [listener_filter_matcher](
               Envoy::Network::ListenerFilterManager& filter_manager) -> void {
      filter_manager.addAcceptFilter(listener_filter_matcher,
                                     std::make_unique<Filter>());
    };
  }

  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<istio::set_internal_dst_address::v1::Config>();
  }

  std::string name() const override { return "istio.set_internal_dst_address"; }
};

REGISTER_FACTORY(FilterFactory, Envoy::Server::Configuration::NamedListenerFilterConfigFactory);

}  // namespace SetInternalDstAddress
}  // namespace Istio
