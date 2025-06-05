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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_REFLECTOR_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_REFLECTOR_H_

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_introspector.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

// `TypeReflector` is an interface for constructing new instances of types are
// runtime. It handles type reflection.
class TypeReflector : public virtual TypeIntrospector {
 public:
  // Legacy type reflector, will prefer builders for legacy value.
  static TypeReflector& LegacyBuiltin();
  // Will prefer builders for modern values.
  static TypeReflector& ModernBuiltin();

  static TypeReflector& Builtin() {
    // TODO: Check if it's safe to default to modern.
    // Legacy will prefer legacy container builders for faster interop with
    // client extensions.
    return LegacyBuiltin();
  }

  // `NewListValueBuilder` returns a new `ListValueBuilderInterface` for the
  // corresponding `ListType` `type`.
  virtual absl::StatusOr<absl::Nonnull<ListValueBuilderPtr>>
  NewListValueBuilder(ValueFactory& value_factory, const ListType& type) const;

  // `NewMapValueBuilder` returns a new `MapValueBuilderInterface` for the
  // corresponding `MapType` `type`.
  virtual absl::StatusOr<absl::Nonnull<MapValueBuilderPtr>> NewMapValueBuilder(
      ValueFactory& value_factory, const MapType& type) const;

  // `NewStructValueBuilder` returns a new `StructValueBuilder` for the
  // corresponding `StructType` `type`.
  virtual absl::StatusOr<absl::Nullable<StructValueBuilderPtr>>
  NewStructValueBuilder(ValueFactory& value_factory,
                        const StructType& type) const;

  // `NewValueBuilder` returns a new `ValueBuilder` for the corresponding type
  // `name`.  It is primarily used to handle wrapper types which sometimes show
  // up literally in expressions.
  absl::StatusOr<absl::Nullable<ValueBuilderPtr>> NewValueBuilder(
      ValueFactory& value_factory, absl::string_view name) const;

  // `FindValue` returns a new `Value` for the corresponding name `name`. This
  // can be used to translate enum names to numeric values.
  virtual absl::StatusOr<bool> FindValue(ValueFactory& value_factory,
                                         absl::string_view name,
                                         Value& result) const;

  // `DeserializeValue` deserializes the bytes of `value` according to
  // `type_url`. Returns `NOT_FOUND` if `type_url` is unrecognized.
  absl::StatusOr<absl::optional<Value>> DeserializeValue(
      ValueFactory& value_factory, absl::string_view type_url,
      const absl::Cord& value) const;

  virtual absl::Nullable<const google::protobuf::DescriptorPool*> descriptor_pool()
      const {
    return nullptr;
  }

  virtual absl::Nullable<google::protobuf::MessageFactory*> message_factory() const {
    return nullptr;
  }

 protected:
  virtual absl::StatusOr<absl::optional<Value>> DeserializeValueImpl(
      ValueFactory& value_factory, absl::string_view type_url,
      const absl::Cord& value) const;
};

Shared<TypeReflector> NewThreadCompatibleTypeReflector(
    MemoryManagerRef memory_manager);

namespace common_internal {

// Implementation backing LegacyBuiltin().
class LegacyTypeReflector : public TypeReflector {
 public:
  absl::StatusOr<absl::Nonnull<ListValueBuilderPtr>> NewListValueBuilder(
      ValueFactory& value_factory, const ListType& type) const override;

  absl::StatusOr<absl::Nonnull<MapValueBuilderPtr>> NewMapValueBuilder(
      ValueFactory& value_factory, const MapType& type) const override;
};

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_REFLECTOR_H_
