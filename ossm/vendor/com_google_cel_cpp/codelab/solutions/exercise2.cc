// Copyright 2021 Google LLC
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

#include "codelab/exercise2.h"

#include <memory>

#include "cel/expr/checked.pb.h"
#include "cel/expr/syntax.pb.h"
#include "google/rpc/context/attribute_context.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "checker/type_checker_builder.h"
#include "codelab/cel_compiler.h"
#include "common/decl.h"
#include "common/type.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/standard_library.h"
#include "eval/public/activation.h"
#include "eval/public/activation_bind_helper.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel_codelab {
namespace {

using ::cel::expr::CheckedExpr;
using ::google::api::expr::runtime::Activation;
using ::google::api::expr::runtime::CelError;
using ::google::api::expr::runtime::CelExpression;
using ::google::api::expr::runtime::CelExpressionBuilder;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::CreateCelExpressionBuilder;
using ::google::api::expr::runtime::InterpreterOptions;
using ::google::api::expr::runtime::ProtoUnsetFieldOptions;
using ::google::api::expr::runtime::RegisterBuiltinFunctions;
using ::google::rpc::context::AttributeContext;

absl::StatusOr<std::unique_ptr<cel::Compiler>> MakeCelCompiler() {
  // Note: we are using the generated descriptor pool here for simplicity, but
  // it has the drawback of including all message types that are linked into the
  // binary instead of just the ones expected for the CEL environment.
  google::protobuf::LinkMessageReflection<AttributeContext>();
  CEL_ASSIGN_OR_RETURN(
      std::unique_ptr<cel::CompilerBuilder> builder,
      cel::NewCompilerBuilder(google::protobuf::DescriptorPool::generated_pool()));

  CEL_RETURN_IF_ERROR(builder->AddLibrary(cel::StandardCompilerLibrary()));
  // === Start Codelab ===
  cel::TypeCheckerBuilder& checker_builder = builder->GetCheckerBuilder();
  CEL_RETURN_IF_ERROR(checker_builder.AddVariable(
      cel::MakeVariableDecl("bool_var", cel::BoolType())));
  CEL_RETURN_IF_ERROR(checker_builder.AddContextDeclaration(
      AttributeContext::descriptor()->full_name()));
  // === End Codelab ===

  return builder->Build();
}

// Parse a cel expression and evaluate it against the given activation and
// arena.
absl::StatusOr<bool> EvalCheckedExpr(const CheckedExpr& checked_expr,
                                     const Activation& activation,
                                     google::protobuf::Arena* arena) {
  // Setup a default environment for building expressions.
  InterpreterOptions options;
  std::unique_ptr<CelExpressionBuilder> builder = CreateCelExpressionBuilder(
      google::protobuf::DescriptorPool::generated_pool(),
      google::protobuf::MessageFactory::generated_factory(), options);
  CEL_RETURN_IF_ERROR(
      RegisterBuiltinFunctions(builder->GetRegistry(), options));

  // Note, the expression_plan below is reusable for different inputs, but we
  // create one just in time for evaluation here.
  CEL_ASSIGN_OR_RETURN(std::unique_ptr<CelExpression> expression_plan,
                       builder->CreateExpression(&checked_expr));

  CEL_ASSIGN_OR_RETURN(CelValue result,
                       expression_plan->Evaluate(activation, arena));

  if (bool value; result.GetValue(&value)) {
    return value;
  } else if (const CelError * value; result.GetValue(&value)) {
    return *value;
  } else {
    return absl::InvalidArgumentError(absl::StrCat(
        "expected 'bool' result got '", result.DebugString(), "'"));
  }
}
}  // namespace

absl::StatusOr<bool> CompileAndEvaluateWithBoolVar(absl::string_view cel_expr,
                                                   bool bool_var) {
  CEL_ASSIGN_OR_RETURN(std::unique_ptr<cel::Compiler> compiler,
                       MakeCelCompiler());

  CEL_ASSIGN_OR_RETURN(CheckedExpr checked_expr,
                       CompileToCheckedExpr(*compiler, cel_expr));

  Activation activation;
  google::protobuf::Arena arena;
  // === Start Codelab ===
  activation.InsertValue("bool_var", CelValue::CreateBool(bool_var));
  // === End Codelab ===

  return EvalCheckedExpr(checked_expr, activation, &arena);
}

absl::StatusOr<bool> CompileAndEvaluateWithContext(
    absl::string_view cel_expr, const AttributeContext& context) {
  CEL_ASSIGN_OR_RETURN(std::unique_ptr<cel::Compiler> compiler,
                       MakeCelCompiler());

  CEL_ASSIGN_OR_RETURN(CheckedExpr checked_expr,
                       CompileToCheckedExpr(*compiler, cel_expr));

  Activation activation;
  google::protobuf::Arena arena;
  // === Start Codelab ===
  CEL_RETURN_IF_ERROR(BindProtoToActivation(
      &context, &arena, &activation, ProtoUnsetFieldOptions::kBindDefault));
  // === End Codelab ===

  return EvalCheckedExpr(checked_expr, activation, &arena);
}

}  // namespace cel_codelab
