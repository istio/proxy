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

#include <google/protobuf/util/json_util.h>
#include <random>
#include <string>
#include <unordered_map>

#include "extensions/stackdriver/common/constants.h"
#include "extensions/stackdriver/metric/registry.h"
#include "extensions/stackdriver/stackdriver.h"

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
using stackdriver::config::v1alpha1::PluginConfig;
using ::Wasm::Common::kDownstreamMetadataKey;
using ::Wasm::Common::kUpstreamMetadataKey;
using ::wasm::common::NodeInfo;
using ::Wasm::Common::RequestInfo;

constexpr char kStackdriverExporter[] = "stackdriver_exporter";
constexpr char kExporterRegistered[] = "registered";

void StackdriverRootContext::onConfigure(
    std::unique_ptr<WasmData> configuration) {
  // Parse configuration JSON string.
  JsonParseOptions json_options;
  Status status =
      JsonStringToMessage(configuration->toString(), &config_, json_options);
  if (status != Status::OK) {
    logWarn("Cannot parse Stackdriver plugin configuraiton JSON string " +
            configuration->toString() + ", " + status.message().ToString());
    return;
  }

  status = ::Wasm::Common::extractLocalNodeMetadata(&local_node_info_);
  if (status != Status::OK) {
    logWarn("cannot extract local node metadata: " + status.ToString());
    return;
  }

  // Register OC Stackdriver exporter and views to be exported.
  // Note exporter and views are global singleton so they should only be
  // registered once.
  WasmDataPtr registered;
  if (WasmResult::Ok == getSharedData(kStackdriverExporter, &registered)) {
    return;
  }

  setSharedData(kStackdriverExporter, kExporterRegistered);
  opencensus::exporters::stats::StackdriverExporter::Register(
      getStackdriverOptions(local_node_info_, config_.monitoring_endpoint()));

  // Register opencensus measures and views.
  registerViews();
}

PluginConfig::ReporterKind StackdriverRootContext::reporterKind() {
  return config_.kind();
}

void StackdriverRootContext::onStart(std::unique_ptr<WasmData>) {
#ifndef NULL_PLUGIN
// TODO: Start a timer to trigger exporting
#endif
}

void StackdriverRootContext::onTick() {
#ifndef NULL_PLUGIN
// TODO: Add exporting logic with WASM gRPC API
#endif
}

void StackdriverRootContext::record(const RequestInfo &request_info,
                                    const NodeInfo &peer_node_info) {
  ::Extensions::Stackdriver::Metric::record(config_.kind(), local_node_info_,
                                            peer_node_info, request_info);
}

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

StackdriverRootContext *StackdriverContext::getRootContext() {
  RootContext *root = this->root();
  return dynamic_cast<StackdriverRootContext *>(root);
}

void StackdriverContext::onLog() {
  bool outbound =
      getRootContext()->reporterKind() ==
      PluginConfig::ReporterKind::PluginConfig_ReporterKind_OUTBOUND;
  ::Wasm::Common::populateHTTPRequestInfo(outbound, &request_info_);

  // Fill in peer node metadata in request info.
  if (getRootContext()->reporterKind() ==
      PluginConfig::ReporterKind::PluginConfig_ReporterKind_INBOUND) {
    google::protobuf::Struct downstream_metadata;
    if (getMetadataStruct(Common::Wasm::MetadataType::Request,
                          kDownstreamMetadataKey, &downstream_metadata) !=
        Common::Wasm::WasmResult::Ok) {
      logWarn(
          absl::StrCat("cannot get metadata for: ", kDownstreamMetadataKey));
      return;
    }

    auto status = ::Wasm::Common::extractNodeMetadata(downstream_metadata,
                                                      &peer_node_info_);
    if (status != Status::OK) {
      logWarn("cannot parse downstream peer node metadata " +
              downstream_metadata.DebugString() + ": " + status.ToString());
    }
  } else if (getRootContext()->reporterKind() ==
             PluginConfig::ReporterKind::PluginConfig_ReporterKind_OUTBOUND) {
    google::protobuf::Struct upstream_metadata;
    if (getMetadataStruct(Common::Wasm::MetadataType::Request,
                          kUpstreamMetadataKey,
                          &upstream_metadata) != Common::Wasm::WasmResult::Ok) {
      logWarn(absl::StrCat("cannot get metadata for: ", kUpstreamMetadataKey));
      return;
    }

    auto status = ::Wasm::Common::extractNodeMetadata(upstream_metadata,
                                                      &peer_node_info_);
    if (status != Status::OK) {
      logWarn("cannot parse upstream peer node metadata " +
              upstream_metadata.DebugString() + ": " + status.ToString());
    }
  }

  // Record telemetry based on request info.
  getRootContext()->record(request_info_, peer_node_info_);
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
