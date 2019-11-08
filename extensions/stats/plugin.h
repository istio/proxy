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

using MetadataType = Envoy::Extensions::Common::Wasm::MetadataType;
using WasmResult = Envoy::Extensions::Common::Wasm::WasmResult;
using NullPluginRootRegistry =
    ::Envoy::Extensions::Common::Wasm::Null::NullPluginRootRegistry;

#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace Stats {

using StringView = absl::string_view;

constexpr StringView Sep = "#@";

// The following need to be std::strings because the receiver expects a string.
const std::string unknown = "unknown";
const std::string vSource = "source";
const std::string vDest = "destination";
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
  FIELD_FUNC(request_protocol)               \
  FIELD_FUNC(response_code)                  \
  FIELD_FUNC(response_flags)                 \
  FIELD_FUNC(connection_security_policy)     \
  FIELD_FUNC(permissive_response_code)       \
  FIELD_FUNC(permissive_response_policyid)

struct IstioDimensions {
#define DEFINE_FIELD(name) std::string(name);
  STD_ISTIO_DIMENSIONS(DEFINE_FIELD)
#undef DEFINE_FIELD

  // utility fields
  bool outbound = false;

  // Ordered dimension list is used by the metrics API.
  static std::vector<MetricTag> metricTags() {
#define DEFINE_METRIC(name) {#name, MetricTag::TagType::String},
    return std::vector<MetricTag>{STD_ISTIO_DIMENSIONS(DEFINE_METRIC)};
#undef DEFINE_METRIC
  }

  // values is used on the datapath, only when new dimensions are found.
  std::vector<std::string> values() {
#define VALUES(name) name,
    return std::vector<std::string>{STD_ISTIO_DIMENSIONS(VALUES)};
#undef VALUES
  }

  void setFieldsUnknownIfEmpty() {
#define SET_IF_EMPTY(name) \
  if ((name).empty()) {    \
    (name) = unknown;      \
  }
    STD_ISTIO_DIMENSIONS(SET_IF_EMPTY)
#undef SET_IF_EMPTY
  }

  // Example Prometheus output
  //
  // istio_requests_total{
  // connection_security_policy="unknown",
  // destination_app="svc01-0-8",
  // destination_principal="unknown",
  // destination_service="svc01-0-8.service-graph01.svc.cluster.local",
  // destination_service_name="svc01-0-8",
  // destination_service_namespace="service-graph01",
  // destination_version="v1",
  // destination_workload="svc01-0-8",
  // destination_workload_namespace="service-graph01",
  // permissive_response_code="none",
  // permissive_response_policyid="none",
  // reporter="source",
  // request_protocol="http",
  // response_code="200",
  // response_flags="-",
  // source_app="svc01-0",
  // source_principal="unknown",
  // source_version="v2",
  // source_workload="svc01-0v2",
  // source_workload_namespace="service-graph01"
  // }

 private:
  void map_node(bool is_source, const wasm::common::NodeInfo& node) {
    if (is_source) {
      source_workload = node.workload_name();
      source_workload_namespace = node.namespace_();

      auto source_labels = node.labels();
      source_app = source_labels["app"];
      source_version = source_labels["version"];
    } else {
      destination_workload = node.workload_name();
      destination_workload_namespace = node.namespace_();

      auto destination_labels = node.labels();
      destination_app = destination_labels["app"];
      destination_version = destination_labels["version"];

      destination_service_namespace = node.namespace_();
    }
  }

  // Called during request processing.
  void map_peer(const wasm::common::NodeInfo& peer_node) {
    map_node(!outbound, peer_node);
  }

  // maps from request context to dimensions.
  // local node derived dimensions are already filled in.
  void map_request(const ::Wasm::Common::RequestInfo& request) {
    source_principal = request.source_principal;
    destination_principal = request.destination_principal;
    destination_service = request.destination_service_host;
    destination_service_name = request.destination_service_name;

    request_protocol = request.request_protocol;
    response_code = std::to_string(request.response_code);
    response_flags = request.response_flag;

    connection_security_policy =
        std::string(::Wasm::Common::AuthenticationPolicyString(
            request.service_auth_policy));

    permissive_response_code = request.rbac_permissive_engine_result.empty()
                                   ? "none"
                                   : request.rbac_permissive_engine_result;
    permissive_response_policyid = request.rbac_permissive_policy_id.empty()
                                       ? "none"
                                       : request.rbac_permissive_policy_id;

    setFieldsUnknownIfEmpty();
  }

 public:
  // Called during intialization.
  // initialize properties that do not vary by requests.
  // Properties are different based on inbound / outbound.
  void init(bool out_bound, wasm::common::NodeInfo& local_node) {
    outbound = out_bound;
    reporter = out_bound ? vSource : vDest;

    map_node(out_bound, local_node);
  }

  // maps peer_node and request to dimensions.
  void map(const wasm::common::NodeInfo& peer_node,
           const ::Wasm::Common::RequestInfo& request) {
    map_peer(peer_node);
    map_request(request);
  }

  std::string to_string() const {
#define TO_STRING(name) "\"", #name, "\":\"", name, "\" ,",
    return absl::StrCat("{" STD_ISTIO_DIMENSIONS(TO_STRING) "}");
#undef TO_STRING
  }

  // debug function to specify a textual key.
  // must match HashValue
  std::string debug_key() {
    auto key =
        absl::StrJoin({reporter, request_protocol, response_code,
                       response_flags, connection_security_policy,
                       permissive_response_code, permissive_response_policyid},
                      "#");
    if (outbound) {
      return absl::StrJoin(
          {key, destination_app, destination_version, destination_service_name,
           destination_service_namespace},
          "#");
    } else {
      return absl::StrJoin({key, source_app, source_version, source_workload,
                            source_workload_namespace},
                           "#");
    }
  }

  // smart hash uses fields based on context.
  // This function is required to make IstioDimensions type hashable.
  struct HashIstioDimensions {
    size_t operator()(const IstioDimensions& c) const {
      const size_t kMul = static_cast<size_t>(0x9ddfea08eb382d69);
      size_t h = 0;
      h += std::hash<std::string>()(c.request_protocol) * kMul;
      h += std::hash<std::string>()(c.response_code) * kMul;
      h += std::hash<std::string>()(c.response_flags) * kMul;
      h += std::hash<std::string>()(c.connection_security_policy) * kMul;
      h += std::hash<std::string>()(c.permissive_response_code) * kMul;
      h += std::hash<std::string>()(c.permissive_response_policyid) * kMul;
      h += c.outbound * kMul;
      if (c.outbound) {  // only care about dest properties
        h += std::hash<std::string>()(c.destination_service_namespace) * kMul;
        h += std::hash<std::string>()(c.destination_service_name) * kMul;
        h += std::hash<std::string>()(c.destination_app) * kMul;
        h += std::hash<std::string>()(c.destination_version) * kMul;
        return h;
      } else {  // only care about source properties
        h += std::hash<std::string>()(c.source_workload_namespace) * kMul;
        h += std::hash<std::string>()(c.source_workload) * kMul;
        h += std::hash<std::string>()(c.source_app) * kMul;
        h += std::hash<std::string>()(c.source_version) * kMul;
        return h;
      }
    }
  };

  // This function is required to make IstioDimensions type hashable.
  friend bool operator==(const IstioDimensions& lhs,
                         const IstioDimensions& rhs) {
    return (
#define COMPARE(name) lhs.name == rhs.name&&
        STD_ISTIO_DIMENSIONS(COMPARE) lhs.outbound == rhs.outbound);
#undef COMPARE
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

// StatGen creates a SimpleStat based on resolved metric_id.
class StatGen {
 public:
  explicit StatGen(std::string name, MetricType metric_type,
                   ValueExtractorFn value_fn, std::string field_separator,
                   std::string value_separator)
      : name_(name),
        value_fn_(value_fn),
        metric_(metric_type, name, IstioDimensions::metricTags(),
                field_separator, value_separator){};

  StatGen() = delete;
  inline StringView name() const { return name_; };

  // Resolve metric based on provided dimension values.
  SimpleStat resolve(std::vector<std::string>& vals) {
    auto metric_id = metric_.resolveWithFields(vals);
    return SimpleStat(metric_id, value_fn_);
  };

 private:
  std::string name_;
  ValueExtractorFn value_fn_;
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

  bool onConfigure(std::unique_ptr<WasmData>) override;
  void report(const ::Wasm::Common::RequestInfo& request_info);
  bool outbound() const { return outbound_; }

 private:
  stats::PluginConfig config_;
  wasm::common::NodeInfo local_node_info_;
  ::Wasm::Common::NodeInfoCache node_info_cache_;

  IstioDimensions istio_dimensions_;

  StringView peer_metadata_id_key_;
  StringView peer_metadata_key_;
  bool outbound_;
  bool debug_;

  int64_t cache_hits_accumulator_ = 0;
  uint32_t cache_hits_;
  uint32_t cache_misses_;

  // Resolved metric where value can be recorded.
  // Maps resolved dimensions to a set of related metrics.
  std::unordered_map<IstioDimensions, std::vector<SimpleStat>,
                     IstioDimensions::HashIstioDimensions>
      metrics_;

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
  explicit PluginContext(uint32_t id, RootContext* root) : Context(id, root) {}

  void onLog() override {
    auto rootCtx = rootContext();
    ::Wasm::Common::populateHTTPRequestInfo(rootCtx->outbound(),
                                            &request_info_);
    rootCtx->report(request_info_);
  };

  // TODO remove the following 3 functions when streamInfo adds support for
  // response_duration, request_size and response_size.
  FilterHeadersStatus onRequestHeaders() override {
    request_info_.start_timestamp = getCurrentTimeNanoseconds();
    return FilterHeadersStatus::Continue;
  };

  FilterDataStatus onRequestBody(size_t body_buffer_length, bool) override {
    request_info_.request_size += body_buffer_length;
    return FilterDataStatus::Continue;
  };

  FilterDataStatus onResponseBody(size_t body_buffer_length, bool) override {
    request_info_.response_size += body_buffer_length;
    return FilterDataStatus::Continue;
  };

 private:
  inline PluginRootContext* rootContext() {
    return dynamic_cast<PluginRootContext*>(this->root());
  };

  ::Wasm::Common::RequestInfo request_info_;
};

#ifdef NULL_PLUGIN
NULL_PLUGIN_ROOT_REGISTRY;
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
