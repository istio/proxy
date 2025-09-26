// Copyright 2021 Google LLC
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

#include "eval/public/structs/field_access_impl.h"

#include <array>
#include <limits>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/testing/matchers.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/testing.h"
#include "internal/time.h"
#include "testutil/util.h"
#include "cel/expr/conformance/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"

namespace google::api::expr::runtime::internal {

namespace {

using ::absl_testing::StatusIs;
using ::cel::expr::conformance::proto3::TestAllTypes;
using ::cel::internal::MaxDuration;
using ::cel::internal::MaxTimestamp;
using ::google::protobuf::Arena;
using ::google::protobuf::FieldDescriptor;
using ::testing::HasSubstr;
using testutil::EqualsProto;

TEST(FieldAccessTest, SetDuration) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_duration");
  auto status = SetValueToSingleField(CelValue::CreateDuration(MaxDuration()),
                                      field, &msg, &arena);
  EXPECT_TRUE(status.ok());
}

TEST(FieldAccessTest, SetDurationBadDuration) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_duration");
  auto status = SetValueToSingleField(
      CelValue::CreateDuration(MaxDuration() + absl::Seconds(1)), field, &msg,
      &arena);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(FieldAccessTest, SetDurationBadInputType) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_duration");
  auto status =
      SetValueToSingleField(CelValue::CreateInt64(1), field, &msg, &arena);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(FieldAccessTest, SetTimestamp) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_timestamp");
  auto status = SetValueToSingleField(CelValue::CreateTimestamp(MaxTimestamp()),
                                      field, &msg, &arena);
  EXPECT_TRUE(status.ok());
}

TEST(FieldAccessTest, SetTimestampBadTime) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_timestamp");
  auto status = SetValueToSingleField(
      CelValue::CreateTimestamp(MaxTimestamp() + absl::Seconds(1)), field, &msg,
      &arena);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(FieldAccessTest, SetTimestampBadInputType) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_timestamp");
  auto status =
      SetValueToSingleField(CelValue::CreateInt64(1), field, &msg, &arena);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(FieldAccessTest, SetInt32Overflow) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_int32");
  EXPECT_THAT(
      SetValueToSingleField(
          CelValue::CreateInt64(std::numeric_limits<int32_t>::max() + 1L),
          field, &msg, &arena),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Could not assign")));
}

TEST(FieldAccessTest, SetUint32Overflow) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_uint32");
  EXPECT_THAT(
      SetValueToSingleField(
          CelValue::CreateUint64(std::numeric_limits<uint32_t>::max() + 1L),
          field, &msg, &arena),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Could not assign")));
}

TEST(FieldAccessTest, SetMessage) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("standalone_message");
  TestAllTypes::NestedMessage* nested_msg =
      google::protobuf::Arena::Create<TestAllTypes::NestedMessage>(&arena);
  nested_msg->set_bb(1);
  auto status = SetValueToSingleField(
      CelProtoWrapper::CreateMessage(nested_msg, &arena), field, &msg, &arena);
  EXPECT_TRUE(status.ok());
}

TEST(FieldAccessTest, SetMessageWithNull) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("standalone_message");
  auto status =
      SetValueToSingleField(CelValue::CreateNull(), field, &msg, &arena);
  EXPECT_TRUE(status.ok());
}

struct AccessFieldTestParam {
  absl::string_view field_name;
  absl::string_view message_textproto;
  CelValue cel_value;
};

std::string GetTestName(
    const testing::TestParamInfo<AccessFieldTestParam>& info) {
  return std::string(info.param.field_name);
}

class SingleFieldTest : public testing::TestWithParam<AccessFieldTestParam> {
 public:
  absl::string_view field_name() const { return GetParam().field_name; }
  absl::string_view message_textproto() const {
    return GetParam().message_textproto;
  }
  CelValue cel_value() const { return GetParam().cel_value; }
};

TEST_P(SingleFieldTest, Getter) {
  TestAllTypes test_message;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(message_textproto(), &test_message));
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue accessed_value,
      CreateValueFromSingleField(
          &test_message,
          test_message.GetDescriptor()->FindFieldByName(field_name()),
          ProtoWrapperTypeOptions::kUnsetProtoDefault,
          &CelProtoWrapper::InternalWrapMessage, &arena));

  EXPECT_THAT(accessed_value, test::EqualsCelValue(cel_value()));
}

TEST_P(SingleFieldTest, Setter) {
  TestAllTypes test_message;
  CelValue to_set = cel_value();
  google::protobuf::Arena arena;

  ASSERT_OK(SetValueToSingleField(
      to_set, test_message.GetDescriptor()->FindFieldByName(field_name()),
      &test_message, &arena));

  EXPECT_THAT(test_message, EqualsProto(message_textproto()));
}

INSTANTIATE_TEST_SUITE_P(
    AllTypes, SingleFieldTest,
    testing::ValuesIn<AccessFieldTestParam>({
        {"single_int32", "single_int32: 1", CelValue::CreateInt64(1)},
        {"single_int64", "single_int64: 1", CelValue::CreateInt64(1)},
        {"single_uint32", "single_uint32: 1", CelValue::CreateUint64(1)},
        {"single_uint64", "single_uint64: 1", CelValue::CreateUint64(1)},
        {"single_sint32", "single_sint32: 1", CelValue::CreateInt64(1)},
        {"single_sint64", "single_sint64: 1", CelValue::CreateInt64(1)},
        {"single_fixed32", "single_fixed32: 1", CelValue::CreateUint64(1)},
        {"single_fixed64", "single_fixed64: 1", CelValue::CreateUint64(1)},
        {"single_sfixed32", "single_sfixed32: 1", CelValue::CreateInt64(1)},
        {"single_sfixed64", "single_sfixed64: 1", CelValue::CreateInt64(1)},
        {"single_float", "single_float: 1.0", CelValue::CreateDouble(1.0)},
        {"single_double", "single_double: 1.0", CelValue::CreateDouble(1.0)},
        {"single_bool", "single_bool: true", CelValue::CreateBool(true)},
        {"single_string", "single_string: 'abcd'",
         CelValue::CreateStringView("abcd")},
        {"single_bytes", "single_bytes: 'asdf'",
         CelValue::CreateBytesView("asdf")},
        {"standalone_enum", "standalone_enum: BAZ", CelValue::CreateInt64(2)},
        // Basic coverage for unwrapping -- specifics are managed by the
        // wrapping library.
        {"single_int64_wrapper", "single_int64_wrapper { value: 20 }",
         CelValue::CreateInt64(20)},
        {"single_value", "single_value { null_value: NULL_VALUE }",
         CelValue::CreateNull()},
    }),
    &GetTestName);

TEST(CreateValueFromSingleFieldTest, GetMessage) {
  TestAllTypes test_message;
  google::protobuf::Arena arena;

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      "standalone_message { bb: 10 }", &test_message));

  ASSERT_OK_AND_ASSIGN(
      CelValue accessed_value,
      CreateValueFromSingleField(
          &test_message,
          test_message.GetDescriptor()->FindFieldByName("standalone_message"),
          ProtoWrapperTypeOptions::kUnsetProtoDefault,
          &CelProtoWrapper::InternalWrapMessage, &arena));

  EXPECT_THAT(accessed_value, test::IsCelMessage(EqualsProto("bb: 10")));
}

TEST(SetValueToSingleFieldTest, WrongType) {
  TestAllTypes test_message;
  google::protobuf::Arena arena;

  EXPECT_THAT(SetValueToSingleField(
                  CelValue::CreateDouble(1.0),
                  test_message.GetDescriptor()->FindFieldByName("single_int32"),
                  &test_message, &arena),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(SetValueToSingleFieldTest, IntOutOfRange) {
  CelValue out_of_range = CelValue::CreateInt64(1LL << 31);
  TestAllTypes test_message;
  const google::protobuf::Descriptor* descriptor = test_message.GetDescriptor();
  google::protobuf::Arena arena;

  EXPECT_THAT(SetValueToSingleField(out_of_range,
                                    descriptor->FindFieldByName("single_int32"),
                                    &test_message, &arena),
              StatusIs(absl::StatusCode::kInvalidArgument));

  // proto enums are are represented as int32, but CEL converts to/from int64.
  EXPECT_THAT(SetValueToSingleField(
                  out_of_range, descriptor->FindFieldByName("standalone_enum"),
                  &test_message, &arena),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(SetValueToSingleFieldTest, UintOutOfRange) {
  CelValue out_of_range = CelValue::CreateUint64(1LL << 32);
  TestAllTypes test_message;
  const google::protobuf::Descriptor* descriptor = test_message.GetDescriptor();
  google::protobuf::Arena arena;

  EXPECT_THAT(SetValueToSingleField(
                  out_of_range, descriptor->FindFieldByName("single_uint32"),
                  &test_message, &arena),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(SetValueToSingleFieldTest, SetMessage) {
  TestAllTypes::NestedMessage nested_message;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"(
      bb: 42
  )",
                                                  &nested_message));
  google::protobuf::Arena arena;
  CelValue nested_value =
      CelProtoWrapper::CreateMessage(&nested_message, &arena);
  TestAllTypes test_message;
  const google::protobuf::Descriptor* descriptor = test_message.GetDescriptor();

  ASSERT_OK(SetValueToSingleField(
      nested_value, descriptor->FindFieldByName("standalone_message"),
      &test_message, &arena));
  EXPECT_THAT(test_message, EqualsProto("standalone_message { bb: 42 }"));
}

TEST(SetValueToSingleFieldTest, SetAnyMessage) {
  TestAllTypes::NestedMessage nested_message;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"(
      bb: 42
  )",
                                                  &nested_message));
  google::protobuf::Arena arena;
  CelValue nested_value =
      CelProtoWrapper::CreateMessage(&nested_message, &arena);
  TestAllTypes test_message;
  const google::protobuf::Descriptor* descriptor = test_message.GetDescriptor();

  ASSERT_OK(SetValueToSingleField(nested_value,
                                  descriptor->FindFieldByName("single_any"),
                                  &test_message, &arena));

  TestAllTypes::NestedMessage unpacked;
  test_message.single_any().UnpackTo(&unpacked);
  EXPECT_THAT(unpacked, EqualsProto("bb: 42"));
}

TEST(SetValueToSingleFieldTest, SetMessageToNullNoop) {
  google::protobuf::Arena arena;
  TestAllTypes test_message;
  const google::protobuf::Descriptor* descriptor = test_message.GetDescriptor();

  ASSERT_OK(SetValueToSingleField(
      CelValue::CreateNull(), descriptor->FindFieldByName("standalone_message"),
      &test_message, &arena));
  EXPECT_THAT(test_message, EqualsProto(test_message.default_instance()));
}

class RepeatedFieldTest : public testing::TestWithParam<AccessFieldTestParam> {
 public:
  absl::string_view field_name() const { return GetParam().field_name; }
  absl::string_view message_textproto() const {
    return GetParam().message_textproto;
  }
  CelValue cel_value() const { return GetParam().cel_value; }
};

TEST_P(RepeatedFieldTest, GetFirstElem) {
  TestAllTypes test_message;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(message_textproto(), &test_message));
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue accessed_value,
      CreateValueFromRepeatedField(
          &test_message,
          test_message.GetDescriptor()->FindFieldByName(field_name()), 0,
          &CelProtoWrapper::InternalWrapMessage, &arena));

  EXPECT_THAT(accessed_value, test::EqualsCelValue(cel_value()));
}

TEST_P(RepeatedFieldTest, AppendElem) {
  TestAllTypes test_message;
  CelValue to_add = cel_value();
  google::protobuf::Arena arena;

  ASSERT_OK(AddValueToRepeatedField(
      to_add, test_message.GetDescriptor()->FindFieldByName(field_name()),
      &test_message, &arena));

  EXPECT_THAT(test_message, EqualsProto(message_textproto()));
}

INSTANTIATE_TEST_SUITE_P(
    AllTypes, RepeatedFieldTest,
    testing::ValuesIn<AccessFieldTestParam>(
        {{"repeated_int32", "repeated_int32: 1", CelValue::CreateInt64(1)},
         {"repeated_int64", "repeated_int64: 1", CelValue::CreateInt64(1)},
         {"repeated_uint32", "repeated_uint32: 1", CelValue::CreateUint64(1)},
         {"repeated_uint64", "repeated_uint64: 1", CelValue::CreateUint64(1)},
         {"repeated_sint32", "repeated_sint32: 1", CelValue::CreateInt64(1)},
         {"repeated_sint64", "repeated_sint64: 1", CelValue::CreateInt64(1)},
         {"repeated_fixed32", "repeated_fixed32: 1", CelValue::CreateUint64(1)},
         {"repeated_fixed64", "repeated_fixed64: 1", CelValue::CreateUint64(1)},
         {"repeated_sfixed32", "repeated_sfixed32: 1",
          CelValue::CreateInt64(1)},
         {"repeated_sfixed64", "repeated_sfixed64: 1",
          CelValue::CreateInt64(1)},
         {"repeated_float", "repeated_float: 1.0", CelValue::CreateDouble(1.0)},
         {"repeated_double", "repeated_double: 1.0",
          CelValue::CreateDouble(1.0)},
         {"repeated_bool", "repeated_bool: true", CelValue::CreateBool(true)},
         {"repeated_string", "repeated_string: 'abcd'",
          CelValue::CreateStringView("abcd")},
         {"repeated_bytes", "repeated_bytes: 'asdf'",
          CelValue::CreateBytesView("asdf")},
         {"repeated_nested_enum", "repeated_nested_enum: BAZ",
          CelValue::CreateInt64(2)}}),
    &GetTestName);

TEST(RepeatedFieldTest, GetMessage) {
  TestAllTypes test_message;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      "repeated_nested_message { bb: 30 }", &test_message));
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue accessed_value,
                       CreateValueFromRepeatedField(
                           &test_message,
                           test_message.GetDescriptor()->FindFieldByName(
                               "repeated_nested_message"),
                           0, &CelProtoWrapper::InternalWrapMessage, &arena));

  EXPECT_THAT(accessed_value, test::IsCelMessage(EqualsProto("bb: 30")));
}

TEST(AddValueToRepeatedFieldTest, WrongType) {
  TestAllTypes test_message;
  google::protobuf::Arena arena;

  EXPECT_THAT(
      AddValueToRepeatedField(
          CelValue::CreateDouble(1.0),
          test_message.GetDescriptor()->FindFieldByName("repeated_int32"),
          &test_message, &arena),
      StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(AddValueToRepeatedFieldTest, IntOutOfRange) {
  CelValue out_of_range = CelValue::CreateInt64(1LL << 31);
  TestAllTypes test_message;
  const google::protobuf::Descriptor* descriptor = test_message.GetDescriptor();
  google::protobuf::Arena arena;

  EXPECT_THAT(AddValueToRepeatedField(
                  out_of_range, descriptor->FindFieldByName("repeated_int32"),
                  &test_message, &arena),
              StatusIs(absl::StatusCode::kInvalidArgument));

  // proto enums are are represented as int32, but CEL converts to/from int64.
  EXPECT_THAT(
      AddValueToRepeatedField(
          out_of_range, descriptor->FindFieldByName("repeated_nested_enum"),
          &test_message, &arena),
      StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(AddValueToRepeatedFieldTest, UintOutOfRange) {
  CelValue out_of_range = CelValue::CreateUint64(1LL << 32);
  TestAllTypes test_message;
  const google::protobuf::Descriptor* descriptor = test_message.GetDescriptor();
  google::protobuf::Arena arena;

  EXPECT_THAT(AddValueToRepeatedField(
                  out_of_range, descriptor->FindFieldByName("repeated_uint32"),
                  &test_message, &arena),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(AddValueToRepeatedFieldTest, AddMessage) {
  TestAllTypes::NestedMessage nested_message;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"(
      bb: 42
  )",
                                                  &nested_message));
  google::protobuf::Arena arena;
  CelValue nested_value =
      CelProtoWrapper::CreateMessage(&nested_message, &arena);
  TestAllTypes test_message;
  const google::protobuf::Descriptor* descriptor = test_message.GetDescriptor();

  ASSERT_OK(AddValueToRepeatedField(
      nested_value, descriptor->FindFieldByName("repeated_nested_message"),
      &test_message, &arena));
  EXPECT_THAT(test_message, EqualsProto("repeated_nested_message { bb: 42 }"));
}

constexpr std::array<const char*, 9> kWrapperFieldNames = {
    "single_bool_wrapper",   "single_int64_wrapper",  "single_int32_wrapper",
    "single_uint64_wrapper", "single_uint32_wrapper", "single_double_wrapper",
    "single_float_wrapper",  "single_string_wrapper", "single_bytes_wrapper"};

// Unset wrapper type fields are treated as null if accessed after option
// enabled.
TEST(CreateValueFromFieldTest, UnsetWrapperTypesNullIfEnabled) {
  CelValue result;
  TestAllTypes test_message;
  google::protobuf::Arena arena;

  for (const auto& field : kWrapperFieldNames) {
    ASSERT_OK_AND_ASSIGN(
        result, CreateValueFromSingleField(
                    &test_message,
                    TestAllTypes::GetDescriptor()->FindFieldByName(field),
                    ProtoWrapperTypeOptions::kUnsetNull,
                    &CelProtoWrapper::InternalWrapMessage, &arena));
    ASSERT_TRUE(result.IsNull()) << field << ": " << result.DebugString();
  }
}

// Unset wrapper type fields are treated as proto default under old
// behavior.
TEST(CreateValueFromFieldTest, UnsetWrapperTypesDefaultValueIfDisabled) {
  CelValue result;
  TestAllTypes test_message;
  google::protobuf::Arena arena;

  for (const auto& field : kWrapperFieldNames) {
    ASSERT_OK_AND_ASSIGN(
        result, CreateValueFromSingleField(
                    &test_message,
                    TestAllTypes::GetDescriptor()->FindFieldByName(field),
                    ProtoWrapperTypeOptions::kUnsetProtoDefault,
                    &CelProtoWrapper::InternalWrapMessage, &arena));
    ASSERT_FALSE(result.IsNull()) << field << ": " << result.DebugString();
  }
}

// If a wrapper type is set to default value, the corresponding CelValue is the
// proto default value.
TEST(CreateValueFromFieldTest, SetWrapperTypesDefaultValue) {
  CelValue result;
  TestAllTypes test_message;
  google::protobuf::Arena arena;

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        single_bool_wrapper {}
        single_int64_wrapper {}
        single_int32_wrapper {}
        single_uint64_wrapper {}
        single_uint32_wrapper {}
        single_double_wrapper {}
        single_float_wrapper {}
        single_string_wrapper {}
        single_bytes_wrapper {}
      )pb",
      &test_message));

  ASSERT_OK_AND_ASSIGN(
      result,
      CreateValueFromSingleField(
          &test_message,
          TestAllTypes::GetDescriptor()->FindFieldByName("single_bool_wrapper"),
          ProtoWrapperTypeOptions::kUnsetNull,
          &CelProtoWrapper::InternalWrapMessage, &arena));
  EXPECT_THAT(result, test::IsCelBool(false));

  ASSERT_OK_AND_ASSIGN(result,
                       CreateValueFromSingleField(
                           &test_message,
                           TestAllTypes::GetDescriptor()->FindFieldByName(
                               "single_int64_wrapper"),
                           ProtoWrapperTypeOptions::kUnsetNull,
                           &CelProtoWrapper::InternalWrapMessage, &arena));
  EXPECT_THAT(result, test::IsCelInt64(0));

  ASSERT_OK_AND_ASSIGN(result,
                       CreateValueFromSingleField(
                           &test_message,
                           TestAllTypes::GetDescriptor()->FindFieldByName(
                               "single_int32_wrapper"),
                           ProtoWrapperTypeOptions::kUnsetNull,
                           &CelProtoWrapper::InternalWrapMessage, &arena));
  EXPECT_THAT(result, test::IsCelInt64(0));

  ASSERT_OK_AND_ASSIGN(
      result,
      CreateValueFromSingleField(&test_message,
                                 TestAllTypes::GetDescriptor()->FindFieldByName(
                                     "single_uint64_wrapper"),
                                 ProtoWrapperTypeOptions::kUnsetNull,
                                 &CelProtoWrapper::InternalWrapMessage,

                                 &arena));
  EXPECT_THAT(result, test::IsCelUint64(0));

  ASSERT_OK_AND_ASSIGN(
      result,
      CreateValueFromSingleField(&test_message,
                                 TestAllTypes::GetDescriptor()->FindFieldByName(
                                     "single_uint32_wrapper"),
                                 ProtoWrapperTypeOptions::kUnsetNull,
                                 &CelProtoWrapper::InternalWrapMessage,

                                 &arena));
  EXPECT_THAT(result, test::IsCelUint64(0));

  ASSERT_OK_AND_ASSIGN(result,
                       CreateValueFromSingleField(
                           &test_message,
                           TestAllTypes::GetDescriptor()->FindFieldByName(
                               "single_double_wrapper"),
                           ProtoWrapperTypeOptions::kUnsetNull,

                           &CelProtoWrapper::InternalWrapMessage, &arena));
  EXPECT_THAT(result, test::IsCelDouble(0.0f));

  ASSERT_OK_AND_ASSIGN(result,
                       CreateValueFromSingleField(
                           &test_message,
                           TestAllTypes::GetDescriptor()->FindFieldByName(
                               "single_float_wrapper"),
                           ProtoWrapperTypeOptions::kUnsetNull,

                           &CelProtoWrapper::InternalWrapMessage, &arena));
  EXPECT_THAT(result, test::IsCelDouble(0.0f));

  ASSERT_OK_AND_ASSIGN(result,
                       CreateValueFromSingleField(
                           &test_message,
                           TestAllTypes::GetDescriptor()->FindFieldByName(
                               "single_string_wrapper"),
                           ProtoWrapperTypeOptions::kUnsetNull,

                           &CelProtoWrapper::InternalWrapMessage, &arena));
  EXPECT_THAT(result, test::IsCelString(""));

  ASSERT_OK_AND_ASSIGN(result,
                       CreateValueFromSingleField(
                           &test_message,
                           TestAllTypes::GetDescriptor()->FindFieldByName(
                               "single_bytes_wrapper"),
                           ProtoWrapperTypeOptions::kUnsetNull,

                           &CelProtoWrapper::InternalWrapMessage, &arena));
  EXPECT_THAT(result, test::IsCelBytes(""));
}

}  // namespace

}  // namespace google::api::expr::runtime::internal
