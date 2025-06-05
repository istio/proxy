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

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_TYPE_CHECKER_IMPL_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_TYPE_CHECKER_IMPL_H_

#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "checker/checker_options.h"
#include "checker/internal/type_check_env.h"
#include "checker/type_checker.h"
#include "checker/validation_result.h"
#include "common/ast.h"
#include "google/protobuf/arena.h"

namespace cel::checker_internal {

// Implementation of the TypeChecker interface.
//
// See cel::TypeCheckerBuilder for constructing instances.
class TypeCheckerImpl : public TypeChecker {
 public:
  TypeCheckerImpl(TypeCheckEnv env, CheckerOptions options = {})
      : env_(std::move(env)), options_(options) {}

  TypeCheckerImpl(const TypeCheckerImpl&) = delete;
  TypeCheckerImpl& operator=(const TypeCheckerImpl&) = delete;
  TypeCheckerImpl(TypeCheckerImpl&&) = delete;
  TypeCheckerImpl& operator=(TypeCheckerImpl&&) = delete;

  absl::StatusOr<ValidationResult> Check(
      std::unique_ptr<Ast> ast) const override;

 private:
  TypeCheckEnv env_;
  google::protobuf::Arena type_arena_;
  CheckerOptions options_;
};

}  // namespace cel::checker_internal

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_TYPE_CHECKER_IMPL_H_
