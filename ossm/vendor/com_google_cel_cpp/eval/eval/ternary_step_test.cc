#include "eval/eval/ternary_step.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
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

using ::absl_testing::StatusIs;
using ::cel::BoolValue;
using ::cel::Cast;
using ::cel::ErrorValue;
using ::cel::InstanceOf;
using ::cel::IntValue;
using ::cel::RuntimeOptions;
using ::cel::TypeProvider;
using ::cel::UnknownValue;
using ::cel::ValueManager;
using ::cel::ast_internal::Expr;
using ::cel::extensions::ProtoMemoryManagerRef;
using ::google::protobuf::Arena;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Truly;

class LogicStepTest : public testing::TestWithParam<bool> {
 public:
  absl::Status EvaluateLogic(CelValue arg0, CelValue arg1, CelValue arg2,
                             CelValue* result, bool enable_unknown) {
    Expr expr0;
    expr0.set_id(1);
    auto& ident_expr0 = expr0.mutable_ident_expr();
    ident_expr0.set_name("name0");

    Expr expr1;
    expr1.set_id(2);
    auto& ident_expr1 = expr1.mutable_ident_expr();
    ident_expr1.set_name("name1");

    Expr expr2;
    expr2.set_id(3);
    auto& ident_expr2 = expr2.mutable_ident_expr();
    ident_expr2.set_name("name2");

    ExecutionPath path;

    CEL_ASSIGN_OR_RETURN(auto step, CreateIdentStep(ident_expr0, expr0.id()));
    path.push_back(std::move(step));

    CEL_ASSIGN_OR_RETURN(step, CreateIdentStep(ident_expr1, expr1.id()));
    path.push_back(std::move(step));

    CEL_ASSIGN_OR_RETURN(step, CreateIdentStep(ident_expr2, expr2.id()));
    path.push_back(std::move(step));

    CEL_ASSIGN_OR_RETURN(step, CreateTernaryStep(4));
    path.push_back(std::move(step));

    cel::RuntimeOptions options;
    if (enable_unknown) {
      options.unknown_processing =
          cel::UnknownProcessingOptions::kAttributeOnly;
    }
    CelExpressionFlatImpl impl(
        FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                       TypeProvider::Builtin(), options));

    Activation activation;
    std::string value("test");

    activation.InsertValue("name0", arg0);
    activation.InsertValue("name1", arg1);
    activation.InsertValue("name2", arg2);
    auto status0 = impl.Evaluate(activation, &arena_);
    if (!status0.ok()) return status0.status();

    *result = status0.value();
    return absl::OkStatus();
  }

 private:
  Arena arena_;
};

TEST_P(LogicStepTest, TestBoolCond) {
  CelValue result;
  absl::Status status =
      EvaluateLogic(CelValue::CreateBool(true), CelValue::CreateBool(true),
                    CelValue::CreateBool(false), &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status =
      EvaluateLogic(CelValue::CreateBool(false), CelValue::CreateBool(true),
                    CelValue::CreateBool(false), &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());
}

TEST_P(LogicStepTest, TestErrorHandling) {
  CelValue result;
  CelError error = absl::CancelledError();
  CelValue error_value = CelValue::CreateError(&error);
  ASSERT_OK(EvaluateLogic(error_value, CelValue::CreateBool(true),
                          CelValue::CreateBool(false), &result, GetParam()));
  ASSERT_TRUE(result.IsError());

  ASSERT_OK(EvaluateLogic(CelValue::CreateBool(true), error_value,
                          CelValue::CreateBool(false), &result, GetParam()));
  ASSERT_TRUE(result.IsError());

  ASSERT_OK(EvaluateLogic(CelValue::CreateBool(false), error_value,
                          CelValue::CreateBool(false), &result, GetParam()));
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());
}

TEST_F(LogicStepTest, TestUnknownHandling) {
  CelValue result;
  UnknownSet unknown_set;
  CelError cel_error = absl::CancelledError();
  CelValue unknown_value = CelValue::CreateUnknownSet(&unknown_set);
  CelValue error_value = CelValue::CreateError(&cel_error);
  ASSERT_OK(EvaluateLogic(unknown_value, CelValue::CreateBool(true),
                          CelValue::CreateBool(false), &result, true));
  ASSERT_TRUE(result.IsUnknownSet());

  ASSERT_OK(EvaluateLogic(CelValue::CreateBool(true), unknown_value,
                          CelValue::CreateBool(false), &result, true));
  ASSERT_TRUE(result.IsUnknownSet());

  ASSERT_OK(EvaluateLogic(CelValue::CreateBool(false), unknown_value,
                          CelValue::CreateBool(false), &result, true));
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());

  ASSERT_OK(EvaluateLogic(error_value, unknown_value,
                          CelValue::CreateBool(false), &result, true));
  ASSERT_TRUE(result.IsError());

  ASSERT_OK(EvaluateLogic(unknown_value, error_value,
                          CelValue::CreateBool(false), &result, true));
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

  ASSERT_OK(EvaluateLogic(CelValue::CreateUnknownSet(&unknown_set0),
                          CelValue::CreateUnknownSet(&unknown_set1),
                          CelValue::CreateBool(false), &result, true));
  ASSERT_TRUE(result.IsUnknownSet());
  const auto& attrs = result.UnknownSetOrDie()->unknown_attributes();
  ASSERT_THAT(attrs, testing::SizeIs(1));
  EXPECT_THAT(attrs.begin()->variable_name(), Eq("name0"));
}

INSTANTIATE_TEST_SUITE_P(LogicStepTest, LogicStepTest, testing::Bool());

class TernaryStepDirectTest : public testing::TestWithParam<bool> {
 public:
  TernaryStepDirectTest()
      : value_factory_(TypeProvider::Builtin(),
                       ProtoMemoryManagerRef(&arena_)) {}

  bool Shortcircuiting() { return GetParam(); }

  ValueManager& value_manager() { return value_factory_.get(); }

 protected:
  Arena arena_;
  cel::ManagedValueFactory value_factory_;
};

TEST_P(TernaryStepDirectTest, ReturnLhs) {
  cel::Activation activation;
  RuntimeOptions opts;
  ExecutionFrameBase frame(activation, opts, value_manager());

  std::unique_ptr<DirectExpressionStep> step = CreateDirectTernaryStep(
      CreateConstValueDirectStep(BoolValue(true), -1),
      CreateConstValueDirectStep(IntValue(1), -1),
      CreateConstValueDirectStep(IntValue(2), -1), -1, Shortcircuiting());

  cel::Value result;
  AttributeTrail attr_unused;

  ASSERT_OK(step->Evaluate(frame, result, attr_unused));

  ASSERT_TRUE(InstanceOf<IntValue>(result));
  EXPECT_EQ(Cast<IntValue>(result).NativeValue(), 1);
}

TEST_P(TernaryStepDirectTest, ReturnRhs) {
  cel::Activation activation;
  RuntimeOptions opts;
  ExecutionFrameBase frame(activation, opts, value_manager());

  std::unique_ptr<DirectExpressionStep> step = CreateDirectTernaryStep(
      CreateConstValueDirectStep(BoolValue(false), -1),
      CreateConstValueDirectStep(IntValue(1), -1),
      CreateConstValueDirectStep(IntValue(2), -1), -1, Shortcircuiting());

  cel::Value result;
  AttributeTrail attr_unused;

  ASSERT_OK(step->Evaluate(frame, result, attr_unused));

  ASSERT_TRUE(InstanceOf<IntValue>(result));
  EXPECT_EQ(Cast<IntValue>(result).NativeValue(), 2);
}

TEST_P(TernaryStepDirectTest, ForwardError) {
  cel::Activation activation;
  RuntimeOptions opts;
  ExecutionFrameBase frame(activation, opts, value_manager());

  cel::Value error_value =
      value_manager().CreateErrorValue(absl::InternalError("test error"));

  std::unique_ptr<DirectExpressionStep> step = CreateDirectTernaryStep(
      CreateConstValueDirectStep(error_value, -1),
      CreateConstValueDirectStep(IntValue(1), -1),
      CreateConstValueDirectStep(IntValue(2), -1), -1, Shortcircuiting());

  cel::Value result;
  AttributeTrail attr_unused;

  ASSERT_OK(step->Evaluate(frame, result, attr_unused));

  ASSERT_TRUE(InstanceOf<ErrorValue>(result));
  EXPECT_THAT(Cast<ErrorValue>(result).NativeValue(),
              StatusIs(absl::StatusCode::kInternal, "test error"));
}

TEST_P(TernaryStepDirectTest, ForwardUnknown) {
  cel::Activation activation;
  RuntimeOptions opts;
  opts.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  ExecutionFrameBase frame(activation, opts, value_manager());

  std::vector<cel::Attribute> attrs{{cel::Attribute("var")}};

  cel::UnknownValue unknown_value =
      value_manager().CreateUnknownValue(cel::AttributeSet(attrs));

  std::unique_ptr<DirectExpressionStep> step = CreateDirectTernaryStep(
      CreateConstValueDirectStep(unknown_value, -1),
      CreateConstValueDirectStep(IntValue(2), -1),
      CreateConstValueDirectStep(IntValue(3), -1), -1, Shortcircuiting());

  cel::Value result;
  AttributeTrail attr_unused;

  ASSERT_OK(step->Evaluate(frame, result, attr_unused));
  ASSERT_TRUE(InstanceOf<UnknownValue>(result));
  EXPECT_THAT(Cast<UnknownValue>(result).NativeValue().unknown_attributes(),
              ElementsAre(Truly([](const cel::Attribute& attr) {
                return attr.variable_name() == "var";
              })));
}

TEST_P(TernaryStepDirectTest, UnexpectedCondtionKind) {
  cel::Activation activation;
  RuntimeOptions opts;
  ExecutionFrameBase frame(activation, opts, value_manager());

  std::unique_ptr<DirectExpressionStep> step = CreateDirectTernaryStep(
      CreateConstValueDirectStep(IntValue(-1), -1),
      CreateConstValueDirectStep(IntValue(1), -1),
      CreateConstValueDirectStep(IntValue(2), -1), -1, Shortcircuiting());

  cel::Value result;
  AttributeTrail attr_unused;

  ASSERT_OK(step->Evaluate(frame, result, attr_unused));

  ASSERT_TRUE(InstanceOf<ErrorValue>(result));
  EXPECT_THAT(Cast<ErrorValue>(result).NativeValue(),
              StatusIs(absl::StatusCode::kUnknown,
                       HasSubstr("No matching overloads found")));
}

TEST_P(TernaryStepDirectTest, Shortcircuiting) {
  class RecordCallStep : public DirectExpressionStep {
   public:
    explicit RecordCallStep(bool& was_called)
        : DirectExpressionStep(-1), was_called_(&was_called) {}
    absl::Status Evaluate(ExecutionFrameBase& frame, cel::Value& result,
                          AttributeTrail& trail) const override {
      *was_called_ = true;
      result = IntValue(1);
      return absl::OkStatus();
    }

   private:
    absl::Nonnull<bool*> was_called_;
  };

  bool lhs_was_called = false;
  bool rhs_was_called = false;

  cel::Activation activation;
  RuntimeOptions opts;
  ExecutionFrameBase frame(activation, opts, value_manager());

  std::unique_ptr<DirectExpressionStep> step = CreateDirectTernaryStep(
      CreateConstValueDirectStep(BoolValue(false), -1),
      std::make_unique<RecordCallStep>(lhs_was_called),
      std::make_unique<RecordCallStep>(rhs_was_called), -1, Shortcircuiting());

  cel::Value result;
  AttributeTrail attr_unused;

  ASSERT_OK(step->Evaluate(frame, result, attr_unused));

  ASSERT_TRUE(InstanceOf<IntValue>(result));
  EXPECT_THAT(Cast<IntValue>(result).NativeValue(), Eq(1));
  bool expect_eager_eval = !Shortcircuiting();
  EXPECT_EQ(lhs_was_called, expect_eager_eval);
  EXPECT_TRUE(rhs_was_called);
}

INSTANTIATE_TEST_SUITE_P(TernaryStepDirectTest, TernaryStepDirectTest,
                         testing::Bool());

}  // namespace

}  // namespace google::api::expr::runtime
