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

#include "compiler/compiler_library_subset_factory.h"

#include <string>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "checker/type_checker_subset_factory.h"
#include "compiler/compiler.h"
#include "parser/parser_subset_factory.h"

namespace cel {

CompilerLibrarySubset MakeStdlibSubset(
    absl::flat_hash_set<std::string> macro_names,
    absl::flat_hash_set<std::string> function_overload_ids,
    StdlibSubsetOptions options) {
  CompilerLibrarySubset subset;
  subset.library_id = "stdlib";
  switch (options.macro_list) {
    case cel::StdlibSubsetOptions::ListKind::kInclude:
      subset.should_include_macro =
          IncludeMacrosByNamePredicate(std::move(macro_names));
      break;
    case cel::StdlibSubsetOptions::ListKind::kExclude:
      subset.should_include_macro =
          ExcludeMacrosByNamePredicate(std::move(macro_names));
      break;
    case cel::StdlibSubsetOptions::ListKind::kIgnore:
      subset.should_include_macro = nullptr;
      break;
  }

  switch (options.function_list) {
    case cel::StdlibSubsetOptions::ListKind::kInclude:
      subset.should_include_overload =
          IncludeOverloadsByIdPredicate(std::move(function_overload_ids));
      break;
    case cel::StdlibSubsetOptions::ListKind::kExclude:
      subset.should_include_overload =
          ExcludeOverloadsByIdPredicate(std::move(function_overload_ids));
      break;
    case cel::StdlibSubsetOptions::ListKind::kIgnore:
      subset.should_include_overload = nullptr;
      break;
  }

  return subset;
}

CompilerLibrarySubset MakeStdlibSubset(
    absl::Span<const absl::string_view> macro_names,
    absl::Span<const absl::string_view> function_overload_ids,
    StdlibSubsetOptions options) {
  return MakeStdlibSubset(
      absl::flat_hash_set<std::string>(macro_names.begin(), macro_names.end()),
      absl::flat_hash_set<std::string>(function_overload_ids.begin(),
                                       function_overload_ids.end()),
      options);
}

CompilerLibrarySubset MakeStdlibSubsetByOverloadId(
    absl::Span<const absl::string_view> function_overload_ids,
    StdlibSubsetOptions options) {
  options.macro_list = StdlibSubsetOptions::ListKind::kIgnore;
  return MakeStdlibSubset({}, function_overload_ids, options);
}

CompilerLibrarySubset MakeStdlibSubsetByMacroName(
    absl::Span<const absl::string_view> macro_names,
    StdlibSubsetOptions options) {
  options.function_list = StdlibSubsetOptions::ListKind::kIgnore;
  return MakeStdlibSubset(macro_names, {}, options);
}

}  // namespace cel
