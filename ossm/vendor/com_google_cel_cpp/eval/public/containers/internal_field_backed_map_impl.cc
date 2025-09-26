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

#include "eval/public/containers/internal_field_backed_map_impl.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/field_access_impl.h"
#include "eval/public/structs/protobuf_value_factory.h"
#include "extensions/protobuf/internal/map_reflection.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime::internal {

namespace {
using google::protobuf::Descriptor;
using google::protobuf::FieldDescriptor;
using google::protobuf::MapValueConstRef;
using google::protobuf::Message;

// Map entries have two field tags
// 1 - for key
// 2 - for value
constexpr int kKeyTag = 1;
constexpr int kValueTag = 2;

class KeyList : public CelList {
 public:
  // message contains the "repeated" field
  // descriptor FieldDescriptor for the field
  KeyList(const google::protobuf::Message* message,
          const google::protobuf::FieldDescriptor* descriptor,
          const ProtobufValueFactory& factory, google::protobuf::Arena* arena)
      : message_(message),
        descriptor_(descriptor),
        reflection_(message_->GetReflection()),
        factory_(factory),
        arena_(arena) {}

  // List size.
  int size() const override {
    return reflection_->FieldSize(*message_, descriptor_);
  }

  // List element access operator.
  CelValue operator[](int index) const override {
    const Message* entry =
        &reflection_->GetRepeatedMessage(*message_, descriptor_, index);

    if (entry == nullptr) {
      return CelValue::CreateNull();
    }

    const Descriptor* entry_descriptor = entry->GetDescriptor();
    // Key Tag == 1
    const FieldDescriptor* key_desc =
        entry_descriptor->FindFieldByNumber(kKeyTag);

    absl::StatusOr<CelValue> key_value = CreateValueFromSingleField(
        entry, key_desc, ProtoWrapperTypeOptions::kUnsetProtoDefault, factory_,
        arena_);
    if (!key_value.ok()) {
      return CreateErrorValue(arena_, key_value.status());
    }
    return *key_value;
  }

 private:
  const google::protobuf::Message* message_;
  const google::protobuf::FieldDescriptor* descriptor_;
  const google::protobuf::Reflection* reflection_;
  const ProtobufValueFactory& factory_;
  google::protobuf::Arena* arena_;
};

bool MatchesMapKeyType(const FieldDescriptor* key_desc, const CelValue& key) {
  switch (key_desc->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return key.IsBool();
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      // fall through
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return key.IsInt64();
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      // fall through
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return key.IsUint64();
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      return key.IsString();
    default:
      return false;
  }
}

absl::Status InvalidMapKeyType(absl::string_view key_type) {
  return absl::InvalidArgumentError(
      absl::StrCat("Invalid map key type: '", key_type, "'"));
}

}  // namespace

FieldBackedMapImpl::FieldBackedMapImpl(
    const google::protobuf::Message* message, const google::protobuf::FieldDescriptor* descriptor,
    ProtobufValueFactory factory, google::protobuf::Arena* arena)
    : message_(message),
      descriptor_(descriptor),
      key_desc_(descriptor_->message_type()->FindFieldByNumber(kKeyTag)),
      value_desc_(descriptor_->message_type()->FindFieldByNumber(kValueTag)),
      reflection_(message_->GetReflection()),
      factory_(std::move(factory)),
      arena_(arena),
      key_list_(
          std::make_unique<KeyList>(message, descriptor, factory_, arena)) {}

int FieldBackedMapImpl::size() const {
  return reflection_->FieldSize(*message_, descriptor_);
}

absl::StatusOr<const CelList*> FieldBackedMapImpl::ListKeys() const {
  return key_list_.get();
}

absl::StatusOr<bool> FieldBackedMapImpl::Has(const CelValue& key) const {
  MapValueConstRef value_ref;
  return LookupMapValue(key, &value_ref);
}

absl::optional<CelValue> FieldBackedMapImpl::operator[](CelValue key) const {
  // Fast implementation which uses a friend method to do a hash-based key
  // lookup.
  MapValueConstRef value_ref;
  auto lookup_result = LookupMapValue(key, &value_ref);
  if (!lookup_result.ok()) {
    return CreateErrorValue(arena_, lookup_result.status());
  }
  if (!*lookup_result) {
    return absl::nullopt;
  }

  // Get value descriptor treating it as a repeated field.
  // All values in protobuf map have the same type.
  // The map is not empty, because LookupMapValue returned true.
  absl::StatusOr<CelValue> result = CreateValueFromMapValue(
      message_, value_desc_, &value_ref, factory_, arena_);
  if (!result.ok()) {
    return CreateErrorValue(arena_, result.status());
  }
  return *result;
}

absl::StatusOr<bool> FieldBackedMapImpl::LookupMapValue(
    const CelValue& key, MapValueConstRef* value_ref) const {
  if (!MatchesMapKeyType(key_desc_, key)) {
    return InvalidMapKeyType(key_desc_->cpp_type_name());
  }

  std::string map_key_string;
  google::protobuf::MapKey proto_key;
  switch (key_desc_->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL: {
      bool key_value;
      key.GetValue(&key_value);
      proto_key.SetBoolValue(key_value);
    } break;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32: {
      int64_t key_value;
      key.GetValue(&key_value);
      if (key_value > std::numeric_limits<int32_t>::max() ||
          key_value < std::numeric_limits<int32_t>::lowest()) {
        return absl::OutOfRangeError("integer overflow");
      }
      proto_key.SetInt32Value(key_value);
    } break;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64: {
      int64_t key_value;
      key.GetValue(&key_value);
      proto_key.SetInt64Value(key_value);
    } break;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
      CelValue::StringHolder key_value;
      key.GetValue(&key_value);
      map_key_string.assign(key_value.value().data(), key_value.value().size());
      proto_key.SetStringValue(map_key_string);
    } break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32: {
      uint64_t key_value;
      key.GetValue(&key_value);
      if (key_value > std::numeric_limits<uint32_t>::max()) {
        return absl::OutOfRangeError("unsigned integer overlow");
      }
      proto_key.SetUInt32Value(key_value);
    } break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64: {
      uint64_t key_value;
      key.GetValue(&key_value);
      proto_key.SetUInt64Value(key_value);
    } break;
    default:
      return InvalidMapKeyType(key_desc_->cpp_type_name());
  }
  // Look the value up
  return cel::extensions::protobuf_internal::LookupMapValue(
      *reflection_, *message_, *descriptor_, proto_key, value_ref);
}

absl::StatusOr<bool> FieldBackedMapImpl::LegacyHasMapValue(
    const CelValue& key) const {
  auto lookup_result = LegacyLookupMapValue(key);
  if (!lookup_result.has_value()) {
    return false;
  }
  auto result = *lookup_result;
  if (result.IsError()) {
    return *(result.ErrorOrDie());
  }
  return true;
}

absl::optional<CelValue> FieldBackedMapImpl::LegacyLookupMapValue(
    const CelValue& key) const {
  // Ensure that the key matches the key type.
  if (!MatchesMapKeyType(key_desc_, key)) {
    return CreateErrorValue(arena_,
                            InvalidMapKeyType(key_desc_->cpp_type_name()));
  }

  int map_size = size();
  for (int i = 0; i < map_size; i++) {
    const Message* entry =
        &reflection_->GetRepeatedMessage(*message_, descriptor_, i);
    if (entry == nullptr) continue;

    // Key Tag == 1
    absl::StatusOr<CelValue> key_value = CreateValueFromSingleField(
        entry, key_desc_, ProtoWrapperTypeOptions::kUnsetProtoDefault, factory_,
        arena_);
    if (!key_value.ok()) {
      return CreateErrorValue(arena_, key_value.status());
    }

    bool match = false;
    switch (key_desc_->cpp_type()) {
      case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
        match = key.BoolOrDie() == key_value->BoolOrDie();
        break;
      case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
        // fall through
      case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
        match = key.Int64OrDie() == key_value->Int64OrDie();
        break;
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
        // fall through
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
        match = key.Uint64OrDie() == key_value->Uint64OrDie();
        break;
      case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
        match = key.StringOrDie() == key_value->StringOrDie();
        break;
      default:
        // this would normally indicate a bad key type, which should not be
        // possible based on the earlier test.
        break;
    }

    if (match) {
      absl::StatusOr<CelValue> value_cel_value = CreateValueFromSingleField(
          entry, value_desc_, ProtoWrapperTypeOptions::kUnsetProtoDefault,
          factory_, arena_);
      if (!value_cel_value.ok()) {
        return CreateErrorValue(arena_, value_cel_value.status());
      }
      return *value_cel_value;
    }
  }
  return {};
}

}  // namespace google::api::expr::runtime::internal
