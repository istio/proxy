// Copyright 2022 Google LLC
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

#include <memory>
#include <string>

#include "include/proxy-wasm/context.h"
#include "include/proxy-wasm/wasm.h"

#include "test/utility.h"

namespace proxy_wasm {

INSTANTIATE_TEST_SUITE_P(WasmEngines, TestVm, testing::ValuesIn(getWasmEngines()),
                         [](const testing::TestParamInfo<std::string> &info) {
                           return info.param;
                         });

TEST_P(TestVm, MallocNoHostcalls) {
  if (engine_ != "v8") {
    return;
  }
  auto source = readTestWasmFile("bad_malloc.wasm");
  ASSERT_FALSE(source.empty());
  auto wasm = TestWasm(std::move(vm_));
  ASSERT_TRUE(wasm.load(source, false));
  ASSERT_TRUE(wasm.initialize());

  uint64_t ptr = 0;
  void *result = wasm.allocMemory(0x1000, &ptr);
  EXPECT_NE(result, nullptr);
  EXPECT_FALSE(wasm.isFailed());

  // Check application logs.
  auto *context = dynamic_cast<TestContext *>(wasm.vm_context());
  EXPECT_TRUE(context->isLogEmpty());
  // Check integration logs.
  auto *integration = dynamic_cast<TestIntegration *>(wasm.wasm_vm()->integration().get());
  EXPECT_FALSE(integration->isErrorLogged("Function: proxy_on_memory_allocate failed"));
  EXPECT_FALSE(integration->isErrorLogged("restricted_callback"));
}

TEST_P(TestVm, MallocWithLog) {
  if (engine_ != "v8") {
    return;
  }
  auto source = readTestWasmFile("bad_malloc.wasm");
  ASSERT_FALSE(source.empty());
  auto wasm = TestWasm(std::move(vm_));
  ASSERT_TRUE(wasm.load(source, false));
  ASSERT_TRUE(wasm.initialize());

  uint64_t ptr = 0;
  // 0xAAAA => hostcall to proxy_log (allowed).
  void *result = wasm.allocMemory(0xAAAA, &ptr);
  EXPECT_NE(result, nullptr);
  EXPECT_FALSE(wasm.isFailed());

  // Check application logs.
  auto *context = dynamic_cast<TestContext *>(wasm.vm_context());
  EXPECT_TRUE(context->isLogged("this is fine"));
  // Check integration logs.
  auto *integration = dynamic_cast<TestIntegration *>(wasm.wasm_vm()->integration().get());
  EXPECT_FALSE(integration->isErrorLogged("Function: proxy_on_memory_allocate failed"));
  EXPECT_FALSE(integration->isErrorLogged("restricted_callback"));
}

TEST_P(TestVm, MallocWithHostcall) {
  if (engine_ != "v8") {
    return;
  }
  auto source = readTestWasmFile("bad_malloc.wasm");
  ASSERT_FALSE(source.empty());
  auto wasm = TestWasm(std::move(vm_));
  ASSERT_TRUE(wasm.load(source, false));
  ASSERT_TRUE(wasm.initialize());

  uint64_t ptr = 0;
  // 0xBBBB => hostcall to proxy_done (not allowed).
  void *result = wasm.allocMemory(0xBBBB, &ptr);
  EXPECT_EQ(result, nullptr);
  EXPECT_TRUE(wasm.isFailed());

  // Check application logs.
  auto *context = dynamic_cast<TestContext *>(wasm.vm_context());
  EXPECT_TRUE(context->isLogEmpty());
  // Check integration logs.
  auto *integration = dynamic_cast<TestIntegration *>(wasm.wasm_vm()->integration().get());
  EXPECT_TRUE(integration->isErrorLogged("Function: proxy_on_memory_allocate failed"));
  EXPECT_TRUE(integration->isErrorLogged("restricted_callback"));
}

} // namespace proxy_wasm
