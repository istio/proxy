// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/eval/equality_steps.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "base/attribute.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/value_testing.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "runtime/activation.h"
#include "runtime/internal/runtime_type_provider.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {
namespace {

using ::absl_testing::IsOk;
using ::cel::Attribute;
using ::cel::DoubleValue;
using ::cel::ErrorValue;
using ::cel::IntValue;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::ValueKind;
using ::cel::test::BoolValueIs;
using ::cel::test::ValueKindIs;

class ValueStep : public ExpressionStep, public DirectExpressionStep {
 public:
  ValueStep(Value value, Attribute attr)
      : ExpressionStep(-1),
        DirectExpressionStep(-1),
        value_(std::move(value)),
        attr_(std::move(attr)) {}
  explicit ValueStep(Value value)
      : ExpressionStep(-1),
        DirectExpressionStep(-1),
        value_(std::move(value)),
        attr_() {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    frame->value_stack().Push(value_, attr_);
    return absl::OkStatus();
  }

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute_trail) const override {
    result = value_;
    attribute_trail = attr_;
    return absl::OkStatus();
  }

 private:
  Value value_;
  AttributeTrail attr_;
};

TEST(RecursiveTest, PartialAttrUnknown) {
  cel::Activation activation;
  google::protobuf::Arena arena;
  cel::RuntimeOptions opts;
  opts.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());

  // A little contrived for simplicity, but this is for cases where e.g.
  // `msg == Msg{}` but msg.foo is unknown.
  auto plan = CreateDirectEqualityStep(
      std::make_unique<ValueStep>(IntValue(1), cel::Attribute("foo")),
      std::make_unique<ValueStep>(IntValue(2)), false, -1);

  activation.SetUnknownPatterns({cel::AttributePattern(
      "foo", {cel::AttributeQualifierPattern::OfString("bar")})});

  ExecutionFrameBase frame(activation, opts, type_provider,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena);

  cel::Value result;
  AttributeTrail attribute_trail;
  ASSERT_THAT(plan->Evaluate(frame, result, attribute_trail), IsOk());

  EXPECT_THAT(result, ValueKindIs(ValueKind::kUnknown));
}

TEST(RecursiveTest, PartialAttrUnknownDisabled) {
  cel::Activation activation;
  google::protobuf::Arena arena;
  cel::RuntimeOptions opts;
  opts.unknown_processing = cel::UnknownProcessingOptions::kDisabled;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());

  auto plan = CreateDirectEqualityStep(
      std::make_unique<ValueStep>(IntValue(1), cel::Attribute("foo")),
      std::make_unique<ValueStep>(IntValue(2)), false, -1);

  activation.SetUnknownPatterns({cel::AttributePattern(
      "foo", {cel::AttributeQualifierPattern::OfString("bar")})});
  ExecutionFrameBase frame(activation, opts, type_provider,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena);

  cel::Value result;
  AttributeTrail attribute_trail;
  ASSERT_THAT(plan->Evaluate(frame, result, attribute_trail), IsOk());

  EXPECT_THAT(result, BoolValueIs(false));
}

TEST(IterativeTest, PartialAttrUnknown) {
  cel::Activation activation;
  google::protobuf::Arena arena;
  cel::RuntimeOptions opts;
  opts.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());

  FlatExpressionEvaluatorState state(
      /*value_stack_size=*/5,
      /*comprehension_slot_count=*/0, type_provider,
      cel::internal::GetTestingDescriptorPool(),
      cel::internal::GetTestingMessageFactory(), &arena);

  std::vector<std::unique_ptr<const ExpressionStep>> steps;
  steps.push_back(
      std::make_unique<ValueStep>(IntValue(1), cel::Attribute("foo")));
  steps.push_back(std::make_unique<ValueStep>(IntValue(2)));
  steps.push_back(CreateEqualityStep(false, -1));

  activation.SetUnknownPatterns({cel::AttributePattern(
      "foo", {cel::AttributeQualifierPattern::OfString("bar")})});

  ExecutionFrame frame(steps, activation, opts, state);

  ASSERT_OK_AND_ASSIGN(Value result, frame.Evaluate());

  EXPECT_THAT(result, ValueKindIs(ValueKind::kUnknown));
}

TEST(IterativeTest, PartialAttrUnknownDisabled) {
  cel::Activation activation;
  google::protobuf::Arena arena;
  cel::RuntimeOptions opts;
  opts.unknown_processing = cel::UnknownProcessingOptions::kDisabled;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());

  FlatExpressionEvaluatorState state(
      /*value_stack_size=*/5,
      /*comprehension_slot_count=*/0, type_provider,
      cel::internal::GetTestingDescriptorPool(),
      cel::internal::GetTestingMessageFactory(), &arena);

  std::vector<std::unique_ptr<const ExpressionStep>> steps;
  steps.push_back(
      std::make_unique<ValueStep>(IntValue(1), cel::Attribute("foo")));
  steps.push_back(std::make_unique<ValueStep>(IntValue(2)));
  steps.push_back(CreateEqualityStep(false, -1));

  activation.SetUnknownPatterns({cel::AttributePattern(
      "foo", {cel::AttributeQualifierPattern::OfString("bar")})});
  ExecutionFrame frame(steps, activation, opts, state);

  ASSERT_OK_AND_ASSIGN(Value result, frame.Evaluate());

  EXPECT_THAT(result, BoolValueIs(false));
}

enum class InputType { kInt1, kInt2, kDouble1, kList, kMap, kError, kUnknown };
enum class OutputType { kBoolTrue, kBoolFalse, kError, kUnknown };

struct EqualsTestCase {
  InputType lhs;
  InputType rhs;
  bool negation;
  OutputType expected_result;
};

class EqualsTest : public ::testing::TestWithParam<EqualsTestCase> {};

Value MakeValue(InputType type, google::protobuf::Arena* absl_nonnull arena) {
  switch (type) {
    case InputType::kInt1:
      return IntValue(1);
    case InputType::kInt2:
      return IntValue(2);
    case InputType::kDouble1:
      return DoubleValue(1.0);
    case InputType::kUnknown:
      return UnknownValue();
    case InputType::kList: {
      auto builder = cel::NewListValueBuilder(arena);
      ABSL_CHECK_OK((builder)->Add(IntValue(1)));
      return (std::move(*builder)).Build();
    }
    case InputType::kMap: {
      auto builder = cel::NewMapValueBuilder(arena);
      ABSL_CHECK_OK((builder)->Put(IntValue(1), IntValue(2)));
      return (std::move(*builder)).Build();
    }
    case InputType::kError:
    default:
      return ErrorValue(absl::InternalError("error"));
  }
}

TEST_P(EqualsTest, Recursive) {
  const EqualsTestCase& test_case = GetParam();
  cel::Activation activation;
  google::protobuf::Arena arena;
  cel::RuntimeOptions opts;
  opts.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());

  auto plan = CreateDirectEqualityStep(
      std::make_unique<ValueStep>(MakeValue(test_case.lhs, &arena)),
      std::make_unique<ValueStep>(MakeValue(test_case.rhs, &arena)),
      test_case.negation, -1);

  ExecutionFrameBase frame(activation, opts, type_provider,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena);

  cel::Value result;
  AttributeTrail attribute_trail;
  ASSERT_THAT(plan->Evaluate(frame, result, attribute_trail), IsOk());

  switch (test_case.expected_result) {
    case OutputType::kBoolTrue:
      EXPECT_THAT(result, BoolValueIs(true));
      break;
    case OutputType::kBoolFalse:
      EXPECT_THAT(result, BoolValueIs(false));
      break;
    case OutputType::kError:
      EXPECT_THAT(result, ValueKindIs(ValueKind::kError));
      break;
    case OutputType::kUnknown:
      EXPECT_THAT(result, ValueKindIs(ValueKind::kUnknown));
      break;
  }
}

TEST_P(EqualsTest, Iterative) {
  const EqualsTestCase& test_case = GetParam();
  cel::Activation activation;
  google::protobuf::Arena arena;
  cel::RuntimeOptions opts;
  opts.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());

  FlatExpressionEvaluatorState state(
      /*value_stack_size=*/5,
      /*comprehension_slot_count=*/0, type_provider,
      cel::internal::GetTestingDescriptorPool(),
      cel::internal::GetTestingMessageFactory(), &arena);

  std::vector<std::unique_ptr<const ExpressionStep>> steps;
  steps.push_back(
      std::make_unique<ValueStep>(MakeValue(test_case.lhs, &arena)));
  steps.push_back(
      std::make_unique<ValueStep>(MakeValue(test_case.rhs, &arena)));
  steps.push_back(CreateEqualityStep(test_case.negation, -1));

  ExecutionFrame frame(steps, activation, opts, state);

  ASSERT_OK_AND_ASSIGN(Value result, frame.Evaluate());

  switch (test_case.expected_result) {
    case OutputType::kBoolTrue:
      EXPECT_THAT(result, BoolValueIs(true));
      break;
    case OutputType::kBoolFalse:
      EXPECT_THAT(result, BoolValueIs(false));
      break;
    case OutputType::kError:
      EXPECT_THAT(result, ValueKindIs(ValueKind::kError));
      break;
    case OutputType::kUnknown:
      EXPECT_THAT(result, ValueKindIs(ValueKind::kUnknown));
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(EqualsTest, EqualsTest,
                         testing::Values<EqualsTestCase>(
                             EqualsTestCase{
                                 InputType::kInt1,
                                 InputType::kInt2,
                                 false,
                                 OutputType::kBoolFalse,
                             },
                             EqualsTestCase{
                                 InputType::kInt1,
                                 InputType::kInt1,
                                 false,
                                 OutputType::kBoolTrue,
                             },
                             EqualsTestCase{
                                 InputType::kInt1,
                                 InputType::kList,
                                 false,
                                 OutputType::kBoolFalse,
                             },
                             EqualsTestCase{
                                 InputType::kInt1,
                                 InputType::kDouble1,
                                 false,
                                 OutputType::kBoolTrue,
                             },
                             EqualsTestCase{
                                 InputType::kInt2,
                                 InputType::kDouble1,
                                 false,
                                 OutputType::kBoolFalse,
                             },
                             EqualsTestCase{
                                 InputType::kInt1,
                                 InputType::kError,
                                 false,
                                 OutputType::kError,
                             },
                             EqualsTestCase{
                                 InputType::kError,
                                 InputType::kInt1,
                                 false,
                                 OutputType::kError,
                             },
                             EqualsTestCase{
                                 InputType::kInt1,
                                 InputType::kUnknown,
                                 false,
                                 OutputType::kUnknown,
                             },
                             EqualsTestCase{
                                 InputType::kUnknown,
                                 InputType::kInt1,
                                 false,
                                 OutputType::kUnknown,
                             },
                             EqualsTestCase{
                                 InputType::kError,
                                 InputType::kUnknown,
                                 false,
                                 OutputType::kError,
                             },
                             EqualsTestCase{
                                 InputType::kUnknown,
                                 InputType::kError,
                                 false,
                                 OutputType::kError,
                             },
                             // !=
                             EqualsTestCase{
                                 InputType::kInt1,
                                 InputType::kInt2,
                                 true,
                                 OutputType::kBoolTrue,
                             },
                             EqualsTestCase{
                                 InputType::kError,
                                 InputType::kInt1,
                                 true,
                                 OutputType::kError,
                             },
                             EqualsTestCase{
                                 InputType::kUnknown,
                                 InputType::kInt1,
                                 true,
                                 OutputType::kUnknown,
                             },
                             EqualsTestCase{
                                 InputType::kInt1,
                                 InputType::kDouble1,
                                 true,
                                 OutputType::kBoolFalse,
                             }));

struct InTestCase {
  InputType lhs;
  InputType rhs;
  OutputType expected_result;
};

class InTest : public ::testing::TestWithParam<InTestCase> {};

TEST_P(InTest, Recursive) {
  const InTestCase& test_case = GetParam();
  cel::Activation activation;
  google::protobuf::Arena arena;
  cel::RuntimeOptions opts;
  opts.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());

  auto plan = CreateDirectInStep(
      std::make_unique<ValueStep>(MakeValue(test_case.lhs, &arena)),
      std::make_unique<ValueStep>(MakeValue(test_case.rhs, &arena)), -1);

  ExecutionFrameBase frame(activation, opts, type_provider,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena);

  cel::Value result;
  AttributeTrail attribute_trail;
  ASSERT_THAT(plan->Evaluate(frame, result, attribute_trail), IsOk());

  switch (test_case.expected_result) {
    case OutputType::kBoolTrue:
      EXPECT_THAT(result, BoolValueIs(true));
      break;
    case OutputType::kBoolFalse:
      EXPECT_THAT(result, BoolValueIs(false));
      break;
    case OutputType::kError:
      EXPECT_THAT(result, ValueKindIs(ValueKind::kError));
      break;
    case OutputType::kUnknown:
      EXPECT_THAT(result, ValueKindIs(ValueKind::kUnknown));
      break;
  }
}

TEST_P(InTest, Iterative) {
  const InTestCase& test_case = GetParam();
  cel::Activation activation;
  google::protobuf::Arena arena;
  cel::RuntimeOptions opts;
  opts.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());

  FlatExpressionEvaluatorState state(
      /*value_stack_size=*/5,
      /*comprehension_slot_count=*/0, type_provider,
      cel::internal::GetTestingDescriptorPool(),
      cel::internal::GetTestingMessageFactory(), &arena);

  std::vector<std::unique_ptr<const ExpressionStep>> steps;
  steps.push_back(
      std::make_unique<ValueStep>(MakeValue(test_case.lhs, &arena)));
  steps.push_back(
      std::make_unique<ValueStep>(MakeValue(test_case.rhs, &arena)));
  steps.push_back(CreateInStep(-1));

  ExecutionFrame frame(steps, activation, opts, state);

  ASSERT_OK_AND_ASSIGN(Value result, frame.Evaluate());

  switch (test_case.expected_result) {
    case OutputType::kBoolTrue:
      EXPECT_THAT(result, BoolValueIs(true));
      break;
    case OutputType::kBoolFalse:
      EXPECT_THAT(result, BoolValueIs(false));
      break;
    case OutputType::kError:
      EXPECT_THAT(result, ValueKindIs(ValueKind::kError));
      break;
    case OutputType::kUnknown:
      EXPECT_THAT(result, ValueKindIs(ValueKind::kUnknown));
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(InTest, InTest,
                         testing::Values<InTestCase>(
                             InTestCase{
                                 InputType::kInt1,
                                 InputType::kInt2,
                                 OutputType::kError,
                             },
                             InTestCase{
                                 InputType::kInt1,
                                 InputType::kList,
                                 OutputType::kBoolTrue,
                             },
                             InTestCase{
                                 InputType::kInt1,
                                 InputType::kMap,
                                 OutputType::kBoolTrue,
                             },
                             InTestCase{
                                 InputType::kDouble1,
                                 InputType::kList,
                                 OutputType::kBoolTrue,
                             },
                             InTestCase{
                                 InputType::kInt2,
                                 InputType::kList,
                                 OutputType::kBoolFalse,
                             },
                             InTestCase{
                                 InputType::kDouble1,
                                 InputType::kMap,
                                 OutputType::kBoolTrue,
                             },
                             InTestCase{
                                 InputType::kInt2,
                                 InputType::kMap,
                                 OutputType::kBoolFalse,
                             },
                             InTestCase{
                                 InputType::kList,
                                 InputType::kMap,
                                 OutputType::kError,
                             },
                             InTestCase{
                                 InputType::kList,
                                 InputType::kList,
                                 OutputType::kBoolFalse,
                             },
                             InTestCase{
                                 InputType::kError,
                                 InputType::kList,
                                 OutputType::kError,
                             },
                             InTestCase{
                                 InputType::kInt1,
                                 InputType::kError,
                                 OutputType::kError,
                             },
                             InTestCase{
                                 InputType::kUnknown,
                                 InputType::kList,
                                 OutputType::kUnknown,
                             },
                             InTestCase{
                                 InputType::kInt1,
                                 InputType::kUnknown,
                                 OutputType::kUnknown,
                             },
                             InTestCase{
                                 InputType::kUnknown,
                                 InputType::kError,
                                 OutputType::kError,
                             }));

}  // namespace
}  // namespace google::api::expr::runtime
