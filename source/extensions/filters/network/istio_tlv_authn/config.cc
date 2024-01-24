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

#include "source/extensions/filters/network/istio_tlv_authn/config.h"

#include "envoy/registry/registry.h"
#include "source/common/common/hash.h"
#include "source/common/network/proxy_protocol_filter_state.h"
#include "source/extensions/filters/network/common/factory_base.h"
#include "source/extensions/filters/network/istio_tlv_authn/config.pb.h"
#include "source/extensions/filters/network/istio_tlv_authn/config.pb.validate.h"
#include "source/common/router/string_accessor_impl.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace IstioAuthn {

// Copied from filter_objects.h to avoid duplicate REGISTER_FACTORY from include
constexpr absl::string_view PeerPrincipalKey = "io.istio.peer_principal";

void IstioTLVAuthnFilter::onEvent(Network::ConnectionEvent event) {
  switch (event) {
  case Network::ConnectionEvent::Connected:
    // TLS handshake success triggers this event.
    populate();
    break;
  default:
    break;
  }
}

void IstioTLVAuthnFilter::initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) {
  read_callbacks_ = &callbacks;
  read_callbacks_->connection().addConnectionCallbacks(*this);
}

const uint8_t principalTlvType = 0xD0;

void IstioTLVAuthnFilter::populate() const {
  Network::Connection& conn = read_callbacks_->connection();
  const auto* proxy_proto =
      conn.streamInfo().filterState()->getDataReadOnly<Network::ProxyProtocolFilterState>(
          Network::ProxyProtocolFilterState::key());
  const auto& tlv_vector = proxy_proto->value().tlv_vector_;

  auto tlv =
      std::find_if(tlv_vector.begin(), tlv_vector.end(), [](const Network::ProxyProtocolTLV& tlv) {
        return tlv.type == principalTlvType;
      });
  if (tlv == tlv_vector.end()) {
    return;
  }

  ENVOY_LOG(debug, "istio_tlv_authn: setting io.istio.peer_principal from PROXY Protocol TLV");
  std::string id(tlv->value.begin(), tlv->value.end());
  auto peer_principal = std::make_shared<Router::StringAccessorImpl>(id);
  conn.streamInfo().filterState()->setData(PeerPrincipalKey, peer_principal,
                                           StreamInfo::FilterState::StateType::ReadOnly,
                                           StreamInfo::FilterState::LifeSpan::Connection, shared_);
}

class IstioTLVAuthnConfigFactory
    : public Common::FactoryBase<io::istio::network::tlv_authn::Config> {
public:
  IstioTLVAuthnConfigFactory() : FactoryBase("io.istio.network.tlv_authn") {}

private:
  Network::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const io::istio::network::tlv_authn::Config& config,
                                    Server::Configuration::FactoryContext&) override {
    return [shared = config.shared()](Network::FilterManager& filter_manager) -> void {
      filter_manager.addReadFilter(std::make_shared<IstioTLVAuthnFilter>(shared));
    };
  }
};

REGISTER_FACTORY(IstioTLVAuthnConfigFactory,
                 Server::Configuration::NamedNetworkFilterConfigFactory);

} // namespace IstioAuthn
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
