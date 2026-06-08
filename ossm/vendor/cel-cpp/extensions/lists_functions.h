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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_LISTS_FUNCTIONS_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_LISTS_FUNCTIONS_H_

#include "absl/status/status.h"
#include "checker/type_checker_builder.h"
#include "compiler/compiler.h"
#include "parser/macro_registry.h"
#include "parser/options.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"

namespace cel::extensions {

// Register implementations for list extension functions.
//
// lists.range(n: int) -> list(int)
//
// <list(T)>.distinct() -> list(T)
//
// <list(dyn)>.flatten() -> list(dyn)
// <list(dyn)>.flatten(limit: int) -> list(dyn)
//
// <list(T)>.reverse() -> list(T)
//
// <list(T)>.sort() -> list(T)
//
// <list(T)>.slice(start: int, end: int) -> list(T)
absl::Status RegisterListsFunctions(FunctionRegistry& registry,
                                    const RuntimeOptions& options);

// Register list macros.
//
// <list(T)>.sortBy(<element name>, <element key expression>)
absl::Status RegisterListsMacros(MacroRegistry& registry,
                                 const ParserOptions& options);

// Type check declarations for the lists extension library.
// Provides decls for the following functions:
//
// lists.range(n: int) -> list(int)
//
// <list(T)>.distinct() -> list(T)
//
// <list(dyn)>.flatten() -> list(dyn)
// <list(dyn)>.flatten(limit: int) -> list(dyn)
//
// <list(T)>.reverse() -> list(T)
//
// <list(T_)>.sort() -> list(T_) where T_ is partially orderable
//
// <list(T)>.slice(start: int, end: int) -> list(T)
CheckerLibrary ListsCheckerLibrary();

// Provides decls for the following functions:
//
// lists.range(n: int) -> list(int)
//
// <list(T)>.distinct() -> list(T)
//
// <list(dyn)>.flatten() -> list(dyn)
// <list(dyn)>.flatten(limit: int) -> list(dyn)
//
// <list(T)>.reverse() -> list(T)
//
// <list(T_)>.sort() -> list(T_) where T_ is partially orderable
//
// <list(T)>.slice(start: int, end: int) -> list(T)
//
// and the following macros:
//
// <list(T)>.sortBy(<element name>, <element key expression>)
CompilerLibrary ListsCompilerLibrary();

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_SETS_FUNCTIONS_H_
