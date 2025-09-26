// Copyright 2025 Google LLC
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
TEST_P(TestVm, AllowOnHeadersStopIteration) {
  // Read the wasm source.
  auto source = readTestWasmFile("stop_iteration.wasm");
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

  // Create root context, call onStart() and onConfigure()
  ContextBase *root_context = wasm->start(plugin);
  ASSERT_TRUE(root_context != nullptr);
  ASSERT_TRUE(wasm->configure(root_context, plugin));

  auto wasm_handle = std::make_shared<WasmHandleBase>(wasm);
  auto plugin_handle = std::make_shared<PluginHandleBase>(wasm_handle, plugin);

  // By default, stream context onRequestHeaders and onResponseHeaders
  // translates FilterHeadersStatus::StopIteration to
  // FilterHeadersStatus::StopAllIterationAndWatermark.
  {
    auto stream_context = TestContext(wasm.get(), root_context->id(), plugin_handle);
    stream_context.onCreate();
    EXPECT_EQ(stream_context.onRequestHeaders(/*headers=*/0, /*end_of_stream=*/false),
              FilterHeadersStatus::StopAllIterationAndWatermark);
    EXPECT_EQ(stream_context.onResponseHeaders(/*headers=*/0, /*end_of_stream=*/false),
              FilterHeadersStatus::StopAllIterationAndWatermark);
    stream_context.onDone();
    stream_context.onDelete();
  }
  ASSERT_FALSE(wasm->isFailed());

  // Create a stream context that propagates FilterHeadersStatus::StopIteration.
  {
    auto stream_context = TestContext(wasm.get(), root_context->id(), plugin_handle);
    stream_context.set_allow_on_headers_stop_iteration(true);
    stream_context.onCreate();
    EXPECT_EQ(stream_context.onRequestHeaders(/*headers=*/0, /*end_of_stream=*/false),
              FilterHeadersStatus::StopIteration);
    EXPECT_EQ(stream_context.onResponseHeaders(/*headers=*/0, /*end_of_stream=*/false),
              FilterHeadersStatus::StopIteration);
    stream_context.onDone();
    stream_context.onDelete();
  }
  ASSERT_FALSE(wasm->isFailed());
}

} // namespace proxy_wasm
