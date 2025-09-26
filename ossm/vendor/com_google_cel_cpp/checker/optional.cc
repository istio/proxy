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

#include "checker/optional.h"

#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/status/status.h"
#include "base/builtins.h"
#include "checker/internal/builtins_arena.h"
#include "checker/type_checker_builder.h"
#include "common/decl.h"
#include "common/type.h"
#include "internal/status_macros.h"

namespace cel {
namespace {

Type OptionalOfV() {
  static const absl::NoDestructor<OptionalType> kInstance(
      checker_internal::BuiltinsArena(), TypeParamType("V"));

  return *kInstance;
}

Type TypeOfOptionalOfV() {
  static const absl::NoDestructor<TypeType> kInstance(
      checker_internal::BuiltinsArena(), OptionalOfV());

  return *kInstance;
}

Type ListOfV() {
  static const absl::NoDestructor<ListType> kInstance(
      checker_internal::BuiltinsArena(), TypeParamType("V"));

  return *kInstance;
}

Type OptionalListOfV() {
  static const absl::NoDestructor<OptionalType> kInstance(
      checker_internal::BuiltinsArena(), ListOfV());

  return *kInstance;
}

Type MapOfKV() {
  static const absl::NoDestructor<MapType> kInstance(
      checker_internal::BuiltinsArena(), TypeParamType("K"),
      TypeParamType("V"));

  return *kInstance;
}

Type OptionalMapOfKV() {
  static const absl::NoDestructor<OptionalType> kInstance(
      checker_internal::BuiltinsArena(), MapOfKV());

  return *kInstance;
}

class OptionalNames {
 public:
  static constexpr char kOptionalType[] = "optional_type";
  static constexpr char kOptionalOf[] = "optional.of";
  static constexpr char kOptionalOfNonZeroValue[] = "optional.ofNonZeroValue";
  static constexpr char kOptionalNone[] = "optional.none";
  static constexpr char kOptionalValue[] = "value";
  static constexpr char kOptionalHasValue[] = "hasValue";
  static constexpr char kOptionalOr[] = "or";
  static constexpr char kOptionalOrValue[] = "orValue";
  static constexpr char kOptionalSelect[] = "_?._";
  static constexpr char kOptionalIndex[] = "_[?_]";
};

class OptionalOverloads {
 public:
  // Creation
  static constexpr char kOptionalOf[] = "optional_of";
  static constexpr char kOptionalOfNonZeroValue[] = "optional_ofNonZeroValue";
  static constexpr char kOptionalNone[] = "optional_none";
  // Basic accessors
  static constexpr char kOptionalValue[] = "optional_value";
  static constexpr char kOptionalHasValue[] = "optional_hasValue";
  // Chaining `or` overloads.
  static constexpr char kOptionalOr[] = "optional_or_optional";
  static constexpr char kOptionalOrValue[] = "optional_orValue_value";
  // Selection
  static constexpr char kOptionalSelect[] = "select_optional_field";
  // Indexing
  static constexpr char kListOptionalIndexInt[] = "list_optindex_optional_int";
  static constexpr char kOptionalListOptionalIndexInt[] =
      "optional_list_optindex_optional_int";
  static constexpr char kMapOptionalIndexValue[] =
      "map_optindex_optional_value";
  static constexpr char kOptionalMapOptionalIndexValue[] =
      "optional_map_optindex_optional_value";
  // Syntactic sugar for chained indexing.
  static constexpr char kOptionalListIndexInt[] = "optional_list_index_int";
  static constexpr char kOptionalMapIndexValue[] = "optional_map_index_value";
};

absl::Status RegisterOptionalDecls(TypeCheckerBuilder& builder) {
  CEL_ASSIGN_OR_RETURN(
      auto of,
      MakeFunctionDecl(OptionalNames::kOptionalOf,
                       MakeOverloadDecl(OptionalOverloads::kOptionalOf,
                                        OptionalOfV(), TypeParamType("V"))));

  CEL_ASSIGN_OR_RETURN(
      auto of_non_zero,
      MakeFunctionDecl(
          OptionalNames::kOptionalOfNonZeroValue,
          MakeOverloadDecl(OptionalOverloads::kOptionalOfNonZeroValue,
                           OptionalOfV(), TypeParamType("V"))));

  CEL_ASSIGN_OR_RETURN(
      auto none,
      MakeFunctionDecl(
          OptionalNames::kOptionalNone,
          MakeOverloadDecl(OptionalOverloads::kOptionalNone, OptionalOfV())));

  CEL_ASSIGN_OR_RETURN(
      auto value, MakeFunctionDecl(OptionalNames::kOptionalValue,
                                   MakeMemberOverloadDecl(
                                       OptionalOverloads::kOptionalValue,
                                       TypeParamType("V"), OptionalOfV())));

  CEL_ASSIGN_OR_RETURN(
      auto has_value, MakeFunctionDecl(OptionalNames::kOptionalHasValue,
                                       MakeMemberOverloadDecl(
                                           OptionalOverloads::kOptionalHasValue,
                                           BoolType(), OptionalOfV())));

  CEL_ASSIGN_OR_RETURN(
      auto or_,
      MakeFunctionDecl(
          OptionalNames::kOptionalOr,
          MakeMemberOverloadDecl(OptionalOverloads::kOptionalOr, OptionalOfV(),
                                 OptionalOfV(), OptionalOfV())));

  CEL_ASSIGN_OR_RETURN(auto or_value,
                       MakeFunctionDecl(OptionalNames::kOptionalOrValue,
                                        MakeMemberOverloadDecl(
                                            OptionalOverloads::kOptionalOrValue,
                                            TypeParamType("V"), OptionalOfV(),
                                            TypeParamType("V"))));

  // This is special cased by the type checker -- just adding a Decl to prevent
  // accidental user overloading.
  CEL_ASSIGN_OR_RETURN(
      auto select,
      MakeFunctionDecl(
          OptionalNames::kOptionalSelect,
          MakeOverloadDecl(OptionalOverloads::kOptionalSelect, OptionalOfV(),
                           DynType(), StringType())));

  CEL_ASSIGN_OR_RETURN(
      auto opt_index,
      MakeFunctionDecl(
          OptionalNames::kOptionalIndex,
          MakeOverloadDecl(OptionalOverloads::kOptionalListOptionalIndexInt,
                           OptionalOfV(), OptionalListOfV(), IntType()),
          MakeOverloadDecl(OptionalOverloads::kListOptionalIndexInt,
                           OptionalOfV(), ListOfV(), IntType()),
          MakeOverloadDecl(OptionalOverloads::kMapOptionalIndexValue,
                           OptionalOfV(), MapOfKV(), TypeParamType("K")),
          MakeOverloadDecl(OptionalOverloads::kOptionalMapOptionalIndexValue,
                           OptionalOfV(), OptionalMapOfKV(),
                           TypeParamType("K"))));

  CEL_ASSIGN_OR_RETURN(
      auto index,
      MakeFunctionDecl(
          cel::builtin::kIndex,
          MakeOverloadDecl(OptionalOverloads::kOptionalListIndexInt,
                           OptionalOfV(), OptionalListOfV(), IntType()),
          MakeOverloadDecl(OptionalOverloads::kOptionalMapIndexValue,
                           OptionalOfV(), OptionalMapOfKV(),
                           TypeParamType("K"))));

  CEL_RETURN_IF_ERROR(builder.AddVariable(
      MakeVariableDecl(OptionalNames::kOptionalType, TypeOfOptionalOfV())));

  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(of)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(of_non_zero)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(none)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(value)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(has_value)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(or_)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(or_value)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(opt_index)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(select)));
  CEL_RETURN_IF_ERROR(builder.MergeFunction(std::move(index)));

  return absl::OkStatus();
}

}  // namespace

CheckerLibrary OptionalCheckerLibrary() {
  return CheckerLibrary({
      "optional",
      &RegisterOptionalDecls,
  });
}

}  // namespace cel
