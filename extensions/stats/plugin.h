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

#include <unordered_map>

#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "extensions/common/context.h"
#include "extensions/common/node_info.pb.h"
#include "extensions/common/node_info_cache.h"
#include "extensions/stats/config.pb.h"
#include "google/protobuf/util/json_util.h"

// WASM_PROLOG
#ifndef NULL_PLUGIN
#include "proxy_wasm_intrinsics.h"

#else  // NULL_PLUGIN

#include "extensions/common/wasm/null/null_plugin.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {

using WasmResult = Envoy::Extensions::Common::Wasm::WasmResult;
using NullPluginRegistry =
    ::Envoy::Extensions::Common::Wasm::Null::NullPluginRegistry;
using Envoy::Extensions::Common::Wasm::Null::Plugin::FilterStatus;

#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace Stats {

using StringView = absl::string_view;

constexpr StringView Sep = "#@";

// The following need to be std::strings because the receiver expects a string.
const std::string unknown = "unknown";
const std::string source = "source";
const std::string destination = "destination";
const std::string vDash = "-";

const std::string default_field_separator = ";.;";
const std::string default_value_separator = "=.=";
const std::string default_stat_prefix = "istio";

using google::protobuf::util::JsonParseOptions;
using google::protobuf::util::Status;

#define CONFIG_DEFAULT(name) \
  config_.name().empty() ? default_##name : config_.name()

#define STD_ISTIO_DIMENSIONS(FIELD_FUNC)     \
  FIELD_FUNC(reporter)                       \
  FIELD_FUNC(source_workload)                \
  FIELD_FUNC(source_workload_namespace)      \
  FIELD_FUNC(source_principal)               \
  FIELD_FUNC(source_app)                     \
  FIELD_FUNC(source_version)                 \
  FIELD_FUNC(destination_workload)           \
  FIELD_FUNC(destination_workload_namespace) \
  FIELD_FUNC(destination_principal)          \
  FIELD_FUNC(destination_app)                \
  FIELD_FUNC(destination_version)            \
  FIELD_FUNC(destination_service)            \
  FIELD_FUNC(destination_service_name)       \
  FIELD_FUNC(destination_service_namespace)  \
  FIELD_FUNC(destination_port)               \
  FIELD_FUNC(request_protocol)               \
  FIELD_FUNC(response_code)                  \
  FIELD_FUNC(grpc_response_status)           \
  FIELD_FUNC(response_flags)                 \
  FIELD_FUNC(connection_security_policy)     \
  FIELD_FUNC(permissive_response_code)       \
  FIELD_FUNC(permissive_response_policyid)

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

const int32_t count_standard_labels =
    static_cast<int32_t>(StandardLabels::xxx_last_metric);

// Ordered dimension list is used by the metrics API.
static std::vector<MetricTag> StandardLabels() {
#define DEFINE_METRIC_TAG(name) {#name, MetricTag::TagType::String},
  return std::vector<MetricTag>{STD_ISTIO_DIMENSIONS(DEFINE_METRIC_TAG)};
#undef DEFINE_METRIC_TAG
}

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

using ValueExtractorFn =
    uint64_t (*)(const ::Wasm::Common::RequestInfo& request_info);

// SimpleStat record a pre-resolved metric based on the values function.
class SimpleStat {
 public:
  SimpleStat(uint32_t metric_id, ValueExtractorFn value_fn)
      : metric_id_(metric_id), value_fn_(value_fn){};

  inline void record(const ::Wasm::Common::RequestInfo& request_info) {
    recordMetric(metric_id_, value_fn_(request_info));
  };

  uint32_t metric_id_;

 private:
  ValueExtractorFn value_fn_;
};

// StatFactory creates a stat generator given tags.
struct StatFactory {
  std::string name;
  MetricType type;
  ValueExtractorFn extractor;
  bool is_tcp;
};

static std::vector<StatFactory> StandardStats() {
  return {
      // HTTP, HTTP/2, and GRPC metrics
      StatFactory{
          "requests_total", MetricType::Counter,

          [](const ::Wasm::Common::RequestInfo&) -> uint64_t { return 1; },
          false},
      StatFactory{"request_duration_milliseconds", MetricType::Histogram,
                  [](const ::Wasm::Common::RequestInfo& request_info)
                      -> uint64_t { return request_info.duration / 1000; },
                  false},
      StatFactory{"request_bytes", MetricType::Histogram,

                  [](const ::Wasm::Common::RequestInfo& request_info)
                      -> uint64_t { return request_info.request_size; },
                  false},
      StatFactory{"response_bytes", MetricType::Histogram,

                  [](const ::Wasm::Common::RequestInfo& request_info)
                      -> uint64_t { return request_info.response_size; },
                  false},
      // TCP metrics.
      StatFactory{"tcp_sent_bytes_total", MetricType::Counter,
                  [](const ::Wasm::Common::RequestInfo& request_info)
                      -> uint64_t { return request_info.tcp_sent_bytes; },
                  true},
      StatFactory{"tcp_received_bytes_total", MetricType::Counter,
                  [](const ::Wasm::Common::RequestInfo& request_info)
                      -> uint64_t { return request_info.tcp_received_bytes; },
                  true},
      StatFactory{
          "tcp_connections_opened_total", MetricType::Counter,
          [](const ::Wasm::Common::RequestInfo& request_info) -> uint64_t {
            return request_info.tcp_connections_opened;
          },
          true},
      StatFactory{
          "tcp_connections_closed_total", MetricType::Counter,
          [](const ::Wasm::Common::RequestInfo& request_info) -> uint64_t {
            return request_info.tcp_connections_closed;
          },
          true},
  };
}

// StatGen creates a SimpleStat based on resolved metric_id.
class StatGen {
 public:
  explicit StatGen(const std::string& stat_prefix,
                   const StatFactory& metric_factory,
                   const std::vector<MetricTag>& tags,
                   const std::string& field_separator,
                   const std::string& value_separator)
      : metric_factory_(metric_factory),
        metric_(metric_factory_.type,
                absl::StrCat(stat_prefix, metric_factory_.name), tags,
                field_separator, value_separator){};

  StatGen() = delete;
  inline StringView name() const { return metric_factory_.name; };
  inline bool is_tcp_metric() const { return metric_factory_.is_tcp; }

  // Resolve metric based on provided dimension values.
  SimpleStat resolve(std::vector<std::string>& vals) {
    auto metric_id = metric_.resolveWithFields(vals);
    return SimpleStat(metric_id, metric_factory_.extractor);
  };

 private:
  StatFactory metric_factory_;
  Metric metric_;
};

// PluginRootContext is the root context for all streams processed by the
// thread. It has the same lifetime as the worker thread and acts as target
// for interactions that outlives individual stream, e.g. timer, async calls.
class PluginRootContext : public RootContext {
 public:
  PluginRootContext(uint32_t id, StringView root_id)
      : RootContext(id, root_id) {
    Metric cache_count(MetricType::Counter, "statsfilter",
                       {MetricTag{"cache", MetricTag::TagType::String}});
    cache_hits_ = cache_count.resolve("hit");
    cache_misses_ = cache_count.resolve("miss");
  }

  ~PluginRootContext() = default;

  bool onConfigure(size_t) override;
  bool onDone() override;
  void onTick() override;
  // Report will return false when peer metadata exchange is not found for TCP,
  // so that we wait to report metrics till we find peer metadata or get
  // information that it's not available.
  bool report(::Wasm::Common::RequestInfo& request_info, bool is_tcp);
  bool outbound() const { return outbound_; }
  bool useHostHeaderFallback() const { return use_host_header_fallback_; };
  void addToTCPRequestQueue(
      uint32_t id, std::shared_ptr<::Wasm::Common::RequestInfo> request_info);
  void deleteFromTCPRequestQueue(uint32_t id);

 protected:
  // Update the dimensions and the expressions data structures with the new
  // configuration.
  void initializeDimensions();
  // Destroy host resources for the allocated expressions.
  void cleanupExpressions();

 private:
  stats::PluginConfig config_;
  wasm::common::NodeInfo local_node_info_;
  ::Wasm::Common::NodeInfoCache node_info_cache_;

  IstioDimensions istio_dimensions_;
  std::vector<uint32_t> expressions_;
  std::vector<MetricTag> tags_;

  StringView peer_metadata_id_key_;
  StringView peer_metadata_key_;
  bool outbound_;
  bool debug_;
  bool use_host_header_fallback_;

  int64_t cache_hits_accumulator_ = 0;
  uint32_t cache_hits_;
  uint32_t cache_misses_;

  // Resolved metric where value can be recorded.
  // Maps resolved dimensions to a set of related metrics.
  std::unordered_map<IstioDimensions, std::vector<SimpleStat>,
                     HashIstioDimensions>
      metrics_;
  std::unordered_map<uint32_t, std::shared_ptr<::Wasm::Common::RequestInfo>>
      tcp_request_queue_;
  // Peer stats to be generated for a dimensioned metrics set.
  std::vector<StatGen> stats_;
};

class PluginRootContextOutbound : public PluginRootContext {
 public:
  PluginRootContextOutbound(uint32_t id, StringView root_id)
      : PluginRootContext(id, root_id){};
};

class PluginRootContextInbound : public PluginRootContext {
 public:
  PluginRootContextInbound(uint32_t id, StringView root_id)
      : PluginRootContext(id, root_id){};
};

// Per-stream context.
class PluginContext : public Context {
 public:
  explicit PluginContext(uint32_t id, RootContext* root)
      : Context(id, root),
        upstream_closed_(false),
        downstream_closed_(false),
        tcp_connection_closed_(false),
        context_id_(id) {
    request_info_ = std::make_shared<::Wasm::Common::RequestInfo>();
  }

  void onLog() override { rootContext()->report(*request_info_, false); };

  FilterStatus onNewConnection() override {
    request_info_->tcp_connections_opened++;
    rootContext()->addToTCPRequestQueue(context_id_, request_info_);
    return FilterStatus::Continue;
  }

  // Called on onData call, so counting the data that is received.
  FilterStatus onDownstreamData(size_t size, bool) override {
    request_info_->tcp_received_bytes += size;
    return FilterStatus::Continue;
  }
  // Called on onWrite call, so counting the data that is sent.
  FilterStatus onUpstreamData(size_t size, bool) override {
    request_info_->tcp_sent_bytes += size;
    return FilterStatus::Continue;
  }

  void onDownstreamConnectionClose(PeerType) override {
    downstream_closed_ = true;
    if (upstream_closed_ && !tcp_connection_closed_) {
      logTCPOnClose();
    }
  }
  void onUpstreamConnectionClose(PeerType) override {
    upstream_closed_ = true;
    if (downstream_closed_ && !tcp_connection_closed_) {
      logTCPOnClose();
    }
  }

 private:
  inline PluginRootContext* rootContext() {
    return dynamic_cast<PluginRootContext*>(this->root());
  };

  void logTCPOnClose() {
    tcp_connection_closed_ = true;
    rootContext()->deleteFromTCPRequestQueue(context_id_);
    request_info_->tcp_connections_closed++;
    rootContext()->report(*request_info_, true);
  }

  bool upstream_closed_;
  bool downstream_closed_;
  bool tcp_connection_closed_;
  uint32_t context_id_;
  std::shared_ptr<::Wasm::Common::RequestInfo> request_info_;
};

#ifdef NULL_PLUGIN
NULL_PLUGIN_REGISTRY;
#endif

static RegisterContextFactory register_Stats(
    CONTEXT_FACTORY(Stats::PluginContext),
    ROOT_FACTORY(Stats::PluginRootContext));

static RegisterContextFactory register_StatsOutbound(
    CONTEXT_FACTORY(Stats::PluginContext),
    ROOT_FACTORY(Stats::PluginRootContextOutbound), "stats_outbound");

static RegisterContextFactory register_StatsInbound(
    CONTEXT_FACTORY(Stats::PluginContext),
    ROOT_FACTORY(Stats::PluginRootContextInbound), "stats_inbound");

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
