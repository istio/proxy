#include "eval/public/set_util.h"

#include <cstddef>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "google/protobuf/empty.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/unknown_set.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {

using google::protobuf::Arena;
using protobuf::Empty;
using protobuf::ListValue;
using protobuf::Struct;

constexpr char kExampleText[] = "abc";
constexpr char kExampleText2[] = "abd";

std::string* ExampleStr() {
  static std::string* example = new std::string(kExampleText);
  return example;
}

std::string* ExampleStr2() {
  static std::string* example = new std::string(kExampleText2);
  return example;
}

// Returns a vector that has an example for each type, ordered by the type
// ordering in |CelValueLessThan|. Length 13
std::vector<CelValue> TypeExamples(Arena* arena) {
  Empty* empty = Arena::Create<Empty>(arena);
  Struct* proto_map = Arena::Create<Struct>(arena);
  ListValue* proto_list = Arena::Create<ListValue>(arena);
  UnknownSet* unknown_set = Arena::Create<UnknownSet>(arena);
  return {CelValue::CreateBool(false),
          CelValue::CreateInt64(0),
          CelValue::CreateUint64(0),
          CelValue::CreateDouble(0.0),
          CelValue::CreateStringView(kExampleText),
          CelValue::CreateBytes(ExampleStr()),
          CelProtoWrapper::CreateMessage(empty, arena),
          CelValue::CreateDuration(absl::ZeroDuration()),
          CelValue::CreateTimestamp(absl::Now()),
          CelProtoWrapper::CreateMessage(proto_list, arena),
          CelProtoWrapper::CreateMessage(proto_map, arena),
          CelValue::CreateUnknownSet(unknown_set),
          CreateErrorValue(arena, "test", absl::StatusCode::kInternal)};
}

// Parameterized test for confirming type orderings are correct. Compares all
// pairs of type examples to confirm the expected type priority.
class TypeOrderingTest : public testing::TestWithParam<std::tuple<int, int>> {
 public:
  TypeOrderingTest() {
    i_ = std::get<0>(GetParam());
    j_ = std::get<1>(GetParam());
  }

 protected:
  int i_;
  int j_;
  Arena arena_;
};

TEST_P(TypeOrderingTest, TypeLessThan) {
  auto examples = TypeExamples(&arena_);
  CelValue lhs = examples[i_];
  CelValue rhs = examples[j_];

  // Strict less than.
  EXPECT_EQ(CelValueLessThan(lhs, rhs), i_ < j_);
  // Equality.
  EXPECT_EQ(CelValueEqual(lhs, rhs), i_ == j_);
}

std::string TypeOrderingTestName(
    testing::TestParamInfo<std::tuple<int, int>> param) {
  int i = std::get<0>(param.param);
  int j = std::get<1>(param.param);
  return absl::StrCat(CelValue::TypeName(CelValue::Type(i)), "_",
                      CelValue::TypeName(CelValue::Type(j)));
}

INSTANTIATE_TEST_SUITE_P(TypePairs, TypeOrderingTest,
                         testing::Combine(testing::Range(0, 13),
                                          testing::Range(0, 13)),
                         &TypeOrderingTestName);

TEST(CelValueLessThanComparator, StdSetSupport) {
  Arena arena;
  auto examples = TypeExamples(&arena);
  std::set<CelValue, CelValueLessThanComparator> value_set(&CelValueLessThan);

  for (CelValue value : examples) {
    auto insert = value_set.insert(value);
    bool was_inserted = insert.second;
    EXPECT_TRUE(was_inserted)
        << absl::StrCat("Insertion failed ", CelValue::TypeName(value.type()));
  }

  for (CelValue value : examples) {
    auto insert = value_set.insert(value);
    bool was_inserted = insert.second;
    EXPECT_FALSE(was_inserted) << absl::StrCat(
        "Re-insertion succeeded ", CelValue::TypeName(value.type()));
  }
}

enum class ExpectedCmp { kEq, kLt, kGt };

struct PrimitiveCmpTestCase {
  CelValue lhs;
  CelValue rhs;
  ExpectedCmp expected;
};

// Test for primitive types that just use operator< for the underlying value.
class PrimitiveCmpTest : public testing::TestWithParam<PrimitiveCmpTestCase> {
 public:
  PrimitiveCmpTest() {
    lhs_ = GetParam().lhs;
    rhs_ = GetParam().rhs;
    expected_ = GetParam().expected;
  }

 protected:
  CelValue lhs_;
  CelValue rhs_;
  ExpectedCmp expected_;
};

TEST_P(PrimitiveCmpTest, Basic) {
  switch (expected_) {
    case ExpectedCmp::kLt:
      EXPECT_TRUE(CelValueLessThan(lhs_, rhs_));
      break;
    case ExpectedCmp::kGt:
      EXPECT_TRUE(CelValueGreaterThan(lhs_, rhs_));
      break;
    case ExpectedCmp::kEq:
      EXPECT_TRUE(CelValueEqual(lhs_, rhs_));
      break;
  }
}

std::string PrimitiveCmpTestName(
    testing::TestParamInfo<PrimitiveCmpTestCase> info) {
  absl::string_view cmp_name;
  switch (info.param.expected) {
    case ExpectedCmp::kEq:
      cmp_name = "Eq";
      break;
    case ExpectedCmp::kLt:
      cmp_name = "Lt";
      break;
    case ExpectedCmp::kGt:
      cmp_name = "Gt";
      break;
  }
  return absl::StrCat(CelValue::TypeName(info.param.lhs.type()), "_", cmp_name);
}

INSTANTIATE_TEST_SUITE_P(
    Pairs, PrimitiveCmpTest,
    testing::ValuesIn(std::vector<PrimitiveCmpTestCase>{
        {CelValue::CreateStringView(kExampleText),
         CelValue::CreateStringView(kExampleText), ExpectedCmp::kEq},
        {CelValue::CreateStringView(kExampleText),
         CelValue::CreateStringView(kExampleText2), ExpectedCmp::kLt},
        {CelValue::CreateStringView(kExampleText2),
         CelValue::CreateStringView(kExampleText), ExpectedCmp::kGt},
        {CelValue::CreateBytes(ExampleStr()),
         CelValue::CreateBytes(ExampleStr()), ExpectedCmp::kEq},
        {CelValue::CreateBytes(ExampleStr()),
         CelValue::CreateBytes(ExampleStr2()), ExpectedCmp::kLt},
        {CelValue::CreateBytes(ExampleStr2()),
         CelValue::CreateBytes(ExampleStr()), ExpectedCmp::kGt},
        {CelValue::CreateBool(false), CelValue::CreateBool(false),
         ExpectedCmp::kEq},
        {CelValue::CreateBool(false), CelValue::CreateBool(true),
         ExpectedCmp::kLt},
        {CelValue::CreateBool(true), CelValue::CreateBool(false),
         ExpectedCmp::kGt},
        {CelValue::CreateInt64(1), CelValue::CreateInt64(1), ExpectedCmp::kEq},
        {CelValue::CreateInt64(1), CelValue::CreateInt64(2), ExpectedCmp::kLt},
        {CelValue::CreateInt64(2), CelValue::CreateInt64(1), ExpectedCmp::kGt},
        {CelValue::CreateUint64(1), CelValue::CreateUint64(1),
         ExpectedCmp::kEq},
        {CelValue::CreateUint64(1), CelValue::CreateUint64(2),
         ExpectedCmp::kLt},
        {CelValue::CreateUint64(2), CelValue::CreateUint64(1),
         ExpectedCmp::kGt},
        {CelValue::CreateDuration(absl::Minutes(1)),
         CelValue::CreateDuration(absl::Minutes(1)), ExpectedCmp::kEq},
        {CelValue::CreateDuration(absl::Minutes(1)),
         CelValue::CreateDuration(absl::Minutes(2)), ExpectedCmp::kLt},
        {CelValue::CreateDuration(absl::Minutes(2)),
         CelValue::CreateDuration(absl::Minutes(1)), ExpectedCmp::kGt},
        {CelValue::CreateTimestamp(absl::FromUnixSeconds(1)),
         CelValue::CreateTimestamp(absl::FromUnixSeconds(1)), ExpectedCmp::kEq},
        {CelValue::CreateTimestamp(absl::FromUnixSeconds(1)),
         CelValue::CreateTimestamp(absl::FromUnixSeconds(2)), ExpectedCmp::kLt},
        {CelValue::CreateTimestamp(absl::FromUnixSeconds(2)),
         CelValue::CreateTimestamp(absl::FromUnixSeconds(1)),
         ExpectedCmp::kGt}}),
    &PrimitiveCmpTestName);

TEST(CelValueLessThan, PtrCmpMessage) {
  Arena arena;

  CelValue lhs =
      CelProtoWrapper::CreateMessage(Arena::Create<Empty>(&arena), &arena);
  CelValue rhs =
      CelProtoWrapper::CreateMessage(Arena::Create<Empty>(&arena), &arena);

  if (lhs.MessageOrDie() > rhs.MessageOrDie()) {
    std::swap(lhs, rhs);
  }

  EXPECT_TRUE(CelValueLessThan(lhs, rhs));
  EXPECT_FALSE(CelValueLessThan(rhs, lhs));
  EXPECT_FALSE(CelValueLessThan(lhs, lhs));
}

TEST(CelValueLessThan, PtrCmpUnknownSet) {
  Arena arena;

  CelValue lhs = CelValue::CreateUnknownSet(Arena::Create<UnknownSet>(&arena));
  CelValue rhs = CelValue::CreateUnknownSet(Arena::Create<UnknownSet>(&arena));

  if (lhs.UnknownSetOrDie() > rhs.UnknownSetOrDie()) {
    std::swap(lhs, rhs);
  }

  EXPECT_TRUE(CelValueLessThan(lhs, rhs));
  EXPECT_FALSE(CelValueLessThan(rhs, lhs));
  EXPECT_FALSE(CelValueLessThan(lhs, lhs));
}

TEST(CelValueLessThan, PtrCmpError) {
  Arena arena;

  CelValue lhs = CreateErrorValue(&arena, "test1", absl::StatusCode::kInternal);
  CelValue rhs = CreateErrorValue(&arena, "test2", absl::StatusCode::kInternal);

  if (lhs.ErrorOrDie() > rhs.ErrorOrDie()) {
    std::swap(lhs, rhs);
  }

  EXPECT_TRUE(CelValueLessThan(lhs, rhs));
  EXPECT_FALSE(CelValueLessThan(rhs, lhs));
  EXPECT_FALSE(CelValueLessThan(lhs, lhs));
}

TEST(CelValueLessThan, CelListSameSize) {
  ContainerBackedListImpl cel_list_1(std::vector<CelValue>{
      CelValue::CreateInt64(1), CelValue::CreateInt64(2)});

  ContainerBackedListImpl cel_list_2(std::vector<CelValue>{
      CelValue::CreateInt64(1), CelValue::CreateInt64(3)});

  EXPECT_TRUE(CelValueLessThan(CelValue::CreateList(&cel_list_1),
                               CelValue::CreateList(&cel_list_2)));
}

TEST(CelValueLessThan, CelListDifferentSizes) {
  ContainerBackedListImpl cel_list_1(
      std::vector<CelValue>{CelValue::CreateInt64(2)});

  ContainerBackedListImpl cel_list_2(std::vector<CelValue>{
      CelValue::CreateInt64(1), CelValue::CreateInt64(3)});

  EXPECT_TRUE(CelValueLessThan(CelValue::CreateList(&cel_list_1),
                               CelValue::CreateList(&cel_list_2)));
}

TEST(CelValueLessThan, CelListEqual) {
  ContainerBackedListImpl cel_list_1(std::vector<CelValue>{
      CelValue::CreateInt64(1), CelValue::CreateInt64(2)});

  ContainerBackedListImpl cel_list_2(std::vector<CelValue>{
      CelValue::CreateInt64(1), CelValue::CreateInt64(2)});

  EXPECT_FALSE(CelValueLessThan(CelValue::CreateList(&cel_list_1),
                                CelValue::CreateList(&cel_list_2)));
  EXPECT_TRUE(CelValueEqual(CelValue::CreateList(&cel_list_2),
                            CelValue::CreateList(&cel_list_1)));
}

TEST(CelValueLessThan, CelListSupportProtoListCompatible) {
  Arena arena;

  ListValue list_value;

  list_value.add_values()->set_bool_value(true);
  list_value.add_values()->set_number_value(1.0);
  list_value.add_values()->set_string_value("abc");

  CelValue proto_list = CelProtoWrapper::CreateMessage(&list_value, &arena);
  ASSERT_TRUE(proto_list.IsList());

  std::vector<CelValue> list_values{CelValue::CreateBool(true),
                                    CelValue::CreateDouble(1.0),
                                    CelValue::CreateStringView("abd")};

  ContainerBackedListImpl list_backing(list_values);

  CelValue cel_list = CelValue::CreateList(&list_backing);

  EXPECT_TRUE(CelValueLessThan(proto_list, cel_list));
}

TEST(CelValueLessThan, CelMapSameSize) {
  std::vector<std::pair<CelValue, CelValue>> values{
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)},
      {CelValue::CreateInt64(3), CelValue::CreateInt64(6)}};

  auto cel_map_backing_1 =
      CreateContainerBackedMap(absl::MakeSpan(values)).value();

  std::vector<std::pair<CelValue, CelValue>> values2{
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)},
      {CelValue::CreateInt64(4), CelValue::CreateInt64(6)}};

  auto cel_map_backing_2 =
      CreateContainerBackedMap(absl::MakeSpan(values2)).value();

  std::vector<std::pair<CelValue, CelValue>> values3{
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)},
      {CelValue::CreateInt64(3), CelValue::CreateInt64(8)}};

  auto cel_map_backing_3 =
      CreateContainerBackedMap(absl::MakeSpan(values3)).value();

  CelValue map1 = CelValue::CreateMap(cel_map_backing_1.get());
  CelValue map2 = CelValue::CreateMap(cel_map_backing_2.get());
  CelValue map3 = CelValue::CreateMap(cel_map_backing_3.get());

  EXPECT_TRUE(CelValueLessThan(map1, map2));
  EXPECT_TRUE(CelValueLessThan(map1, map3));
  EXPECT_TRUE(CelValueLessThan(map3, map2));
}

TEST(CelValueLessThan, CelMapDifferentSizes) {
  std::vector<std::pair<CelValue, CelValue>> values{
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)},
      {CelValue::CreateInt64(2), CelValue::CreateInt64(4)}};

  auto cel_map_1 = CreateContainerBackedMap(absl::MakeSpan(values)).value();

  std::vector<std::pair<CelValue, CelValue>> values2{
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)},
      {CelValue::CreateInt64(2), CelValue::CreateInt64(4)},
      {CelValue::CreateInt64(3), CelValue::CreateInt64(6)}};

  auto cel_map_2 = CreateContainerBackedMap(absl::MakeSpan(values2)).value();

  EXPECT_TRUE(CelValueLessThan(CelValue::CreateMap(cel_map_1.get()),
                               CelValue::CreateMap(cel_map_2.get())));
}

TEST(CelValueLessThan, CelMapEqual) {
  std::vector<std::pair<CelValue, CelValue>> values{
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)},
      {CelValue::CreateInt64(2), CelValue::CreateInt64(4)},
      {CelValue::CreateInt64(3), CelValue::CreateInt64(6)}};

  auto cel_map_1 = CreateContainerBackedMap(absl::MakeSpan(values)).value();

  std::vector<std::pair<CelValue, CelValue>> values2{
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)},
      {CelValue::CreateInt64(2), CelValue::CreateInt64(4)},
      {CelValue::CreateInt64(3), CelValue::CreateInt64(6)}};

  auto cel_map_2 = CreateContainerBackedMap(absl::MakeSpan(values2)).value();

  EXPECT_FALSE(CelValueLessThan(CelValue::CreateMap(cel_map_1.get()),
                                CelValue::CreateMap(cel_map_2.get())));
  EXPECT_TRUE(CelValueEqual(CelValue::CreateMap(cel_map_2.get()),
                            CelValue::CreateMap(cel_map_1.get())));
}

TEST(CelValueLessThan, CelMapSupportProtoMapCompatible) {
  Arena arena;

  const std::vector<std::string> kFields = {"field1", "field2", "field3"};

  Struct value_struct;

  auto& value1 = (*value_struct.mutable_fields())[kFields[0]];
  value1.set_bool_value(true);

  auto& value2 = (*value_struct.mutable_fields())[kFields[1]];
  value2.set_number_value(1.0);

  auto& value3 = (*value_struct.mutable_fields())[kFields[2]];
  value3.set_string_value("test");

  CelValue proto_struct = CelProtoWrapper::CreateMessage(&value_struct, &arena);
  ASSERT_TRUE(proto_struct.IsMap());

  std::vector<std::pair<CelValue, CelValue>> values{
      {CelValue::CreateStringView(kFields[2]),
       CelValue::CreateStringView("test")},
      {CelValue::CreateStringView(kFields[1]), CelValue::CreateDouble(1.0)},
      {CelValue::CreateStringView(kFields[0]), CelValue::CreateBool(true)}};

  auto backing_map = CreateContainerBackedMap(absl::MakeSpan(values)).value();

  CelValue cel_map = CelValue::CreateMap(backing_map.get());

  EXPECT_TRUE(!CelValueLessThan(cel_map, proto_struct) &&
              !CelValueGreaterThan(cel_map, proto_struct));
}

TEST(CelValueLessThan, NestedMap) {
  Arena arena;

  ListValue list_value;

  list_value.add_values()->set_bool_value(true);
  list_value.add_values()->set_number_value(1.0);
  list_value.add_values()->set_string_value("test");

  std::vector<CelValue> list_values{CelValue::CreateBool(true),
                                    CelValue::CreateDouble(1.0),
                                    CelValue::CreateStringView("test")};

  ContainerBackedListImpl list_backing(list_values);

  CelValue cel_list = CelValue::CreateList(&list_backing);

  Struct value_struct;

  *(value_struct.mutable_fields()->operator[]("field").mutable_list_value()) =
      list_value;

  std::vector<std::pair<CelValue, CelValue>> values{
      {CelValue::CreateStringView("field"), cel_list}};

  auto backing_map = CreateContainerBackedMap(absl::MakeSpan(values)).value();

  CelValue cel_map = CelValue::CreateMap(backing_map.get());
  CelValue proto_map = CelProtoWrapper::CreateMessage(&value_struct, &arena);
  EXPECT_TRUE(!CelValueLessThan(cel_map, proto_map) &&
              !CelValueLessThan(proto_map, cel_map));
}

}  // namespace
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
