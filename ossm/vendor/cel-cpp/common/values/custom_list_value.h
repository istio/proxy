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

// `CustomListValue` represents values of the primitive `list` type.
// `CustomListValueView` is a non-owning view of `CustomListValue`.
// `CustomListValueInterface` is the abstract base class of implementations.
// `CustomListValue` and `CustomListValueView` act as smart pointers to
// `CustomListValueInterface`.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_LIST_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_LIST_VALUE_H_

#include <cstddef>
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
#include "common/native_type.h"
#include "common/value_kind.h"
#include "common/values/custom_value.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class CustomListValueInterface;
class CustomListValueInterfaceIterator;
class CustomListValue;
struct CustomListValueDispatcher;
using CustomListValueContent = CustomValueContent;

struct CustomListValueDispatcher {
  using GetTypeId =
      NativeTypeId (*)(const CustomListValueDispatcher* absl_nonnull dispatcher,
                       CustomListValueContent content);

  using GetArena = google::protobuf::Arena* absl_nullable (*)(
      const CustomListValueDispatcher* absl_nonnull dispatcher,
      CustomListValueContent content);

  using DebugString =
      std::string (*)(const CustomListValueDispatcher* absl_nonnull dispatcher,
                      CustomListValueContent content);

  using SerializeTo = absl::Status (*)(
      const CustomListValueDispatcher* absl_nonnull dispatcher,
      CustomListValueContent content,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output);

  using ConvertToJsonArray = absl::Status (*)(
      const CustomListValueDispatcher* absl_nonnull dispatcher,
      CustomListValueContent content,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json);

  using Equal = absl::Status (*)(
      const CustomListValueDispatcher* absl_nonnull dispatcher,
      CustomListValueContent content, const ListValue& other,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result);

  using IsZeroValue =
      bool (*)(const CustomListValueDispatcher* absl_nonnull dispatcher,
               CustomListValueContent content);

  using IsEmpty =
      bool (*)(const CustomListValueDispatcher* absl_nonnull dispatcher,
               CustomListValueContent content);

  using Size =
      size_t (*)(const CustomListValueDispatcher* absl_nonnull dispatcher,
                 CustomListValueContent content);

  using Get = absl::Status (*)(
      const CustomListValueDispatcher* absl_nonnull dispatcher,
      CustomListValueContent content, size_t index,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result);

  using ForEach = absl::Status (*)(
      const CustomListValueDispatcher* absl_nonnull dispatcher,
      CustomListValueContent content,
      absl::FunctionRef<absl::StatusOr<bool>(size_t, const Value&)> callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena);

  using NewIterator = absl::StatusOr<absl_nonnull ValueIteratorPtr> (*)(
      const CustomListValueDispatcher* absl_nonnull dispatcher,
      CustomListValueContent content);

  using Contains = absl::Status (*)(
      const CustomListValueDispatcher* absl_nonnull dispatcher,
      CustomListValueContent content, const Value& other,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result);

  using Clone = CustomListValue (*)(
      const CustomListValueDispatcher* absl_nonnull dispatcher,
      CustomListValueContent content, google::protobuf::Arena* absl_nonnull arena);

  absl_nonnull GetTypeId get_type_id;

  absl_nonnull GetArena get_arena;

  // If null, simply returns "list".
  absl_nullable DebugString debug_string = nullptr;

  // If null, attempts to serialize results in an UNIMPLEMENTED error.
  absl_nullable SerializeTo serialize_to = nullptr;

  // If null, attempts to convert to JSON results in an UNIMPLEMENTED error.
  absl_nullable ConvertToJsonArray convert_to_json_array = nullptr;

  // If null, an nonoptimal fallback implementation for equality is used.
  absl_nullable Equal equal = nullptr;

  absl_nonnull IsZeroValue is_zero_value;

  // If null, `size(...) == 0` is used.
  absl_nullable IsEmpty is_empty = nullptr;

  absl_nonnull Size size;

  absl_nonnull Get get;

  // If null, a fallback implementation using `size` and `get` is used.
  absl_nullable ForEach for_each = nullptr;

  // If null, a fallback implementation using `size` and `get` is used.
  absl_nullable NewIterator new_iterator = nullptr;

  // If null, a fallback implementation is used.
  absl_nullable Contains contains = nullptr;

  absl_nonnull Clone clone;
};

class CustomListValueInterface {
 public:
  CustomListValueInterface() = default;
  CustomListValueInterface(const CustomListValueInterface&) = delete;
  CustomListValueInterface(CustomListValueInterface&&) = delete;

  virtual ~CustomListValueInterface() = default;

  CustomListValueInterface& operator=(const CustomListValueInterface&) = delete;
  CustomListValueInterface& operator=(CustomListValueInterface&&) = delete;

  using ForEachCallback = absl::FunctionRef<absl::StatusOr<bool>(const Value&)>;

  using ForEachWithIndexCallback =
      absl::FunctionRef<absl::StatusOr<bool>(size_t, const Value&)>;

 private:
  friend class CustomListValueInterfaceIterator;
  friend class CustomListValue;
  friend absl::Status common_internal::ListValueEqual(
      const CustomListValueInterface& lhs, const ListValue& rhs,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result);

  virtual std::string DebugString() const = 0;

  virtual absl::Status SerializeTo(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const;

  virtual absl::Status ConvertToJsonArray(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const = 0;

  virtual absl::Status Equal(
      const ListValue& other,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const;

  virtual bool IsZeroValue() const { return IsEmpty(); }

  virtual bool IsEmpty() const { return Size() == 0; }

  virtual size_t Size() const = 0;

  virtual absl::Status Get(
      size_t index, const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const = 0;

  virtual absl::Status ForEach(
      ForEachWithIndexCallback callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  virtual absl::StatusOr<absl_nonnull ValueIteratorPtr> NewIterator() const;

  virtual absl::Status Contains(
      const Value& other,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const;

  virtual CustomListValue Clone(google::protobuf::Arena* absl_nonnull arena) const = 0;

  virtual NativeTypeId GetNativeTypeId() const = 0;

  struct Content {
    const CustomListValueInterface* absl_nonnull interface;
    const google::protobuf::Arena* absl_nullable arena;
  };
};

// Creates a custom list value from a manual dispatch table `dispatcher` and
// opaque data `content` whose format is only know to functions in the manual
// dispatch table. The dispatch table should probably be valid for the lifetime
// of the process, but at a minimum must outlive all instances of the resulting
// value.
//
// IMPORTANT: This approach to implementing CustomListValue should only be
// used when you know exactly what you are doing. When in doubt, just implement
// CustomListValueInterface.
CustomListValue UnsafeCustomListValue(
    const CustomListValueDispatcher* absl_nonnull dispatcher
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    CustomListValueContent content);

class CustomListValue final
    : private common_internal::ListValueMixin<CustomListValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kList;

  // Constructs a custom list value from an implementation of
  // `CustomListValueInterface` `interface` whose lifetime is tied to that of
  // the arena `arena`.
  CustomListValue(const CustomListValueInterface* absl_nonnull
                  interface ABSL_ATTRIBUTE_LIFETIME_BOUND,
                  google::protobuf::Arena* absl_nonnull arena
                      ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    ABSL_DCHECK(interface != nullptr);
    ABSL_DCHECK(arena != nullptr);
    content_ = CustomListValueContent::From(CustomListValueInterface::Content{
        .interface = interface, .arena = arena});
  }

  CustomListValue();
  CustomListValue(const CustomListValue&) = default;
  CustomListValue(CustomListValue&&) = default;
  CustomListValue& operator=(const CustomListValue&) = default;
  CustomListValue& operator=(CustomListValue&&) = default;

  static constexpr ValueKind kind() { return kKind; }

  NativeTypeId GetTypeId() const;

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

  // See Value::ConvertToJsonArray().
  absl::Status ConvertToJsonArray(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const;

  absl::Status Equal(const Value& other,
                     const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                     google::protobuf::MessageFactory* absl_nonnull message_factory,
                     google::protobuf::Arena* absl_nonnull arena,
                     Value* absl_nonnull result) const;
  using ListValueMixin::Equal;

  bool IsZeroValue() const;

  CustomListValue Clone(google::protobuf::Arena* absl_nonnull arena) const;

  bool IsEmpty() const;

  size_t Size() const;

  // See ListValueInterface::Get for documentation.
  absl::Status Get(size_t index,
                   const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                   google::protobuf::MessageFactory* absl_nonnull message_factory,
                   google::protobuf::Arena* absl_nonnull arena,
                   Value* absl_nonnull result) const;
  using ListValueMixin::Get;

  using ForEachCallback = typename CustomListValueInterface::ForEachCallback;

  using ForEachWithIndexCallback =
      typename CustomListValueInterface::ForEachWithIndexCallback;

  absl::Status ForEach(
      ForEachWithIndexCallback callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;
  using ListValueMixin::ForEach;

  absl::StatusOr<absl_nonnull ValueIteratorPtr> NewIterator() const;

  absl::Status Contains(
      const Value& other,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const;
  using ListValueMixin::Contains;

  const CustomListValueDispatcher* absl_nullable dispatcher() const {
    return dispatcher_;
  }

  CustomListValueContent content() const {
    ABSL_DCHECK(dispatcher_ != nullptr);
    return content_;
  }

  const CustomListValueInterface* absl_nullable interface() const {
    if (dispatcher_ == nullptr) {
      return content_.To<CustomListValueInterface::Content>().interface;
    }
    return nullptr;
  }

  friend void swap(CustomListValue& lhs, CustomListValue& rhs) noexcept {
    using std::swap;
    swap(lhs.dispatcher_, rhs.dispatcher_);
    swap(lhs.content_, rhs.content_);
  }

 private:
  friend class common_internal::ValueMixin<CustomListValue>;
  friend class common_internal::ListValueMixin<CustomListValue>;
  friend CustomListValue UnsafeCustomListValue(
      const CustomListValueDispatcher* absl_nonnull dispatcher
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      CustomListValueContent content);

  CustomListValue(const CustomListValueDispatcher* absl_nonnull dispatcher,
                  CustomListValueContent content)
      : dispatcher_(dispatcher), content_(content) {
    ABSL_DCHECK(dispatcher != nullptr);
    ABSL_DCHECK(dispatcher->get_type_id != nullptr);
    ABSL_DCHECK(dispatcher->get_arena != nullptr);
    ABSL_DCHECK(dispatcher->is_zero_value != nullptr);
    ABSL_DCHECK(dispatcher->size != nullptr);
    ABSL_DCHECK(dispatcher->get != nullptr);
    ABSL_DCHECK(dispatcher->clone != nullptr);
  }

  const CustomListValueDispatcher* absl_nullable dispatcher_ = nullptr;
  CustomListValueContent content_ = CustomListValueContent::Zero();
};

inline std::ostream& operator<<(std::ostream& out,
                                const CustomListValue& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<CustomListValue> final {
  static NativeTypeId Id(const CustomListValue& type) {
    return type.GetTypeId();
  }
};

inline CustomListValue UnsafeCustomListValue(
    const CustomListValueDispatcher* absl_nonnull dispatcher
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    CustomListValueContent content) {
  return CustomListValue(dispatcher, content);
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_LIST_VALUE_H_
