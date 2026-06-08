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

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_NAMESPACE_GENERATOR_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_NAMESPACE_GENERATOR_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/functional/function_ref.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace cel::checker_internal {

// Utility class for generating namespace qualified candidates for reference
// resolution.
class NamespaceGenerator {
 public:
  static absl::StatusOr<NamespaceGenerator> Create(absl::string_view container);

  // Copyable and movable.
  NamespaceGenerator(const NamespaceGenerator&) = default;
  NamespaceGenerator& operator=(const NamespaceGenerator&) = default;
  NamespaceGenerator(NamespaceGenerator&&) = default;
  NamespaceGenerator& operator=(NamespaceGenerator&&) = default;

  // For the simple case of an unqualified name, generate all qualified
  // candidates and pass them to the provided callback. The callback may return
  // false to terminate early.
  //
  // The supplied string_view is only valid for the duration of the callback
  // invocation: the callback must handle copying the underlying string if the
  // value needs to be persisted.
  //
  // Example:
  // For container (com.google)
  // and unqualified name foo
  //
  // com.google.foo, com.foo, foo
  void GenerateCandidates(absl::string_view unqualified_name,
                          absl::FunctionRef<bool(absl::string_view)> callback);

  // For a partially qualified name, generate all the qualified candidates in
  // order of resolution precedence and pass them to the provided callback. The
  // callback may return false to terminate early.
  //
  // The supplied string_view is only valid for the duration of the callback
  // invocation: the callback must handle copying the underlying string if the
  // value needs to be persisted.
  //
  // Example:
  // For container (com.google)
  // and partially qualified name Foo.bar
  //
  // (com.google.Foo.bar), <com.google.Foo.bar, 1>
  // (com.google.Foo).bar, <com.google.Foo, 0>
  // (com.Foo.bar), <com.Foo.bar, 1>
  // (com.Foo).bar, <com.Foo, 0>
  // (Foo.bar), <Foo.bar, 1>
  // (Foo).bar, <Foo, 0>
  void GenerateCandidates(
      absl::Span<const std::string> partly_qualified_name,
      absl::FunctionRef<bool(absl::string_view, int)> callback);

 private:
  explicit NamespaceGenerator(std::vector<std::string> candidates)
      : candidates_(std::move(candidates)) {}

  // list of prefixes ordered from most qualified to least.
  std::vector<std::string> candidates_;
};
}  // namespace cel::checker_internal

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_NAMESPACE_GENERATOR_H_
