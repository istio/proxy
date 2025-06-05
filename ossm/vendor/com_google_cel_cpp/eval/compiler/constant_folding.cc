// Copyright 2019 Google LLC
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

#include "eval/compiler/constant_folding.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/variant.h"
#include "base/ast_internal/ast_impl.h"
#include "base/ast_internal/expr.h"
#include "base/builtins.h"
#include "base/kind.h"
#include "base/type_provider.h"
#include "common/allocator.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/compiler/resolver.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/evaluator_core.h"
#include "internal/status_macros.h"
#include "runtime/activation.h"
#include "runtime/internal/convert_constant.h"
#include "google/protobuf/message.h"

namespace cel::runtime_internal {

namespace {

using ::cel::ast_internal::AstImpl;
using ::cel::ast_internal::Call;
using ::cel::ast_internal::Comprehension;
using ::cel::ast_internal::Constant;
using ::cel::ast_internal::CreateList;
using ::cel::ast_internal::CreateStruct;
using ::cel::ast_internal::Expr;
using ::cel::ast_internal::Ident;
using ::cel::ast_internal::Select;
using ::cel::builtin::kAnd;
using ::cel::builtin::kOr;
using ::cel::builtin::kTernary;
using ::cel::runtime_internal::ConvertConstant;
using ::google::api::expr::runtime::CreateConstValueDirectStep;
using ::google::api::expr::runtime::CreateConstValueStep;
using ::google::api::expr::runtime::EvaluationListener;
using ::google::api::expr::runtime::ExecutionFrame;
using ::google::api::expr::runtime::ExecutionPath;
using ::google::api::expr::runtime::ExecutionPathView;
using ::google::api::expr::runtime::FlatExpressionEvaluatorState;
using ::google::api::expr::runtime::PlannerContext;
using ::google::api::expr::runtime::ProgramOptimizer;
using ::google::api::expr::runtime::ProgramOptimizerFactory;
using ::google::api::expr::runtime::Resolver;

class ConstantFoldingExtension : public ProgramOptimizer {
 public:
  ConstantFoldingExtension(
      Allocator<> allocator,
      absl::Nullable<google::protobuf::MessageFactory*> message_factory,
      const TypeProvider& type_provider)
      : memory_manager_(allocator),
        state_(kDefaultStackLimit, kComprehensionSlotCount, type_provider,
               MemoryManager(allocator)),
        message_factory_(message_factory) {}

  absl::Status OnPreVisit(google::api::expr::runtime::PlannerContext& context,
                          const Expr& node) override;
  absl::Status OnPostVisit(google::api::expr::runtime::PlannerContext& context,
                           const Expr& node) override;

 private:
  enum class IsConst {
    kConditional,
    kNonConst,
  };
  // Most constant folding evaluations are simple
  // binary operators.
  static constexpr size_t kDefaultStackLimit = 4;

  // Comprehensions are not evaluated -- the current implementation can't detect
  // if the comprehension variables are only used in a const way.
  static constexpr size_t kComprehensionSlotCount = 0;

  MemoryManager memory_manager_;
  Activation empty_;
  FlatExpressionEvaluatorState state_;
  // Not yet used, will be in future.
  ABSL_ATTRIBUTE_UNUSED
  absl::Nullable<google::protobuf::MessageFactory*> message_factory_;

  std::vector<IsConst> is_const_;
};

absl::Status ConstantFoldingExtension::OnPreVisit(PlannerContext& context,
                                                  const Expr& node) {
  struct IsConstVisitor {
    IsConst operator()(const Constant&) { return IsConst::kConditional; }
    IsConst operator()(const Ident&) { return IsConst::kNonConst; }
    IsConst operator()(const Comprehension&) {
      // Not yet supported, need to identify whether range and
      // iter vars are compatible with const folding.
      return IsConst::kNonConst;
    }
    IsConst operator()(const CreateStruct& create_struct) {
      return IsConst::kNonConst;
    }
    IsConst operator()(const cel::MapExpr& map_expr) {
      // Not yet supported but should be possible in the future.
      // Empty maps are rare and not currently supported as they may eventually
      // have similar issues to empty list when used within comprehensions or
      // macros.
      if (map_expr.entries().empty()) {
        return IsConst::kNonConst;
      }
      return IsConst::kConditional;
    }
    IsConst operator()(const CreateList& create_list) {
      if (create_list.elements().empty()) {
        // TODO: Don't fold for empty list to allow comprehension
        // list append optimization.
        return IsConst::kNonConst;
      }
      return IsConst::kConditional;
    }

    IsConst operator()(const Select&) { return IsConst::kConditional; }

    IsConst operator()(const cel::UnspecifiedExpr&) {
      return IsConst::kNonConst;
    }

    IsConst operator()(const Call& call) {
      // Short Circuiting operators not yet supported.
      if (call.function() == kAnd || call.function() == kOr ||
          call.function() == kTernary) {
        return IsConst::kNonConst;
      }

      // For now we skip constant folding for cel.@block. We do not yet setup
      // slots. When we enable constant folding for comprehensions (like
      // cel.bind), we can address cel.@block.
      if (call.function() == "cel.@block") {
        return IsConst::kNonConst;
      }

      int arg_len = call.args().size() + (call.has_target() ? 1 : 0);
      std::vector<cel::Kind> arg_matcher(arg_len, cel::Kind::kAny);
      // Check for any lazy overloads (activation dependant)
      if (!resolver
               .FindLazyOverloads(call.function(), call.has_target(),
                                  arg_matcher)
               .empty()) {
        return IsConst::kNonConst;
      }

      return IsConst::kConditional;
    }

    const Resolver& resolver;
  };

  IsConst is_const =
      absl::visit(IsConstVisitor{context.resolver()}, node.kind());
  is_const_.push_back(is_const);

  return absl::OkStatus();
}

absl::Status ConstantFoldingExtension::OnPostVisit(PlannerContext& context,
                                                   const Expr& node) {
  if (is_const_.empty()) {
    return absl::InternalError("ConstantFoldingExtension called out of order.");
  }

  IsConst is_const = is_const_.back();
  is_const_.pop_back();

  if (is_const == IsConst::kNonConst) {
    // update parent
    if (!is_const_.empty()) {
      is_const_.back() = IsConst::kNonConst;
    }
    return absl::OkStatus();
  }
  ExecutionPathView subplan = context.GetSubplan(node);
  if (subplan.empty()) {
    // This subexpression is already optimized out or suppressed.
    return absl::OkStatus();
  }
  // copy string to managed handle if backed by the original program.
  Value value;
  if (node.has_const_expr()) {
    CEL_ASSIGN_OR_RETURN(
        value, ConvertConstant(node.const_expr(), state_.value_factory()));
  } else {
    ExecutionFrame frame(subplan, empty_, context.options(), state_);
    state_.Reset();
    // Update stack size to accommodate sub expression.
    // This only results in a vector resize if the new maxsize is greater than
    // the current capacity.
    state_.value_stack().SetMaxSize(subplan.size());

    auto result = frame.Evaluate();
    // If this would be a runtime error, then don't adjust the program plan, but
    // rather allow the error to occur at runtime to preserve the evaluation
    // contract with non-constant folding use cases.
    if (!result.ok()) {
      return absl::OkStatus();
    }
    value = *result;
    if (value->Is<UnknownValue>()) {
      return absl::OkStatus();
    }
  }

  // If recursive planning enabled (recursion limit unbounded or at least 1),
  // use a recursive (direct) step for the folded constant.
  //
  // Constant folding is applied leaf to root based on the program plan so far,
  // so the planner will have an opportunity to validate that the recursion
  // limit is being followed when visiting parent nodes in the AST.
  if (context.options().max_recursion_depth != 0) {
    return context.ReplaceSubplan(
        node, CreateConstValueDirectStep(std::move(value), node.id()), 1);
  }

  // Otherwise make a stack machine plan.
  ExecutionPath new_plan;
  CEL_ASSIGN_OR_RETURN(
      new_plan.emplace_back(),
      CreateConstValueStep(std::move(value), node.id(), false));

  return context.ReplaceSubplan(node, std::move(new_plan));
}

}  // namespace

ProgramOptimizerFactory CreateConstantFoldingOptimizer(
    Allocator<> allocator,
    absl::Nullable<google::protobuf::MessageFactory*> message_factory) {
  return [allocator, message_factory](PlannerContext& ctx, const AstImpl&)
             -> absl::StatusOr<std::unique_ptr<ProgramOptimizer>> {
    return std::make_unique<ConstantFoldingExtension>(
        allocator, message_factory, ctx.value_factory().type_provider());
  };
}

}  // namespace cel::runtime_internal
