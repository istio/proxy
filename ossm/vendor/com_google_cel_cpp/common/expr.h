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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_EXPR_H_
#define THIRD_PARTY_CEL_CPP_COMMON_EXPR_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "common/constant.h"

namespace cel {

using ExprId = int64_t;

class Expr;
class UnspecifiedExpr;
class IdentExpr;
class SelectExpr;
class CallExpr;
class ListExprElement;
class ListExpr;
class StructExprField;
class StructExpr;
class MapExprEntry;
class MapExpr;
class ComprehensionExpr;

inline constexpr absl::string_view kAccumulatorVariableName = "__result__";

bool operator==(const Expr& lhs, const Expr& rhs);

inline bool operator!=(const Expr& lhs, const Expr& rhs) {
  return !operator==(lhs, rhs);
}

bool operator==(const ListExprElement& lhs, const ListExprElement& rhs);

inline bool operator!=(const ListExprElement& lhs, const ListExprElement& rhs) {
  return !operator==(lhs, rhs);
}

bool operator==(const StructExprField& lhs, const StructExprField& rhs);

inline bool operator!=(const StructExprField& lhs, const StructExprField& rhs) {
  return !operator==(lhs, rhs);
}

bool operator==(const MapExprEntry& lhs, const MapExprEntry& rhs);

inline bool operator!=(const MapExprEntry& lhs, const MapExprEntry& rhs) {
  return !operator==(lhs, rhs);
}

// `UnspecifiedExpr` is the default alternative of `Expr`. It is used for
// default construction of `Expr` or as a placeholder for when errors occur.
class UnspecifiedExpr final {
 public:
  UnspecifiedExpr() = default;
  UnspecifiedExpr(UnspecifiedExpr&&) = default;
  UnspecifiedExpr& operator=(UnspecifiedExpr&&) = default;

  UnspecifiedExpr(const UnspecifiedExpr&) = delete;
  UnspecifiedExpr& operator=(const UnspecifiedExpr&) = delete;

  void Clear() {}

  friend void swap(UnspecifiedExpr&, UnspecifiedExpr&) noexcept {}

 private:
  friend class Expr;

  static const UnspecifiedExpr& default_instance();
};

inline bool operator==(const UnspecifiedExpr&, const UnspecifiedExpr&) {
  return true;
}

inline bool operator!=(const UnspecifiedExpr& lhs, const UnspecifiedExpr& rhs) {
  return !operator==(lhs, rhs);
}

// `IdentExpr` is an alternative of `Expr`, representing an identifier.
class IdentExpr final {
 public:
  IdentExpr() = default;
  IdentExpr(IdentExpr&&) = default;
  IdentExpr& operator=(IdentExpr&&) = default;

  explicit IdentExpr(std::string name) { set_name(std::move(name)); }

  explicit IdentExpr(absl::string_view name) { set_name(name); }

  explicit IdentExpr(const char* name) { set_name(name); }

  IdentExpr(const IdentExpr&) = delete;
  IdentExpr& operator=(const IdentExpr&) = delete;

  void Clear() { name_.clear(); }

  // Holds a single, unqualified identifier, possibly preceded by a '.'.
  //
  // Qualified names are represented by the [Expr.Select][] expression.
  ABSL_MUST_USE_RESULT const std::string& name() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return name_;
  }

  void set_name(std::string name) { name_ = std::move(name); }

  void set_name(absl::string_view name) {
    name_.assign(name.data(), name.size());
  }

  void set_name(const char* name) { set_name(absl::NullSafeStringView(name)); }

  ABSL_MUST_USE_RESULT std::string release_name() { return release(name_); }

  friend void swap(IdentExpr& lhs, IdentExpr& rhs) noexcept {
    using std::swap;
    swap(lhs.name_, rhs.name_);
  }

 private:
  friend class Expr;

  static const IdentExpr& default_instance();

  static std::string release(std::string& property) {
    std::string result;
    result.swap(property);
    return result;
  }

  std::string name_;
};

inline bool operator==(const IdentExpr& lhs, const IdentExpr& rhs) {
  return lhs.name() == rhs.name();
}

inline bool operator!=(const IdentExpr& lhs, const IdentExpr& rhs) {
  return !operator==(lhs, rhs);
}

// `SelectExpr` is an alternative of `Expr`, representing field access.
class SelectExpr final {
 public:
  SelectExpr() = default;
  SelectExpr(SelectExpr&&) = default;
  SelectExpr& operator=(SelectExpr&&) = default;

  SelectExpr(const SelectExpr&) = delete;
  SelectExpr& operator=(const SelectExpr&) = delete;

  void Clear() {
    operand_.reset();
    field_.clear();
    test_only_ = false;
  }

  ABSL_MUST_USE_RESULT bool has_operand() const { return operand_ != nullptr; }

  // The target of the selection expression.
  //
  // For example, in the select expression `request.auth`, the `request`
  // portion of the expression is the `operand`.
  ABSL_MUST_USE_RESULT const Expr& operand() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Expr& mutable_operand() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  void set_operand(Expr operand);

  void set_operand(std::unique_ptr<Expr> operand);

  ABSL_MUST_USE_RESULT std::unique_ptr<Expr> release_operand() {
    return release(operand_);
  }

  // The name of the field to select.
  //
  // For example, in the select expression `request.auth`, the `auth` portion
  // of the expression would be the `field`.
  ABSL_MUST_USE_RESULT const std::string& field() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return field_;
  }

  void set_field(std::string field) { field_ = std::move(field); }

  void set_field(absl::string_view field) {
    field_.assign(field.data(), field.size());
  }

  void set_field(const char* field) {
    set_field(absl::NullSafeStringView(field));
  }

  ABSL_MUST_USE_RESULT std::string release_field() { return release(field_); }

  // Whether the select is to be interpreted as a field presence test.
  //
  // This results from the macro `has(request.auth)`.
  ABSL_MUST_USE_RESULT bool test_only() const { return test_only_; }

  void set_test_only(bool test_only) { test_only_ = test_only; }

  friend void swap(SelectExpr& lhs, SelectExpr& rhs) noexcept {
    using std::swap;
    swap(lhs.operand_, rhs.operand_);
    swap(lhs.field_, rhs.field_);
    swap(lhs.test_only_, rhs.test_only_);
  }

 private:
  friend class Expr;

  static const SelectExpr& default_instance();

  static std::string release(std::string& property) {
    std::string result;
    result.swap(property);
    return result;
  }

  static std::unique_ptr<Expr> release(std::unique_ptr<Expr>& property) {
    std::unique_ptr<Expr> result;
    result.swap(property);
    return result;
  }

  std::unique_ptr<Expr> operand_;
  std::string field_;
  bool test_only_ = false;
};

inline bool operator==(const SelectExpr& lhs, const SelectExpr& rhs) {
  return lhs.operand() == rhs.operand() && lhs.field() == rhs.field() &&
         lhs.test_only() == rhs.test_only();
}

inline bool operator!=(const SelectExpr& lhs, const SelectExpr& rhs) {
  return !operator==(lhs, rhs);
}

// `CallExpr` is an alternative of `Expr`, representing a function call.
class CallExpr final {
 public:
  CallExpr() = default;
  CallExpr(CallExpr&&) = default;
  CallExpr& operator=(CallExpr&&) = default;

  CallExpr(const CallExpr&) = delete;
  CallExpr& operator=(const CallExpr&) = delete;

  void Clear();

  // The name of the function or method being called.
  ABSL_MUST_USE_RESULT const std::string& function() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return function_;
  }

  void set_function(std::string function) { function_ = std::move(function); }

  void set_function(absl::string_view function) {
    function_.assign(function.data(), function.size());
  }

  void set_function(const char* function) {
    set_function(absl::NullSafeStringView(function));
  }

  ABSL_MUST_USE_RESULT std::string release_function() {
    return release(function_);
  }

  ABSL_MUST_USE_RESULT bool has_target() const { return target_ != nullptr; }

  // The target of an method call-style expression. For example, `x` in `x.f()`.
  ABSL_MUST_USE_RESULT const Expr& target() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Expr& mutable_target() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  void set_target(Expr target);

  void set_target(std::unique_ptr<Expr> target);

  ABSL_MUST_USE_RESULT std::unique_ptr<Expr> release_target() {
    return release(target_);
  }

  // The arguments.
  ABSL_MUST_USE_RESULT const std::vector<Expr>& args() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return args_;
  }

  ABSL_MUST_USE_RESULT std::vector<Expr>& mutable_args()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return args_;
  }

  void set_args(std::vector<Expr> args);

  void set_args(absl::Span<Expr> args);

  Expr& add_args() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  ABSL_MUST_USE_RESULT std::vector<Expr> release_args();

  friend void swap(CallExpr& lhs, CallExpr& rhs) noexcept {
    using std::swap;
    swap(lhs.function_, rhs.function_);
    swap(lhs.target_, rhs.target_);
    swap(lhs.args_, rhs.args_);
  }

 private:
  friend class Expr;

  static const CallExpr& default_instance();

  static std::string release(std::string& property) {
    std::string result;
    result.swap(property);
    return result;
  }

  static std::unique_ptr<Expr> release(std::unique_ptr<Expr>& property) {
    std::unique_ptr<Expr> result;
    result.swap(property);
    return result;
  }

  std::string function_;
  std::unique_ptr<Expr> target_;
  std::vector<Expr> args_;
};

bool operator==(const CallExpr& lhs, const CallExpr& rhs);

inline bool operator!=(const CallExpr& lhs, const CallExpr& rhs) {
  return !operator==(lhs, rhs);
}

// `ListExprElement` represents an element in `ListExpr`.
class ListExprElement final {
 public:
  ListExprElement() = default;
  ListExprElement(ListExprElement&&) = default;
  ListExprElement& operator=(ListExprElement&&) = default;

  ListExprElement(const ListExprElement&) = delete;
  ListExprElement& operator=(const ListExprElement&) = delete;

  void Clear();

  ABSL_MUST_USE_RESULT bool has_expr() const { return expr_ != nullptr; }

  ABSL_MUST_USE_RESULT const Expr& expr() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  ABSL_MUST_USE_RESULT Expr& mutable_expr() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  void set_expr(Expr expr);

  void set_expr(std::unique_ptr<Expr> expr);

  ABSL_MUST_USE_RESULT Expr release_expr();

  ABSL_MUST_USE_RESULT bool optional() const { return optional_; }

  void set_optional(bool optional) { optional_ = optional; }

  friend void swap(ListExprElement& lhs, ListExprElement& rhs) noexcept;

 private:
  static Expr release(std::unique_ptr<Expr>& property);

  std::unique_ptr<Expr> expr_;
  bool optional_ = false;
};

// `ListExpr` is an alternative of `Expr`, representing a list.
class ListExpr final {
 public:
  ListExpr() = default;
  ListExpr(ListExpr&&) = default;
  ListExpr& operator=(ListExpr&&) = default;

  ListExpr(const ListExpr&) = delete;
  ListExpr& operator=(const ListExpr&) = delete;

  void Clear();

  // The elements of the list.
  ABSL_MUST_USE_RESULT const std::vector<ListExprElement>& elements() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return elements_;
  }

  ABSL_MUST_USE_RESULT std::vector<ListExprElement>& mutable_elements()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return elements_;
  }

  void set_elements(std::vector<ListExprElement> elements);

  void set_elements(absl::Span<ListExprElement> elements);

  ListExprElement& add_elements() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  ABSL_MUST_USE_RESULT std::vector<ListExprElement> release_elements();

  friend void swap(ListExpr& lhs, ListExpr& rhs) noexcept {
    using std::swap;
    swap(lhs.elements_, rhs.elements_);
  }

 private:
  friend class Expr;

  static const ListExpr& default_instance();

  std::vector<ListExprElement> elements_;
};

bool operator==(const ListExpr& lhs, const ListExpr& rhs);

inline bool operator!=(const ListExpr& lhs, const ListExpr& rhs) {
  return !operator==(lhs, rhs);
}

// `StructExprField` represents a field in `StructExpr`.
class StructExprField final {
 public:
  StructExprField() = default;
  StructExprField(StructExprField&&) = default;
  StructExprField& operator=(StructExprField&&) = default;

  StructExprField(const StructExprField&) = delete;
  StructExprField& operator=(const StructExprField&) = delete;

  void Clear();

  ABSL_MUST_USE_RESULT ExprId id() const { return id_; }

  void set_id(ExprId id) { id_ = id; }

  ABSL_MUST_USE_RESULT const std::string& name() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return name_;
  }

  void set_name(std::string name) { name_ = std::move(name); }

  void set_name(absl::string_view name) {
    name_.assign(name.data(), name.size());
  }

  void set_name(const char* name) { set_name(absl::NullSafeStringView(name)); }

  ABSL_MUST_USE_RESULT std::string release_name() { return std::move(name_); }

  ABSL_MUST_USE_RESULT bool has_value() const { return value_ != nullptr; }

  ABSL_MUST_USE_RESULT const Expr& value() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  ABSL_MUST_USE_RESULT Expr& mutable_value() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  void set_value(Expr value);

  void set_value(std::unique_ptr<Expr> value);

  ABSL_MUST_USE_RESULT Expr release_value();

  ABSL_MUST_USE_RESULT bool optional() const { return optional_; }

  void set_optional(bool optional) { optional_ = optional; }

  friend void swap(StructExprField& lhs, StructExprField& rhs) noexcept;

 private:
  static Expr release(std::unique_ptr<Expr>& property);

  ExprId id_ = 0;
  std::string name_;
  std::unique_ptr<Expr> value_;
  bool optional_ = false;
};

// `StructExpr` is an alternative of `Expr`, representing a struct.
class StructExpr final {
 public:
  StructExpr() = default;
  StructExpr(StructExpr&&) = default;
  StructExpr& operator=(StructExpr&&) = default;

  StructExpr(const StructExpr&) = delete;
  StructExpr& operator=(const StructExpr&) = delete;

  void Clear();

  // The type name of the struct to be created.
  ABSL_MUST_USE_RESULT const std::string& name() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return name_;
  }

  void set_name(std::string name) { name_ = std::move(name); }

  void set_name(absl::string_view name) {
    name_.assign(name.data(), name.size());
  }

  void set_name(const char* name) { set_name(absl::NullSafeStringView(name)); }

  ABSL_MUST_USE_RESULT std::string release_name() { return release(name_); }

  // The fields of the struct.
  ABSL_MUST_USE_RESULT const std::vector<StructExprField>& fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return fields_;
  }

  ABSL_MUST_USE_RESULT std::vector<StructExprField>& mutable_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return fields_;
  }

  void set_fields(std::vector<StructExprField> fields);

  void set_fields(absl::Span<StructExprField> fields);

  StructExprField& add_fields() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  ABSL_MUST_USE_RESULT std::vector<StructExprField> release_fields();

  friend void swap(StructExpr& lhs, StructExpr& rhs) noexcept {
    using std::swap;
    swap(lhs.name_, rhs.name_);
    swap(lhs.fields_, rhs.fields_);
  }

 private:
  friend class Expr;

  static const StructExpr& default_instance();

  static std::string release(std::string& property) {
    std::string result;
    result.swap(property);
    return result;
  }

  std::string name_;
  std::vector<StructExprField> fields_;
};

bool operator==(const StructExpr& lhs, const StructExpr& rhs);

inline bool operator!=(const StructExpr& lhs, const StructExpr& rhs) {
  return !operator==(lhs, rhs);
}

// `MapExprEntry` represents an entry in `MapExpr`.
class MapExprEntry final {
 public:
  MapExprEntry() = default;
  MapExprEntry(MapExprEntry&&) = default;
  MapExprEntry& operator=(MapExprEntry&&) = default;

  MapExprEntry(const MapExprEntry&) = delete;
  MapExprEntry& operator=(const MapExprEntry&) = delete;

  void Clear();

  ABSL_MUST_USE_RESULT ExprId id() const { return id_; }

  void set_id(ExprId id) { id_ = id; }

  ABSL_MUST_USE_RESULT bool has_key() const { return key_ != nullptr; }

  ABSL_MUST_USE_RESULT const Expr& key() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  ABSL_MUST_USE_RESULT Expr& mutable_key() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  void set_key(Expr key);

  void set_key(std::unique_ptr<Expr> key);

  ABSL_MUST_USE_RESULT Expr release_key();

  ABSL_MUST_USE_RESULT bool has_value() const { return value_ != nullptr; }

  ABSL_MUST_USE_RESULT const Expr& value() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  ABSL_MUST_USE_RESULT Expr& mutable_value() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  void set_value(Expr value);

  void set_value(std::unique_ptr<Expr> value);

  ABSL_MUST_USE_RESULT Expr release_value();

  ABSL_MUST_USE_RESULT bool optional() const { return optional_; }

  void set_optional(bool optional) { optional_ = optional; }

  friend void swap(MapExprEntry& lhs, MapExprEntry& rhs) noexcept;

 private:
  static Expr release(std::unique_ptr<Expr>& property);

  ExprId id_ = 0;
  std::unique_ptr<Expr> key_;
  std::unique_ptr<Expr> value_;
  bool optional_ = false;
};

// `MapExpr` is an alternative of `Expr`, representing a map.
class MapExpr final {
 public:
  MapExpr() = default;
  MapExpr(MapExpr&&) = default;
  MapExpr& operator=(MapExpr&&) = default;

  MapExpr(const MapExpr&) = delete;
  MapExpr& operator=(const MapExpr&) = delete;

  void Clear();

  // The entries of the map.
  ABSL_MUST_USE_RESULT const std::vector<MapExprEntry>& entries() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return entries_;
  }

  ABSL_MUST_USE_RESULT std::vector<MapExprEntry>& mutable_entries()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return entries_;
  }

  void set_entries(std::vector<MapExprEntry> entries);

  void set_entries(absl::Span<MapExprEntry> entries);

  MapExprEntry& add_entries() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  ABSL_MUST_USE_RESULT std::vector<MapExprEntry> release_entries();

  friend void swap(MapExpr& lhs, MapExpr& rhs) noexcept {
    using std::swap;
    swap(lhs.entries_, rhs.entries_);
  }

 private:
  friend class Expr;

  static const MapExpr& default_instance();

  std::vector<MapExprEntry> entries_;
};

bool operator==(const MapExpr& lhs, const MapExpr& rhs);

inline bool operator!=(const MapExpr& lhs, const MapExpr& rhs) {
  return !operator==(lhs, rhs);
}

// `ComprehensionExpr` is an alternative of `Expr`, representing a
// comprehension. These are always synthetic as there is no way to express them
// directly in the Common Expression Language, and are created by macros.
class ComprehensionExpr final {
 public:
  ComprehensionExpr() = default;
  ComprehensionExpr(ComprehensionExpr&&) = default;
  ComprehensionExpr& operator=(ComprehensionExpr&&) = default;

  ComprehensionExpr(const ComprehensionExpr&) = delete;
  ComprehensionExpr& operator=(const ComprehensionExpr&) = delete;

  void Clear() {
    iter_var_.clear();
    iter_range_.reset();
    accu_var_.clear();
    accu_init_.reset();
    loop_condition_.reset();
    loop_step_.reset();
    result_.reset();
  }

  ABSL_MUST_USE_RESULT const std::string& iter_var() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return iter_var_;
  }

  void set_iter_var(std::string iter_var) { iter_var_ = std::move(iter_var); }

  void set_iter_var(absl::string_view iter_var) {
    iter_var_.assign(iter_var.data(), iter_var.size());
  }

  void set_iter_var(const char* iter_var) {
    set_iter_var(absl::NullSafeStringView(iter_var));
  }

  ABSL_MUST_USE_RESULT std::string release_iter_var() {
    return release(iter_var_);
  }

  ABSL_MUST_USE_RESULT const std::string& iter_var2() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return iter_var2_;
  }

  void set_iter_var2(std::string iter_var2) {
    iter_var2_ = std::move(iter_var2);
  }

  void set_iter_var2(absl::string_view iter_var2) {
    iter_var2_.assign(iter_var2.data(), iter_var2.size());
  }

  void set_iter_var2(const char* iter_var2) {
    set_iter_var2(absl::NullSafeStringView(iter_var2));
  }

  ABSL_MUST_USE_RESULT std::string release_iter_var2() {
    return release(iter_var2_);
  }

  ABSL_MUST_USE_RESULT bool has_iter_range() const {
    return iter_range_ != nullptr;
  }

  ABSL_MUST_USE_RESULT const Expr& iter_range() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Expr& mutable_iter_range() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  void set_iter_range(Expr iter_range);

  void set_iter_range(std::unique_ptr<Expr> iter_range);

  ABSL_MUST_USE_RESULT std::unique_ptr<Expr> release_iter_range() {
    return release(iter_range_);
  }

  ABSL_MUST_USE_RESULT const std::string& accu_var() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return accu_var_;
  }

  void set_accu_var(std::string accu_var) { accu_var_ = std::move(accu_var); }

  void set_accu_var(absl::string_view accu_var) {
    accu_var_.assign(accu_var.data(), accu_var.size());
  }

  void set_accu_var(const char* accu_var) {
    set_accu_var(absl::NullSafeStringView(accu_var));
  }

  ABSL_MUST_USE_RESULT std::string release_accu_var() {
    return release(accu_var_);
  }

  ABSL_MUST_USE_RESULT bool has_accu_init() const {
    return accu_init_ != nullptr;
  }

  ABSL_MUST_USE_RESULT const Expr& accu_init() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Expr& mutable_accu_init() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  void set_accu_init(Expr accu_init);

  void set_accu_init(std::unique_ptr<Expr> accu_init);

  ABSL_MUST_USE_RESULT std::unique_ptr<Expr> release_accu_init() {
    return release(accu_init_);
  }

  ABSL_MUST_USE_RESULT bool has_loop_condition() const {
    return loop_condition_ != nullptr;
  }

  ABSL_MUST_USE_RESULT const Expr& loop_condition() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Expr& mutable_loop_condition() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  void set_loop_condition(Expr loop_condition);

  void set_loop_condition(std::unique_ptr<Expr> loop_condition);

  ABSL_MUST_USE_RESULT std::unique_ptr<Expr> release_loop_condition() {
    return release(loop_condition_);
  }

  ABSL_MUST_USE_RESULT bool has_loop_step() const {
    return loop_step_ != nullptr;
  }

  ABSL_MUST_USE_RESULT const Expr& loop_step() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Expr& mutable_loop_step() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  void set_loop_step(Expr loop_step);

  void set_loop_step(std::unique_ptr<Expr> loop_step);

  ABSL_MUST_USE_RESULT std::unique_ptr<Expr> release_loop_step() {
    return release(loop_step_);
  }

  ABSL_MUST_USE_RESULT bool has_result() const { return result_ != nullptr; }

  ABSL_MUST_USE_RESULT const Expr& result() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Expr& mutable_result() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  void set_result(Expr result);

  void set_result(std::unique_ptr<Expr> result);

  ABSL_MUST_USE_RESULT std::unique_ptr<Expr> release_result() {
    return release(result_);
  }

  friend void swap(ComprehensionExpr& lhs, ComprehensionExpr& rhs) noexcept {
    using std::swap;
    swap(lhs.iter_var_, rhs.iter_var_);
    swap(lhs.iter_var2_, rhs.iter_var2_);
    swap(lhs.iter_range_, rhs.iter_range_);
    swap(lhs.accu_var_, rhs.accu_var_);
    swap(lhs.accu_init_, rhs.accu_init_);
    swap(lhs.loop_condition_, rhs.loop_condition_);
    swap(lhs.loop_step_, rhs.loop_step_);
    swap(lhs.result_, rhs.result_);
  }

 private:
  friend class Expr;

  static const ComprehensionExpr& default_instance();

  static std::string release(std::string& property) {
    std::string result;
    result.swap(property);
    return result;
  }

  static std::unique_ptr<Expr> release(std::unique_ptr<Expr>& property) {
    std::unique_ptr<Expr> result;
    result.swap(property);
    return result;
  }

  std::string iter_var_;
  std::string iter_var2_;
  std::unique_ptr<Expr> iter_range_;
  std::string accu_var_;
  std::unique_ptr<Expr> accu_init_;
  std::unique_ptr<Expr> loop_condition_;
  std::unique_ptr<Expr> loop_step_;
  std::unique_ptr<Expr> result_;
};

inline bool operator==(const ComprehensionExpr& lhs,
                       const ComprehensionExpr& rhs) {
  return lhs.iter_var() == rhs.iter_var() &&
         lhs.iter_range() == rhs.iter_range() &&
         lhs.accu_var() == rhs.accu_var() &&
         lhs.accu_init() == rhs.accu_init() &&
         lhs.loop_condition() == rhs.loop_condition() &&
         lhs.loop_step() == rhs.loop_step() && lhs.result() == rhs.result();
}

inline bool operator!=(const ComprehensionExpr& lhs,
                       const ComprehensionExpr& rhs) {
  return !operator==(lhs, rhs);
}

using ExprKind =
    absl::variant<UnspecifiedExpr, Constant, IdentExpr, SelectExpr, CallExpr,
                  ListExpr, StructExpr, MapExpr, ComprehensionExpr>;

enum class ExprKindCase {
  kUnspecifiedExpr,
  kConstant,
  kIdentExpr,
  kSelectExpr,
  kCallExpr,
  kListExpr,
  kStructExpr,
  kMapExpr,
  kComprehensionExpr,
};

// `Expr` is a node in the Common Expression Language's abstract syntax tree. It
// is composed of a numeric ID and a kind variant.
class Expr final {
 public:
  Expr() = default;
  Expr(Expr&&) = default;
  Expr& operator=(Expr&&);

  Expr(const Expr&) = delete;
  Expr& operator=(const Expr&) = delete;

  void Clear();

  ABSL_MUST_USE_RESULT ExprId id() const { return id_; }

  void set_id(ExprId id) { id_ = id; }

  ABSL_MUST_USE_RESULT const ExprKind& kind() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return kind_;
  }

  ABSL_MUST_USE_RESULT ExprKind& mutable_kind() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return kind_;
  }

  void set_kind(ExprKind kind);

  ABSL_MUST_USE_RESULT ExprKind release_kind();

  ABSL_MUST_USE_RESULT bool has_const_expr() const {
    return absl::holds_alternative<Constant>(kind());
  }

  ABSL_MUST_USE_RESULT const Constant& const_expr() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return get_kind<Constant>();
  }

  Constant& mutable_const_expr() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return try_emplace_kind<Constant>();
  }

  void set_const_expr(Constant const_expr) {
    try_emplace_kind<Constant>() = std::move(const_expr);
  }

  ABSL_MUST_USE_RESULT Constant release_const_expr() {
    return release_kind<Constant>();
  }

  ABSL_MUST_USE_RESULT bool has_ident_expr() const {
    return absl::holds_alternative<IdentExpr>(kind());
  }

  ABSL_MUST_USE_RESULT const IdentExpr& ident_expr() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return get_kind<IdentExpr>();
  }

  IdentExpr& mutable_ident_expr() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return try_emplace_kind<IdentExpr>();
  }

  void set_ident_expr(IdentExpr ident_expr) {
    try_emplace_kind<IdentExpr>() = std::move(ident_expr);
  }

  ABSL_MUST_USE_RESULT IdentExpr release_ident_expr() {
    return release_kind<IdentExpr>();
  }

  ABSL_MUST_USE_RESULT bool has_select_expr() const {
    return absl::holds_alternative<SelectExpr>(kind());
  }

  ABSL_MUST_USE_RESULT const SelectExpr& select_expr() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return get_kind<SelectExpr>();
  }

  SelectExpr& mutable_select_expr() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return try_emplace_kind<SelectExpr>();
  }

  void set_select_expr(SelectExpr select_expr) {
    try_emplace_kind<SelectExpr>() = std::move(select_expr);
  }

  ABSL_MUST_USE_RESULT SelectExpr release_select_expr() {
    return release_kind<SelectExpr>();
  }

  ABSL_MUST_USE_RESULT bool has_call_expr() const {
    return absl::holds_alternative<CallExpr>(kind());
  }

  ABSL_MUST_USE_RESULT const CallExpr& call_expr() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return get_kind<CallExpr>();
  }

  CallExpr& mutable_call_expr() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return try_emplace_kind<CallExpr>();
  }

  void set_call_expr(CallExpr call_expr);

  ABSL_MUST_USE_RESULT CallExpr release_call_expr();

  ABSL_MUST_USE_RESULT bool has_list_expr() const {
    return absl::holds_alternative<ListExpr>(kind());
  }

  ABSL_MUST_USE_RESULT const ListExpr& list_expr() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return get_kind<ListExpr>();
  }

  ListExpr& mutable_list_expr() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return try_emplace_kind<ListExpr>();
  }

  void set_list_expr(ListExpr list_expr);

  ABSL_MUST_USE_RESULT ListExpr release_list_expr();

  ABSL_MUST_USE_RESULT bool has_struct_expr() const {
    return absl::holds_alternative<StructExpr>(kind());
  }

  ABSL_MUST_USE_RESULT const StructExpr& struct_expr() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return get_kind<StructExpr>();
  }

  StructExpr& mutable_struct_expr() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return try_emplace_kind<StructExpr>();
  }

  void set_struct_expr(StructExpr struct_expr);

  ABSL_MUST_USE_RESULT StructExpr release_struct_expr();

  ABSL_MUST_USE_RESULT bool has_map_expr() const {
    return absl::holds_alternative<MapExpr>(kind());
  }

  ABSL_MUST_USE_RESULT const MapExpr& map_expr() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return get_kind<MapExpr>();
  }

  MapExpr& mutable_map_expr() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return try_emplace_kind<MapExpr>();
  }

  void set_map_expr(MapExpr map_expr);

  ABSL_MUST_USE_RESULT MapExpr release_map_expr();

  ABSL_MUST_USE_RESULT bool has_comprehension_expr() const {
    return absl::holds_alternative<ComprehensionExpr>(kind());
  }

  ABSL_MUST_USE_RESULT const ComprehensionExpr& comprehension_expr() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return get_kind<ComprehensionExpr>();
  }

  ComprehensionExpr& mutable_comprehension_expr()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return try_emplace_kind<ComprehensionExpr>();
  }

  void set_comprehension_expr(ComprehensionExpr comprehension_expr) {
    try_emplace_kind<ComprehensionExpr>() = std::move(comprehension_expr);
  }

  ABSL_MUST_USE_RESULT ComprehensionExpr release_comprehension_expr() {
    return release_kind<ComprehensionExpr>();
  }

  ExprKindCase kind_case() const;

  friend void swap(Expr& lhs, Expr& rhs) noexcept;

 private:
  friend class IdentExpr;
  friend class SelectExpr;
  friend class CallExpr;
  friend class ListExpr;
  friend class StructExpr;
  friend class MapExpr;
  friend class ComprehensionExpr;
  friend class ListExprElement;
  friend class StructExprField;
  friend class MapExprEntry;

  static const Expr& default_instance();

  template <typename T, typename... Args>
  ABSL_MUST_USE_RESULT T& try_emplace_kind(Args&&... args)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    if (auto* alt = absl::get_if<T>(&mutable_kind()); alt) {
      return *alt;
    }
    return kind_.emplace<T>(std::forward<Args>(args)...);
  }

  template <typename T>
  ABSL_MUST_USE_RESULT const T& get_kind() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    if (const auto* alt = absl::get_if<T>(&kind()); alt) {
      return *alt;
    }
    return T::default_instance();
  }

  template <typename T>
  ABSL_MUST_USE_RESULT T release_kind();

  ExprId id_ = 0;
  ExprKind kind_;
};

inline bool operator==(const Expr& lhs, const Expr& rhs) {
  return lhs.id() == rhs.id() && lhs.kind() == rhs.kind();
}

inline bool operator==(const CallExpr& lhs, const CallExpr& rhs) {
  return lhs.function() == rhs.function() && lhs.target() == rhs.target() &&
         absl::c_equal(lhs.args(), rhs.args());
}

inline const Expr& SelectExpr::operand() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return has_operand() ? *operand_ : Expr::default_instance();
}

inline Expr& SelectExpr::mutable_operand() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  if (!has_operand()) {
    operand_ = std::make_unique<Expr>();
  }
  return *operand_;
}

inline void SelectExpr::set_operand(Expr operand) {
  mutable_operand() = std::move(operand);
}

inline void SelectExpr::set_operand(std::unique_ptr<Expr> operand) {
  operand_ = std::move(operand);
}

inline const Expr& ComprehensionExpr::iter_range() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return has_iter_range() ? *iter_range_ : Expr::default_instance();
}

inline Expr& ComprehensionExpr::mutable_iter_range()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  if (!has_iter_range()) {
    iter_range_ = std::make_unique<Expr>();
  }
  return *iter_range_;
}

inline void ComprehensionExpr::set_iter_range(Expr iter_range) {
  mutable_iter_range() = std::move(iter_range);
}

inline void ComprehensionExpr::set_iter_range(
    std::unique_ptr<Expr> iter_range) {
  iter_range_ = std::move(iter_range);
}

inline const Expr& ComprehensionExpr::accu_init() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return has_accu_init() ? *accu_init_ : Expr::default_instance();
}

inline Expr& ComprehensionExpr::mutable_accu_init()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  if (!has_accu_init()) {
    accu_init_ = std::make_unique<Expr>();
  }
  return *accu_init_;
}

inline void ComprehensionExpr::set_accu_init(Expr accu_init) {
  mutable_accu_init() = std::move(accu_init);
}

inline void ComprehensionExpr::set_accu_init(std::unique_ptr<Expr> accu_init) {
  accu_init_ = std::move(accu_init);
}

inline const Expr& ComprehensionExpr::loop_step() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return has_loop_step() ? *loop_step_ : Expr::default_instance();
}

inline Expr& ComprehensionExpr::mutable_loop_step()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  if (!has_loop_step()) {
    loop_step_ = std::make_unique<Expr>();
  }
  return *loop_step_;
}

inline void ComprehensionExpr::set_loop_step(Expr loop_step) {
  mutable_loop_step() = std::move(loop_step);
}

inline void ComprehensionExpr::set_loop_step(std::unique_ptr<Expr> loop_step) {
  loop_step_ = std::move(loop_step);
}

inline const Expr& ComprehensionExpr::loop_condition() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return has_loop_condition() ? *loop_condition_ : Expr::default_instance();
}

inline Expr& ComprehensionExpr::mutable_loop_condition()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  if (!has_loop_condition()) {
    loop_condition_ = std::make_unique<Expr>();
  }
  return *loop_condition_;
}

inline void ComprehensionExpr::set_loop_condition(Expr loop_condition) {
  mutable_loop_condition() = std::move(loop_condition);
}

inline void ComprehensionExpr::set_loop_condition(
    std::unique_ptr<Expr> loop_condition) {
  loop_condition_ = std::move(loop_condition);
}

inline const Expr& ComprehensionExpr::result() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return has_result() ? *result_ : Expr::default_instance();
}

inline Expr& ComprehensionExpr::mutable_result() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  if (!has_result()) {
    result_ = std::make_unique<Expr>();
  }
  return *result_;
}

inline void ComprehensionExpr::set_result(Expr result) {
  mutable_result() = std::move(result);
}

inline void ComprehensionExpr::set_result(std::unique_ptr<Expr> result) {
  result_ = std::move(result);
}

inline bool operator==(const ListExprElement& lhs, const ListExprElement& rhs) {
  return lhs.expr() == rhs.expr() && lhs.optional() == rhs.optional();
}

inline bool operator==(const ListExpr& lhs, const ListExpr& rhs) {
  return absl::c_equal(lhs.elements(), rhs.elements());
}

inline bool operator==(const StructExprField& lhs, const StructExprField& rhs) {
  return lhs.id() == rhs.id() && lhs.name() == rhs.name() &&
         lhs.value() == rhs.value() && lhs.optional() == rhs.optional();
}

inline bool operator==(const StructExpr& lhs, const StructExpr& rhs) {
  return lhs.name() == rhs.name() && absl::c_equal(lhs.fields(), rhs.fields());
}

inline bool operator==(const MapExprEntry& lhs, const MapExprEntry& rhs) {
  return lhs.id() == rhs.id() && lhs.key() == rhs.key() &&
         lhs.value() == rhs.value() && lhs.optional() == rhs.optional();
}

inline bool operator==(const MapExpr& lhs, const MapExpr& rhs) {
  return absl::c_equal(lhs.entries(), rhs.entries());
}

inline void MapExpr::Clear() { entries_.clear(); }

inline void MapExpr::set_entries(std::vector<MapExprEntry> entries) {
  entries_ = std::move(entries);
}

inline void MapExpr::set_entries(absl::Span<MapExprEntry> entries) {
  entries_.clear();
  entries_.reserve(entries.size());
  for (auto& entry : entries) {
    entries_.push_back(std::move(entry));
  }
}

inline MapExprEntry& MapExpr::add_entries() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return mutable_entries().emplace_back();
}

inline std::vector<MapExprEntry> MapExpr::release_entries() {
  std::vector<MapExprEntry> entries;
  entries.swap(entries_);
  return entries;
}

inline void Expr::Clear() {
  id_ = 0;
  mutable_kind().emplace<UnspecifiedExpr>();
}

inline Expr& Expr::operator=(Expr&&) = default;

inline void Expr::set_kind(ExprKind kind) { kind_ = std::move(kind); }

inline ABSL_MUST_USE_RESULT ExprKind Expr::release_kind() {
  ExprKind kind = std::move(kind_);
  kind_.emplace<UnspecifiedExpr>();
  return kind;
}

inline void Expr::set_call_expr(CallExpr call_expr) {
  try_emplace_kind<CallExpr>() = std::move(call_expr);
}

inline ABSL_MUST_USE_RESULT CallExpr Expr::release_call_expr() {
  return release_kind<CallExpr>();
}

inline void Expr::set_list_expr(ListExpr list_expr) {
  try_emplace_kind<ListExpr>() = std::move(list_expr);
}

inline ListExpr Expr::release_list_expr() { return release_kind<ListExpr>(); }

inline void Expr::set_struct_expr(StructExpr struct_expr) {
  try_emplace_kind<StructExpr>() = std::move(struct_expr);
}

inline StructExpr Expr::release_struct_expr() {
  return release_kind<StructExpr>();
}

inline void Expr::set_map_expr(MapExpr map_expr) {
  try_emplace_kind<MapExpr>() = std::move(map_expr);
}

inline MapExpr Expr::release_map_expr() { return release_kind<MapExpr>(); }

template <typename T>
ABSL_MUST_USE_RESULT T Expr::release_kind() {
  T result;
  if (auto* alt = absl::get_if<T>(&mutable_kind()); alt) {
    result = std::move(*alt);
  }
  kind_.emplace<UnspecifiedExpr>();
  return result;
}

inline ExprKindCase Expr::kind_case() const {
  static_assert(absl::variant_size_v<ExprKind> == 9);
  if (kind_.index() <= 9) {
    return static_cast<ExprKindCase>(kind_.index());
  }
  return ExprKindCase::kUnspecifiedExpr;
}

inline void swap(Expr& lhs, Expr& rhs) noexcept {
  using std::swap;
  swap(lhs.id_, rhs.id_);
  swap(lhs.kind_, rhs.kind_);
}

inline void CallExpr::Clear() {
  function_.clear();
  target_.reset();
  args_.clear();
}

inline const Expr& CallExpr::target() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return has_target() ? *target_ : Expr::default_instance();
}

inline Expr& CallExpr::mutable_target() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  if (!has_target()) {
    target_ = std::make_unique<Expr>();
  }
  return *target_;
}

inline void CallExpr::set_target(Expr target) {
  mutable_target() = std::move(target);
}

inline void CallExpr::set_target(std::unique_ptr<Expr> target) {
  target_ = std::move(target);
}

inline void CallExpr::set_args(std::vector<Expr> args) {
  args_ = std::move(args);
}

inline void CallExpr::set_args(absl::Span<Expr> args) {
  args_.clear();
  args_.reserve(args.size());
  for (auto& arg : args) {
    args_.push_back(std::move(arg));
  }
}

inline Expr& CallExpr::add_args() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return mutable_args().emplace_back();
}

inline std::vector<Expr> CallExpr::release_args() {
  std::vector<Expr> args;
  args.swap(args_);
  return args;
}

inline void ListExprElement::Clear() {
  expr_.reset();
  optional_ = false;
}

inline ABSL_MUST_USE_RESULT const Expr& ListExprElement::expr() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return has_expr() ? *expr_ : Expr::default_instance();
}

inline ABSL_MUST_USE_RESULT Expr& ListExprElement::mutable_expr()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  if (!has_expr()) {
    expr_ = std::make_unique<Expr>();
  }
  return *expr_;
}

inline void ListExprElement::set_expr(Expr expr) {
  mutable_expr() = std::move(expr);
}

inline void ListExprElement::set_expr(std::unique_ptr<Expr> expr) {
  expr_ = std::move(expr);
}

inline ABSL_MUST_USE_RESULT Expr ListExprElement::release_expr() {
  return release(expr_);
}

inline void swap(ListExprElement& lhs, ListExprElement& rhs) noexcept {
  using std::swap;
  swap(lhs.expr_, rhs.expr_);
  swap(lhs.optional_, rhs.optional_);
}

inline Expr ListExprElement::release(std::unique_ptr<Expr>& property) {
  std::unique_ptr<Expr> result;
  result.swap(property);
  if (result != nullptr) {
    return std::move(*result);
  }
  return Expr{};
}

inline void ListExpr::Clear() { elements_.clear(); }

inline void ListExpr::set_elements(std::vector<ListExprElement> elements) {
  elements_ = std::move(elements);
}

inline void ListExpr::set_elements(absl::Span<ListExprElement> elements) {
  elements_.clear();
  elements_.reserve(elements.size());
  for (auto& element : elements) {
    elements_.push_back(std::move(element));
  }
}

inline ListExprElement& ListExpr::add_elements() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return mutable_elements().emplace_back();
}

inline std::vector<ListExprElement> ListExpr::release_elements() {
  std::vector<ListExprElement> elements;
  elements.swap(elements_);
  return elements;
}

inline void StructExprField::Clear() {
  id_ = 0;
  name_.clear();
  value_.reset();
  optional_ = false;
}

inline ABSL_MUST_USE_RESULT const Expr& StructExprField::value() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return has_value() ? *value_ : Expr::default_instance();
}

inline ABSL_MUST_USE_RESULT Expr& StructExprField::mutable_value()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  if (!has_value()) {
    value_ = std::make_unique<Expr>();
  }
  return *value_;
}

inline void StructExprField::set_value(Expr value) {
  mutable_value() = std::move(value);
}

inline void StructExprField::set_value(std::unique_ptr<Expr> value) {
  value_ = std::move(value);
}

inline ABSL_MUST_USE_RESULT Expr StructExprField::release_value() {
  return release(value_);
}

inline void swap(StructExprField& lhs, StructExprField& rhs) noexcept {
  using std::swap;
  swap(lhs.id_, rhs.id_);
  swap(lhs.name_, rhs.name_);
  swap(lhs.value_, rhs.value_);
  swap(lhs.optional_, rhs.optional_);
}

inline Expr StructExprField::release(std::unique_ptr<Expr>& property) {
  std::unique_ptr<Expr> result;
  result.swap(property);
  if (result != nullptr) {
    return std::move(*result);
  }
  return Expr{};
}

inline void StructExpr::Clear() {
  name_.clear();
  fields_.clear();
}

inline void StructExpr::set_fields(std::vector<StructExprField> fields) {
  fields_ = std::move(fields);
}

inline void StructExpr::set_fields(absl::Span<StructExprField> fields) {
  fields_.clear();
  fields_.reserve(fields.size());
  for (auto& field : fields) {
    fields_.push_back(std::move(field));
  }
}

inline StructExprField& StructExpr::add_fields() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return mutable_fields().emplace_back();
}

inline std::vector<StructExprField> StructExpr::release_fields() {
  std::vector<StructExprField> fields;
  fields.swap(fields_);
  return fields;
}

inline void MapExprEntry::Clear() {
  id_ = 0;
  key_.reset();
  value_.reset();
  optional_ = false;
}

inline ABSL_MUST_USE_RESULT const Expr& MapExprEntry::key() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return has_key() ? *key_ : Expr::default_instance();
}

inline ABSL_MUST_USE_RESULT Expr& MapExprEntry::mutable_key()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  if (!has_key()) {
    key_ = std::make_unique<Expr>();
  }
  return *key_;
}

inline void MapExprEntry::set_key(Expr key) { mutable_key() = std::move(key); }

inline void MapExprEntry::set_key(std::unique_ptr<Expr> key) {
  key_ = std::move(key);
}

inline ABSL_MUST_USE_RESULT Expr MapExprEntry::release_key() {
  return release(key_);
}

inline ABSL_MUST_USE_RESULT const Expr& MapExprEntry::value() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return has_value() ? *value_ : Expr::default_instance();
}

inline ABSL_MUST_USE_RESULT Expr& MapExprEntry::mutable_value()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  if (!has_value()) {
    value_ = std::make_unique<Expr>();
  }
  return *value_;
}

inline void MapExprEntry::set_value(Expr value) {
  mutable_value() = std::move(value);
}

inline void MapExprEntry::set_value(std::unique_ptr<Expr> value) {
  value_ = std::move(value);
}

inline ABSL_MUST_USE_RESULT Expr MapExprEntry::release_value() {
  return release(value_);
}

inline void swap(MapExprEntry& lhs, MapExprEntry& rhs) noexcept {
  using std::swap;
  swap(lhs.id_, rhs.id_);
  swap(lhs.key_, rhs.key_);
  swap(lhs.value_, rhs.value_);
  swap(lhs.optional_, rhs.optional_);
}

inline Expr MapExprEntry::release(std::unique_ptr<Expr>& property) {
  std::unique_ptr<Expr> result;
  result.swap(property);
  if (result != nullptr) {
    return std::move(*result);
  }
  return Expr{};
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_EXPR_H_
