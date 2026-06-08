// Copyright 2023 Google LLC
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

#include "common/value.h"

#include "google/protobuf/struct.pb.h"
#include "google/protobuf/type.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "absl/base/attributes.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/optional.h"
#include "common/type.h"
#include "common/value_testing.h"
#include "internal/parse_text_proto.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "cel/expr/conformance/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_enum_reflection.h"

namespace cel {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::internal::DynamicParseTextProto;
using ::cel::internal::GetTestingDescriptorPool;
using ::cel::internal::GetTestingMessageFactory;
using ::testing::An;
using ::testing::Eq;
using ::testing::NotNull;
using ::testing::Optional;

using TestAllTypesProto3 = ::cel::expr::conformance::proto3::TestAllTypes;

TEST(Value, GeneratedEnum) {
  EXPECT_EQ(Value::Enum(google::protobuf::NULL_VALUE), NullValue());
  EXPECT_EQ(Value::Enum(google::protobuf::SYNTAX_EDITIONS), IntValue(2));
}

TEST(Value, DynamicEnum) {
  EXPECT_THAT(
      Value::Enum(google::protobuf::GetEnumDescriptor<google::protobuf::NullValue>(), 0),
      test::IsNullValue());
  EXPECT_THAT(
      Value::Enum(google::protobuf::GetEnumDescriptor<google::protobuf::NullValue>()
                      ->FindValueByNumber(0)),
      test::IsNullValue());
  EXPECT_THAT(
      Value::Enum(google::protobuf::GetEnumDescriptor<google::protobuf::Syntax>(), 2),
      test::IntValueIs(2));
  EXPECT_THAT(Value::Enum(google::protobuf::GetEnumDescriptor<google::protobuf::Syntax>()
                              ->FindValueByNumber(2)),
              test::IntValueIs(2));
}

TEST(Value, DynamicClosedEnum) {
  google::protobuf::FileDescriptorProto file_descriptor;
  file_descriptor.set_name("test/closed_enum.proto");
  file_descriptor.set_package("test");
  file_descriptor.set_syntax("editions");
  file_descriptor.set_edition(google::protobuf::EDITION_2023);
  {
    auto* enum_descriptor = file_descriptor.add_enum_type();
    enum_descriptor->set_name("ClosedEnum");
    enum_descriptor->mutable_options()->mutable_features()->set_enum_type(
        google::protobuf::FeatureSet::CLOSED);
    auto* enum_value_descriptor = enum_descriptor->add_value();
    enum_value_descriptor->set_number(1);
    enum_value_descriptor->set_name("FOO");
    enum_value_descriptor = enum_descriptor->add_value();
    enum_value_descriptor->set_number(2);
    enum_value_descriptor->set_name("BAR");
  }
  google::protobuf::DescriptorPool pool;
  ASSERT_THAT(pool.BuildFile(file_descriptor), NotNull());
  const auto* enum_descriptor = pool.FindEnumTypeByName("test.ClosedEnum");
  ASSERT_THAT(enum_descriptor, NotNull());
  EXPECT_THAT(Value::Enum(enum_descriptor, 0),
              test::ErrorValueIs(StatusIs(absl::StatusCode::kInvalidArgument)));
}

TEST(Value, Is) {
  google::protobuf::Arena arena;

  EXPECT_TRUE(Value(BoolValue()).Is<BoolValue>());
  EXPECT_TRUE(Value(BoolValue(true)).IsTrue());
  EXPECT_TRUE(Value(BoolValue(false)).IsFalse());

  EXPECT_TRUE(Value(BytesValue()).Is<BytesValue>());

  EXPECT_TRUE(Value(DoubleValue()).Is<DoubleValue>());

  EXPECT_TRUE(Value(DurationValue()).Is<DurationValue>());

  EXPECT_TRUE(Value(ErrorValue()).Is<ErrorValue>());

  EXPECT_TRUE(Value(IntValue()).Is<IntValue>());

  EXPECT_TRUE(Value(ListValue()).Is<ListValue>());
  EXPECT_TRUE(Value(CustomListValue()).Is<ListValue>());
  EXPECT_TRUE(Value(CustomListValue()).Is<CustomListValue>());
  EXPECT_TRUE(Value(ParsedJsonListValue()).Is<ListValue>());
  EXPECT_TRUE(Value(ParsedJsonListValue()).Is<ParsedJsonListValue>());
  {
    auto message = DynamicParseTextProto<TestAllTypesProto3>(
        &arena, R"pb()pb", GetTestingDescriptorPool(),
        GetTestingMessageFactory());
    const auto* field = ABSL_DIE_IF_NULL(
        message->GetDescriptor()->FindFieldByName("repeated_int32"));
    EXPECT_TRUE(Value(ParsedRepeatedFieldValue(message, field, &arena))
                    .Is<ListValue>());
    EXPECT_TRUE(Value(ParsedRepeatedFieldValue(message, field, &arena))
                    .Is<ParsedRepeatedFieldValue>());
  }

  EXPECT_TRUE(Value(MapValue()).Is<MapValue>());
  EXPECT_TRUE(Value(CustomMapValue()).Is<MapValue>());
  EXPECT_TRUE(Value(CustomMapValue()).Is<CustomMapValue>());
  EXPECT_TRUE(Value(ParsedJsonMapValue()).Is<MapValue>());
  EXPECT_TRUE(Value(ParsedJsonMapValue()).Is<ParsedJsonMapValue>());
  {
    auto message = DynamicParseTextProto<TestAllTypesProto3>(
        &arena, R"pb()pb", GetTestingDescriptorPool(),
        GetTestingMessageFactory());
    const auto* field = ABSL_DIE_IF_NULL(
        message->GetDescriptor()->FindFieldByName("map_int32_int32"));
    EXPECT_TRUE(
        Value(ParsedMapFieldValue(message, field, &arena)).Is<MapValue>());
    EXPECT_TRUE(Value(ParsedMapFieldValue(message, field, &arena))
                    .Is<ParsedMapFieldValue>());
  }

  EXPECT_TRUE(Value(NullValue()).Is<NullValue>());

  EXPECT_TRUE(Value(OptionalValue()).Is<OpaqueValue>());
  EXPECT_TRUE(Value(OptionalValue()).Is<OptionalValue>());

  EXPECT_TRUE(Value(ParsedMessageValue()).Is<StructValue>());
  EXPECT_TRUE(Value(ParsedMessageValue()).Is<MessageValue>());
  EXPECT_TRUE(Value(ParsedMessageValue()).Is<ParsedMessageValue>());

  EXPECT_TRUE(Value(StringValue()).Is<StringValue>());

  EXPECT_TRUE(Value(TimestampValue()).Is<TimestampValue>());

  EXPECT_TRUE(Value(TypeValue(StringType())).Is<TypeValue>());

  EXPECT_TRUE(Value(UintValue()).Is<UintValue>());

  EXPECT_TRUE(Value(UnknownValue()).Is<UnknownValue>());
}

template <typename T>
constexpr T& AsLValueRef(T& t ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return t;
}

template <typename T>
constexpr const T& AsConstLValueRef(T& t ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return t;
}

template <typename T>
constexpr T&& AsRValueRef(T& t ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return static_cast<T&&>(t);
}

template <typename T>
constexpr const T&& AsConstRValueRef(T& t ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return static_cast<const T&&>(t);
}

TEST(Value, As) {
  google::protobuf::Arena arena;

  EXPECT_THAT(Value(BoolValue()).As<BoolValue>(), Optional(An<BoolValue>()));
  EXPECT_THAT(Value(BoolValue()).As<ErrorValue>(), Eq(absl::nullopt));

  {
    Value value(BytesValue{});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<BytesValue>(),
                Optional(An<BytesValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<BytesValue>(),
                Optional(An<BytesValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<BytesValue>(),
                Optional(An<BytesValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<BytesValue>(),
                Optional(An<BytesValue>()));
  }

  EXPECT_THAT(Value(DoubleValue()).As<DoubleValue>(),
              Optional(An<DoubleValue>()));
  EXPECT_THAT(Value(DoubleValue()).As<ErrorValue>(), Eq(absl::nullopt));

  EXPECT_THAT(Value(DurationValue()).As<DurationValue>(),
              Optional(An<DurationValue>()));
  EXPECT_THAT(Value(DurationValue()).As<ErrorValue>(), Eq(absl::nullopt));

  {
    Value value(ErrorValue{});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<ErrorValue>(),
                Optional(An<ErrorValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<ErrorValue>(),
                Optional(An<ErrorValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<ErrorValue>(),
                Optional(An<ErrorValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<ErrorValue>(),
                Optional(An<ErrorValue>()));
    EXPECT_THAT(Value(ErrorValue()).As<BoolValue>(), Eq(absl::nullopt));
  }

  EXPECT_THAT(Value(IntValue()).As<IntValue>(), Optional(An<IntValue>()));
  EXPECT_THAT(Value(IntValue()).As<ErrorValue>(), Eq(absl::nullopt));

  {
    Value value(ListValue{});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<ListValue>(),
                Optional(An<ListValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<ListValue>(),
                Optional(An<ListValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<ListValue>(),
                Optional(An<ListValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<ListValue>(),
                Optional(An<ListValue>()));
    EXPECT_THAT(Value(ListValue()).As<ErrorValue>(), Eq(absl::nullopt));
  }

  {
    Value value(ParsedJsonListValue{});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<ListValue>(),
                Optional(An<ListValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<ListValue>(),
                Optional(An<ListValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<ListValue>(),
                Optional(An<ListValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<ListValue>(),
                Optional(An<ListValue>()));
    EXPECT_THAT(Value(ListValue()).As<ErrorValue>(), Eq(absl::nullopt));
  }

  {
    Value value(ParsedJsonListValue{});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<ParsedJsonListValue>(),
                Optional(An<ParsedJsonListValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<ParsedJsonListValue>(),
                Optional(An<ParsedJsonListValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<ParsedJsonListValue>(),
                Optional(An<ParsedJsonListValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<ParsedJsonListValue>(),
                Optional(An<ParsedJsonListValue>()));
  }

  {
    Value value(CustomListValue{});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<ListValue>(),
                Optional(An<ListValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<ListValue>(),
                Optional(An<ListValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<ListValue>(),
                Optional(An<ListValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<ListValue>(),
                Optional(An<ListValue>()));
    EXPECT_THAT(Value(ListValue()).As<ErrorValue>(), Eq(absl::nullopt));
  }

  {
    Value value(CustomListValue{});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<CustomListValue>(),
                Optional(An<CustomListValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<CustomListValue>(),
                Optional(An<CustomListValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<CustomListValue>(),
                Optional(An<CustomListValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<CustomListValue>(),
                Optional(An<CustomListValue>()));
  }

  {
    auto message = DynamicParseTextProto<TestAllTypesProto3>(
        &arena, R"pb()pb", GetTestingDescriptorPool(),
        GetTestingMessageFactory());
    const auto* field = ABSL_DIE_IF_NULL(
        message->GetDescriptor()->FindFieldByName("repeated_int32"));
    Value value(ParsedRepeatedFieldValue{message, field, &arena});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<ListValue>(),
                Optional(An<ListValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<ListValue>(),
                Optional(An<ListValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<ListValue>(),
                Optional(An<ListValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<ListValue>(),
                Optional(An<ListValue>()));
  }

  {
    auto message = DynamicParseTextProto<TestAllTypesProto3>(
        &arena, R"pb()pb", GetTestingDescriptorPool(),
        GetTestingMessageFactory());
    const auto* field = ABSL_DIE_IF_NULL(
        message->GetDescriptor()->FindFieldByName("repeated_int32"));
    Value value(ParsedRepeatedFieldValue{message, field, &arena});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<ParsedRepeatedFieldValue>(),
                Optional(An<ParsedRepeatedFieldValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<ParsedRepeatedFieldValue>(),
                Optional(An<ParsedRepeatedFieldValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<ParsedRepeatedFieldValue>(),
                Optional(An<ParsedRepeatedFieldValue>()));
    EXPECT_THAT(
        AsConstRValueRef<Value>(other_value).As<ParsedRepeatedFieldValue>(),
        Optional(An<ParsedRepeatedFieldValue>()));
  }

  {
    Value value(MapValue{});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<MapValue>(),
                Optional(An<MapValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<MapValue>(),
                Optional(An<MapValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<MapValue>(),
                Optional(An<MapValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<MapValue>(),
                Optional(An<MapValue>()));
    EXPECT_THAT(Value(MapValue()).As<ErrorValue>(), Eq(absl::nullopt));
  }

  {
    Value value(ParsedJsonMapValue{});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<MapValue>(),
                Optional(An<MapValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<MapValue>(),
                Optional(An<MapValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<MapValue>(),
                Optional(An<MapValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<MapValue>(),
                Optional(An<MapValue>()));
    EXPECT_THAT(Value(MapValue()).As<ErrorValue>(), Eq(absl::nullopt));
  }

  {
    Value value(ParsedJsonMapValue{});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<ParsedJsonMapValue>(),
                Optional(An<ParsedJsonMapValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<ParsedJsonMapValue>(),
                Optional(An<ParsedJsonMapValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<ParsedJsonMapValue>(),
                Optional(An<ParsedJsonMapValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<ParsedJsonMapValue>(),
                Optional(An<ParsedJsonMapValue>()));
  }

  {
    Value value(CustomMapValue{});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<MapValue>(),
                Optional(An<MapValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<MapValue>(),
                Optional(An<MapValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<MapValue>(),
                Optional(An<MapValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<MapValue>(),
                Optional(An<MapValue>()));
    EXPECT_THAT(Value(MapValue()).As<ErrorValue>(), Eq(absl::nullopt));
  }

  {
    Value value(CustomMapValue{});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<CustomMapValue>(),
                Optional(An<CustomMapValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<CustomMapValue>(),
                Optional(An<CustomMapValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<CustomMapValue>(),
                Optional(An<CustomMapValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<CustomMapValue>(),
                Optional(An<CustomMapValue>()));
  }

  {
    auto message = DynamicParseTextProto<TestAllTypesProto3>(
        &arena, R"pb()pb", GetTestingDescriptorPool(),
        GetTestingMessageFactory());
    const auto* field = ABSL_DIE_IF_NULL(
        message->GetDescriptor()->FindFieldByName("map_int32_int32"));
    Value value(ParsedMapFieldValue{message, field, &arena});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<MapValue>(),
                Optional(An<MapValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<MapValue>(),
                Optional(An<MapValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<MapValue>(),
                Optional(An<MapValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<MapValue>(),
                Optional(An<MapValue>()));
  }

  {
    auto message = DynamicParseTextProto<TestAllTypesProto3>(
        &arena, R"pb()pb", GetTestingDescriptorPool(),
        GetTestingMessageFactory());
    const auto* field = ABSL_DIE_IF_NULL(
        message->GetDescriptor()->FindFieldByName("map_int32_int32"));
    Value value(ParsedMapFieldValue{message, field, &arena});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<ParsedMapFieldValue>(),
                Optional(An<ParsedMapFieldValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<ParsedMapFieldValue>(),
                Optional(An<ParsedMapFieldValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<ParsedMapFieldValue>(),
                Optional(An<ParsedMapFieldValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<ParsedMapFieldValue>(),
                Optional(An<ParsedMapFieldValue>()));
  }

  {
    Value value(ParsedMessageValue{
        DynamicParseTextProto<TestAllTypesProto3>(&arena, R"pb()pb",
                                                  GetTestingDescriptorPool(),
                                                  GetTestingMessageFactory()),
        &arena});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<MessageValue>(),
                Optional(An<MessageValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<MessageValue>(),
                Optional(An<MessageValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<MessageValue>(),
                Optional(An<MessageValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<MessageValue>(),
                Optional(An<MessageValue>()));
    EXPECT_THAT(Value(ParsedMessageValue{
                          DynamicParseTextProto<TestAllTypesProto3>(
                              &arena, R"pb()pb", GetTestingDescriptorPool(),
                              GetTestingMessageFactory()),
                          &arena})
                    .As<ErrorValue>(),
                Eq(absl::nullopt));
  }

  EXPECT_THAT(Value(NullValue()).As<NullValue>(), Optional(An<NullValue>()));
  EXPECT_THAT(Value(NullValue()).As<ErrorValue>(), Eq(absl::nullopt));

  {
    Value value(OptionalValue{});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<OpaqueValue>(),
                Optional(An<OpaqueValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<OpaqueValue>(),
                Optional(An<OpaqueValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<OpaqueValue>(),
                Optional(An<OpaqueValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<OpaqueValue>(),
                Optional(An<OpaqueValue>()));
    EXPECT_THAT(Value(OpaqueValue(OptionalValue())).As<ErrorValue>(),
                Eq(absl::nullopt));
  }

  {
    Value value(OptionalValue{});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<OptionalValue>(),
                Optional(An<OptionalValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<OptionalValue>(),
                Optional(An<OptionalValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<OptionalValue>(),
                Optional(An<OptionalValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<OptionalValue>(),
                Optional(An<OptionalValue>()));
    EXPECT_THAT(Value(OptionalValue()).As<ErrorValue>(), Eq(absl::nullopt));
  }

  {
    OpaqueValue value(OptionalValue{});
    OpaqueValue other_value = value;
    EXPECT_THAT(AsLValueRef<OpaqueValue>(value).As<OptionalValue>(),
                Optional(An<OptionalValue>()));
    EXPECT_THAT(AsConstLValueRef<OpaqueValue>(value).As<OptionalValue>(),
                Optional(An<OptionalValue>()));
    EXPECT_THAT(AsRValueRef<OpaqueValue>(value).As<OptionalValue>(),
                Optional(An<OptionalValue>()));
    EXPECT_THAT(AsConstRValueRef<OpaqueValue>(other_value).As<OptionalValue>(),
                Optional(An<OptionalValue>()));
  }

  {
    Value value(ParsedMessageValue{
        DynamicParseTextProto<TestAllTypesProto3>(&arena, R"pb()pb",
                                                  GetTestingDescriptorPool(),
                                                  GetTestingMessageFactory()),
        &arena});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<ParsedMessageValue>(),
                Optional(An<ParsedMessageValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<ParsedMessageValue>(),
                Optional(An<ParsedMessageValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<ParsedMessageValue>(),
                Optional(An<ParsedMessageValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<ParsedMessageValue>(),
                Optional(An<ParsedMessageValue>()));
  }

  {
    Value value(StringValue{});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<StringValue>(),
                Optional(An<StringValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<StringValue>(),
                Optional(An<StringValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<StringValue>(),
                Optional(An<StringValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<StringValue>(),
                Optional(An<StringValue>()));
    EXPECT_THAT(Value(StringValue()).As<ErrorValue>(), Eq(absl::nullopt));
  }

  {
    Value value(ParsedMessageValue{
        DynamicParseTextProto<TestAllTypesProto3>(&arena, R"pb()pb",
                                                  GetTestingDescriptorPool(),
                                                  GetTestingMessageFactory()),
        &arena});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<StructValue>(),
                Optional(An<StructValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<StructValue>(),
                Optional(An<StructValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<StructValue>(),
                Optional(An<StructValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<StructValue>(),
                Optional(An<StructValue>()));
  }

  EXPECT_THAT(Value(TimestampValue()).As<TimestampValue>(),
              Optional(An<TimestampValue>()));
  EXPECT_THAT(Value(TimestampValue()).As<ErrorValue>(), Eq(absl::nullopt));

  {
    Value value(TypeValue(StringType{}));
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<TypeValue>(),
                Optional(An<TypeValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<TypeValue>(),
                Optional(An<TypeValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<TypeValue>(),
                Optional(An<TypeValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<TypeValue>(),
                Optional(An<TypeValue>()));
    EXPECT_THAT(Value(TypeValue(StringType())).As<ErrorValue>(),
                Eq(absl::nullopt));
  }

  EXPECT_THAT(Value(UintValue()).As<UintValue>(), Optional(An<UintValue>()));
  EXPECT_THAT(Value(UintValue()).As<ErrorValue>(), Eq(absl::nullopt));

  {
    Value value(UnknownValue{});
    Value other_value = value;
    EXPECT_THAT(AsLValueRef<Value>(value).As<UnknownValue>(),
                Optional(An<UnknownValue>()));
    EXPECT_THAT(AsConstLValueRef<Value>(value).As<UnknownValue>(),
                Optional(An<UnknownValue>()));
    EXPECT_THAT(AsRValueRef<Value>(value).As<UnknownValue>(),
                Optional(An<UnknownValue>()));
    EXPECT_THAT(AsConstRValueRef<Value>(other_value).As<UnknownValue>(),
                Optional(An<UnknownValue>()));
    EXPECT_THAT(Value(UnknownValue()).As<ErrorValue>(), Eq(absl::nullopt));
  }
}

template <typename To, typename From>
decltype(auto) DoGet(From&& from) {
  return std::forward<From>(from).template Get<To>();
}

TEST(Value, Get) {
  google::protobuf::Arena arena;

  EXPECT_THAT(DoGet<BoolValue>(Value(BoolValue())), An<BoolValue>());

  {
    Value value(BytesValue{});
    Value other_value = value;
    EXPECT_THAT(DoGet<BytesValue>(AsLValueRef<Value>(value)), An<BytesValue>());
    EXPECT_THAT(DoGet<BytesValue>(AsConstLValueRef<Value>(value)),
                An<BytesValue>());
    EXPECT_THAT(DoGet<BytesValue>(AsRValueRef<Value>(value)), An<BytesValue>());
    EXPECT_THAT(DoGet<BytesValue>(AsConstRValueRef<Value>(other_value)),
                An<BytesValue>());
  }

  EXPECT_THAT(DoGet<DoubleValue>(Value(DoubleValue())), An<DoubleValue>());

  EXPECT_THAT(DoGet<DurationValue>(Value(DurationValue())),
              An<DurationValue>());

  {
    Value value(ErrorValue{});
    Value other_value = value;
    EXPECT_THAT(DoGet<ErrorValue>(AsLValueRef<Value>(value)), An<ErrorValue>());
    EXPECT_THAT(DoGet<ErrorValue>(AsConstLValueRef<Value>(value)),
                An<ErrorValue>());
    EXPECT_THAT(DoGet<ErrorValue>(AsRValueRef<Value>(value)), An<ErrorValue>());
    EXPECT_THAT(DoGet<ErrorValue>(AsConstRValueRef<Value>(other_value)),
                An<ErrorValue>());
  }

  EXPECT_THAT(DoGet<IntValue>(Value(IntValue())), An<IntValue>());

  {
    Value value(ListValue{});
    Value other_value = value;
    EXPECT_THAT(DoGet<ListValue>(AsLValueRef<Value>(value)), An<ListValue>());
    EXPECT_THAT(DoGet<ListValue>(AsConstLValueRef<Value>(value)),
                An<ListValue>());
    EXPECT_THAT(DoGet<ListValue>(AsRValueRef<Value>(value)), An<ListValue>());
    EXPECT_THAT(DoGet<ListValue>(AsConstRValueRef<Value>(other_value)),
                An<ListValue>());
  }

  {
    Value value(ParsedJsonListValue{});
    Value other_value = value;
    EXPECT_THAT(DoGet<ListValue>(AsLValueRef<Value>(value)), An<ListValue>());
    EXPECT_THAT(DoGet<ListValue>(AsConstLValueRef<Value>(value)),
                An<ListValue>());
    EXPECT_THAT(DoGet<ListValue>(AsRValueRef<Value>(value)), An<ListValue>());
    EXPECT_THAT(DoGet<ListValue>(AsConstRValueRef<Value>(other_value)),
                An<ListValue>());
  }

  {
    Value value(ParsedJsonListValue{});
    Value other_value = value;
    EXPECT_THAT(DoGet<ParsedJsonListValue>(AsLValueRef<Value>(value)),
                An<ParsedJsonListValue>());
    EXPECT_THAT(DoGet<ParsedJsonListValue>(AsConstLValueRef<Value>(value)),
                An<ParsedJsonListValue>());
    EXPECT_THAT(DoGet<ParsedJsonListValue>(AsRValueRef<Value>(value)),
                An<ParsedJsonListValue>());
    EXPECT_THAT(
        DoGet<ParsedJsonListValue>(AsConstRValueRef<Value>(other_value)),
        An<ParsedJsonListValue>());
  }

  {
    Value value(CustomListValue{});
    Value other_value = value;
    EXPECT_THAT(DoGet<ListValue>(AsLValueRef<Value>(value)), An<ListValue>());
    EXPECT_THAT(DoGet<ListValue>(AsConstLValueRef<Value>(value)),
                An<ListValue>());
    EXPECT_THAT(DoGet<ListValue>(AsRValueRef<Value>(value)), An<ListValue>());
    EXPECT_THAT(DoGet<ListValue>(AsConstRValueRef<Value>(other_value)),
                An<ListValue>());
  }

  {
    Value value(CustomListValue{});
    Value other_value = value;
    EXPECT_THAT(DoGet<CustomListValue>(AsLValueRef<Value>(value)),
                An<CustomListValue>());
    EXPECT_THAT(DoGet<CustomListValue>(AsConstLValueRef<Value>(value)),
                An<CustomListValue>());
    EXPECT_THAT(DoGet<CustomListValue>(AsRValueRef<Value>(value)),
                An<CustomListValue>());
    EXPECT_THAT(DoGet<CustomListValue>(AsConstRValueRef<Value>(other_value)),
                An<CustomListValue>());
  }

  {
    auto message = DynamicParseTextProto<TestAllTypesProto3>(
        &arena, R"pb()pb", GetTestingDescriptorPool(),
        GetTestingMessageFactory());
    const auto* field = ABSL_DIE_IF_NULL(
        message->GetDescriptor()->FindFieldByName("repeated_int32"));
    Value value(ParsedRepeatedFieldValue{message, field, &arena});
    Value other_value = value;
    EXPECT_THAT(DoGet<ListValue>(AsLValueRef<Value>(value)), An<ListValue>());
    EXPECT_THAT(DoGet<ListValue>(AsConstLValueRef<Value>(value)),
                An<ListValue>());
    EXPECT_THAT(DoGet<ListValue>(AsRValueRef<Value>(value)), An<ListValue>());
    EXPECT_THAT(DoGet<ListValue>(AsConstRValueRef<Value>(other_value)),
                An<ListValue>());
  }

  {
    auto message = DynamicParseTextProto<TestAllTypesProto3>(
        &arena, R"pb()pb", GetTestingDescriptorPool(),
        GetTestingMessageFactory());
    const auto* field = ABSL_DIE_IF_NULL(
        message->GetDescriptor()->FindFieldByName("repeated_int32"));
    Value value(ParsedRepeatedFieldValue{message, field, &arena});
    Value other_value = value;
    EXPECT_THAT(DoGet<ParsedRepeatedFieldValue>(AsLValueRef<Value>(value)),
                An<ParsedRepeatedFieldValue>());
    EXPECT_THAT(DoGet<ParsedRepeatedFieldValue>(AsConstLValueRef<Value>(value)),
                An<ParsedRepeatedFieldValue>());
    EXPECT_THAT(DoGet<ParsedRepeatedFieldValue>(AsRValueRef<Value>(value)),
                An<ParsedRepeatedFieldValue>());
    EXPECT_THAT(
        DoGet<ParsedRepeatedFieldValue>(AsConstRValueRef<Value>(other_value)),
        An<ParsedRepeatedFieldValue>());
  }

  {
    Value value(MapValue{});
    Value other_value = value;
    EXPECT_THAT(DoGet<MapValue>(AsLValueRef<Value>(value)), An<MapValue>());
    EXPECT_THAT(DoGet<MapValue>(AsConstLValueRef<Value>(value)),
                An<MapValue>());
    EXPECT_THAT(DoGet<MapValue>(AsRValueRef<Value>(value)), An<MapValue>());
    EXPECT_THAT(DoGet<MapValue>(AsConstRValueRef<Value>(other_value)),
                An<MapValue>());
  }

  {
    Value value(ParsedJsonMapValue{});
    Value other_value = value;
    EXPECT_THAT(DoGet<MapValue>(AsLValueRef<Value>(value)), An<MapValue>());
    EXPECT_THAT(DoGet<MapValue>(AsConstLValueRef<Value>(value)),
                An<MapValue>());
    EXPECT_THAT(DoGet<MapValue>(AsRValueRef<Value>(value)), An<MapValue>());
    EXPECT_THAT(DoGet<MapValue>(AsConstRValueRef<Value>(other_value)),
                An<MapValue>());
  }

  {
    Value value(ParsedJsonMapValue{});
    Value other_value = value;
    EXPECT_THAT(DoGet<ParsedJsonMapValue>(AsLValueRef<Value>(value)),
                An<ParsedJsonMapValue>());
    EXPECT_THAT(DoGet<ParsedJsonMapValue>(AsConstLValueRef<Value>(value)),
                An<ParsedJsonMapValue>());
    EXPECT_THAT(DoGet<ParsedJsonMapValue>(AsRValueRef<Value>(value)),
                An<ParsedJsonMapValue>());
    EXPECT_THAT(DoGet<ParsedJsonMapValue>(AsConstRValueRef<Value>(other_value)),
                An<ParsedJsonMapValue>());
  }

  {
    Value value(CustomMapValue{});
    Value other_value = value;
    EXPECT_THAT(DoGet<MapValue>(AsLValueRef<Value>(value)), An<MapValue>());
    EXPECT_THAT(DoGet<MapValue>(AsConstLValueRef<Value>(value)),
                An<MapValue>());
    EXPECT_THAT(DoGet<MapValue>(AsRValueRef<Value>(value)), An<MapValue>());
    EXPECT_THAT(DoGet<MapValue>(AsConstRValueRef<Value>(other_value)),
                An<MapValue>());
  }

  {
    Value value(CustomMapValue{});
    Value other_value = value;
    EXPECT_THAT(DoGet<CustomMapValue>(AsLValueRef<Value>(value)),
                An<CustomMapValue>());
    EXPECT_THAT(DoGet<CustomMapValue>(AsConstLValueRef<Value>(value)),
                An<CustomMapValue>());
    EXPECT_THAT(DoGet<CustomMapValue>(AsRValueRef<Value>(value)),
                An<CustomMapValue>());
    EXPECT_THAT(DoGet<CustomMapValue>(AsConstRValueRef<Value>(other_value)),
                An<CustomMapValue>());
  }

  {
    auto message = DynamicParseTextProto<TestAllTypesProto3>(
        &arena, R"pb()pb", GetTestingDescriptorPool(),
        GetTestingMessageFactory());
    const auto* field = ABSL_DIE_IF_NULL(
        message->GetDescriptor()->FindFieldByName("map_int32_int32"));
    Value value(ParsedMapFieldValue{message, field, &arena});
    Value other_value = value;
    EXPECT_THAT(DoGet<MapValue>(AsLValueRef<Value>(value)), An<MapValue>());
    EXPECT_THAT(DoGet<MapValue>(AsConstLValueRef<Value>(value)),
                An<MapValue>());
    EXPECT_THAT(DoGet<MapValue>(AsRValueRef<Value>(value)), An<MapValue>());
    EXPECT_THAT(DoGet<MapValue>(AsConstRValueRef<Value>(other_value)),
                An<MapValue>());
  }

  {
    auto message = DynamicParseTextProto<TestAllTypesProto3>(
        &arena, R"pb()pb", GetTestingDescriptorPool(),
        GetTestingMessageFactory());
    const auto* field = ABSL_DIE_IF_NULL(
        message->GetDescriptor()->FindFieldByName("map_int32_int32"));
    Value value(ParsedMapFieldValue{message, field, &arena});
    Value other_value = value;
    EXPECT_THAT(DoGet<ParsedMapFieldValue>(AsLValueRef<Value>(value)),
                An<ParsedMapFieldValue>());
    EXPECT_THAT(DoGet<ParsedMapFieldValue>(AsConstLValueRef<Value>(value)),
                An<ParsedMapFieldValue>());
    EXPECT_THAT(DoGet<ParsedMapFieldValue>(AsRValueRef<Value>(value)),
                An<ParsedMapFieldValue>());
    EXPECT_THAT(
        DoGet<ParsedMapFieldValue>(AsConstRValueRef<Value>(other_value)),
        An<ParsedMapFieldValue>());
  }

  {
    Value value(ParsedMessageValue{
        DynamicParseTextProto<TestAllTypesProto3>(&arena, R"pb()pb",
                                                  GetTestingDescriptorPool(),
                                                  GetTestingMessageFactory()),
        &arena});
    Value other_value = value;
    EXPECT_THAT(DoGet<MessageValue>(AsLValueRef<Value>(value)),
                An<MessageValue>());
    EXPECT_THAT(DoGet<MessageValue>(AsConstLValueRef<Value>(value)),
                An<MessageValue>());
    EXPECT_THAT(DoGet<MessageValue>(AsRValueRef<Value>(value)),
                An<MessageValue>());
    EXPECT_THAT(DoGet<MessageValue>(AsConstRValueRef<Value>(other_value)),
                An<MessageValue>());
  }

  EXPECT_THAT(DoGet<NullValue>(Value(NullValue())), An<NullValue>());

  {
    Value value(OptionalValue{});
    Value other_value = value;
    EXPECT_THAT(DoGet<OpaqueValue>(AsLValueRef<Value>(value)),
                An<OpaqueValue>());
    EXPECT_THAT(DoGet<OpaqueValue>(AsConstLValueRef<Value>(value)),
                An<OpaqueValue>());
    EXPECT_THAT(DoGet<OpaqueValue>(AsRValueRef<Value>(value)),
                An<OpaqueValue>());
    EXPECT_THAT(DoGet<OpaqueValue>(AsConstRValueRef<Value>(other_value)),
                An<OpaqueValue>());
  }

  {
    Value value(OptionalValue{});
    Value other_value = value;
    EXPECT_THAT(DoGet<OptionalValue>(AsLValueRef<Value>(value)),
                An<OptionalValue>());
    EXPECT_THAT(DoGet<OptionalValue>(AsConstLValueRef<Value>(value)),
                An<OptionalValue>());
    EXPECT_THAT(DoGet<OptionalValue>(AsRValueRef<Value>(value)),
                An<OptionalValue>());
    EXPECT_THAT(DoGet<OptionalValue>(AsConstRValueRef<Value>(other_value)),
                An<OptionalValue>());
  }

  {
    OpaqueValue value(OptionalValue{});
    OpaqueValue other_value = value;
    EXPECT_THAT(DoGet<OptionalValue>(AsLValueRef<OpaqueValue>(value)),
                An<OptionalValue>());
    EXPECT_THAT(DoGet<OptionalValue>(AsConstLValueRef<OpaqueValue>(value)),
                An<OptionalValue>());
    EXPECT_THAT(DoGet<OptionalValue>(AsRValueRef<OpaqueValue>(value)),
                An<OptionalValue>());
    EXPECT_THAT(
        DoGet<OptionalValue>(AsConstRValueRef<OpaqueValue>(other_value)),
        An<OptionalValue>());
  }

  {
    Value value(ParsedMessageValue{
        DynamicParseTextProto<TestAllTypesProto3>(&arena, R"pb()pb",
                                                  GetTestingDescriptorPool(),
                                                  GetTestingMessageFactory()),
        &arena});
    Value other_value = value;
    EXPECT_THAT(DoGet<ParsedMessageValue>(AsLValueRef<Value>(value)),
                An<ParsedMessageValue>());
    EXPECT_THAT(DoGet<ParsedMessageValue>(AsConstLValueRef<Value>(value)),
                An<ParsedMessageValue>());
    EXPECT_THAT(DoGet<ParsedMessageValue>(AsRValueRef<Value>(value)),
                An<ParsedMessageValue>());
    EXPECT_THAT(DoGet<ParsedMessageValue>(AsConstRValueRef<Value>(other_value)),
                An<ParsedMessageValue>());
  }

  {
    Value value(StringValue{});
    Value other_value = value;
    EXPECT_THAT(DoGet<StringValue>(AsLValueRef<Value>(value)),
                An<StringValue>());
    EXPECT_THAT(DoGet<StringValue>(AsConstLValueRef<Value>(value)),
                An<StringValue>());
    EXPECT_THAT(DoGet<StringValue>(AsRValueRef<Value>(value)),
                An<StringValue>());
    EXPECT_THAT(DoGet<StringValue>(AsConstRValueRef<Value>(other_value)),
                An<StringValue>());
  }

  {
    Value value(ParsedMessageValue{
        DynamicParseTextProto<TestAllTypesProto3>(&arena, R"pb()pb",
                                                  GetTestingDescriptorPool(),
                                                  GetTestingMessageFactory()),
        &arena});
    Value other_value = value;
    EXPECT_THAT(DoGet<StructValue>(AsLValueRef<Value>(value)),
                An<StructValue>());
    EXPECT_THAT(DoGet<StructValue>(AsConstLValueRef<Value>(value)),
                An<StructValue>());
    EXPECT_THAT(DoGet<StructValue>(AsRValueRef<Value>(value)),
                An<StructValue>());
    EXPECT_THAT(DoGet<StructValue>(AsConstRValueRef<Value>(other_value)),
                An<StructValue>());
  }

  EXPECT_THAT(DoGet<TimestampValue>(Value(TimestampValue())),
              An<TimestampValue>());

  {
    Value value(TypeValue(StringType{}));
    Value other_value = value;
    EXPECT_THAT(DoGet<TypeValue>(AsLValueRef<Value>(value)), An<TypeValue>());
    EXPECT_THAT(DoGet<TypeValue>(AsConstLValueRef<Value>(value)),
                An<TypeValue>());
    EXPECT_THAT(DoGet<TypeValue>(AsRValueRef<Value>(value)), An<TypeValue>());
    EXPECT_THAT(DoGet<TypeValue>(AsConstRValueRef<Value>(other_value)),
                An<TypeValue>());
  }

  EXPECT_THAT(DoGet<UintValue>(Value(UintValue())), An<UintValue>());

  {
    Value value(UnknownValue{});
    Value other_value = value;
    EXPECT_THAT(DoGet<UnknownValue>(AsLValueRef<Value>(value)),
                An<UnknownValue>());
    EXPECT_THAT(DoGet<UnknownValue>(AsConstLValueRef<Value>(value)),
                An<UnknownValue>());
    EXPECT_THAT(DoGet<UnknownValue>(AsRValueRef<Value>(value)),
                An<UnknownValue>());
    EXPECT_THAT(DoGet<UnknownValue>(AsConstRValueRef<Value>(other_value)),
                An<UnknownValue>());
  }
}

TEST(Value, NumericHeterogeneousEquality) {
  EXPECT_EQ(IntValue(1), UintValue(1));
  EXPECT_EQ(UintValue(1), IntValue(1));
  EXPECT_EQ(IntValue(1), DoubleValue(1));
  EXPECT_EQ(DoubleValue(1), IntValue(1));
  EXPECT_EQ(UintValue(1), DoubleValue(1));
  EXPECT_EQ(DoubleValue(1), UintValue(1));

  EXPECT_NE(IntValue(1), UintValue(2));
  EXPECT_NE(UintValue(1), IntValue(2));
  EXPECT_NE(IntValue(1), DoubleValue(2));
  EXPECT_NE(DoubleValue(1), IntValue(2));
  EXPECT_NE(UintValue(1), DoubleValue(2));
  EXPECT_NE(DoubleValue(1), UintValue(2));
}

using ValueIteratorTest = common_internal::ValueTest<>;

TEST_F(ValueIteratorTest, Empty) {
  auto iterator = NewEmptyValueIterator();
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(ValueIteratorTest, Empty1) {
  auto iterator = NewEmptyValueIterator();
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(ValueIteratorTest, Empty2) {
  auto iterator = NewEmptyValueIterator();
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

}  // namespace
}  // namespace cel
