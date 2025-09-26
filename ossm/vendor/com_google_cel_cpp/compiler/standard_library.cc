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

#include "compiler/standard_library.h"

#include "absl/status/status.h"
#include "checker/standard_library.h"
#include "compiler/compiler.h"
#include "internal/status_macros.h"
#include "parser/macro.h"
#include "parser/parser_interface.h"

namespace cel {

namespace {

absl::Status AddStandardLibraryMacros(ParserBuilder& builder) {
  // For consistency with the Parse free functions, follow the convenience
  // option to disable all the standard macros.
  if (builder.GetOptions().disable_standard_macros) {
    return absl::OkStatus();
  }
  for (const auto& macro : Macro::AllMacros()) {
    CEL_RETURN_IF_ERROR(builder.AddMacro(macro));
  }
  return absl::OkStatus();
}

}  // namespace

CompilerLibrary StandardCompilerLibrary() {
  CompilerLibrary library =
      CompilerLibrary::FromCheckerLibrary(StandardCheckerLibrary());
  library.configure_parser = AddStandardLibraryMacros;
  return library;
}

}  // namespace cel
