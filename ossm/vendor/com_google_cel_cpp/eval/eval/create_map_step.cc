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
#include "common/type.h"
#include "common/value.h"
#include "common/value_manager.h"
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

  if (frame->enable_unknowns()) {
    absl::optional<UnknownValue> unknown_set =
        frame->attribute_utility().IdentifyAndMergeUnknowns(
            args, frame->value_stack().GetAttributeSpan(args.size()), true);
    if (unknown_set.has_value()) {
      return *unknown_set;
    }
  }

  CEL_ASSIGN_OR_RETURN(
      auto builder, frame->value_manager().NewMapValueBuilder(cel::MapType{}));
  builder->Reserve(entry_count_);

  for (size_t i = 0; i < entry_count_; i += 1) {
    auto& map_key = args[2 * i];
    CEL_RETURN_IF_ERROR(cel::CheckMapKey(map_key));
    auto& map_value = args[(2 * i) + 1];
    if (optional_indices_.contains(static_cast<int32_t>(i))) {
      if (auto optional_map_value = cel::As<cel::OptionalValue>(map_value);
          optional_map_value) {
        if (!optional_map_value->HasValue()) {
          continue;
        }
        auto key_status =
            builder->Put(std::move(map_key), optional_map_value->Value());
        if (!key_status.ok()) {
          return frame->value_factory().CreateErrorValue(key_status);
        }
      } else {
        return cel::TypeConversionError(map_value.DebugString(),
                                        "optional_type")
            .NativeValue();
      }
    } else {
      auto key_status = builder->Put(std::move(map_key), std::move(map_value));
      if (!key_status.ok()) {
        return frame->value_factory().CreateErrorValue(key_status);
      }
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
  Value key;
  Value value;
  AttributeTrail tmp_attr;
  auto unknowns = frame.attribute_utility().CreateAccumulator();

  CEL_ASSIGN_OR_RETURN(
      auto builder, frame.value_manager().NewMapValueBuilder(cel::MapType()));
  builder->Reserve(entry_count_);

  for (size_t i = 0; i < entry_count_; i += 1) {
    int map_key_index = 2 * i;
    int map_value_index = map_key_index + 1;
    CEL_RETURN_IF_ERROR(deps_[map_key_index]->Evaluate(frame, key, tmp_attr));

    if (InstanceOf<ErrorValue>(key)) {
      result = key;
      return absl::OkStatus();
    }

    if (frame.unknown_processing_enabled()) {
      if (InstanceOf<UnknownValue>(key)) {
        unknowns.Add(Cast<UnknownValue>(key));
      } else if (frame.attribute_utility().CheckForUnknownPartial(tmp_attr)) {
        unknowns.Add(tmp_attr);
      }
    }

    CEL_RETURN_IF_ERROR(
        deps_[map_value_index]->Evaluate(frame, value, tmp_attr));

    if (InstanceOf<ErrorValue>(value)) {
      result = value;
      return absl::OkStatus();
    }

    if (frame.unknown_processing_enabled()) {
      if (InstanceOf<UnknownValue>(value)) {
        unknowns.Add(Cast<UnknownValue>(value));
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
      if (auto optional_map_value =
              cel::As<cel::OptionalValue>(static_cast<const Value&>(value));
          optional_map_value) {
        if (!optional_map_value->HasValue()) {
          continue;
        }
        auto key_status =
            builder->Put(std::move(key), optional_map_value->Value());
        if (!key_status.ok()) {
          result = frame.value_manager().CreateErrorValue(key_status);
          return absl::OkStatus();
        }
        continue;
      }
      return cel::TypeConversionError(value.DebugString(), "optional_type")
          .NativeValue();
    }

    CEL_RETURN_IF_ERROR(cel::CheckMapKey(key));
    auto put_status = builder->Put(std::move(key), std::move(value));
    if (!put_status.ok()) {
      result = frame.value_manager().CreateErrorValue(put_status);
      return absl::OkStatus();
    }
  }

  if (!unknowns.IsEmpty()) {
    result = std::move(unknowns).Build();
    return absl::OkStatus();
  }

  result = std::move(*builder).Build();
  return absl::OkStatus();
}

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

}  // namespace google::api::expr::runtime
