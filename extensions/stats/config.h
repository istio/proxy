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

struct Node {
  common::NodeInfo node_info;
  // key computed from the
  std::string key;

  Node(common::NodeInfo nodeInfo) {
    auto labels = nodeInfo.labels();
    absl::StrAppend(&key, nodeInfo.workload_name(), Sep, nodeInfo.namespace_(),
                    Sep, labels["app"], Sep, labels["version"]);
    node_info = nodeInfo;
  }

  Node() = delete;
};

using NodeSharedPtr = std::shared_ptr<Node>;

class NodeInfoCache {
 public:
  // Fetches and caches Peer information by peerId
  // TODO Remove this when it is cheap to directly get things from StreamInfo.
  // At present this involves de-serializing to google.Protobuf.Struct and then
  // another round trip to NodeInfo. This Should at most hold N entries.
  const NodeSharedPtr getPeerById(StringView peerMetadataIdKey,
                                  StringView peerMetadataKey);

 private:
  absl::flat_hash_map<std::string, NodeSharedPtr> cache_;
};

using valueExtractorFn = uint64_t (*)(const Common::RequestInfo& request_info);

class SimpleStat {
 public:
  SimpleStat(uint32_t metric_id, valueExtractorFn value)
      : metric_id_(metric_id), valueFn_(value){};

  inline void record(const Common::RequestInfo& request_info) {
    recordMetric(metric_id_, valueFn_(request_info));
  };

 private:
  uint32_t metric_id_;
  valueExtractorFn valueFn_;
};

using SimpleStatSharedPtr = std::shared_ptr<SimpleStat>;
using MetricSharedPtr = std::shared_ptr<Metric>;

#define SYMIFEMPTY(ex, sym) (ex).empty() ? (sym) : (ex)

using mapperFn = std::function<std::string(
    bool outbound, const common::NodeInfo& source, const common::NodeInfo& dest,
    const Common::RequestInfo& requestInfo)>;

struct Mapping {
 public:
  Mapping(std::string name, mapperFn mapper) : name_(name), mapper_(mapper){};

  Mapping() = delete;

  std::string name_;
  mapperFn mapper_;
};
std::vector<Mapping> getStandardMappings();

class Mappings {
 public:
  Mappings(std::vector<Mapping> mappings) : mappings_(mappings){};

  Mappings() : Mappings(getStandardMappings()) {}

  // metricTags converts mappings into ordered tags
  std::vector<MetricTag> metricTags() {
    std::vector<MetricTag> ret;

    ret.reserve(mappings_.size());
    for (const auto& mapping : mappings_) {
      ret.push_back({mapping.name_, MetricTag::TagType::String});
    }
    return ret;
  }

  std::vector<std::string> eval(bool outbound, const common::NodeInfo& source,
                                const common::NodeInfo& dest,
                                const Common::RequestInfo& requestInfo) {
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
          auto val = (expr);                                             \
          logDebug(absl::StrCat((key), "=", ToString(val)));             \
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

std::vector<Mapping> getStandardMappings() {
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
              (outbound ? unknown : (requestInfo.mTLS ? vMTLS : vNone))),
      MAPPING_SYM("permissive_response_code",
                  requestInfo.permissive_response_code, vNone),
      MAPPING_SYM("permissive_response_policyid",
                  requestInfo.permissive_response_policyid, vNone)};
}

// StatGen is dimensioned using standard Istio dimensions.
// Standard Istio metrics have the following dimensions
class StatGen {
 public:
  StatGen(std::string name, MetricType metricType, valueExtractorFn valueFn)
      : name_(name), valueFn_(valueFn) {
    metric_ =
        std::make_shared<Metric>(metricType, name, mappings_.metricTags());
  };

  StatGen() = delete;
  inline StringView name() const { return name_; };

  // return a SimpleSharedPtr
  SimpleStatSharedPtr resolve(bool outbound, const common::NodeInfo& source,
                              const common::NodeInfo& dest,
                              const Common::RequestInfo& requestInfo) {
    auto vals = mappings_.eval(outbound, source, dest, requestInfo);
    auto metric_id = metric_->resolveWithFields(vals);
    logDebug(absl::StrCat(__FUNCTION__, ":", __LINE__, ":", metric_id, "/",
                          source.name(), dest.name(), requestInfo.mTLS));
    return std::make_shared<SimpleStat>(metric_id, valueFn_);
  };

 private:
  std::string name_;
  valueExtractorFn valueFn_;
  MetricSharedPtr metric_;
  Mappings mappings_;
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
  absl::flat_hash_map<std::string, SimpleStatSharedPtr> metric_map_;

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
    Common::populateRequestInfo(&request_info_);
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
