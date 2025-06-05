// Copyright 2024 Google LLC
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

#include "eval/eval/optional_or_step.h"

#include <memory>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/value_testing.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "internal/testing.h"
#include "runtime/activation.h"
#include "runtime/internal/errors.h"
#include "runtime/managed_value_factory.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {
namespace {

using ::absl_testing::StatusIs;
using ::cel::Activation;
using ::cel::As;
using ::cel::ErrorValue;
using ::cel::InstanceOf;
using ::cel::IntValue;
using ::cel::ManagedValueFactory;
using ::cel::MemoryManagerRef;
using ::cel::OptionalValue;
using ::cel::RuntimeOptions;
using ::cel::TypeReflector;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::ValueKind;
using ::cel::test::ErrorValueIs;
using ::cel::test::IntValueIs;
using ::cel::test::OptionalValueIs;
using ::cel::test::ValueKindIs;
using ::testing::HasSubstr;
using ::testing::NiceMock;

class MockDirectStep : public DirectExpressionStep {
 public:
  MOCK_METHOD(absl::Status, Evaluate,
              (ExecutionFrameBase & frame, Value& result,
               AttributeTrail& scratch),
              (const, override));
};

std::unique_ptr<DirectExpressionStep> MockNeverCalledDirectStep() {
  auto* mock = new NiceMock<MockDirectStep>();
  EXPECT_CALL(*mock, Evaluate).Times(0);
  return absl::WrapUnique(mock);
}

std::unique_ptr<DirectExpressionStep> MockExpectCallDirectStep() {
  auto* mock = new NiceMock<MockDirectStep>();
  EXPECT_CALL(*mock, Evaluate)
      .Times(1)
      .WillRepeatedly(
          [](ExecutionFrameBase& frame, Value& result, AttributeTrail& attr) {
            result = ErrorValue(absl::InternalError("expected to be unused"));
            return absl::OkStatus();
          });
  return absl::WrapUnique(mock);
}

class OptionalOrTest : public testing::Test {
 public:
  OptionalOrTest()
      : value_factory_(TypeReflector::Builtin(),
                       MemoryManagerRef::ReferenceCounting()) {}

 protected:
  ManagedValueFactory value_factory_;
  Activation empty_activation_;
};

TEST_F(OptionalOrTest, OptionalOrLeftPresentShortcutRight) {
  RuntimeOptions options;
  ExecutionFrameBase frame(empty_activation_, options, value_factory_.get());

  std::unique_ptr<DirectExpressionStep> step = CreateDirectOptionalOrStep(
      /*expr_id=*/-1,
      CreateConstValueDirectStep(OptionalValue::Of(
          value_factory_.get().GetMemoryManager(), IntValue(42))),
      MockNeverCalledDirectStep(),
      /*is_or_value=*/false,
      /*short_circuiting=*/true);

  Value result;
  AttributeTrail scratch;

  ASSERT_OK(step->Evaluate(frame, result, scratch));

  EXPECT_THAT(result, OptionalValueIs(IntValueIs(42)));
}

TEST_F(OptionalOrTest, OptionalOrLeftErrorShortcutsRight) {
  RuntimeOptions options;
  ExecutionFrameBase frame(empty_activation_, options, value_factory_.get());

  std::unique_ptr<DirectExpressionStep> step = CreateDirectOptionalOrStep(
      /*expr_id=*/-1,
      CreateConstValueDirectStep(ErrorValue(absl::InternalError("error"))),
      MockNeverCalledDirectStep(),
      /*is_or_value=*/false,
      /*short_circuiting=*/true);

  Value result;
  AttributeTrail scratch;

  ASSERT_OK(step->Evaluate(frame, result, scratch));
  EXPECT_THAT(result, ValueKindIs(ValueKind::kError));
}

TEST_F(OptionalOrTest, OptionalOrLeftErrorExhaustiveRight) {
  RuntimeOptions options;
  ExecutionFrameBase frame(empty_activation_, options, value_factory_.get());

  std::unique_ptr<DirectExpressionStep> step = CreateDirectOptionalOrStep(
      /*expr_id=*/-1,
      CreateConstValueDirectStep(ErrorValue(absl::InternalError("error"))),
      MockExpectCallDirectStep(),
      /*is_or_value=*/false,
      /*short_circuiting=*/false);

  Value result;
  AttributeTrail scratch;

  ASSERT_OK(step->Evaluate(frame, result, scratch));
  EXPECT_THAT(result, ValueKindIs(ValueKind::kError));
}

TEST_F(OptionalOrTest, OptionalOrLeftUnknownShortcutsRight) {
  RuntimeOptions options;
  ExecutionFrameBase frame(empty_activation_, options, value_factory_.get());

  std::unique_ptr<DirectExpressionStep> step = CreateDirectOptionalOrStep(
      /*expr_id=*/-1, CreateConstValueDirectStep(UnknownValue()),
      MockNeverCalledDirectStep(),
      /*is_or_value=*/false,
      /*short_circuiting=*/true);

  Value result;
  AttributeTrail scratch;

  ASSERT_OK(step->Evaluate(frame, result, scratch));
  EXPECT_THAT(result, ValueKindIs(ValueKind::kUnknown));
}

TEST_F(OptionalOrTest, OptionalOrLeftUnknownExhaustiveRight) {
  RuntimeOptions options;
  ExecutionFrameBase frame(empty_activation_, options, value_factory_.get());

  std::unique_ptr<DirectExpressionStep> step = CreateDirectOptionalOrStep(
      /*expr_id=*/-1, CreateConstValueDirectStep(UnknownValue()),
      MockExpectCallDirectStep(),
      /*is_or_value=*/false,
      /*short_circuiting=*/false);

  Value result;
  AttributeTrail scratch;

  ASSERT_OK(step->Evaluate(frame, result, scratch));
  EXPECT_THAT(result, ValueKindIs(ValueKind::kUnknown));
}

TEST_F(OptionalOrTest, OptionalOrLeftAbsentReturnRight) {
  RuntimeOptions options;
  ExecutionFrameBase frame(empty_activation_, options, value_factory_.get());

  std::unique_ptr<DirectExpressionStep> step = CreateDirectOptionalOrStep(
      /*expr_id=*/-1, CreateConstValueDirectStep(OptionalValue::None()),
      CreateConstValueDirectStep(OptionalValue::Of(
          value_factory_.get().GetMemoryManager(), IntValue(42))),
      /*is_or_value=*/false,
      /*short_circuiting=*/true);

  Value result;
  AttributeTrail scratch;

  ASSERT_OK(step->Evaluate(frame, result, scratch));

  EXPECT_THAT(result, OptionalValueIs(IntValueIs(42)));
}

TEST_F(OptionalOrTest, OptionalOrLeftWrongType) {
  RuntimeOptions options;
  ExecutionFrameBase frame(empty_activation_, options, value_factory_.get());

  std::unique_ptr<DirectExpressionStep> step = CreateDirectOptionalOrStep(
      /*expr_id=*/-1, CreateConstValueDirectStep(IntValue(42)),
      MockNeverCalledDirectStep(),
      /*is_or_value=*/false,
      /*short_circuiting=*/true);

  Value result;
  AttributeTrail scratch;

  ASSERT_OK(step->Evaluate(frame, result, scratch));

  EXPECT_THAT(result,
              ErrorValueIs(StatusIs(
                  absl::StatusCode::kUnknown,
                  HasSubstr(cel::runtime_internal::kErrNoMatchingOverload))));
}

TEST_F(OptionalOrTest, OptionalOrRightWrongType) {
  RuntimeOptions options;
  ExecutionFrameBase frame(empty_activation_, options, value_factory_.get());

  std::unique_ptr<DirectExpressionStep> step = CreateDirectOptionalOrStep(
      /*expr_id=*/-1, CreateConstValueDirectStep(OptionalValue::None()),
      CreateConstValueDirectStep(IntValue(42)),
      /*is_or_value=*/false,
      /*short_circuiting=*/true);

  Value result;
  AttributeTrail scratch;

  ASSERT_OK(step->Evaluate(frame, result, scratch));

  EXPECT_THAT(result,
              ErrorValueIs(StatusIs(
                  absl::StatusCode::kUnknown,
                  HasSubstr(cel::runtime_internal::kErrNoMatchingOverload))));
}

TEST_F(OptionalOrTest, OptionalOrValueLeftPresentShortcutRight) {
  RuntimeOptions options;
  ExecutionFrameBase frame(empty_activation_, options, value_factory_.get());

  std::unique_ptr<DirectExpressionStep> step = CreateDirectOptionalOrStep(
      /*expr_id=*/-1,
      CreateConstValueDirectStep(OptionalValue::Of(
          value_factory_.get().GetMemoryManager(), IntValue(42))),
      MockNeverCalledDirectStep(),
      /*is_or_value=*/true,
      /*short_circuiting=*/true);

  Value result;
  AttributeTrail scratch;

  ASSERT_OK(step->Evaluate(frame, result, scratch));

  EXPECT_THAT(result, IntValueIs(42));
}

TEST_F(OptionalOrTest, OptionalOrValueLeftPresentExhaustiveRight) {
  RuntimeOptions options;
  ExecutionFrameBase frame(empty_activation_, options, value_factory_.get());

  std::unique_ptr<DirectExpressionStep> step = CreateDirectOptionalOrStep(
      /*expr_id=*/-1,
      CreateConstValueDirectStep(OptionalValue::Of(
          value_factory_.get().GetMemoryManager(), IntValue(42))),
      MockExpectCallDirectStep(),
      /*is_or_value=*/true,
      /*short_circuiting=*/false);

  Value result;
  AttributeTrail scratch;

  ASSERT_OK(step->Evaluate(frame, result, scratch));

  EXPECT_THAT(result, IntValueIs(42));
}

TEST_F(OptionalOrTest, OptionalOrValueLeftErrorShortcutsRight) {
  RuntimeOptions options;
  ExecutionFrameBase frame(empty_activation_, options, value_factory_.get());

  std::unique_ptr<DirectExpressionStep> step = CreateDirectOptionalOrStep(
      /*expr_id=*/-1,
      CreateConstValueDirectStep(ErrorValue(absl::InternalError("error"))),
      MockNeverCalledDirectStep(),
      /*is_or_value=*/true,
      /*short_circuiting=*/true);

  Value result;
  AttributeTrail scratch;

  ASSERT_OK(step->Evaluate(frame, result, scratch));
  EXPECT_THAT(result, ValueKindIs(ValueKind::kError));
}

TEST_F(OptionalOrTest, OptionalOrValueLeftUnknownShortcutsRight) {
  RuntimeOptions options;
  ExecutionFrameBase frame(empty_activation_, options, value_factory_.get());

  std::unique_ptr<DirectExpressionStep> step = CreateDirectOptionalOrStep(
      /*expr_id=*/-1, CreateConstValueDirectStep(UnknownValue()),
      MockNeverCalledDirectStep(), true, true);

  Value result;
  AttributeTrail scratch;

  ASSERT_OK(step->Evaluate(frame, result, scratch));
  EXPECT_THAT(result, ValueKindIs(ValueKind::kUnknown));
}

TEST_F(OptionalOrTest, OptionalOrValueLeftAbsentReturnRight) {
  RuntimeOptions options;
  ExecutionFrameBase frame(empty_activation_, options, value_factory_.get());

  std::unique_ptr<DirectExpressionStep> step = CreateDirectOptionalOrStep(
      /*expr_id=*/-1, CreateConstValueDirectStep(OptionalValue::None()),
      CreateConstValueDirectStep(IntValue(42)),
      /*is_or_value=*/true,
      /*short_circuiting=*/true);

  Value result;
  AttributeTrail scratch;

  ASSERT_OK(step->Evaluate(frame, result, scratch));

  EXPECT_THAT(result, IntValueIs(42));
}

TEST_F(OptionalOrTest, OptionalOrValueLeftWrongType) {
  RuntimeOptions options;
  ExecutionFrameBase frame(empty_activation_, options, value_factory_.get());

  std::unique_ptr<DirectExpressionStep> step = CreateDirectOptionalOrStep(
      /*expr_id=*/-1, CreateConstValueDirectStep(IntValue(42)),
      MockNeverCalledDirectStep(), true, true);

  Value result;
  AttributeTrail scratch;

  ASSERT_OK(step->Evaluate(frame, result, scratch));

  EXPECT_THAT(result,
              ErrorValueIs(StatusIs(
                  absl::StatusCode::kUnknown,
                  HasSubstr(cel::runtime_internal::kErrNoMatchingOverload))));
}

}  // namespace
}  // namespace google::api::expr::runtime
