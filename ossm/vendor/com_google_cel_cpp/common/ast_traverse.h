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

#include "absl/status/status.h"
#include "common/ast_visitor.h"
#include "common/expr.h"

namespace cel {

namespace common_internal {
struct AstTraverseContext;
}

struct TraversalOptions {
  // Enable use of the comprehension specific callbacks.
  bool use_comprehension_callbacks;
  // Opaque context used by the traverse manager.
  const common_internal::AstTraverseContext* manager_context;

  TraversalOptions()
      : use_comprehension_callbacks(false), manager_context(nullptr) {}
};

// Helper class for managing the traversal of the AST.
// Allows for passing a signal to halt the traversal.
//
// Usage:
//
// AstTraverseManager manager(/*options=*/{});
//
// MyVisitor visitor(&manager);
// CEL_RETURN_IF_ERROR(manager.AstTraverse(expr, visitor));
//
// This class is thread-hostile and should only be used in synchronous code.
class AstTraverseManager {
 public:
  explicit AstTraverseManager(TraversalOptions options);
  AstTraverseManager();

  ~AstTraverseManager();

  AstTraverseManager(const AstTraverseManager&) = delete;
  AstTraverseManager& operator=(const AstTraverseManager&) = delete;
  AstTraverseManager(AstTraverseManager&&) = delete;
  AstTraverseManager& operator=(AstTraverseManager&&) = delete;

  // Managed traversal of the AST. Allows for interrupting the traversal.
  // Re-entrant traversal is not supported and will result in a
  // FailedPrecondition error.
  absl::Status AstTraverse(const Expr& expr, AstVisitor& visitor);

  // Signals a request for the traversal to halt. The traversal routine will
  // check for this signal at the start of each Expr node visitation.
  // This has no effect if no traversal is in progress.
  void RequestHalt();

 private:
  TraversalOptions options_;
  std::unique_ptr<common_internal::AstTraverseContext> context_;
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
