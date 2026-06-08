// Copyright 2017 Google LLC
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

#include "eval/eval/create_struct_step.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::Cast;
using ::cel::ErrorValue;
using ::cel::InstanceOf;
using ::cel::StructValueBuilderInterface;
using ::cel::UnknownValue;
using ::cel::Value;

// `CreateStruct` implementation for message/struct.
class CreateStructStepForStruct final : public ExpressionStepBase {
 public:
  CreateStructStepForStruct(int64_t expr_id, std::string name,
                            std::vector<std::string> entries,
                            absl::flat_hash_set<int32_t> optional_indices)
      : ExpressionStepBase(expr_id),
        name_(std::move(name)),
        entries_(std::move(entries)),
        optional_indices_(std::move(optional_indices)) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::StatusOr<Value> DoEvaluate(ExecutionFrame* frame) const;

  std::string name_;
  std::vector<std::string> entries_;
  absl::flat_hash_set<int32_t> optional_indices_;
};

absl::StatusOr<Value> CreateStructStepForStruct::DoEvaluate(
    ExecutionFrame* frame) const {
  int entries_size = entries_.size();

  auto args = frame->value_stack().GetSpan(entries_size);

  for (const auto& arg : args) {
    if (arg.IsError()) {
      return arg;
    }
  }

  if (frame->enable_unknowns()) {
    absl::optional<UnknownValue> unknown_set =
        frame->attribute_utility().IdentifyAndMergeUnknowns(
            args, frame->value_stack().GetAttributeSpan(entries_size),
            /*use_partial=*/true);
    if (unknown_set.has_value()) {
      return *unknown_set;
    }
  }

  CEL_ASSIGN_OR_RETURN(auto builder,
                       frame->type_provider().NewValueBuilder(
                           name_, frame->message_factory(), frame->arena()));
  if (builder == nullptr) {
    return ErrorValue(
        absl::NotFoundError(absl::StrCat("Unable to find builder: ", name_)));
  }

  for (int i = 0; i < entries_size; ++i) {
    const auto& entry = entries_[i];
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
          return optional_arg_value;
        }
        CEL_ASSIGN_OR_RETURN(
            absl::optional<ErrorValue> error_value,
            builder->SetFieldByName(entry, std::move(optional_arg_value)));
        if (error_value) {
          return std::move(*error_value);
        }
      } else {
        return cel::TypeConversionError(arg.DebugString(), "optional_type");
      }
    } else {
      CEL_ASSIGN_OR_RETURN(absl::optional<ErrorValue> error_value,
                           builder->SetFieldByName(entry, arg));
      if (error_value) {
        return std::move(*error_value);
      }
    }
  }

  return std::move(*builder).Build();
}

absl::Status CreateStructStepForStruct::Evaluate(ExecutionFrame* frame) const {
  if (frame->value_stack().size() < entries_.size()) {
    return absl::InternalError("CreateStructStepForStruct: stack underflow");
  }
  CEL_ASSIGN_OR_RETURN(Value result, DoEvaluate(frame));
  frame->value_stack().PopAndPush(entries_.size(), std::move(result));

  return absl::OkStatus();
}

class DirectCreateStructStep : public DirectExpressionStep {
 public:
  DirectCreateStructStep(
      int64_t expr_id, std::string name, std::vector<std::string> field_keys,
      std::vector<std::unique_ptr<DirectExpressionStep>> deps,
      absl::flat_hash_set<int32_t> optional_indices)
      : DirectExpressionStep(expr_id),
        name_(std::move(name)),
        field_keys_(std::move(field_keys)),
        deps_(std::move(deps)),
        optional_indices_(std::move(optional_indices)) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& trail) const override;

 private:
  std::string name_;
  std::vector<std::string> field_keys_;
  std::vector<std::unique_ptr<DirectExpressionStep>> deps_;
  absl::flat_hash_set<int32_t> optional_indices_;
};

absl::Status DirectCreateStructStep::Evaluate(ExecutionFrameBase& frame,
                                              Value& result,
                                              AttributeTrail& trail) const {
  Value field_value;
  AttributeTrail field_attr;
  auto unknowns = frame.attribute_utility().CreateAccumulator();

  CEL_ASSIGN_OR_RETURN(auto builder,
                       frame.type_provider().NewValueBuilder(
                           name_, frame.message_factory(), frame.arena()));
  if (builder == nullptr) {
    result = cel::ErrorValue(
        absl::NotFoundError(absl::StrCat("Unable to find builder: ", name_)));
    return absl::OkStatus();
  }

  for (int i = 0; i < field_keys_.size(); i++) {
    CEL_RETURN_IF_ERROR(deps_[i]->Evaluate(frame, field_value, field_attr));

    // TODO(uncreated-issue/67): if the value is an error, we should be able to return
    // early, however some client tests depend on the error message the struct
    // impl returns in the stack machine version.
    if (field_value.IsError()) {
      result = std::move(field_value);
      return absl::OkStatus();
    }

    if (frame.unknown_processing_enabled()) {
      if (field_value.IsUnknown()) {
        unknowns.Add(field_value.GetUnknown());
      } else if (frame.attribute_utility().CheckForUnknownPartial(field_attr)) {
        unknowns.Add(field_attr);
      }
    }

    if (!unknowns.IsEmpty()) {
      continue;
    }

    if (optional_indices_.contains(static_cast<int32_t>(i))) {
      if (auto optional_arg = field_value.AsOptional(); optional_arg) {
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
        CEL_ASSIGN_OR_RETURN(
            absl::optional<ErrorValue> error_value,
            builder->SetFieldByName(field_keys_[i],
                                    std::move(optional_arg_value)));
        if (error_value) {
          result = std::move(*error_value);
          return absl::OkStatus();
        }
        continue;
      } else {
        result = cel::TypeConversionError(field_value.DebugString(),
                                          "optional_type");
        return absl::OkStatus();
      }
    }

    CEL_ASSIGN_OR_RETURN(
        absl::optional<ErrorValue> error_value,
        builder->SetFieldByName(field_keys_[i], std::move(field_value)));
    if (error_value) {
      result = std::move(*error_value);
      return absl::OkStatus();
    }
  }

  if (!unknowns.IsEmpty()) {
    result = std::move(unknowns).Build();
    return absl::OkStatus();
  }

  CEL_ASSIGN_OR_RETURN(result, std::move(*builder).Build());
  return absl::OkStatus();
}

}  // namespace

std::unique_ptr<DirectExpressionStep> CreateDirectCreateStructStep(
    std::string resolved_name, std::vector<std::string> field_keys,
    std::vector<std::unique_ptr<DirectExpressionStep>> deps,
    absl::flat_hash_set<int32_t> optional_indices, int64_t expr_id) {
  return std::make_unique<DirectCreateStructStep>(
      expr_id, std::move(resolved_name), std::move(field_keys), std::move(deps),
      std::move(optional_indices));
}

std::unique_ptr<ExpressionStep> CreateCreateStructStep(
    std::string name, std::vector<std::string> field_keys,
    absl::flat_hash_set<int32_t> optional_indices, int64_t expr_id) {
  // MakeOptionalIndicesSet(create_struct_expr)
  return std::make_unique<CreateStructStepForStruct>(
      expr_id, std::move(name), std::move(field_keys),
      std::move(optional_indices));
}
}  // namespace google::api::expr::runtime
