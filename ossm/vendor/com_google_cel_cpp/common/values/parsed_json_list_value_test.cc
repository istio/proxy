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

#include <cstddef>
#include <vector>

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/value_manager.h"
#include "common/value_testing.h"
#include "internal/parse_text_proto.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::internal::GetTestingDescriptorPool;
using ::cel::internal::GetTestingMessageFactory;
using ::cel::test::BoolValueIs;
using ::cel::test::ErrorValueIs;
using ::cel::test::IsNullValue;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::PrintToStringParamName;
using ::testing::TestWithParam;
using ::testing::VariantWith;

using TestAllTypesProto3 = ::google::api::expr::test::v1::proto3::TestAllTypes;

class ParsedJsonListValueTest : public TestWithParam<AllocatorKind> {
 public:
  void SetUp() override {
    switch (GetParam()) {
      case AllocatorKind::kArena:
        arena_.emplace();
        value_manager_ = NewThreadCompatibleValueManager(
            MemoryManager::Pooling(arena()),
            NewThreadCompatibleTypeReflector(MemoryManager::Pooling(arena())));
        break;
      case AllocatorKind::kNewDelete:
        value_manager_ = NewThreadCompatibleValueManager(
            MemoryManager::ReferenceCounting(),
            NewThreadCompatibleTypeReflector(
                MemoryManager::ReferenceCounting()));
        break;
    }
  }

  void TearDown() override {
    value_manager_.reset();
    arena_.reset();
  }

  Allocator<> allocator() {
    return arena_ ? Allocator(ArenaAllocator<>{&*arena_})
                  : Allocator(NewDeleteAllocator<>{});
  }

  absl::Nullable<google::protobuf::Arena*> arena() { return allocator().arena(); }

  absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool() {
    return GetTestingDescriptorPool();
  }

  absl::Nonnull<google::protobuf::MessageFactory*> message_factory() {
    return GetTestingMessageFactory();
  }

  ValueManager& value_manager() { return **value_manager_; }

  template <typename T>
  auto GeneratedParseTextProto(absl::string_view text) {
    return ::cel::internal::GeneratedParseTextProto<T>(
        allocator(), text, descriptor_pool(), message_factory());
  }

  template <typename T>
  auto DynamicParseTextProto(absl::string_view text) {
    return ::cel::internal::DynamicParseTextProto<T>(
        allocator(), text, descriptor_pool(), message_factory());
  }

 private:
  absl::optional<google::protobuf::Arena> arena_;
  absl::optional<Shared<ValueManager>> value_manager_;
};

TEST_P(ParsedJsonListValueTest, Kind) {
  EXPECT_EQ(ParsedJsonListValue::kind(), ParsedJsonListValue::kKind);
  EXPECT_EQ(ParsedJsonListValue::kind(), ValueKind::kList);
}

TEST_P(ParsedJsonListValueTest, GetTypeName) {
  EXPECT_EQ(ParsedJsonListValue::GetTypeName(), ParsedJsonListValue::kName);
  EXPECT_EQ(ParsedJsonListValue::GetTypeName(), "google.protobuf.ListValue");
}

TEST_P(ParsedJsonListValueTest, GetRuntimeType) {
  EXPECT_EQ(ParsedJsonListValue::GetRuntimeType(), JsonListType());
}

TEST_P(ParsedJsonListValueTest, DebugString_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(R"pb()pb"));
  EXPECT_EQ(valid_value.DebugString(), "[]");
}

TEST_P(ParsedJsonListValueTest, IsZeroValue_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(R"pb()pb"));
  EXPECT_TRUE(valid_value.IsZeroValue());
}

TEST_P(ParsedJsonListValueTest, SerializeTo_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(R"pb()pb"));
  absl::Cord serialized;
  EXPECT_THAT(valid_value.SerializeTo(value_manager(), serialized), IsOk());
  EXPECT_THAT(serialized, IsEmpty());
}

TEST_P(ParsedJsonListValueTest, ConvertToJson_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(R"pb()pb"));
  EXPECT_THAT(valid_value.ConvertToJson(value_manager()),
              IsOkAndHolds(VariantWith<JsonArray>(JsonArray())));
}

TEST_P(ParsedJsonListValueTest, Equal_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(R"pb()pb"));
  EXPECT_THAT(valid_value.Equal(value_manager(), BoolValue()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(
      valid_value.Equal(
          value_manager(),
          ParsedJsonListValue(
              DynamicParseTextProto<google::protobuf::ListValue>(R"pb()pb"))),
      IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(valid_value.Equal(value_manager(), ListValue()),
              IsOkAndHolds(BoolValueIs(true)));
}

TEST_P(ParsedJsonListValueTest, Empty_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(R"pb()pb"));
  EXPECT_TRUE(valid_value.IsEmpty());
}

TEST_P(ParsedJsonListValueTest, Size_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(R"pb()pb"));
  EXPECT_EQ(valid_value.Size(), 0);
}

TEST_P(ParsedJsonListValueTest, Get_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(
          R"pb(values {}
               values { bool_value: true })pb"));
  EXPECT_THAT(valid_value.Get(value_manager(), 0), IsOkAndHolds(IsNullValue()));
  EXPECT_THAT(valid_value.Get(value_manager(), 1),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(
      valid_value.Get(value_manager(), 2),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kInvalidArgument))));
}

TEST_P(ParsedJsonListValueTest, ForEach_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(
          R"pb(values {}
               values { bool_value: true })pb"));
  {
    std::vector<Value> values;
    EXPECT_THAT(
        valid_value.ForEach(value_manager(),
                            [&](const Value& element) -> absl::StatusOr<bool> {
                              values.push_back(element);
                              return true;
                            }),
        IsOk());
    EXPECT_THAT(values, ElementsAre(IsNullValue(), BoolValueIs(true)));
  }
  {
    std::vector<Value> values;
    EXPECT_THAT(valid_value.ForEach(
                    value_manager(),
                    [&](size_t, const Value& element) -> absl::StatusOr<bool> {
                      values.push_back(element);
                      return true;
                    }),
                IsOk());
    EXPECT_THAT(values, ElementsAre(IsNullValue(), BoolValueIs(true)));
  }
}

TEST_P(ParsedJsonListValueTest, NewIterator_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(
          R"pb(values {}
               values { bool_value: true })pb"));
  ASSERT_OK_AND_ASSIGN(auto iterator, valid_value.NewIterator(value_manager()));
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(value_manager()), IsOkAndHolds(IsNullValue()));
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(value_manager()), IsOkAndHolds(BoolValueIs(true)));
  ASSERT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(value_manager()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(ParsedJsonListValueTest, Contains_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(
          R"pb(values {}
               values { bool_value: true }
               values { number_value: 1.0 }
               values { string_value: "foo" }
               values { list_value: {} }
               values { struct_value: {} })pb"));
  EXPECT_THAT(valid_value.Contains(value_manager(), BytesValue()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(valid_value.Contains(value_manager(), NullValue()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(valid_value.Contains(value_manager(), BoolValue(false)),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(valid_value.Contains(value_manager(), BoolValue(true)),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(valid_value.Contains(value_manager(), DoubleValue(0.0)),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(valid_value.Contains(value_manager(), DoubleValue(1.0)),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(valid_value.Contains(value_manager(), StringValue("bar")),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(valid_value.Contains(value_manager(), StringValue("foo")),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(valid_value.Contains(
                  value_manager(),
                  ParsedJsonListValue(
                      DynamicParseTextProto<google::protobuf::ListValue>(
                          R"pb(values {}
                               values { bool_value: true }
                               values { number_value: 1.0 }
                               values { string_value: "foo" }
                               values { list_value: {} }
                               values { struct_value: {} })pb"))),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(valid_value.Contains(value_manager(), ListValue()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(
      valid_value.Contains(
          value_manager(),
          ParsedJsonMapValue(DynamicParseTextProto<google::protobuf::Struct>(
              R"pb(fields {
                     key: "foo"
                     value: { bool_value: true }
                   })pb"))),
      IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(valid_value.Contains(value_manager(), MapValue()),
              IsOkAndHolds(BoolValueIs(true)));
}

INSTANTIATE_TEST_SUITE_P(ParsedJsonListValueTest, ParsedJsonListValueTest,
                         ::testing::Values(AllocatorKind::kArena,
                                           AllocatorKind::kNewDelete),
                         PrintToStringParamName());

}  // namespace
}  // namespace cel
