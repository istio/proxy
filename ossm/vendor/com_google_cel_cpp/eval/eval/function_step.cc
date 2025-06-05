#include "eval/eval/function_step.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/ast_internal/expr.h"
#include "base/function.h"
#include "base/function_descriptor.h"
#include "base/kind.h"
#include "common/casting.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "internal/status_macros.h"
#include "runtime/activation_interface.h"
#include "runtime/function_overload_reference.h"
#include "runtime/function_provider.h"
#include "runtime/function_registry.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::FunctionEvaluationContext;

using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::ValueKindToKind;

// Determine if the overload should be considered. Overloads that can consume
// errors or unknown sets must be allowed as a non-strict function.
bool ShouldAcceptOverload(const cel::FunctionDescriptor& descriptor,
                          absl::Span<const cel::Value> arguments) {
  for (size_t i = 0; i < arguments.size(); i++) {
    if (arguments[i]->Is<cel::UnknownValue>() ||
        arguments[i]->Is<cel::ErrorValue>()) {
      return !descriptor.is_strict();
    }
  }
  return true;
}

bool ArgumentKindsMatch(const cel::FunctionDescriptor& descriptor,
                        absl::Span<const cel::Value> arguments) {
  auto types_size = descriptor.types().size();

  if (types_size != arguments.size()) {
    return false;
  }

  for (size_t i = 0; i < types_size; i++) {
    const auto& arg = arguments[i];
    cel::Kind param_kind = descriptor.types()[i];
    if (arg->kind() != param_kind && param_kind != cel::Kind::kAny) {
      return false;
    }
  }

  return true;
}

// Adjust new type names to legacy equivalent. int -> int64_t.
// Temporary fix to migrate value types without breaking clients.
// TODO: Update client tests that depend on this value.
std::string ToLegacyKindName(absl::string_view type_name) {
  if (type_name == "int" || type_name == "uint") {
    return absl::StrCat(type_name, "64");
  }

  return std::string(type_name);
}

std::string CallArgTypeString(absl::Span<const cel::Value> args) {
  std::string call_sig_string = "";

  for (size_t i = 0; i < args.size(); i++) {
    const auto& arg = args[i];
    if (!call_sig_string.empty()) {
      absl::StrAppend(&call_sig_string, ", ");
    }
    absl::StrAppend(
        &call_sig_string,
        ToLegacyKindName(cel::KindToString(ValueKindToKind(arg->kind()))));
  }
  return absl::StrCat("(", call_sig_string, ")");
}

// Convert partially unknown arguments to unknowns before passing to the
// function.
// TODO: See if this can be refactored to remove the eager
// arguments copy.
// Argument and attribute spans are expected to be equal length.
std::vector<cel::Value> CheckForPartialUnknowns(
    ExecutionFrame* frame, absl::Span<const cel::Value> args,
    absl::Span<const AttributeTrail> attrs) {
  std::vector<cel::Value> result;
  result.reserve(args.size());
  for (size_t i = 0; i < args.size(); i++) {
    const AttributeTrail& trail = attrs.subspan(i, 1)[0];

    if (frame->attribute_utility().CheckForUnknown(trail,
                                                   /*use_partial=*/true)) {
      result.push_back(
          frame->attribute_utility().CreateUnknownSet(trail.attribute()));
    } else {
      result.push_back(args.at(i));
    }
  }

  return result;
}

bool IsUnknownFunctionResultError(const Value& result) {
  if (!result->Is<cel::ErrorValue>()) {
    return false;
  }

  const auto& status = result.GetError().NativeValue();

  if (status.code() != absl::StatusCode::kUnavailable) {
    return false;
  }
  auto payload = status.GetPayload(
      cel::runtime_internal::kPayloadUrlUnknownFunctionResult);
  return payload.has_value() && payload.value() == "true";
}

// Simple wrapper around a function resolution result. A function call should
// resolve to a single function implementation and a descriptor or none.
using ResolveResult = absl::optional<cel::FunctionOverloadReference>;

// Implementation of ExpressionStep that finds suitable CelFunction overload and
// invokes it. Abstract base class standardizes behavior between lazy and eager
// function bindings. Derived classes provide ResolveFunction behavior.
class AbstractFunctionStep : public ExpressionStepBase {
 public:
  // Constructs FunctionStep that uses overloads specified.
  AbstractFunctionStep(const std::string& name, size_t num_arguments,
                       int64_t expr_id)
      : ExpressionStepBase(expr_id),
        name_(name),
        num_arguments_(num_arguments) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

  // Handles overload resolution and updating result appropriately.
  // Shouldn't update frame state.
  //
  // A non-ok result is an unrecoverable error, either from an illegal
  // evaluation state or forwarded from an extension function. Errors where
  // evaluation can reasonably condition are returned in the result as a
  // cel::ErrorValue.
  absl::StatusOr<Value> DoEvaluate(ExecutionFrame* frame) const;

  virtual absl::StatusOr<ResolveResult> ResolveFunction(
      absl::Span<const cel::Value> args, const ExecutionFrame* frame) const = 0;

 protected:
  std::string name_;
  size_t num_arguments_;
};

inline absl::StatusOr<Value> Invoke(
    const cel::FunctionOverloadReference& overload, int64_t expr_id,
    absl::Span<const cel::Value> args, ExecutionFrameBase& frame) {
  FunctionEvaluationContext context(frame.value_manager());

  CEL_ASSIGN_OR_RETURN(Value result,
                       overload.implementation.Invoke(context, args));

  if (frame.unknown_function_results_enabled() &&
      IsUnknownFunctionResultError(result)) {
    return frame.attribute_utility().CreateUnknownSet(overload.descriptor,
                                                      expr_id, args);
  }
  return result;
}

Value NoOverloadResult(absl::string_view name,
                       absl::Span<const cel::Value> args,
                       ExecutionFrameBase& frame) {
  // No matching overloads.
  // Such absence can be caused by presence of CelError in arguments.
  // To enable behavior of functions that accept CelError( &&, || ), CelErrors
  // should be propagated along execution path.
  for (size_t i = 0; i < args.size(); i++) {
    const auto& arg = args[i];
    if (cel::InstanceOf<cel::ErrorValue>(arg)) {
      return arg;
    }
  }

  if (frame.unknown_processing_enabled()) {
    // Already converted partial unknowns to unknown sets so just merge.
    absl::optional<UnknownValue> unknown_set =
        frame.attribute_utility().MergeUnknowns(args);
    if (unknown_set.has_value()) {
      return *unknown_set;
    }
  }

  // If no errors or unknowns in input args, create new CelError for missing
  // overload.
  return frame.value_manager().CreateErrorValue(
      cel::runtime_internal::CreateNoMatchingOverloadError(
          absl::StrCat(name, CallArgTypeString(args))));
}

absl::StatusOr<Value> AbstractFunctionStep::DoEvaluate(
    ExecutionFrame* frame) const {
  // Create Span object that contains input arguments to the function.
  auto input_args = frame->value_stack().GetSpan(num_arguments_);

  std::vector<cel::Value> unknowns_args;
  // Preprocess args. If an argument is partially unknown, convert it to an
  // unknown attribute set.
  if (frame->enable_unknowns()) {
    auto input_attrs = frame->value_stack().GetAttributeSpan(num_arguments_);
    unknowns_args = CheckForPartialUnknowns(frame, input_args, input_attrs);
    input_args = absl::MakeConstSpan(unknowns_args);
  }

  // Derived class resolves to a single function overload or none.
  CEL_ASSIGN_OR_RETURN(ResolveResult matched_function,
                       ResolveFunction(input_args, frame));

  // Overload found and is allowed to consume the arguments.
  if (matched_function.has_value() &&
      ShouldAcceptOverload(matched_function->descriptor, input_args)) {
    return Invoke(*matched_function, id(), input_args, *frame);
  }

  return NoOverloadResult(name_, input_args, *frame);
}

absl::Status AbstractFunctionStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(num_arguments_)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }

  // DoEvaluate may return a status for non-recoverable errors  (e.g.
  // unexpected typing, illegal expression state). Application errors that can
  // reasonably be handled as a cel error will appear in the result value.
  CEL_ASSIGN_OR_RETURN(auto result, DoEvaluate(frame));

  frame->value_stack().PopAndPush(num_arguments_, std::move(result));

  return absl::OkStatus();
}

absl::StatusOr<ResolveResult> ResolveStatic(
    absl::Span<const cel::Value> input_args,
    absl::Span<const cel::FunctionOverloadReference> overloads) {
  ResolveResult result = absl::nullopt;

  for (const auto& overload : overloads) {
    if (ArgumentKindsMatch(overload.descriptor, input_args)) {
      // More than one overload matches our arguments.
      if (result.has_value()) {
        return absl::Status(absl::StatusCode::kInternal,
                            "Cannot resolve overloads");
      }

      result.emplace(overload);
    }
  }
  return result;
}

absl::StatusOr<ResolveResult> ResolveLazy(
    absl::Span<const cel::Value> input_args, absl::string_view name,
    bool receiver_style,
    absl::Span<const cel::FunctionRegistry::LazyOverload> providers,
    const ExecutionFrameBase& frame) {
  ResolveResult result = absl::nullopt;

  std::vector<cel::Kind> arg_types(input_args.size());

  std::transform(
      input_args.begin(), input_args.end(), arg_types.begin(),
      [](const cel::Value& value) { return ValueKindToKind(value->kind()); });

  cel::FunctionDescriptor matcher{name, receiver_style, arg_types};

  const cel::ActivationInterface& activation = frame.activation();
  for (auto provider : providers) {
    // The LazyFunctionStep has so far only resolved by function shape, check
    // that the runtime argument kinds agree with the specific descriptor for
    // the provider candidates.
    if (!ArgumentKindsMatch(provider.descriptor, input_args)) {
      continue;
    }

    CEL_ASSIGN_OR_RETURN(auto overload,
                         provider.provider.GetFunction(matcher, activation));
    if (overload.has_value()) {
      // More than one overload matches our arguments.
      if (result.has_value()) {
        return absl::Status(absl::StatusCode::kInternal,
                            "Cannot resolve overloads");
      }

      result.emplace(overload.value());
    }
  }

  return result;
}

class EagerFunctionStep : public AbstractFunctionStep {
 public:
  EagerFunctionStep(std::vector<cel::FunctionOverloadReference> overloads,
                    const std::string& name, size_t num_args, int64_t expr_id)
      : AbstractFunctionStep(name, num_args, expr_id),
        overloads_(std::move(overloads)) {}

  absl::StatusOr<ResolveResult> ResolveFunction(
      absl::Span<const cel::Value> input_args,
      const ExecutionFrame* frame) const override {
    return ResolveStatic(input_args, overloads_);
  }

 private:
  std::vector<cel::FunctionOverloadReference> overloads_;
};

class LazyFunctionStep : public AbstractFunctionStep {
 public:
  // Constructs LazyFunctionStep that attempts to lookup function implementation
  // at runtime.
  LazyFunctionStep(const std::string& name, size_t num_args,
                   bool receiver_style,
                   std::vector<cel::FunctionRegistry::LazyOverload> providers,
                   int64_t expr_id)
      : AbstractFunctionStep(name, num_args, expr_id),
        receiver_style_(receiver_style),
        providers_(std::move(providers)) {}

  absl::StatusOr<ResolveResult> ResolveFunction(
      absl::Span<const cel::Value> input_args,
      const ExecutionFrame* frame) const override;

 private:
  bool receiver_style_;
  std::vector<cel::FunctionRegistry::LazyOverload> providers_;
};

absl::StatusOr<ResolveResult> LazyFunctionStep::ResolveFunction(
    absl::Span<const cel::Value> input_args,
    const ExecutionFrame* frame) const {
  return ResolveLazy(input_args, name_, receiver_style_, providers_, *frame);
}

class StaticResolver {
 public:
  explicit StaticResolver(std::vector<cel::FunctionOverloadReference> overloads)
      : overloads_(std::move(overloads)) {}

  absl::StatusOr<ResolveResult> Resolve(ExecutionFrameBase& frame,
                                        absl::Span<const Value> input) const {
    return ResolveStatic(input, overloads_);
  }

 private:
  std::vector<cel::FunctionOverloadReference> overloads_;
};

class LazyResolver {
 public:
  explicit LazyResolver(
      std::vector<cel::FunctionRegistry::LazyOverload> providers,
      std::string name, bool receiver_style)
      : providers_(std::move(providers)),
        name_(std::move(name)),
        receiver_style_(receiver_style) {}

  absl::StatusOr<ResolveResult> Resolve(ExecutionFrameBase& frame,
                                        absl::Span<const Value> input) const {
    return ResolveLazy(input, name_, receiver_style_, providers_, frame);
  }

 private:
  std::vector<cel::FunctionRegistry::LazyOverload> providers_;
  std::string name_;
  bool receiver_style_;
};

template <typename Resolver>
class DirectFunctionStepImpl : public DirectExpressionStep {
 public:
  DirectFunctionStepImpl(
      int64_t expr_id, const std::string& name,
      std::vector<std::unique_ptr<DirectExpressionStep>> arg_steps,
      Resolver&& resolver)
      : DirectExpressionStep(expr_id),
        name_(name),
        arg_steps_(std::move(arg_steps)),
        resolver_(std::forward<Resolver>(resolver)) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, cel::Value& result,
                        AttributeTrail& trail) const override {
    absl::InlinedVector<Value, 2> args;
    absl::InlinedVector<AttributeTrail, 2> arg_trails;

    args.resize(arg_steps_.size());
    arg_trails.resize(arg_steps_.size());

    for (size_t i = 0; i < arg_steps_.size(); i++) {
      CEL_RETURN_IF_ERROR(
          arg_steps_[i]->Evaluate(frame, args[i], arg_trails[i]));
    }

    if (frame.unknown_processing_enabled()) {
      for (size_t i = 0; i < arg_trails.size(); i++) {
        if (frame.attribute_utility().CheckForUnknown(arg_trails[i],
                                                      /*use_partial=*/true)) {
          args[i] = frame.attribute_utility().CreateUnknownSet(
              arg_trails[i].attribute());
        }
      }
    }

    CEL_ASSIGN_OR_RETURN(ResolveResult resolved_function,
                         resolver_.Resolve(frame, args));

    if (resolved_function.has_value() &&
        ShouldAcceptOverload(resolved_function->descriptor, args)) {
      CEL_ASSIGN_OR_RETURN(result,
                           Invoke(*resolved_function, expr_id_, args, frame));

      return absl::OkStatus();
    }

    result = NoOverloadResult(name_, args, frame);

    return absl::OkStatus();
  }

  absl::optional<std::vector<const DirectExpressionStep*>> GetDependencies()
      const override {
    std::vector<const DirectExpressionStep*> dependencies;
    dependencies.reserve(arg_steps_.size());
    for (const auto& arg_step : arg_steps_) {
      dependencies.push_back(arg_step.get());
    }
    return dependencies;
  }

  absl::optional<std::vector<std::unique_ptr<DirectExpressionStep>>>
  ExtractDependencies() override {
    return std::move(arg_steps_);
  }

 private:
  friend Resolver;
  std::string name_;
  std::vector<std::unique_ptr<DirectExpressionStep>> arg_steps_;
  Resolver resolver_;
};

}  // namespace

std::unique_ptr<DirectExpressionStep> CreateDirectFunctionStep(
    int64_t expr_id, const cel::ast_internal::Call& call,
    std::vector<std::unique_ptr<DirectExpressionStep>> deps,
    std::vector<cel::FunctionOverloadReference> overloads) {
  return std::make_unique<DirectFunctionStepImpl<StaticResolver>>(
      expr_id, call.function(), std::move(deps),
      StaticResolver(std::move(overloads)));
}

std::unique_ptr<DirectExpressionStep> CreateDirectLazyFunctionStep(
    int64_t expr_id, const cel::ast_internal::Call& call,
    std::vector<std::unique_ptr<DirectExpressionStep>> deps,
    std::vector<cel::FunctionRegistry::LazyOverload> providers) {
  return std::make_unique<DirectFunctionStepImpl<LazyResolver>>(
      expr_id, call.function(), std::move(deps),
      LazyResolver(std::move(providers), call.function(), call.has_target()));
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateFunctionStep(
    const cel::ast_internal::Call& call_expr, int64_t expr_id,
    std::vector<cel::FunctionRegistry::LazyOverload> lazy_overloads) {
  bool receiver_style = call_expr.has_target();
  size_t num_args = call_expr.args().size() + (receiver_style ? 1 : 0);
  const std::string& name = call_expr.function();
  return std::make_unique<LazyFunctionStep>(name, num_args, receiver_style,
                                            std::move(lazy_overloads), expr_id);
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateFunctionStep(
    const cel::ast_internal::Call& call_expr, int64_t expr_id,
    std::vector<cel::FunctionOverloadReference> overloads) {
  bool receiver_style = call_expr.has_target();
  size_t num_args = call_expr.args().size() + (receiver_style ? 1 : 0);
  const std::string& name = call_expr.function();
  return std::make_unique<EagerFunctionStep>(std::move(overloads), name,
                                             num_args, expr_id);
}

}  // namespace google::api::expr::runtime
