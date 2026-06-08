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

// IWYU pragma: private

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUES_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUES_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

// absl::Cord is trivially relocatable IFF we are not using ASan or MSan. When
// using ASan or MSan absl::Cord will poison/unpoison its inline storage.
#if defined(ABSL_HAVE_ADDRESS_SANITIZER) || defined(ABSL_HAVE_MEMORY_SANITIZER)
#define CEL_COMMON_INTERNAL_VALUE_VARIANT_TRIVIAL_ABI
#else
#define CEL_COMMON_INTERNAL_VALUE_VARIANT_TRIVIAL_ABI ABSL_ATTRIBUTE_TRIVIAL_ABI
#endif

namespace cel {

class ValueInterface;
class ListValueInterface;
class MapValueInterface;
class StructValueInterface;

class Value;
class BoolValue;
class BytesValue;
class DoubleValue;
class DurationValue;
class ABSL_ATTRIBUTE_TRIVIAL_ABI ErrorValue;
class IntValue;
class ListValue;
class MapValue;
class NullValue;
class OpaqueValue;
class OptionalValue;
class StringValue;
class StructValue;
class TimestampValue;
class TypeValue;
class UintValue;
class UnknownValue;
class ParsedMessageValue;
class ParsedMapFieldValue;
class ParsedRepeatedFieldValue;
class ParsedJsonListValue;
class ParsedJsonMapValue;

class CustomListValue;
class CustomListValueInterface;

class CustomMapValue;
class CustomMapValueInterface;

class CustomStructValue;
class CustomStructValueInterface;

class ValueIterator;
using ValueIteratorPtr = std::unique_ptr<ValueIterator>;

class ValueIterator {
 public:
  virtual ~ValueIterator() = default;

  virtual bool HasNext() = 0;

  // Returns a view of the next value. If the underlying implementation cannot
  // directly return a view of a value, the value will be stored in `scratch`,
  // and the returned view will be that of `scratch`.
  virtual absl::Status Next(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) = 0;

  absl::StatusOr<Value> Next(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena);

  // Next1 returns values for lists and keys for maps.
  virtual absl::StatusOr<bool> Next1(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull key_or_value);

  absl::StatusOr<absl::optional<Value>> Next1(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena);

  // Next2 returns indices (in ascending order) and values for lists and keys
  // (in any order) and values for maps.
  virtual absl::StatusOr<bool> Next2(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nullable key,
      Value* absl_nullable value) = 0;

  absl::StatusOr<absl::optional<std::pair<Value, Value>>> Next2(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena);
};

namespace common_internal {

class SharedByteString;
class SharedByteStringView;

class LegacyListValue;

class LegacyMapValue;

class LegacyStructValue;

class ListValueVariant;

class MapValueVariant;

class StructValueVariant;

class CEL_COMMON_INTERNAL_VALUE_VARIANT_TRIVIAL_ABI ValueVariant;

ErrorValue GetDefaultErrorValue();

CustomListValue GetEmptyDynListValue();

CustomMapValue GetEmptyDynDynMapValue();

OptionalValue GetEmptyDynOptionalValue();

absl::Status ListValueEqual(
    const ListValue& lhs, const ListValue& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result);

absl::Status ListValueEqual(
    const CustomListValueInterface& lhs, const ListValue& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result);

absl::Status MapValueEqual(
    const MapValue& lhs, const MapValue& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result);

absl::Status MapValueEqual(
    const CustomMapValueInterface& lhs, const MapValue& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result);

absl::Status StructValueEqual(
    const StructValue& lhs, const StructValue& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result);

absl::Status StructValueEqual(
    const CustomStructValueInterface& lhs, const StructValue& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result);

const SharedByteString& AsSharedByteString(const BytesValue& value);

const SharedByteString& AsSharedByteString(const StringValue& value);

using ListValueForEachCallback =
    absl::FunctionRef<absl::StatusOr<bool>(const Value&)>;
using ListValueForEach2Callback =
    absl::FunctionRef<absl::StatusOr<bool>(size_t, const Value&)>;

template <typename Base>
class ValueMixin {
 public:
  absl::StatusOr<Value> Equal(
      const Value& other,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  friend Base;
};

template <typename Base>
class ListValueMixin : public ValueMixin<Base> {
 public:
  using ValueMixin<Base>::Equal;

  absl::StatusOr<Value> Get(
      size_t index, const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  using ForEachCallback = absl::FunctionRef<absl::StatusOr<bool>(const Value&)>;

  absl::Status ForEach(
      ForEachCallback callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const {
    return static_cast<const Base*>(this)->ForEach(
        [callback](size_t, const Value& value) -> absl::StatusOr<bool> {
          return callback(value);
        },
        descriptor_pool, message_factory, arena);
  }

  absl::StatusOr<Value> Contains(
      const Value& other,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  friend Base;
};

template <typename Base>
class MapValueMixin : public ValueMixin<Base> {
 public:
  using ValueMixin<Base>::Equal;

  absl::StatusOr<Value> Get(
      const Value& key,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  absl::StatusOr<absl::optional<Value>> Find(
      const Value& other,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  absl::StatusOr<Value> Has(
      const Value& key,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  absl::StatusOr<ListValue> ListKeys(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  friend Base;
};

template <typename Base>
class StructValueMixin : public ValueMixin<Base> {
 public:
  using ValueMixin<Base>::Equal;

  absl::StatusOr<Value> GetFieldByName(
      absl::string_view name,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  absl::Status GetFieldByName(
      absl::string_view name,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
    return static_cast<const Base*>(this)->GetFieldByName(
        name, ProtoWrapperTypeOptions::kUnsetNull, descriptor_pool,
        message_factory, arena, result);
  }

  absl::StatusOr<Value> GetFieldByName(
      absl::string_view name, ProtoWrapperTypeOptions unboxing_options,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  absl::StatusOr<Value> GetFieldByNumber(
      int64_t number,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  absl::Status GetFieldByNumber(
      int64_t number,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
    return static_cast<const Base*>(this)->GetFieldByNumber(
        number, ProtoWrapperTypeOptions::kUnsetNull, descriptor_pool,
        message_factory, arena, result);
  }

  absl::StatusOr<Value> GetFieldByNumber(
      int64_t number, ProtoWrapperTypeOptions unboxing_options,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  absl::StatusOr<std::pair<Value, int>> Qualify(
      absl::Span<const SelectQualifier> qualifiers, bool presence_test,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  friend Base;
};

template <typename Base>
class OpaqueValueMixin : public ValueMixin<Base> {
 public:
  using ValueMixin<Base>::Equal;

  friend Base;
};

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUES_H_
