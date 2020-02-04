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

#include "extensions/stats/proxy_expr.h"
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

constexpr long long kDefaultTCPReportDurationNanoseconds = 15000000000;  // 15s

namespace {

void clearTcpMetrics(::Wasm::Common::RequestInfo& request_info) {
  request_info.tcp_connections_opened = 0;
  request_info.tcp_sent_bytes = 0;
  request_info.tcp_received_bytes = 0;
}

}  // namespace

void PluginRootContext::initializeDimensions() {
  // Metric tags
  std::vector<MetricTag> tags = IstioDimensions::defaultTags();
  if (!config_.metrics(0).dimensions().empty()) {
    tags.reserve(tags.size() + config_.metrics(0).dimensions().size());
    expressions_.reserve(config_.metrics(0).dimensions().size());
    for (const auto& dim : config_.metrics(0).dimensions()) {
      uint32_t token = 0;
      if (createExpression(dim.second, &token) != WasmResult::Ok) {
        LOG_WARN(
            absl::StrCat("Cannot create a new tag dimension: ", dim.first));
        continue;
      }
      tags.push_back({dim.first, MetricTag::TagType::String});
      expressions_.push_back(token);
    }
  }

  // Local data does not change, so populate it on config load.
  istio_dimensions_.init(outbound_, local_node_info_, expressions_.size());
}

bool PluginRootContext::onConfigure(size_t) {
  std::unique_ptr<WasmData> configuration = getConfiguration();
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

  initializeDimensions();

  stats_ = std::vector<StatGen>{
      // HTTP, HTTP/2, and GRPC metrics
      StatGen(
          absl::StrCat(stat_prefix, "requests_total"), MetricType::Counter,
          tags,
          [](const ::Wasm::Common::RequestInfo&) -> uint64_t { return 1; },
          field_separator, value_separator, /*is_tcp_metric=*/false),
      StatGen(
          absl::StrCat(stat_prefix, "request_duration_milliseconds"),
          MetricType::Histogram, tags,
          [](const ::Wasm::Common::RequestInfo& request_info) -> uint64_t {
            return request_info.duration / 1000;
          },
          field_separator, value_separator, /*is_tcp_metric=*/false),
      StatGen(
          absl::StrCat(stat_prefix, "request_bytes"), MetricType::Histogram,
          tags,
          [](const ::Wasm::Common::RequestInfo& request_info) -> uint64_t {
            return request_info.request_size;
          },
          field_separator, value_separator, /*is_tcp_metric=*/false),
      StatGen(
          absl::StrCat(stat_prefix, "response_bytes"), MetricType::Histogram,
          tags,
          [](const ::Wasm::Common::RequestInfo& request_info) -> uint64_t {
            return request_info.response_size;
          },
          field_separator, value_separator, /*is_tcp_metric=*/false),
      // TCP metrics.
      StatGen(
          absl::StrCat(stat_prefix, "tcp_sent_bytes_total"),
          MetricType::Counter, tags,
          [](const ::Wasm::Common::RequestInfo& request_info) -> uint64_t {
            return request_info.tcp_sent_bytes;
          },
          field_separator, value_separator, /*is_tcp_metric=*/true),
      StatGen(
          absl::StrCat(stat_prefix, "tcp_received_bytes_total"),
          MetricType::Counter, tags,
          [](const ::Wasm::Common::RequestInfo& request_info) -> uint64_t {
            return request_info.tcp_received_bytes;
          },
          field_separator, value_separator, /*is_tcp_metric=*/true),
      StatGen(
          absl::StrCat(stat_prefix, "tcp_connections_opened_total"),
          MetricType::Counter, tags,
          [](const ::Wasm::Common::RequestInfo& request_info) -> uint64_t {
            return request_info.tcp_connections_opened;
          },
          field_separator, value_separator, /*is_tcp_metric=*/true),
      StatGen(
          absl::StrCat(stat_prefix, "tcp_connections_closed_total"),
          MetricType::Counter, tags,
          [](const ::Wasm::Common::RequestInfo& request_info) -> uint64_t {
            return request_info.tcp_connections_closed;
          },
          field_separator, value_separator, /*is_tcp_metric=*/true),
  };

  long long tcp_report_duration_nanos = kDefaultTCPReportDurationNanoseconds;
  if (config_.has_tcp_reporting_duration()) {
    tcp_report_duration_nanos =
        ::google::protobuf::util::TimeUtil::DurationToNanoseconds(
            config_.tcp_reporting_duration());
  }
  proxy_set_tick_period_milliseconds(tcp_report_duration_nanos);
  return true;
}

bool PluginRootContext::onDone() {
  for (uint32_t token : expressions_) {
    exprDelete(token);
  }
  return true;
}

void PluginRootContext::onTick() {
  if (tcp_request_queue_.size() < 1) {
    return;
  }
  for (auto const& item : tcp_request_queue_) {
    // requestinfo is null, so continue.
    if (item.second == nullptr) {
      continue;
    }
    if (report(*item.second, true)) {
      // Clear existing data in TCP metrics, so that we don't double count the
      // metrics.
      clearTcpMetrics(*item.second);
    }
  }
}

bool PluginRootContext::report(::Wasm::Common::RequestInfo& request_info,
                               bool is_tcp) {
  std::string peer_id;
  const auto peer_node_ptr = node_info_cache_.getPeerById(
      peer_metadata_id_key_, peer_metadata_key_, peer_id);

  const wasm::common::NodeInfo& peer_node =
      peer_node_ptr ? *peer_node_ptr : ::Wasm::Common::EmptyNodeInfo;

  // map and overwrite previous mapping.
  const auto& destination_node_info = outbound_ ? peer_node : local_node_info_;

  if (is_tcp) {
    // For TCP, if peer metadata is not available, peer id is set as not found.
    // Otherwise, we wait for metadata exchange to happen before we report  any
    // metric.
    // TODO(gargnupur): Remove outbound_ from condition below, when
    // https://github.com/envoyproxy/envoy-wasm/issues/291 is fixed.
    if (peer_node_ptr == nullptr &&
        peer_id != ::Wasm::Common::kMetadataNotFoundValue && !outbound_) {
      return false;
    }
    if (!request_info.is_populated) {
      ::Wasm::Common::populateTCPRequestInfo(
          outbound_, &request_info, destination_node_info.namespace_());
    }
  } else {
    ::Wasm::Common::populateHTTPRequestInfo(outbound_, useHostHeaderFallback(),
                                            &request_info,
                                            destination_node_info.namespace_());
  }

  istio_dimensions_.map(peer_node, request_info);
  for (size_t i = 0; i < expressions_.size(); i++) {
    evaluateExpression(expressions_[i], &istio_dimensions_.custom_values.at(i));
  }

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
    return true;
  }

  // fetch dimensions in the required form for resolve.
  auto values = istio_dimensions_.values();

  std::vector<SimpleStat> stats;
  for (auto& statgen : stats_) {
    if (statgen.is_tcp_metric() != is_tcp) {
      continue;
    }
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
  return true;
}

void PluginRootContext::addToTCPRequestQueue(
    uint32_t id, std::shared_ptr<::Wasm::Common::RequestInfo> request_info) {
  tcp_request_queue_[id] = request_info;
}

void PluginRootContext::deleteFromTCPRequestQueue(uint32_t id) {
  tcp_request_queue_.erase(id);
}

#ifdef NULL_PLUGIN
NullPluginRegistry* context_registry_{};

class StatsFactory : public NullVmPluginFactory {
 public:
  std::string name() const override { return "envoy.wasm.stats"; }

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
