// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "runtime/internal/runtime_env.h"

#include <atomic>
#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/synchronization/mutex.h"
#include "internal/noop_delete.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/message.h"

namespace cel::runtime_internal {

RuntimeEnv::KeepAlives::~KeepAlives() {
  while (!deque.empty()) {
    deque.pop_back();
  }
}

google::protobuf::MessageFactory* absl_nonnull RuntimeEnv::MutableMessageFactory() const {
  google::protobuf::MessageFactory* absl_nullable shared_message_factory =
      message_factory_ptr.load(std::memory_order_relaxed);
  if (shared_message_factory != nullptr) {
    return shared_message_factory;
  }
  absl::MutexLock lock(&message_factory_mutex);
  shared_message_factory = message_factory_ptr.load(std::memory_order_relaxed);
  if (shared_message_factory == nullptr) {
    if (descriptor_pool.get() == google::protobuf::DescriptorPool::generated_pool()) {
      // Using the generated descriptor pool, just use the generated message
      // factory.
      message_factory = std::shared_ptr<google::protobuf::MessageFactory>(
          google::protobuf::MessageFactory::generated_factory(),
          internal::NoopDeleteFor<google::protobuf::MessageFactory>());
    } else {
      auto dynamic_message_factory =
          std::make_shared<google::protobuf::DynamicMessageFactory>();
      // Ensure we do not delegate to the generated factory, if the default
      // every changes. We prefer being hermetic.
      dynamic_message_factory->SetDelegateToGeneratedFactory(false);
      message_factory = std::move(dynamic_message_factory);
    }
    shared_message_factory = message_factory.get();
    message_factory_ptr.store(shared_message_factory,
                              std::memory_order_seq_cst);
  }
  return shared_message_factory;
}

void RuntimeEnv::KeepAlive(std::shared_ptr<const void> keep_alive) {
  if (keep_alive == nullptr) {
    return;
  }
  keep_alives.deque.push_back(std::move(keep_alive));
}

}  // namespace cel::runtime_internal
