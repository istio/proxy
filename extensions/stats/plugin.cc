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

void map_node(IstioDimensions& instance, bool is_source,
              const wasm::common::NodeInfo& node) {
  if (is_source) {
    instance[source_workload] = node.workload_name();
    instance[source_workload_namespace] = node.namespace_();

    auto source_labels = node.labels();
    instance[source_app] = source_labels["app"];
    instance[source_version] = source_labels["version"];
  } else {
    instance[destination_workload] = node.workload_name();
    instance[destination_workload_namespace] = node.namespace_();

    auto destination_labels = node.labels();
    instance[destination_app] = destination_labels["app"];
    instance[destination_version] = destination_labels["version"];

    instance[destination_service_namespace] = node.namespace_();
  }
}

// Called during request processing.
void map_peer(IstioDimensions& instance, bool outbound,
              const wasm::common::NodeInfo& peer_node) {
  map_node(instance, !outbound, peer_node);
}

void map_unknown_if_empty(IstioDimensions& instance) {
#define SET_IF_EMPTY(name)      \
  if (instance[name].empty()) { \
    instance[name] = unknown;   \
  }
  STD_ISTIO_DIMENSIONS(SET_IF_EMPTY)
#undef SET_IF_EMPTY
}

// maps from request context to dimensions.
// local node derived dimensions are already filled in.
void map_request(IstioDimensions& instance,
                 const ::Wasm::Common::RequestInfo& request) {
  instance[source_principal] = request.source_principal;
  instance[destination_principal] = request.destination_principal;
  instance[destination_service] = request.destination_service_host;
  instance[destination_service_name] = request.destination_service_name;
  instance[destination_port] = std::to_string(request.destination_port);
  instance[request_protocol] = request.request_protocol;
  instance[response_code] = std::to_string(request.response_code);
  instance[response_flags] = request.response_flag;
  instance[connection_security_policy] = std::string(
      ::Wasm::Common::AuthenticationPolicyString(request.service_auth_policy));
  instance[permissive_response_code] =
      request.rbac_permissive_engine_result.empty()
          ? "none"
          : request.rbac_permissive_engine_result;
  instance[permissive_response_policyid] =
      request.rbac_permissive_policy_id.empty()
          ? "none"
          : request.rbac_permissive_policy_id;
}

// maps peer_node and request to dimensions.
void map(IstioDimensions& instance, bool outbound,
         const wasm::common::NodeInfo& peer_node,
         const ::Wasm::Common::RequestInfo& request) {
  map_peer(instance, outbound, peer_node);
  map_request(instance, request);
  map_unknown_if_empty(instance);
  if (request.request_protocol == "grpc") {
    instance[grpc_response_status] = std::to_string(request.grpc_status);
  } else {
    instance[grpc_response_status] = "";
  }
}

void clearTcpMetrics(::Wasm::Common::RequestInfo& request_info) {
  request_info.tcp_connections_opened = 0;
  request_info.tcp_sent_bytes = 0;
  request_info.tcp_received_bytes = 0;
}

}  // namespace

void PluginRootContext::initializeDimensions() {
  // Clean-up existing expressions.
  cleanupExpressions();

  // Seed the common metric tags with the default set.
  std::vector<MetricFactory> factories = DefaultMetrics();
  Map<std::string, std::vector<MetricTag>> metric_tags;
  Map<std::string, Map<std::string, Optional<size_t>>> metric_indexes;
  std::vector<MetricTag> default_tags = DefaultLabels();
  for (const auto& factory : factories) {
    metric_tags[factory.name] = default_tags;
    for (size_t i = 0; i < count_standard_labels; i++) {
      metric_indexes[factory.name][default_tags[i].name] = i;
    }
  }

  // Process the dimension overrides.
  for (const auto& metric : config_.metrics()) {
    // sort tag override tags
    std::vector<std::string> tags;
    const auto size = metric.dimensions().size();
    tags.reserve(size);
    for (const auto& dim : metric.dimensions()) {
      tags.push_back(dim.first);
    }
    std::sort(tags.begin(), tags.end());

    for (const auto& factory : factories) {
      if (!metric.name().empty() && metric.name() != factory.name) {
        continue;
      }
      auto& indexes = metric_indexes[factory.name];
      // Process tag deletions.
      for (const auto& tag : metric.tags_to_remove()) {
        auto it = indexes.find(tag);
        if (it != indexes.end()) {
          it->second = {};
        }
      }
      // Process tag overrides.
      for (const auto& tag : tags) {
        auto expr_index = addExpression(metric.dimensions().at(tag));
        auto it = indexes.find(tag);
        if (it != indexes.end()) {
          it->second = expr_index;
        } else {
          metric_tags[factory.name].push_back(
              {tag, MetricTag::TagType::String});
          indexes[tag] = count_standard_labels + expr_index.value();
        }
      }
    }
  }

  // Local data does not change, so populate it on config load.
  istio_dimensions_.resize(count_standard_labels + expressions_.size());
  istio_dimensions_[reporter] = outbound_ ? source : destination;
  map_node(istio_dimensions_, outbound_, local_node_info_);

  // Instantiate stat factories using the new dimensions
  auto field_separator = CONFIG_DEFAULT(field_separator);
  auto value_separator = CONFIG_DEFAULT(value_separator);
  auto stat_prefix = CONFIG_DEFAULT(stat_prefix);

  // prepend "_" to opt out of automatic namespacing
  // If "_" is not prepended, envoy_ is automatically added by prometheus
  // scraper"
  stat_prefix = absl::StrCat("_", stat_prefix, "_");

  stats_ = std::vector<StatGen>();
  std::vector<MetricTag> tags;
  std::vector<size_t> indexes;
  for (const auto& factory : factories) {
    tags.clear();
    indexes.clear();
    size_t size = metric_tags[factory.name].size();
    tags.reserve(size);
    indexes.reserve(size);
    for (const auto& tag : metric_tags[factory.name]) {
      auto index = metric_indexes[factory.name][tag.name];
      if (index.has_value()) {
        tags.push_back(tag);
        indexes.push_back(index.value());
      }
    }
    stats_.emplace_back(stat_prefix, factory, tags, indexes, field_separator,
                        value_separator);
  }

  Metric build(MetricType::Gauge, absl::StrCat(stat_prefix, "build"),
               {MetricTag{"component", MetricTag::TagType::String},
                MetricTag{"tag", MetricTag::TagType::String}});
  build.record(1, "proxy", absl::StrCat(local_node_info_.istio_version(), ";"));
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

  initializeDimensions();

  long long tcp_report_duration_nanos = kDefaultTCPReportDurationNanoseconds;
  if (config_.has_tcp_reporting_duration()) {
    tcp_report_duration_nanos =
        ::google::protobuf::util::TimeUtil::DurationToNanoseconds(
            config_.tcp_reporting_duration());
  }
  proxy_set_tick_period_milliseconds(tcp_report_duration_nanos);

  return true;
}

void PluginRootContext::cleanupExpressions() {
  for (uint32_t token : expressions_) {
    exprDelete(token);
  }
  expressions_.clear();
  input_expressions_.clear();
}

Optional<size_t> PluginRootContext::addExpression(const std::string& input) {
  auto it = input_expressions_.find(input);
  if (it == input_expressions_.end()) {
    uint32_t token = 0;
    if (createExpression(input, &token) != WasmResult::Ok) {
      LOG_WARN(absl::StrCat("Cannot create an expression: " + input));
      return {};
    }
    size_t result = expressions_.size();
    input_expressions_[input] = result;
    expressions_.push_back(token);
    return result;
  }
  return it->second;
}

bool PluginRootContext::onDone() {
  cleanupExpressions();
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

  map(istio_dimensions_, outbound_, peer_node, request_info);
  for (size_t i = 0; i < expressions_.size(); i++) {
    evaluateExpression(expressions_[i],
                       &istio_dimensions_.at(count_standard_labels + i));
  }

  auto stats_it = metrics_.find(istio_dimensions_);
  if (stats_it != metrics_.end()) {
    for (auto& stat : stats_it->second) {
      stat.record(request_info);
      LOG_DEBUG(
          absl::StrCat("metricKey cache hit ", ", stat=", stat.metric_id_));
    }
    cache_hits_accumulator_++;
    if (cache_hits_accumulator_ == 100) {
      incrementMetric(cache_hits_, cache_hits_accumulator_);
      cache_hits_accumulator_ = 0;
    }
    return true;
  }

  std::vector<SimpleStat> stats;
  for (auto& statgen : stats_) {
    if (statgen.is_tcp_metric() != is_tcp) {
      continue;
    }
    auto stat = statgen.resolve(istio_dimensions_);
    LOG_DEBUG(absl::StrCat("metricKey cache miss ", statgen.name(), " ",
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
