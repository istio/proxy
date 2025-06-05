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

#include <utility>
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
using ::cel::test::StringValueIs;
using ::testing::AnyOf;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Pair;
using ::testing::PrintToStringParamName;
using ::testing::TestWithParam;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

using TestAllTypesProto3 = ::google::api::expr::test::v1::proto3::TestAllTypes;

class ParsedJsonMapValueTest : public TestWithParam<AllocatorKind> {
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

TEST_P(ParsedJsonMapValueTest, Kind) {
  EXPECT_EQ(ParsedJsonMapValue::kind(), ParsedJsonMapValue::kKind);
  EXPECT_EQ(ParsedJsonMapValue::kind(), ValueKind::kMap);
}

TEST_P(ParsedJsonMapValueTest, GetTypeName) {
  EXPECT_EQ(ParsedJsonMapValue::GetTypeName(), ParsedJsonMapValue::kName);
  EXPECT_EQ(ParsedJsonMapValue::GetTypeName(), "google.protobuf.Struct");
}

TEST_P(ParsedJsonMapValueTest, GetRuntimeType) {
  ParsedJsonMapValue value;
  EXPECT_EQ(ParsedJsonMapValue::GetRuntimeType(), JsonMapType());
}

TEST_P(ParsedJsonMapValueTest, DebugString_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(R"pb()pb"));
  EXPECT_EQ(valid_value.DebugString(), "{}");
}

TEST_P(ParsedJsonMapValueTest, IsZeroValue_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(R"pb()pb"));
  EXPECT_TRUE(valid_value.IsZeroValue());
}

TEST_P(ParsedJsonMapValueTest, SerializeTo_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(R"pb()pb"));
  absl::Cord serialized;
  EXPECT_THAT(valid_value.SerializeTo(value_manager(), serialized), IsOk());
  EXPECT_THAT(serialized, IsEmpty());
}

TEST_P(ParsedJsonMapValueTest, ConvertToJson_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(R"pb()pb"));
  EXPECT_THAT(valid_value.ConvertToJson(value_manager()),
              IsOkAndHolds(VariantWith<JsonObject>(JsonObject())));
}

TEST_P(ParsedJsonMapValueTest, Equal_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(R"pb()pb"));
  EXPECT_THAT(valid_value.Equal(value_manager(), BoolValue()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(
      valid_value.Equal(
          value_manager(),
          ParsedJsonMapValue(
              DynamicParseTextProto<google::protobuf::Struct>(R"pb()pb"))),
      IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(valid_value.Equal(value_manager(), MapValue()),
              IsOkAndHolds(BoolValueIs(true)));
}

TEST_P(ParsedJsonMapValueTest, Empty_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(R"pb()pb"));
  EXPECT_TRUE(valid_value.IsEmpty());
}

TEST_P(ParsedJsonMapValueTest, Size_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(R"pb()pb"));
  EXPECT_EQ(valid_value.Size(), 0);
}

TEST_P(ParsedJsonMapValueTest, Get_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(
          R"pb(fields {
                 key: "foo"
                 value: {}
               }
               fields {
                 key: "bar"
                 value: { bool_value: true }
               })pb"));
  EXPECT_THAT(
      valid_value.Get(value_manager(), BoolValue()),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kNotFound))));
  EXPECT_THAT(valid_value.Get(value_manager(), StringValue("foo")),
              IsOkAndHolds(IsNullValue()));
  EXPECT_THAT(valid_value.Get(value_manager(), StringValue("bar")),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(
      valid_value.Get(value_manager(), StringValue("baz")),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kNotFound))));
}

TEST_P(ParsedJsonMapValueTest, Find_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(
          R"pb(fields {
                 key: "foo"
                 value: {}
               }
               fields {
                 key: "bar"
                 value: { bool_value: true }
               })pb"));
  EXPECT_THAT(valid_value.Find(value_manager(), BoolValue()),
              IsOkAndHolds(Pair(IsNullValue(), IsFalse())));
  EXPECT_THAT(valid_value.Find(value_manager(), StringValue("foo")),
              IsOkAndHolds(Pair(IsNullValue(), IsTrue())));
  EXPECT_THAT(valid_value.Find(value_manager(), StringValue("bar")),
              IsOkAndHolds(Pair(BoolValueIs(true), IsTrue())));
  EXPECT_THAT(valid_value.Find(value_manager(), StringValue("baz")),
              IsOkAndHolds(Pair(IsNullValue(), IsFalse())));
}

TEST_P(ParsedJsonMapValueTest, Has_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(
          R"pb(fields {
                 key: "foo"
                 value: {}
               }
               fields {
                 key: "bar"
                 value: { bool_value: true }
               })pb"));
  EXPECT_THAT(valid_value.Has(value_manager(), BoolValue()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(valid_value.Has(value_manager(), StringValue("foo")),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(valid_value.Has(value_manager(), StringValue("bar")),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(valid_value.Has(value_manager(), StringValue("baz")),
              IsOkAndHolds(BoolValueIs(false)));
}

TEST_P(ParsedJsonMapValueTest, ListKeys_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(
          R"pb(fields {
                 key: "foo"
                 value: {}
               }
               fields {
                 key: "bar"
                 value: { bool_value: true }
               })pb"));
  ASSERT_OK_AND_ASSIGN(auto keys, valid_value.ListKeys(value_manager()));
  EXPECT_THAT(keys.Size(), IsOkAndHolds(2));
  EXPECT_THAT(keys.DebugString(),
              AnyOf("[\"foo\", \"bar\"]", "[\"bar\", \"foo\"]"));
  EXPECT_THAT(keys.Contains(value_manager(), BoolValue()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(keys.Contains(value_manager(), StringValue("bar")),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(keys.Get(value_manager(), 0),
              IsOkAndHolds(AnyOf(StringValueIs("foo"), StringValueIs("bar"))));
  EXPECT_THAT(keys.Get(value_manager(), 1),
              IsOkAndHolds(AnyOf(StringValueIs("foo"), StringValueIs("bar"))));
  EXPECT_THAT(
      keys.ConvertToJson(value_manager()),
      IsOkAndHolds(AnyOf(VariantWith<JsonArray>(MakeJsonArray(
                             {JsonString("foo"), JsonString("bar")})),
                         VariantWith<JsonArray>(MakeJsonArray(
                             {JsonString("bar"), JsonString("foo")})))));
}

TEST_P(ParsedJsonMapValueTest, ForEach_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(
          R"pb(fields {
                 key: "foo"
                 value: {}
               }
               fields {
                 key: "bar"
                 value: { bool_value: true }
               })pb"));
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      valid_value.ForEach(
          value_manager(),
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          }),
      IsOk());
  EXPECT_THAT(entries, UnorderedElementsAre(
                           Pair(StringValueIs("foo"), IsNullValue()),
                           Pair(StringValueIs("bar"), BoolValueIs(true))));
}

TEST_P(ParsedJsonMapValueTest, NewIterator_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(
          R"pb(fields {
                 key: "foo"
                 value: {}
               }
               fields {
                 key: "bar"
                 value: { bool_value: true }
               })pb"));
  ASSERT_OK_AND_ASSIGN(auto iterator, valid_value.NewIterator(value_manager()));
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(value_manager()),
              IsOkAndHolds(AnyOf(StringValueIs("foo"), StringValueIs("bar"))));
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(value_manager()),
              IsOkAndHolds(AnyOf(StringValueIs("foo"), StringValueIs("bar"))));
  ASSERT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(value_manager()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

INSTANTIATE_TEST_SUITE_P(ParsedJsonMapValueTest, ParsedJsonMapValueTest,
                         ::testing::Values(AllocatorKind::kArena,
                                           AllocatorKind::kNewDelete),
                         PrintToStringParamName());

}  // namespace
}  // namespace cel
