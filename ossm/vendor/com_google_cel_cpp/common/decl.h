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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_DECL_H_
#define THIRD_PARTY_CEL_CPP_COMMON_DECL_H_

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/constant.h"
#include "common/type.h"
#include "internal/status_macros.h"

namespace cel {

class VariableDecl;
class OverloadDecl;
class FunctionDecl;

// `VariableDecl` represents a declaration of a variable, composed of its name
// and type, and optionally a constant value.
class VariableDecl final {
 public:
  VariableDecl() = default;
  VariableDecl(const VariableDecl&) = default;
  VariableDecl(VariableDecl&&) = default;
  VariableDecl& operator=(const VariableDecl&) = default;
  VariableDecl& operator=(VariableDecl&&) = default;

  ABSL_MUST_USE_RESULT const std::string& name() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return name_;
  }

  void set_name(std::string name) { name_ = std::move(name); }

  void set_name(absl::string_view name) {
    name_.assign(name.data(), name.size());
  }

  void set_name(const char* name) { set_name(absl::NullSafeStringView(name)); }

  ABSL_MUST_USE_RESULT std::string release_name() {
    std::string released;
    released.swap(name_);
    return released;
  }

  ABSL_MUST_USE_RESULT const Type& type() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return type_;
  }

  ABSL_MUST_USE_RESULT Type& mutable_type() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return type_;
  }

  void set_type(Type type) { mutable_type() = std::move(type); }

  ABSL_MUST_USE_RESULT bool has_value() const { return value_.has_value(); }

  ABSL_MUST_USE_RESULT const Constant& value() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return has_value() ? *value_ : Constant::default_instance();
  }

  Constant& mutable_value() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    if (!has_value()) {
      value_.emplace();
    }
    return *value_;
  }

  void set_value(absl::optional<Constant> value) { value_ = std::move(value); }

  void set_value(Constant value) { mutable_value() = std::move(value); }

  ABSL_MUST_USE_RESULT Constant release_value() {
    absl::optional<Constant> released;
    released.swap(value_);
    return std::move(released).value_or(Constant{});
  }

 private:
  std::string name_;
  Type type_ = DynType{};
  absl::optional<Constant> value_;
};

inline VariableDecl MakeVariableDecl(std::string name, Type type) {
  VariableDecl variable_decl;
  variable_decl.set_name(std::move(name));
  variable_decl.set_type(std::move(type));
  return variable_decl;
}

inline VariableDecl MakeConstantVariableDecl(std::string name, Type type,
                                             Constant value) {
  VariableDecl variable_decl;
  variable_decl.set_name(std::move(name));
  variable_decl.set_type(std::move(type));
  variable_decl.set_value(std::move(value));
  return variable_decl;
}

inline bool operator==(const VariableDecl& lhs, const VariableDecl& rhs) {
  return lhs.name() == rhs.name() && lhs.type() == rhs.type() &&
         lhs.has_value() == rhs.has_value() && lhs.value() == rhs.value();
}

inline bool operator!=(const VariableDecl& lhs, const VariableDecl& rhs) {
  return !operator==(lhs, rhs);
}

// `OverloadDecl` represents a single overload of `FunctionDecl`.
class OverloadDecl final {
 public:
  OverloadDecl() = default;
  OverloadDecl(const OverloadDecl&) = default;
  OverloadDecl(OverloadDecl&&) = default;
  OverloadDecl& operator=(const OverloadDecl&) = default;
  OverloadDecl& operator=(OverloadDecl&&) = default;

  ABSL_MUST_USE_RESULT const std::string& id() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return id_;
  }

  void set_id(std::string id) { id_ = std::move(id); }

  void set_id(absl::string_view id) { id_.assign(id.data(), id.size()); }

  void set_id(const char* id) { set_id(absl::NullSafeStringView(id)); }

  ABSL_MUST_USE_RESULT std::string release_id() {
    std::string released;
    released.swap(id_);
    return released;
  }

  ABSL_MUST_USE_RESULT const std::vector<Type>& args() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return args_;
  }

  ABSL_MUST_USE_RESULT std::vector<Type>& mutable_args()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return args_;
  }

  ABSL_MUST_USE_RESULT std::vector<Type> release_args() {
    std::vector<Type> released;
    released.swap(mutable_args());
    return released;
  }

  ABSL_MUST_USE_RESULT const Type& result() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return result_;
  }

  ABSL_MUST_USE_RESULT Type& mutable_result() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return result_;
  }

  void set_result(Type result) { mutable_result() = std::move(result); }

  ABSL_MUST_USE_RESULT bool member() const { return member_; }

  void set_member(bool member) { member_ = member; }

  absl::flat_hash_set<std::string> GetTypeParams() const;

 private:
  std::string id_;
  std::vector<Type> args_;
  Type result_ = DynType{};
  bool member_ = false;
};

inline bool operator==(const OverloadDecl& lhs, const OverloadDecl& rhs) {
  return lhs.id() == rhs.id() && absl::c_equal(lhs.args(), rhs.args()) &&
         lhs.result() == rhs.result() && lhs.member() == rhs.member();
}

inline bool operator!=(const OverloadDecl& lhs, const OverloadDecl& rhs) {
  return !operator==(lhs, rhs);
}

template <typename... Args>
OverloadDecl MakeOverloadDecl(std::string id, Type result, Args&&... args) {
  OverloadDecl overload_decl;
  overload_decl.set_id(std::move(id));
  overload_decl.set_result(std::move(result));
  overload_decl.set_member(false);
  auto& mutable_args = overload_decl.mutable_args();
  mutable_args.reserve(sizeof...(Args));
  (mutable_args.push_back(std::forward<Args>(args)), ...);
  return overload_decl;
}

template <typename... Args>
OverloadDecl MakeMemberOverloadDecl(std::string id, Type result,
                                    Args&&... args) {
  OverloadDecl overload_decl;
  overload_decl.set_id(std::move(id));
  overload_decl.set_result(std::move(result));
  overload_decl.set_member(true);
  auto& mutable_args = overload_decl.mutable_args();
  mutable_args.reserve(sizeof...(Args));
  (mutable_args.push_back(std::forward<Args>(args)), ...);
  return overload_decl;
}

struct OverloadDeclHash {
  using is_transparent = void;

  size_t operator()(const OverloadDecl& overload_decl) const {
    return (*this)(overload_decl.id());
  }

  size_t operator()(absl::string_view id) const { return absl::HashOf(id); }
};

struct OverloadDeclEqualTo {
  using is_transparent = void;

  bool operator()(const OverloadDecl& lhs, const OverloadDecl& rhs) const {
    return (*this)(lhs.id(), rhs.id());
  }

  bool operator()(const OverloadDecl& lhs, absl::string_view rhs) const {
    return (*this)(lhs.id(), rhs);
  }

  bool operator()(absl::string_view lhs, const OverloadDecl& rhs) const {
    return (*this)(lhs, rhs.id());
  }

  bool operator()(absl::string_view lhs, absl::string_view rhs) const {
    return lhs == rhs;
  }
};

using OverloadDeclHashSet =
    absl::flat_hash_set<OverloadDecl, OverloadDeclHash, OverloadDeclEqualTo>;

template <typename... Overloads>
absl::StatusOr<FunctionDecl> MakeFunctionDecl(std::string name,
                                              Overloads&&... overloads);

// `FunctionDecl` represents a function declaration.
class FunctionDecl final {
 public:
  FunctionDecl() = default;
  FunctionDecl(const FunctionDecl&) = default;
  FunctionDecl(FunctionDecl&&) = default;
  FunctionDecl& operator=(const FunctionDecl&) = default;
  FunctionDecl& operator=(FunctionDecl&&) = default;

  ABSL_MUST_USE_RESULT const std::string& name() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return name_;
  }

  void set_name(std::string name) { name_ = std::move(name); }

  void set_name(absl::string_view name) {
    name_.assign(name.data(), name.size());
  }

  void set_name(const char* name) { set_name(absl::NullSafeStringView(name)); }

  ABSL_MUST_USE_RESULT std::string release_name() {
    std::string released;
    released.swap(name_);
    return released;
  }

  absl::Status AddOverload(const OverloadDecl& overload) {
    absl::Status status;
    AddOverloadImpl(overload, status);
    return status;
  }

  absl::Status AddOverload(OverloadDecl&& overload) {
    absl::Status status;
    AddOverloadImpl(std::move(overload), status);
    return status;
  }

  ABSL_MUST_USE_RESULT absl::Span<const OverloadDecl> overloads() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return overloads_.insertion_order;
  }

 private:
  struct Overloads {
    std::vector<OverloadDecl> insertion_order;
    OverloadDeclHashSet set;

    void Reserve(size_t size) {
      insertion_order.reserve(size);
      set.reserve(size);
    }
  };

  template <typename... Overloads>
  friend absl::StatusOr<FunctionDecl> MakeFunctionDecl(
      std::string name, Overloads&&... overloads);

  void AddOverloadImpl(const OverloadDecl& overload, absl::Status& status);
  void AddOverloadImpl(OverloadDecl&& overload, absl::Status& status);

  std::string name_;
  Overloads overloads_;
};

inline bool operator==(const FunctionDecl& lhs, const FunctionDecl& rhs) {
  return lhs.name() == rhs.name() &&
         absl::c_equal(lhs.overloads(), rhs.overloads());
}

inline bool operator!=(const FunctionDecl& lhs, const FunctionDecl& rhs) {
  return !operator==(lhs, rhs);
}

template <typename... Overloads>
absl::StatusOr<FunctionDecl> MakeFunctionDecl(std::string name,
                                              Overloads&&... overloads) {
  FunctionDecl function_decl;
  function_decl.set_name(std::move(name));
  function_decl.overloads_.Reserve(sizeof...(Overloads));
  absl::Status status;
  (function_decl.AddOverloadImpl(std::forward<Overloads>(overloads), status),
   ...);
  CEL_RETURN_IF_ERROR(status);
  return function_decl;
}

namespace common_internal {

// Checks whether `from` is assignable to `to`.
// This can probably be in a better place, it is here currently to ease testing.
bool TypeIsAssignable(const Type& to, const Type& from);

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_DECL_H_
