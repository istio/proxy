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

#ifndef THIRD_PARTY_CEL_CPP_PARSER_PARSER_SUBSET_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_PARSER_PARSER_SUBSET_FACTORY_H_

#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "parser/parser_interface.h"

namespace cel {

// Predicate that only includes the given macro by name.
cel::ParserLibrarySubset::MacroPredicate IncludeMacrosByNamePredicate(
    absl::flat_hash_set<std::string> macro_names);
cel::ParserLibrarySubset::MacroPredicate IncludeMacrosByNamePredicate(
    absl::Span<const absl::string_view> macro_names);

// Predicate that excludes the given macros by name.
cel::ParserLibrarySubset::MacroPredicate ExcludeMacrosByNamePredicate(
    absl::flat_hash_set<std::string> macro_names);
cel::ParserLibrarySubset::MacroPredicate ExcludeMacrosByNamePredicate(
    absl::Span<const absl::string_view> macro_names);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_PARSER_PARSER_SUBSET_FACTORY_H_
