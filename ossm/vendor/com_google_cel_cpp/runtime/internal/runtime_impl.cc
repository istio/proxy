// Copyright 2023 Google LLC
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
#include "runtime/internal/runtime_impl.h"

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "base/ast.h"
#include "base/type_provider.h"
#include "common/native_type.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/comprehension_slots.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "internal/casts.h"
#include "internal/status_macros.h"
#include "runtime/activation_interface.h"
#include "runtime/runtime.h"

namespace cel::runtime_internal {
namespace {

using ::google::api::expr::runtime::AttributeTrail;
using ::google::api::expr::runtime::ComprehensionSlots;
using ::google::api::expr::runtime::DirectExpressionStep;
using ::google::api::expr::runtime::ExecutionFrameBase;
using ::google::api::expr::runtime::FlatExpression;
using ::google::api::expr::runtime::WrappedDirectStep;

class ProgramImpl final : public TraceableProgram {
 public:
  using EvaluationListener = TraceableProgram::EvaluationListener;
  ProgramImpl(
      const std::shared_ptr<const RuntimeImpl::Environment>& environment,
      FlatExpression impl)
      : environment_(environment), impl_(std::move(impl)) {}

  absl::StatusOr<Value> Evaluate(const ActivationInterface& activation,
                                 ValueManager& value_factory) const override {
    return Trace(activation, EvaluationListener(), value_factory);
  }

  absl::StatusOr<Value> Trace(const ActivationInterface& activation,
                              EvaluationListener callback,
                              ValueManager& value_factory) const override {
    auto state = impl_.MakeEvaluatorState(value_factory);
    return impl_.EvaluateWithCallback(activation, std::move(callback), state);
  }

  const TypeProvider& GetTypeProvider() const override {
    return environment_->type_registry.GetComposedTypeProvider();
  }

 private:
  // Keep the Runtime environment alive while programs reference it.
  std::shared_ptr<const RuntimeImpl::Environment> environment_;
  FlatExpression impl_;
};

class RecursiveProgramImpl final : public TraceableProgram {
 public:
  using EvaluationListener = TraceableProgram::EvaluationListener;
  RecursiveProgramImpl(
      const std::shared_ptr<const RuntimeImpl::Environment>& environment,
      FlatExpression impl, absl::Nonnull<const DirectExpressionStep*> root)
      : environment_(environment), impl_(std::move(impl)), root_(root) {}

  absl::StatusOr<Value> Evaluate(const ActivationInterface& activation,
                                 ValueManager& value_factory) const override {
    return Trace(activation, /*callback=*/nullptr, value_factory);
  }

  absl::StatusOr<Value> Trace(const ActivationInterface& activation,
                              EvaluationListener callback,
                              ValueManager& value_factory) const override {
    ComprehensionSlots slots(impl_.comprehension_slots_size());
    ExecutionFrameBase frame(activation, std::move(callback), impl_.options(),
                             value_factory, slots);

    Value result;
    AttributeTrail attribute;
    CEL_RETURN_IF_ERROR(root_->Evaluate(frame, result, attribute));

    return result;
  }

  const TypeProvider& GetTypeProvider() const override {
    return environment_->type_registry.GetComposedTypeProvider();
  }

 private:
  // Keep the Runtime environment alive while programs reference it.
  std::shared_ptr<const RuntimeImpl::Environment> environment_;
  FlatExpression impl_;
  absl::Nonnull<const DirectExpressionStep*> root_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<Program>> RuntimeImpl::CreateProgram(
    std::unique_ptr<Ast> ast,
    const Runtime::CreateProgramOptions& options) const {
  return CreateTraceableProgram(std::move(ast), options);
}

absl::StatusOr<std::unique_ptr<TraceableProgram>>
RuntimeImpl::CreateTraceableProgram(
    std::unique_ptr<Ast> ast,
    const Runtime::CreateProgramOptions& options) const {
  CEL_ASSIGN_OR_RETURN(auto flat_expr, expr_builder_.CreateExpressionImpl(
                                           std::move(ast), options.issues));

  // Special case if the program is fully recursive.
  //
  // This implementation avoids unnecessary allocs at evaluation time which
  // improves performance notably for small expressions.
  if (expr_builder_.options().max_recursion_depth != 0 &&
      !flat_expr.subexpressions().empty() &&
      // mainline expression is exactly one recursive step.
      flat_expr.subexpressions().front().size() == 1 &&
      flat_expr.subexpressions().front().front()->GetNativeTypeId() ==
          NativeTypeId::For<WrappedDirectStep>()) {
    const DirectExpressionStep* root =
        internal::down_cast<const WrappedDirectStep*>(
            flat_expr.subexpressions().front().front().get())
            ->wrapped();
    return std::make_unique<RecursiveProgramImpl>(environment_,
                                                  std::move(flat_expr), root);
  }

  return std::make_unique<ProgramImpl>(environment_, std::move(flat_expr));
}

bool TestOnly_IsRecursiveImpl(const Program* program) {
  return dynamic_cast<const RecursiveProgramImpl*>(program) != nullptr;
}

}  // namespace cel::runtime_internal
