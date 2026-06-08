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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_STRUCT_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_STRUCT_VALUE_H_

#include <cstdint>
#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/functional/function_ref.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/custom_value.h"
#include "common/values/values.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

class CustomStructValueInterface;
class CustomStructValue;
class Value;
struct CustomStructValueDispatcher;
using CustomStructValueContent = CustomValueContent;

struct CustomStructValueDispatcher {
  using GetTypeId = NativeTypeId (*)(
      const CustomStructValueDispatcher* absl_nonnull dispatcher,
      CustomStructValueContent content);

  using GetArena = google::protobuf::Arena* absl_nullable (*)(
      const CustomStructValueDispatcher* absl_nonnull dispatcher,
      CustomStructValueContent content);

  using GetTypeName = absl::string_view (*)(
      const CustomStructValueDispatcher* absl_nonnull dispatcher,
      CustomStructValueContent content);

  using DebugString = std::string (*)(
      const CustomStructValueDispatcher* absl_nonnull dispatcher,
      CustomStructValueContent content);

  using GetRuntimeType =
      StructType (*)(const CustomStructValueDispatcher* absl_nonnull dispatcher,
                     CustomStructValueContent content);

  using SerializeTo = absl::Status (*)(
      const CustomStructValueDispatcher* absl_nonnull dispatcher,
      CustomStructValueContent content,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output);

  using ConvertToJsonObject = absl::Status (*)(
      const CustomStructValueDispatcher* absl_nonnull dispatcher,
      CustomStructValueContent content,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json);

  using Equal = absl::Status (*)(
      const CustomStructValueDispatcher* absl_nonnull dispatcher,
      CustomStructValueContent content, const StructValue& other,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result);

  using IsZeroValue =
      bool (*)(const CustomStructValueDispatcher* absl_nonnull dispatcher,
               CustomStructValueContent content);

  using GetFieldByName = absl::Status (*)(
      const CustomStructValueDispatcher* absl_nonnull dispatcher,
      CustomStructValueContent content, absl::string_view name,
      ProtoWrapperTypeOptions unboxing_options,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result);

  using GetFieldByNumber = absl::Status (*)(
      const CustomStructValueDispatcher* absl_nonnull dispatcher,
      CustomStructValueContent content, int64_t number,
      ProtoWrapperTypeOptions unboxing_options,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result);

  using HasFieldByName = absl::StatusOr<bool> (*)(
      const CustomStructValueDispatcher* absl_nonnull dispatcher,
      CustomStructValueContent content, absl::string_view name);

  using HasFieldByNumber = absl::StatusOr<bool> (*)(
      const CustomStructValueDispatcher* absl_nonnull dispatcher,
      CustomStructValueContent content, int64_t number);

  using ForEachField = absl::Status (*)(
      const CustomStructValueDispatcher* absl_nonnull dispatcher,
      CustomStructValueContent content,
      absl::FunctionRef<absl::StatusOr<bool>(absl::string_view, const Value&)>
          callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena);

  using Quality = absl::Status (*)(
      const CustomStructValueDispatcher* absl_nonnull dispatcher,
      CustomStructValueContent content,
      absl::Span<const SelectQualifier> qualifiers, bool presence_test,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result,
      int* absl_nonnull count);

  using Clone = CustomStructValue (*)(
      const CustomStructValueDispatcher* absl_nonnull dispatcher,
      CustomStructValueContent content, google::protobuf::Arena* absl_nonnull arena);

  absl_nonnull GetTypeId get_type_id;

  absl_nonnull GetArena get_arena;

  absl_nonnull GetTypeName get_type_name;

  absl_nullable DebugString debug_string = nullptr;

  absl_nullable GetRuntimeType get_runtime_type = nullptr;

  absl_nullable SerializeTo serialize_to = nullptr;

  absl_nullable ConvertToJsonObject convert_to_json_object = nullptr;

  absl_nullable Equal equal = nullptr;

  absl_nonnull IsZeroValue is_zero_value;

  absl_nonnull GetFieldByName get_field_by_name;

  absl_nullable GetFieldByNumber get_field_by_number = nullptr;

  absl_nonnull HasFieldByName has_field_by_name;

  absl_nullable HasFieldByNumber has_field_by_number = nullptr;

  absl_nonnull ForEachField for_each_field;

  absl_nullable Quality qualify = nullptr;

  absl_nonnull Clone clone;
};

class CustomStructValueInterface {
 public:
  CustomStructValueInterface() = default;
  CustomStructValueInterface(const CustomStructValueInterface&) = delete;
  CustomStructValueInterface(CustomStructValueInterface&&) = delete;

  virtual ~CustomStructValueInterface() = default;

  CustomStructValueInterface& operator=(const CustomStructValueInterface&) =
      delete;
  CustomStructValueInterface& operator=(CustomStructValueInterface&&) = delete;

  using ForEachFieldCallback =
      absl::FunctionRef<absl::StatusOr<bool>(absl::string_view, const Value&)>;

 private:
  friend class CustomStructValue;
  friend absl::Status common_internal::StructValueEqual(
      const CustomStructValueInterface& lhs, const StructValue& rhs,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result);

  virtual std::string DebugString() const = 0;

  virtual absl::Status SerializeTo(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const = 0;

  virtual absl::Status ConvertToJsonObject(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const = 0;

  virtual absl::string_view GetTypeName() const = 0;

  virtual StructType GetRuntimeType() const {
    return common_internal::MakeBasicStructType(GetTypeName());
  }

  virtual absl::Status Equal(
      const StructValue& other,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const;

  virtual bool IsZeroValue() const = 0;

  virtual absl::Status GetFieldByName(
      absl::string_view name, ProtoWrapperTypeOptions unboxing_options,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const = 0;

  virtual absl::Status GetFieldByNumber(
      int64_t number, ProtoWrapperTypeOptions unboxing_options,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const = 0;

  virtual absl::StatusOr<bool> HasFieldByName(absl::string_view name) const = 0;

  virtual absl::StatusOr<bool> HasFieldByNumber(int64_t number) const = 0;

  virtual absl::Status ForEachField(
      ForEachFieldCallback callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const = 0;

  virtual absl::Status Qualify(
      absl::Span<const SelectQualifier> qualifiers, bool presence_test,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result,
      int* absl_nonnull count) const;

  virtual CustomStructValue Clone(google::protobuf::Arena* absl_nonnull arena) const = 0;

  virtual NativeTypeId GetNativeTypeId() const = 0;

  struct Content {
    const CustomStructValueInterface* absl_nonnull interface;
    google::protobuf::Arena* absl_nonnull arena;
  };
};

// Creates a custom struct value from a manual dispatch table `dispatcher` and
// opaque data `content` whose format is only know to functions in the manual
// dispatch table. The dispatch table should probably be valid for the lifetime
// of the process, but at a minimum must outlive all instances of the resulting
// value.
//
// IMPORTANT: This approach to implementing CustomStructValues should only be
// used when you know exactly what you are doing. When in doubt, just implement
// CustomStructValueInterface.
CustomStructValue UnsafeCustomStructValue(
    const CustomStructValueDispatcher* absl_nonnull dispatcher
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    CustomStructValueContent content);

class CustomStructValue final
    : private common_internal::StructValueMixin<CustomStructValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kStruct;

  // Constructs a custom struct value from an implementation of
  // `CustomStructValueInterface` `interface` whose lifetime is tied to that of
  // the arena `arena`.
  CustomStructValue(const CustomStructValueInterface* absl_nonnull
                    interface ABSL_ATTRIBUTE_LIFETIME_BOUND,
                    google::protobuf::Arena* absl_nonnull arena
                        ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    ABSL_DCHECK(interface != nullptr);
    ABSL_DCHECK(arena != nullptr);
    content_ =
        CustomStructValueContent::From(CustomStructValueInterface::Content{
            .interface = interface, .arena = arena});
  }

  CustomStructValue() = default;
  CustomStructValue(const CustomStructValue&) = default;
  CustomStructValue(CustomStructValue&&) = default;
  CustomStructValue& operator=(const CustomStructValue&) = default;
  CustomStructValue& operator=(CustomStructValue&&) = default;

  static constexpr ValueKind kind() { return kKind; }

  NativeTypeId GetTypeId() const;

  StructType GetRuntimeType() const;

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  // See Value::SerializeTo().
  absl::Status SerializeTo(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const;

  // See Value::ConvertToJson().
  absl::Status ConvertToJson(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const;

  // See Value::ConvertToJsonObject().
  absl::Status ConvertToJsonObject(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const;

  absl::Status Equal(const Value& other,
                     const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                     google::protobuf::MessageFactory* absl_nonnull message_factory,
                     google::protobuf::Arena* absl_nonnull arena,
                     Value* absl_nonnull result) const;
  using StructValueMixin::Equal;

  bool IsZeroValue() const;

  CustomStructValue Clone(google::protobuf::Arena* absl_nonnull arena) const;

  absl::Status GetFieldByName(
      absl::string_view name, ProtoWrapperTypeOptions unboxing_options,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const;
  using StructValueMixin::GetFieldByName;

  absl::Status GetFieldByNumber(
      int64_t number, ProtoWrapperTypeOptions unboxing_options,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const;
  using StructValueMixin::GetFieldByNumber;

  absl::StatusOr<bool> HasFieldByName(absl::string_view name) const;

  absl::StatusOr<bool> HasFieldByNumber(int64_t number) const;

  using ForEachFieldCallback = CustomStructValueInterface::ForEachFieldCallback;

  absl::Status ForEachField(
      ForEachFieldCallback callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  absl::Status Qualify(
      absl::Span<const SelectQualifier> qualifiers, bool presence_test,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result,
      int* absl_nonnull count) const;
  using StructValueMixin::Qualify;

  const CustomStructValueDispatcher* absl_nullable dispatcher() const {
    return dispatcher_;
  }

  CustomStructValueContent content() const {
    ABSL_DCHECK(dispatcher_ != nullptr);
    return content_;
  }

  const CustomStructValueInterface* absl_nullable interface() const {
    if (dispatcher_ == nullptr) {
      return content_.To<CustomStructValueInterface::Content>().interface;
    }
    return nullptr;
  }

  explicit operator bool() const {
    if (dispatcher_ == nullptr) {
      return content_.To<CustomStructValueInterface::Content>().interface !=
             nullptr;
    }
    return true;
  }

  friend void swap(CustomStructValue& lhs, CustomStructValue& rhs) noexcept {
    using std::swap;
    swap(lhs.dispatcher_, rhs.dispatcher_);
    swap(lhs.content_, rhs.content_);
  }

 private:
  friend class common_internal::ValueMixin<CustomStructValue>;
  friend class common_internal::StructValueMixin<CustomStructValue>;
  friend CustomStructValue UnsafeCustomStructValue(
      const CustomStructValueDispatcher* absl_nonnull dispatcher
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      CustomStructValueContent content);

  // Constructs a custom struct value from a dispatcher and content. Only
  // accessible from `UnsafeCustomStructValue`.
  CustomStructValue(const CustomStructValueDispatcher* absl_nonnull dispatcher
                        ABSL_ATTRIBUTE_LIFETIME_BOUND,
                    CustomStructValueContent content)
      : dispatcher_(dispatcher), content_(content) {
    ABSL_DCHECK(dispatcher != nullptr);
    ABSL_DCHECK(dispatcher->get_type_id != nullptr);
    ABSL_DCHECK(dispatcher->get_arena != nullptr);
    ABSL_DCHECK(dispatcher->get_type_name != nullptr);
    ABSL_DCHECK(dispatcher->is_zero_value != nullptr);
    ABSL_DCHECK(dispatcher->get_field_by_name != nullptr);
    ABSL_DCHECK(dispatcher->has_field_by_name != nullptr);
    ABSL_DCHECK(dispatcher->for_each_field != nullptr);
    ABSL_DCHECK(dispatcher->clone != nullptr);
  }

  const CustomStructValueDispatcher* absl_nullable dispatcher_ = nullptr;
  CustomStructValueContent content_ = CustomStructValueContent::Zero();
};

inline std::ostream& operator<<(std::ostream& out,
                                const CustomStructValue& value) {
  return out << value.DebugString();
}

template <>
struct NativeTypeTraits<CustomStructValue> final {
  static NativeTypeId Id(const CustomStructValue& type) {
    return type.GetTypeId();
  }
};

inline CustomStructValue UnsafeCustomStructValue(
    const CustomStructValueDispatcher* absl_nonnull dispatcher
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    CustomStructValueContent content) {
  return CustomStructValue(dispatcher, content);
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_STRUCT_VALUE_H_
