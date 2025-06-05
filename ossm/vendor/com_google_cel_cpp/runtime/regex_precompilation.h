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

#ifndef THIRD_PARTY_CEL_CPP_REGEX_PRECOMPILATION_FOLDING_H_
#define THIRD_PARTY_CEL_CPP_REGEX_PRECOMPILATION_FOLDING_H_

#include "absl/status/status.h"
#include "common/memory.h"
#include "runtime/runtime_builder.h"

namespace cel::extensions {

// Enable constant folding in the runtime being built.
//
// Constant folding eagerly evaluates sub-expressions with all constant inputs
// at plan time to simplify the resulting program. User extensions functions are
// executed if they are eagerly bound.
//
// The provided memory manager must outlive the runtime object built
// from builder.
absl::Status EnableRegexPrecompilation(RuntimeBuilder& builder);

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_REGEX_PRECOMPILATION_FOLDING_H_
