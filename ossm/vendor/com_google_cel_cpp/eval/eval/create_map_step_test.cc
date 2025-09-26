// Copyright 2024 Google LLC
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

#include "eval/eval/create_map_step.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "base/type_provider.h"
#include "common/expr.h"
#include "eval/eval/cel_expression_flat_impl.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/ident_step.h"
#include "eval/public/activation.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_set.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "runtime/internal/runtime_env.h"
#include "runtime/internal/runtime_env_testing.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {

namespace {

using ::absl_testing::StatusIs;
using ::cel::Expr;
using ::cel::TypeProvider;
using ::cel::runtime_internal::NewTestingRuntimeEnv;
using ::cel::runtime_internal::RuntimeEnv;
using ::google::protobuf::Arena;

absl::StatusOr<ExecutionPath> CreateStackMachineProgram(
    const std::vector<std::pair<CelValue, CelValue>>& values,
    Activation& activation) {
  ExecutionPath path;

  Expr expr1;
  Expr expr0;

  std::vector<Expr> exprs;
  exprs.reserve(values.size() * 2);
  int index = 0;

  auto& create_struct = expr1.mutable_struct_expr();
  for (const auto& item : values) {
    std::string key_name = absl::StrCat("key", index);
    std::string value_name = absl::StrCat("value", index);

    auto& key_expr = exprs.emplace_back();
    auto& key_ident = key_expr.mutable_ident_expr();
    key_ident.set_name(key_name);
    CEL_ASSIGN_OR_RETURN(auto step_key,
                         CreateIdentStep(key_ident, exprs.back().id()));

    auto& value_expr = exprs.emplace_back();
    auto& value_ident = value_expr.mutable_ident_expr();
    value_ident.set_name(value_name);
    CEL_ASSIGN_OR_RETURN(auto step_value,
                         CreateIdentStep(value_ident, exprs.back().id()));

    path.push_back(std::move(step_key));
    path.push_back(std::move(step_value));

    activation.InsertValue(key_name, item.first);
    activation.InsertValue(value_name, item.second);

    create_struct.mutable_fields().emplace_back();
    index++;
  }

  CEL_ASSIGN_OR_RETURN(
      auto step1, CreateCreateStructStepForMap(values.size(), {}, expr1.id()));
  path.push_back(std::move(step1));
  return path;
}

absl::StatusOr<ExecutionPath> CreateRecursiveProgram(
    const std::vector<std::pair<CelValue, CelValue>>& values,
    Activation& activation) {
  ExecutionPath path;

  int index = 0;
  std::vector<std::unique_ptr<DirectExpressionStep>> deps;
  for (const auto& item : values) {
    std::string key_name = absl::StrCat("key", index);
    std::string value_name = absl::StrCat("value", index);

    deps.push_back(CreateDirectIdentStep(key_name, -1));

    deps.push_back(CreateDirectIdentStep(value_name, -1));

    activation.InsertValue(key_name, item.first);
    activation.InsertValue(value_name, item.second);

    index++;
  }
  path.push_back(std::make_unique<WrappedDirectStep>(
      CreateDirectCreateMapStep(std::move(deps), {}, -1), -1));

  return path;
}

// Helper method. Creates simple pipeline containing CreateStruct step that
// builds Map and runs it.
// Equivalent to {key0: value0, ...}
absl::StatusOr<CelValue> RunCreateMapExpression(
    const absl_nonnull std::shared_ptr<const RuntimeEnv>& env,
    const std::vector<std::pair<CelValue, CelValue>>& values,
    google::protobuf::Arena* arena, bool enable_unknowns, bool enable_recursive_program) {
  Activation activation;

  ExecutionPath path;
  if (enable_recursive_program) {
    CEL_ASSIGN_OR_RETURN(path, CreateRecursiveProgram(values, activation));
  } else {
    CEL_ASSIGN_OR_RETURN(path, CreateStackMachineProgram(values, activation));
  }
  cel::RuntimeOptions options;
  if (enable_unknowns) {
    options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  }

  CelExpressionFlatImpl cel_expr(
      env,
      FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                     env->type_registry.GetComposedTypeProvider(), options));
  return cel_expr.Evaluate(activation, arena);
}

class CreateMapStepTest
    : public testing::TestWithParam<std::tuple<bool, bool>> {
 public:
  CreateMapStepTest() : env_(NewTestingRuntimeEnv()) {}

  bool enable_unknowns() { return std::get<0>(GetParam()); }
  bool enable_recursive_program() { return std::get<1>(GetParam()); }

  absl::StatusOr<CelValue> RunMapExpression(
      const std::vector<std::pair<CelValue, CelValue>>& values) {
    return RunCreateMapExpression(env_, values, &arena_, enable_unknowns(),
                                  enable_recursive_program());
  }

 protected:
  absl_nonnull std::shared_ptr<const RuntimeEnv> env_;
  google::protobuf::Arena arena_;
};

// Test that Empty Map is created successfully.
TEST_P(CreateMapStepTest, TestCreateEmptyMap) {
  ASSERT_OK_AND_ASSIGN(CelValue result, RunMapExpression({}));
  ASSERT_TRUE(result.IsMap());

  const CelMap* cel_map = result.MapOrDie();
  ASSERT_EQ(cel_map->size(), 0);
}

// Test message creation if unknown argument is passed
TEST(CreateMapStepTest, TestMapCreateWithUnknown) {
  absl_nonnull std::shared_ptr<const RuntimeEnv> env = NewTestingRuntimeEnv();
  Arena arena;
  UnknownSet unknown_set;
  std::vector<std::pair<CelValue, CelValue>> entries;

  std::vector<std::string> kKeys = {"test2", "test1"};

  entries.push_back(
      {CelValue::CreateString(&kKeys[0]), CelValue::CreateInt64(2)});
  entries.push_back({CelValue::CreateString(&kKeys[1]),
                     CelValue::CreateUnknownSet(&unknown_set)});

  ASSERT_OK_AND_ASSIGN(CelValue result, RunCreateMapExpression(
                                            env, entries, &arena, true, false));
  ASSERT_TRUE(result.IsUnknownSet());
}

TEST(CreateMapStepTest, TestMapCreateWithError) {
  absl_nonnull std::shared_ptr<const RuntimeEnv> env = NewTestingRuntimeEnv();
  Arena arena;
  UnknownSet unknown_set;
  absl::Status error = absl::CancelledError();
  std::vector<std::pair<CelValue, CelValue>> entries;
  entries.push_back({CelValue::CreateStringView("foo"),
                     CelValue::CreateUnknownSet(&unknown_set)});
  entries.push_back(
      {CelValue::CreateStringView("bar"), CelValue::CreateError(&error)});

  ASSERT_OK_AND_ASSIGN(CelValue result, RunCreateMapExpression(
                                            env, entries, &arena, true, false));
  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(*result.ErrorOrDie(), StatusIs(absl::StatusCode::kCancelled));
}

TEST(CreateMapStepTest, TestMapCreateWithErrorRecursiveProgram) {
  absl_nonnull std::shared_ptr<const RuntimeEnv> env = NewTestingRuntimeEnv();
  Arena arena;
  UnknownSet unknown_set;
  absl::Status error = absl::CancelledError();
  std::vector<std::pair<CelValue, CelValue>> entries;
  entries.push_back({CelValue::CreateStringView("foo"),
                     CelValue::CreateUnknownSet(&unknown_set)});
  entries.push_back(
      {CelValue::CreateStringView("bar"), CelValue::CreateError(&error)});

  ASSERT_OK_AND_ASSIGN(CelValue result, RunCreateMapExpression(
                                            env, entries, &arena, true, true));
  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(*result.ErrorOrDie(), StatusIs(absl::StatusCode::kCancelled));
}

TEST(CreateMapStepTest, TestMapCreateWithUnknownRecursiveProgram) {
  absl_nonnull std::shared_ptr<const RuntimeEnv> env = NewTestingRuntimeEnv();
  Arena arena;
  UnknownSet unknown_set;
  std::vector<std::pair<CelValue, CelValue>> entries;

  std::vector<std::string> kKeys = {"test2", "test1"};

  entries.push_back(
      {CelValue::CreateString(&kKeys[0]), CelValue::CreateInt64(2)});
  entries.push_back({CelValue::CreateString(&kKeys[1]),
                     CelValue::CreateUnknownSet(&unknown_set)});

  ASSERT_OK_AND_ASSIGN(CelValue result, RunCreateMapExpression(
                                            env, entries, &arena, true, true));
  ASSERT_TRUE(result.IsUnknownSet());
}

// Test that String Map is created successfully.
TEST_P(CreateMapStepTest, TestCreateStringMap) {
  Arena arena;

  std::vector<std::pair<CelValue, CelValue>> entries;

  std::vector<std::string> kKeys = {"test2", "test1"};

  entries.push_back(
      {CelValue::CreateString(&kKeys[0]), CelValue::CreateInt64(2)});
  entries.push_back(
      {CelValue::CreateString(&kKeys[1]), CelValue::CreateInt64(1)});

  ASSERT_OK_AND_ASSIGN(CelValue result, RunMapExpression(entries));
  ASSERT_TRUE(result.IsMap());

  const CelMap* cel_map = result.MapOrDie();
  ASSERT_EQ(cel_map->size(), 2);

  auto lookup0 = cel_map->Get(&arena, CelValue::CreateString(&kKeys[0]));
  ASSERT_TRUE(lookup0.has_value());
  ASSERT_TRUE(lookup0->IsInt64()) << lookup0->DebugString();
  EXPECT_EQ(lookup0->Int64OrDie(), 2);

  auto lookup1 = cel_map->Get(&arena, CelValue::CreateString(&kKeys[1]));
  ASSERT_TRUE(lookup1.has_value());
  ASSERT_TRUE(lookup1->IsInt64());
  EXPECT_EQ(lookup1->Int64OrDie(), 1);
}

INSTANTIATE_TEST_SUITE_P(CreateMapStep, CreateMapStepTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace

}  // namespace google::api::expr::runtime
