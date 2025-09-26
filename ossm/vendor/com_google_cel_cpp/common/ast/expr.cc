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

#include "common/ast/expr.h"

#include <memory>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/functional/overload.h"
#include "absl/types/variant.h"

namespace cel::ast_internal {

namespace {

const Type& default_type() {
  static absl::NoDestructor<Type> type(TypeKind{UnspecifiedType()});
  return *type;
}

TypeKind CopyImpl(const TypeKind& other) {
  return absl::visit(absl::Overload(
                         [](const std::unique_ptr<Type>& other) -> TypeKind {
                           if (other == nullptr) {
                             return std::make_unique<Type>();
                           }
                           return std::make_unique<Type>(*other);
                         },
                         [](const auto& other) -> TypeKind {
                           // Other variants define copy ctor.
                           return other;
                         }),
                     other);
}

}  // namespace

const Extension::Version& Extension::Version::DefaultInstance() {
  static absl::NoDestructor<Version> instance;
  return *instance;
}

const Extension& Extension::DefaultInstance() {
  static absl::NoDestructor<Extension> instance;
  return *instance;
}

Extension::Extension(const Extension& other)
    : id_(other.id_),
      affected_components_(other.affected_components_),
      version_(std::make_unique<Version>(*other.version_)) {}

Extension& Extension::operator=(const Extension& other) {
  id_ = other.id_;
  affected_components_ = other.affected_components_;
  version_ = std::make_unique<Version>(*other.version_);
  return *this;
}

const Type& ListType::elem_type() const {
  if (elem_type_ != nullptr) {
    return *elem_type_;
  }
  return default_type();
}

bool ListType::operator==(const ListType& other) const {
  return elem_type() == other.elem_type();
}

const Type& MapType::key_type() const {
  if (key_type_ != nullptr) {
    return *key_type_;
  }
  return default_type();
}

const Type& MapType::value_type() const {
  if (value_type_ != nullptr) {
    return *value_type_;
  }
  return default_type();
}

bool MapType::operator==(const MapType& other) const {
  return key_type() == other.key_type() && value_type() == other.value_type();
}

const Type& FunctionType::result_type() const {
  if (result_type_ != nullptr) {
    return *result_type_;
  }
  return default_type();
}

bool FunctionType::operator==(const FunctionType& other) const {
  return result_type() == other.result_type() && arg_types_ == other.arg_types_;
}

const Type& Type::type() const {
  auto* value = absl::get_if<std::unique_ptr<Type>>(&type_kind_);
  if (value != nullptr) {
    if (*value != nullptr) return **value;
  }
  return default_type();
}

Type::Type(const Type& other) : type_kind_(CopyImpl(other.type_kind_)) {}

Type& Type::operator=(const Type& other) {
  type_kind_ = CopyImpl(other.type_kind_);
  return *this;
}

FunctionType::FunctionType(const FunctionType& other)
    : result_type_(std::make_unique<Type>(other.result_type())),
      arg_types_(other.arg_types()) {}

FunctionType& FunctionType::operator=(const FunctionType& other) {
  result_type_ = std::make_unique<Type>(other.result_type());
  arg_types_ = other.arg_types();
  return *this;
}

}  // namespace cel::ast_internal
