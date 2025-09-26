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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_RUNTIME_ENV_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_RUNTIME_ENV_H_

#include <atomic>
#include <deque>
#include <memory>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_type_registry.h"
#include "internal/well_known_types.h"
#include "runtime/function_registry.h"
#include "runtime/type_registry.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::runtime_internal {

// Shared state used by the runtime during creation, configuration, planning,
// and evaluation. Passed around via `std::shared_ptr`.
//
// TODO(uncreated-issue/66): Make this a class.
struct RuntimeEnv final {
  explicit RuntimeEnv(absl_nonnull std::shared_ptr<const google::protobuf::DescriptorPool>
                          descriptor_pool,
                      absl_nullable std::shared_ptr<google::protobuf::MessageFactory>
                          message_factory = nullptr)
      : descriptor_pool(std::move(descriptor_pool)),
        message_factory(std::move(message_factory)),
        legacy_type_registry(this->descriptor_pool.get(),
                             this->message_factory.get()),
        type_registry(legacy_type_registry.InternalGetModernRegistry()),
        function_registry(legacy_function_registry.InternalGetRegistry()) {
    if (this->message_factory != nullptr) {
      message_factory_ptr.store(this->message_factory.get(),
                                std::memory_order_seq_cst);
    }
  }

  // Not copyable or moveable.
  RuntimeEnv(const RuntimeEnv&) = delete;
  RuntimeEnv(RuntimeEnv&&) = delete;
  RuntimeEnv& operator=(const RuntimeEnv&) = delete;
  RuntimeEnv& operator=(RuntimeEnv&&) = delete;

  // Ideally the environment would already be initialized, but things are a bit
  // awkward. This should only be called once immediately after construction.
  absl::Status Initialize() {
    return well_known_types.Initialize(descriptor_pool.get());
  }

  bool IsInitialized() const { return well_known_types.IsInitialized(); }

  ABSL_ATTRIBUTE_UNUSED
  const absl_nonnull std::shared_ptr<const google::protobuf::DescriptorPool>
      descriptor_pool;

 private:
  // These fields deal with a message factory that is lazily initialized as
  // needed. This might be called during the planning phase of an expression or
  // during evaluation. We want the ability to get the message factory when it
  // is already created to be cheap, so we use an atomic and a mutex for the
  // slow path.
  //
  // Do not access any of these fields directly, use member functions.
  mutable absl::Mutex message_factory_mutex;
  mutable absl_nullable std::shared_ptr<google::protobuf::MessageFactory> message_factory
      ABSL_GUARDED_BY(message_factory_mutex);
  // std::atomic<std::shared_ptr<?>> is not really a simple atomic, so we
  // avoid it.
  mutable std::atomic<google::protobuf::MessageFactory* absl_nullable>
      message_factory_ptr = nullptr;

  struct KeepAlives final {
    KeepAlives() = default;

    ~KeepAlives();

    // Not copyable or moveable.
    KeepAlives(const KeepAlives&) = delete;
    KeepAlives(KeepAlives&&) = delete;
    KeepAlives& operator=(const KeepAlives&) = delete;
    KeepAlives& operator=(KeepAlives&&) = delete;

    std::deque<std::shared_ptr<const void>> deque;
  };

  KeepAlives keep_alives;

 public:
  // Because of legacy shenanigans, we use shared_ptr here. For legacy, this is
  // an unowned shared_ptr (a noop deleter) pointing to the modern equivalent
  // which is a member of the legacy variant.
  google::api::expr::runtime::CelTypeRegistry legacy_type_registry;
  google::api::expr::runtime::CelFunctionRegistry legacy_function_registry;
  TypeRegistry& type_registry;
  FunctionRegistry& function_registry;

  well_known_types::Reflection well_known_types;

  google::protobuf::MessageFactory* absl_nonnull MutableMessageFactory() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  // Not thread safe. Adds `keep_alive` to a list owned by this environment
  // and ensures it survives at least as long as this environment. Keep alives
  // are released in reverse order of their registration. This mimics normal
  // destructor rules of members.
  //
  // IMPORTANT: This should only be when building the runtime, and not after.
  void KeepAlive(std::shared_ptr<const void> keep_alive);
};

}  // namespace cel::runtime_internal

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_RUNTIME_ENV_H_
