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

#ifndef THIRD_PARTY_CEL_CPP_BASE_AST_INTERNAL_AST_IMPL_H_
#define THIRD_PARTY_CEL_CPP_BASE_AST_INTERNAL_AST_IMPL_H_

#include <cstdint>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "common/ast.h"
#include "common/ast/expr.h"
#include "internal/casts.h"

namespace cel::ast_internal {

// Runtime implementation of a CEL abstract syntax tree.
// CEL users should not use this directly.
// If AST inspection is needed, prefer to use an existing tool or traverse the
// the protobuf representation.
class AstImpl : public Ast {
 public:
  using ReferenceMap = absl::flat_hash_map<int64_t, Reference>;
  using TypeMap = absl::flat_hash_map<int64_t, Type>;

  // Overloads for down casting from the public interface to the internal
  // implementation.
  static AstImpl& CastFromPublicAst(Ast& ast) {
    return cel::internal::down_cast<AstImpl&>(ast);
  }

  static const AstImpl& CastFromPublicAst(const Ast& ast) {
    return cel::internal::down_cast<const AstImpl&>(ast);
  }

  static AstImpl* CastFromPublicAst(Ast* ast) {
    return cel::internal::down_cast<AstImpl*>(ast);
  }

  static const AstImpl* CastFromPublicAst(const Ast* ast) {
    return cel::internal::down_cast<const AstImpl*>(ast);
  }

  AstImpl() : is_checked_(false) {}

  AstImpl(Expr expr, SourceInfo source_info)
      : root_expr_(std::move(expr)),
        source_info_(std::move(source_info)),
        is_checked_(false) {}

  AstImpl(Expr expr, SourceInfo source_info, ReferenceMap reference_map,
          TypeMap type_map, std::string expr_version)
      : root_expr_(std::move(expr)),
        source_info_(std::move(source_info)),
        reference_map_(std::move(reference_map)),
        type_map_(std::move(type_map)),
        expr_version_(std::move(expr_version)),
        is_checked_(true) {}

  // Move-only
  AstImpl(const AstImpl& other) = delete;
  AstImpl& operator=(const AstImpl& other) = delete;
  AstImpl(AstImpl&& other) = default;
  AstImpl& operator=(AstImpl&& other) = default;

  // Implement public Ast APIs.
  bool IsChecked() const override { return is_checked_; }

  // CEL internal functions.
  void set_is_checked(bool is_checked) { is_checked_ = is_checked; }

  const Expr& root_expr() const { return root_expr_; }
  Expr& root_expr() { return root_expr_; }

  const SourceInfo& source_info() const { return source_info_; }
  SourceInfo& source_info() { return source_info_; }

  const Type& GetType(int64_t expr_id) const;
  const Type& GetReturnType() const;
  const Reference* GetReference(int64_t expr_id) const;

  const absl::flat_hash_map<int64_t, Reference>& reference_map() const {
    return reference_map_;
  }

  ReferenceMap& reference_map() { return reference_map_; }

  const TypeMap& type_map() const { return type_map_; }

  TypeMap& type_map() { return type_map_; }

  absl::string_view expr_version() const { return expr_version_; }
  void set_expr_version(absl::string_view expr_version) {
    expr_version_ = expr_version;
  }

 private:
  Expr root_expr_;
  // The source info derived from input that generated the parsed `expr` and
  // any optimizations made during the type-checking pass.
  SourceInfo source_info_;
  // A map from expression ids to resolved references.
  //
  // The following entries are in this table:
  //
  // - An Ident or Select expression is represented here if it resolves to a
  //   declaration. For instance, if `a.b.c` is represented by
  //   `select(select(id(a), b), c)`, and `a.b` resolves to a declaration,
  //   while `c` is a field selection, then the reference is attached to the
  //   nested select expression (but not to the id or or the outer select).
  //   In turn, if `a` resolves to a declaration and `b.c` are field selections,
  //   the reference is attached to the ident expression.
  // - Every Call expression has an entry here, identifying the function being
  //   called.
  // - Every CreateStruct expression for a message has an entry, identifying
  //   the message.
  ReferenceMap reference_map_;
  // A map from expression ids to types.
  //
  // Every expression node which has a type different than DYN has a mapping
  // here. If an expression has type DYN, it is omitted from this map to save
  // space.
  TypeMap type_map_;
  // The expr version indicates the major / minor version number of the `expr`
  // representation.
  //
  // The most common reason for a version change will be to indicate to the CEL
  // runtimes that transformations have been performed on the expr during static
  // analysis. In some cases, this will save the runtime the work of applying
  // the same or similar transformations prior to evaluation.
  std::string expr_version_;

  bool is_checked_;
};

}  // namespace cel::ast_internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_AST_INTERNAL_AST_IMPL_H_
