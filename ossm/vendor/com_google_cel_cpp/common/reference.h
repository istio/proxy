// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_REFERENCE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_REFERENCE_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "common/constant.h"

namespace cel {

class Reference;
class VariableReference;
class FunctionReference;

using ReferenceKind = absl::variant<VariableReference, FunctionReference>;

// `VariableReference` is a resolved reference to a `VariableDecl`.
class VariableReference final {
 public:
  bool has_value() const { return value_.has_value(); }

  void set_value(Constant value) { value_ = std::move(value); }

  const Constant& value() const ABSL_ATTRIBUTE_LIFETIME_BOUND { return value_; }

  Constant& mutable_value() ABSL_ATTRIBUTE_LIFETIME_BOUND { return value_; }

  ABSL_MUST_USE_RESULT Constant release_value() {
    using std::swap;
    Constant value;
    swap(mutable_value(), value);
    return value;
  }

  friend void swap(VariableReference& lhs, VariableReference& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

 private:
  friend class Reference;

  static const VariableReference& default_instance();

  Constant value_;
};

inline bool operator==(const VariableReference& lhs,
                       const VariableReference& rhs) {
  return lhs.value() == rhs.value();
}

inline bool operator!=(const VariableReference& lhs,
                       const VariableReference& rhs) {
  return !operator==(lhs, rhs);
}

// `FunctionReference` is a resolved reference to a `FunctionDecl`.
class FunctionReference final {
 public:
  const std::vector<std::string>& overloads() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return overloads_;
  }

  void set_overloads(std::vector<std::string> overloads) {
    mutable_overloads() = std::move(overloads);
  }

  std::vector<std::string>& mutable_overloads() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return overloads_;
  }

  ABSL_MUST_USE_RESULT std::vector<std::string> release_overloads() {
    std::vector<std::string> overloads;
    overloads.swap(mutable_overloads());
    return overloads;
  }

  friend void swap(FunctionReference& lhs, FunctionReference& rhs) noexcept {
    using std::swap;
    swap(lhs.overloads_, rhs.overloads_);
  }

 private:
  friend class Reference;

  static const FunctionReference& default_instance();

  std::vector<std::string> overloads_;
};

inline bool operator==(const FunctionReference& lhs,
                       const FunctionReference& rhs) {
  return absl::c_equal(lhs.overloads(), rhs.overloads());
}

inline bool operator!=(const FunctionReference& lhs,
                       const FunctionReference& rhs) {
  return !operator==(lhs, rhs);
}

// `Reference` is a resolved reference to a `VariableDecl` or `FunctionDecl`. By
// default `Reference` is a `VariableReference`.
class Reference final {
 public:
  const std::string& name() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return name_;
  }

  void set_name(std::string name) { name_ = std::move(name); }

  void set_name(absl::string_view name) {
    name_.assign(name.data(), name.size());
  }

  void set_name(const char* name) { set_name(absl::NullSafeStringView(name)); }

  ABSL_MUST_USE_RESULT std::string release_name() {
    std::string name;
    name.swap(name_);
    return name;
  }

  void set_kind(ReferenceKind kind) { kind_ = std::move(kind); }

  const ReferenceKind& kind() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return kind_;
  }

  ReferenceKind& mutable_kind() ABSL_ATTRIBUTE_LIFETIME_BOUND { return kind_; }

  ABSL_MUST_USE_RESULT ReferenceKind release_kind() {
    using std::swap;
    ReferenceKind kind;
    swap(kind, kind_);
    return kind;
  }

  ABSL_MUST_USE_RESULT bool has_variable() const {
    return absl::holds_alternative<VariableReference>(kind());
  }

  ABSL_MUST_USE_RESULT const VariableReference& variable() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    if (const auto* alt = absl::get_if<VariableReference>(&kind()); alt) {
      return *alt;
    }
    return VariableReference::default_instance();
  }

  void set_variable(VariableReference variable) {
    mutable_variable() = std::move(variable);
  }

  VariableReference& mutable_variable() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    if (!has_variable()) {
      mutable_kind().emplace<VariableReference>();
    }
    return absl::get<VariableReference>(mutable_kind());
  }

  ABSL_MUST_USE_RESULT VariableReference release_variable() {
    VariableReference variable_reference;
    if (auto* alt = absl::get_if<VariableReference>(&mutable_kind()); alt) {
      variable_reference = std::move(*alt);
    }
    mutable_kind().emplace<VariableReference>();
    return variable_reference;
  }

  ABSL_MUST_USE_RESULT bool has_function() const {
    return absl::holds_alternative<FunctionReference>(kind());
  }

  ABSL_MUST_USE_RESULT const FunctionReference& function() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    if (const auto* alt = absl::get_if<FunctionReference>(&kind()); alt) {
      return *alt;
    }
    return FunctionReference::default_instance();
  }

  void set_function(FunctionReference function) {
    mutable_function() = std::move(function);
  }

  FunctionReference& mutable_function() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    if (!has_function()) {
      mutable_kind().emplace<FunctionReference>();
    }
    return absl::get<FunctionReference>(mutable_kind());
  }

  ABSL_MUST_USE_RESULT FunctionReference release_function() {
    FunctionReference function_reference;
    if (auto* alt = absl::get_if<FunctionReference>(&mutable_kind()); alt) {
      function_reference = std::move(*alt);
    }
    mutable_kind().emplace<VariableReference>();
    return function_reference;
  }

  friend void swap(Reference& lhs, Reference& rhs) noexcept {
    using std::swap;
    swap(lhs.name_, rhs.name_);
    swap(lhs.kind_, rhs.kind_);
  }

 private:
  std::string name_;
  ReferenceKind kind_;
};

inline bool operator==(const Reference& lhs, const Reference& rhs) {
  return lhs.name() == rhs.name() && lhs.kind() == rhs.kind();
}

inline bool operator!=(const Reference& lhs, const Reference& rhs) {
  return !operator==(lhs, rhs);
}

inline Reference MakeVariableReference(std::string name) {
  Reference reference;
  reference.set_name(std::move(name));
  reference.mutable_kind().emplace<VariableReference>();
  return reference;
}

inline Reference MakeConstantVariableReference(std::string name,
                                               Constant constant) {
  Reference reference;
  reference.set_name(std::move(name));
  reference.mutable_kind().emplace<VariableReference>().set_value(
      std::move(constant));
  return reference;
}

inline Reference MakeFunctionReference(std::string name,
                                       std::vector<std::string> overloads) {
  Reference reference;
  reference.set_name(std::move(name));
  reference.mutable_kind().emplace<FunctionReference>().set_overloads(
      std::move(overloads));
  return reference;
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_REFERENCE_H_
