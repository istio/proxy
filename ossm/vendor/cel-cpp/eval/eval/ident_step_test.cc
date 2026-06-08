#include "eval/eval/ident_step.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "base/type_provider.h"
#include "common/casting.h"
#include "common/expr.h"
#include "common/memory.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/cel_expression_flat_impl.h"
#include "eval/eval/evaluator_core.h"
#include "eval/public/activation.h"
#include "eval/public/cel_attribute.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "runtime/activation.h"
#include "runtime/internal/runtime_env_testing.h"
#include "runtime/internal/runtime_type_provider.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {

namespace {

using ::absl_testing::StatusIs;
using ::cel::Cast;
using ::cel::ErrorValue;
using ::cel::Expr;
using ::cel::InstanceOf;
using ::cel::IntValue;
using ::cel::MemoryManagerRef;
using ::cel::RuntimeOptions;
using ::cel::TypeProvider;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::runtime_internal::NewTestingRuntimeEnv;
using ::google::protobuf::Arena;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::SizeIs;

TEST(IdentStepTest, TestIdentStep) {
  Expr expr;
  auto& ident_expr = expr.mutable_ident_expr();
  ident_expr.set_name("name0");

  ASSERT_OK_AND_ASSIGN(auto step, CreateIdentStep(ident_expr, expr.id()));

  ExecutionPath path;
  path.push_back(std::move(step));

  auto env = NewTestingRuntimeEnv();
  CelExpressionFlatImpl impl(
      env, FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                          env->type_registry.GetComposedTypeProvider(),
                          cel::RuntimeOptions{}));

  Activation activation;
  Arena arena;
  std::string value("test");

  activation.InsertValue("name0", CelValue::CreateString(&value));
  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  CelValue result = status0.value();

  ASSERT_TRUE(result.IsString());
  EXPECT_THAT(result.StringOrDie().value(), Eq("test"));
}

TEST(IdentStepTest, TestIdentStepNameNotFound) {
  Expr expr;
  auto& ident_expr = expr.mutable_ident_expr();
  ident_expr.set_name("name0");

  ASSERT_OK_AND_ASSIGN(auto step, CreateIdentStep(ident_expr, expr.id()));

  ExecutionPath path;
  path.push_back(std::move(step));

  auto env = NewTestingRuntimeEnv();
  CelExpressionFlatImpl impl(
      env, FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                          env->type_registry.GetComposedTypeProvider(),
                          cel::RuntimeOptions{}));

  Activation activation;
  Arena arena;
  std::string value("test");

  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  CelValue result = status0.value();
  ASSERT_TRUE(result.IsError());
}

TEST(IdentStepTest, DisableMissingAttributeErrorsOK) {
  Expr expr;
  auto& ident_expr = expr.mutable_ident_expr();
  ident_expr.set_name("name0");

  ASSERT_OK_AND_ASSIGN(auto step, CreateIdentStep(ident_expr, expr.id()));

  ExecutionPath path;
  path.push_back(std::move(step));
  cel::RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kDisabled;
  auto env = NewTestingRuntimeEnv();
  CelExpressionFlatImpl impl(
      env,
      FlatExpression(std::move(path),
                     /*comprehension_slot_count=*/0,
                     env->type_registry.GetComposedTypeProvider(), options));

  Activation activation;
  Arena arena;
  std::string value("test");

  activation.InsertValue("name0", CelValue::CreateString(&value));
  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  CelValue result = status0.value();

  ASSERT_TRUE(result.IsString());
  EXPECT_THAT(result.StringOrDie().value(), Eq("test"));

  const CelAttributePattern pattern("name0", {});
  activation.set_missing_attribute_patterns({pattern});

  status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  EXPECT_THAT(status0->StringOrDie().value(), Eq("test"));
}

TEST(IdentStepTest, TestIdentStepMissingAttributeErrors) {
  Expr expr;
  auto& ident_expr = expr.mutable_ident_expr();
  ident_expr.set_name("name0");

  ASSERT_OK_AND_ASSIGN(auto step, CreateIdentStep(ident_expr, expr.id()));

  ExecutionPath path;
  path.push_back(std::move(step));

  cel::RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kDisabled;
  options.enable_missing_attribute_errors = true;

  auto env = NewTestingRuntimeEnv();
  CelExpressionFlatImpl impl(
      env,
      FlatExpression(std::move(path),
                     /*comprehension_slot_count=*/0,
                     env->type_registry.GetComposedTypeProvider(), options));

  Activation activation;
  Arena arena;
  std::string value("test");

  activation.InsertValue("name0", CelValue::CreateString(&value));
  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  CelValue result = status0.value();

  ASSERT_TRUE(result.IsString());
  EXPECT_THAT(result.StringOrDie().value(), Eq("test"));

  CelAttributePattern pattern("name0", {});
  activation.set_missing_attribute_patterns({pattern});

  status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  EXPECT_EQ(status0->ErrorOrDie()->code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(status0->ErrorOrDie()->message(), "MissingAttributeError: name0");
}

TEST(IdentStepTest, TestIdentStepUnknownAttribute) {
  Expr expr;
  auto& ident_expr = expr.mutable_ident_expr();
  ident_expr.set_name("name0");

  ASSERT_OK_AND_ASSIGN(auto step, CreateIdentStep(ident_expr, expr.id()));

  ExecutionPath path;
  path.push_back(std::move(step));

  // Expression with unknowns enabled.
  cel::RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  auto env = NewTestingRuntimeEnv();
  CelExpressionFlatImpl impl(
      env,
      FlatExpression(std::move(path),
                     /*comprehension_slot_count=*/0,
                     env->type_registry.GetComposedTypeProvider(), options));

  Activation activation;
  Arena arena;
  std::string value("test");

  activation.InsertValue("name0", CelValue::CreateString(&value));
  std::vector<CelAttributePattern> unknown_patterns;
  unknown_patterns.push_back(CelAttributePattern("name_bad", {}));

  activation.set_unknown_attribute_patterns(unknown_patterns);
  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  CelValue result = status0.value();

  ASSERT_TRUE(result.IsString());
  EXPECT_THAT(result.StringOrDie().value(), Eq("test"));

  unknown_patterns.push_back(CelAttributePattern("name0", {}));

  activation.set_unknown_attribute_patterns(unknown_patterns);
  status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  result = status0.value();

  ASSERT_TRUE(result.IsUnknownSet());
}

TEST(DirectIdentStepTest, Basic) {
  google::protobuf::Arena arena;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());
  cel::Activation activation;
  RuntimeOptions options;

  activation.InsertOrAssignValue("var1", IntValue(42));

  ExecutionFrameBase frame(activation, options, type_provider,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena);
  Value result;
  AttributeTrail trail;

  auto step = CreateDirectIdentStep("var1", -1);

  ASSERT_OK(step->Evaluate(frame, result, trail));

  ASSERT_TRUE(InstanceOf<IntValue>(result));
  EXPECT_THAT(Cast<IntValue>(result).NativeValue(), Eq(42));
}

TEST(DirectIdentStepTest, UnknownAttribute) {
  google::protobuf::Arena arena;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());
  cel::Activation activation;
  RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;

  activation.InsertOrAssignValue("var1", IntValue(42));
  activation.SetUnknownPatterns({CreateCelAttributePattern("var1", {})});

  ExecutionFrameBase frame(activation, options, type_provider,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena);
  Value result;
  AttributeTrail trail;

  auto step = CreateDirectIdentStep("var1", -1);

  ASSERT_OK(step->Evaluate(frame, result, trail));

  ASSERT_TRUE(InstanceOf<UnknownValue>(result));
  EXPECT_THAT(Cast<UnknownValue>(result).attribute_set(), SizeIs(1));
}

TEST(DirectIdentStepTest, MissingAttribute) {
  google::protobuf::Arena arena;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());
  cel::Activation activation;
  RuntimeOptions options;
  options.enable_missing_attribute_errors = true;

  activation.InsertOrAssignValue("var1", IntValue(42));
  activation.SetMissingPatterns({CreateCelAttributePattern("var1", {})});

  ExecutionFrameBase frame(activation, options, type_provider,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena);
  Value result;
  AttributeTrail trail;

  auto step = CreateDirectIdentStep("var1", -1);

  ASSERT_OK(step->Evaluate(frame, result, trail));

  ASSERT_TRUE(InstanceOf<ErrorValue>(result));
  EXPECT_THAT(Cast<ErrorValue>(result).NativeValue(),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("var1")));
}

TEST(DirectIdentStepTest, NotFound) {
  google::protobuf::Arena arena;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());
  cel::Activation activation;
  RuntimeOptions options;

  ExecutionFrameBase frame(activation, options, type_provider,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena);
  Value result;
  AttributeTrail trail;

  auto step = CreateDirectIdentStep("var1", -1);

  ASSERT_OK(step->Evaluate(frame, result, trail));

  ASSERT_TRUE(InstanceOf<ErrorValue>(result));
  EXPECT_THAT(Cast<ErrorValue>(result).NativeValue(),
              StatusIs(absl::StatusCode::kUnknown,
                       HasSubstr("\"var1\" found in Activation")));
}

}  // namespace

}  // namespace google::api::expr::runtime
