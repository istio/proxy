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

#include "extensions/common/wasm/null/null.h"

#include "envoy/config/filter/http/authn/v2alpha1/config.pb.h"

namespace Envoy {
namespace Extensions {
namespace Wasm {
namespace AuthnWasm {

using istio::envoy::config::filter::http::authn::v2alpha1::FilterConfig;
using StringView = absl::string_view;
using Common::Wasm::Null::NullVmPluginRootRegistry;
using Common::Wasm::Null::Plugin::Context;
using Common::Wasm::Null::Plugin::ContextFactory;
using Common::Wasm::Null::Plugin::RootContext;
using Common::Wasm::Null::Plugin::RootFactory;
using Common::Wasm::Null::Plugin::WasmData;

// PluginRootContext is the root context for all streams processed by the
// thread. It has the same lifetime as the worker thread and acts as target for
// interactions that outlives individual stream, e.g. timer, async calls.
class PluginRootContext : public RootContext {
 public:
  PluginRootContext(uint32_t id, absl::string_view root_id)
      : RootContext(id, root_id) {}
  ~PluginRootContext() = default;

  void onConfigure(std::unique_ptr<WasmData>) override;
  void onStart() override;
  void onTick() override;

  const FilterConfig& filter_config() { return filter_config_; };

 private:
  FilterConfig filter_config_;
};

// Per-stream context.
class PluginContext : public Context {
 public:
  explicit PluginContext(uint32_t id, RootContext* root) : Context(id, root) {}

  void onCreate() override;
  Http::FilterHeadersStatus onRequestHeaders() override;
  Http::FilterHeadersStatus onResponseHeaders() override;

  const FilterConfig& filter_config() {
    return rootContext()->filter_config();
  };

 private:
  inline PluginRootContext* rootContext() {
    return dynamic_cast<PluginRootContext*>(this->root());
  };
};

NULL_PLUGIN_ROOT_REGISTRY;

static RegisterContextFactory register_AuthnWasm(
    CONTEXT_FACTORY(PluginContext), ROOT_FACTORY(PluginRootContext));

}  // namespace AuthnWasm
}  // namespace Wasm
}  // namespace Extensions
}  // namespace Envoy
