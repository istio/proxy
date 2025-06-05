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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/types/optional.h"
#include "base/kind.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "internal/status_macros.h"
#include "runtime/function_overload_reference.h"
#include "runtime/function_registry.h"
#include "runtime/type_registry.h"

namespace google::api::expr::runtime {

using ::cel::Value;

Resolver::Resolver(
    absl::string_view container, const cel::FunctionRegistry& function_registry,
    const cel::TypeRegistry&, cel::ValueManager& value_factory,
    const absl::flat_hash_map<std::string, cel::TypeRegistry::Enumeration>&
        resolveable_enums,
    bool resolve_qualified_type_identifiers)
    : namespace_prefixes_(),
      enum_value_map_(),
      function_registry_(function_registry),
      value_factory_(value_factory),
      resolveable_enums_(resolveable_enums),
      resolve_qualified_type_identifiers_(resolve_qualified_type_identifiers) {
  // The constructor for the registry determines the set of possible namespace
  // prefixes which may appear within the given expression container, and also
  // eagerly maps possible enum names to enum values.

  auto container_elements = absl::StrSplit(container, '.');
  std::string prefix = "";
  namespace_prefixes_.push_back(prefix);
  for (const auto& elem : container_elements) {
    // Tolerate trailing / leading '.'.
    if (elem.empty()) {
      continue;
    }
    absl::StrAppend(&prefix, elem, ".");
    namespace_prefixes_.insert(namespace_prefixes_.begin(), prefix);
  }

  for (const auto& prefix : namespace_prefixes_) {
    for (auto iter = resolveable_enums_.begin();
         iter != resolveable_enums_.end(); ++iter) {
      absl::string_view enum_name = iter->first;
      if (!absl::StartsWith(enum_name, prefix)) {
        continue;
      }

      auto remainder = absl::StripPrefix(enum_name, prefix);
      const auto& enum_type = iter->second;

      for (const auto& enumerator : enum_type.enumerators) {
        auto key = absl::StrCat(remainder, !remainder.empty() ? "." : "",
                                enumerator.name);
        enum_value_map_[key] = value_factory.CreateIntValue(enumerator.number);
      }
    }
  }
}

std::vector<std::string> Resolver::FullyQualifiedNames(absl::string_view name,
                                                       int64_t expr_id) const {
  // TODO: refactor the reference resolution into this method.
  // and handle the case where this id is in the reference map as either a
  // function name or identifier name.
  std::vector<std::string> names;
  // Handle the case where the name contains a leading '.' indicating it is
  // already fully-qualified.
  if (absl::StartsWith(name, ".")) {
    std::string fully_qualified_name = std::string(name.substr(1));
    names.push_back(fully_qualified_name);
    return names;
  }

  // namespace prefixes is guaranteed to contain at least empty string, so this
  // function will always produce at least one result.
  for (const auto& prefix : namespace_prefixes_) {
    std::string fully_qualified_name = absl::StrCat(prefix, name);
    names.push_back(fully_qualified_name);
  }
  return names;
}

absl::optional<cel::Value> Resolver::FindConstant(absl::string_view name,
                                                  int64_t expr_id) const {
  auto names = FullyQualifiedNames(name, expr_id);
  for (const auto& name : names) {
    // Attempt to resolve the fully qualified name to a known enum.
    auto enum_entry = enum_value_map_.find(name);
    if (enum_entry != enum_value_map_.end()) {
      return enum_entry->second;
    }
    // Conditionally resolve fully qualified names as type values if the option
    // to do so is configured in the expression builder. If the type name is
    // not qualified, then it too may be returned as a constant value.
    if (resolve_qualified_type_identifiers_ || !absl::StrContains(name, ".")) {
      auto type_value = value_factory_.FindType(name);
      if (type_value.ok() && type_value->has_value()) {
        return value_factory_.CreateTypeValue(**type_value);
      }
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

absl::StatusOr<absl::optional<std::pair<std::string, cel::Type>>>
Resolver::FindType(absl::string_view name, int64_t expr_id) const {
  auto qualified_names = FullyQualifiedNames(name, expr_id);
  for (auto& qualified_name : qualified_names) {
    CEL_ASSIGN_OR_RETURN(auto maybe_type,
                         value_factory_.FindType(qualified_name));
    if (maybe_type.has_value()) {
      return std::make_pair(std::move(qualified_name), std::move(*maybe_type));
    }
  }
  return absl::nullopt;
}

}  // namespace google::api::expr::runtime
