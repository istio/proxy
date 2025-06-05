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
//
// Functional tests for protobuf backed CEL structs in the default runtime.

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "extensions/protobuf/runtime_adapter.h"
#include "extensions/protobuf/value.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "runtime/activation.h"
#include "runtime/runtime.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/text_format.h"

namespace cel::extensions {
namespace {

using ::absl_testing::StatusIs;
using ::cel::test::BoolValueIs;
using ::cel::test::BytesValueIs;
using ::cel::test::DoubleValueIs;
using ::cel::test::DurationValueIs;
using ::cel::test::ErrorValueIs;
using ::cel::test::IntValueIs;
using ::cel::test::IsNullValue;
using ::cel::test::ListValueIs;
using ::cel::test::MapValueIs;
using ::cel::test::StringValueIs;
using ::cel::test::StructValueIs;
using ::cel::test::TimestampValueIs;
using ::cel::test::UintValueIs;
using ::cel::test::ValueMatcher;
using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::google::api::expr::test::v1::proto3::TestAllTypes;
using ::testing::_;
using ::testing::AnyOf;
using ::testing::HasSubstr;

struct TestCase {
  std::string name;
  std::string expr;
  std::string msg_textproto;
  ValueMatcher matcher;
};

std::ostream& operator<<(std::ostream& out, const TestCase& tc) {
  return out << tc.name;
}

class ProtobufValueEndToEndTest
    : public common_internal::ThreadCompatibleValueTest<TestCase> {
 public:
  ProtobufValueEndToEndTest() = default;

 protected:
  const TestCase& test_case() const { return std::get<1>(GetParam()); }
};

TEST_P(ProtobufValueEndToEndTest, Runner) {
  TestAllTypes message;

  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(test_case().msg_textproto, &message));

  ASSERT_OK_AND_ASSIGN(Value value,
                       ProtoMessageToValue(value_manager(), message));

  Activation activation;
  activation.InsertOrAssignValue("msg", std::move(value));

  RuntimeOptions opts;
  opts.enable_empty_wrapper_null_unboxing = true;
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(
                           google::protobuf::DescriptorPool::generated_pool(), opts));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const Runtime> runtime,
                       std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, Parse(test_case().expr));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TraceableProgram> program,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));

  ASSERT_OK_AND_ASSIGN(Value result,
                       program->Evaluate(activation, value_manager()));

  EXPECT_THAT(result, test_case().matcher);
}

INSTANTIATE_TEST_SUITE_P(
    Singular, ProtobufValueEndToEndTest,
    testing::Combine(
        testing::Values(MemoryManagement::kPooling,
                        MemoryManagement::kReferenceCounting),
        testing::ValuesIn(std::vector<TestCase>{
            {"single_int64", "msg.single_int64",
             R"pb(
               single_int64: 42
             )pb",
             IntValueIs(42)},
            {"single_int64_has", "has(msg.single_int64)",
             R"pb(
               single_int64: 42
             )pb",
             BoolValueIs(true)},
            {"single_int64_has_false", "has(msg.single_int64)", "",
             BoolValueIs(false)},
            {"single_int32", "msg.single_int32",
             R"pb(
               single_int32: 42
             )pb",
             IntValueIs(42)},
            {"single_uint64", "msg.single_uint64",
             R"pb(
               single_uint64: 42
             )pb",
             UintValueIs(42)},
            {"single_uint32", "msg.single_uint32",
             R"pb(
               single_uint32: 42
             )pb",
             UintValueIs(42)},
            {"single_sint64", "msg.single_sint64",
             R"pb(
               single_sint64: 42
             )pb",
             IntValueIs(42)},
            {"single_sint32", "msg.single_sint32",
             R"pb(
               single_sint32: 42
             )pb",
             IntValueIs(42)},
            {"single_fixed64", "msg.single_fixed64",
             R"pb(
               single_fixed64: 42
             )pb",
             UintValueIs(42)},
            {"single_fixed32", "msg.single_fixed32",
             R"pb(
               single_fixed32: 42
             )pb",
             UintValueIs(42)},
            {"single_sfixed64", "msg.single_sfixed64",
             R"pb(
               single_sfixed64: 42
             )pb",
             IntValueIs(42)},
            {"single_sfixed32", "msg.single_sfixed32",
             R"pb(
               single_sfixed32: 42
             )pb",
             IntValueIs(42)},
            {"single_float", "msg.single_float",
             R"pb(
               single_float: 4.25
             )pb",
             DoubleValueIs(4.25)},
            {"single_double", "msg.single_double",
             R"pb(
               single_double: 4.25
             )pb",
             DoubleValueIs(4.25)},
            {"single_bool", "msg.single_bool",
             R"pb(
               single_bool: true
             )pb",
             BoolValueIs(true)},
            {"single_string", "msg.single_string",
             R"pb(
               single_string: "Hello 😀"
             )pb",
             StringValueIs("Hello 😀")},
            {"single_bytes", "msg.single_bytes",
             R"pb(
               single_bytes: "Hello"
             )pb",
             BytesValueIs("Hello")},
            {"wkt_duration", "msg.single_duration",
             R"pb(
               single_duration { seconds: 10 }
             )pb",
             DurationValueIs(absl::Seconds(10))},
            {"wkt_duration_default", "msg.single_duration", "",
             DurationValueIs(absl::Seconds(0))},
            {"wkt_timestamp", "msg.single_timestamp",
             R"pb(
               single_timestamp { seconds: 10 }
             )pb",
             TimestampValueIs(absl::FromUnixSeconds(10))},
            {"wkt_timestamp_default", "msg.single_timestamp", "",
             TimestampValueIs(absl::UnixEpoch())},
            {"wkt_int64", "msg.single_int64_wrapper",
             R"pb(
               single_int64_wrapper { value: -20 }
             )pb",
             IntValueIs(-20)},
            {"wkt_int64_default", "msg.single_int64_wrapper", "",
             IsNullValue()},
            {"wkt_int32", "msg.single_int32_wrapper",
             R"pb(
               single_int32_wrapper { value: -10 }
             )pb",
             IntValueIs(-10)},
            {"wkt_int32_default", "msg.single_int32_wrapper", "",
             IsNullValue()},
            {"wkt_uint64", "msg.single_uint64_wrapper",
             R"pb(
               single_uint64_wrapper { value: 10 }
             )pb",
             UintValueIs(10)},
            {"wkt_uint64_default", "msg.single_uint64_wrapper", "",
             IsNullValue()},
            {"wkt_uint32", "msg.single_uint32_wrapper",
             R"pb(
               single_uint32_wrapper { value: 11 }
             )pb",
             UintValueIs(11)},
            {"wkt_uint32_default", "msg.single_uint32_wrapper", "",
             IsNullValue()},
            {"wkt_float", "msg.single_float_wrapper",
             R"pb(
               single_float_wrapper { value: 10.25 }
             )pb",
             DoubleValueIs(10.25)},
            {"wkt_float_default", "msg.single_float_wrapper", "",
             IsNullValue()},
            {"wkt_double", "msg.single_double_wrapper",
             R"pb(
               single_double_wrapper { value: 10.25 }
             )pb",
             DoubleValueIs(10.25)},
            {"wkt_double_default", "msg.single_double_wrapper", "",
             IsNullValue()},
            {"wkt_bool", "msg.single_bool_wrapper",
             R"pb(
               single_bool_wrapper { value: false }
             )pb",
             BoolValueIs(false)},
            {"wkt_bool_default", "msg.single_bool_wrapper", "", IsNullValue()},
            {"wkt_string", "msg.single_string_wrapper",
             R"pb(
               single_string_wrapper { value: "abcd" }
             )pb",
             StringValueIs("abcd")},
            {"wkt_string_default", "msg.single_string_wrapper", "",
             IsNullValue()},
            {"wkt_bytes", "msg.single_bytes_wrapper",
             R"pb(
               single_bytes_wrapper { value: "abcd" }
             )pb",
             BytesValueIs("abcd")},
            {"wkt_bytes_default", "msg.single_bytes_wrapper", "",
             IsNullValue()},
            {"wkt_null", "msg.null_value",
             R"pb(
               null_value: NULL_VALUE
             )pb",
             IsNullValue()},
            {"message_field", "msg.standalone_message",
             R"pb(
               standalone_message { bb: 2 }
             )pb",
             StructValueIs(_)},
            {"message_field_has", "has(msg.standalone_message)",
             R"pb(
               standalone_message { bb: 2 }
             )pb",
             BoolValueIs(true)},
            {"message_field_has_false", "has(msg.standalone_message)", "",
             BoolValueIs(false)},
            {"single_enum", "msg.standalone_enum",
             R"pb(
               standalone_enum: BAR
             )pb",
             // BAR
             IntValueIs(1)}})),
    ProtobufValueEndToEndTest::ToString);

INSTANTIATE_TEST_SUITE_P(
    Repeated, ProtobufValueEndToEndTest,
    testing::Combine(
        testing::Values(MemoryManagement::kPooling,
                        MemoryManagement::kReferenceCounting),
        testing::ValuesIn(std::vector<TestCase>{
            {"repeated_int64", "msg.repeated_int64[0]",
             R"pb(
               repeated_int64: 42
             )pb",
             IntValueIs(42)},
            {"repeated_int64_has", "has(msg.repeated_int64)",
             R"pb(
               repeated_int64: 42
             )pb",
             BoolValueIs(true)},
            {"repeated_int64_has_false", "has(msg.repeated_int64)", "",
             BoolValueIs(false)},
            {"repeated_int32", "msg.repeated_int32[0]",
             R"pb(
               repeated_int32: 42
             )pb",
             IntValueIs(42)},
            {"repeated_uint64", "msg.repeated_uint64[0]",
             R"pb(
               repeated_uint64: 42
             )pb",
             UintValueIs(42)},
            {"repeated_uint32", "msg.repeated_uint32[0]",
             R"pb(
               repeated_uint32: 42
             )pb",
             UintValueIs(42)},
            {"repeated_sint64", "msg.repeated_sint64[0]",
             R"pb(
               repeated_sint64: 42
             )pb",
             IntValueIs(42)},
            {"repeated_sint32", "msg.repeated_sint32[0]",
             R"pb(
               repeated_sint32: 42
             )pb",
             IntValueIs(42)},
            {"repeated_fixed64", "msg.repeated_fixed64[0]",
             R"pb(
               repeated_fixed64: 42
             )pb",
             UintValueIs(42)},
            {"repeated_fixed32", "msg.repeated_fixed32[0]",
             R"pb(
               repeated_fixed32: 42
             )pb",
             UintValueIs(42)},
            {"repeated_sfixed64", "msg.repeated_sfixed64[0]",
             R"pb(
               repeated_sfixed64: 42
             )pb",
             IntValueIs(42)},
            {"repeated_sfixed32", "msg.repeated_sfixed32[0]",
             R"pb(
               repeated_sfixed32: 42
             )pb",
             IntValueIs(42)},
            {"repeated_float", "msg.repeated_float[0]",
             R"pb(
               repeated_float: 4.25
             )pb",
             DoubleValueIs(4.25)},
            {"repeated_double", "msg.repeated_double[0]",
             R"pb(
               repeated_double: 4.25
             )pb",
             DoubleValueIs(4.25)},
            {"repeated_bool", "msg.repeated_bool[0]",
             R"pb(
               repeated_bool: true
             )pb",
             BoolValueIs(true)},
            {"repeated_string", "msg.repeated_string[0]",
             R"pb(
               repeated_string: "Hello 😀"
             )pb",
             StringValueIs("Hello 😀")},
            {"repeated_bytes", "msg.repeated_bytes[0]",
             R"pb(
               repeated_bytes: "Hello"
             )pb",
             BytesValueIs("Hello")},
            {"wkt_duration", "msg.repeated_duration[0]",
             R"pb(
               repeated_duration { seconds: 10 }
             )pb",
             DurationValueIs(absl::Seconds(10))},
            {"wkt_timestamp", "msg.repeated_timestamp[0]",
             R"pb(
               repeated_timestamp { seconds: 10 }
             )pb",
             TimestampValueIs(absl::FromUnixSeconds(10))},
            {"wkt_int64", "msg.repeated_int64_wrapper[0]",
             R"pb(
               repeated_int64_wrapper { value: -20 }
             )pb",
             IntValueIs(-20)},
            {"wkt_int32", "msg.repeated_int32_wrapper[0]",
             R"pb(
               repeated_int32_wrapper { value: -10 }
             )pb",
             IntValueIs(-10)},
            {"wkt_uint64", "msg.repeated_uint64_wrapper[0]",
             R"pb(
               repeated_uint64_wrapper { value: 10 }
             )pb",
             UintValueIs(10)},
            {"wkt_uint32", "msg.repeated_uint32_wrapper[0]",
             R"pb(
               repeated_uint32_wrapper { value: 11 }
             )pb",
             UintValueIs(11)},
            {"wkt_float", "msg.repeated_float_wrapper[0]",
             R"pb(
               repeated_float_wrapper { value: 10.25 }
             )pb",
             DoubleValueIs(10.25)},
            {"wkt_double", "msg.repeated_double_wrapper[0]",
             R"pb(
               repeated_double_wrapper { value: 10.25 }
             )pb",
             DoubleValueIs(10.25)},
            {"wkt_bool", "msg.repeated_bool_wrapper[0]",
             R"pb(

               repeated_bool_wrapper { value: false }
             )pb",
             BoolValueIs(false)},
            {"wkt_string", "msg.repeated_string_wrapper[0]",
             R"pb(
               repeated_string_wrapper { value: "abcd" }
             )pb",
             StringValueIs("abcd")},
            {"wkt_bytes", "msg.repeated_bytes_wrapper[0]",
             R"pb(
               repeated_bytes_wrapper { value: "abcd" }
             )pb",
             BytesValueIs("abcd")},
            {"wkt_null", "msg.repeated_null_value[0]",
             R"pb(
               repeated_null_value: NULL_VALUE
             )pb",
             IsNullValue()},
            {"message_field", "msg.repeated_nested_message[0]",
             R"pb(
               repeated_nested_message { bb: 42 }
             )pb",
             StructValueIs(_)},
            {"repeated_enum", "msg.repeated_nested_enum[0]",
             R"pb(
               repeated_nested_enum: BAR
             )pb",
             // BAR
             IntValueIs(1)},
            // Implements CEL list interface
            {"repeated_size", "msg.repeated_int64.size()",
             R"pb(
               repeated_int64: 42 repeated_int64: 43
             )pb",
             IntValueIs(2)},
            {"in_repeated", "42 in msg.repeated_int64",
             R"pb(
               repeated_int64: 42 repeated_int64: 43
             )pb",
             BoolValueIs(true)},
            {"in_repeated_false", "44 in msg.repeated_int64",
             R"pb(
               repeated_int64: 42 repeated_int64: 43
             )pb",
             BoolValueIs(false)},
            {"repeated_compre_exists", "msg.repeated_int64.exists(x, x > 42)",
             R"pb(
               repeated_int64: 42 repeated_int64: 43
             )pb",
             BoolValueIs(true)},
            {"repeated_compre_map", "msg.repeated_int64.map(x, x * 2)[0]",
             R"pb(
               repeated_int64: 42 repeated_int64: 43
             )pb",
             IntValueIs(84)},
        })),
    ProtobufValueEndToEndTest::ToString);

INSTANTIATE_TEST_SUITE_P(
    Maps, ProtobufValueEndToEndTest,
    testing::Combine(
        testing::Values(MemoryManagement::kPooling,
                        MemoryManagement::kReferenceCounting),
        testing::ValuesIn(std::vector<TestCase>{
            {"map_bool_int64", "msg.map_bool_int64[false]",
             R"pb(
               map_bool_int64 { key: false value: 42 }
             )pb",
             IntValueIs(42)},
            {"map_bool_int64_has", "has(msg.map_bool_int64)",
             R"pb(
               map_bool_int64 { key: false value: 42 }
             )pb",
             BoolValueIs(true)},
            {"map_bool_int64_has_false", "has(msg.map_bool_int64)", "",
             BoolValueIs(false)},
            {"map_bool_int32", "msg.map_bool_int32[false]",
             R"pb(
               map_bool_int32 { key: false value: 42 }
             )pb",
             IntValueIs(42)},
            {"map_bool_uint64", "msg.map_bool_uint64[false]",
             R"pb(
               map_bool_uint64 { key: false value: 42 }
             )pb",
             UintValueIs(42)},
            {"map_bool_uint32", "msg.map_bool_uint32[false]",
             R"pb(
               map_bool_uint32 { key: false, value: 42 }
             )pb",
             UintValueIs(42)},
            {"map_bool_float", "msg.map_bool_float[false]",
             R"pb(
               map_bool_float { key: false value: 4.25 }
             )pb",
             DoubleValueIs(4.25)},
            {"map_bool_double", "msg.map_bool_double[false]",
             R"pb(
               map_bool_double { key: false value: 4.25 }
             )pb",
             DoubleValueIs(4.25)},
            {"map_bool_bool", "msg.map_bool_bool[false]",
             R"pb(
               map_bool_bool { key: false value: true }
             )pb",
             BoolValueIs(true)},
            {"map_bool_string", "msg.map_bool_string[false]",
             R"pb(
               map_bool_string { key: false value: "Hello 😀" }
             )pb",
             StringValueIs("Hello 😀")},
            {"map_bool_bytes", "msg.map_bool_bytes[false]",
             R"pb(
               map_bool_bytes { key: false value: "Hello" }
             )pb",
             BytesValueIs("Hello")},
            {"wkt_duration", "msg.map_bool_duration[false]",
             R"pb(
               map_bool_duration {
                 key: false
                 value { seconds: 10 }
               }
             )pb",
             DurationValueIs(absl::Seconds(10))},
            {"wkt_timestamp", "msg.map_bool_timestamp[false]",
             R"pb(
               map_bool_timestamp {
                 key: false
                 value { seconds: 10 }
               }
             )pb",
             TimestampValueIs(absl::FromUnixSeconds(10))},
            {"wkt_int64", "msg.map_bool_int64_wrapper[false]",
             R"pb(
               map_bool_int64_wrapper {
                 key: false
                 value { value: -20 }
               }
             )pb",
             IntValueIs(-20)},
            {"wkt_int32", "msg.map_bool_int32_wrapper[false]",
             R"pb(
               map_bool_int32_wrapper {
                 key: false
                 value { value: -10 }
               }
             )pb",
             IntValueIs(-10)},
            {"wkt_uint64", "msg.map_bool_uint64_wrapper[false]",
             R"pb(
               map_bool_uint64_wrapper {
                 key: false
                 value { value: 10 }
               }
             )pb",
             UintValueIs(10)},
            {"wkt_uint32", "msg.map_bool_uint32_wrapper[false]",
             R"pb(
               map_bool_uint32_wrapper {
                 key: false
                 value { value: 11 }
               }
             )pb",
             UintValueIs(11)},
            {"wkt_float", "msg.map_bool_float_wrapper[false]",
             R"pb(
               map_bool_float_wrapper {
                 key: false
                 value { value: 10.25 }
               }
             )pb",
             DoubleValueIs(10.25)},
            {"wkt_double", "msg.map_bool_double_wrapper[false]",
             R"pb(
               map_bool_double_wrapper {
                 key: false
                 value { value: 10.25 }
               }
             )pb",
             DoubleValueIs(10.25)},
            {"wkt_bool", "msg.map_bool_bool_wrapper[false]",
             R"pb(
               map_bool_bool_wrapper {
                 key: false
                 value { value: false }
               }
             )pb",
             BoolValueIs(false)},
            {"wkt_string", "msg.map_bool_string_wrapper[false]",
             R"pb(
               map_bool_string_wrapper {
                 key: false
                 value { value: "abcd" }
               }
             )pb",
             StringValueIs("abcd")},
            {"wkt_bytes", "msg.map_bool_bytes_wrapper[false]",
             R"pb(
               map_bool_bytes_wrapper {
                 key: false
                 value { value: "abcd" }
               }
             )pb",
             BytesValueIs("abcd")},
            {"wkt_null", "msg.map_bool_null_value[false]",
             R"pb(
               map_bool_null_value { key: false value: NULL_VALUE }
             )pb",
             IsNullValue()},
            {"message_field", "msg.map_bool_message[false]",
             R"pb(
               map_bool_message {
                 key: false
                 value { bb: 42 }
               }
             )pb",
             StructValueIs(_)},
            {"map_bool_enum", "msg.map_bool_enum[false]",
             R"pb(
               map_bool_enum { key: false value: BAR }
             )pb",
             // BAR
             IntValueIs(1)},
            // Simplified for remaining key types.
            {"map_int32_int64", "msg.map_int32_int64[42]",
             R"pb(
               map_int32_int64 { key: 42 value: -42 }
             )pb",
             IntValueIs(-42)},
            {"map_int64_int64", "msg.map_int64_int64[42]",
             R"pb(
               map_int64_int64 { key: 42 value: -42 }
             )pb",
             IntValueIs(-42)},
            {"map_uint32_int64", "msg.map_uint32_int64[42u]",
             R"pb(
               map_uint32_int64 { key: 42 value: -42 }
             )pb",
             IntValueIs(-42)},
            {"map_uint64_int64", "msg.map_uint64_int64[42u]",
             R"pb(
               map_uint64_int64 { key: 42 value: -42 }
             )pb",
             IntValueIs(-42)},
            {"map_string_int64", "msg.map_string_int64['key1']",
             R"pb(
               map_string_int64 { key: "key1" value: -42 }
             )pb",
             IntValueIs(-42)},
            // Implements CEL map
            {"in_map_int64_true", "42 in msg.map_int64_int64",
             R"pb(
               map_int64_int64 { key: 42 value: -42 }
               map_int64_int64 { key: 43 value: -43 }
             )pb",
             BoolValueIs(true)},
            {"in_map_int64_false", "44 in msg.map_int64_int64",
             R"pb(
               map_int64_int64 { key: 42 value: -42 }
               map_int64_int64 { key: 43 value: -43 }
             )pb",
             BoolValueIs(false)},
            {"int_map_int64_compre_exists",
             "msg.map_int64_int64.exists(key, key > 42)",
             R"pb(
               map_int64_int64 { key: 42 value: -42 }
               map_int64_int64 { key: 43 value: -43 }
             )pb",
             BoolValueIs(true)},
            {"int_map_int64_compre_map",
             "msg.map_int64_int64.map(key, key + 20)[0]",
             R"pb(
               map_int64_int64 { key: 42 value: -42 }
               map_int64_int64 { key: 43 value: -43 }
             )pb",

             IntValueIs(AnyOf(62, 63))},
            {"map_string_key_not_found", "msg.map_string_int64['key2']",
             R"pb(
               map_string_int64 { key: "key1" value: -42 }
             )pb",
             ErrorValueIs(StatusIs(absl::StatusCode::kNotFound,
                                   HasSubstr("Key not found in map")))},
            {"map_string_select_key", "msg.map_string_int64.key1",
             R"pb(
               map_string_int64 { key: "key1" value: -42 }
             )pb",
             IntValueIs(-42)},
            {"map_string_has_key", "has(msg.map_string_int64.key1)",
             R"pb(
               map_string_int64 { key: "key1" value: -42 }
             )pb",
             BoolValueIs(true)},
            {"map_string_has_key_false", "has(msg.map_string_int64.key2)",
             R"pb(
               map_string_int64 { key: "key1" value: -42 }
             )pb",
             BoolValueIs(false)},
            {"map_int32_out_of_range", "msg.map_int32_int64[0x1FFFFFFFF]",
             R"pb(
               map_int32_int64 { key: 10 value: -42 }
             )pb",
             ErrorValueIs(StatusIs(absl::StatusCode::kNotFound,
                                   HasSubstr("Key not found in map")))},
            {"map_uint32_out_of_range", "msg.map_uint32_int64[0x1FFFFFFFFu]",
             R"pb(
               map_uint32_int64 { key: 10 value: -42 }
             )pb",
             ErrorValueIs(StatusIs(absl::StatusCode::kNotFound,
                                   HasSubstr("Key not found in map")))}})),
    ProtobufValueEndToEndTest::ToString);

MATCHER_P(CelSizeIs, size, "") {
  auto s = arg.Size();
  return s.ok() && *s == size;
}

INSTANTIATE_TEST_SUITE_P(
    JsonWrappers, ProtobufValueEndToEndTest,
    testing::Combine(
        testing::Values(MemoryManagement::kPooling,
                        MemoryManagement::kReferenceCounting),
        testing::ValuesIn(std::vector<TestCase>{
            {"single_struct", "msg.single_struct",
             R"pb(
               single_struct {
                 fields {
                   key: "field1"
                   value { null_value: NULL_VALUE }
                 }
               }
             )pb",
             MapValueIs(CelSizeIs(1))},
            {"single_struct_null_value_field", "msg.single_struct['field1']",
             R"pb(
               single_struct {
                 fields {
                   key: "field1"
                   value { null_value: NULL_VALUE }
                 }
               }
             )pb",
             IsNullValue()},
            {"single_struct_number_value_field", "msg.single_struct['field1']",
             R"pb(
               single_struct {
                 fields {
                   key: "field1"
                   value { number_value: 10.25 }
                 }
               }
             )pb",
             DoubleValueIs(10.25)},
            {"single_struct_bool_value_field", "msg.single_struct['field1']",
             R"pb(
               single_struct {
                 fields {
                   key: "field1"
                   value { bool_value: true }
                 }
               }
             )pb",
             BoolValueIs(true)},
            {"single_struct_string_value_field", "msg.single_struct['field1']",
             R"pb(
               single_struct {
                 fields {
                   key: "field1"
                   value { string_value: "abcd" }
                 }
               }
             )pb",
             StringValueIs("abcd")},
            {"single_struct_struct_value_field", "msg.single_struct['field1']",
             R"pb(
               single_struct {
                 fields {
                   key: "field1"
                   value {
                     struct_value {
                       fields {
                         key: "field2",
                         value: { null_value: NULL_VALUE }
                       }
                     }
                   }
                 }
               }
             )pb",
             MapValueIs(CelSizeIs(1))},
            {"single_struct_list_value_field", "msg.single_struct['field1']",
             R"pb(
               single_struct {
                 fields {
                   key: "field1"
                   value { list_value { values { null_value: NULL_VALUE } } }
                 }
               }
             )pb",
             ListValueIs(CelSizeIs(1))},
            {"single_struct_select_field", "msg.single_struct.field1",
             R"pb(
               single_struct {
                 fields {
                   key: "field1"
                   value { bool_value: true }
                 }
               }
             )pb",
             BoolValueIs(true)},
            {"single_struct_has_field", "has(msg.single_struct.field1)",
             R"pb(
               single_struct {
                 fields {
                   key: "field1"
                   value { bool_value: true }
                 }
               }
             )pb",
             BoolValueIs(true)},
            {"single_struct_has_field_false", "has(msg.single_struct.field2)",
             R"pb(
               single_struct {
                 fields {
                   key: "field1"
                   value { bool_value: true }
                 }
               }
             )pb",
             BoolValueIs(false)},
            {"single_struct_map_size", "msg.single_struct.size()",
             R"pb(
               single_struct {
                 fields {
                   key: "field1"
                   value { bool_value: true }
                 }
                 fields {
                   key: "field2"
                   value { bool_value: false }
                 }
               }
             )pb",
             IntValueIs(2)},
            {"single_struct_map_in", "'field2' in msg.single_struct",
             R"pb(
               single_struct {
                 fields {
                   key: "field1"
                   value { bool_value: true }
                 }
                 fields {
                   key: "field2"
                   value { bool_value: false }
                 }
               }
             )pb",
             BoolValueIs(true)},
            {"single_struct_map_compre_exists",
             "msg.single_struct.exists(key, key == 'field2')",
             R"pb(
               single_struct {
                 fields {
                   key: "field1"
                   value { bool_value: true }
                 }
                 fields {
                   key: "field2"
                   value { bool_value: false }
                 }
               }
             )pb",
             BoolValueIs(true)},
            {"single_struct_map_compre_map",
             "'__field1' in msg.single_struct.map(key, '__' + key)",
             R"pb(
               single_struct {
                 fields {
                   key: "field1"
                   value { bool_value: true }
                 }
                 fields {
                   key: "field2"
                   value { bool_value: false }
                 }
               }
             )pb",
             BoolValueIs(true)},
            {"single_list_value", "msg.list_value",
             R"pb(
               list_value { values { null_value: NULL_VALUE } }
             )pb",
             ListValueIs(CelSizeIs(1))},
            {"single_list_value_index_null", "msg.list_value[0]",
             R"pb(
               list_value { values { null_value: NULL_VALUE } }
             )pb",
             IsNullValue()},
            {"single_list_value_index_number", "msg.list_value[0]",
             R"pb(
               list_value { values { number_value: 10.25 } }
             )pb",
             DoubleValueIs(10.25)},
            {"single_list_value_index_string", "msg.list_value[0]",
             R"pb(
               list_value { values { string_value: "abc" } }
             )pb",
             StringValueIs("abc")},
            {"single_list_value_index_bool", "msg.list_value[0]",
             R"pb(
               list_value { values { bool_value: false } }
             )pb",
             BoolValueIs(false)},
            {"single_list_value_list_size", "msg.list_value.size()",
             R"pb(
               list_value {
                 values { bool_value: false }
                 values { bool_value: false }
               }
             )pb",
             IntValueIs(2)},
            {"single_list_value_list_in", "10.25 in msg.list_value",
             R"pb(
               list_value {
                 values { number_value: 10.0 }
                 values { number_value: 10.25 }
               }
             )pb",
             BoolValueIs(true)},
            {"single_list_value_list_compre_exists",
             "msg.list_value.exists(x, x == 10.25)",
             R"pb(
               list_value {
                 values { number_value: 10.0 }
                 values { number_value: 10.25 }
               }
             )pb",
             BoolValueIs(true)},
            {"single_list_value_list_compre_map",
             "msg.list_value.map(x, x + 0.5)[1]",
             R"pb(
               list_value {
                 values { number_value: 10.0 }
                 values { number_value: 10.25 }
               }
             )pb",
             DoubleValueIs(10.75)},
            {"single_list_value_index_struct", "msg.list_value[0]",
             R"pb(
               list_value {
                 values {
                   struct_value {
                     fields {
                       key: "field1"
                       value { null_value: NULL_VALUE }
                     }
                   }
                 }
               }
             )pb",
             MapValueIs(CelSizeIs(1))},
            {"single_list_value_index_list", "msg.list_value[0]",
             R"pb(
               list_value {
                 values { list_value { values { null_value: NULL_VALUE } } }
               }
             )pb",
             ListValueIs(CelSizeIs(1))},
            {"single_json_value_null", "msg.single_value",
             R"pb(
               single_value { null_value: NULL_VALUE }
             )pb",
             IsNullValue()},
            {"single_json_value_number", "msg.single_value",
             R"pb(
               single_value { number_value: 13.25 }
             )pb",
             DoubleValueIs(13.25)},
            {"single_json_value_string", "msg.single_value",
             R"pb(
               single_value { string_value: "abcd" }
             )pb",
             StringValueIs("abcd")},
            {"single_json_value_bool", "msg.single_value",
             R"pb(
               single_value { bool_value: false }
             )pb",
             BoolValueIs(false)},
            {"single_json_value_struct", "msg.single_value",
             R"pb(
               single_value { struct_value {} }
             )pb",
             MapValueIs(CelSizeIs(0))},
            {"single_json_value_list", "msg.single_value",
             R"pb(
               single_value { list_value {} }
             )pb",
             ListValueIs(CelSizeIs(0))},
        })),
    ProtobufValueEndToEndTest::ToString);

// TODO: any support needs the reflection impl for looking up the
// type name and corresponding deserializer (outside of the WKTs which are
// special cased).
INSTANTIATE_TEST_SUITE_P(
    Any, ProtobufValueEndToEndTest,
    testing::Combine(
        testing::Values(MemoryManagement::kPooling,
                        MemoryManagement::kReferenceCounting),
        testing::ValuesIn(std::vector<TestCase>{
            {"single_any_wkt_int64", "msg.single_any",
             R"pb(
               single_any {
                 [type.googleapis.com/google.protobuf.Int64Value] { value: 42 }
               }
             )pb",
             IntValueIs(42)},
            {"single_any_wkt_int32", "msg.single_any",
             R"pb(
               single_any {
                 [type.googleapis.com/google.protobuf.Int32Value] { value: 42 }
               }
             )pb",
             IntValueIs(42)},
            {"single_any_wkt_uint64", "msg.single_any",
             R"pb(
               single_any {
                 [type.googleapis.com/google.protobuf.UInt64Value] { value: 42 }
               }
             )pb",
             UintValueIs(42)},
            {"single_any_wkt_uint32", "msg.single_any",
             R"pb(
               single_any {
                 [type.googleapis.com/google.protobuf.UInt32Value] { value: 42 }
               }
             )pb",
             UintValueIs(42)},
            {"single_any_wkt_double", "msg.single_any",
             R"pb(
               single_any {
                 [type.googleapis.com/google.protobuf.DoubleValue] {
                   value: 30.5
                 }
               }
             )pb",
             DoubleValueIs(30.5)},
            {"single_any_wkt_string", "msg.single_any",
             R"pb(
               single_any {
                 [type.googleapis.com/google.protobuf.StringValue] {
                   value: "abcd"
                 }
               }
             )pb",
             StringValueIs("abcd")},

            {"repeated_any_wkt_string", "msg.repeated_any[0]",
             R"pb(
               repeated_any {
                 [type.googleapis.com/google.protobuf.StringValue] {
                   value: "abcd"
                 }
               }
             )pb",
             StringValueIs("abcd")},
            {"map_int64_any_wkt_string", "msg.map_int64_any[0]",
             R"pb(
               map_int64_any {
                 key: 0
                 value {
                   [type.googleapis.com/google.protobuf.StringValue] {
                     value: "abcd"
                   }
                 }
               }
             )pb",
             StringValueIs("abcd")},
        })),
    ProtobufValueEndToEndTest::ToString);

}  // namespace
}  // namespace cel::extensions
