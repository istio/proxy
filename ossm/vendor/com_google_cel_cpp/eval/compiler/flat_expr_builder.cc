/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "eval/compiler/flat_expr_builder.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <memory>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/ast.h"
#include "base/ast_internal/ast_impl.h"
#include "base/ast_internal/expr.h"
#include "base/builtins.h"
#include "common/ast.h"
#include "common/ast_traverse.h"
#include "common/ast_visitor.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "common/values/legacy_value_manager.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/compiler/resolver.h"
#include "eval/eval/comprehension_step.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/container_access_step.h"
#include "eval/eval/create_list_step.h"
#include "eval/eval/create_map_step.h"
#include "eval/eval/create_struct_step.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/function_step.h"
#include "eval/eval/ident_step.h"
#include "eval/eval/jump_step.h"
#include "eval/eval/lazy_init_step.h"
#include "eval/eval/logic_step.h"
#include "eval/eval/optional_or_step.h"
#include "eval/eval/select_step.h"
#include "eval/eval/shadowable_value_step.h"
#include "eval/eval/ternary_step.h"
#include "eval/eval/trace_step.h"
#include "internal/status_macros.h"
#include "runtime/internal/convert_constant.h"
#include "runtime/internal/issue_collector.h"
#include "runtime/runtime_issue.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::Ast;
using ::cel::AstTraverse;
using ::cel::RuntimeIssue;
using ::cel::StringValue;
using ::cel::Value;
using ::cel::ValueManager;
using ::cel::ast_internal::AstImpl;
using ::cel::runtime_internal::ConvertConstant;
using ::cel::runtime_internal::IssueCollector;

constexpr absl::string_view kOptionalOrFn = "or";
constexpr absl::string_view kOptionalOrValueFn = "orValue";

// Forward declare to resolve circular dependency for short_circuiting visitors.
class FlatExprVisitor;

// Helper for bookkeeping variables mapped to indexes.
class IndexManager {
 public:
  IndexManager() : next_free_slot_(0), max_slot_count_(0) {}

  size_t ReserveSlots(size_t n) {
    size_t result = next_free_slot_;
    next_free_slot_ += n;
    if (next_free_slot_ > max_slot_count_) {
      max_slot_count_ = next_free_slot_;
    }
    return result;
  }

  size_t ReleaseSlots(size_t n) {
    next_free_slot_ -= n;
    return next_free_slot_;
  }

  size_t max_slot_count() const { return max_slot_count_; }

 private:
  size_t next_free_slot_;
  size_t max_slot_count_;
};

// Helper for computing jump offsets.
//
// Jumps should be self-contained to a single expression node -- jumping
// outside that range is a bug.
struct ProgramStepIndex {
  int index;
  ProgramBuilder::Subexpression* subexpression;
};

// A convenience wrapper for offset-calculating logic.
class Jump {
 public:
  // Default constructor for empty jump.
  //
  // Users must check that jump is non-empty before calling member functions.
  explicit Jump() : self_index_{-1, nullptr}, jump_step_(nullptr) {}
  Jump(ProgramStepIndex self_index, JumpStepBase* jump_step)
      : self_index_(self_index), jump_step_(jump_step) {}

  static absl::StatusOr<int> CalculateOffset(ProgramStepIndex base,
                                             ProgramStepIndex target) {
    if (target.subexpression != base.subexpression) {
      return absl::InternalError(
          "Jump target must be contained in the parent"
          "subexpression");
    }

    int offset = base.subexpression->CalculateOffset(base.index, target.index);
    return offset;
  }

  absl::Status set_target(ProgramStepIndex target) {
    CEL_ASSIGN_OR_RETURN(int offset, CalculateOffset(self_index_, target));

    jump_step_->set_jump_offset(offset);
    return absl::OkStatus();
  }

  bool exists() { return jump_step_ != nullptr; }

 private:
  ProgramStepIndex self_index_;
  JumpStepBase* jump_step_;
};

class CondVisitor {
 public:
  virtual ~CondVisitor() = default;
  virtual void PreVisit(const cel::ast_internal::Expr* expr) = 0;
  virtual void PostVisitArg(int arg_num,
                            const cel::ast_internal::Expr* expr) = 0;
  virtual void PostVisit(const cel::ast_internal::Expr* expr) = 0;
  virtual void PostVisitTarget(const cel::ast_internal::Expr* expr) {}
};

enum class BinaryCond {
  kAnd = 0,
  kOr,
  kOptionalOr,
  kOptionalOrValue,
};

// Visitor managing the "&&" and "||" operatiions.
// Implements short-circuiting if enabled.
//
// With short-circuiting enabled, generates a program like:
//   +-------------+------------------------+-----------------------+
//   | PC          | Step                   | Stack                 |
//   +-------------+------------------------+-----------------------+
//   | i + 0       | <Arg1>                 | arg1                  |
//   | i + 1       | ConditionalJump i + 4  | arg1                  |
//   | i + 2       | <Arg2>                 | arg1, arg2            |
//   | i + 3       | BooleanOperator        | Op(arg1, arg2)        |
//   | i + 4       | <rest of program>      | arg1 | Op(arg1, arg2) |
//   +-------------+------------------------+------------------------+
class BinaryCondVisitor : public CondVisitor {
 public:
  explicit BinaryCondVisitor(FlatExprVisitor* visitor, BinaryCond cond,
                             bool short_circuiting)
      : visitor_(visitor), cond_(cond), short_circuiting_(short_circuiting) {}

  void PreVisit(const cel::ast_internal::Expr* expr) override;
  void PostVisitArg(int arg_num, const cel::ast_internal::Expr* expr) override;
  void PostVisit(const cel::ast_internal::Expr* expr) override;
  void PostVisitTarget(const cel::ast_internal::Expr* expr) override;

 private:
  FlatExprVisitor* visitor_;
  const BinaryCond cond_;
  Jump jump_step_;
  bool short_circuiting_;
};

class TernaryCondVisitor : public CondVisitor {
 public:
  explicit TernaryCondVisitor(FlatExprVisitor* visitor) : visitor_(visitor) {}

  void PreVisit(const cel::ast_internal::Expr* expr) override;
  void PostVisitArg(int arg_num, const cel::ast_internal::Expr* expr) override;
  void PostVisit(const cel::ast_internal::Expr* expr) override;

 private:
  FlatExprVisitor* visitor_;
  Jump jump_to_second_;
  Jump error_jump_;
  Jump jump_after_first_;
};

class ExhaustiveTernaryCondVisitor : public CondVisitor {
 public:
  explicit ExhaustiveTernaryCondVisitor(FlatExprVisitor* visitor)
      : visitor_(visitor) {}

  void PreVisit(const cel::ast_internal::Expr* expr) override;
  void PostVisitArg(int arg_num, const cel::ast_internal::Expr* expr) override {
  }
  void PostVisit(const cel::ast_internal::Expr* expr) override;

 private:
  FlatExprVisitor* visitor_;
};

// Returns whether this comprehension appears to be a standard map/filter
// macro implementation. It is not exhaustive, so it is unsafe to use with
// custom comprehensions outside of the standard macros or hand crafted ASTs.
bool IsOptimizableListAppend(
    const cel::ast_internal::Comprehension* comprehension,
    bool enable_comprehension_list_append) {
  if (!enable_comprehension_list_append) {
    return false;
  }
  absl::string_view accu_var = comprehension->accu_var();
  if (accu_var.empty() ||
      comprehension->result().ident_expr().name() != accu_var) {
    return false;
  }
  if (!comprehension->accu_init().has_list_expr()) {
    return false;
  }

  if (!comprehension->loop_step().has_call_expr()) {
    return false;
  }

  // Macro loop_step for a filter() will contain a ternary:
  //   filter ? accu_var + [elem] : accu_var
  // Macro loop_step for a map() will contain a list concat operation:
  //   accu_var + [elem]
  const auto* call_expr = &comprehension->loop_step().call_expr();

  if (call_expr->function() == cel::builtin::kTernary &&
      call_expr->args().size() == 3) {
    if (!call_expr->args()[1].has_call_expr()) {
      return false;
    }
    call_expr = &(call_expr->args()[1].call_expr());
  }

  return call_expr->function() == cel::builtin::kAdd &&
         call_expr->args().size() == 2 &&
         call_expr->args()[0].has_ident_expr() &&
         call_expr->args()[0].ident_expr().name() == accu_var &&
         call_expr->args()[1].has_list_expr() &&
         call_expr->args()[1].list_expr().elements().size() == 1;
}

// Assuming `IsOptimizableListAppend()` return true, return a pointer to the
// call `accu_var + [elem]`.
const cel::ast_internal::Call* GetOptimizableListAppendCall(
    const cel::ast_internal::Comprehension* comprehension) {
  ABSL_DCHECK(IsOptimizableListAppend(
      comprehension, /*enable_comprehension_list_append=*/true));

  // Macro loop_step for a filter() will contain a ternary:
  //   filter ? accu_var + [elem] : accu_var
  // Macro loop_step for a map() will contain a list concat operation:
  //   accu_var + [elem]
  const auto* call_expr = &comprehension->loop_step().call_expr();

  if (call_expr->function() == cel::builtin::kTernary &&
      call_expr->args().size() == 3) {
    call_expr = &(call_expr->args()[1].call_expr());
  }
  return call_expr;
}

// Assuming `IsOptimizableListAppend()` return true, return a pointer to the
// node `[elem]`.
const cel::ast_internal::Expr* GetOptimizableListAppendOperand(
    const cel::ast_internal::Comprehension* comprehension) {
  return &GetOptimizableListAppendCall(comprehension)->args()[1];
}

bool IsBind(const cel::ast_internal::Comprehension* comprehension) {
  static constexpr absl::string_view kUnusedIterVar = "#unused";

  return comprehension->loop_condition().const_expr().has_bool_value() &&
         comprehension->loop_condition().const_expr().bool_value() == false &&
         comprehension->iter_var() == kUnusedIterVar &&
         comprehension->iter_range().has_list_expr() &&
         comprehension->iter_range().list_expr().elements().empty();
}

bool IsBlock(const cel::ast_internal::Call* call) {
  return call->function() == "cel.@block";
}

// Visitor for Comprehension expressions.
class ComprehensionVisitor {
 public:
  explicit ComprehensionVisitor(FlatExprVisitor* visitor, bool short_circuiting,
                                bool is_trivial, size_t iter_slot,
                                size_t accu_slot)
      : visitor_(visitor),
        next_step_(nullptr),
        cond_step_(nullptr),
        short_circuiting_(short_circuiting),
        is_trivial_(is_trivial),
        accu_init_extracted_(false),
        iter_slot_(iter_slot),
        accu_slot_(accu_slot) {}

  void PreVisit(const cel::ast_internal::Expr* expr);
  absl::Status PostVisitArg(cel::ComprehensionArg arg_num,
                            const cel::ast_internal::Expr* comprehension_expr) {
    if (is_trivial_) {
      PostVisitArgTrivial(arg_num, comprehension_expr);
      return absl::OkStatus();
    } else {
      return PostVisitArgDefault(arg_num, comprehension_expr);
    }
  }
  void PostVisit(const cel::ast_internal::Expr* expr);

  void MarkAccuInitExtracted() { accu_init_extracted_ = true; }

 private:
  void PostVisitArgTrivial(cel::ComprehensionArg arg_num,
                           const cel::ast_internal::Expr* comprehension_expr);

  absl::Status PostVisitArgDefault(
      cel::ComprehensionArg arg_num,
      const cel::ast_internal::Expr* comprehension_expr);

  FlatExprVisitor* visitor_;
  ComprehensionNextStep* next_step_;
  ComprehensionCondStep* cond_step_;
  ProgramStepIndex next_step_pos_;
  ProgramStepIndex cond_step_pos_;
  bool short_circuiting_;
  bool is_trivial_;
  bool accu_init_extracted_;
  size_t iter_slot_;
  size_t accu_slot_;
};

absl::flat_hash_set<int32_t> MakeOptionalIndicesSet(
    const cel::ast_internal::CreateList& create_list_expr) {
  absl::flat_hash_set<int32_t> optional_indices;
  for (size_t i = 0; i < create_list_expr.elements().size(); ++i) {
    if (create_list_expr.elements()[i].optional()) {
      optional_indices.insert(static_cast<int32_t>(i));
    }
  }
  return optional_indices;
}

absl::flat_hash_set<int32_t> MakeOptionalIndicesSet(
    const cel::ast_internal::CreateStruct& create_struct_expr) {
  absl::flat_hash_set<int32_t> optional_indices;
  for (size_t i = 0; i < create_struct_expr.fields().size(); ++i) {
    if (create_struct_expr.fields()[i].optional()) {
      optional_indices.insert(static_cast<int32_t>(i));
    }
  }
  return optional_indices;
}

absl::flat_hash_set<int32_t> MakeOptionalIndicesSet(
    const cel::MapExpr& map_expr) {
  absl::flat_hash_set<int32_t> optional_indices;
  for (size_t i = 0; i < map_expr.entries().size(); ++i) {
    if (map_expr.entries()[i].optional()) {
      optional_indices.insert(static_cast<int32_t>(i));
    }
  }
  return optional_indices;
}

class FlatExprVisitor : public cel::AstVisitor {
 public:
  FlatExprVisitor(
      const Resolver& resolver, const cel::RuntimeOptions& options,
      std::vector<std::unique_ptr<ProgramOptimizer>> program_optimizers,
      const absl::flat_hash_map<int64_t, cel::ast_internal::Reference>&
          reference_map,
      ValueManager& value_factory, IssueCollector& issue_collector,
      ProgramBuilder& program_builder, PlannerContext& extension_context,
      bool enable_optional_types)
      : resolver_(resolver),
        value_factory_(value_factory),
        progress_status_(absl::OkStatus()),
        resolved_select_expr_(nullptr),
        options_(options),
        program_optimizers_(std::move(program_optimizers)),
        issue_collector_(issue_collector),
        program_builder_(program_builder),
        extension_context_(extension_context),
        enable_optional_types_(enable_optional_types) {}

  void PreVisitExpr(const cel::ast_internal::Expr& expr) override {
    ValidateOrError(!absl::holds_alternative<cel::UnspecifiedExpr>(expr.kind()),
                    "Invalid empty expression");
    if (!progress_status_.ok()) {
      return;
    }
    if (resume_from_suppressed_branch_ == nullptr &&
        suppressed_branches_.find(&expr) != suppressed_branches_.end()) {
      resume_from_suppressed_branch_ = &expr;
    }

    if (block_.has_value()) {
      BlockInfo& block = *block_;
      if (block.in && block.bindings_set.contains(&expr)) {
        block.current_binding = &expr;
      }
    }

    program_builder_.EnterSubexpression(&expr);

    for (const std::unique_ptr<ProgramOptimizer>& optimizer :
         program_optimizers_) {
      absl::Status status = optimizer->OnPreVisit(extension_context_, expr);
      if (!status.ok()) {
        SetProgressStatusError(status);
      }
    }
  }

  void PostVisitExpr(const cel::ast_internal::Expr& expr) override {
    if (!progress_status_.ok()) {
      return;
    }
    if (&expr == resume_from_suppressed_branch_) {
      resume_from_suppressed_branch_ = nullptr;
    }

    for (const std::unique_ptr<ProgramOptimizer>& optimizer :
         program_optimizers_) {
      absl::Status status = optimizer->OnPostVisit(extension_context_, expr);
      if (!status.ok()) {
        SetProgressStatusError(status);
        return;
      }
    }

    auto* subexpression = program_builder_.current();
    if (subexpression != nullptr && options_.enable_recursive_tracing &&
        subexpression->IsRecursive()) {
      auto program = subexpression->ExtractRecursiveProgram();
      subexpression->set_recursive_program(
          std::make_unique<TraceStep>(std::move(program.step)), program.depth);
    }

    program_builder_.ExitSubexpression(&expr);

    if (!comprehension_stack_.empty() &&
        comprehension_stack_.back().is_optimizable_bind &&
        (&comprehension_stack_.back().comprehension->accu_init() == &expr)) {
      SetProgressStatusError(
          MaybeExtractSubexpression(&expr, comprehension_stack_.back()));
    }

    if (block_.has_value()) {
      BlockInfo& block = *block_;
      if (block.current_binding == &expr) {
        int index = program_builder_.ExtractSubexpression(&expr);
        if (index == -1) {
          SetProgressStatusError(
              absl::InvalidArgumentError("failed to extract subexpression"));
          return;
        }
        block.subexpressions[block.current_index++] = index;
        block.current_binding = nullptr;
      }
    }
  }

  void PostVisitConst(const cel::ast_internal::Expr& expr,
                      const cel::ast_internal::Constant& const_expr) override {
    if (!progress_status_.ok()) {
      return;
    }

    absl::StatusOr<cel::Value> converted_value =
        ConvertConstant(const_expr, value_factory_);

    if (!converted_value.ok()) {
      SetProgressStatusError(converted_value.status());
      return;
    }

    if (options_.max_recursion_depth > 0 || options_.max_recursion_depth < 0) {
      SetRecursiveStep(CreateConstValueDirectStep(
                           std::move(converted_value).value(), expr.id()),
                       1);
      return;
    }

    AddStep(
        CreateConstValueStep(std::move(converted_value).value(), expr.id()));
  }

  struct SlotLookupResult {
    int slot;
    int subexpression;
  };

  // Helper to lookup a variable mapped to a slot.
  //
  // If lazy evaluation enabled and ided as a lazy expression,
  // subexpression and slot will be set.
  SlotLookupResult LookupSlot(absl::string_view path) {
    if (block_.has_value()) {
      const BlockInfo& block = *block_;
      if (block.in) {
        absl::string_view index_suffix = path;
        if (absl::ConsumePrefix(&index_suffix, "@index")) {
          size_t index;
          if (!absl::SimpleAtoi(index_suffix, &index)) {
            SetProgressStatusError(
                issue_collector_.AddIssue(RuntimeIssue::CreateError(
                    absl::InvalidArgumentError("bad @index"))));
            return {-1, -1};
          }
          if (index >= block.size) {
            SetProgressStatusError(
                issue_collector_.AddIssue(RuntimeIssue::CreateError(
                    absl::InvalidArgumentError(absl::StrCat(
                        "invalid @index greater than number of bindings: ",
                        index, " >= ", block.size)))));
            return {-1, -1};
          }
          if (index >= block.current_index) {
            SetProgressStatusError(
                issue_collector_.AddIssue(RuntimeIssue::CreateError(
                    absl::InvalidArgumentError(absl::StrCat(
                        "@index references current or future binding: ", index,
                        " >= ", block.current_index)))));
            return {-1, -1};
          }
          return {static_cast<int>(block.index + index),
                  block.subexpressions[index]};
        }
      }
    }
    if (!comprehension_stack_.empty()) {
      for (int i = comprehension_stack_.size() - 1; i >= 0; i--) {
        const ComprehensionStackRecord& record = comprehension_stack_[i];
        if (record.iter_var_in_scope &&
            record.comprehension->iter_var() == path) {
          if (record.is_optimizable_bind) {
            SetProgressStatusError(issue_collector_.AddIssue(
                RuntimeIssue::CreateWarning(absl::InvalidArgumentError(
                    "Unexpected iter_var access in trivial comprehension"))));
            return {-1, -1};
          }
          return {static_cast<int>(record.iter_slot), -1};
        }
        if (record.accu_var_in_scope &&
            record.comprehension->accu_var() == path) {
          int slot = record.accu_slot;
          int subexpression = -1;
          if (record.is_optimizable_bind) {
            subexpression = record.subexpression;
          }
          return {slot, subexpression};
        }
      }
    }
    if (absl::StartsWith(path, "@it:") || absl::StartsWith(path, "@it2:") ||
        absl::StartsWith(path, "@ac:")) {
      // If we see a CSE generated comprehension variable that was not
      // resolvable through the normal comprehension scope resolution, reject it
      // now rather than surfacing errors at activation time.
      SetProgressStatusError(
          issue_collector_.AddIssue(RuntimeIssue::CreateError(
              absl::InvalidArgumentError("out of scope reference to CSE "
                                         "generated comprehension variable"))));
    }
    return {-1, -1};
  }

  // Ident node handler.
  // Invoked after child nodes are processed.
  void PostVisitIdent(const cel::ast_internal::Expr& expr,
                      const cel::ast_internal::Ident& ident_expr) override {
    if (!progress_status_.ok()) {
      return;
    }
    std::string path = ident_expr.name();
    if (!ValidateOrError(
            !path.empty(),
            "Invalid expression: identifier 'name' must not be empty")) {
      return;
    }

    // Attempt to resolve a select expression as a namespaced identifier for an
    // enum or type constant value.
    absl::optional<cel::Value> const_value;
    int64_t select_root_id = -1;

    while (!namespace_stack_.empty()) {
      const auto& select_node = namespace_stack_.front();
      // Generate path in format "<ident>.<field 0>.<field 1>...".
      auto select_expr = select_node.first;
      auto qualified_path = absl::StrCat(path, ".", select_node.second);

      // Attempt to find a constant enum or type value which matches the
      // qualified path present in the expression. Whether the identifier
      // can be resolved to a type instance depends on whether the option to
      // 'enable_qualified_type_identifiers' is set to true.
      const_value = resolver_.FindConstant(qualified_path, select_expr->id());
      if (const_value) {
        resolved_select_expr_ = select_expr;
        select_root_id = select_expr->id();
        path = qualified_path;
        namespace_stack_.clear();
        break;
      }
      namespace_stack_.pop_front();
    }

    if (!const_value) {
      // Attempt to resolve a simple identifier as an enum or type constant
      // value.
      const_value = resolver_.FindConstant(path, expr.id());
      select_root_id = expr.id();
    }

    if (const_value) {
      if (options_.max_recursion_depth != 0) {
        SetRecursiveStep(CreateDirectShadowableValueStep(
                             std::move(path), std::move(const_value).value(),
                             select_root_id),
                         1);
        return;
      }
      AddStep(CreateShadowableValueStep(
          std::move(path), std::move(const_value).value(), select_root_id));
      return;
    }

    // If this is a comprehension variable, check for the assigned slot.
    SlotLookupResult slot = LookupSlot(path);

    if (slot.subexpression >= 0) {
      auto* subexpression =
          program_builder_.GetExtractedSubexpression(slot.subexpression);
      if (subexpression == nullptr) {
        SetProgressStatusError(
            absl::InternalError("bad subexpression reference"));
        return;
      }
      if (subexpression->IsRecursive()) {
        const auto& program = subexpression->recursive_program();
        SetRecursiveStep(
            CreateDirectLazyInitStep(slot.slot, program.step.get(), expr.id()),
            program.depth + 1);
      } else {
        // Off by one since mainline expression will be index 0.
        AddStep(
            CreateLazyInitStep(slot.slot, slot.subexpression + 1, expr.id()));
      }
      return;
    } else if (slot.slot >= 0) {
      if (options_.max_recursion_depth != 0) {
        SetRecursiveStep(
            CreateDirectSlotIdentStep(ident_expr.name(), slot.slot, expr.id()),
            1);
      } else {
        AddStep(CreateIdentStepForSlot(ident_expr, slot.slot, expr.id()));
      }
      return;
    }
    if (options_.max_recursion_depth != 0) {
      SetRecursiveStep(CreateDirectIdentStep(ident_expr.name(), expr.id()), 1);
    } else {
      AddStep(CreateIdentStep(ident_expr, expr.id()));
    }
  }

  void PreVisitSelect(const cel::ast_internal::Expr& expr,
                      const cel::ast_internal::Select& select_expr) override {
    if (!progress_status_.ok()) {
      return;
    }
    if (!ValidateOrError(
            !select_expr.field().empty(),
            "Invalid expression: select 'field' must not be empty")) {
      return;
    }

    // Not exactly the cleanest solution - we peek into child of
    // select_expr.
    // Chain of multiple SELECT ending with IDENT can represent namespaced
    // entity.
    if (!select_expr.test_only() && (select_expr.operand().has_ident_expr() ||
                                     select_expr.operand().has_select_expr())) {
      // select expressions are pushed in reverse order:
      // google.type.Expr is pushed as:
      // - field: 'Expr'
      // - field: 'type'
      // - id: 'google'
      //
      // The search order though is as follows:
      // - id: 'google.type.Expr'
      // - id: 'google.type', field: 'Expr'
      // - id: 'google', field: 'type', field: 'Expr'
      for (size_t i = 0; i < namespace_stack_.size(); i++) {
        auto ns = namespace_stack_[i];
        namespace_stack_[i] = {
            ns.first, absl::StrCat(select_expr.field(), ".", ns.second)};
      }
      namespace_stack_.push_back({&expr, select_expr.field()});
    } else {
      namespace_stack_.clear();
    }
  }

  // Select node handler.
  // Invoked after child nodes are processed.
  void PostVisitSelect(const cel::ast_internal::Expr& expr,
                       const cel::ast_internal::Select& select_expr) override {
    if (!progress_status_.ok()) {
      return;
    }

    // Check if we are "in the middle" of namespaced name.
    // This is currently enum specific. Constant expression that corresponds
    // to resolved enum value has been already created, thus preceding chain
    // of selects is no longer relevant.
    if (resolved_select_expr_) {
      if (&expr == resolved_select_expr_) {
        resolved_select_expr_ = nullptr;
      }
      return;
    }

    auto depth = RecursionEligible();
    if (depth.has_value()) {
      auto deps = ExtractRecursiveDependencies();
      if (deps.size() != 1) {
        SetProgressStatusError(absl::InternalError(
            "unexpected number of dependencies for select operation."));
        return;
      }
      StringValue field =
          value_factory_.CreateUncheckedStringValue(select_expr.field());

      SetRecursiveStep(
          CreateDirectSelectStep(std::move(deps[0]), std::move(field),
                                 select_expr.test_only(), expr.id(),
                                 options_.enable_empty_wrapper_null_unboxing,
                                 enable_optional_types_),
          *depth + 1);
      return;
    }

    AddStep(CreateSelectStep(select_expr, expr.id(),
                             options_.enable_empty_wrapper_null_unboxing,
                             value_factory_, enable_optional_types_));
  }

  // Call node handler group.
  // We provide finer granularity for Call node callbacks to allow special
  // handling for short-circuiting
  // PreVisitCall is invoked before child nodes are processed.
  void PreVisitCall(const cel::ast_internal::Expr& expr,
                    const cel::ast_internal::Call& call_expr) override {
    if (!progress_status_.ok()) {
      return;
    }

    std::unique_ptr<CondVisitor> cond_visitor;
    if (call_expr.function() == cel::builtin::kAnd) {
      cond_visitor = std::make_unique<BinaryCondVisitor>(
          this, BinaryCond::kAnd, options_.short_circuiting);
    } else if (call_expr.function() == cel::builtin::kOr) {
      cond_visitor = std::make_unique<BinaryCondVisitor>(
          this, BinaryCond::kOr, options_.short_circuiting);
    } else if (call_expr.function() == cel::builtin::kTernary) {
      if (options_.short_circuiting) {
        cond_visitor = std::make_unique<TernaryCondVisitor>(this);
      } else {
        cond_visitor = std::make_unique<ExhaustiveTernaryCondVisitor>(this);
      }
    } else if (enable_optional_types_ &&
               call_expr.function() == kOptionalOrFn &&
               call_expr.has_target() && call_expr.args().size() == 1) {
      cond_visitor = std::make_unique<BinaryCondVisitor>(
          this, BinaryCond::kOptionalOr, options_.short_circuiting);
    } else if (enable_optional_types_ &&
               call_expr.function() == kOptionalOrValueFn &&
               call_expr.has_target() && call_expr.args().size() == 1) {
      cond_visitor = std::make_unique<BinaryCondVisitor>(
          this, BinaryCond::kOptionalOrValue, options_.short_circuiting);
    } else if (IsBlock(&call_expr)) {
      // cel.@block
      if (block_.has_value()) {
        // There can only be one for now.
        SetProgressStatusError(
            absl::InvalidArgumentError("multiple cel.@block are not allowed"));
        return;
      }
      block_ = BlockInfo();
      BlockInfo& block = *block_;
      block.in = true;
      if (call_expr.args().empty()) {
        SetProgressStatusError(absl::InvalidArgumentError(
            "malformed cel.@block: missing list of bound expressions"));
        return;
      }
      if (call_expr.args().size() != 2) {
        SetProgressStatusError(absl::InvalidArgumentError(
            "malformed cel.@block: missing bound expression"));
        return;
      }
      if (!call_expr.args()[0].has_list_expr()) {
        SetProgressStatusError(
            absl::InvalidArgumentError("malformed cel.@block: first argument "
                                       "is not a list of bound expressions"));
        return;
      }
      const auto& list_expr = call_expr.args().front().list_expr();
      block.size = list_expr.elements().size();
      if (block.size == 0) {
        SetProgressStatusError(absl::InvalidArgumentError(
            "malformed cel.@block: list of bound expressions is empty"));
        return;
      }
      block.bindings_set.reserve(block.size);
      for (const auto& list_expr_element : list_expr.elements()) {
        if (list_expr_element.optional()) {
          SetProgressStatusError(
              absl::InvalidArgumentError("malformed cel.@block: list of bound "
                                         "expressions contains an optional"));
          return;
        }
        block.bindings_set.insert(&list_expr_element.expr());
      }
      block.index = index_manager().ReserveSlots(block.size);
      block.expr = &expr;
      block.bindings = &call_expr.args()[0];
      block.bound = &call_expr.args()[1];
      block.subexpressions.resize(block.size, -1);
    } else {
      return;
    }

    if (cond_visitor) {
      cond_visitor->PreVisit(&expr);
      cond_visitor_stack_.push({&expr, std::move(cond_visitor)});
    }
  }

  absl::optional<int> RecursionEligible() {
    if (program_builder_.current() == nullptr) {
      return absl::nullopt;
    }
    absl::optional<int> depth =
        program_builder_.current()->RecursiveDependencyDepth();
    if (!depth.has_value()) {
      // one or more of the dependencies isn't eligible.
      return depth;
    }
    if (options_.max_recursion_depth < 0 ||
        *depth < options_.max_recursion_depth) {
      return depth;
    }
    return absl::nullopt;
  }

  std::vector<std::unique_ptr<DirectExpressionStep>>
  ExtractRecursiveDependencies() {
    // Must check recursion eligibility before calling.
    ABSL_DCHECK(program_builder_.current() != nullptr);

    return program_builder_.current()->ExtractRecursiveDependencies();
  }

  void MaybeMakeTernaryRecursive(const cel::ast_internal::Expr* expr) {
    if (options_.max_recursion_depth == 0) {
      return;
    }
    if (expr->call_expr().args().size() != 3) {
      SetProgressStatusError(absl::InvalidArgumentError(
          "unexpected number of args for builtin ternary"));
    }

    const cel::ast_internal::Expr* condition_expr =
        &expr->call_expr().args()[0];
    const cel::ast_internal::Expr* left_expr = &expr->call_expr().args()[1];
    const cel::ast_internal::Expr* right_expr = &expr->call_expr().args()[2];

    auto* condition_plan = program_builder_.GetSubexpression(condition_expr);
    auto* left_plan = program_builder_.GetSubexpression(left_expr);
    auto* right_plan = program_builder_.GetSubexpression(right_expr);

    int max_depth = 0;
    if (condition_plan == nullptr || !condition_plan->IsRecursive()) {
      return;
    }
    max_depth = std::max(max_depth, condition_plan->recursive_program().depth);

    if (left_plan == nullptr || !left_plan->IsRecursive()) {
      return;
    }
    max_depth = std::max(max_depth, left_plan->recursive_program().depth);

    if (right_plan == nullptr || !right_plan->IsRecursive()) {
      return;
    }
    max_depth = std::max(max_depth, right_plan->recursive_program().depth);

    if (options_.max_recursion_depth >= 0 &&
        max_depth >= options_.max_recursion_depth) {
      return;
    }

    SetRecursiveStep(
        CreateDirectTernaryStep(condition_plan->ExtractRecursiveProgram().step,
                                left_plan->ExtractRecursiveProgram().step,
                                right_plan->ExtractRecursiveProgram().step,
                                expr->id(), options_.short_circuiting),
        max_depth + 1);
  }

  void MaybeMakeShortcircuitRecursive(const cel::ast_internal::Expr* expr,
                                      bool is_or) {
    if (options_.max_recursion_depth == 0) {
      return;
    }
    if (expr->call_expr().args().size() != 2) {
      SetProgressStatusError(absl::InvalidArgumentError(
          "unexpected number of args for builtin boolean operator &&/||"));
    }
    const cel::ast_internal::Expr* left_expr = &expr->call_expr().args()[0];
    const cel::ast_internal::Expr* right_expr = &expr->call_expr().args()[1];

    auto* left_plan = program_builder_.GetSubexpression(left_expr);
    auto* right_plan = program_builder_.GetSubexpression(right_expr);

    int max_depth = 0;
    if (left_plan == nullptr || !left_plan->IsRecursive()) {
      return;
    }
    max_depth = std::max(max_depth, left_plan->recursive_program().depth);

    if (right_plan == nullptr || !right_plan->IsRecursive()) {
      return;
    }
    max_depth = std::max(max_depth, right_plan->recursive_program().depth);

    if (options_.max_recursion_depth >= 0 &&
        max_depth >= options_.max_recursion_depth) {
      return;
    }

    if (is_or) {
      SetRecursiveStep(
          CreateDirectOrStep(left_plan->ExtractRecursiveProgram().step,
                             right_plan->ExtractRecursiveProgram().step,
                             expr->id(), options_.short_circuiting),
          max_depth + 1);
    } else {
      SetRecursiveStep(
          CreateDirectAndStep(left_plan->ExtractRecursiveProgram().step,
                              right_plan->ExtractRecursiveProgram().step,
                              expr->id(), options_.short_circuiting),
          max_depth + 1);
    }
  }

  void MaybeMakeOptionalShortcircuitRecursive(
      const cel::ast_internal::Expr* expr, bool is_or_value) {
    if (options_.max_recursion_depth == 0) {
      return;
    }
    if (!expr->call_expr().has_target() ||
        expr->call_expr().args().size() != 1) {
      SetProgressStatusError(absl::InvalidArgumentError(
          "unexpected number of args for optional.or{Value}"));
    }
    const cel::ast_internal::Expr* left_expr = &expr->call_expr().target();
    const cel::ast_internal::Expr* right_expr = &expr->call_expr().args()[0];

    auto* left_plan = program_builder_.GetSubexpression(left_expr);
    auto* right_plan = program_builder_.GetSubexpression(right_expr);

    int max_depth = 0;
    if (left_plan == nullptr || !left_plan->IsRecursive()) {
      return;
    }
    max_depth = std::max(max_depth, left_plan->recursive_program().depth);

    if (right_plan == nullptr || !right_plan->IsRecursive()) {
      return;
    }
    max_depth = std::max(max_depth, right_plan->recursive_program().depth);

    if (options_.max_recursion_depth >= 0 &&
        max_depth >= options_.max_recursion_depth) {
      return;
    }

    SetRecursiveStep(CreateDirectOptionalOrStep(
                         expr->id(), left_plan->ExtractRecursiveProgram().step,
                         right_plan->ExtractRecursiveProgram().step,
                         is_or_value, options_.short_circuiting),
                     max_depth + 1);
  }

  void MaybeMakeBindRecursive(
      const cel::ast_internal::Expr* expr,
      const cel::ast_internal::Comprehension* comprehension, size_t accu_slot) {
    if (options_.max_recursion_depth == 0) {
      return;
    }

    auto* result_plan =
        program_builder_.GetSubexpression(&comprehension->result());

    if (result_plan == nullptr || !result_plan->IsRecursive()) {
      return;
    }

    int result_depth = result_plan->recursive_program().depth;

    if (options_.max_recursion_depth > 0 &&
        result_depth >= options_.max_recursion_depth) {
      return;
    }

    auto program = result_plan->ExtractRecursiveProgram();
    SetRecursiveStep(
        CreateDirectBindStep(accu_slot, std::move(program.step), expr->id()),
        result_depth + 1);
  }

  void MaybeMakeComprehensionRecursive(
      const cel::ast_internal::Expr* expr,
      const cel::ast_internal::Comprehension* comprehension, size_t iter_slot,
      size_t accu_slot) {
    if (options_.max_recursion_depth == 0) {
      return;
    }

    auto* accu_plan =
        program_builder_.GetSubexpression(&comprehension->accu_init());

    if (accu_plan == nullptr || !accu_plan->IsRecursive()) {
      return;
    }

    auto* range_plan =
        program_builder_.GetSubexpression(&comprehension->iter_range());

    if (range_plan == nullptr || !range_plan->IsRecursive()) {
      return;
    }

    auto* loop_plan =
        program_builder_.GetSubexpression(&comprehension->loop_step());

    if (loop_plan == nullptr || !loop_plan->IsRecursive()) {
      return;
    }

    auto* condition_plan =
        program_builder_.GetSubexpression(&comprehension->loop_condition());

    if (condition_plan == nullptr || !condition_plan->IsRecursive()) {
      return;
    }

    auto* result_plan =
        program_builder_.GetSubexpression(&comprehension->result());

    if (result_plan == nullptr || !result_plan->IsRecursive()) {
      return;
    }

    int max_depth = 0;
    max_depth = std::max(max_depth, accu_plan->recursive_program().depth);
    max_depth = std::max(max_depth, range_plan->recursive_program().depth);
    max_depth = std::max(max_depth, loop_plan->recursive_program().depth);
    max_depth = std::max(max_depth, condition_plan->recursive_program().depth);
    max_depth = std::max(max_depth, result_plan->recursive_program().depth);

    if (options_.max_recursion_depth > 0 &&
        max_depth >= options_.max_recursion_depth) {
      return;
    }

    auto step = CreateDirectComprehensionStep(
        iter_slot, accu_slot, range_plan->ExtractRecursiveProgram().step,
        accu_plan->ExtractRecursiveProgram().step,
        loop_plan->ExtractRecursiveProgram().step,
        condition_plan->ExtractRecursiveProgram().step,
        result_plan->ExtractRecursiveProgram().step, options_.short_circuiting,
        expr->id());

    SetRecursiveStep(std::move(step), max_depth + 1);
  }

  // Invoked after all child nodes are processed.
  void PostVisitCall(const cel::ast_internal::Expr& expr,
                     const cel::ast_internal::Call& call_expr) override {
    if (!progress_status_.ok()) {
      return;
    }

    auto cond_visitor = FindCondVisitor(&expr);
    if (cond_visitor) {
      cond_visitor->PostVisit(&expr);
      cond_visitor_stack_.pop();
      if (call_expr.function() == cel::builtin::kTernary) {
        MaybeMakeTernaryRecursive(&expr);
      } else if (call_expr.function() == cel::builtin::kOr) {
        MaybeMakeShortcircuitRecursive(&expr, /* is_or= */ true);
      } else if (call_expr.function() == cel::builtin::kAnd) {
        MaybeMakeShortcircuitRecursive(&expr, /* is_or= */ false);
      } else if (enable_optional_types_) {
        if (call_expr.function() == kOptionalOrFn) {
          MaybeMakeOptionalShortcircuitRecursive(&expr,
                                                 /* is_or_value= */ false);
        } else if (call_expr.function() == kOptionalOrValueFn) {
          MaybeMakeOptionalShortcircuitRecursive(&expr,
                                                 /* is_or_value= */ true);
        }
      }
      return;
    }

    // Special case for "_[_]".
    if (call_expr.function() == cel::builtin::kIndex) {
      auto depth = RecursionEligible();
      if (depth.has_value()) {
        auto args = ExtractRecursiveDependencies();
        if (args.size() != 2) {
          SetProgressStatusError(absl::InvalidArgumentError(
              "unexpected number of args for builtin index operator"));
        }
        SetRecursiveStep(CreateDirectContainerAccessStep(
                             std::move(args[0]), std::move(args[1]),
                             enable_optional_types_, expr.id()),
                         *depth + 1);
        return;
      }
      AddStep(CreateContainerAccessStep(call_expr, expr.id(),
                                        enable_optional_types_));
      return;
    }

    if (block_.has_value()) {
      BlockInfo& block = *block_;
      if (block.expr == &expr) {
        block.in = false;
        index_manager().ReleaseSlots(block.size);
        AddStep(CreateClearSlotsStep(block.index, block.size, -1));
        return;
      }
    }

    // Establish the search criteria for a given function.
    absl::string_view function = call_expr.function();

    // Check to see if this is a special case of add that should really be
    // treated as a list append
    if (!comprehension_stack_.empty() &&
        comprehension_stack_.back().is_optimizable_list_append) {
      // Already checked that this is an optimizeable comprehension,
      // check that this is the correct list append node.
      const cel::ast_internal::Comprehension* comprehension =
          comprehension_stack_.back().comprehension;
      const cel::ast_internal::Expr& loop_step = comprehension->loop_step();
      // Macro loop_step for a map() will contain a list concat operation:
      //   accu_var + [elem]
      if (&loop_step == &expr) {
        function = cel::builtin::kRuntimeListAppend;
      }
      // Macro loop_step for a filter() will contain a ternary:
      //   filter ? accu_var + [elem] : accu_var
      if (loop_step.has_call_expr() &&
          loop_step.call_expr().function() == cel::builtin::kTernary &&
          loop_step.call_expr().args().size() == 3 &&
          &(loop_step.call_expr().args()[1]) == &expr) {
        function = cel::builtin::kRuntimeListAppend;
      }
    }

    AddResolvedFunctionStep(&call_expr, &expr, function);
  }

  void PreVisitComprehension(
      const cel::ast_internal::Expr& expr,
      const cel::ast_internal::Comprehension& comprehension) override {
    if (!progress_status_.ok()) {
      return;
    }
    if (!ValidateOrError(options_.enable_comprehension,
                         "Comprehension support is disabled")) {
      return;
    }
    const auto& accu_var = comprehension.accu_var();
    const auto& iter_var = comprehension.iter_var();
    ValidateOrError(!accu_var.empty(),
                    "Invalid comprehension: 'accu_var' must not be empty");
    ValidateOrError(!iter_var.empty(),
                    "Invalid comprehension: 'iter_var' must not be empty");
    ValidateOrError(
        accu_var != iter_var,
        "Invalid comprehension: 'accu_var' must not be the same as 'iter_var'");
    ValidateOrError(comprehension.has_accu_init(),
                    "Invalid comprehension: 'accu_init' must be set");
    ValidateOrError(comprehension.has_loop_condition(),
                    "Invalid comprehension: 'loop_condition' must be set");
    ValidateOrError(comprehension.has_loop_step(),
                    "Invalid comprehension: 'loop_step' must be set");
    ValidateOrError(comprehension.has_result(),
                    "Invalid comprehension: 'result' must be set");

    size_t iter_slot, accu_slot, slot_count;
    bool is_bind = IsBind(&comprehension);
    if (is_bind) {
      accu_slot = iter_slot = index_manager_.ReserveSlots(1);
      slot_count = 1;
    } else {
      iter_slot = index_manager_.ReserveSlots(2);
      accu_slot = iter_slot + 1;
      slot_count = 2;
    }
    // If this is in the scope of an optimized bind accu-init, account the slots
    // to the outermost bind-init scope.
    //
    // The init expression is effectively inlined at the first usage in the
    // critical path (which is unknown at plan time), so the used slots need to
    // be dedicated for the entire scope of that bind.
    for (ComprehensionStackRecord& record : comprehension_stack_) {
      if (record.in_accu_init && record.is_optimizable_bind) {
        record.slot_count += slot_count;
        slot_count = 0;
        break;
      }
      // If no bind init subexpression, account normally.
    }

    comprehension_stack_.push_back(
        {&expr, &comprehension, iter_slot, accu_slot, slot_count,
         /*subexpression=*/-1,
         IsOptimizableListAppend(&comprehension,
                                 options_.enable_comprehension_list_append),
         is_bind,
         /*.iter_var_in_scope=*/false,
         /*.accu_var_in_scope=*/false,
         /*.in_accu_init=*/false,
         std::make_unique<ComprehensionVisitor>(
             this, options_.short_circuiting, is_bind, iter_slot, accu_slot)});
    comprehension_stack_.back().visitor->PreVisit(&expr);
  }

  // Invoked after all child nodes are processed.
  void PostVisitComprehension(
      const cel::ast_internal::Expr& expr,
      const cel::ast_internal::Comprehension& comprehension_expr) override {
    if (!progress_status_.ok()) {
      return;
    }

    ComprehensionStackRecord& record = comprehension_stack_.back();
    if (comprehension_stack_.empty() ||
        record.comprehension != &comprehension_expr) {
      return;
    }

    record.visitor->PostVisit(&expr);

    index_manager_.ReleaseSlots(record.slot_count);
    comprehension_stack_.pop_back();
  }

  void PreVisitComprehensionSubexpression(
      const cel::ast_internal::Expr& expr,
      const cel::ast_internal::Comprehension& compr,
      cel::ComprehensionArg comprehension_arg) override {
    if (!progress_status_.ok()) {
      return;
    }

    if (comprehension_stack_.empty() ||
        comprehension_stack_.back().comprehension != &compr) {
      return;
    }

    ComprehensionStackRecord& record = comprehension_stack_.back();

    switch (comprehension_arg) {
      case cel::ITER_RANGE: {
        record.in_accu_init = false;
        record.iter_var_in_scope = false;
        record.accu_var_in_scope = false;
        break;
      }
      case cel::ACCU_INIT: {
        record.in_accu_init = true;
        record.iter_var_in_scope = false;
        record.accu_var_in_scope = false;
        break;
      }
      case cel::LOOP_CONDITION: {
        record.in_accu_init = false;
        record.iter_var_in_scope = true;
        record.accu_var_in_scope = true;
        break;
      }
      case cel::LOOP_STEP: {
        record.in_accu_init = false;
        record.iter_var_in_scope = true;
        record.accu_var_in_scope = true;
        break;
      }
      case cel::RESULT: {
        record.in_accu_init = false;
        record.iter_var_in_scope = false;
        record.accu_var_in_scope = true;
        break;
      }
    }
  }

  void PostVisitComprehensionSubexpression(
      const cel::ast_internal::Expr& expr,
      const cel::ast_internal::Comprehension& compr,
      cel::ComprehensionArg comprehension_arg) override {
    if (!progress_status_.ok()) {
      return;
    }

    if (comprehension_stack_.empty() ||
        comprehension_stack_.back().comprehension != &compr) {
      return;
    }

    SetProgressStatusError(comprehension_stack_.back().visitor->PostVisitArg(
        comprehension_arg, comprehension_stack_.back().expr));
  }

  // Invoked after each argument node processed.
  void PostVisitArg(const cel::ast_internal::Expr& expr, int arg_num) override {
    if (!progress_status_.ok()) {
      return;
    }
    auto cond_visitor = FindCondVisitor(&expr);
    if (cond_visitor) {
      cond_visitor->PostVisitArg(arg_num, &expr);
    }
  }

  void PostVisitTarget(const cel::ast_internal::Expr& expr) override {
    if (!progress_status_.ok()) {
      return;
    }
    auto cond_visitor = FindCondVisitor(&expr);
    if (cond_visitor) {
      cond_visitor->PostVisitTarget(&expr);
    }
  }

  // CreateList node handler.
  // Invoked after child nodes are processed.
  void PostVisitList(const cel::ast_internal::Expr& expr,
                     const cel::ast_internal::CreateList& list_expr) override {
    if (!progress_status_.ok()) {
      return;
    }

    if (block_.has_value()) {
      BlockInfo& block = *block_;
      if (block.bindings == &expr) {
        // Do nothing, this is the cel.@block bindings list.
        return;
      }
    }

    if (!comprehension_stack_.empty()) {
      const ComprehensionStackRecord& comprehension =
          comprehension_stack_.back();
      if (comprehension.is_optimizable_list_append) {
        if (&(comprehension.comprehension->accu_init()) == &expr) {
          if (options_.max_recursion_depth != 0) {
            SetRecursiveStep(CreateDirectMutableListStep(expr.id()), 1);
            return;
          }
          AddStep(CreateMutableListStep(expr.id()));
          return;
        }
        if (GetOptimizableListAppendOperand(comprehension.comprehension) ==
            &expr) {
          return;
        }
      }
    }
    absl::optional<int> depth = RecursionEligible();
    if (depth.has_value()) {
      auto deps = ExtractRecursiveDependencies();
      if (deps.size() != list_expr.elements().size()) {
        SetProgressStatusError(absl::InternalError(
            "Unexpected number of plan elements for CreateList expr"));
        return;
      }
      auto step = CreateDirectListStep(
          std::move(deps), MakeOptionalIndicesSet(list_expr), expr.id());
      SetRecursiveStep(std::move(step), *depth + 1);
      return;
    }
    AddStep(CreateCreateListStep(list_expr, expr.id()));
  }

  // CreateStruct node handler.
  // Invoked after child nodes are processed.
  void PostVisitStruct(
      const cel::ast_internal::Expr& expr,
      const cel::ast_internal::CreateStruct& struct_expr) override {
    if (!progress_status_.ok()) {
      return;
    }

    auto status_or_resolved_fields =
        ResolveCreateStructFields(struct_expr, expr.id());
    if (!status_or_resolved_fields.ok()) {
      SetProgressStatusError(status_or_resolved_fields.status());
      return;
    }
    std::string resolved_name =
        std::move(status_or_resolved_fields.value().first);
    std::vector<std::string> fields =
        std::move(status_or_resolved_fields.value().second);

    auto depth = RecursionEligible();
    if (depth.has_value()) {
      auto deps = ExtractRecursiveDependencies();
      if (deps.size() != struct_expr.fields().size()) {
        SetProgressStatusError(absl::InternalError(
            "Unexpected number of plan elements for CreateStruct expr"));
        return;
      }
      auto step = CreateDirectCreateStructStep(
          std::move(resolved_name), std::move(fields), std::move(deps),
          MakeOptionalIndicesSet(struct_expr), expr.id());
      SetRecursiveStep(std::move(step), *depth + 1);
      return;
    }

    AddStep(CreateCreateStructStep(std::move(resolved_name), std::move(fields),
                                   MakeOptionalIndicesSet(struct_expr),
                                   expr.id()));
  }

  void PostVisitMap(const cel::ast_internal::Expr& expr,
                    const cel::MapExpr& map_expr) override {
    for (const auto& entry : map_expr.entries()) {
      ValidateOrError(entry.has_key(), "Map entry missing key");
      ValidateOrError(entry.has_value(), "Map entry missing value");
    }
    auto depth = RecursionEligible();
    if (depth.has_value()) {
      auto deps = ExtractRecursiveDependencies();
      if (deps.size() != 2 * map_expr.entries().size()) {
        SetProgressStatusError(absl::InternalError(
            "Unexpected number of plan elements for CreateStruct expr"));
        return;
      }
      auto step = CreateDirectCreateMapStep(
          std::move(deps), MakeOptionalIndicesSet(map_expr), expr.id());
      SetRecursiveStep(std::move(step), *depth + 1);
      return;
    }
    AddStep(CreateCreateStructStepForMap(map_expr.entries().size(),
                                         MakeOptionalIndicesSet(map_expr),
                                         expr.id()));
  }

  absl::Status progress_status() const { return progress_status_; }

  cel::ValueManager& value_factory() { return value_factory_; }

  // Mark a branch as suppressed. The visitor will continue as normal, but
  // any emitted program steps are ignored.
  //
  // Only applies to branches that have not yet been visited (pre-order).
  void SuppressBranch(const cel::ast_internal::Expr* expr) {
    suppressed_branches_.insert(expr);
  }

  void AddResolvedFunctionStep(const cel::ast_internal::Call* call_expr,
                               const cel::ast_internal::Expr* expr,
                               absl::string_view function) {
    // Establish the search criteria for a given function.
    bool receiver_style = call_expr->has_target();
    size_t num_args = call_expr->args().size() + (receiver_style ? 1 : 0);
    auto arguments_matcher = ArgumentsMatcher(num_args);

    // First, search for lazily defined function overloads.
    // Lazy functions shadow eager functions with the same signature.
    auto lazy_overloads = resolver_.FindLazyOverloads(
        function, call_expr->has_target(), arguments_matcher, expr->id());
    if (!lazy_overloads.empty()) {
      auto depth = RecursionEligible();
      if (depth.has_value()) {
        auto args = program_builder_.current()->ExtractRecursiveDependencies();
        SetRecursiveStep(CreateDirectLazyFunctionStep(
                             expr->id(), *call_expr, std::move(args),
                             std::move(lazy_overloads)),
                         *depth + 1);
        return;
      }
      AddStep(CreateFunctionStep(*call_expr, expr->id(),
                                 std::move(lazy_overloads)));
      return;
    }

    // Second, search for eagerly defined function overloads.
    auto overloads = resolver_.FindOverloads(function, receiver_style,
                                             arguments_matcher, expr->id());
    if (overloads.empty()) {
      // Create a warning that the overload could not be found. Depending on the
      // builder_warnings configuration, this could result in termination of the
      // CelExpression creation or an inspectable warning for use within runtime
      // logging.
      auto status = issue_collector_.AddIssue(RuntimeIssue::CreateWarning(
          absl::InvalidArgumentError(
              "No overloads provided for FunctionStep creation"),
          RuntimeIssue::ErrorCode::kNoMatchingOverload));
      if (!status.ok()) {
        SetProgressStatusError(status);
        return;
      }
    }
    auto recursion_depth = RecursionEligible();
    if (recursion_depth.has_value()) {
      // Nonnull while active -- nullptr indicates logic error elsewhere in the
      // builder.
      ABSL_DCHECK(program_builder_.current() != nullptr);
      auto args = program_builder_.current()->ExtractRecursiveDependencies();
      SetRecursiveStep(
          CreateDirectFunctionStep(expr->id(), *call_expr, std::move(args),
                                   std::move(overloads)),
          *recursion_depth + 1);
      return;
    }
    AddStep(CreateFunctionStep(*call_expr, expr->id(), std::move(overloads)));
  }

  void AddStep(absl::StatusOr<std::unique_ptr<ExpressionStep>> step) {
    if (step.ok()) {
      AddStep(*std::move(step));
    } else {
      SetProgressStatusError(step.status());
    }
  }

  void AddStep(std::unique_ptr<ExpressionStep> step) {
    if (progress_status_.ok() && !PlanningSuppressed()) {
      program_builder_.AddStep(std::move(step));
    }
  }

  void SetRecursiveStep(std::unique_ptr<DirectExpressionStep> step, int depth) {
    if (!progress_status_.ok() || PlanningSuppressed()) {
      return;
    }
    if (program_builder_.current() == nullptr) {
      SetProgressStatusError(absl::InternalError(
          "CEL AST traversal out of order in flat_expr_builder."));
      return;
    }
    program_builder_.current()->set_recursive_program(std::move(step), depth);
  }

  void SetProgressStatusError(const absl::Status& status) {
    if (progress_status_.ok() && !status.ok()) {
      progress_status_ = status;
    }
  }

  // Index of the next step to be inserted, in terms of the current
  // subexpression
  ProgramStepIndex GetCurrentIndex() const {
    // Nonnull while active -- nullptr indicates logic error in the builder.
    ABSL_DCHECK(program_builder_.current() != nullptr);
    return {static_cast<int>(program_builder_.current()->elements().size()),
            program_builder_.current()};
  }

  CondVisitor* FindCondVisitor(const cel::ast_internal::Expr* expr) const {
    if (cond_visitor_stack_.empty()) {
      return nullptr;
    }

    const auto& latest = cond_visitor_stack_.top();

    return (latest.first == expr) ? latest.second.get() : nullptr;
  }

  IndexManager& index_manager() { return index_manager_; }

  size_t slot_count() const { return index_manager_.max_slot_count(); }

  void AddOptimizer(std::unique_ptr<ProgramOptimizer> optimizer) {
    program_optimizers_.push_back(std::move(optimizer));
  }

  // Tests the boolean predicate, and if false produces an InvalidArgumentError
  // which concatenates the error_message and any optional message_parts as the
  // error status message.
  template <typename... MP>
  bool ValidateOrError(bool valid_expression, absl::string_view error_message,
                       MP... message_parts) {
    if (valid_expression) {
      return true;
    }
    SetProgressStatusError(absl::InvalidArgumentError(
        absl::StrCat(error_message, message_parts...)));
    return false;
  }

 private:
  struct ComprehensionStackRecord {
    const cel::ast_internal::Expr* expr;
    const cel::ast_internal::Comprehension* comprehension;
    size_t iter_slot;
    size_t accu_slot;
    size_t slot_count;
    // -1 indicates this shouldn't be used.
    int subexpression;
    bool is_optimizable_list_append;
    bool is_optimizable_bind;
    bool iter_var_in_scope;
    bool accu_var_in_scope;
    bool in_accu_init;
    std::unique_ptr<ComprehensionVisitor> visitor;
  };

  struct BlockInfo {
    // True if we are currently visiting the `cel.@block` node or any of its
    // children.
    bool in = false;
    // Pointer to the `cel.@block` node.
    const cel::ast_internal::Expr* expr = nullptr;
    // Pointer to the `cel.@block` bindings, that is the first argument to the
    // function.
    const cel::ast_internal::Expr* bindings = nullptr;
    // Set of pointers to the elements of `bindings` above.
    absl::flat_hash_set<const cel::ast_internal::Expr*> bindings_set;
    // Pointer to the `cel.@block` bound expression, that is the second argument
    // to the function.
    const cel::ast_internal::Expr* bound = nullptr;
    // The number of entries in the `cel.@block`.
    size_t size = 0;
    // Starting slot index for `cel.@block`. We occupy he slot indices `index`
    // through `index + size + (var_size * 2)`.
    size_t index = 0;
    // The current slot index we are processing, any index references must be
    // less than this to be valid.
    size_t current_index = 0;
    // Pointer to the current `cel.@block` being processed, that is one of the
    // elements within the first argument.
    const cel::ast_internal::Expr* current_binding = nullptr;
    // Mapping between block indices and their subexpressions, fixed size with
    // exactly `size` elements. Unprocessed indices are set to `-1`.
    std::vector<int> subexpressions;
  };

  bool PlanningSuppressed() const {
    return resume_from_suppressed_branch_ != nullptr;
  }

  absl::Status MaybeExtractSubexpression(const cel::ast_internal::Expr* expr,
                                         ComprehensionStackRecord& record) {
    if (!record.is_optimizable_bind) {
      return absl::OkStatus();
    }

    int index = program_builder_.ExtractSubexpression(expr);
    if (index == -1) {
      return absl::InternalError("Failed to extract subexpression");
    }

    record.subexpression = index;

    record.visitor->MarkAccuInitExtracted();

    return absl::OkStatus();
  }

  // Resolve the name of the message type being created and the names of set
  // fields.
  absl::StatusOr<std::pair<std::string, std::vector<std::string>>>
  ResolveCreateStructFields(
      const cel::ast_internal::CreateStruct& create_struct_expr,
      int64_t expr_id) {
    absl::string_view ast_name = create_struct_expr.name();

    absl::optional<std::pair<std::string, cel::Type>> type;
    CEL_ASSIGN_OR_RETURN(type, resolver_.FindType(ast_name, expr_id));

    if (!type.has_value()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Invalid struct creation: missing type info for '", ast_name, "'"));
    }

    std::string resolved_name = std::move(type).value().first;

    std::vector<std::string> fields;
    fields.reserve(create_struct_expr.fields().size());
    for (const auto& entry : create_struct_expr.fields()) {
      if (entry.name().empty()) {
        return absl::InvalidArgumentError("Struct field missing name");
      }
      if (!entry.has_value()) {
        return absl::InvalidArgumentError("Struct field missing value");
      }
      CEL_ASSIGN_OR_RETURN(
          auto field, value_factory().FindStructTypeFieldByName(resolved_name,
                                                                entry.name()));
      if (!field.has_value()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Invalid message creation: field '", entry.name(),
                         "' not found in '", resolved_name, "'"));
      }
      fields.push_back(entry.name());
    }

    return std::make_pair(std::move(resolved_name), std::move(fields));
  }

  const Resolver& resolver_;
  ValueManager& value_factory_;
  absl::Status progress_status_;

  std::stack<
      std::pair<const cel::ast_internal::Expr*, std::unique_ptr<CondVisitor>>>
      cond_visitor_stack_;

  // Tracks SELECT-...SELECT-IDENT chains.
  std::deque<std::pair<const cel::ast_internal::Expr*, std::string>>
      namespace_stack_;

  // When multiple SELECT-...SELECT-IDENT chain is resolved as namespace, this
  // field is used as marker suppressing CelExpression creation for SELECTs.
  const cel::ast_internal::Expr* resolved_select_expr_;

  const cel::RuntimeOptions& options_;

  std::vector<ComprehensionStackRecord> comprehension_stack_;
  absl::flat_hash_set<const cel::ast_internal::Expr*> suppressed_branches_;
  const cel::ast_internal::Expr* resume_from_suppressed_branch_ = nullptr;
  std::vector<std::unique_ptr<ProgramOptimizer>> program_optimizers_;
  IssueCollector& issue_collector_;

  ProgramBuilder& program_builder_;
  PlannerContext extension_context_;
  IndexManager index_manager_;

  bool enable_optional_types_;
  absl::optional<BlockInfo> block_;
};

void BinaryCondVisitor::PreVisit(const cel::ast_internal::Expr* expr) {
  switch (cond_) {
    case BinaryCond::kAnd:
      ABSL_FALLTHROUGH_INTENDED;
    case BinaryCond::kOr:
      visitor_->ValidateOrError(
          !expr->call_expr().has_target() &&
              expr->call_expr().args().size() == 2,
          "Invalid argument count for a binary function call.");
      break;
    case BinaryCond::kOptionalOr:
      ABSL_FALLTHROUGH_INTENDED;
    case BinaryCond::kOptionalOrValue:
      visitor_->ValidateOrError(expr->call_expr().has_target() &&
                                    expr->call_expr().args().size() == 1,
                                "Invalid argument count for or/orValue call.");
      break;
  }
}

void BinaryCondVisitor::PostVisitArg(int arg_num,
                                     const cel::ast_internal::Expr* expr) {
  if (short_circuiting_ && arg_num == 0 &&
      (cond_ == BinaryCond::kAnd || cond_ == BinaryCond::kOr)) {
    // If first branch evaluation result is enough to determine output,
    // jump over the second branch and provide result of the first argument as
    // final output.
    // Retain a pointer to the jump step so we can update the target after
    // planning the second argument.
    absl::StatusOr<std::unique_ptr<JumpStepBase>> jump_step;
    switch (cond_) {
      case BinaryCond::kAnd:
        jump_step = CreateCondJumpStep(false, true, {}, expr->id());
        break;
      case BinaryCond::kOr:
        jump_step = CreateCondJumpStep(true, true, {}, expr->id());
        break;
      default:
        ABSL_UNREACHABLE();
    }
    if (jump_step.ok()) {
      jump_step_ = Jump(visitor_->GetCurrentIndex(), jump_step->get());
    }
    visitor_->AddStep(std::move(jump_step));
  }
}

void BinaryCondVisitor::PostVisitTarget(const cel::ast_internal::Expr* expr) {
  if (short_circuiting_ && (cond_ == BinaryCond::kOptionalOr ||
                            cond_ == BinaryCond::kOptionalOrValue)) {
    // If first branch evaluation result is enough to determine output,
    // jump over the second branch and provide result of the first argument as
    // final output.
    // Retain a pointer to the jump step so we can update the target after
    // planning the second argument.
    absl::StatusOr<std::unique_ptr<JumpStepBase>> jump_step;
    switch (cond_) {
      case BinaryCond::kOptionalOr:
        jump_step = CreateOptionalHasValueJumpStep(false, expr->id());
        break;
      case BinaryCond::kOptionalOrValue:
        jump_step = CreateOptionalHasValueJumpStep(true, expr->id());
        break;
      default:
        ABSL_UNREACHABLE();
    }
    if (jump_step.ok()) {
      jump_step_ = Jump(visitor_->GetCurrentIndex(), jump_step->get());
    }
    visitor_->AddStep(std::move(jump_step));
  }
}

void BinaryCondVisitor::PostVisit(const cel::ast_internal::Expr* expr) {
  switch (cond_) {
    case BinaryCond::kAnd:
      visitor_->AddStep(CreateAndStep(expr->id()));
      break;
    case BinaryCond::kOr:
      visitor_->AddStep(CreateOrStep(expr->id()));
      break;
    case BinaryCond::kOptionalOr:
      visitor_->AddStep(
          CreateOptionalOrStep(/*is_or_value=*/false, expr->id()));
      break;
    case BinaryCond::kOptionalOrValue:
      visitor_->AddStep(CreateOptionalOrStep(/*is_or_value=*/true, expr->id()));
      break;
    default:
      ABSL_UNREACHABLE();
  }
  if (short_circuiting_) {
    // If short-circuiting is enabled, point the conditional jump past the
    // boolean operator step.
    visitor_->SetProgressStatusError(
        jump_step_.set_target(visitor_->GetCurrentIndex()));
  }
}

void TernaryCondVisitor::PreVisit(const cel::ast_internal::Expr* expr) {
  visitor_->ValidateOrError(
      !expr->call_expr().has_target() && expr->call_expr().args().size() == 3,
      "Invalid argument count for a ternary function call.");
}

void TernaryCondVisitor::PostVisitArg(int arg_num,
                                      const cel::ast_internal::Expr* expr) {
  // Ternary operator "_?_:_" requires a special handing.
  // In contrary to regular function call, its execution affects the control
  // flow of the overall CEL expression.
  // If condition value (argument 0) is True, then control flow is unaffected
  // as it is passed to the first conditional branch. Then, at the end of this
  // branch, the jump is performed over the second conditional branch.
  // If condition value is False, then jump is performed and control is passed
  // to the beginning of the second conditional branch.
  // If condition value is Error, then jump is peformed to bypass both
  // conditional branches and provide Error as result of ternary operation.

  // condition argument for ternary operator
  if (arg_num == 0) {
    // Jump in case of error or non-bool
    auto error_jump = CreateBoolCheckJumpStep({}, expr->id());
    if (error_jump.ok()) {
      error_jump_ = Jump(visitor_->GetCurrentIndex(), error_jump->get());
    }
    visitor_->AddStep(std::move(error_jump));

    // Jump to the second branch of execution
    // Value is to be removed from the stack.
    auto jump_to_second = CreateCondJumpStep(false, false, {}, expr->id());
    if (jump_to_second.ok()) {
      jump_to_second_ =
          Jump(visitor_->GetCurrentIndex(), jump_to_second->get());
    }
    visitor_->AddStep(std::move(jump_to_second));
  } else if (arg_num == 1) {
    // Jump after the first and over the second branch of execution.
    // Value is to be removed from the stack.
    auto jump_after_first = CreateJumpStep({}, expr->id());
    if (!jump_after_first.ok()) {
      visitor_->SetProgressStatusError(jump_after_first.status());
    }

    jump_after_first_ =
        Jump(visitor_->GetCurrentIndex(), jump_after_first->get());

    visitor_->AddStep(std::move(jump_after_first));

    if (visitor_->ValidateOrError(
            jump_to_second_.exists(),
            "Error configuring ternary operator: jump_to_second_ is null")) {
      visitor_->SetProgressStatusError(
          jump_to_second_.set_target(visitor_->GetCurrentIndex()));
    }
  }
  // Code executed after traversing the final branch of execution
  // (arg_num == 2) is placed in PostVisitCall, to make this method less
  // clattered.
}

void TernaryCondVisitor::PostVisit(const cel::ast_internal::Expr*) {
  // Determine and set jump offset in jump instruction.
  if (visitor_->ValidateOrError(
          error_jump_.exists(),
          "Error configuring ternary operator: error_jump_ is null")) {
    visitor_->SetProgressStatusError(
        error_jump_.set_target(visitor_->GetCurrentIndex()));
  }
  if (visitor_->ValidateOrError(
          jump_after_first_.exists(),
          "Error configuring ternary operator: jump_after_first_ is null")) {
    visitor_->SetProgressStatusError(
        jump_after_first_.set_target(visitor_->GetCurrentIndex()));
  }
}

void ExhaustiveTernaryCondVisitor::PreVisit(
    const cel::ast_internal::Expr* expr) {
  visitor_->ValidateOrError(
      !expr->call_expr().has_target() && expr->call_expr().args().size() == 3,
      "Invalid argument count for a ternary function call.");
}

void ExhaustiveTernaryCondVisitor::PostVisit(
    const cel::ast_internal::Expr* expr) {
  visitor_->AddStep(CreateTernaryStep(expr->id()));
}

void ComprehensionVisitor::PreVisit(const cel::ast_internal::Expr* expr) {
  if (is_trivial_) {
    visitor_->SuppressBranch(&expr->comprehension_expr().iter_range());
    visitor_->SuppressBranch(&expr->comprehension_expr().loop_condition());
    visitor_->SuppressBranch(&expr->comprehension_expr().loop_step());
  }
}

absl::Status ComprehensionVisitor::PostVisitArgDefault(
    cel::ComprehensionArg arg_num, const cel::ast_internal::Expr* expr) {
  switch (arg_num) {
    case cel::ITER_RANGE: {
      // post process iter_range to list its keys if it's a map
      // and initialize the loop index.
      visitor_->AddStep(CreateComprehensionInitStep(expr->id()));
      break;
    }
    case cel::ACCU_INIT: {
      next_step_pos_ = visitor_->GetCurrentIndex();
      next_step_ =
          new ComprehensionNextStep(iter_slot_, accu_slot_, expr->id());
      visitor_->AddStep(std::unique_ptr<ExpressionStep>(next_step_));
      break;
    }
    case cel::LOOP_CONDITION: {
      cond_step_pos_ = visitor_->GetCurrentIndex();
      cond_step_ = new ComprehensionCondStep(iter_slot_, accu_slot_,
                                             short_circuiting_, expr->id());
      visitor_->AddStep(std::unique_ptr<ExpressionStep>(cond_step_));
      break;
    }
    case cel::LOOP_STEP: {
      auto jump_to_next = CreateJumpStep({}, expr->id());
      Jump jump_helper(visitor_->GetCurrentIndex(), jump_to_next->get());
      visitor_->AddStep(std::move(jump_to_next));
      visitor_->SetProgressStatusError(jump_helper.set_target(next_step_pos_));

      // Set offsets.
      CEL_ASSIGN_OR_RETURN(
          int jump_from_cond,
          Jump::CalculateOffset(cond_step_pos_, visitor_->GetCurrentIndex()));

      cond_step_->set_jump_offset(jump_from_cond);

      CEL_ASSIGN_OR_RETURN(
          int jump_from_next,
          Jump::CalculateOffset(next_step_pos_, visitor_->GetCurrentIndex()));

      next_step_->set_jump_offset(jump_from_next);
      break;
    }
    case cel::RESULT: {
      visitor_->AddStep(CreateComprehensionFinishStep(accu_slot_, expr->id()));

      CEL_ASSIGN_OR_RETURN(
          int jump_from_next,
          Jump::CalculateOffset(next_step_pos_, visitor_->GetCurrentIndex()));
      next_step_->set_error_jump_offset(jump_from_next);

      CEL_ASSIGN_OR_RETURN(
          int jump_from_cond,
          Jump::CalculateOffset(cond_step_pos_, visitor_->GetCurrentIndex()));
      cond_step_->set_error_jump_offset(jump_from_cond);
      break;
    }
  }
  return absl::OkStatus();
}

void ComprehensionVisitor::PostVisitArgTrivial(
    cel::ComprehensionArg arg_num, const cel::ast_internal::Expr* expr) {
  switch (arg_num) {
    case cel::ITER_RANGE: {
      break;
    }
    case cel::ACCU_INIT: {
      if (!accu_init_extracted_) {
        visitor_->AddStep(CreateAssignSlotAndPopStep(accu_slot_));
      }
      break;
    }
    case cel::LOOP_CONDITION: {
      break;
    }
    case cel::LOOP_STEP: {
      break;
    }
    case cel::RESULT: {
      visitor_->AddStep(CreateClearSlotStep(accu_slot_, expr->id()));
      break;
    }
  }
}

void ComprehensionVisitor::PostVisit(const cel::ast_internal::Expr* expr) {
  if (is_trivial_) {
    visitor_->MaybeMakeBindRecursive(expr, &expr->comprehension_expr(),
                                     accu_slot_);
    return;
  }
  visitor_->MaybeMakeComprehensionRecursive(expr, &expr->comprehension_expr(),
                                            iter_slot_, accu_slot_);
}

// Flattens the expression table into the end of the mainline expression vector
// and returns an index to the individual sub expressions.
std::vector<ExecutionPathView> FlattenExpressionTable(
    ProgramBuilder& program_builder, ExecutionPath& main) {
  std::vector<std::pair<size_t, size_t>> ranges;
  main = program_builder.FlattenMain();
  ranges.push_back(std::make_pair(0, main.size()));

  std::vector<ExecutionPath> subexpressions =
      program_builder.FlattenSubexpressions();
  for (auto& subexpression : subexpressions) {
    ranges.push_back(std::make_pair(main.size(), subexpression.size()));
    absl::c_move(subexpression, std::back_inserter(main));
  }

  std::vector<ExecutionPathView> subexpression_indexes;
  subexpression_indexes.reserve(ranges.size());
  for (const auto& range : ranges) {
    subexpression_indexes.push_back(
        absl::MakeSpan(main).subspan(range.first, range.second));
  }
  return subexpression_indexes;
}

}  // namespace

absl::StatusOr<FlatExpression> FlatExprBuilder::CreateExpressionImpl(
    std::unique_ptr<Ast> ast, std::vector<RuntimeIssue>* issues) const {
  // These objects are expected to remain scoped to one build call -- references
  // to them shouldn't be persisted in any part of the result expression.
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry_.GetComposedTypeProvider());

  RuntimeIssue::Severity max_severity = options_.fail_on_warnings
                                            ? RuntimeIssue::Severity::kWarning
                                            : RuntimeIssue::Severity::kError;
  IssueCollector issue_collector(max_severity);
  Resolver resolver(container_, function_registry_, type_registry_,
                    value_factory, type_registry_.resolveable_enums(),
                    options_.enable_qualified_type_identifiers);

  ProgramBuilder program_builder;
  PlannerContext extension_context(resolver, options_, value_factory,
                                   issue_collector, program_builder);

  auto& ast_impl = AstImpl::CastFromPublicAst(*ast);

  if (absl::StartsWith(container_, ".") || absl::EndsWith(container_, ".")) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid expression container: '", container_, "'"));
  }

  for (const std::unique_ptr<AstTransform>& transform : ast_transforms_) {
    CEL_RETURN_IF_ERROR(transform->UpdateAst(extension_context, ast_impl));
  }

  std::vector<std::unique_ptr<ProgramOptimizer>> optimizers;
  for (const ProgramOptimizerFactory& optimizer_factory : program_optimizers_) {
    CEL_ASSIGN_OR_RETURN(auto optimizer,
                         optimizer_factory(extension_context, ast_impl));
    if (optimizer != nullptr) {
      optimizers.push_back(std::move(optimizer));
    }
  }

  FlatExprVisitor visitor(resolver, options_, std::move(optimizers),
                          ast_impl.reference_map(), value_factory,
                          issue_collector, program_builder, extension_context,
                          enable_optional_types_);

  cel::TraversalOptions opts;
  opts.use_comprehension_callbacks = true;
  AstTraverse(ast_impl.root_expr(), visitor, opts);

  if (!visitor.progress_status().ok()) {
    return visitor.progress_status();
  }

  if (issues != nullptr) {
    (*issues) = issue_collector.ExtractIssues();
  }

  ExecutionPath execution_path;
  std::vector<ExecutionPathView> subexpressions =
      FlattenExpressionTable(program_builder, execution_path);

  return FlatExpression(std::move(execution_path), std::move(subexpressions),
                        visitor.slot_count(),
                        type_registry_.GetComposedTypeProvider(), options_);
}

}  // namespace google::api::expr::runtime
