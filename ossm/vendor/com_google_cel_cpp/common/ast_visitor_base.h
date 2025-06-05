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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_AST_VISITOR_BASE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_AST_VISITOR_BASE_H_

#include "common/ast_visitor.h"
#include "common/constant.h"
#include "common/expr.h"

namespace cel {

// Trivial base implementation of AstVisitor.
class AstVisitorBase : public AstVisitor {
 public:
  AstVisitorBase() = default;

  // Non-copyable
  AstVisitorBase(const AstVisitorBase&) = delete;
  AstVisitorBase& operator=(AstVisitorBase const&) = delete;

  ~AstVisitorBase() override {}

  // Const node handler.
  // Invoked after child nodes are processed.
  void PostVisitConst(const Expr&, const Constant&) override {}

  // Ident node handler.
  // Invoked after child nodes are processed.
  void PostVisitIdent(const Expr&, const IdentExpr&) override {}

  void PreVisitSelect(const Expr&, const SelectExpr&) override {}

  // Select node handler
  // Invoked after child nodes are processed.
  void PostVisitSelect(const Expr&, const SelectExpr&) override {}

  // Call node handler group
  // We provide finer granularity for Call node callbacks to allow special
  // handling for short-circuiting
  // PreVisitCall is invoked before child nodes are processed.
  void PreVisitCall(const Expr&, const CallExpr&) override {}

  // Invoked after all child nodes are processed.
  void PostVisitCall(const Expr&, const CallExpr&) override {}

  // Invoked before all child nodes are processed.
  void PreVisitComprehension(const Expr&, const ComprehensionExpr&) override {}

  // Invoked after all child nodes are processed.
  void PostVisitComprehension(const Expr&, const ComprehensionExpr&) override {}

  // Invoked after each argument node processed.
  // For Call arg_num is the index of the argument.
  // For Comprehension arg_num is specified by ComprehensionArg.
  // Expr is the call expression.
  void PostVisitArg(const Expr&, int) override {}

  // Invoked after target node processed.
  void PostVisitTarget(const Expr&) override {}

  // List node handler
  // Invoked after child nodes are processed.
  void PostVisitList(const Expr&, const ListExpr&) override {}

  // Struct node handler
  // Invoked after child nodes are processed.
  void PostVisitStruct(const Expr&, const StructExpr&) override {}

  // Map node handler
  // Invoked after child nodes are processed.
  void PostVisitMap(const Expr&, const MapExpr&) override {}
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_AST_VISITOR_BASE_H_
