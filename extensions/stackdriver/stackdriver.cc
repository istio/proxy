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

#include "extensions/stackdriver/stackdriver.h"

#include <google/protobuf/util/json_util.h>

#include <random>
#include <string>
#include <unordered_map>

#include "extensions/stackdriver/edges/mesh_edges_service_client.h"
#include "extensions/stackdriver/log/exporter.h"
#include "extensions/stackdriver/metric/registry.h"

#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"
#else

#include "extensions/common/wasm/null/null.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {
#endif
namespace Stackdriver {

using namespace opencensus::exporters::stats;
using namespace google::protobuf::util;
using namespace ::Extensions::Stackdriver::Common;
using namespace ::Extensions::Stackdriver::Metric;
using Envoy::Extensions::Common::Wasm::Null::Plugin::getStringValue;
using ::Extensions::Stackdriver::Edges::EdgeReporter;
using Extensions::Stackdriver::Edges::MeshEdgesServiceClientImpl;
using Extensions::Stackdriver::Log::ExporterImpl;
using ::Extensions::Stackdriver::Log::Logger;
using stackdriver::config::v1alpha1::PluginConfig;
using ::Wasm::Common::kDownstreamMetadataIdKey;
using ::Wasm::Common::kDownstreamMetadataKey;
using ::Wasm::Common::kUpstreamMetadataIdKey;
using ::Wasm::Common::kUpstreamMetadataKey;
using ::wasm::common::NodeInfo;
using ::Wasm::Common::RequestInfo;

constexpr char kStackdriverExporter[] = "stackdriver_exporter";
constexpr char kExporterRegistered[] = "registered";
constexpr int kDefaultLogExportMilliseconds = 10000;                      // 10s
constexpr long int kDefaultEdgeReportDurationNanoseconds = 600000000000;  // 10m

namespace {

// Gets monitoring service endpoint from node metadata. Returns empty string if
// it is not found.
std::string getMonitoringEndpoint() {
  std::string monitoring_service;
  if (!getStringValue({"node", "metadata", kMonitoringEndpointKey},
                      &monitoring_service)) {
    return "";
  }
  return monitoring_service;
}

// Gets logging service endpoint from node metadata. Returns empty string if it
// is not found.
std::string getLoggingEndpoint() {
  std::string logging_service;
  if (!getStringValue({"node", "metadata", kLoggingEndpointKey},
                      &logging_service)) {
    return "";
  }
  return logging_service;
}

// Get mesh telemetry service endpoint from node metadata. Returns empty string
// if it is not found.
std::string getMeshTelemetryEndpoint() {
  std::string mesh_telemetry_service;
  if (!getStringValue({"node", "metadata", kMeshTelemetryEndpointKey},
                      &mesh_telemetry_service)) {
    return "";
  }
  return mesh_telemetry_service;
}

}  // namespace

bool StackdriverRootContext::onConfigure(
    std::unique_ptr<WasmData> configuration) {
  // TODO: add config validation to reject the listener if project id is not in
  // metadata. Parse configuration JSON string.
  JsonParseOptions json_options;
  Status status =
      JsonStringToMessage(configuration->toString(), &config_, json_options);
  if (status != Status::OK) {
    logWarn("Cannot parse Stackdriver plugin configuration JSON string " +
            configuration->toString() + ", " + status.message().ToString());
    return false;
  }

  status = ::Wasm::Common::extractLocalNodeMetadata(&local_node_info_);
  if (status != Status::OK) {
    logWarn("cannot extract local node metadata: " + status.ToString());
    return false;
  }

  direction_ = ::Wasm::Common::getTrafficDirection();
  use_host_header_fallback_ = !config_.disable_host_header_fallback();

  if (!logger_) {
    // logger should only be initiated once, for now there is no reason to
    // recreate logger because of config update.
    auto exporter = std::make_unique<ExporterImpl>(this, getLoggingEndpoint());
    // logger takes ownership of exporter.
    logger_ = std::make_unique<Logger>(local_node_info_, std::move(exporter));
  }

  if (!edge_reporter_) {
    // edge reporter should only be initiated once, for now there is no reason
    // to recreate edge reporter because of config update.
    auto edges_client = std::make_unique<MeshEdgesServiceClientImpl>(
        this, getMeshTelemetryEndpoint());
    edge_reporter_ = std::make_unique<EdgeReporter>(local_node_info_,
                                                    std::move(edges_client));
  }

  if (config_.has_mesh_edges_reporting_duration()) {
    edge_report_duration_nanos_ =
        ::google::protobuf::util::TimeUtil::DurationToNanoseconds(
            config_.mesh_edges_reporting_duration());
  } else {
    edge_report_duration_nanos_ = kDefaultEdgeReportDurationNanoseconds;
  }

  node_info_cache_.setMaxCacheSize(config_.max_peer_cache_size());

  // Register OC Stackdriver exporter and views to be exported.
  // Note exporter and views are global singleton so they should only be
  // registered once.
  WasmDataPtr registered;
  if (WasmResult::Ok == getSharedData(kStackdriverExporter, &registered)) {
    return true;
  }

  setSharedData(kStackdriverExporter, kExporterRegistered);
  opencensus::exporters::stats::StackdriverExporter::Register(
      getStackdriverOptions(local_node_info_, getMonitoringEndpoint()));

  // Register opencensus measures and views.
  registerViews();
  return true;
}

void StackdriverRootContext::onStart(std::unique_ptr<WasmData>) {
  if (enableServerAccessLog() || enableEdgeReporting()) {
    proxy_setTickPeriodMilliseconds(kDefaultLogExportMilliseconds);
  }
}

void StackdriverRootContext::onTick() {
  if (enableServerAccessLog()) {
    logger_->exportLogEntry();
  }
  if (enableEdgeReporting()) {
    auto cur = static_cast<long int>(getCurrentTimeNanoseconds());
    if ((cur - last_edge_report_call_nanos_) > edge_report_duration_nanos_) {
      edge_reporter_->reportEdges();
      last_edge_report_call_nanos_ = cur;
    }
  }
}

void StackdriverRootContext::record(const RequestInfo& request_info) {
  const auto peer_node_info_ptr = getPeerNode();
  const NodeInfo& peer_node_info =
      peer_node_info_ptr ? *peer_node_info_ptr : ::Wasm::Common::EmptyNodeInfo;
  ::Extensions::Stackdriver::Metric::record(isOutbound(), local_node_info_,
                                            peer_node_info, request_info);
  if (enableServerAccessLog()) {
    logger_->addLogEntry(request_info, peer_node_info);
  }
  if (enableEdgeReporting()) {
    std::string peer_id;
    if (!getStringValue(
            {"filter_state", ::Wasm::Common::kDownstreamMetadataIdKey},
            &peer_id)) {
      LOG_DEBUG(absl::StrCat(
          "cannot get metadata for: ", ::Wasm::Common::kDownstreamMetadataIdKey,
          "; skipping edge."));
      return;
    }
    edge_reporter_->addEdge(request_info, peer_id, peer_node_info);
  }
}

inline bool StackdriverRootContext::isOutbound() {
  return direction_ == ::Wasm::Common::TrafficDirection::Outbound;
}

::Wasm::Common::NodeInfoPtr StackdriverRootContext::getPeerNode() {
  bool isOutbound = this->isOutbound();
  const auto& id_key =
      isOutbound ? kUpstreamMetadataIdKey : kDownstreamMetadataIdKey;
  const auto& metadata_key =
      isOutbound ? kUpstreamMetadataKey : kDownstreamMetadataKey;
  return node_info_cache_.getPeerById(id_key, metadata_key);
}

inline bool StackdriverRootContext::enableServerAccessLog() {
  return !config_.disable_server_access_logging() && !isOutbound();
}

inline bool StackdriverRootContext::enableEdgeReporting() {
  return config_.enable_mesh_edges_reporting() && !isOutbound();
}

// TODO(bianpengyuan) Add final export once root context supports onDone.
// https://github.com/envoyproxy/envoy-wasm/issues/240

FilterHeadersStatus StackdriverContext::onRequestHeaders() {
  request_info_.start_timestamp = getCurrentTimeNanoseconds();
  return FilterHeadersStatus::Continue;
}

FilterDataStatus StackdriverContext::onRequestBody(size_t body_buffer_length,
                                                   bool) {
  // TODO: switch to stream_info.bytesSent/bytesReceived to avoid extra compute.
  request_info_.request_size += body_buffer_length;
  return FilterDataStatus::Continue;
}

FilterDataStatus StackdriverContext::onResponseBody(size_t body_buffer_length,
                                                    bool) {
  // TODO: switch to stream_info.bytesSent/bytesReceived to avoid extra compute.
  request_info_.response_size += body_buffer_length;
  return FilterDataStatus::Continue;
}

StackdriverRootContext* StackdriverContext::getRootContext() {
  RootContext* root = this->root();
  return dynamic_cast<StackdriverRootContext*>(root);
}

void StackdriverContext::onLog() {
  auto* root = getRootContext();
  bool isOutbound = root->isOutbound();
  ::Wasm::Common::populateHTTPRequestInfo(
      isOutbound, root->useHostHeaderFallback(), &request_info_);

  // Record telemetry based on request info.
  root->record(request_info_);
}

}  // namespace Stackdriver

#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace Null
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
#endif
