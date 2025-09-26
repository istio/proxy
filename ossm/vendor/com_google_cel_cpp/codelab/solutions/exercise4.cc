// Copyright 2022 Google LLC
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

#include "codelab/exercise4.h"

#include <memory>

#include "cel/expr/checked.pb.h"
#include "google/rpc/context/attribute_context.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
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
#include "eval/public/cel_function_adapter.h"
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
using ::google::api::expr::runtime::BindProtoToActivation;
using ::google::api::expr::runtime::CelError;
using ::google::api::expr::runtime::CelExpressionBuilder;
using ::google::api::expr::runtime::CelMap;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::CreateCelExpressionBuilder;
using ::google::api::expr::runtime::FunctionAdapter;
using ::google::api::expr::runtime::InterpreterOptions;
using ::google::api::expr::runtime::RegisterBuiltinFunctions;
using ::google::rpc::context::AttributeContext;

// Handle the parametric type overload with a single generic CelValue overload.
absl::StatusOr<bool> ContainsExtensionFunction(google::protobuf::Arena* arena,
                                               const CelMap* map,
                                               CelValue::StringHolder key,
                                               const CelValue& value) {
  absl::optional<CelValue> entry = (*map)[CelValue::CreateString(key)];
  if (!entry.has_value()) {
    return false;
  }
  if (value.IsInt64() && entry->IsInt64()) {
    return value.Int64OrDie() == entry->Int64OrDie();
  } else if (value.IsString() && entry->IsString()) {
    return value.StringOrDie().value() == entry->StringOrDie().value();
  }
  return false;
}

absl::StatusOr<std::unique_ptr<cel::Compiler>> MakeConfiguredCompiler() {
  // Setup for handling for protobuf types.
  // Using the generated descriptor pool is simpler to configure, but often
  // adds more types than necessary.
  google::protobuf::LinkMessageReflection<AttributeContext>();
  CEL_ASSIGN_OR_RETURN(
      std::unique_ptr<cel::CompilerBuilder> builder,
      cel::NewCompilerBuilder(google::protobuf::DescriptorPool::generated_pool()));
  CEL_RETURN_IF_ERROR(builder->AddLibrary(cel::StandardCompilerLibrary()));
  // Adds fields of AttributeContext as variables.
  CEL_RETURN_IF_ERROR(builder->GetCheckerBuilder().AddContextDeclaration(
      AttributeContext::descriptor()->full_name()));

  // Codelab part 1:
  // Add a declaration for the map<string, V>.contains(string, V) function.
  auto& checker_builder = builder->GetCheckerBuilder();
  // Note: we use MakeMemberOverloadDecl instead of MakeOverloadDecl
  // because the function is receiver style, meaning that it is called as
  // e1.f(e2) instead of f(e1, e2).
  CEL_ASSIGN_OR_RETURN(
      cel::FunctionDecl decl,
      cel::MakeFunctionDecl(
          "contains",
          cel::MakeMemberOverloadDecl(
              "map_contains_string_string", cel::BoolType(),
              cel::MapType(checker_builder.arena(), cel::StringType(),
                           cel::TypeParamType("V")),
              cel::StringType(), cel::TypeParamType("V"))));
  // Note: we use MergeFunction instead of AddFunction because we are adding
  // an overload to an already declared function with the same name.
  CEL_RETURN_IF_ERROR(checker_builder.MergeFunction(decl));
  return builder->Build();
}

class Evaluator {
 public:
  Evaluator() {
    builder_ = CreateCelExpressionBuilder(
        google::protobuf::DescriptorPool::generated_pool(),
        google::protobuf::MessageFactory::generated_factory(), options_);
  }

  absl::Status SetupEvaluatorEnvironment() {
    CEL_RETURN_IF_ERROR(RegisterBuiltinFunctions(builder_->GetRegistry()));
    // Codelab part 2:
    // Register the map.contains(string, string) function.
    // Hint: use `FunctionAdapter::CreateAndRegister` to adapt from a free
    // function ContainsExtensionFunction.
    using AdapterT = FunctionAdapter<absl::StatusOr<bool>, const CelMap*,
                                     CelValue::StringHolder, CelValue>;
    CEL_RETURN_IF_ERROR(AdapterT::CreateAndRegister(
        "contains", /*receiver_style=*/true, &ContainsExtensionFunction,
        builder_->GetRegistry()));
    return absl::OkStatus();
  }

  absl::StatusOr<bool> Evaluate(const CheckedExpr& expr,
                                const AttributeContext& context) {
    Activation activation;
    CEL_RETURN_IF_ERROR(BindProtoToActivation(&context, &arena_, &activation));
    CEL_ASSIGN_OR_RETURN(auto plan, builder_->CreateExpression(&expr));
    CEL_ASSIGN_OR_RETURN(CelValue result, plan->Evaluate(activation, &arena_));

    if (bool value; result.GetValue(&value)) {
      return value;
    } else if (const CelError* value; result.GetValue(&value)) {
      return *value;
    } else {
      return absl::InvalidArgumentError(
          absl::StrCat("unexpected return type: ", result.DebugString()));
    }
  }

 private:
  google::protobuf::Arena arena_;
  std::unique_ptr<CelExpressionBuilder> builder_;
  InterpreterOptions options_;
};

}  // namespace

absl::StatusOr<bool> EvaluateWithExtensionFunction(
    absl::string_view expr, const AttributeContext& context) {
  // Prepare a checked expression.
  CEL_ASSIGN_OR_RETURN(std::unique_ptr<cel::Compiler> compiler,
                       MakeConfiguredCompiler());
  CEL_ASSIGN_OR_RETURN(auto checked_expr,
                       CompileToCheckedExpr(*compiler, expr));

  // Prepare an evaluation environment.
  Evaluator evaluator;
  CEL_RETURN_IF_ERROR(evaluator.SetupEvaluatorEnvironment());

  // Evaluate a checked expression against a particular activation
  return evaluator.Evaluate(checked_expr, context);
}

}  // namespace cel_codelab
