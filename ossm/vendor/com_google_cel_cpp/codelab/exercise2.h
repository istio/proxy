// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_CODELAB_EXERCISE1_H_
#define THIRD_PARTY_CEL_CPP_CODELAB_EXERCISE1_H_

#include "google/rpc/context/attribute_context.pb.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace cel_codelab {

// Compile a cel expression and evaluate it. Binds a simple boolean to the
// activation as 'bool_var' for use in the expression.
//
// cel_expr should result in a bool, otherwise an InvalidArgument error is
// returned.
absl::StatusOr<bool> CompileAndEvaluateWithBoolVar(absl::string_view cel_expr,
                                                   bool bool_var);

// Compile a cel expression and evaluate it. Binds an instance of the
// AttributeContext message to the activation (binding the subfields directly).
absl::StatusOr<bool> CompileAndEvaluateWithContext(
    absl::string_view cel_expr,
    const google::rpc::context::AttributeContext& context);

}  // namespace cel_codelab

#endif  // THIRD_PARTY_CEL_CPP_CODELAB_EXERCISE1_H_
