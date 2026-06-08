// Copyright 2025 Google LLC
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

#include "tools/cel_field_extractor.h"

#include <algorithm>
#include <string>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_join.h"
#include "tools/navigable_ast.h"

namespace cel {

namespace {

bool IsComprehensionDefinedField(const cel::NavigableProtoAstNode& node) {
  const cel::NavigableProtoAstNode* current_node = &node;

  while (current_node->parent() != nullptr) {
    current_node = current_node->parent();

    if (current_node->node_kind() != cel::NodeKind::kComprehension) {
      continue;
    }

    std::string ident_name = node.expr()->ident_expr().name();
    bool iter_var_match =
        ident_name == current_node->expr()->comprehension_expr().iter_var();
    bool iter_var2_match =
        ident_name == current_node->expr()->comprehension_expr().iter_var2();
    bool accu_var_match =
        ident_name == current_node->expr()->comprehension_expr().accu_var();

    if (iter_var_match || iter_var2_match || accu_var_match) {
      return true;
    }
  }

  return false;
}

}  // namespace

absl::flat_hash_set<std::string> ExtractFieldPaths(
    const cel::expr::Expr& expr) {
  NavigableProtoAst ast = NavigableProtoAst::Build(expr);

  absl::flat_hash_set<std::string> field_paths;
  std::vector<std::string> fields_in_scope;

  // Preorder traversal works because the select nodes (in a well-formed
  // expression) always have only one operand, so its operand is visited
  // next in the loop iteration (which results in the path being extended,
  // completed, or discarded if uninteresting).
  for (const cel::NavigableProtoAstNode& node :
       ast.Root().DescendantsPreorder()) {
    if (node.node_kind() == cel::NodeKind::kSelect) {
      fields_in_scope.push_back(node.expr()->select_expr().field());
      continue;
    }
    if (node.node_kind() == cel::NodeKind::kIdent &&
        !IsComprehensionDefinedField(node)) {
      fields_in_scope.push_back(node.expr()->ident_expr().name());
      std::reverse(fields_in_scope.begin(), fields_in_scope.end());
      field_paths.insert(absl::StrJoin(fields_in_scope, "."));
    }
    fields_in_scope.clear();
  }

  return field_paths;
}

}  // namespace cel
