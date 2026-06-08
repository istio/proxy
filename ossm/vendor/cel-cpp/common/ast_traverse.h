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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_AST_TRAVERSE_NATIVE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_AST_TRAVERSE_NATIVE_H_

#include <memory>

#include "absl/base/attributes.h"
#include "common/ast_visitor.h"
#include "common/expr.h"

namespace cel {

namespace common_internal {
struct AstTraversalState;
}

struct TraversalOptions {
  // Enable use of the comprehension specific callbacks.
  bool use_comprehension_callbacks = false;
};

// Helper class for managing the traversal of the AST.
// Allows caller to step through the traversal.
//
// Usage:
//
// AstTraversal traversal = AstTraversal::Create(expr);
//
// MyVisitor visitor();
// while(!traversal.IsDone()) {
//   traversal.Step(visitor);
// }
//
// This class is thread-hostile and should only be used in synchronous code.
class AstTraversal {
 public:
  static AstTraversal Create(const cel::Expr& ast ABSL_ATTRIBUTE_LIFETIME_BOUND,
                             const TraversalOptions& options = {});

  ~AstTraversal();

  AstTraversal(const AstTraversal&) = delete;
  AstTraversal& operator=(const AstTraversal&) = delete;
  AstTraversal(AstTraversal&&) = default;
  AstTraversal& operator=(AstTraversal&&) = default;

  // Advances the traversal. Returns true if there is more work to do. This is a
  // no-op if the traversal is done and IsDone() is true.
  bool Step(AstVisitor& visitor);

  // Returns true if there is no work left to do.
  bool IsDone();

 private:
  explicit AstTraversal(TraversalOptions options);
  TraversalOptions options_;
  std::unique_ptr<common_internal::AstTraversalState> state_;
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
void AstTraverse(const Expr& expr, AstVisitor& visitor,
                 TraversalOptions options = TraversalOptions());

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_AST_TRAVERSE_NATIVE_H_
