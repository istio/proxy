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
#include <utility>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/protobuf/text_format.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "codelab/cel_compiler.h"
#include "eval/public/activation.h"
#include "eval/public/activation_bind_helper.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_function_adapter.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "internal/status_macros.h"

namespace google::api::expr::codelab {
namespace {

using ::google::api::expr::runtime::Activation;
using ::google::api::expr::runtime::BindProtoToActivation;
using ::google::api::expr::runtime::CelError;
using ::google::api::expr::runtime::CelMap;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::CreateCelExpressionBuilder;
using ::google::api::expr::runtime::FunctionAdapter;
using ::google::api::expr::runtime::InterpreterOptions;
using ::google::api::expr::runtime::RegisterBuiltinFunctions;

using ::google::rpc::context::AttributeContext;

absl::StatusOr<bool> ContainsExtensionFunction(
    google::protobuf::Arena* arena, const CelMap* map, CelValue::StringHolder key,
    CelValue::StringHolder expected_value) {
  absl::optional<CelValue> entry = (*map)[CelValue::CreateString(key)];
  if (entry.has_value()) {
    if (CelValue::StringHolder entry_value; entry->GetValue(&entry_value)) {
      return entry_value.value() == expected_value.value();
    }
  }
  return false;
}

class Compiler {
 public:
  explicit Compiler(std::unique_ptr<CelCompilerInterface> compiler)
      : compiler_(std::move(compiler)) {}

  absl::Status SetupCheckerEnvironment() {
    // Codelab part 1:
    // Add a declaration for the map.contains(string, string) function.
    Decl decl;
    if (!google::protobuf::TextFormat::ParseFromString(
            R"pb(
              name: "contains"
              function {
                overloads {
                  overload_id: "map_contains_string_string"
                  result_type { primitive: BOOL }
                  is_instance_function: true
                  params {
                    map_type {
                      key_type { primitive: STRING }
                      value_type { dyn {} }
                    }
                  }
                  params { primitive: STRING }
                  params { primitive: STRING }
                }
              })pb",
            &decl)) {
      return absl::InternalError("Failed to setup type check environment.");
    }
    return compiler_->AddDeclaration(std::move(decl));
  }

  absl::StatusOr<CheckedExpr> Compile(absl::string_view expr) {
    return compiler_->Compile(expr);
  }

 private:
  std::unique_ptr<CelCompilerInterface> compiler_;
};

class Evaluator {
 public:
  Evaluator() { builder_ = CreateCelExpressionBuilder(options_); }

  absl::Status SetupEvaluatorEnvironment() {
    CEL_RETURN_IF_ERROR(RegisterBuiltinFunctions(builder_->GetRegistry()));
    // Codelab part 2:
    // Register the map.contains(string, string) function.
    // Hint: use `CelFunctionAdapter::CreateAndRegister` to adapt from
    // ContainsExtensionFunction.
    using AdapterT =
        FunctionAdapter<absl::StatusOr<bool>, const CelMap*,
                        CelValue::StringHolder, CelValue::StringHolder>;
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
    } else if (const CelError * value; result.GetValue(&value)) {
      return *value;
    } else {
      return absl::InvalidArgumentError(
          absl::StrCat("unexpected return type: ", result.DebugString()));
    }
  }

 private:
  google::protobuf::Arena arena_;
  std::unique_ptr<runtime::CelExpressionBuilder> builder_;
  InterpreterOptions options_;
};

}  // namespace

absl::StatusOr<bool> EvaluateWithExtensionFunction(
    absl::string_view expr, const AttributeContext& context) {
  // Prepare a checked expression.
  Compiler compiler(GetDefaultCompiler());
  CEL_RETURN_IF_ERROR(compiler.SetupCheckerEnvironment());
  CEL_ASSIGN_OR_RETURN(auto checked_expr, compiler.Compile(expr));

  // Prepare an evaluation environment.
  Evaluator evaluator;
  CEL_RETURN_IF_ERROR(evaluator.SetupEvaluatorEnvironment());

  // Evaluate a checked expression against a particular activation;
  return evaluator.Evaluate(checked_expr, context);
}

}  // namespace google::api::expr::codelab
