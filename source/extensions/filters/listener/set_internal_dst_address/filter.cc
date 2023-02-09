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

#include "source/extensions/filters/listener/set_internal_dst_address/filter.h"

#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"
#include "source/common/common/hash.h"
#include "source/common/network/filter_state_dst_address.h"
#include "source/common/network/utility.h"
#include "source/extensions/filters/listener/set_internal_dst_address/config.pb.h"

namespace Istio {
namespace SetInternalDstAddress {

constexpr std::string_view MetadataKey = "tunnel";
constexpr std::string_view DestinationAddressField = "destination";
constexpr std::string_view TunnelAddressField = "address";

absl::optional<uint64_t> Authority::hash() const { return Envoy::HashUtil::xxHash64(value_); }

Envoy::Network::FilterStatus Filter::onAccept(Envoy::Network::ListenerFilterCallbacks& cb) {
  auto& socket = cb.socket();
  // First, try the dynamic metadata from the endpoint.
  const auto iter = cb.dynamicMetadata().filter_metadata().find(MetadataKey);
  if (iter != cb.dynamicMetadata().filter_metadata().end()) {
    auto address_it = iter->second.fields().find(DestinationAddressField);
    if (address_it != iter->second.fields().end() && address_it->second.has_string_value()) {
      const auto local_address = Envoy::Network::Utility::parseInternetAddressAndPortNoThrow(
          address_it->second.string_value(), /*v6only=*/false);
      if (local_address) {
        ENVOY_LOG_MISC(trace, "Restore local address: {}", local_address->asString());
        socket.connectionInfoProvider().restoreLocalAddress(local_address);
      } else {
        ENVOY_LOG_MISC(trace, "Failed to parse {} address: {}", DestinationAddressField,
                       address_it->second.string_value());
      }
    } else {
      ENVOY_LOG_MISC(trace, "Missing metadata field '{}'", DestinationAddressField);
    }
    address_it = iter->second.fields().find(TunnelAddressField);
    if (address_it != iter->second.fields().end() && address_it->second.has_string_value()) {
      const auto tunnel_address = Envoy::Network::Utility::parseInternetAddressAndPortNoThrow(
          address_it->second.string_value(), /*v6only=*/false);
      if (tunnel_address) {
        ENVOY_LOG_MISC(trace, "Restore ORIGINAL_DST address: {}", tunnel_address->asString());
        // Should never throw as the stream info is initialized as empty.
        cb.filterState().setData(
            Envoy::Network::DestinationAddress::key(),
            std::make_shared<Envoy::Network::DestinationAddress>(tunnel_address),
            Envoy::StreamInfo::FilterState::StateType::ReadOnly);
      } else {
        ENVOY_LOG_MISC(trace, "Failed to parse {} address: {}", TunnelAddressField,
                       address_it->second.string_value());
      }
    } else {
      ENVOY_LOG_MISC(trace, "Missing metadata field '{}'", TunnelAddressField);
    }
    return Envoy::Network::FilterStatus::Continue;
  } else {
    ENVOY_LOG_MISC(trace, "Cannot find dynamic metadata '{}'", MetadataKey);
  }
  // Second, check the filter state;
  const auto* object = cb.filterState().getDataReadOnly<Authority>(FilterStateKey);
  if (object) {
    const auto local_address = Envoy::Network::Utility::parseInternetAddressAndPortNoThrow(
        object->value_, /*v6only=*/false);
    if (local_address) {
      ENVOY_LOG_MISC(trace, "Restore local address from filter state: {}",
                     local_address->asString());
      socket.connectionInfoProvider().restoreLocalAddress(local_address);
      const auto tunnel_address =
          object->port_ > 0
              ? Envoy::Network::Utility::getAddressWithPort(*local_address, object->port_)
              : local_address;
      cb.filterState().setData(Envoy::Network::DestinationAddress::key(),
                               std::make_shared<Envoy::Network::DestinationAddress>(tunnel_address),
                               Envoy::StreamInfo::FilterState::StateType::ReadOnly);
    } else {
      ENVOY_LOG_MISC(trace, "Failed to parse filter state address: {}", object->value_);
    }
  }
  return Envoy::Network::FilterStatus::Continue;
}

class FilterFactory : public Envoy::Server::Configuration::NamedListenerFilterConfigFactory {
public:
  // NamedListenerFilterConfigFactory
  Envoy::Network::ListenerFilterFactoryCb createListenerFilterFactoryFromProto(
      const Envoy::Protobuf::Message&,
      const Envoy::Network::ListenerFilterMatcherSharedPtr& listener_filter_matcher,
      Envoy::Server::Configuration::ListenerFactoryContext&) override {
    return
        [listener_filter_matcher](Envoy::Network::ListenerFilterManager& filter_manager) -> void {
          filter_manager.addAcceptFilter(listener_filter_matcher, std::make_unique<Filter>());
        };
  }

  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<istio::set_internal_dst_address::v1::Config>();
  }

  std::string name() const override { return "istio.set_internal_dst_address"; }
};

REGISTER_FACTORY(FilterFactory, Envoy::Server::Configuration::NamedListenerFilterConfigFactory);

} // namespace SetInternalDstAddress
} // namespace Istio
