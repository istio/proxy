// Copyright 2023 Google LLC
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

#include "runtime/constant_folding.h"

#include <memory>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/typeinfo.h"
#include "eval/compiler/constant_folding.h"
#include "internal/casts.h"
#include "internal/noop_delete.h"
#include "internal/status_macros.h"
#include "runtime/internal/runtime_friend_access.h"
#include "runtime/internal/runtime_impl.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"

namespace cel::extensions {
namespace {

using ::cel::internal::down_cast;
using ::cel::runtime_internal::RuntimeFriendAccess;
using ::cel::runtime_internal::RuntimeImpl;

absl::StatusOr<RuntimeImpl* absl_nonnull> RuntimeImplFromBuilder(
    RuntimeBuilder& builder ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  Runtime& runtime = RuntimeFriendAccess::GetMutableRuntime(builder);
  if (RuntimeFriendAccess::RuntimeTypeId(runtime) != TypeId<RuntimeImpl>()) {
    return absl::UnimplementedError(
        "constant folding only supported on the default cel::Runtime "
        "implementation.");
  }
  return down_cast<RuntimeImpl*>(&runtime);
}

absl::Status EnableConstantFoldingImpl(
    RuntimeBuilder& builder, absl_nullable std::shared_ptr<google::protobuf::Arena> arena,
    absl_nullable std::shared_ptr<google::protobuf::MessageFactory> message_factory) {
  CEL_ASSIGN_OR_RETURN(RuntimeImpl* absl_nonnull runtime_impl,
                       RuntimeImplFromBuilder(builder));
  if (arena != nullptr) {
    runtime_impl->environment().KeepAlive(arena);
  }
  if (message_factory != nullptr) {
    runtime_impl->environment().KeepAlive(message_factory);
  }
  runtime_impl->expr_builder().AddProgramOptimizer(
      runtime_internal::CreateConstantFoldingOptimizer(
          std::move(arena), std::move(message_factory)));
  return absl::OkStatus();
}

}  // namespace

absl::Status EnableConstantFolding(RuntimeBuilder& builder) {
  return EnableConstantFoldingImpl(builder, nullptr, nullptr);
}

absl::Status EnableConstantFolding(RuntimeBuilder& builder,
                                   google::protobuf::Arena* absl_nonnull arena) {
  ABSL_DCHECK(arena != nullptr);
  return EnableConstantFoldingImpl(
      builder,
      std::shared_ptr<google::protobuf::Arena>(arena,
                                     internal::NoopDeleteFor<google::protobuf::Arena>()),
      nullptr);
}

absl::Status EnableConstantFolding(
    RuntimeBuilder& builder,
    absl_nonnull std::shared_ptr<google::protobuf::Arena> arena) {
  ABSL_DCHECK(arena != nullptr);
  return EnableConstantFoldingImpl(builder, std::move(arena), nullptr);
}

absl::Status EnableConstantFolding(
    RuntimeBuilder& builder,
    google::protobuf::MessageFactory* absl_nonnull message_factory) {
  ABSL_DCHECK(message_factory != nullptr);
  return EnableConstantFoldingImpl(
      builder, nullptr,
      std::shared_ptr<google::protobuf::MessageFactory>(
          message_factory, internal::NoopDeleteFor<google::protobuf::MessageFactory>()));
}

absl::Status EnableConstantFolding(
    RuntimeBuilder& builder,
    absl_nonnull std::shared_ptr<google::protobuf::MessageFactory> message_factory) {
  ABSL_DCHECK(message_factory != nullptr);
  return EnableConstantFoldingImpl(builder, nullptr,
                                   std::move(message_factory));
}

absl::Status EnableConstantFolding(
    RuntimeBuilder& builder, google::protobuf::Arena* absl_nonnull arena,
    google::protobuf::MessageFactory* absl_nonnull message_factory) {
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  return EnableConstantFoldingImpl(
      builder,
      std::shared_ptr<google::protobuf::Arena>(arena,
                                     internal::NoopDeleteFor<google::protobuf::Arena>()),
      std::shared_ptr<google::protobuf::MessageFactory>(
          message_factory, internal::NoopDeleteFor<google::protobuf::MessageFactory>()));
}

absl::Status EnableConstantFolding(
    RuntimeBuilder& builder, google::protobuf::Arena* absl_nonnull arena,
    absl_nonnull std::shared_ptr<google::protobuf::MessageFactory> message_factory) {
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  return EnableConstantFoldingImpl(
      builder,
      std::shared_ptr<google::protobuf::Arena>(arena,
                                     internal::NoopDeleteFor<google::protobuf::Arena>()),
      std::move(message_factory));
}

absl::Status EnableConstantFolding(
    RuntimeBuilder& builder, absl_nonnull std::shared_ptr<google::protobuf::Arena> arena,
    google::protobuf::MessageFactory* absl_nonnull message_factory) {
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  return EnableConstantFoldingImpl(
      builder, std::move(arena),
      std::shared_ptr<google::protobuf::MessageFactory>(
          message_factory, internal::NoopDeleteFor<google::protobuf::MessageFactory>()));
}

absl::Status EnableConstantFolding(
    RuntimeBuilder& builder, absl_nonnull std::shared_ptr<google::protobuf::Arena> arena,
    absl_nonnull std::shared_ptr<google::protobuf::MessageFactory> message_factory) {
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  return EnableConstantFoldingImpl(builder, std::move(arena),
                                   std::move(message_factory));
}

}  // namespace cel::extensions
