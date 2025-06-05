// Copyright 2023 Google LLC
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

#include "eval/eval/lazy_init_step.h"

#include <cstddef>
#include <vector>

#include "base/type_provider.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/evaluator_core.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/testing.h"
#include "runtime/activation.h"
#include "runtime/managed_value_factory.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {
namespace {

using ::cel::Activation;
using ::cel::IntValue;
using ::cel::ManagedValueFactory;
using ::cel::RuntimeOptions;
using ::cel::TypeProvider;
using ::cel::ValueManager;
using ::cel::extensions::ProtoMemoryManagerRef;
using ::testing::IsNull;

class LazyInitStepTest : public testing::Test {
 private:
  // arbitrary numbers enough for basic tests.
  static constexpr size_t kValueStack = 5;
  static constexpr size_t kComprehensionSlotCount = 3;

 public:
  LazyInitStepTest()
      : value_factory_(TypeProvider::Builtin(), ProtoMemoryManagerRef(&arena_)),
        evaluator_state_(kValueStack, kComprehensionSlotCount,
                         value_factory_.get()) {}

 protected:
  ValueManager& value_factory() { return value_factory_.get(); };

  google::protobuf::Arena arena_;
  ManagedValueFactory value_factory_;
  FlatExpressionEvaluatorState evaluator_state_;
  RuntimeOptions runtime_options_;
  Activation activation_;
};

TEST_F(LazyInitStepTest, CreateCheckInitStepDoesInit) {
  ExecutionPath path;
  ExecutionPath subpath;

  path.push_back(CreateLazyInitStep(/*slot_index=*/0,
                                    /*subexpression_index=*/1, -1));

  ASSERT_OK_AND_ASSIGN(
      subpath.emplace_back(),
      CreateConstValueStep(value_factory().CreateIntValue(42), -1, false));

  std::vector<ExecutionPathView> expression_table{path, subpath};

  ExecutionFrame frame(expression_table, activation_, runtime_options_,
                       evaluator_state_);
  ASSERT_OK_AND_ASSIGN(auto value, frame.Evaluate());

  EXPECT_TRUE(value->Is<IntValue>() && value.GetInt().NativeValue() == 42);
}

TEST_F(LazyInitStepTest, CreateCheckInitStepSkipInit) {
  ExecutionPath path;
  ExecutionPath subpath;

  // This is the expected usage, but in this test we are just depending on the
  // fact that these don't change the stack and fit the program layout
  // requirements.
  path.push_back(CreateLazyInitStep(/*slot_index=*/0, -1, -1));

  ASSERT_OK_AND_ASSIGN(
      subpath.emplace_back(),
      CreateConstValueStep(value_factory().CreateIntValue(42), -1, false));

  std::vector<ExecutionPathView> expression_table{path, subpath};

  ExecutionFrame frame(expression_table, activation_, runtime_options_,
                       evaluator_state_);
  frame.comprehension_slots().Set(0, value_factory().CreateIntValue(42));
  ASSERT_OK_AND_ASSIGN(auto value, frame.Evaluate());

  EXPECT_TRUE(value->Is<IntValue>() && value.GetInt().NativeValue() == 42);
}

TEST_F(LazyInitStepTest, CreateAssignSlotAndPopStepBasic) {
  ExecutionPath path;

  path.push_back(CreateAssignSlotAndPopStep(0));

  ExecutionFrame frame(path, activation_, runtime_options_, evaluator_state_);
  frame.comprehension_slots().ClearSlot(0);

  frame.value_stack().Push(value_factory().CreateIntValue(42));

  // This will error because no return value, step will still evaluate.
  frame.Evaluate().IgnoreError();

  auto* slot = frame.comprehension_slots().Get(0);
  ASSERT_TRUE(slot != nullptr);
  EXPECT_TRUE(slot->value->Is<IntValue>() &&
              slot->value.GetInt().NativeValue() == 42);
  EXPECT_TRUE(frame.value_stack().empty());
}

TEST_F(LazyInitStepTest, CreateClearSlotStepBasic) {
  ExecutionPath path;

  path.push_back(CreateClearSlotStep(0, -1));

  ExecutionFrame frame(path, activation_, runtime_options_, evaluator_state_);
  frame.comprehension_slots().Set(0, value_factory().CreateIntValue(42));

  // This will error because no return value, step will still evaluate.
  frame.Evaluate().IgnoreError();

  auto* slot = frame.comprehension_slots().Get(0);
  ASSERT_TRUE(slot == nullptr);
}

TEST_F(LazyInitStepTest, CreateClearSlotsStepBasic) {
  ExecutionPath path;

  path.push_back(CreateClearSlotsStep(0, 2, -1));

  ExecutionFrame frame(path, activation_, runtime_options_, evaluator_state_);
  frame.comprehension_slots().Set(0, value_factory().CreateIntValue(42));
  frame.comprehension_slots().Set(1, value_factory().CreateIntValue(42));

  // This will error because no return value, step will still evaluate.
  frame.Evaluate().IgnoreError();

  EXPECT_THAT(frame.comprehension_slots().Get(0), IsNull());
  EXPECT_THAT(frame.comprehension_slots().Get(1), IsNull());
}

}  // namespace
}  // namespace google::api::expr::runtime
