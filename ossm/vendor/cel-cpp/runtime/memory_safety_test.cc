// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//       https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Tests for memory safety using the CEL Evaluator.
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "checker/validation_result.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/optional.h"
#include "compiler/standard_library.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "runtime/activation.h"
#include "runtime/constant_folding.h"
#include "runtime/function_adapter.h"
#include "runtime/reference_resolver.h"
#include "runtime/regex_precompilation.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "cel/expr/conformance/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"

namespace cel {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::cel::expr::conformance::proto3::NestedTestAllTypes;
using ::cel::expr::conformance::proto3::TestAllTypes;
using ::cel::test::ValueMatcher;
using ::google::protobuf::Any;
using ::testing::Not;

struct TestCase {
  std::string name;
  std::string expression;
  absl::flat_hash_map<absl::string_view,
                      absl::variant<Value, NestedTestAllTypes>>
      activation;
  test::ValueMatcher expected_matcher;
  bool reference_resolver_enabled = false;
};

enum Options { kDefault, kExhaustive, kFoldConstants };

using ParamType = std::tuple<TestCase, Options>;

absl::StatusOr<std::unique_ptr<Compiler>> CreateCompiler() {
  google::protobuf::LinkMessageReflection<cel::expr::conformance::proto3::TestAllTypes>();
  google::protobuf::LinkMessageReflection<
      cel::expr::conformance::proto3::NestedTestAllTypes>();

  CEL_ASSIGN_OR_RETURN(
      std::unique_ptr<CompilerBuilder> b,
      NewCompilerBuilder(google::protobuf::DescriptorPool::generated_pool()));
  CEL_RETURN_IF_ERROR(b->AddLibrary(StandardCompilerLibrary()));
  CEL_RETURN_IF_ERROR(b->AddLibrary(OptionalCompilerLibrary()));
  b->GetCheckerBuilder().set_container("cel.expr.conformance.proto3");
  auto& cb = b->GetCheckerBuilder();
  CEL_RETURN_IF_ERROR(cb.AddVariable(MakeVariableDecl("bool_var", BoolType())));
  CEL_RETURN_IF_ERROR(
      cb.AddVariable(MakeVariableDecl("string_var", StringType())));
  CEL_RETURN_IF_ERROR(
      cb.AddVariable(MakeVariableDecl("condition", BoolType())));
  CEL_RETURN_IF_ERROR(cb.AddVariable(MakeVariableDecl(
      "nested_test_all_types", MessageType(NestedTestAllTypes::descriptor()))));

  CEL_RETURN_IF_ERROR(cb.AddFunction(
      MakeFunctionDecl("IsPrivate", MakeOverloadDecl("IsPrivate_string",
                                                     BoolType(), StringType()))
          .value()));
  CEL_RETURN_IF_ERROR(cb.AddFunction(
      MakeFunctionDecl(
          "net.IsPrivate",
          MakeOverloadDecl("net_IsPrivate_string", BoolType(), StringType()))
          .value()));

  return b->Build();
}

const Compiler& GetCompiler() {
  static const Compiler* compiler = []() {
    auto compiler = CreateCompiler();
    ABSL_QCHECK_OK(compiler.status());
    return compiler->release();
  }();
  return *compiler;
}

std::string TestCaseName(const testing::TestParamInfo<ParamType>& param_info) {
  const ParamType& param = param_info.param;
  absl::string_view opt;
  switch (std::get<1>(param)) {
    case Options::kDefault:
      opt = "default";
      break;
    case Options::kExhaustive:
      opt = "exhaustive";
      break;
    case Options::kFoldConstants:
      opt = "opt";
      break;
  }

  return absl::StrCat(std::get<0>(param).name, "_", opt);
}

bool IsPrivateIpv4Impl(const StringValue& addr) {
  // Implementation for demonstration, this is simple but incomplete and
  // brittle.
  std::string buf;
  return absl::StartsWith(addr.ToStringView(&buf), "192.168.") ||
         absl::StartsWith(addr.ToStringView(&buf), "10.");
}

absl::StatusOr<std::unique_ptr<Runtime>> ConfigureRuntimeImpl(
    bool resolve_references, Options evaluation_options) {
  RuntimeOptions options;
  switch (evaluation_options) {
    case Options::kDefault:
      options.short_circuiting = true;
      break;
    case Options::kExhaustive:
      options.short_circuiting = false;
      break;
    case Options::kFoldConstants:
      options.enable_comprehension_list_append = true;
      options.short_circuiting = true;
      break;
  }
  options.enable_qualified_type_identifiers = resolve_references;
  options.container = "cel.expr.conformance.proto3";
  CEL_ASSIGN_OR_RETURN(cel::RuntimeBuilder runtime_builder,
                       CreateStandardRuntimeBuilder(
                           google::protobuf::DescriptorPool::generated_pool(), options));
  if (resolve_references) {
    CEL_RETURN_IF_ERROR(EnableReferenceResolver(
        runtime_builder, ReferenceResolverEnabled::kAlways));
  }
  if (evaluation_options == Options::kFoldConstants) {
    CEL_RETURN_IF_ERROR(extensions::EnableConstantFolding(runtime_builder));
    CEL_RETURN_IF_ERROR(extensions::EnableRegexPrecompilation(runtime_builder));
  }

  auto s = UnaryFunctionAdapter<bool, const StringValue&>::Register(
      "IsPrivate", false, &IsPrivateIpv4Impl,
      runtime_builder.function_registry());
  CEL_RETURN_IF_ERROR(s);
  s.Update(UnaryFunctionAdapter<bool, const StringValue&>::Register(
      "net.IsPrivate", false, &IsPrivateIpv4Impl,
      runtime_builder.function_registry()));
  CEL_RETURN_IF_ERROR(s);

  return std::move(runtime_builder).Build();
}

class EvaluatorMemorySafetyTest : public testing::TestWithParam<ParamType> {
 public:
  EvaluatorMemorySafetyTest() = default;

 protected:
  const TestCase& GetTestCase() { return std::get<0>(GetParam()); }

  absl::StatusOr<std::unique_ptr<Runtime>> ConfigureRuntime() {
    return ConfigureRuntimeImpl(GetTestCase().reference_resolver_enabled,
                                std::get<1>(GetParam()));
  }
};

void InitActivation(const TestCase& test_case, google::protobuf::Arena& arena,
                    Activation& activation) {
  for (const auto& [key, value] : test_case.activation) {
    if (absl::holds_alternative<Value>(value)) {
      activation.InsertOrAssignValue(key, std::get<Value>(value));
    } else {
      // Note: This assumes that the TestCase is valid for the given TEST.
      // Changes to the activation map will invalidate the pointer to message
      // that gets wrapped here.
      activation.InsertOrAssignValue(
          key, Value::WrapMessageUnsafe(
                   &std::get<NestedTestAllTypes>(value),
                   google::protobuf::DescriptorPool::generated_pool(),
                   google::protobuf::MessageFactory::generated_factory(), &arena));
    }
  }
}

TEST_P(EvaluatorMemorySafetyTest, Basic) {
  const auto& test_case = GetTestCase();

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Runtime> runtime, ConfigureRuntime());

  ASSERT_OK_AND_ASSIGN(ValidationResult validation,
                       GetCompiler().Compile(test_case.expression));

  ASSERT_TRUE(validation.IsValid()) << validation.FormatError();
  ASSERT_OK_AND_ASSIGN(auto ast, validation.ReleaseAst());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       runtime->CreateProgram(std::move(ast)));

  Activation activation;
  google::protobuf::Arena arena;
  InitActivation(test_case, arena, activation);
  absl::StatusOr<Value> got = program->Evaluate(&arena, activation);

  EXPECT_THAT(got, IsOkAndHolds(test_case.expected_matcher));
}

TEST_P(EvaluatorMemorySafetyTest, ProgramSafeAfterRuntimeDestroyed) {
  const auto& test_case = GetTestCase();

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Runtime> runtime, ConfigureRuntime());

  ASSERT_OK_AND_ASSIGN(ValidationResult validation,
                       GetCompiler().Compile(test_case.expression));

  ASSERT_TRUE(validation.IsValid()) << validation.FormatError();
  ASSERT_OK_AND_ASSIGN(auto ast, validation.ReleaseAst());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       runtime->CreateProgram(std::move(ast)));

  Activation activation;
  google::protobuf::Arena arena;
  InitActivation(test_case, arena, activation);
  runtime.reset();
  absl::StatusOr<Value> got = program->Evaluate(&arena, activation);
  EXPECT_THAT(got, IsOkAndHolds(test_case.expected_matcher));
}

// Helper for making an eternal string value without looking like a memory leak.
Value MakeStringValue(absl::string_view str) {
  static absl::NoDestructor<google::protobuf::Arena> kArena;
  return StringValue::Wrap(str, kArena.get());
}

NestedTestAllTypes MakeNestedTestAllTypes(absl::string_view textproto) {
  NestedTestAllTypes msg;
  ABSL_CHECK(google::protobuf::TextFormat::ParseFromString(textproto, &msg));
  return msg;
}

MATCHER_P(ParsedProtoStructEquals, expected, "") {
  const cel::StructValue& got = arg;
  if (!got.IsParsedMessage()) {
    return false;
  }
  auto& msg = got.GetParsedMessage();
  auto cmp = absl::WrapUnique(msg->New());
  if (!google::protobuf::TextFormat::ParseFromString(expected, cmp.get())) {
    *result_listener << "Failed to parse expected proto";
    return false;
  }
  return google::protobuf::util::MessageDifferencer::Equals(*msg, *cmp);
}

INSTANTIATE_TEST_SUITE_P(
    Expression, EvaluatorMemorySafetyTest,
    testing::Combine(
        testing::ValuesIn(std::vector<TestCase>{
            {
                "bool",
                "(true && false) || bool_var || string_var == 'test_str'",
                {{"bool_var", BoolValue(false)},
                 {"string_var", MakeStringValue("test_str")}},
                test::BoolValueIs(true),
            },
            {
                "const_str",
                "condition ? 'left_hand_string' : 'right_hand_string'",
                {{"condition", BoolValue(false)}},
                test::StringValueIs("right_hand_string"),
            },
            {
                "long_const_string",
                "condition ? 'left_hand_string' : "
                "'long_right_hand_string_0123456789'",
                {{"condition", BoolValue(false)}},
                test::StringValueIs("long_right_hand_string_0123456789"),
            },
            {
                "computed_string",
                "(condition ? 'a.b' : 'b.c') + '.d.e.f'",
                {{"condition", BoolValue(false)}},
                test::StringValueIs("b.c.d.e.f"),
            },
            {
                "regex",
                R"('192.168.128.64'.matches(r'^192\.168\.[0-2]?[0-9]?[0-9]\.[0-2]?[0-9]?[0-9]') )",
                {},
                test::BoolValueIs(true),
            },
            {
                "list_create",
                "[1, 2, 3, 4, 5, 6][3] == 4",
                {},
                test::BoolValueIs(true),
            },
            {
                "list_create_strings",
                "['1', '2', '3', '4', '5', '6'][2] == '3'",
                {},
                test::BoolValueIs(true),
            },
            {
                "map_create",
                "{'1': 'one', '2': 'two'}['2']",
                {},
                test::StringValueIs("two"),
            },
            {
                "struct_create",
                R"cel(
                  NestedTestAllTypes{
                    child: NestedTestAllTypes{
                      payload: TestAllTypes{
                        repeated_int32: [1, 2, 3]
                      }
                    },
                    payload: TestAllTypes{
                      repeated_string: ["foo", "bar", "baz"]
                    }
                  })cel",
                {},
                test::StructValueIs(ParsedProtoStructEquals(R"pb(
                  child { payload { repeated_int32: [ 1, 2, 3 ] } }
                  payload { repeated_string: [ "foo", "bar", "baz" ] }
                )pb")),
            },
            {"extension_function",
             "IsPrivate('8.8.8.8')",
             {},
             test::BoolValueIs(false),
             /*enable_reference_resolver=*/false},
            {"namespaced_function",
             "net.IsPrivate('192.168.0.1')",
             {},
             test::BoolValueIs(true),
             /*enable_reference_resolver=*/true},
            {
                "comprehension",
                "['abc', 'def', 'ghi', 'jkl'].exists(el, el == 'mno')",
                {},
                test::BoolValueIs(false),
            },
            {
                "comprehension_complex",
                "['a' + 'b' + 'c', 'd' + 'ef', 'g' + 'hi', 'j' + 'kl']"
                ".exists(el, el.startsWith('g'))",
                {},
                test::BoolValueIs(true),
            },
            TestCase{
                "unsafe_message_access",
                "nested_test_all_types.child.payload",
                {{"nested_test_all_types",
                  MakeNestedTestAllTypes(R"pb(child {
                                                payload { single_int32: 1 }
                                              })pb")}},
                test::StructValueIs(
                    ParsedProtoStructEquals(R"pb(single_int32: 1)pb")),
            },
            TestCase{
                "unsafe_message_access_repeated_field",
                "nested_test_all_types.payload.repeated_int32.size() == 3",
                {{"nested_test_all_types",
                  MakeNestedTestAllTypes(R"pb(payload {
                                                repeated_int32: 1
                                                repeated_int32: 2
                                                repeated_int32: 3
                                              })pb")}},
                test::BoolValueIs(true),
            },
            TestCase{
                "unsafe_message_access_repeated_field_index",
                "nested_test_all_types.payload.repeated_int32[1] == 2",
                {{"nested_test_all_types",
                  MakeNestedTestAllTypes(R"pb(payload {
                                                repeated_int32: 1
                                                repeated_int32: 2
                                                repeated_int32: 3
                                              })pb")}},
                test::BoolValueIs(true),
            },
            TestCase{
                "unsafe_message_access_map_field",
                "nested_test_all_types.payload.map_int32_string.size() == 2",
                {{"nested_test_all_types",
                  MakeNestedTestAllTypes(
                      R"pb(payload {
                             map_int32_string { key: 1 value: "foo" }
                             map_int32_string { key: 2 value: "bar" }
                           })pb")}},
                test::BoolValueIs(true),
            },
            TestCase{
                "unsafe_message_access_map_field_index",
                "nested_test_all_types.payload.map_int32_string[1] == 'foo'",
                {{"nested_test_all_types",
                  MakeNestedTestAllTypes(
                      R"pb(payload {
                             map_int32_string { key: 1 value: "foo" }
                             map_int32_string { key: 2 value: "bar" }
                           })pb")}},
                test::BoolValueIs(true),
            },
            TestCase{
                "unsafe_message_access_string_field",
                "nested_test_all_types.payload.single_string == 'foo'",
                {{"nested_test_all_types", MakeNestedTestAllTypes(
                                               R"pb(payload {
                                                      single_string: "foo"
                                                    })pb")}},
                test::BoolValueIs(true),
            },
            TestCase{
                "unsafe_message_access_assign",
                "NestedTestAllTypes{payload: "
                "nested_test_all_types.child.payload}",
                {{"nested_test_all_types",
                  MakeNestedTestAllTypes(R"pb(child {
                                                payload { single_int32: 1 }
                                              })pb")}},
                test::StructValueIs(ParsedProtoStructEquals(R"pb(payload {
                                                                   single_int32:
                                                                       1
                                                                 })pb")),
            },
            TestCase{
                "unsafe_message_access_assign_repeated_field",
                "TestAllTypes{repeated_int32: "
                "nested_test_all_types.payload.repeated_int32}",
                {{"nested_test_all_types", MakeNestedTestAllTypes(R"pb(
                    payload { repeated_int32: [ 1, 2, 3 ] }
                  )pb")}},
                test::StructValueIs(ParsedProtoStructEquals(
                    R"pb(repeated_int32: [ 1, 2, 3 ])pb")),
            },
            TestCase{
                "unsafe_message_access_assign_map_field",
                "TestAllTypes{map_int32_string: "
                "nested_test_all_types.payload.map_int32_string}",
                {{"nested_test_all_types", MakeNestedTestAllTypes(R"pb(
                    payload {
                      map_int32_string { key: 1 value: "foo" }
                      map_int32_string { key: 2 value: "bar" }
                    }
                  )pb")}},
                test::StructValueIs(ParsedProtoStructEquals(
                    R"pb(map_int32_string { key: 1 value: "foo" }
                         map_int32_string { key: 2 value: "bar" })pb")),
            },
        }),
        testing::Values(Options::kDefault, Options::kExhaustive,
                        Options::kFoldConstants)),
    &TestCaseName);

MATCHER_P(IsSameInstance, expected, "") {
  return std::mem_fn(&ParsedMessageValue::operator->)(&arg) == expected;
}

class ViewTypesMemorySafetyTest : public testing::TestWithParam<Options> {
 protected:
  Options EvaluationOptions() { return GetParam(); }
};

// Test cases demonstrating how inputs as views are handled.
TEST_P(ViewTypesMemorySafetyTest, WrappedMessage) {
  // Arrange: create the runtime and expression.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Runtime> runtime,
                       ConfigureRuntimeImpl(false, EvaluationOptions()));
  constexpr absl::string_view kProtoValue = R"pb(
    child { payload { repeated_int32: [ 1, 2, 3 ] } }
    payload { repeated_string: [ "foo", "bar", "baz" ] }
  )pb";

  ASSERT_OK_AND_ASSIGN(
      ValidationResult validation,
      GetCompiler().Compile(
          "condition ? nested_test_all_types : NestedTestAllTypes{}"));
  ASSERT_TRUE(validation.IsValid()) << validation.FormatError();
  ASSERT_OK_AND_ASSIGN(auto ast, validation.ReleaseAst());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       runtime->CreateProgram(std::move(ast)));

  // Act: wrap the message and evaluate the expression.
  google::protobuf::Arena arena;
  NestedTestAllTypes* proto =
      NestedTestAllTypes::default_instance().New(&arena);
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kProtoValue, proto));
  Activation activation;
  activation.InsertOrAssignValue("condition", BoolValue(true));
  activation.InsertOrAssignValue(
      "nested_test_all_types",
      Value::WrapMessage(proto, google::protobuf::DescriptorPool::generated_pool(),
                         google::protobuf::MessageFactory::generated_factory(), &arena));
  ASSERT_OK_AND_ASSIGN(Value result, program->Evaluate(&arena, activation));

  // Assert: the result is the input message.
  ASSERT_TRUE(result.IsParsedMessage());
  const ParsedMessageValue& result_msg = result.GetParsedMessage();
  EXPECT_THAT(result_msg,
              test::StructValueIs(ParsedProtoStructEquals(kProtoValue)));
  EXPECT_EQ(result_msg->GetArena(), &arena);
  EXPECT_THAT(result_msg, IsSameInstance(proto));
}

// Test cases demonstrating how inputs as views are handled.
TEST_P(ViewTypesMemorySafetyTest, WrappedMessageFields) {
  // Arrange: create the runtime and expression.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Runtime> runtime,
                       ConfigureRuntimeImpl(false, EvaluationOptions()));
  constexpr absl::string_view kProtoValue = R"pb(
    child { payload { repeated_int32: [ 1, 2, 3 ] } }
    payload { repeated_string: [ "foo", "bar", "baz" ] }
  )pb";
  ASSERT_OK_AND_ASSIGN(
      ValidationResult validation,
      GetCompiler().Compile("nested_test_all_types.child.payload"));
  ASSERT_TRUE(validation.IsValid()) << validation.FormatError();
  ASSERT_OK_AND_ASSIGN(auto ast, validation.ReleaseAst());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       runtime->CreateProgram(std::move(ast)));

  // Act: wrap the message and evaluate the expression.
  google::protobuf::Arena arena;
  NestedTestAllTypes* proto =
      NestedTestAllTypes::default_instance().New(&arena);
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kProtoValue, proto));
  Activation activation;
  activation.InsertOrAssignValue("condition", BoolValue(true));
  activation.InsertOrAssignValue(
      "nested_test_all_types",
      Value::WrapMessage(proto, google::protobuf::DescriptorPool::generated_pool(),
                         google::protobuf::MessageFactory::generated_factory(), &arena));
  ASSERT_OK_AND_ASSIGN(Value result, program->Evaluate(&arena, activation));

  // Assert: the result is an alias of a sub-message in the input.
  ASSERT_TRUE(result.IsParsedMessage());
  const ParsedMessageValue& result_msg = result.GetParsedMessage();
  EXPECT_THAT(result_msg, test::StructValueIs(ParsedProtoStructEquals(
                              "repeated_int32: [ 1, 2, 3 ]")));
  EXPECT_EQ(result_msg->GetArena(), &arena);
  EXPECT_THAT(result_msg, IsSameInstance(&(proto->child().payload())));
}

TEST_P(ViewTypesMemorySafetyTest, WrappedMessageDifferentArena) {
  // Arrange: create the runtime and expression.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Runtime> runtime,
                       ConfigureRuntimeImpl(false, EvaluationOptions()));
  constexpr absl::string_view kProtoValue = R"pb(
    child { payload { repeated_int32: [ 1, 2, 3 ] } }
    payload { repeated_string: [ "foo", "bar", "baz" ] }
  )pb";

  ASSERT_OK_AND_ASSIGN(
      ValidationResult validation,
      GetCompiler().Compile(
          "condition ? nested_test_all_types : NestedTestAllTypes{}"));
  ASSERT_TRUE(validation.IsValid()) << validation.FormatError();
  ASSERT_OK_AND_ASSIGN(auto ast, validation.ReleaseAst());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       runtime->CreateProgram(std::move(ast)));

  // Act: wrap the message and evaluate the expression.
  google::protobuf::Arena arena;
  google::protobuf::Arena other_arena;
  NestedTestAllTypes* proto =
      NestedTestAllTypes::default_instance().New(&other_arena);
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kProtoValue, proto));
  Activation activation;
  activation.InsertOrAssignValue("condition", BoolValue(true));
  activation.InsertOrAssignValue(
      "nested_test_all_types",
      Value::WrapMessage(proto, google::protobuf::DescriptorPool::generated_pool(),
                         google::protobuf::MessageFactory::generated_factory(), &arena));
  ASSERT_OK_AND_ASSIGN(Value result, program->Evaluate(&arena, activation));

  // Assert: the result is a copy of the input message.
  ASSERT_TRUE(result.IsParsedMessage());
  const ParsedMessageValue& result_msg = result.GetParsedMessage();
  EXPECT_THAT(result_msg,
              test::StructValueIs(ParsedProtoStructEquals(kProtoValue)));
  EXPECT_EQ(result_msg->GetArena(), &arena);
  EXPECT_THAT(result_msg, Not(IsSameInstance(proto)));
}

TEST_P(ViewTypesMemorySafetyTest, WrappedMessageFromAny) {
  // Arrange: create the runtime.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Runtime> runtime,
                       ConfigureRuntimeImpl(false, EvaluationOptions()));
  constexpr absl::string_view kProtoValue = R"pb(
    child { payload { repeated_int32: [ 1, 2, 3 ] } }
    payload { repeated_string: [ "foo", "bar", "baz" ] }
  )pb";

  ASSERT_OK_AND_ASSIGN(
      ValidationResult validation,
      GetCompiler().Compile(
          "condition ? nested_test_all_types : NestedTestAllTypes{}"));
  ASSERT_TRUE(validation.IsValid()) << validation.FormatError();
  ASSERT_OK_AND_ASSIGN(auto ast, validation.ReleaseAst());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       runtime->CreateProgram(std::move(ast)));

  // Act: wrap the message and evaluate the expression.
  google::protobuf::Arena arena;
  NestedTestAllTypes proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kProtoValue, &proto));
  Any any;
  any.PackFrom(proto);
  Activation activation;
  activation.InsertOrAssignValue("condition", BoolValue(true));
  activation.InsertOrAssignValue(
      "nested_test_all_types",
      Value::WrapMessage(&any, google::protobuf::DescriptorPool::generated_pool(),
                         google::protobuf::MessageFactory::generated_factory(), &arena));

  // Assert
  ASSERT_OK_AND_ASSIGN(Value result, program->Evaluate(&arena, activation));
  ASSERT_TRUE(result.IsParsedMessage());
  const ParsedMessageValue& result_msg = result.GetParsedMessage();
  EXPECT_THAT(result_msg,
              test::StructValueIs(ParsedProtoStructEquals(kProtoValue)));
  EXPECT_EQ(result_msg->GetArena(), &arena);
}

TEST_P(ViewTypesMemorySafetyTest, UnsafeWrappedMessageDifferentArena) {
  // Arrange: create the runtime and expression.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Runtime> runtime,
                       ConfigureRuntimeImpl(false, EvaluationOptions()));
  constexpr absl::string_view kProtoValue = R"pb(
    child { payload { repeated_int32: [ 1, 2, 3 ] } }
    payload { repeated_string: [ "foo", "bar", "baz" ] }
  )pb";

  ASSERT_OK_AND_ASSIGN(
      ValidationResult validation,
      GetCompiler().Compile(
          "condition ? nested_test_all_types : NestedTestAllTypes{}"));
  ASSERT_TRUE(validation.IsValid()) << validation.FormatError();
  ASSERT_OK_AND_ASSIGN(auto ast, validation.ReleaseAst());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       runtime->CreateProgram(std::move(ast)));

  // Act: wrap the message and evaluate the expression.
  // The unsafe version will alias the input message, so caller must ensure
  // the input outlives the use of the `Value` rather than assuming it
  // is managed by the evaluation arena.
  google::protobuf::Arena arena;
  NestedTestAllTypes proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kProtoValue, &proto));
  Activation activation;
  activation.InsertOrAssignValue("condition", BoolValue(true));
  activation.InsertOrAssignValue(
      "nested_test_all_types",
      Value::WrapMessageUnsafe(&proto, google::protobuf::DescriptorPool::generated_pool(),
                               google::protobuf::MessageFactory::generated_factory(),
                               &arena));
  ASSERT_OK_AND_ASSIGN(Value result, program->Evaluate(&arena, activation));

  // Assert: the result is an alias of the input message.
  ASSERT_TRUE(result.IsParsedMessage());
  const ParsedMessageValue& result_msg = result.GetParsedMessage();
  EXPECT_THAT(result_msg,
              test::StructValueIs(ParsedProtoStructEquals(kProtoValue)));
  EXPECT_EQ(result_msg->GetArena(), nullptr);
  EXPECT_THAT(result_msg, IsSameInstance(&proto));
}

TEST_P(ViewTypesMemorySafetyTest, UnsafeWrappedMessageFields) {
  // Arrange: create the runtime and expression.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Runtime> runtime,
                       ConfigureRuntimeImpl(false, EvaluationOptions()));
  constexpr absl::string_view kProtoValue = R"pb(
    child { payload { repeated_int32: [ 1, 2, 3 ] } }
    payload { repeated_string: [ "foo", "bar", "baz" ] }
  )pb";
  ASSERT_OK_AND_ASSIGN(
      ValidationResult validation,
      GetCompiler().Compile("nested_test_all_types.child.payload"));
  ASSERT_TRUE(validation.IsValid()) << validation.FormatError();
  ASSERT_OK_AND_ASSIGN(auto ast, validation.ReleaseAst());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       runtime->CreateProgram(std::move(ast)));

  // Act: wrap the message and evaluate the expression.
  google::protobuf::Arena arena;
  NestedTestAllTypes proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kProtoValue, &proto));
  Activation activation;
  activation.InsertOrAssignValue("condition", BoolValue(true));
  activation.InsertOrAssignValue(
      "nested_test_all_types",
      Value::WrapMessageUnsafe(&proto, google::protobuf::DescriptorPool::generated_pool(),
                               google::protobuf::MessageFactory::generated_factory(),
                               &arena));
  ASSERT_OK_AND_ASSIGN(Value result, program->Evaluate(&arena, activation));

  // Assert: the result is an alias of a sub-message in the input.
  ASSERT_TRUE(result.IsParsedMessage());
  const ParsedMessageValue& result_msg = result.GetParsedMessage();
  EXPECT_THAT(result_msg, test::StructValueIs(ParsedProtoStructEquals(
                              "repeated_int32: [ 1, 2, 3 ]")));
  EXPECT_EQ(result_msg->GetArena(), nullptr);
  EXPECT_THAT(result_msg, IsSameInstance(&(proto.child().payload())));
}

TEST_P(ViewTypesMemorySafetyTest, UnsafeWrappedMessageRepeatedField) {
  // Arrange: create the runtime and expression.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Runtime> runtime,
                       ConfigureRuntimeImpl(false, EvaluationOptions()));
  constexpr absl::string_view kProtoValue = R"pb(
    payload { repeated_nested_message: { bb: 42 } }
  )pb";
  ASSERT_OK_AND_ASSIGN(
      ValidationResult validation,
      GetCompiler().Compile(
          "nested_test_all_types.payload.repeated_nested_message[0]"));
  ASSERT_TRUE(validation.IsValid()) << validation.FormatError();
  ASSERT_OK_AND_ASSIGN(auto ast, validation.ReleaseAst());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       runtime->CreateProgram(std::move(ast)));

  // Act: wrap the message and evaluate the expression.
  google::protobuf::Arena arena;
  NestedTestAllTypes proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kProtoValue, &proto));
  Activation activation;
  activation.InsertOrAssignValue("condition", BoolValue(true));
  activation.InsertOrAssignValue(
      "nested_test_all_types",
      Value::WrapMessageUnsafe(&proto, google::protobuf::DescriptorPool::generated_pool(),
                               google::protobuf::MessageFactory::generated_factory(),
                               &arena));
  ASSERT_OK_AND_ASSIGN(Value result, program->Evaluate(&arena, activation));

  // Assert: the result is an alias of a sub-message in the input.
  ASSERT_TRUE(result.IsParsedMessage());
  const ParsedMessageValue& result_msg = result.GetParsedMessage();
  EXPECT_THAT(result_msg,
              test::StructValueIs(ParsedProtoStructEquals("bb: 42")));
  EXPECT_EQ(result_msg->GetArena(), nullptr);
  EXPECT_THAT(result_msg,
              IsSameInstance(&(proto.payload().repeated_nested_message(0))));
}

TEST_P(ViewTypesMemorySafetyTest, UnsafeWrappedMessageMapField) {
  // Arrange: create the runtime and expression.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Runtime> runtime,
                       ConfigureRuntimeImpl(false, EvaluationOptions()));
  constexpr absl::string_view kProtoValue = R"pb(
    payload {
      map_string_message: {
        key: "foo"
        value: { bb: 42 }
      }
      map_string_message: {
        key: "bar"
        value: { bb: 43 }
      }
    })pb";
  ASSERT_OK_AND_ASSIGN(
      ValidationResult validation,
      GetCompiler().Compile(
          "nested_test_all_types.payload.map_string_message['foo']"));
  ASSERT_TRUE(validation.IsValid()) << validation.FormatError();
  ASSERT_OK_AND_ASSIGN(auto ast, validation.ReleaseAst());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       runtime->CreateProgram(std::move(ast)));

  // Act: wrap the message and evaluate the expression.
  google::protobuf::Arena arena;
  NestedTestAllTypes proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kProtoValue, &proto));
  Activation activation;
  activation.InsertOrAssignValue("condition", BoolValue(true));
  activation.InsertOrAssignValue(
      "nested_test_all_types",
      Value::WrapMessageUnsafe(&proto, google::protobuf::DescriptorPool::generated_pool(),
                               google::protobuf::MessageFactory::generated_factory(),
                               &arena));
  ASSERT_OK_AND_ASSIGN(Value result, program->Evaluate(&arena, activation));

  // Assert: the result is an alias of a sub-message in the input.
  ASSERT_TRUE(result.IsParsedMessage());
  const ParsedMessageValue& result_msg = result.GetParsedMessage();
  EXPECT_THAT(result_msg,
              test::StructValueIs(ParsedProtoStructEquals("bb: 42")));
  EXPECT_EQ(result_msg->GetArena(), nullptr);
  EXPECT_THAT(
      result_msg,
      IsSameInstance(&(proto.payload().map_string_message().at("foo"))));
}

INSTANTIATE_TEST_SUITE_P(Cases, ViewTypesMemorySafetyTest,
                         testing::Values(Options::kDefault,
                                         Options::kExhaustive,
                                         Options::kFoldConstants),
                         [](const testing::TestParamInfo<Options>& info) {
                           switch (info.param) {
                             case Options::kDefault:
                               return "default";
                             case Options::kExhaustive:
                               return "exhaustive";
                             case Options::kFoldConstants:
                               return "opt";
                           }
                         });

}  // namespace
}  // namespace cel
