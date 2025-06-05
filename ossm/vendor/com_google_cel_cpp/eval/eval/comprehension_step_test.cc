#include "eval/eval/comprehension_step.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/ast_internal/expr.h"
#include "base/type_provider.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/cel_expression_flat_impl.h"
#include "eval/eval/comprehension_slots.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/eval/ident_step.h"
#include "eval/public/activation.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/cel_proto_wrapper.h"
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
using ::cel::IntValue;
using ::cel::TypeProvider;
using ::cel::Value;
using ::cel::ast_internal::Expr;
using ::cel::ast_internal::Ident;
using ::cel::extensions::ProtoMemoryManagerRef;
using ::cel::test::BoolValueIs;
using ::google::protobuf::ListValue;
using ::google::protobuf::Struct;
using ::google::protobuf::Arena;
using ::testing::_;
using ::testing::Eq;
using ::testing::Return;
using ::testing::SizeIs;

Ident CreateIdent(const std::string& var) {
  Ident expr;
  expr.set_name(var);
  return expr;
}

class ListKeysStepTest : public testing::Test {
 public:
  ListKeysStepTest() = default;

  std::unique_ptr<CelExpressionFlatImpl> MakeExpression(
      ExecutionPath&& path, bool unknown_attributes = false) {
    cel::RuntimeOptions options;
    if (unknown_attributes) {
      options.unknown_processing =
          cel::UnknownProcessingOptions::kAttributeAndFunction;
    }
    return std::make_unique<CelExpressionFlatImpl>(
        FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                       TypeProvider::Builtin(), options));
  }

 private:
  Expr dummy_expr_;
};

class GetListKeysResultStep : public ExpressionStepBase {
 public:
  GetListKeysResultStep() : ExpressionStepBase(-1, false) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    frame->value_stack().Pop(1);
    return absl::OkStatus();
  }
};

MATCHER_P(CelStringValue, val, "") {
  const CelValue& to_match = arg;
  absl::string_view value = val;
  return to_match.IsString() && to_match.StringOrDie().value() == value;
}

TEST_F(ListKeysStepTest, ListPassedThrough) {
  ExecutionPath path;
  Ident ident = CreateIdent("var");
  auto result = CreateIdentStep(ident, 0);
  ASSERT_OK(result);
  path.push_back(*std::move(result));
  result = CreateComprehensionInitStep(1);
  ASSERT_OK(result);
  path.push_back(*std::move(result));
  path.push_back(std::make_unique<GetListKeysResultStep>());

  auto expression = MakeExpression(std::move(path));

  Activation activation;
  Arena arena;
  ListValue value;
  value.add_values()->set_number_value(1.0);
  value.add_values()->set_number_value(2.0);
  value.add_values()->set_number_value(3.0);
  activation.InsertValue("var", CelProtoWrapper::CreateMessage(&value, &arena));

  auto eval_result = expression->Evaluate(activation, &arena);

  ASSERT_OK(eval_result);
  ASSERT_TRUE(eval_result->IsList());
  EXPECT_THAT(*eval_result->ListOrDie(), SizeIs(3));
}

TEST_F(ListKeysStepTest, MapToKeyList) {
  ExecutionPath path;
  Ident ident = CreateIdent("var");
  auto result = CreateIdentStep(ident, 0);
  ASSERT_OK(result);
  path.push_back(*std::move(result));
  result = CreateComprehensionInitStep(1);
  ASSERT_OK(result);
  path.push_back(*std::move(result));
  path.push_back(std::make_unique<GetListKeysResultStep>());

  auto expression = MakeExpression(std::move(path));

  Activation activation;
  Arena arena;
  Struct value;
  (*value.mutable_fields())["key1"].set_number_value(1.0);
  (*value.mutable_fields())["key2"].set_number_value(2.0);
  (*value.mutable_fields())["key3"].set_number_value(3.0);

  activation.InsertValue("var", CelProtoWrapper::CreateMessage(&value, &arena));

  auto eval_result = expression->Evaluate(activation, &arena);

  ASSERT_OK(eval_result);
  ASSERT_TRUE(eval_result->IsList());
  EXPECT_THAT(*eval_result->ListOrDie(), SizeIs(3));
  std::vector<CelValue> keys;
  keys.reserve(eval_result->ListOrDie()->size());
  for (int i = 0; i < eval_result->ListOrDie()->size(); i++) {
    keys.push_back(eval_result->ListOrDie()->operator[](i));
  }
  EXPECT_THAT(keys, testing::UnorderedElementsAre(CelStringValue("key1"),
                                                  CelStringValue("key2"),
                                                  CelStringValue("key3")));
}

TEST_F(ListKeysStepTest, MapPartiallyUnknown) {
  ExecutionPath path;
  Ident ident = CreateIdent("var");
  auto result = CreateIdentStep(ident, 0);
  ASSERT_OK(result);
  path.push_back(*std::move(result));
  result = CreateComprehensionInitStep(1);
  ASSERT_OK(result);
  path.push_back(*std::move(result));
  path.push_back(std::make_unique<GetListKeysResultStep>());

  auto expression =
      MakeExpression(std::move(path), /*unknown_attributes=*/true);

  Activation activation;
  Arena arena;
  Struct value;
  (*value.mutable_fields())["key1"].set_number_value(1.0);
  (*value.mutable_fields())["key2"].set_number_value(2.0);
  (*value.mutable_fields())["key3"].set_number_value(3.0);

  activation.InsertValue("var", CelProtoWrapper::CreateMessage(&value, &arena));
  activation.set_unknown_attribute_patterns({CelAttributePattern(
      "var",
      {CreateCelAttributeQualifierPattern(CelValue::CreateStringView("key2")),
       CreateCelAttributeQualifierPattern(CelValue::CreateStringView("foo")),
       CelAttributeQualifierPattern::CreateWildcard()})});

  auto eval_result = expression->Evaluate(activation, &arena);

  ASSERT_OK(eval_result);
  ASSERT_TRUE(eval_result->IsUnknownSet());
  const auto& attrs = eval_result->UnknownSetOrDie()->unknown_attributes();

  EXPECT_THAT(attrs, SizeIs(1));
  EXPECT_THAT(attrs.begin()->variable_name(), Eq("var"));
  EXPECT_THAT(attrs.begin()->qualifier_path(), SizeIs(0));
}

TEST_F(ListKeysStepTest, ErrorPassedThrough) {
  ExecutionPath path;
  Ident ident = CreateIdent("var");
  auto result = CreateIdentStep(ident, 0);
  ASSERT_OK(result);
  path.push_back(*std::move(result));
  result = CreateComprehensionInitStep(1);
  ASSERT_OK(result);
  path.push_back(*std::move(result));
  path.push_back(std::make_unique<GetListKeysResultStep>());

  auto expression = MakeExpression(std::move(path));

  Activation activation;
  Arena arena;

  // Var not in activation, turns into cel error at eval time.
  auto eval_result = expression->Evaluate(activation, &arena);

  ASSERT_OK(eval_result);
  ASSERT_TRUE(eval_result->IsError());
  EXPECT_THAT(eval_result->ErrorOrDie()->message(),
              testing::HasSubstr("\"var\""));
  EXPECT_EQ(eval_result->ErrorOrDie()->code(), absl::StatusCode::kUnknown);
}

TEST_F(ListKeysStepTest, UnknownSetPassedThrough) {
  ExecutionPath path;
  Ident ident = CreateIdent("var");
  auto result = CreateIdentStep(ident, 0);
  ASSERT_OK(result);
  path.push_back(*std::move(result));
  result = CreateComprehensionInitStep(1);
  ASSERT_OK(result);
  path.push_back(*std::move(result));
  path.push_back(std::make_unique<GetListKeysResultStep>());

  auto expression =
      MakeExpression(std::move(path), /*unknown_attributes=*/true);

  Activation activation;
  Arena arena;

  activation.set_unknown_attribute_patterns({CelAttributePattern("var", {})});

  auto eval_result = expression->Evaluate(activation, &arena);

  ASSERT_OK(eval_result);
  ASSERT_TRUE(eval_result->IsUnknownSet());
  EXPECT_THAT(eval_result->UnknownSetOrDie()->unknown_attributes(), SizeIs(1));
}

class MockDirectStep : public DirectExpressionStep {
 public:
  MockDirectStep() : DirectExpressionStep(-1) {}

  MOCK_METHOD(absl::Status, Evaluate,
              (ExecutionFrameBase&, Value&, AttributeTrail&),
              (const, override));
};

// Test fixture for comprehensions.
//
// Comprehensions are quite involved so tests here focus on edge cases that are
// hard to exercise normally in functional-style tests for the planner.
class DirectComprehensionTest : public testing::Test {
 public:
  DirectComprehensionTest()
      : value_manager_(TypeProvider::Builtin(), ProtoMemoryManagerRef(&arena_)),
        slots_(2) {}

  // returns a two element list for testing [1, 2].
  absl::StatusOr<cel::ListValue> MakeList() {
    CEL_ASSIGN_OR_RETURN(auto builder, value_manager_.get().NewListValueBuilder(
                                           cel::ListType()));

    CEL_RETURN_IF_ERROR(builder->Add(IntValue(1)));
    CEL_RETURN_IF_ERROR(builder->Add(IntValue(2)));
    return std::move(*builder).Build();
  }

 protected:
  google::protobuf::Arena arena_;
  cel::ManagedValueFactory value_manager_;
  ComprehensionSlots slots_;
  cel::Activation empty_activation_;
};

TEST_F(DirectComprehensionTest, PropagateRangeNonOkStatus) {
  cel::RuntimeOptions options;

  ExecutionFrameBase frame(empty_activation_, /*callback=*/nullptr, options,
                           value_manager_.get(), slots_);

  auto range_step = std::make_unique<MockDirectStep>();
  MockDirectStep* mock = range_step.get();

  ON_CALL(*mock, Evaluate(_, _, _))
      .WillByDefault(Return(absl::InternalError("test range error")));

  auto compre_step = CreateDirectComprehensionStep(
      0, 1,
      /*range_step=*/std::move(range_step),
      /*accu_init=*/CreateConstValueDirectStep(BoolValue(false)),
      /*loop_step=*/CreateConstValueDirectStep(BoolValue(false)),
      /*condition_step=*/CreateConstValueDirectStep(BoolValue(true)),
      /*result_step=*/CreateDirectSlotIdentStep("__result__", 1, -1),
      /*shortcircuiting=*/true, -1);

  Value result;
  AttributeTrail trail;
  EXPECT_THAT(compre_step->Evaluate(frame, result, trail),
              StatusIs(absl::StatusCode::kInternal, "test range error"));
}

TEST_F(DirectComprehensionTest, PropagateAccuInitNonOkStatus) {
  cel::RuntimeOptions options;

  ExecutionFrameBase frame(empty_activation_, /*callback=*/nullptr, options,
                           value_manager_.get(), slots_);

  auto accu_init = std::make_unique<MockDirectStep>();
  MockDirectStep* mock = accu_init.get();

  ON_CALL(*mock, Evaluate(_, _, _))
      .WillByDefault(Return(absl::InternalError("test accu init error")));

  ASSERT_OK_AND_ASSIGN(auto list, MakeList());

  auto compre_step = CreateDirectComprehensionStep(
      0, 1,
      /*range_step=*/CreateConstValueDirectStep(std::move(list)),
      /*accu_init=*/std::move(accu_init),
      /*loop_step=*/CreateConstValueDirectStep(BoolValue(false)),
      /*condition_step=*/CreateConstValueDirectStep(BoolValue(true)),
      /*result_step=*/CreateDirectSlotIdentStep("__result__", 1, -1),
      /*shortcircuiting=*/true, -1);

  Value result;
  AttributeTrail trail;
  EXPECT_THAT(compre_step->Evaluate(frame, result, trail),
              StatusIs(absl::StatusCode::kInternal, "test accu init error"));
}

TEST_F(DirectComprehensionTest, PropagateLoopNonOkStatus) {
  cel::RuntimeOptions options;

  ExecutionFrameBase frame(empty_activation_, /*callback=*/nullptr, options,
                           value_manager_.get(), slots_);

  auto loop_step = std::make_unique<MockDirectStep>();
  MockDirectStep* mock = loop_step.get();

  ON_CALL(*mock, Evaluate(_, _, _))
      .WillByDefault(Return(absl::InternalError("test loop error")));

  ASSERT_OK_AND_ASSIGN(auto list, MakeList());

  auto compre_step = CreateDirectComprehensionStep(
      0, 1,
      /*range_step=*/CreateConstValueDirectStep(std::move(list)),
      /*accu_init=*/CreateConstValueDirectStep(BoolValue(false)),
      /*loop_step=*/std::move(loop_step),
      /*condition_step=*/CreateConstValueDirectStep(BoolValue(true)),
      /*result_step=*/CreateDirectSlotIdentStep("__result__", 1, -1),
      /*shortcircuiting=*/true, -1);

  Value result;
  AttributeTrail trail;
  EXPECT_THAT(compre_step->Evaluate(frame, result, trail),
              StatusIs(absl::StatusCode::kInternal, "test loop error"));
}

TEST_F(DirectComprehensionTest, PropagateConditionNonOkStatus) {
  cel::RuntimeOptions options;

  ExecutionFrameBase frame(empty_activation_, /*callback=*/nullptr, options,
                           value_manager_.get(), slots_);

  auto condition = std::make_unique<MockDirectStep>();
  MockDirectStep* mock = condition.get();

  ON_CALL(*mock, Evaluate(_, _, _))
      .WillByDefault(Return(absl::InternalError("test condition error")));

  ASSERT_OK_AND_ASSIGN(auto list, MakeList());

  auto compre_step = CreateDirectComprehensionStep(
      0, 1,
      /*range_step=*/CreateConstValueDirectStep(std::move(list)),
      /*accu_init=*/CreateConstValueDirectStep(BoolValue(false)),
      /*loop_step=*/CreateConstValueDirectStep(BoolValue(false)),
      /*condition_step=*/std::move(condition),
      /*result_step=*/CreateDirectSlotIdentStep("__result__", 1, -1),
      /*shortcircuiting=*/true, -1);

  Value result;
  AttributeTrail trail;
  EXPECT_THAT(compre_step->Evaluate(frame, result, trail),
              StatusIs(absl::StatusCode::kInternal, "test condition error"));
}

TEST_F(DirectComprehensionTest, PropagateResultNonOkStatus) {
  cel::RuntimeOptions options;

  ExecutionFrameBase frame(empty_activation_, /*callback=*/nullptr, options,
                           value_manager_.get(), slots_);

  auto result_step = std::make_unique<MockDirectStep>();
  MockDirectStep* mock = result_step.get();

  ON_CALL(*mock, Evaluate(_, _, _))
      .WillByDefault(Return(absl::InternalError("test result error")));

  ASSERT_OK_AND_ASSIGN(auto list, MakeList());

  auto compre_step = CreateDirectComprehensionStep(
      0, 1,
      /*range_step=*/CreateConstValueDirectStep(std::move(list)),
      /*accu_init=*/CreateConstValueDirectStep(BoolValue(false)),
      /*loop_step=*/CreateConstValueDirectStep(BoolValue(false)),
      /*condition_step=*/CreateConstValueDirectStep(BoolValue(true)),
      /*result_step=*/std::move(result_step),
      /*shortcircuiting=*/true, -1);

  Value result;
  AttributeTrail trail;
  EXPECT_THAT(compre_step->Evaluate(frame, result, trail),
              StatusIs(absl::StatusCode::kInternal, "test result error"));
}

TEST_F(DirectComprehensionTest, Shortcircuit) {
  cel::RuntimeOptions options;

  ExecutionFrameBase frame(empty_activation_, /*callback=*/nullptr, options,
                           value_manager_.get(), slots_);

  auto loop_step = std::make_unique<MockDirectStep>();
  MockDirectStep* mock = loop_step.get();

  EXPECT_CALL(*mock, Evaluate(_, _, _))
      .Times(0)
      .WillRepeatedly([](ExecutionFrameBase&, Value& result, AttributeTrail&) {
        result = BoolValue(false);
        return absl::OkStatus();
      });

  ASSERT_OK_AND_ASSIGN(auto list, MakeList());

  auto compre_step = CreateDirectComprehensionStep(
      0, 1,
      /*range_step=*/CreateConstValueDirectStep(std::move(list)),
      /*accu_init=*/CreateConstValueDirectStep(BoolValue(false)),
      /*loop_step=*/std::move(loop_step),
      /*condition_step=*/CreateConstValueDirectStep(BoolValue(false)),
      /*result_step=*/CreateDirectSlotIdentStep("__result__", 1, -1),
      /*shortcircuiting=*/true, -1);

  Value result;
  AttributeTrail trail;
  ASSERT_OK(compre_step->Evaluate(frame, result, trail));
  EXPECT_THAT(result, BoolValueIs(false));
}

TEST_F(DirectComprehensionTest, IterationLimit) {
  cel::RuntimeOptions options;
  options.comprehension_max_iterations = 2;
  ExecutionFrameBase frame(empty_activation_, /*callback=*/nullptr, options,
                           value_manager_.get(), slots_);

  auto loop_step = std::make_unique<MockDirectStep>();
  MockDirectStep* mock = loop_step.get();

  EXPECT_CALL(*mock, Evaluate(_, _, _))
      .Times(1)
      .WillRepeatedly([](ExecutionFrameBase&, Value& result, AttributeTrail&) {
        result = BoolValue(false);
        return absl::OkStatus();
      });

  ASSERT_OK_AND_ASSIGN(auto list, MakeList());

  auto compre_step = CreateDirectComprehensionStep(
      0, 1,
      /*range_step=*/CreateConstValueDirectStep(std::move(list)),
      /*accu_init=*/CreateConstValueDirectStep(BoolValue(false)),
      /*loop_step=*/std::move(loop_step),
      /*condition_step=*/CreateConstValueDirectStep(BoolValue(true)),
      /*result_step=*/CreateDirectSlotIdentStep("__result__", 1, -1),
      /*shortcircuiting=*/true, -1);

  Value result;
  AttributeTrail trail;
  EXPECT_THAT(compre_step->Evaluate(frame, result, trail),
              StatusIs(absl::StatusCode::kInternal));
}

TEST_F(DirectComprehensionTest, Exhaustive) {
  cel::RuntimeOptions options;

  ExecutionFrameBase frame(empty_activation_, /*callback=*/nullptr, options,
                           value_manager_.get(), slots_);

  auto loop_step = std::make_unique<MockDirectStep>();
  MockDirectStep* mock = loop_step.get();

  EXPECT_CALL(*mock, Evaluate(_, _, _))
      .Times(2)
      .WillRepeatedly([](ExecutionFrameBase&, Value& result, AttributeTrail&) {
        result = BoolValue(false);
        return absl::OkStatus();
      });

  ASSERT_OK_AND_ASSIGN(auto list, MakeList());

  auto compre_step = CreateDirectComprehensionStep(
      0, 1,
      /*range_step=*/CreateConstValueDirectStep(std::move(list)),
      /*accu_init=*/CreateConstValueDirectStep(BoolValue(false)),
      /*loop_step=*/std::move(loop_step),
      /*condition_step=*/CreateConstValueDirectStep(BoolValue(false)),
      /*result_step=*/CreateDirectSlotIdentStep("__result__", 1, -1),
      /*shortcircuiting=*/false, -1);

  Value result;
  AttributeTrail trail;
  ASSERT_OK(compre_step->Evaluate(frame, result, trail));
  EXPECT_THAT(result, BoolValueIs(false));
}

}  // namespace
}  // namespace google::api::expr::runtime
