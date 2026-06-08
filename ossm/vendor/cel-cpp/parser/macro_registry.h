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

#ifndef THIRD_PARTY_CEL_CPP_PARSER_MACRO_REGISTRY_H_
#define THIRD_PARTY_CEL_CPP_PARSER_MACRO_REGISTRY_H_

#include <cstddef>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "parser/macro.h"

namespace cel {

class MacroRegistry final {
 public:
  MacroRegistry() = default;

  // Move-only.
  MacroRegistry(MacroRegistry&&) = default;
  MacroRegistry& operator=(MacroRegistry&&) = default;

  // Registers `macro`.
  absl::Status RegisterMacro(const Macro& macro);

  // Registers all `macros`. If an error is encountered registering one, the
  // rest are not registered and the error is returned.
  absl::Status RegisterMacros(absl::Span<const Macro> macros);

  absl::optional<Macro> FindMacro(absl::string_view name, size_t arg_count,
                                  bool receiver_style) const;

 private:
  bool RegisterMacroImpl(const Macro& macro);

  absl::flat_hash_map<absl::string_view, Macro> macros_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_PARSER_MACRO_REGISTRY_H_
