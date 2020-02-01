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

#include "google/protobuf/util/time_util.h"

using google::protobuf::util::TimeUtil;

// WASM_PROLOG
#ifndef NULL_PLUGIN
#include "proxy_wasm_intrinsics.h"

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

bool PluginRootContext::onConfigure(std::unique_ptr<WasmData> configuration) {
  // Parse configuration JSON string.
  JsonParseOptions json_options;
  Status status =
      JsonStringToMessage(configuration->toString(), &config_, json_options);
  if (status != Status::OK) {
    LOG_WARN(absl::StrCat("Cannot parse plugin configuration JSON string ",
                          configuration->toString()));
    return false;
  }

  status = ::Wasm::Common::extractLocalNodeMetadata(&local_node_info_);
  if (status != Status::OK) {
    LOG_WARN("cannot parse local node metadata ");
    return false;
  }
  outbound_ = ::Wasm::Common::TrafficDirection::Outbound ==
              ::Wasm::Common::getTrafficDirection();

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
  use_host_header_fallback_ = !config_.disable_host_header_fallback();
  node_info_cache_.setMaxCacheSize(config_.max_peer_cache_size());

  auto field_separator = CONFIG_DEFAULT(field_separator);
  auto value_separator = CONFIG_DEFAULT(value_separator);
  auto stat_prefix = CONFIG_DEFAULT(stat_prefix);

  // prepend "_" to opt out of automatic namespacing
  // If "_" is not prepended, envoy_ is automatically added by prometheus
  // scraper"
  stat_prefix = absl::StrCat("_", stat_prefix, "_");

  Metric build(MetricType::Gauge, absl::StrCat(stat_prefix, "build"),
               {MetricTag{"component", MetricTag::TagType::String},
                MetricTag{"tag", MetricTag::TagType::String}});
  build.record(1, "proxy", absl::StrCat(local_node_info_.istio_version(), ";"));

  stats_ = std::vector<StatGen>{
      StatGen(
          absl::StrCat(stat_prefix, "requests_total"), MetricType::Counter,
          [](const ::Wasm::Common::RequestInfo&) -> uint64_t { return 1; },
          field_separator, value_separator),
      StatGen(
          absl::StrCat(stat_prefix, "request_duration_milliseconds"),
          MetricType::Histogram,
          [](const ::Wasm::Common::RequestInfo& request_info) -> uint64_t {
            return request_info.duration / absl::Milliseconds(1);
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
  return true;
}

void PluginRootContext::report() {
  const auto peer_node_ptr =
      node_info_cache_.getPeerById(peer_metadata_id_key_, peer_metadata_key_);
  const wasm::common::NodeInfo& peer_node =
      peer_node_ptr ? *peer_node_ptr : ::Wasm::Common::EmptyNodeInfo;

  // map and overwrite previous mapping.
  const auto& destination_node_info = outbound_ ? peer_node : local_node_info_;
  ::Wasm::Common::RequestInfo request_info;
  ::Wasm::Common::populateHTTPRequestInfo(outbound_, useHostHeaderFallback(),
                                          &request_info,
                                          destination_node_info.namespace_());
  istio_dimensions_.map(peer_node, request_info);

  auto stats_it = metrics_.find(istio_dimensions_);
  if (stats_it != metrics_.end()) {
    for (auto& stat : stats_it->second) {
      stat.record(request_info);
      LOG_DEBUG(absl::StrCat(
          "metricKey cache hit ", istio_dimensions_.debug_key(),
          ", stat=", stat.metric_id_, stats_it->first.to_string()));
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
    LOG_DEBUG(absl::StrCat("metricKey cache miss ", statgen.name(), " ",
                           istio_dimensions_.debug_key(),
                           ", stat=", stat.metric_id_));
    stat.record(request_info);
    stats.push_back(stat);
  }

  incrementMetric(cache_misses_, 1);
  // TODO: When we have c++17, convert to try_emplace.
  metrics_.emplace(istio_dimensions_, stats);
}

#ifdef NULL_PLUGIN
NullPluginRootRegistry* context_registry_{};

class StatsFactory : public NullVmPluginFactory {
 public:
  const std::string name() const override { return "envoy.wasm.stats"; }

  std::unique_ptr<NullVmPlugin> create() const override {
    return std::make_unique<NullPlugin>(context_registry_);
  }
};

static Registry::RegisterFactory<StatsFactory, NullVmPluginFactory> register_;
#endif

}  // namespace Stats

#ifdef NULL_PLUGIN
// WASM_EPILOG
}  // namespace Plugin
}  // namespace Null
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
#endif
