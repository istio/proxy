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

#include "source/extensions/filters/network/istio_authn/config.h"

#include "envoy/registry/registry.h"
#include "source/common/common/hash.h"
#include "source/extensions/filters/network/common/factory_base.h"
#include "source/extensions/filters/network/istio_authn/config.pb.h"
#include "source/extensions/filters/network/istio_authn/config.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace IstioAuthn {

absl::optional<uint64_t> Principal::hash() const {
  // XXX: This should really be a cryptographic hash to avoid SAN collision.
  return HashUtil::xxHash64(principal_);
}

PrincipalInfo getPrincipals(const StreamInfo::FilterState& filter_state) {
  const auto* peer = filter_state.getDataReadOnly<Principal>(PeerPrincipalKey);
  const auto* local = filter_state.getDataReadOnly<Principal>(LocalPrincipalKey);
  return {peer ? peer->principal() : absl::string_view(),
          local ? local->principal() : absl::string_view()};
}

void IstioAuthnFilter::onEvent(Network::ConnectionEvent event) {
  switch (event) {
  case Network::ConnectionEvent::Connected:
    // TLS handshake success triggers this event.
    populate();
    break;
  default:
    break;
  }
}
void IstioAuthnFilter::initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) {
  read_callbacks_ = &callbacks;
  read_callbacks_->connection().addConnectionCallbacks(*this);
}

void IstioAuthnFilter::populate() const {
  Network::Connection& conn = read_callbacks_->connection();
  const auto ssl = conn.ssl();
  if (ssl && ssl->peerCertificatePresented()) {
    for (const std::string& san : ssl->uriSanPeerCertificate()) {
      if (absl::StartsWith(san, SpiffePrefix)) {
        conn.streamInfo().filterState()->setData(PeerPrincipalKey, std::make_shared<Principal>(san),
                                                 StreamInfo::FilterState::StateType::ReadOnly,
                                                 StreamInfo::FilterState::LifeSpan::Connection,
                                                 shared_);
        break;
      }
    }
    for (const std::string& san : ssl->uriSanLocalCertificate()) {
      if (absl::StartsWith(san, SpiffePrefix)) {
        conn.streamInfo().filterState()->setData(
            LocalPrincipalKey, std::make_shared<Principal>(san),
            StreamInfo::FilterState::StateType::ReadOnly,
            StreamInfo::FilterState::LifeSpan::Connection, shared_);
        break;
      }
    }
  }
}

class IstioAuthnConfigFactory : public Common::FactoryBase<io::istio::network::authn::Config> {
public:
  IstioAuthnConfigFactory() : FactoryBase("io.istio.network.authn") {}

private:
  Network::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const io::istio::network::authn::Config& config,
                                    Server::Configuration::FactoryContext&) override {
    return [shared = config.shared()](Network::FilterManager& filter_manager) -> void {
      filter_manager.addReadFilter(std::make_shared<IstioAuthnFilter>(shared));
    };
  }
};

REGISTER_FACTORY(IstioAuthnConfigFactory, Server::Configuration::NamedNetworkFilterConfigFactory);

} // namespace IstioAuthn
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
