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

#include "extensions/math_ext_decls.h"

#include <string>

#include "absl/base/no_destructor.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "checker/internal/builtins_arena.h"
#include "checker/type_checker_builder.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "compiler/compiler.h"
#include "extensions/math_ext_macros.h"
#include "internal/status_macros.h"
#include "parser/parser_interface.h"

namespace cel::extensions {
namespace {

constexpr char kMathExtensionName[] = "cel.lib.ext.math";

const Type& ListIntType() {
  static absl::NoDestructor<Type> kInstance(
      ListType(checker_internal::BuiltinsArena(), IntType()));
  return *kInstance;
}

const Type& ListDoubleType() {
  static absl::NoDestructor<Type> kInstance(
      ListType(checker_internal::BuiltinsArena(), DoubleType()));
  return *kInstance;
}

const Type& ListUintType() {
  static absl::NoDestructor<Type> kInstance(
      ListType(checker_internal::BuiltinsArena(), UintType()));
  return *kInstance;
}

std::string OverloadTypeName(const Type& type) {
  switch (type.kind()) {
    case cel::TypeKind::kInt:
      return "int";
    case TypeKind::kDouble:
      return "double";
    case TypeKind::kUint:
      return "uint";
    case TypeKind::kList:
      return absl::StrCat("list_",
                          OverloadTypeName(type.AsList()->GetElement()));
    default:
      return "unsupported";
  }
}

absl::Status AddMinMaxDecls(TypeCheckerBuilder& builder) {
  const Type kNumerics[] = {IntType(), DoubleType(), UintType()};
  const Type kListNumerics[] = {ListIntType(), ListDoubleType(),
                                ListUintType()};

  constexpr char kMinOverloadPrefix[] = "math_@min_";
  constexpr char kMaxOverloadPrefix[] = "math_@max_";

  FunctionDecl min_decl;
  min_decl.set_name("math.@min");

  FunctionDecl max_decl;
  max_decl.set_name("math.@max");

  for (const Type& type : kNumerics) {
    // Unary overloads
    CEL_RETURN_IF_ERROR(min_decl.AddOverload(MakeOverloadDecl(
        absl::StrCat(kMinOverloadPrefix, OverloadTypeName(type)), type, type)));

    CEL_RETURN_IF_ERROR(max_decl.AddOverload(MakeOverloadDecl(
        absl::StrCat(kMaxOverloadPrefix, OverloadTypeName(type)), type, type)));

    // Pairwise overloads
    for (const Type& other_type : kNumerics) {
      Type out_type = DynType();
      if (type.kind() == other_type.kind()) {
        out_type = type;
      }
      CEL_RETURN_IF_ERROR(min_decl.AddOverload(MakeOverloadDecl(
          absl::StrCat(kMinOverloadPrefix, OverloadTypeName(type), "_",
                       OverloadTypeName(other_type)),
          out_type, type, other_type)));

      CEL_RETURN_IF_ERROR(max_decl.AddOverload(MakeOverloadDecl(
          absl::StrCat(kMaxOverloadPrefix, OverloadTypeName(type), "_",
                       OverloadTypeName(other_type)),
          out_type, type, other_type)));
    }
  }

  // List overloads
  for (const Type& type : kListNumerics) {
    CEL_RETURN_IF_ERROR(min_decl.AddOverload(MakeOverloadDecl(
        absl::StrCat(kMinOverloadPrefix, OverloadTypeName(type)),
        type.AsList()->GetElement(), type)));

    CEL_RETURN_IF_ERROR(max_decl.AddOverload(MakeOverloadDecl(
        absl::StrCat(kMaxOverloadPrefix, OverloadTypeName(type)),
        type.AsList()->GetElement(), type)));
  }

  CEL_RETURN_IF_ERROR(builder.AddFunction(min_decl));
  CEL_RETURN_IF_ERROR(builder.AddFunction(max_decl));

  return absl::OkStatus();
}

absl::Status AddSignednessDecls(TypeCheckerBuilder& builder) {
  const Type kNumerics[] = {IntType(), DoubleType(), UintType()};

  FunctionDecl sqrt_decl;
  sqrt_decl.set_name("math.sqrt");

  FunctionDecl sign_decl;
  sign_decl.set_name("math.sign");

  FunctionDecl abs_decl;
  abs_decl.set_name("math.abs");

  for (const Type& type : kNumerics) {
    CEL_RETURN_IF_ERROR(sqrt_decl.AddOverload(
        MakeOverloadDecl(absl::StrCat("math_sqrt_", OverloadTypeName(type)),
                         DoubleType(), type)));
    CEL_RETURN_IF_ERROR(sign_decl.AddOverload(MakeOverloadDecl(
        absl::StrCat("math_sign_", OverloadTypeName(type)), type, type)));
    CEL_RETURN_IF_ERROR(abs_decl.AddOverload(MakeOverloadDecl(
        absl::StrCat("math_abs_", OverloadTypeName(type)), type, type)));
  }

  CEL_RETURN_IF_ERROR(builder.AddFunction(sqrt_decl));
  CEL_RETURN_IF_ERROR(builder.AddFunction(sign_decl));
  CEL_RETURN_IF_ERROR(builder.AddFunction(abs_decl));

  return absl::OkStatus();
}

absl::Status AddFloatingPointDecls(TypeCheckerBuilder& builder) {
  // Rounding
  CEL_ASSIGN_OR_RETURN(
      auto ceil_decl,
      MakeFunctionDecl(
          "math.ceil",
          MakeOverloadDecl("math_ceil_double", DoubleType(), DoubleType())));

  CEL_ASSIGN_OR_RETURN(
      auto floor_decl,
      MakeFunctionDecl(
          "math.floor",
          MakeOverloadDecl("math_floor_double", DoubleType(), DoubleType())));

  CEL_ASSIGN_OR_RETURN(
      auto round_decl,
      MakeFunctionDecl(
          "math.round",
          MakeOverloadDecl("math_round_double", DoubleType(), DoubleType())));
  CEL_ASSIGN_OR_RETURN(
      auto trunc_decl,
      MakeFunctionDecl(
          "math.trunc",
          MakeOverloadDecl("math_trunc_double", DoubleType(), DoubleType())));

  // FP helpers
  CEL_ASSIGN_OR_RETURN(
      auto is_inf_decl,
      MakeFunctionDecl(
          "math.isInf",
          MakeOverloadDecl("math_isInf_double", BoolType(), DoubleType())));

  CEL_ASSIGN_OR_RETURN(
      auto is_nan_decl,
      MakeFunctionDecl(
          "math.isNaN",
          MakeOverloadDecl("math_isNaN_double", BoolType(), DoubleType())));

  CEL_ASSIGN_OR_RETURN(
      auto is_finite_decl,
      MakeFunctionDecl(
          "math.isFinite",
          MakeOverloadDecl("math_isFinite_double", BoolType(), DoubleType())));

  CEL_RETURN_IF_ERROR(builder.AddFunction(ceil_decl));
  CEL_RETURN_IF_ERROR(builder.AddFunction(floor_decl));
  CEL_RETURN_IF_ERROR(builder.AddFunction(round_decl));
  CEL_RETURN_IF_ERROR(builder.AddFunction(trunc_decl));
  CEL_RETURN_IF_ERROR(builder.AddFunction(is_inf_decl));
  CEL_RETURN_IF_ERROR(builder.AddFunction(is_nan_decl));
  CEL_RETURN_IF_ERROR(builder.AddFunction(is_finite_decl));

  return absl::OkStatus();
}

absl::Status AddBitwiseDecls(TypeCheckerBuilder& builder) {
  const Type kBitwiseTypes[] = {IntType(), UintType()};

  FunctionDecl bit_and_decl;
  bit_and_decl.set_name("math.bitAnd");

  FunctionDecl bit_or_decl;
  bit_or_decl.set_name("math.bitOr");

  FunctionDecl bit_xor_decl;
  bit_xor_decl.set_name("math.bitXor");

  FunctionDecl bit_not_decl;
  bit_not_decl.set_name("math.bitNot");

  FunctionDecl bit_lshift_decl;
  bit_lshift_decl.set_name("math.bitShiftLeft");

  FunctionDecl bit_rshift_decl;
  bit_rshift_decl.set_name("math.bitShiftRight");

  for (const Type& type : kBitwiseTypes) {
    CEL_RETURN_IF_ERROR(bit_and_decl.AddOverload(
        MakeOverloadDecl(absl::StrCat("math_bitAnd_", OverloadTypeName(type),
                                      "_", OverloadTypeName(type)),
                         type, type, type)));

    CEL_RETURN_IF_ERROR(bit_or_decl.AddOverload(
        MakeOverloadDecl(absl::StrCat("math_bitOr_", OverloadTypeName(type),
                                      "_", OverloadTypeName(type)),
                         type, type, type)));

    CEL_RETURN_IF_ERROR(bit_xor_decl.AddOverload(
        MakeOverloadDecl(absl::StrCat("math_bitXor_", OverloadTypeName(type),
                                      "_", OverloadTypeName(type)),
                         type, type, type)));

    CEL_RETURN_IF_ERROR(bit_not_decl.AddOverload(
        MakeOverloadDecl(absl::StrCat("math_bitNot_", OverloadTypeName(type),
                                      "_", OverloadTypeName(type)),
                         type, type)));

    CEL_RETURN_IF_ERROR(bit_lshift_decl.AddOverload(MakeOverloadDecl(
        absl::StrCat("math_bitShiftLeft_", OverloadTypeName(type), "_int"),
        type, type, IntType())));

    CEL_RETURN_IF_ERROR(bit_rshift_decl.AddOverload(MakeOverloadDecl(
        absl::StrCat("math_bitShiftRight_", OverloadTypeName(type), "_int"),
        type, type, IntType())));
  }

  CEL_RETURN_IF_ERROR(builder.AddFunction(bit_and_decl));
  CEL_RETURN_IF_ERROR(builder.AddFunction(bit_or_decl));
  CEL_RETURN_IF_ERROR(builder.AddFunction(bit_xor_decl));
  CEL_RETURN_IF_ERROR(builder.AddFunction(bit_not_decl));
  CEL_RETURN_IF_ERROR(builder.AddFunction(bit_lshift_decl));
  CEL_RETURN_IF_ERROR(builder.AddFunction(bit_rshift_decl));

  return absl::OkStatus();
}

absl::Status AddMathExtensionDeclarations(TypeCheckerBuilder& builder) {
  CEL_RETURN_IF_ERROR(AddMinMaxDecls(builder));
  CEL_RETURN_IF_ERROR(AddSignednessDecls(builder));
  CEL_RETURN_IF_ERROR(AddFloatingPointDecls(builder));
  CEL_RETURN_IF_ERROR(AddBitwiseDecls(builder));

  return absl::OkStatus();
}

absl::Status AddMathExtensionMacros(ParserBuilder& builder) {
  for (const auto& m : math_macros()) {
    CEL_RETURN_IF_ERROR(builder.AddMacro(m));
  }
  return absl::OkStatus();
}

}  // namespace

// Configuration for cel::Compiler to enable the math extension declarations.
CompilerLibrary MathCompilerLibrary() {
  return CompilerLibrary(kMathExtensionName, &AddMathExtensionMacros,
                         &AddMathExtensionDeclarations);
}

// Configuration for cel::TypeChecker to enable the math extension declarations.
CheckerLibrary MathCheckerLibrary() {
  return {
      .id = kMathExtensionName,
      .configure = &AddMathExtensionDeclarations,
  };
}

}  // namespace cel::extensions
