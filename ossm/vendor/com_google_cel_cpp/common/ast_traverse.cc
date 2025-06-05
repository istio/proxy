// Copyright 2018 Google LLC
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

#include "common/ast_traverse.h"

#include <memory>
#include <stack>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/types/variant.h"
#include "common/ast_visitor.h"
#include "common/constant.h"
#include "common/expr.h"

namespace cel {

namespace common_internal {
struct AstTraverseContext {
  bool should_halt = false;
};
}  // namespace common_internal

namespace {

struct ArgRecord {
  // Not null.
  const Expr* expr;

  // For records that are direct arguments to call, we need to call
  // the CallArg visitor immediately after the argument is evaluated.
  const Expr* calling_expr;
  int call_arg;
};

struct ComprehensionRecord {
  // Not null.
  const Expr* expr;

  const ComprehensionExpr* comprehension;
  const Expr* comprehension_expr;
  ComprehensionArg comprehension_arg;
  bool use_comprehension_callbacks;
};

struct ExprRecord {
  // Not null.
  const Expr* expr;
};

using StackRecordKind =
    absl::variant<ExprRecord, ArgRecord, ComprehensionRecord>;

struct StackRecord {
 public:
  static constexpr int kTarget = -2;

  explicit StackRecord(const Expr* e) {
    ExprRecord record;
    record.expr = e;
    record_variant = record;
  }

  StackRecord(const Expr* e, const ComprehensionExpr* comprehension,
              const Expr* comprehension_expr,
              ComprehensionArg comprehension_arg,
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

  StackRecord(const Expr* e, const Expr* call, int argnum) {
    ArgRecord record;
    record.expr = e;
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
    visitor->PreVisitExpr(*expr);
    if (expr->has_select_expr()) {
      visitor->PreVisitSelect(*expr, expr->select_expr());
    } else if (expr->has_call_expr()) {
      visitor->PreVisitCall(*expr, expr->call_expr());
    } else if (expr->has_comprehension_expr()) {
      visitor->PreVisitComprehension(*expr, expr->comprehension_expr());
    } else {
      // No pre-visit action.
    }
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
    const Expr* expr = record.expr;
    struct {
      AstVisitor* visitor;
      const Expr* expr;
      void operator()(const Constant& constant) {
        visitor->PostVisitConst(*expr, expr->const_expr());
      }
      void operator()(const IdentExpr& ident) {
        visitor->PostVisitIdent(*expr, expr->ident_expr());
      }
      void operator()(const SelectExpr& select) {
        visitor->PostVisitSelect(*expr, expr->select_expr());
      }
      void operator()(const CallExpr& call) {
        visitor->PostVisitCall(*expr, expr->call_expr());
      }
      void operator()(const ListExpr& create_list) {
        visitor->PostVisitList(*expr, expr->list_expr());
      }
      void operator()(const StructExpr& create_struct) {
        visitor->PostVisitStruct(*expr, expr->struct_expr());
      }
      void operator()(const MapExpr& map_expr) {
        visitor->PostVisitMap(*expr, expr->map_expr());
      }
      void operator()(const ComprehensionExpr& comprehension) {
        visitor->PostVisitComprehension(*expr, expr->comprehension_expr());
      }
      void operator()(const UnspecifiedExpr&) {
        ABSL_LOG(ERROR) << "Unsupported Expr kind";
      }
    } handler{visitor, record.expr};
    absl::visit(handler, record.expr->kind());

    visitor->PostVisitExpr(*expr);
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

void PushSelectDeps(const SelectExpr* select_expr,
                    std::stack<StackRecord>* stack) {
  if (select_expr->has_operand()) {
    stack->push(StackRecord(&select_expr->operand()));
  }
}

void PushCallDeps(const CallExpr* call_expr, const Expr* expr,
                  std::stack<StackRecord>* stack) {
  const int arg_size = call_expr->args().size();
  // Our contract is that we visit arguments in order.  To do that, we need
  // to push them onto the stack in reverse order.
  for (int i = arg_size - 1; i >= 0; --i) {
    stack->push(StackRecord(&call_expr->args()[i], expr, i));
  }
  // Are we receiver-style?
  if (call_expr->has_target()) {
    stack->push(StackRecord(&call_expr->target(), expr, StackRecord::kTarget));
  }
}

void PushListDeps(const ListExpr* list_expr, std::stack<StackRecord>* stack) {
  const auto& elements = list_expr->elements();
  for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
    const auto& element = *it;
    stack->push(StackRecord(&element.expr()));
  }
}

void PushStructDeps(const StructExpr* struct_expr,
                    std::stack<StackRecord>* stack) {
  const auto& entries = struct_expr->fields();
  for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
    const auto& entry = *it;
    // The contract is to visit key, then value.  So put them on the stack
    // in the opposite order.
    if (entry.has_value()) {
      stack->push(StackRecord(&entry.value()));
    }
  }
}

void PushMapDeps(const MapExpr* map_expr, std::stack<StackRecord>* stack) {
  const auto& entries = map_expr->entries();
  for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
    const auto& entry = *it;
    // The contract is to visit key, then value.  So put them on the stack
    // in the opposite order.
    if (entry.has_value()) {
      stack->push(StackRecord(&entry.value()));
    }
    // The contract is to visit key, then value.  So put them on the stack
    // in the opposite order.
    if (entry.has_key()) {
      stack->push(StackRecord(&entry.key()));
    }
  }
}

void PushComprehensionDeps(const ComprehensionExpr* c, const Expr* expr,
                           std::stack<StackRecord>* stack,
                           bool use_comprehension_callbacks) {
  StackRecord iter_range(&c->iter_range(), c, expr, ITER_RANGE,
                         use_comprehension_callbacks);
  StackRecord accu_init(&c->accu_init(), c, expr, ACCU_INIT,
                        use_comprehension_callbacks);
  StackRecord loop_condition(&c->loop_condition(), c, expr, LOOP_CONDITION,
                             use_comprehension_callbacks);
  StackRecord loop_step(&c->loop_step(), c, expr, LOOP_STEP,
                        use_comprehension_callbacks);
  StackRecord result(&c->result(), c, expr, RESULT,
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
      const TraversalOptions& options;
      const ExprRecord& record;
      void operator()(const Constant& constant) {}
      void operator()(const IdentExpr& ident) {}
      void operator()(const SelectExpr& select) {
        PushSelectDeps(&record.expr->select_expr(), &stack);
      }
      void operator()(const CallExpr& call) {
        PushCallDeps(&record.expr->call_expr(), record.expr, &stack);
      }
      void operator()(const ListExpr& create_list) {
        PushListDeps(&record.expr->list_expr(), &stack);
      }
      void operator()(const StructExpr& create_struct) {
        PushStructDeps(&record.expr->struct_expr(), &stack);
      }
      void operator()(const MapExpr& map_expr) {
        PushMapDeps(&record.expr->map_expr(), &stack);
      }
      void operator()(const ComprehensionExpr& comprehension) {
        PushComprehensionDeps(&record.expr->comprehension_expr(), record.expr,
                              &stack, options.use_comprehension_callbacks);
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
  const TraversalOptions& options;
};

void PushDependencies(const StackRecord& record, std::stack<StackRecord>& stack,
                      const TraversalOptions& options) {
  absl::visit(PushDepsVisitor{stack, options}, record.record_variant);
}

}  // namespace

AstTraverseManager::AstTraverseManager(TraversalOptions options)
    : options_(options) {}

AstTraverseManager::AstTraverseManager() = default;
AstTraverseManager::~AstTraverseManager() = default;

absl::Status AstTraverseManager::AstTraverse(const Expr& expr,
                                             AstVisitor& visitor) {
  if (context_ != nullptr) {
    return absl::FailedPreconditionError(
        "AstTraverseManager is already in use");
  }
  context_ = std::make_unique<common_internal::AstTraverseContext>();
  TraversalOptions options = options_;
  options.manager_context = context_.get();
  ::cel::AstTraverse(expr, visitor, options);
  context_ = nullptr;
  return absl::OkStatus();
}

void AstTraverseManager::RequestHalt() {
  if (context_ != nullptr) {
    context_->should_halt = true;
  }
}

void AstTraverse(const Expr& expr, AstVisitor& visitor,
                 TraversalOptions options) {
  std::stack<StackRecord> stack;
  stack.push(StackRecord(&expr));

  while (!stack.empty()) {
    if (options.manager_context != nullptr &&
        options.manager_context->should_halt) {
      return;
    }
    StackRecord& record = stack.top();
    if (!record.visited) {
      PreVisit(record, &visitor);
      PushDependencies(record, stack, options);
      record.visited = true;
    } else {
      PostVisit(record, &visitor);
      stack.pop();
    }
  }
}

}  // namespace cel
