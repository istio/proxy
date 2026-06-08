#include "eval/eval/const_value_step.h"

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "base/type_provider.h"
#include "common/constant.h"
#include "common/expr.h"
#include "eval/eval/cel_expression_flat_impl.h"
#include "eval/eval/evaluator_core.h"
#include "eval/internal/errors.h"
#include "eval/public/activation.h"
#include "eval/public/cel_value.h"
#include "eval/public/testing/matchers.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "runtime/internal/runtime_env.h"
#include "runtime/internal/runtime_env_testing.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {

namespace {

using ::absl_testing::StatusIs;
using ::cel::Constant;
using ::cel::Expr;
using ::cel::TypeProvider;
using ::cel::runtime_internal::NewTestingRuntimeEnv;
using ::cel::runtime_internal::RuntimeEnv;
using ::testing::Eq;
using ::testing::HasSubstr;

absl::StatusOr<CelValue> RunConstantExpression(
    const absl_nonnull std::shared_ptr<const RuntimeEnv>& env, const Expr* expr,
    const Constant& const_expr, google::protobuf::Arena* arena) {
  CEL_ASSIGN_OR_RETURN(auto step,
                       CreateConstValueStep(const_expr, expr->id(), arena));

  google::api::expr::runtime::ExecutionPath path;
  path.push_back(std::move(step));

  CelExpressionFlatImpl impl(
      env, FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                          env->type_registry.GetComposedTypeProvider(),
                          cel::RuntimeOptions{}));

  google::api::expr::runtime::Activation activation;

  return impl.Evaluate(activation, arena);
}

class ConstValueStepTest : public ::testing::Test {
 public:
  ConstValueStepTest() : env_(NewTestingRuntimeEnv()) {}

 protected:
  absl_nonnull std::shared_ptr<const RuntimeEnv> env_;
  google::protobuf::Arena arena_;
};

TEST_F(ConstValueStepTest, TestEvaluationConstInt64) {
  Expr expr;
  auto& const_expr = expr.mutable_const_expr();
  const_expr.set_int64_value(1);

  auto status = RunConstantExpression(env_, &expr, const_expr, &arena_);

  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsInt64());
  EXPECT_THAT(value.Int64OrDie(), Eq(1));
}

TEST_F(ConstValueStepTest, TestEvaluationConstUint64) {
  Expr expr;
  auto& const_expr = expr.mutable_const_expr();
  const_expr.set_uint64_value(1);

  auto status = RunConstantExpression(env_, &expr, const_expr, &arena_);

  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsUint64());
  EXPECT_THAT(value.Uint64OrDie(), Eq(1));
}

TEST_F(ConstValueStepTest, TestEvaluationConstBool) {
  Expr expr;
  auto& const_expr = expr.mutable_const_expr();
  const_expr.set_bool_value(true);

  auto status = RunConstantExpression(env_, &expr, const_expr, &arena_);

  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.BoolOrDie(), Eq(true));
}

TEST_F(ConstValueStepTest, TestEvaluationConstNull) {
  Expr expr;
  auto& const_expr = expr.mutable_const_expr();
  const_expr.set_null_value(nullptr);

  auto status = RunConstantExpression(env_, &expr, const_expr, &arena_);

  ASSERT_OK(status);

  auto value = status.value();

  EXPECT_TRUE(value.IsNull());
}

TEST_F(ConstValueStepTest, TestEvaluationConstString) {
  Expr expr;
  auto& const_expr = expr.mutable_const_expr();
  const_expr.set_string_value("test");

  auto status = RunConstantExpression(env_, &expr, const_expr, &arena_);

  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsString());
  EXPECT_THAT(value.StringOrDie().value(), Eq("test"));
}

TEST_F(ConstValueStepTest, TestEvaluationConstDouble) {
  Expr expr;
  auto& const_expr = expr.mutable_const_expr();
  const_expr.set_double_value(1.0);

  auto status = RunConstantExpression(env_, &expr, const_expr, &arena_);

  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsDouble());
  EXPECT_THAT(value.DoubleOrDie(), testing::DoubleEq(1.0));
}

// Test Bytes constant
// For now, bytes are equivalent to string.
TEST_F(ConstValueStepTest, TestEvaluationConstBytes) {
  Expr expr;
  auto& const_expr = expr.mutable_const_expr();
  const_expr.set_bytes_value("test");

  auto status = RunConstantExpression(env_, &expr, const_expr, &arena_);

  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsBytes());
  EXPECT_THAT(value.BytesOrDie().value(), Eq("test"));
}

TEST_F(ConstValueStepTest, TestEvaluationConstDuration) {
  Expr expr;
  auto& const_expr = expr.mutable_const_expr();
  const_expr.set_duration_value(absl::Seconds(5) + absl::Nanoseconds(2000));

  auto status = RunConstantExpression(env_, &expr, const_expr, &arena_);

  ASSERT_OK(status);

  auto value = status.value();

  EXPECT_THAT(value,
              test::IsCelDuration(absl::Seconds(5) + absl::Nanoseconds(2000)));
}

TEST_F(ConstValueStepTest, TestEvaluationConstDurationOutOfRange) {
  Expr expr;
  auto& const_expr = expr.mutable_const_expr();
  const_expr.set_duration_value(cel::runtime_internal::kDurationHigh);

  auto status = RunConstantExpression(env_, &expr, const_expr, &arena_);

  ASSERT_OK(status);

  auto value = status.value();

  EXPECT_THAT(value,
              test::IsCelError(StatusIs(absl::StatusCode::kInvalidArgument,
                                        HasSubstr("out of range"))));
}

TEST_F(ConstValueStepTest, TestEvaluationConstTimestamp) {
  Expr expr;
  auto& const_expr = expr.mutable_const_expr();
  const_expr.set_time_value(absl::FromUnixSeconds(3600) +
                            absl::Nanoseconds(1000));

  auto status = RunConstantExpression(env_, &expr, const_expr, &arena_);

  ASSERT_OK(status);

  auto value = status.value();

  EXPECT_THAT(value, test::IsCelTimestamp(absl::FromUnixSeconds(3600) +
                                          absl::Nanoseconds(1000)));
}

}  // namespace

}  // namespace google::api::expr::runtime
