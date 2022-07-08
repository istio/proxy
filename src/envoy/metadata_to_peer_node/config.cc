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

#include <string>

#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"
#include "src/envoy/metadata_to_peer_node/config/metadata_to_peer_node.pb.h"
#include "src/envoy/metadata_to_peer_node/metadata_to_peer_node.h"

namespace Envoy {
namespace MetadataToPeerNode {

namespace {
constexpr absl::string_view kFactoryName =
    "envoy.filters.listener.metadata_to_peer_node";
}
/**
 * Config registration for the metadata to peer node filter. @see
 * NamedNetworkFilterConfigFactory.
 */
class MetadataToPeerNodeConfigFactory
    : public Server::Configuration::NamedListenerFilterConfigFactory {
 public:
  // NamedListenerFilterConfigFactory
  Network::ListenerFilterFactoryCb createListenerFilterFactoryFromProto(
      const Protobuf::Message& message,
      const Network::ListenerFilterMatcherSharedPtr& listener_filter_matcher,
      Server::Configuration::ListenerFactoryContext&) override {
    // downcast it to the workload metadata config
    const auto& typed_config =
        dynamic_cast<const istio::telemetry::metadatatopeernode::v1::Config&>(
            message);

    ConfigSharedPtr config = std::make_shared<Config>(typed_config);
    return [listener_filter_matcher,
            config](Network::ListenerFilterManager& filter_manager) -> void {
      filter_manager.addAcceptFilter(listener_filter_matcher,
                                     std::make_unique<Filter>(config));
    };
  }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<istio::telemetry::metadatatopeernode::v1::Config>();
  }

  std::string name() const override { return std::string(kFactoryName); }
};

/**
 * Static registration for the workload metadata filter. @see RegisterFactory.
 */
REGISTER_FACTORY(MetadataToPeerNodeConfigFactory,
                 Server::Configuration::NamedListenerFilterConfigFactory);

}  // namespace MetadataToPeerNode
}  // namespace Envoy
