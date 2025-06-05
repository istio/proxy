// Copyright 2023 Google LLC
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

#include "eval/eval/cel_expression_flat_impl.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/comprehension_slots.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/internal/adapter_activation_impl.h"
#include "eval/internal/interop.h"
#include "eval/public/base_activation.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_value.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/casts.h"
#include "internal/status_macros.h"
#include "runtime/managed_value_factory.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {
namespace {

using ::cel::Value;
using ::cel::ValueManager;
using ::cel::extensions::ProtoMemoryManagerArena;
using ::cel::extensions::ProtoMemoryManagerRef;

EvaluationListener AdaptListener(const CelEvaluationListener& listener) {
  if (!listener) return nullptr;
  return [&](int64_t expr_id, const Value& value,
             ValueManager& factory) -> absl::Status {
    if (value->Is<cel::OpaqueValue>()) {
      // Opaque types are used to implement some optimized operations.
      // These aren't representable as legacy values and shouldn't be
      // inspectable by clients.
      return absl::OkStatus();
    }
    google::protobuf::Arena* arena = ProtoMemoryManagerArena(factory.GetMemoryManager());
    CelValue legacy_value =
        cel::interop_internal::ModernValueToLegacyValueOrDie(arena, value);
    return listener(expr_id, legacy_value, arena);
  };
}
}  // namespace

CelExpressionFlatEvaluationState::CelExpressionFlatEvaluationState(
    google::protobuf::Arena* arena, const FlatExpression& expression)
    : arena_(arena),
      state_(expression.MakeEvaluatorState(ProtoMemoryManagerRef(arena_))) {}

absl::StatusOr<CelValue> CelExpressionFlatImpl::Trace(
    const BaseActivation& activation, CelEvaluationState* _state,
    CelEvaluationListener callback) const {
  auto state =
      ::cel::internal::down_cast<CelExpressionFlatEvaluationState*>(_state);
  state->state().Reset();
  cel::interop_internal::AdapterActivationImpl modern_activation(activation);

  CEL_ASSIGN_OR_RETURN(
      cel::Value value,
      flat_expression_.EvaluateWithCallback(
          modern_activation, AdaptListener(callback), state->state()));

  return cel::interop_internal::ModernValueToLegacyValueOrDie(state->arena(),
                                                              value);
}

std::unique_ptr<CelEvaluationState> CelExpressionFlatImpl::InitializeState(
    google::protobuf::Arena* arena) const {
  return std::make_unique<CelExpressionFlatEvaluationState>(arena,
                                                            flat_expression_);
}

absl::StatusOr<CelValue> CelExpressionFlatImpl::Evaluate(
    const BaseActivation& activation, CelEvaluationState* state) const {
  return Trace(activation, state, CelEvaluationListener());
}

absl::StatusOr<std::unique_ptr<CelExpressionRecursiveImpl>>
CelExpressionRecursiveImpl::Create(FlatExpression flat_expr) {
  if (flat_expr.path().empty() ||
      flat_expr.path().front()->GetNativeTypeId() !=
          cel::NativeTypeId::For<WrappedDirectStep>()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Expected a recursive program step", flat_expr.path().size()));
  }

  auto* instance = new CelExpressionRecursiveImpl(std::move(flat_expr));

  return absl::WrapUnique(instance);
}

absl::StatusOr<CelValue> CelExpressionRecursiveImpl::Trace(
    const BaseActivation& activation, google::protobuf::Arena* arena,
    CelEvaluationListener callback) const {
  cel::interop_internal::AdapterActivationImpl modern_activation(activation);
  cel::ManagedValueFactory factory = flat_expression_.MakeValueFactory(
      cel::extensions::ProtoMemoryManagerRef(arena));

  ComprehensionSlots slots(flat_expression_.comprehension_slots_size());
  ExecutionFrameBase execution_frame(modern_activation, AdaptListener(callback),
                                     flat_expression_.options(), factory.get(),
                                     slots);

  cel::Value result;
  AttributeTrail trail;
  CEL_RETURN_IF_ERROR(root_->Evaluate(execution_frame, result, trail));

  return cel::interop_internal::ModernValueToLegacyValueOrDie(arena, result);
}

absl::StatusOr<CelValue> CelExpressionRecursiveImpl::Evaluate(
    const BaseActivation& activation, google::protobuf::Arena* arena) const {
  return Trace(activation, arena, /*callback=*/nullptr);
}

}  // namespace google::api::expr::runtime
