// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/public/ast_traverse.h"

#include <stack>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/log/absl_log.h"
#include "absl/types/variant.h"
#include "eval/public/ast_visitor.h"
#include "eval/public/source_position.h"

namespace google::api::expr::runtime {

using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::SourceInfo;
using Ident = google::api::expr::v1alpha1::Expr::Ident;
using Select = google::api::expr::v1alpha1::Expr::Select;
using Call = google::api::expr::v1alpha1::Expr::Call;
using CreateList = google::api::expr::v1alpha1::Expr::CreateList;
using CreateStruct = google::api::expr::v1alpha1::Expr::CreateStruct;
using Comprehension = google::api::expr::v1alpha1::Expr::Comprehension;

namespace {

struct ArgRecord {
  // Not null.
  const Expr* expr;
  // Not null.
  const SourceInfo* source_info;

  // For records that are direct arguments to call, we need to call
  // the CallArg visitor immediately after the argument is evaluated.
  const Expr* calling_expr;
  int call_arg;
};

struct ComprehensionRecord {
  // Not null.
  const Expr* expr;
  // Not null.
  const SourceInfo* source_info;

  const Comprehension* comprehension;
  const Expr* comprehension_expr;
  ComprehensionArg comprehension_arg;
  bool use_comprehension_callbacks;
};

struct ExprRecord {
  // Not null.
  const Expr* expr;
  // Not null.
  const SourceInfo* source_info;
};

using StackRecordKind =
    absl::variant<ExprRecord, ArgRecord, ComprehensionRecord>;

struct StackRecord {
 public:
  ABSL_ATTRIBUTE_UNUSED static constexpr int kNotCallArg = -1;
  static constexpr int kTarget = -2;

  StackRecord(const Expr* e, const SourceInfo* info) {
    ExprRecord record;
    record.expr = e;
    record.source_info = info;
    record_variant = record;
  }

  StackRecord(const Expr* e, const SourceInfo* info,
              const Comprehension* comprehension,
              const Expr* comprehension_expr,
              ComprehensionArg comprehension_arg,
              bool use_comprehension_callbacks) {
    if (use_comprehension_callbacks) {
      ComprehensionRecord record;
      record.expr = e;
      record.source_info = info;
      record.comprehension = comprehension;
      record.comprehension_expr = comprehension_expr;
      record.comprehension_arg = comprehension_arg;
      record.use_comprehension_callbacks = use_comprehension_callbacks;
      record_variant = record;
      return;
    }
    ArgRecord record;
    record.expr = e;
    record.source_info = info;
    record.calling_expr = comprehension_expr;
    record.call_arg = comprehension_arg;
    record_variant = record;
  }

  StackRecord(const Expr* e, const SourceInfo* info, const Expr* call,
              int argnum) {
    ArgRecord record;
    record.expr = e;
    record.source_info = info;
    record.calling_expr = call;
    record.call_arg = argnum;
    record_variant = record;
  }
  StackRecordKind record_variant;
  bool visited = false;
};

struct PreVisitor {
  void operator()(const ExprRecord& record) {
    const Expr* expr = record.expr;
    const SourcePosition position(expr->id(), record.source_info);
    visitor->PreVisitExpr(expr, &position);
    switch (expr->expr_kind_case()) {
      case Expr::kConstExpr:
        visitor->PreVisitConst(&expr->const_expr(), expr, &position);
        break;
      case Expr::kIdentExpr:
        visitor->PreVisitIdent(&expr->ident_expr(), expr, &position);
        break;
      case Expr::kSelectExpr:
        visitor->PreVisitSelect(&expr->select_expr(), expr, &position);
        break;
      case Expr::kCallExpr:
        visitor->PreVisitCall(&expr->call_expr(), expr, &position);
        break;
      case Expr::kListExpr:
        visitor->PreVisitCreateList(&expr->list_expr(), expr, &position);
        break;
      case Expr::kStructExpr:
        visitor->PreVisitCreateStruct(&expr->struct_expr(), expr, &position);
        break;
      case Expr::kComprehensionExpr:
        visitor->PreVisitComprehension(&expr->comprehension_expr(), expr,
                                       &position);
        break;
      default:
        // No pre-visit action.
        break;
    }
  }

  // Do nothing for Arg variant.
  void operator()(const ArgRecord&) {}

  void operator()(const ComprehensionRecord& record) {
    const Expr* expr = record.expr;
    const SourcePosition position(expr->id(), record.source_info);
    visitor->PreVisitComprehensionSubexpression(
        expr, record.comprehension, record.comprehension_arg, &position);
  }

  AstVisitor* visitor;
};

void PreVisit(const StackRecord& record, AstVisitor* visitor) {
  absl::visit(PreVisitor{visitor}, record.record_variant);
}

struct PostVisitor {
  void operator()(const ExprRecord& record) {
    const Expr* expr = record.expr;
    const SourcePosition position(expr->id(), record.source_info);
    switch (expr->expr_kind_case()) {
      case Expr::kConstExpr:
        visitor->PostVisitConst(&expr->const_expr(), expr, &position);
        break;
      case Expr::kIdentExpr:
        visitor->PostVisitIdent(&expr->ident_expr(), expr, &position);
        break;
      case Expr::kSelectExpr:
        visitor->PostVisitSelect(&expr->select_expr(), expr, &position);
        break;
      case Expr::kCallExpr:
        visitor->PostVisitCall(&expr->call_expr(), expr, &position);
        break;
      case Expr::kListExpr:
        visitor->PostVisitCreateList(&expr->list_expr(), expr, &position);
        break;
      case Expr::kStructExpr:
        visitor->PostVisitCreateStruct(&expr->struct_expr(), expr, &position);
        break;
      case Expr::kComprehensionExpr:
        visitor->PostVisitComprehension(&expr->comprehension_expr(), expr,
                                        &position);
        break;
      default:
        ABSL_LOG(ERROR) << "Unsupported Expr kind: " << expr->expr_kind_case();
    }

    visitor->PostVisitExpr(expr, &position);
  }

  void operator()(const ArgRecord& record) {
    const Expr* expr = record.expr;
    const SourcePosition position(expr->id(), record.source_info);
    if (record.call_arg == StackRecord::kTarget) {
      visitor->PostVisitTarget(record.calling_expr, &position);
    } else {
      visitor->PostVisitArg(record.call_arg, record.calling_expr, &position);
    }
  }

  void operator()(const ComprehensionRecord& record) {
    const Expr* expr = record.expr;
    const SourcePosition position(expr->id(), record.source_info);
    visitor->PostVisitComprehensionSubexpression(
        expr, record.comprehension, record.comprehension_arg, &position);
  }

  AstVisitor* visitor;
};

void PostVisit(const StackRecord& record, AstVisitor* visitor) {
  absl::visit(PostVisitor{visitor}, record.record_variant);
}

void PushSelectDeps(const Select* select_expr, const SourceInfo* source_info,
                    std::stack<StackRecord>* stack) {
  if (select_expr->has_operand()) {
    stack->push(StackRecord(&select_expr->operand(), source_info));
  }
}

void PushCallDeps(const Call* call_expr, const Expr* expr,
                  const SourceInfo* source_info,
                  std::stack<StackRecord>* stack) {
  const int arg_size = call_expr->args_size();
  // Our contract is that we visit arguments in order.  To do that, we need
  // to push them onto the stack in reverse order.
  for (int i = arg_size - 1; i >= 0; --i) {
    stack->push(StackRecord(&call_expr->args(i), source_info, expr, i));
  }
  // Are we receiver-style?
  if (call_expr->has_target()) {
    stack->push(StackRecord(&call_expr->target(), source_info, expr,
                            StackRecord::kTarget));
  }
}

void PushListDeps(const CreateList* list_expr, const SourceInfo* source_info,
                  std::stack<StackRecord>* stack) {
  const auto& elements = list_expr->elements();
  for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
    const auto& element = *it;
    stack->push(StackRecord(&element, source_info));
  }
}

void PushStructDeps(const CreateStruct* struct_expr,
                    const SourceInfo* source_info,
                    std::stack<StackRecord>* stack) {
  const auto& entries = struct_expr->entries();
  for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
    const auto& entry = *it;
    // The contract is to visit key, then value.  So put them on the stack
    // in the opposite order.
    if (entry.has_value()) {
      stack->push(StackRecord(&entry.value(), source_info));
    }

    if (entry.has_map_key()) {
      stack->push(StackRecord(&entry.map_key(), source_info));
    }
  }
}

void PushComprehensionDeps(const Comprehension* c, const Expr* expr,
                           const SourceInfo* source_info,
                           std::stack<StackRecord>* stack,
                           bool use_comprehension_callbacks) {
  StackRecord iter_range(&c->iter_range(), source_info, c, expr, ITER_RANGE,
                         use_comprehension_callbacks);
  StackRecord accu_init(&c->accu_init(), source_info, c, expr, ACCU_INIT,
                        use_comprehension_callbacks);
  StackRecord loop_condition(&c->loop_condition(), source_info, c, expr,
                             LOOP_CONDITION, use_comprehension_callbacks);
  StackRecord loop_step(&c->loop_step(), source_info, c, expr, LOOP_STEP,
                        use_comprehension_callbacks);
  StackRecord result(&c->result(), source_info, c, expr, RESULT,
                     use_comprehension_callbacks);
  // Push them in reverse order.
  stack->push(result);
  stack->push(loop_step);
  stack->push(loop_condition);
  stack->push(accu_init);
  stack->push(iter_range);
}

struct PushDepsVisitor {
  void operator()(const ExprRecord& record) {
    const Expr* expr = record.expr;
    switch (expr->expr_kind_case()) {
      case Expr::kSelectExpr:
        PushSelectDeps(&expr->select_expr(), record.source_info, &stack);
        break;
      case Expr::kCallExpr:
        PushCallDeps(&expr->call_expr(), expr, record.source_info, &stack);
        break;
      case Expr::kListExpr:
        PushListDeps(&expr->list_expr(), record.source_info, &stack);
        break;
      case Expr::kStructExpr:
        PushStructDeps(&expr->struct_expr(), record.source_info, &stack);
        break;
      case Expr::kComprehensionExpr:
        PushComprehensionDeps(&expr->comprehension_expr(), expr,
                              record.source_info, &stack,
                              options.use_comprehension_callbacks);
        break;
      default:
        break;
    }
  }

  void operator()(const ArgRecord& record) {
    stack.push(StackRecord(record.expr, record.source_info));
  }

  void operator()(const ComprehensionRecord& record) {
    stack.push(StackRecord(record.expr, record.source_info));
  }

  std::stack<StackRecord>& stack;
  const TraversalOptions& options;
};

void PushDependencies(const StackRecord& record, std::stack<StackRecord>& stack,
                      const TraversalOptions& options) {
  absl::visit(PushDepsVisitor{stack, options}, record.record_variant);
}

}  // namespace

void AstTraverse(const Expr* expr, const SourceInfo* source_info,
                 AstVisitor* visitor, TraversalOptions options) {
  std::stack<StackRecord> stack;
  stack.push(StackRecord(expr, source_info));

  while (!stack.empty()) {
    StackRecord& record = stack.top();
    if (!record.visited) {
      PreVisit(record, visitor);
      PushDependencies(record, stack, options);
      record.visited = true;
    } else {
      PostVisit(record, visitor);
      stack.pop();
    }
  }
}

}  // namespace google::api::expr::runtime
