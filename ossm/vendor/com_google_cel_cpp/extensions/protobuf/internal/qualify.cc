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

#include "extensions/protobuf/internal/qualify.h"

#include <string>
#include <utility>

#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "base/attribute.h"
#include "base/builtins.h"
#include "common/kind.h"
#include "common/memory.h"
#include "extensions/protobuf/internal/map_reflection.h"
#include "internal/status_macros.h"
#include "runtime/internal/errors.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"
#include "google/protobuf/reflection.h"

namespace cel::extensions::protobuf_internal {

namespace {

const google::protobuf::FieldDescriptor* GetNormalizedFieldByNumber(
    const google::protobuf::Descriptor* descriptor, const google::protobuf::Reflection* reflection,
    int field_number) {
  const google::protobuf::FieldDescriptor* field_desc =
      descriptor->FindFieldByNumber(field_number);
  if (field_desc == nullptr && reflection != nullptr) {
    field_desc = reflection->FindKnownExtensionByNumber(field_number);
  }
  return field_desc;
}

// JSON container types and Any have special unpacking rules.
//
// Not considered for qualify traversal for simplicity, but
// could be supported in a follow-up if needed.
bool IsUnsupportedQualifyType(const google::protobuf::Descriptor& desc) {
  switch (desc.well_known_type()) {
    case google::protobuf::Descriptor::WELLKNOWNTYPE_ANY:
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT:
    case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE:
    case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE:
      return true;
    default:
      return false;
  }
}

constexpr int kKeyTag = 1;
constexpr int kValueTag = 2;

bool MatchesMapKeyType(const google::protobuf::FieldDescriptor* key_desc,
                       const cel::AttributeQualifier& key) {
  switch (key_desc->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return key.kind() == cel::Kind::kBool;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      // fall through
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return key.kind() == cel::Kind::kInt64;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      // fall through
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return key.kind() == cel::Kind::kUint64;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      return key.kind() == cel::Kind::kString;

    default:
      return false;
  }
}

absl::StatusOr<absl::optional<google::protobuf::MapValueConstRef>> LookupMapValue(
    const google::protobuf::Message* message, const google::protobuf::Reflection* reflection,
    const google::protobuf::FieldDescriptor* field_desc,
    const google::protobuf::FieldDescriptor* key_desc,
    const cel::AttributeQualifier& key) {
  if (!MatchesMapKeyType(key_desc, key)) {
    return runtime_internal::CreateInvalidMapKeyTypeError(
        key_desc->cpp_type_name());
  }

  std::string proto_key_string;
  google::protobuf::MapKey proto_key;
  switch (key_desc->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      proto_key.SetBoolValue(*key.GetBoolKey());
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32: {
      int64_t key_value = *key.GetInt64Key();
      if (key_value > std::numeric_limits<int32_t>::max() ||
          key_value < std::numeric_limits<int32_t>::lowest()) {
        return absl::OutOfRangeError("integer overflow");
      }
      proto_key.SetInt32Value(key_value);
    } break;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      proto_key.SetInt64Value(*key.GetInt64Key());
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
      proto_key_string = std::string(*key.GetStringKey());
      proto_key.SetStringValue(proto_key_string);
    } break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32: {
      uint64_t key_value = *key.GetUint64Key();
      if (key_value > std::numeric_limits<uint32_t>::max()) {
        return absl::OutOfRangeError("unsigned integer overflow");
      }
      proto_key.SetUInt32Value(key_value);
    } break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64: {
      proto_key.SetUInt64Value(*key.GetUint64Key());
    } break;
    default:
      return runtime_internal::CreateInvalidMapKeyTypeError(
          key_desc->cpp_type_name());
  }

  // Look the value up
  google::protobuf::MapValueConstRef value_ref;
  bool found = cel::extensions::protobuf_internal::LookupMapValue(
      *reflection, *message, *field_desc, proto_key, &value_ref);
  if (!found) {
    return absl::nullopt;
  }
  return value_ref;
}

bool FieldIsPresent(const google::protobuf::Message* message,
                    const google::protobuf::FieldDescriptor* field_desc,
                    const google::protobuf::Reflection* reflection) {
  if (field_desc->is_map()) {
    // When the map field appears in a has(msg.map_field) expression, the map
    // is considered 'present' when it is non-empty. Since maps are repeated
    // fields they don't participate with standard proto presence testing
    // since the repeated field is always at least empty.
    return reflection->FieldSize(*message, field_desc) != 0;
  }

  if (field_desc->is_repeated()) {
    // When the list field appears in a has(msg.list_field) expression, the
    // list is considered 'present' when it is non-empty.
    return reflection->FieldSize(*message, field_desc) != 0;
  }

  // Standard proto presence test for non-repeated fields.
  return reflection->HasField(*message, field_desc);
}

}  // namespace

absl::Status ProtoQualifyState::ApplySelectQualifier(
    const cel::SelectQualifier& qualifier, MemoryManagerRef memory_manager) {
  return absl::visit(
      absl::Overload(
          [&](const cel::AttributeQualifier& qualifier) -> absl::Status {
            if (repeated_field_desc_ == nullptr) {
              return absl::UnimplementedError(
                  "dynamic field access on message not supported");
            }
            return ApplyAttributeQualifer(qualifier, memory_manager);
          },
          [&](const cel::FieldSpecifier& field_specifier) -> absl::Status {
            if (repeated_field_desc_ != nullptr) {
              return absl::UnimplementedError(
                  "strong field access on container not supported");
            }
            return ApplyFieldSpecifier(field_specifier, memory_manager);
          }),
      qualifier);
}

absl::Status ProtoQualifyState::ApplyLastQualifierHas(
    const cel::SelectQualifier& qualifier, MemoryManagerRef memory_manager) {
  const cel::FieldSpecifier* specifier =
      absl::get_if<cel::FieldSpecifier>(&qualifier);
  return absl::visit(
      absl::Overload(
          [&](const cel::AttributeQualifier& qualifier) mutable
          -> absl::Status {
            if (qualifier.kind() != cel::Kind::kString ||
                repeated_field_desc_ == nullptr ||
                !repeated_field_desc_->is_map()) {
              SetResultFromError(
                  runtime_internal::CreateNoMatchingOverloadError("has"),
                  memory_manager);
              return absl::OkStatus();
            }
            return MapHas(qualifier, memory_manager);
          },
          [&](const cel::FieldSpecifier& field_specifier) mutable
          -> absl::Status {
            const auto* field_desc = GetNormalizedFieldByNumber(
                descriptor_, reflection_, specifier->number);
            if (field_desc == nullptr) {
              SetResultFromError(
                  runtime_internal::CreateNoSuchFieldError(specifier->name),
                  memory_manager);
              return absl::OkStatus();
            }
            SetResultFromBool(
                FieldIsPresent(message_, field_desc, reflection_));
            return absl::OkStatus();
          }),
      qualifier);
}

absl::Status ProtoQualifyState::ApplyLastQualifierGet(
    const cel::SelectQualifier& qualifier, MemoryManagerRef memory_manager) {
  return absl::visit(
      absl::Overload(
          [&](const cel::AttributeQualifier& attr_qualifier) mutable
          -> absl::Status {
            if (repeated_field_desc_ == nullptr) {
              return absl::UnimplementedError(
                  "dynamic field access on message not supported");
            }
            if (repeated_field_desc_->is_map()) {
              return ApplyLastQualifierGetMap(attr_qualifier, memory_manager);
            }
            return ApplyLastQualifierGetList(attr_qualifier, memory_manager);
          },
          [&](const cel::FieldSpecifier& specifier) mutable -> absl::Status {
            if (repeated_field_desc_ != nullptr) {
              return absl::UnimplementedError(
                  "strong field access on container not supported");
            }
            return ApplyLastQualifierMessageGet(specifier, memory_manager);
          }),
      qualifier);
}

absl::Status ProtoQualifyState::ApplyFieldSpecifier(
    const cel::FieldSpecifier& field_specifier,
    MemoryManagerRef memory_manager) {
  const google::protobuf::FieldDescriptor* field_desc = GetNormalizedFieldByNumber(
      descriptor_, reflection_, field_specifier.number);
  if (field_desc == nullptr) {
    SetResultFromError(
        runtime_internal::CreateNoSuchFieldError(field_specifier.name),
        memory_manager);
    return absl::OkStatus();
  }

  if (field_desc->is_repeated()) {
    repeated_field_desc_ = field_desc;
    return absl::OkStatus();
  }

  if (field_desc->cpp_type() != google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE ||
      IsUnsupportedQualifyType(*field_desc->message_type())) {
    CEL_RETURN_IF_ERROR(SetResultFromField(message_, field_desc,
                                           ProtoWrapperTypeOptions::kUnsetNull,
                                           memory_manager));
    return absl::OkStatus();
  }

  message_ = &reflection_->GetMessage(*message_, field_desc);
  descriptor_ = message_->GetDescriptor();
  reflection_ = message_->GetReflection();
  return absl::OkStatus();
}

absl::StatusOr<int> ProtoQualifyState::CheckListIndex(
    const cel::AttributeQualifier& qualifier) const {
  if (qualifier.kind() != cel::Kind::kInt64) {
    return runtime_internal::CreateNoMatchingOverloadError(
        cel::builtin::kIndex);
  }

  int index = *qualifier.GetInt64Key();
  int size = reflection_->FieldSize(*message_, repeated_field_desc_);
  if (index < 0 || index >= size) {
    return absl::InvalidArgumentError(
        absl::StrCat("index out of bounds: index=", index, " size=", size));
  }
  return index;
}

absl::Status ProtoQualifyState::ApplyAttributeQualifierList(
    const cel::AttributeQualifier& qualifier, MemoryManagerRef memory_manager) {
  ABSL_DCHECK_NE(repeated_field_desc_, nullptr);
  ABSL_DCHECK(!repeated_field_desc_->is_map());
  ABSL_DCHECK_EQ(repeated_field_desc_->cpp_type(),
                 google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE);

  auto index_or = CheckListIndex(qualifier);
  if (!index_or.ok()) {
    SetResultFromError(std::move(index_or).status(), memory_manager);
    return absl::OkStatus();
  }

  if (IsUnsupportedQualifyType(*repeated_field_desc_->message_type())) {
    CEL_RETURN_IF_ERROR(SetResultFromRepeatedField(
        message_, repeated_field_desc_, *index_or, memory_manager));
    return absl::OkStatus();
  }

  message_ = &reflection_->GetRepeatedMessage(*message_, repeated_field_desc_,
                                              *index_or);
  descriptor_ = message_->GetDescriptor();
  reflection_ = message_->GetReflection();
  repeated_field_desc_ = nullptr;
  return absl::OkStatus();
}

absl::StatusOr<google::protobuf::MapValueConstRef> ProtoQualifyState::CheckMapIndex(
    const cel::AttributeQualifier& qualifier) const {
  const auto* key_desc =
      repeated_field_desc_->message_type()->FindFieldByNumber(kKeyTag);

  CEL_ASSIGN_OR_RETURN(
      absl::optional<google::protobuf::MapValueConstRef> value_ref,
      LookupMapValue(message_, reflection_, repeated_field_desc_, key_desc,
                     qualifier));

  if (!value_ref.has_value()) {
    return runtime_internal::CreateNoSuchKeyError("");
  }
  return std::move(value_ref).value();
}

absl::Status ProtoQualifyState::ApplyAttributeQualifierMap(
    const cel::AttributeQualifier& qualifier, MemoryManagerRef memory_manager) {
  ABSL_DCHECK_NE(repeated_field_desc_, nullptr);
  ABSL_DCHECK(repeated_field_desc_->is_map());
  ABSL_DCHECK_EQ(repeated_field_desc_->cpp_type(),
                 google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE);

  absl::StatusOr<google::protobuf::MapValueConstRef> value_ref = CheckMapIndex(qualifier);
  if (!value_ref.ok()) {
    SetResultFromError(std::move(value_ref).status(), memory_manager);
    return absl::OkStatus();
  }

  const auto* value_desc =
      repeated_field_desc_->message_type()->FindFieldByNumber(kValueTag);

  if (value_desc->cpp_type() != google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE ||
      IsUnsupportedQualifyType(*value_desc->message_type())) {
    CEL_RETURN_IF_ERROR(SetResultFromMapField(message_, value_desc, *value_ref,
                                              memory_manager));
    return absl::OkStatus();
  }

  message_ = &(value_ref->GetMessageValue());
  descriptor_ = message_->GetDescriptor();
  reflection_ = message_->GetReflection();
  repeated_field_desc_ = nullptr;
  return absl::OkStatus();
}

absl::Status ProtoQualifyState::ApplyAttributeQualifer(
    const cel::AttributeQualifier& qualifier, MemoryManagerRef memory_manager) {
  ABSL_DCHECK_NE(repeated_field_desc_, nullptr);
  if (repeated_field_desc_->cpp_type() !=
      google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
    return absl::InternalError("Unexpected qualify intermediate type");
  }
  if (repeated_field_desc_->is_map()) {
    return ApplyAttributeQualifierMap(qualifier, memory_manager);
  }  // else simple repeated
  return ApplyAttributeQualifierList(qualifier, memory_manager);
}

absl::Status ProtoQualifyState::MapHas(const cel::AttributeQualifier& key,
                                       MemoryManagerRef memory_manager) {
  const auto* key_desc =
      repeated_field_desc_->message_type()->FindFieldByNumber(kKeyTag);

  absl::StatusOr<absl::optional<google::protobuf::MapValueConstRef>> value_ref =
      LookupMapValue(message_, reflection_, repeated_field_desc_, key_desc,
                     key);

  if (!value_ref.ok()) {
    SetResultFromError(std::move(value_ref).status(), memory_manager);
    return absl::OkStatus();
  }

  SetResultFromBool(value_ref->has_value());
  return absl::OkStatus();
}

absl::Status ProtoQualifyState::ApplyLastQualifierMessageGet(
    const cel::FieldSpecifier& specifier, MemoryManagerRef memory_manager) {
  const auto* field_desc =
      GetNormalizedFieldByNumber(descriptor_, reflection_, specifier.number);
  if (field_desc == nullptr) {
    SetResultFromError(runtime_internal::CreateNoSuchFieldError(specifier.name),
                       memory_manager);
    return absl::OkStatus();
  }
  return SetResultFromField(message_, field_desc,
                            ProtoWrapperTypeOptions::kUnsetNull,
                            memory_manager);
}

absl::Status ProtoQualifyState::ApplyLastQualifierGetList(
    const cel::AttributeQualifier& qualifier, MemoryManagerRef memory_manager) {
  ABSL_DCHECK(!repeated_field_desc_->is_map());

  absl::StatusOr<int> index = CheckListIndex(qualifier);
  if (!index.ok()) {
    SetResultFromError(std::move(index).status(), memory_manager);
    return absl::OkStatus();
  }
  return SetResultFromRepeatedField(message_, repeated_field_desc_, *index,
                                    memory_manager);
}

absl::Status ProtoQualifyState::ApplyLastQualifierGetMap(
    const cel::AttributeQualifier& qualifier, MemoryManagerRef memory_manager) {
  ABSL_DCHECK(repeated_field_desc_->is_map());

  absl::StatusOr<google::protobuf::MapValueConstRef> value_ref = CheckMapIndex(qualifier);

  if (!value_ref.ok()) {
    SetResultFromError(std::move(value_ref).status(), memory_manager);
    return absl::OkStatus();
  }

  const auto* value_desc =
      repeated_field_desc_->message_type()->FindFieldByNumber(kValueTag);
  return SetResultFromMapField(message_, value_desc, *value_ref,
                               memory_manager);
}

}  // namespace cel::extensions::protobuf_internal
