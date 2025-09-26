// Copyright 2021 Google LLC
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

#include "eval/compiler/resolver.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/kind.h"
#include "common/type.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "internal/status_macros.h"
#include "runtime/function_overload_reference.h"
#include "runtime/function_registry.h"
#include "runtime/type_registry.h"

namespace google::api::expr::runtime {
namespace {

using ::cel::TypeValue;
using ::cel::Value;
using ::cel::runtime_internal::GetEnumValueTable;

std::vector<std::string> MakeNamespaceCandidates(absl::string_view container) {
  std::vector<std::string> namespace_prefixes;
  std::string prefix = "";
  namespace_prefixes.push_back(prefix);
  auto container_elements = absl::StrSplit(container, '.');
  for (const auto& elem : container_elements) {
    // Tolerate trailing / leading '.'.
    if (elem.empty()) {
      continue;
    }
    absl::StrAppend(&prefix, elem, ".");
    // longest prefix first.
    namespace_prefixes.insert(namespace_prefixes.begin(), prefix);
  }
  return namespace_prefixes;
}

}  // namespace

Resolver::Resolver(absl::string_view container,
                   const cel::FunctionRegistry& function_registry,
                   const cel::TypeRegistry& type_registry,
                   const cel::TypeReflector& type_reflector,
                   bool resolve_qualified_type_identifiers)
    : namespace_prefixes_(MakeNamespaceCandidates(container)),
      enum_value_map_(GetEnumValueTable(type_registry)),
      function_registry_(function_registry),
      type_reflector_(type_reflector),
      resolve_qualified_type_identifiers_(resolve_qualified_type_identifiers) {}

std::vector<std::string> Resolver::FullyQualifiedNames(absl::string_view name,
                                                       int64_t expr_id) const {
  // TODO(issues/105): refactor the reference resolution into this method.
  // and handle the case where this id is in the reference map as either a
  // function name or identifier name.
  std::vector<std::string> names;

  auto prefixes = GetPrefixesFor(name);
  names.reserve(prefixes.size());
  for (const auto& prefix : prefixes) {
    std::string fully_qualified_name = absl::StrCat(prefix, name);
    names.push_back(fully_qualified_name);
  }
  return names;
}

absl::Span<const std::string> Resolver::GetPrefixesFor(
    absl::string_view& name) const {
  static const absl::NoDestructor<std::string> kEmptyPrefix("");
  if (absl::StartsWith(name, ".")) {
    name = name.substr(1);
    return absl::MakeConstSpan(kEmptyPrefix.get(), 1);
  }
  return namespace_prefixes_;
}

absl::optional<cel::Value> Resolver::FindConstant(absl::string_view name,
                                                  int64_t expr_id) const {
  auto prefixes = GetPrefixesFor(name);
  for (const auto& prefix : prefixes) {
    std::string qualified_name = absl::StrCat(prefix, name);
    // Attempt to resolve the fully qualified name to a known enum.
    auto enum_entry = enum_value_map_->find(qualified_name);
    if (enum_entry != enum_value_map_->end()) {
      return enum_entry->second;
    }
    // Attempt to resolve the fully qualified name to a known type.
    if (resolve_qualified_type_identifiers_) {
      auto type_value = type_reflector_.FindType(qualified_name);
      if (type_value.ok() && type_value->has_value()) {
        return TypeValue(**type_value);
      }
    }
  }

  if (!resolve_qualified_type_identifiers_ && !absl::StrContains(name, '.')) {
    auto type_value = type_reflector_.FindType(name);

    if (type_value.ok() && type_value->has_value()) {
      return TypeValue(**type_value);
    }
  }
  return absl::nullopt;
}

std::vector<cel::FunctionOverloadReference> Resolver::FindOverloads(
    absl::string_view name, bool receiver_style,
    const std::vector<cel::Kind>& types, int64_t expr_id) const {
  // Resolve the fully qualified names and then search the function registry
  // for possible matches.
  std::vector<cel::FunctionOverloadReference> funcs;
  auto names = FullyQualifiedNames(name, expr_id);
  for (auto it = names.begin(); it != names.end(); it++) {
    // Only one set of overloads is returned along the namespace hierarchy as
    // the function name resolution follows the same behavior as variable name
    // resolution, meaning the most specific definition wins. This is different
    // from how C++ namespaces work, as they will accumulate the overload set
    // over the namespace hierarchy.
    funcs = function_registry_.FindStaticOverloads(*it, receiver_style, types);
    if (!funcs.empty()) {
      return funcs;
    }
  }
  return funcs;
}

std::vector<cel::FunctionOverloadReference> Resolver::FindOverloads(
    absl::string_view name, bool receiver_style, size_t arity,
    int64_t expr_id) const {
  std::vector<cel::FunctionOverloadReference> funcs;
  auto prefixes = GetPrefixesFor(name);
  for (const auto& prefix : prefixes) {
    std::string qualified_name = absl::StrCat(prefix, name);
    // Only one set of overloads is returned along the namespace hierarchy as
    // the function name resolution follows the same behavior as variable name
    // resolution, meaning the most specific definition wins. This is different
    // from how C++ namespaces work, as they will accumulate the overload set
    // over the namespace hierarchy.
    funcs = function_registry_.FindStaticOverloadsByArity(
        qualified_name, receiver_style, arity);
    if (!funcs.empty()) {
      return funcs;
    }
  }
  return funcs;
}

std::vector<cel::FunctionRegistry::LazyOverload> Resolver::FindLazyOverloads(
    absl::string_view name, bool receiver_style,
    const std::vector<cel::Kind>& types, int64_t expr_id) const {
  // Resolve the fully qualified names and then search the function registry
  // for possible matches.
  std::vector<cel::FunctionRegistry::LazyOverload> funcs;
  auto names = FullyQualifiedNames(name, expr_id);
  for (const auto& name : names) {
    funcs = function_registry_.FindLazyOverloads(name, receiver_style, types);
    if (!funcs.empty()) {
      return funcs;
    }
  }
  return funcs;
}

std::vector<cel::FunctionRegistry::LazyOverload> Resolver::FindLazyOverloads(
    absl::string_view name, bool receiver_style, size_t arity,
    int64_t expr_id) const {
  std::vector<cel::FunctionRegistry::LazyOverload> funcs;
  auto prefixes = GetPrefixesFor(name);
  for (const auto& prefix : prefixes) {
    std::string qualified_name = absl::StrCat(prefix, name);
    funcs = function_registry_.FindLazyOverloadsByArity(name, receiver_style,
                                                        arity);
    if (!funcs.empty()) {
      return funcs;
    }
  }
  return funcs;
}

absl::StatusOr<absl::optional<std::pair<std::string, cel::Type>>>
Resolver::FindType(absl::string_view name, int64_t expr_id) const {
  auto prefixes = GetPrefixesFor(name);
  for (auto& prefix : prefixes) {
    std::string qualified_name = absl::StrCat(prefix, name);
    CEL_ASSIGN_OR_RETURN(auto maybe_type,
                         type_reflector_.FindType(qualified_name));
    if (maybe_type.has_value()) {
      return std::make_pair(std::move(qualified_name), std::move(*maybe_type));
    }
  }
  return absl::nullopt;
}

}  // namespace google::api::expr::runtime
