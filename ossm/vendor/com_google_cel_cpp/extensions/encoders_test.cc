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

#include "extensions/encoders.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status_matchers.h"
#include "checker/standard_library.h"
#include "checker/validation_result.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "runtime/activation.h"
#include "runtime/runtime.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {
namespace {

using ::absl_testing::IsOk;

struct TestCase {
  std::string expr;
};

class EncodersTest : public ::testing::TestWithParam<TestCase> {};

TEST_P(EncodersTest, ParseCheckEval) {
  const TestCase& test_case = GetParam();

  // Configure the compiler.
  ASSERT_OK_AND_ASSIGN(
      auto compiler_builder,
      NewCompilerBuilder(internal::GetTestingDescriptorPool()));
  ASSERT_THAT(compiler_builder->AddLibrary(StandardCheckerLibrary()), IsOk());
  ASSERT_THAT(
      compiler_builder->AddLibrary(extensions::EncodersCheckerLibrary()),
      IsOk());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Compiler> compiler,
                       std::move(*compiler_builder).Build());

  // Configure the runtime.
  cel::RuntimeOptions runtime_options;
  ASSERT_OK_AND_ASSIGN(
      auto runtime_builder,
      CreateStandardRuntimeBuilder(internal::GetTestingDescriptorPool(),
                                   runtime_options));
  ASSERT_THAT(RegisterEncodersFunctions(runtime_builder.function_registry(),
                                        runtime_options),
              IsOk());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const Runtime> runtime,
                       std::move(runtime_builder).Build());

  // Compile, plan, evaluate.
  ASSERT_OK_AND_ASSIGN(ValidationResult result,
                       compiler->Compile(test_case.expr));
  ASSERT_TRUE(result.IsValid());
  ASSERT_OK_AND_ASSIGN(auto program,
                       runtime->CreateProgram(*result.ReleaseAst()));

  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(auto value, program->Evaluate(&arena, activation));
  ASSERT_TRUE(value.IsBool());
  ASSERT_TRUE(value.GetBool());
}

INSTANTIATE_TEST_SUITE_P(
    EncodersTest, EncodersTest,
    testing::Values(TestCase{"base64.encode(b'hello') == 'aGVsbG8='"},
                    TestCase{"base64.decode('aGVsbG8=') == b'hello'"}));

}  // namespace
}  // namespace cel::extensions
