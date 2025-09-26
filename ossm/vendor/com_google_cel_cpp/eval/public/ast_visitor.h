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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_VISITOR_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_VISITOR_H_

#include "cel/expr/syntax.pb.h"
#include "eval/public/source_position.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// ComprehensionArg specifies arg_num values passed to PostVisitArg
// for subexpressions of Comprehension.
enum ComprehensionArg {
  ITER_RANGE,
  ACCU_INIT,
  LOOP_CONDITION,
  LOOP_STEP,
  RESULT,
};

// Callback handler class, used in conjunction with AstTraverse.
// Methods of this class are invoked when AST nodes with corresponding
// types are processed.
//
// For all types with children, the children will be visited in the natural
// order from first to last.  For structs, keys are visited before values.
class AstVisitor {
 public:
  virtual ~AstVisitor() {}

  // Expr node handler method. Called for all Expr nodes.
  // Is invoked before child Expr nodes being processed.
  // TODO(issues/22): this method is not pure virtual to avoid dependencies
  // breakage. Change it in subsequent CLs.
  virtual void PreVisitExpr(const cel::expr::Expr*,
                            const SourcePosition*) {}

  // Expr node handler method. Called for all Expr nodes.
  // Is invoked after child Expr nodes are processed.
  // TODO(issues/22): this method is not pure virtual to avoid dependencies
  // breakage. Change it in subsequent CLs.
  virtual void PostVisitExpr(const cel::expr::Expr*,
                             const SourcePosition*) {}

  // Const node handler.
  // Invoked before child nodes are processed.
  // TODO(issues/22): this method is not pure virtual to avoid dependencies
  // breakage. Change it in subsequent CLs.
  virtual void PreVisitConst(const cel::expr::Constant*,
                             const cel::expr::Expr*,
                             const SourcePosition*) {}

  // Const node handler.
  // Invoked after child nodes are processed.
  virtual void PostVisitConst(const cel::expr::Constant*,
                              const cel::expr::Expr*,
                              const SourcePosition*) = 0;

  // Ident node handler.
  // Invoked before child nodes are processed.
  // TODO(issues/22): this method is not pure virtual to avoid dependencies
  // breakage. Change it in subsequent CLs.
  virtual void PreVisitIdent(const cel::expr::Expr::Ident*,
                             const cel::expr::Expr*,
                             const SourcePosition*) {}

  // Ident node handler.
  // Invoked after child nodes are processed.
  virtual void PostVisitIdent(const cel::expr::Expr::Ident*,
                              const cel::expr::Expr*,
                              const SourcePosition*) = 0;

  // Select node handler
  // Invoked before child nodes are processed.
  // TODO(issues/22): this method is not pure virtual to avoid dependencies
  // breakage. Change it in subsequent CLs.
  virtual void PreVisitSelect(const cel::expr::Expr::Select*,
                              const cel::expr::Expr*,
                              const SourcePosition*) {}

  // Select node handler
  // Invoked after child nodes are processed.
  virtual void PostVisitSelect(const cel::expr::Expr::Select*,
                               const cel::expr::Expr*,
                               const SourcePosition*) = 0;

  // Call node handler group
  // We provide finer granularity for Call node callbacks to allow special
  // handling for short-circuiting
  // PreVisitCall is invoked before child nodes are processed.
  virtual void PreVisitCall(const cel::expr::Expr::Call*,
                            const cel::expr::Expr*,
                            const SourcePosition*) = 0;

  // Invoked after all child nodes are processed.
  virtual void PostVisitCall(const cel::expr::Expr::Call*,
                             const cel::expr::Expr*,
                             const SourcePosition*) = 0;

  // Invoked after target node is processed.
  // Expr is the call expression.
  virtual void PostVisitTarget(const cel::expr::Expr*,
                               const SourcePosition*) = 0;

  // Invoked before all child nodes are processed.
  virtual void PreVisitComprehension(
      const cel::expr::Expr::Comprehension*,
      const cel::expr::Expr*, const SourcePosition*) = 0;

  // Invoked before comprehension child node is processed.
  virtual void PreVisitComprehensionSubexpression(
      const cel::expr::Expr* subexpr,
      const cel::expr::Expr::Comprehension* compr,
      ComprehensionArg comprehension_arg, const SourcePosition*) {}

  // Invoked after comprehension child node is processed.
  virtual void PostVisitComprehensionSubexpression(
      const cel::expr::Expr* subexpr,
      const cel::expr::Expr::Comprehension* compr,
      ComprehensionArg comprehension_arg, const SourcePosition*) {}

  // Invoked after all child nodes are processed.
  virtual void PostVisitComprehension(
      const cel::expr::Expr::Comprehension*,
      const cel::expr::Expr*, const SourcePosition*) = 0;

  // Invoked after each argument node processed.
  // For Call arg_num is the index of the argument.
  // For Comprehension arg_num is specified by ComprehensionArg.
  // Expr is the call expression.
  virtual void PostVisitArg(int arg_num, const cel::expr::Expr*,
                            const SourcePosition*) = 0;

  // CreateList node handler
  // Invoked before child nodes are processed.
  // TODO(issues/22): this method is not pure virtual to avoid dependencies
  // breakage. Change it in subsequent CLs.
  virtual void PreVisitCreateList(const cel::expr::Expr::CreateList*,
                                  const cel::expr::Expr*,
                                  const SourcePosition*) {}

  // CreateList node handler
  // Invoked after child nodes are processed.
  virtual void PostVisitCreateList(const cel::expr::Expr::CreateList*,
                                   const cel::expr::Expr*,
                                   const SourcePosition*) = 0;

  // CreateStruct node handler
  // Invoked before child nodes are processed.
  // TODO(issues/22): this method is not pure virtual to avoid dependencies
  // breakage. Change it in subsequent CLs.
  virtual void PreVisitCreateStruct(
      const cel::expr::Expr::CreateStruct*,
      const cel::expr::Expr*, const SourcePosition*) {}

  // CreateStruct node handler
  // Invoked after child nodes are processed.
  virtual void PostVisitCreateStruct(
      const cel::expr::Expr::CreateStruct*,
      const cel::expr::Expr*, const SourcePosition*) = 0;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_VISITOR_H_
