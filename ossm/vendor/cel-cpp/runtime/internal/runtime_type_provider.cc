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

#include "runtime/internal/runtime_type_provider.h"

#include <utility>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/type.h"
#include "common/type_introspector.h"
#include "common/value.h"
#include "common/values/value_builder.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::runtime_internal {

absl::Status RuntimeTypeProvider::RegisterType(const OpaqueType& type) {
  auto insertion = types_.insert(std::pair{type.name(), Type(type)});
  if (!insertion.second) {
    return absl::AlreadyExistsError(
        absl::StrCat("type already registered: ", insertion.first->first));
  }
  return absl::OkStatus();
}

absl::StatusOr<absl::optional<Type>> RuntimeTypeProvider::FindTypeImpl(
    absl::string_view name) const {
  // We do not have to worry about well known types here.
  // `TypeIntrospector::FindType` handles those directly.
  const auto* desc = descriptor_pool_->FindMessageTypeByName(name);
  if (desc == nullptr) {
    if (const auto it = types_.find(name); it != types_.end()) {
      return it->second;
    }
    return absl::nullopt;
  }
  return MessageType(desc);
}

absl::StatusOr<absl::optional<TypeIntrospector::EnumConstant>>
RuntimeTypeProvider::FindEnumConstantImpl(absl::string_view type,
                                          absl::string_view value) const {
  const google::protobuf::EnumDescriptor* enum_desc =
      descriptor_pool_->FindEnumTypeByName(type);
  // google.protobuf.NullValue is special cased in the base class.
  if (enum_desc == nullptr) {
    return absl::nullopt;
  }

  // Note: we don't support strong enum typing at this time so only the fully
  // qualified enum values are meaningful, so we don't provide any signal if the
  // enum type is found but can't match the value name.
  const google::protobuf::EnumValueDescriptor* value_desc =
      enum_desc->FindValueByName(value);
  if (value_desc == nullptr) {
    return absl::nullopt;
  }

  return TypeIntrospector::EnumConstant{
      EnumType(enum_desc), enum_desc->full_name(), value_desc->name(),
      value_desc->number()};
}

absl::StatusOr<absl::optional<StructTypeField>>
RuntimeTypeProvider::FindStructTypeFieldByNameImpl(
    absl::string_view type, absl::string_view name) const {
  // We do not have to worry about well known types here.
  // `TypeIntrospector::FindStructTypeFieldByName` handles those directly.
  const auto* desc = descriptor_pool_->FindMessageTypeByName(type);
  if (desc == nullptr) {
    return absl::nullopt;
  }
  const auto* field_desc = desc->FindFieldByName(name);
  if (field_desc == nullptr) {
    field_desc = descriptor_pool_->FindExtensionByPrintableName(desc, name);
    if (field_desc == nullptr) {
      return absl::nullopt;
    }
  }
  return MessageTypeField(field_desc);
}

absl::StatusOr<absl_nullable ValueBuilderPtr>
RuntimeTypeProvider::NewValueBuilder(
    absl::string_view name,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  return common_internal::NewValueBuilder(arena, descriptor_pool_,
                                          message_factory, name);
}

}  // namespace cel::runtime_internal
