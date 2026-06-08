// Copyright 2025 Google LLC.
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

#ifndef THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_CEL_TEST_FACTORIES_H_
#define THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_CEL_TEST_FACTORIES_H_

#include <functional>
#include <memory>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "testing/testrunner/cel_test_context.h"
#include "cel/expr/conformance/test/suite.pb.h"
namespace cel::test {
namespace internal {

using CelTestContextFactoryFn =
    std::function<absl::StatusOr<std::unique_ptr<CelTestContext>>()>;
using CelTestSuiteFactoryFn =
    std::function<cel::expr::conformance::test::TestSuite()>;

// Returns the factory function for creating a CelTestContext.
inline CelTestContextFactoryFn& GetCelTestContextFactory() {
  static absl::NoDestructor<CelTestContextFactoryFn> factory;
  return *factory;
}

// Sets the factory function for creating a CelTestContext. Only one factory
// function can be set. Usage details can be found in cel_test_context.h.
inline bool SetCelTestContextFactory(CelTestContextFactoryFn factory) {
  ABSL_DCHECK(GetCelTestContextFactory() == nullptr)
      << "CelTestContextFactory is already set.";
  GetCelTestContextFactory() = std::move(factory);
  return true;
}

// Returns the factory function for creating a CelTestSuite.
inline CelTestSuiteFactoryFn& GetCelTestSuiteFactory() {
  static absl::NoDestructor<CelTestSuiteFactoryFn> factory;
  return *factory;
}

// Sets the factory function for creating a CelTestSuite. Only one factory
// function can be set.
inline bool SetCelTestSuiteFactory(CelTestSuiteFactoryFn factory) {
  ABSL_DCHECK(GetCelTestSuiteFactory() == nullptr)
      << "CelTestSuiteFactory is already set.";
  GetCelTestSuiteFactory() = std::move(factory);
  return true;
}
}  // namespace internal

// Register cel test context factories from a function or lambda.
//
// The return value of `factory_fn` should be a
// `absl::StatusOr<std::unique_ptr<CelTestContext>>>`.
#define CEL_REGISTER_TEST_CONTEXT_FACTORY(factory_fn)              \
  namespace {                                                      \
  const bool kTestContextFactoryRegistrationResult_##__LINE__ =    \
      ::cel::test::internal::SetCelTestContextFactory(factory_fn); \
  }

// Register cel test suite factory from a function or lambda. This is used to
// provide a custom test suite to the test runner which is useful for cases
// where the test suite is dynamically generated or where the test suite needs
// to be generated from a user provided source.
//
// The return value of `factory_fn` should be a
// `::cel::expr::conformance::test::TestSuite`.
#define CEL_REGISTER_TEST_SUITE_FACTORY(factory_fn)              \
  namespace {                                                    \
  const bool kTestSuiteFactoryRegistrationResult_##__LINE__ =    \
      ::cel::test::internal::SetCelTestSuiteFactory(factory_fn); \
  }

}  // namespace cel::test

#endif  // THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_CEL_TEST_FACTORIES_H_
