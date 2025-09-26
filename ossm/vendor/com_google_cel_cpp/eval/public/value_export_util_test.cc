#include "eval/public/value_export_util.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "testutil/util.h"

namespace google::api::expr::runtime {

namespace {

using google::protobuf::Duration;
using google::protobuf::ListValue;
using google::protobuf::Struct;
using google::protobuf::Timestamp;
using google::protobuf::Value;
using google::protobuf::Arena;

TEST(ValueExportUtilTest, ConvertBoolValue) {
  CelValue cel_value = CelValue::CreateBool(true);
  Value value;
  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kBoolValue);
  EXPECT_EQ(value.bool_value(), true);
}

TEST(ValueExportUtilTest, ConvertInt64Value) {
  CelValue cel_value = CelValue::CreateInt64(-1);
  Value value;
  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kNumberValue);
  EXPECT_DOUBLE_EQ(value.number_value(), -1);
}

TEST(ValueExportUtilTest, ConvertUint64Value) {
  CelValue cel_value = CelValue::CreateUint64(1);
  Value value;
  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kNumberValue);
  EXPECT_DOUBLE_EQ(value.number_value(), 1);
}

TEST(ValueExportUtilTest, ConvertDoubleValue) {
  CelValue cel_value = CelValue::CreateDouble(1.3);
  Value value;
  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kNumberValue);
  EXPECT_DOUBLE_EQ(value.number_value(), 1.3);
}

TEST(ValueExportUtilTest, ConvertStringValue) {
  std::string test = "test";
  CelValue cel_value = CelValue::CreateString(&test);
  Value value;
  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kStringValue);
  EXPECT_EQ(value.string_value(), "test");
}

TEST(ValueExportUtilTest, ConvertBytesValue) {
  std::string test = "test";
  CelValue cel_value = CelValue::CreateBytes(&test);
  Value value;
  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kStringValue);
  // Check that the result is BASE64 encoded.
  EXPECT_EQ(value.string_value(), "dGVzdA==");
}

TEST(ValueExportUtilTest, ConvertDurationValue) {
  Duration duration;
  duration.set_seconds(2);
  duration.set_nanos(3);
  CelValue cel_value = CelProtoWrapper::CreateDuration(&duration);
  Value value;
  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kStringValue);
  EXPECT_EQ(value.string_value(), "2.000000003s");
}

TEST(ValueExportUtilTest, ConvertTimestampValue) {
  Timestamp timestamp;
  timestamp.set_seconds(1000000000);
  timestamp.set_nanos(3);
  CelValue cel_value = CelProtoWrapper::CreateTimestamp(&timestamp);
  Value value;
  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kStringValue);
  EXPECT_EQ(value.string_value(), "2001-09-09T01:46:40.000000003Z");
}

TEST(ValueExportUtilTest, ConvertStructMessage) {
  Struct struct_msg;
  (*struct_msg.mutable_fields())["string_value"].set_string_value("test");
  Arena arena;
  CelValue cel_value = CelProtoWrapper::CreateMessage(&struct_msg, &arena);
  Value value;
  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kStructValue);
  EXPECT_THAT(value.struct_value(), testutil::EqualsProto(struct_msg));
}

TEST(ValueExportUtilTest, ConvertValueMessage) {
  Value value_in;
  // key-based access forces value to be a struct.
  (*value_in.mutable_struct_value()->mutable_fields())["boolean_value"]
      .set_bool_value(true);
  Arena arena;
  CelValue cel_value = CelProtoWrapper::CreateMessage(&value_in, &arena);
  Value value_out;
  EXPECT_OK(ExportAsProtoValue(cel_value, &value_out));
  EXPECT_THAT(value_in, testutil::EqualsProto(value_out));
}

TEST(ValueExportUtilTest, ConvertListValueMessage) {
  ListValue list_value;
  list_value.add_values()->set_string_value("test");
  list_value.add_values()->set_bool_value(true);
  Arena arena;
  CelValue cel_value = CelProtoWrapper::CreateMessage(&list_value, &arena);
  Value value_out;
  EXPECT_OK(ExportAsProtoValue(cel_value, &value_out));
  EXPECT_THAT(list_value, testutil::EqualsProto(value_out.list_value()));
}

TEST(ValueExportUtilTest, ConvertRepeatedBoolValue) {
  Arena arena;
  Value value;

  TestMessage* msg = Arena::Create<TestMessage>(&arena);
  msg->add_bool_list(true);
  msg->add_bool_list(false);
  CelValue cel_value = CelProtoWrapper::CreateMessage(msg, &arena);

  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kStructValue);

  Value list_value = value.struct_value().fields().at("bool_list");

  EXPECT_TRUE(list_value.has_list_value());
  EXPECT_EQ(list_value.list_value().values(0).bool_value(), true);
  EXPECT_EQ(list_value.list_value().values(1).bool_value(), false);
}

TEST(ValueExportUtilTest, ConvertRepeatedInt32Value) {
  Arena arena;
  Value value;

  TestMessage* msg = Arena::Create<TestMessage>(&arena);
  msg->add_int32_list(2);
  msg->add_int32_list(3);
  CelValue cel_value = CelProtoWrapper::CreateMessage(msg, &arena);

  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kStructValue);

  Value list_value = value.struct_value().fields().at("int32_list");

  EXPECT_TRUE(list_value.has_list_value());
  EXPECT_DOUBLE_EQ(list_value.list_value().values(0).number_value(), 2);
  EXPECT_DOUBLE_EQ(list_value.list_value().values(1).number_value(), 3);
}

TEST(ValueExportUtilTest, ConvertRepeatedInt64Value) {
  Arena arena;
  Value value;

  TestMessage* msg = Arena::Create<TestMessage>(&arena);
  msg->add_int64_list(2);
  msg->add_int64_list(3);
  CelValue cel_value = CelProtoWrapper::CreateMessage(msg, &arena);

  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kStructValue);

  Value list_value = value.struct_value().fields().at("int64_list");

  EXPECT_TRUE(list_value.has_list_value());
  EXPECT_EQ(list_value.list_value().values(0).string_value(), "2");
  EXPECT_EQ(list_value.list_value().values(1).string_value(), "3");
}

TEST(ValueExportUtilTest, ConvertRepeatedUint64Value) {
  Arena arena;
  Value value;

  TestMessage* msg = Arena::Create<TestMessage>(&arena);
  msg->add_uint64_list(2);
  msg->add_uint64_list(3);
  CelValue cel_value = CelProtoWrapper::CreateMessage(msg, &arena);

  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kStructValue);

  Value list_value = value.struct_value().fields().at("uint64_list");

  EXPECT_TRUE(list_value.has_list_value());
  EXPECT_EQ(list_value.list_value().values(0).string_value(), "2");
  EXPECT_EQ(list_value.list_value().values(1).string_value(), "3");
}

TEST(ValueExportUtilTest, ConvertRepeatedDoubleValue) {
  Arena arena;
  Value value;

  TestMessage* msg = Arena::Create<TestMessage>(&arena);
  msg->add_double_list(2);
  msg->add_double_list(3);
  CelValue cel_value = CelProtoWrapper::CreateMessage(msg, &arena);

  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kStructValue);

  Value list_value = value.struct_value().fields().at("double_list");

  EXPECT_TRUE(list_value.has_list_value());
  EXPECT_DOUBLE_EQ(list_value.list_value().values(0).number_value(), 2);
  EXPECT_DOUBLE_EQ(list_value.list_value().values(1).number_value(), 3);
}

TEST(ValueExportUtilTest, ConvertRepeatedStringValue) {
  Arena arena;
  Value value;

  TestMessage* msg = Arena::Create<TestMessage>(&arena);
  msg->add_string_list("test1");
  msg->add_string_list("test2");
  CelValue cel_value = CelProtoWrapper::CreateMessage(msg, &arena);

  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kStructValue);

  Value list_value = value.struct_value().fields().at("string_list");

  EXPECT_TRUE(list_value.has_list_value());
  EXPECT_EQ(list_value.list_value().values(0).string_value(), "test1");
  EXPECT_EQ(list_value.list_value().values(1).string_value(), "test2");
}

TEST(ValueExportUtilTest, ConvertRepeatedBytesValue) {
  Arena arena;
  Value value;

  TestMessage* msg = Arena::Create<TestMessage>(&arena);
  msg->add_bytes_list("test1");
  msg->add_bytes_list("test2");
  CelValue cel_value = CelProtoWrapper::CreateMessage(msg, &arena);

  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kStructValue);

  Value list_value = value.struct_value().fields().at("bytes_list");

  EXPECT_TRUE(list_value.has_list_value());
  EXPECT_EQ(list_value.list_value().values(0).string_value(), "dGVzdDE=");
  EXPECT_EQ(list_value.list_value().values(1).string_value(), "dGVzdDI=");
}

TEST(ValueExportUtilTest, ConvertCelList) {
  Arena arena;
  Value value;

  std::vector<CelValue> values;
  values.push_back(CelValue::CreateInt64(2));
  values.push_back(CelValue::CreateInt64(3));
  CelList *cel_list = Arena::Create<ContainerBackedListImpl>(&arena, values);
  CelValue cel_value = CelValue::CreateList(cel_list);

  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kListValue);

  EXPECT_DOUBLE_EQ(value.list_value().values(0).number_value(), 2);
  EXPECT_DOUBLE_EQ(value.list_value().values(1).number_value(), 3);
}

TEST(ValueExportUtilTest, ConvertCelMapWithStringKey) {
  Value value;
  std::vector<std::pair<CelValue, CelValue>> map_entries;

  std::string key1 = "key1";
  std::string key2 = "key2";
  std::string value1 = "value1";
  std::string value2 = "value2";

  map_entries.push_back(
      {CelValue::CreateString(&key1), CelValue::CreateString(&value1)});
  map_entries.push_back(
      {CelValue::CreateString(&key2), CelValue::CreateString(&value2)});

  auto cel_map = CreateContainerBackedMap(
                     absl::Span<std::pair<CelValue, CelValue>>(map_entries))
                     .value();
  CelValue cel_value = CelValue::CreateMap(cel_map.get());

  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kStructValue);

  const auto& fields = value.struct_value().fields();

  EXPECT_EQ(fields.at(key1).string_value(), value1);
  EXPECT_EQ(fields.at(key2).string_value(), value2);
}

TEST(ValueExportUtilTest, ConvertCelMapWithInt64Key) {
  Value value;
  std::vector<std::pair<CelValue, CelValue>> map_entries;

  int key1 = -1;
  int key2 = 2;
  std::string value1 = "value1";
  std::string value2 = "value2";

  map_entries.push_back(
      {CelValue::CreateInt64(key1), CelValue::CreateString(&value1)});
  map_entries.push_back(
      {CelValue::CreateInt64(key2), CelValue::CreateString(&value2)});

  auto cel_map = CreateContainerBackedMap(
                     absl::Span<std::pair<CelValue, CelValue>>(map_entries))
                     .value();
  CelValue cel_value = CelValue::CreateMap(cel_map.get());

  EXPECT_OK(ExportAsProtoValue(cel_value, &value));
  EXPECT_EQ(value.kind_case(), Value::KindCase::kStructValue);

  const auto& fields = value.struct_value().fields();

  EXPECT_EQ(fields.at(absl::StrCat(key1)).string_value(), value1);
  EXPECT_EQ(fields.at(absl::StrCat(key2)).string_value(), value2);
}

}  // namespace

}  // namespace google::api::expr::runtime
