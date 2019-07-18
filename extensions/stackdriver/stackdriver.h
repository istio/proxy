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

#include "extensions/stackdriver/config/stackdriver_plugin_config.pb.h"
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
  ~StackdriverRootContext() = default;

  void onConfigure(std::unique_ptr<WasmData> configuration) override;
  void onStart() override;
  void onTick() override;

  // Get reporter kind of this filter from plugin config.
  stackdriver::config::PluginConfig::ReporterKind reporterKind();

 private:
  opencensus::exporters::stats::StackdriverOptions getStackdriverOptions();

  // Config for Stackdriver plugin.
  stackdriver::config::PluginConfig config_;
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
