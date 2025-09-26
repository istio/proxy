// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_ATTRIBUTE_H_
#define THIRD_PARTY_CEL_CPP_BASE_ATTRIBUTE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/kind.h"

namespace cel {

// AttributeQualifier represents a segment in
// attribute resolutuion path. A segment can be qualified by values of
// following types: string/int64_t/uint64_t/bool.
class AttributeQualifier final {
 private:
  struct ComparatorVisitor;

  using Variant = absl::variant<Kind, int64_t, uint64_t, std::string, bool>;

 public:
  static AttributeQualifier OfInt(int64_t value) {
    return AttributeQualifier(absl::in_place_type<int64_t>, std::move(value));
  }

  static AttributeQualifier OfUint(uint64_t value) {
    return AttributeQualifier(absl::in_place_type<uint64_t>, std::move(value));
  }

  static AttributeQualifier OfString(std::string value) {
    return AttributeQualifier(absl::in_place_type<std::string>,
                              std::move(value));
  }

  static AttributeQualifier OfBool(bool value) {
    return AttributeQualifier(absl::in_place_type<bool>, std::move(value));
  }

  AttributeQualifier() = default;

  AttributeQualifier(const AttributeQualifier&) = default;
  AttributeQualifier(AttributeQualifier&&) = default;

  AttributeQualifier& operator=(const AttributeQualifier&) = default;
  AttributeQualifier& operator=(AttributeQualifier&&) = default;

  Kind kind() const;

  // Family of Get... methods. Return values if requested type matches the
  // stored one.
  absl::optional<int64_t> GetInt64Key() const {
    return absl::holds_alternative<int64_t>(value_)
               ? absl::optional<int64_t>(absl::get<1>(value_))
               : absl::nullopt;
  }

  absl::optional<uint64_t> GetUint64Key() const {
    return absl::holds_alternative<uint64_t>(value_)
               ? absl::optional<uint64_t>(absl::get<2>(value_))
               : absl::nullopt;
  }

  absl::optional<absl::string_view> GetStringKey() const {
    return absl::holds_alternative<std::string>(value_)
               ? absl::optional<absl::string_view>(absl::get<3>(value_))
               : absl::nullopt;
  }

  absl::optional<bool> GetBoolKey() const {
    return absl::holds_alternative<bool>(value_)
               ? absl::optional<bool>(absl::get<4>(value_))
               : absl::nullopt;
  }

  bool operator==(const AttributeQualifier& other) const {
    return IsMatch(other);
  }

  bool operator<(const AttributeQualifier& other) const;

  bool IsMatch(absl::string_view other_key) const {
    absl::optional<absl::string_view> key = GetStringKey();
    return (key.has_value() && key.value() == other_key);
  }

 private:
  friend class Attribute;
  friend struct ComparatorVisitor;

  template <typename T>
  AttributeQualifier(absl::in_place_type_t<T> in_place_type, T&& value)
      : value_(in_place_type, std::forward<T>(value)) {}

  bool IsMatch(const AttributeQualifier& other) const;

  // The previous implementation of Attribute preserved all value
  // instances, regardless of whether they are supported in this context or not.
  // We represented unsupported types by using the first alternative and thus
  // preserve backwards compatibility with the result of `type()` above.
  Variant value_;
};

// AttributeQualifierPattern matches a segment in
// attribute resolutuion path. AttributeQualifierPattern is capable of
// matching path elements of types string/int64/uint64/bool.
class AttributeQualifierPattern final {
 private:
  // Qualifier value. If not set, treated as wildcard.
  std::optional<AttributeQualifier> value_;

  explicit AttributeQualifierPattern(std::optional<AttributeQualifier> value)
      : value_(std::move(value)) {}

 public:
  static AttributeQualifierPattern OfInt(int64_t value) {
    return AttributeQualifierPattern(AttributeQualifier::OfInt(value));
  }

  static AttributeQualifierPattern OfUint(uint64_t value) {
    return AttributeQualifierPattern(AttributeQualifier::OfUint(value));
  }

  static AttributeQualifierPattern OfString(std::string value) {
    return AttributeQualifierPattern(
        AttributeQualifier::OfString(std::move(value)));
  }

  static AttributeQualifierPattern OfBool(bool value) {
    return AttributeQualifierPattern(AttributeQualifier::OfBool(value));
  }

  static AttributeQualifierPattern CreateWildcard() {
    return AttributeQualifierPattern(std::nullopt);
  }

  explicit AttributeQualifierPattern(AttributeQualifier qualifier)
      : AttributeQualifierPattern(
            std::optional<AttributeQualifier>(std::move(qualifier))) {}

  bool IsWildcard() const { return !value_.has_value(); }

  bool IsMatch(const AttributeQualifier& qualifier) const {
    if (IsWildcard()) return true;
    return value_.value() == qualifier;
  }

  bool IsMatch(absl::string_view other_key) const {
    if (!value_.has_value()) return true;
    return value_->IsMatch(other_key);
  }
};

// Attribute represents resolved attribute path.
class Attribute final {
 public:
  explicit Attribute(std::string variable_name)
      : Attribute(std::move(variable_name), {}) {}

  Attribute(std::string variable_name,
            std::vector<AttributeQualifier> qualifier_path)
      : impl_(std::make_shared<Impl>(std::move(variable_name),
                                     std::move(qualifier_path))) {}

  absl::string_view variable_name() const { return impl_->variable_name; }

  bool has_variable_name() const { return !impl_->variable_name.empty(); }

  absl::Span<const AttributeQualifier> qualifier_path() const {
    return impl_->qualifier_path;
  }

  bool operator==(const Attribute& other) const;

  bool operator<(const Attribute& other) const;

  const absl::StatusOr<std::string> AsString() const;

 private:
  struct Impl final {
    Impl(std::string variable_name,
         std::vector<AttributeQualifier> qualifier_path)
        : variable_name(std::move(variable_name)),
          qualifier_path(std::move(qualifier_path)) {}

    std::string variable_name;
    std::vector<AttributeQualifier> qualifier_path;
  };

  std::shared_ptr<const Impl> impl_;
};

// AttributePattern is a fully-qualified absolute attribute path pattern.
// Supported segments steps in the path are:
// - field selection;
// - map lookup by key;
// - list access by index.
class AttributePattern final {
 public:
  // MatchType enum specifies how closely pattern is matching the attribute:
  enum class MatchType {
    NONE,     // Pattern does not match attribute itself nor its children
    PARTIAL,  // Pattern matches an entity nested within attribute;
    FULL      // Pattern matches an attribute itself.
  };

  AttributePattern(std::string variable,
                   std::vector<AttributeQualifierPattern> qualifier_path)
      : variable_(std::move(variable)),
        qualifier_path_(std::move(qualifier_path)) {}

  absl::string_view variable() const { return variable_; }

  absl::Span<const AttributeQualifierPattern> qualifier_path() const {
    return qualifier_path_;
  }

  // Matches the pattern to an attribute.
  // Distinguishes between no-match, partial match and full match cases.
  MatchType IsMatch(const Attribute& attribute) const {
    MatchType result = MatchType::NONE;
    if (attribute.variable_name() != variable_) {
      return result;
    }

    auto max_index = qualifier_path().size();
    result = MatchType::FULL;
    if (qualifier_path().size() > attribute.qualifier_path().size()) {
      max_index = attribute.qualifier_path().size();
      result = MatchType::PARTIAL;
    }

    for (size_t i = 0; i < max_index; i++) {
      if (!(qualifier_path()[i].IsMatch(attribute.qualifier_path()[i]))) {
        return MatchType::NONE;
      }
    }
    return result;
  }

 private:
  std::string variable_;
  std::vector<AttributeQualifierPattern> qualifier_path_;
};

struct FieldSpecifier {
  int64_t number;
  std::string name;
};

using SelectQualifier = absl::variant<FieldSpecifier, AttributeQualifier>;

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_ATTRIBUTE_H_
