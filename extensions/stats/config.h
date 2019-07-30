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

// NodeMetadata key is the key in the node metadata struct that is passed
// between peers.
constexpr absl::string_view NodeMetadataKey = "istio.io/metadata";
constexpr absl::string_view NodeIdKey = "id";
constexpr absl::string_view WholeNodeKey = ".";

// DownstreamMetadataKey is the key in the request metadata for downstream peer
// metadata
constexpr absl::string_view DownstreamMetadataKey =
    "envoy.wasm.metadata_exchange.downstream";
constexpr absl::string_view DownstreamMetadataIdKey =
    "envoy.wasm.metadata_exchange.downstream_id";

// UpstreamMetadataKey is the key in the request metadata for downstream peer
// metadata
constexpr absl::string_view UpstreamMetadataKey =
    "envoy.wasm.metadata_exchange.upstream";
constexpr absl::string_view UpstreamMetadataIdKey =
    "envoy.wasm.metadata_exchange.upstream_id";

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

#define SYMIFEMPTY(ex, sym) (ex).empty() ? (sym) : (ex)

#define UNKNOWNIFEMPTY(ex) SYMIFEMPTY((ex), unknown)

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

using mapperFn = std::function<std::string(
    bool outbound, const common::NodeInfo& source, const common::NodeInfo& dest,
    const Common::RequestInfo& requestInfo)>;

class Mapping {
 public:
  Mapping(std::string name, mapperFn mapper) : name_(name), mapper_(mapper){};

  Mapping() = delete;

  std::string name_;
  mapperFn mapper_;
};

#define MAPPING_SYM(k, f, sym)                                           \
  {                                                                      \
    (k),                                                                 \
        [](bool ABSL_ATTRIBUTE_UNUSED outbound,                          \
           const common::NodeInfo& ABSL_ATTRIBUTE_UNUSED source,         \
           const common::NodeInfo& ABSL_ATTRIBUTE_UNUSED dest,           \
           const Common::RequestInfo& ABSL_ATTRIBUTE_UNUSED requestInfo) \
            -> std::string {                                             \
          auto source_labels = source.labels();                          \
          auto dest_labels = dest.labels();                              \
          return (SYMIFEMPTY((sym), ToString((f))));                     \
        }                                                                \
  }
#define MAPPING(k, f) MAPPING_SYM((k), (f), unknown)

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
      MAPPING_SYM("response_code", requestInfo.response_flag, vDash),

      MAPPING("connection_security_policy",
              (outbound ? unknown : (requestInfo.mTLS ? vMTLS : vNone)))

  };
}

// StatGen is dimensioned using standard Istio dimensions.
// Standard Istio metrics have the following dimensions
//  reporter: conditional((context.reporter.kind | "inbound") == "outbound",
//  "source", "destination")
//    --> Peer info
//  source_workload: source.workload.name | "unknown"
//  source_workload_namespace: source.workload.namespace | "unknown"
//  source_principal: source.principal | "unknown"
//  source_app: source.labels["app"] | "unknown"
//  source_version: source.labels["version"] | "unknown"
//  destination_workload: destination.workload.name | "unknown"
//  destination_workload_namespace: destination.workload.namespace | "unknown"
//  destination_principal: destination.principal | "unknown"
//  destination_app: destination.labels["app"] | "unknown"
//  destination_version: destination.labels["version"] | "unknown"
// --> service
//  destination_service: destination.service.host | "unknown"
//  destination_service_name: destination.service.name | "unknown"
//  destination_service_namespace: destination.service.namespace | "unknown"
// --> request
//  request_protocol: api.protocol | context.protocol | "unknown"
//  response_code: response.code | 200
//  response_flags: context.proxy_error_code | "-"
//  permissive_response_code: rbac.permissive.response_code | "none"
//  permissive_response_policyid: rbac.permissive.effective_policy_id | "none"
//  connection_security_policy: conditional((context.reporter.kind | "inbound")
//  == "outbound", "unknown", conditional(connection.mtls | false, "mutual_tls",
//  "none"))
class StatGen {
 public:
  StatGen(std::string name, MetricType metricType, valueExtractorFn valueFn)
      : name_(name),
        valueFn_(valueFn),
        metric_(
            metricType, name,
            {MetricTag{"reporter", MetricTag::TagType::String},
             MetricTag{"source_app", MetricTag::TagType::String},
             MetricTag{"source_namespace", MetricTag::TagType::String},
             MetricTag{"source_workload", MetricTag::TagType::String},
             MetricTag{"source_workload_namespace", MetricTag::TagType::String},
             MetricTag{"source_version", MetricTag::TagType::String},
             MetricTag{"destination_app", MetricTag::TagType::String},
             MetricTag{"destination_namespace", MetricTag::TagType::String},
             MetricTag{"destination_workload", MetricTag::TagType::String},
             MetricTag{"destination_workload_namespace",
                       MetricTag::TagType::String},
             MetricTag{"destination_version", MetricTag::TagType::String},
             MetricTag{"destination_service", MetricTag::TagType::String},
             MetricTag{"destination_service_name", MetricTag::TagType::String},
             MetricTag{"destination_service_namespace",
                       MetricTag::TagType::String},
             MetricTag{"source_principal", MetricTag::TagType::String},
             MetricTag{"destination_principal", MetricTag::TagType::String},
             MetricTag{"request_protocol", MetricTag::TagType::String},
             MetricTag{"response_code", MetricTag::TagType::Int},
             MetricTag{"connection_mtls", MetricTag::TagType::Bool}}){};

  StatGen() = delete;
  inline StringView name() const { return name_; };

  // return a SimpleSharedPtr
  SimpleStatSharedPtr resolve(std::string reporter,
                              const common::NodeInfo& source,
                              const common::NodeInfo& dest,
                              const Common::RequestInfo& requestInfo) {
    logInfo(absl::StrCat(__FUNCTION__, ":", __LINE__, ":", reporter,
                         source.DebugString(), dest.DebugString()));
    auto source_labels = source.labels();
    auto dest_labels = dest.labels();
    auto metric_id = metric_.resolve(
        reporter, source_labels["app"], source.namespace_(),
        source.workload_name(), source.namespace_(), source_labels["version"],
        UNKNOWNIFEMPTY(dest_labels["app"]), UNKNOWNIFEMPTY(dest.namespace_()),
        dest.workload_name(), UNKNOWNIFEMPTY(dest.namespace_()),
        dest_labels["version"], requestInfo.destination_service_host,
        dest.workload_name(), dest.namespace_(),
        UNKNOWNIFEMPTY(requestInfo.source_principal),
        UNKNOWNIFEMPTY(requestInfo.destination_principal),
        requestInfo.request_protocol, requestInfo.response_code,
        requestInfo.mTLS);

    logInfo(absl::StrCat(__FUNCTION__, ":", __LINE__, ":", metric_id, "/",
                         source.name(), dest.name(), requestInfo.mTLS));
    return std::make_shared<SimpleStat>(metric_id, valueFn_);
  };

 private:
  std::string name_;
  valueExtractorFn valueFn_;
  Metric metric_;
};

class IstioRequestsTotal : public StatGen {
 public:
  IstioRequestsTotal()
      : StatGen("istio_requests_total", MetricType::Counter, value) {}
  static uint64_t value(const Common::RequestInfo&) { return 1; }
};

// PluginRootContext is the root context for all streams processed by the
// thread. It has the same lifetime as the worker thread and acts as target for
// interactions that outlives individual stream, e.g. timer, async calls.
class PluginRootContext : public RootContext {
 public:
  PluginRootContext(uint32_t id, StringView root_id)
      : RootContext(id, root_id),
        istio_requests_total_metric_(
            Counter<std::string, std::string, std::string, std::string>::New(
                "istio_requests_total", "source_app", "source_version",
                "destination_app", "destination_version")) {}
  ~PluginRootContext() = default;

  void onConfigure(std::unique_ptr<WasmData>) override;
  void onStart() override{};
  void onTick() override{};

  void report(const Common::RequestInfo& requestInfo);

  inline stats::PluginConfig::Direction direction() {
    return config_.direction();
  };

 private:
  Counter<std::string, std::string, std::string, std::string>*
      istio_requests_total_metric_;
  absl::flat_hash_map<std::string, SimpleCounter> counter_map_;

  stats::PluginConfig config_;
  common::NodeInfo local_node_info_;
  NodeInfoCache node_info_cache_;
  absl::flat_hash_map<std::string, SimpleStatSharedPtr> metric_map_;
  std::vector<StatGen> stats_ = {IstioRequestsTotal()};
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
