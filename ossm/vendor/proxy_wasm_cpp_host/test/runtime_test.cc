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

#include "gtest/gtest.h"

#include <memory>
#include <string>
#include <thread>

#include "include/proxy-wasm/context.h"
#include "include/proxy-wasm/limits.h"
#include "include/proxy-wasm/wasm.h"

#include "test/utility.h"

namespace proxy_wasm {
namespace {

INSTANTIATE_TEST_SUITE_P(WasmEngines, TestVm, testing::ValuesIn(getWasmEngines()),
                         [](const testing::TestParamInfo<std::string> &info) {
                           return info.param;
                         });

TEST_P(TestVm, BadSignature) {
  auto source = readTestWasmFile("clock.wasm");
  ASSERT_FALSE(source.empty());
  auto wasm = TestWasm(std::move(vm_));
  ASSERT_TRUE(wasm.load(source, false));
  ASSERT_TRUE(wasm.initialize());

  WasmCallVoid<0> non_existent;
  wasm.wasm_vm()->getFunction("non_existent", &non_existent);
  EXPECT_TRUE(non_existent == nullptr);

  WasmCallWord<2> bad_signature_run;
  wasm.wasm_vm()->getFunction("run", &bad_signature_run);
  EXPECT_TRUE(bad_signature_run == nullptr);

  WasmCallVoid<0> run;
  wasm.wasm_vm()->getFunction("run", &run);
  ASSERT_TRUE(run != nullptr);
}

TEST_P(TestVm, StraceLogLevel) {
  if (engine_ == "wavm") {
    // TODO(mathetake): strace is yet to be implemented for WAVM.
    // See https://github.com/proxy-wasm/proxy-wasm-cpp-host/issues/120.
    return;
  }

  auto source = readTestWasmFile("clock.wasm");
  ASSERT_FALSE(source.empty());
  auto wasm = TestWasm(std::move(vm_));
  ASSERT_TRUE(wasm.load(source, false));
  ASSERT_TRUE(wasm.initialize());

  WasmCallVoid<0> run;
  wasm.wasm_vm()->getFunction("run", &run);
  ASSERT_TRUE(run != nullptr);

  auto *host = dynamic_cast<TestIntegration *>(wasm.wasm_vm()->integration().get());
  host->setLogLevel(LogLevel::info);
  run(wasm.vm_context());
  EXPECT_TRUE(host->isTraceLogEmpty());

  host->setLogLevel(LogLevel::trace);
  run(wasm.vm_context());
  EXPECT_TRUE(host->isTraceLogged("[host->vm] run()"));
  EXPECT_TRUE(host->isTraceLogged("[vm->host] wasi_snapshot_preview1.clock_time_get(1, 1, "));
  EXPECT_TRUE(host->isTraceLogged("[vm<-host] wasi_snapshot_preview1.clock_time_get return: 0"));
  EXPECT_TRUE(host->isTraceLogged("[host<-vm] run return: void"));
}

TEST_P(TestVm, TerminateExecution) {
  // TODO(chaoqin-li1123): implement execution termination for other runtime.
  if (engine_ != "v8") {
    return;
  }
  auto source = readTestWasmFile("resource_limits.wasm");
  ASSERT_FALSE(source.empty());
  auto wasm = TestWasm(std::move(vm_));
  ASSERT_TRUE(wasm.load(source, false));
  ASSERT_TRUE(wasm.initialize());

  std::thread terminate([&]() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    wasm.wasm_vm()->terminate();
  });

  WasmCallVoid<0> infinite_loop;
  wasm.wasm_vm()->getFunction("infinite_loop", &infinite_loop);
  ASSERT_TRUE(infinite_loop != nullptr);
  infinite_loop(wasm.vm_context());

  terminate.join();

  // Check integration logs.
  auto *host = dynamic_cast<TestIntegration *>(wasm.wasm_vm()->integration().get());
  EXPECT_TRUE(host->isErrorLogged("Function: infinite_loop failed"));
  EXPECT_TRUE(host->isErrorLogged("termination_exception"));
}

TEST_P(TestVm, WasmMemoryLimit) {
  // TODO(PiotrSikora): enforce memory limits in other engines.
  if (engine_ != "v8") {
    return;
  }
  auto source = readTestWasmFile("resource_limits.wasm");
  ASSERT_FALSE(source.empty());
  auto wasm = TestWasm(std::move(vm_));
  ASSERT_TRUE(wasm.load(source, false));
  ASSERT_TRUE(wasm.initialize());

  WasmCallVoid<0> infinite_memory;
  wasm.wasm_vm()->getFunction("infinite_memory", &infinite_memory);
  ASSERT_TRUE(infinite_memory != nullptr);
  infinite_memory(wasm.vm_context());

  EXPECT_GE(wasm.wasm_vm()->getMemorySize(), PROXY_WASM_HOST_MAX_WASM_MEMORY_SIZE_BYTES * 0.95);
  EXPECT_LE(wasm.wasm_vm()->getMemorySize(), PROXY_WASM_HOST_MAX_WASM_MEMORY_SIZE_BYTES);

  // Check integration logs.
  auto *host = dynamic_cast<TestIntegration *>(wasm.wasm_vm()->integration().get());
  EXPECT_TRUE(host->isErrorLogged("Function: infinite_memory failed"));
  // Trap message
  if (engine_ == "wavm") {
    EXPECT_TRUE(host->isErrorLogged("wavm.reachedUnreachable"));
  } else {
    EXPECT_TRUE(host->isErrorLogged("unreachable"));
  }
  // Backtrace
  if (engine_ == "v8") {
    EXPECT_TRUE(host->isErrorLogged("Proxy-Wasm plugin in-VM backtrace:"));
    EXPECT_TRUE(host->isErrorLogged("rg_oom"));
    EXPECT_TRUE(host->isErrorLogged(" - alloc::alloc::handle_alloc_error"));
  }
}

TEST_P(TestVm, Trap) {
  auto source = readTestWasmFile("trap.wasm");
  ASSERT_FALSE(source.empty());
  auto wasm = TestWasm(std::move(vm_));
  ASSERT_TRUE(wasm.load(source, false));
  ASSERT_TRUE(wasm.initialize());

  WasmCallVoid<0> trigger;
  wasm.wasm_vm()->getFunction("trigger", &trigger);
  ASSERT_TRUE(trigger != nullptr);
  trigger(wasm.vm_context());

  // Check integration logs.
  auto *host = dynamic_cast<TestIntegration *>(wasm.wasm_vm()->integration().get());
  EXPECT_TRUE(host->isErrorLogged("Function: trigger failed"));
  // Trap message
  if (engine_ == "wavm") {
    EXPECT_TRUE(host->isErrorLogged("wavm.reachedUnreachable"));
  } else {
    EXPECT_TRUE(host->isErrorLogged("unreachable"));
  }
  // Backtrace
  if (engine_ == "v8") {
    EXPECT_TRUE(host->isErrorLogged("Proxy-Wasm plugin in-VM backtrace:"));
    EXPECT_TRUE(host->isErrorLogged(" - std::panicking::begin_panic"));
    EXPECT_TRUE(host->isErrorLogged(" - trigger"));
  }
}

TEST_P(TestVm, Trap2) {
  auto source = readTestWasmFile("trap.wasm");
  ASSERT_FALSE(source.empty());
  auto wasm = TestWasm(std::move(vm_));
  ASSERT_TRUE(wasm.load(source, false));
  ASSERT_TRUE(wasm.initialize());

  WasmCallWord<1> trigger2;
  wasm.wasm_vm()->getFunction("trigger2", &trigger2);
  ASSERT_TRUE(trigger2 != nullptr);
  trigger2(wasm.vm_context(), 0);

  // Check integration logs.
  auto *host = dynamic_cast<TestIntegration *>(wasm.wasm_vm()->integration().get());
  EXPECT_TRUE(host->isErrorLogged("Function: trigger2 failed"));
  // Trap message
  if (engine_ == "wavm") {
    EXPECT_TRUE(host->isErrorLogged("wavm.reachedUnreachable"));
  } else {
    EXPECT_TRUE(host->isErrorLogged("unreachable"));
  }
  // Backtrace
  if (engine_ == "v8") {
    EXPECT_TRUE(host->isErrorLogged("Proxy-Wasm plugin in-VM backtrace:"));
    EXPECT_TRUE(host->isErrorLogged(" - std::panicking::begin_panic"));
    EXPECT_TRUE(host->isErrorLogged(" - trigger2"));
  }
}

class TestCounterContext : public TestContext {
public:
  TestCounterContext(WasmBase *wasm) : TestContext(wasm) {}

  void increment() { counter++; }
  size_t getCount() { return counter; }

private:
  size_t counter = 0;
};

class TestCounterWasm : public TestWasm {
public:
  TestCounterWasm(std::unique_ptr<WasmVm> wasm_vm) : TestWasm(std::move(wasm_vm)) {}

  ContextBase *createVmContext() override { return new TestCounterContext(this); };
};

void callback() {
  auto *context = dynamic_cast<TestCounterContext *>(contextOrEffectiveContext());
  context->increment();
}

Word callback2(Word val) { return val + 100; }

TEST_P(TestVm, Callback) {
  auto source = readTestWasmFile("callback.wasm");
  ASSERT_FALSE(source.empty());
  auto wasm = TestCounterWasm(std::move(vm_));
  ASSERT_TRUE(wasm.load(source, false));

  wasm.wasm_vm()->registerCallback(
      "env", "callback", &callback,
      &ConvertFunctionWordToUint32<decltype(callback), callback>::convertFunctionWordToUint32);

  wasm.wasm_vm()->registerCallback(
      "env", "callback2", &callback2,
      &ConvertFunctionWordToUint32<decltype(callback2), callback2>::convertFunctionWordToUint32);

  ASSERT_TRUE(wasm.initialize());

  WasmCallVoid<0> run;
  wasm.wasm_vm()->getFunction("run", &run);
  ASSERT_TRUE(run != nullptr);
  for (auto i = 0; i < 5; i++) {
    run(wasm.vm_context());
  }
  auto *context = dynamic_cast<TestCounterContext *>(wasm.vm_context());
  EXPECT_EQ(context->getCount(), 5);

  WasmCallWord<1> run2;
  wasm.wasm_vm()->getFunction("run2", &run2);
  ASSERT_TRUE(run2 != nullptr);
  Word res = run2(wasm.vm_context(), Word{0});
  EXPECT_EQ(res.u32(), 100100); // 10000 (global) + 100 (in callback)
}

} // namespace
} // namespace proxy_wasm
