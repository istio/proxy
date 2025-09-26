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
#include "eval/internal/cel_value_equal.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "google/rpc/context/attribute_context.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/structs/trivial_legacy_type_info.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"

namespace cel::interop_internal {
namespace {

using ::google::api::expr::runtime::CelList;
using ::google::api::expr::runtime::CelMap;
using ::google::api::expr::runtime::CelProtoWrapper;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::ContainerBackedListImpl;
using ::google::api::expr::runtime::CreateContainerBackedMap;
using ::google::api::expr::runtime::MessageWrapper;
using ::google::api::expr::runtime::TestMessage;
using ::google::api::expr::runtime::TrivialTypeInfo;
using ::testing::_;
using ::testing::Combine;
using ::testing::Optional;
using ::testing::Values;
using ::testing::ValuesIn;

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

}  // namespace
}  // namespace cel::interop_internal
