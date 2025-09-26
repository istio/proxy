// Copyright 2022 Google LLC
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

#include "runtime/standard/logical_functions.h"

#include <functional>
#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/builtins.h"
#include "common/function_descriptor.h"
#include "common/kind.h"
#include "common/value.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "runtime/function.h"
#include "runtime/function_overload_reference.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Matcher;
using ::testing::Truly;

MATCHER_P3(DescriptorIs, name, arg_kinds, is_receiver, "") {
  const FunctionOverloadReference& ref = arg;
  const FunctionDescriptor& descriptor = ref.descriptor;
  return descriptor.name() == name &&
         descriptor.ShapeMatches(is_receiver, arg_kinds);
}

MATCHER_P(IsBool, expected, "") {
  const Value& value = arg;
  return value->Is<BoolValue>() && value.GetBool().NativeValue() == expected;
}

// TODO(uncreated-issue/48): replace this with a parsed expr when the non-protobuf
// parser is available.
absl::StatusOr<Value> TestDispatchToFunction(
    const FunctionRegistry& registry, absl::string_view simple_name,
    absl::Span<const Value> args,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  std::vector<Kind> arg_matcher_;
  arg_matcher_.reserve(args.size());
  for (const auto& value : args) {
    arg_matcher_.push_back(ValueKindToKind(value->kind()));
  }
  std::vector<FunctionOverloadReference> refs = registry.FindStaticOverloads(
      simple_name, /*receiver_style=*/false, arg_matcher_);

  if (refs.size() != 1) {
    return absl::InvalidArgumentError("ambiguous overloads");
  }

  return refs[0].implementation.Invoke(args, descriptor_pool, message_factory,
                                       arena);
}

TEST(RegisterLogicalFunctions, NotStrictlyFalseRegistered) {
  FunctionRegistry registry;
  RuntimeOptions options;

  ASSERT_OK(RegisterLogicalFunctions(registry, options));

  EXPECT_THAT(
      registry.FindStaticOverloads(builtin::kNotStrictlyFalse,
                                   /*receiver_style=*/false, {Kind::kAny}),
      ElementsAre(DescriptorIs(builtin::kNotStrictlyFalse,
                               std::vector<Kind>{Kind::kBool}, false)));
}

TEST(RegisterLogicalFunctions, LogicalNotRegistered) {
  FunctionRegistry registry;
  RuntimeOptions options;

  ASSERT_OK(RegisterLogicalFunctions(registry, options));

  EXPECT_THAT(
      registry.FindStaticOverloads(builtin::kNot,
                                   /*receiver_style=*/false, {Kind::kAny}),
      ElementsAre(
          DescriptorIs(builtin::kNot, std::vector<Kind>{Kind::kBool}, false)));
}

struct TestCase {
  using ArgumentFactory = std::function<std::vector<Value>()>;

  std::string function;
  ArgumentFactory arguments;
  absl::StatusOr<Matcher<Value>> result_matcher;
};

class LogicalFunctionsTest : public testing::TestWithParam<TestCase> {
 protected:
  google::protobuf::Arena arena_;
};

TEST_P(LogicalFunctionsTest, Runner) {
  const TestCase& test_case = GetParam();
  cel::FunctionRegistry registry;

  ASSERT_OK(RegisterLogicalFunctions(registry, RuntimeOptions()));

  std::vector<Value> args = test_case.arguments();

  absl::StatusOr<Value> result = TestDispatchToFunction(
      registry, test_case.function, args,
      cel::internal::GetTestingDescriptorPool(),
      cel::internal::GetTestingMessageFactory(), &arena_);

  EXPECT_EQ(result.ok(), test_case.result_matcher.ok());

  if (!test_case.result_matcher.ok()) {
    EXPECT_EQ(result.status().code(), test_case.result_matcher.status().code());
    EXPECT_THAT(result.status().message(),
                HasSubstr(test_case.result_matcher.status().message()));
  } else {
    ASSERT_TRUE(result.ok()) << "unexpected error" << result.status();
    EXPECT_THAT(*result, *test_case.result_matcher);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Cases, LogicalFunctionsTest,
    testing::ValuesIn(std::vector<TestCase>{
        TestCase{builtin::kNot,
                 []() -> std::vector<Value> { return {BoolValue(true)}; },
                 IsBool(false)},
        TestCase{builtin::kNot,
                 []() -> std::vector<Value> { return {BoolValue(false)}; },
                 IsBool(true)},
        TestCase{builtin::kNot,
                 []() -> std::vector<Value> {
                   return {BoolValue(true), BoolValue(false)};
                 },
                 absl::InvalidArgumentError("")},
        TestCase{builtin::kNotStrictlyFalse,
                 []() -> std::vector<Value> { return {BoolValue(true)}; },
                 IsBool(true)},
        TestCase{builtin::kNotStrictlyFalse,
                 []() -> std::vector<Value> { return {BoolValue(false)}; },
                 IsBool(false)},
        TestCase{builtin::kNotStrictlyFalse,
                 []() -> std::vector<Value> {
                   return {ErrorValue(absl::InternalError("test"))};
                 },
                 IsBool(true)},
        TestCase{builtin::kNotStrictlyFalse,
                 []() -> std::vector<Value> { return {UnknownValue()}; },
                 IsBool(true)},
        TestCase{builtin::kNotStrictlyFalse,
                 []() -> std::vector<Value> { return {IntValue(42)}; },
                 Truly([](const Value& v) {
                   return v->Is<ErrorValue>() &&
                          absl::StrContains(
                              v.GetError().NativeValue().message(),
                              "No matching overloads");
                 })},
    }));

}  // namespace
}  // namespace cel
