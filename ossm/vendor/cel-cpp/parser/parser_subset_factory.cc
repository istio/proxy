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

#include "parser/parser_subset_factory.h"

#include <string>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "parser/macro.h"
#include "parser/parser_interface.h"

namespace cel {

cel::ParserLibrarySubset::MacroPredicate IncludeMacrosByNamePredicate(
    absl::flat_hash_set<std::string> macro_names) {
  return [macro_names_set = std::move(macro_names)](const Macro& macro) {
    return macro_names_set.contains(macro.function());
  };
}

cel::ParserLibrarySubset::MacroPredicate IncludeMacrosByNamePredicate(
    absl::Span<const absl::string_view> macro_names) {
  return IncludeMacrosByNamePredicate(
      absl::flat_hash_set<std::string>(macro_names.begin(), macro_names.end()));
}

cel::ParserLibrarySubset::MacroPredicate ExcludeMacrosByNamePredicate(
    absl::flat_hash_set<std::string> macro_names) {
  return [macro_names_set = std::move(macro_names)](const Macro& macro) {
    return !macro_names_set.contains(macro.function());
  };
}

cel::ParserLibrarySubset::MacroPredicate ExcludeMacrosByNamePredicate(
    absl::Span<const absl::string_view> macro_names) {
  return ExcludeMacrosByNamePredicate(
      absl::flat_hash_set<std::string>(macro_names.begin(), macro_names.end()));
}

}  // namespace cel
