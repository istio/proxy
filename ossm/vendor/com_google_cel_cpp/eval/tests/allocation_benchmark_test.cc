// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//       https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <string>
#include <utility>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/rpc/context/attribute_context.pb.h"
#include "google/protobuf/text_format.h"
#include "absl/base/attributes.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/substitute.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/tests/request_context.pb.h"
#include "internal/benchmark.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "parser/parser.h"

namespace google::api::expr::runtime {
namespace {

using ::absl_testing::StatusIs;
using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::testing::HasSubstr;

// Evaluates cel expression:
// '"1" + "1" + ...'
static void BM_StrCatLocalArena(benchmark::State& state) {
  std::string expr("'1'");
  int len = state.range(0);
  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  for (int i = 0; i < len; i++) {
    expr = absl::Substitute("($0 + $0)", expr);
  }

  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(expr));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&parsed_expr.expr(),
                                                 &parsed_expr.source_info()));

  for (auto _ : state) {
    google::protobuf::Arena arena;
    Activation activation;
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    CelValue::StringHolder holder;
    ASSERT_TRUE(result.GetValue(&holder));
    ASSERT_EQ(holder.value().length(), 1 << len);
  }
}
BENCHMARK(BM_StrCatLocalArena)->DenseRange(0, 8, 2);

// Evaluates cel expression:
// '("1" + "1") + ...'
static void BM_StrCatSharedArena(benchmark::State& state) {
  google::protobuf::Arena arena;
  std::string expr("'1'");
  int len = state.range(0);
  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  for (int i = 0; i < len; i++) {
    expr = absl::Substitute("($0 + $0)", expr);
  }

  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(expr));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&parsed_expr.expr(),
                                                 &parsed_expr.source_info()));

  for (auto _ : state) {
    Activation activation;
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    CelValue::StringHolder holder;
    ASSERT_TRUE(result.GetValue(&holder));
    ASSERT_EQ(holder.value().length(), 1 << len);
  }
}

// Expression grows exponentially.
BENCHMARK(BM_StrCatSharedArena)->DenseRange(0, 8, 2);

// Series of simple expressions that are expected to require an allocation.
static void BM_AllocateString(benchmark::State& state) {
  google::protobuf::Arena arena;
  std::string expr("'1' + '1'");
  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(expr));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&parsed_expr.expr(),
                                                 &parsed_expr.source_info()));

  for (auto _ : state) {
    Activation activation;
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    CelValue::StringHolder holder;
    ASSERT_TRUE(result.GetValue(&holder));
    ASSERT_EQ(holder.value(), "11");
  }
}
BENCHMARK(BM_AllocateString);

static void BM_AllocateError(benchmark::State& state) {
  google::protobuf::Arena arena;
  std::string expr("1 / 0");
  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(expr));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&parsed_expr.expr(),
                                                 &parsed_expr.source_info()));

  for (auto _ : state) {
    Activation activation;
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    const CelError* value;
    ASSERT_TRUE(result.GetValue(&value));
    ASSERT_THAT(*value, StatusIs(absl::StatusCode::kInvalidArgument,
                                 HasSubstr("divide by zero")));
  }
}
BENCHMARK(BM_AllocateError);

static void BM_AllocateMap(benchmark::State& state) {
  google::protobuf::Arena arena;
  std::string expr("{1: 2, 3: 4}");
  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(expr));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&parsed_expr.expr(),
                                                 &parsed_expr.source_info()));

  for (auto _ : state) {
    Activation activation;
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsMap());
  }
}

BENCHMARK(BM_AllocateMap);

static void BM_AllocateMessage(benchmark::State& state) {
  google::protobuf::Arena arena;
  std::string expr(
      "google.api.expr.runtime.RequestContext{"
      "ip: '192.168.0.1',"
      "path: '/root'}");
  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(expr));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&parsed_expr.expr(),
                                                 &parsed_expr.source_info()));

  for (auto _ : state) {
    Activation activation;
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsMessage());
  }
}

BENCHMARK(BM_AllocateMessage);

static void BM_AllocateLargeMessage(benchmark::State& state) {
  // Make sure attribute context is loaded in the generated descriptor pool.
  rpc::context::AttributeContext context;
  static_cast<void>(context);

  google::protobuf::Arena arena;
  std::string expr(R"(
  google.rpc.context.AttributeContext{
      source: google.rpc.context.AttributeContext.Peer{
        ip: '192.168.0.1',
        port: 1025,
        labels: {"abc": "123", "def": "456"}
      },
      request: google.rpc.context.AttributeContext.Request{
        method: 'GET',
        path: 'root',
        host: 'www.example.com'
      },
      resource: google.rpc.context.AttributeContext.Resource{
        labels: {"abc": "123", "def": "456"},
      }
  })");
  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(expr));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&parsed_expr.expr(),
                                                 &parsed_expr.source_info()));

  for (auto _ : state) {
    Activation activation;
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsMessage());
  }
}

BENCHMARK(BM_AllocateLargeMessage);

static void BM_AllocateList(benchmark::State& state) {
  google::protobuf::Arena arena;
  std::string expr("[1, 2, 3, 4]");
  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(expr));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&parsed_expr.expr(),
                                                 &parsed_expr.source_info()));

  for (auto _ : state) {
    Activation activation;
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsList());
  }
}
BENCHMARK(BM_AllocateList);

}  // namespace
}  // namespace google::api::expr::runtime
