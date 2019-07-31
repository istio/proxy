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

#include <google/protobuf/util/json_util.h>
#include "absl/container/flat_hash_map.h"
#include "extensions/common/context.h"
#include "extensions/common/node_info.pb.h"
#include "extensions/stats/config.pb.h"

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

using StringView = absl::string_view;

constexpr StringView Sep = "#";

// The following need to be std::strings because the receiver expects a string.
const std::string unknown = "unknown";
const std::string vSource = "source";
const std::string vDest = "destination";
const std::string vMTLS = "mutual_tls";
const std::string vNone = "none";
const std::string vDash = "-";

using google::protobuf::util::JsonParseOptions;
using google::protobuf::util::Status;

// AttributeContext used as an input to map keys.
struct AttributeContext {
  const bool outbound;
  const common::NodeInfo& source;
  const common::NodeInfo& destination;
  const Common::RequestInfo& request;
};

#define ISTIO_DIMENSIONS            \
  X(reporter)                       \
  X(source_workload)                \
  X(source_workload_namespace)      \
  X(source_principal)               \
  X(source_app)                     \
  X(source_version)                 \
  X(destination_workload)           \
  X(destination_workload_namespace) \
  X(destination_principal)          \
  X(destination_app)                \
  X(destination_version)            \
  X(destination_service_host)       \
  X(destination_service_name)       \
  X(destination_service_namespace)  \
  X(request_protocol)               \
  X(response_code)                  \
  X(response_flags)                 \
  X(connection_security_policy)

struct IstioDimensions {
#define X(name) std::string(name);
  ISTIO_DIMENSIONS
#undef X

  // dimension_list is used
  static const std::vector<std::string> List() {
    return std::vector<std::string>{
#define X(name) #name,
        ISTIO_DIMENSIONS
#undef X
    };
  }

  // Ordered metric list.
  static std::vector<MetricTag> metricTags() {
    return std::vector<MetricTag>{
#define X(name) {#name, MetricTag::TagType::String},
        ISTIO_DIMENSIONS
#undef X
    };
  }

  // values is used on the datapath, only when new dimensions have been found.
  std::vector<std::string> values() {
    return std::vector<std::string>{
#define X(name) name,
        ISTIO_DIMENSIONS
#undef X
    };
  }

  // maps from attribute context to dimensions.
  void map(AttributeContext& ctx) {
    reporter = ctx.outbound ? vSource : vDest;

    source_workload = ctx.source.workload_name();
    source_workload_namespace = ctx.source.namespace_();
    source_principal = ctx.request.source_principal;

    auto source_labels = ctx.source.labels();

    source_app = source_labels["app"];
    source_version = source_labels["version"];

    destination_workload = ctx.destination.workload_name();
    destination_workload_namespace = ctx.destination.namespace_();
    destination_principal = ctx.request.destination_principal;

    auto destination_labels = ctx.destination.labels();

    destination_app = destination_labels["app"];
    destination_version = destination_labels["version"];

    destination_service_host = ctx.request.destination_service_host;
    destination_service_name = ctx.destination.workload_name();
    destination_service_namespace = ctx.destination.namespace_();

    request_protocol = ctx.request.request_protocol;
    response_code = std::to_string(ctx.request.response_code);
    response_flags = ctx.request.response_flag;
  }
};

// Node holds node_info proto and a computed key
struct Node {
  // node_info is obtained from local node or metadata exchange header.
  common::NodeInfo node_info;
  // key computed from the node_info;
  std::string key;
};

// InitializeNode loads the Node object and initializes the key
static bool InitializeNode(StringView peer_metadata_key, Node* node) {
  // Missed the cache
  auto metadata = getMetadataStruct(MetadataType::Request, peer_metadata_key);
  auto status = Common::extractNodeMetadata(metadata, &node->node_info);
  if (status != Status::OK) {
    logWarn("cannot parse peer node metadata " + metadata.DebugString() + ": " +
            status.ToString());
    return false;
  }

  auto labels = node->node_info.labels();
  node->key = absl::StrCat(node->node_info.workload_name(), Sep,
                           node->node_info.namespace_(), Sep, labels["app"],
                           Sep, labels["version"]);

  return true;
}

class NodeInfoCache {
 public:
  // Fetches and caches Peer information by peerId
  // TODO Remove this when it is cheap to directly get things from StreamInfo.
  // At present this involves de-serializing to google.Protobuf.Struct and then
  // another round trip to NodeInfo. This Should at most hold N entries.
  // Node is owned by the cache. Do not store a pointer.
  const Node& getPeerById(StringView peerMetadataIdKey,
                          StringView peerMetadataKey);

 private:
  absl::flat_hash_map<std::string, Node> cache_;
};

using ValueExtractorFn = uint64_t (*)(const Common::RequestInfo& request_info);

// SimpleStat record a pre-resolved metric based on the values function.
class SimpleStat {
 public:
  SimpleStat(uint32_t metric_id, ValueExtractorFn value_fn)
      : metric_id_(metric_id), value_fn_(value_fn){};

  inline void record(const Common::RequestInfo& request_info) {
    recordMetric(metric_id_, value_fn_(request_info));
  };

 private:
  uint32_t metric_id_;
  ValueExtractorFn value_fn_;
};

#define SYMIFEMPTY(ex, sym) (ex).empty() ? (sym) : (ex)

using MapperFn = std::function<std::string(
    bool outbound, const common::NodeInfo& source, const common::NodeInfo& dest,
    const Common::RequestInfo& request_info)>;

// Mapping stores a key name and the associated mapper function.
// The mapping order is important during evaluation since it must match the
// order of declared dimensions.
struct Mapping {
 public:
  Mapping(std::string name, MapperFn mapper) : name_(name), mapper_(mapper){};

  Mapping() = delete;

  std::string name_;
  MapperFn mapper_;
};

// Mappings is an ordered list of Mapping objects.
class Mappings {
 public:
  explicit Mappings(std::vector<Mapping> mappings) : mappings_(mappings){};

  Mappings() = delete;

  // metricTags converts mappings into ordered tags
  std::vector<MetricTag> metricTags() const {
    std::vector<MetricTag> ret;

    ret.reserve(mappings_.size());
    for (const auto& mapping : mappings_) {
      ret.push_back({mapping.name_, MetricTag::TagType::String});
    }
    return ret;
  }

  std::vector<std::string> eval(bool outbound, const common::NodeInfo& source,
                                const common::NodeInfo& dest,
                                const Common::RequestInfo& requestInfo) const {
    std::vector<std::string> vals;

    vals.reserve(mappings_.size());
    for (const auto& mapping : mappings_) {
      vals.push_back(mapping.mapper_(outbound, source, dest, requestInfo));
    }
    return vals;
  }

 private:
  std::vector<Mapping> mappings_;
};

#define MAPPING_SYM(key, expr, sym)                                      \
  {                                                                      \
    (key),                                                               \
        [](bool ABSL_ATTRIBUTE_UNUSED outbound,                          \
           const common::NodeInfo& ABSL_ATTRIBUTE_UNUSED source,         \
           const common::NodeInfo& ABSL_ATTRIBUTE_UNUSED dest,           \
           const Common::RequestInfo& ABSL_ATTRIBUTE_UNUSED requestInfo) \
            -> std::string {                                             \
          auto source_labels = source.labels();                          \
          auto dest_labels = dest.labels();                              \
          auto val = ToString((expr));                                   \
          logDebug(absl::StrCat((key), "=", val));                       \
          return (SYMIFEMPTY(ToString(val), (sym)));                     \
        }                                                                \
  }
#define MAPPING(key, expr) MAPPING_SYM((key), (expr), unknown)

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

std::vector<Mapping> istioStandardDimensionsMappings() {
  return {
      MAPPING("reporter", (outbound ? vSource : vDest)),
      // --> Peer info source
      MAPPING("source_workload", source.workload_name()),
      MAPPING("source_workload_namespace", source.namespace_()),
      MAPPING("source_principal", requestInfo.source_principal),
      MAPPING("source_app", source_labels["app"]),
      MAPPING("source_version", source_labels["version"]),

      // --> Peer info destination
      MAPPING("destination_workload", dest.workload_name()),
      MAPPING("destination_workload_namespace", dest.namespace_()),
      MAPPING("destination_principal", requestInfo.destination_principal),
      MAPPING("destination_app", dest_labels["app"]),
      MAPPING("destination_version", dest_labels["version"]),

      // --> Service info
      MAPPING("destination_service_host", requestInfo.destination_service_host),
      MAPPING("destination_service_name", dest.workload_name()),
      MAPPING("destination_service_namespace", dest.namespace_()),

      MAPPING("request_protocol", requestInfo.request_protocol),
      MAPPING("response_code", requestInfo.response_code),
      MAPPING_SYM("response_flags", requestInfo.response_flag, vDash),

      MAPPING("connection_security_policy",
              (outbound ? unknown : (requestInfo.mTLS ? vMTLS : vNone)))};
}

// StatGen is dimensioned using standard Istio dimensions.
// The standard dimensions are defined in istioStandardDimensionsMappings.
class StatGen {
 public:
  explicit StatGen(std::string name, MetricType metric_type,
                   ValueExtractorFn value_fn)
      : name_(name),
        value_fn_(value_fn),
        mappings_(istioStandardDimensionsMappings()),
        metric_(metric_type, name, mappings_.metricTags()){};

  StatGen() = delete;
  inline StringView name() const { return name_; };

  SimpleStat resolve(bool outbound, const common::NodeInfo& source,
                     const common::NodeInfo& dest,
                     const Common::RequestInfo& request_info) {
    auto vals = mappings_.eval(outbound, source, dest, request_info);
    auto metric_id = metric_.resolveWithFields(vals);
    logDebug(absl::StrCat(__FILE__, "::", __FUNCTION__, ":", __LINE__, ":",
                          metric_id, "/", source.name(), dest.name(),
                          request_info.mTLS));
    return SimpleStat(metric_id, value_fn_);
  };

 private:
  std::string name_;
  ValueExtractorFn value_fn_;
  Mappings mappings_;
  Metric metric_;
};

class RequestsTotal : public StatGen {
 public:
  RequestsTotal()
      : StatGen("istio_requests_total", MetricType::Counter, value) {}
  static uint64_t value(const Common::RequestInfo&) { return 1; }
};

class RequestDuration : public StatGen {
 public:
  RequestDuration()
      : StatGen("istio_request_duration_seconds", MetricType::Histogram,
                value) {}
  static uint64_t value(const Common::RequestInfo& request_info) {
    return request_info.end_timestamp - request_info.start_timestamp;
  }
};

class RequestBytes : public StatGen {
 public:
  RequestBytes()
      : StatGen("istio_request_bytes", MetricType::Histogram, value) {}
  static uint64_t value(const Common::RequestInfo& request_info) {
    return request_info.request_size;
  }
};

class ResponseBytes : public StatGen {
 public:
  ResponseBytes()
      : StatGen("istio_response_bytes", MetricType::Histogram, value) {}
  static uint64_t value(const Common::RequestInfo& request_info) {
    return request_info.response_size;
  }
};

// PluginRootContext is the root context for all streams processed by the
// thread. It has the same lifetime as the worker thread and acts as target for
// interactions that outlives individual stream, e.g. timer, async calls.
class PluginRootContext : public RootContext {
 public:
  PluginRootContext(uint32_t id, StringView root_id)
      : RootContext(id, root_id) {}

  ~PluginRootContext() = default;

  void onConfigure(std::unique_ptr<WasmData>) override;
  void onStart() override{};
  void onTick() override{};

  void report(const Common::RequestInfo& requestInfo);

  inline stats::PluginConfig::Direction direction() {
    return config_.direction();
  };

 private:
  stats::PluginConfig config_;
  common::NodeInfo local_node_info_;
  NodeInfoCache node_info_cache_;

  // Resolved metric where value can be recorded.
  absl::flat_hash_map<std::string, SimpleStat> metric_map_;

  // Peer stats to be generated for dimensioned pair.
  std::vector<StatGen> stats_ = {RequestsTotal(), RequestDuration(),
                                 RequestBytes(), ResponseBytes()};
};

// Per-stream context.
class PluginContext : public Context {
 public:
  explicit PluginContext(uint32_t id, RootContext* root) : Context(id, root) {}

  void onCreate() override{};
  void onLog() override {
    Common::populateHTTPRequestInfo(&request_info_);
    rootContext()->report(request_info_);
  };

  // TODO remove the following 3 functions when streamInfo adds support for
  // response_duration, request_size and response_size.
  FilterHeadersStatus onRequestHeaders() override {
    request_info_.start_timestamp = proxy_getCurrentTimeNanoseconds();
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

  Common::RequestInfo request_info_;
};

NULL_PLUGIN_ROOT_REGISTRY;

static RegisterContextFactory register_Stats(CONTEXT_FACTORY(PluginContext),
                                             ROOT_FACTORY(PluginRootContext));

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
