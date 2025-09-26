#include "eval/eval/function_step.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/builtins.h"
#include "base/type_provider.h"
#include "common/constant.h"
#include "common/expr.h"
#include "common/kind.h"
#include "common/value.h"
#include "eval/eval/cel_expression_flat_impl.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/ident_step.h"
#include "eval/internal/interop.h"
#include "eval/public/activation.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/portable_cel_function_adapter.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/testing/matchers.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/testing.h"
#include "runtime/function_overload_reference.h"
#include "runtime/function_registry.h"
#include "runtime/internal/runtime_env_testing.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_functions.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {

namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::CallExpr;
using ::cel::Expr;
using ::cel::IdentExpr;
using ::cel::TypeProvider;
using ::cel::runtime_internal::NewTestingRuntimeEnv;
using ::testing::Eq;
using ::testing::Not;
using ::testing::Truly;

int GetExprId() {
  static int id = 0;
  id++;
  return id;
}

// Simple function that takes no arguments and returns a constant value.
class ConstFunction : public CelFunction {
 public:
  explicit ConstFunction(const CelValue& value, absl::string_view name)
      : CelFunction(CreateDescriptor(name)), value_(value) {}

  static CelFunctionDescriptor CreateDescriptor(absl::string_view name) {
    return CelFunctionDescriptor{name, false, {}};
  }

  static CallExpr MakeCall(absl::string_view name) {
    CallExpr call;
    call.set_function(std::string(name));
    call.set_target(nullptr);
    return call;
  }

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    if (!args.empty()) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Bad arguments number");
    }

    *result = value_;
    return absl::OkStatus();
  }

 private:
  CelValue value_;
};

enum class ShouldReturnUnknown : bool { kYes = true, kNo = false };

class AddFunction : public CelFunction {
 public:
  AddFunction()
      : CelFunction(CreateDescriptor()), should_return_unknown_(false) {}
  explicit AddFunction(ShouldReturnUnknown should_return_unknown)
      : CelFunction(CreateDescriptor()),
        should_return_unknown_(static_cast<bool>(should_return_unknown)) {}

  static CelFunctionDescriptor CreateDescriptor() {
    return CelFunctionDescriptor{
        "_+_", false, {CelValue::Type::kInt64, CelValue::Type::kInt64}};
  }

  static CallExpr MakeCall() {
    CallExpr call;
    call.set_function("_+_");
    call.mutable_args().emplace_back();
    call.mutable_args().emplace_back();
    call.set_target(nullptr);
    return call;
  }

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    if (args.size() != 2 || !args[0].IsInt64() || !args[1].IsInt64()) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Mismatched arguments passed to method");
    }
    if (should_return_unknown_) {
      *result =
          CreateUnknownFunctionResultError(arena, "Add can't be resolved.");
      return absl::OkStatus();
    }

    int64_t arg0 = args[0].Int64OrDie();
    int64_t arg1 = args[1].Int64OrDie();

    *result = CelValue::CreateInt64(arg0 + arg1);
    return absl::OkStatus();
  }

 private:
  bool should_return_unknown_;
};

class SinkFunction : public CelFunction {
 public:
  explicit SinkFunction(CelValue::Type type, bool is_strict = true)
      : CelFunction(CreateDescriptor(type, is_strict)) {}

  static CelFunctionDescriptor CreateDescriptor(CelValue::Type type,
                                                bool is_strict = true) {
    return CelFunctionDescriptor{"Sink", false, {type}, is_strict};
  }

  static CallExpr MakeCall() {
    CallExpr call;
    call.set_function("Sink");
    call.mutable_args().emplace_back();
    call.set_target(nullptr);
    return call;
  }

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    // Return value is ignored.
    *result = CelValue::CreateInt64(0);
    return absl::OkStatus();
  }
};

// Create and initialize a registry with some default functions.
void AddDefaults(CelFunctionRegistry& registry) {
  static UnknownSet* unknown_set = new UnknownSet();
  EXPECT_TRUE(registry
                  .Register(std::make_unique<ConstFunction>(
                      CelValue::CreateInt64(3), "Const3"))
                  .ok());
  EXPECT_TRUE(registry
                  .Register(std::make_unique<ConstFunction>(
                      CelValue::CreateInt64(2), "Const2"))
                  .ok());
  EXPECT_TRUE(registry
                  .Register(std::make_unique<ConstFunction>(
                      CelValue::CreateUnknownSet(unknown_set), "ConstUnknown"))
                  .ok());
  EXPECT_TRUE(registry.Register(std::make_unique<AddFunction>()).ok());

  EXPECT_TRUE(
      registry.Register(std::make_unique<SinkFunction>(CelValue::Type::kList))
          .ok());

  EXPECT_TRUE(
      registry.Register(std::make_unique<SinkFunction>(CelValue::Type::kMap))
          .ok());

  EXPECT_TRUE(
      registry
          .Register(std::make_unique<SinkFunction>(CelValue::Type::kMessage))
          .ok());
}

std::vector<CelValue::Type> ArgumentMatcher(int argument_count) {
  std::vector<CelValue::Type> argument_matcher(argument_count);
  for (int i = 0; i < argument_count; i++) {
    argument_matcher[i] = CelValue::Type::kAny;
  }
  return argument_matcher;
}

std::vector<CelValue::Type> ArgumentMatcher(const CallExpr& call) {
  return ArgumentMatcher(call.has_target() ? call.args().size() + 1
                                           : call.args().size());
}

std::unique_ptr<CelExpressionFlatImpl> CreateExpressionImpl(
    const cel::RuntimeOptions& options,
    std::unique_ptr<DirectExpressionStep> expr) {
  ExecutionPath path;
  path.push_back(std::make_unique<WrappedDirectStep>(std::move(expr), -1));

  auto env = NewTestingRuntimeEnv();
  return std::make_unique<CelExpressionFlatImpl>(
      env,
      FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                     env->type_registry.GetComposedTypeProvider(), options));
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> MakeTestFunctionStep(
    const CallExpr& call, const CelFunctionRegistry& registry) {
  auto argument_matcher = ArgumentMatcher(call);
  auto lazy_overloads = registry.ModernFindLazyOverloads(
      call.function(), call.has_target(), argument_matcher);
  if (!lazy_overloads.empty()) {
    return CreateFunctionStep(call, GetExprId(), lazy_overloads);
  }
  auto overloads = registry.FindStaticOverloads(
      call.function(), call.has_target(), argument_matcher);
  return CreateFunctionStep(call, GetExprId(), overloads);
}

// Test common functions with varying levels of unknown support.
class FunctionStepTest
    : public testing::TestWithParam<UnknownProcessingOptions> {
 public:
  // underlying expression impl moves path
  std::unique_ptr<CelExpressionFlatImpl> GetExpression(ExecutionPath&& path) {
    cel::RuntimeOptions options;
    options.unknown_processing = GetParam();

    auto env = NewTestingRuntimeEnv();
    return std::make_unique<CelExpressionFlatImpl>(
        env,
        FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                       env->type_registry.GetComposedTypeProvider(), options));
  }
};

TEST_P(FunctionStepTest, SimpleFunctionTest) {
  ExecutionPath path;

  CelFunctionRegistry registry;
  AddDefaults(registry);

  CallExpr call1 = ConstFunction::MakeCall("Const3");
  CallExpr call2 = ConstFunction::MakeCall("Const2");
  CallExpr add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsInt64());
  EXPECT_THAT(value.Int64OrDie(), Eq(5));
}

TEST_P(FunctionStepTest, TestStackUnderflow) {
  ExecutionPath path;

  CelFunctionRegistry registry;
  AddDefaults(registry);

  AddFunction add_func;

  CallExpr call1 = ConstFunction::MakeCall("Const3");
  CallExpr add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step2));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  EXPECT_THAT(impl->Evaluate(activation, &arena), Not(IsOk()));
}

// Test situation when no overloads match input arguments during evaluation.
TEST_P(FunctionStepTest, TestNoMatchingOverloadsDuringEvaluation) {
  ExecutionPath path;

  CelFunctionRegistry registry;
  AddDefaults(registry);

  ASSERT_TRUE(registry
                  .Register(std::make_unique<ConstFunction>(
                      CelValue::CreateUint64(4), "Const4"))
                  .ok());

  CallExpr call1 = ConstFunction::MakeCall("Const3");
  CallExpr call2 = ConstFunction::MakeCall("Const4");
  // Add expects {int64, int64} but it's {int64, uint64}.
  CallExpr add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsError());
  EXPECT_THAT(*value.ErrorOrDie(),
              StatusIs(absl::StatusCode::kUnknown,
                       testing::HasSubstr("_+_(int64, uint64)")));
}

// Test situation when no overloads match input arguments during evaluation.
TEST_P(FunctionStepTest, TestNoMatchingOverloadsUnexpectedArgCount) {
  ExecutionPath path;

  CelFunctionRegistry registry;
  AddDefaults(registry);

  CallExpr call1 = ConstFunction::MakeCall("Const3");

  // expect overloads for {int64, int64} but get call for {int64, int64, int64}.
  CallExpr add_call = AddFunction::MakeCall();
  add_call.mutable_args().emplace_back();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(call1, registry));

  ASSERT_OK_AND_ASSIGN(
      auto step3,
      CreateFunctionStep(add_call, -1,
                         registry.FindStaticOverloads(
                             add_call.function(), false,
                             {cel::Kind::kInt64, cel::Kind::kInt64})));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));
  path.push_back(std::move(step3));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsError());
  EXPECT_THAT(*value.ErrorOrDie(),
              StatusIs(absl::StatusCode::kUnknown,
                       testing::HasSubstr("_+_(int64, int64, int64)")));
}

// Test situation when no overloads match input arguments during evaluation
// and at least one of arguments is error.
TEST_P(FunctionStepTest,
       TestNoMatchingOverloadsDuringEvaluationErrorForwarding) {
  ExecutionPath path;
  CelFunctionRegistry registry;
  AddDefaults(registry);

  CelError error0 = absl::CancelledError();
  CelError error1 = absl::CancelledError();

  // Constants have ERROR type, while AddFunction expects INT.
  ASSERT_TRUE(registry
                  .Register(std::make_unique<ConstFunction>(
                      CelValue::CreateError(&error0), "ConstError1"))
                  .ok());
  ASSERT_TRUE(registry
                  .Register(std::make_unique<ConstFunction>(
                      CelValue::CreateError(&error1), "ConstError2"))
                  .ok());

  CallExpr call1 = ConstFunction::MakeCall("ConstError1");
  CallExpr call2 = ConstFunction::MakeCall("ConstError2");
  CallExpr add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsError());
  EXPECT_THAT(*value.ErrorOrDie(), Eq(error0));
}

TEST_P(FunctionStepTest, LazyFunctionTest) {
  ExecutionPath path;
  Activation activation;
  CelFunctionRegistry registry;
  ASSERT_OK(
      registry.RegisterLazyFunction(ConstFunction::CreateDescriptor("Const3")));
  ASSERT_OK(activation.InsertFunction(
      std::make_unique<ConstFunction>(CelValue::CreateInt64(3), "Const3")));
  ASSERT_OK(
      registry.RegisterLazyFunction(ConstFunction::CreateDescriptor("Const2")));
  ASSERT_OK(activation.InsertFunction(
      std::make_unique<ConstFunction>(CelValue::CreateInt64(2), "Const2")));
  ASSERT_OK(registry.Register(std::make_unique<AddFunction>()));

  CallExpr call1 = ConstFunction::MakeCall("Const3");
  CallExpr call2 = ConstFunction::MakeCall("Const2");
  CallExpr add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsInt64());
  EXPECT_THAT(value.Int64OrDie(), Eq(5));
}

TEST_P(FunctionStepTest, LazyFunctionOverloadingTest) {
  ExecutionPath path;
  Activation activation;
  CelFunctionRegistry registry;
  auto floor_int = PortableUnaryFunctionAdapter<int64_t, int64_t>::Create(
      "Floor", false, [](google::protobuf::Arena*, int64_t val) { return val; });
  auto floor_double = PortableUnaryFunctionAdapter<int64_t, double>::Create(
      "Floor", false,
      [](google::protobuf::Arena*, double val) { return std::floor(val); });

  ASSERT_OK(registry.RegisterLazyFunction(floor_int->descriptor()));
  ASSERT_OK(activation.InsertFunction(std::move(floor_int)));
  ASSERT_OK(registry.RegisterLazyFunction(floor_double->descriptor()));
  ASSERT_OK(activation.InsertFunction(std::move(floor_double)));
  ASSERT_OK(registry.Register(
      PortableBinaryFunctionAdapter<bool, int64_t, int64_t>::Create(
          "_<_", false, [](google::protobuf::Arena*, int64_t lhs, int64_t rhs) -> bool {
            return lhs < rhs;
          })));

  cel::Constant lhs;
  lhs.set_int64_value(20);
  cel::Constant rhs;
  rhs.set_double_value(21.9);

  CallExpr call1;
  call1.mutable_args().emplace_back();
  call1.set_function("Floor");
  CallExpr call2;
  call2.mutable_args().emplace_back();
  call2.set_function("Floor");

  CallExpr lt_call;
  lt_call.mutable_args().emplace_back();
  lt_call.mutable_args().emplace_back();
  lt_call.set_function("_<_");

  ASSERT_OK_AND_ASSIGN(
      auto step0,
      CreateConstValueStep(cel::interop_internal::CreateIntValue(20), -1));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(
      auto step2,
      CreateConstValueStep(cel::interop_internal::CreateDoubleValue(21.9), -1));
  ASSERT_OK_AND_ASSIGN(auto step3, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step4, MakeTestFunctionStep(lt_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));
  path.push_back(std::move(step3));
  path.push_back(std::move(step4));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsBool());
  EXPECT_TRUE(value.BoolOrDie());
}

// Test situation when no overloads match input arguments during evaluation
// and at least one of arguments is error.
TEST_P(FunctionStepTest,
       TestNoMatchingOverloadsDuringEvaluationErrorForwardingLazy) {
  ExecutionPath path;
  Activation activation;
  google::protobuf::Arena arena;
  CelFunctionRegistry registry;

  AddDefaults(registry);

  CelError error0 = absl::CancelledError();
  CelError error1 = absl::CancelledError();

  // Constants have ERROR type, while AddFunction expects INT.
  ASSERT_OK(registry.RegisterLazyFunction(
      ConstFunction::CreateDescriptor("ConstError1")));
  ASSERT_OK(activation.InsertFunction(std::make_unique<ConstFunction>(
      CelValue::CreateError(&error0), "ConstError1")));
  ASSERT_OK(registry.RegisterLazyFunction(
      ConstFunction::CreateDescriptor("ConstError2")));
  ASSERT_OK(activation.InsertFunction(std::make_unique<ConstFunction>(
      CelValue::CreateError(&error1), "ConstError2")));

  CallExpr call1 = ConstFunction::MakeCall("ConstError1");
  CallExpr call2 = ConstFunction::MakeCall("ConstError2");
  CallExpr add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsError());
  EXPECT_THAT(*value.ErrorOrDie(), Eq(error0));
}

std::string TestNameFn(testing::TestParamInfo<UnknownProcessingOptions> opt) {
  switch (opt.param) {
    case UnknownProcessingOptions::kDisabled:
      return "disabled";
    case UnknownProcessingOptions::kAttributeOnly:
      return "attribute_only";
    case UnknownProcessingOptions::kAttributeAndFunction:
      return "attribute_and_function";
  }
  return "";
}

INSTANTIATE_TEST_SUITE_P(
    UnknownSupport, FunctionStepTest,
    testing::Values(UnknownProcessingOptions::kDisabled,
                    UnknownProcessingOptions::kAttributeOnly,
                    UnknownProcessingOptions::kAttributeAndFunction),
    &TestNameFn);

class FunctionStepTestUnknowns
    : public testing::TestWithParam<UnknownProcessingOptions> {
 public:
  std::unique_ptr<CelExpressionFlatImpl> GetExpression(ExecutionPath&& path) {
    cel::RuntimeOptions options;
    options.unknown_processing = GetParam();

    auto env = NewTestingRuntimeEnv();
    return std::make_unique<CelExpressionFlatImpl>(
        env,
        FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                       env->type_registry.GetComposedTypeProvider(), options));
  }
};

TEST_P(FunctionStepTestUnknowns, PassedUnknownTest) {
  ExecutionPath path;

  CelFunctionRegistry registry;
  AddDefaults(registry);

  CallExpr call1 = ConstFunction::MakeCall("Const3");
  CallExpr call2 = ConstFunction::MakeCall("ConstUnknown");
  CallExpr add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsUnknownSet());
}

TEST_P(FunctionStepTestUnknowns, PartialUnknownHandlingTest) {
  ExecutionPath path;

  CelFunctionRegistry registry;
  AddDefaults(registry);

  // Build the expression path that corresponds to CEL expression
  // "sink(param)".
  IdentExpr ident1;
  ident1.set_name("param");
  CallExpr call1 = SinkFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, CreateIdentStep(ident1, GetExprId()));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call1, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  TestMessage msg;
  google::protobuf::Arena arena;
  activation.InsertValue("param", CelProtoWrapper::CreateMessage(&msg, &arena));
  CelAttributePattern pattern(
      "param",
      {CreateCelAttributeQualifierPattern(CelValue::CreateBool(true))});

  // Set attribute pattern that marks attribute "param[true]" as unknown.
  // It should result in "param" being handled as partially unknown, which is
  // is handled as fully unknown when used as function input argument.
  activation.set_unknown_attribute_patterns({pattern});

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsUnknownSet());
}

TEST_P(FunctionStepTestUnknowns, UnknownVsErrorPrecedenceTest) {
  ExecutionPath path;
  CelFunctionRegistry registry;
  AddDefaults(registry);

  CelError error0 = absl::CancelledError();
  CelValue error_value = CelValue::CreateError(&error0);

  ASSERT_TRUE(
      registry
          .Register(std::make_unique<ConstFunction>(error_value, "ConstError"))
          .ok());

  CallExpr call1 = ConstFunction::MakeCall("ConstError");
  CallExpr call2 = ConstFunction::MakeCall("ConstUnknown");
  CallExpr add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsError());
  // Making sure we propagate the error.
  ASSERT_EQ(*value.ErrorOrDie(), *error_value.ErrorOrDie());
}

INSTANTIATE_TEST_SUITE_P(
    UnknownFunctionSupport, FunctionStepTestUnknowns,
    testing::Values(UnknownProcessingOptions::kAttributeOnly,
                    UnknownProcessingOptions::kAttributeAndFunction),
    &TestNameFn);

TEST(FunctionStepTestUnknownFunctionResults, CaptureArgs) {
  ExecutionPath path;
  CelFunctionRegistry registry;

  ASSERT_OK(registry.Register(
      std::make_unique<ConstFunction>(CelValue::CreateInt64(2), "Const2")));
  ASSERT_OK(registry.Register(
      std::make_unique<ConstFunction>(CelValue::CreateInt64(3), "Const3")));
  ASSERT_OK(registry.Register(
      std::make_unique<AddFunction>(ShouldReturnUnknown::kYes)));

  CallExpr call1 = ConstFunction::MakeCall("Const2");
  CallExpr call2 = ConstFunction::MakeCall("Const3");
  CallExpr add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));
  cel::RuntimeOptions options;
  options.unknown_processing =
      cel::UnknownProcessingOptions::kAttributeAndFunction;
  auto env = NewTestingRuntimeEnv();
  CelExpressionFlatImpl impl(
      env,
      FlatExpression(std::move(path),
                     /*comprehension_slot_count=*/0,
                     env->type_registry.GetComposedTypeProvider(), options));

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsUnknownSet());
}

TEST(FunctionStepTestUnknownFunctionResults, MergeDownCaptureArgs) {
  ExecutionPath path;
  CelFunctionRegistry registry;

  ASSERT_OK(registry.Register(
      std::make_unique<ConstFunction>(CelValue::CreateInt64(2), "Const2")));
  ASSERT_OK(registry.Register(
      std::make_unique<ConstFunction>(CelValue::CreateInt64(3), "Const3")));
  ASSERT_OK(registry.Register(
      std::make_unique<AddFunction>(ShouldReturnUnknown::kYes)));

  // Add(Add(2, 3), Add(2, 3))

  CallExpr call1 = ConstFunction::MakeCall("Const2");
  CallExpr call2 = ConstFunction::MakeCall("Const3");
  CallExpr add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));
  ASSERT_OK_AND_ASSIGN(auto step3, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step4, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step5, MakeTestFunctionStep(add_call, registry));
  ASSERT_OK_AND_ASSIGN(auto step6, MakeTestFunctionStep(add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));
  path.push_back(std::move(step3));
  path.push_back(std::move(step4));
  path.push_back(std::move(step5));
  path.push_back(std::move(step6));

  cel::RuntimeOptions options;
  options.unknown_processing =
      cel::UnknownProcessingOptions::kAttributeAndFunction;
  auto env = NewTestingRuntimeEnv();
  CelExpressionFlatImpl impl(
      env,
      FlatExpression(std::move(path),
                     /*comprehension_slot_count=*/0,
                     env->type_registry.GetComposedTypeProvider(), options));

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsUnknownSet());
}

TEST(FunctionStepTestUnknownFunctionResults, MergeCaptureArgs) {
  ExecutionPath path;
  CelFunctionRegistry registry;

  ASSERT_OK(registry.Register(
      std::make_unique<ConstFunction>(CelValue::CreateInt64(2), "Const2")));
  ASSERT_OK(registry.Register(
      std::make_unique<ConstFunction>(CelValue::CreateInt64(3), "Const3")));
  ASSERT_OK(registry.Register(
      std::make_unique<AddFunction>(ShouldReturnUnknown::kYes)));

  // Add(Add(2, 3), Add(3, 2))

  CallExpr call1 = ConstFunction::MakeCall("Const2");
  CallExpr call2 = ConstFunction::MakeCall("Const3");
  CallExpr add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));
  ASSERT_OK_AND_ASSIGN(auto step3, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step4, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step5, MakeTestFunctionStep(add_call, registry));
  ASSERT_OK_AND_ASSIGN(auto step6, MakeTestFunctionStep(add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));
  path.push_back(std::move(step3));
  path.push_back(std::move(step4));
  path.push_back(std::move(step5));
  path.push_back(std::move(step6));

  cel::RuntimeOptions options;
  options.unknown_processing =
      cel::UnknownProcessingOptions::kAttributeAndFunction;
  auto env = NewTestingRuntimeEnv();
  CelExpressionFlatImpl impl(
      env,
      FlatExpression(std::move(path),
                     /*comprehension_slot_count=*/0,
                     env->type_registry.GetComposedTypeProvider(), options));

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsUnknownSet()) << *(value.ErrorOrDie());
}

TEST(FunctionStepTestUnknownFunctionResults, UnknownVsErrorPrecedenceTest) {
  ExecutionPath path;
  CelFunctionRegistry registry;

  CelError error0 = absl::CancelledError();
  CelValue error_value = CelValue::CreateError(&error0);
  UnknownSet unknown_set;
  CelValue unknown_value = CelValue::CreateUnknownSet(&unknown_set);

  ASSERT_OK(registry.Register(
      std::make_unique<ConstFunction>(error_value, "ConstError")));
  ASSERT_OK(registry.Register(
      std::make_unique<ConstFunction>(unknown_value, "ConstUnknown")));
  ASSERT_OK(registry.Register(
      std::make_unique<AddFunction>(ShouldReturnUnknown::kYes)));

  CallExpr call1 = ConstFunction::MakeCall("ConstError");
  CallExpr call2 = ConstFunction::MakeCall("ConstUnknown");
  CallExpr add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  cel::RuntimeOptions options;
  options.unknown_processing =
      cel::UnknownProcessingOptions::kAttributeAndFunction;
  auto env = NewTestingRuntimeEnv();
  CelExpressionFlatImpl impl(
      env,
      FlatExpression(std::move(path),
                     /*comprehension_slot_count=*/0,
                     env->type_registry.GetComposedTypeProvider(), options));

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsError());
  // Making sure we propagate the error.
  ASSERT_EQ(*value.ErrorOrDie(), *error_value.ErrorOrDie());
}

class MessageFunction : public CelFunction {
 public:
  MessageFunction()
      : CelFunction(
            CelFunctionDescriptor("Fn", false, {CelValue::Type::kMessage})) {}

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    if (args.size() != 1 || !args.at(0).IsMessage()) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Bad arguments number");
    }

    *result = CelValue::CreateStringView("message");
    return absl::OkStatus();
  }
};

class MessageIdentityFunction : public CelFunction {
 public:
  MessageIdentityFunction()
      : CelFunction(
            CelFunctionDescriptor("Fn", false, {CelValue::Type::kMessage})) {}

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    if (args.size() != 1 || !args.at(0).IsMessage()) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Bad arguments number");
    }

    *result = args.at(0);
    return absl::OkStatus();
  }
};

class NullFunction : public CelFunction {
 public:
  NullFunction()
      : CelFunction(
            CelFunctionDescriptor("Fn", false, {CelValue::Type::kNullType})) {}

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    if (args.size() != 1 || args.at(0).type() != CelValue::Type::kNullType) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Bad arguments number");
    }

    *result = CelValue::CreateStringView("null");
    return absl::OkStatus();
  }
};

TEST(FunctionStepStrictnessTest,
     IfFunctionStrictAndGivenUnknownSkipsInvocation) {
  UnknownSet unknown_set;
  CelFunctionRegistry registry;
  ASSERT_OK(registry.Register(std::make_unique<ConstFunction>(
      CelValue::CreateUnknownSet(&unknown_set), "ConstUnknown")));
  ASSERT_OK(registry.Register(std::make_unique<SinkFunction>(
      CelValue::Type::kUnknownSet, /*is_strict=*/true)));
  ExecutionPath path;
  CallExpr call0 = ConstFunction::MakeCall("ConstUnknown");
  CallExpr call1 = SinkFunction::MakeCall();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ExpressionStep> step0,
                       MakeTestFunctionStep(call0, registry));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ExpressionStep> step1,
                       MakeTestFunctionStep(call1, registry));
  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  cel::RuntimeOptions options;
  options.unknown_processing =
      cel::UnknownProcessingOptions::kAttributeAndFunction;
  auto env = NewTestingRuntimeEnv();
  CelExpressionFlatImpl impl(
      env,
      FlatExpression(std::move(path),
                     /*comprehension_slot_count=*/0,
                     env->type_registry.GetComposedTypeProvider(), options));
  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsUnknownSet());
}

TEST(FunctionStepStrictnessTest, IfFunctionNonStrictAndGivenUnknownInvokesIt) {
  UnknownSet unknown_set;
  CelFunctionRegistry registry;
  ASSERT_OK(registry.Register(std::make_unique<ConstFunction>(
      CelValue::CreateUnknownSet(&unknown_set), "ConstUnknown")));
  ASSERT_OK(registry.Register(std::make_unique<SinkFunction>(
      CelValue::Type::kUnknownSet, /*is_strict=*/false)));
  ExecutionPath path;
  CallExpr call0 = ConstFunction::MakeCall("ConstUnknown");
  CallExpr call1 = SinkFunction::MakeCall();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ExpressionStep> step0,
                       MakeTestFunctionStep(call0, registry));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ExpressionStep> step1,
                       MakeTestFunctionStep(call1, registry));
  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  Expr placeholder_expr;
  cel::RuntimeOptions options;
  options.unknown_processing =
      cel::UnknownProcessingOptions::kAttributeAndFunction;
  auto env = NewTestingRuntimeEnv();
  CelExpressionFlatImpl impl(
      env,
      FlatExpression(std::move(path),
                     /*comprehension_slot_count=*/0,
                     env->type_registry.GetComposedTypeProvider(), options));
  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation, &arena));
  ASSERT_THAT(value, test::IsCelInt64(Eq(0)));
}

class DirectFunctionStepTest : public testing::Test {
 public:
  DirectFunctionStepTest() = default;

  void SetUp() override {
    ASSERT_OK(cel::RegisterStandardFunctions(registry_, options_));
  }

  std::vector<cel::FunctionOverloadReference> GetOverloads(
      absl::string_view name, int64_t arguments_size) {
    std::vector<cel::Kind> matcher;
    matcher.resize(arguments_size, cel::Kind::kAny);
    return registry_.FindStaticOverloads(name, false, matcher);
  }

  // Helper for shorthand constructing direct expr deps.
  //
  // Works around copies in init-list construction.
  std::vector<std::unique_ptr<DirectExpressionStep>> MakeDeps(
      std::unique_ptr<DirectExpressionStep> dep,
      std::unique_ptr<DirectExpressionStep> dep2) {
    std::vector<std::unique_ptr<DirectExpressionStep>> result;
    result.reserve(2);
    result.push_back(std::move(dep));
    result.push_back(std::move(dep2));
    return result;
  };

 protected:
  cel::FunctionRegistry registry_;
  cel::RuntimeOptions options_;
  google::protobuf::Arena arena_;
};

TEST_F(DirectFunctionStepTest, SimpleCall) {
  cel::IntValue(1);

  CallExpr call;
  call.set_function(cel::builtin::kAdd);
  call.mutable_args().emplace_back();
  call.mutable_args().emplace_back();

  std::vector<std::unique_ptr<DirectExpressionStep>> deps;
  deps.push_back(CreateConstValueDirectStep(cel::IntValue(1)));
  deps.push_back(CreateConstValueDirectStep(cel::IntValue(1)));

  auto expr = CreateDirectFunctionStep(-1, call, std::move(deps),
                                       GetOverloads(cel::builtin::kAdd, 2));

  auto plan = CreateExpressionImpl(options_, std::move(expr));

  Activation activation;
  ASSERT_OK_AND_ASSIGN(auto value, plan->Evaluate(activation, &arena_));

  EXPECT_THAT(value, test::IsCelInt64(2));
}

TEST_F(DirectFunctionStepTest, RecursiveCall) {
  cel::IntValue(1);

  CallExpr call;
  call.set_function(cel::builtin::kAdd);
  call.mutable_args().emplace_back();
  call.mutable_args().emplace_back();

  auto overloads = GetOverloads(cel::builtin::kAdd, 2);

  auto MakeLeaf = [&]() {
    return CreateDirectFunctionStep(
        -1, call,
        MakeDeps(CreateConstValueDirectStep(cel::IntValue(1)),
                 CreateConstValueDirectStep(cel::IntValue(1))),
        overloads);
  };

  auto expr = CreateDirectFunctionStep(
      -1, call,
      MakeDeps(CreateDirectFunctionStep(
                   -1, call, MakeDeps(MakeLeaf(), MakeLeaf()), overloads),
               CreateDirectFunctionStep(
                   -1, call, MakeDeps(MakeLeaf(), MakeLeaf()), overloads)),
      overloads);

  auto plan = CreateExpressionImpl(options_, std::move(expr));

  Activation activation;
  ASSERT_OK_AND_ASSIGN(auto value, plan->Evaluate(activation, &arena_));

  EXPECT_THAT(value, test::IsCelInt64(8));
}

TEST_F(DirectFunctionStepTest, ErrorHandlingCall) {
  cel::IntValue(1);

  CallExpr add_call;
  add_call.set_function(cel::builtin::kAdd);
  add_call.mutable_args().emplace_back();
  add_call.mutable_args().emplace_back();

  CallExpr div_call;
  div_call.set_function(cel::builtin::kDivide);
  div_call.mutable_args().emplace_back();
  div_call.mutable_args().emplace_back();

  auto add_overloads = GetOverloads(cel::builtin::kAdd, 2);
  auto div_overloads = GetOverloads(cel::builtin::kDivide, 2);

  auto error_expr = CreateDirectFunctionStep(
      -1, div_call,
      MakeDeps(CreateConstValueDirectStep(cel::IntValue(1)),
               CreateConstValueDirectStep(cel::IntValue(0))),
      div_overloads);

  auto expr = CreateDirectFunctionStep(
      -1, add_call,
      MakeDeps(std::move(error_expr),
               CreateConstValueDirectStep(cel::IntValue(1))),
      add_overloads);

  auto plan = CreateExpressionImpl(options_, std::move(expr));

  Activation activation;
  ASSERT_OK_AND_ASSIGN(auto value, plan->Evaluate(activation, &arena_));

  EXPECT_THAT(value,
              test::IsCelError(StatusIs(absl::StatusCode::kInvalidArgument,
                                        testing::HasSubstr("divide by zero"))));
}

TEST_F(DirectFunctionStepTest, NoOverload) {
  cel::IntValue(1);

  CallExpr call;
  call.set_function(cel::builtin::kAdd);
  call.mutable_args().emplace_back();
  call.mutable_args().emplace_back();

  std::vector<std::unique_ptr<DirectExpressionStep>> deps;
  deps.push_back(CreateConstValueDirectStep(cel::IntValue(1)));
  deps.push_back(CreateConstValueDirectStep(cel::StringValue("2")));

  auto expr = CreateDirectFunctionStep(-1, call, std::move(deps),
                                       GetOverloads(cel::builtin::kAdd, 2));

  auto plan = CreateExpressionImpl(options_, std::move(expr));

  Activation activation;
  ASSERT_OK_AND_ASSIGN(auto value, plan->Evaluate(activation, &arena_));

  EXPECT_THAT(value, Truly(CheckNoMatchingOverloadError));
}

TEST_F(DirectFunctionStepTest, NoOverload0Args) {
  cel::IntValue(1);

  CallExpr call;
  call.set_function(cel::builtin::kAdd);

  std::vector<std::unique_ptr<DirectExpressionStep>> deps;
  auto expr = CreateDirectFunctionStep(-1, call, std::move(deps),
                                       GetOverloads(cel::builtin::kAdd, 2));

  auto plan = CreateExpressionImpl(options_, std::move(expr));

  Activation activation;
  ASSERT_OK_AND_ASSIGN(auto value, plan->Evaluate(activation, &arena_));

  EXPECT_THAT(value, Truly(CheckNoMatchingOverloadError));
}

}  // namespace
}  // namespace google::api::expr::runtime
