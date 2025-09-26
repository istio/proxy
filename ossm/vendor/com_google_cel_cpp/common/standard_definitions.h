// Copyright 2025 Google LLC
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
// Constants used for standard definitions for CEL.
#ifndef THIRD_PARTY_CEL_CPP_COMMON_STANDARD_DEFINITIONS_H_
#define THIRD_PARTY_CEL_CPP_COMMON_STANDARD_DEFINITIONS_H_

#include "absl/strings/string_view.h"

namespace cel {

// Standard function names as represented in an AST.
// TODO(uncreated-issue/71): use a namespace instead of a class.
struct StandardFunctions {
  // Comparison
  static constexpr absl::string_view kEqual = "_==_";
  static constexpr absl::string_view kInequal = "_!=_";
  static constexpr absl::string_view kLess = "_<_";
  static constexpr absl::string_view kLessOrEqual = "_<=_";
  static constexpr absl::string_view kGreater = "_>_";
  static constexpr absl::string_view kGreaterOrEqual = "_>=_";

  // Logical
  static constexpr absl::string_view kAnd = "_&&_";
  static constexpr absl::string_view kOr = "_||_";
  static constexpr absl::string_view kNot = "!_";

  // Strictness
  static constexpr absl::string_view kNotStrictlyFalse = "@not_strictly_false";
  // Deprecated '__not_strictly_false__' function. Preserved for backwards
  // compatibility with stored expressions.
  static constexpr absl::string_view kNotStrictlyFalseDeprecated =
      "__not_strictly_false__";

  // Arithmetical
  static constexpr absl::string_view kAdd = "_+_";
  static constexpr absl::string_view kSubtract = "_-_";
  static constexpr absl::string_view kNeg = "-_";
  static constexpr absl::string_view kMultiply = "_*_";
  static constexpr absl::string_view kDivide = "_/_";
  static constexpr absl::string_view kModulo = "_%_";

  // String operations
  static constexpr absl::string_view kRegexMatch = "matches";
  static constexpr absl::string_view kStringContains = "contains";
  static constexpr absl::string_view kStringEndsWith = "endsWith";
  static constexpr absl::string_view kStringStartsWith = "startsWith";

  // Container operations
  static constexpr absl::string_view kIn = "@in";
  // Deprecated '_in_' operator. Preserved for backwards compatibility with
  // stored expressions.
  static constexpr absl::string_view kInDeprecated = "_in_";
  // Deprecated 'in()' function. Preserved for backwards compatibility with
  // stored expressions.
  static constexpr absl::string_view kInFunction = "in";
  static constexpr absl::string_view kIndex = "_[_]";
  static constexpr absl::string_view kSize = "size";

  static constexpr absl::string_view kTernary = "_?_:_";

  // Timestamp and Duration
  static constexpr absl::string_view kDuration = "duration";
  static constexpr absl::string_view kTimestamp = "timestamp";
  static constexpr absl::string_view kFullYear = "getFullYear";
  static constexpr absl::string_view kMonth = "getMonth";
  static constexpr absl::string_view kDayOfYear = "getDayOfYear";
  static constexpr absl::string_view kDayOfMonth = "getDayOfMonth";
  static constexpr absl::string_view kDate = "getDate";
  static constexpr absl::string_view kDayOfWeek = "getDayOfWeek";
  static constexpr absl::string_view kHours = "getHours";
  static constexpr absl::string_view kMinutes = "getMinutes";
  static constexpr absl::string_view kSeconds = "getSeconds";
  static constexpr absl::string_view kMilliseconds = "getMilliseconds";

  // Type conversions
  static constexpr absl::string_view kBool = "bool";
  static constexpr absl::string_view kBytes = "bytes";
  static constexpr absl::string_view kDouble = "double";
  static constexpr absl::string_view kDyn = "dyn";
  static constexpr absl::string_view kInt = "int";
  static constexpr absl::string_view kString = "string";
  static constexpr absl::string_view kType = "type";
  static constexpr absl::string_view kUint = "uint";

  // Runtime-only functions.
  // The convention for runtime-only functions where only the runtime needs to
  // differentiate behavior is to prefix the function with `#`.
  // Note, this is a different convention from CEL internal functions where the
  // whole stack needs to be aware of the function id.
  static constexpr absl::string_view kRuntimeListAppend = "#list_append";
};

// Standard overload IDs used by type checkers.
// TODO(uncreated-issue/71): use a namespace instead of a class.
struct StandardOverloadIds {
  // Add operator _+_
  static constexpr absl::string_view kAddInt = "add_int64";
  static constexpr absl::string_view kAddUint = "add_uint64";
  static constexpr absl::string_view kAddDouble = "add_double";
  static constexpr absl::string_view kAddDurationDuration =
      "add_duration_duration";
  static constexpr absl::string_view kAddDurationTimestamp =
      "add_duration_timestamp";
  static constexpr absl::string_view kAddTimestampDuration =
      "add_timestamp_duration";
  static constexpr absl::string_view kAddString = "add_string";
  static constexpr absl::string_view kAddBytes = "add_bytes";
  static constexpr absl::string_view kAddList = "add_list";
  // Subtract operator _-_
  static constexpr absl::string_view kSubtractInt = "subtract_int64";
  static constexpr absl::string_view kSubtractUint = "subtract_uint64";
  static constexpr absl::string_view kSubtractDouble = "subtract_double";
  static constexpr absl::string_view kSubtractDurationDuration =
      "subtract_duration_duration";
  static constexpr absl::string_view kSubtractTimestampDuration =
      "subtract_timestamp_duration";
  static constexpr absl::string_view kSubtractTimestampTimestamp =
      "subtract_timestamp_timestamp";
  // Multiply operator _*_
  static constexpr absl::string_view kMultiplyInt = "multiply_int64";
  static constexpr absl::string_view kMultiplyUint = "multiply_uint64";
  static constexpr absl::string_view kMultiplyDouble = "multiply_double";
  // Division operator _/_
  static constexpr absl::string_view kDivideInt = "divide_int64";
  static constexpr absl::string_view kDivideUint = "divide_uint64";
  static constexpr absl::string_view kDivideDouble = "divide_double";
  // Modulo operator _%_
  static constexpr absl::string_view kModuloInt = "modulo_int64";
  static constexpr absl::string_view kModuloUint = "modulo_uint64";
  // Negation operator -_
  static constexpr absl::string_view kNegateInt = "negate_int64";
  static constexpr absl::string_view kNegateDouble = "negate_double";
  // Logical operators
  static constexpr absl::string_view kNot = "logical_not";
  static constexpr absl::string_view kAnd = "logical_and";
  static constexpr absl::string_view kOr = "logical_or";
  static constexpr absl::string_view kConditional = "conditional";
  // Comprehension logic
  static constexpr absl::string_view kNotStrictlyFalse = "not_strictly_false";
  static constexpr absl::string_view kNotStrictlyFalseDeprecated =
      "__not_strictly_false__";
  // Equality operators
  static constexpr absl::string_view kEquals = "equals";
  static constexpr absl::string_view kNotEquals = "not_equals";
  // Relational operators
  static constexpr absl::string_view kLessBool = "less_bool";
  static constexpr absl::string_view kLessString = "less_string";
  static constexpr absl::string_view kLessBytes = "less_bytes";
  static constexpr absl::string_view kLessDuration = "less_duration";
  static constexpr absl::string_view kLessTimestamp = "less_timestamp";
  static constexpr absl::string_view kLessInt = "less_int64";
  static constexpr absl::string_view kLessIntUint = "less_int64_uint64";
  static constexpr absl::string_view kLessIntDouble = "less_int64_double";
  static constexpr absl::string_view kLessDouble = "less_double";
  static constexpr absl::string_view kLessDoubleInt = "less_double_int64";
  static constexpr absl::string_view kLessDoubleUint = "less_double_uint64";
  static constexpr absl::string_view kLessUint = "less_uint64";
  static constexpr absl::string_view kLessUintInt = "less_uint64_int64";
  static constexpr absl::string_view kLessUintDouble = "less_uint64_double";
  static constexpr absl::string_view kGreaterBool = "greater_bool";
  static constexpr absl::string_view kGreaterString = "greater_string";
  static constexpr absl::string_view kGreaterBytes = "greater_bytes";
  static constexpr absl::string_view kGreaterDuration = "greater_duration";
  static constexpr absl::string_view kGreaterTimestamp = "greater_timestamp";
  static constexpr absl::string_view kGreaterInt = "greater_int64";
  static constexpr absl::string_view kGreaterIntUint = "greater_int64_uint64";
  static constexpr absl::string_view kGreaterIntDouble = "greater_int64_double";
  static constexpr absl::string_view kGreaterDouble = "greater_double";
  static constexpr absl::string_view kGreaterDoubleInt = "greater_double_int64";
  static constexpr absl::string_view kGreaterDoubleUint =
      "greater_double_uint64";
  static constexpr absl::string_view kGreaterUint = "greater_uint64";
  static constexpr absl::string_view kGreaterUintInt = "greater_uint64_int64";
  static constexpr absl::string_view kGreaterUintDouble =
      "greater_uint64_double";
  static constexpr absl::string_view kGreaterEqualsBool = "greater_equals_bool";
  static constexpr absl::string_view kGreaterEqualsString =
      "greater_equals_string";
  static constexpr absl::string_view kGreaterEqualsBytes =
      "greater_equals_bytes";
  static constexpr absl::string_view kGreaterEqualsDuration =
      "greater_equals_duration";
  static constexpr absl::string_view kGreaterEqualsTimestamp =
      "greater_equals_timestamp";
  static constexpr absl::string_view kGreaterEqualsInt = "greater_equals_int64";
  static constexpr absl::string_view kGreaterEqualsIntUint =
      "greater_equals_int64_uint64";
  static constexpr absl::string_view kGreaterEqualsIntDouble =
      "greater_equals_int64_double";
  static constexpr absl::string_view kGreaterEqualsDouble =
      "greater_equals_double";
  static constexpr absl::string_view kGreaterEqualsDoubleInt =
      "greater_equals_double_int64";
  static constexpr absl::string_view kGreaterEqualsDoubleUint =
      "greater_equals_double_uint64";
  static constexpr absl::string_view kGreaterEqualsUint =
      "greater_equals_uint64";
  static constexpr absl::string_view kGreaterEqualsUintInt =
      "greater_equals_uint64_int64";
  static constexpr absl::string_view kGreaterEqualsUintDouble =
      "greater_equals_uint_double";
  static constexpr absl::string_view kLessEqualsBool = "less_equals_bool";
  static constexpr absl::string_view kLessEqualsString = "less_equals_string";
  static constexpr absl::string_view kLessEqualsBytes = "less_equals_bytes";
  static constexpr absl::string_view kLessEqualsDuration =
      "less_equals_duration";
  static constexpr absl::string_view kLessEqualsTimestamp =
      "less_equals_timestamp";
  static constexpr absl::string_view kLessEqualsInt = "less_equals_int64";
  static constexpr absl::string_view kLessEqualsIntUint =
      "less_equals_int64_uint64";
  static constexpr absl::string_view kLessEqualsIntDouble =
      "less_equals_int64_double";
  static constexpr absl::string_view kLessEqualsDouble = "less_equals_double";
  static constexpr absl::string_view kLessEqualsDoubleInt =
      "less_equals_double_int64";
  static constexpr absl::string_view kLessEqualsDoubleUint =
      "less_equals_double_uint64";
  static constexpr absl::string_view kLessEqualsUint = "less_equals_uint64";
  static constexpr absl::string_view kLessEqualsUintInt =
      "less_equals_uint64_int64";
  static constexpr absl::string_view kLessEqualsUintDouble =
      "less_equals_uint64_double";
  // Container operators
  static constexpr absl::string_view kIndexList = "index_list";
  static constexpr absl::string_view kIndexMap = "index_map";
  static constexpr absl::string_view kInList = "in_list";
  static constexpr absl::string_view kInMap = "in_map";
  static constexpr absl::string_view kSizeBytes = "size_bytes";
  static constexpr absl::string_view kSizeList = "size_list";
  static constexpr absl::string_view kSizeMap = "size_map";
  static constexpr absl::string_view kSizeString = "size_string";
  static constexpr absl::string_view kSizeBytesMember = "bytes_size";
  static constexpr absl::string_view kSizeListMember = "list_size";
  static constexpr absl::string_view kSizeMapMember = "map_size";
  static constexpr absl::string_view kSizeStringMember = "string_size";
  // String functions
  static constexpr absl::string_view kContainsString = "contains_string";
  static constexpr absl::string_view kEndsWithString = "ends_with_string";
  static constexpr absl::string_view kStartsWithString = "starts_with_string";
  // String RE2 functions
  static constexpr absl::string_view kMatches = "matches";
  static constexpr absl::string_view kMatchesMember = "matches_string";
  // Timestamp / duration accessors
  static constexpr absl::string_view kTimestampToYear = "timestamp_to_year";
  static constexpr absl::string_view kTimestampToYearWithTz =
      "timestamp_to_year_with_tz";
  static constexpr absl::string_view kTimestampToMonth = "timestamp_to_month";
  static constexpr absl::string_view kTimestampToMonthWithTz =
      "timestamp_to_month_with_tz";
  static constexpr absl::string_view kTimestampToDayOfYear =
      "timestamp_to_day_of_year";
  static constexpr absl::string_view kTimestampToDayOfYearWithTz =
      "timestamp_to_day_of_year_with_tz";
  static constexpr absl::string_view kTimestampToDayOfMonth =
      "timestamp_to_day_of_month";
  static constexpr absl::string_view kTimestampToDayOfMonthWithTz =
      "timestamp_to_day_of_month_with_tz";
  static constexpr absl::string_view kTimestampToDayOfWeek =
      "timestamp_to_day_of_week";
  static constexpr absl::string_view kTimestampToDayOfWeekWithTz =
      "timestamp_to_day_of_week_with_tz";
  static constexpr absl::string_view kTimestampToDate =
      "timestamp_to_day_of_month_1_based";
  static constexpr absl::string_view kTimestampToDateWithTz =
      "timestamp_to_day_of_month_1_based_with_tz";
  static constexpr absl::string_view kTimestampToHours = "timestamp_to_hours";
  static constexpr absl::string_view kTimestampToHoursWithTz =
      "timestamp_to_hours_with_tz";
  static constexpr absl::string_view kDurationToHours = "duration_to_hours";
  static constexpr absl::string_view kTimestampToMinutes =
      "timestamp_to_minutes";
  static constexpr absl::string_view kTimestampToMinutesWithTz =
      "timestamp_to_minutes_with_tz";
  static constexpr absl::string_view kDurationToMinutes = "duration_to_minutes";
  static constexpr absl::string_view kTimestampToSeconds =
      "timestamp_to_seconds";
  static constexpr absl::string_view kTimestampToSecondsWithTz =
      "timestamp_to_seconds_tz";
  static constexpr absl::string_view kDurationToSeconds = "duration_to_seconds";
  static constexpr absl::string_view kTimestampToMilliseconds =
      "timestamp_to_milliseconds";
  static constexpr absl::string_view kTimestampToMillisecondsWithTz =
      "timestamp_to_milliseconds_with_tz";
  static constexpr absl::string_view kDurationToMilliseconds =
      "duration_to_milliseconds";
  // Type conversions
  static constexpr absl::string_view kToDyn = "to_dyn";
  // to_uint
  static constexpr absl::string_view kUintToUint = "uint64_to_uint64";
  static constexpr absl::string_view kDoubleToUint = "double_to_uint64";
  static constexpr absl::string_view kIntToUint = "int64_to_uint64";
  static constexpr absl::string_view kStringToUint = "string_to_uint64";
  // to_int
  static constexpr absl::string_view kUintToInt = "uint64_to_int64";
  static constexpr absl::string_view kDoubleToInt = "double_to_int64";
  static constexpr absl::string_view kIntToInt = "int64_to_int64";
  static constexpr absl::string_view kStringToInt = "string_to_int64";
  static constexpr absl::string_view kTimestampToInt = "timestamp_to_int64";
  static constexpr absl::string_view kDurationToInt = "duration_to_int64";
  // to_double
  static constexpr absl::string_view kDoubleToDouble = "double_to_double";
  static constexpr absl::string_view kUintToDouble = "uint64_to_double";
  static constexpr absl::string_view kIntToDouble = "int64_to_double";
  static constexpr absl::string_view kStringToDouble = "string_to_double";
  // to_bool
  static constexpr absl::string_view kBoolToBool = "bool_to_bool";
  static constexpr absl::string_view kStringToBool = "string_to_bool";
  // to_bytes
  static constexpr absl::string_view kBytesToBytes = "bytes_to_bytes";
  static constexpr absl::string_view kStringToBytes = "string_to_bytes";
  // to_string
  static constexpr absl::string_view kStringToString = "string_to_string";
  static constexpr absl::string_view kBytesToString = "bytes_to_string";
  static constexpr absl::string_view kBoolToString = "bool_to_string";
  static constexpr absl::string_view kDoubleToString = "double_to_string";
  static constexpr absl::string_view kIntToString = "int64_to_string";
  static constexpr absl::string_view kUintToString = "uint64_to_string";
  static constexpr absl::string_view kDurationToString = "duration_to_string";
  static constexpr absl::string_view kTimestampToString = "timestamp_to_string";
  // to_timestamp
  static constexpr absl::string_view kTimestampToTimestamp =
      "timestamp_to_timestamp";
  static constexpr absl::string_view kIntToTimestamp = "int64_to_timestamp";
  static constexpr absl::string_view kStringToTimestamp = "string_to_timestamp";
  // to_duration
  static constexpr absl::string_view kDurationToDuration =
      "duration_to_duration";
  static constexpr absl::string_view kIntToDuration = "int64_to_duration";
  static constexpr absl::string_view kStringToDuration = "string_to_duration";
  // to_type
  static constexpr absl::string_view kToType = "type";
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_STANDARD_DEFINITIONS_H_
