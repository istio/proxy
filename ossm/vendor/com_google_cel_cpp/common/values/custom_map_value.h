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

// `CustomMapValue` represents values of the primitive `map` type.
// `CustomMapValueView` is a non-owning view of `CustomMapValue`.
// `CustomMapValueInterface` is the abstract base class of implementations.
// `CustomMapValue` and `CustomMapValueView` act as smart pointers to
// `CustomMapValueInterface`.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MAP_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MAP_VALUE_H_

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
class ListValue;
class CustomMapValueInterface;
class CustomMapValueInterfaceKeysIterator;
class CustomMapValue;
using CustomMapValueContent = CustomValueContent;

struct CustomMapValueDispatcher {
  using GetTypeId =
      NativeTypeId (*)(const CustomMapValueDispatcher* absl_nonnull dispatcher,
                       CustomMapValueContent content);

  using GetArena = google::protobuf::Arena* absl_nullable (*)(
      const CustomMapValueDispatcher* absl_nonnull dispatcher,
      CustomMapValueContent content);

  using DebugString =
      std::string (*)(const CustomMapValueDispatcher* absl_nonnull dispatcher,
                      CustomMapValueContent content);

  using SerializeTo = absl::Status (*)(
      const CustomMapValueDispatcher* absl_nonnull dispatcher,
      CustomMapValueContent content,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output);

  using ConvertToJsonObject = absl::Status (*)(
      const CustomMapValueDispatcher* absl_nonnull dispatcher,
      CustomMapValueContent content,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json);

  using Equal = absl::Status (*)(
      const CustomMapValueDispatcher* absl_nonnull dispatcher,
      CustomMapValueContent content, const MapValue& other,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result);

  using IsZeroValue =
      bool (*)(const CustomMapValueDispatcher* absl_nonnull dispatcher,
               CustomMapValueContent content);

  using IsEmpty =
      bool (*)(const CustomMapValueDispatcher* absl_nonnull dispatcher,
               CustomMapValueContent content);

  using Size =
      size_t (*)(const CustomMapValueDispatcher* absl_nonnull dispatcher,
                 CustomMapValueContent content);

  using Find = absl::StatusOr<bool> (*)(
      const CustomMapValueDispatcher* absl_nonnull dispatcher,
      CustomMapValueContent content, const Value& key,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result);

  using Has = absl::StatusOr<bool> (*)(
      const CustomMapValueDispatcher* absl_nonnull dispatcher,
      CustomMapValueContent content, const Value& key,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena);

  using ListKeys = absl::Status (*)(
      const CustomMapValueDispatcher* absl_nonnull dispatcher,
      CustomMapValueContent content,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, ListValue* absl_nonnull result);

  using ForEach = absl::Status (*)(
      const CustomMapValueDispatcher* absl_nonnull dispatcher,
      CustomMapValueContent content,
      absl::FunctionRef<absl::StatusOr<bool>(const Value&, const Value&)>
          callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena);

  using NewIterator = absl::StatusOr<absl_nonnull ValueIteratorPtr> (*)(
      const CustomMapValueDispatcher* absl_nonnull dispatcher,
      CustomMapValueContent content);

  using Clone = CustomMapValue (*)(
      const CustomMapValueDispatcher* absl_nonnull dispatcher,
      CustomMapValueContent content, google::protobuf::Arena* absl_nonnull arena);

  absl_nonnull GetTypeId get_type_id;

  absl_nonnull GetArena get_arena;

  // If null, simply returns "map".
  absl_nullable DebugString debug_string = nullptr;

  // If null, attempts to serialize results in an UNIMPLEMENTED error.
  absl_nullable SerializeTo serialize_to = nullptr;

  // If null, attempts to convert to JSON results in an UNIMPLEMENTED error.
  absl_nullable ConvertToJsonObject convert_to_json_object = nullptr;

  // If null, an nonoptimal fallback implementation for equality is used.
  absl_nullable Equal equal = nullptr;

  absl_nonnull IsZeroValue is_zero_value;

  // If null, `size(...) == 0` is used.
  absl_nullable IsEmpty is_empty = nullptr;

  absl_nonnull Size size;

  absl_nonnull Find find;

  absl_nonnull Has has;

  absl_nonnull ListKeys list_keys;

  // If null, a fallback implementation based on `list_keys` is used.
  absl_nullable ForEach for_each = nullptr;

  // If null, a fallback implementation based on `list_keys` is used.
  absl_nullable NewIterator new_iterator = nullptr;

  absl_nonnull Clone clone;
};

class CustomMapValueInterface {
 public:
  CustomMapValueInterface() = default;
  CustomMapValueInterface(const CustomMapValueInterface&) = delete;
  CustomMapValueInterface(CustomMapValueInterface&&) = delete;

  virtual ~CustomMapValueInterface() = default;

  CustomMapValueInterface& operator=(const CustomMapValueInterface&) = delete;
  CustomMapValueInterface& operator=(CustomMapValueInterface&&) = delete;

  using ForEachCallback =
      absl::FunctionRef<absl::StatusOr<bool>(const Value&, const Value&)>;

 private:
  friend class CustomMapValueInterfaceIterator;
  friend class CustomMapValue;
  friend absl::Status common_internal::MapValueEqual(
      const CustomMapValueInterface& lhs, const MapValue& rhs,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result);

  virtual std::string DebugString() const = 0;

  virtual absl::Status SerializeTo(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const;

  virtual absl::Status ConvertToJsonObject(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const = 0;

  virtual absl::Status Equal(
      const MapValue& other,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const;

  virtual bool IsZeroValue() const { return IsEmpty(); }

  // Returns `true` if this map contains no entries, `false` otherwise.
  virtual bool IsEmpty() const { return Size() == 0; }

  // Returns the number of entries in this map.
  virtual size_t Size() const = 0;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  virtual absl::Status ListKeys(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena,
      ListValue* absl_nonnull result) const = 0;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  virtual absl::Status ForEach(
      ForEachCallback callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  // By default, implementations do not guarantee any iteration order. Unless
  // specified otherwise, assume the iteration order is random.
  virtual absl::StatusOr<absl_nonnull ValueIteratorPtr> NewIterator() const;

  virtual CustomMapValue Clone(google::protobuf::Arena* absl_nonnull arena) const = 0;

  virtual absl::StatusOr<bool> Find(
      const Value& key,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const = 0;

  virtual absl::StatusOr<bool> Has(
      const Value& key,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const = 0;

  virtual NativeTypeId GetNativeTypeId() const = 0;

  struct Content {
    const CustomMapValueInterface* absl_nonnull interface;
    google::protobuf::Arena* absl_nonnull arena;
  };
};

// Creates a custom map value from a manual dispatch table `dispatcher` and
// opaque data `content` whose format is only know to functions in the manual
// dispatch table. The dispatch table should probably be valid for the lifetime
// of the process, but at a minimum must outlive all instances of the resulting
// value.
//
// IMPORTANT: This approach to implementing CustomMapValue should only be
// used when you know exactly what you are doing. When in doubt, just implement
// CustomMapValueInterface.
CustomMapValue UnsafeCustomMapValue(const CustomMapValueDispatcher* absl_nonnull
                                    dispatcher ABSL_ATTRIBUTE_LIFETIME_BOUND,
                                    CustomMapValueContent content);

class CustomMapValue final
    : private common_internal::MapValueMixin<CustomMapValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kMap;

  // Constructs a custom map value from an implementation of
  // `CustomMapValueInterface` `interface` whose lifetime is tied to that of
  // the arena `arena`.
  CustomMapValue(const CustomMapValueInterface* absl_nonnull
                 interface ABSL_ATTRIBUTE_LIFETIME_BOUND,
                 google::protobuf::Arena* absl_nonnull arena
                     ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    ABSL_DCHECK(interface != nullptr);
    ABSL_DCHECK(arena != nullptr);
    content_ = CustomMapValueContent::From(CustomMapValueInterface::Content{
        .interface = interface, .arena = arena});
  }

  // By default, this creates an empty map whose type is `map(dyn, dyn)`. Unless
  // you can help it, you should use a more specific typed map value.
  CustomMapValue();
  CustomMapValue(const CustomMapValue&) = default;
  CustomMapValue(CustomMapValue&&) = default;
  CustomMapValue& operator=(const CustomMapValue&) = default;
  CustomMapValue& operator=(CustomMapValue&&) = default;

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
  using MapValueMixin::Equal;

  bool IsZeroValue() const;

  CustomMapValue Clone(google::protobuf::Arena* absl_nonnull arena) const;

  bool IsEmpty() const;

  size_t Size() const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status Get(const Value& key,
                   const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                   google::protobuf::MessageFactory* absl_nonnull message_factory,
                   google::protobuf::Arena* absl_nonnull arena,
                   Value* absl_nonnull result) const;
  using MapValueMixin::Get;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<bool> Find(
      const Value& key,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const;
  using MapValueMixin::Find;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status Has(const Value& key,
                   const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                   google::protobuf::MessageFactory* absl_nonnull message_factory,
                   google::protobuf::Arena* absl_nonnull arena,
                   Value* absl_nonnull result) const;
  using MapValueMixin::Has;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status ListKeys(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, ListValue* absl_nonnull result) const;
  using MapValueMixin::ListKeys;

  // See the corresponding type declaration of `MapValueInterface` for
  // documentation.
  using ForEachCallback = typename CustomMapValueInterface::ForEachCallback;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status ForEach(
      ForEachCallback callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<absl_nonnull ValueIteratorPtr> NewIterator() const;

  const CustomMapValueDispatcher* absl_nullable dispatcher() const {
    return dispatcher_;
  }

  CustomMapValueContent content() const {
    ABSL_DCHECK(dispatcher_ != nullptr);
    return content_;
  }

  const CustomMapValueInterface* absl_nullable interface() const {
    if (dispatcher_ == nullptr) {
      return content_.To<CustomMapValueInterface::Content>().interface;
    }
    return nullptr;
  }

  friend void swap(CustomMapValue& lhs, CustomMapValue& rhs) noexcept {
    using std::swap;
    swap(lhs.dispatcher_, rhs.dispatcher_);
    swap(lhs.content_, rhs.content_);
  }

 private:
  friend class common_internal::ValueMixin<CustomMapValue>;
  friend class common_internal::MapValueMixin<CustomMapValue>;
  friend CustomMapValue UnsafeCustomMapValue(
      const CustomMapValueDispatcher* absl_nonnull dispatcher
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      CustomMapValueContent content);

  CustomMapValue(const CustomMapValueDispatcher* absl_nonnull dispatcher,
                 CustomMapValueContent content)
      : dispatcher_(dispatcher), content_(content) {
    ABSL_DCHECK(dispatcher != nullptr);
    ABSL_DCHECK(dispatcher->get_type_id != nullptr);
    ABSL_DCHECK(dispatcher->get_arena != nullptr);
    ABSL_DCHECK(dispatcher->is_zero_value != nullptr);
    ABSL_DCHECK(dispatcher->size != nullptr);
    ABSL_DCHECK(dispatcher->find != nullptr);
    ABSL_DCHECK(dispatcher->has != nullptr);
    ABSL_DCHECK(dispatcher->list_keys != nullptr);
    ABSL_DCHECK(dispatcher->clone != nullptr);
  }

  const CustomMapValueDispatcher* absl_nullable dispatcher_ = nullptr;
  CustomMapValueContent content_ = CustomMapValueContent::Zero();
};

inline std::ostream& operator<<(std::ostream& out, const CustomMapValue& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<CustomMapValue> final {
  static NativeTypeId Id(const CustomMapValue& type) {
    return type.GetTypeId();
  }
};

inline CustomMapValue UnsafeCustomMapValue(
    const CustomMapValueDispatcher* absl_nonnull dispatcher
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    CustomMapValueContent content) {
  return CustomMapValue(dispatcher, content);
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MAP_VALUE_H_
