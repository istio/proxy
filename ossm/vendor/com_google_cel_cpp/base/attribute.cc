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

#include "base/attribute.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/base/macros.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/variant.h"
#include "base/kind.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

// Visitor for appending string representation for different qualifier kinds.
class AttributeStringPrinter {
 public:
  // String representation for the given qualifier is appended to output.
  // output must be non-null.
  explicit AttributeStringPrinter(std::string* output, Kind type)
      : output_(*output), type_(type) {}

  absl::Status operator()(const Kind& ignored) const {
    // Attributes are represented as a variant, with illegal attribute
    // qualifiers represented with their type as the first alternative.
    return absl::InvalidArgumentError(
        absl::StrCat("Unsupported attribute qualifier ", KindToString(type_)));
  }

  absl::Status operator()(int64_t index) {
    absl::StrAppend(&output_, "[", index, "]");
    return absl::OkStatus();
  }

  absl::Status operator()(uint64_t index) {
    absl::StrAppend(&output_, "[", index, "]");
    return absl::OkStatus();
  }

  absl::Status operator()(bool bool_key) {
    absl::StrAppend(&output_, "[", (bool_key) ? "true" : "false", "]");
    return absl::OkStatus();
  }

  absl::Status operator()(const std::string& field) {
    absl::StrAppend(&output_, ".", field);
    return absl::OkStatus();
  }

 private:
  std::string& output_;
  Kind type_;
};

struct AttributeQualifierTypeVisitor final {
  Kind operator()(const Kind& type) const { return type; }

  Kind operator()(int64_t ignored) const {
    static_cast<void>(ignored);
    return Kind::kInt64;
  }

  Kind operator()(uint64_t ignored) const {
    static_cast<void>(ignored);
    return Kind::kUint64;
  }

  Kind operator()(const std::string& ignored) const {
    static_cast<void>(ignored);
    return Kind::kString;
  }

  Kind operator()(bool ignored) const {
    static_cast<void>(ignored);
    return Kind::kBool;
  }
};

struct AttributeQualifierTypeComparator final {
  const Kind lhs;

  bool operator()(const Kind& rhs) const {
    return static_cast<int>(lhs) < static_cast<int>(rhs);
  }

  bool operator()(int64_t) const { return false; }

  bool operator()(uint64_t other) const { return false; }

  bool operator()(const std::string&) const { return false; }

  bool operator()(bool other) const { return false; }
};

struct AttributeQualifierIntComparator final {
  const int64_t lhs;

  bool operator()(const Kind&) const { return true; }

  bool operator()(int64_t rhs) const { return lhs < rhs; }

  bool operator()(uint64_t) const { return true; }

  bool operator()(const std::string&) const { return true; }

  bool operator()(bool) const { return false; }
};

struct AttributeQualifierUintComparator final {
  const uint64_t lhs;

  bool operator()(const Kind&) const { return true; }

  bool operator()(int64_t) const { return false; }

  bool operator()(uint64_t rhs) const { return lhs < rhs; }

  bool operator()(const std::string&) const { return true; }

  bool operator()(bool) const { return false; }
};

struct AttributeQualifierStringComparator final {
  const std::string& lhs;

  bool operator()(const Kind&) const { return true; }

  bool operator()(int64_t) const { return false; }

  bool operator()(uint64_t) const { return false; }

  bool operator()(const std::string& rhs) const { return lhs < rhs; }

  bool operator()(bool) const { return false; }
};

struct AttributeQualifierBoolComparator final {
  const bool lhs;

  bool operator()(const Kind&) const { return true; }

  bool operator()(int64_t) const { return true; }

  bool operator()(uint64_t) const { return true; }

  bool operator()(const std::string&) const { return true; }

  bool operator()(bool rhs) const { return lhs < rhs; }
};

}  // namespace

struct AttributeQualifier::ComparatorVisitor final {
  const AttributeQualifier::Variant& rhs;

  bool operator()(const Kind& lhs) const {
    return absl::visit(AttributeQualifierTypeComparator{lhs}, rhs);
  }

  bool operator()(int64_t lhs) const {
    return absl::visit(AttributeQualifierIntComparator{lhs}, rhs);
  }

  bool operator()(uint64_t lhs) const {
    return absl::visit(AttributeQualifierUintComparator{lhs}, rhs);
  }

  bool operator()(const std::string& lhs) const {
    return absl::visit(AttributeQualifierStringComparator{lhs}, rhs);
  }

  bool operator()(bool lhs) const {
    return absl::visit(AttributeQualifierBoolComparator{lhs}, rhs);
  }
};

Kind AttributeQualifier::kind() const {
  return absl::visit(AttributeQualifierTypeVisitor{}, value_);
}

bool AttributeQualifier::operator<(const AttributeQualifier& other) const {
  // The order is not publicly documented because it is subject to change.
  // Currently we sort in the following order, with each type being sorted
  // against itself: bool, int, uint, string, type.
  return absl::visit(ComparatorVisitor{other.value_}, value_);
}

bool Attribute::operator==(const Attribute& other) const {
  // We cannot check pointer equality as a short circuit because we have to
  // treat all invalid AttributeQualifier as not equal to each other.
  // TODO we only support Ident-rooted attributes at the moment.
  if (variable_name() != other.variable_name()) {
    return false;
  }

  if (qualifier_path().size() != other.qualifier_path().size()) {
    return false;
  }

  for (size_t i = 0; i < qualifier_path().size(); i++) {
    if (!(qualifier_path()[i] == other.qualifier_path()[i])) {
      return false;
    }
  }

  return true;
}

bool Attribute::operator<(const Attribute& other) const {
  if (impl_.get() == other.impl_.get()) {
    return false;
  }
  auto lhs_begin = qualifier_path().begin();
  auto lhs_end = qualifier_path().end();
  auto rhs_begin = other.qualifier_path().begin();
  auto rhs_end = other.qualifier_path().end();
  while (lhs_begin != lhs_end && rhs_begin != rhs_end) {
    if (*lhs_begin < *rhs_begin) {
      return true;
    }
    if (!(*lhs_begin == *rhs_begin)) {
      return false;
    }
    lhs_begin++;
    rhs_begin++;
  }
  if (lhs_begin == lhs_end && rhs_begin == rhs_end) {
    // Neither has any elements left, they are equal. Compare variable names.
    return variable_name() < other.variable_name();
  }
  if (lhs_begin == lhs_end) {
    // Left has no more elements. Right is greater.
    return true;
  }
  // Right has no more elements. Left is greater.
  ABSL_ASSERT(rhs_begin == rhs_end);
  return false;
}

const absl::StatusOr<std::string> Attribute::AsString() const {
  if (variable_name().empty()) {
    return absl::InvalidArgumentError(
        "Only ident rooted attributes are supported.");
  }

  std::string result = std::string(variable_name());

  for (const auto& qualifier : qualifier_path()) {
    CEL_RETURN_IF_ERROR(absl::visit(
        AttributeStringPrinter(&result, qualifier.kind()), qualifier.value_));
  }

  return result;
}

bool AttributeQualifier::IsMatch(const AttributeQualifier& other) const {
  if (absl::holds_alternative<Kind>(value_) ||
      absl::holds_alternative<Kind>(other.value_)) {
    return false;
  }
  return value_ == other.value_;
}

}  // namespace cel
