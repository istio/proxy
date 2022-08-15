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

#include "src/envoy/tls_passthrough/filter.h"

#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"
#include "src/envoy/tls_passthrough/config.pb.h"

namespace Istio {
namespace TLSPassthrough {

// Implementation considers that peer URI SAN is a hash key for TLS connections,
// that is two downstream TLS connections can share upstream internal connection
// if they have the same peer URI SAN.
absl::optional<uint64_t> SslInfoObject::hash() const {
  if (ssl_info_) {
    auto peer_uri_san = ssl_info_->uriSanPeerCertificate();
    if (!peer_uri_san.empty()) {
      std::cout << "HASHING!" << absl::StrJoin(peer_uri_san, ",") << std::endl;
      return Envoy::HashUtil::xxHash64(absl::StrJoin(peer_uri_san, ","));
    }
  }
  return {};
}

Envoy::Network::FilterStatus CaptureTLSFilter::onNewConnection() {
  const auto ssl_info = read_callbacks_->connection().ssl();
  if (ssl_info != nullptr) {
    read_callbacks_->connection().streamInfo().filterState()->setData(
        SslInfoFilterStateKey,
        std::make_shared<SslInfoObject>(ssl_info),
        Envoy::StreamInfo::FilterState::StateType::Mutable,
        Envoy::StreamInfo::FilterState::LifeSpan::Connection,
        Envoy::StreamInfo::FilterState::StreamSharing::
            SharedWithUpstreamConnection);
  } else {
    ENVOY_LOG(trace, "CaptureTLS: plaintext connection, expect TLS");
  }
  return Envoy::Network::FilterStatus::Continue;
}

Envoy::Network::FilterStatus RestoreTLSFilter::onNewConnection() {
  const auto filter_state =
      read_callbacks_->connection().streamInfo().filterState();
  const SslInfoObject* ssl_object =
      filter_state->getDataMutable<SslInfoObject>(SslInfoFilterStateKey);
  if (ssl_object && ssl_object->ssl()) {
    read_callbacks_->connection().connectionInfoSetter().setSslConnection(
        ssl_object->ssl());
  } else {
    ENVOY_LOG(trace, "RestoreTLS: filter state object not found");
  }
  return Envoy::Network::FilterStatus::Continue;
}

class CaptureTLSFilterFactory
    : public Envoy::Server::Configuration::NamedNetworkFilterConfigFactory {
 public:
  // NamedNetworkFilterConfigFactory
  Envoy::Network::FilterFactoryCb createFilterFactoryFromProto(
      const Envoy::Protobuf::Message&,
      Envoy::Server::Configuration::FactoryContext&) override {
    return [](Envoy::Network::FilterManager& filter_manager) {
      filter_manager.addReadFilter(std::make_shared<CaptureTLSFilter>());
    };
  }

  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<istio::tls_passthrough::v1::CaptureTLS>();
  }

  std::string name() const override { return "istio.capture_tls"; }
};

class RestoreTLSFilterFactory
    : public Envoy::Server::Configuration::NamedNetworkFilterConfigFactory {
 public:
  // NamedNetworkFilterConfigFactory
  Envoy::Network::FilterFactoryCb createFilterFactoryFromProto(
      const Envoy::Protobuf::Message&,
      Envoy::Server::Configuration::FactoryContext&) override {
    return [](Envoy::Network::FilterManager& filter_manager) {
      filter_manager.addReadFilter(std::make_shared<RestoreTLSFilter>());
    };
  }

  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<istio::tls_passthrough::v1::RestoreTLS>();
  }

  std::string name() const override { return "istio.restore_tls"; }
};

REGISTER_FACTORY(CaptureTLSFilterFactory,
                 Envoy::Server::Configuration::NamedNetworkFilterConfigFactory);

REGISTER_FACTORY(RestoreTLSFilterFactory,
                 Envoy::Server::Configuration::NamedNetworkFilterConfigFactory);

}  // namespace TLSPassthrough
}  // namespace Istio
