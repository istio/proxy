// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "checker/internal/namespace_generator.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "internal/lexis.h"

namespace cel::checker_internal {
namespace {

bool FieldSelectInterpretationCandidates(
    absl::string_view prefix,
    absl::Span<const std::string> partly_qualified_name,
    absl::FunctionRef<bool(absl::string_view, int)> callback) {
  for (int i = 0; i < partly_qualified_name.size(); ++i) {
    std::string buf;
    int count = partly_qualified_name.size() - i;
    auto end_idx = count - 1;
    auto ident = absl::StrJoin(partly_qualified_name.subspan(0, count), ".");
    absl::string_view candidate = ident;
    if (absl::StartsWith(candidate, ".")) {
      candidate = candidate.substr(1);
    }
    if (!prefix.empty()) {
      buf = absl::StrCat(prefix, ".", candidate);
      candidate = buf;
    }
    if (!callback(candidate, end_idx)) {
      return false;
    }
  }
  return true;
}

}  // namespace

absl::StatusOr<NamespaceGenerator> NamespaceGenerator::Create(
    absl::string_view container) {
  std::vector<std::string> candidates;

  if (container.empty()) {
    return NamespaceGenerator(std::move(candidates));
  }

  if (absl::StartsWith(container, ".")) {
    return absl::InvalidArgumentError("container must not start with a '.'");
  }
  std::string prefix;
  for (auto segment : absl::StrSplit(container, '.')) {
    if (!internal::LexisIsIdentifier(segment)) {
      return absl::InvalidArgumentError(
          "container must only contain valid identifier segments");
    }
    if (prefix.empty()) {
      prefix = segment;
    } else {
      absl::StrAppend(&prefix, ".", segment);
    }
    candidates.push_back(prefix);
  }
  std::reverse(candidates.begin(), candidates.end());
  return NamespaceGenerator(std::move(candidates));
}

void NamespaceGenerator::GenerateCandidates(
    absl::string_view unqualified_name,
    absl::FunctionRef<bool(absl::string_view)> callback) {
  if (absl::StartsWith(unqualified_name, ".")) {
    callback(unqualified_name.substr(1));
    return;
  }
  for (const auto& prefix : candidates_) {
    std::string candidate = absl::StrCat(prefix, ".", unqualified_name);
    if (!callback(candidate)) {
      return;
    }
  }
  callback(unqualified_name);
}

void NamespaceGenerator::GenerateCandidates(
    absl::Span<const std::string> partly_qualified_name,
    absl::FunctionRef<bool(absl::string_view, int)> callback) {
  // Special case for explicit root relative name. e.g. '.com.example.Foo'
  if (!partly_qualified_name.empty() &&
      absl::StartsWith(partly_qualified_name[0], ".")) {
    FieldSelectInterpretationCandidates("", partly_qualified_name, callback);
    return;
  }

  for (const auto& prefix : candidates_) {
    if (!FieldSelectInterpretationCandidates(prefix, partly_qualified_name,
                                             callback)) {
      return;
    }
  }
  FieldSelectInterpretationCandidates("", partly_qualified_name, callback);
}

}  // namespace cel::checker_internal
