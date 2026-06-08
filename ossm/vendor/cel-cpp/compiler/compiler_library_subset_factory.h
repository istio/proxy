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

#ifndef THIRD_PARTY_CEL_CPP_COMPILER_COMPILER_LIBRARY_SUBSET_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_COMPILER_COMPILER_LIBRARY_SUBSET_FACTORY_H_

#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "compiler/compiler.h"

namespace cel {

struct StdlibSubsetOptions {
  enum class ListKind {
    // Include the given list of macros or functions, default to exclude.
    kInclude,
    // Exclude the given list of macros or functions, default to include.
    kExclude,
    // Ignore the given list of macros or functions. This is used to clarify
    // intent of an empty list.
    kIgnore
  };
  ListKind macro_list = ListKind::kInclude;
  ListKind function_list = ListKind::kInclude;
};

// Creates a subset of the CEL standard library.
//
// Example usage:
//   // Include only the core boolean operators, and exists/all.
//   // std::unique_ptr<CompilerBuilder> builder = ...;
//   builder->AddLibrary(StandardCompilerLibrary());
//   // Add the subset.
//   builder->AddLibrarySubset(MakeStdlibSubset(
//       {"exists", "all"},
//       {"logical_and", "logical_or", "logical_not", "not_strictly_false",
//       "equal", "inequal"});
//
//   // Exclude list concatenation and map macros.
//   builder->AddLibrarySubset(MakeStdlibSubset(
//       {"map"},
//       {"add_list"},
//       { .macro_list = StdlibSubsetOptions::ListKind::kExclude,
//         .function_list = StdlibSubsetOptions::ListKind::kExclude
//       }));
CompilerLibrarySubset MakeStdlibSubset(
    absl::flat_hash_set<std::string> macro_names,
    absl::flat_hash_set<std::string> function_overload_ids,
    StdlibSubsetOptions options = {});

CompilerLibrarySubset MakeStdlibSubset(
    absl::Span<const absl::string_view> macro_names,
    absl::Span<const absl::string_view> function_overload_ids,
    StdlibSubsetOptions options = {});

CompilerLibrarySubset MakeStdlibSubsetByOverloadId(
    absl::Span<const absl::string_view> function_overload_ids,
    StdlibSubsetOptions options = {});

CompilerLibrarySubset MakeStdlibSubsetByMacroName(
    absl::Span<const absl::string_view> macro_names,
    StdlibSubsetOptions options = {});

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMPILER_COMPILER_LIBRARY_SUBSET_FACTORY_H_
