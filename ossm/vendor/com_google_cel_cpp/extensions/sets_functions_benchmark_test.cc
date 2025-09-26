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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "common/value.h"
#include "eval/internal/interop.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "extensions/sets_functions.h"
#include "internal/benchmark.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {
namespace {

using ::cel::Value;
using ::cel::expr::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::google::api::expr::runtime::Activation;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::ContainerBackedListImpl;
using ::google::api::expr::runtime::CreateCelExpressionBuilder;
using ::google::api::expr::runtime::InterpreterOptions;
using ::google::api::expr::runtime::RegisterBuiltinFunctions;

enum class ListImpl : int { kLegacy = 0, kWrappedModern = 1, kRhsConstant = 2 };
int ToNumber(ListImpl impl) { return static_cast<int>(impl); }
ListImpl FromNumber(int number) {
  switch (number) {
    case 0:
      return ListImpl::kLegacy;
    case 1:
      return ListImpl::kWrappedModern;
    case 2:
      return ListImpl::kRhsConstant;
    default:
      return ListImpl::kLegacy;
  }
}

struct TestCase {
  std::string test_name;
  std::string expr;
  ListImpl list_impl;
  int size;
  CelValue result;

  std::string MakeLabel(int len) const {
    std::string list_impl;
    switch (this->list_impl) {
      case ListImpl::kRhsConstant:
        list_impl = "rhs_constant";
        break;
      case ListImpl::kWrappedModern:
        list_impl = "wrapped_modern";
        break;
      case ListImpl::kLegacy:
        list_impl = "legacy";
        break;
    }

    return absl::StrCat(test_name, "/", list_impl, "/", len);
  }
};

class ListStorage {
 public:
  virtual ~ListStorage() = default;
};

class LegacyListStorage : public ListStorage {
 public:
  LegacyListStorage(ContainerBackedListImpl x, ContainerBackedListImpl y)
      : x_(std::move(x)), y_(std::move(y)) {}

  CelValue x() { return CelValue::CreateList(&x_); }
  CelValue y() { return CelValue::CreateList(&y_); }

 private:
  ContainerBackedListImpl x_;
  ContainerBackedListImpl y_;
};

class ModernListStorage : public ListStorage {
 public:
  ModernListStorage(Value x, Value y) : x_(std::move(x)), y_(std::move(y)) {}

  CelValue x() {
    return interop_internal::ModernValueToLegacyValueOrDie(&arena_, x_);
  }
  CelValue y() {
    return interop_internal::ModernValueToLegacyValueOrDie(&arena_, y_);
  }

 private:
  google::protobuf::Arena arena_;
  Value x_;
  Value y_;
};

absl::StatusOr<std::unique_ptr<ListStorage>> RegisterLegacyLists(
    bool overlap, int len, Activation& activation) {
  std::vector<CelValue> x;
  std::vector<CelValue> y;
  x.reserve(len + 1);
  y.reserve(len + 1);
  if (overlap) {
    x.push_back(CelValue::CreateInt64(2));
    y.push_back(CelValue::CreateInt64(1));
  }

  for (int i = 0; i < len; i++) {
    x.push_back(CelValue::CreateInt64(1));
    y.push_back(CelValue::CreateInt64(2));
  }

  auto result = std::make_unique<LegacyListStorage>(
      ContainerBackedListImpl(std::move(x)),
      ContainerBackedListImpl(std::move(y)));

  activation.InsertValue("x", result->x());
  activation.InsertValue("y", result->y());
  return result;
}

// Constant list literal that has the same elements as the bound test cases.
std::string ConstantList(bool overlap, int len) {
  std::string list_body;
  for (int i = 0; i < len; i++) {
  }
  return absl::StrCat("[", overlap ? "1, " : "",
                      absl::StrJoin(std::vector<std::string>(len, "2"), ", "),
                      "]");
}

absl::StatusOr<std::unique_ptr<ListStorage>> RegisterModernLists(
    bool overlap, int len, google::protobuf::Arena* absl_nonnull arena,
    Activation& activation) {
  auto x_builder = cel::NewListValueBuilder(arena);
  auto y_builder = cel::NewListValueBuilder(arena);

  x_builder->Reserve(len + 1);
  y_builder->Reserve(len + 1);

  if (overlap) {
    CEL_RETURN_IF_ERROR(x_builder->Add(cel::IntValue(2)));
    CEL_RETURN_IF_ERROR(y_builder->Add(cel::IntValue(1)));
  }

  for (int i = 0; i < len; i++) {
    CEL_RETURN_IF_ERROR(x_builder->Add(cel::IntValue(1)));
    CEL_RETURN_IF_ERROR(y_builder->Add(cel::IntValue(2)));
  }

  auto x = std::move(*x_builder).Build();
  auto y = std::move(*y_builder).Build();
  auto result = std::make_unique<ModernListStorage>(std::move(x), std::move(y));
  activation.InsertValue("x", result->x());
  activation.InsertValue("y", result->y());

  return result;
}

absl::StatusOr<std::unique_ptr<ListStorage>> RegisterLists(
    bool overlap, int len, bool use_modern, google::protobuf::Arena* absl_nonnull arena,
    Activation& activation) {
  if (use_modern) {
    return RegisterModernLists(overlap, len, arena, activation);
  } else {
    return RegisterLegacyLists(overlap, len, activation);
  }
}

void RunBenchmark(const TestCase& test_case, benchmark::State& state) {
  bool lists_overlap = test_case.result.BoolOrDie();

  std::string expr = test_case.expr;
  if (test_case.list_impl == ListImpl::kRhsConstant) {
    expr = absl::StrReplaceAll(
        expr, {{"y", ConstantList(lists_overlap, test_case.size)}});
  }
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(expr));

  google::protobuf::Arena arena;

  InterpreterOptions options;
  options.constant_folding = true;
  options.constant_arena = &arena;
  options.enable_qualified_identifier_rewrites = true;
  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));
  ASSERT_OK(RegisterSetsFunctions(builder->GetRegistry()->InternalGetRegistry(),
                                  cel::RuntimeOptions{}));
  ASSERT_OK_AND_ASSIGN(
      auto cel_expr, builder->CreateExpression(&(parsed_expr.expr()), nullptr));

  Activation activation;
  ASSERT_OK_AND_ASSIGN(
      auto storage,
      RegisterLists(test_case.result.BoolOrDie(), test_case.size,
                    test_case.list_impl == ListImpl::kWrappedModern, &arena,
                    activation));

  state.SetLabel(test_case.MakeLabel(test_case.size));
  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    ASSERT_EQ(result.BoolOrDie(), test_case.result.BoolOrDie())
        << test_case.test_name;
  }
}

void BM_SetsIntersectsTrue(benchmark::State& state) {
  ListImpl impl = FromNumber(state.range(0));
  int size = state.range(1);

  RunBenchmark({"sets.intersects_true", "sets.intersects(x, y)", impl, size,
                CelValue::CreateBool(true)},
               state);
}

void BM_SetsIntersectsFalse(benchmark::State& state) {
  ListImpl impl = FromNumber(state.range(0));
  int size = state.range(1);

  RunBenchmark({"sets.intersects_false", "sets.intersects(x, y)", impl, size,
                CelValue::CreateBool(false)},
               state);
}

void BM_SetsIntersectsComprehensionTrue(benchmark::State& state) {
  ListImpl impl = FromNumber(state.range(0));
  int size = state.range(1);

  RunBenchmark({"comprehension_intersects_true", "x.exists(i, i in y)", impl,
                size, CelValue::CreateBool(true)},
               state);
}

void BM_SetsIntersectsComprehensionFalse(benchmark::State& state) {
  ListImpl impl = FromNumber(state.range(0));
  int size = state.range(1);

  RunBenchmark({"comprehension_intersects_false", "x.exists(i, i in y)", impl,
                size, CelValue::CreateBool(false)},
               state);
}

void BM_SetsEquivalentTrue(benchmark::State& state) {
  ListImpl impl = FromNumber(state.range(0));
  int size = state.range(1);

  RunBenchmark({"sets.equivalent_true", "sets.equivalent(x, y)", impl, size,
                CelValue::CreateBool(true)},
               state);
}

void BM_SetsEquivalentFalse(benchmark::State& state) {
  ListImpl impl = FromNumber(state.range(0));
  int size = state.range(1);

  RunBenchmark({"sets.equivalent_false", "sets.equivalent(x, y)", impl, size,
                CelValue::CreateBool(false)},
               state);
}

void BM_SetsEquivalentComprehensionTrue(benchmark::State& state) {
  ListImpl impl = FromNumber(state.range(0));
  int size = state.range(1);

  RunBenchmark(
      {"comprehension_equivalent_true", "x.all(i, i in y) && y.all(j, j in x)",
       impl, size, CelValue::CreateBool(true)},
      state);
}

void BM_SetsEquivalentComprehensionFalse(benchmark::State& state) {
  ListImpl impl = FromNumber(state.range(0));
  int size = state.range(1);

  RunBenchmark(
      {"comprehension_equivalent_false", "x.all(i, i in y) && y.all(j, j in x)",
       impl, size, CelValue::CreateBool(false)},
      state);
}

template <typename Benchmark>
void BenchArgs(Benchmark* bench) {
  for (ListImpl impl :
       {ListImpl::kLegacy, ListImpl::kWrappedModern, ListImpl::kRhsConstant}) {
    for (int size : {1, 8, 32, 64, 256}) {
      bench->ArgPair(ToNumber(impl), size);
    }
  }
}

BENCHMARK(BM_SetsIntersectsComprehensionTrue)->Apply(BenchArgs);
BENCHMARK(BM_SetsIntersectsComprehensionFalse)->Apply(BenchArgs);
BENCHMARK(BM_SetsIntersectsTrue)->Apply(BenchArgs);
BENCHMARK(BM_SetsIntersectsFalse)->Apply(BenchArgs);

BENCHMARK(BM_SetsEquivalentComprehensionTrue)->Apply(BenchArgs);
BENCHMARK(BM_SetsEquivalentComprehensionFalse)->Apply(BenchArgs);
BENCHMARK(BM_SetsEquivalentTrue)->Apply(BenchArgs);
BENCHMARK(BM_SetsEquivalentFalse)->Apply(BenchArgs);

}  // namespace
}  // namespace cel::extensions
