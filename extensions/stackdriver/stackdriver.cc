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

#include "stackdriver.h"

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

constexpr char kStackdriverExporter[] = "stackdriver_exporter";
constexpr char kExporterRegistered[] = "registered";

void StackdriverRootContext::onConfigure(
    std::unique_ptr<WasmData> configuration) {
  // TODO: Add config for Stackdriver plugin, such as reporter kind.
  UNREFERENCED_PARAMETER(configuration);

  // Only register exporter once in main thread when initiating base WASM
  // module.
  auto registered = getSharedData(kStackdriverExporter);
  if (!registered->view().empty()) {
    return;
  }
  setSharedData(kStackdriverExporter, kExporterRegistered);

  opencensus::exporters::stats::StackdriverExporter::Register(
      getStackdriverOptions());

  // TODO: Register opencensus measures, tags and views.
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
