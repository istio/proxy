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

#include "common/ast/metadata.h"

#include <memory>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/functional/overload.h"
#include "absl/types/variant.h"

namespace cel {

namespace {

const TypeSpec& DefaultTypeSpec() {
  static absl::NoDestructor<TypeSpec> type(TypeSpecKind{UnsetTypeSpec()});
  return *type;
}

TypeSpecKind CopyImpl(const TypeSpecKind& other) {
  return absl::visit(
      absl::Overload(
          [](const std::unique_ptr<TypeSpec>& other) -> TypeSpecKind {
            if (other == nullptr) {
              return std::make_unique<TypeSpec>();
            }
            return std::make_unique<TypeSpec>(*other);
          },
          [](const auto& other) -> TypeSpecKind {
            // Other variants define copy ctor.
            return other;
          }),
      other);
}

}  // namespace

const ExtensionSpec::Version& ExtensionSpec::Version::DefaultInstance() {
  static absl::NoDestructor<Version> instance;
  return *instance;
}

const ExtensionSpec& ExtensionSpec::DefaultInstance() {
  static absl::NoDestructor<ExtensionSpec> instance;
  return *instance;
}

ExtensionSpec::ExtensionSpec(const ExtensionSpec& other)
    : id_(other.id_),
      affected_components_(other.affected_components_),
      version_(std::make_unique<Version>(*other.version_)) {}

ExtensionSpec& ExtensionSpec::operator=(const ExtensionSpec& other) {
  id_ = other.id_;
  affected_components_ = other.affected_components_;
  version_ = std::make_unique<Version>(*other.version_);
  return *this;
}

const TypeSpec& ListTypeSpec::elem_type() const {
  if (elem_type_ != nullptr) {
    return *elem_type_;
  }
  return DefaultTypeSpec();
}

bool ListTypeSpec::operator==(const ListTypeSpec& other) const {
  return elem_type() == other.elem_type();
}

const TypeSpec& MapTypeSpec::key_type() const {
  if (key_type_ != nullptr) {
    return *key_type_;
  }
  return DefaultTypeSpec();
}

const TypeSpec& MapTypeSpec::value_type() const {
  if (value_type_ != nullptr) {
    return *value_type_;
  }
  return DefaultTypeSpec();
}

bool MapTypeSpec::operator==(const MapTypeSpec& other) const {
  return key_type() == other.key_type() && value_type() == other.value_type();
}

const TypeSpec& FunctionTypeSpec::result_type() const {
  if (result_type_ != nullptr) {
    return *result_type_;
  }
  return DefaultTypeSpec();
}

bool FunctionTypeSpec::operator==(const FunctionTypeSpec& other) const {
  return result_type() == other.result_type() && arg_types_ == other.arg_types_;
}

const TypeSpec& TypeSpec::type() const {
  auto* value = absl::get_if<std::unique_ptr<TypeSpec>>(&type_kind_);
  if (value != nullptr) {
    if (*value != nullptr) return **value;
  }
  return DefaultTypeSpec();
}

TypeSpec::TypeSpec(const TypeSpec& other)
    : type_kind_(CopyImpl(other.type_kind_)) {}

TypeSpec& TypeSpec::operator=(const TypeSpec& other) {
  type_kind_ = CopyImpl(other.type_kind_);
  return *this;
}

FunctionTypeSpec::FunctionTypeSpec(const FunctionTypeSpec& other)
    : result_type_(std::make_unique<TypeSpec>(other.result_type())),
      arg_types_(other.arg_types()) {}

FunctionTypeSpec& FunctionTypeSpec::operator=(const FunctionTypeSpec& other) {
  result_type_ = std::make_unique<TypeSpec>(other.result_type());
  arg_types_ = other.arg_types();
  return *this;
}

}  // namespace cel
