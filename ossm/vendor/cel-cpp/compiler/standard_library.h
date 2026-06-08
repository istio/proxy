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

#ifndef THIRD_PARTY_CEL_CPP_COMPILER_STANDARD_LIBRARY_H_
#define THIRD_PARTY_CEL_CPP_COMPILER_STANDARD_LIBRARY_H_

#include "compiler/compiler.h"

namespace cel {

// Returns a CompilerLibrary containing all of the standard CEL declarations
// and macros.
CompilerLibrary StandardCompilerLibrary();

}  // namespace cel
#endif  // THIRD_PARTY_CEL_CPP_COMPILER_STANDARD_LIBRARY_H_
