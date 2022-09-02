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

#pragma once

#include <optional>
#include <unordered_map>

#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "extensions/common/context.h"
#include "extensions/common/wasm/json_util.h"

// WASM_PROLOG
#ifndef NULL_PLUGIN
#include "proxy_wasm_intrinsics.h"

#else  // NULL_PLUGIN

#include "include/proxy-wasm/null_plugin.h"

namespace proxy_wasm {
namespace null_plugin {

#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace Stats {

template <typename K, typename V>
using Map = std::unordered_map<K, V>;

constexpr std::string_view Sep = "#@";

// The following need to be std::strings because the receiver expects a string.
const std::string unknown = "unknown";
const std::string source = "source";
const std::string destination = "destination";
const std::string vDash = "-";

const std::string default_field_separator = ";.;";
const std::string default_value_separator = "=.=";
const std::string default_stat_prefix = "istio";

// The order of the fields is important! The metrics indicate the cut-off line
// using an index.
#define STD_ISTIO_DIMENSIONS(FIELD_FUNC)     \
  FIELD_FUNC(reporter)                       \
  FIELD_FUNC(source_workload)                \
  FIELD_FUNC(source_workload_namespace)      \
  FIELD_FUNC(source_principal)               \
  FIELD_FUNC(source_app)                     \
  FIELD_FUNC(source_version)                 \
  FIELD_FUNC(source_canonical_service)       \
  FIELD_FUNC(source_canonical_revision)      \
  FIELD_FUNC(source_cluster)                 \
  FIELD_FUNC(destination_workload)           \
  FIELD_FUNC(destination_workload_namespace) \
  FIELD_FUNC(destination_principal)          \
  FIELD_FUNC(destination_app)                \
  FIELD_FUNC(destination_version)            \
  FIELD_FUNC(destination_service)            \
  FIELD_FUNC(destination_service_name)       \
  FIELD_FUNC(destination_service_namespace)  \
  FIELD_FUNC(destination_canonical_service)  \
  FIELD_FUNC(destination_canonical_revision) \
  FIELD_FUNC(destination_cluster)            \
  FIELD_FUNC(request_protocol)               \
  FIELD_FUNC(response_flags)                 \
  FIELD_FUNC(connection_security_policy)     \
  FIELD_FUNC(response_code)                  \
  FIELD_FUNC(grpc_response_status)

// Aggregate metric values in a shared and reusable bag.
using IstioDimensions = std::vector<std::string>;

enum class StandardLabels : int32_t {
#define DECLARE_LABEL(name) name,
  STD_ISTIO_DIMENSIONS(DECLARE_LABEL)
#undef DECLARE_LABEL
      xxx_last_metric
};

#define DECLARE_CONSTANT(name) \
  const int32_t name = static_cast<int32_t>(StandardLabels::name);
STD_ISTIO_DIMENSIONS(DECLARE_CONSTANT)
#undef DECLARE_CONSTANT

// All labels.
const size_t count_standard_labels =
    static_cast<size_t>(StandardLabels::xxx_last_metric);

// Labels related to peer information.
const size_t count_peer_labels =
    static_cast<size_t>(StandardLabels::destination_cluster) + 1;

// Labels related to TCP streams, including peer information.
const size_t count_tcp_labels =
    static_cast<size_t>(StandardLabels::connection_security_policy) + 1;

struct HashIstioDimensions {
  size_t operator()(const IstioDimensions& c) const {
    const size_t kMul = static_cast<size_t>(0x9ddfea08eb382d69);
    size_t h = 0;
    for (const auto& value : c) {
      h += std::hash<std::string>()(value) * kMul;
    }
    return h;
  }
};

// Value extractor can mutate the request info to flush data between multiple
// reports.
using ValueExtractorFn =
    std::function<uint64_t(::Wasm::Common::RequestInfo& request_info)>;

// SimpleStat record a pre-resolved metric based on the values function.
class SimpleStat {
 public:
  SimpleStat(uint32_t metric_id, ValueExtractorFn value_fn, MetricType type,
             bool recurrent)
      : metric_id_(metric_id),
        recurrent_(recurrent),
        value_fn_(value_fn),
        type_(type){};

  inline void record(::Wasm::Common::RequestInfo& request_info) {
    const uint64_t val = value_fn_(request_info);
    // Optimization: do not record 0 COUNTER values
    if (type_ == MetricType::Counter && val == 0) {
      return;
    }
    recordMetric(metric_id_, val);
  };

  const uint32_t metric_id_;
  const bool recurrent_;

 private:
  ValueExtractorFn value_fn_;
  MetricType type_;
};

// MetricFactory creates a stat generator given tags.
struct MetricFactory {
  std::string name;
  MetricType type;
  ValueExtractorFn extractor;
  uint32_t protocols;
  size_t count_labels;
  // True for metrics supporting reporting mid-stream.
  bool recurrent;
};

// StatGen creates a SimpleStat based on resolved metric_id.
class StatGen {
 public:
  explicit StatGen(const std::string& stat_prefix,
                   const MetricFactory& metric_factory,
                   const std::vector<MetricTag>& tags,
                   const std::vector<size_t>& indexes,
                   const std::string& field_separator,
                   const std::string& value_separator)
      : recurrent_(metric_factory.recurrent),
        protocols_(metric_factory.protocols),
        indexes_(indexes),
        extractor_(metric_factory.extractor),
        metric_(metric_factory.type,
                absl::StrCat(stat_prefix, metric_factory.name), tags,
                field_separator, value_separator) {
    if (tags.size() != indexes.size()) {
      logAbort("metric tags.size() != indexes.size()");
    }
  };

  StatGen() = delete;
  inline std::string_view name() const { return metric_.name; };
  inline bool matchesProtocol(::Wasm::Common::Protocol protocol) const {
    return (protocols_ & static_cast<uint32_t>(protocol)) != 0;
  }

  // Resolve metric based on provided dimension values by
  // combining the tags with the indexed dimensions and resolving
  // to a metric ID.
  SimpleStat resolve(const IstioDimensions& instance) {
    // Using a lower level API to avoid creating an intermediary vector
    size_t s = metric_.prefix.size();
    for (const auto& tag : metric_.tags) {
      s += tag.name.size() + metric_.value_separator.size();
    }
    for (size_t i : indexes_) {
      s += instance[i].size() + metric_.field_separator.size();
    }
    s += metric_.name.size();

    std::string n;
    n.reserve(s);
    n.append(metric_.prefix);
    for (size_t i = 0; i < metric_.tags.size(); i++) {
      n.append(metric_.tags[i].name);
      n.append(metric_.value_separator);
      n.append(instance[indexes_[i]]);
      n.append(metric_.field_separator);
    }
    n.append(metric_.name);
    auto metric_id = metric_.resolveFullName(n);
    return SimpleStat(metric_id, extractor_, metric_.type, recurrent_);
  };

  const bool recurrent_;

 private:
  const uint32_t protocols_;
  const std::vector<size_t> indexes_;
  const ValueExtractorFn extractor_;
  Metric metric_;
};

enum class MetadataMode : uint8_t {
  kLocalNodeMetadataMode = 0,
  kHostMetadataMode = 1,
  kClusterMetadataMode = 2,
};

// PluginRootContext is the root context for all streams processed by the
// thread. It has the same lifetime as the worker thread and acts as target
// for interactions that outlives individual stream, e.g. timer, async calls.
class PluginRootContext : public RootContext {
 public:
  PluginRootContext(uint32_t id, std::string_view root_id, bool is_outbound)
      : RootContext(id, root_id), outbound_(is_outbound) {
    Metric cache_count(MetricType::Counter, "metric_cache_count",
                       {MetricTag{"wasm_filter", MetricTag::TagType::String},
                        MetricTag{"cache", MetricTag::TagType::String}});
    cache_hits_ = cache_count.resolve("stats_filter", "hit");
    cache_misses_ = cache_count.resolve("stats_filter", "miss");
    empty_node_info_ = ::Wasm::Common::extractEmptyNodeFlatBuffer();
    if (outbound_) {
      peer_metadata_id_key_ = ::Wasm::Common::kUpstreamMetadataIdKey;
      peer_metadata_key_ = ::Wasm::Common::kUpstreamMetadataKey;
    } else {
      peer_metadata_id_key_ = ::Wasm::Common::kDownstreamMetadataIdKey;
      peer_metadata_key_ = ::Wasm::Common::kDownstreamMetadataKey;
    }
  }

  ~PluginRootContext() = default;

  bool onConfigure(size_t) override;
  bool configure(size_t);
  bool onDone() override;
  void onTick() override;
  void report(::Wasm::Common::RequestInfo& request_info, bool end_stream);
  bool useHostHeaderFallback() const { return use_host_header_fallback_; };
  void addToRequestQueue(uint32_t context_id,
                         ::Wasm::Common::RequestInfo* request_info);
  void deleteFromRequestQueue(uint32_t context_id);

 protected:
  const std::vector<MetricTag>& defaultTags();
  const std::vector<MetricFactory>& defaultMetrics();
  // Update the dimensions and the expressions data structures with the new
  // configuration.
  bool initializeDimensions(const ::nlohmann::json& j);
  // Destroy host resources for the allocated expressions.
  void cleanupExpressions();
  // Allocate an expression if necessary and return its token position.
  std::optional<size_t> addStringExpression(const std::string& input);
  // Allocate an int expression and return its token if successful.
  std::optional<uint32_t> addIntExpression(const std::string& input);

 private:
  flatbuffers::DetachedBuffer local_node_info_;
  flatbuffers::DetachedBuffer empty_node_info_;

  IstioDimensions istio_dimensions_;

  struct expressionInfo {
    uint32_t token;
    std::string expression;
  };
  // String expressions evaluated into dimensions
  std::vector<struct expressionInfo> expressions_;
  Map<std::string, size_t> input_expressions_;

  // Int expressions evaluated to metric values
  std::vector<uint32_t> int_expressions_;

  const bool outbound_;
  std::string_view peer_metadata_id_key_;
  std::string_view peer_metadata_key_;
  bool use_host_header_fallback_;
  MetadataMode metadata_mode_;

  int64_t cache_hits_accumulator_ = 0;
  uint32_t cache_hits_;
  uint32_t cache_misses_;

  // Resolved metric where value can be recorded.
  // Maps resolved dimensions to a set of related metrics.
  std::unordered_map<IstioDimensions, std::vector<SimpleStat>,
                     HashIstioDimensions>
      metrics_;
  Map<uint32_t, ::Wasm::Common::RequestInfo*> request_queue_;
  // Peer stats to be generated for a dimensioned metrics set.
  std::vector<StatGen> stats_;
  bool initialized_ = false;
};

class PluginRootContextOutbound : public PluginRootContext {
 public:
  PluginRootContextOutbound(uint32_t id, std::string_view root_id)
      : PluginRootContext(id, root_id, /* is outbound */ true){};
};

class PluginRootContextInbound : public PluginRootContext {
 public:
  PluginRootContextInbound(uint32_t id, std::string_view root_id)
      : PluginRootContext(id, root_id, /* is outbound */ false){};
};

// Per-stream context.
class PluginContext : public Context {
 public:
  explicit PluginContext(uint32_t id, RootContext* root) : Context(id, root) {}

  // Called for both HTTP and TCP streams, as a final data callback.
  void onLog() override {
    rootContext()->deleteFromRequestQueue(id());
    if (request_info_.request_protocol == ::Wasm::Common::Protocol::TCP) {
      request_info_.tcp_connections_closed++;
    }
    rootContext()->report(request_info_, true);
  };

  // HTTP streams start with headers.
  FilterHeadersStatus onRequestHeaders(uint32_t, bool) override {
    ::Wasm::Common::populateRequestProtocol(&request_info_);
    // Save host value for recurrent reporting.
    // Beware that url_host and any other request headers are only available in
    // this callback and onLog(), certainly not in onTick().
    if (rootContext()->useHostHeaderFallback()) {
      getValue({"request", "host"}, &request_info_.url_host);
    }
    return FilterHeadersStatus::Continue;
  }

  // Metadata should be available (if any) at the time of adding to the queue.
  // Since HTTP metadata exchange uses headers in both directions, this is a
  // safe place to register for both inbound and outbound streams.
  FilterHeadersStatus onResponseHeaders(uint32_t, bool) override {
    rootContext()->addToRequestQueue(id(), &request_info_);
    return FilterHeadersStatus::Continue;
  }

  // TCP streams start with new connections.
  FilterStatus onNewConnection() override {
    request_info_.request_protocol = ::Wasm::Common::Protocol::TCP;
    request_info_.tcp_connections_opened++;
    rootContext()->addToRequestQueue(id(), &request_info_);
    return FilterStatus::Continue;
  }

  // Called on onData call, so counting the data that is received.
  FilterStatus onDownstreamData(size_t size, bool) override {
    request_info_.tcp_received_bytes += size;
    return FilterStatus::Continue;
  }
  // Called on onWrite call, so counting the data that is sent.
  FilterStatus onUpstreamData(size_t size, bool) override {
    request_info_.tcp_sent_bytes += size;
    return FilterStatus::Continue;
  }

 private:
  inline PluginRootContext* rootContext() {
    return dynamic_cast<PluginRootContext*>(this->root());
  };

  ::Wasm::Common::RequestInfo request_info_;
};

#ifdef NULL_PLUGIN
PROXY_WASM_NULL_PLUGIN_REGISTRY;
#endif

static RegisterContextFactory register_StatsOutbound(
    CONTEXT_FACTORY(Stats::PluginContext),
    ROOT_FACTORY(Stats::PluginRootContextOutbound), "stats_outbound");

static RegisterContextFactory register_StatsInbound(
    CONTEXT_FACTORY(Stats::PluginContext),
    ROOT_FACTORY(Stats::PluginRootContextInbound), "stats_inbound");

}  // namespace Stats

// WASM_EPILOG
#ifdef NULL_PLUGIN
}  // namespace null_plugin
}  // namespace proxy_wasm
#endif
