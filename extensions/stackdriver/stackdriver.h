#ifndef INCLUDE_STACKDRIVER_H
#define INCLUDE_STACKDRIVER_H

#include "opencensus/exporters/stats/stackdriver/stackdriver_exporter.h"

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
using namespace Plugin;
#endif

namespace Stackdriver {

#ifdef NULL_PLUGIN
NULL_PLUGIN_ROOT_REGISTRY;
#endif

// StackdriverRootContext is the root context for all streams processed by the
// thread. It has the same lifetime as the worker thread and acts as target for
// interactions that outlives individual stream, e.g. timer, async calls.
class StackdriverRootContext : public RootContext {
 public:
  StackdriverRootContext(uint32_t id, StringView root_id)
      : RootContext(id, root_id) {}
  ~StackdriverRootContext() {}

  void onConfigure(std::unique_ptr<WasmData> /* configuration */) override;
  void onStart() override;
  void onTick() override;

 private:
  opencensus::exporters::stats::StackdriverOptions GetStackdriverOptions();
};

// StackdriverContext is per stream context. It has the same lifetime as
// the request stream itself.
class StackdriverContext : public Context {
 public:
  StackdriverContext(uint32_t id, RootContext* root) : Context(id, root) {}
  void onLog() override;

  // TODO: add other WASM filter hooks.
};

static RegisterContextFactory register_StackdriverContext(
    CONTEXT_FACTORY(StackdriverContext), ROOT_FACTORY(StackdriverRootContext));

}  // namespace Stackdriver

#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace Null
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
#endif

#endif  // INCLUDE_STACKDRIVER_H
