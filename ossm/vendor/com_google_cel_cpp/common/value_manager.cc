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

#include "common/value_manager.h"

#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type_reflector.h"
#include "common/values/thread_compatible_value_manager.h"
#include "internal/status_macros.h"

namespace cel {

Shared<ValueManager> NewThreadCompatibleValueManager(
    MemoryManagerRef memory_manager, Shared<TypeReflector> type_reflector) {
  return memory_manager
      .MakeShared<common_internal::ThreadCompatibleValueManager>(
          memory_manager, std::move(type_reflector));
}

absl::StatusOr<Json> ValueManager::ConvertToJson(absl::string_view type_url,
                                                 const absl::Cord& value) {
  CEL_ASSIGN_OR_RETURN(auto deserialized_value,
                       DeserializeValue(type_url, value));
  if (!deserialized_value.has_value()) {
    return absl::NotFoundError(
        absl::StrCat("no deserializer for `", type_url, "`"));
  }
  return deserialized_value->ConvertToJson(*this);
}

}  // namespace cel
