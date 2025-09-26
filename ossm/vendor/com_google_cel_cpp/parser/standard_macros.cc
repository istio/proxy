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

#include "parser/standard_macros.h"

#include "absl/status/status.h"
#include "internal/status_macros.h"
#include "parser/macro.h"
#include "parser/macro_registry.h"
#include "parser/options.h"

namespace cel {

absl::Status RegisterStandardMacros(MacroRegistry& registry,
                                    const ParserOptions& options) {
  CEL_RETURN_IF_ERROR(registry.RegisterMacro(HasMacro()));
  CEL_RETURN_IF_ERROR(registry.RegisterMacro(AllMacro()));
  CEL_RETURN_IF_ERROR(registry.RegisterMacro(ExistsMacro()));
  CEL_RETURN_IF_ERROR(registry.RegisterMacro(ExistsOneMacro()));
  CEL_RETURN_IF_ERROR(registry.RegisterMacro(Map2Macro()));
  CEL_RETURN_IF_ERROR(registry.RegisterMacro(Map3Macro()));
  CEL_RETURN_IF_ERROR(registry.RegisterMacro(FilterMacro()));
  if (options.enable_optional_syntax) {
    CEL_RETURN_IF_ERROR(registry.RegisterMacro(OptMapMacro()));
    CEL_RETURN_IF_ERROR(registry.RegisterMacro(OptFlatMapMacro()));
  }
  return absl::OkStatus();
}

}  // namespace cel
