/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_TRAVERSE_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_TRAVERSE_H_

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "eval/public/ast_visitor.h"

namespace google::api::expr::runtime {

struct TraversalOptions {
  bool use_comprehension_callbacks;

  TraversalOptions() : use_comprehension_callbacks(false) {}
};

// Traverses the AST representation in an expr proto.
//
// expr: root node of the tree.
// source_info: optional additional parse information about the expression
// visitor: the callback object that receives the visitation notifications
//
// Traversal order follows the pattern:
// PreVisitExpr
// ..PreVisit{ExprKind}
// ....PreVisit{ArgumentIndex}
// .......PreVisitExpr (subtree)
// .......PostVisitExpr (subtree)
// ....PostVisit{ArgumentIndex}
// ..PostVisit{ExprKind}
// PostVisitExpr
//
// Example callback order for fn(1, var):
// PreVisitExpr
// ..PreVisitCall(fn)
// ......PreVisitExpr
// ........PostVisitConst(1)
// ......PostVisitExpr
// ....PostVisitArg(fn, 0)
// ......PreVisitExpr
// ........PostVisitIdent(var)
// ......PostVisitExpr
// ....PostVisitArg(fn, 1)
// ..PostVisitCall(fn)
// PostVisitExpr
void AstTraverse(const google::api::expr::v1alpha1::Expr* expr,
                 const google::api::expr::v1alpha1::SourceInfo* source_info,
                 AstVisitor* visitor,
                 TraversalOptions options = TraversalOptions());

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_TRAVERSE_H_
