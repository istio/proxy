// Copyright 2021 Google LLC
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

#include "common/ast_rewrite.h"

#include <stack>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "common/ast_visitor.h"
#include "common/constant.h"
#include "common/expr.h"

namespace cel {

namespace {

struct ArgRecord {
  // Not null.
  Expr* expr;

  // For records that are direct arguments to call, we need to call
  // the CallArg visitor immediately after the argument is evaluated.
  const Expr* calling_expr;
  int call_arg;
};

struct ComprehensionRecord {
  // Not null.
  Expr* expr;

  const ComprehensionExpr* comprehension;
  const Expr* comprehension_expr;
  ComprehensionArg comprehension_arg;
  bool use_comprehension_callbacks;
};

struct ExprRecord {
  // Not null.
  Expr* expr;
};

using StackRecordKind =
    absl::variant<ExprRecord, ArgRecord, ComprehensionRecord>;

struct StackRecord {
 public:
  static constexpr int kTarget = -2;

  explicit StackRecord(Expr* e) {
    ExprRecord record;
    record.expr = e;
    record_variant = record;
  }

  StackRecord(Expr* e, ComprehensionExpr* comprehension,
              Expr* comprehension_expr, ComprehensionArg comprehension_arg,
              bool use_comprehension_callbacks) {
    if (use_comprehension_callbacks) {
      ComprehensionRecord record;
      record.expr = e;
      record.comprehension = comprehension;
      record.comprehension_expr = comprehension_expr;
      record.comprehension_arg = comprehension_arg;
      record.use_comprehension_callbacks = use_comprehension_callbacks;
      record_variant = record;
      return;
    }
    ArgRecord record;
    record.expr = e;
    record.calling_expr = comprehension_expr;
    record.call_arg = comprehension_arg;
    record_variant = record;
  }

  StackRecord(Expr* e, const Expr* call, int argnum) {
    ArgRecord record;
    record.expr = e;
    record.calling_expr = call;
    record.call_arg = argnum;
    record_variant = record;
  }

  Expr* expr() const { return absl::get<ExprRecord>(record_variant).expr; }

  bool IsExprRecord() const {
    return absl::holds_alternative<ExprRecord>(record_variant);
  }

  StackRecordKind record_variant;
  bool visited = false;
};

struct PreVisitor {
  void operator()(const ExprRecord& record) {
    struct {
      AstVisitor* visitor;
      const Expr* expr;
      void operator()(const Constant&) {
        // No pre-visit action.
      }
      void operator()(const IdentExpr&) {
        // No pre-visit action.
      }
      void operator()(const SelectExpr& select) {
        visitor->PreVisitSelect(*expr, select);
      }
      void operator()(const CallExpr& call) {
        visitor->PreVisitCall(*expr, call);
      }
      void operator()(const ListExpr&) {
        // No pre-visit action.
      }
      void operator()(const StructExpr&) {
        // No pre-visit action.
      }
      void operator()(const MapExpr&) {
        // No pre-visit action.
      }
      void operator()(const ComprehensionExpr& comprehension) {
        visitor->PreVisitComprehension(*expr, comprehension);
      }
      void operator()(const UnspecifiedExpr&) {
        // No pre-visit action.
      }
    } handler{visitor, record.expr};
    visitor->PreVisitExpr(*record.expr);
    absl::visit(handler, record.expr->kind());
  }

  // Do nothing for Arg variant.
  void operator()(const ArgRecord&) {}

  void operator()(const ComprehensionRecord& record) {
    visitor->PreVisitComprehensionSubexpression(*record.comprehension_expr,
                                                *record.comprehension,
                                                record.comprehension_arg);
  }

  AstVisitor* visitor;
};

void PreVisit(const StackRecord& record, AstVisitor* visitor) {
  absl::visit(PreVisitor{visitor}, record.record_variant);
}

struct PostVisitor {
  void operator()(const ExprRecord& record) {
    struct {
      AstVisitor* visitor;
      const Expr* expr;
      void operator()(const Constant& constant) {
        visitor->PostVisitConst(*expr, constant);
      }
      void operator()(const IdentExpr& ident) {
        visitor->PostVisitIdent(*expr, ident);
      }
      void operator()(const SelectExpr& select) {
        visitor->PostVisitSelect(*expr, select);
      }
      void operator()(const CallExpr& call) {
        visitor->PostVisitCall(*expr, call);
      }
      void operator()(const ListExpr& create_list) {
        visitor->PostVisitList(*expr, create_list);
      }
      void operator()(const StructExpr& create_struct) {
        visitor->PostVisitStruct(*expr, create_struct);
      }
      void operator()(const MapExpr& map_expr) {
        visitor->PostVisitMap(*expr, map_expr);
      }
      void operator()(const ComprehensionExpr& comprehension) {
        visitor->PostVisitComprehension(*expr, comprehension);
      }
      void operator()(const UnspecifiedExpr&) {
        ABSL_LOG(ERROR) << "Unsupported Expr kind";
      }
    } handler{visitor, record.expr};
    absl::visit(handler, record.expr->kind());

    visitor->PostVisitExpr(*record.expr);
  }

  void operator()(const ArgRecord& record) {
    if (record.call_arg == StackRecord::kTarget) {
      visitor->PostVisitTarget(*record.calling_expr);
    } else {
      visitor->PostVisitArg(*record.calling_expr, record.call_arg);
    }
  }

  void operator()(const ComprehensionRecord& record) {
    visitor->PostVisitComprehensionSubexpression(*record.comprehension_expr,
                                                 *record.comprehension,
                                                 record.comprehension_arg);
  }

  AstVisitor* visitor;
};

void PostVisit(const StackRecord& record, AstVisitor* visitor) {
  absl::visit(PostVisitor{visitor}, record.record_variant);
}

void PushSelectDeps(SelectExpr* select_expr, std::stack<StackRecord>* stack) {
  if (select_expr->has_operand()) {
    stack->push(StackRecord(&select_expr->mutable_operand()));
  }
}

void PushCallDeps(CallExpr* call_expr, Expr* expr,
                  std::stack<StackRecord>* stack) {
  const int arg_size = call_expr->args().size();
  // Our contract is that we visit arguments in order.  To do that, we need
  // to push them onto the stack in reverse order.
  for (int i = arg_size - 1; i >= 0; --i) {
    stack->push(StackRecord(&call_expr->mutable_args()[i], expr, i));
  }
  // Are we receiver-style?
  if (call_expr->has_target()) {
    stack->push(
        StackRecord(&call_expr->mutable_target(), expr, StackRecord::kTarget));
  }
}

void PushListDeps(ListExpr* list_expr, std::stack<StackRecord>* stack) {
  auto& elements = list_expr->mutable_elements();
  for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
    auto& element = *it;
    stack->push(StackRecord(&element.mutable_expr()));
  }
}

void PushStructDeps(StructExpr* struct_expr, std::stack<StackRecord>* stack) {
  auto& entries = struct_expr->mutable_fields();
  for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
    auto& entry = *it;
    // The contract is to visit key, then value.  So put them on the stack
    // in the opposite order.
    if (entry.has_value()) {
      stack->push(StackRecord(&entry.mutable_value()));
    }
  }
}

void PushMapDeps(MapExpr* struct_expr, std::stack<StackRecord>* stack) {
  auto& entries = struct_expr->mutable_entries();
  for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
    auto& entry = *it;
    // The contract is to visit key, then value.  So put them on the stack
    // in the opposite order.
    if (entry.has_value()) {
      stack->push(StackRecord(&entry.mutable_value()));
    }
    // The contract is to visit key, then value.  So put them on the stack
    // in the opposite order.
    if (entry.has_key()) {
      stack->push(StackRecord(&entry.mutable_key()));
    }
  }
}

void PushComprehensionDeps(ComprehensionExpr* c, Expr* expr,
                           std::stack<StackRecord>* stack,
                           bool use_comprehension_callbacks) {
  StackRecord iter_range(&c->mutable_iter_range(), c, expr, ITER_RANGE,
                         use_comprehension_callbacks);
  StackRecord accu_init(&c->mutable_accu_init(), c, expr, ACCU_INIT,
                        use_comprehension_callbacks);
  StackRecord loop_condition(&c->mutable_loop_condition(), c, expr,
                             LOOP_CONDITION, use_comprehension_callbacks);
  StackRecord loop_step(&c->mutable_loop_step(), c, expr, LOOP_STEP,
                        use_comprehension_callbacks);
  StackRecord result(&c->mutable_result(), c, expr, RESULT,
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
    struct {
      std::stack<StackRecord>& stack;
      const RewriteTraversalOptions& options;
      const ExprRecord& record;
      void operator()(const Constant&) {}
      void operator()(const IdentExpr&) {}
      void operator()(const SelectExpr&) {
        PushSelectDeps(&record.expr->mutable_select_expr(), &stack);
      }
      void operator()(const CallExpr&) {
        PushCallDeps(&record.expr->mutable_call_expr(), record.expr, &stack);
      }
      void operator()(const ListExpr&) {
        PushListDeps(&record.expr->mutable_list_expr(), &stack);
      }
      void operator()(const StructExpr&) {
        PushStructDeps(&record.expr->mutable_struct_expr(), &stack);
      }
      void operator()(const MapExpr&) {
        PushMapDeps(&record.expr->mutable_map_expr(), &stack);
      }
      void operator()(const ComprehensionExpr&) {
        PushComprehensionDeps(&record.expr->mutable_comprehension_expr(),
                              record.expr, &stack,
                              options.use_comprehension_callbacks);
      }
      void operator()(const UnspecifiedExpr&) {}
    } handler{stack, options, record};
    absl::visit(handler, record.expr->kind());
  }

  void operator()(const ArgRecord& record) {
    stack.push(StackRecord(record.expr));
  }

  void operator()(const ComprehensionRecord& record) {
    stack.push(StackRecord(record.expr));
  }

  std::stack<StackRecord>& stack;
  const RewriteTraversalOptions& options;
};

void PushDependencies(const StackRecord& record, std::stack<StackRecord>& stack,
                      const RewriteTraversalOptions& options) {
  absl::visit(PushDepsVisitor{stack, options}, record.record_variant);
}

}  // namespace

bool AstRewrite(Expr& expr, AstRewriter& visitor,
                RewriteTraversalOptions options) {
  std::stack<StackRecord> stack;
  std::vector<const Expr*> traversal_path;

  stack.push(StackRecord(&expr));
  bool rewritten = false;

  while (!stack.empty()) {
    StackRecord& record = stack.top();
    if (!record.visited) {
      if (record.IsExprRecord()) {
        traversal_path.push_back(record.expr());
        visitor.TraversalStackUpdate(absl::MakeSpan(traversal_path));

        if (visitor.PreVisitRewrite(*record.expr())) {
          rewritten = true;
        }
      }
      PreVisit(record, &visitor);
      PushDependencies(record, stack, options);
      record.visited = true;
    } else {
      PostVisit(record, &visitor);
      if (record.IsExprRecord()) {
        if (visitor.PostVisitRewrite(*record.expr())) {
          rewritten = true;
        }

        traversal_path.pop_back();
        visitor.TraversalStackUpdate(absl::MakeSpan(traversal_path));
      }
      stack.pop();
    }
  }

  return rewritten;
}

}  // namespace cel
