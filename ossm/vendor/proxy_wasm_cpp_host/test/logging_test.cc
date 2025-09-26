// Copyright 2023 Google LLC
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

#include "gtest/gtest.h"
#include "include/proxy-wasm/wasm.h"
#include "test/utility.h"

namespace proxy_wasm {

INSTANTIATE_TEST_SUITE_P(WasmEngines, TestVm, testing::ValuesIn(getWasmEngines()),
                         [](const testing::TestParamInfo<std::string> &info) {
                           return info.param;
                         });

// TestVm is parameterized for each engine and creates a VM on construction.
TEST_P(TestVm, HttpLogging) {
  // Read the wasm source.
  auto source = readTestWasmFile("http_logging.wasm");
  ASSERT_FALSE(source.empty());

  // Create a WasmBase and load the plugin.
  auto wasm = std::make_shared<TestWasm>(std::move(vm_));
  ASSERT_TRUE(wasm->load(source, /*allow_precompiled=*/false));
  ASSERT_TRUE(wasm->initialize());

  // Create a plugin.
  const auto plugin = std::make_shared<PluginBase>(
      /*name=*/"test", /*root_id=*/"", /*vm_id=*/"",
      /*engine=*/wasm->wasm_vm()->getEngineName(), /*plugin_config=*/"",
      /*fail_open=*/false, /*key=*/"");

  // Create root context, call onStart().
  ContextBase *root_context = wasm->start(plugin);
  ASSERT_TRUE(root_context != nullptr);

  // On the root context, call onConfigure().
  ASSERT_TRUE(wasm->configure(root_context, plugin));

  // Create a stream context.
  {
    auto wasm_handle = std::make_shared<WasmHandleBase>(wasm);
    auto plugin_handle = std::make_shared<PluginHandleBase>(wasm_handle, plugin);
    auto stream_context = TestContext(wasm.get(), root_context->id(), plugin_handle);
    stream_context.onCreate();
    EXPECT_TRUE(stream_context.isLogged("onCreate called"));
    stream_context.onRequestHeaders(/*headers=*/0, /*end_of_stream=*/false);
    EXPECT_TRUE(stream_context.isLogged("onRequestHeaders called"));
    stream_context.onResponseHeaders(/*headers=*/0, /*end_of_stream=*/false);
    EXPECT_TRUE(stream_context.isLogged("onResponseHeaders called"));
    stream_context.onDone();
    EXPECT_TRUE(stream_context.isLogged("onDone called"));
    stream_context.onDelete();
    EXPECT_TRUE(stream_context.isLogged("onDelete called"));
  }
  EXPECT_FALSE(wasm->isFailed());
}

} // namespace proxy_wasm
