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
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
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
#include "internal/message_type_name.h"
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
using ::cel::test::BytesValueIs;
using ::cel::test::DoubleValueIs;
using ::cel::test::DurationValueIs;
using ::cel::test::ErrorValueIs;
using ::cel::test::IntValueIs;
using ::cel::test::IsNullValue;
using ::cel::test::StringValueIs;
using ::cel::test::UintValueIs;
using ::testing::_;
using ::testing::AnyOf;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::PrintToStringParamName;
using ::testing::TestWithParam;
using ::testing::VariantWith;

using TestAllTypesProto3 = ::google::api::expr::test::v1::proto3::TestAllTypes;

class ParsedMapFieldValueTest : public TestWithParam<AllocatorKind> {
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
  auto DynamicParseTextProto(absl::string_view text) {
    return ::cel::internal::DynamicParseTextProto<T>(
        allocator(), text, descriptor_pool(), message_factory());
  }

  template <typename T>
  absl::Nonnull<const google::protobuf::FieldDescriptor*> DynamicGetField(
      absl::string_view name) {
    return ABSL_DIE_IF_NULL(
        ABSL_DIE_IF_NULL(descriptor_pool()->FindMessageTypeByName(
                             internal::MessageTypeNameFor<T>()))
            ->FindFieldByName(name));
  }

 private:
  absl::optional<google::protobuf::Arena> arena_;
  absl::optional<Shared<ValueManager>> value_manager_;
};

TEST_P(ParsedMapFieldValueTest, Field) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"));
  EXPECT_TRUE(value);
}

TEST_P(ParsedMapFieldValueTest, Kind) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"));
  EXPECT_EQ(value.kind(), ParsedMapFieldValue::kKind);
  EXPECT_EQ(value.kind(), ValueKind::kMap);
}

TEST_P(ParsedMapFieldValueTest, GetTypeName) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"));
  EXPECT_EQ(value.GetTypeName(), ParsedMapFieldValue::kName);
  EXPECT_EQ(value.GetTypeName(), "map");
}

TEST_P(ParsedMapFieldValueTest, GetRuntimeType) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"));
  EXPECT_EQ(value.GetRuntimeType(), MapType());
}

TEST_P(ParsedMapFieldValueTest, DebugString) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"));
  EXPECT_THAT(value.DebugString(), _);
}

TEST_P(ParsedMapFieldValueTest, IsZeroValue) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"));
  EXPECT_TRUE(value.IsZeroValue());
}

TEST_P(ParsedMapFieldValueTest, SerializeTo) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"));
  absl::Cord serialized;
  EXPECT_THAT(value.SerializeTo(value_manager(), serialized), IsOk());
  EXPECT_THAT(serialized, IsEmpty());
}

TEST_P(ParsedMapFieldValueTest, ConvertToJson) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"));
  EXPECT_THAT(value.ConvertToJson(value_manager()),
              IsOkAndHolds(VariantWith<JsonObject>(JsonObject())));
}

TEST_P(ParsedMapFieldValueTest, Equal_MapField) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"));
  EXPECT_THAT(value.Equal(value_manager(), BoolValue()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(
      value.Equal(value_manager(),
                  ParsedMapFieldValue(
                      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
                      DynamicGetField<TestAllTypesProto3>("map_int32_int32"))),
      IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(value.Equal(value_manager(), MapValue()),
              IsOkAndHolds(BoolValueIs(true)));
}

TEST_P(ParsedMapFieldValueTest, Equal_JsonMap) {
  ParsedMapFieldValue map_value(
      DynamicParseTextProto<TestAllTypesProto3>(
          R"pb(map_string_string { key: "foo" value: "bar" }
               map_string_string { key: "bar" value: "foo" })pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_string"));
  ParsedJsonMapValue json_value(DynamicParseTextProto<google::protobuf::Struct>(
      R"pb(
        fields {
          key: "foo"
          value { string_value: "bar" }
        }
        fields {
          key: "bar"
          value { string_value: "foo" }
        }
      )pb"));
  EXPECT_THAT(map_value.Equal(value_manager(), json_value),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(json_value.Equal(value_manager(), map_value),
              IsOkAndHolds(BoolValueIs(true)));
}

TEST_P(ParsedMapFieldValueTest, Empty) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"));
  EXPECT_TRUE(value.IsEmpty());
}

TEST_P(ParsedMapFieldValueTest, Size) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"));
  EXPECT_EQ(value.Size(), 0);
}

TEST_P(ParsedMapFieldValueTest, Get) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_bool { key: "foo" value: false }
        map_string_bool { key: "bar" value: true }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_bool"));
  EXPECT_THAT(
      value.Get(value_manager(), BoolValue()),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kNotFound))));
  EXPECT_THAT(value.Get(value_manager(), StringValue("foo")),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Get(value_manager(), StringValue("bar")),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(
      value.Get(value_manager(), StringValue("baz")),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kNotFound))));
}

TEST_P(ParsedMapFieldValueTest, Find) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_bool { key: "foo" value: false }
        map_string_bool { key: "bar" value: true }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_bool"));
  EXPECT_THAT(value.Find(value_manager(), BoolValue()),
              IsOkAndHolds(Pair(IsNullValue(), IsFalse())));
  EXPECT_THAT(value.Find(value_manager(), StringValue("foo")),
              IsOkAndHolds(Pair(BoolValueIs(false), IsTrue())));
  EXPECT_THAT(value.Find(value_manager(), StringValue("bar")),
              IsOkAndHolds(Pair(BoolValueIs(true), IsTrue())));
  EXPECT_THAT(value.Find(value_manager(), StringValue("baz")),
              IsOkAndHolds(Pair(IsNullValue(), IsFalse())));
}

TEST_P(ParsedMapFieldValueTest, Has) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_bool { key: "foo" value: false }
        map_string_bool { key: "bar" value: true }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_bool"));
  EXPECT_THAT(value.Has(value_manager(), BoolValue()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Has(value_manager(), StringValue("foo")),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(value.Has(value_manager(), StringValue("bar")),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(value.Has(value_manager(), StringValue("baz")),
              IsOkAndHolds(BoolValueIs(false)));
}

TEST_P(ParsedMapFieldValueTest, ListKeys) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_bool { key: "foo" value: false }
        map_string_bool { key: "bar" value: true }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_bool"));
  ASSERT_OK_AND_ASSIGN(auto keys, value.ListKeys(value_manager()));
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

TEST_P(ParsedMapFieldValueTest, ForEach_StringBool) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_bool { key: "foo" value: false }
        map_string_bool { key: "bar" value: true }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_bool"));
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          value_manager(),
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          }),
      IsOk());
  EXPECT_THAT(entries, UnorderedElementsAre(
                           Pair(StringValueIs("foo"), BoolValueIs(false)),
                           Pair(StringValueIs("bar"), BoolValueIs(true))));
}

TEST_P(ParsedMapFieldValueTest, ForEach_Int32Double) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_int32_double { key: 1 value: 2 }
        map_int32_double { key: 2 value: 1 }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_int32_double"));
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          value_manager(),
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          }),
      IsOk());
  EXPECT_THAT(entries,
              UnorderedElementsAre(Pair(IntValueIs(1), DoubleValueIs(2)),
                                   Pair(IntValueIs(2), DoubleValueIs(1))));
}

TEST_P(ParsedMapFieldValueTest, ForEach_Int64Float) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_int64_float { key: 1 value: 2 }
        map_int64_float { key: 2 value: 1 }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_float"));
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          value_manager(),
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          }),
      IsOk());
  EXPECT_THAT(entries,
              UnorderedElementsAre(Pair(IntValueIs(1), DoubleValueIs(2)),
                                   Pair(IntValueIs(2), DoubleValueIs(1))));
}

TEST_P(ParsedMapFieldValueTest, ForEach_UInt32UInt64) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_uint32_uint64 { key: 1 value: 2 }
        map_uint32_uint64 { key: 2 value: 1 }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_uint32_uint64"));
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          value_manager(),
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          }),
      IsOk());
  EXPECT_THAT(entries,
              UnorderedElementsAre(Pair(UintValueIs(1), UintValueIs(2)),
                                   Pair(UintValueIs(2), UintValueIs(1))));
}

TEST_P(ParsedMapFieldValueTest, ForEach_UInt64Int32) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_uint64_int32 { key: 1 value: 2 }
        map_uint64_int32 { key: 2 value: 1 }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_uint64_int32"));
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          value_manager(),
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          }),
      IsOk());
  EXPECT_THAT(entries,
              UnorderedElementsAre(Pair(UintValueIs(1), IntValueIs(2)),
                                   Pair(UintValueIs(2), IntValueIs(1))));
}

TEST_P(ParsedMapFieldValueTest, ForEach_BoolUInt32) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_bool_uint32 { key: true value: 2 }
        map_bool_uint32 { key: false value: 1 }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_bool_uint32"));
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          value_manager(),
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          }),
      IsOk());
  EXPECT_THAT(entries,
              UnorderedElementsAre(Pair(BoolValueIs(true), UintValueIs(2)),
                                   Pair(BoolValueIs(false), UintValueIs(1))));
}

TEST_P(ParsedMapFieldValueTest, ForEach_StringString) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_string { key: "foo" value: "bar" }
        map_string_string { key: "bar" value: "foo" }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_string"));
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          value_manager(),
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          }),
      IsOk());
  EXPECT_THAT(entries, UnorderedElementsAre(
                           Pair(StringValueIs("foo"), StringValueIs("bar")),
                           Pair(StringValueIs("bar"), StringValueIs("foo"))));
}

TEST_P(ParsedMapFieldValueTest, ForEach_StringDuration) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_duration {
          key: "foo"
          value: { seconds: 1 nanos: 1 }
        }
        map_string_duration {
          key: "bar"
          value: {}
        }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_duration"));
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          value_manager(),
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          }),
      IsOk());
  EXPECT_THAT(
      entries,
      UnorderedElementsAre(
          Pair(StringValueIs("foo"),
               DurationValueIs(absl::Seconds(1) + absl::Nanoseconds(1))),
          Pair(StringValueIs("bar"), DurationValueIs(absl::ZeroDuration()))));
}

TEST_P(ParsedMapFieldValueTest, ForEach_StringBytes) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_bytes { key: "foo" value: "bar" }
        map_string_bytes { key: "bar" value: "foo" }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_bytes"));
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          value_manager(),
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          }),
      IsOk());
  EXPECT_THAT(entries, UnorderedElementsAre(
                           Pair(StringValueIs("foo"), BytesValueIs("bar")),
                           Pair(StringValueIs("bar"), BytesValueIs("foo"))));
}

TEST_P(ParsedMapFieldValueTest, ForEach_StringEnum) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_enum { key: "foo" value: BAR }
        map_string_enum { key: "bar" value: FOO }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_enum"));
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          value_manager(),
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          }),
      IsOk());
  EXPECT_THAT(entries,
              UnorderedElementsAre(Pair(StringValueIs("foo"), IntValueIs(1)),
                                   Pair(StringValueIs("bar"), IntValueIs(0))));
}

TEST_P(ParsedMapFieldValueTest, ForEach_StringNull) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_null_value { key: "foo" value: NULL_VALUE }
        map_string_null_value { key: "bar" value: NULL_VALUE }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_null_value"));
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          value_manager(),
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          }),
      IsOk());
  EXPECT_THAT(entries,
              UnorderedElementsAre(Pair(StringValueIs("foo"), IsNullValue()),
                                   Pair(StringValueIs("bar"), IsNullValue())));
}

TEST_P(ParsedMapFieldValueTest, NewIterator) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_bool { key: "foo" value: false }
        map_string_bool { key: "bar" value: true }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_bool"));
  ASSERT_OK_AND_ASSIGN(auto iterator, value.NewIterator(value_manager()));
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

INSTANTIATE_TEST_SUITE_P(ParsedMapFieldValueTest, ParsedMapFieldValueTest,
                         ::testing::Values(AllocatorKind::kArena,
                                           AllocatorKind::kNewDelete),
                         PrintToStringParamName());

}  // namespace
}  // namespace cel
