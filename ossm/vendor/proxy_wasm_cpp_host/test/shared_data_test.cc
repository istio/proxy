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

#include "src/shared_data.h"

#include <thread>

#include "gtest/gtest.h"

#include "include/proxy-wasm/vm_id_handle.h"

namespace proxy_wasm {

TEST(SharedData, SingleThread) {
  SharedData shared_data(false);
  std::string_view vm_id = "id";

  // Validate we get an 'Ok' response when fetching keys before anything
  // is initialized.
  std::vector<std::string> keys;
  EXPECT_EQ(WasmResult::Ok, shared_data.keys(vm_id, &keys));
  EXPECT_EQ(0, keys.size());

  // Validate that we clear the result set
  std::vector<std::string> nonEmptyKeys(2);
  nonEmptyKeys[0] = "valueA";
  nonEmptyKeys[1] = "valueB";
  EXPECT_EQ(WasmResult::Ok, shared_data.keys(vm_id, &nonEmptyKeys));
  EXPECT_EQ(0, nonEmptyKeys.size());

  std::pair<std::string, uint32_t> result;
  EXPECT_EQ(WasmResult::NotFound, shared_data.get("non-exist", "non-exists", &result));

  std::string_view key = "key";
  std::string_view value = "1";
  EXPECT_EQ(WasmResult::Ok, shared_data.set(vm_id, key, value, 0));
  // ok for cas == 0
  EXPECT_EQ(WasmResult::Ok, shared_data.set(vm_id, key, value, 0));
  EXPECT_EQ(WasmResult::CasMismatch, shared_data.set(vm_id, key, value, 100));

  EXPECT_EQ(WasmResult::Ok, shared_data.get(vm_id, key, &result));
  EXPECT_EQ(value, result.first);
  value = "2";
  EXPECT_EQ(result.second, 2);

  EXPECT_EQ(WasmResult::Ok, shared_data.set(vm_id, key, value, result.second));
  EXPECT_EQ(WasmResult::Ok, shared_data.get(vm_id, key, &result));
  EXPECT_EQ(value, result.first);
  EXPECT_EQ(result.second, 3);

  EXPECT_EQ(WasmResult::Ok, shared_data.keys(vm_id, &keys));
  EXPECT_EQ(1, keys.size());
  EXPECT_EQ(key, keys[0]);

  keys.clear();
  EXPECT_EQ(WasmResult::CasMismatch, shared_data.remove(vm_id, key, 911, nullptr));
  EXPECT_EQ(WasmResult::Ok, shared_data.keys(vm_id, &keys));
  EXPECT_EQ(1, keys.size());

  EXPECT_EQ(WasmResult::Ok, shared_data.remove(vm_id, key, 0, nullptr));
  EXPECT_EQ(WasmResult::NotFound, shared_data.get(vm_id, key, &result));

  EXPECT_EQ(WasmResult::NotFound, shared_data.remove(vm_id, "non-existent_key", 0, nullptr));

  EXPECT_EQ(WasmResult::Ok, shared_data.set(vm_id, key, value, 0));
  EXPECT_EQ(WasmResult::Ok, shared_data.set(vm_id, key, value, 0));
  EXPECT_EQ(WasmResult::Ok, shared_data.get(vm_id, key, &result));

  uint32_t expectedCasValue = result.second;

  std::pair<std::string, uint32_t> removeResult;
  EXPECT_EQ(WasmResult::Ok, shared_data.remove(vm_id, key, 0, &removeResult));
  EXPECT_EQ(value, removeResult.first);
  EXPECT_EQ(removeResult.second, expectedCasValue);
}

void incrementData(SharedData *shared_data, std::string_view vm_id, std::string_view key) {
  std::pair<std::string, uint32_t> result;
  for (auto i = 0; i < 10; i++) {
    while (true) {
      if (WasmResult::Ok != shared_data->get(vm_id, key, &result)) {
        continue;
      }
      if (WasmResult::Ok != shared_data->set(vm_id, key, result.first + "a", result.second)) {
        continue;
      };
      break;
    }
  }
}

TEST(SharedData, Concurrent) {
  SharedData shared_data(false);
  std::pair<std::string, uint32_t> result;

  std::string_view vm_id = "id";
  std::string_view key = "key";
  std::string_view value;
  EXPECT_EQ(WasmResult::Ok, shared_data.set(vm_id, key, value, 0));
  std::thread first(incrementData, &shared_data, vm_id, key);
  std::thread second(incrementData, &shared_data, vm_id, key);
  first.join();
  second.join();

  EXPECT_EQ(WasmResult::Ok, shared_data.get(vm_id, key, &result));
  EXPECT_EQ(result.first, "aaaaaaaaaaaaaaaaaaaa");
}

TEST(SharedData, DeleteByVmId) {
  SharedData shared_data(false);
  std::string_view vm_id = "id";
  std::string_view key = "key";
  std::string_view value;
  EXPECT_EQ(WasmResult::Ok, shared_data.set(vm_id, key, value, 0));

  shared_data.deleteByVmId(vm_id);
  std::pair<std::string, uint32_t> result;
  EXPECT_EQ(WasmResult::NotFound, shared_data.get(vm_id, key, &result));
}

TEST(SharedData, VmIdHandleCleanup) {
  SharedData shared_data;
  std::string_view vm_id = "proxy_wasm_shared_data_test";
  auto handle = getVmIdHandle(vm_id);
  std::string_view key = "key";
  std::string_view value = "this is value";
  EXPECT_EQ(WasmResult::Ok, shared_data.set(vm_id, key, value, 0));

  std::pair<std::string, uint32_t> result;
  EXPECT_EQ(WasmResult::Ok, shared_data.get(vm_id, key, &result));
  EXPECT_EQ(value, result.first);

  handle.reset();
  EXPECT_EQ(WasmResult::NotFound, shared_data.get(vm_id, key, &result));
}

} // namespace proxy_wasm
