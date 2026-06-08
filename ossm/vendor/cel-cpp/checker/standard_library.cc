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

#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/status/status.h"
#include "checker/internal/builtins_arena.h"
#include "checker/type_checker_builder.h"
#include "common/constant.h"
#include "common/decl.h"
#include "common/standard_definitions.h"
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

absl::Status AddArithmeticOps(TypeCheckerBuilder& builder) {
  FunctionDecl add_op;
  add_op.set_name(StandardFunctions::kAdd);
  CEL_RETURN_IF_ERROR(add_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kAddInt, IntType(), IntType(), IntType())));
  CEL_RETURN_IF_ERROR(add_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kAddDouble, DoubleType(),
                       DoubleType(), DoubleType())));
  CEL_RETURN_IF_ERROR(add_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kAddUint, UintType(), UintType(), UintType())));
  // timestamp math
  CEL_RETURN_IF_ERROR(add_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kAddDurationDuration,
                       DurationType(), DurationType(), DurationType())));
  CEL_RETURN_IF_ERROR(add_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kAddDurationTimestamp,
                       TimestampType(), DurationType(), TimestampType())));
  CEL_RETURN_IF_ERROR(add_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kAddTimestampDuration,
                       TimestampType(), TimestampType(), DurationType())));
  // string concat
  CEL_RETURN_IF_ERROR(add_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kAddBytes, BytesType(), BytesType(), BytesType())));
  CEL_RETURN_IF_ERROR(add_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kAddString, StringType(),
                       StringType(), StringType())));
  // list concat
  CEL_RETURN_IF_ERROR(add_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kAddList, ListOfA(), ListOfA(), ListOfA())));

  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(add_op)));

  FunctionDecl subtract_op;
  subtract_op.set_name(StandardFunctions::kSubtract);
  CEL_RETURN_IF_ERROR(subtract_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kSubtractInt, IntType(), IntType(), IntType())));
  CEL_RETURN_IF_ERROR(subtract_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kSubtractUint, UintType(), UintType(), UintType())));
  CEL_RETURN_IF_ERROR(subtract_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kSubtractDouble, DoubleType(),
                       DoubleType(), DoubleType())));
  // Timestamp math
  CEL_RETURN_IF_ERROR(subtract_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kSubtractDurationDuration,
                       DurationType(), DurationType(), DurationType())));
  CEL_RETURN_IF_ERROR(subtract_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kSubtractTimestampDuration,
                       TimestampType(), TimestampType(), DurationType())));
  CEL_RETURN_IF_ERROR(subtract_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kSubtractTimestampTimestamp,
                       DurationType(), TimestampType(), TimestampType())));

  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(subtract_op)));

  FunctionDecl multiply_op;
  multiply_op.set_name(StandardFunctions::kMultiply);
  CEL_RETURN_IF_ERROR(multiply_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kMultiplyInt, IntType(), IntType(), IntType())));
  CEL_RETURN_IF_ERROR(multiply_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kMultiplyUint, UintType(), UintType(), UintType())));
  CEL_RETURN_IF_ERROR(multiply_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kMultiplyDouble, DoubleType(),
                       DoubleType(), DoubleType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(multiply_op)));

  FunctionDecl division_op;
  division_op.set_name(StandardFunctions::kDivide);
  CEL_RETURN_IF_ERROR(division_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kDivideInt, IntType(), IntType(), IntType())));
  CEL_RETURN_IF_ERROR(division_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kDivideUint, UintType(), UintType(), UintType())));
  CEL_RETURN_IF_ERROR(division_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kDivideDouble, DoubleType(),
                       DoubleType(), DoubleType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(division_op)));

  FunctionDecl modulo_op;
  modulo_op.set_name(StandardFunctions::kModulo);
  CEL_RETURN_IF_ERROR(modulo_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kModuloInt, IntType(), IntType(), IntType())));
  CEL_RETURN_IF_ERROR(modulo_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kModuloUint, UintType(), UintType(), UintType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(modulo_op)));

  FunctionDecl negate_op;
  negate_op.set_name(StandardFunctions::kNeg);
  CEL_RETURN_IF_ERROR(negate_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kNegateInt, IntType(), IntType())));
  CEL_RETURN_IF_ERROR(negate_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kNegateDouble, DoubleType(), DoubleType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(negate_op)));

  return absl::OkStatus();
}

absl::Status AddLogicalOps(TypeCheckerBuilder& builder) {
  FunctionDecl not_op;
  not_op.set_name(StandardFunctions::kNot);
  CEL_RETURN_IF_ERROR(not_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kNot, BoolType(), BoolType())));

  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(not_op)));

  FunctionDecl and_op;
  and_op.set_name(StandardFunctions::kAnd);
  CEL_RETURN_IF_ERROR(and_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kAnd, BoolType(), BoolType(), BoolType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(and_op)));

  FunctionDecl or_op;
  or_op.set_name(StandardFunctions::kOr);
  CEL_RETURN_IF_ERROR(or_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kOr, BoolType(), BoolType(), BoolType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(or_op)));

  FunctionDecl conditional_op;
  conditional_op.set_name(StandardFunctions::kTernary);
  CEL_RETURN_IF_ERROR(conditional_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kConditional, TypeParamA(),
                       BoolType(), TypeParamA(), TypeParamA())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(conditional_op)));

  FunctionDecl not_strictly_false;
  not_strictly_false.set_name(StandardFunctions::kNotStrictlyFalse);
  CEL_RETURN_IF_ERROR(not_strictly_false.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kNotStrictlyFalse, BoolType(), BoolType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(not_strictly_false)));

  FunctionDecl not_strictly_false_deprecated;
  not_strictly_false_deprecated.set_name(
      StandardFunctions::kNotStrictlyFalseDeprecated);
  CEL_RETURN_IF_ERROR(not_strictly_false_deprecated.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kNotStrictlyFalseDeprecated,
                       BoolType(), BoolType())));
  CEL_RETURN_IF_ERROR(
      builder.AddFunction(std::move(not_strictly_false_deprecated)));

  return absl::OkStatus();
}

absl::Status AddTypeConversions(TypeCheckerBuilder& builder) {
  FunctionDecl to_dyn;
  to_dyn.set_name(StandardFunctions::kDyn);
  CEL_RETURN_IF_ERROR(to_dyn.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kToDyn, DynType(), TypeParamA())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_dyn)));

  // Uint
  FunctionDecl to_uint;
  to_uint.set_name(StandardFunctions::kUint);
  CEL_RETURN_IF_ERROR(to_uint.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kUintToUint, UintType(), UintType())));
  CEL_RETURN_IF_ERROR(to_uint.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kIntToUint, UintType(), IntType())));
  CEL_RETURN_IF_ERROR(to_uint.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kDoubleToUint, UintType(), DoubleType())));
  CEL_RETURN_IF_ERROR(to_uint.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kStringToUint, UintType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_uint)));

  // Int
  FunctionDecl to_int;
  to_int.set_name(StandardFunctions::kInt);
  CEL_RETURN_IF_ERROR(to_int.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kIntToInt, IntType(), IntType())));
  CEL_RETURN_IF_ERROR(to_int.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kUintToInt, IntType(), UintType())));
  CEL_RETURN_IF_ERROR(to_int.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kDoubleToInt, IntType(), DoubleType())));
  CEL_RETURN_IF_ERROR(to_int.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kStringToInt, IntType(), StringType())));
  CEL_RETURN_IF_ERROR(to_int.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kTimestampToInt, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(to_int.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kDurationToInt, IntType(), DurationType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_int)));

  FunctionDecl to_double;
  to_double.set_name(StandardFunctions::kDouble);
  CEL_RETURN_IF_ERROR(to_double.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kDoubleToDouble, DoubleType(), DoubleType())));
  CEL_RETURN_IF_ERROR(to_double.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kIntToDouble, DoubleType(), IntType())));
  CEL_RETURN_IF_ERROR(to_double.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kUintToDouble, DoubleType(), UintType())));
  CEL_RETURN_IF_ERROR(to_double.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kStringToDouble, DoubleType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_double)));

  FunctionDecl to_bool;
  to_bool.set_name("bool");
  CEL_RETURN_IF_ERROR(to_bool.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kBoolToBool, BoolType(), BoolType())));
  CEL_RETURN_IF_ERROR(to_bool.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kStringToBool, BoolType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_bool)));

  FunctionDecl to_string;
  to_string.set_name(StandardFunctions::kString);
  CEL_RETURN_IF_ERROR(to_string.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kStringToString, StringType(), StringType())));
  CEL_RETURN_IF_ERROR(to_string.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kBytesToString, StringType(), BytesType())));
  CEL_RETURN_IF_ERROR(to_string.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kBoolToString, StringType(), BoolType())));
  CEL_RETURN_IF_ERROR(to_string.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kDoubleToString, StringType(), DoubleType())));
  CEL_RETURN_IF_ERROR(to_string.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kIntToString, StringType(), IntType())));
  CEL_RETURN_IF_ERROR(to_string.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kUintToString, StringType(), UintType())));
  CEL_RETURN_IF_ERROR(to_string.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kTimestampToString, StringType(), TimestampType())));
  CEL_RETURN_IF_ERROR(to_string.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kDurationToString, StringType(), DurationType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_string)));

  FunctionDecl to_bytes;
  to_bytes.set_name(StandardFunctions::kBytes);
  CEL_RETURN_IF_ERROR(to_bytes.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kBytesToBytes, BytesType(), BytesType())));
  CEL_RETURN_IF_ERROR(to_bytes.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kStringToBytes, BytesType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_bytes)));

  FunctionDecl to_timestamp;
  to_timestamp.set_name(StandardFunctions::kTimestamp);
  CEL_RETURN_IF_ERROR(to_timestamp.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kTimestampToTimestamp,
                       TimestampType(), TimestampType())));
  CEL_RETURN_IF_ERROR(to_timestamp.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kStringToTimestamp, TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(to_timestamp.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kIntToTimestamp, TimestampType(), IntType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_timestamp)));

  FunctionDecl to_duration;
  to_duration.set_name(StandardFunctions::kDuration);
  CEL_RETURN_IF_ERROR(to_duration.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kDurationToDuration, DurationType(),
                       DurationType())));
  CEL_RETURN_IF_ERROR(to_duration.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kStringToDuration, DurationType(), StringType())));
  CEL_RETURN_IF_ERROR(to_duration.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kIntToDuration, DurationType(), IntType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_duration)));

  FunctionDecl to_type;
  to_type.set_name(StandardFunctions::kType);
  CEL_RETURN_IF_ERROR(to_type.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kToType, Type(TypeOfA()), TypeParamA())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(to_type)));

  return absl::OkStatus();
}

absl::Status AddEqualityOps(TypeCheckerBuilder& builder) {
  FunctionDecl equals_op;
  equals_op.set_name(StandardFunctions::kEqual);
  CEL_RETURN_IF_ERROR(equals_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kEquals, BoolType(), TypeParamA(), TypeParamA())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(equals_op)));

  FunctionDecl not_equals_op;
  not_equals_op.set_name(StandardFunctions::kInequal);
  CEL_RETURN_IF_ERROR(not_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kNotEquals, BoolType(),
                       TypeParamA(), TypeParamA())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(not_equals_op)));

  return absl::OkStatus();
}

absl::Status AddContainerOps(TypeCheckerBuilder& builder) {
  FunctionDecl index;
  index.set_name(StandardFunctions::kIndex);
  CEL_RETURN_IF_ERROR(index.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kIndexList, TypeParamA(), ListOfA(), IntType())));
  CEL_RETURN_IF_ERROR(index.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kIndexMap, TypeParamB(), MapOfAB(), TypeParamA())));
  CEL_RETURN_IF_ERROR(builder.MergeFunction(std::move(index)));

  FunctionDecl in_op;
  in_op.set_name(StandardFunctions::kIn);
  CEL_RETURN_IF_ERROR(in_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kInList, BoolType(), TypeParamA(), ListOfA())));
  CEL_RETURN_IF_ERROR(in_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kInMap, BoolType(), TypeParamA(), MapOfAB())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(in_op)));

  FunctionDecl in_function_deprecated;
  in_function_deprecated.set_name(StandardFunctions::kInFunction);
  CEL_RETURN_IF_ERROR(in_function_deprecated.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kInList, BoolType(), TypeParamA(), ListOfA())));
  CEL_RETURN_IF_ERROR(in_function_deprecated.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kInMap, BoolType(), TypeParamA(), MapOfAB())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(in_function_deprecated)));

  FunctionDecl in_op_deprecated;
  in_op_deprecated.set_name(StandardFunctions::kInDeprecated);
  CEL_RETURN_IF_ERROR(in_op_deprecated.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kInList, BoolType(), TypeParamA(), ListOfA())));
  CEL_RETURN_IF_ERROR(in_op_deprecated.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kInMap, BoolType(), TypeParamA(), MapOfAB())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(in_op_deprecated)));

  FunctionDecl size;
  size.set_name(StandardFunctions::kSize);
  CEL_RETURN_IF_ERROR(size.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kSizeList, IntType(), ListOfA())));
  CEL_RETURN_IF_ERROR(size.AddOverload(MakeMemberOverloadDecl(
      StandardOverloadIds::kSizeListMember, IntType(), ListOfA())));
  CEL_RETURN_IF_ERROR(size.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kSizeMap, IntType(), MapOfAB())));
  CEL_RETURN_IF_ERROR(size.AddOverload(MakeMemberOverloadDecl(
      StandardOverloadIds::kSizeMapMember, IntType(), MapOfAB())));
  CEL_RETURN_IF_ERROR(size.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kSizeBytes, IntType(), BytesType())));
  CEL_RETURN_IF_ERROR(size.AddOverload(MakeMemberOverloadDecl(
      StandardOverloadIds::kSizeBytesMember, IntType(), BytesType())));
  CEL_RETURN_IF_ERROR(size.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kSizeString, IntType(), StringType())));
  CEL_RETURN_IF_ERROR(size.AddOverload(MakeMemberOverloadDecl(
      StandardOverloadIds::kSizeStringMember, IntType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(size)));

  return absl::OkStatus();
}

absl::Status AddRelationOps(TypeCheckerBuilder& builder) {
  FunctionDecl less_op;
  less_op.set_name(StandardFunctions::kLess);
  // Numeric types
  CEL_RETURN_IF_ERROR(less_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kLessInt, BoolType(), IntType(), IntType())));
  CEL_RETURN_IF_ERROR(less_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kLessUint, BoolType(), UintType(), UintType())));
  CEL_RETURN_IF_ERROR(less_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kLessDouble, BoolType(),
                       DoubleType(), DoubleType())));

  // Non-numeric types
  CEL_RETURN_IF_ERROR(less_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kLessBool, BoolType(), BoolType(), BoolType())));
  CEL_RETURN_IF_ERROR(less_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kLessString, BoolType(),
                       StringType(), StringType())));
  CEL_RETURN_IF_ERROR(less_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kLessBytes, BoolType(), BytesType(), BytesType())));
  CEL_RETURN_IF_ERROR(less_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kLessDuration, BoolType(),
                       DurationType(), DurationType())));
  CEL_RETURN_IF_ERROR(less_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kLessTimestamp, BoolType(),
                       TimestampType(), TimestampType())));

  FunctionDecl greater_op;
  greater_op.set_name(StandardFunctions::kGreater);
  // Numeric types
  CEL_RETURN_IF_ERROR(greater_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kGreaterInt, BoolType(), IntType(), IntType())));
  CEL_RETURN_IF_ERROR(greater_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kGreaterUint, BoolType(), UintType(), UintType())));
  CEL_RETURN_IF_ERROR(greater_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kGreaterDouble, BoolType(),
                       DoubleType(), DoubleType())));

  // Non-numeric types
  CEL_RETURN_IF_ERROR(greater_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kGreaterBool, BoolType(), BoolType(), BoolType())));
  CEL_RETURN_IF_ERROR(greater_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kGreaterString, BoolType(),
                       StringType(), StringType())));
  CEL_RETURN_IF_ERROR(greater_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kGreaterBytes, BoolType(),
                       BytesType(), BytesType())));
  CEL_RETURN_IF_ERROR(greater_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kGreaterDuration, BoolType(),
                       DurationType(), DurationType())));
  CEL_RETURN_IF_ERROR(greater_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kGreaterTimestamp, BoolType(),
                       TimestampType(), TimestampType())));

  FunctionDecl less_equals_op;
  less_equals_op.set_name(StandardFunctions::kLessOrEqual);
  // Numeric types
  CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kLessEqualsInt, BoolType(), IntType(), IntType())));
  CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kLessEqualsUint, BoolType(),
                       UintType(), UintType())));
  CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kLessEqualsDouble, BoolType(),
                       DoubleType(), DoubleType())));

  // Non-numeric types
  CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kLessEqualsBool, BoolType(),
                       BoolType(), BoolType())));
  CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kLessEqualsString, BoolType(),
                       StringType(), StringType())));
  CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kLessEqualsBytes, BoolType(),
                       BytesType(), BytesType())));
  CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kLessEqualsDuration, BoolType(),
                       DurationType(), DurationType())));
  CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kLessEqualsTimestamp, BoolType(),
                       TimestampType(), TimestampType())));

  FunctionDecl greater_equals_op;
  greater_equals_op.set_name(StandardFunctions::kGreaterOrEqual);
  // Numeric types
  CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kGreaterEqualsInt, BoolType(),
                       IntType(), IntType())));
  CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kGreaterEqualsUint, BoolType(),
                       UintType(), UintType())));
  CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kGreaterEqualsDouble, BoolType(),
                       DoubleType(), DoubleType())));
  // Non-numeric types
  CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kGreaterEqualsBool, BoolType(),
                       BoolType(), BoolType())));
  CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kGreaterEqualsString, BoolType(),
                       StringType(), StringType())));
  CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kGreaterEqualsBytes, BoolType(),
                       BytesType(), BytesType())));
  CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kGreaterEqualsDuration, BoolType(),
                       DurationType(), DurationType())));
  CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
      MakeOverloadDecl(StandardOverloadIds::kGreaterEqualsTimestamp, BoolType(),
                       TimestampType(), TimestampType())));

  if (builder.options().enable_cross_numeric_comparisons) {
    // Less
    CEL_RETURN_IF_ERROR(less_op.AddOverload(MakeOverloadDecl(
        StandardOverloadIds::kLessIntUint, BoolType(), IntType(), UintType())));
    CEL_RETURN_IF_ERROR(less_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kLessIntDouble, BoolType(),
                         IntType(), DoubleType())));
    CEL_RETURN_IF_ERROR(less_op.AddOverload(MakeOverloadDecl(
        StandardOverloadIds::kLessUintInt, BoolType(), UintType(), IntType())));
    CEL_RETURN_IF_ERROR(less_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kLessUintDouble, BoolType(),
                         UintType(), DoubleType())));
    CEL_RETURN_IF_ERROR(less_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kLessDoubleInt, BoolType(),
                         DoubleType(), IntType())));
    CEL_RETURN_IF_ERROR(less_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kLessDoubleUint, BoolType(),
                         DoubleType(), UintType())));
    // Greater
    CEL_RETURN_IF_ERROR(greater_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kGreaterIntUint, BoolType(),
                         IntType(), UintType())));
    CEL_RETURN_IF_ERROR(greater_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kGreaterIntDouble, BoolType(),
                         IntType(), DoubleType())));
    CEL_RETURN_IF_ERROR(greater_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kGreaterUintInt, BoolType(),
                         UintType(), IntType())));
    CEL_RETURN_IF_ERROR(greater_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kGreaterUintDouble, BoolType(),
                         UintType(), DoubleType())));
    CEL_RETURN_IF_ERROR(greater_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kGreaterDoubleInt, BoolType(),
                         DoubleType(), IntType())));
    CEL_RETURN_IF_ERROR(greater_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kGreaterDoubleUint, BoolType(),
                         DoubleType(), UintType())));
    // LessEqual
    CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kLessEqualsIntUint, BoolType(),
                         IntType(), UintType())));
    CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kLessEqualsIntDouble, BoolType(),
                         IntType(), DoubleType())));

    CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kLessEqualsUintInt, BoolType(),
                         UintType(), IntType())));
    CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kLessEqualsUintDouble, BoolType(),
                         UintType(), DoubleType())));
    CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kLessEqualsDoubleInt, BoolType(),
                         DoubleType(), IntType())));
    CEL_RETURN_IF_ERROR(less_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kLessEqualsDoubleUint, BoolType(),
                         DoubleType(), UintType())));
    // GreaterEqual
    CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kGreaterEqualsIntUint, BoolType(),
                         IntType(), UintType())));
    CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kGreaterEqualsIntDouble,
                         BoolType(), IntType(), DoubleType())));
    CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kGreaterEqualsUintInt, BoolType(),
                         UintType(), IntType())));
    CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kGreaterEqualsUintDouble,
                         BoolType(), UintType(), DoubleType())));
    CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kGreaterEqualsDoubleInt,
                         BoolType(), DoubleType(), IntType())));
    CEL_RETURN_IF_ERROR(greater_equals_op.AddOverload(
        MakeOverloadDecl(StandardOverloadIds::kGreaterEqualsDoubleUint,
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
  contains.set_name(StandardFunctions::kStringContains);
  CEL_RETURN_IF_ERROR(contains.AddOverload(
      MakeMemberOverloadDecl(StandardOverloadIds::kContainsString, BoolType(),
                             StringType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(contains)));

  FunctionDecl starts_with;
  starts_with.set_name(StandardFunctions::kStringStartsWith);
  CEL_RETURN_IF_ERROR(starts_with.AddOverload(
      MakeMemberOverloadDecl(StandardOverloadIds::kStartsWithString, BoolType(),
                             StringType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(starts_with)));

  FunctionDecl ends_with;
  ends_with.set_name(StandardFunctions::kStringEndsWith);
  CEL_RETURN_IF_ERROR(ends_with.AddOverload(
      MakeMemberOverloadDecl(StandardOverloadIds::kEndsWithString, BoolType(),
                             StringType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(ends_with)));

  return absl::OkStatus();
}

absl::Status AddRegexFunctions(TypeCheckerBuilder& builder) {
  FunctionDecl matches;
  matches.set_name(StandardFunctions::kRegexMatch);
  CEL_RETURN_IF_ERROR(matches.AddOverload(
      MakeMemberOverloadDecl(StandardOverloadIds::kMatchesMember, BoolType(),
                             StringType(), StringType())));
  CEL_RETURN_IF_ERROR(matches.AddOverload(MakeOverloadDecl(
      StandardOverloadIds::kMatches, BoolType(), StringType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(matches)));
  return absl::OkStatus();
}

absl::Status AddTimeFunctions(TypeCheckerBuilder& builder) {
  FunctionDecl get_full_year;
  get_full_year.set_name(StandardFunctions::kFullYear);
  CEL_RETURN_IF_ERROR(get_full_year.AddOverload(MakeMemberOverloadDecl(
      StandardOverloadIds::kTimestampToYear, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_full_year.AddOverload(
      MakeMemberOverloadDecl(StandardOverloadIds::kTimestampToYearWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_full_year)));

  FunctionDecl get_month;
  get_month.set_name(StandardFunctions::kMonth);
  CEL_RETURN_IF_ERROR(get_month.AddOverload(MakeMemberOverloadDecl(
      StandardOverloadIds::kTimestampToMonth, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_month.AddOverload(
      MakeMemberOverloadDecl(StandardOverloadIds::kTimestampToMonthWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_month)));

  FunctionDecl get_day_of_year;
  get_day_of_year.set_name(StandardFunctions::kDayOfYear);
  CEL_RETURN_IF_ERROR(get_day_of_year.AddOverload(MakeMemberOverloadDecl(
      StandardOverloadIds::kTimestampToDayOfYear, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_day_of_year.AddOverload(
      MakeMemberOverloadDecl(StandardOverloadIds::kTimestampToDayOfYearWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_day_of_year)));

  FunctionDecl get_day_of_month;
  get_day_of_month.set_name(StandardFunctions::kDayOfMonth);
  CEL_RETURN_IF_ERROR(get_day_of_month.AddOverload(
      MakeMemberOverloadDecl(StandardOverloadIds::kTimestampToDayOfMonth,
                             IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_day_of_month.AddOverload(
      MakeMemberOverloadDecl(StandardOverloadIds::kTimestampToDayOfMonthWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_day_of_month)));

  FunctionDecl get_date;
  get_date.set_name(StandardFunctions::kDate);
  CEL_RETURN_IF_ERROR(get_date.AddOverload(MakeMemberOverloadDecl(
      StandardOverloadIds::kTimestampToDate, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_date.AddOverload(
      MakeMemberOverloadDecl(StandardOverloadIds::kTimestampToDateWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_date)));

  FunctionDecl get_day_of_week;
  get_day_of_week.set_name(StandardFunctions::kDayOfWeek);
  CEL_RETURN_IF_ERROR(get_day_of_week.AddOverload(MakeMemberOverloadDecl(
      StandardOverloadIds::kTimestampToDayOfWeek, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_day_of_week.AddOverload(
      MakeMemberOverloadDecl(StandardOverloadIds::kTimestampToDayOfWeekWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_day_of_week)));

  FunctionDecl get_hours;
  get_hours.set_name(StandardFunctions::kHours);
  CEL_RETURN_IF_ERROR(get_hours.AddOverload(MakeMemberOverloadDecl(
      StandardOverloadIds::kTimestampToHours, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_hours.AddOverload(
      MakeMemberOverloadDecl(StandardOverloadIds::kTimestampToHoursWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(get_hours.AddOverload(MakeMemberOverloadDecl(
      StandardOverloadIds::kDurationToHours, IntType(), DurationType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_hours)));

  FunctionDecl get_minutes;
  get_minutes.set_name(StandardFunctions::kMinutes);
  CEL_RETURN_IF_ERROR(get_minutes.AddOverload(MakeMemberOverloadDecl(
      StandardOverloadIds::kTimestampToMinutes, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_minutes.AddOverload(
      MakeMemberOverloadDecl(StandardOverloadIds::kTimestampToMinutesWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(get_minutes.AddOverload(MakeMemberOverloadDecl(
      StandardOverloadIds::kDurationToMinutes, IntType(), DurationType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_minutes)));

  FunctionDecl get_seconds;
  get_seconds.set_name(StandardFunctions::kSeconds);
  CEL_RETURN_IF_ERROR(get_seconds.AddOverload(MakeMemberOverloadDecl(
      StandardOverloadIds::kTimestampToSeconds, IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_seconds.AddOverload(
      MakeMemberOverloadDecl(StandardOverloadIds::kTimestampToSecondsWithTz,
                             IntType(), TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(get_seconds.AddOverload(MakeMemberOverloadDecl(
      StandardOverloadIds::kDurationToSeconds, IntType(), DurationType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_seconds)));

  FunctionDecl get_milliseconds;
  get_milliseconds.set_name(StandardFunctions::kMilliseconds);
  CEL_RETURN_IF_ERROR(get_milliseconds.AddOverload(
      MakeMemberOverloadDecl(StandardOverloadIds::kTimestampToMilliseconds,
                             IntType(), TimestampType())));
  CEL_RETURN_IF_ERROR(get_milliseconds.AddOverload(MakeMemberOverloadDecl(
      StandardOverloadIds::kTimestampToMillisecondsWithTz, IntType(),
      TimestampType(), StringType())));
  CEL_RETURN_IF_ERROR(get_milliseconds.AddOverload(
      MakeMemberOverloadDecl(StandardOverloadIds::kDurationToMilliseconds,
                             IntType(), DurationType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(get_milliseconds)));

  return absl::OkStatus();
}

absl::Status AddTypeConstantVariables(TypeCheckerBuilder& builder) {
  CEL_RETURN_IF_ERROR(builder.AddVariable(
      MakeVariableDecl(StandardFunctions::kDyn, TypeDynType())));

  CEL_RETURN_IF_ERROR(
      builder.AddVariable(MakeVariableDecl("bool", TypeBoolType())));

  CEL_RETURN_IF_ERROR(
      builder.AddVariable(MakeVariableDecl("null_type", TypeNullType())));

  CEL_RETURN_IF_ERROR(builder.AddVariable(
      MakeVariableDecl(StandardFunctions::kInt, TypeIntType())));

  CEL_RETURN_IF_ERROR(builder.AddVariable(
      MakeVariableDecl(StandardFunctions::kUint, TypeUintType())));

  CEL_RETURN_IF_ERROR(builder.AddVariable(
      MakeVariableDecl(StandardFunctions::kDouble, TypeDoubleType())));

  CEL_RETURN_IF_ERROR(builder.AddVariable(
      MakeVariableDecl(StandardFunctions::kString, TypeStringType())));

  CEL_RETURN_IF_ERROR(builder.AddVariable(
      MakeVariableDecl(StandardFunctions::kBytes, TypeBytesType())));

  // Note: timestamp and duration are only referenced by the corresponding
  // protobuf type names and handled by the type lookup logic.

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
  // TODO(uncreated-issue/74): This is interpreted as an enum (int) or null in
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
  CEL_RETURN_IF_ERROR(AddContainerOps(builder));
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
CheckerLibrary StandardCheckerLibrary() {
  return {"stdlib", AddStandardLibraryDecls};
}
}  // namespace cel
