// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_AST_H_
#define THIRD_PARTY_CEL_CPP_COMMON_AST_H_

#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "common/ast/metadata.h"  // IWYU pragma: export
#include "common/expr.h"

namespace cel {

// In memory representation of a CEL abstract syntax tree.
//
// If AST inspection or manipulation is needed, prefer to use an existing tool
// or traverse the protobuf representation rather than directly manipulating
// through this class. See `cel::NavigableAst` and `cel::AstTraverse`.
//
// Type and reference maps are only populated if the AST is checked. Any changes
// to the AST are not automatically reflected in the type or reference maps.
//
// To create a new instance from a protobuf representation, use the conversion
// utilities in `common/ast_proto.h`.
class Ast final {
 public:
  using ReferenceMap = absl::flat_hash_map<int64_t, Reference>;
  using TypeMap = absl::flat_hash_map<int64_t, TypeSpec>;

  Ast() : is_checked_(false) {}

  Ast(Expr expr, SourceInfo source_info)
      : root_expr_(std::move(expr)),
        source_info_(std::move(source_info)),
        is_checked_(false) {}

  Ast(Expr expr, SourceInfo source_info, ReferenceMap reference_map,
      TypeMap type_map, std::string expr_version)
      : root_expr_(std::move(expr)),
        source_info_(std::move(source_info)),
        reference_map_(std::move(reference_map)),
        type_map_(std::move(type_map)),
        expr_version_(std::move(expr_version)),
        is_checked_(true) {}

  Ast(const Ast& other) = default;
  Ast& operator=(const Ast& other) = default;
  Ast(Ast&& other) = default;
  Ast& operator=(Ast&& other) = default;

  // Deprecated. Use `is_checked()` instead.
  bool IsChecked() const { return is_checked_; }

  bool is_checked() const { return is_checked_; }
  void set_is_checked(bool is_checked) { is_checked_ = is_checked; }

  // The root expression of the AST.
  //
  // This is the entry point for evaluation and determines the overall result
  // of the expression given a context.
  const Expr& root_expr() const { return root_expr_; }
  Expr& mutable_root_expr() { return root_expr_; }

  // Metadata about the source expression.
  const SourceInfo& source_info() const { return source_info_; }
  SourceInfo& mutable_source_info() { return source_info_; }

  // Returns the type of the expression with the given `expr_id`.
  //
  // Returns `nullptr` if the expression node is not found or has dynamic type.
  const TypeSpec* absl_nullable GetType(int64_t expr_id) const;
  const TypeSpec& GetTypeOrDyn(int64_t expr_id) const;
  const TypeSpec& GetReturnType() const;

  // Returns the resolved reference for the expression with the given `expr_id`.
  //
  // Returns `nullptr` if the expression node is not found or no reference was
  // resolved.
  const Reference* absl_nullable GetReference(int64_t expr_id) const;

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
  //
  // Unpopulated if the AST is not checked.
  const ReferenceMap& reference_map() const { return reference_map_; }
  ReferenceMap& mutable_reference_map() { return reference_map_; }

  // A map from expression ids to types.
  //
  // Every expression node which has a type different than DYN has a mapping
  // here. If an expression has type DYN, it is omitted from this map to save
  // space.
  //
  // Unpopulated if the AST is not checked.
  const TypeMap& type_map() const { return type_map_; }
  TypeMap& mutable_type_map() { return type_map_; }

  // The expr version indicates the major / minor version number of the `expr`
  // representation.
  //
  // The most common reason for a version change will be to indicate to the CEL
  // runtimes that transformations have been performed on the expr during static
  // analysis.
  absl::string_view expr_version() const { return expr_version_; }
  void set_expr_version(absl::string_view expr_version) {
    expr_version_ = expr_version;
  }

 private:
  Expr root_expr_;
  SourceInfo source_info_;
  ReferenceMap reference_map_;
  TypeMap type_map_;
  std::string expr_version_;
  bool is_checked_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_AST_H_
