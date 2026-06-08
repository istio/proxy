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

#include "extensions/comprehensions_v2.h"

#include "absl/base/no_destructor.h"
#include "absl/status/status.h"
#include "checker/internal/builtins_arena.h"
#include "checker/type_checker_builder.h"
#include "common/decl.h"
#include "common/type.h"
#include "compiler/compiler.h"
#include "extensions/comprehensions_v2_macros.h"
#include "internal/status_macros.h"
#include "parser/parser_interface.h"

using ::cel::checker_internal::BuiltinsArena;

namespace cel::extensions {

namespace {

// Arbitrary type parameter name A.
TypeParamType TypeParamA() { return TypeParamType("A"); }

// Arbitrary type parameter name B.
TypeParamType TypeParamB() { return TypeParamType("B"); }

Type MapOfAB() {
  static absl::NoDestructor<Type> kInstance(
      MapType(BuiltinsArena(), TypeParamA(), TypeParamB()));
  return *kInstance;
}

absl::Status AddComprehensionsV2Functions(TypeCheckerBuilder& builder) {
  FunctionDecl map_insert;
  map_insert.set_name("cel.@mapInsert");
  CEL_RETURN_IF_ERROR(map_insert.AddOverload(
      MakeOverloadDecl("@mapInsert_map_key_value", MapOfAB(), MapOfAB(),
                       TypeParamA(), TypeParamB())));
  CEL_RETURN_IF_ERROR(map_insert.AddOverload(
      MakeOverloadDecl("@mapInsert_map_map", MapOfAB(), MapOfAB(), MapOfAB())));
  return builder.AddFunction(map_insert);
}

absl::Status ConfigureParser(ParserBuilder& parser_builder) {
  return RegisterComprehensionsV2Macros(parser_builder);
}

}  // namespace

CompilerLibrary ComprehensionsV2CompilerLibrary() {
  return CompilerLibrary("cel.lib.ext.comprev2", &ConfigureParser,
                         &AddComprehensionsV2Functions);
}

}  // namespace cel::extensions
