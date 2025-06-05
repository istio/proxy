// Copyright 2020 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_QUALIFIED_REFERENCE_RESOLVER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_QUALIFIED_REFERENCE_RESOLVER_H_

#include <memory>

#include "absl/status/statusor.h"
#include "base/ast.h"
#include "base/ast_internal/ast_impl.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/compiler/resolver.h"
#include "runtime/internal/issue_collector.h"

namespace google::api::expr::runtime {

// Resolves possibly qualified names in the provided expression, updating
// subexpressions with to use the fully qualified name, or a constant
// expressions in the case of enums.
//
// Returns true if updates were applied.
//
// Will warn or return a non-ok status if references can't be resolved (no
// function overload could match a call) or are inconsistent (reference map
// points to an expr node that isn't a reference).
absl::StatusOr<bool> ResolveReferences(
    const Resolver& resolver, cel::runtime_internal::IssueCollector& issues,
    cel::ast_internal::AstImpl& ast);

enum class ReferenceResolverOption {
  // Always attempt to resolve references based on runtime types and functions.
  kAlways,
  // Only attempt to resolve for checked expressions with reference metadata.
  kCheckedOnly,
};

std::unique_ptr<AstTransform> NewReferenceResolverExtension(
    ReferenceResolverOption option);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_QUALIFIED_REFERENCE_RESOLVER_H_
