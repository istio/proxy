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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_INTERNAL_CEL_VALUE_EQUAL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_INTERNAL_CEL_VALUE_EQUAL_H_

#include "absl/types/optional.h"
#include "eval/public/cel_value.h"

namespace cel::interop_internal {

// Implementation for general equality between CELValues. Exposed for
// consistent behavior in set membership functions.
//
// Returns nullopt if the comparison is undefined between differently typed
// values.
absl::optional<bool> CelValueEqualImpl(
    const google::api::expr::runtime::CelValue& v1,
    const google::api::expr::runtime::CelValue& v2);

}  // namespace cel::interop_internal

#endif  // THIRD_PARTY_CEL_CPP_EVAL_INTERNAL_CEL_VALUE_EQUAL_H_
