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

#include "codelab/cel_compiler.h"

#include <memory>
#include <utility>

#include "google/rpc/context/attribute_context.pb.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "common/decl.h"
#include "common/type.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/standard_library.h"
#include "eval/public/activation.h"
#include "eval/public/activation_bind_helper.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_function_adapter.h"
#include "eval/public/cel_value.h"
#include "eval/public/testing/matchers.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel_codelab {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::BoolType;
using ::cel::MakeFunctionDecl;
using ::cel::MakeOverloadDecl;
using ::cel::MakeVariableDecl;
using ::cel::StringType;
using ::google::api::expr::runtime::Activation;
using ::google::api::expr::runtime::BindProtoToActivation;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::CreateCelExpressionBuilder;
using ::google::api::expr::runtime::FunctionAdapter;
using ::google::api::expr::runtime::RegisterBuiltinFunctions;
using ::google::api::expr::runtime::test::IsCelBool;
using ::google::rpc::context::AttributeContext;
using ::testing::HasSubstr;

std::unique_ptr<cel::CompilerBuilder> MakeDefaultCompilerBuilder() {
  google::protobuf::LinkMessageReflection<AttributeContext>();
  auto builder =
      cel::NewCompilerBuilder(google::protobuf::DescriptorPool::generated_pool());
  ABSL_CHECK_OK(builder.status());

  ABSL_CHECK_OK((*builder)->AddLibrary(cel::StandardCompilerLibrary()));
  ABSL_CHECK_OK((*builder)->GetCheckerBuilder().AddContextDeclaration(
      "google.rpc.context.AttributeContext"));

  return std::move(builder).value();
}

TEST(DefaultCompiler, Basic) {
  ASSERT_OK_AND_ASSIGN(auto compiler, MakeDefaultCompilerBuilder()->Build());
  EXPECT_THAT(compiler->Compile("1 < 2").status(), IsOk());
}

TEST(DefaultCompiler, AddFunctionDecl) {
  auto builder = MakeDefaultCompilerBuilder();
  ASSERT_OK_AND_ASSIGN(
      cel::FunctionDecl decl,
      MakeFunctionDecl("IpMatch",
                       MakeOverloadDecl("IpMatch_string_string", BoolType(),
                                        StringType(), StringType())));
  EXPECT_THAT(builder->GetCheckerBuilder().AddFunction(decl), IsOk());

  ASSERT_OK_AND_ASSIGN(auto compiler, builder->Build());

  EXPECT_THAT(CompileToCheckedExpr(
                  *compiler, "IpMatch('255.255.255.255', '255.255.255.255')")
                  .status(),
              IsOk());
  EXPECT_THAT(
      CompileToCheckedExpr(*compiler, "IpMatch('255.255.255.255', 123436)")
          .status(),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("no matching overload")));
}

TEST(DefaultCompiler, EndToEnd) {
  google::protobuf::Arena arena;

  auto compiler_builder = MakeDefaultCompilerBuilder();
  ASSERT_OK_AND_ASSIGN(
      cel::FunctionDecl func_decl,
      MakeFunctionDecl("MyFunc", MakeOverloadDecl("MyFunc", BoolType())));
  ASSERT_THAT(compiler_builder->GetCheckerBuilder().AddFunction(func_decl),
              IsOk());

  ASSERT_THAT(compiler_builder->GetCheckerBuilder().AddVariable(
                  MakeVariableDecl("my_var", BoolType())),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto compiler, compiler_builder->Build());

  ASSERT_OK_AND_ASSIGN(
      auto expr,
      CompileToCheckedExpr(
          *compiler,
          "(my_var || MyFunc()) && request.host == 'www.google.com'"));

  auto builder =
      CreateCelExpressionBuilder(google::protobuf::DescriptorPool::generated_pool(),
                                 google::protobuf::MessageFactory::generated_factory());
  ASSERT_THAT(RegisterBuiltinFunctions(builder->GetRegistry()), IsOk());
  ASSERT_THAT(FunctionAdapter<bool>::CreateAndRegister(
                  "MyFunc", false, [](google::protobuf::Arena*) { return true; },
                  builder->GetRegistry()),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto plan, builder->CreateExpression(&expr));

  AttributeContext context;
  context.mutable_request()->set_host("www.google.com");
  Activation activation;
  ASSERT_THAT(BindProtoToActivation(&context, &arena, &activation), IsOk());
  activation.InsertValue("my_var", CelValue::CreateBool(false));

  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));

  EXPECT_THAT(result, IsCelBool(true));
}

}  // namespace
}  // namespace cel_codelab
