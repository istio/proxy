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

#include "src/shared_queue.h"

#include <thread>

#include "gtest/gtest.h"

#include "include/proxy-wasm/vm_id_handle.h"

namespace proxy_wasm {

TEST(SharedQueue, NextQueueToken) {
  SharedQueue shared_queue(false);
  for (auto i = 1; i < 5; i++) {
    EXPECT_EQ(i, shared_queue.nextQueueToken());
  }
  EXPECT_EQ(5, shared_queue.registerQueue("a", "b", 1, nullptr, "c"));
}

TEST(SharedQueue, SingleThread) {
  SharedQueue shared_queue(false);
  std::string_view vm_id = "id";
  std::string_view vm_key = "vm_key";
  std::string_view queue_name = "name";
  uint32_t context_id = 1;

  for (auto i = 0; i < 3; i++) {
    // same token
    EXPECT_EQ(1, shared_queue.registerQueue(vm_id, queue_name, context_id, nullptr, vm_key));
  }
  EXPECT_EQ(1, shared_queue.resolveQueue(vm_id, queue_name));
  EXPECT_EQ(0, shared_queue.resolveQueue(vm_id, "non-exist"));
  EXPECT_EQ(0, shared_queue.resolveQueue("non-exist", queue_name));

  bool called = false;
  std::function<void(std::function<void()>)> call_on_thread =
      [&called](const std::function<void()> &f) {
        called = true;
        f(); // TODO(mathetake): test whether onQueueReady is called with mock WasmHandle
      };
  queue_name = "name2";
  auto token = shared_queue.registerQueue(vm_id, queue_name, context_id, call_on_thread, vm_key);
  EXPECT_EQ(2, token);

  std::string data;
  EXPECT_EQ(WasmResult::NotFound, shared_queue.dequeue(0, &data));
  EXPECT_EQ(WasmResult::Empty, shared_queue.dequeue(token, &data));

  std::string value = "value";
  EXPECT_EQ(WasmResult::NotFound, shared_queue.enqueue(0, value));
  EXPECT_EQ(WasmResult::Ok, shared_queue.enqueue(token, value));
  EXPECT_TRUE(called);

  EXPECT_EQ(WasmResult::Ok, shared_queue.dequeue(token, &data));
  EXPECT_EQ(data, "value");
}

void enqueueData(SharedQueue *shared_queue, uint32_t token, size_t num) {
  for (size_t i = 0; i < num; i++) {
    shared_queue->enqueue(token, "a");
  }
}

void dequeueData(SharedQueue *shared_queue, uint32_t token, size_t *dequeued_count) {
  std::string data;
  while (WasmResult::Ok == shared_queue->dequeue(token, &data)) {
    (*dequeued_count)++;
  }
}

TEST(SharedQueue, Concurrent) {
  SharedQueue shared_queue(false);
  std::string_view vm_id = "id";
  std::string_view vm_key = "vm_key";
  std::string_view queue_name = "name";
  uint32_t context_id = 1;

  auto queued_count = 0;
  std::mutex mutex;
  std::function<void(std::function<void()>)> call_on_thread =
      [&mutex, &queued_count](const std::function<void()> &f) {
        std::lock_guard<std::mutex> lock(mutex);
        queued_count++;
        f(); // TODO(mathetake): test whether onQueueReady is called with mock WasmHandle
      };
  auto token = shared_queue.registerQueue(vm_id, queue_name, context_id, call_on_thread, vm_key);
  EXPECT_EQ(1, token);

  std::thread enqueue_first(enqueueData, &shared_queue, token, 100);
  std::thread enqueue_second(enqueueData, &shared_queue, token, 100);
  enqueue_first.join();
  enqueue_second.join();
  EXPECT_EQ(queued_count, 200);

  size_t first_cnt = 0;
  size_t second_cnt = 0;
  std::thread dequeue_first(dequeueData, &shared_queue, token, &first_cnt);
  std::thread dequeue_second(dequeueData, &shared_queue, token, &second_cnt);
  dequeue_first.join();
  dequeue_second.join();
  EXPECT_EQ(first_cnt + second_cnt, 200);
}

TEST(SharedQueue, DeleteByVmId) {
  SharedQueue shared_queue(false);
  const auto *vm_id_1 = "id_1";
  const auto *vm_id_2 = "id_2";
  std::string_view vm_key = "vm_key";
  uint32_t context_id = 1;
  auto queue_num_per_vm = 3;

  for (auto i = 1; i < queue_num_per_vm; i++) {
    EXPECT_EQ(i,
              shared_queue.registerQueue(vm_id_1, std::to_string(i), context_id, nullptr, vm_key));
    EXPECT_EQ(i, shared_queue.resolveQueue(vm_id_1, std::to_string(i)));
  }

  for (auto i = queue_num_per_vm; i < 2 * queue_num_per_vm; i++) {
    EXPECT_EQ(i,
              shared_queue.registerQueue(vm_id_2, std::to_string(i), context_id, nullptr, vm_key));
    EXPECT_EQ(i, shared_queue.resolveQueue(vm_id_2, std::to_string(i)));
  }

  shared_queue.deleteByVmId(vm_id_1);
  for (auto i = 1; i < queue_num_per_vm; i++) {
    EXPECT_EQ(0, shared_queue.resolveQueue(vm_id_1, std::to_string(i)));
  }

  for (auto i = queue_num_per_vm; i < 2 * queue_num_per_vm; i++) {
    EXPECT_EQ(i, shared_queue.resolveQueue(vm_id_2, std::to_string(i)));
  }
}

TEST(SharedQueue, VmIdHandleCleanup) {
  SharedQueue shared_queue;
  std::string_view vm_id = "proxy_wasm_shared_queue_test";
  std::string_view queue_name = "name";

  auto handle = getVmIdHandle(vm_id);
  EXPECT_EQ(1, shared_queue.registerQueue(vm_id, queue_name, 1, nullptr, "vm_key"));
  EXPECT_EQ(1, shared_queue.resolveQueue(vm_id, queue_name));

  handle.reset();
  EXPECT_EQ(0, shared_queue.resolveQueue(vm_id, queue_name));
}

} // namespace proxy_wasm
