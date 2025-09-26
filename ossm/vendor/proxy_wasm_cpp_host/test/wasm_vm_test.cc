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
#include <vector>

#include "include/proxy-wasm/wasm_vm.h"

#include "test/utility.h"

namespace proxy_wasm {
namespace {

INSTANTIATE_TEST_SUITE_P(WasmEngines, TestVm, testing::ValuesIn(getWasmEngines()),
                         [](const testing::TestParamInfo<std::string> &info) {
                           return info.param;
                         });

TEST_P(TestVm, Basic) {
  if (engine_ == "wasmedge") {
    EXPECT_EQ(vm_->cloneable(), proxy_wasm::Cloneable::NotCloneable);
  } else if (engine_ == "wasmtime" || engine_ == "v8" || engine_ == "wamr") {
    EXPECT_EQ(vm_->cloneable(), proxy_wasm::Cloneable::CompiledBytecode);
  } else if (engine_ == "wavm") {
    EXPECT_EQ(vm_->cloneable(), proxy_wasm::Cloneable::InstantiatedModule);
  } else {
    FAIL();
  }
  EXPECT_EQ(vm_->getEngineName(), engine_);
}

TEST_P(TestVm, Memory) {
  auto source = readTestWasmFile("abi_export.wasm");
  ASSERT_TRUE(vm_->load(source, {}, {}));
  ASSERT_TRUE(vm_->link(""));

  Word word;
  ASSERT_TRUE(vm_->setWord(0x2000, Word(100)));
  ASSERT_TRUE(vm_->getWord(0x2000, &word));
  ASSERT_EQ(100, word.u64_);

  uint32_t data[2] = {htowasm(static_cast<uint32_t>(-1), vm_->usesWasmByteOrder()),
                      htowasm(200, vm_->usesWasmByteOrder())};
  ASSERT_TRUE(vm_->setMemory(0x200, sizeof(int32_t) * 2, static_cast<void *>(data)));
  ASSERT_TRUE(vm_->getWord(0x200, &word));
  ASSERT_EQ(-1, static_cast<int32_t>(word.u64_));
  ASSERT_TRUE(vm_->getWord(0x204, &word));
  ASSERT_EQ(200, static_cast<int32_t>(word.u64_));
}

TEST_P(TestVm, Clone) {
  if (vm_->cloneable() == proxy_wasm::Cloneable::NotCloneable) {
    return;
  }
  auto source = readTestWasmFile("abi_export.wasm");
  ASSERT_TRUE(vm_->load(source, {}, {}));
  ASSERT_TRUE(vm_->link(""));
  const auto address = 0x2000;
  Word word;
  {
    auto clone = vm_->clone();
    ASSERT_TRUE(clone != nullptr);
    ASSERT_NE(vm_, clone);
    if (clone->cloneable() != proxy_wasm::Cloneable::InstantiatedModule) {
      ASSERT_TRUE(clone->link(""));
    }

    ASSERT_TRUE(clone->setWord(address, Word(100)));
    ASSERT_TRUE(clone->getWord(address, &word));
    ASSERT_EQ(100, word.u64_);
  }

  // check memory arrays are not overrapped
  ASSERT_TRUE(vm_->getWord(address, &word));
  ASSERT_NE(100, word.u64_);
}

#if defined(__linux__) && defined(__x86_64__)

TEST_P(TestVm, CloneUntilOutOfMemory) {
  if (vm_->cloneable() == proxy_wasm::Cloneable::NotCloneable) {
    return;
  }
  if (engine_ == "wavm") {
    // TODO(PiotrSikora): Figure out why this fails on the CI.
    return;
  }

  auto source = readTestWasmFile("abi_export.wasm");
  ASSERT_TRUE(vm_->load(source, {}, {}));
  ASSERT_TRUE(vm_->link(""));

  size_t max_clones = 100000;
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
  max_clones = 1000;
#endif
#endif

  std::vector<std::unique_ptr<WasmVm>> clones;
  for (size_t i = 0; i < max_clones; i++) {
    auto clone = vm_->clone();
    if (clone == nullptr) {
      break;
    }
    if (clone->cloneable() != proxy_wasm::Cloneable::InstantiatedModule) {
      if (!clone->link("")) {
        break;
      }
    }
    // Prevent clone from droping out of scope and freeing memory.
    clones.push_back(std::move(clone));
  }

  size_t min_clones = 1000;
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
  min_clones = 100;
#endif
#endif
  EXPECT_GE(clones.size(), min_clones);
}

#endif

} // namespace
} // namespace proxy_wasm
