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

// IWYU pragma: private

#include "common/types/thread_compatible_type_introspector.h"

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/type.h"
#include "common/type_introspector.h"

namespace cel::common_internal {

absl::StatusOr<absl::optional<Type>>
ThreadCompatibleTypeIntrospector::FindTypeImpl(TypeFactory&,
                                               absl::string_view) const {
  return absl::nullopt;
}

absl::StatusOr<absl::optional<StructTypeField>>
ThreadCompatibleTypeIntrospector::FindStructTypeFieldByNameImpl(
    TypeFactory&, absl::string_view, absl::string_view) const {
  return absl::nullopt;
}

}  // namespace cel::common_internal
