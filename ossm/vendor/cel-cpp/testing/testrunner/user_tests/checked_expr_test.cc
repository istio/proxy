// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>
#include <utility>

#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/status_macros.h"
#include "internal/testing_descriptor_pool.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "testing/testrunner/cel_test_context.h"
#include "testing/testrunner/cel_test_factories.h"
#include "cel/expr/conformance/test/suite.pb.h"
#include "google/protobuf/text_format.h"

namespace cel::testing {

using ::cel::test::CelTestContext;

template <typename T>
T ParseTextProtoOrDie(absl::string_view text_proto) {
  T result;
  ABSL_CHECK(google::protobuf::TextFormat::ParseFromString(text_proto, &result));
  return result;
}

CEL_REGISTER_TEST_SUITE_FACTORY([]() {
  return ParseTextProtoOrDie<cel::expr::conformance::test::TestSuite>(R"pb(
    name: "cli_expression_tests"
    description: "Tests designed for expressions passed via CLI flags."
    sections: {
      name: "subtraction_test"
      description: "Tests subtraction of two variables."
      tests: {
        name: "variable_subtraction"
        description: "Test that subtraction of two variables works."
        input: {
          key: "x"
          value { value { int64_value: 10 } }
        }
        input {
          key: "y"
          value { value { int64_value: 5 } }
        }
        output { result_value { int64_value: 5 } }
      }
    }
  )pb");
});

CEL_REGISTER_TEST_CONTEXT_FACTORY(
    []() -> absl::StatusOr<std::unique_ptr<CelTestContext>> {
      ABSL_LOG(INFO) << "Creating runtime-only test context for CheckedExpr";

      // Create a runtime.
      CEL_ASSIGN_OR_RETURN(cel::RuntimeBuilder runtime_builder,
                           cel::CreateStandardRuntimeBuilder(
                               cel::internal::GetTestingDescriptorPool(), {}));
      CEL_ASSIGN_OR_RETURN(std::unique_ptr<const cel::Runtime> runtime,
                           std::move(runtime_builder).Build());

      // Create the context with the runtime, but no compiler.
      // The test runner will inject the CheckedExpr source later.
      return CelTestContext::CreateFromRuntime(std::move(runtime));
    });
}  // namespace cel::testing
