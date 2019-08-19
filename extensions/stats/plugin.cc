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

  google::protobuf::Value node_metadata;
  if (getMetadataValue(MetadataType::Node, ::Wasm::Common::kIstioMetadataKey,
                       &node_metadata) != Common::Wasm::MetadataResult::Ok) {
    logWarn(absl::StrCat("cannot get metadata for: ",
                         ::Wasm::Common::kIstioMetadataKey));
    return;
  }

  status = ::Wasm::Common::extractNodeMetadata(node_metadata.struct_value(),
                                               &local_node_info_);
  if (status != Status::OK) {
    logWarn("cannot parse local node metadata " + node_metadata.DebugString() +
            ": " + status.ToString());
    return;
  }
  PluginDirection direction;
  auto dirn_result = getPluginDirection(&direction);
  if (MetadataResult::Ok == dirn_result) {
    outbound_ = PluginDirection::Outbound == direction;
  } else {
    logWarn(absl::StrCat("Unable to get plugin direction: ", dirn_result));
  }
  // Local data does not change, so populate it on config load.
  istio_dimensions_.init(outbound_, local_node_info_);

  if (outbound_) {
    peer_metadata_id_key_ = ::Wasm::Common::kUpstreamMetadataIdKey;
    peer_metadata_key_ = ::Wasm::Common::kUpstreamMetadataKey;
  } else {
    peer_metadata_id_key_ = ::Wasm::Common::kDownstreamMetadataIdKey;
    peer_metadata_key_ = ::Wasm::Common::kDownstreamMetadataKey;
  }
  debug_ = config_.debug();
  node_info_cache_.set_max_cache_size(config_.max_peer_cache_size());

  auto field_separator = CONFIG_DEFAULT(field_separator);
  auto value_separator = CONFIG_DEFAULT(value_separator);
  auto stat_prefix = CONFIG_DEFAULT(stat_prefix);

  // prepend "_" to opt out of automatic namespacing
  // If "_" is not prepended, envoy_ is automatically added by prometheus
  // scraper"
  stat_prefix = absl::StrCat("_", stat_prefix, "_");

  stats_ = std::vector<StatGen>{
      StatGen(
          absl::StrCat(stat_prefix, "requests_total"), MetricType::Counter,
          [](const ::Wasm::Common::RequestInfo&) -> uint64_t { return 1; },
          field_separator, value_separator),
      StatGen(
          absl::StrCat(stat_prefix, "request_duration_seconds"),
          MetricType::Histogram,
          [](const ::Wasm::Common::RequestInfo& request_info) -> uint64_t {
            return request_info.end_timestamp - request_info.start_timestamp;
          },
          field_separator, value_separator),
      StatGen(
          absl::StrCat(stat_prefix, "request_bytes"), MetricType::Histogram,
          [](const ::Wasm::Common::RequestInfo& request_info) -> uint64_t {
            return request_info.request_size;
          },
          field_separator, value_separator),
      StatGen(
          absl::StrCat(stat_prefix, "response_bytes"), MetricType::Histogram,
          [](const ::Wasm::Common::RequestInfo& request_info) -> uint64_t {
            return request_info.response_size;
          },
          field_separator, value_separator)};
}

void PluginRootContext::report(
    const ::Wasm::Common::RequestInfo& request_info) {
  const auto& peer_node =
      node_info_cache_.getPeerById(peer_metadata_id_key_, peer_metadata_key_);

  // map and overwrite previous mapping.
  istio_dimensions_.map(peer_node, request_info);

  auto stats_it = metrics_.find(istio_dimensions_);
  if (stats_it != metrics_.end()) {
    for (auto& stat : stats_it->second) {
      stat.record(request_info);
      CTXDEBUG("metricKey cache hit ", istio_dimensions_.debug_key(),
               ", stat=", stat.metric_id_, stats_it->first.to_string());
    }
    cache_hits_accumulator_++;
    if (cache_hits_accumulator_ == 100) {
      incrementMetric(cache_hits_, cache_hits_accumulator_);
      cache_hits_accumulator_ = 0;
    }
    return;
  }

  // fetch dimensions in the required form for resolve.
  auto values = istio_dimensions_.values();

  std::vector<SimpleStat> stats;
  for (auto& statgen : stats_) {
    auto stat = statgen.resolve(values);
    CTXDEBUG("metricKey cache miss ", statgen.name(), " ",
             istio_dimensions_.debug_key(), ", stat=", stat.metric_id_);
    stat.record(request_info);
    stats.push_back(stat);
  }

  incrementMetric(cache_misses_, 1);
  metrics_.try_emplace(istio_dimensions_, stats);
}

const wasm::common::NodeInfo& NodeInfoCache::getPeerById(
    StringView peer_metadata_id_key, StringView peer_metadata_key) {
  std::string peer_id;
  if (getMetadataStringValue(MetadataType::Request, peer_metadata_id_key,
                             &peer_id) != Common::Wasm::MetadataResult::Ok) {
    logWarn(absl::StrCat("cannot get metadata for: ", peer_metadata_id_key));
    return cache_[""];
  }

  auto nodeinfo_it = cache_.find(peer_id);
  if (nodeinfo_it != cache_.end()) {
    return nodeinfo_it->second;
  }

  // Do not let the cache grow beyond max_cache_size_.
  if (cache_.size() > max_cache_size_) {
    auto it = cache_.begin();
    cache_.erase(cache_.begin(), std::next(it, max_cache_size_ / 4));
    logInfo(absl::StrCat("cleaned cache, new cache_size:", cache_.size()));
  }

  google::protobuf::Struct metadata;
  if (getMetadataStruct(MetadataType::Request, peer_metadata_key, &metadata) !=
      Common::Wasm::MetadataResult::Ok) {
    logWarn(absl::StrCat("cannot get metadata for: ", peer_metadata_key));
    return cache_[""];
  }

  auto status =
      ::Wasm::Common::extractNodeMetadata(metadata, &(cache_[peer_id]));
  if (status != Status::OK) {
    logWarn("cannot parse peer node metadata " + metadata.DebugString() + ": " +
            status.ToString());
    return cache_[""];
  }

  return cache_[peer_id];
}

// Registration glue

NullPluginRootRegistry* context_registry_{};

class StatsFactory : public NullPluginFactory {
 public:
  const std::string name() const override { return "envoy.wasm.stats"; }

  std::unique_ptr<NullVmPlugin> create() const override {
    return std::make_unique<NullPlugin>(context_registry_);
  }
};

static Registry::RegisterFactory<StatsFactory, NullPluginFactory> register_;

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
