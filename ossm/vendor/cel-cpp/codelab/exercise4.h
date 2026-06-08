// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_CODELAB_EXERCISE4_H_
#define THIRD_PARTY_CEL_CPP_CODELAB_EXERCISE4_H_

#include "google/rpc/context/attribute_context.pb.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace cel_codelab {

// Compile and evaluate an expression with google.rpc.context.AttributeContext
// as context.
// The environment includes the custom map member function
// .contains(string, string).
absl::StatusOr<bool> EvaluateWithExtensionFunction(
    absl::string_view cel_expr,
    const google::rpc::context::AttributeContext& context);

}  // namespace cel_codelab

#endif  // THIRD_PARTY_CEL_CPP_CODELAB_EXERCISE4_H_
