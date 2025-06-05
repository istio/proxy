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

#include "include/proxy-wasm/vm_id_handle.h"

#include "gtest/gtest.h"

namespace proxy_wasm {

TEST(VmIdHandle, Basic) {
  const auto *vm_id = "vm_id";
  auto handle = getVmIdHandle(vm_id);
  EXPECT_TRUE(handle);

  bool called = false;
  registerVmIdHandleCallback([&called](std::string_view /*vm_id*/) { called = true; });

  handle.reset();
  EXPECT_TRUE(called);

  handle = getVmIdHandle(vm_id);
  auto handle2 = getVmIdHandle(vm_id);
  called = false;
  handle.reset();
  EXPECT_FALSE(called);
  handle2.reset();
  EXPECT_TRUE(called);
}

} // namespace proxy_wasm
