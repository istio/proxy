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

#include "envoy/network/filter.h"
#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"
#include "source/common/common/hash.h"
#include "source/extensions/filters/network/common/factory_base.h"
#include "source/extensions/filters/network/istio_authn/config.pb.h"
#include "source/extensions/filters/network/istio_authn/config.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace IstioAuthn {

absl::optional<uint64_t> PeerPrincipal::hash() const {
  // XXX: This should really be a cryptographic hash to avoid SAN collision.
  return Envoy::HashUtil::xxHash64(principal_);
}

class IstioAuthnFilter : public Network::ReadFilter {
 public:
  // Network::ReadFilter
  Network::FilterStatus onData(Buffer::Instance&, bool) override {
    return Network::FilterStatus::Continue;
  }
  Network::FilterStatus onNewConnection() override {
    return Network::FilterStatus::Continue;
  }
  void initializeReadFilterCallbacks(
      Network::ReadFilterCallbacks& callbacks) override {
    Network::Connection& conn = callbacks.connection();
    const auto ssl = conn.ssl();
    if (ssl && ssl->peerCertificatePresented()) {
      for (const std::string& san : ssl->uriSanPeerCertificate()) {
        if (absl::StartsWith(san, SpiffePrefix)) {
          conn.streamInfo().filterState()->setData(
              PeerPrincipalKey, std::make_shared<PeerPrincipal>(san),
              StreamInfo::FilterState::StateType::ReadOnly,
              StreamInfo::FilterState::LifeSpan::Connection,
              StreamInfo::FilterState::StreamSharing::
                  SharedWithUpstreamConnection);
          break;
        }
      }
    }
  }
};

class IstioAuthnConfigFactory
    : public Common::FactoryBase<io::istio::network::authn::Config> {
 public:
  IstioAuthnConfigFactory() : FactoryBase("io.istio.network.authn") {}

 private:
  Network::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const io::istio::network::authn::Config&,
      Server::Configuration::FactoryContext&) override {
    return [](Network::FilterManager& filter_manager) -> void {
      filter_manager.addReadFilter(std::make_shared<IstioAuthnFilter>());
    };
  }
};

REGISTER_FACTORY(IstioAuthnConfigFactory,
                 Server::Configuration::NamedNetworkFilterConfigFactory);

}  // namespace IstioAuthn
}  // namespace NetworkFilters
}  // namespace Extensions
}  // namespace Envoy
