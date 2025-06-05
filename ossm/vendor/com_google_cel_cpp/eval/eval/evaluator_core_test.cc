#include "eval/eval/evaluator_core.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "base/type_provider.h"
#include "eval/compiler/cel_expression_builder_flat_impl.h"
#include "eval/eval/cel_expression_flat_impl.h"
#include "eval/internal/interop.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_value.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/testing.h"
#include "runtime/activation.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

using ::cel::IntValue;
using ::cel::TypeProvider;
using ::cel::extensions::ProtoMemoryManagerRef;
using ::cel::interop_internal::CreateIntValue;
using ::google::api::expr::v1alpha1::Expr;
using ::google::api::expr::runtime::RegisterBuiltinFunctions;
using ::testing::_;
using ::testing::Eq;

// Fake expression implementation
// Pushes int64_t(0) on top of value stack.
class FakeConstExpressionStep : public ExpressionStep {
 public:
  FakeConstExpressionStep() : ExpressionStep(0, true) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    frame->value_stack().Push(CreateIntValue(0));
    return absl::OkStatus();
  }
};

// Fake expression implementation
// Increments argument on top of the stack.
class FakeIncrementExpressionStep : public ExpressionStep {
 public:
  FakeIncrementExpressionStep() : ExpressionStep(0, true) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    auto value = frame->value_stack().Peek();
    frame->value_stack().Pop(1);
    EXPECT_TRUE(value->Is<IntValue>());
    int64_t val = value.GetInt().NativeValue();
    frame->value_stack().Push(CreateIntValue(val + 1));
    return absl::OkStatus();
  }
};

TEST(EvaluatorCoreTest, ExecutionFrameNext) {
  ExecutionPath path;
  google::protobuf::Arena arena;
  auto manager = ProtoMemoryManagerRef(&arena);
  auto const_step = std::make_unique<const FakeConstExpressionStep>();
  auto incr_step1 = std::make_unique<const FakeIncrementExpressionStep>();
  auto incr_step2 = std::make_unique<const FakeIncrementExpressionStep>();

  path.push_back(std::move(const_step));
  path.push_back(std::move(incr_step1));
  path.push_back(std::move(incr_step2));

  auto dummy_expr = std::make_unique<Expr>();

  cel::RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kDisabled;
  cel::Activation activation;
  FlatExpressionEvaluatorState state(path.size(),
                                     /*comprehension_slots_size=*/0,
                                     TypeProvider::Builtin(), manager);
  ExecutionFrame frame(path, activation, options, state);

  EXPECT_THAT(frame.Next(), Eq(path[0].get()));
  EXPECT_THAT(frame.Next(), Eq(path[1].get()));
  EXPECT_THAT(frame.Next(), Eq(path[2].get()));
  EXPECT_THAT(frame.Next(), Eq(nullptr));
}

TEST(EvaluatorCoreTest, SimpleEvaluatorTest) {
  ExecutionPath path;
  auto const_step = std::make_unique<FakeConstExpressionStep>();
  auto incr_step1 = std::make_unique<FakeIncrementExpressionStep>();
  auto incr_step2 = std::make_unique<FakeIncrementExpressionStep>();

  path.push_back(std::move(const_step));
  path.push_back(std::move(incr_step1));
  path.push_back(std::move(incr_step2));

  CelExpressionFlatImpl impl(FlatExpression(
      std::move(path), 0, cel::TypeProvider::Builtin(), cel::RuntimeOptions{}));

  Activation activation;
  google::protobuf::Arena arena;

  auto status = impl.Evaluate(activation, &arena);
  EXPECT_OK(status);

  auto value = status.value();
  EXPECT_TRUE(value.IsInt64());
  EXPECT_THAT(value.Int64OrDie(), Eq(2));
}

class MockTraceCallback {
 public:
  MOCK_METHOD(void, Call,
              (int64_t expr_id, const CelValue& value, google::protobuf::Arena*));
};

TEST(EvaluatorCoreTest, TraceTest) {
  Expr expr;
  google::api::expr::v1alpha1::SourceInfo source_info;

  // 1 && [1,2,3].all(x, x > 0)

  expr.set_id(1);
  auto and_call = expr.mutable_call_expr();
  and_call->set_function("_&&_");

  auto true_expr = and_call->add_args();
  true_expr->set_id(2);
  true_expr->mutable_const_expr()->set_int64_value(1);

  auto comp_expr = and_call->add_args();
  comp_expr->set_id(3);
  auto comp = comp_expr->mutable_comprehension_expr();
  comp->set_iter_var("x");
  comp->set_accu_var("accu");

  auto list_expr = comp->mutable_iter_range();
  list_expr->set_id(4);
  auto el1_expr = list_expr->mutable_list_expr()->add_elements();
  el1_expr->set_id(11);
  el1_expr->mutable_const_expr()->set_int64_value(1);
  auto el2_expr = list_expr->mutable_list_expr()->add_elements();
  el2_expr->set_id(12);
  el2_expr->mutable_const_expr()->set_int64_value(2);
  auto el3_expr = list_expr->mutable_list_expr()->add_elements();
  el3_expr->set_id(13);
  el3_expr->mutable_const_expr()->set_int64_value(3);

  auto accu_init_expr = comp->mutable_accu_init();
  accu_init_expr->set_id(20);
  accu_init_expr->mutable_const_expr()->set_bool_value(true);

  auto loop_cond_expr = comp->mutable_loop_condition();
  loop_cond_expr->set_id(21);
  loop_cond_expr->mutable_const_expr()->set_bool_value(true);

  auto loop_step_expr = comp->mutable_loop_step();
  loop_step_expr->set_id(22);
  auto condition = loop_step_expr->mutable_call_expr();
  condition->set_function("_>_");

  auto iter_expr = condition->add_args();
  iter_expr->set_id(23);
  iter_expr->mutable_ident_expr()->set_name("x");

  auto zero_expr = condition->add_args();
  zero_expr->set_id(24);
  zero_expr->mutable_const_expr()->set_int64_value(0);

  auto result_expr = comp->mutable_result();
  result_expr->set_id(25);
  result_expr->mutable_const_expr()->set_bool_value(true);

  cel::RuntimeOptions options;
  options.short_circuiting = false;
  CelExpressionBuilderFlatImpl builder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&expr, &source_info));

  Activation activation;
  google::protobuf::Arena arena;

  MockTraceCallback callback;

  EXPECT_CALL(callback, Call(accu_init_expr->id(), _, &arena));
  EXPECT_CALL(callback, Call(el1_expr->id(), _, &arena));
  EXPECT_CALL(callback, Call(el2_expr->id(), _, &arena));
  EXPECT_CALL(callback, Call(el3_expr->id(), _, &arena));

  EXPECT_CALL(callback, Call(list_expr->id(), _, &arena));

  EXPECT_CALL(callback, Call(loop_cond_expr->id(), _, &arena)).Times(3);
  EXPECT_CALL(callback, Call(iter_expr->id(), _, &arena)).Times(3);
  EXPECT_CALL(callback, Call(zero_expr->id(), _, &arena)).Times(3);
  EXPECT_CALL(callback, Call(loop_step_expr->id(), _, &arena)).Times(3);

  EXPECT_CALL(callback, Call(result_expr->id(), _, &arena));
  EXPECT_CALL(callback, Call(comp_expr->id(), _, &arena));
  EXPECT_CALL(callback, Call(true_expr->id(), _, &arena));
  EXPECT_CALL(callback, Call(expr.id(), _, &arena));

  auto eval_status = cel_expr->Trace(
      activation, &arena,
      [&](int64_t expr_id, const CelValue& value, google::protobuf::Arena* arena) {
        callback.Call(expr_id, value, arena);
        return absl::OkStatus();
      });
  ASSERT_OK(eval_status);
}

}  // namespace google::api::expr::runtime
