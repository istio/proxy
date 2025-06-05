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
#include "eval/public/equality_function_registrar.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/any.pb.h"
#include "google/rpc/context/attribute_context.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "eval/public/activation.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/structs/trivial_legacy_type_info.h"
#include "eval/public/testing/matchers.h"
#include "eval/testutil/test_message.pb.h"  // IWYU pragma: keep
#include "internal/benchmark.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "parser/parser.h"

namespace google::api::expr::runtime {
namespace {

using ::absl_testing::StatusIs;
using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::rpc::context::AttributeContext;
using ::testing::_;
using ::testing::Combine;
using ::testing::HasSubstr;
using ::testing::Optional;
using ::testing::Values;
using ::testing::ValuesIn;

MATCHER_P2(DefinesHomogenousOverload, name, argument_type,
           absl::StrCat(name, " for ", CelValue::TypeName(argument_type))) {
  const CelFunctionRegistry& registry = arg;
  return !registry
              .FindOverloads(name, /*receiver_style=*/false,
                             {argument_type, argument_type})
              .empty();
  return false;
}

struct EqualityTestCase {
  enum class ErrorKind { kMissingOverload, kMissingIdentifier };
  absl::string_view expr;
  absl::variant<bool, ErrorKind> result;
  CelValue lhs = CelValue::CreateNull();
  CelValue rhs = CelValue::CreateNull();
};

bool IsNumeric(CelValue::Type type) {
  return type == CelValue::Type::kDouble || type == CelValue::Type::kInt64 ||
         type == CelValue::Type::kUint64;
}

const CelList& CelListExample1() {
  static ContainerBackedListImpl* example =
      new ContainerBackedListImpl({CelValue::CreateInt64(1)});
  return *example;
}

const CelList& CelListExample2() {
  static ContainerBackedListImpl* example =
      new ContainerBackedListImpl({CelValue::CreateInt64(2)});
  return *example;
}

const CelMap& CelMapExample1() {
  static CelMap* example = []() {
    std::vector<std::pair<CelValue, CelValue>> values{
        {CelValue::CreateInt64(1), CelValue::CreateInt64(2)}};
    // Implementation copies values into a hash map.
    auto map = CreateContainerBackedMap(absl::MakeSpan(values));
    return map->release();
  }();
  return *example;
}

const CelMap& CelMapExample2() {
  static CelMap* example = []() {
    std::vector<std::pair<CelValue, CelValue>> values{
        {CelValue::CreateInt64(2), CelValue::CreateInt64(4)}};
    auto map = CreateContainerBackedMap(absl::MakeSpan(values));
    return map->release();
  }();
  return *example;
}

const std::vector<CelValue>& ValueExamples1() {
  static std::vector<CelValue>* examples = []() {
    google::protobuf::Arena arena;
    auto result = std::make_unique<std::vector<CelValue>>();

    result->push_back(CelValue::CreateNull());
    result->push_back(CelValue::CreateBool(false));
    result->push_back(CelValue::CreateInt64(1));
    result->push_back(CelValue::CreateUint64(1));
    result->push_back(CelValue::CreateDouble(1.0));
    result->push_back(CelValue::CreateStringView("string"));
    result->push_back(CelValue::CreateBytesView("bytes"));
    // No arena allocs expected in this example.
    result->push_back(CelProtoWrapper::CreateMessage(
        std::make_unique<TestMessage>().release(), &arena));
    result->push_back(CelValue::CreateDuration(absl::Seconds(1)));
    result->push_back(CelValue::CreateTimestamp(absl::FromUnixSeconds(1)));
    result->push_back(CelValue::CreateList(&CelListExample1()));
    result->push_back(CelValue::CreateMap(&CelMapExample1()));
    result->push_back(CelValue::CreateCelTypeView("type"));

    return result.release();
  }();
  return *examples;
}

const std::vector<CelValue>& ValueExamples2() {
  static std::vector<CelValue>* examples = []() {
    google::protobuf::Arena arena;
    auto result = std::make_unique<std::vector<CelValue>>();
    auto message2 = std::make_unique<TestMessage>();
    message2->set_int64_value(2);

    result->push_back(CelValue::CreateNull());
    result->push_back(CelValue::CreateBool(true));
    result->push_back(CelValue::CreateInt64(2));
    result->push_back(CelValue::CreateUint64(2));
    result->push_back(CelValue::CreateDouble(2.0));
    result->push_back(CelValue::CreateStringView("string2"));
    result->push_back(CelValue::CreateBytesView("bytes2"));
    // No arena allocs expected in this example.
    result->push_back(
        CelProtoWrapper::CreateMessage(message2.release(), &arena));
    result->push_back(CelValue::CreateDuration(absl::Seconds(2)));
    result->push_back(CelValue::CreateTimestamp(absl::FromUnixSeconds(2)));
    result->push_back(CelValue::CreateList(&CelListExample2()));
    result->push_back(CelValue::CreateMap(&CelMapExample2()));
    result->push_back(CelValue::CreateCelTypeView("type2"));

    return result.release();
  }();
  return *examples;
}

class CelValueEqualImplTypesTest
    : public testing::TestWithParam<std::tuple<CelValue, CelValue, bool>> {
 public:
  CelValueEqualImplTypesTest() = default;

  const CelValue& lhs() { return std::get<0>(GetParam()); }

  const CelValue& rhs() { return std::get<1>(GetParam()); }

  bool should_be_equal() { return std::get<2>(GetParam()); }
};

std::string CelValueEqualTestName(
    const testing::TestParamInfo<std::tuple<CelValue, CelValue, bool>>&
        test_case) {
  return absl::StrCat(CelValue::TypeName(std::get<0>(test_case.param).type()),
                      CelValue::TypeName(std::get<1>(test_case.param).type()),
                      (std::get<2>(test_case.param)) ? "Equal" : "Inequal");
}

TEST_P(CelValueEqualImplTypesTest, Basic) {
  absl::optional<bool> result = CelValueEqualImpl(lhs(), rhs());

  if (lhs().IsNull() || rhs().IsNull()) {
    if (lhs().IsNull() && rhs().IsNull()) {
      EXPECT_THAT(result, Optional(true));
    } else {
      EXPECT_THAT(result, Optional(false));
    }
  } else if (lhs().type() == rhs().type() ||
             (IsNumeric(lhs().type()) && IsNumeric(rhs().type()))) {
    EXPECT_THAT(result, Optional(should_be_equal()));
  } else {
    EXPECT_THAT(result, Optional(false));
  }
}

INSTANTIATE_TEST_SUITE_P(EqualityBetweenTypes, CelValueEqualImplTypesTest,
                         Combine(ValuesIn(ValueExamples1()),
                                 ValuesIn(ValueExamples1()), Values(true)),
                         &CelValueEqualTestName);

INSTANTIATE_TEST_SUITE_P(InequalityBetweenTypes, CelValueEqualImplTypesTest,
                         Combine(ValuesIn(ValueExamples1()),
                                 ValuesIn(ValueExamples2()), Values(false)),
                         &CelValueEqualTestName);

struct NumericInequalityTestCase {
  std::string name;
  CelValue a;
  CelValue b;
};

const std::vector<NumericInequalityTestCase>& NumericValuesNotEqualExample() {
  static std::vector<NumericInequalityTestCase>* examples = []() {
    google::protobuf::Arena arena;
    auto result = std::make_unique<std::vector<NumericInequalityTestCase>>();
    result->push_back({"NegativeIntAndUint", CelValue::CreateInt64(-1),
                       CelValue::CreateUint64(2)});
    result->push_back(
        {"IntAndLargeUint", CelValue::CreateInt64(1),
         CelValue::CreateUint64(
             static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1)});
    result->push_back(
        {"IntAndLargeDouble", CelValue::CreateInt64(2),
         CelValue::CreateDouble(
             static_cast<double>(std::numeric_limits<int64_t>::max()) + 1025)});
    result->push_back(
        {"IntAndSmallDouble", CelValue::CreateInt64(2),
         CelValue::CreateDouble(
             static_cast<double>(std::numeric_limits<int64_t>::lowest()) -
             1025)});
    result->push_back(
        {"UintAndLargeDouble", CelValue::CreateUint64(2),
         CelValue::CreateDouble(
             static_cast<double>(std::numeric_limits<uint64_t>::max()) +
             2049)});
    result->push_back({"NegativeDoubleAndUint", CelValue::CreateDouble(-2.0),
                       CelValue::CreateUint64(123)});

    // NaN tests.
    result->push_back({"NanAndDouble", CelValue::CreateDouble(NAN),
                       CelValue::CreateDouble(1.0)});
    result->push_back({"NanAndNan", CelValue::CreateDouble(NAN),
                       CelValue::CreateDouble(NAN)});
    result->push_back({"DoubleAndNan", CelValue::CreateDouble(1.0),
                       CelValue::CreateDouble(NAN)});
    result->push_back(
        {"IntAndNan", CelValue::CreateInt64(1), CelValue::CreateDouble(NAN)});
    result->push_back(
        {"NanAndInt", CelValue::CreateDouble(NAN), CelValue::CreateInt64(1)});
    result->push_back(
        {"UintAndNan", CelValue::CreateUint64(1), CelValue::CreateDouble(NAN)});
    result->push_back(
        {"NanAndUint", CelValue::CreateDouble(NAN), CelValue::CreateUint64(1)});

    return result.release();
  }();
  return *examples;
}

using NumericInequalityTest = testing::TestWithParam<NumericInequalityTestCase>;
TEST_P(NumericInequalityTest, NumericValues) {
  NumericInequalityTestCase test_case = GetParam();
  absl::optional<bool> result = CelValueEqualImpl(test_case.a, test_case.b);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, false);
}

INSTANTIATE_TEST_SUITE_P(
    InequalityBetweenNumericTypesTest, NumericInequalityTest,
    ValuesIn(NumericValuesNotEqualExample()),
    [](const testing::TestParamInfo<NumericInequalityTest::ParamType>& info) {
      return info.param.name;
    });

TEST(CelValueEqualImplTest, LossyNumericEquality) {
  absl::optional<bool> result = CelValueEqualImpl(
      CelValue::CreateDouble(
          static_cast<double>(std::numeric_limits<int64_t>::max()) - 1),
      CelValue::CreateInt64(std::numeric_limits<int64_t>::max()));
  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(*result);
}

TEST(CelValueEqualImplTest, ListMixedTypesInequal) {
  ContainerBackedListImpl lhs({CelValue::CreateInt64(1)});
  ContainerBackedListImpl rhs({CelValue::CreateStringView("abc")});

  EXPECT_THAT(
      CelValueEqualImpl(CelValue::CreateList(&lhs), CelValue::CreateList(&rhs)),
      Optional(false));
}

TEST(CelValueEqualImplTest, NestedList) {
  ContainerBackedListImpl inner_lhs({CelValue::CreateInt64(1)});
  ContainerBackedListImpl lhs({CelValue::CreateList(&inner_lhs)});
  ContainerBackedListImpl inner_rhs({CelValue::CreateNull()});
  ContainerBackedListImpl rhs({CelValue::CreateList(&inner_rhs)});

  EXPECT_THAT(
      CelValueEqualImpl(CelValue::CreateList(&lhs), CelValue::CreateList(&rhs)),
      Optional(false));
}

TEST(CelValueEqualImplTest, MapMixedValueTypesInequal) {
  std::vector<std::pair<CelValue, CelValue>> lhs_data{
      {CelValue::CreateInt64(1), CelValue::CreateStringView("abc")}};
  std::vector<std::pair<CelValue, CelValue>> rhs_data{
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)}};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelMap> lhs,
                       CreateContainerBackedMap(absl::MakeSpan(lhs_data)));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelMap> rhs,
                       CreateContainerBackedMap(absl::MakeSpan(rhs_data)));

  EXPECT_THAT(CelValueEqualImpl(CelValue::CreateMap(lhs.get()),
                                CelValue::CreateMap(rhs.get())),
              Optional(false));
}

TEST(CelValueEqualImplTest, MapMixedKeyTypesEqual) {
  std::vector<std::pair<CelValue, CelValue>> lhs_data{
      {CelValue::CreateUint64(1), CelValue::CreateStringView("abc")}};
  std::vector<std::pair<CelValue, CelValue>> rhs_data{
      {CelValue::CreateInt64(1), CelValue::CreateStringView("abc")}};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelMap> lhs,
                       CreateContainerBackedMap(absl::MakeSpan(lhs_data)));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelMap> rhs,
                       CreateContainerBackedMap(absl::MakeSpan(rhs_data)));

  EXPECT_THAT(CelValueEqualImpl(CelValue::CreateMap(lhs.get()),
                                CelValue::CreateMap(rhs.get())),
              Optional(true));
}

TEST(CelValueEqualImplTest, MapMixedKeyTypesInequal) {
  std::vector<std::pair<CelValue, CelValue>> lhs_data{
      {CelValue::CreateInt64(1), CelValue::CreateStringView("abc")}};
  std::vector<std::pair<CelValue, CelValue>> rhs_data{
      {CelValue::CreateInt64(2), CelValue::CreateInt64(2)}};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelMap> lhs,
                       CreateContainerBackedMap(absl::MakeSpan(lhs_data)));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelMap> rhs,
                       CreateContainerBackedMap(absl::MakeSpan(rhs_data)));

  EXPECT_THAT(CelValueEqualImpl(CelValue::CreateMap(lhs.get()),
                                CelValue::CreateMap(rhs.get())),
              Optional(false));
}

TEST(CelValueEqualImplTest, NestedMaps) {
  std::vector<std::pair<CelValue, CelValue>> inner_lhs_data{
      {CelValue::CreateInt64(2), CelValue::CreateStringView("abc")}};
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<CelMap> inner_lhs,
      CreateContainerBackedMap(absl::MakeSpan(inner_lhs_data)));
  std::vector<std::pair<CelValue, CelValue>> lhs_data{
      {CelValue::CreateInt64(1), CelValue::CreateMap(inner_lhs.get())}};

  std::vector<std::pair<CelValue, CelValue>> inner_rhs_data{
      {CelValue::CreateInt64(2), CelValue::CreateNull()}};
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<CelMap> inner_rhs,
      CreateContainerBackedMap(absl::MakeSpan(inner_rhs_data)));
  std::vector<std::pair<CelValue, CelValue>> rhs_data{
      {CelValue::CreateInt64(1), CelValue::CreateMap(inner_rhs.get())}};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelMap> lhs,
                       CreateContainerBackedMap(absl::MakeSpan(lhs_data)));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelMap> rhs,
                       CreateContainerBackedMap(absl::MakeSpan(rhs_data)));

  EXPECT_THAT(CelValueEqualImpl(CelValue::CreateMap(lhs.get()),
                                CelValue::CreateMap(rhs.get())),
              Optional(false));
}

TEST(CelValueEqualImplTest, ProtoEqualityDifferingTypenameInequal) {
  // If message wrappers report a different typename, treat as inequal without
  // calling into the provided equal implementation.
  google::protobuf::Arena arena;
  TestMessage example;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"(
    int32_value: 1
    uint32_value: 2
    string_value: "test"
  )",
                                                  &example));

  CelValue lhs = CelProtoWrapper::CreateMessage(&example, &arena);
  CelValue rhs = CelValue::CreateMessageWrapper(
      MessageWrapper(&example, TrivialTypeInfo::GetInstance()));

  EXPECT_THAT(CelValueEqualImpl(lhs, rhs), Optional(false));
}

TEST(CelValueEqualImplTest, ProtoEqualityNoAccessorInequal) {
  // If message wrappers report no access apis, then treat as inequal.
  google::protobuf::Arena arena;
  TestMessage example;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"(
    int32_value: 1
    uint32_value: 2
    string_value: "test"
  )",
                                                  &example));

  CelValue lhs = CelValue::CreateMessageWrapper(
      MessageWrapper(&example, TrivialTypeInfo::GetInstance()));
  CelValue rhs = CelValue::CreateMessageWrapper(
      MessageWrapper(&example, TrivialTypeInfo::GetInstance()));

  EXPECT_THAT(CelValueEqualImpl(lhs, rhs), Optional(false));
}

TEST(CelValueEqualImplTest, ProtoEqualityAny) {
  google::protobuf::Arena arena;
  TestMessage packed_value;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"(
    int32_value: 1
    uint32_value: 2
    string_value: "test"
  )",
                                                  &packed_value));

  TestMessage lhs;
  lhs.mutable_any_value()->PackFrom(packed_value);

  TestMessage rhs;
  rhs.mutable_any_value()->PackFrom(packed_value);

  EXPECT_THAT(CelValueEqualImpl(CelProtoWrapper::CreateMessage(&lhs, &arena),
                                CelProtoWrapper::CreateMessage(&rhs, &arena)),
              Optional(true));

  // Equality falls back to bytewise comparison if type is missing.
  lhs.mutable_any_value()->clear_type_url();
  rhs.mutable_any_value()->clear_type_url();
  EXPECT_THAT(CelValueEqualImpl(CelProtoWrapper::CreateMessage(&lhs, &arena),
                                CelProtoWrapper::CreateMessage(&rhs, &arena)),
              Optional(true));
}

// Add transitive dependencies in appropriate order for the dynamic descriptor
// pool.
// Return false if the dependencies could not be added to the pool.
bool AddDepsToPool(const google::protobuf::FileDescriptor* descriptor,
                   google::protobuf::DescriptorPool& pool) {
  for (int i = 0; i < descriptor->dependency_count(); i++) {
    if (!AddDepsToPool(descriptor->dependency(i), pool)) {
      return false;
    }
  }
  google::protobuf::FileDescriptorProto descriptor_proto;
  descriptor->CopyTo(&descriptor_proto);
  return pool.BuildFile(descriptor_proto) != nullptr;
}

// Equivalent descriptors managed by separate descriptor pools are not equal, so
// the underlying messages are not considered equal.
TEST(CelValueEqualImplTest, DynamicDescriptorAndGeneratedInequal) {
  // Simulate a dynamically loaded descriptor that happens to match the
  // compiled version.
  google::protobuf::DescriptorPool pool;
  google::protobuf::DynamicMessageFactory factory;
  google::protobuf::Arena arena;
  factory.SetDelegateToGeneratedFactory(false);

  ASSERT_TRUE(AddDepsToPool(TestMessage::descriptor()->file(), pool));

  TestMessage example_message;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(R"pb(
                                            int64_value: 12345
                                            bool_list: false
                                            bool_list: true
                                            message_value { float_value: 1.0 }
                                          )pb",
                                          &example_message));

  // Messages from a loaded descriptor and generated versions can't be compared
  // via MessageDifferencer, so return false.
  std::unique_ptr<google::protobuf::Message> example_dynamic_message(
      factory
          .GetPrototype(pool.FindMessageTypeByName(
              TestMessage::descriptor()->full_name()))
          ->New());

  ASSERT_TRUE(example_dynamic_message->ParseFromString(
      example_message.SerializeAsString()));

  EXPECT_THAT(CelValueEqualImpl(
                  CelProtoWrapper::CreateMessage(&example_message, &arena),
                  CelProtoWrapper::CreateMessage(example_dynamic_message.get(),
                                                 &arena)),
              Optional(false));
}

TEST(CelValueEqualImplTest, DynamicMessageAndMessageEqual) {
  google::protobuf::DynamicMessageFactory factory;
  google::protobuf::Arena arena;
  factory.SetDelegateToGeneratedFactory(false);

  TestMessage example_message;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(R"pb(
                                            int64_value: 12345
                                            bool_list: false
                                            bool_list: true
                                            message_value { float_value: 1.0 }
                                          )pb",
                                          &example_message));

  // Dynamic message and generated Message subclass with the same generated
  // descriptor are comparable.
  std::unique_ptr<google::protobuf::Message> example_dynamic_message(
      factory.GetPrototype(TestMessage::descriptor())->New());

  ASSERT_TRUE(example_dynamic_message->ParseFromString(
      example_message.SerializeAsString()));

  EXPECT_THAT(CelValueEqualImpl(
                  CelProtoWrapper::CreateMessage(&example_message, &arena),
                  CelProtoWrapper::CreateMessage(example_dynamic_message.get(),
                                                 &arena)),
              Optional(true));
}

class EqualityFunctionTest
    : public testing::TestWithParam<std::tuple<EqualityTestCase, bool>> {
 public:
  EqualityFunctionTest() {
    options_.enable_heterogeneous_equality = std::get<1>(GetParam());
    options_.enable_empty_wrapper_null_unboxing = true;
    builder_ = CreateCelExpressionBuilder(options_);
  }

  CelFunctionRegistry& registry() { return *builder_->GetRegistry(); }

  absl::StatusOr<CelValue> Evaluate(absl::string_view expr, const CelValue& lhs,
                                    const CelValue& rhs) {
    CEL_ASSIGN_OR_RETURN(ParsedExpr parsed_expr, parser::Parse(expr));
    Activation activation;
    activation.InsertValue("lhs", lhs);
    activation.InsertValue("rhs", rhs);

    CEL_ASSIGN_OR_RETURN(auto expression,
                         builder_->CreateExpression(
                             &parsed_expr.expr(), &parsed_expr.source_info()));

    return expression->Evaluate(activation, &arena_);
  }

 protected:
  std::unique_ptr<CelExpressionBuilder> builder_;
  InterpreterOptions options_;
  google::protobuf::Arena arena_;
};

constexpr std::array<CelValue::Type, 11> kEqualableTypes = {
    CelValue::Type::kInt64,  CelValue::Type::kUint64,
    CelValue::Type::kString, CelValue::Type::kDouble,
    CelValue::Type::kBytes,  CelValue::Type::kDuration,
    CelValue::Type::kMap,    CelValue::Type::kList,
    CelValue::Type::kBool,   CelValue::Type::kTimestamp};

TEST(RegisterEqualityFunctionsTest, EqualDefined) {
  InterpreterOptions default_options;
  CelFunctionRegistry registry;
  ASSERT_OK(RegisterEqualityFunctions(&registry, default_options));
  for (CelValue::Type type : kEqualableTypes) {
    EXPECT_THAT(registry, DefinesHomogenousOverload(builtin::kEqual, type));
  }
}

TEST(RegisterEqualityFunctionsTest, InequalDefined) {
  InterpreterOptions default_options;
  CelFunctionRegistry registry;
  ASSERT_OK(RegisterEqualityFunctions(&registry, default_options));
  for (CelValue::Type type : kEqualableTypes) {
    EXPECT_THAT(registry, DefinesHomogenousOverload(builtin::kInequal, type));
  }
}

TEST_P(EqualityFunctionTest, SmokeTest) {
  EqualityTestCase test_case = std::get<0>(GetParam());
  google::protobuf::LinkMessageReflection<AttributeContext>();

  ASSERT_OK(RegisterEqualityFunctions(&registry(), options_));
  ASSERT_OK_AND_ASSIGN(auto result,
                       Evaluate(test_case.expr, test_case.lhs, test_case.rhs));

  if (absl::holds_alternative<bool>(test_case.result)) {
    EXPECT_THAT(result, test::IsCelBool(absl::get<bool>(test_case.result)));
  } else {
    switch (absl::get<EqualityTestCase::ErrorKind>(test_case.result)) {
      case EqualityTestCase::ErrorKind::kMissingOverload:
        EXPECT_THAT(result, test::IsCelError(
                                StatusIs(absl::StatusCode::kUnknown,
                                         HasSubstr("No matching overloads"))))
            << test_case.expr;
        break;
      case EqualityTestCase::ErrorKind::kMissingIdentifier:
        EXPECT_THAT(result, test::IsCelError(
                                StatusIs(absl::StatusCode::kUnknown,
                                         HasSubstr("found in Activation"))));
        break;
      default:
        EXPECT_THAT(result, test::IsCelError(_));
        break;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    Equality, EqualityFunctionTest,
    Combine(testing::ValuesIn<EqualityTestCase>(
                {{"null == null", true},
                 {"true == false", false},
                 {"1 == 1", true},
                 {"-2 == -1", false},
                 {"1.1 == 1.2", false},
                 {"'a' == 'a'", true},
                 {"lhs == rhs", false, CelValue::CreateBytesView("a"),
                  CelValue::CreateBytesView("b")},
                 {"lhs == rhs", false,
                  CelValue::CreateDuration(absl::Seconds(1)),
                  CelValue::CreateDuration(absl::Seconds(2))},
                 {"lhs == rhs", true,
                  CelValue::CreateTimestamp(absl::FromUnixSeconds(20)),
                  CelValue::CreateTimestamp(absl::FromUnixSeconds(20))},
                 // This should fail before getting to the equal operator.
                 {"no_such_identifier == 1",
                  EqualityTestCase::ErrorKind::kMissingIdentifier},
                 {"{1: no_such_identifier} == {1: 1}",
                  EqualityTestCase::ErrorKind::kMissingIdentifier}}),
            // heterogeneous equality enabled
            testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    Inequality, EqualityFunctionTest,
    Combine(testing::ValuesIn<EqualityTestCase>(
                {{"null != null", false},
                 {"true != false", true},
                 {"1 != 1", false},
                 {"-2 != -1", true},
                 {"1.1 != 1.2", true},
                 {"'a' != 'a'", false},
                 {"lhs != rhs", true, CelValue::CreateBytesView("a"),
                  CelValue::CreateBytesView("b")},
                 {"lhs != rhs", true,
                  CelValue::CreateDuration(absl::Seconds(1)),
                  CelValue::CreateDuration(absl::Seconds(2))},
                 {"lhs != rhs", true,
                  CelValue::CreateTimestamp(absl::FromUnixSeconds(20)),
                  CelValue::CreateTimestamp(absl::FromUnixSeconds(30))},
                 // This should fail before getting to the equal operator.
                 {"no_such_identifier != 1",
                  EqualityTestCase::ErrorKind::kMissingIdentifier},
                 {"{1: no_such_identifier} != {1: 1}",
                  EqualityTestCase::ErrorKind::kMissingIdentifier}}),
            // heterogeneous equality enabled
            testing::Bool()));

INSTANTIATE_TEST_SUITE_P(HeterogeneousNumericContainers, EqualityFunctionTest,
                         Combine(testing::ValuesIn<EqualityTestCase>({
                                     {"{1: 2} == {1u: 2}", true},
                                     {"{1: 2} == {2u: 2}", false},
                                     {"{1: 2} == {true: 2}", false},
                                     {"{1: 2} != {1u: 2}", false},
                                     {"{1: 2} != {2u: 2}", true},
                                     {"{1: 2} != {true: 2}", true},
                                     {"[1u, 2u, 3.0] != [1, 2.0, 3]", false},
                                     {"[1u, 2u, 3.0] == [1, 2.0, 3]", true},
                                     {"[1u, 2u, 3.0] != [1, 2.1, 3]", true},
                                     {"[1u, 2u, 3.0] == [1, 2.1, 3]", false},
                                 }),
                                 // heterogeneous equality enabled
                                 testing::Values(true)));

INSTANTIATE_TEST_SUITE_P(
    HomogenousNumericContainers, EqualityFunctionTest,
    Combine(testing::ValuesIn<EqualityTestCase>({
                {"{1: 2} == {1u: 2}", false},
                {"{1: 2} == {2u: 2}", false},
                {"{1: 2} == {true: 2}", false},
                {"{1: 2} != {1u: 2}", true},
                {"{1: 2} != {2u: 2}", true},
                {"{1: 2} != {true: 2}", true},
                {"[1u, 2u, 3.0] != [1, 2.0, 3]",
                 EqualityTestCase::ErrorKind::kMissingOverload},
                {"[1u, 2u, 3.0] == [1, 2.0, 3]",
                 EqualityTestCase::ErrorKind::kMissingOverload},
                {"[1u, 2u, 3.0] != [1, 2.1, 3]",
                 EqualityTestCase::ErrorKind::kMissingOverload},
                {"[1u, 2u, 3.0] == [1, 2.1, 3]",
                 EqualityTestCase::ErrorKind::kMissingOverload},
            }),
            // heterogeneous equality enabled
            testing::Values(false)));

INSTANTIATE_TEST_SUITE_P(
    NullInequalityLegacy, EqualityFunctionTest,
    Combine(testing::ValuesIn<EqualityTestCase>(
                {{"null != null", false},
                 {"true != null",
                  EqualityTestCase::ErrorKind::kMissingOverload},
                 {"1 != null", EqualityTestCase::ErrorKind::kMissingOverload},
                 {"-2 != null", EqualityTestCase::ErrorKind::kMissingOverload},
                 {"1.1 != null", EqualityTestCase::ErrorKind::kMissingOverload},
                 {"'a' != null", EqualityTestCase::ErrorKind::kMissingOverload},
                 {"lhs != null", EqualityTestCase::ErrorKind::kMissingOverload,
                  CelValue::CreateBytesView("a")},
                 {"lhs != null", EqualityTestCase::ErrorKind::kMissingOverload,
                  CelValue::CreateDuration(absl::Seconds(1))},
                 {"lhs != null", EqualityTestCase::ErrorKind::kMissingOverload,
                  CelValue::CreateTimestamp(absl::FromUnixSeconds(20))}}),
            // heterogeneous equality enabled
            testing::Values<bool>(false)));

INSTANTIATE_TEST_SUITE_P(
    NullEqualityLegacy, EqualityFunctionTest,
    Combine(testing::ValuesIn<EqualityTestCase>(
                {{"null == null", true},
                 {"true == null",
                  EqualityTestCase::ErrorKind::kMissingOverload},
                 {"1 == null", EqualityTestCase::ErrorKind::kMissingOverload},
                 {"-2 == null", EqualityTestCase::ErrorKind::kMissingOverload},
                 {"1.1 == null", EqualityTestCase::ErrorKind::kMissingOverload},
                 {"'a' == null", EqualityTestCase::ErrorKind::kMissingOverload},
                 {"lhs == null", EqualityTestCase::ErrorKind::kMissingOverload,
                  CelValue::CreateBytesView("a")},
                 {"lhs == null", EqualityTestCase::ErrorKind::kMissingOverload,
                  CelValue::CreateDuration(absl::Seconds(1))},
                 {"lhs == null", EqualityTestCase::ErrorKind::kMissingOverload,
                  CelValue::CreateTimestamp(absl::FromUnixSeconds(20))}}),
            // heterogeneous equality enabled
            testing::Values<bool>(false)));

INSTANTIATE_TEST_SUITE_P(
    NullInequality, EqualityFunctionTest,
    Combine(testing::ValuesIn<EqualityTestCase>(
                {{"null != null", false},
                 {"true != null", true},
                 {"null != false", true},
                 {"1 != null", true},
                 {"null != 1", true},
                 {"-2 != null", true},
                 {"null != -2", true},
                 {"1.1 != null", true},
                 {"null != 1.1", true},
                 {"'a' != null", true},
                 {"lhs != null", true, CelValue::CreateBytesView("a")},
                 {"lhs != null", true,
                  CelValue::CreateDuration(absl::Seconds(1))},
                 {"google.api.expr.runtime.TestMessage{} != null", true},
                 {"google.api.expr.runtime.TestMessage{}.string_wrapper_value"
                  " != null",
                  false},
                 {"google.api.expr.runtime.TestMessage{string_wrapper_value: "
                  "google.protobuf.StringValue{}}.string_wrapper_value != null",
                  true},
                 {"{} != null", true},
                 {"[] != null", true}}),
            // heterogeneous equality enabled
            testing::Values<bool>(true)));

INSTANTIATE_TEST_SUITE_P(
    NullEquality, EqualityFunctionTest,
    Combine(testing::ValuesIn<EqualityTestCase>({
                {"null == null", true},
                {"true == null", false},
                {"null == false", false},
                {"1 == null", false},
                {"null == 1", false},
                {"-2 == null", false},
                {"null == -2", false},
                {"1.1 == null", false},
                {"null == 1.1", false},
                {"'a' == null", false},
                {"lhs == null", false, CelValue::CreateBytesView("a")},
                {"lhs == null", false,
                 CelValue::CreateDuration(absl::Seconds(1))},
                {"google.api.expr.runtime.TestMessage{} == null", false},

                {"google.api.expr.runtime.TestMessage{}.string_wrapper_value"
                 " == null",
                 true},
                {"google.api.expr.runtime.TestMessage{string_wrapper_value: "
                 "google.protobuf.StringValue{}}.string_wrapper_value == null",
                 false},
                {"{} == null", false},
                {"[] == null", false},
            }),
            // heterogeneous equality enabled
            testing::Values<bool>(true)));

INSTANTIATE_TEST_SUITE_P(
    ProtoEquality, EqualityFunctionTest,
    Combine(testing::ValuesIn<EqualityTestCase>({
                {"google.api.expr.runtime.TestMessage{} == null", false},
                {"google.api.expr.runtime.TestMessage{string_wrapper_value: "
                 "google.protobuf.StringValue{}}.string_wrapper_value == ''",
                 true},
                {"google.api.expr.runtime.TestMessage{"
                 "int64_wrapper_value: "
                 "google.protobuf.Int64Value{value: 1},"
                 "double_value: 1.1} == "
                 "google.api.expr.runtime.TestMessage{"
                 "int64_wrapper_value: "
                 "google.protobuf.Int64Value{value: 1},"
                 "double_value: 1.1}",
                 true},
                // ProtoDifferencer::Equals distinguishes set fields vs
                // defaulted
                {"google.api.expr.runtime.TestMessage{"
                 "string_wrapper_value: google.protobuf.StringValue{}} == "
                 "google.api.expr.runtime.TestMessage{}",
                 false},
                // Differently typed messages inequal.
                {"google.api.expr.runtime.TestMessage{} == "
                 "google.rpc.context.AttributeContext{}",
                 false},
            }),
            // heterogeneous equality enabled
            testing::Values<bool>(true)));

void RunBenchmark(absl::string_view expr, benchmark::State& benchmark) {
  InterpreterOptions opts;
  auto builder = CreateCelExpressionBuilder(opts);
  ASSERT_OK(RegisterEqualityFunctions(builder->GetRegistry(), opts));
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, parser::Parse(expr));
  google::protobuf::Arena arena;
  Activation activation;

  ASSERT_OK_AND_ASSIGN(auto plan,
                       builder->CreateExpression(&parsed_expr.expr(),
                                                 &parsed_expr.source_info()));

  for (auto _ : benchmark) {
    ASSERT_OK_AND_ASSIGN(auto result, plan->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
  }
}

void RunIdentBenchmark(const CelValue& lhs, const CelValue& rhs,
                       benchmark::State& benchmark) {
  InterpreterOptions opts;
  auto builder = CreateCelExpressionBuilder(opts);
  ASSERT_OK(RegisterEqualityFunctions(builder->GetRegistry(), opts));
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, parser::Parse("lhs == rhs"));
  google::protobuf::Arena arena;
  Activation activation;
  activation.InsertValue("lhs", lhs);
  activation.InsertValue("rhs", rhs);

  ASSERT_OK_AND_ASSIGN(auto plan,
                       builder->CreateExpression(&parsed_expr.expr(),
                                                 &parsed_expr.source_info()));

  for (auto _ : benchmark) {
    ASSERT_OK_AND_ASSIGN(auto result, plan->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
  }
}

void BM_EqualsInt(benchmark::State& s) { RunBenchmark("42 == 43", s); }

BENCHMARK(BM_EqualsInt);

void BM_EqualsString(benchmark::State& s) {
  RunBenchmark("'1234' == '1235'", s);
}

BENCHMARK(BM_EqualsString);

void BM_EqualsCreatedList(benchmark::State& s) {
  RunBenchmark("[1, 2, 3, 4, 5] == [1, 2, 3, 4, 6]", s);
}

BENCHMARK(BM_EqualsCreatedList);

void BM_EqualsBoundLegacyList(benchmark::State& s) {
  ContainerBackedListImpl lhs(
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2),
       CelValue::CreateInt64(3), CelValue::CreateInt64(4),
       CelValue::CreateInt64(5)});
  ContainerBackedListImpl rhs(
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2),
       CelValue::CreateInt64(3), CelValue::CreateInt64(4),
       CelValue::CreateInt64(6)});

  RunIdentBenchmark(CelValue::CreateList(&lhs), CelValue::CreateList(&rhs), s);
}

BENCHMARK(BM_EqualsBoundLegacyList);

void BM_EqualsCreatedMap(benchmark::State& s) {
  RunBenchmark("{1: 2, 2: 3, 3: 6} == {1: 2, 2: 3, 3: 6}", s);
}

BENCHMARK(BM_EqualsCreatedMap);

}  // namespace
}  // namespace google::api::expr::runtime
