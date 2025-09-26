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

#ifndef THIRD_PARTY_CEL_CPP_CODELAB_COMPILER_H_
#define THIRD_PARTY_CEL_CPP_CODELAB_COMPILER_H_

#include "cel/expr/checked.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "checker/validation_result.h"
#include "common/ast_proto.h"
#include "compiler/compiler.h"
#include "internal/status_macros.h"

namespace cel_codelab {

// Helper for compiling expression and converting to proto.
//
// Simplifies error handling for brevity in the codelab.
inline absl::StatusOr<cel::expr::CheckedExpr> CompileToCheckedExpr(
    const cel::Compiler& compiler, absl::string_view expr) {
  CEL_ASSIGN_OR_RETURN(cel::ValidationResult result, compiler.Compile(expr));

  if (!result.IsValid() || result.GetAst() == nullptr) {
    return absl::InvalidArgumentError(result.FormatError());
  }

  cel::expr::CheckedExpr pb;
  CEL_RETURN_IF_ERROR(cel::AstToCheckedExpr(*result.GetAst(), &pb));
  return pb;
};

}  // namespace cel_codelab

#endif  // THIRD_PARTY_CEL_CPP_CODELAB_COMPILER_H_
