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

#ifndef THIRD_PARTY_CEL_CPP_TOOLS_TESTRUNNER_CEL_TEST_CONTEXT_H_
#define THIRD_PARTY_CEL_CPP_TOOLS_TESTRUNNER_CEL_TEST_CONTEXT_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "cel/expr/checked.pb.h"
#include "cel/expr/value.pb.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "common/value.h"
#include "compiler/compiler.h"
#include "eval/public/cel_expression.h"
#include "runtime/activation.h"
#include "runtime/runtime.h"
#include "testing/testrunner/cel_expression_source.h"
#include "cel/expr/conformance/test/suite.pb.h"
#include "google/protobuf/arena.h"
namespace cel::test {

// The context class for a CEL test, holding configurations needed to evaluate
// compiled CEL expressions.
class CelTestContext {
 public:
  using CelActivationFactoryFn = std::function<absl::StatusOr<cel::Activation>(
      const cel::expr::conformance::test::TestCase& test_case,
      google::protobuf::Arena* arena)>;
  using AssertFn = std::function<void(
      const cel::Value& computed,
      const cel::expr::conformance::test::TestCase& test_case,
      google::protobuf::Arena* arena)>;

  // Creates a CelTestContext using a `CelExpressionBuilder`.
  //
  // The `CelExpressionBuilder` helps in setting up the environment for
  // building the CEL expression.
  //
  // Example usage:
  //
  // CEL_REGISTER_TEST_CONTEXT_FACTORY(
  //       []() -> absl::StatusOr<std::unique_ptr<CelTestContext>> {
  //     // SAFE: This setup code now runs when the lambda is invoked at
  //     runtime,
  //     // long after all static initializations are complete.
  //     auto cel_expression_builder =
  //         google::api::expr::runtime::CreateCelExpressionBuilder();
  //    CelTestContextOptions options;
  //     return CelTestContext::CreateFromCelExpressionBuilder(
  //         std::move(cel_expression_builder), std::move(options));
  //   });
  static std::unique_ptr<CelTestContext> CreateFromCelExpressionBuilder(
      std::unique_ptr<google::api::expr::runtime::CelExpressionBuilder>
          cel_expression_builder) {
    return absl::WrapUnique(
        new CelTestContext(std::move(cel_expression_builder)));
  }

  // Creates a CelTestContext using a `cel::Runtime`.
  //
  // The `cel::Runtime` is used to evaluate the CEL expression by managing
  // the state needed to generate Program.
  static std::unique_ptr<CelTestContext> CreateFromRuntime(
      std::unique_ptr<const cel::Runtime> runtime) {
    return absl::WrapUnique(new CelTestContext(std::move(runtime)));
  }

  const cel::Runtime* absl_nullable runtime() const { return runtime_.get(); }

  const google::api::expr::runtime::CelExpressionBuilder* absl_nullable
  cel_expression_builder() const {
    return cel_expression_builder_.get();
  }

  const cel::Compiler* absl_nullable compiler() const {
    return compiler_.get();
  }

  const CelExpressionSource* absl_nullable expression_source() const {
    return expression_source_.get();
  }

  const absl::flat_hash_map<std::string, cel::expr::Value>&
  custom_bindings() const {
    return custom_bindings_;
  }

  bool enable_coverage() const { return enable_coverage_; }

  // Allows the runner to inject the expression source
  // parsed from command-line flags.
  void SetExpressionSource(CelExpressionSource source) {
    expression_source_ =
        std::make_unique<CelExpressionSource>(std::move(source));
  }

  // Allows the runner to inject an optional CEL compiler.
  void SetCompiler(std::unique_ptr<const cel::Compiler> compiler) {
    compiler_ = std::move(compiler);
  }

  // Allows the runner to inject custom bindings.
  void SetCustomBindings(
      absl::flat_hash_map<std::string, cel::expr::Value>
          custom_bindings) {
    custom_bindings_ = std::move(custom_bindings);
  }

  // Allows the runner to inject a custom activation factory. If not set, an
  // empty activation will be used. Custom bindings and test case inputs will
  // be added to the activation returned by the factory.
  void SetActivationFactory(CelActivationFactoryFn activation_factory) {
    activation_factory_ = std::move(activation_factory);
  }

  // Allows the runner to enable coverage collection.
  void SetEnableCoverage(bool enable) { enable_coverage_ = enable; }

  const CelActivationFactoryFn& activation_factory() const {
    return activation_factory_;
  }

  // Allows the runner to inject a custom assertion function. If not set, the
  // default assertion logic in TestRunner will be used.
  void SetAssertFn(AssertFn assert_fn) { assert_fn_ = std::move(assert_fn); }

  const AssertFn& assert_fn() const { return assert_fn_; }

 private:
  // Delete copy and move constructors.
  CelTestContext(const CelTestContext&) = delete;
  CelTestContext& operator=(const CelTestContext&) = delete;
  CelTestContext(CelTestContext&&) = delete;
  CelTestContext& operator=(CelTestContext&&) = delete;

  // Make the constructors private to enforce the use of the factory methods.
  explicit CelTestContext(
      std::unique_ptr<google::api::expr::runtime::CelExpressionBuilder>
          cel_expression_builder)
      : cel_expression_builder_(std::move(cel_expression_builder)) {}

  explicit CelTestContext(std::unique_ptr<const cel::Runtime> runtime)
      : runtime_(std::move(runtime)) {}

  // An optional CEL compiler. This is required for test cases where
  // input or output values are themselves CEL expressions that need to be
  // resolved at runtime or cel expression source is raw string or cel file.
  std::unique_ptr<const cel::Compiler> compiler_ = nullptr;

  // A map of variable names to values that provides default bindings for the
  // evaluation.
  //
  // These bindings can be considered context-wide defaults. If a variable name
  // exists in both these custom bindings and in a specific TestCase's input,
  // the value from the TestCase will take precedence and override this one.
  // This logic is handled by the test runner when it constructs the final
  // activation.
  absl::flat_hash_map<std::string, cel::expr::Value> custom_bindings_;

  // The source for the CEL expression to be evaluated in the test.
  std::unique_ptr<CelExpressionSource> expression_source_;

  // This helps in setting up the environment for building the CEL
  // expression. Users should either provide a runtime, or the
  // CelExpressionBuilder.
  std::unique_ptr<google::api::expr::runtime::CelExpressionBuilder>
      cel_expression_builder_;

  // The runtime is used to evaluate the CEL expression by managing the state
  // needed to generate Program. Users should either provide a runtime, or the
  // CelExpressionBuilder.
  std::unique_ptr<const cel::Runtime> runtime_;

  CelActivationFactoryFn activation_factory_;
  AssertFn assert_fn_;

  // Whether to enable coverage collection.
  bool enable_coverage_ = false;
};

}  // namespace cel::test

#endif  // THIRD_PARTY_CEL_CPP_TOOLS_TESTRUNNER_CEL_TEST_CONTEXT_H_
