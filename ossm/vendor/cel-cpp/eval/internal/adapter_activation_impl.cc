// Copyright 2022 Google LLC
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

#include "eval/internal/adapter_activation_impl.h"

#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/value.h"
#include "eval/internal/interop.h"
#include "eval/public/cel_value.h"
#include "internal/status_macros.h"
#include "runtime/function_overload_reference.h"
#include "runtime/internal/activation_attribute_matcher_access.h"
#include "runtime/internal/attribute_matcher.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::interop_internal {

using ::google::api::expr::runtime::CelFunction;

absl::StatusOr<bool> AdapterActivationImpl::FindVariable(
    absl::string_view name,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  // This implementation should only be used during interop, when we can
  // always assume the memory manager is backed by a protobuf arena.

  absl::optional<google::api::expr::runtime::CelValue> legacy_value =
      legacy_activation_.FindValue(name, arena);
  if (!legacy_value.has_value()) {
    return false;
  }
  CEL_RETURN_IF_ERROR(ModernValue(arena, *legacy_value, *result));
  return true;
}

std::vector<FunctionOverloadReference>
AdapterActivationImpl::FindFunctionOverloads(absl::string_view name) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::vector<const CelFunction*> legacy_candidates =
      legacy_activation_.FindFunctionOverloads(name);
  std::vector<FunctionOverloadReference> result;
  result.reserve(legacy_candidates.size());
  for (const auto* candidate : legacy_candidates) {
    if (candidate == nullptr) {
      continue;
    }
    result.push_back({candidate->descriptor(), *candidate});
  }
  return result;
}

absl::Span<const AttributePattern> AdapterActivationImpl::GetUnknownAttributes()
    const {
  return legacy_activation_.unknown_attribute_patterns();
}

absl::Span<const AttributePattern> AdapterActivationImpl::GetMissingAttributes()
    const {
  return legacy_activation_.missing_attribute_patterns();
}

const runtime_internal::AttributeMatcher* absl_nullable
AdapterActivationImpl::GetAttributeMatcher() const {
  return runtime_internal::ActivationAttributeMatcherAccess::
      GetAttributeMatcher(legacy_activation_);
}

}  // namespace cel::interop_internal
