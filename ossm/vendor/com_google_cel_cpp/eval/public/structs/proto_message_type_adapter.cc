// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/public/structs/proto_message_type_adapter.h"

#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/util/message_differencer.h"
#include "absl/base/no_destructor.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/memory.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/internal_field_backed_list_impl.h"
#include "eval/public/containers/internal_field_backed_map_impl.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/structs/cel_proto_wrap_util.h"
#include "eval/public/structs/field_access_impl.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "extensions/protobuf/internal/qualify.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/casts.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime {
namespace {

using ::cel::extensions::ProtoMemoryManagerArena;
using ::cel::extensions::ProtoMemoryManagerRef;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::Message;
using ::google::protobuf::Reflection;

using LegacyQualifyResult = LegacyTypeAccessApis::LegacyQualifyResult;

const std::string& UnsupportedTypeName() {
  static absl::NoDestructor<std::string> kUnsupportedTypeName(
      "<unknown message>");
  return *kUnsupportedTypeName;
}

CelValue MessageCelValueFactory(const google::protobuf::Message* message);

inline absl::StatusOr<const google::protobuf::Message*> UnwrapMessage(
    const MessageWrapper& value, absl::string_view op) {
  if (!value.HasFullProto() || value.message_ptr() == nullptr) {
    return absl::InternalError(
        absl::StrCat(op, " called on non-message type."));
  }
  return static_cast<const google::protobuf::Message*>(value.message_ptr());
}

inline absl::StatusOr<google::protobuf::Message*> UnwrapMessage(
    const MessageWrapper::Builder& value, absl::string_view op) {
  if (!value.HasFullProto() || value.message_ptr() == nullptr) {
    return absl::InternalError(
        absl::StrCat(op, " called on non-message type."));
  }
  return static_cast<google::protobuf::Message*>(value.message_ptr());
}

bool ProtoEquals(const google::protobuf::Message& m1, const google::protobuf::Message& m2) {
  // Equality behavior is undefined for message differencer if input messages
  // have different descriptors. For CEL just return false.
  if (m1.GetDescriptor() != m2.GetDescriptor()) {
    return false;
  }
  return google::protobuf::util::MessageDifferencer::Equals(m1, m2);
}

// Implements CEL's notion of field presence for protobuf.
// Assumes all arguments non-null.
bool CelFieldIsPresent(const google::protobuf::Message* message,
                       const google::protobuf::FieldDescriptor* field_desc,
                       const google::protobuf::Reflection* reflection) {
  if (field_desc->is_map()) {
    // When the map field appears in a has(msg.map_field) expression, the map
    // is considered 'present' when it is non-empty. Since maps are repeated
    // fields they don't participate with standard proto presence testing since
    // the repeated field is always at least empty.
    return reflection->FieldSize(*message, field_desc) != 0;
  }

  if (field_desc->is_repeated()) {
    // When the list field appears in a has(msg.list_field) expression, the list
    // is considered 'present' when it is non-empty.
    return reflection->FieldSize(*message, field_desc) != 0;
  }

  // Standard proto presence test for non-repeated fields.
  return reflection->HasField(*message, field_desc);
}

// Shared implementation for HasField.
// Handles list or map specific behavior before calling reflection helpers.
absl::StatusOr<bool> HasFieldImpl(const google::protobuf::Message* message,
                                  const google::protobuf::Descriptor* descriptor,
                                  absl::string_view field_name) {
  ABSL_ASSERT(descriptor == message->GetDescriptor());
  const Reflection* reflection = message->GetReflection();
  const FieldDescriptor* field_desc = descriptor->FindFieldByName(field_name);
  if (field_desc == nullptr && reflection != nullptr) {
    // Search to see whether the field name is referring to an extension.
    field_desc = reflection->FindKnownExtensionByName(field_name);
  }
  if (field_desc == nullptr) {
    return absl::NotFoundError(absl::StrCat("no_such_field : ", field_name));
  }

  if (reflection == nullptr) {
    return absl::FailedPreconditionError(
        "google::protobuf::Reflection unavailble in CEL field access.");
  }
  return CelFieldIsPresent(message, field_desc, reflection);
}

absl::StatusOr<CelValue> CreateCelValueFromField(
    const google::protobuf::Message* message, const google::protobuf::FieldDescriptor* field_desc,
    ProtoWrapperTypeOptions unboxing_option, google::protobuf::Arena* arena) {
  if (field_desc->is_map()) {
    auto* map = google::protobuf::Arena::Create<internal::FieldBackedMapImpl>(
        arena, message, field_desc, &MessageCelValueFactory, arena);

    return CelValue::CreateMap(map);
  }
  if (field_desc->is_repeated()) {
    auto* list = google::protobuf::Arena::Create<internal::FieldBackedListImpl>(
        arena, message, field_desc, &MessageCelValueFactory, arena);
    return CelValue::CreateList(list);
  }

  CEL_ASSIGN_OR_RETURN(
      CelValue result,
      internal::CreateValueFromSingleField(message, field_desc, unboxing_option,
                                           &MessageCelValueFactory, arena));
  return result;
}

// Shared implementation for GetField.
// Handles list or map specific behavior before calling reflection helpers.
absl::StatusOr<CelValue> GetFieldImpl(const google::protobuf::Message* message,
                                      const google::protobuf::Descriptor* descriptor,
                                      absl::string_view field_name,
                                      ProtoWrapperTypeOptions unboxing_option,
                                      cel::MemoryManagerRef memory_manager) {
  ABSL_ASSERT(descriptor == message->GetDescriptor());
  const Reflection* reflection = message->GetReflection();
  const FieldDescriptor* field_desc = descriptor->FindFieldByName(field_name);
  if (field_desc == nullptr && reflection != nullptr) {
    std::string ext_name(field_name);
    field_desc = reflection->FindKnownExtensionByName(ext_name);
  }
  if (field_desc == nullptr) {
    return CreateNoSuchFieldError(memory_manager, field_name);
  }

  google::protobuf::Arena* arena = ProtoMemoryManagerArena(memory_manager);

  return CreateCelValueFromField(message, field_desc, unboxing_option, arena);
}

// State machine for incrementally applying qualifiers.
//
// Reusing the state machine to represent intermediate states (as opposed to
// returning the intermediates) is more efficient for longer select chains while
// still allowing decomposition of the qualify routine.
class LegacyQualifyState final
    : public cel::extensions::protobuf_internal::ProtoQualifyState {
 public:
  using ProtoQualifyState::ProtoQualifyState;

  LegacyQualifyState(const LegacyQualifyState&) = delete;
  LegacyQualifyState& operator=(const LegacyQualifyState&) = delete;

  absl::optional<CelValue>& result() { return result_; }

 private:
  void SetResultFromError(absl::Status status,
                          cel::MemoryManagerRef memory_manager) override {
    result_ = CreateErrorValue(memory_manager, status);
  }

  void SetResultFromBool(bool value) override {
    result_ = CelValue::CreateBool(value);
  }

  absl::Status SetResultFromField(
      const google::protobuf::Message* message, const google::protobuf::FieldDescriptor* field,
      ProtoWrapperTypeOptions unboxing_option,
      cel::MemoryManagerRef memory_manager) override {
    CEL_ASSIGN_OR_RETURN(result_, CreateCelValueFromField(
                                      message, field, unboxing_option,
                                      ProtoMemoryManagerArena(memory_manager)));
    return absl::OkStatus();
  }

  absl::Status SetResultFromRepeatedField(
      const google::protobuf::Message* message, const google::protobuf::FieldDescriptor* field,
      int index, cel::MemoryManagerRef memory_manager) override {
    CEL_ASSIGN_OR_RETURN(result_,
                         internal::CreateValueFromRepeatedField(
                             message, field, index, &MessageCelValueFactory,
                             ProtoMemoryManagerArena(memory_manager)));
    return absl::OkStatus();
  }

  absl::Status SetResultFromMapField(
      const google::protobuf::Message* message, const google::protobuf::FieldDescriptor* field,
      const google::protobuf::MapValueConstRef& value,
      cel::MemoryManagerRef memory_manager) override {
    CEL_ASSIGN_OR_RETURN(result_,
                         internal::CreateValueFromMapValue(
                             message, field, &value, &MessageCelValueFactory,
                             ProtoMemoryManagerArena(memory_manager)));
    return absl::OkStatus();
  }

  absl::optional<CelValue> result_;
};

absl::StatusOr<LegacyQualifyResult> QualifyImpl(
    const google::protobuf::Message* message, const google::protobuf::Descriptor* descriptor,
    absl::Span<const cel::SelectQualifier> path, bool presence_test,
    cel::MemoryManagerRef memory_manager) {
  google::protobuf::Arena* arena = ProtoMemoryManagerArena(memory_manager);
  ABSL_DCHECK(descriptor == message->GetDescriptor());
  LegacyQualifyState qualify_state(message, descriptor,
                                   message->GetReflection());

  for (int i = 0; i < path.size() - 1; i++) {
    const auto& qualifier = path.at(i);
    CEL_RETURN_IF_ERROR(qualify_state.ApplySelectQualifier(
        qualifier, ProtoMemoryManagerRef(arena)));
    if (qualify_state.result().has_value()) {
      LegacyQualifyResult result;
      result.value = std::move(qualify_state.result()).value();
      result.qualifier_count = result.value.IsError() ? -1 : i + 1;
      return result;
    }
  }

  const auto& last_qualifier = path.back();
  LegacyQualifyResult result;
  result.qualifier_count = -1;

  if (presence_test) {
    CEL_RETURN_IF_ERROR(qualify_state.ApplyLastQualifierHas(
        last_qualifier, ProtoMemoryManagerRef(arena)));
  } else {
    CEL_RETURN_IF_ERROR(qualify_state.ApplyLastQualifierGet(
        last_qualifier, ProtoMemoryManagerRef(arena)));
  }
  result.value = *qualify_state.result();
  return result;
}

std::vector<absl::string_view> ListFieldsImpl(
    const CelValue::MessageWrapper& instance) {
  if (instance.message_ptr() == nullptr) {
    return std::vector<absl::string_view>();
  }
  ABSL_ASSERT(instance.HasFullProto());
  const auto* message =
      static_cast<const google::protobuf::Message*>(instance.message_ptr());
  const auto* reflect = message->GetReflection();
  std::vector<const google::protobuf::FieldDescriptor*> fields;
  reflect->ListFields(*message, &fields);
  std::vector<absl::string_view> field_names;
  field_names.reserve(fields.size());
  for (const auto* field : fields) {
    field_names.emplace_back(field->name());
  }
  return field_names;
}

class DucktypedMessageAdapter : public LegacyTypeAccessApis,
                                public LegacyTypeMutationApis,
                                public LegacyTypeInfoApis {
 public:
  // Implement field access APIs.
  absl::StatusOr<bool> HasField(
      absl::string_view field_name,
      const CelValue::MessageWrapper& value) const override {
    CEL_ASSIGN_OR_RETURN(const google::protobuf::Message* message,
                         UnwrapMessage(value, "HasField"));
    return HasFieldImpl(message, message->GetDescriptor(), field_name);
  }

  absl::StatusOr<CelValue> GetField(
      absl::string_view field_name, const CelValue::MessageWrapper& instance,
      ProtoWrapperTypeOptions unboxing_option,
      cel::MemoryManagerRef memory_manager) const override {
    CEL_ASSIGN_OR_RETURN(const google::protobuf::Message* message,
                         UnwrapMessage(instance, "GetField"));
    return GetFieldImpl(message, message->GetDescriptor(), field_name,
                        unboxing_option, memory_manager);
  }

  absl::StatusOr<LegacyTypeAccessApis::LegacyQualifyResult> Qualify(
      absl::Span<const cel::SelectQualifier> qualifiers,
      const CelValue::MessageWrapper& instance, bool presence_test,
      cel::MemoryManagerRef memory_manager) const override {
    CEL_ASSIGN_OR_RETURN(const google::protobuf::Message* message,
                         UnwrapMessage(instance, "Qualify"));

    return QualifyImpl(message, message->GetDescriptor(), qualifiers,
                       presence_test, memory_manager);
  }

  bool IsEqualTo(
      const CelValue::MessageWrapper& instance,
      const CelValue::MessageWrapper& other_instance) const override {
    absl::StatusOr<const google::protobuf::Message*> lhs =
        UnwrapMessage(instance, "IsEqualTo");
    absl::StatusOr<const google::protobuf::Message*> rhs =
        UnwrapMessage(other_instance, "IsEqualTo");
    if (!lhs.ok() || !rhs.ok()) {
      // Treat this as though the underlying types are different, just return
      // false.
      return false;
    }
    return ProtoEquals(**lhs, **rhs);
  }

  // Implement TypeInfo Apis
  absl::string_view GetTypename(
      const MessageWrapper& wrapped_message) const override {
    if (!wrapped_message.HasFullProto() ||
        wrapped_message.message_ptr() == nullptr) {
      return UnsupportedTypeName();
    }
    auto* message =
        static_cast<const google::protobuf::Message*>(wrapped_message.message_ptr());
    return message->GetDescriptor()->full_name();
  }

  std::string DebugString(
      const MessageWrapper& wrapped_message) const override {
    if (!wrapped_message.HasFullProto() ||
        wrapped_message.message_ptr() == nullptr) {
      return UnsupportedTypeName();
    }
    auto* message =
        static_cast<const google::protobuf::Message*>(wrapped_message.message_ptr());
    return message->ShortDebugString();
  }

  bool DefinesField(absl::string_view field_name) const override {
    // Pretend all our fields exist. Real errors will be returned from field
    // getters and setters.
    return true;
  }

  absl::StatusOr<CelValue::MessageWrapper::Builder> NewInstance(
      cel::MemoryManagerRef memory_manager) const override {
    return absl::UnimplementedError("NewInstance is not implemented");
  }

  absl::StatusOr<CelValue> AdaptFromWellKnownType(
      cel::MemoryManagerRef memory_manager,
      CelValue::MessageWrapper::Builder instance) const override {
    if (!instance.HasFullProto() || instance.message_ptr() == nullptr) {
      return absl::UnimplementedError(
          "MessageLite is not supported, descriptor is required");
    }
    return ProtoMessageTypeAdapter(
               static_cast<const google::protobuf::Message*>(instance.message_ptr())
                   ->GetDescriptor(),
               nullptr)
        .AdaptFromWellKnownType(memory_manager, instance);
  }

  absl::Status SetField(
      absl::string_view field_name, const CelValue& value,
      cel::MemoryManagerRef memory_manager,
      CelValue::MessageWrapper::Builder& instance) const override {
    if (!instance.HasFullProto() || instance.message_ptr() == nullptr) {
      return absl::UnimplementedError(
          "MessageLite is not supported, descriptor is required");
    }
    return ProtoMessageTypeAdapter(
               static_cast<const google::protobuf::Message*>(instance.message_ptr())
                   ->GetDescriptor(),
               nullptr)
        .SetField(field_name, value, memory_manager, instance);
  }

  std::vector<absl::string_view> ListFields(
      const CelValue::MessageWrapper& instance) const override {
    return ListFieldsImpl(instance);
  }

  const LegacyTypeAccessApis* GetAccessApis(
      const MessageWrapper& wrapped_message) const override {
    return this;
  }

  const LegacyTypeMutationApis* GetMutationApis(
      const MessageWrapper& wrapped_message) const override {
    return this;
  }

  static const DucktypedMessageAdapter& GetSingleton() {
    static absl::NoDestructor<DucktypedMessageAdapter> instance;
    return *instance;
  }
};

CelValue MessageCelValueFactory(const google::protobuf::Message* message) {
  return CelValue::CreateMessageWrapper(
      MessageWrapper(message, &DucktypedMessageAdapter::GetSingleton()));
}

}  // namespace

std::string ProtoMessageTypeAdapter::DebugString(
    const MessageWrapper& wrapped_message) const {
  if (!wrapped_message.HasFullProto() ||
      wrapped_message.message_ptr() == nullptr) {
    return UnsupportedTypeName();
  }
  auto* message =
      static_cast<const google::protobuf::Message*>(wrapped_message.message_ptr());
  return message->ShortDebugString();
}

absl::string_view ProtoMessageTypeAdapter::GetTypename(
    const MessageWrapper& wrapped_message) const {
  return descriptor_->full_name();
}

const LegacyTypeMutationApis* ProtoMessageTypeAdapter::GetMutationApis(
    const MessageWrapper& wrapped_message) const {
  // Defer checks for misuse on wrong message kind in the accessor calls.
  return this;
}

const LegacyTypeAccessApis* ProtoMessageTypeAdapter::GetAccessApis(
    const MessageWrapper& wrapped_message) const {
  // Defer checks for misuse on wrong message kind in the builder calls.
  return this;
}

absl::optional<LegacyTypeInfoApis::FieldDescription>
ProtoMessageTypeAdapter::FindFieldByName(absl::string_view field_name) const {
  if (descriptor_ == nullptr) {
    return absl::nullopt;
  }

  const google::protobuf::FieldDescriptor* field_descriptor =
      descriptor_->FindFieldByName(field_name);

  if (field_descriptor == nullptr) {
    return absl::nullopt;
  }

  return LegacyTypeInfoApis::FieldDescription{field_descriptor->number(),
                                              field_descriptor->name()};
}

absl::Status ProtoMessageTypeAdapter::ValidateSetFieldOp(
    bool assertion, absl::string_view field, absl::string_view detail) const {
  if (!assertion) {
    return absl::InvalidArgumentError(
        absl::Substitute("SetField failed on message $0, field '$1': $2",
                         descriptor_->full_name(), field, detail));
  }
  return absl::OkStatus();
}

absl::StatusOr<CelValue::MessageWrapper::Builder>
ProtoMessageTypeAdapter::NewInstance(
    cel::MemoryManagerRef memory_manager) const {
  if (message_factory_ == nullptr) {
    return absl::UnimplementedError(
        absl::StrCat("Cannot create message ", descriptor_->name()));
  }

  // This implementation requires arena-backed memory manager.
  google::protobuf::Arena* arena = ProtoMemoryManagerArena(memory_manager);
  const Message* prototype = message_factory_->GetPrototype(descriptor_);

  Message* msg = (prototype != nullptr) ? prototype->New(arena) : nullptr;

  if (msg == nullptr) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to create message ", descriptor_->name()));
  }
  return MessageWrapper::Builder(msg);
}

bool ProtoMessageTypeAdapter::DefinesField(absl::string_view field_name) const {
  return descriptor_->FindFieldByName(field_name) != nullptr;
}

absl::StatusOr<bool> ProtoMessageTypeAdapter::HasField(
    absl::string_view field_name, const CelValue::MessageWrapper& value) const {
  CEL_ASSIGN_OR_RETURN(const google::protobuf::Message* message,
                       UnwrapMessage(value, "HasField"));
  return HasFieldImpl(message, descriptor_, field_name);
}

absl::StatusOr<CelValue> ProtoMessageTypeAdapter::GetField(
    absl::string_view field_name, const CelValue::MessageWrapper& instance,
    ProtoWrapperTypeOptions unboxing_option,
    cel::MemoryManagerRef memory_manager) const {
  CEL_ASSIGN_OR_RETURN(const google::protobuf::Message* message,
                       UnwrapMessage(instance, "GetField"));

  return GetFieldImpl(message, descriptor_, field_name, unboxing_option,
                      memory_manager);
}

absl::StatusOr<LegacyTypeAccessApis::LegacyQualifyResult>
ProtoMessageTypeAdapter::Qualify(
    absl::Span<const cel::SelectQualifier> qualifiers,
    const CelValue::MessageWrapper& instance, bool presence_test,
    cel::MemoryManagerRef memory_manager) const {
  CEL_ASSIGN_OR_RETURN(const google::protobuf::Message* message,
                       UnwrapMessage(instance, "Qualify"));

  return QualifyImpl(message, descriptor_, qualifiers, presence_test,
                     memory_manager);
}

absl::Status ProtoMessageTypeAdapter::SetField(
    const google::protobuf::FieldDescriptor* field, const CelValue& value,
    google::protobuf::Arena* arena, google::protobuf::Message* message) const {
  if (field->is_map()) {
    constexpr int kKeyField = 1;
    constexpr int kValueField = 2;

    const CelMap* cel_map;
    CEL_RETURN_IF_ERROR(ValidateSetFieldOp(
        value.GetValue<const CelMap*>(&cel_map) && cel_map != nullptr,
        field->name(), "value is not CelMap"));

    auto entry_descriptor = field->message_type();

    CEL_RETURN_IF_ERROR(
        ValidateSetFieldOp(entry_descriptor != nullptr, field->name(),
                           "failed to find map entry descriptor"));
    auto key_field_descriptor = entry_descriptor->FindFieldByNumber(kKeyField);
    auto value_field_descriptor =
        entry_descriptor->FindFieldByNumber(kValueField);

    CEL_RETURN_IF_ERROR(
        ValidateSetFieldOp(key_field_descriptor != nullptr, field->name(),
                           "failed to find key field descriptor"));

    CEL_RETURN_IF_ERROR(
        ValidateSetFieldOp(value_field_descriptor != nullptr, field->name(),
                           "failed to find value field descriptor"));

    CEL_ASSIGN_OR_RETURN(const CelList* key_list, cel_map->ListKeys(arena));
    for (int i = 0; i < key_list->size(); i++) {
      CelValue key = (*key_list).Get(arena, i);

      auto value = (*cel_map).Get(arena, key);
      CEL_RETURN_IF_ERROR(ValidateSetFieldOp(value.has_value(), field->name(),
                                             "error serializing CelMap"));
      Message* entry_msg = message->GetReflection()->AddMessage(message, field);
      CEL_RETURN_IF_ERROR(internal::SetValueToSingleField(
          key, key_field_descriptor, entry_msg, arena));
      CEL_RETURN_IF_ERROR(internal::SetValueToSingleField(
          value.value(), value_field_descriptor, entry_msg, arena));
    }

  } else if (field->is_repeated()) {
    const CelList* cel_list;
    CEL_RETURN_IF_ERROR(ValidateSetFieldOp(
        value.GetValue<const CelList*>(&cel_list) && cel_list != nullptr,
        field->name(), "expected CelList value"));

    for (int i = 0; i < cel_list->size(); i++) {
      CEL_RETURN_IF_ERROR(internal::AddValueToRepeatedField(
          (*cel_list).Get(arena, i), field, message, arena));
    }
  } else {
    CEL_RETURN_IF_ERROR(
        internal::SetValueToSingleField(value, field, message, arena));
  }
  return absl::OkStatus();
}

absl::Status ProtoMessageTypeAdapter::SetField(
    absl::string_view field_name, const CelValue& value,
    cel::MemoryManagerRef memory_manager,
    CelValue::MessageWrapper::Builder& instance) const {
  // Assume proto arena implementation if this provider is used.
  google::protobuf::Arena* arena =
      cel::extensions::ProtoMemoryManagerArena(memory_manager);

  CEL_ASSIGN_OR_RETURN(google::protobuf::Message * mutable_message,
                       UnwrapMessage(instance, "SetField"));

  const google::protobuf::FieldDescriptor* field_descriptor =
      descriptor_->FindFieldByName(field_name);
  CEL_RETURN_IF_ERROR(
      ValidateSetFieldOp(field_descriptor != nullptr, field_name, "not found"));

  return SetField(field_descriptor, value, arena, mutable_message);
}

absl::Status ProtoMessageTypeAdapter::SetFieldByNumber(
    int64_t field_number, const CelValue& value,
    cel::MemoryManagerRef memory_manager,
    CelValue::MessageWrapper::Builder& instance) const {
  // Assume proto arena implementation if this provider is used.
  google::protobuf::Arena* arena =
      cel::extensions::ProtoMemoryManagerArena(memory_manager);

  CEL_ASSIGN_OR_RETURN(google::protobuf::Message * mutable_message,
                       UnwrapMessage(instance, "SetField"));

  const google::protobuf::FieldDescriptor* field_descriptor =
      descriptor_->FindFieldByNumber(field_number);
  CEL_RETURN_IF_ERROR(ValidateSetFieldOp(
      field_descriptor != nullptr, absl::StrCat(field_number), "not found"));

  return SetField(field_descriptor, value, arena, mutable_message);
}

absl::StatusOr<CelValue> ProtoMessageTypeAdapter::AdaptFromWellKnownType(
    cel::MemoryManagerRef memory_manager,
    CelValue::MessageWrapper::Builder instance) const {
  // Assume proto arena implementation if this provider is used.
  google::protobuf::Arena* arena =
      cel::extensions::ProtoMemoryManagerArena(memory_manager);
  CEL_ASSIGN_OR_RETURN(google::protobuf::Message * message,
                       UnwrapMessage(instance, "AdaptFromWellKnownType"));
  return internal::UnwrapMessageToValue(message, &MessageCelValueFactory,
                                        arena);
}

bool ProtoMessageTypeAdapter::IsEqualTo(
    const CelValue::MessageWrapper& instance,
    const CelValue::MessageWrapper& other_instance) const {
  absl::StatusOr<const google::protobuf::Message*> lhs =
      UnwrapMessage(instance, "IsEqualTo");
  absl::StatusOr<const google::protobuf::Message*> rhs =
      UnwrapMessage(other_instance, "IsEqualTo");
  if (!lhs.ok() || !rhs.ok()) {
    // Treat this as though the underlying types are different, just return
    // false.
    return false;
  }
  return ProtoEquals(**lhs, **rhs);
}

std::vector<absl::string_view> ProtoMessageTypeAdapter::ListFields(
    const CelValue::MessageWrapper& instance) const {
  return ListFieldsImpl(instance);
}

const LegacyTypeInfoApis& GetGenericProtoTypeInfoInstance() {
  return DucktypedMessageAdapter::GetSingleton();
}

}  // namespace google::api::expr::runtime
