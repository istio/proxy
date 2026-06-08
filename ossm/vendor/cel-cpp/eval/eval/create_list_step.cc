#include "eval/eval/create_list_step.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/expr.h"
#include "common/value.h"
#include "common/values/list_value_builder.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/attribute_utility.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::Cast;
using ::cel::ErrorValue;
using ::cel::InstanceOf;
using ::cel::ListValueBuilderPtr;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::common_internal::NewListValueBuilder;

class CreateListStep : public ExpressionStepBase {
 public:
  CreateListStep(int64_t expr_id, int list_size,
                 absl::flat_hash_set<int> optional_indices)
      : ExpressionStepBase(expr_id),
        list_size_(list_size),
        optional_indices_(std::move(optional_indices)) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::Status DoEvaluate(ExecutionFrame* frame, Value* result) const;

  int list_size_;
  absl::flat_hash_set<int32_t> optional_indices_;
};

absl::Status CreateListStep::Evaluate(ExecutionFrame* frame) const {
  if (list_size_ < 0) {
    return absl::Status(absl::StatusCode::kInternal,
                        "CreateListStep: list size is <0");
  }

  if (!frame->value_stack().HasEnough(list_size_)) {
    return absl::Status(absl::StatusCode::kInternal,
                        "CreateListStep: stack underflow");
  }

  Value result;
  CEL_RETURN_IF_ERROR(DoEvaluate(frame, &result));

  frame->value_stack().PopAndPush(list_size_, std::move(result));
  return absl::OkStatus();
}

absl::Status CreateListStep::DoEvaluate(ExecutionFrame* frame,
                                        Value* result) const {
  auto args = frame->value_stack().GetSpan(list_size_);

  for (const auto& arg : args) {
    if (arg.IsError()) {
      *result = arg;
      return absl::OkStatus();
    }
  }

  if (frame->enable_unknowns()) {
    absl::optional<UnknownValue> unknown_set =
        frame->attribute_utility().IdentifyAndMergeUnknowns(
            args, frame->value_stack().GetAttributeSpan(list_size_),
            /*use_partial=*/true);
    if (unknown_set.has_value()) {
      *result = std::move(*unknown_set);
      return absl::OkStatus();
    }
  }

  ListValueBuilderPtr builder = NewListValueBuilder(frame->arena());
  builder->Reserve(args.size());

  for (size_t i = 0; i < args.size(); ++i) {
    const auto& arg = args[i];
    if (optional_indices_.contains(static_cast<int32_t>(i))) {
      if (auto optional_arg = arg.AsOptional(); optional_arg) {
        if (!optional_arg->HasValue()) {
          continue;
        }
        Value optional_arg_value;
        optional_arg->Value(&optional_arg_value);
        if (optional_arg_value.IsError()) {
          // Error should never be in optional, but better safe than sorry.
          *result = std::move(optional_arg_value);
          return absl::OkStatus();
        }
        CEL_RETURN_IF_ERROR(builder->Add(std::move(optional_arg_value)));
      } else {
        *result = cel::TypeConversionError(arg.GetTypeName(), "optional_type");
        return absl::OkStatus();
      }
    } else {
      CEL_RETURN_IF_ERROR(builder->Add(arg));
    }
  }

  *result = std::move(*builder).Build();
  return absl::OkStatus();
}

absl::flat_hash_set<int32_t> MakeOptionalIndicesSet(
    const cel::ListExpr& create_list_expr) {
  absl::flat_hash_set<int32_t> optional_indices;
  for (size_t i = 0; i < create_list_expr.elements().size(); ++i) {
    if (create_list_expr.elements()[i].optional()) {
      optional_indices.insert(static_cast<int32_t>(i));
    }
  }
  return optional_indices;
}

class CreateListDirectStep : public DirectExpressionStep {
 public:
  CreateListDirectStep(
      std::vector<std::unique_ptr<DirectExpressionStep>> elements,
      absl::flat_hash_set<int32_t> optional_indices, int64_t expr_id)
      : DirectExpressionStep(expr_id),
        elements_(std::move(elements)),
        optional_indices_(std::move(optional_indices)) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute_trail) const override {
    ListValueBuilderPtr builder = NewListValueBuilder(frame.arena());
    builder->Reserve(elements_.size());

    AttributeUtility::Accumulator unknowns =
        frame.attribute_utility().CreateAccumulator();
    AttributeTrail tmp_attr;

    for (size_t i = 0; i < elements_.size(); ++i) {
      const auto& element = elements_[i];
      CEL_RETURN_IF_ERROR(element->Evaluate(frame, result, tmp_attr));

      if (result.IsError()) {
        return absl::OkStatus();
      }

      if (frame.attribute_tracking_enabled()) {
        if (frame.missing_attribute_errors_enabled()) {
          if (frame.attribute_utility().CheckForMissingAttribute(tmp_attr)) {
            CEL_ASSIGN_OR_RETURN(
                result, frame.attribute_utility().CreateMissingAttributeError(
                            tmp_attr.attribute()));
            return absl::OkStatus();
          }
        }
        if (frame.unknown_processing_enabled()) {
          if (result.IsUnknown()) {
            unknowns.Add(result.GetUnknown());
          }
          if (frame.attribute_utility().CheckForUnknown(tmp_attr,
                                                        /*use_partial=*/true)) {
            unknowns.Add(tmp_attr);
          }
        }
      }

      if (!unknowns.IsEmpty()) {
        // We found an unknown, there is no point in attempting to create a
        // list. Instead iterate through the remaining elements and look for
        // more unknowns.
        continue;
      }

      // Conditionally add if optional.
      if (optional_indices_.contains(static_cast<int32_t>(i))) {
        if (auto optional_arg = result.AsOptional(); optional_arg) {
          if (!optional_arg->HasValue()) {
            continue;
          }
          Value optional_arg_value;
          optional_arg->Value(&optional_arg_value);
          if (optional_arg_value.IsError()) {
            // Error should never be in optional, but better safe than sorry.
            result = std::move(optional_arg_value);
            return absl::OkStatus();
          }
          CEL_RETURN_IF_ERROR(builder->Add(std::move(optional_arg_value)));
          continue;
        }
        result =
            cel::TypeConversionError(result.GetTypeName(), "optional_type");
        return absl::OkStatus();
      }

      // Otherwise just add.
      CEL_RETURN_IF_ERROR(builder->Add(std::move(result)));
    }

    if (!unknowns.IsEmpty()) {
      result = std::move(unknowns).Build();
      return absl::OkStatus();
    }
    result = std::move(*builder).Build();

    return absl::OkStatus();
  }

 private:
  std::vector<std::unique_ptr<DirectExpressionStep>> elements_;
  absl::flat_hash_set<int32_t> optional_indices_;
};

class MutableListStep : public ExpressionStepBase {
 public:
  explicit MutableListStep(int64_t expr_id) : ExpressionStepBase(expr_id) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;
};

absl::Status MutableListStep::Evaluate(ExecutionFrame* frame) const {
  frame->value_stack().Push(cel::CustomListValue(
      cel::common_internal::NewMutableListValue(frame->arena()),
      frame->arena()));
  return absl::OkStatus();
}

class DirectMutableListStep : public DirectExpressionStep {
 public:
  explicit DirectMutableListStep(int64_t expr_id)
      : DirectExpressionStep(expr_id) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute) const override;
};

absl::Status DirectMutableListStep::Evaluate(
    ExecutionFrameBase& frame, Value& result,
    AttributeTrail& attribute_trail) const {
  result = cel::CustomListValue(
      cel::common_internal::NewMutableListValue(frame.arena()), frame.arena());
  return absl::OkStatus();
}

}  // namespace

std::unique_ptr<DirectExpressionStep> CreateDirectListStep(
    std::vector<std::unique_ptr<DirectExpressionStep>> deps,
    absl::flat_hash_set<int32_t> optional_indices, int64_t expr_id) {
  return std::make_unique<CreateListDirectStep>(
      std::move(deps), std::move(optional_indices), expr_id);
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateListStep(
    const cel::ListExpr& create_list_expr, int64_t expr_id) {
  return std::make_unique<CreateListStep>(
      expr_id, create_list_expr.elements().size(),
      MakeOptionalIndicesSet(create_list_expr));
}

std::unique_ptr<ExpressionStep> CreateMutableListStep(int64_t expr_id) {
  return std::make_unique<MutableListStep>(expr_id);
}

std::unique_ptr<DirectExpressionStep> CreateDirectMutableListStep(
    int64_t expr_id) {
  return std::make_unique<DirectMutableListStep>(expr_id);
}

}  // namespace google::api::expr::runtime
