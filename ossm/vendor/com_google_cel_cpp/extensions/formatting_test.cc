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

#include "extensions/formatting.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "cel/expr/syntax.pb.h"
#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "common/value.h"
#include "extensions/protobuf/runtime_adapter.h"
#include "internal/parse_text_proto.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "parser/options.h"
#include "parser/parser.h"
#include "runtime/activation.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "cel/expr/conformance/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {
namespace {

using ::absl_testing::IsOk;
using ::cel::expr::conformance::proto3::TestAllTypes;
using ::cel::expr::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::google::api::expr::parser::ParserOptions;
using ::testing::HasSubstr;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

struct FormattingTestCase {
  std::string name;
  std::string format;
  std::string format_args;
  absl::flat_hash_map<std::string,
                      std::variant<std::string, bool, int64_t, uint64_t, int,
                                   double, absl::Duration, absl::Time, Value>>
      dyn_args;
  std::string expected;
  std::optional<std::string> error = std::nullopt;
};

google::protobuf::Arena* GetTestArena() {
  static absl::NoDestructor<google::protobuf::Arena> arena;
  return &*arena;
}

template <typename T>
ParsedMessageValue MakeMessage(absl::string_view text) {
  return ParsedMessageValue(
      internal::DynamicParseTextProto<T>(GetTestArena(), text,
                                         internal::GetTestingDescriptorPool(),
                                         internal::GetTestingMessageFactory()),
      GetTestArena());
}

using StringFormatTest = TestWithParam<FormattingTestCase>;
TEST_P(StringFormatTest, TestStringFormatting) {
  const FormattingTestCase& test_case = GetParam();
  google::protobuf::Arena arena;
  const RuntimeOptions options;
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));
  auto registration_status =
      RegisterStringFormattingFunctions(builder.function_registry(), options);
  if (test_case.error.has_value() && !registration_status.ok()) {
    EXPECT_THAT(registration_status.message(), HasSubstr(*test_case.error));
    return;
  } else {
    ASSERT_THAT(registration_status, IsOk());
  }
  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  auto expr_str = absl::StrFormat("'''%s'''.format([%s])", test_case.format,
                                  test_case.format_args);
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr,
                       Parse(expr_str, "<input>", ParserOptions{}));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));

  Activation activation;
  for (const auto& [name, value] : test_case.dyn_args) {
    if (std::holds_alternative<std::string>(value)) {
      activation.InsertOrAssignValue(name,
                                     StringValue{std::get<std::string>(value)});
    } else if (std::holds_alternative<bool>(value)) {
      activation.InsertOrAssignValue(name, BoolValue{std::get<bool>(value)});
    } else if (std::holds_alternative<int>(value)) {
      activation.InsertOrAssignValue(name, IntValue{std::get<int>(value)});
    } else if (std::holds_alternative<int64_t>(value)) {
      activation.InsertOrAssignValue(name, IntValue{std::get<int64_t>(value)});
    } else if (std::holds_alternative<uint64_t>(value)) {
      activation.InsertOrAssignValue(name,
                                     UintValue{std::get<uint64_t>(value)});
    } else if (std::holds_alternative<double>(value)) {
      activation.InsertOrAssignValue(name,
                                     DoubleValue{std::get<double>(value)});
    } else if (std::holds_alternative<absl::Duration>(value)) {
      activation.InsertOrAssignValue(
          name, DurationValue{std::get<absl::Duration>(value)});
    } else if (std::holds_alternative<absl::Time>(value)) {
      activation.InsertOrAssignValue(
          name, TimestampValue{std::get<absl::Time>(value)});
    } else if (std::holds_alternative<Value>(value)) {
      activation.InsertOrAssignValue(name, std::get<Value>(value));
    }
  }
  auto result = program->Evaluate(&arena, activation);
  if (test_case.error.has_value()) {
    if (result.ok()) {
      EXPECT_THAT(result->DebugString(), HasSubstr(*test_case.error));
    } else {
      EXPECT_THAT(result.status().message(), HasSubstr(*test_case.error));
    }
  } else {
    if (!result.ok()) {
      // Make it easier to debug the test case.
      ASSERT_THAT(result.status().message(), "");
      // Make sure test case stops here.
      ASSERT_TRUE(result.ok());
    }
    ASSERT_TRUE(result->Is<StringValue>());
    EXPECT_THAT(result->GetString().ToString(), test_case.expected);
  }
}

INSTANTIATE_TEST_SUITE_P(
    TestStringFormatting, StringFormatTest,
    ValuesIn<FormattingTestCase>({
        {
            .name = "Basic",
            .format = "%s %s!",
            .format_args = "'hello', 'world'",
            .expected = "hello world!",
        },
        {
            .name = "EscapedPercentSign",
            .format = "Percent sign %%!",
            .format_args = "'hello', 'world'",
            .expected = "Percent sign %!",
        },
        {
            .name = "IncompleteCase",
            .format = "%",
            .format_args = "'hello'",
            .error = "unexpected end of format string",
        },
        {
            .name = "MissingFormatArg",
            .format = "%s",
            .format_args = "",
            .error = "index 0 out of range",
        },
        {
            .name = "MissingFormatArg2",
            .format = "%s, %s",
            .format_args = "'hello'",
            .error = "index 1 out of range",
        },
        {
            .name = "InvalidPrecision",
            .format = "%.6",
            .format_args = "'hello'",
            .error = "unable to find end of precision specifier",
        },
        {
            .name = "InvalidPrecision2",
            .format = "%.f",
            .format_args = "'hello'",
            .error = "unable to convert precision specifier to integer",
        },
        {
            .name = "InvalidPrecision3",
            .format = "%.",
            .format_args = "'hello'",
            .error = "unable to find end of precision specifier",
        },
        {
            .name = "DecimalFormatingClause",
            .format = "int %d, uint %d",
            .format_args = "-1, uint(2)",
            .expected = R"(int -1, uint 2)",
        },
        {
            .name = "OctalFormatingClause",
            .format = "int %o, uint %o",
            .format_args = "-10, uint(20)",
            .expected = R"(int -12, uint 24)",
        },
        {
            .name = "OctalDoesNotWorkWithDouble",
            .format = "double %o",
            .format_args = "double(\"-Inf\")",
            .error =
                "octal clause can only be used on integers, was given double",
        },
        {
            .name = "HexFormatingClause",
            .format = "int %x, uint %X, string %x, bytes %X",
            .format_args = "-10, uint(255), 'hello', b'world'",
            .expected = "int -a, uint FF, string 68656c6c6f, bytes 776F726C64",
        },
        {
            .name = "HexFormatingClauseLeadingZero",
            .format = "string: %x",
            .format_args = R"(b'\x00\x00hello\x00')",
            .expected = "string: 000068656c6c6f00",
        },
        {
            .name = "HexDoesNotWorkWithDouble",
            .format = "double %x",
            .format_args = "double(\"-Inf\")",
            .error = "hex clause can only be used on integers, byte buffers, "
                     "and strings, was given double",
        },
        {
            .name = "BinaryFormatingClause",
            .format = "int %b, uint %b, bool %b, bool %b",
            .format_args = "-32, uint(20), false, true",
            .expected = "int -100000, uint 10100, bool 0, bool 1",
        },
        {
            .name = "BinaryFormatingClauseLimits",
            .format = "min_int %b, max_int %b, max_uint %b",
            .format_args =
                absl::StrCat(std::numeric_limits<int64_t>::min(), ",",
                             std::numeric_limits<int64_t>::max(), ",",
                             std::numeric_limits<uint64_t>::max(), "u"),
            .expected = "min_int "
                        "-10000000000000000000000000000000000000000000000000000"
                        "00000000000, max_int "
                        "111111111111111111111111111111111111111111111111111111"
                        "111111111, max_uint "
                        "111111111111111111111111111111111111111111111111111111"
                        "1111111111",
        },
        {
            .name = "BinaryFormatingClauseZero",
            .format = "zero %b",
            .format_args = "0",
            .expected = "zero 0",
        },
        {
            .name = "HexFormatingClauseLimits",
            .format = "min_int %x, max_int %x, max_uint %x",
            .format_args =
                absl::StrCat(std::numeric_limits<int64_t>::min(), ",",
                             std::numeric_limits<int64_t>::max(), ",",
                             std::numeric_limits<uint64_t>::max(), "u"),
            .expected = "min_int -8000000000000000, max_int 7fffffffffffffff, "
                        "max_uint ffffffffffffffff",
        },
        {
            .name = "OctalFormatingClauseLimits",
            .format = "min_int %o, max_int %o, max_uint %o",
            .format_args =
                absl::StrCat(std::numeric_limits<int64_t>::min(), ",",
                             std::numeric_limits<int64_t>::max(), ",",
                             std::numeric_limits<uint64_t>::max(), "u"),
            .expected =
                "min_int -1000000000000000000000, max_int "
                "777777777777777777777, max_uint 1777777777777777777777",
        },
        {
            .name = "FixedClauseFormatting",
            .format = "%f",
            .format_args = "10000.1234",
            .expected = "10000.123400",
        },
        {
            .name = "FixedClauseFormattingWithPrecision",
            .format = "%.2f",
            .format_args = "10000.1234",
            .expected = "10000.12",
        },
        {
            .name = "ListSupportForStringWithQuotes",
            .format = "%s",
            .format_args = R"(["a\"b","a\\b"])",
            .expected = "[a\"b, a\\b]",
        },
        {
            .name = "ListSupportForStringWithDouble",
            .format = "%s",
            .format_args =
                R"([double("NaN"),double("Infinity"), double("-Infinity")])",
            .expected = "[NaN, Infinity, -Infinity]",
        },
        FormattingTestCase{
            .name = "FixedClauseFormattingWithDynArgs",
            .format = "%.2f %d",
            .format_args = "arg, message.single_int32",
            .dyn_args =
                {
                    {"arg", 10000.1234},
                    {"message",
                     MakeMessage<TestAllTypes>(R"pb(single_int32: 42)pb")},
                },
            .expected = "10000.12 42",
        },
        {
            .name = "NoOp",
            .format = "no substitution",
            .expected = "no substitution",
        },
        {
            .name = "MidStringSubstitution",
            .format = "str is %s and some more",
            .format_args = "'filler'",
            .expected = "str is filler and some more",
        },
        {
            .name = "PercentEscaping",
            .format = "%% and also %%",
            .expected = "% and also %",
        },
        {
            .name = "SubstitutionInsideEscapedPercentSigns",
            .format = "%%%s%%",
            .format_args = "'text'",
            .expected = "%text%",
        },
        {
            .name = "SubstitutionWithOneEscapedPercentSignOnTheRight",
            .format = "%s%%",
            .format_args = "'percent on the right'",
            .expected = "percent on the right%",
        },
        {
            .name = "SubstitutionWithOneEscapedPercentSignOnTheLeft",
            .format = "%%%s",
            .format_args = "'percent on the left'",
            .expected = "%percent on the left",
        },
        {
            .name = "MultipleSubstitutions",
            .format = "%d %d %d, %s %s %s, %d %d %d, %s %s %s",
            .format_args = "1, 2, 3, 'A', 'B', 'C', 4, 5, 6, 'D', 'E', 'F'",
            .expected = "1 2 3, A B C, 4 5 6, D E F",
        },
        {
            .name = "PercentSignEscapeSequenceSupport",
            .format = "\u0025\u0025escaped \u0025s\u0025\u0025",
            .format_args = "'percent'",
            .expected = "%escaped percent%",
        },
        {
            .name = "FixedPointFormattingClause",
            .format = "%.3f",
            .format_args = "1.2345",
            .expected = "1.234",
        },
        {
            .name = "BinaryFormattingClause",
            .format = "this is 5 in binary: %b",
            .format_args = "5",
            .expected = "this is 5 in binary: 101",
        },
        {
            .name = "UintSupportForBinaryFormatting",
            .format = "unsigned 64 in binary: %b",
            .format_args = "uint(64)",
            .expected = "unsigned 64 in binary: 1000000",
        },
        {
            .name = "BoolSupportForBinaryFormatting",
            .format = "bit set from bool: %b",
            .format_args = "true",
            .expected = "bit set from bool: 1",
        },
        {
            .name = "OctalFormattingClause",
            .format = "%o",
            .format_args = "11",
            .expected = "13",
        },
        {
            .name = "UintSupportForOctalFormattingClause",
            .format = "this is an unsigned octal: %o",
            .format_args = "uint(65535)",
            .expected = "this is an unsigned octal: 177777",
        },
        {
            .name = "LowercaseHexadecimalFormattingClause",
            .format = "%x is 20 in hexadecimal",
            .format_args = "30",
            .expected = "1e is 20 in hexadecimal",
        },
        {
            .name = "UppercaseHexadecimalFormattingClause",
            .format = "%X is 20 in hexadecimal",
            .format_args = "30",
            .expected = "1E is 20 in hexadecimal",
        },
        {
            .name = "UnsignedSupportForHexadecimalFormattingClause",
            .format = "%X is 6000 in hexadecimal",
            .format_args = "uint(6000)",
            .expected = "1770 is 6000 in hexadecimal",
        },
        {
            .name = "StringSupportWithHexadecimalFormattingClause",
            .format = "%x",
            .format_args = R"("Hello world!")",
            .expected = "48656c6c6f20776f726c6421",
        },
        {
            .name = "StringSupportWithUppercaseHexadecimalFormattingClause",
            .format = "%X",
            .format_args = R"("Hello world!")",
            .expected = "48656C6C6F20776F726C6421",
        },
        {
            .name = "ByteSupportWithHexadecimalFormattingClause",
            .format = "%x",
            .format_args = R"(b"byte string")",
            .expected = "6279746520737472696e67",
        },
        {
            .name = "ByteSupportWithUppercaseHexadecimalFormattingClause",
            .format = "%X",
            .format_args = R"(b"byte string")",
            .expected = "6279746520737472696E67",
        },
        {
            .name = "ScientificNotationFormattingClause",
            .format = "%.6e",
            .format_args = "1052.032911275",
            .expected = "1.052033e+03",
        },
        {
            .name = "ScientificNotationFormattingClause2",
            .format = "%e",
            .format_args = "1234.0",
            .expected = "1.234000e+03",
        },
        {
            .name = "DefaultPrecisionForFixedPointClause",
            .format = "%f",
            .format_args = "2.71828",
            .expected = "2.718280",
        },
        {
            .name = "DefaultPrecisionForScientificNotation",
            .format = "%e",
            .format_args = "2.71828",
            .expected = "2.718280e+00",
        },
        {
            .name = "NaNSupportForFixedPoint",
            .format = "%f",
            .format_args = "\"NaN\"",
            .expected = "NaN",
        },
        {
            .name = "PositiveInfinitySupportForFixedPoint",
            .format = "%f",
            .format_args = "\"Infinity\"",
            .expected = "Infinity",
        },
        {
            .name = "NegativeInfinitySupportForFixedPoint",
            .format = "%f",
            .format_args = "\"-Infinity\"",
            .expected = "-Infinity",
        },
        {
            .name = "UintSupportForDecimalClause",
            .format = "%d",
            .format_args = "uint(64)",
            .expected = "64",
        },
        {
            .name = "NullSupportForString",
            .format = "null: %s",
            .format_args = "null",
            .expected = "null: null",
        },
        {
            .name = "IntSupportForString",
            .format = "%s",
            .format_args = "999999999999",
            .expected = "999999999999",
        },
        {
            .name = "BytesSupportForString",
            .format = "some bytes: %s",
            .format_args = "b\"xyz\"",
            .expected = "some bytes: xyz",
        },
        {
            .name = "TypeSupportForString",
            .format = "type is %s",
            .format_args = "type(\"test string\")",
            .expected = "type is string",
        },
        {
            .name = "TimestampSupportForString",
            .format = "%s",
            .format_args = "timestamp(\"2023-02-03T23:31:20+00:00\")",
            .expected = "2023-02-03T23:31:20Z",
        },
        {
            .name = "DurationSupportForString",
            .format = "%s",
            .format_args = "duration(\"1h45m47s\")",
            .expected = "6347s",
        },
        {
            .name = "ListSupportForString",
            .format = "%s",
            .format_args =
                R"(["abc", 3.14, null, [9, 8, 7, 6], timestamp("2023-02-03T23:31:20Z")])",
            .expected =
                R"([abc, 3.14, null, [9, 8, 7, 6], 2023-02-03T23:31:20Z])",
        },
        {
            .name = "MapSupportForString",
            .format = "%s",
            .format_args =
                R"({"key1": b"xyz", "key5": null, "key2": duration("7200s"), "key4": true, "key3": 2.71828})",
            .expected =
                R"({key1: xyz, key2: 7200s, key3: 2.71828, key4: true, key5: null})",
        },
        {
            .name = "MapSupportAllKeyTypes",
            .format = "map with multiple key types: %s",
            .format_args =
                R"({1: "value1", uint(2): "value2", true: double("NaN")})",
            .expected = "map with multiple key types: {1: value1, 2: value2, "
                        "true: NaN}",
        },
        {
            .name = "MapAfterDecimalFormatting",
            .format = "%d %s",
            .format_args = R"(42, {"key": 1})",
            .expected = "42 {key: 1}",
        },
        {
            .name = "BooleanSupportForString",
            .format = "true bool: %s, false bool: %s",
            .format_args = "true, false",
            .expected = "true bool: true, false bool: false",
        },
        FormattingTestCase{
            .name = "DynTypeSupportForStringFormattingClause",
            .format = "Dynamic String: %s",
            .format_args = R"(dynStr)",
            .dyn_args = {{"dynStr", std::string("a string")}},
            .expected = "Dynamic String: a string",
        },
        FormattingTestCase{
            .name = "DynTypeSupportForNumbersWithStringFormattingClause",
            .format = "Dynamic Int Str: %s Dynamic Double Str: %s",
            .format_args = R"(dynIntStr, dynDoubleStr)",
            .dyn_args =
                {
                    {"dynIntStr", 32},
                    {"dynDoubleStr", 56.8},
                },
            .expected = "Dynamic Int Str: 32 Dynamic Double Str: 56.8",
        },
        FormattingTestCase{
            .name = "DynTypeSupportForIntegerFormattingClause",
            .format = "Dynamic Int: %d",
            .format_args = R"(dynInt)",
            .dyn_args = {{"dynInt", 128}},
            .expected = "Dynamic Int: 128",
        },
        FormattingTestCase{
            .name = "DynTypeSupportForIntegerFormattingClauseUnsigned",
            .format = "Dynamic Unsigned Int: %d",
            .format_args = R"(dynUnsignedInt)",
            .dyn_args = {{"dynUnsignedInt", uint64_t{256}}},
            .expected = "Dynamic Unsigned Int: 256",
        },
        FormattingTestCase{
            .name = "DynTypeSupportForHexFormattingClause",
            .format = "Dynamic Hex Int: %x",
            .format_args = R"(dynHexInt)",
            .dyn_args = {{"dynHexInt", 22}},
            .expected = "Dynamic Hex Int: 16",
        },
        FormattingTestCase{
            .name = "DynTypeSupportForHexFormattingClauseUppercase",
            .format = "Dynamic Hex Int: %X (uppercase)",
            .format_args = R"(dynHexInt)",
            .dyn_args = {{"dynHexInt", 26}},
            .expected = "Dynamic Hex Int: 1A (uppercase)",
        },
        FormattingTestCase{
            .name = "DynTypeSupportForUnsignedHexFormattingClause",
            .format = "Dynamic Hex Int: %x (unsigned)",
            .format_args = R"(dynUnsignedHexInt)",
            .dyn_args = {{"dynUnsignedHexInt", uint64_t{500}}},
            .expected = "Dynamic Hex Int: 1f4 (unsigned)",
        },
        FormattingTestCase{
            .name = "DynTypeSupportForFixedPointFormattingClause",
            .format = "Dynamic Double: %.3f",
            .format_args = R"(dynDouble)",
            .dyn_args = {{"dynDouble", 4.5}},
            .expected = "Dynamic Double: 4.500",
        },
        FormattingTestCase{
            .name = "DynTypeSupportForFixedPointFormattingClauseCommaSeparatorL"
                    "ocale",
            .format = "Dynamic Double: %f",
            .format_args = R"(dynDouble)",
            .dyn_args = {{"dynDouble", 4.5}},
            .expected = "Dynamic Double: 4.500000",
        },
        FormattingTestCase{
            .name = "DynTypeSupportForScientificNotation",
            .format = "(Dynamic Type) E: %e",
            .format_args = R"(dynE)",
            .dyn_args = {{"dynE", 2.71828}},
            .expected = "(Dynamic Type) E: 2.718280e+00",
        },
        FormattingTestCase{
            .name = "DynTypeNaNInfinitySupportForFixedPoint",
            .format = "NaN: %f, Infinity: %f",
            .format_args = R"(dynNaN, dynInf)",
            .dyn_args = {{"dynNaN", std::nan("")},
                         {"dynInf", std::numeric_limits<double>::infinity()}},
            .expected = "NaN: NaN, Infinity: Infinity",
        },
        FormattingTestCase{
            .name = "DynTypeSupportForTimestamp",
            .format = "Dynamic Type Timestamp: %s",
            .format_args = R"(dynTime)",
            .dyn_args = {{"dynTime", absl::FromUnixSeconds(1257894000)}},
            .expected = "Dynamic Type Timestamp: 2009-11-10T23:00:00Z",
        },
        FormattingTestCase{
            .name = "DynTypeSupportForDuration",
            .format = "Dynamic Type Duration: %s",
            .format_args = R"(dynDuration)",
            .dyn_args = {{"dynDuration", absl::Hours(2) + absl::Minutes(25) +
                                             absl::Seconds(47)}},
            .expected = "Dynamic Type Duration: 8747s",
        },
        FormattingTestCase{
            .name = "DynTypeSupportForMaps",
            .format = "Dynamic Type Map with Duration: %s",
            .format_args = R"({6:dyn(duration("422s"))})",
            .expected = "Dynamic Type Map with Duration: {6: 422s}",
        },
        FormattingTestCase{
            .name = "DurationsWithSubseconds",
            .format = "Durations with subseconds: %s",
            .format_args =
                R"([duration("422s"), duration("2s123ms"), duration("1us"), duration("1ns"), duration("-1000000ns")])",
            .expected = "Durations with subseconds: [422s, 2.123s, 0.000001s, "
                        "0.000000001s, -0.001s]",
        },
        {
            .name = "UnrecognizedFormattingClause",
            .format = "%a",
            .format_args = "1",
            .error = "unrecognized formatting clause \"a\"",
        },
        {
            .name = "OutOfBoundsArgIndex",
            .format = "%d %d %d",
            .format_args = "0, 1",
            .error = "index 2 out of range",
        },
        {
            .name = "StringSubstitutionIsNotAllowedWithBinaryClause",
            .format = "string is %b",
            .format_args = "\"abc\"",
            .error = "binary clause can only be used on integers and bools, "
                     "was given string",
        },
        {
            .name = "DurationSubstitutionIsNotAllowedWithDecimalClause",
            .format = "%d",
            .format_args = "duration(\"30m2s\")",
            .error = "decimal clause can only be used on numbers, was given "
                     "google.protobuf.Duration",
        },
        {
            .name = "StringSubstitutionIsNotAllowedWithOctalClause",
            .format = "octal: %o",
            .format_args = "\"a string\"",
            .error =
                "octal clause can only be used on integers, was given string",
        },
        {
            .name = "DoubleSubstitutionIsNotAllowedWithHexClause",
            .format = "double is %x",
            .format_args = "0.5",
            .error = "hex clause can only be used on integers, byte buffers, "
                     "and strings, was given double",
        },
        {
            .name = "UppercaseIsNotAllowedForScientificClause",
            .format = "double is %E",
            .format_args = "0.5",
            .error = "unrecognized formatting clause \"E\"",
        },
        {
            .name = "ObjectIsNotAllowed",
            .format = "object is %s",
            .format_args = "cel.expr.conformance.proto3.TestAllTypes{}",
            .error = "could not convert argument "
                     "cel.expr.conformance.proto3.TestAllTypes to string",
        },
        {
            .name = "ObjectInsideList",
            .format = "%s",
            .format_args = "[1, 2, cel.expr.conformance.proto3.TestAllTypes{}]",
            .error = "could not convert argument "
                     "cel.expr.conformance.proto3.TestAllTypes to string",
        },
        {
            .name = "ObjectInsideMap",
            .format = "%s",
            .format_args =
                "{1: \"a\", 2: cel.expr.conformance.proto3.TestAllTypes{}}",
            .error = "could not convert argument "
                     "cel.expr.conformance.proto3.TestAllTypes to string",
        },
        {
            .name = "NullNotAllowedForDecimalClause",
            .format = "null: %d",
            .format_args = "null",
            .error = "decimal clause can only be used on numbers, was given "
                     "null_type",
        },
        {
            .name = "NullNotAllowedForScientificNotationClause",
            .format = "null: %e",
            .format_args = "null",
            .error = "expected a double but got a null_type",
        },
        {
            .name = "NullNotAllowedForFixedPointClause",
            .format = "null: %f",
            .format_args = "null",
            .error = "expected a double but got a null_type",
        },
        {
            .name = "NullNotAllowedForHexadecimalClause",
            .format = "null: %x",
            .format_args = "null",
            .error = "hex clause can only be used on integers, byte buffers, "
                     "and strings, was given null_type",
        },
        {
            .name = "NullNotAllowedForUppercaseHexadecimalClause",
            .format = "null: %X",
            .format_args = "null",
            .error = "hex clause can only be used on integers, byte buffers, "
                     "and strings, was given null_type",
        },
        {
            .name = "NullNotAllowedForBinaryClause",
            .format = "null: %b",
            .format_args = "null",
            .error = "binary clause can only be used on integers and bools, "
                     "was given null_type",
        },
        {
            .name = "NullNotAllowedForOctalClause",
            .format = "null: %o",
            .format_args = "null",
            .error = "octal clause can only be used on integers, was given "
                     "null_type",
        },
        {
            .name = "NegativeBinaryFormattingClause",
            .format = "this is -5 in binary: %b",
            .format_args = "-5",
            .expected = "this is -5 in binary: -101",
        },
        {
            .name = "NegativeOctalFormattingClause",
            .format = "%o",
            .format_args = "-11",
            .expected = "-13",
        },
        {
            .name = "NegativeHexadecimalFormattingClause",
            .format = "%x is -30 in hexadecimal",
            .format_args = "-30",
            .expected = "-1e is -30 in hexadecimal",
        },
        {
            .name = "DefaultPrecisionForString",
            .format = "%s",
            .format_args = "2.71",
            .expected = "2.71",
        },
        {
            .name = "DefaultListPrecisionForString",
            .format = "%s",
            .format_args = "[2.71]",
            .expected =
                "[2.71]",  // Different from Golang (2.710000) consistent with
                           // the precision of a double outside of a list.
        },
        {
            .name = "AutomaticRoundingForString",
            .format = "%s",
            .format_args = "10002.71",
            .expected = "10002.7",  // Different from Golang (10002.71) which
                                    // does not round.
        },
        {
            .name = "DefaultScientificNotationForString",
            .format = "%s",
            .format_args = "0.000000002",
            .expected = "2e-09",
        },
        {
            .name = "DefaultListScientificNotationForString",
            .format = "%s",
            .format_args = "[0.000000002]",
            .expected =
                "[2e-09]",  // Different from Golang (0.000000) consistent with
                            // the notation of a double outside of a list.
        },
        {
            .name = "NaNSupportForString",
            .format = "%s",
            .format_args = R"(double("NaN"))",
            .expected = "NaN",
        },
        {
            .name = "PositiveInfinitySupportForString",
            .format = "%s",
            .format_args = R"(double("Inf"))",
            .expected = "Infinity",
        },
        {
            .name = "NegativeInfinitySupportForString",
            .format = "%s",
            .format_args = R"(double("-Inf"))",
            .expected = "-Infinity",
        },
        {
            .name = "InfinityListSupportForString",
            .format = "%s",
            .format_args = R"([double("NaN"), double("+Inf"), double("-Inf")])",
            .expected = "[NaN, Infinity, -Infinity]",
        },
        {
            .name = "SmallDurationSupportForString",
            .format = "%s",
            .format_args = R"(duration("2ns"))",
            .expected = "0.000000002s",
        },
    }),
    [](const testing::TestParamInfo<StringFormatTest::ParamType>& info) {
      return info.param.name;
    });

}  // namespace
}  // namespace cel::extensions
