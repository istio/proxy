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

#include "extensions/common/proto_util.h"
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
using ::Wasm::Common::RequestInfo;

constexpr char kStackdriverExporter[] = "stackdriver_exporter";
constexpr char kExporterRegistered[] = "registered";
constexpr int kDefaultLogExportMilliseconds = 10000;  // 10s

namespace {

// Get metric export interval from node metadata. Returns 60 seconds if interval
// is not found in metadata.
int getMonitoringExportInterval() {
  std::string interval_s = "";
  if (getValue({"node", "metadata", kMonitoringExportIntervalKey},
               &interval_s)) {
    return std::stoi(interval_s);
  }
  return 60;
}

// Get logging export interval from node metadata in milliseconds. Returns 60
// seconds if interval is not found in metadata.
int getLoggingExportIntervalMilliseconds() {
  std::string interval_s = "";
  if (getValue({"node", "metadata", kLoggingExportIntervalKey}, &interval_s)) {
    return std::stoi(interval_s) * 1000;
  }
  return kDefaultLogExportMilliseconds;
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

// Get secure stackdriver endpoint for e2e testing.
std::string getSecureEndpoint() {
  std::string secure_endpoint;
  if (!getValue({"node", "metadata", kSecureStackdriverEndpointKey},
                &secure_endpoint)) {
    return "";
  }
  return secure_endpoint;
}

// Get insecure stackdriver endpoint for e2e testing.
std::string getInsecureEndpoint() {
  std::string insecure_endpoint;
  if (!getValue({"node", "metadata", kInsecureStackdriverEndpointKey},
                &insecure_endpoint)) {
    return "";
  }
  return insecure_endpoint;
}

// Get GCP monitoring endpoint. When this is provided, it will override the
// default production endpoint. This should be used to test staging monitoring
// endpoint.
std::string getMonitoringEndpoint() {
  std::string monitoring_endpoint;
  if (!getValue({"node", "metadata", kMonitoringEndpointKey},
                &monitoring_endpoint)) {
    return "";
  }
  return monitoring_endpoint;
}

}  // namespace

// onConfigure == false makes the proxy crash.
// Only policy plugins should return false.
bool StackdriverRootContext::onConfigure(size_t size) {
  initialized_ = configure(size);
  return true;
}

bool StackdriverRootContext::configure(size_t) {
  // onStart is called prior to onConfigure
  if (enableServerAccessLog() || enableEdgeReporting()) {
    proxy_set_tick_period_milliseconds(getLoggingExportIntervalMilliseconds());
  } else {
    proxy_set_tick_period_milliseconds(0);
  }

  WasmDataPtr configuration = getConfiguration();
  // TODO: add config validation to reject the listener if project id is not in
  // metadata. Parse configuration JSON string.
  JsonParseOptions json_options;
  json_options.ignore_unknown_fields = true;
  Status status =
      JsonStringToMessage(configuration->toString(), &config_, json_options);
  if (status != Status::OK) {
    logWarn("Cannot parse Stackdriver plugin configuration JSON string " +
            configuration->toString() + ", " + status.message().ToString());
    return false;
  }

  if (!::Wasm::Common::extractLocalNodeFlatBuffer(&local_node_info_)) {
    logWarn("cannot extract local node metadata");
    return false;
  }

  direction_ = ::Wasm::Common::getTrafficDirection();
  use_host_header_fallback_ = !config_.disable_host_header_fallback();
  const ::Wasm::Common::FlatNode& local_node =
      *flatbuffers::GetRoot<::Wasm::Common::FlatNode>(local_node_info_.data());

  // Common stackdriver stub option for logging, edge and monitoring.
  ::Extensions::Stackdriver::Common::StackdriverStubOption stub_option;
  stub_option.sts_port = getSTSPort();
  stub_option.test_token_path = getTokenFile();
  stub_option.test_root_pem_path = getCACertFile();
  stub_option.secure_endpoint = getSecureEndpoint();
  stub_option.insecure_endpoint = getInsecureEndpoint();
  stub_option.monitoring_endpoint = getMonitoringEndpoint();
  const auto platform_metadata = local_node.platform_metadata();
  if (platform_metadata) {
    const auto project_iter = platform_metadata->LookupByKey(kGCPProjectKey);
    if (project_iter) {
      stub_option.project_id = flatbuffers::GetString(project_iter->value());
    }
  }

  if (!logger_ && enableServerAccessLog()) {
    // logger should only be initiated once, for now there is no reason to
    // recreate logger because of config update.
    auto logging_stub_option = stub_option;
    logging_stub_option.default_endpoint = kLoggingService;
    auto exporter = std::make_unique<ExporterImpl>(this, logging_stub_option);
    // logger takes ownership of exporter.
    logger_ = std::make_unique<Logger>(local_node, std::move(exporter));
  }

  if (!edge_reporter_ && enableEdgeReporting()) {
    // edge reporter should only be initiated once, for now there is no reason
    // to recreate edge reporter because of config update.
    auto edge_stub_option = stub_option;
    edge_stub_option.default_endpoint = kMeshTelemetryService;
    auto edges_client =
        std::make_unique<MeshEdgesServiceClientImpl>(this, edge_stub_option);

    if (config_.max_edges_batch_size() > 0 &&
        config_.max_edges_batch_size() <= 1000) {
      edge_reporter_ = std::make_unique<EdgeReporter>(
          local_node, std::move(edges_client), config_.max_edges_batch_size());
    } else {
      edge_reporter_ = std::make_unique<EdgeReporter>(
          local_node, std::move(edges_client),
          ::Extensions::Stackdriver::Edges::kDefaultAssertionBatchSize);
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

  // Register OC Stackdriver exporter and views to be exported.
  // Note exporter and views are global singleton so they should only be
  // registered once.
  WasmDataPtr registered;
  if (WasmResult::Ok == getSharedData(kStackdriverExporter, &registered)) {
    return true;
  }

  setSharedData(kStackdriverExporter, kExporterRegistered);
  auto monitoring_stub_option = stub_option;
  monitoring_stub_option.default_endpoint = kMonitoringService;
  opencensus::exporters::stats::StackdriverExporter::Register(
      getStackdriverOptions(local_node, monitoring_stub_option));
  opencensus::stats::StatsExporter::SetInterval(
      absl::Seconds(getMonitoringExportInterval()));

  // Register opencensus measures and views.
  registerViews();

  return true;
}

bool StackdriverRootContext::onStart(size_t) { return true; }

void StackdriverRootContext::onTick() {
  if (enableServerAccessLog()) {
    logger_->exportLogEntry(/* is_on_done= */ false);
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

bool StackdriverRootContext::onDone() {
  bool done = true;
  // Check if logger is empty. In base Wasm VM, only onStart and onDone are
  // called, but onConfigure is not triggered. onConfigure is only triggered in
  // thread local VM, which makes it possible that logger_ is empty ptr even
  // when logging is enabled.
  if (logger_ && enableServerAccessLog() &&
      logger_->exportLogEntry(/* is_on_done= */ true)) {
    done = false;
  }
  // TODO: add on done for edge.
  return done;
}

void StackdriverRootContext::record() {
  const bool outbound = isOutbound();
  const auto& metadata_key =
      outbound ? kUpstreamMetadataKey : kDownstreamMetadataKey;
  std::string peer;
  const ::Wasm::Common::FlatNode& peer_node =
      *flatbuffers::GetRoot<::Wasm::Common::FlatNode>(
          getValue({metadata_key}, &peer) ? peer.data()
                                          : empty_node_info_.data());
  const ::Wasm::Common::FlatNode& local_node =
      *flatbuffers::GetRoot<::Wasm::Common::FlatNode>(local_node_info_.data());
  const ::Wasm::Common::FlatNode& destination_node_info =
      outbound ? peer_node : local_node;

  ::Wasm::Common::RequestInfo request_info;
  ::Wasm::Common::populateHTTPRequestInfo(
      isOutbound(), useHostHeaderFallback(), &request_info,
      flatbuffers::GetString(destination_node_info.namespace_()));
  ::Extensions::Stackdriver::Metric::record(
      isOutbound(), local_node, peer_node, request_info,
      !config_.disable_http_size_metrics());
  if (enableServerAccessLog() && shouldLogThisRequest()) {
    ::Wasm::Common::populateExtendedHTTPRequestInfo(&request_info);
    logger_->addLogEntry(request_info, peer_node);
  }
  if (enableEdgeReporting()) {
    std::string peer_id;
    if (!getValue({::Wasm::Common::kDownstreamMetadataIdKey}, &peer_id)) {
      LOG_DEBUG(absl::StrCat(
          "cannot get metadata for: ", ::Wasm::Common::kDownstreamMetadataIdKey,
          "; skipping edge."));
      return;
    }
    edge_reporter_->addEdge(request_info, peer_id, peer_node);
  }
}

inline bool StackdriverRootContext::isOutbound() {
  return direction_ == ::Wasm::Common::TrafficDirection::Outbound;
}

inline bool StackdriverRootContext::enableServerAccessLog() {
  return !config_.disable_server_access_logging() && !isOutbound();
}

inline bool StackdriverRootContext::enableEdgeReporting() {
  return config_.enable_mesh_edges_reporting() && !isOutbound();
}

bool StackdriverRootContext::shouldLogThisRequest() {
  std::string shouldLog = "";
  if (!getValue({::Wasm::Common::kAccessLogPolicyKey}, &shouldLog)) {
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
  if (!getRootContext()->initialized()) {
    return;
  }
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
