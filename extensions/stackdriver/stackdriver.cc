// NOLINT(namespace-envoy)
#include <google/protobuf/util/json_util.h>
#include <random>
#include <string>
#include <unordered_map>

#include "stackdriver.h"

#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"
#else

#include "extensions/common/wasm/null/null.h"

using namespace opencensus::exporters::stats;

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {
#endif

namespace Stackdriver {

constexpr char kStackdriverExporter[] = "stackdriver_exporter";
constexpr char kExporterRegistered[] = "registered";

void StackdriverRootContext::onConfigure(
    std::unique_ptr<WasmData> /* configuration */) {
  // TODO: Add config for Stackdriver plugin, such as reporter kind.

  // Only register exporter once in main thread when initiating base WASM
  // module.
  auto registered = getSharedData(kStackdriverExporter);
  if (registered->view().size() != 0) {
    return;
  }
  setSharedData(kStackdriverExporter, kExporterRegistered);

  opencensus::exporters::stats::StackdriverExporter::Register(
      GetStackdriverOptions());

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

StackdriverOptions StackdriverRootContext::GetStackdriverOptions() {
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
