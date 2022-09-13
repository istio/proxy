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
#include "src/envoy/workload_metadata/config/workload_metadata.pb.h"
#include "src/envoy/workload_metadata/workload_metadata.h"

namespace Envoy {
namespace WorkloadMetadata {

namespace {
constexpr absl::string_view kFactoryName =
    "envoy.filters.listener.workload_metadata";

constexpr char kClusterID[] = "CLUSTER_ID";
// TODO: should this just be blank?
constexpr char kDefaultClusterID[] = "Kubernetes";
}  // namespace
/**
 * Config registration for the workload metadata filter. @see
 * NamedNetworkFilterConfigFactory.
 */
class WorkloadMetadataConfigFactory
    : public Server::Configuration::NamedListenerFilterConfigFactory {
 public:
  // NamedListenerFilterConfigFactory
  Network::ListenerFilterFactoryCb createListenerFilterFactoryFromProto(
      const Protobuf::Message& message,
      const Network::ListenerFilterMatcherSharedPtr& listener_filter_matcher,
      Server::Configuration::ListenerFactoryContext& context) override {
    // downcast it to the workload metadata config

    auto node = context.localInfo().node();
    auto node_meta = node.metadata();
    auto cluster_name = node_meta.fields().contains(kClusterID)
                            ? node_meta.fields().at(kClusterID).string_value()
                            : kDefaultClusterID;

    const auto& typed_config =
        dynamic_cast<const istio::telemetry::workloadmetadata::v1::
                         WorkloadMetadataResources&>(message);

    ConfigSharedPtr config =
        std::make_shared<Config>(context.scope(), cluster_name, typed_config);
    return [listener_filter_matcher,
            config](Network::ListenerFilterManager& filter_manager) -> void {
      filter_manager.addAcceptFilter(listener_filter_matcher,
                                     std::make_unique<Filter>(config));
    };
  }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<
        istio::telemetry::workloadmetadata::v1::WorkloadMetadataResources>();
  }

  std::string name() const override { return std::string(kFactoryName); }
};

/**
 * Static registration for the workload metadata filter. @see RegisterFactory.
 */
REGISTER_FACTORY(WorkloadMetadataConfigFactory,
                 Server::Configuration::NamedListenerFilterConfigFactory);

}  // namespace WorkloadMetadata
}  // namespace Envoy
