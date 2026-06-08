#include "eval/eval/logic_step.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/attribute.h"
#include "base/attribute_set.h"
#include "base/type_provider.h"
#include "common/casting.h"
#include "common/expr.h"
#include "common/unknown.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/cel_expression_flat_impl.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/ident_step.h"
#include "eval/public/activation.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_set.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "runtime/activation.h"
#include "runtime/internal/runtime_env.h"
#include "runtime/internal/runtime_env_testing.h"
#include "runtime/internal/runtime_type_provider.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {

namespace {

using ::absl_testing::IsOk;
using ::cel::Attribute;
using ::cel::AttributeSet;
using ::cel::BoolValue;
using ::cel::Cast;
using ::cel::Expr;
using ::cel::InstanceOf;
using ::cel::IntValue;
using ::cel::TypeProvider;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::runtime_internal::NewTestingRuntimeEnv;
using ::cel::runtime_internal::RuntimeEnv;
using ::google::protobuf::Arena;
using ::testing::Eq;

class LogicStepTest : public testing::TestWithParam<bool> {
 public:
  LogicStepTest() : env_(NewTestingRuntimeEnv()) {}

  absl::Status EvaluateLogic(CelValue arg0, CelValue arg1, bool is_or,
                             CelValue* result, bool enable_unknown) {
    Expr expr0;
    auto& ident_expr0 = expr0.mutable_ident_expr();
    ident_expr0.set_name("name0");

    Expr expr1;
    auto& ident_expr1 = expr1.mutable_ident_expr();
    ident_expr1.set_name("name1");

    ExecutionPath path;
    CEL_ASSIGN_OR_RETURN(auto step, CreateIdentStep(ident_expr0, expr0.id()));
    path.push_back(std::move(step));

    CEL_ASSIGN_OR_RETURN(step, CreateIdentStep(ident_expr1, expr1.id()));
    path.push_back(std::move(step));

    CEL_ASSIGN_OR_RETURN(step, (is_or) ? CreateOrStep(2) : CreateAndStep(2));
    path.push_back(std::move(step));

    auto dummy_expr = std::make_unique<Expr>();
    cel::RuntimeOptions options;
    if (enable_unknown) {
      options.unknown_processing =
          cel::UnknownProcessingOptions::kAttributeOnly;
    }
    CelExpressionFlatImpl impl(
        env_,
        FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                       env_->type_registry.GetComposedTypeProvider(), options));

    Activation activation;
    activation.InsertValue("name0", arg0);
    activation.InsertValue("name1", arg1);
    CEL_ASSIGN_OR_RETURN(CelValue value, impl.Evaluate(activation, &arena_));
    *result = value;
    return absl::OkStatus();
  }

 private:
  absl_nonnull std::shared_ptr<const RuntimeEnv> env_;
  Arena arena_;
};

TEST_P(LogicStepTest, TestAndLogic) {
  CelValue result;
  absl::Status status =
      EvaluateLogic(CelValue::CreateBool(true), CelValue::CreateBool(true),
                    false, &result, GetParam());
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status =
      EvaluateLogic(CelValue::CreateBool(true), CelValue::CreateBool(false),
                    false, &result, GetParam());
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());

  status =
      EvaluateLogic(CelValue::CreateBool(false), CelValue::CreateBool(true),
                    false, &result, GetParam());
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());

  status =
      EvaluateLogic(CelValue::CreateBool(false), CelValue::CreateBool(false),
                    false, &result, GetParam());
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());
}

TEST_P(LogicStepTest, TestOrLogic) {
  CelValue result;
  absl::Status status =
      EvaluateLogic(CelValue::CreateBool(true), CelValue::CreateBool(true),
                    true, &result, GetParam());
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status =
      EvaluateLogic(CelValue::CreateBool(true), CelValue::CreateBool(false),
                    true, &result, GetParam());
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status = EvaluateLogic(CelValue::CreateBool(false),
                         CelValue::CreateBool(true), true, &result, GetParam());
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status =
      EvaluateLogic(CelValue::CreateBool(false), CelValue::CreateBool(false),
                    true, &result, GetParam());
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());
}

TEST_P(LogicStepTest, TestAndLogicErrorHandling) {
  CelValue result;
  CelError error = absl::CancelledError();
  CelValue error_value = CelValue::CreateError(&error);
  absl::Status status = EvaluateLogic(error_value, CelValue::CreateBool(true),
                                      false, &result, GetParam());
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsError());

  status = EvaluateLogic(CelValue::CreateBool(true), error_value, false,
                         &result, GetParam());
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsError());

  status = EvaluateLogic(CelValue::CreateBool(false), error_value, false,
                         &result, GetParam());
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());

  status = EvaluateLogic(error_value, CelValue::CreateBool(false), false,
                         &result, GetParam());
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());
}

TEST_P(LogicStepTest, TestOrLogicErrorHandling) {
  CelValue result;
  CelError error = absl::CancelledError();
  CelValue error_value = CelValue::CreateError(&error);
  absl::Status status = EvaluateLogic(error_value, CelValue::CreateBool(false),
                                      true, &result, GetParam());
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsError());

  status = EvaluateLogic(CelValue::CreateBool(false), error_value, true,
                         &result, GetParam());
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsError());

  status = EvaluateLogic(CelValue::CreateBool(true), error_value, true, &result,
                         GetParam());
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status = EvaluateLogic(error_value, CelValue::CreateBool(true), true, &result,
                         GetParam());
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());
}

TEST_F(LogicStepTest, TestAndLogicUnknownHandling) {
  CelValue result;
  UnknownSet unknown_set;
  CelError cel_error = absl::CancelledError();
  CelValue unknown_value = CelValue::CreateUnknownSet(&unknown_set);
  CelValue error_value = CelValue::CreateError(&cel_error);
  absl::Status status = EvaluateLogic(unknown_value, CelValue::CreateBool(true),
                                      false, &result, true);
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(CelValue::CreateBool(true), unknown_value, false,
                         &result, true);
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(CelValue::CreateBool(false), unknown_value, false,
                         &result, true);
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());

  status = EvaluateLogic(unknown_value, CelValue::CreateBool(false), false,
                         &result, true);
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());

  status = EvaluateLogic(error_value, unknown_value, false, &result, true);
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(unknown_value, error_value, false, &result, true);
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsUnknownSet());

  Expr expr0;
  auto& ident_expr0 = expr0.mutable_ident_expr();
  ident_expr0.set_name("name0");

  Expr expr1;
  auto& ident_expr1 = expr1.mutable_ident_expr();
  ident_expr1.set_name("name1");

  CelAttribute attr0(expr0.ident_expr().name(), {}),
      attr1(expr1.ident_expr().name(), {});
  UnknownAttributeSet unknown_attr_set0({attr0});
  UnknownAttributeSet unknown_attr_set1({attr1});
  UnknownSet unknown_set0(unknown_attr_set0);
  UnknownSet unknown_set1(unknown_attr_set1);

  EXPECT_THAT(unknown_attr_set0.size(), Eq(1));
  EXPECT_THAT(unknown_attr_set1.size(), Eq(1));

  status = EvaluateLogic(CelValue::CreateUnknownSet(&unknown_set0),
                         CelValue::CreateUnknownSet(&unknown_set1), false,
                         &result, true);
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsUnknownSet());
  ASSERT_THAT(result.UnknownSetOrDie()->unknown_attributes().size(), Eq(2));
}

TEST_F(LogicStepTest, TestOrLogicUnknownHandling) {
  CelValue result;
  UnknownSet unknown_set;
  CelError cel_error = absl::CancelledError();
  CelValue unknown_value = CelValue::CreateUnknownSet(&unknown_set);
  CelValue error_value = CelValue::CreateError(&cel_error);
  absl::Status status = EvaluateLogic(
      unknown_value, CelValue::CreateBool(false), true, &result, true);
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(CelValue::CreateBool(false), unknown_value, true,
                         &result, true);
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(CelValue::CreateBool(true), unknown_value, true,
                         &result, true);
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status = EvaluateLogic(unknown_value, CelValue::CreateBool(true), true,
                         &result, true);
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status = EvaluateLogic(unknown_value, error_value, true, &result, true);
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(error_value, unknown_value, true, &result, true);
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsUnknownSet());

  Expr expr0;
  auto& ident_expr0 = expr0.mutable_ident_expr();
  ident_expr0.set_name("name0");

  Expr expr1;
  auto& ident_expr1 = expr1.mutable_ident_expr();
  ident_expr1.set_name("name1");

  CelAttribute attr0(expr0.ident_expr().name(), {}),
      attr1(expr1.ident_expr().name(), {});
  UnknownAttributeSet unknown_attr_set0({attr0});
  UnknownAttributeSet unknown_attr_set1({attr1});

  UnknownSet unknown_set0(unknown_attr_set0);
  UnknownSet unknown_set1(unknown_attr_set1);

  EXPECT_THAT(unknown_attr_set0.size(), Eq(1));
  EXPECT_THAT(unknown_attr_set1.size(), Eq(1));

  status = EvaluateLogic(CelValue::CreateUnknownSet(&unknown_set0),
                         CelValue::CreateUnknownSet(&unknown_set1), true,
                         &result, true);
  ASSERT_THAT(status, IsOk());
  ASSERT_TRUE(result.IsUnknownSet());
  ASSERT_THAT(result.UnknownSetOrDie()->unknown_attributes().size(), Eq(2));
}

INSTANTIATE_TEST_SUITE_P(LogicStepTest, LogicStepTest, testing::Bool());

enum class BinaryOp { kAnd, kOr };
enum class UnaryOp { kNot, kNotStrictlyFalse };

enum class OpArg {
  kTrue,
  kFalse,
  kUnknown,
  kError,
  // Arbitrary incorrect type
  kInt
};

enum class OpResult {
  kTrue,
  kFalse,
  kUnknown,
  kError,
};

struct BinaryTestCase {
  std::string name;
  BinaryOp op;
  OpArg arg0;
  OpArg arg1;
  OpResult result;
};

UnknownValue MakeUnknownValue(std::string attr) {
  std::vector<Attribute> attrs;
  attrs.push_back(Attribute(std::move(attr)));
  return cel::UnknownValue(cel::Unknown(AttributeSet(attrs)));
}

std::unique_ptr<DirectExpressionStep> MakeArgStep(OpArg arg,
                                                  absl::string_view name) {
  switch (arg) {
    case OpArg::kTrue:
      return CreateConstValueDirectStep(BoolValue(true));
    case OpArg::kFalse:
      return CreateConstValueDirectStep(BoolValue(false));
    case OpArg::kUnknown:
      return CreateConstValueDirectStep(MakeUnknownValue(std::string(name)));
    case OpArg::kError:
      return CreateConstValueDirectStep(
          cel::ErrorValue(absl::InternalError(name)));
    case OpArg::kInt:
      return CreateConstValueDirectStep(IntValue(42));
  }
};

class DirectBinaryLogicStepTest
    : public testing::TestWithParam<std::tuple<bool, BinaryTestCase>> {
 public:
  DirectBinaryLogicStepTest() = default;

  bool ShortcircuitingEnabled() { return std::get<0>(GetParam()); }
  const BinaryTestCase& GetTestCase() { return std::get<1>(GetParam()); }

 protected:
  Arena arena_;
};

TEST_P(DirectBinaryLogicStepTest, TestCases) {
  const BinaryTestCase& test_case = GetTestCase();

  std::unique_ptr<DirectExpressionStep> lhs =
      MakeArgStep(test_case.arg0, "lhs");
  std::unique_ptr<DirectExpressionStep> rhs =
      MakeArgStep(test_case.arg1, "rhs");

  std::unique_ptr<DirectExpressionStep> op =
      (test_case.op == BinaryOp::kAnd)
          ? CreateDirectAndStep(std::move(lhs), std::move(rhs), -1,
                                ShortcircuitingEnabled())
          : CreateDirectOrStep(std::move(lhs), std::move(rhs), -1,
                               ShortcircuitingEnabled());

  cel::Activation activation;
  cel::RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());
  ExecutionFrameBase frame(activation, options, type_provider,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value value;
  AttributeTrail attr;
  ASSERT_THAT(op->Evaluate(frame, value, attr), IsOk());

  switch (test_case.result) {
    case OpResult::kTrue:
      ASSERT_TRUE(value.IsBool());
      EXPECT_TRUE(value.GetBool().NativeValue());
      break;
    case OpResult::kFalse:
      ASSERT_TRUE(value.IsBool());
      EXPECT_FALSE(value.GetBool().NativeValue());
      break;
    case OpResult::kUnknown:
      EXPECT_TRUE(value.IsUnknown());
      break;
    case OpResult::kError:
      EXPECT_TRUE(value.IsError());
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    DirectBinaryLogicStepTest, DirectBinaryLogicStepTest,
    testing::Combine(testing::Bool(),
                     testing::ValuesIn<std::vector<BinaryTestCase>>({
                         {
                             "AndFalseFalse",
                             BinaryOp::kAnd,
                             OpArg::kFalse,
                             OpArg::kFalse,
                             OpResult::kFalse,
                         },
                         {
                             "AndFalseTrue",
                             BinaryOp::kAnd,
                             OpArg::kFalse,
                             OpArg::kTrue,
                             OpResult::kFalse,
                         },
                         {
                             "AndTrueFalse",
                             BinaryOp::kAnd,
                             OpArg::kTrue,
                             OpArg::kFalse,
                             OpResult::kFalse,
                         },
                         {
                             "AndTrueTrue",
                             BinaryOp::kAnd,
                             OpArg::kTrue,
                             OpArg::kTrue,
                             OpResult::kTrue,
                         },

                         {
                             "AndTrueError",
                             BinaryOp::kAnd,
                             OpArg::kTrue,
                             OpArg::kError,
                             OpResult::kError,
                         },
                         {
                             "AndErrorTrue",
                             BinaryOp::kAnd,
                             OpArg::kError,
                             OpArg::kTrue,
                             OpResult::kError,
                         },
                         {
                             "AndFalseError",
                             BinaryOp::kAnd,
                             OpArg::kFalse,
                             OpArg::kError,
                             OpResult::kFalse,
                         },
                         {
                             "AndErrorFalse",
                             BinaryOp::kAnd,
                             OpArg::kError,
                             OpArg::kFalse,
                             OpResult::kFalse,
                         },
                         {
                             "AndErrorError",
                             BinaryOp::kAnd,
                             OpArg::kError,
                             OpArg::kError,
                             OpResult::kError,
                         },

                         {
                             "AndTrueUnknown",
                             BinaryOp::kAnd,
                             OpArg::kTrue,
                             OpArg::kUnknown,
                             OpResult::kUnknown,
                         },
                         {
                             "AndUnknownTrue",
                             BinaryOp::kAnd,
                             OpArg::kUnknown,
                             OpArg::kTrue,
                             OpResult::kUnknown,
                         },
                         {
                             "AndFalseUnknown",
                             BinaryOp::kAnd,
                             OpArg::kFalse,
                             OpArg::kUnknown,
                             OpResult::kFalse,
                         },
                         {
                             "AndUnknownFalse",
                             BinaryOp::kAnd,
                             OpArg::kUnknown,
                             OpArg::kFalse,
                             OpResult::kFalse,
                         },
                         {
                             "AndUnknownUnknown",
                             BinaryOp::kAnd,
                             OpArg::kUnknown,
                             OpArg::kUnknown,
                             OpResult::kUnknown,
                         },
                         {
                             "AndUnknownError",
                             BinaryOp::kAnd,
                             OpArg::kUnknown,
                             OpArg::kError,
                             OpResult::kUnknown,
                         },
                         {
                             "AndErrorUnknown",
                             BinaryOp::kAnd,
                             OpArg::kError,
                             OpArg::kUnknown,
                             OpResult::kUnknown,
                         },
                         // Or cases are simplified since the logic generalizes
                         // and is covered by and cases.
                     })),
    [](const testing::TestParamInfo<DirectBinaryLogicStepTest::ParamType>& info)
        -> std::string {
      bool shortcircuiting_enabled = std::get<0>(info.param);
      absl::string_view name = std::get<1>(info.param).name;
      return absl::StrCat(
          name, (shortcircuiting_enabled ? "ShortcircuitingEnabled" : ""));
    });

struct UnaryTestCase {
  std::string name;
  UnaryOp op;
  OpArg arg;
  OpResult result;
};

class DirectUnaryLogicStepTest : public testing::TestWithParam<UnaryTestCase> {
 public:
  DirectUnaryLogicStepTest() = default;

  const UnaryTestCase& GetTestCase() { return GetParam(); }

 protected:
  Arena arena_;
};

TEST_P(DirectUnaryLogicStepTest, TestCases) {
  const UnaryTestCase& test_case = GetTestCase();

  std::unique_ptr<DirectExpressionStep> arg = MakeArgStep(test_case.arg, "arg");

  std::unique_ptr<DirectExpressionStep> op =
      (test_case.op == UnaryOp::kNot)
          ? CreateDirectNotStep(std::move(arg), -1)
          : CreateDirectNotStrictlyFalseStep(std::move(arg), -1);

  cel::Activation activation;
  cel::RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());
  ExecutionFrameBase frame(activation, options, type_provider,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value value;
  AttributeTrail attr;
  ASSERT_THAT(op->Evaluate(frame, value, attr), IsOk());

  switch (test_case.result) {
    case OpResult::kTrue:
      ASSERT_TRUE(value.IsBool());
      EXPECT_TRUE(value.GetBool().NativeValue());
      break;
    case OpResult::kFalse:
      ASSERT_TRUE(value.IsBool());
      EXPECT_FALSE(value.GetBool().NativeValue());
      break;
    case OpResult::kUnknown:
      EXPECT_TRUE(value.IsUnknown());
      break;
    case OpResult::kError:
      EXPECT_TRUE(value.IsError());
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    DirectUnaryLogicStepTest, DirectUnaryLogicStepTest,
    testing::ValuesIn<std::vector<UnaryTestCase>>(
        {UnaryTestCase{"NotTrue", UnaryOp::kNot, OpArg::kTrue,
                       OpResult::kFalse},
         UnaryTestCase{"NotError", UnaryOp::kNot, OpArg::kError,
                       OpResult::kError},
         UnaryTestCase{"NotUnknown", UnaryOp::kNot, OpArg::kUnknown,
                       OpResult::kUnknown},
         UnaryTestCase{"NotInt", UnaryOp::kNot, OpArg::kInt, OpResult::kError},
         UnaryTestCase{"NotFalse", UnaryOp::kNot, OpArg::kFalse,
                       OpResult::kTrue},
         UnaryTestCase{"NotStrictlyFalseTrue", UnaryOp::kNotStrictlyFalse,
                       OpArg::kTrue, OpResult::kTrue},
         UnaryTestCase{"NotStrictlyFalseError", UnaryOp::kNotStrictlyFalse,
                       OpArg::kError, OpResult::kTrue},
         UnaryTestCase{"NotStrictlyFalseUnknown", UnaryOp::kNotStrictlyFalse,
                       OpArg::kUnknown, OpResult::kTrue},
         UnaryTestCase{"NotStrictlyFalseInt", UnaryOp::kNotStrictlyFalse,
                       OpArg::kInt, OpResult::kError},
         UnaryTestCase{"NotStrictlyFalseFalse", UnaryOp::kNotStrictlyFalse,
                       OpArg::kFalse, OpResult::kFalse}}),
    [](const testing::TestParamInfo<DirectUnaryLogicStepTest::ParamType>& info)
        -> std::string { return info.param.name; });

}  // namespace

}  // namespace google::api::expr::runtime
