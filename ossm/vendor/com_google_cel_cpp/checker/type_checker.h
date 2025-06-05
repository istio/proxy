// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_H_

#include <memory>

#include "absl/status/statusor.h"
#include "checker/validation_result.h"
#include "common/ast.h"

namespace cel {

// TypeChecker interface.
//
// Checks references and type agreement for a parsed CEL expression.
//
// TODO: see Compiler for bundled parse and type check from a
// source expression string.
class TypeChecker {
 public:
  virtual ~TypeChecker() = default;

  // Checks the references and type agreement of the given parsed expression
  // based on the configured CEL environment.
  //
  // Most type checking errors are returned as Issues in the validation result.
  // A non-ok status is returned if type checking can't reasonably complete
  // (e.g. if an internal precondition is violated or an extension returns an
  // error).
  virtual absl::StatusOr<ValidationResult> Check(
      std::unique_ptr<Ast> ast) const = 0;

  // TODO: add overload for cref AST.
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_H_
