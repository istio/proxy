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

#include "checker/standard_library.h"

#include <string>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/status/status.h"
#include "base/builtins.h"
#include "checker/internal/builtins_arena.h"
#include "checker/type_checker_builder.h"
#include "common/constant.h"
#include "common/decl.h"
#include "common/type.h"
#include "internal/status_macros.h"

namespace cel {
namespace {

using ::cel::checker_internal::BuiltinsArena;

// Arbitrary type parameter name A.
TypeParamType TypeParamA() { return TypeParamType("A"); }

// Arbitrary type parameter name B.
TypeParamType TypeParamB() { return TypeParamType("B"); }

Type ListOfA() {
  static absl::NoDestructor<Type> kInstance(
      ListType(BuiltinsArena(), TypeParamA()));
  return *kInstance;
}

Type MapOfAB() {
  static absl::NoDestructor<Type> kInstance(
      MapType(BuiltinsArena(), TypeParamA(), TypeParamB()));
  return *kInstance;
}

Type TypeOfType() {
  static absl::NoDestructor<Type> kInstance(
      TypeType(BuiltinsArena(), TypeType()));
  return *kInstance;
}

Type TypeOfA() {
  static absl::NoDestructor<Type> kInstance(
      TypeType(BuiltinsArena(), TypeParamA()));
  return *kInstance;
}

Type TypeNullType() {
  static absl::NoDestructor<Type> kInstance(
      TypeType(BuiltinsArena(), NullType()));
  return *kInstance;
}

Type TypeBoolType() {
  static absl::NoDestructor<Type> kInstance(
      TypeType(BuiltinsArena(), BoolType()));
  return *kInstance;
}

Type TypeIntType() {
  static absl::NoDestructor<Type> kInstance(
      TypeType(BuiltinsArena(), IntType()));
  return *kInstance;
}

Type TypeUintType() {
  static absl::NoDestructor<Type> kInstance(
      TypeType(BuiltinsArena(), UintType()));
  return *kInstance;
}

Type TypeDoubleType() {
  static absl::NoDestructor<Type> kInstance(
      TypeType(BuiltinsArena(), DoubleType()));
  return *kInstance;
}

Type TypeStringType() {
  static absl::NoDestructor<Type> kInstance(
      TypeType(BuiltinsArena(), StringType()));
  return *kInstance;
}

Type TypeBytesType() {
  static absl::NoDestructor<Type> kInstance(
      TypeType(BuiltinsArena(), BytesType()));
  return *kInstance;
}

Type TypeDurationType() {
  static absl::NoDestructor<Type> kInstance(
      TypeType(BuiltinsArena(), DurationType()));
  return *kInstance;
}

Type TypeTimestampType() {
  static absl::NoDestructor<Type> kInstance(
      TypeType(BuiltinsArena(), TimestampType()));
  return *kInstance;
}

Type TypeDynType() {
  static absl::NoDestructor<Type> kInstance(
      TypeType(BuiltinsArena(), DynType()));
  return *kInstance;
}

Type TypeListType() {
  static absl::NoDestructor<Type> kInstance(
      TypeType(BuiltinsArena(), ListOfA()));
  return *kInstance;
}

Type TypeMapType() {
  static absl::NoDestructor<Type> kInstance(
      TypeType(BuiltinsArena(), MapOfAB()));
  return *kInstance;
}

// TODO: Move to common after initial typechecker impl is stable.
class StandardOverloads {
 public:
  // Add operator _+_
  static constexpr char kAddInt[] = "add_int64";
  static constexpr char kAddUint[] = "add_uint64";
  static constexpr char kAddDouble[] = "add_double";
  static constexpr char kAddDurationDuration[] = "add_duration_duration";
  static constexpr char kAddDurationTimestamp[] = "add_duration_timestamp";
  static constexpr char kAddTimestampDuration[] = "add_timestamp_duration";
  static constexpr char kAddString[] = "add_string";
  static constexpr char kAddBytes[] = "add_bytes";
  static constexpr char kAddList[] = "add_list";
  // Subtract operator _-_
  static constexpr char kSubtractInt[] = "subtract_int64";
  static constexpr char kSubtractUint[] = "subtract_uint64";
  static constexpr char kSubtractDouble[] = "subtract_double";
  static constexpr char kSubtractDurationDuration[] =
      "subtract_duration_duration";
  static constexpr char kSubtractTimestampDuration[] =
      "subtract_timestamp_duration";
  static constexpr char kSubtractTimestampTimestamp[] =
      "subtract_timestamp_timestamp";
  // Multiply operator _*_
  static constexpr char kMultiplyInt[] = "multiply_int64";
  static constexpr char kMultiplyUint[] = "multiply_uint64";
  static constexpr char kMultiplyDouble[] = "multiply_double";
  // Division operator _/_
  static constexpr char kDivideInt[] = "divide_int64";
  static constexpr char kDivideUint[] = "divide_uint64";
  static constexpr char kDivideDouble[] = "divide_double";
  // Modulo operator _%_
  static constexpr char kModuloInt[] = "modulo_int64";
  static constexpr char kModuloUint[] = "modulo_uint64";
  // Negation operator -_
  static constexpr char kNegateInt[] = "negate_int64";
  static constexpr char kNegateDouble[] = "negate_double";
  // Logical operators
  static constexpr char kNot[] = "logical_not";
  static constexpr char kAnd[] = "logical_and";
  static constexpr char kOr[] = "logical_or";
  static constexpr char kConditional[] = "conditional";
  // Comprehension logic
  static constexpr char kNotStrictlyFalse[] = "not_strictly_false";
  static constexpr char kNotStrictlyFalseDeprecated[] =
      "__not_strictly_false__";
  // Equality operators
  static constexpr char kEquals[] = "equals";
  static constexpr char kNotEquals[] = "not_equals";
  // Relational operators
  static constexpr char kLessBool[] = "less_bool";
  static constexpr char kLessString[] = "less_string";
  static constexpr char kLessBytes[] = "less_bytes";
  static constexpr char kLessDuration[] = "less_duration";
  static constexpr char kLessTimestamp[] = "less_timestamp";
  static constexpr char kLessInt[] = "less_int64";
  static constexpr char kLessIntUint[] = "less_int64_uint64";
  static constexpr char kLessIntDouble[] = "less_int64_double";
  static constexpr char kLessDouble[] = "less_double";
  static constexpr char kLessDoubleInt[] = "less_double_int64";
  static constexpr char kLessDoubleUint[] = "less_double_uint64";
  static constexpr char kLessUint[] = "less_uint64";
  static constexpr char kLessUintInt[] = "less_uint64_int64";
  static constexpr char kLessUintDouble[] = "less_uint64_double";
  static constexpr char kGreaterBool[] = "greater_bool";
  static constexpr char kGreaterString[] = "greater_string";
  static constexpr char kGreaterBytes[] = "greater_bytes";
  static constexpr char kGreaterDuration[] = "greater_duration";
  static constexpr char kGreaterTimestamp[] = "greater_timestamp";
  static constexpr char kGreaterInt[] = "greater_int64";
  static constexpr char kGreaterIntUint[] = "greater_int64_uint64";
  static constexpr char kGreaterIntDouble[] = "greater_int64_double";
  static constexpr char kGreaterDouble[] = "greater_double";
  static constexpr char kGreaterDoubleInt[] = "greater_double_int64";
  static constexpr char kGreaterDoubleUint[] = "greater_double_uint64";
  static constexpr char kGreaterUint[] = "greater_uint64";
  static constexpr char kGreaterUintInt[] = "greater_uint64_int64";
  static constexpr char kGreaterUintDouble[] = "greater_uint64_double";
  static constexpr char kGreaterEqualsBool[] = "greater_equals_bool";
  static constexpr char kGreaterEqualsString[] = "greater_equals_string";
  static constexpr char kGreaterEqualsBytes[] = "greater_equals_bytes";
  static constexpr char kGreaterEqualsDuration[] = "greater_equals_duration";
  static constexpr char kGreaterEqualsTimestamp[] = "greater_equals_timestamp";
  static constexpr char kGreaterEqualsInt[] = "greater_equals_int64";
  static constexpr char kGreaterEqualsIntUint[] = "greater_equals_int64_uint64";
  static constexpr char kGreaterEqualsIntDouble[] =
      "greater_equals_int64_double";
  static constexpr char kGreaterEqualsDouble[] = "greater_equals_double";
  static constexpr char kGreaterEqualsDoubleInt[] =
      "greater_equals_double_int64";
  static constexpr char kGreaterEqualsDoubleUint[] =
      "greater_equals_double_uint64";
  static constexpr char kGreaterEqualsUint[] = "greater_equals_uint64";
  static constexpr char kGreaterEqualsUintInt[] = "greater_equals_uint64_int64";
  static constexpr char kGreaterEqualsUintDouble[] =
      "greater_equals_uint_double";
  static constexpr char kLessEqualsBool[] = "less_equals_bool";
  static constexpr char kLessEqualsString[] = "less_equals_string";
  static constexpr char kLessEqualsBytes[] = "less_equals_bytes";
  static constexpr char kLessEqualsDuration[] = "less_equals_duration";
  static constexpr char kLessEqualsTimestamp[] = "less_equals_timestamp";
  static constexpr char kLessEqualsInt[] = "less_equals_int64";
  static constexpr char kLessEqualsIntUint[] = "less_equals_int64_uint64";
  static constexpr char kLessEqualsIntDouble[] = "less_equals_int64_double";
  static constexpr char kLessEqualsDouble[] = "less_equals_double";
  static constexpr char kLessEqualsDoubleInt[] = "less_equals_double_int64";
  static constexpr char kLessEqualsDoubleUint[] = "less_equals_double_uint64";
  static constexpr char kLessEqualsUint[] = "less_equals_uint64";
  static constexpr char kLessEqualsUintInt[] = "less_equals_uint64_int64";
  static constexpr char kLessEqualsUintDouble[] = "less_equals_uint64_double";
  // Container operators
  static constexpr char kIndexList[] = "index_list";
  static constexpr char kIndexMap[] = "index_map";
  static constexpr char kInList[] = "in_list";
  static constexpr char kInMap[] = "in_map";
  static constexpr char kSizeBytes[] = "size_bytes";
  static constexpr char kSizeList[] = "size_list";
  static constexpr char kSizeMap[] = "size_map";
  static constexpr char kSizeString[] = "size_string";
  static constexpr char kSizeBytesMember[] = "bytes_size";
  static constexpr char kSizeListMember[] = "list_size";
  static constexpr char kSizeMapMember[] = "map_size";
  static constexpr char kSizeStringMember[] = "string_size";
  // String functions
  static constexpr char kContainsString[] = "contains_string";
  static constexpr char kEndsWithString[] = "ends_with_string";
  static constexpr char kStartsWithString[] = "starts_with_string";
  // String RE2 functions
  static constexpr char kMatches[] = "matches";
  static constexpr char kMatchesMember[] = "matches_string";
  // Timestamp / duration accessors
  static constexpr char kTimestampToYear[] = "timestamp_to_year";
  static constexpr char kTimestampToYearWithTz[] = "timestamp_to_year_with_tz";
  static constexpr char kTimestampToMonth[] = "timestamp_to_month";
  static constexpr char kTimestampToMonthWithTz[] =
      "timestamp_to_month_with_tz";
  static constexpr char kTimestampToDayOfYear[] = "timestamp_to_day_of_year";
  static constexpr char kTimestampToDayOfYearWithTz[] =
      "timestamp_to_day_of_year_with_tz";
  static constexpr char kTimestampToDayOfMonth[] = "timestamp_to_day_of_month";
  static constexpr char kTimestampToDayOfMonthWithTz[] =
      "timestamp_to_day_of_month_with_tz";
  static constexpr char kTimestampToDayOfWeek[] = "timestamp_to_day_of_week";
  static constexpr char kTimestampToDayOfWeekWithTz[] =
      "timestamp_to_day_of_week_with_tz";
  static constexpr char kTimestampToDate[] =
      "timestamp_to_day_of_month_1_based";
  static constexpr char kTimestampToDateWithTz[] =
      "timestamp_to_day_of_month_1_based_with_tz";
  static constexpr char kTimestampToHours[] = "timestamp_to_hours";
  static constexpr char kTimestampToHoursWithTz[] =
      "timestamp_to_hours_with_tz";
  static constexpr char kDurationToHours[] = "duration_to_hours";
  static constexpr char kTimestampToMinutes[] = "timestamp_to_minutes";
  static constexpr char kTimestampToMinutesWithTz[] =
      "timestamp_to_minutes_with_tz";
  static constexpr char kDurationToMinutes[] = "duration_to_minutes";
  static constexpr char kTimestampToSeconds[] = "timestamp_to_seconds";
  static constexpr char kTimestampToSecondsWithTz[] = "timestamp_to_seconds_tz";
  static constexpr char kDurationToSeconds[] = "duration_to_seconds";
  static constexpr char kTimestampToMilliseconds[] =
      "timestamp_to_milliseconds";
  static constexpr char kTimestampToMillisecondsWithTz[] =
      "timestamp_to_milliseconds_with_tz";
  static constexpr char kDurationToMilliseconds[] = "duration_to_milliseconds";
  // Type conversions
  static constexpr char kToDyn[] = "to_dyn";
  // to_uint
  static constexpr char kUintToUint[] = "uint64_to_uint64";
  static constexpr char kDoubleToUint[] = "double_to_uint64";
  static constexpr char kIntToUint[] = "int64_to_uint64";
  static constexpr char kStringToUint[] = "string_to_uint64";
  // to_int
  static constexpr char kUintToInt[] = "uint64_to_int64";
  static constexpr char kDoubleToInt[] = "double_to_int64";
  static constexpr char kIntToInt[] = "int64_to_int64";
  static constexpr char kStringToInt[] = "string_to_int64";
  static constexpr char kTimestampToInt[] = "timestamp_to_int64";
  static constexpr char kDurationToInt[] = "duration_to_int64";
  // to_double
  static constexpr char kDoubleToDouble[] = "double_to_double";
  static constexpr char kUintToDouble[] = "uint64_to_double";
  static constexpr char kIntToDouble[] = "int64_to_double";
  static constexpr char kStringToDouble[] = "string_to_double";
  // to_bool
  static constexpr char kBoolToBool[] = "bool_to_bool";
  static constexpr char kStringToBool[] = "string_to_bool";
  // to_bytes
  static constexpr char kBytesToBytes[] = "bytes_to_bytes";
  static constexpr char kStringToBytes[] = "string_to_bytes";
  // to_string
  static constexpr char kStringToString[] = "string_to_string";
  static constexpr char kBytesToString[] = "bytes_to_string";
  static constexpr char kBoolToString[] = "bool_to_string";
  static constexpr char kDoubleToString[] = "double_to_string";
  static constexpr char kIntToString[] = "int64_to_string";
  static constexpr char kUintToString[] = "uint64_to_string";
  static constexpr char kDurationToString[] = "duration_to_string";
  static constexpr char kTimestampToString[] = "timestamp_to_string";
  // to_timestamp
  static constexpr char kTimestampToTimestamp[] = "timestamp_to_timestamp";
  static constexpr char kIntToTimestamp[] = "int64_to_timestamp";
  static constexpr char kStringToTimestamp[] = "string_to_timestamp";
  // to_duration
  static constexpr char kDurationToDuration[] = "duration_to_duration";
  static constexpr char kIntToDuration[] = "int64_to_duration";
  static constexpr char kStringToDuration[] = "string_to_duration";
  // to_type
  static constexpr char kToType[] = "type";
};

absl::Status AddArithmeticOps(TypeCheckerBuilder& builder) {
  FunctionDecl add_op;
  add_op.set_name(builtin::kAdd);
  CEL_RETURN_IF_ERROR(add_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kAddInt, IntType(), IntType(), IntType())));
  CEL_RETURN_IF_ERROR(add_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kAddDouble, DoubleType(),
                       DoubleType(), DoubleType())));
  CEL_RETURN_IF_ERROR(add_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kAddUint, UintType(), UintType(), UintType())));
  // timestamp math
  CEL_RETURN_IF_ERROR(add_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kAddDurationDuration, DurationType(),
                       DurationType(), DurationType())));
  CEL_RETURN_IF_ERROR(add_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kAddDurationTimestamp,
                       TimestampType(), DurationType(), TimestampType())));
  CEL_RETURN_IF_ERROR(add_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kAddTimestampDuration,
                       TimestampType(), TimestampType(), DurationType())));
  // string concat
  CEL_RETURN_IF_ERROR(add_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kAddBytes, BytesType(), BytesType(), BytesType())));
  CEL_RETURN_IF_ERROR(add_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kAddString, StringType(),
                       StringType(), StringType())));
  // list concat
  CEL_RETURN_IF_ERROR(add_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kAddList, ListOfA(), ListOfA(), ListOfA())));

  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(add_op)));

  FunctionDecl subtract_op;
  subtract_op.set_name(builtin::kSubtract);
  CEL_RETURN_IF_ERROR(subtract_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kSubtractInt, IntType(), IntType(), IntType())));
  CEL_RETURN_IF_ERROR(subtract_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kSubtractUint, UintType(), UintType(), UintType())));
  CEL_RETURN_IF_ERROR(subtract_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kSubtractDouble, DoubleType(),
                       DoubleType(), DoubleType())));
  // Timestamp math
  CEL_RETURN_IF_ERROR(subtract_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kSubtractDurationDuration,
                       DurationType(), DurationType(), DurationType())));
  CEL_RETURN_IF_ERROR(subtract_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kSubtractTimestampDuration,
                       TimestampType(), TimestampType(), DurationType())));
  CEL_RETURN_IF_ERROR(subtract_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kSubtractTimestampTimestamp,
                       DurationType(), TimestampType(), TimestampType())));

  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(subtract_op)));

  FunctionDecl multiply_op;
  multiply_op.set_name(builtin::kMultiply);
  CEL_RETURN_IF_ERROR(multiply_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kMultiplyInt, IntType(), IntType(), IntType())));
  CEL_RETURN_IF_ERROR(multiply_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kMultiplyUint, UintType(), UintType(), UintType())));
  CEL_RETURN_IF_ERROR(multiply_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kMultiplyDouble, DoubleType(),
                       DoubleType(), DoubleType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(multiply_op)));

  FunctionDecl division_op;
  division_op.set_name(builtin::kDivide);
  CEL_RETURN_IF_ERROR(division_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kDivideInt, IntType(), IntType(), IntType())));
  CEL_RETURN_IF_ERROR(division_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kDivideUint, UintType(), UintType(), UintType())));
  CEL_RETURN_IF_ERROR(division_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kDivideDouble, DoubleType(),
                       DoubleType(), DoubleType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(division_op)));

  FunctionDecl modulo_op;
  modulo_op.set_name(builtin::kModulo);
  CEL_RETURN_IF_ERROR(modulo_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kModuloInt, IntType(), IntType(), IntType())));
  CEL_RETURN_IF_ERROR(modulo_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kModuloUint, UintType(), UintType(), UintType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(modulo_op)));

  FunctionDecl negate_op;
  negate_op.set_name(builtin::kNeg);
  CEL_RETURN_IF_ERROR(negate_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kNegateInt, IntType(), IntType())));
  CEL_RETURN_IF_ERROR(negate_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kNegateDouble, DoubleType(), DoubleType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(negate_op)));

  return absl::OkStatus();
}

absl::Status AddLogicalOps(TypeCheckerBuilder& builder) {
  FunctionDecl not_op;
  not_op.set_name(builtin::kNot);
  CEL_RETURN_IF_ERROR(not_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kNot, BoolType(), BoolType())));

  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(not_op)));

  FunctionDecl and_op;
  and_op.set_name(builtin::kAnd);
  CEL_RETURN_IF_ERROR(and_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kAnd, BoolType(), BoolType(), BoolType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(and_op)));

  FunctionDecl or_op;
  or_op.set_name(builtin::kOr);
  CEL_RETURN_IF_ERROR(or_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kOr, BoolType(), BoolType(), BoolType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(or_op)));

  FunctionDecl conditional_op;
  conditional_op.set_name(builtin::kTernary);
  CEL_RETURN_IF_ERROR(conditional_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kConditional, TypeParamA(),
                       BoolType(), TypeParamA(), TypeParamA())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(conditional_op)));

  FunctionDecl not_strictly_false;
  not_strictly_false.set_name(builtin::kNotStrictlyFalse);
  CEL_RETURN_IF_ERROR(not_strictly_false.AddOverload(MakeOverloadDecl(
      StandardOverloads::kNotStrictlyFalse, BoolType(), BoolType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(not_strictly_false)));

  FunctionDecl not_strictly_false_deprecated;
  not_strictly_false_deprecated.set_name(builtin::kNotStrictlyFalseDeprecated);
  CEL_RETURN_IF_ERROR(not_strictly_false_deprecated.AddOverload(
      MakeOverloadDecl(StandardOverloads::kNotStrictlyFalseDeprecated,
                       BoolType(), BoolType())));
  CEL_RETURN_IF_ERROR(
      builder.AddFunction(std::move(not_strictly_false_deprecated)));

  return absl::OkStatus();
}

absl::Status AddTypeConversions(TypeCheckerBuilder& builder) {
  FunctionDecl to_dyn;
  to_dyn.set_name(builtin::kDyn);
  CEL_RETURN_IF_ERROR(to_dyn.AddOverload(
      MakeOverloadDecl(StandardOverloads::kToDyn, DynType(), TypeParamA())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_dyn)));

  // Uint
  FunctionDecl to_uint;
  to_uint.set_name(builtin::kUint);
  CEL_RETURN_IF_ERROR(to_uint.AddOverload(MakeOverloadDecl(
      StandardOverloads::kUintToUint, UintType(), UintType())));
  CEL_RETURN_IF_ERROR(to_uint.AddOverload(
      MakeOverloadDecl(StandardOverloads::kIntToUint, UintType(), IntType())));
  CEL_RETURN_IF_ERROR(to_uint.AddOverload(MakeOverloadDecl(
      StandardOverloads::kDoubleToUint, UintType(), DoubleType())));
  CEL_RETURN_IF_ERROR(to_uint.AddOverload(MakeOverloadDecl(
      StandardOverloads::kStringToUint, UintType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_uint)));

  // Int
  FunctionDecl to_int;
  to_int.set_name(builtin::kInt);
  CEL_RETURN_IF_ERROR(to_int.AddOverload(
      MakeOverloadDecl(StandardOverloads::kIntToInt, IntType(), IntType())));
  CEL_RETURN_IF_ERROR(to_int.AddOverload(
      MakeOverloadDecl(StandardOverloads::kUintToInt, IntType(), UintType())));
  CEL_RETURN_IF_ERROR(to_int.AddOverload(MakeOverloadDecl(
      StandardOverloads::kDoubleToInt, IntType(), DoubleType())));
  CEL_RETURN_IF_ERROR(to_int.AddOverload(MakeOverloadDecl(
      StandardOverloads::kStringToInt, IntType(), StringType())));
  CEL_RETURN_IF_ERROR(to_int.AddOverload(MakeOverloadDecl(
      StandardOverloads::kTimestampToInt, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(to_int.AddOverload(MakeOverloadDecl(
      StandardOverloads::kDurationToInt, IntType(), DurationType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_int)));

  FunctionDecl to_double;
  to_double.set_name(builtin::kDouble);
  CEL_RETURN_IF_ERROR(to_double.AddOverload(MakeOverloadDecl(
      StandardOverloads::kDoubleToDouble, DoubleType(), DoubleType())));
  CEL_RETURN_IF_ERROR(to_double.AddOverload(MakeOverloadDecl(
      StandardOverloads::kIntToDouble, DoubleType(), IntType())));
  CEL_RETURN_IF_ERROR(to_double.AddOverload(MakeOverloadDecl(
      StandardOverloads::kUintToDouble, DoubleType(), UintType())));
  CEL_RETURN_IF_ERROR(to_double.AddOverload(MakeOverloadDecl(
      StandardOverloads::kStringToDouble, DoubleType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_double)));

  FunctionDecl to_bool;
  to_bool.set_name("bool");
  CEL_RETURN_IF_ERROR(to_bool.AddOverload(MakeOverloadDecl(
      StandardOverloads::kBoolToBool, BoolType(), BoolType())));
  CEL_RETURN_IF_ERROR(to_bool.AddOverload(MakeOverloadDecl(
      StandardOverloads::kStringToBool, BoolType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_bool)));

  FunctionDecl to_string;
  to_string.set_name(builtin::kString);
  CEL_RETURN_IF_ERROR(to_string.AddOverload(MakeOverloadDecl(
      StandardOverloads::kStringToString, StringType(), StringType())));
  CEL_RETURN_IF_ERROR(to_string.AddOverload(MakeOverloadDecl(
      StandardOverloads::kBytesToString, StringType(), BytesType())));
  CEL_RETURN_IF_ERROR(to_string.AddOverload(MakeOverloadDecl(
      StandardOverloads::kBoolToString, StringType(), BoolType())));
  CEL_RETURN_IF_ERROR(to_string.AddOverload(MakeOverloadDecl(
      StandardOverloads::kDoubleToString, StringType(), DoubleType())));
  CEL_RETURN_IF_ERROR(to_string.AddOverload(MakeOverloadDecl(
      StandardOverloads::kIntToString, StringType(), IntType())));
  CEL_RETURN_IF_ERROR(to_string.AddOverload(MakeOverloadDecl(
      StandardOverloads::kUintToString, StringType(), UintType())));
  CEL_RETURN_IF_ERROR(to_string.AddOverload(MakeOverloadDecl(
      StandardOverloads::kTimestampToString, StringType(), TimestampType())));
  CEL_RETURN_IF_ERROR(to_string.AddOverload(MakeOverloadDecl(
      StandardOverloads::kDurationToString, StringType(), DurationType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_string)));

  FunctionDecl to_bytes;
  to_bytes.set_name(builtin::kBytes);
  CEL_RETURN_IF_ERROR(to_bytes.AddOverload(MakeOverloadDecl(
      StandardOverloads::kBytesToBytes, BytesType(), BytesType())));
  CEL_RETURN_IF_ERROR(to_bytes.AddOverload(MakeOverloadDecl(
      StandardOverloads::kStringToBytes, BytesType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_bytes)));

  FunctionDecl to_timestamp;
  to_timestamp.set_name(builtin::kTimestamp);
  CEL_RETURN_IF_ERROR(to_timestamp.AddOverload(
      MakeOverloadDecl(StandardOverloads::kTimestampToTimestamp,
                       TimestampType(), TimestampType())));
  CEL_RETURN_IF_ERROR(to_timestamp.AddOverload(MakeOverloadDecl(
      StandardOverloads::kStringToTimestamp, TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(to_timestamp.AddOverload(MakeOverloadDecl(
      StandardOverloads::kIntToTimestamp, TimestampType(), IntType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_timestamp)));

  FunctionDecl to_duration;
  to_duration.set_name(builtin::kDuration);
  CEL_RETURN_IF_ERROR(to_duration.AddOverload(MakeOverloadDecl(
      StandardOverloads::kDurationToDuration, DurationType(), DurationType())));
  CEL_RETURN_IF_ERROR(to_duration.AddOverload(MakeOverloadDecl(
      StandardOverloads::kStringToDuration, DurationType(), StringType())));
  CEL_RETURN_IF_ERROR(to_duration.AddOverload(MakeOverloadDecl(
      StandardOverloads::kIntToDuration, DurationType(), IntType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_duration)));

  FunctionDecl to_type;
  to_type.set_name(builtin::kType);
  CEL_RETURN_IF_ERROR(to_type.AddOverload(MakeOverloadDecl(
      StandardOverloads::kToType, Type(TypeOfA()), TypeParamA())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_type)));

  return absl::OkStatus();
}

absl::Status AddEqualityOps(TypeCheckerBuilder& builder) {
  FunctionDecl equals_op;
  equals_op.set_name(builtin::kEqual);
  CEL_RETURN_IF_ERROR(equals_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kEquals, BoolType(), TypeParamA(), TypeParamA())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(equals_op)));

  FunctionDecl not_equals_op;
  not_equals_op.set_name(builtin::kInequal);
  CEL_RETURN_IF_ERROR(not_equals_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kNotEquals, BoolType(), TypeParamA(), TypeParamA())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(not_equals_op)));

  return absl::OkStatus();
}

absl::Status AddConatainerOps(TypeCheckerBuilder& builder) {
  FunctionDecl index;
  index.set_name(builtin::kIndex);
  CEL_RETURN_IF_ERROR(index.AddOverload(MakeOverloadDecl(
      StandardOverloads::kIndexList, TypeParamA(), ListOfA(), IntType())));
  CEL_RETURN_IF_ERROR(index.AddOverload(MakeOverloadDecl(
      StandardOverloads::kIndexMap, TypeParamB(), MapOfAB(), TypeParamA())));
  CEL_RETURN_IF_ERROR(builder.MergeFunction(std::move(index)));

  FunctionDecl in_op;
  in_op.set_name(builtin::kIn);
  CEL_RETURN_IF_ERROR(in_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kInList, BoolType(), TypeParamA(), ListOfA())));
  CEL_RETURN_IF_ERROR(in_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kInMap, BoolType(), TypeParamA(), MapOfAB())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(in_op)));

  FunctionDecl in_function_deprecated;
  in_function_deprecated.set_name(builtin::kInFunction);
  CEL_RETURN_IF_ERROR(in_function_deprecated.AddOverload(MakeOverloadDecl(
      StandardOverloads::kInList, BoolType(), TypeParamA(), ListOfA())));
  CEL_RETURN_IF_ERROR(in_function_deprecated.AddOverload(MakeOverloadDecl(
      StandardOverloads::kInMap, BoolType(), TypeParamA(), MapOfAB())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(in_function_deprecated)));

  FunctionDecl in_op_deprecated;
  in_op_deprecated.set_name(builtin::kInDeprecated);
  CEL_RETURN_IF_ERROR(in_op_deprecated.AddOverload(MakeOverloadDecl(
      StandardOverloads::kInList, BoolType(), TypeParamA(), ListOfA())));
  CEL_RETURN_IF_ERROR(in_op_deprecated.AddOverload(MakeOverloadDecl(
      StandardOverloads::kInMap, BoolType(), TypeParamA(), MapOfAB())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(in_op_deprecated)));

  FunctionDecl size;
  size.set_name(builtin::kSize);
  CEL_RETURN_IF_ERROR(size.AddOverload(
      MakeOverloadDecl(StandardOverloads::kSizeList, IntType(), ListOfA())));
  CEL_RETURN_IF_ERROR(size.AddOverload(MakeMemberOverloadDecl(
      StandardOverloads::kSizeListMember, IntType(), ListOfA())));
  CEL_RETURN_IF_ERROR(size.AddOverload(
      MakeOverloadDecl(StandardOverloads::kSizeMap, IntType(), MapOfAB())));
  CEL_RETURN_IF_ERROR(size.AddOverload(MakeMemberOverloadDecl(
      StandardOverloads::kSizeMapMember, IntType(), MapOfAB())));
  CEL_RETURN_IF_ERROR(size.AddOverload(
      MakeOverloadDecl(StandardOverloads::kSizeBytes, IntType(), BytesType())));
  CEL_RETURN_IF_ERROR(size.AddOverload(MakeMemberOverloadDecl(
      StandardOverloads::kSizeBytesMember, IntType(), BytesType())));
  CEL_RETURN_IF_ERROR(size.AddOverload(MakeOverloadDecl(
      StandardOverloads::kSizeString, IntType(), StringType())));
  CEL_RETURN_IF_ERROR(size.AddOverload(MakeMemberOverloadDecl(
      StandardOverloads::kSizeStringMember, IntType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(size)));

  return absl::OkStatus();
}

absl::Status AddRelationOps(TypeCheckerBuilder& builder) {
  FunctionDecl less_op;
  less_op.set_name(builtin::kLess);
  // Numeric types
  CEL_RETURN_IF_ERROR(less_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kLessInt, BoolType(), IntType(), IntType())));
  CEL_RETURN_IF_ERROR(less_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kLessUint, BoolType(), UintType(), UintType())));
  CEL_RETURN_IF_ERROR(less_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kLessDouble, BoolType(), DoubleType(), DoubleType())));

  // Non-numeric types
  CEL_RETURN_IF_ERROR(less_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kLessBool, BoolType(), BoolType(), BoolType())));
  CEL_RETURN_IF_ERROR(less_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kLessString, BoolType(), StringType(), StringType())));
  CEL_RETURN_IF_ERROR(less_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kLessBytes, BoolType(), BytesType(), BytesType())));
  CEL_RETURN_IF_ERROR(less_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kLessDuration, BoolType(),
                       DurationType(), DurationType())));
  CEL_RETURN_IF_ERROR(less_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kLessTimestamp, BoolType(),
                       TimestampType(), TimestampType())));

  FunctionDecl greater_op;
  greater_op.set_name(builtin::kGreater);
  // Numeric types
  CEL_RETURN_IF_ERROR(greater_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kGreaterInt, BoolType(), IntType(), IntType())));
  CEL_RETURN_IF_ERROR(greater_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kGreaterUint, BoolType(), UintType(), UintType())));
  CEL_RETURN_IF_ERROR(greater_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kGreaterDouble, BoolType(),
                       DoubleType(), DoubleType())));

  // Non-numeric types
  CEL_RETURN_IF_ERROR(greater_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kGreaterBool, BoolType(), BoolType(), BoolType())));
  CEL_RETURN_IF_ERROR(greater_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kGreaterString, BoolType(),
                       StringType(), StringType())));
  CEL_RETURN_IF_ERROR(greater_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kGreaterBytes, BoolType(), BytesType(), BytesType())));
  CEL_RETURN_IF_ERROR(greater_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kGreaterDuration, BoolType(),
                       DurationType(), DurationType())));
  CEL_RETURN_IF_ERROR(greater_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kGreaterTimestamp, BoolType(),
                       TimestampType(), TimestampType())));

  FunctionDecl less_equals_op;
  less_equals_op.set_name(builtin::kLessOrEqual);
  // Numeric types
  CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kLessEqualsInt, BoolType(), IntType(), IntType())));
  CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kLessEqualsUint, BoolType(), UintType(), UintType())));
  CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kLessEqualsDouble, BoolType(),
                       DoubleType(), DoubleType())));

  // Non-numeric types
  CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kLessEqualsBool, BoolType(), BoolType(), BoolType())));
  CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kLessEqualsString, BoolType(),
                       StringType(), StringType())));
  CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kLessEqualsBytes, BoolType(),
                       BytesType(), BytesType())));
  CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kLessEqualsDuration, BoolType(),
                       DurationType(), DurationType())));
  CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kLessEqualsTimestamp, BoolType(),
                       TimestampType(), TimestampType())));

  FunctionDecl greater_equals_op;
  greater_equals_op.set_name(builtin::kGreaterOrEqual);
  // Numeric types
  CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(MakeOverloadDecl(
      StandardOverloads::kGreaterEqualsInt, BoolType(), IntType(), IntType())));
  CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kGreaterEqualsUint, BoolType(),
                       UintType(), UintType())));
  CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kGreaterEqualsDouble, BoolType(),
                       DoubleType(), DoubleType())));
  // Non-numeric types
  CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kGreaterEqualsBool, BoolType(),
                       BoolType(), BoolType())));
  CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kGreaterEqualsString, BoolType(),
                       StringType(), StringType())));
  CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kGreaterEqualsBytes, BoolType(),
                       BytesType(), BytesType())));
  CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kGreaterEqualsDuration, BoolType(),
                       DurationType(), DurationType())));
  CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloads::kGreaterEqualsTimestamp, BoolType(),
                       TimestampType(), TimestampType())));

  if (builder.options().enable_cross_numeric_comparisons) {
    // Less
    CEL_RETURN_IF_ERROR(less_op.AddOverload(MakeOverloadDecl(
        StandardOverloads::kLessIntUint, BoolType(), IntType(), UintType())));
    CEL_RETURN_IF_ERROR(less_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kLessIntDouble, BoolType(),
                         IntType(), DoubleType())));
    CEL_RETURN_IF_ERROR(less_op.AddOverload(MakeOverloadDecl(
        StandardOverloads::kLessUintInt, BoolType(), UintType(), IntType())));
    CEL_RETURN_IF_ERROR(less_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kLessUintDouble, BoolType(),
                         UintType(), DoubleType())));
    CEL_RETURN_IF_ERROR(less_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kLessDoubleInt, BoolType(),
                         DoubleType(), IntType())));
    CEL_RETURN_IF_ERROR(less_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kLessDoubleUint, BoolType(),
                         DoubleType(), UintType())));
    // Greater
    CEL_RETURN_IF_ERROR(greater_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kGreaterIntUint, BoolType(),
                         IntType(), UintType())));
    CEL_RETURN_IF_ERROR(greater_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kGreaterIntDouble, BoolType(),
                         IntType(), DoubleType())));
    CEL_RETURN_IF_ERROR(greater_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kGreaterUintInt, BoolType(),
                         UintType(), IntType())));
    CEL_RETURN_IF_ERROR(greater_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kGreaterUintDouble, BoolType(),
                         UintType(), DoubleType())));
    CEL_RETURN_IF_ERROR(greater_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kGreaterDoubleInt, BoolType(),
                         DoubleType(), IntType())));
    CEL_RETURN_IF_ERROR(greater_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kGreaterDoubleUint, BoolType(),
                         DoubleType(), UintType())));
    // LessEqual
    CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kLessEqualsIntUint, BoolType(),
                         IntType(), UintType())));
    CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kLessEqualsIntDouble, BoolType(),
                         IntType(), DoubleType())));

    CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kLessEqualsUintInt, BoolType(),
                         UintType(), IntType())));
    CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kLessEqualsUintDouble, BoolType(),
                         UintType(), DoubleType())));
    CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kLessEqualsDoubleInt, BoolType(),
                         DoubleType(), IntType())));
    CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kLessEqualsDoubleUint, BoolType(),
                         DoubleType(), UintType())));
    // GreaterEqual
    CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kGreaterEqualsIntUint, BoolType(),
                         IntType(), UintType())));
    CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kGreaterEqualsIntDouble, BoolType(),
                         IntType(), DoubleType())));
    CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kGreaterEqualsUintInt, BoolType(),
                         UintType(), IntType())));
    CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kGreaterEqualsUintDouble,
                         BoolType(), UintType(), DoubleType())));
    CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kGreaterEqualsDoubleInt, BoolType(),
                         DoubleType(), IntType())));
    CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloads::kGreaterEqualsDoubleUint,
                         BoolType(), DoubleType(), UintType())));
  }

  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(less_op)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(greater_op)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(less_equals_op)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(greater_equals_op)));

  return absl::OkStatus();
}

absl::Status AddStringFunctions(TypeCheckerBuilder& builder) {
  FunctionDecl contains;
  contains.set_name(builtin::kStringContains);
  CEL_RETURN_IF_ERROR(contains.AddOverload(
      MakeMemberOverloadDecl(StandardOverloads::kContainsString, BoolType(),
                             StringType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(contains)));

  FunctionDecl starts_with;
  starts_with.set_name(builtin::kStringStartsWith);
  CEL_RETURN_IF_ERROR(starts_with.AddOverload(
      MakeMemberOverloadDecl(StandardOverloads::kStartsWithString, BoolType(),
                             StringType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(starts_with)));

  FunctionDecl ends_with;
  ends_with.set_name(builtin::kStringEndsWith);
  CEL_RETURN_IF_ERROR(ends_with.AddOverload(
      MakeMemberOverloadDecl(StandardOverloads::kEndsWithString, BoolType(),
                             StringType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(ends_with)));

  return absl::OkStatus();
}

absl::Status AddRegexFunctions(TypeCheckerBuilder& builder) {
  FunctionDecl matches;
  matches.set_name(builtin::kRegexMatch);
  CEL_RETURN_IF_ERROR(matches.AddOverload(
      MakeMemberOverloadDecl(StandardOverloads::kMatchesMember, BoolType(),
                             StringType(), StringType())));
  CEL_RETURN_IF_ERROR(matches.AddOverload(MakeOverloadDecl(
      StandardOverloads::kMatches, BoolType(), StringType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(matches)));
  return absl::OkStatus();
}

absl::Status AddTimeFunctions(TypeCheckerBuilder& builder) {
  FunctionDecl get_full_year;
  get_full_year.set_name(builtin::kFullYear);
  CEL_RETURN_IF_ERROR(get_full_year.AddOverload(MakeMemberOverloadDecl(
      StandardOverloads::kTimestampToYear, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_full_year.AddOverload(
      MakeMemberOverloadDecl(StandardOverloads::kTimestampToYearWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_full_year)));

  FunctionDecl get_month;
  get_month.set_name(builtin::kMonth);
  CEL_RETURN_IF_ERROR(get_month.AddOverload(MakeMemberOverloadDecl(
      StandardOverloads::kTimestampToMonth, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_month.AddOverload(
      MakeMemberOverloadDecl(StandardOverloads::kTimestampToMonthWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_month)));

  FunctionDecl get_day_of_year;
  get_day_of_year.set_name(builtin::kDayOfYear);
  CEL_RETURN_IF_ERROR(get_day_of_year.AddOverload(MakeMemberOverloadDecl(
      StandardOverloads::kTimestampToDayOfYear, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_day_of_year.AddOverload(
      MakeMemberOverloadDecl(StandardOverloads::kTimestampToDayOfYearWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_day_of_year)));

  FunctionDecl get_day_of_month;
  get_day_of_month.set_name(builtin::kDayOfMonth);
  CEL_RETURN_IF_ERROR(get_day_of_month.AddOverload(MakeMemberOverloadDecl(
      StandardOverloads::kTimestampToDayOfMonth, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_day_of_month.AddOverload(
      MakeMemberOverloadDecl(StandardOverloads::kTimestampToDayOfMonthWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_day_of_month)));

  FunctionDecl get_date;
  get_date.set_name(builtin::kDate);
  CEL_RETURN_IF_ERROR(get_date.AddOverload(MakeMemberOverloadDecl(
      StandardOverloads::kTimestampToDate, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_date.AddOverload(
      MakeMemberOverloadDecl(StandardOverloads::kTimestampToDateWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_date)));

  FunctionDecl get_day_of_week;
  get_day_of_week.set_name(builtin::kDayOfWeek);
  CEL_RETURN_IF_ERROR(get_day_of_week.AddOverload(MakeMemberOverloadDecl(
      StandardOverloads::kTimestampToDayOfWeek, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_day_of_week.AddOverload(
      MakeMemberOverloadDecl(StandardOverloads::kTimestampToDayOfWeekWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_day_of_week)));

  FunctionDecl get_hours;
  get_hours.set_name(builtin::kHours);
  CEL_RETURN_IF_ERROR(get_hours.AddOverload(MakeMemberOverloadDecl(
      StandardOverloads::kTimestampToHours, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_hours.AddOverload(
      MakeMemberOverloadDecl(StandardOverloads::kTimestampToHoursWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(get_hours.AddOverload(MakeMemberOverloadDecl(
      StandardOverloads::kDurationToHours, IntType(), DurationType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_hours)));

  FunctionDecl get_minutes;
  get_minutes.set_name(builtin::kMinutes);
  CEL_RETURN_IF_ERROR(get_minutes.AddOverload(MakeMemberOverloadDecl(
      StandardOverloads::kTimestampToMinutes, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_minutes.AddOverload(
      MakeMemberOverloadDecl(StandardOverloads::kTimestampToMinutesWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(get_minutes.AddOverload(MakeMemberOverloadDecl(
      StandardOverloads::kDurationToMinutes, IntType(), DurationType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_minutes)));

  FunctionDecl get_seconds;
  get_seconds.set_name(builtin::kSeconds);
  CEL_RETURN_IF_ERROR(get_seconds.AddOverload(MakeMemberOverloadDecl(
      StandardOverloads::kTimestampToSeconds, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_seconds.AddOverload(
      MakeMemberOverloadDecl(StandardOverloads::kTimestampToSecondsWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(get_seconds.AddOverload(MakeMemberOverloadDecl(
      StandardOverloads::kDurationToSeconds, IntType(), DurationType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_seconds)));

  FunctionDecl get_milliseconds;
  get_milliseconds.set_name(builtin::kMilliseconds);
  CEL_RETURN_IF_ERROR(get_milliseconds.AddOverload(
      MakeMemberOverloadDecl(StandardOverloads::kTimestampToMilliseconds,
                             IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_milliseconds.AddOverload(
      MakeMemberOverloadDecl(StandardOverloads::kTimestampToMillisecondsWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(get_milliseconds.AddOverload(MakeMemberOverloadDecl(
      StandardOverloads::kDurationToMilliseconds, IntType(), DurationType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_milliseconds)));

  return absl::OkStatus();
}

absl::Status AddTypeConstantVariables(TypeCheckerBuilder& builder) {
  CEL_RETURN_IF_ERROR(
      builder.AddVariable(MakeVariableDecl(builtin::kDyn, TypeDynType())));

  CEL_RETURN_IF_ERROR(
      builder.AddVariable(MakeVariableDecl("bool", TypeBoolType())));

  CEL_RETURN_IF_ERROR(
      builder.AddVariable(MakeVariableDecl("null_type", TypeNullType())));

  CEL_RETURN_IF_ERROR(
      builder.AddVariable(MakeVariableDecl(builtin::kInt, TypeIntType())));

  CEL_RETURN_IF_ERROR(
      builder.AddVariable(MakeVariableDecl(builtin::kUint, TypeUintType())));

  CEL_RETURN_IF_ERROR(builder.AddVariable(
      MakeVariableDecl(builtin::kDouble, TypeDoubleType())));

  CEL_RETURN_IF_ERROR(builder.AddVariable(
      MakeVariableDecl(builtin::kString, TypeStringType())));

  CEL_RETURN_IF_ERROR(
      builder.AddVariable(MakeVariableDecl(builtin::kBytes, TypeBytesType())));

  CEL_RETURN_IF_ERROR(builder.AddVariable(
      MakeVariableDecl(builtin::kDuration, TypeDurationType())));

  CEL_RETURN_IF_ERROR(builder.AddVariable(
      MakeVariableDecl(builtin::kTimestamp, TypeTimestampType())));

  CEL_RETURN_IF_ERROR(
      builder.AddVariable(MakeVariableDecl("list", TypeListType())));

  CEL_RETURN_IF_ERROR(
      builder.AddVariable(MakeVariableDecl("map", TypeMapType())));

  CEL_RETURN_IF_ERROR(
      builder.AddVariable(MakeVariableDecl("type", TypeOfType())));

  return absl::OkStatus();
}

absl::Status AddEnumConstants(TypeCheckerBuilder& builder) {
  VariableDecl pb_null;
  pb_null.set_name("google.protobuf.NullValue.NULL_VALUE");
  // TODO: This is interpreted as an enum (int) or null in
  // different cases. We should add some additional spec tests to cover this and
  // update the behavior to be consistent.
  pb_null.set_type(IntType());
  pb_null.set_value(Constant(nullptr));
  CEL_RETURN_IF_ERROR(builder.AddVariable(std::move(pb_null)));
  return absl::OkStatus();
}

absl::Status AddStandardLibraryDecls(TypeCheckerBuilder& builder) {
  CEL_RETURN_IF_ERROR(AddLogicalOps(builder));
  CEL_RETURN_IF_ERROR(AddArithmeticOps(builder));
  CEL_RETURN_IF_ERROR(AddTypeConversions(builder));
  CEL_RETURN_IF_ERROR(AddEqualityOps(builder));
  CEL_RETURN_IF_ERROR(AddConatainerOps(builder));
  CEL_RETURN_IF_ERROR(AddRelationOps(builder));
  CEL_RETURN_IF_ERROR(AddStringFunctions(builder));
  CEL_RETURN_IF_ERROR(AddRegexFunctions(builder));
  CEL_RETURN_IF_ERROR(AddTimeFunctions(builder));
  CEL_RETURN_IF_ERROR(AddTypeConstantVariables(builder));
  CEL_RETURN_IF_ERROR(AddEnumConstants(builder));

  return absl::OkStatus();
}

}  // namespace

// Returns a CheckerLibrary containing all of the standard CEL declarations.
CheckerLibrary StandardLibrary() { return {"stdlib", AddStandardLibraryDecls}; }
}  // namespace cel
