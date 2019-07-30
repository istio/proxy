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

#include "extensions/stats/config.h"

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
      getMetadataValue(MetadataType::Node, Common::kIstioMetadataKey);
  status = Common::extractNodeMetadata(node_metadata.struct_value(),
                                       &local_node_info_);
  if (status != Status::OK) {
    logWarn("cannot parse local node metadata " + node_metadata.DebugString() +
            ": " + status.ToString());
  }
}

void PluginRootContext::report(const Common::RequestInfo& requestInfo) {
  auto outbound = stats::PluginConfig_Direction_OUTBOUND == direction();

  auto metadataIdKey = Common::kDownstreamMetadataIdKey;
  auto metadataKey = Common::kDownstreamMetadataKey;
  std::string reporter = "server";

  if (outbound) {
    metadataIdKey = Common::kUpstreamMetadataIdKey;
    metadataKey = Common::kUpstreamMetadataKey;
    reporter = "client";
  }

  auto peer = node_info_cache_.getPeerById(metadataIdKey, metadataKey);

  // check if this peer has associated metrics
  //- source_principal
  //- destination_principal
  //- request_protocol
  //- response_code
  //- connection_mtls
  auto metric_base_key = absl::StrCat(
      peer->key, Sep, requestInfo.source_principal, Sep,
      requestInfo.destination_principal, Sep, requestInfo.request_protocol, Sep,
      requestInfo.response_code, Sep, requestInfo.mTLS);

  logInfo(metric_base_key);

  for (auto& statgen : stats_) {
    auto key = absl::StrCat(metric_base_key, Sep, statgen.name());
    auto metric_it = metric_map_.find(key);
    SimpleStatSharedPtr stat;
    if (metric_it == metric_map_.end()) {
      if (outbound) {
        stat = statgen.resolve(reporter, local_node_info_, peer->node_info,
                               requestInfo);
      } else {
        stat = statgen.resolve(reporter, peer->node_info, local_node_info_,
                               requestInfo);
      }
      metric_map_[key] = stat;
    } else {
      stat = metric_it->second;
    }

    stat->record(requestInfo);
  }
}

const NodeSharedPtr NodeInfoCache::getPeerById(StringView peerMetadataIdKey,
                                               StringView peerMetadataKey) {
  auto peerId =
      getMetadataStringValue(MetadataType::Request, peerMetadataIdKey);
  auto nodeinfo_it = cache_.find(peerId);
  if (nodeinfo_it != cache_.end()) {
    return nodeinfo_it->second;
  }

  // TODO kick out some elements from cache here if size == MAX

  common::NodeInfo nodeInfo;
  // Missed the cache
  auto metadata = getMetadataStruct(MetadataType::Request, peerMetadataKey);
  auto status = Common::extractNodeMetadata(metadata, &nodeInfo);
  if (status != Status::OK) {
    logWarn("cannot parse peer node metadata " + metadata.DebugString() + ": " +
            status.ToString());
  }

  auto new_nodeinfo = std::make_shared<Node>(nodeInfo);
  cache_[peerId] = new_nodeinfo;

  logInfo(absl::StrCat("created: ", new_nodeinfo->node_info.DebugString()));
  return new_nodeinfo;
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
