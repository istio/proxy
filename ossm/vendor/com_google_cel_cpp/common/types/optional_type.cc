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

#include <cstddef>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "common/type.h"

namespace cel {

namespace common_internal {

namespace {

struct OptionalTypeData final {
  const absl::string_view name;
  const size_t parameters_size;
  const Type parameter;
};

// Here by dragons. In order to make `OptionalType` default constructible
// without some sort of dynamic static initializer, we perform some
// type-punning. `OptionalTypeData` and `OpaqueTypeData` must have the same
// layout, with the only exception being that `OptionalTypeData` as a single
// `Type` where `OpaqueTypeData` as a flexible array.
union DynOptionalTypeData final {
  OptionalTypeData optional;
  OpaqueTypeData opaque;
};

static_assert(offsetof(OptionalTypeData, name) ==
              offsetof(OpaqueTypeData, name));
static_assert(offsetof(OptionalTypeData, parameters_size) ==
              offsetof(OpaqueTypeData, parameters_size));
static_assert(offsetof(OptionalTypeData, parameter) ==
              offsetof(OpaqueTypeData, parameters));

ABSL_CONST_INIT const DynOptionalTypeData kDynOptionalTypeData = {
    .optional =
        {
            .name = OptionalType::kName,
            .parameters_size = 1,
            .parameter = DynType(),
        },
};

}  // namespace

}  // namespace common_internal

OptionalType::OptionalType()
    : opaque_(&common_internal::kDynOptionalTypeData.opaque) {}

Type OptionalType::GetParameter() const { return GetParameters().front(); }

}  // namespace cel
