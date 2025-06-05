// Copyright 2023 Google LLC
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
#include "runtime/reference_resolver.h"

#include <cstdint>
#include <utility>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "base/function_adapter.h"
#include "common/value.h"
#include "extensions/protobuf/runtime_adapter.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "parser/parser.h"
#include "runtime/activation.h"
#include "runtime/managed_value_factory.h"
#include "runtime/register_function_helper.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "google/protobuf/text_format.h"

namespace cel {
namespace {

using ::cel::extensions::ProtobufRuntimeAdapter;
using ::google::api::expr::v1alpha1::CheckedExpr;
using ::google::api::expr::v1alpha1::Expr;
using ::google::api::expr::v1alpha1::ParsedExpr;

using ::google::api::expr::parser::Parse;

using ::absl_testing::StatusIs;
using ::testing::HasSubstr;

TEST(ReferenceResolver, ResolveQualifiedFunctions) {
  RuntimeOptions options;
  ASSERT_OK_AND_ASSIGN(RuntimeBuilder builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));

  ASSERT_OK(
      EnableReferenceResolver(builder, ReferenceResolverEnabled::kAlways));

  absl::Status status =
      RegisterHelper<BinaryFunctionAdapter<int64_t, int64_t, int64_t>>::
          RegisterGlobalOverload(
              "com.example.Exp",
              [](ValueManager& value_factory, int64_t base,
                 int64_t exp) -> int64_t {
                int64_t result = 1;
                for (int64_t i = 0; i < exp; ++i) {
                  result *= base;
                }
                return result;
              },
              builder.function_registry());
  ASSERT_OK(status);

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr,
                       Parse("com.example.Exp(2, 3) == 8"));

  ASSERT_OK_AND_ASSIGN(auto program, ProtobufRuntimeAdapter::CreateProgram(
                                         *runtime, parsed_expr));

  ManagedValueFactory value_factory(program->GetTypeProvider(),
                                    MemoryManagerRef::ReferenceCounting());
  Activation activation;

  ASSERT_OK_AND_ASSIGN(Value value,
                       program->Evaluate(activation, value_factory.get()));
  ASSERT_TRUE(value->Is<BoolValue>());
  EXPECT_TRUE(value.GetBool().NativeValue());
}

TEST(ReferenceResolver, ResolveQualifiedFunctionsCheckedOnly) {
  RuntimeOptions options;
  ASSERT_OK_AND_ASSIGN(RuntimeBuilder builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));

  ASSERT_OK(EnableReferenceResolver(
      builder, ReferenceResolverEnabled::kCheckedExpressionOnly));

  absl::Status status =
      RegisterHelper<BinaryFunctionAdapter<int64_t, int64_t, int64_t>>::
          RegisterGlobalOverload(
              "com.example.Exp",
              [](ValueManager& value_factory, int64_t base,
                 int64_t exp) -> int64_t {
                int64_t result = 1;
                for (int64_t i = 0; i < exp; ++i) {
                  result *= base;
                }
                return result;
              },
              builder.function_registry());
  ASSERT_OK(status);

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr,
                       Parse("com.example.Exp(2, 3) == 8"));

  EXPECT_THAT(ProtobufRuntimeAdapter::CreateProgram(*runtime, parsed_expr),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("No overloads provided")));
}

// com.example.x + com.example.y
constexpr absl::string_view kIdentifierExpression = R"pb(
  reference_map: {
    key: 3
    value: { name: "com.example.x" }
  }
  reference_map: {
    key: 4
    value: { overload_id: "add_int64" }
  }
  reference_map: {
    key: 7
    value: { name: "com.example.y" }
  }
  type_map: {
    key: 3
    value: { primitive: INT64 }
  }
  type_map: {
    key: 4
    value: { primitive: INT64 }
  }
  type_map: {
    key: 7
    value: { primitive: INT64 }
  }
  source_info: {
    location: "<input>"
    line_offsets: 30
    positions: { key: 1 value: 0 }
    positions: { key: 2 value: 3 }
    positions: { key: 3 value: 11 }
    positions: { key: 4 value: 14 }
    positions: { key: 5 value: 16 }
    positions: { key: 6 value: 19 }
    positions: { key: 7 value: 27 }
  }
  expr: {
    id: 4
    call_expr: {
      function: "_+_"
      args: {
        id: 3
        # compilers typically already apply this rewrite, but older saved
        # expressions might preserve the original parse.
        select_expr {
          operand {
            id: 8
            select_expr {
              operand: {
                id: 9
                ident_expr { name: "com" }
              }
              field: "example"
            }
          }
          field: "x"
        }
      }
      args: {
        id: 7
        ident_expr: { name: "com.example.y" }
      }
    }
  })pb";

TEST(ReferenceResolver, ResolveQualifiedIdentifiers) {
  RuntimeOptions options;
  ASSERT_OK_AND_ASSIGN(RuntimeBuilder builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));

  ASSERT_OK(EnableReferenceResolver(
      builder, ReferenceResolverEnabled::kCheckedExpressionOnly));

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  CheckedExpr checked_expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kIdentifierExpression,
                                                  &checked_expr));

  ASSERT_OK_AND_ASSIGN(auto program, ProtobufRuntimeAdapter::CreateProgram(
                                         *runtime, checked_expr));

  ManagedValueFactory value_factory(program->GetTypeProvider(),
                                    MemoryManagerRef::ReferenceCounting());
  Activation activation;

  activation.InsertOrAssignValue("com.example.x",
                                 value_factory.get().CreateIntValue(3));
  activation.InsertOrAssignValue("com.example.y",
                                 value_factory.get().CreateIntValue(4));

  ASSERT_OK_AND_ASSIGN(Value value,
                       program->Evaluate(activation, value_factory.get()));

  ASSERT_TRUE(value->Is<IntValue>());
  EXPECT_EQ(value.GetInt().NativeValue(), 7);
}

TEST(ReferenceResolver, ResolveQualifiedIdentifiersSkipParseOnly) {
  RuntimeOptions options;
  ASSERT_OK_AND_ASSIGN(RuntimeBuilder builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));

  ASSERT_OK(EnableReferenceResolver(
      builder, ReferenceResolverEnabled::kCheckedExpressionOnly));

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  CheckedExpr checked_expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kIdentifierExpression,
                                                  &checked_expr));

  // Discard type-check information
  Expr unchecked_expr = checked_expr.expr();
  ASSERT_OK_AND_ASSIGN(auto program, ProtobufRuntimeAdapter::CreateProgram(
                                         *runtime, checked_expr.expr()));

  ManagedValueFactory value_factory(program->GetTypeProvider(),
                                    MemoryManagerRef::ReferenceCounting());
  Activation activation;

  activation.InsertOrAssignValue("com.example.x",
                                 value_factory.get().CreateIntValue(3));
  activation.InsertOrAssignValue("com.example.y",
                                 value_factory.get().CreateIntValue(4));

  ASSERT_OK_AND_ASSIGN(Value value,
                       program->Evaluate(activation, value_factory.get()));

  ASSERT_TRUE(value->Is<ErrorValue>());
  EXPECT_THAT(value.GetError().NativeValue(),
              StatusIs(absl::StatusCode::kUnknown, HasSubstr("\"com\"")));
}

// google.api.expr.test.v1.proto2.GlobalEnum.GAZ == 2
constexpr absl::string_view kEnumExpr = R"pb(
  reference_map: {
    key: 8
    value: {
      name: "google.api.expr.test.v1.proto2.GlobalEnum.GAZ"
      value: { int64_value: 2 }
    }
  }
  reference_map: {
    key: 9
    value: { overload_id: "equals" }
  }
  type_map: {
    key: 8
    value: { primitive: INT64 }
  }
  type_map: {
    key: 9
    value: { primitive: BOOL }
  }
  type_map: {
    key: 10
    value: { primitive: INT64 }
  }
  source_info: {
    location: "<input>"
    line_offsets: 1
    line_offsets: 64
    line_offsets: 77
    positions: { key: 1 value: 13 }
    positions: { key: 2 value: 19 }
    positions: { key: 3 value: 23 }
    positions: { key: 4 value: 28 }
    positions: { key: 5 value: 33 }
    positions: { key: 6 value: 36 }
    positions: { key: 7 value: 43 }
    positions: { key: 8 value: 54 }
    positions: { key: 9 value: 59 }
    positions: { key: 10 value: 62 }
  }
  expr: {
    id: 9
    call_expr: {
      function: "_==_"
      args: {
        id: 8
        ident_expr: { name: "google.api.expr.test.v1.proto2.GlobalEnum.GAZ" }
      }
      args: {
        id: 10
        const_expr: { int64_value: 2 }
      }
    }
  })pb";

TEST(ReferenceResolver, ResolveEnumConstants) {
  RuntimeOptions options;
  ASSERT_OK_AND_ASSIGN(RuntimeBuilder builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));

  ASSERT_OK(EnableReferenceResolver(
      builder, ReferenceResolverEnabled::kCheckedExpressionOnly));

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  CheckedExpr checked_expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kEnumExpr, &checked_expr));

  ASSERT_OK_AND_ASSIGN(auto program, ProtobufRuntimeAdapter::CreateProgram(
                                         *runtime, checked_expr));

  ManagedValueFactory value_factory(program->GetTypeProvider(),
                                    MemoryManagerRef::ReferenceCounting());
  Activation activation;

  ASSERT_OK_AND_ASSIGN(Value value,
                       program->Evaluate(activation, value_factory.get()));

  ASSERT_TRUE(value->Is<BoolValue>());
  EXPECT_TRUE(value.GetBool().NativeValue());
}

TEST(ReferenceResolver, ResolveEnumConstantsSkipParseOnly) {
  RuntimeOptions options;
  ASSERT_OK_AND_ASSIGN(RuntimeBuilder builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));

  ASSERT_OK(EnableReferenceResolver(
      builder, ReferenceResolverEnabled::kCheckedExpressionOnly));

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  CheckedExpr checked_expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kEnumExpr, &checked_expr));

  Expr unchecked_expr = checked_expr.expr();
  ASSERT_OK_AND_ASSIGN(auto program, ProtobufRuntimeAdapter::CreateProgram(
                                         *runtime, unchecked_expr));

  ManagedValueFactory value_factory(program->GetTypeProvider(),
                                    MemoryManagerRef::ReferenceCounting());
  Activation activation;

  ASSERT_OK_AND_ASSIGN(Value value,
                       program->Evaluate(activation, value_factory.get()));

  ASSERT_TRUE(value->Is<ErrorValue>());
  EXPECT_THAT(
      value.GetError().NativeValue(),
      StatusIs(absl::StatusCode::kUnknown,
               HasSubstr("\"google.api.expr.test.v1.proto2.GlobalEnum.GAZ\"")));
}

}  // namespace
}  // namespace cel
