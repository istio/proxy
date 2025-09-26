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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_MATH_EXT_DECLS_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_MATH_EXT_DECLS_H_

#include "checker/type_checker_builder.h"
#include "compiler/compiler.h"

namespace cel::extensions {

// Configuration for cel::Compiler to enable the math extension declarations.
CompilerLibrary MathCompilerLibrary();

// Configuration for cel::TypeChecker to enable the math extension declarations.
CheckerLibrary MathCheckerLibrary();

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_MATH_EXT_DECLS_H_
