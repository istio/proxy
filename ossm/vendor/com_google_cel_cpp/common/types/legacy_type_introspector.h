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

// IWYU pragma: private

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_LEGACY_TYPE_INTROSPECTOR_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_LEGACY_TYPE_INTROSPECTOR_H_

#include "common/type_introspector.h"

namespace cel::common_internal {

// `LegacyTypeIntrospector` is an implementation which should be used when
// converting between `cel::Value` and `google::api::expr::runtime::CelValue`
// and only then.
class LegacyTypeIntrospector : public virtual TypeIntrospector {
 public:
  LegacyTypeIntrospector() = default;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_LEGACY_TYPE_INTROSPECTOR_H_
