// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/public/structs/cel_proto_wrap_util.h"

#include <cassert>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/empty.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/message.h"
#include "absl/base/no_destructor.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/structs/protobuf_value_factory.h"
#include "eval/public/structs/trivial_legacy_type_info.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/proto_time_encoding.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "testutil/util.h"

namespace google::api::expr::runtime::internal {

namespace {

using ::testing::Eq;
using ::testing::UnorderedPointwise;

using google::protobuf::Duration;
using google::protobuf::ListValue;
using google::protobuf::Struct;
using google::protobuf::Timestamp;
using google::protobuf::Value;

using google::protobuf::Any;
using google::protobuf::BoolValue;
using google::protobuf::BytesValue;
using google::protobuf::DoubleValue;
using google::protobuf::FloatValue;
using google::protobuf::Int32Value;
using google::protobuf::Int64Value;
using google::protobuf::StringValue;
using google::protobuf::UInt32Value;
using google::protobuf::UInt64Value;

using google::protobuf::Arena;

CelValue ProtobufValueFactoryImpl(const google::protobuf::Message* m) {
  return CelValue::CreateMessageWrapper(
      CelValue::MessageWrapper(m, TrivialTypeInfo::GetInstance()));
}

class CelProtoWrapperTest : public ::testing::Test {
 protected:
  CelProtoWrapperTest() {}

  void ExpectWrappedMessage(const CelValue& value,
                            const google::protobuf::Message& message) {
    // Test the input value wraps to the destination message type.
    auto* result = MaybeWrapValueToMessage(
        message.GetDescriptor(), message.GetReflection()->GetMessageFactory(),
        value, arena());
    EXPECT_TRUE(result != nullptr);
    EXPECT_THAT(result, testutil::EqualsProto(message));

    // Ensure that double wrapping results in the object being wrapped once.
    auto* identity = MaybeWrapValueToMessage(
        message.GetDescriptor(), message.GetReflection()->GetMessageFactory(),
        ProtobufValueFactoryImpl(result), arena());
    EXPECT_TRUE(identity == nullptr);

    // Check to make sure that even dynamic messages can be used as input to
    // the wrapping call.
    result = MaybeWrapValueToMessage(
        ReflectedCopy(message)->GetDescriptor(),
        ReflectedCopy(message)->GetReflection()->GetMessageFactory(), value,
        arena());
    EXPECT_TRUE(result != nullptr);
    EXPECT_THAT(result, testutil::EqualsProto(message));
  }

  void ExpectNotWrapped(const CelValue& value, const google::protobuf::Message& message) {
    // Test the input value does not wrap by asserting value == result.
    auto result = MaybeWrapValueToMessage(
        message.GetDescriptor(), message.GetReflection()->GetMessageFactory(),
        value, arena());
    EXPECT_TRUE(result == nullptr);
  }

  template <class T>
  void ExpectUnwrappedPrimitive(const google::protobuf::Message& message, T result) {
    CelValue cel_value =
        UnwrapMessageToValue(&message, &ProtobufValueFactoryImpl, arena());
    T value;
    EXPECT_TRUE(cel_value.GetValue(&value));
    EXPECT_THAT(value, Eq(result));

    T dyn_value;
    CelValue cel_dyn_value = UnwrapMessageToValue(
        ReflectedCopy(message).get(), &ProtobufValueFactoryImpl, arena());
    EXPECT_THAT(cel_dyn_value.type(), Eq(cel_value.type()));
    EXPECT_TRUE(cel_dyn_value.GetValue(&dyn_value));
    EXPECT_THAT(value, Eq(dyn_value));
  }

  void ExpectUnwrappedMessage(const google::protobuf::Message& message,
                              google::protobuf::Message* result) {
    CelValue cel_value =
        UnwrapMessageToValue(&message, &ProtobufValueFactoryImpl, arena());
    if (result == nullptr) {
      EXPECT_TRUE(cel_value.IsNull());
      return;
    }
    EXPECT_TRUE(cel_value.IsMessage());
    EXPECT_THAT(cel_value.MessageOrDie(), testutil::EqualsProto(*result));
  }

  std::unique_ptr<google::protobuf::Message> ReflectedCopy(
      const google::protobuf::Message& message) {
    std::unique_ptr<google::protobuf::Message> dynamic_value(
        factory_.GetPrototype(message.GetDescriptor())->New());
    dynamic_value->CopyFrom(message);
    return dynamic_value;
  }

  Arena* arena() { return &arena_; }

 private:
  Arena arena_;
  google::protobuf::DynamicMessageFactory factory_;
};

TEST_F(CelProtoWrapperTest, TestType) {
  Duration msg_duration;
  msg_duration.set_seconds(2);
  msg_duration.set_nanos(3);

  CelValue value_duration2 =
      UnwrapMessageToValue(&msg_duration, &ProtobufValueFactoryImpl, arena());
  EXPECT_THAT(value_duration2.type(), Eq(CelValue::Type::kDuration));

  Timestamp msg_timestamp;
  msg_timestamp.set_seconds(2);
  msg_timestamp.set_nanos(3);

  CelValue value_timestamp2 =
      UnwrapMessageToValue(&msg_timestamp, &ProtobufValueFactoryImpl, arena());
  EXPECT_THAT(value_timestamp2.type(), Eq(CelValue::Type::kTimestamp));
}

// This test verifies CelValue support of Duration type.
TEST_F(CelProtoWrapperTest, TestDuration) {
  Duration msg_duration;
  msg_duration.set_seconds(2);
  msg_duration.set_nanos(3);
  CelValue value =
      UnwrapMessageToValue(&msg_duration, &ProtobufValueFactoryImpl, arena());
  EXPECT_THAT(value.type(), Eq(CelValue::Type::kDuration));

  Duration out;
  auto status = cel::internal::EncodeDuration(value.DurationOrDie(), &out);
  EXPECT_TRUE(status.ok());
  EXPECT_THAT(out, testutil::EqualsProto(msg_duration));
}

// This test verifies CelValue support of Timestamp type.
TEST_F(CelProtoWrapperTest, TestTimestamp) {
  Timestamp msg_timestamp;
  msg_timestamp.set_seconds(2);
  msg_timestamp.set_nanos(3);

  CelValue value =
      UnwrapMessageToValue(&msg_timestamp, &ProtobufValueFactoryImpl, arena());

  EXPECT_TRUE(value.IsTimestamp());
  Timestamp out;
  auto status = cel::internal::EncodeTime(value.TimestampOrDie(), &out);
  EXPECT_TRUE(status.ok());
  EXPECT_THAT(out, testutil::EqualsProto(msg_timestamp));
}

// Dynamic Values test
//
TEST_F(CelProtoWrapperTest, UnwrapMessageToValueNull) {
  Value json;
  json.set_null_value(google::protobuf::NullValue::NULL_VALUE);
  ExpectUnwrappedMessage(json, nullptr);
}

// Test support for unwrapping a google::protobuf::Value to a CEL value.
TEST_F(CelProtoWrapperTest, UnwrapDynamicValueNull) {
  Value value_msg;
  value_msg.set_null_value(protobuf::NULL_VALUE);

  CelValue value = UnwrapMessageToValue(ReflectedCopy(value_msg).get(),
                                        &ProtobufValueFactoryImpl, arena());
  EXPECT_TRUE(value.IsNull());
}

TEST_F(CelProtoWrapperTest, UnwrapMessageToValueBool) {
  bool value = true;

  Value json;
  json.set_bool_value(true);
  ExpectUnwrappedPrimitive(json, value);
}

TEST_F(CelProtoWrapperTest, UnwrapMessageToValueNumber) {
  double value = 1.0;

  Value json;
  json.set_number_value(value);
  ExpectUnwrappedPrimitive(json, value);
}

TEST_F(CelProtoWrapperTest, UnwrapMessageToValueString) {
  const std::string test = "test";
  auto value = CelValue::StringHolder(&test);

  Value json;
  json.set_string_value(test);
  ExpectUnwrappedPrimitive(json, value);
}

TEST_F(CelProtoWrapperTest, UnwrapMessageToValueStruct) {
  const std::vector<std::string> kFields = {"field1", "field2", "field3"};
  Struct value_struct;

  auto& value1 = (*value_struct.mutable_fields())[kFields[0]];
  value1.set_bool_value(true);

  auto& value2 = (*value_struct.mutable_fields())[kFields[1]];
  value2.set_number_value(1.0);

  auto& value3 = (*value_struct.mutable_fields())[kFields[2]];
  value3.set_string_value("test");

  CelValue value =
      UnwrapMessageToValue(&value_struct, &ProtobufValueFactoryImpl, arena());
  ASSERT_TRUE(value.IsMap());

  const CelMap* cel_map = value.MapOrDie();

  CelValue field1 = CelValue::CreateString(&kFields[0]);
  auto field1_presence = cel_map->Has(field1);
  ASSERT_OK(field1_presence);
  EXPECT_TRUE(*field1_presence);
  auto lookup1 = (*cel_map)[field1];
  ASSERT_TRUE(lookup1.has_value());
  ASSERT_TRUE(lookup1->IsBool());
  EXPECT_EQ(lookup1->BoolOrDie(), true);

  CelValue field2 = CelValue::CreateString(&kFields[1]);
  auto field2_presence = cel_map->Has(field2);
  ASSERT_OK(field2_presence);
  EXPECT_TRUE(*field2_presence);
  auto lookup2 = (*cel_map)[field2];
  ASSERT_TRUE(lookup2.has_value());
  ASSERT_TRUE(lookup2->IsDouble());
  EXPECT_DOUBLE_EQ(lookup2->DoubleOrDie(), 1.0);

  CelValue field3 = CelValue::CreateString(&kFields[2]);
  auto field3_presence = cel_map->Has(field3);
  ASSERT_OK(field3_presence);
  EXPECT_TRUE(*field3_presence);
  auto lookup3 = (*cel_map)[field3];
  ASSERT_TRUE(lookup3.has_value());
  ASSERT_TRUE(lookup3->IsString());
  EXPECT_EQ(lookup3->StringOrDie().value(), "test");

  std::string missing = "missing_field";
  CelValue missing_field = CelValue::CreateString(&missing);
  auto missing_field_presence = cel_map->Has(missing_field);
  ASSERT_OK(missing_field_presence);
  EXPECT_FALSE(*missing_field_presence);

  const CelList* key_list = cel_map->ListKeys().value();
  ASSERT_EQ(key_list->size(), kFields.size());

  std::vector<std::string> result_keys;
  for (int i = 0; i < key_list->size(); i++) {
    CelValue key = (*key_list)[i];
    ASSERT_TRUE(key.IsString());
    result_keys.push_back(std::string(key.StringOrDie().value()));
  }

  EXPECT_THAT(result_keys, UnorderedPointwise(Eq(), kFields));
}

// Test support for google::protobuf::Struct when it is created as dynamic
// message
TEST_F(CelProtoWrapperTest, UnwrapDynamicStruct) {
  Struct struct_msg;
  const std::string kFieldInt = "field_int";
  const std::string kFieldBool = "field_bool";
  (*struct_msg.mutable_fields())[kFieldInt].set_number_value(1.);
  (*struct_msg.mutable_fields())[kFieldBool].set_bool_value(true);
  CelValue value = UnwrapMessageToValue(ReflectedCopy(struct_msg).get(),
                                        &ProtobufValueFactoryImpl, arena());
  EXPECT_TRUE(value.IsMap());
  const CelMap* cel_map = value.MapOrDie();
  ASSERT_TRUE(cel_map != nullptr);

  {
    auto lookup = (*cel_map)[CelValue::CreateString(&kFieldInt)];
    ASSERT_TRUE(lookup.has_value());
    auto v = lookup.value();
    ASSERT_TRUE(v.IsDouble());
    EXPECT_THAT(v.DoubleOrDie(), testing::DoubleEq(1.));
  }
  {
    auto lookup = (*cel_map)[CelValue::CreateString(&kFieldBool)];
    ASSERT_TRUE(lookup.has_value());
    auto v = lookup.value();
    ASSERT_TRUE(v.IsBool());
    EXPECT_EQ(v.BoolOrDie(), true);
  }
  {
    auto presence = cel_map->Has(CelValue::CreateBool(true));
    ASSERT_FALSE(presence.ok());
    EXPECT_EQ(presence.status().code(), absl::StatusCode::kInvalidArgument);
    auto lookup = (*cel_map)[CelValue::CreateBool(true)];
    ASSERT_TRUE(lookup.has_value());
    auto v = lookup.value();
    ASSERT_TRUE(v.IsError());
  }
}

TEST_F(CelProtoWrapperTest, UnwrapDynamicValueStruct) {
  const std::string kField1 = "field1";
  const std::string kField2 = "field2";
  Value value_msg;
  (*value_msg.mutable_struct_value()->mutable_fields())[kField1]
      .set_number_value(1);
  (*value_msg.mutable_struct_value()->mutable_fields())[kField2]
      .set_number_value(2);

  CelValue value = UnwrapMessageToValue(ReflectedCopy(value_msg).get(),
                                        &ProtobufValueFactoryImpl, arena());
  EXPECT_TRUE(value.IsMap());
  EXPECT_TRUE(
      (*value.MapOrDie())[CelValue::CreateString(&kField1)].has_value());
  EXPECT_TRUE(
      (*value.MapOrDie())[CelValue::CreateString(&kField2)].has_value());
}

TEST_F(CelProtoWrapperTest, UnwrapMessageToValueList) {
  const std::vector<std::string> kFields = {"field1", "field2", "field3"};

  ListValue list_value;

  list_value.add_values()->set_bool_value(true);
  list_value.add_values()->set_number_value(1.0);
  list_value.add_values()->set_string_value("test");

  CelValue value =
      UnwrapMessageToValue(&list_value, &ProtobufValueFactoryImpl, arena());
  ASSERT_TRUE(value.IsList());

  const CelList* cel_list = value.ListOrDie();

  ASSERT_EQ(cel_list->size(), 3);

  CelValue value1 = (*cel_list)[0];
  ASSERT_TRUE(value1.IsBool());
  EXPECT_EQ(value1.BoolOrDie(), true);

  auto value2 = (*cel_list)[1];
  ASSERT_TRUE(value2.IsDouble());
  EXPECT_DOUBLE_EQ(value2.DoubleOrDie(), 1.0);

  auto value3 = (*cel_list)[2];
  ASSERT_TRUE(value3.IsString());
  EXPECT_EQ(value3.StringOrDie().value(), "test");
}

TEST_F(CelProtoWrapperTest, UnwrapDynamicValueListValue) {
  Value value_msg;
  value_msg.mutable_list_value()->add_values()->set_number_value(1.);
  value_msg.mutable_list_value()->add_values()->set_number_value(2.);

  CelValue value = UnwrapMessageToValue(ReflectedCopy(value_msg).get(),
                                        &ProtobufValueFactoryImpl, arena());
  EXPECT_TRUE(value.IsList());
  EXPECT_THAT((*value.ListOrDie())[0].DoubleOrDie(), testing::DoubleEq(1));
  EXPECT_THAT((*value.ListOrDie())[1].DoubleOrDie(), testing::DoubleEq(2));
}

// Test support of google.protobuf.Any in CelValue.
TEST_F(CelProtoWrapperTest, UnwrapAnyValue) {
  TestMessage test_message;
  test_message.set_string_value("test");

  Any any;
  any.PackFrom(test_message);
  ExpectUnwrappedMessage(any, &test_message);
}

TEST_F(CelProtoWrapperTest, UnwrapInvalidAny) {
  Any any;
  CelValue value =
      UnwrapMessageToValue(&any, &ProtobufValueFactoryImpl, arena());
  ASSERT_TRUE(value.IsError());

  any.set_type_url("/");
  ASSERT_TRUE(
      UnwrapMessageToValue(&any, &ProtobufValueFactoryImpl, arena()).IsError());

  any.set_type_url("/invalid.proto.name");
  ASSERT_TRUE(
      UnwrapMessageToValue(&any, &ProtobufValueFactoryImpl, arena()).IsError());
}

// Test support of google.protobuf.<Type>Value wrappers in CelValue.
TEST_F(CelProtoWrapperTest, UnwrapBoolWrapper) {
  bool value = true;

  BoolValue wrapper;
  wrapper.set_value(value);
  ExpectUnwrappedPrimitive(wrapper, value);
}

TEST_F(CelProtoWrapperTest, UnwrapInt32Wrapper) {
  int64_t value = 12;

  Int32Value wrapper;
  wrapper.set_value(value);
  ExpectUnwrappedPrimitive(wrapper, value);
}

TEST_F(CelProtoWrapperTest, UnwrapUInt32Wrapper) {
  uint64_t value = 12;

  UInt32Value wrapper;
  wrapper.set_value(value);
  ExpectUnwrappedPrimitive(wrapper, value);
}

TEST_F(CelProtoWrapperTest, UnwrapInt64Wrapper) {
  int64_t value = 12;

  Int64Value wrapper;
  wrapper.set_value(value);
  ExpectUnwrappedPrimitive(wrapper, value);
}

TEST_F(CelProtoWrapperTest, UnwrapUInt64Wrapper) {
  uint64_t value = 12;

  UInt64Value wrapper;
  wrapper.set_value(value);
  ExpectUnwrappedPrimitive(wrapper, value);
}

TEST_F(CelProtoWrapperTest, UnwrapFloatWrapper) {
  double value = 42.5;

  FloatValue wrapper;
  wrapper.set_value(value);
  ExpectUnwrappedPrimitive(wrapper, value);
}

TEST_F(CelProtoWrapperTest, UnwrapDoubleWrapper) {
  double value = 42.5;

  DoubleValue wrapper;
  wrapper.set_value(value);
  ExpectUnwrappedPrimitive(wrapper, value);
}

TEST_F(CelProtoWrapperTest, UnwrapStringWrapper) {
  std::string text = "42";
  auto value = CelValue::StringHolder(&text);

  StringValue wrapper;
  wrapper.set_value(text);
  ExpectUnwrappedPrimitive(wrapper, value);
}

TEST_F(CelProtoWrapperTest, UnwrapBytesWrapper) {
  std::string text = "42";
  auto value = CelValue::BytesHolder(&text);

  BytesValue wrapper;
  wrapper.set_value("42");
  ExpectUnwrappedPrimitive(wrapper, value);
}

TEST_F(CelProtoWrapperTest, WrapNull) {
  auto cel_value = CelValue::CreateNull();

  Value json;
  json.set_null_value(protobuf::NULL_VALUE);
  ExpectWrappedMessage(cel_value, json);

  Any any;
  any.PackFrom(json);
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapBool) {
  auto cel_value = CelValue::CreateBool(true);

  Value json;
  json.set_bool_value(true);
  ExpectWrappedMessage(cel_value, json);

  BoolValue wrapper;
  wrapper.set_value(true);
  ExpectWrappedMessage(cel_value, wrapper);

  Any any;
  any.PackFrom(wrapper);
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapBytes) {
  std::string str = "hello world";
  auto cel_value = CelValue::CreateBytes(CelValue::BytesHolder(&str));

  BytesValue wrapper;
  wrapper.set_value(str);
  ExpectWrappedMessage(cel_value, wrapper);

  Any any;
  any.PackFrom(wrapper);
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapBytesToValue) {
  std::string str = "hello world";
  auto cel_value = CelValue::CreateBytes(CelValue::BytesHolder(&str));

  Value json;
  json.set_string_value("aGVsbG8gd29ybGQ=");
  ExpectWrappedMessage(cel_value, json);
}

TEST_F(CelProtoWrapperTest, WrapDuration) {
  auto cel_value = CelValue::CreateDuration(absl::Seconds(300));

  Duration d;
  d.set_seconds(300);
  ExpectWrappedMessage(cel_value, d);

  Any any;
  any.PackFrom(d);
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapDurationToValue) {
  auto cel_value = CelValue::CreateDuration(absl::Seconds(300));

  Value json;
  json.set_string_value("300s");
  ExpectWrappedMessage(cel_value, json);
}

TEST_F(CelProtoWrapperTest, WrapDouble) {
  double num = 1.5;
  auto cel_value = CelValue::CreateDouble(num);

  Value json;
  json.set_number_value(num);
  ExpectWrappedMessage(cel_value, json);

  DoubleValue wrapper;
  wrapper.set_value(num);
  ExpectWrappedMessage(cel_value, wrapper);

  Any any;
  any.PackFrom(wrapper);
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapDoubleToFloatValue) {
  double num = 1.5;
  auto cel_value = CelValue::CreateDouble(num);

  FloatValue wrapper;
  wrapper.set_value(num);
  ExpectWrappedMessage(cel_value, wrapper);

  // Imprecise double -> float representation results in truncation.
  double small_num = -9.9e-100;
  wrapper.set_value(small_num);
  cel_value = CelValue::CreateDouble(small_num);
  ExpectWrappedMessage(cel_value, wrapper);
}

TEST_F(CelProtoWrapperTest, WrapDoubleOverflow) {
  double lowest_double = std::numeric_limits<double>::lowest();
  auto cel_value = CelValue::CreateDouble(lowest_double);

  // Double exceeds float precision, overflow to -infinity.
  FloatValue wrapper;
  wrapper.set_value(-std::numeric_limits<float>::infinity());
  ExpectWrappedMessage(cel_value, wrapper);

  double max_double = std::numeric_limits<double>::max();
  cel_value = CelValue::CreateDouble(max_double);

  wrapper.set_value(std::numeric_limits<float>::infinity());
  ExpectWrappedMessage(cel_value, wrapper);
}

TEST_F(CelProtoWrapperTest, WrapInt64) {
  int32_t num = std::numeric_limits<int32_t>::lowest();
  auto cel_value = CelValue::CreateInt64(num);

  Value json;
  json.set_number_value(static_cast<double>(num));
  ExpectWrappedMessage(cel_value, json);

  Int64Value wrapper;
  wrapper.set_value(num);
  ExpectWrappedMessage(cel_value, wrapper);

  Any any;
  any.PackFrom(wrapper);
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapInt64ToInt32Value) {
  int32_t num = std::numeric_limits<int32_t>::lowest();
  auto cel_value = CelValue::CreateInt64(num);

  Int32Value wrapper;
  wrapper.set_value(num);
  ExpectWrappedMessage(cel_value, wrapper);
}

TEST_F(CelProtoWrapperTest, WrapFailureInt64ToInt32Value) {
  int64_t num = std::numeric_limits<int64_t>::lowest();
  auto cel_value = CelValue::CreateInt64(num);

  Int32Value wrapper;
  ExpectNotWrapped(cel_value, wrapper);
}

TEST_F(CelProtoWrapperTest, WrapInt64ToValue) {
  int64_t max = std::numeric_limits<int64_t>::max();
  auto cel_value = CelValue::CreateInt64(max);

  Value json;
  json.set_string_value(absl::StrCat(max));
  ExpectWrappedMessage(cel_value, json);

  int64_t min = std::numeric_limits<int64_t>::min();
  cel_value = CelValue::CreateInt64(min);

  json.set_string_value(absl::StrCat(min));
  ExpectWrappedMessage(cel_value, json);
}

TEST_F(CelProtoWrapperTest, WrapUint64) {
  uint32_t num = std::numeric_limits<uint32_t>::max();
  auto cel_value = CelValue::CreateUint64(num);

  Value json;
  json.set_number_value(static_cast<double>(num));
  ExpectWrappedMessage(cel_value, json);

  UInt64Value wrapper;
  wrapper.set_value(num);
  ExpectWrappedMessage(cel_value, wrapper);

  Any any;
  any.PackFrom(wrapper);
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapUint64ToUint32Value) {
  uint32_t num = std::numeric_limits<uint32_t>::max();
  auto cel_value = CelValue::CreateUint64(num);

  UInt32Value wrapper;
  wrapper.set_value(num);
  ExpectWrappedMessage(cel_value, wrapper);
}

TEST_F(CelProtoWrapperTest, WrapUint64ToValue) {
  uint64_t num = std::numeric_limits<uint64_t>::max();
  auto cel_value = CelValue::CreateUint64(num);

  Value json;
  json.set_string_value(absl::StrCat(num));
  ExpectWrappedMessage(cel_value, json);
}

TEST_F(CelProtoWrapperTest, WrapFailureUint64ToUint32Value) {
  uint64_t num = std::numeric_limits<uint64_t>::max();
  auto cel_value = CelValue::CreateUint64(num);

  UInt32Value wrapper;
  ExpectNotWrapped(cel_value, wrapper);
}

TEST_F(CelProtoWrapperTest, WrapString) {
  std::string str = "test";
  auto cel_value = CelValue::CreateString(CelValue::StringHolder(&str));

  Value json;
  json.set_string_value(str);
  ExpectWrappedMessage(cel_value, json);

  StringValue wrapper;
  wrapper.set_value(str);
  ExpectWrappedMessage(cel_value, wrapper);

  Any any;
  any.PackFrom(wrapper);
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapTimestamp) {
  absl::Time ts = absl::FromUnixSeconds(1615852799);
  auto cel_value = CelValue::CreateTimestamp(ts);

  Timestamp t;
  t.set_seconds(1615852799);
  ExpectWrappedMessage(cel_value, t);

  Any any;
  any.PackFrom(t);
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapTimestampToValue) {
  absl::Time ts = absl::FromUnixSeconds(1615852799);
  auto cel_value = CelValue::CreateTimestamp(ts);

  Value json;
  json.set_string_value("2021-03-15T23:59:59Z");
  ExpectWrappedMessage(cel_value, json);
}

TEST_F(CelProtoWrapperTest, WrapList) {
  std::vector<CelValue> list_elems = {
      CelValue::CreateDouble(1.5),
      CelValue::CreateInt64(-2L),
  };
  ContainerBackedListImpl list(std::move(list_elems));
  auto cel_value = CelValue::CreateList(&list);

  Value json;
  json.mutable_list_value()->add_values()->set_number_value(1.5);
  json.mutable_list_value()->add_values()->set_number_value(-2.);
  ExpectWrappedMessage(cel_value, json);
  ExpectWrappedMessage(cel_value, json.list_value());

  Any any;
  any.PackFrom(json.list_value());
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapFailureListValueBadJSON) {
  TestMessage message;
  std::vector<CelValue> list_elems = {
      CelValue::CreateDouble(1.5),
      UnwrapMessageToValue(&message, &ProtobufValueFactoryImpl, arena()),
  };
  ContainerBackedListImpl list(std::move(list_elems));
  auto cel_value = CelValue::CreateList(&list);

  Value json;
  ExpectNotWrapped(cel_value, json);
}

TEST_F(CelProtoWrapperTest, WrapStruct) {
  const std::string kField1 = "field1";
  std::vector<std::pair<CelValue, CelValue>> args = {
      {CelValue::CreateString(CelValue::StringHolder(&kField1)),
       CelValue::CreateBool(true)}};
  auto cel_map =
      CreateContainerBackedMap(
          absl::Span<std::pair<CelValue, CelValue>>(args.data(), args.size()))
          .value();
  auto cel_value = CelValue::CreateMap(cel_map.get());

  Value json;
  (*json.mutable_struct_value()->mutable_fields())[kField1].set_bool_value(
      true);
  ExpectWrappedMessage(cel_value, json);
  ExpectWrappedMessage(cel_value, json.struct_value());

  Any any;
  any.PackFrom(json.struct_value());
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapFailureStructBadKeyType) {
  std::vector<std::pair<CelValue, CelValue>> args = {
      {CelValue::CreateInt64(1L), CelValue::CreateBool(true)}};
  auto cel_map =
      CreateContainerBackedMap(
          absl::Span<std::pair<CelValue, CelValue>>(args.data(), args.size()))
          .value();
  auto cel_value = CelValue::CreateMap(cel_map.get());

  Value json;
  ExpectNotWrapped(cel_value, json);
}

TEST_F(CelProtoWrapperTest, WrapFailureStructBadValueType) {
  const std::string kField1 = "field1";
  TestMessage bad_value;
  std::vector<std::pair<CelValue, CelValue>> args = {
      {CelValue::CreateString(CelValue::StringHolder(&kField1)),
       UnwrapMessageToValue(&bad_value, &ProtobufValueFactoryImpl, arena())}};
  auto cel_map =
      CreateContainerBackedMap(
          absl::Span<std::pair<CelValue, CelValue>>(args.data(), args.size()))
          .value();
  auto cel_value = CelValue::CreateMap(cel_map.get());
  Value json;
  ExpectNotWrapped(cel_value, json);
}

class TestMap : public CelMapBuilder {
 public:
  absl::StatusOr<const CelList*> ListKeys() const override {
    return absl::UnimplementedError("test");
  }
};

TEST_F(CelProtoWrapperTest, WrapFailureStructListKeysUnimplemented) {
  const std::string kField1 = "field1";
  TestMap map;
  ASSERT_OK(map.Add(CelValue::CreateString(CelValue::StringHolder(&kField1)),
                    CelValue::CreateString(CelValue::StringHolder(&kField1))));

  auto cel_value = CelValue::CreateMap(&map);
  Value json;
  ExpectNotWrapped(cel_value, json);
}

TEST_F(CelProtoWrapperTest, WrapFailureWrongType) {
  auto cel_value = CelValue::CreateNull();
  std::vector<const google::protobuf::Message*> wrong_types = {
      &BoolValue::default_instance(),   &BytesValue::default_instance(),
      &DoubleValue::default_instance(), &Duration::default_instance(),
      &FloatValue::default_instance(),  &Int32Value::default_instance(),
      &Int64Value::default_instance(),  &ListValue::default_instance(),
      &StringValue::default_instance(), &Struct::default_instance(),
      &Timestamp::default_instance(),   &UInt32Value::default_instance(),
      &UInt64Value::default_instance(),
  };
  for (const auto* wrong_type : wrong_types) {
    ExpectNotWrapped(cel_value, *wrong_type);
  }
}

TEST_F(CelProtoWrapperTest, WrapFailureErrorToAny) {
  auto cel_value = CreateNoSuchFieldError(arena(), "error_field");
  ExpectNotWrapped(cel_value, Any::default_instance());
}

TEST_F(CelProtoWrapperTest, DebugString) {
  google::protobuf::Empty e;
  // Note: the value factory is trivial so the debug string for a message-typed
  // value is uninteresting.
  EXPECT_EQ(UnwrapMessageToValue(&e, &ProtobufValueFactoryImpl, arena())
                .DebugString(),
            "Message: opaque");

  ListValue list_value;
  list_value.add_values()->set_bool_value(true);
  list_value.add_values()->set_number_value(1.0);
  list_value.add_values()->set_string_value("test");
  CelValue value =
      UnwrapMessageToValue(&list_value, &ProtobufValueFactoryImpl, arena());
  EXPECT_EQ(value.DebugString(),
            "CelList: [bool: 1, double: 1.000000, string: test]");

  Struct value_struct;
  auto& value1 = (*value_struct.mutable_fields())["a"];
  value1.set_bool_value(true);
  auto& value2 = (*value_struct.mutable_fields())["b"];
  value2.set_number_value(1.0);
  auto& value3 = (*value_struct.mutable_fields())["c"];
  value3.set_string_value("test");

  value =
      UnwrapMessageToValue(&value_struct, &ProtobufValueFactoryImpl, arena());
  EXPECT_THAT(
      value.DebugString(),
      testing::AllOf(testing::StartsWith("CelMap: {"),
                     testing::HasSubstr("<string: a>: <bool: 1>"),
                     testing::HasSubstr("<string: b>: <double: 1.0"),
                     testing::HasSubstr("<string: c>: <string: test>")));
}

}  // namespace

}  // namespace google::api::expr::runtime::internal
