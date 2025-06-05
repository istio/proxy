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

#include "eval/compiler/instrumentation.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/ast_internal/ast_impl.h"
#include "base/ast_internal/expr.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"

namespace google::api::expr::runtime {

namespace {

class InstrumentStep : public ExpressionStepBase {
 public:
  explicit InstrumentStep(int64_t expr_id, Instrumentation instrumentation)
      : ExpressionStepBase(/*expr_id=*/expr_id, /*comes_from_ast=*/false),
        expr_id_(expr_id),
        instrumentation_(std::move(instrumentation)) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    if (!frame->value_stack().HasEnough(1)) {
      return absl::InternalError("stack underflow in instrument step.");
    }

    return instrumentation_(expr_id_, frame->value_stack().Peek());

    return absl::OkStatus();
  }

 private:
  int64_t expr_id_;
  Instrumentation instrumentation_;
};

class InstrumentOptimizer : public ProgramOptimizer {
 public:
  explicit InstrumentOptimizer(Instrumentation instrumentation)
      : instrumentation_(std::move(instrumentation)) {}

  absl::Status OnPreVisit(PlannerContext& context,
                          const cel::ast_internal::Expr& node) override {
    return absl::OkStatus();
  }

  absl::Status OnPostVisit(PlannerContext& context,
                           const cel::ast_internal::Expr& node) override {
    if (context.GetSubplan(node).empty()) {
      return absl::OkStatus();
    }

    return context.AddSubplanStep(
        node, std::make_unique<InstrumentStep>(node.id(), instrumentation_));
  }

 private:
  Instrumentation instrumentation_;
};

}  // namespace

ProgramOptimizerFactory CreateInstrumentationExtension(
    InstrumentationFactory factory) {
  return [fac = std::move(factory)](PlannerContext&,
                                    const cel::ast_internal::AstImpl& ast)
             -> absl::StatusOr<std::unique_ptr<ProgramOptimizer>> {
    Instrumentation ins = fac(ast);
    if (ins) {
      return std::make_unique<InstrumentOptimizer>(std::move(ins));
    }
    return nullptr;
  };
}

}  // namespace google::api::expr::runtime
