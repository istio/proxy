#include "eval/eval/logic_step.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/ast_internal/expr.h"
#include "base/attribute.h"
#include "base/attribute_set.h"
#include "base/type_provider.h"
#include "common/casting.h"
#include "common/value.h"
#include "common/value_manager.h"
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
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "runtime/activation.h"
#include "runtime/managed_value_factory.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::Attribute;
using ::cel::AttributeSet;
using ::cel::BoolValue;
using ::cel::Cast;
using ::cel::ErrorValue;
using ::cel::InstanceOf;
using ::cel::IntValue;
using ::cel::ManagedValueFactory;
using ::cel::TypeProvider;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::ValueManager;
using ::cel::ast_internal::Expr;
using ::cel::extensions::ProtoMemoryManagerRef;
using ::google::protobuf::Arena;
using ::testing::Eq;

class LogicStepTest : public testing::TestWithParam<bool> {
 public:
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
        FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                       TypeProvider::Builtin(), options));

    Activation activation;
    activation.InsertValue("name0", arg0);
    activation.InsertValue("name1", arg1);
    CEL_ASSIGN_OR_RETURN(CelValue value, impl.Evaluate(activation, &arena_));
    *result = value;
    return absl::OkStatus();
  }

 private:
  Arena arena_;
};

TEST_P(LogicStepTest, TestAndLogic) {
  CelValue result;
  absl::Status status =
      EvaluateLogic(CelValue::CreateBool(true), CelValue::CreateBool(true),
                    false, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status =
      EvaluateLogic(CelValue::CreateBool(true), CelValue::CreateBool(false),
                    false, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());

  status =
      EvaluateLogic(CelValue::CreateBool(false), CelValue::CreateBool(true),
                    false, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());

  status =
      EvaluateLogic(CelValue::CreateBool(false), CelValue::CreateBool(false),
                    false, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());
}

TEST_P(LogicStepTest, TestOrLogic) {
  CelValue result;
  absl::Status status =
      EvaluateLogic(CelValue::CreateBool(true), CelValue::CreateBool(true),
                    true, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status =
      EvaluateLogic(CelValue::CreateBool(true), CelValue::CreateBool(false),
                    true, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status = EvaluateLogic(CelValue::CreateBool(false),
                         CelValue::CreateBool(true), true, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status =
      EvaluateLogic(CelValue::CreateBool(false), CelValue::CreateBool(false),
                    true, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());
}

TEST_P(LogicStepTest, TestAndLogicErrorHandling) {
  CelValue result;
  CelError error = absl::CancelledError();
  CelValue error_value = CelValue::CreateError(&error);
  absl::Status status = EvaluateLogic(error_value, CelValue::CreateBool(true),
                                      false, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsError());

  status = EvaluateLogic(CelValue::CreateBool(true), error_value, false,
                         &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsError());

  status = EvaluateLogic(CelValue::CreateBool(false), error_value, false,
                         &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());

  status = EvaluateLogic(error_value, CelValue::CreateBool(false), false,
                         &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());
}

TEST_P(LogicStepTest, TestOrLogicErrorHandling) {
  CelValue result;
  CelError error = absl::CancelledError();
  CelValue error_value = CelValue::CreateError(&error);
  absl::Status status = EvaluateLogic(error_value, CelValue::CreateBool(false),
                                      true, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsError());

  status = EvaluateLogic(CelValue::CreateBool(false), error_value, true,
                         &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsError());

  status = EvaluateLogic(CelValue::CreateBool(true), error_value, true, &result,
                         GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status = EvaluateLogic(error_value, CelValue::CreateBool(true), true, &result,
                         GetParam());
  ASSERT_OK(status);
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
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(CelValue::CreateBool(true), unknown_value, false,
                         &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(CelValue::CreateBool(false), unknown_value, false,
                         &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());

  status = EvaluateLogic(unknown_value, CelValue::CreateBool(false), false,
                         &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());

  status = EvaluateLogic(error_value, unknown_value, false, &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(unknown_value, error_value, false, &result, true);
  ASSERT_OK(status);
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
  ASSERT_OK(status);
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
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(CelValue::CreateBool(false), unknown_value, true,
                         &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(CelValue::CreateBool(true), unknown_value, true,
                         &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status = EvaluateLogic(unknown_value, CelValue::CreateBool(true), true,
                         &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status = EvaluateLogic(unknown_value, error_value, true, &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(error_value, unknown_value, true, &result, true);
  ASSERT_OK(status);
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
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());
  ASSERT_THAT(result.UnknownSetOrDie()->unknown_attributes().size(), Eq(2));
}

INSTANTIATE_TEST_SUITE_P(LogicStepTest, LogicStepTest, testing::Bool());

enum class Op { kAnd, kOr };

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

struct TestCase {
  std::string name;
  Op op;
  OpArg arg0;
  OpArg arg1;
  OpResult result;
};

class DirectLogicStepTest
    : public testing::TestWithParam<std::tuple<bool, TestCase>> {
 public:
  DirectLogicStepTest()
      : value_factory_(TypeProvider::Builtin(),
                       ProtoMemoryManagerRef(&arena_)) {}

  bool ShortcircuitingEnabled() { return std::get<0>(GetParam()); }
  const TestCase& GetTestCase() { return std::get<1>(GetParam()); }

  ValueManager& value_manager() { return value_factory_.get(); }

  UnknownValue MakeUnknownValue(std::string attr) {
    std::vector<Attribute> attrs;
    attrs.push_back(Attribute(std::move(attr)));
    return value_manager().CreateUnknownValue(AttributeSet(attrs));
  }

 protected:
  Arena arena_;
  ManagedValueFactory value_factory_;
};

TEST_P(DirectLogicStepTest, TestCases) {
  const TestCase& test_case = GetTestCase();

  auto MakeArg =
      [&](OpArg arg,
          absl::string_view name) -> std::unique_ptr<DirectExpressionStep> {
    switch (arg) {
      case OpArg::kTrue:
        return CreateConstValueDirectStep(BoolValue(true));
      case OpArg::kFalse:
        return CreateConstValueDirectStep(BoolValue(false));
      case OpArg::kUnknown:
        return CreateConstValueDirectStep(MakeUnknownValue(std::string(name)));
      case OpArg::kError:
        return CreateConstValueDirectStep(
            value_manager().CreateErrorValue(absl::InternalError(name)));
      case OpArg::kInt:
        return CreateConstValueDirectStep(IntValue(42));
    }
  };

  std::unique_ptr<DirectExpressionStep> lhs = MakeArg(test_case.arg0, "lhs");
  std::unique_ptr<DirectExpressionStep> rhs = MakeArg(test_case.arg1, "rhs");

  std::unique_ptr<DirectExpressionStep> op =
      (test_case.op == Op::kAnd)
          ? CreateDirectAndStep(std::move(lhs), std::move(rhs), -1,
                                ShortcircuitingEnabled())
          : CreateDirectOrStep(std::move(lhs), std::move(rhs), -1,
                               ShortcircuitingEnabled());

  cel::Activation activation;
  cel::RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  ExecutionFrameBase frame(activation, options, value_manager());

  Value value;
  AttributeTrail attr;
  ASSERT_OK(op->Evaluate(frame, value, attr));

  switch (test_case.result) {
    case OpResult::kTrue:
      ASSERT_TRUE(InstanceOf<BoolValue>(value));
      EXPECT_TRUE(Cast<BoolValue>(value).NativeValue());
      break;
    case OpResult::kFalse:
      ASSERT_TRUE(InstanceOf<BoolValue>(value));
      EXPECT_FALSE(Cast<BoolValue>(value).NativeValue());
      break;
    case OpResult::kUnknown:
      EXPECT_TRUE(InstanceOf<UnknownValue>(value));
      break;
    case OpResult::kError:
      EXPECT_TRUE(InstanceOf<ErrorValue>(value));
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    DirectLogicStepTest, DirectLogicStepTest,
    testing::Combine(testing::Bool(),
                     testing::ValuesIn<std::vector<TestCase>>({
                         {
                             "AndFalseFalse",
                             Op::kAnd,
                             OpArg::kFalse,
                             OpArg::kFalse,
                             OpResult::kFalse,
                         },
                         {
                             "AndFalseTrue",
                             Op::kAnd,
                             OpArg::kFalse,
                             OpArg::kTrue,
                             OpResult::kFalse,
                         },
                         {
                             "AndTrueFalse",
                             Op::kAnd,
                             OpArg::kTrue,
                             OpArg::kFalse,
                             OpResult::kFalse,
                         },
                         {
                             "AndTrueTrue",
                             Op::kAnd,
                             OpArg::kTrue,
                             OpArg::kTrue,
                             OpResult::kTrue,
                         },

                         {
                             "AndTrueError",
                             Op::kAnd,
                             OpArg::kTrue,
                             OpArg::kError,
                             OpResult::kError,
                         },
                         {
                             "AndErrorTrue",
                             Op::kAnd,
                             OpArg::kError,
                             OpArg::kTrue,
                             OpResult::kError,
                         },
                         {
                             "AndFalseError",
                             Op::kAnd,
                             OpArg::kFalse,
                             OpArg::kError,
                             OpResult::kFalse,
                         },
                         {
                             "AndErrorFalse",
                             Op::kAnd,
                             OpArg::kError,
                             OpArg::kFalse,
                             OpResult::kFalse,
                         },
                         {
                             "AndErrorError",
                             Op::kAnd,
                             OpArg::kError,
                             OpArg::kError,
                             OpResult::kError,
                         },

                         {
                             "AndTrueUnknown",
                             Op::kAnd,
                             OpArg::kTrue,
                             OpArg::kUnknown,
                             OpResult::kUnknown,
                         },
                         {
                             "AndUnknownTrue",
                             Op::kAnd,
                             OpArg::kUnknown,
                             OpArg::kTrue,
                             OpResult::kUnknown,
                         },
                         {
                             "AndFalseUnknown",
                             Op::kAnd,
                             OpArg::kFalse,
                             OpArg::kUnknown,
                             OpResult::kFalse,
                         },
                         {
                             "AndUnknownFalse",
                             Op::kAnd,
                             OpArg::kUnknown,
                             OpArg::kFalse,
                             OpResult::kFalse,
                         },
                         {
                             "AndUnknownUnknown",
                             Op::kAnd,
                             OpArg::kUnknown,
                             OpArg::kUnknown,
                             OpResult::kUnknown,
                         },
                         {
                             "AndUnknownError",
                             Op::kAnd,
                             OpArg::kUnknown,
                             OpArg::kError,
                             OpResult::kUnknown,
                         },
                         {
                             "AndErrorUnknown",
                             Op::kAnd,
                             OpArg::kError,
                             OpArg::kUnknown,
                             OpResult::kUnknown,
                         },

                         // Or cases are simplified since the logic generalizes
                         // and is covered by and cases.
                     })),
    [](const testing::TestParamInfo<DirectLogicStepTest::ParamType>& info)
        -> std::string {
      bool shortcircuiting_enabled = std::get<0>(info.param);
      absl::string_view name = std::get<1>(info.param).name;
      return absl::StrCat(
          name, (shortcircuiting_enabled ? "ShortcircuitingEnabled" : ""));
    });

}  // namespace

}  // namespace google::api::expr::runtime
