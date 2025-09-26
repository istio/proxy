#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_EXPRESSION_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_EXPRESSION_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "cel/expr/checked.pb.h"
#include "cel/expr/syntax.pb.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "eval/public/base_activation.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_type_registry.h"
#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

// CelEvaluationListener is the callback that is passed to (and called by)
// CelExpression::Trace. It gets an expression node ID from the original
// expression, its value and the arena object. If an expression node
// is evaluated multiple times (e.g. as a part of Comprehension.loop_step)
// then the order of the callback invocations is guaranteed to correspond
// the order of variable sub-elements (e.g. the order of elements returned
// by Comprehension.iter_range).
using CelEvaluationListener = std::function<absl::Status(
    int64_t expr_id, const CelValue&, google::protobuf::Arena*)>;

// An opaque state used for evaluation of a CEL expression.
class CelEvaluationState {
 public:
  virtual ~CelEvaluationState() = default;
};

// Base interface for expression evaluating objects.
class CelExpression {
 public:
  virtual ~CelExpression() = default;

  // Initializes the state
  virtual std::unique_ptr<CelEvaluationState> InitializeState(
      google::protobuf::Arena* arena) const = 0;

  // Evaluates expression and returns value.
  // activation contains bindings from parameter names to values
  // arena parameter specifies Arena object where output result and
  // internal data will be allocated.
  virtual absl::StatusOr<CelValue> Evaluate(const BaseActivation& activation,
                                            google::protobuf::Arena* arena) const = 0;

  // Evaluates expression and returns value.
  // activation contains bindings from parameter names to values
  // state must be non-null and created prior to calling Evaluate by
  // InitializeState.
  virtual absl::StatusOr<CelValue> Evaluate(
      const BaseActivation& activation, CelEvaluationState* state) const = 0;

  // Trace evaluates expression calling the callback on each sub-tree.
  virtual absl::StatusOr<CelValue> Trace(
      const BaseActivation& activation, google::protobuf::Arena* arena,
      CelEvaluationListener callback) const = 0;

  // Trace evaluates expression calling the callback on each sub-tree.
  // state must be non-null and created prior to calling Evaluate by
  // InitializeState.
  virtual absl::StatusOr<CelValue> Trace(
      const BaseActivation& activation, CelEvaluationState* state,
      CelEvaluationListener callback) const = 0;
};

// Base class for Expression Builder implementations
// Provides user with factory to register extension functions.
// ExpressionBuilder MUST NOT be destroyed before CelExpression objects
// it built.
class CelExpressionBuilder {
 public:
  CelExpressionBuilder() = default;

  virtual ~CelExpressionBuilder() = default;

  // Creates CelExpression object from AST tree.
  // expr specifies root of AST tree
  //
  // IMPORTANT: The `expr` and `source_info` must outlive the resulting
  // CelExpression.
  virtual absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const cel::expr::Expr* expr,
      const cel::expr::SourceInfo* source_info) const = 0;

  // Creates CelExpression object from AST tree.
  // expr specifies root of AST tree.
  // non-fatal build warnings are written to warnings if encountered.
  //
  // IMPORTANT: The `expr` and `source_info` must outlive the resulting
  // CelExpression.
  virtual absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const cel::expr::Expr* expr,
      const cel::expr::SourceInfo* source_info,
      std::vector<absl::Status>* warnings) const = 0;

  // Creates CelExpression object from a checked expression.
  // This includes an AST, source info, type hints and ident hints.
  //
  // IMPORTANT: The `checked_expr` must outlive the resulting CelExpression.
  virtual absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const cel::expr::CheckedExpr* checked_expr) const {
    // Default implementation just passes through the expr and source info.
    return CreateExpression(&checked_expr->expr(),
                            &checked_expr->source_info());
  }

  // Creates CelExpression object from a checked expression.
  // This includes an AST, source info, type hints and ident hints.
  // non-fatal build warnings are written to warnings if encountered.
  //
  // IMPORTANT: The `checked_expr` must outlive the resulting CelExpression.
  virtual absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const cel::expr::CheckedExpr* checked_expr,
      std::vector<absl::Status>* warnings) const {
    // Default implementation just passes through the expr and source_info.
    return CreateExpression(&checked_expr->expr(), &checked_expr->source_info(),
                            warnings);
  }

  // CelFunction registry. Extension function should be registered with it
  // prior to expression creation.
  virtual CelFunctionRegistry* GetRegistry() const = 0;

  // CEL Type registry. Provides a means to resolve the CEL built-in types to
  // CelValue instances, and to extend the set of types and enums known to
  // expressions by registering them ahead of time.
  virtual CelTypeRegistry* GetTypeRegistry() const = 0;

  virtual void set_container(std::string container) = 0;

  virtual absl::string_view container() const = 0;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_EXPRESSION_H_
