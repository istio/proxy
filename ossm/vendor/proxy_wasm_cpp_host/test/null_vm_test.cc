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

#include "include/proxy-wasm/wasm_vm.h"

#include "include/proxy-wasm/null.h"
#include "include/proxy-wasm/null_vm_plugin.h"

#include "gtest/gtest.h"

namespace proxy_wasm {

class TestNullVmPlugin : public NullVmPlugin {
public:
  TestNullVmPlugin() = default;
  ~TestNullVmPlugin() override = default;
};

TestNullVmPlugin *test_null_vm_plugin = nullptr;

RegisterNullVmPluginFactory register_test_null_vm_plugin("test_null_vm_plugin", []() {
  auto plugin = std::make_unique<TestNullVmPlugin>();
  test_null_vm_plugin = plugin.get();
  return plugin;
});

TEST(WasmVm, Compat) {
  std::string_view foo = "foo";
  std::string bar = "bar";

  EXPECT_NE(foo, bar);
  EXPECT_EQ(foo, "foo");

  std::optional<int> o = std::nullopt;
  EXPECT_FALSE(o);

  o = 1;
  EXPECT_TRUE(o);
}

TEST(WasmVm, Word) {
  Word w(1);
  EXPECT_EQ(w.u32(), 1);
  EXPECT_EQ(sizeof(w.u32()), sizeof(uint32_t));
  EXPECT_EQ(w, 1);
  EXPECT_EQ(sizeof(w), sizeof(uint64_t));
}

class BaseVmTest : public testing::Test {
public:
  BaseVmTest() = default;
};

TEST_F(BaseVmTest, NullVmStartup) {
  auto wasm_vm = createNullVm();
  EXPECT_TRUE(wasm_vm != nullptr);
  EXPECT_TRUE(wasm_vm->getEngineName() == "null");
  EXPECT_TRUE(wasm_vm->cloneable() == Cloneable::InstantiatedModule);
  auto wasm_vm_clone = wasm_vm->clone();
  EXPECT_TRUE(wasm_vm_clone != nullptr);
  EXPECT_TRUE(wasm_vm->load("test_null_vm_plugin", {}, {}));
  EXPECT_NE(test_null_vm_plugin, nullptr);
}

TEST_F(BaseVmTest, ByteOrder) {
  auto wasm_vm = createNullVm();
  EXPECT_TRUE(wasm_vm->load("test_null_vm_plugin", {}, {}));
  EXPECT_FALSE(wasm_vm->usesWasmByteOrder());
}

} // namespace proxy_wasm
