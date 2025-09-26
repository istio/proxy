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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_REFERENCE_RESOLVER_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_REFERENCE_RESOLVER_H_

#include "absl/status/status.h"
#include "runtime/runtime_builder.h"

namespace cel {

enum class ReferenceResolverEnabled { kCheckedExpressionOnly, kAlways };

// Enables expression rewrites to normalize the AST representation of
// references to qualified names of enum constants, variables and functions.
//
// For parse-only expressions, this is only able to disambiguate functions based
// on registered overloads in the runtime.
//
// Note: This may require making a deep copy of the input expression in order to
// apply the rewrites.
//
// Applied adjustments:
//  - for dot-qualified variable names represented as select operations,
//    replaces select operations with an identifier.
//  - for dot-qualified functions, replaces receiver call with a global
//    function call.
//  - for compile time constants (such as enum values), inlines the constant
//    value as a literal.
absl::Status EnableReferenceResolver(RuntimeBuilder& builder,
                                     ReferenceResolverEnabled enabled);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_REFERENCE_RESOLVER_H_
