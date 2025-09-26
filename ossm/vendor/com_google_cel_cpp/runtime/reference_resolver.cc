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

#include "runtime/reference_resolver.h"

#include "absl/base/macros.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/native_type.h"
#include "eval/compiler/qualified_reference_resolver.h"
#include "internal/casts.h"
#include "internal/status_macros.h"
#include "runtime/internal/runtime_friend_access.h"
#include "runtime/internal/runtime_impl.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"

namespace cel {
namespace {

using ::cel::internal::down_cast;
using ::cel::runtime_internal::RuntimeFriendAccess;
using ::cel::runtime_internal::RuntimeImpl;

absl::StatusOr<RuntimeImpl*> RuntimeImplFromBuilder(RuntimeBuilder& builder) {
  Runtime& runtime = RuntimeFriendAccess::GetMutableRuntime(builder);

  if (RuntimeFriendAccess::RuntimeTypeId(runtime) !=
      NativeTypeId::For<RuntimeImpl>()) {
    return absl::UnimplementedError(
        "regex precompilation only supported on the default cel::Runtime "
        "implementation.");
  }

  RuntimeImpl& runtime_impl = down_cast<RuntimeImpl&>(runtime);

  return &runtime_impl;
}

google::api::expr::runtime::ReferenceResolverOption Convert(
    ReferenceResolverEnabled enabled) {
  switch (enabled) {
    case ReferenceResolverEnabled::kCheckedExpressionOnly:
      return google::api::expr::runtime::ReferenceResolverOption::kCheckedOnly;
    case ReferenceResolverEnabled::kAlways:
      return google::api::expr::runtime::ReferenceResolverOption::kAlways;
  }
  ABSL_LOG(FATAL) << "unsupported ReferenceResolverEnabled enumerator: "
                  << static_cast<int>(enabled);
}

}  // namespace

absl::Status EnableReferenceResolver(RuntimeBuilder& builder,
                                     ReferenceResolverEnabled enabled) {
  CEL_ASSIGN_OR_RETURN(RuntimeImpl * runtime_impl,
                       RuntimeImplFromBuilder(builder));
  ABSL_ASSERT(runtime_impl != nullptr);

  runtime_impl->expr_builder().AddAstTransform(
      NewReferenceResolverExtension(Convert(enabled)));
  return absl::OkStatus();
}

}  // namespace cel
