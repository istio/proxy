#include "eval/eval/shadowable_value_step.h"

#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "base/type_provider.h"
#include "common/value.h"
#include "eval/eval/cel_expression_flat_impl.h"
#include "eval/eval/evaluator_core.h"
#include "eval/internal/interop.h"
#include "eval/public/activation.h"
#include "eval/public/cel_value.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::TypeProvider;
using ::cel::interop_internal::CreateTypeValueFromView;
using ::google::protobuf::Arena;
using ::testing::Eq;

absl::StatusOr<CelValue> RunShadowableExpression(std::string identifier,
                                                 cel::Value value,
                                                 const Activation& activation,
                                                 Arena* arena) {
  CEL_ASSIGN_OR_RETURN(
      auto step,
      CreateShadowableValueStep(std::move(identifier), std::move(value), 1));
  ExecutionPath path;
  path.push_back(std::move(step));

  CelExpressionFlatImpl impl(
      FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                     TypeProvider::Builtin(), cel::RuntimeOptions{}));
  return impl.Evaluate(activation, arena);
}

TEST(ShadowableValueStepTest, TestEvaluateNoShadowing) {
  std::string type_name = "google.api.expr.runtime.TestMessage";

  Activation activation;
  Arena arena;

  auto type_value = CreateTypeValueFromView(&arena, type_name);
  auto status =
      RunShadowableExpression(type_name, type_value, activation, &arena);
  ASSERT_OK(status);

  auto value = status.value();
  ASSERT_TRUE(value.IsCelType());
  EXPECT_THAT(value.CelTypeOrDie().value(), Eq(type_name));
}

TEST(ShadowableValueStepTest, TestEvaluateShadowedIdentifier) {
  std::string type_name = "int";
  auto shadow_value = CelValue::CreateInt64(1024L);

  Activation activation;
  activation.InsertValue(type_name, shadow_value);
  Arena arena;

  auto type_value = CreateTypeValueFromView(&arena, type_name);
  auto status =
      RunShadowableExpression(type_name, type_value, activation, &arena);
  ASSERT_OK(status);

  auto value = status.value();
  ASSERT_TRUE(value.IsInt64());
  EXPECT_THAT(value.Int64OrDie(), Eq(1024L));
}

}  // namespace

}  // namespace google::api::expr::runtime
