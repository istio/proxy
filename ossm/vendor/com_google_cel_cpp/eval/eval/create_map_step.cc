// Copyright 2024 Google LLC
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

#include "eval/eval/create_map_step.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/value.h"
#include "common/values/map_value_builder.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::Cast;
using ::cel::ErrorValue;
using ::cel::ErrorValueAssign;
using ::cel::ErrorValueReturn;
using ::cel::InstanceOf;
using ::cel::MapValueBuilderPtr;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::common_internal::NewMapValueBuilder;
using ::cel::common_internal::NewMutableMapValue;

// `CreateStruct` implementation for map.
class CreateStructStepForMap final : public ExpressionStepBase {
 public:
  CreateStructStepForMap(int64_t expr_id, size_t entry_count,
                         absl::flat_hash_set<int32_t> optional_indices)
      : ExpressionStepBase(expr_id),
        entry_count_(entry_count),
        optional_indices_(std::move(optional_indices)) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::StatusOr<Value> DoEvaluate(ExecutionFrame* frame) const;

  size_t entry_count_;
  absl::flat_hash_set<int32_t> optional_indices_;
};

absl::StatusOr<Value> CreateStructStepForMap::DoEvaluate(
    ExecutionFrame* frame) const {
  auto args = frame->value_stack().GetSpan(2 * entry_count_);

  for (const auto& arg : args) {
    if (arg.IsError()) {
      return arg;
    }
  }

  if (frame->enable_unknowns()) {
    absl::optional<UnknownValue> unknown_set =
        frame->attribute_utility().IdentifyAndMergeUnknowns(
            args, frame->value_stack().GetAttributeSpan(args.size()), true);
    if (unknown_set.has_value()) {
      return *unknown_set;
    }
  }

  MapValueBuilderPtr builder = NewMapValueBuilder(frame->arena());
  builder->Reserve(entry_count_);

  for (size_t i = 0; i < entry_count_; i += 1) {
    const auto& map_key = args[2 * i];
    CEL_RETURN_IF_ERROR(cel::CheckMapKey(map_key)).With(ErrorValueReturn());
    const auto& map_value = args[(2 * i) + 1];
    if (optional_indices_.contains(static_cast<int32_t>(i))) {
      if (auto optional_map_value = map_value.AsOptional();
          optional_map_value) {
        if (!optional_map_value->HasValue()) {
          continue;
        }
        Value optional_map_value_value;
        optional_map_value->Value(&optional_map_value_value);
        if (optional_map_value_value.IsError()) {
          // Error should never be in optional, but better safe than sorry.
          return optional_map_value_value;
        }
        CEL_RETURN_IF_ERROR(
            builder->Put(map_key, std::move(optional_map_value_value)));
      } else {
        return cel::TypeConversionError(map_value.DebugString(),
                                        "optional_type");
      }
    } else {
      CEL_RETURN_IF_ERROR(builder->Put(map_key, map_value));
    }
  }

  return std::move(*builder).Build();
}

absl::Status CreateStructStepForMap::Evaluate(ExecutionFrame* frame) const {
  if (frame->value_stack().size() < 2 * entry_count_) {
    return absl::InternalError("CreateStructStepForMap: stack underflow");
  }

  CEL_ASSIGN_OR_RETURN(auto result, DoEvaluate(frame));

  frame->value_stack().PopAndPush(2 * entry_count_, std::move(result));

  return absl::OkStatus();
}

class DirectCreateMapStep : public DirectExpressionStep {
 public:
  DirectCreateMapStep(std::vector<std::unique_ptr<DirectExpressionStep>> deps,
                      absl::flat_hash_set<int32_t> optional_indices,
                      int64_t expr_id)
      : DirectExpressionStep(expr_id),
        deps_(std::move(deps)),
        optional_indices_(std::move(optional_indices)),
        entry_count_(deps_.size() / 2) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute_trail) const override;

 private:
  std::vector<std::unique_ptr<DirectExpressionStep>> deps_;
  absl::flat_hash_set<int32_t> optional_indices_;
  size_t entry_count_;
};

absl::Status DirectCreateMapStep::Evaluate(
    ExecutionFrameBase& frame, Value& result,
    AttributeTrail& attribute_trail) const {
  auto unknowns = frame.attribute_utility().CreateAccumulator();

  MapValueBuilderPtr builder = NewMapValueBuilder(frame.arena());
  builder->Reserve(entry_count_);

  for (size_t i = 0; i < entry_count_; i += 1) {
    Value key;
    Value value;
    AttributeTrail tmp_attr;
    int map_key_index = 2 * i;
    int map_value_index = map_key_index + 1;
    CEL_RETURN_IF_ERROR(deps_[map_key_index]->Evaluate(frame, key, tmp_attr));

    if (key.IsError()) {
      result = std::move(key);
      return absl::OkStatus();
    }

    if (frame.unknown_processing_enabled()) {
      if (key.IsUnknown()) {
        unknowns.Add(key.GetUnknown());
      } else if (frame.attribute_utility().CheckForUnknownPartial(tmp_attr)) {
        unknowns.Add(tmp_attr);
      }
    }

    CEL_RETURN_IF_ERROR(cel::CheckMapKey(key)).With(ErrorValueAssign(result));

    CEL_RETURN_IF_ERROR(
        deps_[map_value_index]->Evaluate(frame, value, tmp_attr));

    if (value.IsError()) {
      result = std::move(value);
      return absl::OkStatus();
    }

    if (frame.unknown_processing_enabled()) {
      if (value.IsUnknown()) {
        unknowns.Add(value.GetUnknown());
      } else if (frame.attribute_utility().CheckForUnknownPartial(tmp_attr)) {
        unknowns.Add(tmp_attr);
      }
    }

    // Preserve the stack machine behavior of forwarding unknowns before
    // errors.
    if (!unknowns.IsEmpty()) {
      continue;
    }

    if (optional_indices_.contains(static_cast<int32_t>(i))) {
      if (auto optional_map_value = value.AsOptional(); optional_map_value) {
        if (!optional_map_value->HasValue()) {
          continue;
        }
        Value optional_map_value_value;
        optional_map_value->Value(&optional_map_value_value);
        if (optional_map_value_value.IsError()) {
          // Error should never be in optional, but better safe than sorry.
          result = optional_map_value_value;
          return absl::OkStatus();
        }
        CEL_RETURN_IF_ERROR(
            builder->Put(std::move(key), std::move(optional_map_value_value)));
        continue;
      }
      result = cel::TypeConversionError(value.DebugString(), "optional_type");
      return absl::OkStatus();
    }

    CEL_RETURN_IF_ERROR(builder->Put(std::move(key), std::move(value)));
  }

  if (!unknowns.IsEmpty()) {
    result = std::move(unknowns).Build();
    return absl::OkStatus();
  }

  result = std::move(*builder).Build();
  return absl::OkStatus();
}

class MutableMapStep final : public ExpressionStep {
 public:
  explicit MutableMapStep(int64_t expr_id) : ExpressionStep(expr_id) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    frame->value_stack().Push(cel::CustomMapValue(
        NewMutableMapValue(frame->arena()), frame->arena()));
    return absl::OkStatus();
  }
};

class DirectMutableMapStep final : public DirectExpressionStep {
 public:
  explicit DirectMutableMapStep(int64_t expr_id)
      : DirectExpressionStep(expr_id) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute) const override {
    result =
        cel::CustomMapValue(NewMutableMapValue(frame.arena()), frame.arena());
    return absl::OkStatus();
  }
};

}  // namespace

std::unique_ptr<DirectExpressionStep> CreateDirectCreateMapStep(
    std::vector<std::unique_ptr<DirectExpressionStep>> deps,
    absl::flat_hash_set<int32_t> optional_indices, int64_t expr_id) {
  return std::make_unique<DirectCreateMapStep>(
      std::move(deps), std::move(optional_indices), expr_id);
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateStructStepForMap(
    size_t entry_count, absl::flat_hash_set<int32_t> optional_indices,
    int64_t expr_id) {
  // Make map-creating step.
  return std::make_unique<CreateStructStepForMap>(expr_id, entry_count,
                                                  std::move(optional_indices));
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateMutableMapStep(
    int64_t expr_id) {
  return std::make_unique<MutableMapStep>(expr_id);
}

std::unique_ptr<DirectExpressionStep> CreateDirectMutableMapStep(
    int64_t expr_id) {
  return std::make_unique<DirectMutableMapStep>(expr_id);
}

}  // namespace google::api::expr::runtime
