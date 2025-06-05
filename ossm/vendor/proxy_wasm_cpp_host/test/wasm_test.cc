// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "include/proxy-wasm/wasm.h"

#include <unordered_set>

#include "gtest/gtest.h"

#include "test/utility.h"

#include "src/wasm.h"

namespace proxy_wasm {

INSTANTIATE_TEST_SUITE_P(WasmEngines, TestVm, testing::ValuesIn(getWasmEngines()),
                         [](const testing::TestParamInfo<std::string> &info) {
                           return info.param;
                         });

// Fail callbacks only used for WasmVMs - not available for NullVM.
TEST_P(TestVm, GetOrCreateThreadLocalWasmFailCallbacks) {
  const auto *const plugin_name = "plugin_name";
  const auto *const root_id = "root_id";
  const auto *const vm_id = "vm_id";
  const auto *const vm_config = "vm_config";
  const auto *const plugin_config = "plugin_config";
  const auto fail_open = false;

  // Create a plugin.
  const auto plugin = std::make_shared<PluginBase>(plugin_name, root_id, vm_id, engine_,
                                                   plugin_config, fail_open, "plugin_key");

  // Define callbacks.
  WasmHandleFactory wasm_handle_factory =
      [this, vm_id, vm_config](std::string_view vm_key) -> std::shared_ptr<WasmHandleBase> {
    auto base_wasm = std::make_shared<WasmBase>(makeVm(engine_), vm_id, vm_config, vm_key,
                                                std::unordered_map<std::string, std::string>{},
                                                AllowedCapabilitiesMap{});
    return std::make_shared<WasmHandleBase>(base_wasm);
  };

  WasmHandleCloneFactory wasm_handle_clone_factory =
      [this](const std::shared_ptr<WasmHandleBase> &base_wasm_handle)
      -> std::shared_ptr<WasmHandleBase> {
    auto wasm = std::make_shared<WasmBase>(
        base_wasm_handle, [this]() -> std::unique_ptr<WasmVm> { return makeVm(engine_); });
    return std::make_shared<WasmHandleBase>(wasm);
  };

  PluginHandleFactory plugin_handle_factory =
      [](const std::shared_ptr<WasmHandleBase> &base_wasm,
         const std::shared_ptr<PluginBase> &plugin) -> std::shared_ptr<PluginHandleBase> {
    return std::make_shared<PluginHandleBase>(base_wasm, plugin);
  };

  // Read the minimal loadable binary.
  auto source = readTestWasmFile("abi_export.wasm");

  // Create base Wasm via createWasm.
  auto base_wasm_handle =
      createWasm("vm_key", source, plugin, wasm_handle_factory, wasm_handle_clone_factory, false);
  ASSERT_TRUE(base_wasm_handle && base_wasm_handle->wasm());

  // Create a thread local plugin.
  auto thread_local_plugin = getOrCreateThreadLocalPlugin(
      base_wasm_handle, plugin, wasm_handle_clone_factory, plugin_handle_factory);
  ASSERT_TRUE(thread_local_plugin && thread_local_plugin->plugin());
  // If the VM is not failed, same WasmBase should be used for the same configuration.
  ASSERT_EQ(getOrCreateThreadLocalPlugin(base_wasm_handle, plugin, wasm_handle_clone_factory,
                                         plugin_handle_factory)
                ->wasm(),
            thread_local_plugin->wasm());

  // Cause runtime crash.
  thread_local_plugin->wasm()->wasm_vm()->fail(FailState::RuntimeError, "runtime error msg");
  ASSERT_TRUE(thread_local_plugin->wasm()->isFailed());
  // the Base Wasm should not be affected by cloned ones.
  ASSERT_FALSE(base_wasm_handle->wasm()->isFailed());

  // Create another thread local plugin with the same configuration.
  // This one should not end up using the failed VM.
  auto thread_local_plugin2 = getOrCreateThreadLocalPlugin(
      base_wasm_handle, plugin, wasm_handle_clone_factory, plugin_handle_factory);
  ASSERT_TRUE(thread_local_plugin2 && thread_local_plugin2->plugin());
  ASSERT_FALSE(thread_local_plugin2->wasm()->isFailed());
  // Verify the pointer to WasmBase is different from the failed one.
  ASSERT_NE(thread_local_plugin2->wasm(), thread_local_plugin->wasm());

  // Cause runtime crash again.
  thread_local_plugin2->wasm()->wasm_vm()->fail(FailState::RuntimeError, "runtime error msg");
  ASSERT_TRUE(thread_local_plugin2->wasm()->isFailed());
  // the Base Wasm should not be affected by cloned ones.
  ASSERT_FALSE(base_wasm_handle->wasm()->isFailed());

  // This time, create another thread local plugin with *different* plugin key for the same vm_key.
  // This one also should not end up using the failed VM.
  const auto plugin2 = std::make_shared<PluginBase>(plugin_name, root_id, vm_id, engine_,
                                                    plugin_config, fail_open, "another_plugin_key");
  auto thread_local_plugin3 = getOrCreateThreadLocalPlugin(
      base_wasm_handle, plugin2, wasm_handle_clone_factory, plugin_handle_factory);
  ASSERT_TRUE(thread_local_plugin3 && thread_local_plugin3->plugin());
  ASSERT_FALSE(thread_local_plugin3->wasm()->isFailed());
  // Verify the pointer to WasmBase is different from the failed one.
  ASSERT_NE(thread_local_plugin3->wasm(), thread_local_plugin->wasm());
  ASSERT_NE(thread_local_plugin3->wasm(), thread_local_plugin2->wasm());
}

// Tests the canary is always applied when making a call `createWasm`
TEST_P(TestVm, AlwaysApplyCanary) {
  // Use different root_id, but the others are the same
  const auto *const plugin_name = "plugin_name";

  const std::string root_ids[2] = {"root_id_1", "root_id_2"};
  const std::string vm_ids[2] = {"vm_id_1", "vm_id_2"};
  const std::string vm_configs[2] = {"vm_config_1", "vm_config_2"};
  const std::string plugin_configs[3] = {"plugin_config_1", "plugin_config_2",
                                         /* raising the error */ ""};
  const std::string plugin_keys[2] = {"plugin_key_1", "plugin_key_2"};
  const auto fail_open = false;

  // Define common callbacks
  auto canary_count = 0;
  WasmHandleCloneFactory wasm_handle_clone_factory_for_canary =
      [&canary_count, this](const std::shared_ptr<WasmHandleBase> &base_wasm_handle)
      -> std::shared_ptr<WasmHandleBase> {
    auto wasm = std::make_shared<TestWasm>(
        base_wasm_handle, [this]() -> std::unique_ptr<WasmVm> { return makeVm(engine_); });
    canary_count++;
    return std::make_shared<WasmHandleBase>(wasm);
  };

  PluginHandleFactory plugin_handle_factory =
      [](const std::shared_ptr<WasmHandleBase> &base_wasm,
         const std::shared_ptr<PluginBase> &plugin) -> std::shared_ptr<PluginHandleBase> {
    return std::make_shared<PluginHandleBase>(base_wasm, plugin);
  };

  // Read the minimal loadable binary.
  auto source = readTestWasmFile("canary_check.wasm");

  WasmHandleFactory wasm_handle_factory_baseline =
      [this, vm_ids, vm_configs](std::string_view vm_key) -> std::shared_ptr<WasmHandleBase> {
    auto base_wasm =
        std::make_shared<TestWasm>(makeVm(engine_), std::unordered_map<std::string, std::string>(),
                                   vm_ids[0], vm_configs[0], vm_key);
    return std::make_shared<WasmHandleBase>(base_wasm);
  };

  // Create a baseline plugin.
  const auto plugin_baseline = std::make_shared<PluginBase>(
      plugin_name, root_ids[0], vm_ids[0], engine_, plugin_configs[0], fail_open, plugin_keys[0]);

  const auto vm_key_baseline = makeVmKey(vm_ids[0], vm_configs[0], "common_code");
  // Create a base Wasm by createWasm.
  auto wasm_handle_baseline =
      createWasm(vm_key_baseline, source, plugin_baseline, wasm_handle_factory_baseline,
                 wasm_handle_clone_factory_for_canary, false);
  ASSERT_TRUE(wasm_handle_baseline && wasm_handle_baseline->wasm());

  // Check if it ran for baseline root context
  EXPECT_TRUE(TestContext::isGlobalLogged("onConfigure: " + root_ids[0]));
  // For each create Wasm, canary should be done.
  EXPECT_EQ(canary_count, 1);

  bool first = true;
  std::unordered_set<std::shared_ptr<WasmHandleBase>> reference_holder;

  for (const auto &root_id : root_ids) {
    for (const auto &vm_id : vm_ids) {
      for (const auto &vm_config : vm_configs) {
        for (const auto &plugin_key : plugin_keys) {
          for (const auto &plugin_config : plugin_configs) {
            canary_count = 0;
            TestContext::resetGlobalLog();
            WasmHandleFactory wasm_handle_factory_comp =
                [this, vm_id,
                 vm_config](std::string_view vm_key) -> std::shared_ptr<WasmHandleBase> {
              auto base_wasm = std::make_shared<TestWasm>(
                  makeVm(engine_), std::unordered_map<std::string, std::string>(), vm_id, vm_config,
                  vm_key);
              return std::make_shared<WasmHandleBase>(base_wasm);
            };
            const auto plugin_comp = std::make_shared<PluginBase>(
                plugin_name, root_id, vm_id, engine_, plugin_config, fail_open, plugin_key);
            const auto vm_key = makeVmKey(vm_id, vm_config, "common_code");
            // Create a base Wasm by createWasm.
            auto wasm_handle_comp =
                createWasm(vm_key, source, plugin_comp, wasm_handle_factory_comp,
                           wasm_handle_clone_factory_for_canary, false);
            // Validate that canarying is cached for the first baseline plugin variant.
            if (first) {
              first = false;
              EXPECT_EQ(canary_count, 0);
            } else {
              // For each create Wasm, canary should be done.
              EXPECT_EQ(canary_count, 1);
              EXPECT_TRUE(TestContext::isGlobalLogged("onConfigure: " + root_id));
            }

            if (plugin_config.empty()) {
              // canary_check.wasm should raise the error at `onConfigure` in canary when the
              // `plugin_config` is empty string.
              EXPECT_EQ(wasm_handle_comp, nullptr);
              continue;
            }

            ASSERT_TRUE(wasm_handle_comp && wasm_handle_comp->wasm());
            // Keep the reference of wasm_handle_comp in order to utilize the WasmHandleBase
            // cache of createWasm. If we don't keep the reference, WasmHandleBase and VM will be
            // destroyed for each iteration.
            reference_holder.insert(wasm_handle_comp);

            // Wasm VM is unique for vm_key.
            if (vm_key == vm_key_baseline) {
              EXPECT_EQ(wasm_handle_baseline->wasm(), wasm_handle_comp->wasm());
            } else {
              EXPECT_NE(wasm_handle_baseline->wasm(), wasm_handle_comp->wasm());
            }

            // plugin->key() is unique for root_id + plugin_config + plugin_key.
            // plugin->key() is used as an identifier of local-specific plugins as well.
            if (root_id == root_ids[0] && plugin_config == plugin_configs[0] &&
                plugin_key == plugin_keys[0]) {
              EXPECT_EQ(plugin_baseline->key(), plugin_comp->key());
            } else {
              EXPECT_NE(plugin_baseline->key(), plugin_comp->key());
            }
          }
        }
      }
    }
  }
}

// Check that there are no stale thread-local cache keys (eventually)
TEST_P(TestVm, CleanupThreadLocalCacheKeys) {
  const auto *const plugin_name = "plugin_name";
  const auto *const root_id = "root_id";
  const auto *const vm_id = "vm_id";
  const auto *const vm_config = "vm_config";
  const auto *const plugin_config = "plugin_config";
  const auto fail_open = false;

  WasmHandleFactory wasm_handle_factory =
      [this, vm_id, vm_config](std::string_view vm_key) -> std::shared_ptr<WasmHandleBase> {
    auto base_wasm = std::make_shared<WasmBase>(makeVm(engine_), vm_id, vm_config, vm_key,
                                                std::unordered_map<std::string, std::string>{},
                                                AllowedCapabilitiesMap{});
    return std::make_shared<WasmHandleBase>(base_wasm);
  };

  WasmHandleCloneFactory wasm_handle_clone_factory =
      [this](const std::shared_ptr<WasmHandleBase> &base_wasm_handle)
      -> std::shared_ptr<WasmHandleBase> {
    auto wasm = std::make_shared<WasmBase>(
        base_wasm_handle, [this]() -> std::unique_ptr<WasmVm> { return makeVm(engine_); });
    return std::make_shared<WasmHandleBase>(wasm);
  };

  PluginHandleFactory plugin_handle_factory =
      [](const std::shared_ptr<WasmHandleBase> &base_wasm,
         const std::shared_ptr<PluginBase> &plugin) -> std::shared_ptr<PluginHandleBase> {
    return std::make_shared<PluginHandleBase>(base_wasm, plugin);
  };

  // Read the minimal loadable binary.
  auto source = readTestWasmFile("abi_export.wasm");

  // Simulate a plugin lifetime.
  const auto plugin1 = std::make_shared<PluginBase>(plugin_name, root_id, vm_id, engine_,
                                                    plugin_config, fail_open, "plugin_1");
  auto base_wasm_handle1 =
      createWasm("vm_1", source, plugin1, wasm_handle_factory, wasm_handle_clone_factory, false);
  ASSERT_TRUE(base_wasm_handle1 && base_wasm_handle1->wasm());

  auto local_plugin1 = getOrCreateThreadLocalPlugin(
      base_wasm_handle1, plugin1, wasm_handle_clone_factory, plugin_handle_factory);
  ASSERT_TRUE(local_plugin1 && local_plugin1->plugin());
  local_plugin1.reset();

  auto stale_plugins_keys = staleLocalPluginsKeysForTesting();
  EXPECT_EQ(1, stale_plugins_keys.size());

  // Now we create another plugin with a slightly different key and expect that there are no stale
  // thread-local cache entries.
  const auto plugin2 = std::make_shared<PluginBase>(plugin_name, root_id, vm_id, engine_,
                                                    plugin_config, fail_open, "plugin_2");
  auto local_plugin2 = getOrCreateThreadLocalPlugin(
      base_wasm_handle1, plugin2, wasm_handle_clone_factory, plugin_handle_factory);
  ASSERT_TRUE(local_plugin2 && local_plugin2->plugin());

  stale_plugins_keys = staleLocalPluginsKeysForTesting();
  EXPECT_TRUE(stale_plugins_keys.empty());

  // Trigger deletion of the thread-local WasmVM cloned from base_wasm_handle1 by freeing objects
  // referencing it.
  local_plugin2.reset();

  auto stale_wasms_keys = staleLocalWasmsKeysForTesting();
  EXPECT_EQ(1, stale_wasms_keys.size());

  // Create another base WASM handle and invoke WASM thread-local cache key cleanup.
  auto base_wasm_handle2 =
      createWasm("vm_2", source, plugin2, wasm_handle_factory, wasm_handle_clone_factory, false);
  ASSERT_TRUE(base_wasm_handle2 && base_wasm_handle2->wasm());

  auto local_plugin3 = getOrCreateThreadLocalPlugin(
      base_wasm_handle2, plugin2, wasm_handle_clone_factory, plugin_handle_factory);
  ASSERT_TRUE(local_plugin3 && local_plugin3->plugin());

  stale_wasms_keys = staleLocalWasmsKeysForTesting();
  EXPECT_TRUE(stale_wasms_keys.empty());
}

} // namespace proxy_wasm
