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

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "common/type.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/values/map_value_builder.h"

namespace cel {

absl::StatusOr<absl::Nonnull<MapValueBuilderPtr>>
TypeReflector::NewMapValueBuilder(ValueFactory& value_factory,
                                  const MapType& type) const {
  return common_internal::NewMapValueBuilder(value_factory);
}

namespace common_internal {

absl::StatusOr<absl::Nonnull<MapValueBuilderPtr>>
LegacyTypeReflector::NewMapValueBuilder(ValueFactory& value_factory,
                                        const MapType& type) const {
  return TypeReflector::NewMapValueBuilder(value_factory, type);
}

}  // namespace common_internal

}  // namespace cel
