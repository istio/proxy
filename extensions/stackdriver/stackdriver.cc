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
using namespace stackdriver::config;
using namespace ::Extensions::Stackdriver::Common;

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
            configuration->toString());
    return;
  }

  // Get node metadata. GetMetadataStruct always returns the whole node metadata
  // even with a key passed in.
  // TODO: clean this up after fixing upstream API to respect node metadata key.
  auto node_metadata = getMetadataStruct(Common::Wasm::MetadataType::Node, "");
  auto istio_metadata = node_metadata.fields().find(kIstioMetadataKey);
  if (istio_metadata != node_metadata.fields().end()) {
    auto status = ExtractNodeMetadata(istio_metadata->second.struct_value(),
                                      &local_node_info_);
    if (status != Status::OK) {
      logWarn("cannot parse local node metadata " +
              node_metadata.DebugString() + ": " + status.ToString());
    }
  }

  // Register OC Stackdriver exporter and views to be exported.
  // Note exporter and views are global singleton so they should only be
  // registered once.
  auto registered = getSharedData(kStackdriverExporter);
  if (!registered->view().empty()) {
    return;
  }
  setSharedData(kStackdriverExporter, kExporterRegistered);

  opencensus::exporters::stats::StackdriverExporter::Register(
      getStackdriverOptions());

  // TODO: Register opencensus measures, tags and views.
}

PluginConfig::ReporterKind StackdriverRootContext::reporterKind() {
  return config_.kind();
}

void StackdriverRootContext::onStart() {
#ifndef NULL_PLUGIN
// TODO: Start a timer to trigger exporting
#endif
}

void StackdriverRootContext::onTick(){
#ifndef NULL_PLUGIN
// TODO: Add exporting logic with WASM gRPC API
#endif
}

StackdriverOptions StackdriverRootContext::getStackdriverOptions() {
  StackdriverOptions options;
  // TODO: Fill in project ID and monitored resource labels either from node
  // metadata or from metadata server.
  return options;
}

FilterHeadersStatus StackdriverContext::onRequestHeaders() {
  request_info_.start_timestamp = proxy_getCurrentTimeNanoseconds();
  request_info_.request_protocol = getProtocol(StreamType::Request)->toString();
  request_info_.destination_service_host =
      getHeaderMapValue(HeaderMapType::RequestHeaders, kAuthorityHeaderKey)
          ->toString();
  request_info_.request_operation =
      getHeaderMapValue(HeaderMapType::RequestHeaders, kMethodHeaderKey)
          ->toString();
  if (getRootContext()->reporterKind() ==
      PluginConfig::ReporterKind::PluginConfig_ReporterKind_INBOUND) {
    auto downstream_metadata = getMetadataStruct(
        Common::Wasm::MetadataType::Request, kDownstreamMetadataKey);
    auto status =
        ExtractNodeMetadata(downstream_metadata, &request_info_.peer_node_info);
    if (status != Status::OK) {
      logWarn("cannot parse downstream peer node metadata " +
              downstream_metadata.DebugString() + ": " + status.ToString());
    }
  }
  return FilterHeadersStatus::Continue;
}

FilterDataStatus StackdriverContext::onRequestBody(size_t body_buffer_length,
                                                   bool) {
  request_info_.request_size += body_buffer_length;
  return FilterDataStatus::Continue;
}

FilterHeadersStatus StackdriverContext::onResponseHeaders() {
  if (getRootContext()->reporterKind() ==
      PluginConfig::ReporterKind::PluginConfig_ReporterKind_OUTBOUND) {
    auto upstream_metadata = getMetadataStruct(
        Common::Wasm::MetadataType::Request, kUpstreamMetadataKey);
    auto status =
        ExtractNodeMetadata(upstream_metadata, &request_info_.peer_node_info);
    if (status != Status::OK) {
      logWarn("cannot parse upstream peer node metadata " +
              upstream_metadata.DebugString() + ": " + status.ToString());
    }
  }
  request_info_.end_timestamp = proxy_getCurrentTimeNanoseconds();
  return FilterHeadersStatus::Continue;
}

FilterDataStatus StackdriverContext::onResponseBody(size_t body_buffer_length,
                                                    bool) {
  request_info_.response_size += body_buffer_length;
  return FilterDataStatus::Continue;
}

StackdriverRootContext *StackdriverContext::getRootContext() {
  RootContext *root = this->root();
  return static_cast<StackdriverRootContext *>(root);
}

void StackdriverContext::onLog() {
  // TODO: Record Istio metrics.
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
