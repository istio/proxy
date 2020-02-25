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
using Envoy::Extensions::Common::Wasm::Null::Plugin::getValue;
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
constexpr int kDefaultLogExportMilliseconds = 10000;  // 10s

namespace {

// Gets monitoring service endpoint from node metadata. Returns empty string if
// it is not found.
std::string getMonitoringEndpoint() {
  std::string monitoring_service;
  if (!getValue({"node", "metadata", kMonitoringEndpointKey},
                &monitoring_service)) {
    return "";
  }
  return monitoring_service;
}

// Gets logging service endpoint from node metadata. Returns empty string if it
// is not found.
std::string getLoggingEndpoint() {
  std::string logging_service;
  if (!getValue({"node", "metadata", kLoggingEndpointKey}, &logging_service)) {
    return "";
  }
  return logging_service;
}

// Get mesh telemetry service endpoint from node metadata. Returns empty string
// if it is not found.
std::string getMeshTelemetryEndpoint() {
  std::string mesh_telemetry_service;
  if (!getValue({"node", "metadata", kMeshTelemetryEndpointKey},
                &mesh_telemetry_service)) {
    return "";
  }
  return mesh_telemetry_service;
}

// Get metric export interval from node metadata. Returns 60 seconds if interval
// is not found in metadata.
int getExportInterval() {
  std::string interval_s = "";
  if (getValue({"node", "metadata", kMonitoringExportIntervalKey},
               &interval_s)) {
    return std::stoi(interval_s);
  }
  return 60;
}

// Get port of security token exchange server from node metadata, if not
// provided or "0" is provided, emtpy will be returned.
std::string getSTSPort() {
  std::string sts_port;
  if (getValue({"node", "metadata", kSTSPortKey}, &sts_port) &&
      sts_port != "0") {
    return sts_port;
  }
  return "";
}

// Get file name for the token test override.
std::string getTokenFile() {
  std::string token_file;
  if (!getValue({"node", "metadata", kTokenFile}, &token_file)) {
    return "";
  }
  return token_file;
}

// Get file name for the root CA PEM file test override.
std::string getCACertFile() {
  std::string ca_cert_file;
  if (!getValue({"node", "metadata", kCACertFile}, &ca_cert_file)) {
    return "";
  }
  return ca_cert_file;
}

}  // namespace

bool StackdriverRootContext::onConfigure(size_t) {
  // onStart is called prior to onConfigure
  if (enableServerAccessLog() || enableEdgeReporting()) {
    proxy_set_tick_period_milliseconds(kDefaultLogExportMilliseconds);
  } else {
    proxy_set_tick_period_milliseconds(0);
  }

  WasmDataPtr configuration = getConfiguration();
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
  std::string sts_port = getSTSPort();
  if (!logger_) {
    // logger should only be initiated once, for now there is no reason to
    // recreate logger because of config update.
    auto exporter = std::make_unique<ExporterImpl>(
        this, getLoggingEndpoint(), sts_port, getTokenFile(), getCACertFile());
    // logger takes ownership of exporter.
    logger_ = std::make_unique<Logger>(local_node_info_, std::move(exporter));
  }

  if (!edge_reporter_) {
    // edge reporter should only be initiated once, for now there is no reason
    // to recreate edge reporter because of config update.
    auto edges_client = std::make_unique<MeshEdgesServiceClientImpl>(
        this, getMeshTelemetryEndpoint(), sts_port, getTokenFile(),
        getCACertFile());

    if (config_.max_edges_batch_size() > 0 &&
        config_.max_edges_batch_size() <= 1000) {
      edge_reporter_ = std::make_unique<EdgeReporter>(
          local_node_info_, std::move(edges_client),
          config_.max_edges_batch_size());
    } else {
      edge_reporter_ = std::make_unique<EdgeReporter>(local_node_info_,
                                                      std::move(edges_client));
    }
  }

  if (config_.has_mesh_edges_reporting_duration()) {
    auto duration = ::google::protobuf::util::TimeUtil::DurationToNanoseconds(
        config_.mesh_edges_reporting_duration());
    // if the interval duration is longer than the epoch duration, use the
    // epoch duration.
    if (duration >= kDefaultEdgeEpochReportDurationNanoseconds) {
      duration = kDefaultEdgeEpochReportDurationNanoseconds;
    }
    edge_new_report_duration_nanos_ = duration;
  } else {
    edge_new_report_duration_nanos_ = kDefaultEdgeNewReportDurationNanoseconds;
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
      getStackdriverOptions(local_node_info_, getMonitoringEndpoint(), sts_port,
                            getTokenFile(), getCACertFile()));
  opencensus::stats::StatsExporter::SetInterval(
      absl::Seconds(getExportInterval()));

  // Register opencensus measures and views.
  registerViews();

  return true;
}

bool StackdriverRootContext::onStart(size_t) { return true; }

void StackdriverRootContext::onTick() {
  if (enableServerAccessLog()) {
    logger_->exportLogEntry();
  }
  if (enableEdgeReporting()) {
    auto cur = static_cast<long int>(getCurrentTimeNanoseconds());
    if ((cur - last_edge_epoch_report_call_nanos_) >
        edge_epoch_report_duration_nanos_) {
      // end of epoch
      edge_reporter_->reportEdges(true /* report ALL edges from epoch*/);
      last_edge_epoch_report_call_nanos_ = cur;
      last_edge_new_report_call_nanos_ = cur;
    } else if ((cur - last_edge_new_report_call_nanos_) >
               edge_new_report_duration_nanos_) {
      // end of intra-epoch interval
      edge_reporter_->reportEdges(false /* only report new edges*/);
      last_edge_new_report_call_nanos_ = cur;
    }
  }
}

void StackdriverRootContext::record() {
  const auto peer_node_info_ptr = getPeerNode();
  const NodeInfo& peer_node_info =
      peer_node_info_ptr ? *peer_node_info_ptr : ::Wasm::Common::EmptyNodeInfo;
  const auto& destination_node_info =
      isOutbound() ? peer_node_info : local_node_info_;

  ::Wasm::Common::RequestInfo request_info;
  ::Wasm::Common::populateHTTPRequestInfo(isOutbound(), useHostHeaderFallback(),
                                          &request_info,
                                          destination_node_info.namespace_());
  ::Extensions::Stackdriver::Metric::record(isOutbound(), local_node_info_,
                                            peer_node_info, request_info);
  if (enableServerAccessLog() && shouldLogThisRequest()) {
    ::Wasm::Common::populateExtendedHTTPRequestInfo(&request_info);
    logger_->addLogEntry(request_info, peer_node_info);
  }
  if (enableEdgeReporting()) {
    std::string peer_id;
    if (!getValue({"filter_state", ::Wasm::Common::kDownstreamMetadataIdKey},
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
  std::string peer_id;
  return node_info_cache_.getPeerById(id_key, metadata_key, peer_id);
}

inline bool StackdriverRootContext::enableServerAccessLog() {
  return !config_.disable_server_access_logging() && !isOutbound();
}

inline bool StackdriverRootContext::enableEdgeReporting() {
  return config_.enable_mesh_edges_reporting() && !isOutbound();
}

bool StackdriverRootContext::shouldLogThisRequest() {
  std::string shouldLog = "";
  if (!getValue({"filter_state", ::Wasm::Common::kAccessLogPolicyKey},
                &shouldLog)) {
    LOG_DEBUG("cannot get envoy access log info from filter state.");
    return true;
  }
  return shouldLog != "no";
}

// TODO(bianpengyuan) Add final export once root context supports onDone.
// https://github.com/envoyproxy/envoy-wasm/issues/240

StackdriverRootContext* StackdriverContext::getRootContext() {
  RootContext* root = this->root();
  return dynamic_cast<StackdriverRootContext*>(root);
}

void StackdriverContext::onLog() {
  // Record telemetry based on request info.
  getRootContext()->record();
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
