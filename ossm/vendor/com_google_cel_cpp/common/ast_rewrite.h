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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_AST_REWRITE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_AST_REWRITE_H_

#include "absl/base/nullability.h"
#include "absl/types/span.h"
#include "common/ast_visitor.h"
#include "common/constant.h"
#include "common/expr.h"

namespace cel {

// Traversal options for AstRewrite.
struct RewriteTraversalOptions {
  // If enabled, use comprehension specific callbacks instead of the general
  // arguments callbacks.
  bool use_comprehension_callbacks;

  RewriteTraversalOptions() : use_comprehension_callbacks(false) {}
};

// Interface for AST rewriters.
// Extends AstVisitor interface with update methods.
// see AstRewrite for more details on usage.
class AstRewriter : public AstVisitor {
 public:
  ~AstRewriter() override {}

  // Rewrite a sub expression before visiting.
  // Occurs before visiting Expr. If expr is modified, it the new value will be
  // visited.
  virtual bool PreVisitRewrite(Expr& expr) = 0;

  // Rewrite a sub expression after visiting.
  // Occurs after visiting expr and it's children. If expr is modified, the old
  // sub expression is visited.
  virtual bool PostVisitRewrite(Expr& expr) = 0;

  // Notify the visitor of updates to the traversal stack.
  virtual void TraversalStackUpdate(
      absl::Span<const Expr* absl_nonnull> path) = 0;
};

// Trivial implementation for AST rewriters.
// Virtual methods are overridden with no-op callbacks.
class AstRewriterBase : public AstRewriter {
 public:
  ~AstRewriterBase() override {}

  void PreVisitExpr(const Expr&) override {}

  void PostVisitExpr(const Expr&) override {}

  void PostVisitConst(const Expr&, const Constant&) override {}

  void PostVisitIdent(const Expr&, const IdentExpr&) override {}

  void PreVisitSelect(const Expr&, const SelectExpr&) override {}

  void PostVisitSelect(const Expr&, const SelectExpr&) override {}

  void PreVisitCall(const Expr&, const CallExpr&) override {}

  void PostVisitCall(const Expr&, const CallExpr&) override {}

  void PreVisitComprehension(const Expr&, const ComprehensionExpr&) override {}

  void PostVisitComprehension(const Expr&, const ComprehensionExpr&) override {}

  void PostVisitArg(const Expr&, int) override {}

  void PostVisitTarget(const Expr&) override {}

  void PostVisitList(const Expr&, const ListExpr&) override {}

  void PostVisitStruct(const Expr&, const StructExpr&) override {}

  void PostVisitMap(const Expr&, const MapExpr&) override {}

  bool PreVisitRewrite(Expr& expr) override { return false; }

  bool PostVisitRewrite(Expr& expr) override { return false; }

  void TraversalStackUpdate(
      absl::Span<const Expr* absl_nonnull> path) override {}
};

// Traverses the AST representation in an expr proto. Returns true if any
// rewrites occur.
//
// Rewrites may happen before and/or after visiting an expr subtree. If a
// change happens during the pre-visit rewrite, the updated subtree will be
// visited. If a change happens during the post-visit rewrite, the old subtree
// will be visited.
//
// expr: root node of the tree.
// source_info: optional additional parse information about the expression
// visitor: the callback object that receives the visitation notifications
// options: options for traversal. see RewriteTraversalOptions. Defaults are
//     used if not sepecified.
//
// Traversal order follows the pattern:
// PreVisitRewrite
// PreVisitExpr
// ..PreVisit{ExprKind}
// ....PreVisit{ArgumentIndex}
// .......PreVisitExpr (subtree)
// .......PostVisitExpr (subtree)
// ....PostVisit{ArgumentIndex}
// ..PostVisit{ExprKind}
// PostVisitExpr
// PostVisitRewrite
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

bool AstRewrite(Expr& expr, AstRewriter& visitor,
                RewriteTraversalOptions options = RewriteTraversalOptions());

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_AST_REWRITE_H_
