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
using ::cel::test::UintValueIs;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::PrintToStringParamName;
using ::testing::TestWithParam;
using ::testing::VariantWith;

using TestAllTypesProto3 = ::google::api::expr::test::v1::proto3::TestAllTypes;

class ParsedRepeatedFieldValueTest : public TestWithParam<AllocatorKind> {
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

TEST_P(ParsedRepeatedFieldValueTest, Field) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"));
  EXPECT_TRUE(value);
}

TEST_P(ParsedRepeatedFieldValueTest, Kind) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"));
  EXPECT_EQ(value.kind(), ParsedRepeatedFieldValue::kKind);
  EXPECT_EQ(value.kind(), ValueKind::kList);
}

TEST_P(ParsedRepeatedFieldValueTest, GetTypeName) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"));
  EXPECT_EQ(value.GetTypeName(), ParsedRepeatedFieldValue::kName);
  EXPECT_EQ(value.GetTypeName(), "list");
}

TEST_P(ParsedRepeatedFieldValueTest, GetRuntimeType) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"));
  EXPECT_EQ(value.GetRuntimeType(), ListType());
}

TEST_P(ParsedRepeatedFieldValueTest, DebugString) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"));
  EXPECT_THAT(value.DebugString(), _);
}

TEST_P(ParsedRepeatedFieldValueTest, IsZeroValue) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"));
  EXPECT_TRUE(value.IsZeroValue());
}

TEST_P(ParsedRepeatedFieldValueTest, SerializeTo) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"));
  absl::Cord serialized;
  EXPECT_THAT(value.SerializeTo(value_manager(), serialized), IsOk());
  EXPECT_THAT(serialized, IsEmpty());
}

TEST_P(ParsedRepeatedFieldValueTest, ConvertToJson) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"));
  EXPECT_THAT(value.ConvertToJson(value_manager()),
              IsOkAndHolds(VariantWith<JsonArray>(JsonArray())));
}

TEST_P(ParsedRepeatedFieldValueTest, Equal_RepeatedField) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"));
  EXPECT_THAT(value.Equal(value_manager(), BoolValue()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(
      value.Equal(value_manager(),
                  ParsedRepeatedFieldValue(
                      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
                      DynamicGetField<TestAllTypesProto3>("repeated_int64"))),
      IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(value.Equal(value_manager(), ListValue()),
              IsOkAndHolds(BoolValueIs(true)));
}

TEST_P(ParsedRepeatedFieldValueTest, Equal_JsonList) {
  ParsedRepeatedFieldValue repeated_value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_int64: 1
                                                     repeated_int64: 0)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"));
  ParsedJsonListValue json_value(
      DynamicParseTextProto<google::protobuf::ListValue>(
          R"pb(
            values { number_value: 1 }
            values { number_value: 0 }
          )pb"));
  EXPECT_THAT(repeated_value.Equal(value_manager(), json_value),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(json_value.Equal(value_manager(), repeated_value),
              IsOkAndHolds(BoolValueIs(true)));
}

TEST_P(ParsedRepeatedFieldValueTest, Empty) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"));
  EXPECT_TRUE(value.IsEmpty());
}

TEST_P(ParsedRepeatedFieldValueTest, Size) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"));
  EXPECT_EQ(value.Size(), 0);
}

TEST_P(ParsedRepeatedFieldValueTest, Get) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_bool: false
                                                     repeated_bool: true)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_bool"));
  EXPECT_THAT(value.Get(value_manager(), 0), IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Get(value_manager(), 1), IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(
      value.Get(value_manager(), 2),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kInvalidArgument))));
}

TEST_P(ParsedRepeatedFieldValueTest, ForEach_Bool) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_bool: false
                                                     repeated_bool: true)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_bool"));
  {
    std::vector<Value> values;
    EXPECT_THAT(
        value.ForEach(value_manager(),
                      [&](const Value& element) -> absl::StatusOr<bool> {
                        values.push_back(element);
                        return true;
                      }),
        IsOk());
    EXPECT_THAT(values, ElementsAre(BoolValueIs(false), BoolValueIs(true)));
  }
  {
    std::vector<Value> values;
    EXPECT_THAT(value.ForEach(
                    value_manager(),
                    [&](size_t, const Value& element) -> absl::StatusOr<bool> {
                      values.push_back(element);
                      return true;
                    }),
                IsOk());
    EXPECT_THAT(values, ElementsAre(BoolValueIs(false), BoolValueIs(true)));
  }
}

TEST_P(ParsedRepeatedFieldValueTest, ForEach_Double) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_double: 1
                                                     repeated_double: 0)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_double"));
  std::vector<Value> values;
  EXPECT_THAT(value.ForEach(value_manager(),
                            [&](const Value& element) -> absl::StatusOr<bool> {
                              values.push_back(element);
                              return true;
                            }),
              IsOk());
  EXPECT_THAT(values, ElementsAre(DoubleValueIs(1), DoubleValueIs(0)));
}

TEST_P(ParsedRepeatedFieldValueTest, ForEach_Float) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_float: 1
                                                     repeated_float: 0)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_float"));
  std::vector<Value> values;
  EXPECT_THAT(value.ForEach(value_manager(),
                            [&](const Value& element) -> absl::StatusOr<bool> {
                              values.push_back(element);
                              return true;
                            }),
              IsOk());
  EXPECT_THAT(values, ElementsAre(DoubleValueIs(1), DoubleValueIs(0)));
}

TEST_P(ParsedRepeatedFieldValueTest, ForEach_UInt64) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_uint64: 1
                                                     repeated_uint64: 0)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_uint64"));
  std::vector<Value> values;
  EXPECT_THAT(value.ForEach(value_manager(),
                            [&](const Value& element) -> absl::StatusOr<bool> {
                              values.push_back(element);
                              return true;
                            }),
              IsOk());
  EXPECT_THAT(values, ElementsAre(UintValueIs(1), UintValueIs(0)));
}

TEST_P(ParsedRepeatedFieldValueTest, ForEach_Int32) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_int32: 1
                                                     repeated_int32: 0)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int32"));
  std::vector<Value> values;
  EXPECT_THAT(value.ForEach(value_manager(),
                            [&](const Value& element) -> absl::StatusOr<bool> {
                              values.push_back(element);
                              return true;
                            }),
              IsOk());
  EXPECT_THAT(values, ElementsAre(IntValueIs(1), IntValueIs(0)));
}

TEST_P(ParsedRepeatedFieldValueTest, ForEach_UInt32) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_uint32: 1
                                                     repeated_uint32: 0)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_uint32"));
  std::vector<Value> values;
  EXPECT_THAT(value.ForEach(value_manager(),
                            [&](const Value& element) -> absl::StatusOr<bool> {
                              values.push_back(element);
                              return true;
                            }),
              IsOk());
  EXPECT_THAT(values, ElementsAre(UintValueIs(1), UintValueIs(0)));
}

TEST_P(ParsedRepeatedFieldValueTest, ForEach_Duration) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(
          R"pb(repeated_duration: { seconds: 1 nanos: 1 }
               repeated_duration: {})pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_duration"));
  std::vector<Value> values;
  EXPECT_THAT(value.ForEach(value_manager(),
                            [&](const Value& element) -> absl::StatusOr<bool> {
                              values.push_back(element);
                              return true;
                            }),
              IsOk());
  EXPECT_THAT(values, ElementsAre(DurationValueIs(absl::Seconds(1) +
                                                  absl::Nanoseconds(1)),
                                  DurationValueIs(absl::ZeroDuration())));
}

TEST_P(ParsedRepeatedFieldValueTest, ForEach_Bytes) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(
          R"pb(repeated_bytes: "bar" repeated_bytes: "foo")pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_bytes"));
  std::vector<Value> values;
  EXPECT_THAT(value.ForEach(value_manager(),
                            [&](const Value& element) -> absl::StatusOr<bool> {
                              values.push_back(element);
                              return true;
                            }),
              IsOk());
  EXPECT_THAT(values, ElementsAre(BytesValueIs("bar"), BytesValueIs("foo")));
}

TEST_P(ParsedRepeatedFieldValueTest, ForEach_Enum) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(
          R"pb(repeated_nested_enum: BAR repeated_nested_enum: FOO)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_nested_enum"));
  std::vector<Value> values;
  EXPECT_THAT(value.ForEach(value_manager(),
                            [&](const Value& element) -> absl::StatusOr<bool> {
                              values.push_back(element);
                              return true;
                            }),
              IsOk());
  EXPECT_THAT(values, ElementsAre(IntValueIs(1), IntValueIs(0)));
}

TEST_P(ParsedRepeatedFieldValueTest, ForEach_Null) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_null_value:
                                                         NULL_VALUE
                                                     repeated_null_value:
                                                         NULL_VALUE)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_null_value"));
  std::vector<Value> values;
  EXPECT_THAT(value.ForEach(value_manager(),
                            [&](const Value& element) -> absl::StatusOr<bool> {
                              values.push_back(element);
                              return true;
                            }),
              IsOk());
  EXPECT_THAT(values, ElementsAre(IsNullValue(), IsNullValue()));
}

TEST_P(ParsedRepeatedFieldValueTest, NewIterator) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_bool: false
                                                     repeated_bool: true)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_bool"));
  ASSERT_OK_AND_ASSIGN(auto iterator, value.NewIterator(value_manager()));
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(value_manager()),
              IsOkAndHolds(BoolValueIs(false)));
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(value_manager()), IsOkAndHolds(BoolValueIs(true)));
  ASSERT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(value_manager()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(ParsedRepeatedFieldValueTest, Contains) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_bool: true)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_bool"));
  EXPECT_THAT(value.Contains(value_manager(), BytesValue()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Contains(value_manager(), NullValue()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Contains(value_manager(), BoolValue(false)),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Contains(value_manager(), BoolValue(true)),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(value.Contains(value_manager(), DoubleValue(0.0)),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Contains(value_manager(), DoubleValue(1.0)),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Contains(value_manager(), StringValue("bar")),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Contains(value_manager(), StringValue("foo")),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Contains(value_manager(), MapValue()),
              IsOkAndHolds(BoolValueIs(false)));
}

INSTANTIATE_TEST_SUITE_P(ParsedRepeatedFieldValueTest,
                         ParsedRepeatedFieldValueTest,
                         ::testing::Values(AllocatorKind::kArena,
                                           AllocatorKind::kNewDelete),
                         PrintToStringParamName());

}  // namespace
}  // namespace cel
