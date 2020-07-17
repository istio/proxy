/* Copyright 2018 Istio Authors. All Rights Reserved.
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
#include "extensions/common/wasm/wasm.h"

#include "src/envoy/extensions/wasm/context.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Istio {

class IstioWasmVmIntegration : public EnvoyWasmVmIntegration {
 public:
  IstioWasmVmIntegration(const Stats::ScopeSharedPtr& scope,
                         absl::string_view runtime,
                         absl::string_view short_runtime)
      : EnvoyWasmVmIntegration(scope, runtime, short_runtime) {}
};

class IstioWasm : public Wasm {
 public:
  IstioWasm(absl::string_view runtime, absl::string_view vm_id,
            absl::string_view vm_configuration, absl::string_view vm_key,
            const Stats::ScopeSharedPtr& scope,
            Upstream::ClusterManager& cluster_manager,
            Event::Dispatcher& dispatcher)
      : Wasm(runtime, vm_id, vm_configuration, vm_key, scope, cluster_manager,
             dispatcher) {}
  IstioWasm(std::shared_ptr<WasmHandle> other, Event::Dispatcher& dispatcher)
      : Wasm(other, dispatcher) {}
  ~IstioWasm() override = default;

  proxy_wasm::ContextBase* createContext(
      const std::shared_ptr<PluginBase>& plugin) override {
    if (create_context_for_testing_) {
      return create_context_for_testing_(
          this, std::static_pointer_cast<Plugin>(plugin));
    }
    return new IstioContext(this, std::static_pointer_cast<Plugin>(plugin));
  }
  proxy_wasm::ContextBase* createRootContext(
      const std::shared_ptr<PluginBase>& plugin) override {
    if (create_root_context_for_testing_) {
      return create_root_context_for_testing_(
          this, std::static_pointer_cast<Plugin>(plugin));
    }
    return new IstioContext(this, std::static_pointer_cast<Plugin>(plugin));
  }
};

class IstioWasmExtension : public EnvoyWasm {
 public:
  IstioWasmExtension() = default;
  ~IstioWasmExtension() override = default;
  std::unique_ptr<EnvoyWasmVmIntegration> createEnvoyWasmVmIntegration(
      const Stats::ScopeSharedPtr& scope, absl::string_view runtime,
      absl::string_view short_runtime) override;
  WasmHandleExtensionFactory wasmFactory() override;
  WasmHandleExtensionCloneFactory wasmCloneFactory() override;
  void onEvent(WasmEvent event, const PluginSharedPtr& plugin) override;
  void onRemoteCacheEntriesChanged(int remote_cache_entries) override;
  void createStats(const Stats::ScopeSharedPtr& scope,
                   const PluginSharedPtr& plugin) override;
  void resetStats() override;

 private:
  std::unique_ptr<CreateWasmStats> create_wasm_stats_;
};

std::unique_ptr<EnvoyWasmVmIntegration>
IstioWasmExtension::createEnvoyWasmVmIntegration(
    const Stats::ScopeSharedPtr& scope, absl::string_view runtime,
    absl::string_view short_runtime) {
  return std::make_unique<IstioWasmVmIntegration>(scope, runtime,
                                                  short_runtime);
}

WasmHandleExtensionFactory IstioWasmExtension::wasmFactory() {
  return [](const VmConfig vm_config, const Stats::ScopeSharedPtr& scope,
            Upstream::ClusterManager& cluster_manager,
            Event::Dispatcher& dispatcher,
            Server::ServerLifecycleNotifier& lifecycle_notifier,
            absl::string_view vm_key) -> WasmHandleBaseSharedPtr {
    auto wasm =
        std::make_shared<IstioWasm>(vm_config.runtime(), vm_config.vm_id(),
                                    anyToBytes(vm_config.configuration()),
                                    vm_key, scope, cluster_manager, dispatcher);
    wasm->initializeLifecycle(lifecycle_notifier);
    return std::static_pointer_cast<WasmHandleBase>(
        std::make_shared<WasmHandle>(std::move(wasm)));
  };
}

WasmHandleExtensionCloneFactory IstioWasmExtension::wasmCloneFactory() {
  return [](const WasmHandleSharedPtr& base_wasm, Event::Dispatcher& dispatcher,
            CreateContextFn create_root_context_for_testing)
             -> WasmHandleBaseSharedPtr {
    auto wasm = std::make_shared<IstioWasm>(base_wasm, dispatcher);
    wasm->setCreateContextForTesting(nullptr, create_root_context_for_testing);
    return std::static_pointer_cast<WasmHandleBase>(
        std::make_shared<WasmHandle>(std::move(wasm)));
  };
}

void IstioWasmExtension::onEvent(WasmEvent event,
                                 const PluginSharedPtr& plugin) {
  EnvoyWasm::onEvent(event, plugin);
}

void IstioWasmExtension::onRemoteCacheEntriesChanged(int entries) {
  EnvoyWasm::onRemoteCacheEntriesChanged(entries);
  create_wasm_stats_->remote_load_cache_entries_.set(entries);
}

void IstioWasmExtension::createStats(const Stats::ScopeSharedPtr& scope,
                                     const PluginSharedPtr& plugin) {
  EnvoyWasm::createStats(scope, plugin);
}

void IstioWasmExtension::resetStats() { EnvoyWasm::resetStats(); }

REGISTER_WASM_EXTENSION(IstioWasmExtension);

}  // namespace Istio
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
