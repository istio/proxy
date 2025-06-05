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
#include "eval/eval/compiler_constant_step.h"

#include <memory>

#include "base/type_provider.h"
#include "common/native_type.h"
#include "common/type_factory.h"
#include "common/type_manager.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "common/values/legacy_value_manager.h"
#include "eval/eval/evaluator_core.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "runtime/activation.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::extensions::ProtoMemoryManagerRef;

class CompilerConstantStepTest : public testing::Test {
 public:
  CompilerConstantStepTest()
      : value_factory_(ProtoMemoryManagerRef(&arena_),
                       cel::TypeProvider::Builtin()),
        state_(2, 0, cel::TypeProvider::Builtin(),
               ProtoMemoryManagerRef(&arena_)) {}

 protected:
  google::protobuf::Arena arena_;
  cel::common_internal::LegacyValueManager value_factory_;

  FlatExpressionEvaluatorState state_;
  cel::Activation empty_activation_;
  cel::RuntimeOptions options_;
};

TEST_F(CompilerConstantStepTest, Evaluate) {
  ExecutionPath path;
  path.push_back(std::make_unique<CompilerConstantStep>(
      value_factory_.CreateIntValue(42), -1, false));

  ExecutionFrame frame(path, empty_activation_, options_, state_);

  ASSERT_OK_AND_ASSIGN(cel::Value result, frame.Evaluate());

  EXPECT_EQ(result.GetInt().NativeValue(), 42);
}

TEST_F(CompilerConstantStepTest, TypeId) {
  CompilerConstantStep step(value_factory_.CreateIntValue(42), -1, false);

  ExpressionStep& abstract_step = step;
  EXPECT_EQ(abstract_step.GetNativeTypeId(),
            cel::NativeTypeId::For<CompilerConstantStep>());
}

TEST_F(CompilerConstantStepTest, Value) {
  CompilerConstantStep step(value_factory_.CreateIntValue(42), -1, false);

  EXPECT_EQ(step.value().GetInt().NativeValue(), 42);
}

}  // namespace
}  // namespace google::api::expr::runtime
