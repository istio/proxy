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
#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_CONVERT_CONSTANT_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_CONVERT_CONSTANT_H_

#include "absl/status/statusor.h"
#include "common/allocator.h"
#include "common/ast.h"
#include "common/value.h"

namespace cel::runtime_internal {

// Adapt AST constant to a Value.
//
// Underlying data is copied for string types to keep the program independent
// from the input AST.
//
// The evaluator assumes most ast constants are valid so unchecked ValueManager
// methods are used.
//
// A status may still be returned if value creation fails according to
// value_factory's policy.
absl::StatusOr<Value> ConvertConstant(const Constant& constant,
                                      Allocator<> allocator);

}  // namespace cel::runtime_internal

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_CONVERT_CONSTANT_H_
