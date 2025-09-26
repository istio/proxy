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

#include "parser/macro_registry.h"

#include <cstddef>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "parser/macro.h"

namespace cel {

absl::Status MacroRegistry::RegisterMacro(const Macro& macro) {
  if (!RegisterMacroImpl(macro)) {
    return absl::AlreadyExistsError(
        absl::StrCat("macro already exists: ", macro.key()));
  }
  return absl::OkStatus();
}

absl::Status MacroRegistry::RegisterMacros(absl::Span<const Macro> macros) {
  for (size_t i = 0; i < macros.size(); ++i) {
    const auto& macro = macros[i];
    if (!RegisterMacroImpl(macro)) {
      for (size_t j = 0; j < i; ++j) {
        macros_.erase(macros[j].key());
      }
      return absl::AlreadyExistsError(
          absl::StrCat("macro already exists: ", macro.key()));
    }
  }
  return absl::OkStatus();
}

absl::optional<Macro> MacroRegistry::FindMacro(absl::string_view name,
                                               size_t arg_count,
                                               bool receiver_style) const {
  // <function>:<argument_count>:<receiver_style>
  if (name.empty() || absl::StrContains(name, ':')) {
    return absl::nullopt;
  }
  // Try argument count specific key first.
  auto key = absl::StrCat(name, ":", arg_count, ":",
                          receiver_style ? "true" : "false");
  if (auto it = macros_.find(key); it != macros_.end()) {
    return it->second;
  }
  // Next try variadic.
  key = absl::StrCat(name, ":*:", receiver_style ? "true" : "false");
  if (auto it = macros_.find(key); it != macros_.end()) {
    return it->second;
  }
  return absl::nullopt;
}

bool MacroRegistry::RegisterMacroImpl(const Macro& macro) {
  return macros_.insert(std::pair{macro.key(), macro}).second;
}

}  // namespace cel
