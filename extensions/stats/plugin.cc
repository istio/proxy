/* Copyright 2019 Istio Authors. All Rights Reserved.
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

#include "extensions/stats/plugin.h"

// WASM_PROLOG
#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"

#else  // NULL_PLUGIN

#include "extensions/common/wasm/null/null.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {

#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace Stats {

void PluginRootContext::onConfigure(std::unique_ptr<WasmData> configuration) {
  // Parse configuration JSON string.
  JsonParseOptions json_options;
  Status status =
      JsonStringToMessage(configuration->toString(), &config_, json_options);
  if (status != Status::OK) {
    logWarn("Cannot parse plugin configuration JSON string " +
            configuration->toString());
    return;
  }

  auto node_metadata =
      getMetadataValue(MetadataType::Node, ::Wasm::Common::kIstioMetadataKey);
  status = ::Wasm::Common::extractNodeMetadata(node_metadata.struct_value(),
                                               &local_node_info_);
  if (status != Status::OK) {
    logWarn("cannot parse local node metadata " + node_metadata.DebugString() +
            ": " + status.ToString());
  }

  outbound_ = stats::PluginConfig_Direction_OUTBOUND == direction();
  if (outbound_) {
    peer_metadata_id_key_ = ::Wasm::Common::kUpstreamMetadataIdKey;
    peer_metadata_key_ = ::Wasm::Common::kUpstreamMetadataKey;
  } else {
    peer_metadata_id_key_ = ::Wasm::Common::kDownstreamMetadataIdKey;
    peer_metadata_key_ = ::Wasm::Common::kDownstreamMetadataKey;
  }
}

void PluginRootContext::report(
    const ::Wasm::Common::RequestInfo& request_info) {
  auto peer =
      node_info_cache_.getPeerById(peer_metadata_id_key_, peer_metadata_key_);

  const auto& source_node_info = outbound_ ? local_node_info_ : peer.node_info;
  const auto& destination_node_info =
      outbound_ ? peer.node_info : local_node_info_;

  ::Wasm::Common::RequestContext ctx{outbound_, source_node_info,
                                     destination_node_info, request_info};

  // check if this peer has associated metrics
  // These fields should vary independently of peer properties.
  // TODO derive this from the mapper
  auto metric_base_key =
      absl::StrCat(peer.key, Sep, request_info.request_protocol, Sep,
                   request_info.response_code, Sep, request_info.response_flag,
                   Sep, request_info.mTLS);

  IstioDimensions istio_dimensions;

  for (auto& statgen : stats_) {
    auto key = absl::StrCat(metric_base_key, Sep, statgen.name());
    auto metric_it = metric_map_.find(key);
    if (metric_it != metric_map_.end()) {
      metric_it->second.record(request_info);
      continue;
    }

    // missed cache
    auto stat = statgen.resolve(istio_dimensions.mapOnce(ctx));

    metric_map_.insert({key, stat});
    stat.record(request_info);
  }
}

const Node& NodeInfoCache::getPeerById(StringView peer_metadata_id_key,
                                       StringView peer_metadata_key) {
  auto peer_id =
      getMetadataStringValue(MetadataType::Request, peer_metadata_id_key);
  auto nodeinfo_it = cache_.find(peer_id);
  if (nodeinfo_it != cache_.end()) {
    return nodeinfo_it->second;
  }

  // TODO kick out some elements from cache here if size == MAX

  InitializeNode(peer_metadata_key, &(cache_[peer_id]));
  return cache_[peer_id];
}

// Registration glue

NullVmPluginRootRegistry* context_registry_{};

class StatsFactory : public NullVmPluginFactory {
 public:
  const std::string name() const override { return "envoy.wasm.stats"; }

  std::unique_ptr<NullVmPlugin> create() const override {
    return std::make_unique<NullVmPlugin>(context_registry_);
  }
};

static Registry::RegisterFactory<StatsFactory, NullVmPluginFactory> register_;

}  // namespace Stats

// WASM_EPILOG
#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace Null
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
#endif