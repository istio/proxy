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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_CEL_EXPRESSION_FLAT_IMPL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_CEL_EXPRESSION_FLAT_IMPL_H_

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/public/base_activation.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_value.h"
#include "internal/casts.h"
#include "runtime/internal/runtime_env.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime {

// Wrapper for FlatExpressionEvaluationState used to implement CelExpression.
class CelExpressionFlatEvaluationState : public CelEvaluationState {
 public:
  CelExpressionFlatEvaluationState(
      google::protobuf::Arena* arena,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      const FlatExpression& expr);

  google::protobuf::Arena* arena() { return state_.arena(); }
  FlatExpressionEvaluatorState& state() { return state_; }

 private:
  FlatExpressionEvaluatorState state_;
};

// Implementation of the CelExpression that evaluates a flattened representation
// of the AST.
//
// This class adapts FlatExpression to implement the CelExpression interface.
class CelExpressionFlatImpl : public CelExpression {
 public:
  CelExpressionFlatImpl(
      absl_nonnull std::shared_ptr<const cel::runtime_internal::RuntimeEnv> env,
      FlatExpression flat_expression)
      : env_(std::move(env)), flat_expression_(std::move(flat_expression)) {}

  // Move-only
  CelExpressionFlatImpl(const CelExpressionFlatImpl&) = delete;
  CelExpressionFlatImpl& operator=(const CelExpressionFlatImpl&) = delete;
  CelExpressionFlatImpl(CelExpressionFlatImpl&&) = default;
  CelExpressionFlatImpl& operator=(CelExpressionFlatImpl&&) = delete;

  // Implement CelExpression.
  std::unique_ptr<CelEvaluationState> InitializeState(
      google::protobuf::Arena* arena) const override;

  absl::StatusOr<CelValue> Evaluate(const BaseActivation& activation,
                                    google::protobuf::Arena* arena) const override {
    return Evaluate(activation, InitializeState(arena).get());
  }

  absl::StatusOr<CelValue> Evaluate(const BaseActivation& activation,
                                    CelEvaluationState* state) const override;
  absl::StatusOr<CelValue> Trace(
      const BaseActivation& activation, google::protobuf::Arena* arena,
      CelEvaluationListener callback) const override {
    return Trace(activation, InitializeState(arena).get(), callback);
  }

  absl::StatusOr<CelValue> Trace(const BaseActivation& activation,
                                 CelEvaluationState* state,
                                 CelEvaluationListener callback) const override;

  // Exposed for inspection in tests.
  const FlatExpression& flat_expression() const { return flat_expression_; }

 private:
  absl_nonnull std::shared_ptr<const cel::runtime_internal::RuntimeEnv> env_;
  FlatExpression flat_expression_;
};

// Implementation of the CelExpression that evaluates a recursive representation
// of the AST.
//
// This class adapts FlatExpression to implement the CelExpression interface.
//
// Assumes that the flat expression is wrapping a simple recursive program.
class CelExpressionRecursiveImpl : public CelExpression {
 private:
  class EvaluationState : public CelEvaluationState {
   public:
    explicit EvaluationState(google::protobuf::Arena* arena) : arena_(arena) {}
    google::protobuf::Arena* arena() { return arena_; }

   private:
    google::protobuf::Arena* arena_;
  };

 public:
  static absl::StatusOr<std::unique_ptr<CelExpressionRecursiveImpl>> Create(
      absl_nonnull std::shared_ptr<const cel::runtime_internal::RuntimeEnv> env,
      FlatExpression flat_expression);

  // Move-only
  CelExpressionRecursiveImpl(const CelExpressionRecursiveImpl&) = delete;
  CelExpressionRecursiveImpl& operator=(const CelExpressionRecursiveImpl&) =
      delete;
  CelExpressionRecursiveImpl(CelExpressionRecursiveImpl&&) = default;
  CelExpressionRecursiveImpl& operator=(CelExpressionRecursiveImpl&&) = delete;

  // Implement CelExpression.
  std::unique_ptr<CelEvaluationState> InitializeState(
      google::protobuf::Arena* arena) const override {
    return std::make_unique<EvaluationState>(arena);
  }

  absl::StatusOr<CelValue> Evaluate(const BaseActivation& activation,
                                    google::protobuf::Arena* arena) const override;

  absl::StatusOr<CelValue> Evaluate(const BaseActivation& activation,
                                    CelEvaluationState* state) const override {
    auto* state_impl = cel::internal::down_cast<EvaluationState*>(state);
    return Evaluate(activation, state_impl->arena());
  }

  absl::StatusOr<CelValue> Trace(const BaseActivation& activation,
                                 google::protobuf::Arena* arena,
                                 CelEvaluationListener callback) const override;

  absl::StatusOr<CelValue> Trace(
      const BaseActivation& activation, CelEvaluationState* state,
      CelEvaluationListener callback) const override {
    auto* state_impl = cel::internal::down_cast<EvaluationState*>(state);
    return Trace(activation, state_impl->arena(), callback);
  }

  // Exposed for inspection in tests.
  const FlatExpression& flat_expression() const { return flat_expression_; }

  const DirectExpressionStep* root() const { return root_; }

 private:
  explicit CelExpressionRecursiveImpl(
      absl_nonnull std::shared_ptr<const cel::runtime_internal::RuntimeEnv> env,
      FlatExpression flat_expression)
      : env_(std::move(env)),
        flat_expression_(std::move(flat_expression)),
        root_(cel::internal::down_cast<const WrappedDirectStep*>(
                  flat_expression_.path()[0].get())
                  ->wrapped()) {}

  absl_nonnull std::shared_ptr<const cel::runtime_internal::RuntimeEnv> env_;
  FlatExpression flat_expression_;
  const DirectExpressionStep* root_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_CEL_EXPRESSION_FLAT_IMPL_H_
