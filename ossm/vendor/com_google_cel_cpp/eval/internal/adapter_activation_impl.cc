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

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "eval/internal/interop.h"
#include "eval/public/cel_value.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "runtime/function_overload_reference.h"
#include "google/protobuf/arena.h"

namespace cel::interop_internal {

using ::google::api::expr::runtime::CelFunction;

absl::StatusOr<bool> AdapterActivationImpl::FindVariable(
    ValueManager& value_factory, absl::string_view name, Value& result) const {
  // This implementation should only be used during interop, when we can
  // always assume the memory manager is backed by a protobuf arena.
  google::protobuf::Arena* arena =
      extensions::ProtoMemoryManagerArena(value_factory.GetMemoryManager());

  absl::optional<google::api::expr::runtime::CelValue> legacy_value =
      legacy_activation_.FindValue(name, arena);
  if (!legacy_value.has_value()) {
    return false;
  }
  CEL_RETURN_IF_ERROR(ModernValue(arena, *legacy_value, result));
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

}  // namespace cel::interop_internal
