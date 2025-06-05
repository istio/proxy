// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_INTROSPECTOR_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_INTROSPECTOR_H_

#include <cstdint>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"

namespace cel {

class TypeFactory;

// `TypeIntrospector` is an interface which allows querying type-related
// information. It handles type introspection, but not type reflection. That is,
// it is not capable of instantiating new values or understanding values. Its
// primary usage is for type checking, and a subset of that shared functionality
// is used by the runtime.
class TypeIntrospector {
 public:
  struct EnumConstant {
    // The type of the enum. For JSON null, this may be a specific type rather
    // than an enum type.
    Type type;
    absl::string_view type_full_name;
    absl::string_view value_name;
    int32_t number;
  };

  virtual ~TypeIntrospector() = default;

  // `FindType` find the type corresponding to name `name`.
  absl::StatusOr<absl::optional<Type>> FindType(TypeFactory& type_factory,
                                                absl::string_view name) const;

  // `FindEnumConstant` find a fully qualified enumerator name `name` in enum
  // type `type`.
  absl::StatusOr<absl::optional<EnumConstant>> FindEnumConstant(
      TypeFactory& type_factory, absl::string_view type,
      absl::string_view value) const;

  // `FindStructTypeFieldByName` find the name, number, and type of the field
  // `name` in type `type`.
  absl::StatusOr<absl::optional<StructTypeField>> FindStructTypeFieldByName(
      TypeFactory& type_factory, absl::string_view type,
      absl::string_view name) const;

  // `FindStructTypeFieldByName` find the name, number, and type of the field
  // `name` in struct type `type`.
  absl::StatusOr<absl::optional<StructTypeField>> FindStructTypeFieldByName(
      TypeFactory& type_factory, const StructType& type,
      absl::string_view name) const {
    return FindStructTypeFieldByName(type_factory, type.name(), name);
  }

 protected:
  virtual absl::StatusOr<absl::optional<Type>> FindTypeImpl(
      TypeFactory& type_factory, absl::string_view name) const;

  virtual absl::StatusOr<absl::optional<EnumConstant>> FindEnumConstantImpl(
      TypeFactory& type_factory, absl::string_view type,
      absl::string_view value) const;

  virtual absl::StatusOr<absl::optional<StructTypeField>>
  FindStructTypeFieldByNameImpl(TypeFactory& type_factory,
                                absl::string_view type,
                                absl::string_view name) const;
};

Shared<TypeIntrospector> NewThreadCompatibleTypeIntrospector(
    MemoryManagerRef memory_manager);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_INTROSPECTOR_H_
